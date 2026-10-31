// HTTP + GPM su ILBackend: il server HTTP esegue gli handler come task GPM (parità PCodeVM §11.5).
// Un handler may-park (SLEEP + GO SUB + canale) deve rispondere correttamente. Il listener non
// esegue BASIC inline: l'handler parcheggia sul suo scheduler e la risposta arriva al resume.
// Sample: samples/Network/HttpGpmHandler.bas (porta 8156, risposta "GPM-OK").

using System.Reflection;
using Xunit;
using Basic.IlBackend;
using Basic.IlBackend.PCode;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class HttpGpmTests
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

    [Fact]
    public void HttpServer_MayParkHandler_RespondsUnderGpm()
    {
        var sample = Path.Combine(Root, "samples", "Community", "Network", "HttpGpmHandler.bas");
        Assert.True(File.Exists(sample), $"Sample mancante: {sample}");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, "HttpGpm_" + Guid.NewGuid().ToString("N"), out _);
        using var ms = new MemoryStream();
        asmBuilder.Save(ms);
        var assembly = Assembly.Load(ms.ToArray());
        var main = assembly.GetType("GeneratedProgram")!
            .GetMethod("Main", BindingFlags.Public | BindingFlags.Static)
            ?? throw new InvalidOperationException("Main non trovato.");

        var originalOut = Console.Out;
        var sw = new StringWriter();
        var runTask = Task.Run(() =>
        {
            Console.SetOut(sw);
            Basic.IlBackend.IlRuntime.SetCurrent(
                new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            try { main.Invoke(null, null); }
            catch (TargetInvocationException tie) { Console.WriteLine("ERR: " + (tie.InnerException ?? tie).Message); }
            catch (Exception ex) { Console.WriteLine("ERR: " + ex.Message); }
            finally { Console.SetOut(originalOut); }
        });

        // Poll dell'endpoint: il server parte in background (HTTP SERVER), poi l'handler
        // may-park (SLEEP 1500 + GO SUB + canale) risponde "GPM-OK".
        string body = "";
        var deadline = DateTime.UtcNow.AddSeconds(20);
        using var client = new HttpClient();
        while (DateTime.UtcNow < deadline)
        {
            try
            {
                var resp = client.GetAsync("http://127.0.0.1:8156/test").GetAwaiter().GetResult();
                body = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
                if (body.Contains("GPM-OK")) break;
            }
            catch { /* server non ancora pronto */ }
            Thread.Sleep(300);
        }

        Assert.Contains("GPM-OK", body);
        Assert.DoesNotContain("ERR:", sw.ToString());
    }

    [Fact]
    public void HttpServer_MayParkHandler_RespondsUnderPCodeVm()
    {
        // Stesso sample sulla PCodeVM: l'handler gira come task GPM sullo scheduler condiviso
        // (SpawnHttpHandlerTask) e risponde "GPM-OK-from-goroutine".
        var sample = Path.Combine(Root, "samples", "Community", "Network", "HttpGpmHandler.bas");
        Assert.True(File.Exists(sample), $"Sample mancante: {sample}");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var chunk = new global::Compiler.Compiler().Compile(program);

        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        var runTask = Task.Run(() => PCodeVmRunner.Run(chunk, cts.Token));

        string body = "";
        var deadline = DateTime.UtcNow.AddSeconds(20);
        using var client = new HttpClient();
        while (DateTime.UtcNow < deadline)
        {
            try
            {
                var resp = client.GetAsync("http://127.0.0.1:8156/test").GetAwaiter().GetResult();
                body = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
                if (body.Contains("GPM-OK")) break;
            }
            catch { /* server non ancora pronto */ }
            Thread.Sleep(300);
        }

        Assert.Contains("GPM-OK", body);
    }
}
