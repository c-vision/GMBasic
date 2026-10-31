// FILE: IlProgramCompiler.Host.cs
// PARTIAL — dominio HOST/APP (compilazione assembly, deploy/patch/copy, RID).
// Migrato da IlProgramCompiler.cs (2026-08-25 — protocollo 3-step).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: dominio HOST/APP (compile/patch/copy/RID).</summary>
public sealed partial class IlProgramCompiler
{


    /// <param name="dllOnly">true = produce SOLO la DLL del programma (l'unico artefatto che
    /// serve a ilc per l'AOT): niente apphost, niente copia della closure managed (~260 MB di
    /// runtime .NET), niente runtimeconfig/deps.json. Il percorso runner/REPL lascia false
    /// (comportamento invariato).</param>
    /// <param name="slim">true (default) = nella DISTRIBUZIONE (non linkRuntime, non dllOnly)
    /// si omettono i gruppi di dipendenze che il programma non può raggiungere e i binari
    /// nativi degli altri RID (vedi <see cref="ClosureSlim"/>). Ogni esclusione è stampata;
    /// <c>COMPILE ... -all</c> lo disattiva.</param>
    public static string Compile(BasicProgram program, string sourcePath, string outputDir, string assemblyName = "BasicProgram", string? rid = null, bool noOptimize = false, bool dumpIl = false, bool linkRuntime = false, bool dllOnly = false, bool slim = true)
    {
        var asmBuilder = CompileToAssembly(program, assemblyName, out _);
        var dir = Path.GetFullPath(outputDir);
        var baseName = Path.GetFileNameWithoutExtension(sourcePath);
        Directory.CreateDirectory(dir);
        rid ??= ResolveRuntimeIdentifier();

        // 1. Save DLL — PersistedAssemblyBuilder produces correct metadata tokens ONLY via Save()
        //    GenerateMetadata() tokens are not valid for complex assemblies (MS docs warning)
        var dllPath = Path.Combine(dir, baseName + ".dll");
        using (var stream = File.Create(dllPath))
            asmBuilder.Save(stream);

        // 1.5 Tabella nome-variabile-globale -> SymbolId, per il codice dinamico (&/EVAL, vedi
        // IlRuntime.TryResolveGlobalSymbolId): il BasicProgram (con la sua SymbolTable) esiste
        // solo in QUESTO processo (quello che compila) — quando l'eseguibile generato viene
        // lanciato è un processo SEPARATO, senza alcun accesso all'oggetto BasicProgram
        // originale. Questa tabella è il ponte: scritta qui su disco a fianco dell'eseguibile,
        // letta a runtime da IlRuntime (EnsureGlobalNameTableLoaded) nel processo lanciato.
        WriteGlobalNameTable(program, dir, baseName);

        // 2. Patch entry point — Save() sets correct tokens; we fix EntryPointToken at CLI header +20
        PatchEntryPoint(dllPath);

        // 2.5 DistIL post-compilation optimization
        // GMB_NO_ILOPTIM=1: salta l'ottimizzatore post-compilazione (diagnosi: distingue un bug del codegen da uno di ILOptim).
        if (!noOptimize && Environment.GetEnvironmentVariable("GMB_NO_ILOPTIM") != "1")
        {
            var (distilOk, distilCount, distilErr) = Basic.IlBackend.ILOptim.ILOptimOptimizer.Optimize(dllPath);
            if (distilOk) Console.Error.WriteLine($"[ILOptim] optimized {distilCount} method(s)");
            else if (distilErr != null) Console.Error.WriteLine($"[ILOptim] skipped: {distilErr}");
        }

        // 2.6 Dump IL disassembly
        if (dumpIl)
        {
            var ilPath = Path.Combine(dir, baseName + ".il");
            try { IlDisassembler.Dump(dllPath, ilPath); }
            catch (Exception ex) { Console.Error.WriteLine($"[IL dump] failed: {ex.Message}"); }
        }

        // dllOnly (build AOT): la DLL è tutto ciò che ilc consuma. Apphost, closure managed
        // (~260 MB) e runtimeconfig/deps.json servono solo al percorso "esegui con dotnet".
        if (dllOnly) return dllPath;

        // 3. Create AppHost wrapper — use relative DLL path (SDK convention)
        var apphost = FindAppHost();
        var exePath = Path.Combine(dir, baseName);
        Microsoft.NET.HostModel.AppHost.HostWriter.CreateAppHost(apphost, exePath, baseName + ".dll", false, null);

        // 4. Code sign on macOS (HostWriter does NOT sign; macOS ARM64 requires it)
        if (OperatingSystem.IsMacOS())
        {
            var psi = new System.Diagnostics.ProcessStartInfo("codesign", $"--force --sign - \"{exePath}\"")
            {
                UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true
            };
            using var proc = System.Diagnostics.Process.Start(psi);
            proc?.WaitForExit();
        }

        if (!OperatingSystem.IsWindows())
            new FileInfo(exePath).UnixFileMode = UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute |
                UnixFileMode.GroupRead | UnixFileMode.GroupExecute | UnixFileMode.OtherRead | UnixFileMode.OtherExecute;

        // 4. Copy runtime dependencies (+ resources: fonts, sprites, configs)
        // linkRuntime: il runner ESEGUE e poi cancella la cartella (test_ilrun, Executor, test grafici): symlink ai file
        // originali invece di copie (ogni run copiava ~850 MB: libtorch, mlx.metallib, onnxruntime, ... → disco pieno).
        // Le dylib caricate dal percorso reale risolvono anche i loro rpath (libMoltenVK in Resources/libs).
        // DISTRIBUZIONE (non linkRuntime, non dllOnly): si omettono i gruppi non raggiungibili
        // dal programma (ClosureSlim) — l'esecuzione resta invariata perché usa i symlink.
        var excluded = (slim && !linkRuntime && !dllOnly) ? AnalyzeForDistribution(dllPath, sourcePath) : [];
        CopyDependencies(dir, typeof(IlRuntime).Assembly.Location, linkRuntime, excluded);
        CopyDependencies(dir, typeof(Runtime.IRuntimeHost).Assembly.Location, linkRuntime, excluded);
        CopyResourceAssets(dir, rid!, linkRuntime);

        // 5. Write runtimeconfig.json (required by dotnet exec / AppHost)
        var runtimeConfigPath = Path.Combine(dir, baseName + ".runtimeconfig.json");
        File.WriteAllText(runtimeConfigPath, @"{
  ""runtimeOptions"": {
    ""tfm"": ""net10.0"",
    ""framework"": {
      ""name"": ""Microsoft.NETCore.App"",
      ""version"": ""10.0.0""
    }
  }
}");

        // 6. Write deps.json so hostpolicy can resolve custom assemblies
        WriteDepsJson(dir, baseName);

        return exePath;
    }

