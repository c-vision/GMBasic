// BlackboardWorkersE2E — esegue samples/Community/Ai/BlackboardWorkersE2E.bas sul backend
// IL e verifica che la concorrenza (GO SUB ... IN wg) scriva al Blackboard senza perdite e
// che il synth (leggendo le chiavi dopo WAIT) ottenga la somma dei quadrati 1..4 = 30.
// Chiude il criterio di Fase 2 del TODO GMBasic-Agent-Runtime: N scrittori concorrenti +
// lettore aggregatore, end-to-end su un programma BASIC reale (niente AI.BLACKBOARDWAIT
// dentro una goroutine — il join usa il WAITGROUP nativo GPM, mai blocco di un worker).

using System.Reflection;
using System.Text;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class BlackboardWorkersE2ETests
{
    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    [Fact]
    public void Blackboard_ConcurrentWorkers_Synth_Reads_All()
    {
        var sample = Path.Combine(FindRepoRoot(), "samples", "Community", "Ai", "BlackboardWorkersE2E.bas");
        Assert.True(File.Exists(sample), $"sample mancante: {sample}");

        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "BBE2E_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        var assembly = Assembly.Load(ms.ToArray());
        var main = assembly.GetType("GeneratedProgram")?.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("Main non trovato.");

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var task = Task.Run(() => main.Invoke(null, null));
            Assert.True(task.Wait(TimeSpan.FromSeconds(30)), $"TIMEOUT E2E blackboard:\n{sw}");
            task.GetAwaiter().GetResult();
        }
        finally
        {
            Console.SetOut(originalOut);
        }

        var output = sw.ToString();
        Assert.Contains("E2E-OK", output);
        Assert.Contains("TOT= 30", output); // PRINT "; tot" formatta con spazi
        Assert.DoesNotContain("E2E-FAIL", output);
    }
}
