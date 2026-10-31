// FILE: Basic.IlBackend.NativeBackend.cs
// NativeBackend — compila la DLL managed generata da IlProgramCompiler in un vero
// eseguibile NATIVO (NativeAOT) tramite `dotnet publish -p:PublishAot=true` su un progetto
// host temporaneo. L'eseguibile risultante è un binario macchina singolo che NON richiede
// (né incorpora) un runtime .NET: dipende solo dalle librerie di sistema (libSystem,
// CoreFoundation, ecc. — verificato con `otool -L`). Il runtime IL di GMBasic (IlRuntime,
// BRT, servizi) viene compilato nativamente da ilc; il codice dinamico (&/EVAL/BSP) usa già
// la PCodeVM interna, compatibile con NativeAOT (MacroEvalDynamic è stato ritirato).

using System.Diagnostics;
using System.Text;

namespace Basic.IlBackend;

/// <summary>
/// Compila la DLL generata da <see cref="IlProgramCompiler.Compile"/> in un eseguibile
/// nativo tramite il toolchain NativeAOT (ilc) orchestrato da `dotnet publish`.
/// </summary>
/// <remarks>
/// Perché un progetto host e non ilc diretto: ilc va invocato con l'elenco completo dei
/// reference assembly del framework AOT + le dipendenze del progetto (incluse le NuGet
/// transitive: SQLitePCLRaw, Npgsql, Silk.NET, ecc.) + il link stage con clang (tutte le
/// librerie statiche del runtime pack). Ricostruire quel rsp/link a mano è fragile; il
/// progetto host con <c>ProjectReference</c> a IlBackend.csproj delega a MSBuild la
/// risoluzione completa del grafo di dipendenze e il link.
///
/// Il trucco per far compilare la DLL GENERATA (non un assembly del progetto): il progetto
/// host ha <c>SkipCompilerExecution=true</c> (niente csc → niente CS5001) e un target copia
/// la DLL generata come <c>$(IntermediateOutputPath)&lt;name&gt;.dll</c> PRIMA di
/// <c>ComputeIlcCompileInputs</c>, così <c>@(IlcCompileInput)</c> punta alla DLL generata
/// (che ha già l'entry point <c>GeneratedProgram.Main</c> e il flag COFF exe).
///
/// La DLL generata da <see cref="IlProgramCompiler.Compile"/> ha già il flag
/// <c>IMAGE_FILE_DLL</c> rimosso e l'entry point patchato (vedi PatchEntryPoint), quindi ilc
/// la accetta come modulo di entrypoint.
/// </remarks>
public static class NativeBackend
{
    /// <summary>
    /// Compila la DLL managed generata in un eseguibile nativo.
    /// </summary>
    /// <param name="dllPath">Percorso della DLL generata da <see cref="IlProgramCompiler.Compile"/>.</param>
    /// <param name="outputDir">Directory di output per l'eseguibile nativo.</param>
    /// <param name="rid">Runtime Identifier (es. osx-arm64). Null → rilevato dalla piattaforma.</param>
    /// <param name="edition">Edizione GMBasic ("Commercial" per i moduli DB commerciali).</param>
    /// <param name="descriptorPath">Root descriptor illink per-programma (vedi IlRuntimeProfiler);
    /// null → nessun descriptor (si compila il grafo intero, comportamento legacy).</param>
    /// <returns>Percorso dell'eseguibile nativo prodotto.</returns>
    public static string Build(string dllPath, string outputDir, string? rid = null, string edition = "",
        string? descriptorPath = null)
    {
        rid ??= ResolveRid();
        Directory.CreateDirectory(outputDir);

        var baseName = Path.GetFileNameWithoutExtension(dllPath);
        // Il progetto host deve stare DENTRO la repo GMBasicSharp: i ProjectReference
        // transitivi di IlBackendRuntime.csproj usano percorsi relativi che MSBuild risolve
        // solo per un progetto della repo, e dal fuori (es. destinazione del programma in
        // /tmp o in una dir di sample) il restore li relativizza al progetto host →
        // MSB3202 "file di progetto non trovato" per OGNI dipendenza, publish fallito.
        // Prima il host stava accanto all'eseguibile: funzionava solo quando la
        // destinazione era dentro la repo. Ora sta in <repo>/obj/aot (gitignored) e viene
        // rimosso a fine build, qualunque sia la destinazione.
        var workDir = Path.Combine(FindRepoRoot(), "obj", "aot",
            baseName + "_" + Guid.NewGuid().ToString("N")[..8]);
        Directory.CreateDirectory(workDir);

        var csproj = Path.Combine(workDir, baseName + ".csproj");
        File.WriteAllText(csproj, GenerateCsproj(baseName, dllPath, rid, edition, workDir, descriptorPath));

        // Copia il descriptor del linker nella workDir del progetto host (il csproj lo referenzia
        // con path relativo a $(MSBuildProjectDirectory)).
        if (descriptorPath != null)
        {
            var descName = Path.GetFileName(descriptorPath);
            File.Copy(descriptorPath, Path.Combine(workDir, descName), true);
        }

        var publishDir = Path.Combine(workDir, "publish");
        // Retry + obj pulito: sotto restore concorrente del repo condiviso il publish può
        // fallire con MSB3202 (ProjectReference annidati "non trovati" anche se esistono), e un
        // obj host avvelenato (restore con CWD/base errata) rende il fallimento permanente.
        // Ricetta verificata (2026-09-08): CWD=workDir + rm obj/bin host prima di OGNI tentativo
        // → publish deterministico 34MB standalone. Riprova fino a 3 volte per il churn del repo.
        string? stdout = null, stderr = null;
        for (var attempt = 1; attempt <= 3; attempt++)
        {
            foreach (var stale in new[] { Path.Combine(workDir, "obj"), Path.Combine(workDir, "bin"), publishDir })
                try { if (Directory.Exists(stale)) Directory.Delete(stale, recursive: true); } catch { }
            var psi = new ProcessStartInfo
            {
                FileName = "dotnet",
                Arguments = $"publish \"{csproj}\" -c Release -r {rid} -p:GmbEdition={EscapeProp(edition)} --self-contained -o \"{publishDir}\"",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                // Ancorare la CWD al dir del progetto host (base corretta per la risoluzione
                // dei ProjectReference annidati del grafo repo).
                WorkingDirectory = workDir,
            };
            using var proc = Process.Start(psi)!;
            stdout = proc.StandardOutput.ReadToEnd();
            stderr = proc.StandardError.ReadToEnd();
            proc.WaitForExit();
            if (proc.ExitCode == 0) { stdout = null; stderr = null; break; }
            if (attempt < 3)
            {
                var msb3202 = (stdout + "\n" + stderr).Contains("MSB3202", StringComparison.OrdinalIgnoreCase);
                System.Threading.Thread.Sleep(msb3202 ? 4000 : 1000); // MSB3202 = churn restore → attesa più lunga
            }
        }
        if (stdout is not null)
        {
            var log = Path.Combine(workDir, "publish.log");
            File.WriteAllText(log, stdout + "\n" + stderr);
            throw new InvalidOperationException(
                $"NativeBackend: dotnet publish fallito (exit ≠ 0 dopo 3 tentativi). Log: {log}\n{Truncate(stderr)}");
        }

        var exe = Path.Combine(publishDir, baseName);
        if (!File.Exists(exe))
        {
            // Alcuni RID aggiungono estensione (win → .exe)
            exe = Path.Combine(publishDir, baseName + ".exe");
            if (!File.Exists(exe))
                throw new InvalidOperationException($"NativeBackend: eseguibile nativo non trovato in {publishDir}.\n{Truncate(stderr)}");
        }

        // Copia l'eseguibile nativo nella directory di output richiesta.
        var destExe = Path.Combine(outputDir, Path.GetFileName(exe));
        File.Copy(exe, destExe, true);

        // L'eseguibile NativeAOT è standalone; copia solo i file NATIVI aggiuntivi
        // (librerie .dylib/.so/.dll per P/Invoke: e_sqlite3, libssl, libgmb_db, ecc.).
        // Niente .pdb (debug) né DLL managed (il codice managed è già nel binario).
        foreach (var extra in Directory.GetFiles(publishDir))
        {
            var name = Path.GetFileName(extra);
            var ext = Path.GetExtension(name).ToLowerInvariant();
            if (ext is not (".dylib" or ".so" or ".dll")) continue;
            File.Copy(extra, Path.Combine(outputDir, name), true);
        }

        // Best-effort: mantiene il workDir per diagnosi? No, pulizia.
        try { Directory.Delete(workDir, true); } catch { }

        return destExe;
    }

