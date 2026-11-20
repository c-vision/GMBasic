// AUDIT: quali sample Concurrency girano davvero con la GPM IL (caso semplice §13-2).
// Compila ogni sample con CompileToAssembly e, se l'entry point usa la GPM, lo esegue.
// Non fallisce: STAMPA lo stato (utile per l'audit, non un gate).

using System.Collections.Immutable;
using System.Reflection;
using Xunit;
using Basic.IlBackend;
using Basic.IlBackend.Gpm;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class ConcurrencySamplesAuditTests
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

    [Theory]
    [InlineData("ChannelBasics.bas")]
    [InlineData("ChanTest.bas")]
    [InlineData("ChanTestSendFirst.bas")]
    [InlineData("GoRoutinesMinimal.bas")]
    [InlineData("GoRoutinesMultiplexing.bas")]
    [InlineData("GoRoutinesWorkers.bas")]
    [InlineData("GoSelectNoSleep.bas")]
    [InlineData("GoSelectParkWake.bas")]
    [InlineData("GoSelectSimple.bas")]
    public void Compile_Concurrency_Sample(string sampleName)
    {
        var sample = Path.Combine(Root, "shared", "samples", "Concurrency", sampleName);
        if (!File.Exists(sample))
        {
            Console.WriteLine($"[audit] {sampleName}: MANCANTE");
            return;
        }
        try
        {
            C99Emission.Require(sample);
            var program = Basic.Orchestrator.Compile(sample, null, false);
            var procedures = program.LoadedModules.Values
                .SelectMany(m => m.Declarations)
                .OfType<FunctionDefinition>()
                .ToImmutableArray();
            var byId = (int id) => procedures.FirstOrDefault(p => p.SymbolId == id);
            var mayPark = MayParkAnalysis.ComputeMayPark(procedures, byId);
            var defs = procedures.ToDictionary(p => p.SymbolId);

            var main = procedures.FirstOrDefault(p => p.OriginalName.Lexeme == "Main");
            bool gpm = main is not null && IlProgramCompiler.ShouldRunMainAsGpmV2(main, defs);

            // Compila (eventuali NotSupported dello stepper emergono qui).
            var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "Audit_" + sampleName.Replace(".bas", ""), out _);
            using var ms = new MemoryStream();
            asmBuilder.Save(ms);
            var assembly = Assembly.Load(ms.ToArray());
            var programType = assembly.GetType("GeneratedProgram")!;

            var stepCount = programType.GetMethods(BindingFlags.Public | BindingFlags.Static)
                .Count(m => m.Name.StartsWith("Step_", StringComparison.Ordinal));
            Console.WriteLine($"[audit] {sampleName}: mayPark={mayPark.Count} gpmEntry={(gpm ? "SI" : "no")} stepMethods={stepCount}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[audit] {sampleName}: ERRORE {ex.GetType().Name}: {ex.Message}");
        }
    }

    [Theory]
    [InlineData("ChannelBasics.bas")]
    [InlineData("ChanTest.bas")]
    [InlineData("ChanTestSendFirst.bas")]
    [InlineData("GoRoutinesMinimal.bas")]
    [InlineData("GoRoutinesMultiplexing.bas")]
    [InlineData("GoSelectNoSleep.bas")]
    [InlineData("GoSelectParkWake.bas")]
    [InlineData("GoSelectSimple.bas")]
    [InlineData("GoRoutinesWorkers.bas")]
    public void Execute_Concurrency_Sample_Via_Gpm(string sampleName)
    {
        var sample = Path.Combine(Root, "shared", "samples", "Concurrency", sampleName);
        Assert.True(File.Exists(sample), $"Sample mancante: {sample}");
        C99Emission.Require(sample);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var assembly = CompileAssembly(program);
        var programType = assembly.GetType("GeneratedProgram")
            ?? throw new InvalidOperationException("GeneratedProgram non trovato.");
        var main = programType.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)
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
                throw new InvalidOperationException($"[exec] {sampleName}: TIMEOUT (driver bloccato). Output:\n{sw}");
            task.GetAwaiter().GetResult();
        }
        catch (TargetInvocationException tie) when (tie.InnerException is not null)
        {
            throw new InvalidOperationException($"[exec] {sampleName}: {tie.InnerException.GetType().Name}: {tie.InnerException.Message}");
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        var output = sw.ToString();
        Assert.False(string.IsNullOrWhiteSpace(output), $"[exec] {sampleName}: output vuoto (nessuna stampa).");
        Console.WriteLine($"[exec] {sampleName}: OK ({output.Length} char)");
    }

    private static Assembly CompileAssembly(BasicProgram program)
    {
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "AuditRun_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        return Assembly.Load(ms.ToArray());
    }
}