    /// <summary>
    /// Compila un programma BASIC in un VERO eseguibile nativo (NativeAOT): genera la DLL
    /// managed con <see cref="Compile"/>, poi la compila in un binario macchina singolo via
    /// <see cref="NativeBackend"/>. L'eseguibile risultante NON richiede né incorpora un
    /// runtime .NET.
    /// </summary>
    /// <param name="program">Il programma BASIC compilato.</param>
    /// <param name="sourcePath">Percorso del file sorgente (per nome base e dir Work).</param>
    /// <param name="outputDir">Directory di output per l'eseguibile nativo.</param>
    /// <param name="rid">Runtime Identifier (es. osx-arm64). Null → rilevato.</param>
    /// <param name="edition">Edizione ("Commercial" per i moduli DB commerciali).</param>
    /// <returns>Percorso dell'eseguibile nativo prodotto.</returns>
    public static string CompileNative(BasicProgram program, string sourcePath, string outputDir,
        string? rid = null, string edition = "")
    {
        // 1. Genera la DLL managed (intermedio del build: la DLL è l'unico input di ilc).
        //    NON accanto all'eseguibile: prima finiva in <out>/il con TUTTA la closure
        //    managed copiata (~260 MB per un `PRINT`), cioè il runtime .NET dentro la
        //    destinazione di un programma che è già nativo. Ora vive in una dir temporanea,
        //    rimossa a fine compilazione; l'unico artefatto di runtime che ne esce è
        //    <nome>.globals.json (letto da &/EVAL).
        var baseName = Path.GetFileNameWithoutExtension(sourcePath);
        var ilDir = Path.Combine(Path.GetTempPath(), $"gmb_aot_{Environment.ProcessId}_{Guid.NewGuid():N}");
        try
        {
            return CompileNativeCore(program, sourcePath, outputDir, rid, edition, ilDir, baseName);
        }
        finally
        {
            try { if (Directory.Exists(ilDir)) Directory.Delete(ilDir, recursive: true); } catch { /* best effort */ }
        }
    }

