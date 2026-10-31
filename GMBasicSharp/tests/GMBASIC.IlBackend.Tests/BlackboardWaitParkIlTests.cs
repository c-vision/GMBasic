using System;
using System.IO;
using System.Threading.Tasks;
using Xunit;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public sealed class BlackboardWaitParkIlTests
{
    // F1 (Strada C): AI.BLACKBOARDWAIT dentro una goroutine (GO SUB) deve PARCHEGGIARE il task
    // GPM (l'attesa gira sul blocking pool via BlackboardWaitAsync) e RIPRENDERE — mai il
    // fail-fast GpmWorkerGuard. Prima del fix: InvalidOperationException dal worker GPM.
    [Fact]
    public void BlackboardWait_InGoroutine_ParksAndResumes_OnIl()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"BBWaitIl_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        var sourcePath = Path.Combine(directory, "Main.bas");
        File.WriteAllText(sourcePath, """
            IMPORT AI WITH ALIAS

            SUB Worker(wg AS WAITGROUP)
                DIM r AS STRING
                r = AI.BLACKBOARDWAIT("k", 50)
                PRINT "got="; r
                DONE wg
            END SUB

            SUB Main
                DIM wg AS WAITGROUP
                GO SUB Worker(wg) IN wg
                WAIT wg
                PRINT "done"
            END SUB
            """);
        var previousDirectory = Directory.GetCurrentDirectory();
        try
        {
            Directory.SetCurrentDirectory(directory);
            var program = Basic.Orchestrator.Compile(sourcePath, null, false);
            var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "IlBBWait_" + Guid.NewGuid().ToString("N"), out _);
            var dll = Path.Combine(directory, "IlBBWait.dll");
            using (var fs = File.Create(dll)) asmBuilder.Save(fs);
            var assembly = System.Reflection.Assembly.LoadFile(dll);
            var main = assembly.GetType("GeneratedProgram")!.GetMethod("Main", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)!;
            var originalOut = Console.Out;
            var sw = new StringWriter();
            try
            {
                Console.SetOut(sw);
                Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
                var task = Task.Run(() => main.Invoke(null, null));
                Assert.True(task.Wait(TimeSpan.FromSeconds(20)), $"[IL] TIMEOUT\n{sw}");
                task.GetAwaiter().GetResult();
            }
            finally { Console.SetOut(originalOut); }

            var output = sw.ToString();
            Assert.Contains("got=", output);
            Assert.Contains("done", output);
        }
        finally
        {
            Directory.SetCurrentDirectory(previousDirectory);
            try { Directory.Delete(directory, recursive: true); } catch { }
        }
    }
}
