// Regression per Bugs/ILBackend-Gpm-Stepper-For-FrameChain-Hang.md: WAITKEY (e, scoperto
// dopo, anche INKEY var senza timeout) chiamavano IlRuntime.ReadKey()/Inkey() (spin-wait
// bloccante, Thread.Sleep) direttamente anche dentro uno stepper GPM, congelando l'INTERO
// thread del driver (nessun altro task/goroutine avanza) finché non arriva un tasto -- non
// erano riconosciuti come punti di blocco cooperativo da MayParkAnalysis.IsBlockPoint.
//
// La verifica END-TO-END affidabile per questi due statement (che richiedono una vera
// grafica "disponibile" per non prendere lo shortcut no-op) è tramite test_graphics_headless
// (SDL_VIDEODRIVER=dummy, vedi il bug doc §8) -- un doppio in-process di IGraphicsService con
// solo IsAvailable=true (senza una vera SdlGraphicsService dietro) fa sì che il percorso PRINT
// del programma generato (che instrada tramite IRuntimeHost.Console/PrintDispatch) smetta di
// scrivere sull'output catturato dal test, un artefatto del doppio finto, non del fix -- per
// questo qui restano solo i test che NON dipendono da "grafica disponibile" end-to-end.
using System.Reflection;
using Xunit;
using Basic.IlBackend;
using Basic.IlBackend.Gpm;
using Basic.Runtime;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Tests;

file sealed class FakeAvailableGraphicsService : NullGraphicsService
{
    public override bool IsAvailable => true;
}

[Collection("GpmRuntimeState")]
public class GpmWaitKeyStepperTests
{
    [Fact]
    public void TwoForLoops_SharedCounter_FrameChainPlusNonFlat_ShouldFinish()
    {
        var source =
            "IMPORT CONCURRENCY\n" +
            "SUB ShowIntro(n AS LONG)\n" +
            "  PRINT \"INTRO\"; n\n" +
            "  SLEEP 10\n" +
            "  PRINT \"INTRO-END\"; n\n" +
            "END SUB\n" +
            "SUB TestNonFlat(cnt AS LONG)\n" +
            "  DIM k AS LONG = 0\n" +
            "  DO\n" +
            "    SLEEP 10\n" +
            "    k = k + 1\n" +
            "    IF k >= cnt THEN EXIT DO\n" +
            "  LOOP\n" +
            "  PRINT \"NONFLAT-DONE\"\n" +
            "END SUB\n" +
            "SUB Main()\n" +
            "  DIM A() AS LONG = {1, 2}\n" +
            "  DIM B() AS LONG = {1, 2}\n" +
            "  FOR I = 0 TO UBOUND(A)\n" +
            "    ShowIntro(A(I))\n" +
            "    TestNonFlat(A(I))\n" +
            "  NEXT I\n" +
            "  FOR I = 0 TO UBOUND(B)\n" +
            "    ShowIntro(B(I))\n" +
            "    TestNonFlat(B(I))\n" +
            "  NEXT I\n" +
            "  PRINT \"MAIN-DONE\"\n" +
            "END SUB\n";

        var output = RunCompiledSample(source, "ScratchTwoFor_");
        Assert.Contains("MAIN-DONE", output);
    }

    /// <summary>Verifica isolata di IlGpmRuntime.WaitKeyStep (G7, frame.Await): con la coda
    /// vuota deve parcheggiare (false) SENZA bloccare, registrando il task come waiter; quando
    /// IlRuntime.QueueInkey arriva, il task deve svegliarsi e il retry deve consumare la chiave.</summary>
    [Fact]
    public void WaitKeyStep_EmptyQueue_ParksWithoutBlocking_ThenWakesOnQueueInkey()
    {
        IlRuntime.ResetProgramState();
        IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(new StringWriter(), new Runtime.RuntimeConfig(), new FakeAvailableGraphicsService()));
        var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask(0);
        task.Scheduler = scheduler;
        var frame = new IlActivationFrame { ProcedureId = 1, Locals = new object?[16] };

        // Coda vuota: torna SUBITO false (park), op IlWaitKeyAwait su frame.Await.
        var parked = IlGpmRuntime.WaitKeyStep(task, frame, out _);
        Assert.False(parked);
        Assert.NotNull(frame.Await);
        task.State = PCodeTaskState.Blocked;

        // Arriva un tasto: QueueInkey sveglia il task registrato.
        IlRuntime.QueueInkey("A");
        Assert.Equal(PCodeTaskState.Runnable, task.State);

        var resolved = IlGpmRuntime.WaitKeyStep(task, frame, out var key3);
        Assert.True(resolved);
        Assert.Equal("A", key3);
        Assert.Null(frame.Await);
    }

    /// <summary>Senza grafica agganciata (NullGraphicsService, IsAvailable=false): WaitKeyStep
    /// deve PARCHEGGIARE registrandosi nel waiter registry. Il wake arriva da QueueInkey.</summary>
    [Fact]
    public void WaitKeyStep_NoGraphicsAttached_ParksWithWaiterRegistry()
    {
        IlRuntime.ResetProgramState();
        IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(new StringWriter(), new Runtime.RuntimeConfig(), new NullGraphicsService()));
        var scheduler = new PCodeScheduler();
        var task = scheduler.CreateTask(0);
        task.Scheduler = scheduler;
        var frame = new IlActivationFrame { ProcedureId = 1, Locals = new object?[16] };

        var resolved = IlGpmRuntime.WaitKeyStep(task, frame, out var key);
        Assert.False(resolved);
        Assert.Equal("", key);
        Assert.NotNull(frame.Await);
        task.State = PCodeTaskState.Blocked;

        // Un tasto iniettato (runner/test) deve svegliare il waiter e il retry consumarlo.
        IlRuntime.QueueInkey("A");
        var resolved2 = IlGpmRuntime.WaitKeyStep(task, frame, out var key2);
        Assert.True(resolved2);
        Assert.Equal("A", key2);
        Assert.Null(frame.Await);
    }

    private static string RunCompiledSample(string source, string prefix)
    {
        var tempDir = Path.Combine(Path.GetTempPath(), "gmbasic-scratch-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDir);
        var sample = Path.Combine(tempDir, prefix + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(sample, source);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, prefix + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        var assembly = Assembly.Load(ms.ToArray());
        var programType = assembly.GetType("GeneratedProgram")!;
        var main = programType.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)!;

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            IlRuntime.ResetProgramState();
            IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var task = Task.Run(() => main.Invoke(null, null));
            Assert.True(task.Wait(TimeSpan.FromSeconds(15)), "driver TIMEOUT\n" + sw);
            task.GetAwaiter().GetResult();
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return sw.ToString();
    }
}
