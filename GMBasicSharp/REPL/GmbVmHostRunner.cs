// FILE: Basic.Executor.GmbVmHostRunner.cs
// Esegue un CompiledChunk con l'Executor C standalone (gmb_vm_host, zero CLR a runtime)
// come processo SEPARATO — sostituisce il vecchio bridge P/Invoke in-process (VmCBridge):
// serializza il chunk in A1/GMBC su un file temporaneo, spawna gmb_vm_host, attende la
// fine. I percorsi C#/C non si mischiano più nello stesso processo — vedi
// Executor-CSharp-C-Split-Plan.md (Obsidian). Condiviso tra ReplEngine (CLI) e
// GMBasicIde (Immediate window): entrambi chiamavano VmCBridge.Run direttamente.

using Basic.Runtime;

namespace Basic.Executor;

internal static class GmbVmHostRunner
{
    /// <summary>Esegue il chunk con gmb_vm_host, inoltrando ogni riga di stdout/stderr ai
    /// callback forniti (streaming al chiamante — console o widget IDE). Le librerie FFI dei
    /// pacchetti REQUIRE risolti (path assoluti) vengono pre-caricate via --ffi-lib, così
    /// `DECLARE ... LIB "nome"` risolve anche le lib di pacchetto (match per basename in
    /// gmb_ffi_dl_open). Ritorna l'exit code del processo, o -1 se l'eseguibile non è stato
    /// trovato/avviato (già segnalato tramite onErrLine).</summary>
    public static int Run(global::Compiler.CompiledChunk chunk, CancellationToken token,
        Action<string> onOutLine, Action<string> onErrLine, IReadOnlyList<string>? ffiLibs = null)
    {
        var exePath = ResolvePath();
        if (exePath is null)
        {
            onErrLine("?gmb_vm_host non trovato. Build: cmake -S native/gmb_vm_c -B native/gmb_vm_c/build " +
                      "&& cmake --build native/gmb_vm_c/build. Poi imposta GMBASIC_VM_HOST al percorso " +
                      "dell'eseguibile, o copialo accanto a Executor.");
            return -1;
        }

        var chunkPath = Path.Combine(Path.GetTempPath(), $"gmbasic_{Guid.NewGuid():N}.chunk");
        System.Diagnostics.Process? proc = null;
        try
        {
            File.WriteAllBytes(chunkPath, ChunkA1Serializer.Serialize(chunk));

            var hostArgs = new List<string> { $"\"{chunkPath}\"" };
            if (ffiLibs != null)
                foreach (var lib in ffiLibs)
                {
                    if (string.IsNullOrEmpty(lib)) continue;
                    hostArgs.Add("--ffi-lib");
                    hostArgs.Add($"\"{lib}\"");
                }

            var psi = new System.Diagnostics.ProcessStartInfo(exePath, string.Join(" ", hostArgs))
            {
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
            };
            ConfigureLibraryPath(psi);
            proc = System.Diagnostics.Process.Start(psi);
            if (proc == null) { onErrLine("?gmb_vm_host non avviato."); return -1; }

            using var reg = token.Register(() => { try { proc.Kill(true); } catch { /* ignore */ } });

            proc.OutputDataReceived += (_, e) => { if (e.Data != null) onOutLine(e.Data); };
            proc.ErrorDataReceived += (_, e) => { if (e.Data != null) onErrLine(e.Data); };
            proc.BeginOutputReadLine();
            proc.BeginErrorReadLine();
            proc.WaitForExit();

            return proc.ExitCode;
        }
        finally
        {
            proc?.Dispose();
            try { File.Delete(chunkPath); } catch { /* best effort */ }
        }
    }

