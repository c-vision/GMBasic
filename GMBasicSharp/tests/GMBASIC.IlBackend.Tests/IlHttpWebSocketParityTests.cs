// Verifica dei gap HTTP/FETCH/WebSocket chiusi sul backend IL + PCodeVM interno:
//  1) PCodeVM: implicit-await INLINE su FETCH(url).Campo (prima solo via variabile) — via &/EVAL.
//  2) WEBSOCKET CONNECT (client) con handshake RFC 6455 reale + OnMessage (prima: socket TCP nudo).
//  3) WebSocket SERVER su HttpService (upgrade HTTP + handler a 3 parametri req/res/ws + loop).
//  4) HTTP CONFIG applicato (timeout/rate-limit/max-body/keep-alive — prima no-op in IL).
using System.Net;
using System.Net.WebSockets;
using System.Reflection;
using System.Text;
using System.Text.Json;
using Xunit;
using Basic.IlBackend;
using Basic.Runtime;

namespace Basic.IlBackend.Tests;

[Collection("GpmRuntimeState")]
public class IlHttpWebSocketParityTests
{
    private static int NextPort()
    {
        var l = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, 0);
        l.Start();
        var port = ((System.Net.IPEndPoint)l.LocalEndpoint).Port;
        l.Stop();
        return port;
    }

    // ─── utilità: compila + esegue un sample IL, catturando l'output ────────────────
    private static string RunCompiledSample(string source, string prefix) => RunCompiledSample(source, prefix, runInline: false);

    private static string RunCompiledSample(string source, string prefix, bool runInline)
    {
        var sample = Path.Combine(Path.GetTempPath(), prefix + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(sample, source);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var asmBuilder = IlProgramCompiler.CompileToAssembly(program, prefix + Guid.NewGuid().ToString("N"), out _);
        var dumpDir = Path.Combine(Path.GetTempPath(), "gmb_wsverify");
        Directory.CreateDirectory(dumpDir);
        var dll = Path.Combine(dumpDir, prefix.TrimEnd('_') + ".dll");
        using (var fs = File.Create(dll)) asmBuilder.Save(fs);
        var assembly = Assembly.LoadFile(dll);
        var programType = assembly.GetType("GeneratedProgram")!;
        var main = programType.GetMethod("Main", BindingFlags.Public | BindingFlags.Static)!;

        var originalOut = Console.Out;
        var sw = new StringWriter();
        try
        {
            Console.SetOut(sw);
            // Isola il bag GLOBAL/statico del run precedente (stesso processo, altri .dll LoadFile):
            // senza, un simbolId globale (es. 514 per `ws`) può ereditare il valore di un ALTRO
            // programma eseguito prima sullo stesso processo (es. un Int16) → handle mai salvato.
            IlRuntime.ResetProgramState();
            IlRuntime.SetCurrent(new HeadlessRuntimeHost(Console.Out, new Runtime.RuntimeConfig()));
            if (runInline)
            {
                // Esegue Main sullo STESSO thread che ha chiamato SetCurrent: _current (thread-local)
                // è il nostro host, senza la race sul _processWideFallback (che un altro test
                // in-process può sovrascrivere da un thread di pool). Il programma è bounded
                // (SLEEP + HTTP SERVER STOP), quindi il blocco è controllato.
                main.Invoke(null, null);
            }
            else
            {
                var task = Task.Run(() => main.Invoke(null, null));
                Assert.True(task.Wait(TimeSpan.FromSeconds(25)), "driver TIMEOUT\n" + sw);
                task.GetAwaiter().GetResult();
            }
        }
        finally
        {
            Console.SetOut(originalOut);
        }
        return sw.ToString();
    }

    // ─── 1) PCodeVM: implicit-await inline su FETCH(url).Campo via &/EVAL ───────────
    [Fact]
    public void PCodeVm_FetchInline_FieldAccess_Awaits()
    {
        var port = NextPort();
        using var server = new HttpListener();
        server.Prefixes.Add($"http://localhost:{port}/");
        server.Start();
        _ = Task.Run(() =>
        {
            while (server.IsListening)
            {
                try
                {
                    var ctx = server.GetContext();
                    var bytes = Encoding.UTF8.GetBytes("ok");
                    ctx.Response.StatusCode = 200;
                    ctx.Response.ContentLength64 = bytes.Length;
                    ctx.Response.OutputStream.Write(bytes, 0, bytes.Length);
                    ctx.Response.Close();
                }
                catch { break; }
            }
        });

        // Programma completo eseguito sulla PCodeVM interna (PCodeVmRunner): l'accesso INLINE
        // FETCH(url$).StatusCode senza variabile intermedia deve parcheggiare finché il future
        // non risolve (prima restituiva null perché l'istanza futura non veniva mai attesa).
        var sample = Path.Combine(Path.GetTempPath(), "vmfetch_inline_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(sample,
            $"DIM url$ = \"http://localhost:{port}/x\"\n" +
            "DIM res = FETCH(url$)\n" +
            "PRINT \"VIAVAR:\"; res.StatusCode\n" +
            "PRINT \"INLINE:\"; FETCH(url$).StatusCode\n");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        Assert.NotNull(program?.MainModule);
        var chunk = new global::Compiler.Compiler().Compile(program!);
        var result = global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk);
        Assert.Contains("VIAVAR: 200", result.Output);
        Assert.Contains("INLINE: 200", result.Output);
    }

    // ─── 2) WEBSOCKET CONNECT client: handshake reale + OnMessage ───────────────────
    [Fact]
    public void WsClient_RealHandshake_Send_OnMessage()
    {
        var port = NextPort();
        using var server = new HttpService();
        var h = server.HttpServerStart(port);
        Assert.True(h >= 0, "server non avviato");
        server.HttpAddWebSocketRoute(h, "/ws", (req, ws) => Task.Run(async () =>
        {
            var buf = new byte[8192];
            var sb = new StringBuilder();
            while (ws.State == WebSocketState.Open)
            {
                var r = await ws.ReceiveAsync(new ArraySegment<byte>(buf), CancellationToken.None);
                if (r.MessageType == WebSocketMessageType.Close)
                {
                    await ws.CloseOutputAsync(WebSocketCloseStatus.NormalClosure, "", CancellationToken.None);
                    break;
                }
                sb.Append(Encoding.UTF8.GetString(buf, 0, r.Count));
                if (r.EndOfMessage)
                {
                    await ws.SendAsync(Encoding.UTF8.GetBytes("Echo: " + sb), WebSocketMessageType.Text, true, CancellationToken.None);
                    sb.Clear();
                }
            }
        }));

        var output = RunCompiledSample(
            "DIM ws AS WEBSOCKET\n" +
            $"WEBSOCKET CONNECT \"localhost\", {port}, ws, \"/ws\"\n" +
            "ws.OnMessage = \"ClientOnMsg\"\n" +
            "ws.Send(\"ping\")\n" +
            "SLEEP 2000\n" +
            "ws.Close()\n" +
            "SUB ClientOnMsg(ws AS WEBSOCKET, msg$)\n" +
            "  PRINT \"RECEIVED:\"; msg$\n" +
            "END SUB\n", "IlWsClient_");
        Assert.Contains("RECEIVED:Echo: ping", output);

        Assert.Contains("RECEIVED:Echo: ping", output);
    }

    [Fact]
    public async Task WsClient_GpmCancellation_SendsCleanClose()
    {
        var port = NextPort();
        var connected = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        var closed = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        using var server = new HttpService();
        var h = server.HttpServerStart(port);
        Assert.True(h >= 0, "server non avviato");
        server.HttpAddWebSocketRoute(h, "/ws", async (req, ws) =>
        {
            connected.TrySetResult();
            var buffer = new byte[1024];
            try
            {
                while (ws.State == WebSocketState.Open)
                {
                    var result = await ws.ReceiveAsync(buffer, CancellationToken.None);
                    if (result.MessageType != WebSocketMessageType.Close) continue;
                    await ws.CloseOutputAsync(WebSocketCloseStatus.NormalClosure, "", CancellationToken.None);
                    closed.TrySetResult();
                    return;
                }
            }
            catch (Exception ex) { closed.TrySetException(ex); }
        });

        var sample = Path.Combine(Path.GetTempPath(), "vmws_cancel_" + Guid.NewGuid().ToString("N") + ".bas");
        File.WriteAllText(sample,
            "DIM ws AS WEBSOCKET\n" +
            $"WEBSOCKET CONNECT \"localhost\", {port}, ws, \"/ws\"\n" +
            "SLEEP 60000\n");
        var program = Basic.Orchestrator.Compile(sample, null, false);
        Assert.NotNull(program?.MainModule);
        var chunk = new global::Compiler.Compiler().Compile(program!);
        using var cancellation = new CancellationTokenSource();
        var run = Task.Run(() => global::Basic.IlBackend.PCode.PCodeVmRunner.Run(chunk, cancellation.Token));

        await connected.Task.WaitAsync(TimeSpan.FromSeconds(10));
        cancellation.Cancel();
        await run.WaitAsync(TimeSpan.FromSeconds(10));
        await closed.Task.WaitAsync(TimeSpan.FromSeconds(10));
    }

    // ─── 3) WebSocket SERVER su HttpService (route a 3 parametri req/res/ws) ────────
    [Fact]
    public void WsServer_Upgrade_Handler_OnMessage()
    {
        // La connessione ClientWebSocket ↔ server HttpListener in-process ha flake note del
        // WebSocket managed .NET (soprattutto se altri test hanno appena saturato il threadpool,
        // ritardando l'avvio del server nel programma BASIC): si riprova su porta fresca.
        for (var attempt = 0; attempt < 3; attempt++)
        {
            var port = NextPort();
            try
            {
                RunWsServerScenario(port);
                return;
            }
            catch (System.Net.WebSockets.WebSocketException wex)
                when (wex.Message.Contains("compressed frame", StringComparison.OrdinalIgnoreCase)
                   || wex.Message.Contains("Unable to connect", StringComparison.OrdinalIgnoreCase)
                   || wex.Message.Contains("Connection refused", StringComparison.OrdinalIgnoreCase))
            {
                // flake nota / avvio lento del server: riprova
            }
        }
        RunWsServerScenario(NextPort()); // se la flake persiste, fallisce qui con l'errore reale
    }

    private static void RunWsServerScenario(int port)
    {
        var source =
            "IMPORT HTTP\n" +
            "IMPORT CONCURRENCY\n" +
            "SUB ServerOnMsg(ws AS WEBSOCKET, msg$)\n" +
            "  ws.Send(\"Echo: \" + msg$)\n" +
            "END SUB\n" +
            "SUB WsHandler(req AS HTTPREQUEST, res AS HTTPRESPONSE, ws AS WEBSOCKET)\n" +
            "  ws.OnMessage = \"ServerOnMsg\"\n" +
            "  ws.Send(\"Benvenuto\")\n" +
            "END SUB\n" +
            $"HTTP ROUTE \"GET\", \"/ws\", WsHandler()\n" +
            $"HTTP SERVER {port}\n" +
            "SLEEP 20000\n" +
            "HTTP SERVER STOP\n";

        // Il server gira in un SOTTOPROCESSO (eseguibile compilato): un processo PULITO, non
        // inquinato dallo stato lasciato da altri test in-process (dopo ClipperSamples
        // l'HttpListener condiviso non accetta più connessioni nello stesso processo — un
        // limite dell'ambiente di test, non dell'implementazione; WebSocketEcho.bas via
        // ILBackend funziona end-to-end in un processo reale).
        var sampleDir = Path.Combine(Path.GetTempPath(), "gmwssrv_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sampleDir);
        var sample = Path.Combine(sampleDir, "IlWsServer.bas");
        File.WriteAllText(sample, source);
        var program = Basic.Orchestrator.Compile(sample, null, false);
        var exePath = IlProgramCompiler.Compile(program!, sample, sampleDir, "IlWsServer", rid: "osx-x64", linkRuntime: true);

        using var proc = new System.Diagnostics.Process
        {
            StartInfo = new System.Diagnostics.ProcessStartInfo(exePath)
            {
                WorkingDirectory = sampleDir,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
            }
        };
        proc.Start();

        try
        {
            using var client = new ClientWebSocket();
            // Attende il server (polling, IPv4/IPv6 loopback) — finestra 20s del server.
            var deadline = DateTime.UtcNow.AddSeconds(20);
            var up = false;
            while (DateTime.UtcNow < deadline)
            {
                try
                {
                    using var probe = new System.Net.Sockets.TcpClient();
                    try { probe.Connect("127.0.0.1", port); }
                    catch { probe.Connect(System.Net.IPAddress.IPv6Loopback, port); }
                    up = true;
                    break;
                }
                catch { Thread.Sleep(200); }
            }
            Assert.True(up, "server non raggiungibile (subprocess)");

            client.ConnectAsync(new Uri($"ws://localhost:{port}/ws"), CancellationToken.None).GetAwaiter().GetResult();
            Assert.Equal("Benvenuto", ReceiveText(client));
            client.SendAsync(Encoding.UTF8.GetBytes("ciao"), WebSocketMessageType.Text, true, CancellationToken.None).GetAwaiter().GetResult();
            Assert.Equal("Echo: ciao", ReceiveText(client));
            client.CloseAsync(WebSocketCloseStatus.NormalClosure, "done", CancellationToken.None).GetAwaiter().GetResult();
        }
        finally
        {
            try { if (!proc.HasExited) proc.Kill(); } catch { }
            try { proc.WaitForExit(5000); } catch { }
            try { Directory.Delete(sampleDir, recursive: true); } catch { }
        }
    }

    private static string ReceiveText(ClientWebSocket ws)
    {
        var buf = new byte[8192];
        var sb = new StringBuilder();
        while (true)
        {
            var r = ws.ReceiveAsync(new ArraySegment<byte>(buf), CancellationToken.None).GetAwaiter().GetResult();
            if (r.MessageType == WebSocketMessageType.Close) break;
            sb.Append(Encoding.UTF8.GetString(buf, 0, r.Count));
            if (r.EndOfMessage) break;
        }
        return sb.ToString();
    }

    [Fact]
    public void Brt_HttpService_WebSocket_Upgrade_Echo()
    {
        var port = NextPort();
        using var server = new HttpService();
        var h = server.HttpServerStart(port);
        Assert.True(h >= 0, "server non avviato");
        server.HttpAddWebSocketRoute(h, "/ws", (req, ws) => Task.Run(async () =>
        {
            var buf = new byte[8192];
            var sb = new StringBuilder();
            while (ws.State == WebSocketState.Open)
            {
                var r = await ws.ReceiveAsync(new ArraySegment<byte>(buf), CancellationToken.None);
                if (r.MessageType == WebSocketMessageType.Close)
                {
                    await ws.CloseOutputAsync(WebSocketCloseStatus.NormalClosure, "", CancellationToken.None);
                    break;
                }
                sb.Append(Encoding.UTF8.GetString(buf, 0, r.Count));
                if (r.EndOfMessage)
                {
                    await ws.SendAsync(Encoding.UTF8.GetBytes("Echo: " + sb), WebSocketMessageType.Text, true, CancellationToken.None);
                    sb.Clear();
                }
            }
        }));

        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        using var client = new ClientWebSocket();
        client.ConnectAsync(new Uri($"ws://localhost:{port}/ws"), cts.Token).GetAwaiter().GetResult();
        client.SendAsync(Encoding.UTF8.GetBytes("pi"), WebSocketMessageType.Text, true, cts.Token).GetAwaiter().GetResult();
        var got = ReceiveText(client);
        Assert.Equal("Echo: pi", got);
        client.CloseAsync(WebSocketCloseStatus.NormalClosure, "ok", CancellationToken.None).GetAwaiter().GetResult();
    }

    // ─── 4) HTTP CONFIG: rate-limit e max-body applicati (prima no-op in IL) ─────────
    [Fact]
    public void HttpConfig_RateLimit_And_MaxBody_Enforced()
    {
        var port = NextPort();
        using var server = new HttpService();
        var h = server.HttpServerStart(port);
        Assert.True(h >= 0, "server non avviato");
        server.HttpConfigure(h, new HttpServerSettings { RateLimitRequestsPerMinute = 1, RateLimitBurstSize = 1, MaxRequestBodySize = 16 });
        server.HttpAddRoute(h, "GET", "/ok", _ => new HttpResponse { StatusCode = 200, Body = "ok" });

        string Get(string path, byte[]? body)
        {
            using var wc = new HttpClient();
            var req = new HttpRequestMessage(HttpMethod.Get, $"http://localhost:{port}{path}");
            if (body != null) req.Content = new ByteArrayContent(body);
            var resp = wc.SendAsync(req).GetAwaiter().GetResult();
            return ((int)resp.StatusCode).ToString();
        }

        // max body: GET con 100 byte di body → 413 (il check è su ContentLength64)
        Assert.Equal("413", Get("/x", new byte[100]));
        // primo GET su rotta reale → 200
        Assert.Equal("200", Get("/ok", null));
        // secondo GET immediato → rate limit → 429
        Assert.Equal("429", Get("/ok", null));
    }

    [Fact]
    public void HttpConfig_RequestTimeout_Enforced()
    {
        var port = NextPort();
        using var server = new HttpService();
        var h = server.HttpServerStart(port);
        Assert.True(h >= 0, "server non avviato");
        server.HttpConfigure(h, new HttpServerSettings { RequestTimeoutMs = 300 });

        // route che dorme 2s: il timeout di 300ms deve far rispondere 503
        server.HttpAddRoute(h, "GET", "/slow", _ => { Thread.Sleep(2000); return new HttpResponse { StatusCode = 200, Body = "tardi" }; });

        using var wc = new HttpClient();
        var resp = wc.GetAsync($"http://localhost:{port}/slow").GetAwaiter().GetResult();
        Assert.Equal(System.Net.HttpStatusCode.ServiceUnavailable, resp.StatusCode);
    }
}
