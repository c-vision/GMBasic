// AI-Provider-Register-Unified (2026-09-03): namespace PROVIDER fuso in AI.
// Verifica che i sample migrati da PROVIDER.* a AI.* COMPILINO ed ESEGUANO sul backend IL,
// confermando che le funzioni AI.REGISTER/SETACTIVE/ACTIVE/PRIORITY/FAIL/RESETFAILED/...
// (ex PROVIDER.*) hanno la stessa semantica con namespace AI unico.
using System.Reflection;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class AiProviderUnifiedSamplesExecutionTests
{
    private static readonly string Root = FindRepoRoot();

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

    private static string RunSample(string sample)
    {
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "AiProv_" + Guid.NewGuid().ToString("N"), out _);
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
            if (!task.Wait(TimeSpan.FromSeconds(30)))
                throw new InvalidOperationException("TIMEOUT. Output:\n" + sw);
            task.GetAwaiter().GetResult();
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return sw.ToString();
    }

    [Fact]
    public void ProviderFailover_AiNamespace_Runs()
    {
        var output = RunSample(Path.Combine(Root, "samples", "Community", "Ai", "ProviderFailover.bas"));
        Assert.Contains("Registrato primary-emb", output);
        Assert.Contains("Test Completato", output);
    }

    [Fact]
    public void ProviderConfigure_AiNamespace_Runs()
    {
        var output = RunSample(Path.Combine(Root, "samples", "Community", "Ai", "ProviderConfigure.bas"));
        Assert.Contains("Attivo finale", output);
    }

    [Fact]
    public void ProviderStatus_AiNamespace_Runs()
    {
        var output = RunSample(Path.Combine(Root, "samples", "Community", "Ai", "ProviderStatus.bas"));
        Assert.Contains("Test Completato", output);
    }
}