    /// <summary>Variabile d'ambiente GMBASIC_VM_HOST per un percorso esplicito, altrimenti
    /// accanto a questo eseguibile (dove un futuro passo di packaging dovrebbe copiarlo —
    /// nessuna invocazione CMake da qui, a differenza del vecchio VmCBridge/gmb_vm_bridge).</summary>
    private static string? ResolvePath()
    {
        var exeName = OperatingSystem.IsWindows() ? "gmb_vm_host.exe" : "gmb_vm_host";
        var envPath = Environment.GetEnvironmentVariable("GMBASIC_VM_HOST");
        if (!string.IsNullOrEmpty(envPath) && File.Exists(envPath)) return envPath;

        var besideExe = Path.Combine(AppContext.BaseDirectory, exeName);
        if (File.Exists(besideExe)) return besideExe;

        // Sviluppo (repo): l'eseguibile vive in native/gmb_vm_c/build (dove lo produce CMake).
        // Risaliamo dal base dir / CWD fino alla root del repo (GMBASIC.sln) — senza questo il
        // default batch `gmbasic file.bas` falliva anche col host compilato.
        var repo = FindRepoRoot();
        if (repo != null)
        {
            foreach (var cand in new[]
            {
                Path.Combine(repo, "native", "gmb_vm_c", "build", exeName),
                Path.Combine(repo, "native", "gmb_vm_c", "build", "bin", exeName),
            })
                if (File.Exists(cand)) return cand;
        }
        return null;
    }

    /// <summary>Root del repo (dir con GMBASIC.sln) risalendo da AppContext.BaseDirectory, oppure
    /// null se packaged (nessun sln). Usata per trovare gmb_vm_host e le librerie runtime.</summary>
    private static string? FindRepoRoot()
    {
        try
        {
            var dir = new DirectoryInfo(AppContext.BaseDirectory);
            while (dir != null)
            {
                if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
                dir = dir.Parent;
            }
        }
        catch { /* best effort */ }
        return null;
    }

    /// <summary>Imposta GMB_TORCH_PATH e il library-path dinamico (DYLD su macOS, LD su Linux)
    /// per il processo host: le dylib del lato ML (cache utente &lt;libs&gt;/ml/&lt;rid&gt;, es. torch) non
    /// sono nel default dyld search path. Stessa configurazione della gamba VM-C del harness
    /// MiniTest.</summary>
    private static void ConfigureLibraryPath(System.Diagnostics.ProcessStartInfo psi)
    {
        var rid = System.Runtime.InteropServices.RuntimeInformation.ProcessArchitecture switch
        {
            System.Runtime.InteropServices.Architecture.Arm64 => OperatingSystem.IsMacOS() ? "osx-arm64" : "linux-arm64",
            System.Runtime.InteropServices.Architecture.X64 => OperatingSystem.IsMacOS() ? "osx-x64" : "linux-x64",
            _ => "osx-arm64",
        };
        var paths = new List<string>();
        // cache utente (~/.gmbasic/libs/ml/<rid>), non il checkout: `GmbiPaths.MlDir` conosce
        // anche la vecchia copia in libs-runtime/ per una macchina non ancora migrata.
        try
        {
            var mlDir = CommonLib.GmbiPaths.MlDir(rid);
            if (Directory.Exists(mlDir))
            {
                psi.Environment["GMB_TORCH_PATH"] = mlDir;
                paths.Add(mlDir);
            }
        }
        catch { /* niente cache: si prosegue senza */ }
        var repo = FindRepoRoot();
        foreach (var d in new[]
        {
            CommonLib.GmbiPaths.LibsCacheDir(rid),                    // ~/.gmbasic/libs/<rid> (cache on-demand)
            repo != null ? Path.Combine(repo, "Resources", "libs", rid) : null,
            Path.Combine(AppContext.BaseDirectory, rid),
            Path.Combine(AppContext.BaseDirectory, "Resources", "libs", rid),
        })
            if (d != null && Directory.Exists(d) && !paths.Contains(d)) paths.Add(d);
        var varName = OperatingSystem.IsMacOS() ? "DYLD_LIBRARY_PATH" : "LD_LIBRARY_PATH";
        var existing = Environment.GetEnvironmentVariable(varName);
        if (!string.IsNullOrEmpty(existing)) paths.Add(existing);
        if (paths.Count > 0) psi.Environment[varName] = string.Join(Path.PathSeparator, paths);
    }
}
