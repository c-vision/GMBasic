// FILE: Mcp.Tests.McpServerTests
using System.Text.Json;
using System.Text.Json.Nodes;
using Basic.Mcp;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class McpServerTests : IDisposable
{
    private readonly StubToolDispatcher _dispatcher = new();

    public McpServerTests()
    {
        McpToolRegistry.ResetForTests();
        McpRuntimeBridge.ResetForTests();
        McpRuntimeBridge.Dispatcher = _dispatcher;
    }

    public void Dispose()
    {
        McpToolRegistry.ResetForTests();
        McpRuntimeBridge.ResetForTests();
    }

    [Fact]
    public async Task Initialize_ReturnsCapabilities()
    {
        var server = CreateServer();
        var response = await server.HandleRequestAsync(
            """{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"xunit"}}}""",
            CancellationToken.None);

        response.Should().NotBeNull();
        using var doc = JsonDocument.Parse(response!);
        doc.RootElement.GetProperty("result").GetProperty("protocolVersion").GetString()
            .Should().Be("2024-11-05");
        doc.RootElement.GetProperty("result").GetProperty("capabilities").GetProperty("tools").Should().NotBeNull();
    }

    [Fact]
    public async Task ToolsList_ReturnsRegisteredTools()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "Add two integers",
            QualifiedProcedureName = "AddNumbers",
            InputSchema = McpSchemaBuilder.FromParameters([
                ("a", "INTEGER", false),
                ("b", "INTEGER", false)
            ])
        });

        var server = CreateServer();
        await server.HandleRequestAsync(
            """{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"xunit"}}}""",
            CancellationToken.None);

        var listResponse = await server.HandleRequestAsync(
            """{"jsonrpc":"2.0","id":2,"method":"tools/list"}""",
            CancellationToken.None);

        listResponse.Should().NotBeNull();
        var tools = JsonNode.Parse(listResponse!)!["result"]!["tools"]!.AsArray();
        tools.Should().HaveCount(1);
        tools[0]!["name"]!.GetValue<string>().Should().Be("add_numbers");
    }

    [Fact]
    public async Task ToolsCall_DispatchesAndReturnsContent()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            QualifiedProcedureName = "AddNumbers",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        var server = CreateServer();
        await server.HandleRequestAsync(
            """{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"xunit"}}}""",
            CancellationToken.None);

        var callResponse = await server.HandleRequestAsync(
            """{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"add_numbers","arguments":{"a":2,"b":2}}}""",
            CancellationToken.None);

        callResponse.Should().NotBeNull();
        _dispatcher.LastProcedure.Should().Be("AddNumbers");
        _dispatcher.LastArguments.Should().Contain("2");

        using var doc = JsonDocument.Parse(callResponse!);
        doc.RootElement.GetProperty("result").GetProperty("content")[0].GetProperty("text").GetString()
            .Should().Be("42");
    }

    [Fact]
    public async Task RoundTrip_PipeTransport()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "echo",
            QualifiedProcedureName = "Echo",
            InputSchema = McpSchemaBuilder.FromParameters([("text", "STRING", false)])
        });

        var input = new StringWriter();
        var output = new StringWriter();
        var requests = new StringWriter();
        await requests.WriteLineAsync(
            """{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"pipe"}}}""");
        await requests.WriteLineAsync("""{"jsonrpc":"2.0","method":"notifications/initialized"}""");
        await requests.WriteLineAsync("""{"jsonrpc":"2.0","id":2,"method":"tools/list"}""");
        await requests.WriteLineAsync(
            """{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"echo","arguments":{"text":"hi"}}}""");

        var reader = new StringReader(requests.ToString());
        var transport = new McpStdioTransport(reader, output);
        var host = new McpServerHost(CreateServer(), transport);

        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        await host.RunAsync(cts.Token);

        var lines = output.ToString().Split('\n', StringSplitOptions.RemoveEmptyEntries);
        lines.Should().HaveCountGreaterThanOrEqualTo(3);
        lines[0].Should().Contain("protocolVersion");
        lines[1].Should().Contain("echo");
        lines[^1].Should().Contain("42");
    }

    [Fact]
    public void ToolCallParser_ParsesStructuredResponse()
    {
        var calls = ToolCallParser.TryParse(
            """{"tool_calls":[{"id":"1","name":"add_numbers","arguments":{"a":2,"b":2}}]}""");
        calls.Should().NotBeNull();
        calls!.Should().ContainSingle();
        calls[0].Name.Should().Be("add_numbers");
    }

    [Fact]
    public async Task ToolsList_ExposesReadOnlyFlagAndOutputSchema()
    {
        // Classificazione MCP-001: `readOnly` compare SOLO quando dichiarato true,
        // `outputSchema` solo quando dichiarato — parità con gmb_mcp_list_tools (runtime C).
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "gmb_stats",
            Description = "Sample statistics by id",
            QualifiedProcedureName = "GmbStats",
            InputSchema = McpSchemaBuilder.FromParameters([("id", "INTEGER", false)]),
            OutputSchema = new JsonObject { ["type"] = "string" },
            ReadOnly = true
        });
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "gmb_mutate",
            QualifiedProcedureName = "GmbMutate",
            InputSchema = McpSchemaBuilder.FromParameters([("id", "INTEGER", false)])
        });

        var tools = await ListToolsAsync(CreateServer());

        var stats = tools.Single(t => t!["name"]!.GetValue<string>() == "gmb_stats");
        stats!["readOnly"]!.GetValue<bool>().Should().BeTrue();
        stats!["outputSchema"]!["type"]!.GetValue<string>().Should().Be("string");

        var mutating = tools.Single(t => t!["name"]!.GetValue<string>() == "gmb_mutate");
        mutating!["readOnly"].Should().BeNull();
        mutating!["outputSchema"].Should().BeNull();
    }

    [Fact]
    public async Task ReadOnlyServer_RejectsMutatingToolWithoutDispatching()
    {
        // Enforcement pre-dispatch (pattern MCP-001 `is_read_only`): il server di sola
        // consultazione rifiuta i tool NON dichiarati READONLY e non li invoca.
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "gmb_stats",
            QualifiedProcedureName = "GmbStats",
            InputSchema = McpSchemaBuilder.FromParameters([("id", "INTEGER", false)]),
            ReadOnly = true
        });
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "gmb_mutate",
            QualifiedProcedureName = "GmbMutate",
            InputSchema = McpSchemaBuilder.FromParameters([("id", "INTEGER", false)])
        });

        var server = new McpServer(dispatcher: _dispatcher, readOnlyServer: true);
        await InitializeAsync(server);

        var rejected = await server.HandleRequestAsync(
            """{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"gmb_mutate","arguments":{"id":1}}}""",
            CancellationToken.None);
        using (var doc = JsonDocument.Parse(rejected!))
            doc.RootElement.GetProperty("error").GetProperty("message").GetString()
                .Should().Contain("Read-only server");
        _dispatcher.Calls.Should().Be(0);

        var accepted = await server.HandleRequestAsync(
            """{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"gmb_stats","arguments":{"id":1}}}""",
            CancellationToken.None);
        using (var doc = JsonDocument.Parse(accepted!))
            doc.RootElement.GetProperty("result").GetProperty("isError").GetBoolean().Should().BeFalse();
        _dispatcher.Calls.Should().Be(1);
        _dispatcher.LastProcedure.Should().Be("GmbStats");
    }

    private static async Task InitializeAsync(McpServer server) =>
        await server.HandleRequestAsync(
            """{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"xunit"}}}""",
            CancellationToken.None);

    private static async Task<JsonArray> ListToolsAsync(McpServer server)
    {
        await InitializeAsync(server);
        var response = await server.HandleRequestAsync(
            """{"jsonrpc":"2.0","id":9,"method":"tools/list"}""", CancellationToken.None);
        return JsonNode.Parse(response!)!["result"]!["tools"]!.AsArray();
    }

    private McpServer CreateServer() => new(dispatcher: _dispatcher);
}
