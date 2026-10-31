// FILE: AI.Tests.WeightedRotationTests
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class WeightedRotationTests : IDisposable
{
    public WeightedRotationTests() => ProviderServer.ResetForTests();

    public void Dispose() => ProviderServer.ResetForTests();

    [Fact]
    public void SetWeight_DefaultIsOne()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Completion, "solo-weight-test", "fixture");
        server.GetWeight("solo-weight-test").Should().Be(1);
    }

    [Fact]
    public void SetWeight_ClampsToMinimumOne()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Completion, "clamp-test", "fixture");
        server.SetWeight("clamp-test", 0);
        server.GetWeight("clamp-test").Should().Be(1);
        server.SetWeight("clamp-test", -5);
        server.GetWeight("clamp-test").Should().Be(1);
    }

    [Fact]
    public void Rotate_WithoutCustomWeights_IsUnaffectedByFeature()
    {
        // Same scenario/expectations as the pre-existing Rotate_CyclesProvidersRoundRobin test —
        // confirms the weighted path is a no-op until SETWEIGHT is actually called.
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Completion, "llm-a", "fixture");
        server.Register(ProviderRole.Completion, "llm-b", "fixture");
        server.SetActive(ProviderRole.Completion, "llm-a");

        server.Rotate(ProviderRole.Completion).Should().Be("llm-b");
        server.Rotate(ProviderRole.Completion).Should().Be("fixture-completion");
        server.Rotate(ProviderRole.Completion).Should().Be("llm-a");
    }

    [Fact]
    public void Rotate_WithCustomWeights_DistributesProportionally()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Completion, "heavy", "fixture");
        server.Register(ProviderRole.Completion, "light", "fixture");
        server.SetWeight("heavy", 3);
        server.SetWeight("light", 1);
        // fixture-completion (bootstrap default) is still in the pool at weight 1 — mark it
        // Blocked so this test only observes the two providers under test.
        server.SetStatus("fixture-completion", ProviderStatus.Blocked);

        var picks = new List<string>();
        for (var i = 0; i < 8; i++)
            picks.Add(server.Rotate(ProviderRole.Completion));

        picks.Count(p => p == "heavy").Should().Be(6);
        picks.Count(p => p == "light").Should().Be(2);
    }

    [Fact]
    public void Rotate_WeightedMode_HigherPriorityTierStillWinsOverWeight()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Completion, "high-priority", "fixture");
        server.SetPriority("high-priority", 100);
        server.Register(ProviderRole.Completion, "weighted-low", "fixture");
        server.SetWeight("weighted-low", 10); // huge weight, but lower priority — must not win
        server.SetStatus("fixture-completion", ProviderStatus.Blocked);

        for (var i = 0; i < 5; i++)
            server.Rotate(ProviderRole.Completion).Should().Be("high-priority");
    }

    [Fact]
    public void Rotate_WeightedMode_SkipsBlockedProviderInTier()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Completion, "a", "fixture");
        server.Register(ProviderRole.Completion, "b", "fixture");
        server.SetWeight("a", 5);
        server.SetStatus("a", ProviderStatus.Blocked);
        server.SetStatus("fixture-completion", ProviderStatus.Blocked);

        server.Rotate(ProviderRole.Completion).Should().Be("b");
    }
}