    private static string GenerateCsproj(string baseName, string dllPath, string rid, string edition, string workDir,
        string? descriptorPath)
    {
        var repoRoot = FindRepoRoot();
        var sb = new StringBuilder();
        sb.AppendLine("<Project Sdk=\"Microsoft.NET.Sdk\">");
        sb.AppendLine("  <PropertyGroup>");
        sb.AppendLine("    <OutputType>Exe</OutputType>");
        sb.AppendLine("    <TargetFramework>net10.0</TargetFramework>");
        sb.AppendLine($"    <AssemblyName>{baseName}</AssemblyName>");
        sb.AppendLine($"    <RuntimeIdentifier>{rid}</RuntimeIdentifier>");
        sb.AppendLine("    <SelfContained>true</SelfContained>");
        sb.AppendLine("    <PublishAot>true</PublishAot>");
        // Trimming (illink): rimuove i tipi/metodi non raggiungibili dai root. Misurato su
        // HelloNative: 96MB (nessun trim) → 78MB (TrimMode=link). Disattivato solo il
        // metadata completo della riflessione (IlcGenerateCompleteTypeMetadata=false di default);
        // il codice BASIC dinamico (&/EVAL/BSP) gira sulla PCodeVM interna, non sulla reflection.
        sb.AppendLine("    <PublishTrimmed>true</PublishTrimmed>");
        sb.AppendLine("    <TrimMode>link</TrimMode>");
        sb.AppendLine("    <IlcGenerateCompleteTypeMetadata>false</IlcGenerateCompleteTypeMetadata>");
        sb.AppendLine("    <IlcGenerateStackTraceData>true</IlcGenerateStackTraceData>");
        // SkipCompilerExecution: niente csc (la DLL generata È già l'assembly del programma);
        // senza, csc lancia CS5001 perché il progetto host non ha un Main proprio.
        sb.AppendLine("    <SkipCompilerExecution>true</SkipCompilerExecution>");
        sb.AppendLine("    <ProduceReferenceAssembly>false</ProduceReferenceAssembly>");
        if (!string.IsNullOrEmpty(edition))
            sb.AppendLine($"    <GmbEdition>{edition}</GmbEdition>");
        sb.AppendLine("  </PropertyGroup>");
        sb.AppendLine("  <ItemGroup>");
        // ProjectReference al runtime IL (GMBasic.Runtime.IL): MSBuild risolve il grafo repo e
        // ilcs compila la closure nell'immagine (esito misurato ~34-44MB self-contained).
        // Fragilità nota (2026-09-08): sotto restore concorrente del repo condiviso i
        // ProjectReference relativi annidati possono fallire con MSB3202 anche se i path
        // esistono → mitigato con WorkingDirectory=workDir (Build) + retry sul publish.
        sb.AppendLine($"    <ProjectReference Include=\"{Path.GetFullPath(Path.Combine(repoRoot, "common", "IlBackendRuntime", "IlBackendRuntime.csproj"))}\" />");
        if (!string.IsNullOrEmpty(descriptorPath))
            sb.AppendLine($"    <TrimmerRootDescriptor Include=\"$(MSBuildProjectDirectory)/{Path.GetFileName(descriptorPath)}\" />");
        sb.AppendLine("  </ItemGroup>");
        sb.AppendLine("  <Target Name=\"InjectGeneratedDll\" BeforeTargets=\"CopyFilesToOutputDirectory\">");
        sb.AppendLine("    <MakeDir Directories=\"$(IntermediateOutputPath)\" />");
        sb.AppendLine($"    <Copy SourceFiles=\"{dllPath}\" DestinationFiles=\"$(IntermediateOutputPath){baseName}.dll\" />");
        sb.AppendLine("  </Target>");
        sb.AppendLine("</Project>");
        return sb.ToString();
    }

