// FILE: AI.Tests.MlxSpeculativeParityTests
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Parita' END-TO-END del T2-wiring a modello reale: il decode speculative n-gram
/// (config speculation:"ngram") deve produrre ESATTAMENTE lo stesso output del decode greedy
/// (temperature 0) sullo stesso modello e stesso prompt. Verifica la correttezza del wiring
/// in produzione (MlxLlmCompleter.GenerateFromSession -> SpeculativeDecode), non solo dei
/// singoli primitivi nativi.
/// Env-gated (come gli altri test MLX): GMBASIC_MLX_MODEL_DIR. Usare un modello PICCOLO
/// (es. Llama-3.2-1B-Instruct-4bit): questo test carica il modello DUE volte (greedy +
/// speculative), quindi con un modello ~30B si rischia un timeout GPU (stesso vincolo del
/// commento su MlxRealModelFixture).</summary>
public sealed class MlxSpeculativeParityTests
{
    private const string Prompt =
        "Write a list of ten fruits, one per line, starting with: apple, banana, ";

    [Fact(DisplayName = "T2-wiring: decode speculative (ngram) == decode greedy a modello reale, quando GMBASIC_MLX_MODEL_DIR set")]
    public void SpeculativeDecode_MatchesGreedy_WhenModelEnvSet()
    {
        var modelDir = Environment.GetEnvironmentVariable("GMBASIC_MLX_MODEL_DIR");
        if (string.IsNullOrWhiteSpace(modelDir))
            return; // graceful skip, same opt-in convention

        var greedy = ProviderFactory.CreateCompletion(new ProviderDescriptor(
            ProviderRole.Completion, "t2-greedy", "mlx",
            new Dictionary<string, string>
            {
                ["modelDir"] = modelDir,
                ["temperature"] = "0",          // greedy deterministico
                ["maxTokens"] = "64",
            }));
        var spec = ProviderFactory.CreateCompletion(new ProviderDescriptor(
            ProviderRole.Completion, "t2-spec", "mlx",
            new Dictionary<string, string>
            {
                ["modelDir"] = modelDir,
                ["temperature"] = "0",          // parita' verify batch solo in greedy
                ["speculation"] = "ngram",      // attiva il wiring speculative
                ["speculationMax"] = "8",
                ["maxTokens"] = "64",
            }));

        var gsac = (ISessionAwareLlmCompleter)greedy;
        var ssac = (ISessionAwareLlmCompleter)spec;
        var gsid = gsac.CreateSession();
        var ssid = ssac.CreateSession();
        try
        {
            gsac.AppendMessage(gsid, "user", Prompt);
            ssac.AppendMessage(ssid, "user", Prompt);

            var greedyText = gsac.GenerateFromSession(gsid, 64);
            var specText = ssac.GenerateFromSession(ssid, 64);

            greedyText.Should().NotBeNullOrWhiteSpace();
            // Parita' esatta: lo speculative non deve alterare l'output del greedy.
            specText.Should().Be(greedyText);
        }
        finally
        {
            gsac.DestroySession(gsid);
            ssac.DestroySession(ssid);
        }
    }
}
