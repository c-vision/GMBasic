// VmHostFreshness — garantisce che `native/gmb_vm_c/build/gmb_vm_host` sia più nuovo dei
// sorgenti che il target compila.
//
// Vive in `tests/Shared/` (percorso TRACCIATO da git: `test_vmrun/` è gitignorata, quindi un file
// lì non arriverebbe mai in un clone pulito) ed è LINKATO dai due consumer via <Compile Include>:
// `test_vmrun/test_vmrun.csproj` (harness CLI) e `tests/GMBASIC.IlBackend.Tests` (suite xUnit).
// Una sola implementazione, così non possono divergere (stesso pattern dei file di Executor/
// linkati nel csproj dei test).
//
// Perché serve: `gmb_vm_host` è linkato BY-PATH, senza dipendenza di build verso i sorgenti C.
// Un binario vecchio esegue il runtime vecchio IN SILENZIO. Caso reale 2026-10-26:
// `MiniTests/system/commands.bas` dava solo `cc-ok` su VM/C per un host non ricostruito
// (falso negativo, poi attribuito per errore a un bug di SYSTEM). Il target MSBuild in
// `Directory.Build.targets` copre solo la ricostruzione dell'eseguibile C#: qui la garanzia
// vale anche quando il test parte da un runner che non ricompila nulla.

using System.Diagnostics;

namespace GmbTooling;

public static class VmHostFreshness
{
    // Directory che il target `gmb_vm_host` compila davvero: le add_subdirectory()/link di
    // native/gmb_vm_c/CMakeLists.txt.
    private static readonly string[] SourceDirs =
    {
        "native/gmb_vm_c",
        "native/gmb_runtime_c",
        "native/gmb_doc_c",
        "native/gmb_agent_runtime",
        "native/gmb_compiler_c",
        "native/gmb_db_c",
    };

    /// <summary>
    /// Ritorna il percorso di `gmb_vm_host`, ricostruendolo (incrementale) se manca o se i
    /// sorgenti sono più nuovi dell'eseguibile. Lancia <see cref="InvalidOperationException"/>
    /// se la ricostruzione non è possibile o fallisce: proseguire con un host vecchio produce
    /// misure false, che è peggio di nessuna misura.
    /// I messaggi vanno su stderr: stdout resta l'output del programma BASIC, confrontato con
    /// l'EXPECT dei MiniTest.
    /// </summary>
    public static string Ensure(string root)
    {
        /* Monorepo: il repo C è il submodule `GMBasic/`; nel repo unificato è la root. */
        var nativeRoot = Directory.Exists(Path.Combine(root, "native")) ? root : Path.Combine(root, "GMBasic");
        var buildDir = Path.Combine(nativeRoot, "native", "gmb_vm_c", "build");
        var host = Path.Combine(buildDir, "gmb_vm_host");
        var exists = File.Exists(host);
        if (exists && NewestSourceUtc(nativeRoot) <= File.GetLastWriteTimeUtc(host)) return host;

        Console.Error.WriteLine(exists
            ? "[vm/c] gmb_vm_host da ricostruire: sorgenti più nuovi dell'eseguibile"
            : $"[vm/c] gmb_vm_host assente: {host}");

        if (!File.Exists(Path.Combine(buildDir, "CMakeCache.txt")))
            throw new InvalidOperationException(
                $"[vm/c] build dir non configurata: {buildDir}\n" +
                "       configura: cmake -S native/gmb_vm_c -B native/gmb_vm_c/build");

        var psi = new ProcessStartInfo("cmake")
        {
            WorkingDirectory = nativeRoot,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
        };
        psi.ArgumentList.Add("--build");
        psi.ArgumentList.Add(buildDir);
        psi.ArgumentList.Add("--target");
        psi.ArgumentList.Add("gmb_vm_host");
        psi.ArgumentList.Add("-j");
        psi.ArgumentList.Add("4");

        string buildOut, buildErr;
        int exitCode;
        try
        {
            using var proc = Process.Start(psi)
                ?? throw new InvalidOperationException("[vm/c] impossibile avviare cmake");
            var outTask = proc.StandardOutput.ReadToEndAsync();
            var errTask = proc.StandardError.ReadToEndAsync();
            proc.WaitForExit();
            buildOut = outTask.GetAwaiter().GetResult();
            buildErr = errTask.GetAwaiter().GetResult();
            exitCode = proc.ExitCode;
        }
        catch (InvalidOperationException) { throw; }
        catch (Exception ex)
        {
            throw new InvalidOperationException($"[vm/c] impossibile avviare cmake: {ex.Message}", ex);
        }

        if (exitCode != 0 || !File.Exists(host))
        {
            var lines = (buildErr + "\n" + buildOut)
                .Split('\n')
                .Select(l => l.Trim())
                .Where(l => l.Length > 0 &&
                            (l.Contains("error", StringComparison.OrdinalIgnoreCase) ||
                             l.Contains("FAILED", StringComparison.Ordinal)))
                .Take(10)
                .ToArray();
            throw new InvalidOperationException(
                $"[vm/c] rebuild FALLITO (cmake exit={exitCode}) — mi fermo: un host vecchio dà falsi negativi\n" +
                string.Join("\n", lines.Select(l => "       " + l)) +
                $"\n       per riprodurre: cmake --build \"{buildDir}\" --target gmb_vm_host");
        }

        Console.Error.WriteLine(
            $"[vm/c] gmb_vm_host ricostruito ({File.GetLastWriteTimeUtc(host):yyyy-MM-dd HH:mm:ss} UTC)");
        return host;
    }

    /// <summary>
    /// mtime più recente fra i sorgenti/header/CMake delle directory che il target compila,
    /// esclusi gli alberi di build (`build`, `build_min`), i `tests/` (target separati, non
    /// entrano nell'eseguibile) e `_deps`.
    /// </summary>
    private static DateTime NewestSourceUtc(string nativeRoot)
    {
        var newest = DateTime.MinValue;
        foreach (var rel in SourceDirs)
        {
            var dir = Path.Combine(nativeRoot, rel);
            if (!Directory.Exists(dir)) continue;
            foreach (var file in Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories))
            {
                var p = file.Replace('\\', '/');
                if (p.Contains("/build", StringComparison.Ordinal) ||
                    p.Contains("/tests/", StringComparison.Ordinal) ||
                    p.Contains("/_deps/", StringComparison.Ordinal)) continue;
                var ext = Path.GetExtension(p);
                if (!ext.Equals(".c", StringComparison.OrdinalIgnoreCase) &&
                    !ext.Equals(".h", StringComparison.OrdinalIgnoreCase) &&
                    !ext.Equals(".txt", StringComparison.OrdinalIgnoreCase) &&
                    !ext.Equals(".cmake", StringComparison.OrdinalIgnoreCase)) continue;
                var t = File.GetLastWriteTimeUtc(file);
                if (t > newest) newest = t;
            }
        }
        return newest;
    }
}