    private static string EscapeProp(string edition) =>
        string.IsNullOrEmpty(edition) ? "Community" : edition.Replace("\"", "");

    private static string Truncate(string s, int max = 2000) =>
        s.Length <= max ? s : s[..max] + "\n…(troncato)";

    /// <summary>Root della repo (dove sta GMBASIC.sln) — risolta dalla posizione dell'assembly
    /// NativeBackend (sempre dentro il repo: common/IlBackendCodegen/bin/...), NON dalla CWD.
    /// Bug fix 2026-09-08: FindRepoRoot su CWD produceva un ProjectReference a IlBackendRuntime
    /// inesistente quando il publish girava da fuori repo (es. sample in /tmp per misura AOT) →
    /// ilc NON includeva GMBasic.Runtime.IL → eseguibile 1MB che falliva all'avvio
    /// "Could not find file 'GMBasic.Runtime.IL'". Fallback: walk dalla CWD.</summary>
    private static string FindRepoRoot()
    {
        // Si cerca il PROGETTO che l'host AOT deve referenziare, non un nome di soluzione:
        // nel monorepo la soluzione si chiama GMBasicSharp.slnx e la CWD tipica è la root
        // del monorepo — con il vecchio marker "GMBASIC.sln" la ricerca falliva e si
        // ripiegava sulla CWD, producendo un ProjectReference a un progetto inesistente:
        // ilc compilava solo hello.dll (senza il runtime IL) e l'eseguibile da 1 MB moriva
        // all'avvio con "Could not find file 'GMBasic.Runtime.IL'". Il marker è quindi
        // l'artefatto stesso (common/IlBackendRuntime/IlBackendRuntime.csproj).
        static string? Upwards(string? start)
        {
            var dir = string.IsNullOrEmpty(start) ? null : new DirectoryInfo(start);
            for (; dir != null; dir = dir.Parent)
                if (File.Exists(Path.Combine(dir.FullName, "common", "IlBackendRuntime", "IlBackendRuntime.csproj")))
                    return dir.FullName;
            return null;
        }

        return Upwards(new FileInfo(typeof(NativeBackend).Assembly.Location).DirectoryName)
            ?? Upwards(Directory.GetCurrentDirectory())
            ?? throw new InvalidOperationException(
                "NativeBackend: repo GMBasicSharp non trovata (nessun antenato contiene " +
                "common/IlBackendRuntime/IlBackendRuntime.csproj). Senza quel riferimento ilc " +
                "compilerebbe solo la DLL del programma, producendo un eseguibile che fallisce " +
                "all'avvio con \"Could not find file 'GMBasic.Runtime.IL'\".");
    }

