// FILE: AI.Tests.NGramDrafterTests
using Basic.AI.Speculative;
using FluentAssertions;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class NGramDrafterTests
{
    [Fact]
    public void Learn_ThenDraft_ReturnsObservedContinuation()
    {
        var d = new NGramDrafter();
        // Insegna: ... IMPORT DBF USE FILE ALIAS X ... — il drafter deve proporre la
        // continuazione dopo "IMPORT DBF".
        var seq = new[] { 10, 20, 30, 40, 50, 60, 70, 80 }; // token arbitrari
        d.Learn(seq);

        // Prefix che termina con una sotto-sequenza osservata -> draft = continuazione.
        var prefix = new[] { 10, 20, 30, 40, 50 };
        var draft = d.Draft(prefix, maxDraft: 8, minDraft: 2);

        draft.Should().NotBeEmpty();
        draft.Should().ContainInOrder(60, 70); // la continuazione di 50
    }

    [Fact]
    public void Draft_NoMatch_ReturnsEmpty()
    {
        var d = new NGramDrafter();
        d.Learn(new[] { 1, 2, 3, 4 });
        var draft = d.Draft(new[] { 99, 98, 97, 96, 95 });
        draft.Should().BeEmpty();
    }

    [Fact]
    public void SessionKey_IsPerProjectId()
    {
        var d = new NGramDrafter();
        var w = new[] { 1, 2, 3 };
        var k1 = d.MakeSessionKey(w);
        d.ProjectId = "altro";
        var k2 = d.MakeSessionKey(w);
        k1.Should().NotBe(k2, "chiavi di sessione isolate per progetto");
    }

    [Fact]
    public void UpdateCache_PopulatesSessionLru()
    {
        var d = new NGramDrafter();
        var key = d.MakeSessionKey(new[] { 1, 2, 3 });
        d.UpdateCache(key, new[] { 1, 2, 3, 4, 5 });
        // Il successivo Draft su un prefisso che finisce in 3 deve trovare 4,5 via session/context.
        var draft = d.Draft(new[] { 0, 1, 2, 3 }, maxDraft: 4, minDraft: 1);
        draft.Should().NotBeEmpty();
        draft.Should().Contain(4);
    }

    [Fact]
    public void ProjectLevel_WalksInvertedIndex()
    {
        var d = new NGramDrafter();
        // Insegna: token 5 -> 6, 6 -> 7, 7 -> 8 (catena).
        d.Learn(new[] { 5, 6 });
        d.Learn(new[] { 6, 7 });
        d.Learn(new[] { 7, 8 });
        var draft = d.Draft(new[] { 0, 0, 0, 5 }, maxDraft: 4, minDraft: 2);
        draft.Should().NotBeEmpty();
        draft.Should().ContainInOrder(6, 7, 8);
    }
}
