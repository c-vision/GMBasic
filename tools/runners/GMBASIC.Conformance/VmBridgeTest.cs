// Test rapido della VM/C: compila con C#, esegue con gmb_vm_host (Executor.Native) come
// processo SEPARATO — mai in-process (il vecchio bridge P/Invoke libgmb_vm_bridge è stato
// rimosso, vedi Executor-CSharp-C-Split-Plan.md: nessun bridge, nemmeno nei test).
// Uso: dotnet run --project tests/GMBASIC.Conformance -- vmtest <file.bas>
using System;
using System.Diagnostics;
using System.IO;
using Basic;
using Basic.Runtime;

class VmBridgeTest
{
    public static int Run(string[] args)
    {
        if (args.Length < 2)
        {
            Console.Error.WriteLine("Uso: vmtest <file.bas>");
            return 2;
        }
        var file = Path.GetFullPath(args[1]);
        // CWD = cartella del sample: senza questo, qualunque OPEN/BSAVE/SAVE relativo
        // scrive nella CWD del processo (root del repo) invece che accanto al sample —
        // causa storica di sporcizia in root (AGENTS.md §9).
        Directory.SetCurrentDirectory(Path.GetDirectoryName(file)!);

        // 1. Compile
        var program = Orchestrator.Compile(file, null, false);
        if (program?.MainModule == null)
        {
            Console.Error.WriteLine("Compilazione fallita");
            return 1;
        }

        // 2. Codegen P-Code
        var chunk = new global::Compiler.Compiler().Compile(program);
        Console.WriteLine($"  code={chunk.Code.Count} bytes, consts={chunk.Constants.Count}, entry={chunk.EntryPoint}");
        if (Environment.GetEnvironmentVariable("GMB_DUMP_CHUNK") == "1")
            Console.WriteLine(global::Compiler.Disassembler.Disassemble(chunk, "main"));

        // 3. Esegui con gmb_vm_host come processo esterno.
        var exePath = ResolveGmbVmHostPath();
        if (exePath is null)
        {
            Console.Error.WriteLine("gmb_vm_host non trovato. Build: cmake -S native/gmb_vm_c -B native/gmb_vm_c/build " +
                                     "&& cmake --build native/gmb_vm_c/build. Poi imposta GMBASIC_VM_HOST al percorso " +
                                     "dell'eseguibile, o copialo accanto a questo test.");
            return 2;
        }

        var chunkPath = Path.Combine(Path.GetTempPath(), $"gmbasic_vmtest_{Guid.NewGuid():N}.chunk");
        try
        {
            File.WriteAllBytes(chunkPath, ChunkA1Serializer.Serialize(chunk));

            /* Argomenti del PROGRAMMA: tutto ciò che segue il file (`vmtest file.bas A B`) —
             * parità con la gamba IL, dove COMMAND(1) è il primo argomento dopo il file e non
             * i flag dell'harness. Prima non venivano inoltrati: COMMANDCOUNT() valeva 0 anche
             * con argomenti e nessun test poteva coprire COMMAND$ sulla gamba VM/C. */
            var progArgs = args.Length > 2 ? string.Join(" ", args.Skip(2).Select(a => "\"" + a.Replace("\"", "\\\"") + "\"")) : "";
            var psi = new ProcessStartInfo(exePath, $"\"{chunkPath}\" --entry-source \"{file}\"" + (progArgs.Length > 0 ? " " + progArgs : ""))
            {
                UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true,
                // AGENTS.md §9.1: CWD = cartella del SAMPLE (come la riga 24). Prima
                // WorkingDirectory = FindRepoRoot() faceva risolvere i path relativi del .bas
                // (PROJECTFOLDER(), "work/...", OPEN relativi) contro la ROOT del repo →
                // sporcizia in root (cartelle work/, :memory:/, .db, ecc.). Niente più root.
                WorkingDirectory = Path.GetDirectoryName(file)!,
            };
            using var proc = Process.Start(psi);
            if (proc == null)
            {
                Console.Error.WriteLine("gmb_vm_host non avviato.");
                return 2;
            }
            // Leggi ENTRAMBI i flussi CONCORCORRENTEMENTE. Leggerli in sequenza (stdout poi
            // stderr) può DEADLOCKARE se il figlio riempie il pipe di stderr (buffer 64 KB)
            // mentre il genitore è bloccato su stdout; in .NET `ReadToEnd` può inoltre lanciare
            // ArgumentOutOfRange ("length cannot be greater than the capacity") se il processo
            // termina/crasha durante la lettura (misurato sulla gamba VM/C con output abbondante:
            // TRAIN EVAL su dataset). Task asincroni avviati PRIMA di WaitForExit → nessun deadlock.
            var stdoutTask = proc.StandardOutput.ReadToEndAsync();
            var stderrTask = proc.StandardError.ReadToEndAsync();
            proc.WaitForExit();
            var stdout = stdoutTask.GetAwaiter().GetResult();
            var stderr = stderrTask.GetAwaiter().GetResult();

            Console.WriteLine("  ---- VM/C OUTPUT ----");
            Console.Write(stdout);
            if (!string.IsNullOrEmpty(stderr)) Console.Error.Write(stderr);
            if (string.IsNullOrWhiteSpace(stdout))
                Console.WriteLine("  (nessun output)");
            Console.WriteLine($"  exit={proc.ExitCode}");
            // Propaga l'exit code del vm host (0=ok, 6=errore BASIC a runtime non gestito,
            // 2=uso/IO): prima veniva collassato a 1, vanificando l'uniformazione a 6 su tutte
            // le gambe. Vedi Bugs/(solved) - Semantica-Errore-Builtin-Tra-Backend.md.
            return proc.ExitCode;
        }
        finally
        {
            try { File.Delete(chunkPath); } catch { /* best effort */ }
        }
    }

    /// <summary>Stesso schema di GmbVmHostRunner (common/Executor): variabile d'ambiente
    /// GMBASIC_VM_HOST per un percorso esplicito, altrimenti accanto a questo eseguibile.</summary>
    private static string? ResolveGmbVmHostPath()
    {
        var exeName = OperatingSystem.IsWindows() ? "gmb_vm_host.exe" : "gmb_vm_host";
        var envPath = Environment.GetEnvironmentVariable("GMBASIC_VM_HOST");
        if (!string.IsNullOrEmpty(envPath) && File.Exists(envPath)) return envPath;

        var besideExe = Path.Combine(AppContext.BaseDirectory, exeName);
        if (File.Exists(besideExe)) return besideExe;

        return null;
    }

    }
