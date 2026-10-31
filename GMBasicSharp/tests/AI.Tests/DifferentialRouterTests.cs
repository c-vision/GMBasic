using Basic.AI.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class DifferentialRouterTests
{
    [Fact]
    public void ShortSimplePrompt_RoutesToLocal()
    {
        var d = DifferentialRouter.Decide("Qual è la capitale dell'Italia?", 32);
        d.Tier.Should().Be(RouterTier.Local);
        d.Tes.Should().BeGreaterThanOrEqualTo(DifferentialRouter.LocalThreshold);
    }

    [Fact]
    public void LongHardReasoningPrompt_RoutesToFrontier()
    {
        var hard = new string('x', 4000) + " analizza confronta e sintetizza la dimostrazione,"
            + " valuta criticamente il refactor ottimizzando il teorema matematico e derivando il lemma.";
        var d = DifferentialRouter.Decide(hard, 512);
        d.Tier.Should().Be(RouterTier.Frontier);
        d.Tes.Should().BeLessThan(DifferentialRouter.LocalThreshold);
    }

    [Fact]
    public void Tes_IsBoundedAndMonotonicInLength()
    {
        DifferentialRouter.TokenEconomyScore("breve", 32).Should().BeInRange(0, 1);
        DifferentialRouter.TokenEconomyScore(new string('y', 100), 32)
            .Should().BeGreaterThan(DifferentialRouter.TokenEconomyScore(new string('y', 12000), 1024));
    }

    [Fact]
    public void Difficulty_ClimbsWithReasoningKeywords()
    {
        DifferentialRouter.DifficultyOf("analizza il codice").Should()
            .BeGreaterThan(DifferentialRouter.DifficultyOf("salva il file"));
    }
}
