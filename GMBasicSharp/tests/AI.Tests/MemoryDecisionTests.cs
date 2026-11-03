using Basic.Decision;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class MemoryDecisionTests
{
    private static DecisionResult Noul(bool value, double probability, double? confidence = null)
        => new(DecisionKind.Noul, value ? "true" : "false", value, probability, confidence);

    [Fact]
    public void Requests_AreCanonicalNoul_WithBothTexts()
    {
        var current = MemoryDecisions.IsCurrent("Roma è la capitale", "Roma capitale d'Italia");
        current.Kind.Should().Be(DecisionKind.Noul);
        current.State.Should().Contain("Roma è la capitale").And.Contain("Candidato");

        var supersede = MemoryDecisions.Supersedes("vecchio", "nuovo");
        supersede.Kind.Should().Be(DecisionKind.Noul);
        supersede.State.Should().Contain("vecchio").And.Contain("nuovo");

        var contradiction = MemoryDecisions.Contradicts("A", "B");
        contradiction.Kind.Should().Be(DecisionKind.Noul);
        contradiction.State.Should().Contain("Fatto A").And.Contain("Fatto B");
    }

    [Theory]
    [InlineData(true, 0.90, MemoryVerdict.Strong)]
    [InlineData(true, 0.50, MemoryVerdict.Weak)]
    [InlineData(true, 0.20, MemoryVerdict.None)]
    [InlineData(false, 0.90, MemoryVerdict.None)]   // p(true) = 0.10
    public void ToVerdict_MapsProbability(bool value, double probability, MemoryVerdict expected)
    {
        MemoryDecisions.ToVerdict(Noul(value, probability)).Verdict.Should().Be(expected);
    }

    [Fact]
    public void ToVerdict_KeepsConfidence()
    {
        var decision = MemoryDecisions.ToVerdict(Noul(true, 0.91, 0.77));
        decision.Confidence.Should().Be(0.77);
        decision.Probability.Should().Be(0.91);
    }

    [Fact]
    public void ToVerdict_CustomThresholds()
    {
        MemoryDecisions.ToVerdict(Noul(true, 0.70), strong: 0.65, weak: 0.30)
            .Verdict.Should().Be(MemoryVerdict.Strong);
        MemoryDecisions.ToVerdict(Noul(true, 0.35), strong: 0.65, weak: 0.30)
            .Verdict.Should().Be(MemoryVerdict.Weak);
    }

    [Fact]
    public void ToVerdict_InvalidThresholds_Throw()
    {
        var ex = Record.Exception(() => MemoryDecisions.ToVerdict(Noul(true, 0.9), strong: 0.4, weak: 0.6));
        ex.Should().BeOfType<ArgumentOutOfRangeException>();
    }

    [Fact]
    public void WithMockAndRules()
    {
        var request = MemoryDecisions.IsCurrent("il fatto", "il candidato");
        var rules = new RuleDecisionProvider().Evaluate(
            DecisionSlots.CreateNoul("sì, il fatto è ancora valido"))[0];
        MemoryDecisions.ToVerdict(rules).Verdict.Should().Be(MemoryVerdict.Strong);

        var mock = new MockDecisionProvider("false", 1.0).Evaluate(request)[0];
        MemoryDecisions.ToVerdict(mock).Verdict.Should().Be(MemoryVerdict.None);
    }
}
