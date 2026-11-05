using Basic.Decision;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Provider `clm` (JEV/SystemOne): prosa, wire /v1/systemone, risposta. Gli stessi golden di
/// GMBasic/native/gmb_runtime_c/tests/decision_clm_selfcheck.c: parità fra le due implementazioni.</summary>
[Trait("Category", "Fast")]
public sealed class ClmDecisionProviderTests
{
    private static DecisionRequest Choice(string state) => new(state, DecisionKind.Choice,
        [new DecisionOption("auth", "Authentication"), new DecisionOption("b", "b")]);

    [Fact]
    public void ToText_ObjectoAnnidato()
        => ClmDecisionProvider.ToText("{\"ticket\":\"Login fails\",\"tags\":[\"a\",\"b\"],\"user\":{\"name\":\"Ann\",\"age\":30},\"ok\":true}")
            .Should().Be("ticket: Login fails\ntags:\n  - a\n  - b\nuser:\n  name: Ann\n  age: 30\nok: true");

    [Fact]
    public void ToText_ArrayConOggetto() => ClmDecisionProvider.ToText("[\"x\",{\"k\":1}]").Should().Be("- x\n-\n  k: 1");

    [Theory]
    [InlineData("Il cliente e' irritato.")]
    [InlineData("{\"a\":")]
    public void ToText_NonJson_Invariato(string s) => ClmDecisionProvider.ToText(s).Should().Be(s);

    [Fact]
    public void ToText_DecodificaEscape()
        => ClmDecisionProvider.ToText("{\"s\":\"riga1\\nriga2 \\\"q\\\"\"}").Should().Be("s: riga1\nriga2 \"q\"");

    [Theory]
    [InlineData("{\"a\":1}", "Quale team?", "a: 1\n\nQuale team?")]
    [InlineData("solo stato", "", "solo stato")]
    [InlineData("", "solo domanda", "solo domanda")]
    public void StateText(string s, string i, string want) => ClmDecisionProvider.StateText(s, i).Should().Be(want);

    [Fact]
    public void Request_Choice()
        => ClmDecisionProvider.BuildRequest(Choice("hello"), "").Should().Be(
            "{\"state\":\"hello\",\"questions\":{\"q\":{\"type\":\"choice\",\"instructions\":\"\",\"criteria\":{\"auth\":\"Authentication\",\"b\":\"\"}}}}");

    [Fact]
    public void Request_StatoJson_IncorporatoComeJson()
        => ClmDecisionProvider.BuildRequest(Choice("{\"t\":\"Login\"}"), "Chi?").Should().Be(
            "{\"state\":{\"t\":\"Login\"},\"questions\":{\"q\":{\"type\":\"choice\",\"instructions\":\"Chi?\",\"criteria\":{\"auth\":\"Authentication\",\"b\":\"\"}}}}");

    [Fact]
    public void Request_Score()
        => ClmDecisionProvider.BuildRequest(new DecisionRequest("s", DecisionKind.Score,
                [new DecisionOption("0", "poco"), new DecisionOption("1", "molto")]), "Quanto?").Should().Be(
            "{\"state\":\"s\",\"questions\":{\"q\":{\"type\":\"score\",\"instructions\":\"Quanto?\",\"criteria\":[\"poco\",\"molto\"]}}}");

    [Fact]
    public void Response_Choice()
    {
        var r = ClmDecisionProvider.ParseResponse("{\"type\":\"choice\",\"choice\":\"auth\",\"confidence\":0.6,\"probabilities\":{\"auth\":0.8,\"b\":0.2}}", Choice("x"));
        r.OptionId.Should().Be("auth"); r.Probability.Should().BeApproximately(0.8, 1e-9); r.Confidence.Should().BeApproximately(0.6, 1e-9);
    }

    [Fact]
    public void Response_SottoId_ConfidenceCalcolata()
    {
        var r = ClmDecisionProvider.ParseResponse("{\"q\":{\"type\":\"choice\",\"choice\":\"b\",\"probabilities\":{\"auth\":0.3,\"b\":0.7}}}", Choice("x"));
        r.OptionId.Should().Be("b"); r.Confidence.Should().BeApproximately(0.4, 1e-6);
    }

    [Theory]
    [InlineData(0.83, "true", 0.83, 0.66)]
    [InlineData(0.1, "false", 0.9, 0.8)]
    public void Response_Noul(double noul, string opt, double prob, double conf)
    {
        var req = new DecisionRequest("x", DecisionKind.Noul, DecisionSlots.Noul);
        var r = ClmDecisionProvider.ParseResponse($"{{\"type\":\"noul\",\"noul\":{noul.ToString(System.Globalization.CultureInfo.InvariantCulture)}}}", req);
        r.OptionId.Should().Be(opt); r.Probability.Should().BeApproximately(prob, 1e-9); r.Confidence.Should().BeApproximately(conf, 1e-6);
    }

    [Fact]
    public void Response_Score_Argmax()
    {
        var req = new DecisionRequest("s", DecisionKind.Score, [new DecisionOption("0", "poco"), new DecisionOption("1", "molto")]);
        var r = ClmDecisionProvider.ParseResponse("{\"type\":\"score\",\"score\":0.7,\"probabilities\":{\"0\":0.3,\"1\":0.7}}", req);
        r.OptionId.Should().Be("1"); r.Probability.Should().BeApproximately(0.7, 1e-9);
    }

    [Theory]
    [InlineData("{\"type\":\"noul\",\"noul\":0.5}", "incoerente")]
    [InlineData("{\"type\":\"choice\",\"choice\":\"zzz\",\"probabilities\":{\"zzz\":1}}", "non nel dominio")]
    [InlineData("{\"type\":\"choice\",\"choice\":\"auth\"}", "probabilities")]
    [InlineData("", "vuota")]
    [InlineData("{\"detail\":\"unauthorized\"}", "type")]
    public void Response_Errori_Espliciti(string body, string frammento)
        => FluentActions.Invoking(() => ClmDecisionProvider.ParseResponse(body, Choice("x")))
            .Should().Throw<DecisionProviderException>().WithMessage($"*{frammento}*");

    [Fact]
    public void Confidence()
    {
        ClmDecisionProvider.Confidence([0.5, 0.3, 0.2]).Should().BeApproximately(0.25, 1e-9);
        ClmDecisionProvider.Confidence([0.9]).Should().Be(1.0);
    }
}
