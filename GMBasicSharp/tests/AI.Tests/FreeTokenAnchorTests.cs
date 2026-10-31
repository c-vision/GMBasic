// FILE: AI.Tests.FreeTokenAnchorTests
// §4.4 FreeToken: semantic anchor checkpoint nel tool-loop. Verifica che gli anchor
// (user/toolCall/toolResult/final) siano registrati e che il delta dal toolResult sia
// MINORE del contesto totale → il checkpoint consentirebbe di ri-prefillare solo il delta.

using Basic.Agents;
using Basic.Mcp;
using Basic.Providers;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class FreeTokenAnchorTests : IDisposable
{
    private readonly string _config = Path.Combine(Path.GetTempPath(), "gmb_fta_" + Guid.NewGuid().ToString("N") + ".json");
    public FreeTokenAnchorTests()
    {
        Basic.AI.AiConfig.OverridePathForTests(_config);
        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
        McpToolRegistry.ResetForTests();
        McpConfig.ResetForTests();
        McpMarketplace.ResetForTests();
        McpRuntimeBridge.ResetForTests();
        McpRuntimeBridge.Dispatcher = new FnDispatcher();
    }
    public void Dispose()
    {
        AgentServer.ResetForTests();
        ProviderServer.ResetForTests();
        McpToolRegistry.ResetForTests();
        McpConfig.ResetForTests();
        McpMarketplace.ResetForTests();
        McpRuntimeBridge.ResetForTests();
        Basic.AI.AiConfig.ResetForTests();
        if (File.Exists(_config)) File.Delete(_config);
    }

    private sealed class FnDispatcher : IMcpToolDispatcher
    {
        public Task<McpToolCallResult> DispatchAsync(string proc, string args, CancellationToken ct)
            => Task.FromResult(new McpToolCallResult(false, "result-42"));
    }

    [Fact]
    public void ToolLoop_RecordsAnchors_AndDeltaIsSmallerThanContext()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });
        AgentServer.Instance.Create("ft-agent");
        AgentServer.Instance.RegisterTool("ft-agent", "add_numbers");

        var answer = AgentServer.Instance.Ask("ft-agent", "tool: add_numbers please");
        Assert.False(string.IsNullOrWhiteSpace(answer));

        var stats = AgentServer.Instance.GetAnchorStats("ft-agent");
        Assert.DoesNotContain("error", stats);
        using var doc = System.Text.Json.JsonDocument.Parse(stats);
        var root = doc.RootElement;
        var anchors = root.GetProperty("anchors").GetInt32();
        Assert.True(anchors >= 3, $"attesi >=3 anchor (user, toolCall, toolResult...), avuti {anchors}");
        var ctx = root.GetProperty("ctxChars").GetInt64();
        var delta = root.GetProperty("deltaSinceToolResult").GetInt64();
        var pinned = root.GetProperty("pinnedEstimateChars").GetInt64();
        Assert.True(delta < ctx, "il delta dopo il toolResult deve essere < contesto totale");
        Assert.True(pinned > 0 && pinned < ctx);
    }

    [Fact]
    public void NoAnchors_ReturnsExplicitError()
    {
        AgentServer.Instance.Create("quiet");
        var stats = AgentServer.Instance.GetAnchorStats("quiet");
        Assert.Contains("error", stats);
    }
}
