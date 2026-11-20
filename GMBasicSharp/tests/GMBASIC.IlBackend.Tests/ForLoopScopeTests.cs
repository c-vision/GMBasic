using System.Reflection;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class ForLoopScopeTests
{
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

    [Fact]
    public async Task DimM_ForM1to3_PostLoop_Restores_DimValue()
    {
        var root = FindRepoRoot();
        var source = Path.Combine(root, "shared", "samples", "Loops", "LoopsForNextOptimized.bas");

        C99Emission.Require(source);
        var program = Basic.Orchestrator.Compile(source, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "ForPost_" + Guid.NewGuid().ToString("N"), out _);
        var dumpDir = Path.Combine(Path.GetTempPath(), "gmb_forpost");
        Directory.CreateDirectory(dumpDir);
        var dll = Path.Combine(dumpDir, "forpost.dll");
        using (var fs = File.Create(dll)) asmBuilder.Save(fs);
        Basic.IlBackend.IlDisassembler.Dump(dll, Path.Combine(dumpDir, "forpost.il"));
        var assembly = Assembly.LoadFile(dll);

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
            if (winner != run) throw new InvalidOperationException("TIMEOUT. Output:\n" + sw);
            await run;
        }
        finally
        {
            Console.SetOut(originalOut);
        }

        var output = sw.ToString();
        // §6.1 Variables-Scope-Spec: il contatore FOR è block-scoped (shadowing); dopo il
        // ciclo la DIM esterna M=42 è ripristinata — NON "End+Step" del BASIC classico.
        Assert.Contains("SUCCESSO", output);
        Assert.Matches(@"atteso 42, la DIM esterna\):\s*42", output);
    }

    [Fact]
    public void DimM_ForM1to3_InSub_Restores_DimValue_PCode()
    {
        // Bug FOR-shadowing 2026-09-30: in SUB lo shadowing non avveniva su PCodeVM/VM-C
        // (slot condiviso → M=4). Il sample copre solo il top-level: qui il caso SUB,
        // in-process via PCodeVmRunner (stesso pattern di BlackboardWaitParkPCodeTests).
        var directory = Path.Combine(Path.GetTempPath(), $"ForShadow_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        var sourcePath = Path.Combine(directory, "Main.bas");
        File.WriteAllText(sourcePath, """
            SUB Main()
              DIM M = 42
              FOR M = 1 TO 3
              NEXT M
              IF M = 42 THEN PRINT "SUB-SHADOW-OK" ELSE PRINT "SUB-SHADOW-KO"
            END SUB
            """);

        var previousDirectory = Directory.GetCurrentDirectory();
        try
        {
            Directory.SetCurrentDirectory(directory);
            C99Emission.Require(sourcePath);
            var program = Basic.Orchestrator.Compile(sourcePath, null, false);
            var chunk = new global::Compiler.Compiler().Compile(program);
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(30));

            var result = Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk, cts.Token);

            Assert.True(result.Success, result.Output);
            Assert.Contains("SUB-SHADOW-OK", result.Output);
            Assert.DoesNotContain("SUB-SHADOW-KO", result.Output);
        }
        finally
        {
            Directory.SetCurrentDirectory(previousDirectory);
            Directory.Delete(directory, recursive: true);
        }
    }
}
