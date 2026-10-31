// FILE: AI.Tests.SessionStreamingTests
using Basic.Agents;
using Basic.AI.Rag;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class SessionStreamingTests : IDisposable
{
    public SessionStreamingTests() => AgentServer.ResetForTests();
    public void Dispose() => AgentServer.ResetForTests();

    [Fact]
    public void GenerateFromSessionStream_YieldsTokenByTokenAndConcatenates()
    {
        var stub = new StubSessionStreamingCompleter(["Token1", " ", "Token2"]);
        var received = new List<string>();

        var full = stub.GenerateFromSessionStream(1, 100, received.Add);

        full.Should().Be("Token1 Token2");
        received.Should().Equal("Token1", " ", "Token2");
        stub.IsSessionAware.Should().BeTrue();
    }

    [Fact]
    public void SessionStreaming_IsPerAgentProperty_DefaultOff()
    {
        AgentServer.Instance.Create("sess");
        AgentServer.Instance.Get("sess", "sessionstreaming").Should().Be("OFF");

        AgentServer.Instance.Set("sess", "sessionstreaming", "ON");
        AgentServer.Instance.Get("sess", "sessionstreaming").Should().Be("ON");

        AgentServer.Instance.Set("sess", "sessionstreaming", "OFF");
        AgentServer.Instance.Get("sess", "sessionstreaming").Should().Be("OFF");
    }

    private sealed class StubSessionStreamingCompleter : ISessionStreamingCompleter
    {
        private readonly string[] _tokens;
        public string ModelName => "stub-session";
        public bool IsSessionAware => this is ISessionAwareLlmCompleter;

        public StubSessionStreamingCompleter(string[] tokens) => _tokens = tokens;

        public string Complete(string systemPrompt, string userPrompt) => "";
        public int CreateSession() => 1;
        public void AppendMessage(int sessionId, string role, string content) { }
        public string GenerateFromSession(int sessionId, int maxTokens) => string.Concat(_tokens);
        public void DestroySession(int sessionId) { }

        public string GenerateFromSessionStream(int sessionId, int maxTokens, Action<string> onToken)
        {
            var sb = new System.Text.StringBuilder();
            foreach (var t in _tokens)
            {
                sb.Append(t);
                onToken?.Invoke(t);
            }
            return sb.ToString();
        }
    }
}
