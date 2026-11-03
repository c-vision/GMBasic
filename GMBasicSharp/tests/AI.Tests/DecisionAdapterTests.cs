using Basic.AI.Rag;
using Basic.Decision;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class DecisionAdapterTests
{
    private sealed class FakeCompleter : ILlmCompleter
    {
        private readonly string _reply;
        public FakeCompleter(string reply) => _reply = reply;
        public string ModelName => "fake-adapter";
        public string Complete(string systemPrompt, string userPrompt) => _reply;
    }

    private static DecisionResult Decide(DecisionRequest request, string reply)
        => new AdapterDecisionProvider(new FakeCompleter(reply)).Evaluate(request)[0];

    [Theory]
    [InlineData("true", true)]
    [InlineData("False.", false)]
    [InlineData("Risposta: true", true)]
    public void Noul_ParsesLabel(string reply, bool expected)
    {
        var result = Decide(DecisionSlots.CreateNoul("Il cliente ha detto sì"), reply);
        result.Value.Should().Be(expected);
        result.Probability.Should().Be(1.0);
        result.Confidence.Should().BeNull();
        result.Calibrated.Should().BeNull();
    }

    [Fact]
    public void Score_ParsesNumber()
    {
        var request = DecisionSlots.CreateScoreRange("valuta", 0, 10);
        var result = Decide(request, "3");
        result.Value.Should().Be(3.0);
    }

    [Fact]
    public void Score_PrefersExactNumber_OverPrefix()
    {
        var request = DecisionSlots.CreateScoreRange("valuta", 0, 10);
        Decide(request, "10").Value.Should().Be(10.0);
    }

    [Fact]
    public void Choice_ParsesSlotLetter()
    {
        var request = DecisionSlots.CreateChoice("categoria?", ["billing", "technical"]);
        var result = Decide(request, "B) technical");
        result.OptionId.Should().Be("B");
        result.Value.Should().Be("technical");
    }

    [Fact]
    public void Unmatched_IsExplicitError()
    {
        var ex = Record.Exception(() =>
            Decide(DecisionSlots.CreateChoice("categoria?", ["billing", "technical"]), "non lo so"));
        ex.Should().BeOfType<DecisionProviderException>();
        ex!.Message.Should().Contain("adapter");
    }
}
