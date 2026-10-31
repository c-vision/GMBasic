// FILE: Mcp.Tests.McpHttpListenerTests
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using Basic.Mcp;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class McpHttpListenerTests : IAsyncLifetime
{
    private readonly StubToolDispatcher _dispatcher = new();
    private readonly CapturingAuditSink _auditSink = new();
    private CancellationTokenSource? _hostCts;
    private McpHttpListenerHost? _host;

    public Task InitializeAsync()
    {
        McpToolRegistry.ResetForTests();
        McpRuntimeBridge.ResetForTests();
        McpRuntimeBridge.Dispatcher = _dispatcher;
        return Task.CompletedTask;
    }

    public async Task DisposeAsync()
    {
        _hostCts?.Cancel();
        if (_host is not null)
            await _host.DisposeAsync();
        McpToolRegistry.ResetForTests();
        McpRuntimeBridge.ResetForTests();
    }

    [Fact]
    public async Task Http_RejectsMissingApiKey_AndAuditsFailure()
    {
        await StartHostAsync("test-key");
        using var client = new HttpClient();
        var response = await client.PostAsync(
            BuildUrl("/mcp"),
            new StringContent("""{"jsonrpc":"2.0","id":1,"method":"ping"}""", Encoding.UTF8, "application/json"));

        response.StatusCode.Should().Be(HttpStatusCode.Unauthorized);
        _auditSink.Entries.Should().ContainSingle(e =>
            e.EventType == "auth_failure" && e.Success == false && e.Detail == "missing_api_key");
    }

    [Fact]
    public async Task Http_AcceptsValidApiKey_AndReturnsInitializeResult()
    {
        await StartHostAsync("test-key");
        using var client = new HttpClient();
        client.DefaultRequestHeaders.Add("Authorization", "Bearer test-key");

        var response = await client.PostAsync(
            BuildUrl("/mcp"),
            new StringContent(
                """{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"xunit"}}}""",
                Encoding.UTF8,
                "application/json"));

        response.StatusCode.Should().Be(HttpStatusCode.OK);
        var body = await response.Content.ReadAsStringAsync();
        using var doc = JsonDocument.Parse(body);
        doc.RootElement.GetProperty("result").GetProperty("protocolVersion").GetString()
            .Should().Be("2024-11-05");
    }

    [Fact]
    public async Task Http_ToolsCall_RecordsAuditEntry()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "echo",
            QualifiedProcedureName = "Echo",
            InputSchema = McpSchemaBuilder.FromParameters([("text", "STRING", false)])
        });

        await StartHostAsync("test-key");
        using var client = new HttpClient();
        client.DefaultRequestHeaders.Add("X-API-Key", "test-key");

        await client.PostAsync(
            BuildUrl("/mcp"),
            new StringContent(
                """{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"xunit"}}}""",
                Encoding.UTF8,
                "application/json"));

        await client.PostAsync(
            BuildUrl("/mcp"),
            new StringContent(
                """{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"echo","arguments":{"text":"hi"}}}""",
                Encoding.UTF8,
                "application/json"));

        _auditSink.Entries.Should().Contain(e =>
            e.EventType == "tool_call" && e.ToolName == "echo" && e.Success);
    }

    private async Task StartHostAsync(string apiKey)
    {
        // La porta scelta con GetFreePort() può essere occupata da un altro processo/test tra il
        // rilascio e il bind (race classica). Riprova su porte diverse invece di un timeout cieco.
        var lastError = "nessun tentativo";
        for (var attempt = 0; attempt < 5; attempt++)
        {
            var port = GetFreePort();
            var audit = new McpAuditLog(_auditSink);
            var server = new McpServer(dispatcher: _dispatcher, audit: audit);
            var cts = new CancellationTokenSource();
            var host = new McpHttpListenerHost(server, new McpHttpOptions
            {
                Port = port,
                ApiKey = apiKey,
                RequestsPerMinutePerIp = 1000
            }, audit);

            var runTask = Task.Run(() => host.RunAsync(cts.Token), CancellationToken.None);
            try
            {
                await WaitForListenerAsync(host.Prefix);
            }
            catch (TimeoutException)
            {
                // Il listener non è partito: o la porta era occupata (HttpListenerException in
                // RunAsync, task morto) o bind lento. Libera e riprova con una porta nuova.
                cts.Cancel();
                await host.DisposeAsync();
                try { await runTask; } catch { /* errore di bind: atteso, si riprova */ }
                lastError = $"porta {port} non disponibile";
                continue;
            }
            _hostCts = cts;
            _host = host;
            return;
        }
        throw new TimeoutException($"HTTP MCP listener non è partito dopo 5 porte: {lastError}");
    }

    private string BuildUrl(string path) => $"{_host!.Prefix.TrimEnd('/')}{path}";

    private static int GetFreePort()
    {
        using var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        var port = ((IPEndPoint)listener.LocalEndpoint).Port;
        listener.Stop();
        return port;
    }

    private static async Task WaitForListenerAsync(string prefix)
    {
        using var client = new HttpClient { Timeout = TimeSpan.FromMilliseconds(500) };
        for (var i = 0; i < 40; i++)
        {
            try
            {
                await client.GetAsync(prefix);
                return;
            }
            catch
            {
                await Task.Delay(15);
            }
        }

        throw new TimeoutException($"HTTP MCP listener did not start at {prefix}");
    }
}
