// Due FOR sequenziali sullo stesso lexeme I, in uno stepper GPM (SLEEP forza may-park).
// Il linker registra un SymbolId nuovo per ogni EnterLoopStmt: Start del secondo FOR
// deve azzerare lo STESSO locale che ForCheck/Next usano, altrimenti il secondo corpo
// non gira (SpritesExamples: un solo sprite a 0,0).

using System.Reflection;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class SequentialForGpmStepperTests
{
    [Fact]
    public async Task SequentialFor_SameLexeme_SecondLoopRuns_InGpmStepper()
    {
        var dir = Path.Combine(Path.GetTempPath(), "gm-seq-for-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        var source = Path.Combine(dir, "seqfor.bas");
        File.WriteAllText(source, """
            SUB Main()
              SLEEP 1
              DIM S AS LONG
              S = 0
              FOR I = 0 TO 4
                S = S + 1
              NEXT
              FOR I = 0 TO 4
                S = S + 10
              NEXT
              PRINT "SUM="; S
            END SUB
            """);

        var program = Basic.Orchestrator.Compile(source, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "SeqForGpm_" + Guid.NewGuid().ToString("N"), out _);
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
            var run = Task.Run(() => main.Invoke(null, null));
            var winner = await Task.WhenAny(run, Task.Delay(TimeSpan.FromSeconds(20)));
            if (winner != run)
                throw new InvalidOperationException("TIMEOUT. Output:\n" + sw);
            await run;
        }
        finally
        {
            Console.SetOut(originalOut);
            try { Directory.Delete(dir, true); } catch (IOException) { }
        }

        var output = sw.ToString();
        Assert.Matches(@"SUM=\s*55", output);
    }
}
