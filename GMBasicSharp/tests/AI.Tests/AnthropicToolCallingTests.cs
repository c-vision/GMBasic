// FILE: AI.Tests.AnthropicToolCallingTests
using System.Net;
using System.Text.Json.Nodes;
using Basic.Mcp;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AnthropicToolCallingTests
{
    [Fact]
    public void NativeToolCalling_ParsesToolUseBlocks()
    {
        var handler = new StubHandler((req, _) =>
        {
            req.RequestUri!.AbsolutePath.Should().Be("/v1/messages");
            var body = req.Content!.ReadAsStringAsync().GetAwaiter().GetResult();
            body.Should().Contain("tools");
            body.Should().Contain("read_file");
            const string json = """
                {"content":[{"type":"tool_use","id":"toolu_1","name":"read_file",
                              "input":{"path":"/tmp/a.txt"}}],"stop_reason":"tool_use"}
                """;
            return JsonResponse(json);
        });

        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion, "anthropic-llm", "anthropic",
            new Dictionary<string, string> { ["apiKey"] = "sk-ant-test", ["model"] = "claude-3" });
        var provider = new AnthropicLlmCompleter(descriptor, new HttpClient(handler));
        var tools = new List<McpToolDefinition>
        {
            new() { Name = "read_file", Description = "legge un file", QualifiedProcedureName = "read_file",
                    InputSchema = new JsonObject { ["type"] = "object" } }
        };

        var outcome = provider.CompleteWithTools("sys", "user", tools);

        outcome.ToolCalls.Should().NotBeNull();
        outcome.ToolCalls.Should().HaveCount(1);
        outcome.ToolCalls![0].Id.Should().Be("toolu_1");
        outcome.ToolCalls[0].Name.Should().Be("read_file");
        outcome.ToolCalls[0].ArgumentsJson.Should().Contain("/tmp/a.txt");
    }

    [Fact]
    public void NativeToolCalling_ParsesFinalText()
    {
        var handler = new StubHandler((req, _) =>
        {
            const string json = """{"content":[{"type":"text","text":"Risposta finale."}],"stop_reason":"end_turn"}""";
            return JsonResponse(json);
        });

        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion, "anthropic-llm", "anthropic",
            new Dictionary<string, string> { ["apiKey"] = "sk-ant-test", ["model"] = "claude-3" });
        var provider = new AnthropicLlmCompleter(descriptor, new HttpClient(handler));

        var outcome = provider.CompleteWithTools("sys", "user", []);

        outcome.Text.Should().Be("Risposta finale.");
        outcome.ToolCalls.Should().BeNull();
    }

    private static HttpResponseMessage JsonResponse(string json) =>
        new(HttpStatusCode.OK)
        {
            Content = new StringContent(json, System.Text.Encoding.UTF8, "application/json")
        };

    private sealed class StubHandler(Func<HttpRequestMessage, CancellationToken, HttpResponseMessage> handler)
        : HttpMessageHandler
    {
        protected override Task<HttpResponseMessage> SendAsync(
            HttpRequestMessage request, CancellationToken cancellationToken) =>
            Task.FromResult(handler(request, cancellationToken));
    }
}
