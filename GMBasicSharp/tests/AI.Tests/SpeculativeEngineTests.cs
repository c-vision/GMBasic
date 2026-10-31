// FILE: AI.Tests.SpeculativeEngineTests
using Basic.AI.Speculative;
using FluentAssertions;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class SpeculativeEngineTests
{
    /// <summary>Target mock: accetta i primi `matchLen` token del draft (come se il modello
    /// concordasse) e propone `nextToken` al primo mismatch. Simula il verify nativo.</summary>
    private sealed class MockTarget(int matchLen, int nextToken) : ISpeculativeTarget
    {
        public int VerifyCount { get; private set; }
        public int VerifyDraft(int[] draft, out int next)
        {
            VerifyCount++;
            if (draft.Length == 0) { next = nextToken; return 0; }
            var n = Math.Min(matchLen, draft.Length);
            next = nextToken;
            return n; // accetta i primi `matchLen`
        }
    }

    [Fact]
    public void Generate_AcceptsDraftPrefixAndRollsBackOnMismatch()
    {
        var drafter = new NGramDrafter();
        // Insegna una sequenza osservata: 1,2,3,4,5 (draft = 3,4,5 dopo 1,2).
        drafter.Learn(new[] { 1, 2, 3, 4, 5 });

        // Target accetta 2 token del draft (3,4) e dà 99 al mismatch (il 5 non è confermato).
        var target = new MockTarget(matchLen: 2, nextToken: 99);
        var engine = new SpeculativeEngine(drafter, target);

        var result = engine.Generate(new[] { 1, 2 }, maxTokens: 6, maxDraft: 8);

        result.Should().ContainInOrder(3, 4, 99); // accetta draft[0..1], poi il token corretto
        target.VerifyCount.Should().BeGreaterThan(0);
    }

    [Fact]
    public void Generate_WithNoDraft_AsksSingleTokenFromTarget()
    {
        var drafter = new NGramDrafter(); // nessuna sequenza imparata
        var target = new MockTarget(matchLen: 0, nextToken: 42);
        var engine = new SpeculativeEngine(drafter, target);

        var result = engine.Generate(new[] { 7, 8, 9 }, maxTokens: 4, maxDraft: 8);

        result.Should().Contain(42);
        result.Length.Should().BeLessThanOrEqualTo(4);
    }

    [Fact]
    public void Generate_AllDraftAccepted_NoFallbackNeeded()
    {
        var drafter = new NGramDrafter();
        drafter.Learn(new[] { 10, 11, 12, 13 });

        // Target accetta TUTTO il draft (matchLen = 8 > draft len).
        var target = new MockTarget(matchLen: 8, nextToken: -1);
        var engine = new SpeculativeEngine(drafter, target);

        var result = engine.Generate(new[] { 10, 11 }, maxTokens: 4, maxDraft: 4);

        result.Should().ContainInOrder(12, 13);
    }
}
