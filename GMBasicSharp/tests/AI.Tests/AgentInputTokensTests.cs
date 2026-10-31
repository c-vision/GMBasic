// FILE: AI.Tests.AgentInputTokensTests
using Basic.Agents;
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>E6 — monitoraggio input_tokens/turn: ITokenizingCompleter (CountTokens) sul
/// completer MLX reale + GetInputTokens su AgentServer (default -1 per agente sconosciuto /
/// completer non-tokenizing). Env-gated per la parte reale.</summary>
public sealed class AgentInputTokensTests : IDisposable
{
    public AgentInputTokensTests() => AgentServer.ResetForTests();
    public void Dispose() => AgentServer.ResetForTests();

    [Fact]
    public void GetInputTokens_UnknownAgent_ReturnsMinusOne()
    {
        AgentServer.Instance.GetInputTokens("nessun-agente").Should().Be(-1);
    }

    [Fact]
    public void CountTokens_MlxCompleter_WhenModelEnvSet_CountsPositive()
    {
        var modelDir = Environment.GetEnvironmentVariable("GMBASIC_MLX_MODEL_DIR");
        if (string.IsNullOrWhiteSpace(modelDir))
            return; // graceful skip

        var completer = (ITokenizingCompleter)ProviderFactory.CreateCompletion(new ProviderDescriptor(
            ProviderRole.Completion, "e6-token", "mlx",
            new Dictionary<string, string>
            {
                ["modelDir"] = modelDir, ["maxTokens"] = "16",
            }));

        var n = completer.CountTokens("The quick brown fox jumps over the lazy dog.");
        n.Should().BeGreaterThan(0, "il tokenizer MLX deve contare i token del testo");
        // Testo piu' lungo -> piu' token (coerenza).
        completer.CountTokens("The quick brown fox jumps over the lazy dog. " +
                              "The quick brown fox jumps over the lazy dog.").Should().BeGreaterThanOrEqualTo(n);
    }
}
