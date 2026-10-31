// REGRESSIONE: CONTINUE/EXIT con etichetta di un loop ESTERNO su DO...LOOP annidati.
// Il label di CONTINUE deve saltare al LabelStmt(loopContinueLabel) della LOOP (PRIMA del
// jump-back); prima _loopExitLabels marcava continue ed exit nello stesso punto DOPO il
// jump-back → CONTINUE <loop-esterno> usciva dal loop tutto (o loop infinito).
using System.Reflection;
using System.Text.RegularExpressions;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class LabeledLoopContinueTests
{
    [Fact]
    public async Task ContinueOuter_Resumes_Outer_Iteration()
    {
        var dir = Path.Combine(Path.GetTempPath(), "gm-lblcont-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        var source = Path.Combine(dir, "lblcont.bas");
        File.WriteAllText(source, """
            SUB Main()
              I = 0
              OUTER_DO: DO WHILE I < 3
                I = I + 1
                J = 0
                INNER_DO: DO WHILE J < 5
                  J = J + 1
                  PRINT "DO: I="; I; ", J="; J
                  IF I = 1 AND J = 2 THEN CONTINUE OUTER_DO
                  IF I = 2 AND J = 3 THEN EXIT OUTER_DO
                LOOP
              LOOP
              PRINT "FINE"
            END SUB
            """);

        var program = Basic.Orchestrator.Compile(source, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "LblCont_" + Guid.NewGuid().ToString("N"), out _);
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
            Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var run = Task.Run(() => main.Invoke(null, null));
            var winner = await Task.WhenAny(run, Task.Delay(TimeSpan.FromSeconds(8)));
            if (winner != run) throw new InvalidOperationException("TIMEOUT (loop infinito). Output:\n" + sw);
            await run;
        }
        finally
        {
            Console.SetOut(originalOut);
            try { Directory.Delete(dir, true); } catch (IOException) { }
        }

        var output = sw.ToString();
        Assert.Contains("FINE", output);
        // I=1: (1,1)(1,2) -> CONTINUE esterno; I=2: (2,1)(2,2)(2,3) -> EXIT. Totale 5.
        Assert.Equal(5, Regex.Matches(output, "DO: I=").Count);
    }
}