    private static string CompileNativeCore(BasicProgram program, string sourcePath, string outputDir,
        string? rid, string edition, string ilDir, string baseName)
    {
        var dllPath = Compile(program, sourcePath, ilDir, baseName, rid, dllOnly: true);

        // 2. Compila la DLL in eseguibile nativo.

        // 2b. Profilo di utilizzo del runtime: se il programma NON usa codice dinamico
        // (&/EVAL/BSP), genera un root descriptor illink che radica solo i metodi IlRuntime
        // usati — il resto (DB/AI/ConversionLib/...) non viene compilato nel binario nativo.
        // Vedere Todo/NativeAOT-Link-Solo-Necessario. Best-effort: un errore di analisi non
        // blocca la compilazione (si ripiega sul grafo completo).
        string? descriptorPath = null;
        try
        {
            var profile = IlRuntimeProfiler.Analyze(dllPath);
            Console.Error.WriteLine($"[NativeAOT] profilo: {profile}");
            descriptorPath = IlRuntimeProfiler.WriteDescriptor(profile, baseName, ilDir, baseName);
            if (descriptorPath != null)
                Console.Error.WriteLine($"[NativeAOT] root descriptor -> {descriptorPath}");
        }
        catch (Exception pex)
        {
            Console.Error.WriteLine($"[NativeAOT] profilo fallito (si compila il grafo intero): {pex.Message}");
        }

        var nativeExe = NativeBackend.Build(dllPath, outputDir, rid, edition, descriptorPath);

        // La tabella nome-globale → SymbolId è un DATO di runtime (IlRuntime la legge accanto
        // all'eseguibile per &/EVAL): esce dalla dir temporanea insieme al binario.
        var globals = Path.Combine(ilDir, baseName + ".globals.json");
        if (File.Exists(globals))
            File.Copy(globals, Path.Combine(outputDir, baseName + ".globals.json"), overwrite: true);

        return nativeExe;
    }

    /// <summary>Decide quali gruppi di dipendenze la DISTRIBUZIONE può omettere, analizzando i
    /// metodi del runtime usati dalla DLL generata (stessa analisi dell'AOT). Conservativo:
    /// profilo vuoto o codice dinamico (&/EVAL/BSP) → nessun filtro. Ogni esclusione è
    /// stampata, così la cartella prodotta è spiegabile senza indovinare.</summary>
    private static List<ClosureSlim.Group> AnalyzeForDistribution(string dllPath, string? sourcePath = null)
    {
        // Secondo segnale (indipendente dall'analisi IL): le parole chiave nel sorgente.
        // Se il file non è leggibile si passa null → decide solo l'analisi IL.
        string? sourceText = null;
        try { if (sourcePath != null && File.Exists(sourcePath)) sourceText = File.ReadAllText(sourcePath); } catch { }
        try
        {
            var profile = IlRuntimeProfiler.Analyze(dllPath);
            if (profile.UsesDynamicCode)
            {
                Console.Error.WriteLine("[slim] il programma usa codice dinamico (&/EVAL/BSP): closure completa (nessuna esclusione)");
                return [];
            }
            var excluded = ClosureSlim.Excluded(profile, sourceText);
            foreach (var g in excluded)
                Console.Error.WriteLine($"[slim] escluso: {g.Label}");
            return excluded;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[slim] analisi non disponibile ({ex.Message}): closure completa");
            return [];
        }
    }

