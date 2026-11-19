// G7 — Convergenza: differential legacy(v2→flat) vs v2. Lo stesso sample deve produrre output
// IDENTICO sul flat GPM (path legacy, Proc_<id> → RunProcedureAsGpm) e sul driver v2
// (StepV2_<id>). Qualsiasi divergenza = regressione di convergenza.

using System.Reflection;
using Xunit;
using Basic.IlBackend;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class V2FlatDifferentialTests
{
    private static readonly string Root = FindRepoRoot();

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (dir != null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "GMBasicSharp.slnx"))) return dir.FullName;
            dir = dir.Parent;
        }
        return Directory.GetCurrentDirectory();
    }

    private static string Normalize(string s) => s.Replace("\r\n", "\n").Trim();

    private static string RunFlat(string sampleName)
    {
        var sample = Path.Combine(Root, "shared", "samples", "Concurrency", sampleName);
        Assert.True(File.Exists(sample), $"Sample mancante: {sample}");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "DiffFlat_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        var assembly = Assembly.Load(ms.ToArray());
        var main = assembly.GetType("GeneratedProgram")!
            .GetMethod("Main", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("Main non trovato.");

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var task = Task.Run(() => main.Invoke(null, null));
            if (!task.Wait(TimeSpan.FromSeconds(20)))
                throw new InvalidOperationException($"FLAT TIMEOUT: {sampleName} (il flat non esegue questo sample).");
            task.GetAwaiter().GetResult();
        }
        catch (TargetInvocationException tie) when (tie.InnerException is not null)
        {
            throw new InvalidOperationException($"FLAT FAIL: {sampleName}: {tie.InnerException.GetType().Name}: {tie.InnerException.Message}");
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return Normalize(sw.ToString());
    }

    private static string RunV2(string sampleName)
    {
        var sample = Path.Combine(Root, "shared", "samples", "Concurrency", sampleName);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var mainDef = program.LoadedModules.Values.SelectMany(m => m.Declarations)
            .OfType<FunctionDefinition>()
            .First(p => p.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase));
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "DiffV2_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        var dispatcher = IlGpmV2.CreateV2Dispatcher(Assembly.Load(ms.ToArray()));

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var scheduler = new PCodeScheduler();
            var task = scheduler.CreateTask(0);
            task.Scheduler = scheduler;
            task.Frame = new PCodeCallFrame { OwnerProcedure = mainDef.SymbolId, LocalVariables = new object?[64] };
            scheduler.Schedule(task);
            try
            {
                IlGpmV2Driver.RunV2Inline(scheduler, dispatcher, timeout: TimeSpan.FromSeconds(20));
            }
            catch (TimeoutException) { }
            if (task.PendingException is { } v2Ex)
                throw new InvalidOperationException($"V2 FAIL: {sampleName}: {v2Ex.Message}", v2Ex);
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return Normalize(sw.ToString());
    }

    // Sottoinsieme COMUNE: i sample che il flat esegue (canali/wg/sleep/TRY base). I sample
    // v2-specifici (FUNCTION/BYREF/ricorsione/closure/CHANNEL CLOSE) girano SOLO sul v2 —
    // il flat li ha come capacità aggiunte dalla v2 (vedi G4) — quindi non sono differenziali.
    [Theory]
    [InlineData("GpmV2ControlFlow.bas")]
    [InlineData("GpmV2TryFinally.bas")]
    public void FlatAndV2_ProduceIdenticalOutput(string sampleName)
    {
        var flat = RunFlat(sampleName);
        var v2 = RunV2(sampleName);
        Assert.Equal(flat, v2);
    }
}
