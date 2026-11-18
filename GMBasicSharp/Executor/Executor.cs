// Executor/Executor.cs — Executor V2 (managed): minimal IL-only.
// Compila SEMPRE e SOLO ILBackend C# (Orchestrator.Compile "il" → IlProgramCompiler → run).
// Nessun parametro backend (per design: managed = IL).
using System;
using System.IO;
using System.Reflection;
using Basic;
using Basic.IlBackend;

namespace Basic.ExecutorV2;

public sealed class Executor
{
    /// <summary>Compila il file SOLO in IL e lo esegue in-process. Ritorna exit code.</summary>
    public int Run(string file, bool noOptimize = false)
    {
        // CWD = directory del programma: i sample con path relativi (es. "work/...")
        // creano i loro file NELLA LORO cartella, non nella CWD del processo (repo root
        // quando lanciato da `dotnet run` — AGENTS.md §9.3). Il file va reso assoluto
        // PRIMA di cambiare CWD, altrimenti Orchestrator.Compile risolve un path relativo
        // contro la CWD appena cambiata (bug percorso raddoppiato, come già corretto in REPL).
        var savedCwd = Directory.GetCurrentDirectory();
        file = Path.GetFullPath(file);
        var fileDir = Path.GetDirectoryName(file);
        if (!string.IsNullOrEmpty(fileDir)) Directory.SetCurrentDirectory(fileDir);
        // Entry-point del programma: SET PATH di default = la sua cartella (APPPATH/PROJECTFOLDER).
        Basic.IlBackend.IlRuntime.SetEntrySourcePath(file);
        try
        {
            BasicProgram program;
            try { program = Orchestrator.Compile(file, null, false, "il"); }
            catch (Exception ex) { Console.Error.WriteLine($"[COMPILE] {ex.GetType().Name}: {ex.Message}"); return 2; }
            if (program?.MainModule == null) { Console.Error.WriteLine("[COMPILE] null"); return 2; }

            IlProgramCompiler.CleanupStaleTempDirs("gmb_il_", TimeSpan.FromHours(12));   // residui di run interrotte
            var tmpDir = Path.Combine(Path.GetTempPath(), "gmb_il_" + Guid.NewGuid().ToString("N")[..8]);
            Directory.CreateDirectory(tmpDir);
            try
            {
                var asmPath = IlProgramCompiler.Compile(program, file, tmpDir, "GeneratedProgram", null, noOptimize, true, linkRuntime: true);
                var dllPath = Path.ChangeExtension(asmPath, ".dll");
                var asm = Assembly.LoadFile(Path.GetFullPath(dllPath));
                var main = asm.GetType("GeneratedProgram")?.GetMethod("Main",
                    BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
                if (main == null) { Console.Error.WriteLine("[RUN] GeneratedProgram.Main non trovato"); return 5; }
                main.Invoke(null, null);
                return 0;
            }
            catch (TargetInvocationException tie)
            {
                Console.Error.WriteLine($"[RUN] {(tie.InnerException ?? tie).Message}");
                // STACK TRACE BASIC (parità C99Native "[GMBASIC] ERROR … / in C (riga N)",
                // VM/C e PCode): la catena è ancora intatta (nessun CATCH l'ha scartata).
                var trace = Basic.IlBackend.IlRuntime.TraceText();
                if (trace.Length > 0) Console.Error.Write(trace);
                return 6;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"[ILGEN] {ex.GetType().Name}: {ex.Message}");
                return 3;
            }
            finally
            {
                try { Directory.Delete(tmpDir, true); } catch { }
            }
        }
        finally
        {
            Directory.SetCurrentDirectory(savedCwd);
        }
    }
}
