using Basic.Agents;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class ReasoningTests
{
    [Fact]
    public void PickWinner_MajorityWins()
    {
        Reasoning.PickWinner(["A", "B", "A"]).Should().Be("A");
    }

    [Fact]
    public void PickWinner_TiePrefersFirstReachingMax()
    {
        Reasoning.PickWinner(["X", "Y", "X", "Y", "X"]).Should().Be("X");
        Reasoning.PickWinner(["Y", "X", "Y", "X", "Y"]).Should().Be("Y");
    }

    [Fact]
    public void PickWinner_EmptyAndNullFiltered()
    {
        Reasoning.PickWinner(["", "GOOD", null!]).Should().Be("GOOD");
        Reasoning.PickWinner([]).Should().Be("");
    }

    [Fact]
    public void BestOfN_SamplesExactlyN_AndVotesWinner()
    {
        var calls = 0;
        // Campione stocastico simulato: 2 volte "A", 1 volta "B" -> vince A.
        var result = Reasoning.BestOfN(() => ++calls == 3 ? "B" : "A", 3);
        calls.Should().Be(3);
        result.Should().Be("A");
    }

    [Fact]
    public void SequentialScale_ReinjectsErrorUntilAccepted()
    {
        var feedbacks = new List<string>();
        // Il tentativo 2 usa il feedback del 1 (re-iniezione) e produce un risultato accettato.
        var result = Reasoning.SequentialScale(
            (round, feedback) =>
            {
                feedbacks.Add(feedback);
                return round == 1 ? "BAD" : "GOOD";
            },
            r => r == "GOOD" ? (true, null) : (false, "il risultato deve contenere GOOD"),
            maxRounds: 3);

        result.Should().Be("GOOD");
        feedbacks.Count.Should().Be(2);
        feedbacks[1].Should().Contain("deve contenere GOOD");
    }

    [Fact]
    public void SequentialScale_AllFail_ThrowsExplicit()
    {
        var ex = Record.Exception(() => Reasoning.SequentialScale(
            (_, _) => "BAD",
            r => r == "GOOD" ? (true, null) : (false, "non valido"),
            maxRounds: 2));
        ex.Should().NotBeNull();
        ex!.Message.Should().Contain("nessun risultato accettato");
    }

    [Fact]
    public void BestOfN_AllEmpty_ThrowsExplicit()
    {
        // Best-of-N con campioni tutti vuoti = fallimento ESPLICITO, mai silenzioso.
        var ex = Record.Exception(() => Reasoning.BestOfN(() => "", 3));
        ex.Should().NotBeNull();
        ex!.Message.Should().Contain("nessun campione valido");
    }

    [Fact]
    public void BestOfN_NBelowOne_ClampsToOne()
    {
        var calls = 0;
        Reasoning.BestOfN(() => { calls++; return "ok"; }, 0);
        calls.Should().Be(1);
    }
}
