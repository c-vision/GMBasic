// FILE: AI.Tests.Qwen38TargetSmokeTests
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Smoke del target Qwen3.8-27B (model_type qwen3_5, text_config annidato, head_dim 256)
/// in GMBasic. Env-gated: GMBASIC_MLX_27B_DIR (es. ~/ai/Models/Qwen3.8-27B-8bit). Verifica che
/// il loader GMBasic carichi l'architettura qwen3_5 e generi testo valido. Modello 28GB: load lento.</summary>
public sealed class Qwen38TargetSmokeTests
{
    [Fact(DisplayName = "Qwen3.8-27B (qwen3_5): carica e genera in GMBasic, quando GMBASIC_MLX_27B_DIR set")]
    public void Qwen38_27B_LoadsAndGenerates_WhenEnvSet()
    {
        var dir = Environment.GetEnvironmentVariable("GMBASIC_MLX_27B_DIR");
        if (string.IsNullOrWhiteSpace(dir))
            return; // graceful skip

        var completer = (ISessionAwareLlmCompleter)ProviderFactory.CreateCompletion(new ProviderDescriptor(
            ProviderRole.Completion, "qwen38-27b", "mlx",
            new Dictionary<string, string>
            {
                ["modelDir"] = dir, ["temperature"] = "0", ["maxTokens"] = "32",
            }));

        var sid = completer.CreateSession();
        try
        {
            completer.AppendMessage(sid, "user", "What is the capital of France? Answer in one word.");
            var text = completer.GenerateFromSession(sid, 32);
            text.Should().NotBeNullOrWhiteSpace("il target 27B deve generare testo valido in GMBasic");
        }
        finally
        {
            completer.DestroySession(sid);
        }
    }
}
