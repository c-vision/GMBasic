// FILE: AI.Tests.PrefixLruCacheTests
using Basic.AI.Rag;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Verifica della cache LRU bounded della T1 prefix cache (eviction
/// Least-Recently-Used oltre la capacita'). Componente puro, testabile senza modello.</summary>
[Trait("Category", "Fast")]
public sealed class PrefixLruCacheTests
{
    [Fact]
    public void EvictsLeastRecentlyUsed_WhenOverCapacity()
    {
        var c = new PrefixLruCache(2);
        c.Put("a", "/a");
        c.Put("b", "/b");
        c.Put("c", "/c"); // over capacity: evict a (LRU)

        c.Contains("a").Should().BeFalse();
        c.Contains("b").Should().BeTrue();
        c.Contains("c").Should().BeTrue();
        c.Count.Should().Be(2);
    }

    [Fact]
    public void Get_TouchesAndPreventsEviction()
    {
        var c = new PrefixLruCache(2);
        c.Put("a", "/a");
        c.Put("b", "/b");
        c.Get("a"); // touch a -> b diventa LRU
        c.Put("c", "/c"); // evict b

        c.Contains("a").Should().BeTrue("a e' stata recentemente usata (toccata)");
        c.Contains("b").Should().BeFalse("b e' la least-recently-used");
        c.Contains("c").Should().BeTrue();
    }

    [Fact]
    public void Put_ReturnsEvictedKey()
    {
        var c = new PrefixLruCache(1);
        c.Put("a", "/a");
        var evicted = c.Put("b", "/b");

        evicted.Should().Be("a");
        c.Contains("a").Should().BeFalse();
    }

    [Fact]
    public void Put_ExistingKey_UpdatesAndTouches()
    {
        var c = new PrefixLruCache(2);
        c.Put("a", "/a");
        c.Put("b", "/b");
        c.Put("a", "/a2"); // aggiorna e tocca a
        c.Put("c", "/c"); // evict b (a recente)

        c.Get("a").Should().Be("/a2");
        c.Contains("b").Should().BeFalse();
    }

    [Fact]
    public void Get_Missing_ReturnsNull_DoesNotTouch()
    {
        var c = new PrefixLruCache(1);
        c.Get("missing").Should().BeNull();
        c.Count.Should().Be(0);
    }
}