    /// <summary>Scrive &lt;dir&gt;/&lt;baseName&gt;.globals.json — nome variabile globale
    /// (case-insensitive) -> SymbolId assegnato dal Linker. Letto da IlRuntime nel processo
    /// dell'eseguibile lanciato (vedi commento al punto 1.5 in <see cref="Compile"/> per il
    /// perché). Best-effort: un fallimento qui non deve mai bloccare la compilazione — significa
    /// solo che &amp;/EVAL in quell'eseguibile non avranno cattura delle variabili globali.</summary>
    private static void WriteGlobalNameTable(BasicProgram program, string dir, string baseName)
    {
        try
        {
            var table = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            for (var i = 0; i < program.Symbols.Count; i++)
            {
                var s = program.Symbols.GetSymbol(i);
                if (s.Flags.HasFlag(SymbolFlags.IsGlobal) && !s.Flags.HasFlag(SymbolFlags.IsConst))
                    table[s.OriginalName] = s.Id;
            }
            var json = System.Text.Json.JsonSerializer.Serialize(table);
            File.WriteAllText(Path.Combine(dir, baseName + ".globals.json"), json);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[globals.json] scrittura fallita (non bloccante, & /EVAL perdono la cattura globali in questo eseguibile): {ex.Message}");
        }
    }

    private static void PatchEntryPoint(string dllPath)
    {
        var bytes = File.ReadAllBytes(dllPath);
        var asm = System.Reflection.Assembly.Load(bytes);
        var main = asm.GetType("GeneratedProgram")?.GetMethod("Main", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
        if (main == null) return;
        int token = main.MetadataToken;
        int peOffset = BitConverter.ToInt32(bytes, 0x3C);
        ushort optSize = BitConverter.ToUInt16(bytes, peOffset + 20);
        int cliDirRva = BitConverter.ToInt32(bytes, peOffset + 24 + 96 + 14 * 8);
        int sectionsOff = peOffset + 24 + optSize;
        ushort numSec = BitConverter.ToUInt16(bytes, peOffset + 6);
        for (int i = 0; i < numSec; i++)
        {
            int s = sectionsOff + i * 40;
            int rva = BitConverter.ToInt32(bytes, s + 12);
            int size = BitConverter.ToInt32(bytes, s + 8);
            int raw = BitConverter.ToInt32(bytes, s + 20);
            if (rva <= cliDirRva && cliDirRva < rva + size)
            {
                BitConverter.GetBytes(token).CopyTo(bytes, raw + (cliDirRva - rva) + 20);
                File.WriteAllBytes(dllPath, bytes);
                return;
            }
        }
    }

    private static string FindAppHost()
    {
        var paths = new[] {
            "/usr/local/share/dotnet/packs/Microsoft.NETCore.App.Host.osx-arm64",
            "/usr/local/share/dotnet/packs/Microsoft.NETCore.App.Host.linux-x64",
            "/usr/share/dotnet/packs/Microsoft.NETCore.App.Host.osx-arm64",
            "/usr/share/dotnet/packs/Microsoft.NETCore.App.Host.linux-x64",
        };
        foreach (var basePath in paths)
        {
            if (!Directory.Exists(basePath)) continue;
            var versions = Directory.GetDirectories(basePath);
            foreach (var ver in versions.OrderDescending())
            {
                var apphost = Path.Combine(ver, "runtimes", Path.GetFileName(basePath).Replace("Microsoft.NETCore.App.Host.", ""), "native", "apphost");
                if (File.Exists(apphost)) return apphost;
            }
        }
        // Fallback: search entire packs directory
        var candidates = Directory.GetFiles(
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".dotnet", "packs"),
            "apphost", SearchOption.AllDirectories);
        if (candidates.Length > 0) return candidates[0];

        throw new FileNotFoundException("AppHost template not found. Install .NET SDK 10.");
    }

