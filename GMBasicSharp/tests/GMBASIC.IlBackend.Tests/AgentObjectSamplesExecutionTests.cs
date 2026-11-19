// AI Agents A1 (2026-09-02): CLASSE AGENT wrapper (include/Agent.inc) — property access
// write-through su un agente (gap UDT property access). Compila ed ESEGUE il sample
// samples/Ai/AgentObject.bas sul backend IL e verifica i marker in output:
// SET via property scrive sul record reale (riletto con AI.AGENTGET = Cross*) e GET rilegge.
using System.Reflection;
using System.Text;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class AgentObjectSamplesExecutionTests
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

    private static string RunSample(string sample)
    {
        C99Emission.Require(sample);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "A1Agent_" + Guid.NewGuid().ToString("N"), out _);
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
            if (!task.Wait(TimeSpan.FromSeconds(20)))
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
    public void AgentObject_Class_Wrapper_PropertyAccess_Runs()
    {
        var output = RunSample(Path.Combine(Root, "shared", "samples", "Ai", "AgentObject.bas"));
        // SET via property (write-through) + GET via property
        Assert.Contains("Model=qwen3.5-probe", output);
        Assert.Contains("Temp= 0.42", output);
        Assert.Contains("MaxTokens= 512", output);
        Assert.Contains("Provider=fixture", output);
        Assert.Contains("Memory=ON", output);
        // Verifica cross: il SET via property ha scritto DAVVERO sul record (riletto con AI.AGENTGET)
        Assert.Contains("CrossModel=qwen3.5-probe", output);
        Assert.Contains("CrossTemp=0.42", output);
        Assert.Contains("CrossMemory=ON", output);
        Assert.Contains("AGENT-OBJECT-OK", output);
    }
}