    /// <summary>Closure managed del runtime già buildata: TUTTE le dll della dir di output di
    /// IlBackendRuntime (Debug se esiste, altrimenti Release). Usata come HintPath references
    /// dall'host AOT (approccio binario, niente grafo ProjectReference — v. GenerateCsproj).
    /// Il bin di output contiene le dll dei progetti + le package copiate (Sqlite/SQLitePCLRaw),
    /// NON le BCL del framework (mai copy-local) → elenco = closure applicativa completa.</summary>
    private static List<string> RuntimeAssemblies(string repoRoot)
    {
        var candidates = new[]
        {
            Path.Combine(repoRoot, "common", "IlBackendRuntime", "bin", "Debug", "net10.0"),
            Path.Combine(repoRoot, "common", "IlBackendRuntime", "bin", "Release", "net10.0"),
        };
        var dir = candidates.FirstOrDefault(Directory.Exists);
        if (dir is null) return new List<string>();
        return Directory.GetFiles(dir, "*.dll")
            .OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase)
            .ToList();
    }

    private static string ResolveRid()
    {
        if (OperatingSystem.IsMacOS()) return "osx-arm64";
        if (OperatingSystem.IsLinux()) return "linux-x64";
        if (OperatingSystem.IsWindows()) return "win-x64";
        return "osx-arm64";
    }
}