    private static void WriteDepsJson(string dir, string baseName)
    {
        var dlls = Directory.GetFiles(dir, "*.dll").Select(Path.GetFileName).ToArray();
        var depsPath = Path.Combine(dir, baseName + ".deps.json");
        var sb = new System.Text.StringBuilder();
        sb.AppendLine("{");
        sb.AppendLine("  \"runtimeTarget\": {");
        sb.AppendLine("    \"name\": \".NETCoreApp,Version=v10.0\",");
        sb.AppendLine("    \"signature\": \"\"");
        sb.AppendLine("  },");
        sb.AppendLine("  \"compilationOptions\": {},");
        sb.AppendLine("  \"targets\": {");
        sb.AppendLine("    \".NETCoreApp,Version=v10.0\": {");
        for (int i = 0; i < dlls.Length; i++)
        {
            var dll = dlls[i]; var comma = i < dlls.Length - 1 ? "," : "";
            var name = Path.GetFileNameWithoutExtension(dll);
            sb.AppendLine($"      \"{name}/1.0.0\": {{ \"runtime\": {{ \"{dll}\": {{}} }} }}{comma}");
        }
        sb.AppendLine("    }");
        sb.AppendLine("  },");
        sb.AppendLine("  \"libraries\": {");
        for (int i = 0; i < dlls.Length; i++)
        {
            var dll = dlls[i]; var comma = i < dlls.Length - 1 ? "," : "";
            var name = Path.GetFileNameWithoutExtension(dll);
            sb.AppendLine($"    \"{name}/1.0.0\": {{ \"type\": \"project\", \"serviceable\": false, \"sha512\": \"\" }}{comma}");
        }
        sb.AppendLine("  }");
        sb.AppendLine("}");
        File.WriteAllText(depsPath, sb.ToString());
    }

    private static void CopyResourceAssets(string destDir, string rid, bool link = false)
    {
        var resourcesDir = Path.Combine(FindProjectRoot(), "Resources");
        if (!Directory.Exists(resourcesDir)) return;

        // Copy root-level assets (fonts, sprites, configs)
        foreach (var f in Directory.GetFiles(resourcesDir, "*"))
        {
            var ext = Path.GetExtension(f).ToLowerInvariant();
            if (ext is not (".otf" or ".ttf" or ".png" or ".bmp" or ".json")) continue;
            var dest = Path.Combine(destDir, Path.GetFileName(f));
            if (!File.Exists(dest)) PlaceFile(f, dest, link);
        }

        // Copy platform-specific native libs
        var libsDir = Path.Combine(resourcesDir, "libs", rid);
        if (Directory.Exists(libsDir))
        {
            foreach (var f in Directory.GetFiles(libsDir, "*"))
            {
                var dest = Path.Combine(destDir, Path.GetFileName(f));
                if (!File.Exists(dest)) PlaceFile(f, dest, link);
            }
        }
    }

    private static string ResolveRuntimeIdentifier()
    {
        if (OperatingSystem.IsMacOS()) return "osx-arm64";
        if (OperatingSystem.IsLinux()) return "linux-x64";
        if (OperatingSystem.IsWindows()) return "win-x64";
        return "osx-arm64";
    }

    private static string FindProjectRoot()
    {
        var dir = Path.GetDirectoryName(typeof(IlProgramCompiler).Assembly.Location)!;
        while (dir != null && !File.Exists(Path.Combine(dir, "GMBASIC.sln")))
            dir = Path.GetDirectoryName(dir);
        return dir ?? Directory.GetCurrentDirectory();
    }

