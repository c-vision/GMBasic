// DecisionProviderTests.cs — contratto P1 del decision layer (Mock/Rules/Registry).
// Non richiede modello né rete: verifica control flow, tipizzazione del valore e fallback.
using Basic.Decision;
using Xunit;

namespace Basic.IlBackend.Tests;

public sealed class DecisionProviderTests
{
    private static DecisionRequest Noul(string state) => new(
        state, DecisionKind.Noul,
        [new DecisionOption("false", "false"), new DecisionOption("true", "true")]);

    private static DecisionRequest Choice(string state) => new(
        state, DecisionKind.Choice,
        [new DecisionOption("billing"), new DecisionOption("technical"), new DecisionOption("sales")]);

    [Fact]
    public void Mock_Selects_First_Option_And_Maps_Noul_Value()
    {
        var provider = new MockDecisionProvider(probability: 0.9);
        var result = provider.Evaluate(Noul("qualunque cosa")).Single();

        Assert.Equal("false", result.OptionId);
        Assert.Equal(false, result.Value);
        Assert.Equal(0.9, result.Probability);
        Assert.Null(result.Calibrated);          // mai copiato da Probability
    }

    [Fact]
    public void Mock_Forced_Option_Maps_Choice_Value()
    {
        var provider = new MockDecisionProvider("technical");
        var result = provider.Evaluate(Choice("x")).Single();

        Assert.Equal("technical", result.OptionId);
        Assert.Equal("technical", result.Value);
    }

    [Fact]
    public void Rules_Choose_By_Keyword()
    {
        var provider = new RuleDecisionProvider();

        Assert.Equal(true, provider.Evaluate(Noul("il cliente ha detto SÌ")).Single().Value);
        Assert.Equal(false, provider.Evaluate(Noul("no, non è fraud")).Single().Value);
        Assert.Equal("billing", provider.Evaluate(Choice("fattura")).Single().OptionId);
    }

    [Fact]
    public void Registry_Uses_Fallback_When_Primary_Throws()
    {
        var registry = new DecisionProviderRegistry()
            .Register(new ThrowingProvider("primary"))
            .Register(new MockDecisionProvider("true", name: "mock"));

        var result = registry.Evaluate(Noul("x"), preferred: "primary", fallback: ["mock"]).Single();

        Assert.Equal("true", result.OptionId);
        Assert.Equal(true, result.Value);
    }

    [Fact]
    public void Registry_Skips_Provider_Without_Capability()
    {
        var registry = new DecisionProviderRegistry()
            .Register(new NoulOnlyProvider("noul-only"))
            .Register(new MockDecisionProvider("sales", name: "mock"));

        var result = registry.Evaluate(Choice("x"), preferred: "noul-only").Single();

        Assert.Equal("sales", result.OptionId);
    }

    [Fact]
    public void Request_Validation_Rejects_Empty_And_Duplicate_Options()
    {
        Assert.Throws<ArgumentException>(() => new DecisionRequest("x", DecisionKind.Choice, []).Validate());
        Assert.Throws<ArgumentException>(() => new DecisionRequest("x", DecisionKind.Choice,
            [new DecisionOption("a"), new DecisionOption("A")]).Validate());
        Assert.Throws<ArgumentException>(() => new DecisionRequest("x", DecisionKind.Noul,
            [new DecisionOption("only")]).Validate());
    }

    private sealed class ThrowingProvider(string name) : ISystemOneProvider
    {
        public string Name { get; } = name;
        public DecisionCapabilities Capabilities => DecisionCapabilities.Noul | DecisionCapabilities.Choice;
        public IReadOnlyList<DecisionResult> Evaluate(DecisionRequest request, CancellationToken cancellationToken = default)
            => throw new InvalidOperationException("boom");
    }

    private sealed class NoulOnlyProvider(string name) : ISystemOneProvider
    {
        public string Name { get; } = name;
        public DecisionCapabilities Capabilities => DecisionCapabilities.Noul;
        public IReadOnlyList<DecisionResult> Evaluate(DecisionRequest request, CancellationToken cancellationToken = default)
            => throw new InvalidOperationException("non deve essere chiamato per CHOICE");
    }
}

public sealed class DecisionSlotsTests
{
    [Fact]
    public void Canonical_Slots_Are_2_6_16()
    {
        Assert.Equal(2, DecisionSlots.Noul.Count);
        Assert.Equal(6, DecisionSlots.Score.Count);
        Assert.Equal(16, DecisionSlots.Choice.Count);
        Assert.Equal(["false", "true"], DecisionSlots.Noul.Select(o => o.Id));
        Assert.Equal(["0", "1", "2", "3", "4", "5"], DecisionSlots.Score.Select(o => o.Id));
        Assert.Equal(["A", "P"], new[] { DecisionSlots.Choice[0].Id, DecisionSlots.Choice[^1].Id });
    }

    [Fact]
    public void Choice_Maps_Labels_To_Stable_Slots()
    {
        var request = DecisionSlots.CreateChoice("x", ["billing", "technical", "sales"]);
        Assert.Equal(DecisionKind.Choice, request.Kind);
        Assert.Equal(3, request.Options.Count);
        Assert.Equal("A", request.Options[0].Id);
        Assert.Equal("billing", request.Options[0].Label);
        Assert.Equal("C", request.Options[2].Id);
        Assert.Equal("sales", request.Options[2].Label);
        request.Validate();
    }

    [Fact]
    public void Choice_Rejects_More_Than_16()
    {
        var labels = Enumerable.Range(0, 17).Select(i => $"o{i}").ToArray();
        Assert.Throws<ArgumentException>(() => DecisionSlots.CreateChoice("x", labels));
    }
}
