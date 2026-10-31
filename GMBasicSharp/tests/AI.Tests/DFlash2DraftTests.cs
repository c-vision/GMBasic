// FILE: AI.Tests.DFlash2DraftTests
using Basic.AI.Speculative;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Verifica della pipeline DFlash2Draft in GMBasic: il producer (forward del modello)
/// fornisce candidati+logits+embedding; la path selector produce un blocco coerente. Con
/// producer non disponibile -> draft vuoto (greedy fallback, mai stub).</summary>
[Trait("Category", "Fast")]
public sealed class DFlash2DraftTests
{
    private static float[] Emb(int t)
    {
        var e = new float[DFlash2Selector.EmbDim];
        e[t == 1 ? 0 : 1] = 1f;
        return e;
    }

    private sealed class MockProducer : IDFlash2BlockProducer
    {
        public DFlash2Block? Result;
        public DFlash2Block? Produce(int lastVerified, float[] ctx) => Result;
    }

    [Fact]
    public void Draft_UsesSelectorToProduceCoherentBlock()
    {
        var p = new MockProducer
        {
            Result = new DFlash2Block
            {
                Candidates = new[] { new[] { 1, 2 }, new[] { 1, 2 } },
                U = new[] { new[] { 0.9f, 0.1f }, new[] { 0.1f, 0.9f } },
                H = new[] { new float[DFlash2Selector.EmbDim], new float[DFlash2Selector.EmbDim] },
                A = Emb, B = Emb,
            },
        };
        var d = new DFlash2Draft(p);
        var block = d.Draft(lastVerifiedToken: 0, ctxFeature: [], maxDraft: 8);
        block.Should().Equal(1, 2);
    }

    [Fact]
    public void Draft_ProducerUnavailable_ReturnsEmpty()
    {
        var p = new MockProducer { Result = null };
        var d = new DFlash2Draft(p);
        d.Draft(0, [], 8).Should().BeEmpty();
    }

    [Fact]
    public void Draft_RespectsMaxDraft()
    {
        var p = new MockProducer
        {
            Result = new DFlash2Block
            {
                Candidates = new[] { new[] { 1, 2 }, new[] { 1, 2 } },
                U = new[] { new[] { 0.9f, 0.1f }, new[] { 0.1f, 0.9f } },
                H = new[] { new float[DFlash2Selector.EmbDim], new float[DFlash2Selector.EmbDim] },
                A = Emb, B = Emb,
            },
        };
        var d = new DFlash2Draft(p);
        d.Draft(0, [], maxDraft: 1).Should().Equal(1); // taglia a 1
    }
}
