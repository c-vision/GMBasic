// Verifica IlBackend GPM: FETCH forma espressione asincrona (§E.5, modello PCodeVM).
// Il task fa x = FETCH(...) dentro una goroutine: NON deve bloccare gli altri task
// (GOMAXPROCS=1) e al resume il valore (FetchResult) arriva correttamente.
// Self-contained: i sample sono GENERATI qui (non più fixture /tmp/vmfetch_il*.bas
// sparite) e il 404 arriva da un HttpListener locale.
using System.Reflection;
using Xunit;
using Basic.IlBackend;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class IlHttpFetchTests
{
    private static int NextPort()
    {
        var l = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        l.Start();
        var port = ((System.Net.IPEndPoint)l.LocalEndpoint).Port;
        l.Stop();
        return port;
    }

    [Fact]
    public void FetchExpression_Async_In_Goroutine_Resumes_With_Value()
    {
        var port = NextPort();
        using var server = new System.Net.HttpListener();
        server.Prefixes.Add($"http://127.0.0.1:{port}/");
        server.Start();
        _ = Task.Run(() =>
        {
            while (server.IsListening)
            {
                try
                {
                    var ctx = server.GetContext();
                    var bytes = System.Text.Encoding.UTF8.GetBytes("missing");
                    ctx.Response.StatusCode = 404;
                    ctx.Response.ContentLength64 = bytes.Length;
                    ctx.Response.OutputStream.Write(bytes, 0, bytes.Length);
                    ctx.Response.Close();
                }
                catch { break; }
            }
        });
        var source =
            $"IMPORT HTTP\n" +
            $"SUB Worker(url$ AS STRING)\n" +
            $"  DIM res = FETCH(url$)\n" +
            $"  PRINT \"[FETCH] status= \"; res.StatusCode\n" +
            $"END SUB\n" +
            $"DIM url$ = \"http://127.0.0.1:{port}/missing\"\n" +
            $"PRINT \"[MAIN] dopo GO SUB (non bloccante)\"\n" +
            $"GO SUB Worker(url$)\n" +
            $"SLEEP 1500\n" +
            $"PRINT \"[MAIN] fine\"\n";
        var output = RunCompiledSample(source, "IlHttpFetch_Async_");
        Assert.Contains("[MAIN] dopo GO SUB (non bloccante)", output);
        Assert.Contains("[FETCH] status=  404", output);
        Assert.Contains("[MAIN] fine", output);
    }

    [Fact]
    public void FetchExpression_Sync_NoGoroutine_Returns_StatusCode()
    {
        var port = NextPort();
        using var server = new System.Net.HttpListener();
        server.Prefixes.Add($"http://127.0.0.1:{port}/");
        server.Start();
        _ = Task.Run(() =>
        {
            while (server.IsListening)
            {
                try
                {
                    var ctx = server.GetContext();
                    var bytes = System.Text.Encoding.UTF8.GetBytes("missing");
                    ctx.Response.StatusCode = 404;
                    ctx.Response.ContentLength64 = bytes.Length;
                    ctx.Response.OutputStream.Write(bytes, 0, bytes.Length);
                    ctx.Response.Close();
                }
                catch { break; }
            }
        });
        var source =
            $"IMPORT HTTP\n" +
            $"DIM url$ = \"http://127.0.0.1:{port}/missing\"\n" +
            $"DIM res = FETCH(url$)\n" +
            $"PRINT \"[SYNC] status= \"; res.StatusCode\n";
        var output = RunCompiledSample(source, "IlHttpFetch_Sync_");
        Assert.Contains("[SYNC] status=  404", output);
    }

    private static string RunCompiledSample(string source, string assemblyPrefix)
    {
        var sample = Path.Combine(Path.GetTempPath(), assemblyPrefix.TrimEnd('_') + "_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(sample, source);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, assemblyPrefix + Guid.NewGuid().ToString("N"), out _);
        var dumpDir = Path.Combine(Path.GetTempPath(), "gmb_ilfetch");
        Directory.CreateDirectory(dumpDir);
        var dll = Path.Combine(dumpDir, assemblyPrefix.TrimEnd('_') + ".dll");
        using (var fs = File.Create(dll)) asmBuilder.Save(fs);
        Basic.IlBackend.IlDisassembler.Dump(dll, Path.Combine(dumpDir, Path.GetFileNameWithoutExtension(dll) + ".il"));
        var assembly = Assembly.LoadFile(dll);
        var programType = assembly.GetType("GeneratedProgram")!;
        var main = programType.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)!;

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            IlRuntime.ResetProgramState();
            Basic.IlBackend.IlRuntime.SetCurrent(new Runtime.HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            var task = Task.Run(() => main.Invoke(null, null));
            Assert.True(task.Wait(TimeSpan.FromSeconds(20)), "driver TIMEOUT\n" + sw);
            task.GetAwaiter().GetResult();
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return sw.ToString();
    }

    [Fact]
    public void FetchOptions_HeadersPropagated_From_HttpHeaders_Class()
    {
        // Regression test for bug "(solved) HttpFetch-Options-Headers-Non-Propagati":
        // opts.Headers.Authorization = "Bearer ..." on a NEW HTTPOPTIONS must reach the server.
        var port = NextPort();
        string receivedAuth = "";
        using var server = new System.Net.HttpListener();
        server.Prefixes.Add($"http://127.0.0.1:{port}/");
        server.Start();
        _ = Task.Run(() =>
        {
            while (server.IsListening)
            {
                try
                {
                    var ctx = server.GetContext();
                    receivedAuth = ctx.Request.Headers["Authorization"] ?? "";
                    var bytes = System.Text.Encoding.UTF8.GetBytes("ok");
                    ctx.Response.StatusCode = 200;
                    ctx.Response.ContentLength64 = bytes.Length;
                    ctx.Response.OutputStream.Write(bytes, 0, bytes.Length);
                    ctx.Response.Close();
                }
                catch { break; }
            }
        });
        var source =
            $"IMPORT HTTP\n" +
            $"DIM url$ = \"http://127.0.0.1:{port}/\"\n" +
            $"DIM o2 = NEW HTTPOPTIONS\n" +
            $"o2.Headers.Authorization = \"Bearer mytoken\"\n" +
            $"DIM res = FETCH(url$, o2)\n" +
            $"PRINT \"[HEADERS] status= \"; res.StatusCode\n";
        var output = RunCompiledSample(source, "IlHttpFetch_Headers_");
        Assert.Contains("[HEADERS] status=  200", output);
        Assert.Equal("Bearer mytoken", receivedAuth);
    }

    [Fact]
    public void FetchOptions_BearerToken_Propagated()
    {
        // Regression test: opts.BearerToken = token$ must set Authorization header.
        var port = NextPort();
        string receivedAuth = "";
        using var server = new System.Net.HttpListener();
        server.Prefixes.Add($"http://127.0.0.1:{port}/");
        server.Start();
        _ = Task.Run(() =>
        {
            while (server.IsListening)
            {
                try
                {
                    var ctx = server.GetContext();
                    receivedAuth = ctx.Request.Headers["Authorization"] ?? "";
                    var bytes = System.Text.Encoding.UTF8.GetBytes("ok");
                    ctx.Response.StatusCode = 200;
                    ctx.Response.ContentLength64 = bytes.Length;
                    ctx.Response.OutputStream.Write(bytes, 0, bytes.Length);
                    ctx.Response.Close();
                }
                catch { break; }
            }
        });
        var source =
            $"IMPORT HTTP\n" +
            $"DIM url$ = \"http://127.0.0.1:{port}/\"\n" +
            $"DIM o2 = NEW HTTPOPTIONS\n" +
            $"o2.BearerToken = \"mytoken\"\n" +
            $"DIM res = FETCH(url$, o2)\n" +
            $"PRINT \"[BT] status= \"; res.StatusCode\n";
        var output = RunCompiledSample(source, "IlHttpFetch_BearerToken_");
        Assert.Contains("[BT] status=  200", output);
        Assert.Equal("Bearer mytoken", receivedAuth);
    }
}
