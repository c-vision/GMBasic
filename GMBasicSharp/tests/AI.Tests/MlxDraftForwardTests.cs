// FILE: AI.Tests.MlxDraftForwardTests
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>T3 DFlash2 draft model — verifica del forward di proposta del blocco di token
/// candidati (DraftFromPrefix) col modello DRAFT reale.
///
/// NOTA (2026-08-31): il modello DFlash2 reale (~/ai/Models/Qwen3.8-27B-DFlash2-mxfp4) NON e'
/// caricabile dal loader MLX (formato MXFP4 bits4 group32; il loader fa group64 bits8). Quindi
/// questo test valida il MECCANISMO del draft forward usando un modello supportato
/// (GMBASIC_MLX_MODEL_DIR, es. Llama-3.2-1B) come target+draft (stesso vocab). Il supporto MXFP4
/// per il DFlash2 reale e' un gap separato da chiudere.</summary>
public sealed class MlxDraftForwardTests
{
    [Fact(DisplayName = "T3: DraftFromPrefix produce un blocco di token candidati dal modello draft, quando GMBASIC_MLX_MODEL_DIR set")]
    public void DraftFromPrefix_WhenModelEnvSet_ProducesBlockOfTokens()
    {
        var modelDir = Environment.GetEnvironmentVariable("GMBASIC_MLX_MODEL_DIR");
        if (string.IsNullOrWhiteSpace(modelDir))
            return; // graceful skip

        var completer = (MlxLlmCompleter)ProviderFactory.CreateCompletion(new ProviderDescriptor(
            ProviderRole.Completion, "t3-draft", "mlx",
            new Dictionary<string, string>
            {
                ["modelDir"] = modelDir,         // target = modello supportato
                ["draftModelDir"] = modelDir,    // draft = stesso modello (stesso vocab)
                ["maxTokens"] = "16",
                ["temperature"] = "0",
            }));

        completer.DraftEnabled.Should().BeTrue("draftModelDir configurato deve caricare il draft model");

        var prefix = completer.Tokenize("Once upon a time in a land far away, there was a");
        prefix.Length.Should().BeGreaterThan(0);

        var draft = completer.DraftFromPrefix(prefix, 8);
        draft.Should().NotBeEmpty("il draft model deve proporre almeno un token candidato");
        draft.Length.Should().BeLessThanOrEqualTo(8);
        draft.Should().OnlyContain(t => t >= 0, "i token candidati devono essere id validi");
    }

    [Fact(DisplayName = "T3: decode speculative con modello DRAFT == greedy a modello reale, quando GMBASIC_MLX_MODEL_DIR set")]
    public void DraftSpeculative_MatchesGreedy_WhenModelEnvSet()
    {
        var modelDir = Environment.GetEnvironmentVariable("GMBASIC_MLX_MODEL_DIR");
        if (string.IsNullOrWhiteSpace(modelDir))
            return;

        // Baseline greedy (no speculation).
        var greedy = ProviderFactory.CreateCompletion(new ProviderDescriptor(
            ProviderRole.Completion, "t3-greedy", "mlx",
            new Dictionary<string, string>
            {
                ["modelDir"] = modelDir, ["temperature"] = "0", ["maxTokens"] = "64",
            }));
        // Speculative col modello DRAFT (stesso modello come target+draft per il test del meccanismo).
        var spec = ProviderFactory.CreateCompletion(new ProviderDescriptor(
            ProviderRole.Completion, "t3-spec", "mlx",
            new Dictionary<string, string>
            {
                ["modelDir"] = modelDir, ["draftModelDir"] = modelDir,
                ["temperature"] = "0", ["speculation"] = "dflash", ["speculationMax"] = "8",
                ["maxTokens"] = "64",
            }));

        var gsac = (ISessionAwareLlmCompleter)greedy;
        var ssac = (ISessionAwareLlmCompleter)spec;
        var gsid = gsac.CreateSession();
        var ssid = ssac.CreateSession();
        try
        {
            gsac.AppendMessage(gsid, "user", "Once upon a time in a land far away, there was a");
            ssac.AppendMessage(ssid, "user", "Once upon a time in a land far away, there was a");
            var gText = gsac.GenerateFromSession(gsid, 64);
            var sText = ssac.GenerateFromSession(ssid, 64);
            gText.Should().NotBeNullOrWhiteSpace();
            sText.Should().Be(gText, "lo speculative col modello draft deve produrre lo stesso output greedy");
        }
        finally
        {
            gsac.DestroySession(gsid);
            ssac.DestroySession(ssid);
        }
    }
}