    private static void CopyDependencies(string destDir, string assemblyPath, bool link = false,
        IReadOnlyList<ClosureSlim.Group>? excluded = null)
    {
        var srcDir = Path.GetDirectoryName(assemblyPath)!;
        foreach (var f in Directory.GetFiles(srcDir, "*"))
        {
            var ext = Path.GetExtension(f).ToLowerInvariant();
            if (ext is not (".dll" or ".pdb" or ".otf" or ".ttf" or ".png" or ".bmp" or ".json" or ".dylib" or ".so")) continue;
            // .pdb = simboli di debug: servono nel percorso di ESECUZIONE (link, stack trace
            // dei test) — in una DISTRIBUZIONE sono 4,5 MB di peso morto (25 file su 47 MB).
            if (ext == ".pdb" && !link) continue;
            if (excluded is { Count: > 0 } && ClosureSlim.IsExcluded(Path.GetFileName(f), excluded)) continue;
            var dest = Path.Combine(destDir, Path.GetFileName(f));
            // Sovrascrivi SEMPRE: una copia precedente (es. da una build vecchia) non deve
            // rimanere — altrimenti il processo IL usa una dipendenza stantia (es. una
            // IlBackend.dll senza i fix più recenti). Il !File.Exists era un bug: la DLL
            // non veniva mai aggiornata dopo la prima esecuzione.
            PlaceFile(f, dest, link);
        }

        // Binari NATIVI (SQLitePCLRaw: libe_sqlite3, ecc.): la struttura runtimes/<rid>/native/
        // non viene catturata dalla copia piatta. Vive accanto all'host in esecuzione
        // (o, in teoria, accanto all'assembly). Senza di essa il vectordb fallisce con
        // e_sqlite3 DllNotFound nei programmi IL generati.
        var runtimesSrc = Path.Combine(srcDir, "runtimes");
        if (!Directory.Exists(runtimesSrc))
            runtimesSrc = Path.Combine(AppContext.BaseDirectory, "runtimes");
        if (Directory.Exists(runtimesSrc))
            CopyRuntimes(runtimesSrc, destDir, link, excluded);
    }

