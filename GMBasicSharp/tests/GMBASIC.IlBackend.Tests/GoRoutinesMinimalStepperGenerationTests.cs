// G7 — PONYTAIL RIMOSSO: il compilatore IL genera SOLO gli stepper v2 `StepV2_<symbolId>`
// (EmitV2Steppers); il driver v2 (IlGpmV2Driver) è l'unico path GPM. Questo test verifica
// che l'assembly generato contenga StepV2_ per le procedure may-park di GoRoutinesMinimal.bas
// e che Main giri correttamente sul driver v2 (golden sequence).

using System.Collections.Immutable;
using System.Reflection;
using Xunit;
using Basic.IlBackend;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class GoRoutinesMinimalStepperGenerationTests
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

    private static string SamplePath => Path.Combine(Root, "shared", "samples", "Concurrency", "GoRoutinesMinimal.bas");

    private static Assembly CompileAssembly(BasicProgram program)
    {
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "GpmTest_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        return Assembly.Load(ms.ToArray());
    }

    [Fact]
    public void Compile_Generates_StepV2_Methods_For_MayPark_Procedures()
    {
        Assert.True(File.Exists(SamplePath), $"Sample mancante: {SamplePath}");
        var program = Basic.Orchestrator.Compile(SamplePath, null, false);
        var procedures = program.LoadedModules.Values
            .SelectMany(m => m.Declarations)
            .OfType<FunctionDefinition>()
            .ToImmutableArray();
        var byId = (int id) => procedures.FirstOrDefault(p => p.SymbolId == id);
        var mayPark = MayParkAnalysis.ComputeMayPark(procedures, byId);
        Assert.Equal(2, mayPark.Count); // Main + Produttore

        var assembly = CompileAssembly(program);
        var programType = assembly.GetType("GeneratedProgram")
            ?? throw new InvalidOperationException("GeneratedProgram non trovato nell'assembly.");

        var stepV2Methods = programType
            .GetMethods(BindingFlags.Public | BindingFlags.Static)
            .Where(m => m.Name.StartsWith("StepV2_", StringComparison.Ordinal))
            .ToList();

        // Deve esserci UN StepV2_ per ogni procedura may-park nel sottoinsieme v2.
        foreach (var proc in procedures.Where(p => mayPark.Contains(p.SymbolId)))
        {
            var stepName = "StepV2_" + proc.SymbolId;
            var step = programType.GetMethod(stepName, BindingFlags.Public | BindingFlags.Static);
            Assert.True(step is not null, $"Manca lo stepper v2 {stepName} per '{proc.OriginalName.Lexeme}'.");
            Assert.Equal(typeof(IlStepTransition), step!.ReturnType);
            var parameters = step.GetParameters();
            Assert.Equal(2, parameters.Length); // (PCodeTask, IlActivationFrame)
        }

        // Il percorso normale resta: Main e Proc_* esistono ancora.
        Assert.NotNull(programType.GetMethod("Main", BindingFlags.Public | BindingFlags.Static));
        foreach (var proc in procedures)
        {
            var procMethod = programType.GetMethod("Proc_" + proc.SymbolId, BindingFlags.Public | BindingFlags.Static);
            Assert.True(procMethod is not null, $"Manca Proc_{proc.SymbolId} per '{proc.OriginalName.Lexeme}'.");
        }
    }

    [Fact]
    public void EntryPoint_Uses_GpmV2_When_Main_Is_MayPark()
    {
        var program = Basic.Orchestrator.Compile(SamplePath, null, false);
        var procedures = program.LoadedModules.Values
            .SelectMany(m => m.Declarations)
            .OfType<FunctionDefinition>()
            .ToImmutableArray();
        var main = procedures.First(p => p.OriginalName.Lexeme == "Main");
        var byId = (int id) => procedures.FirstOrDefault(p => p.SymbolId == id);
        var mayPark = MayParkAnalysis.ComputeMayPark(procedures, byId);
        Assert.True(mayPark.Contains(main.SymbolId));

        // G7: il gate v2 deve accettare Main (il ponytail non esiste più).
        var defs = procedures.ToDictionary(p => p.SymbolId);
        Assert.True(IlProgramCompiler.ShouldRunMainAsGpmV2(main, defs));
    }

    [Fact]
    public void Execute_GoRoutinesMinimal_Via_Gpm_Produces_Golden_Sequence()
    {
        var program = Basic.Orchestrator.Compile(SamplePath, null, false);
        var assembly = CompileAssembly(program);
        var programType = assembly.GetType("GeneratedProgram")
            ?? throw new InvalidOperationException("GeneratedProgram non trovato.");
        var main = programType.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("Main non trovato.");

        var originalOut = Console.Out;
        var sw = new StringWriter();
        string output = "";
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var task = Task.Run(() => main.Invoke(null, null));
            if (!task.Wait(TimeSpan.FromSeconds(15)))
            {
                output = sw.ToString();
                throw new InvalidOperationException("Il driver v2 non è terminato entro 15s. Output:\n" + output);
            }
            task.GetAwaiter().GetResult();
            output = sw.ToString();
        }
        finally
        {
            // Il Main IL standard emette IlRuntime.PrepareGraphicsHost() (SdlGraphicsService su
            // HeadlessRuntimeHost.DefaultGraphics, un statico di processo): se resta impostato,
            // OGNI HeadlessRuntimeHost successivo nello stesso processo usa SDL e i park grafici
            // v2 deferiscono senza pump → i test grafici headless (che vogliono NullGraphicsService)
            // appesi. Ripristina i default statici per isolare l'esecuzione.
            Runtime.HeadlessRuntimeHost.DefaultGraphics = null;
            Runtime.HeadlessRuntimeHost.DefaultAudio = null;
            Console.SetOut(originalOut);
        }
        var normalized = string.Join(" ", output.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries));
        Assert.True(normalized.Contains("[MAIN] Ricevuto: 10"), "Manca '[MAIN] Ricevuto: 10'.\n--- OUTPUT ---\n" + output);
        Assert.True(normalized.Contains("[MAIN] Ricevuto: 20"), "Manca '[MAIN] Ricevuto: 20'.\n--- OUTPUT ---\n" + output);
        Assert.True(normalized.Contains("[MAIN] Ricevuto: 30"), "Manca '[MAIN] Ricevuto: 30'.\n--- OUTPUT ---\n" + output);
        Assert.Contains("[PRODUTTORE] Finito.", output);
        Assert.Contains("[MAIN] Produttore terminato.", output);
        Assert.Contains("--- TEST COMPLETATO ---", output);
    }

    [Fact]
    public void DriverV2_Runs_Steppers_To_Completion()
    {
        var program = Basic.Orchestrator.Compile(SamplePath, null, false);
        var assembly = CompileAssembly(program);
        var dispatcher = IlGpmV2.CreateV2Dispatcher(assembly);

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var scheduler = new PCodeScheduler();
            var main = program.LoadedModules.Values
                .SelectMany(m => m.Declarations)
                .OfType<FunctionDefinition>()
                .First(p => p.OriginalName.Lexeme == "Main");
            var task = scheduler.CreateTask(0);
            task.Scheduler = scheduler;
            task.Frame = new PCodeCallFrame { OwnerProcedure = main.SymbolId, LocalVariables = new object?[64] };
            scheduler.Schedule(task);

            IlGpmV2Driver.RunV2Inline(scheduler, dispatcher, timeout: TimeSpan.FromSeconds(15));
            if (task.PendingException is { } ex)
                throw new InvalidOperationException("V2 task fallito.", ex);

            var output = sw.ToString();
            Assert.Contains("[PRODUTTORE] Finito.", output);
            Assert.Contains("[MAIN] Produttore terminato.", output);
            Assert.Contains("--- TEST COMPLETATO ---", output);
        }
        finally
        {
            Console.SetOut(originalOut);
        }
    }
}
