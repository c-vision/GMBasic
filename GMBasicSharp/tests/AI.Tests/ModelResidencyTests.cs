// M21 — Registry di residenza unificato multi-provider: politica LRU (memoria iniettabile).
using System.Collections.Generic;
using System.Linq;
using Basic.Providers;
using Xunit;

namespace AI.Tests;

[Collection("ModelResidency")]
public class ModelResidencyTests : System.IDisposable
{
    private readonly System.Func<ulong> _origAvail = ModelResidencyRegistry.AvailableBytes;

    public void Dispose()
    {
        ModelResidencyRegistry.AvailableBytes = _origAvail;
        foreach (var e in ModelResidencyRegistry.Snapshot()) ModelResidencyRegistry.Remove(e.Name);
    }

    private static void Clear()
    {
        foreach (var e in ModelResidencyRegistry.Snapshot()) ModelResidencyRegistry.Remove(e.Name);
    }

    [Fact]
    public void EnsureCapacity_FreesAcrossProviders_Lru()
    {
        Clear();
        ModelResidencyRegistry.AvailableBytes = static () => 0;   // memoria sempre insufficiente
        var demoted = new List<string>();
        var evicted = new List<string>();
        // ordine di uso: a < b < c (a è il LRU)
        ModelResidencyRegistry.Register("a", "mlx", 100, ModelResidencyState.Active, evict: () => evicted.Add("a"));
        ModelResidencyRegistry.Register("b", "llamacpp", 200, ModelResidencyState.Warm, evict: () => evicted.Add("b"));
        ModelResidencyRegistry.Register("c", "mlx", 100, ModelResidencyState.Active,
                                        evict: () => evicted.Add("c"), demote: () => demoted.Add("c"));

        bool ok = ModelResidencyRegistry.EnsureCapacity(1);

        Assert.False(ok);
        Assert.Equal(new[] { "c" }, demoted);            // prima DEMOTE (unico con demote)...
        Assert.Equal(new[] { "a", "b", "c" }, evicted);  // ...poi EVICT di tutti (LRU), mem. sempre 0
        Assert.Empty(ModelResidencyRegistry.Snapshot());
    }

    [Fact]
    public void EnsureCapacity_EvictsLowestPriorityFirst_NotJustLru()
    {
        Clear();
        ModelResidencyRegistry.AvailableBytes = static () => 0;
        var evicted = new List<string>();
        // "high" registrato per PRIMO (LRU) ma priorità alta; "low" recente ma priorità bassa.
        ModelResidencyRegistry.Register("high", "mlx", 100, ModelResidencyState.Warm,
                                        evict: () => evicted.Add("high"), priority: 5);
        ModelResidencyRegistry.Register("low", "llamacpp", 100, ModelResidencyState.Warm,
                                        evict: () => evicted.Add("low"), priority: 0);

        ModelResidencyRegistry.EnsureCapacity(1);

        Assert.Equal(new[] { "low", "high" }, evicted);   // priorità batte il LRU
    }

    [Fact]
    public void Register_TracksDeviceState_Gpu()
    {
        Clear();
        ModelResidencyRegistry.Register("g", "mlx", 100, ModelResidencyState.Active, device: "metal");
        Assert.Equal("metal", ModelResidencyRegistry.Snapshot().Single().Device);
        ModelResidencyRegistry.SetDevice("g", "cpu");
        Assert.Equal("cpu", ModelResidencyRegistry.Snapshot().Single().Device);
        Assert.Contains("@cpu", ModelResidencyRegistry.Describe());
    }

    [Fact]
    public void EnsureCapacity_SatisfiedImmediately_NoActions()
    {
        Clear();
        ModelResidencyRegistry.AvailableBytes = static () => 1_000_000;
        var evicted = new List<string>();
        ModelResidencyRegistry.Register("x", "mlx", 100, ModelResidencyState.Active, evict: () => evicted.Add("x"));

        Assert.True(ModelResidencyRegistry.EnsureCapacity(1000));
        Assert.Empty(evicted);
        Assert.Single(ModelResidencyRegistry.Snapshot());
    }
}
