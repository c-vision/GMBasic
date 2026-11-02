// FILE: AI.Tests.DecisionScoringHeadTests
//
// Test model-free dell'artefatto di calibrazione JEV (`scoring_head.json`): parsing dei
// formati accettati, applicazione di bias/token-id/temperatura alla DecisionRequest e
// helper numerici (softmax grezza, confidenza). Nessun modello nativo: girano sempre.
using Basic.Decision;
using FluentAssertions;

namespace AI.Tests;

public sealed class DecisionScoringHeadTests
{
    [Fact(DisplayName = "scoring head: bias posizionale (24), T_kind e head_T")]
    public void Parse_ArrayBias_AppliesBiasAndKindTemperature()
    {
        var json = """
        {
          "bias": [0.1, 0.2, 0, 0, 0, 0, 0, 0, 0.5, -0.5, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0],
          "head_T": 1.5,
          "T_kind": [2.0, 3.0, 4.0]
        }
        """;

        var head = ScoringHead.Parse(json);

        head.HeadTemperature.Should().Be(1.5f);
        head.IsCalibrated.Should().BeTrue();
        head.TemperatureFor(DecisionKind.Noul).Should().Be(2.0f);
        head.TemperatureFor(DecisionKind.Choice).Should().Be(3.0f);
        head.TemperatureFor(DecisionKind.Score).Should().Be(4.0f);

        var request = DecisionSlots.CreateChoice("x", ["billing", "technical"]);
        var applied = head.Apply(request);

        applied.Options[0].Id.Should().Be("A");
        applied.Options[0].Bias.Should().Be(0.5f);
        applied.Options[0].Temperature.Should().Be(3.0f);
        applied.Options[1].Id.Should().Be("B");
        applied.Options[1].Bias.Should().Be(-0.5f);
    }

    [Fact(DisplayName = "scoring head: bias per etichetta + verbalizer_ids risolvono il token")]
    public void Parse_ObjectBiasAndTokenIds_AreApplied()
    {
        var json = """
        {
          "bias": { "false": -0.5, "true": 0.5 },
          "verbalizer_ids": { "false": 1234, "true": 5678 },
          "T_kind": { "noul": 2.0 }
        }
        """;

        var head = ScoringHead.Parse(json);
        var applied = head.Apply(DecisionSlots.CreateNoul("x"));

        applied.Options[0].Bias.Should().Be(-0.5f);
        applied.Options[0].Temperature.Should().Be(2.0f);
        applied.Options[0].TokenId.Should().Be(1234);
        applied.Options[1].Bias.Should().Be(0.5f);
        applied.Options[1].TokenId.Should().Be(5678);
        applied.Options[1].Temperature.Should().Be(2.0f);
    }

    [Fact(DisplayName = "scoring head: senza artefatto nessun bias/temperatura cambia")]
    public void TryLoad_MissingFile_ReturnsNull()
    {
        var dir = Path.Combine(Path.GetTempPath(), "gmb-scoring-head-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        try
        {
            ScoringHead.TryLoad(dir).Should().BeNull();
        }
        finally
        {
            Directory.Delete(dir, recursive: true);
        }
    }

    [Fact(DisplayName = "scoring head: file malformato è errore esplicito, non calibrazione silenziosa")]
    public void Parse_Malformed_Throws()
    {
        var act = () => ScoringHead.Parse("{ not json");
        act.Should().Throw<DecisionProviderException>();
    }

    [Fact(DisplayName = "scoring head: calibrated=false non dichiara la calibrazione")]
    public void Parse_CalibratedFalse_IsNotCalibrated()
    {
        var head = ScoringHead.Parse("""{ "bias": [], "calibrated": false }""");
        head.IsCalibrated.Should().BeFalse();
    }

    [Fact(DisplayName = "softmax grezza: logprob [0, ln3] -> [0.25, 0.75]")]
    public void Softmax_MatchesReference()
    {
        var probs = ScoringHead.Softmax([0f, (float)Math.Log(3.0)]);
        probs.Should().HaveCount(2);
        probs[0].Should().BeApproximately(0.25, 1e-6);
        probs[1].Should().BeApproximately(0.75, 1e-6);
        (probs[0] + probs[1]).Should().BeApproximately(1.0, 1e-6);
    }

    [Fact(DisplayName = "confidenza: p_top - media(resto), clampata in [0,1]")]
    public void Confidence_UsesTopMinusMeanRest()
    {
        double[] probs = [0.10, 0.70, 0.20];
        ScoringHead.Confidence(probs, 1).Should().BeApproximately(0.70 - 0.15, 1e-9);

        double[] flat = [0.5, 0.5];
        ScoringHead.Confidence(flat, 0).Should().Be(0.0);

        ScoringHead.Confidence([1.0], 0).Should().BeNull();
    }
}
