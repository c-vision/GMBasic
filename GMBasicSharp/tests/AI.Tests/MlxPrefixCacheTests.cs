// FILE: AI.Tests.MlxPrefixCacheTests
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>T1 prefix cache condivisa (via path swap-out/swap-in, niente deep-copy nativa):
/// due sessioni che condividono lo STESSO system prompt devono produrre output identici
/// (correttezza del riuso), e la seconda sessione deve RIUSARE la KV cache (PrefixCacheHits>0)
/// invece di ri-prefillare. Env-gated: GMBASIC_MLX_MODEL_DIR (modello piccolo, es. 1B).</summary>
public sealed class MlxPrefixCacheTests
{
    private const string SystemPrompt = "You are a helpful assistant that answers in one short sentence.";
    private const string UserPrompt = "What is the capital of France?";

    [Fact(DisplayName = "T1: system prompt condiviso -> riuso KV cache e output identico, quando GMBASIC_MLX_MODEL_DIR set")]
    public void SharedSystemPrompt_UsesPrefixCache_AndMatchesFullPrefill_WhenModelEnvSet()
    {
        var modelDir = Environment.GetEnvironmentVariable("GMBASIC_MLX_MODEL_DIR");
        if (string.IsNullOrWhiteSpace(modelDir))
            return; // graceful skip, same opt-in convention

        var cachePath = Path.Combine(Path.GetTempPath(), $"gmb-prefix-{Guid.NewGuid():N}");
        try
        {
            var completer = (ISessionAwareLlmCompleter)ProviderFactory.CreateCompletion(new ProviderDescriptor(
                ProviderRole.Completion, "t1-prefix", "mlx",
                new Dictionary<string, string>
                {
                    ["modelDir"] = modelDir,
                    ["temperature"] = "0",        // greedy deterministico per il confronto
                    ["maxTokens"] = "32",
                    ["prefixCache"] = "true",     // abilita T1
                    ["kvSwapPath"] = cachePath,
                }));

            // Sessione A: prima occorrenza del system prompt -> prefill normale + cache del prefisso.
            var sidA = completer.CreateSession();
            var sidB = completer.CreateSession();
            try
            {
                completer.AppendMessage(sidA, "system", SystemPrompt);
                completer.AppendMessage(sidA, "user", UserPrompt);
                var outA = completer.GenerateFromSession(sidA, 32);

                // Sessione B: stesso system prompt -> deve RIUSARE la cache (niente re-prefill).
                completer.AppendMessage(sidB, "system", SystemPrompt);
                completer.AppendMessage(sidB, "user", UserPrompt);
                var outB = completer.GenerateFromSession(sidB, 32);

                outA.Should().NotBeNullOrWhiteSpace();
                // Correttezza: stessa condizione (system+user) -> stesso output greedy.
                outB.Should().Be(outA);
            }
            finally
            {
                completer.DestroySession(sidA);
                completer.DestroySession(sidB);
            }

            // Il riuso DEVE essere avvenuto (la seconda sessione non ha ri-prefillato).
            var hits = ((MlxLlmCompleter)completer).PrefixCacheHits;
            hits.Should().BeGreaterThanOrEqualTo(1, "la seconda sessione deve riusare la KV del system prompt in cache");
        }
        finally
        {
            try { Directory.Delete(cachePath, recursive: true); } catch { /* best-effort */ }
        }
    }
}