    /// <summary>Mette <paramref name="src"/> in <paramref name="dest"/>: copia, oppure (link=true, non Windows) un SYMLINK al file
    /// originale. Un dest preesistente (file o link) viene sempre sostituito: scrivere "attraverso" un link precedente
    /// sovrascriverebbe il file originale. Se il symlink non e' creabile si ripiega sulla copia.</summary>
    private static void PlaceFile(string src, string dest, bool link)
    {
        try { if (File.Exists(dest) || new FileInfo(dest).LinkTarget is not null) File.Delete(dest); } catch { }
        if (link && !OperatingSystem.IsWindows())
        {
            try
            {
                // Punta al file VERO (la sorgente puo' essere a sua volta un symlink, es. bin/libgmb_gfx.dylib → libs-runtime):
                // cosi' dyld risolve gli rpath dalla posizione reale della dylib.
                var full = Path.GetFullPath(src);
                File.CreateSymbolicLink(dest, new FileInfo(full).ResolveLinkTarget(true)?.FullName ?? full);
                return;
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
        File.Copy(src, dest, true);
    }

    /// <summary>Cancella le cartelle temporanee <c>&lt;prefix&gt;*</c> di esecuzioni interrotte (kill, timeout) piu' vecchie di
    /// <paramref name="maxAge"/>: i runner le cancellano a fine run, ma un processo ucciso le lascia.</summary>
    public static void CleanupStaleTempDirs(string prefix, TimeSpan maxAge)
    {
        try
        {
            var cutoff = DateTime.UtcNow - maxAge;
            foreach (var d in Directory.EnumerateDirectories(Path.GetTempPath(), prefix + "*"))
            {
                try { if (Directory.GetLastWriteTimeUtc(d) < cutoff) Directory.Delete(d, true); } catch { }
            }
        }
        catch { }
    }

    /// <summary>Copia i binari nativi di <c>runtimes/</c> sia preservando i sottopercorsi
    /// <c>&lt;rid&gt;/native/</c> sia FLAT nella root dell'output — ma la copia flat SOLO per
    /// il RID corrente (altrimenti l'ultimo file per nome — di una piattaforma diversa —
    /// sovrascrive e il P/Invoke di <c>e_sqlite3</c> carica la libreria sbagliata).</summary>
    private static void CopyRuntimes(string srcDir, string outputRoot, bool link = false,
        IReadOnlyList<ClosureSlim.Group>? excluded = null)
    {
        var currentRid = System.Runtime.InteropServices.RuntimeInformation.RuntimeIdentifier;
        // Solo i binari che QUESTA macchina può caricare: il RID corrente, la sua famiglia e il
        // livello portable. `runtimes/` ne contiene per android/iOS/Windows/Linux/… (281 MB nel
        // repo): copiarli in una distribuzione locale era ~116 MB di peso morto per programma.
        var acceptableRids = ClosureSlim.AcceptableRids(currentRid);
        foreach (var f in Directory.GetFiles(srcDir, "*", SearchOption.AllDirectories))
        {
            var ext = Path.GetExtension(f).ToLowerInvariant();
            if (ext is not (".dylib" or ".so" or ".dll")) continue;
            // Un unico punto di decisione: `runtimes/**` contiene ANCHE dll gestite
            // (es. runtimes/unix/lib/net9.0/Microsoft.Data.SqlClient.dll), che prima
            // sfuggivano al filtro perché si controllavano solo i prefissi NATIVI.
            if (excluded is { Count: > 0 }
                && (ClosureSlim.IsNativeExcluded(Path.GetFileName(f), excluded)
                    || ClosureSlim.IsExcluded(Path.GetFileName(f), excluded))) continue;
            var rel = Path.GetRelativePath(srcDir, f);
            // primo segmento del path relativo = cartella del RID (`native` = senza RID)
            var firstSep = rel.IndexOf(Path.DirectorySeparatorChar);
            if (firstSep <= 0) continue;
            var ridDir = rel[..firstSep];
            if (!acceptableRids.Contains(ridDir)) continue;
            // Percorso runtimes/<rid>/native/ (loader basato su deps.json)
            var ridDest = Path.Combine(outputRoot, "runtimes", rel);
            Directory.CreateDirectory(Path.GetDirectoryName(ridDest)!);
            PlaceFile(f, ridDest, link);
            // Copia flat nella root SOLO per il rid corrente (probe standard del DllImport)
            if (rel.StartsWith(currentRid + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            {
                var flat = Path.Combine(outputRoot, Path.GetFileName(f));
                PlaceFile(f, flat, link);
            }
        }
    }

    /// <summary>
    /// Mappa il DataType di un parametro/tipo di ritorno di procedura/metodo/campo al
    /// tipo CLR — DataTypeMapper.Map assume RANK 1 per un ArrayType (MakeArrayType() senza
    /// rank), il che è però esattamente corretto qui: un parametro-array in questo dialetto
    /// si dichiara SOLO come "Nome()" (parentesi vuote, vedi Parser.Expressions.Complex.cs,
    /// ParseParameters) — la sintassi stessa non permette di esprimere un rank diverso da 1
    /// per un parametro/tipo di ritorno (a differenza di una DIM locale o di un campo
    /// CLASS/TYPE, che hanno Dimensions espliciti) — quindi il rank NON manca mai per un
    /// array-parametro reale, è semplicemente sempre 1 per costruzione del linguaggio. Vedi
    /// RegisterParameters per come un parametro-array (sempre BYREF per lo stesso motivo,
    /// vedi ParseParameters) viene materializzato in _arrays.
    ///
    /// GAP NOTO per il tipo di RITORNO: questo fix permette a una FUNCTION dichiarata "AS
    /// ARRAY OF T" di compilare con la firma CIL corretta, ma una variabile SCALARE che ne
    /// riceve il risultato (es. <c>DIM X AS ARRAY OF DOUBLE : X = MakeArray()</c>, mai
    /// dichiarata con Dimensions espliciti) non viene registrata in _arrays
    /// (EmitScalarDeclaration la tratta come una variabile qualunque, via _locals) —
    /// indicizzarla dopo (<c>X(0)</c>) fallisce con "accesso a un array non DIMensionato".
    /// Nessun campione BASIC esistente usa questo pattern (verificato); non ancora risolto.
    /// </summary>
}
