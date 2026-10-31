// FILE: AI.Tests.SpeculativeDecodeTests
using Basic.AI.Speculative;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Verifica deterministico (senza modello/GPU) del loop speculative n-gram
/// PARITY-GUARANTEED (SpeculativeDecode.Generate): l'output deve essere IDENTICO al decode
/// greedy puro, senza token duplicati, indipendentemente dalla qualita' dei draft proposti.
/// Il mock "modello" simula le primitive native (verify_draft_batch, next_greedy, commit)
/// con una sequenza greedy ciclica deterministico.</summary>
[Trait("Category", "Fast")]
public sealed class SpeculativeDecodeTests
{
    /// <summary>Mock del motore: greedy deterministico ciclico. Position = lunghezza KV
    /// (0 = dopo il prompt). verify_batch accetta i draft che coincidono col greedy per
    /// posizione e avanza la KV dei token accettati; commit avanza di 1 solo se il token e'
    /// il greedy corrente; next_greedy e' il greedy alla Position corrente.</summary>
    private sealed class MockModel
    {
        private readonly int[] _greedy;
        public int Position;

        public MockModel(int[] greedy) => _greedy = greedy;
        private int At(int pos) => _greedy[pos % _greedy.Length];

        /// <summary>Sequenza greedy pura (senza speculative) da Position 0 — il riferimento.</summary>
        public int[] PureGreedy(int n)
        {
            var r = new int[n];
            for (var i = 0; i < n; i++) r[i] = At(i);
            return r;
        }

        public int NextGreedy() => At(Position);
        public bool Commit(int t)
        {
            if (t != At(Position)) return false;
            Position++;
            return true;
        }
        public int VerifyBatch(int[] draft)
        {
            var acc = 0;
            for (var i = 0; i < draft.Length; i++)
            {
                if (draft[i] == At(Position + i)) acc++;
                else break;
            }
            Position += acc;
            return acc;
        }
    }

    [Fact]
    public void Generate_WithWrongDrafts_MatchesPureGreedy_NoDuplicates()
    {
        // Modello greedy ciclico [3,4,5,6,7, ...]. Prompt [1,2] (il drafter lo usa solo per
        // proporre; il modello e' arbitrario rispetto al prompt).
        var model = new MockModel([3, 4, 5, 6, 7, 3, 4, 5, 6, 7]);
        var drafter = new NGramDrafter();
        // Drafter con una continuazione SBAGLIATA (9,8,...) dopo il prefisso [1,2] -> accepted==0
        // (percorso mismatch-commit) finche' il drafter ha contesto.
        drafter.Learn([1, 2, 9, 8, 7, 6, 5]);

        var output = SpeculativeDecode.Generate(
            drafter, [1, 2], maxTokens: 10, maxDraft: 8, minDraft: 2, eosTokenId: -1,
            verifyDraftBatch: model.VerifyBatch,
            nextGreedy: model.NextGreedy,
            commitToken: model.Commit);

        output.Should().Equal(model.PureGreedy(10));
        // Nessun token duplicato consecutivo (parita' col greedy: niente ri-emissione).
        for (var i = 1; i < output.Length; i++)
            output[i].Should().NotBe(output[i - 1], "il greedy ciclico non ha ripetizioni consecutive");
    }

    [Fact]
    public void Generate_WithCorrectDrafts_MatchesPureGreedy_AcceptPath()
    {
        var model = new MockModel([3, 4, 5, 6, 7, 3, 4, 5, 6, 7]);
        var drafter = new NGramDrafter();
        // Drafter che conosce la continuazione corretta -> accept path (draft interamente accettati).
        drafter.Learn([1, 2, 3, 4, 5, 6, 7, 3, 4]);

        var output = SpeculativeDecode.Generate(
            drafter, [1, 2], maxTokens: 10, maxDraft: 8, minDraft: 2, eosTokenId: -1,
            verifyDraftBatch: model.VerifyBatch,
            nextGreedy: model.NextGreedy,
            commitToken: model.Commit);

        output.Should().Equal(model.PureGreedy(10));
    }

    [Fact]
    public void Generate_StopsAtEos()
    {
        // Greedy [3,4,5,99(EOS),3,...]: lo speculative deve fermarsi al primo EOS.
        var model = new MockModel([3, 4, 5, 99, 3, 4, 5]);
        var drafter = new NGramDrafter();
        drafter.Learn([1, 2, 9, 8, 7]); // draft sbagliati -> accepted==0/mismatch path

        var output = SpeculativeDecode.Generate(
            drafter, [1, 2], maxTokens: 10, maxDraft: 8, minDraft: 2, eosTokenId: 99,
            verifyDraftBatch: model.VerifyBatch,
            nextGreedy: model.NextGreedy,
            commitToken: model.Commit);

        output.Should().Equal(3, 4, 5); // tronca al primo 99 (EOS)
    }

    [Fact]
    public void Generate_RespectsMaxTokens()
    {
        var model = new MockModel([3, 4, 5, 6, 7]);
        var drafter = new NGramDrafter();
        drafter.Learn([1, 2, 3, 4, 5]); // draft corretti

        var output = SpeculativeDecode.Generate(
            drafter, [1, 2], maxTokens: 3, maxDraft: 8, minDraft: 2, eosTokenId: -1,
            verifyDraftBatch: model.VerifyBatch,
            nextGreedy: model.NextGreedy,
            commitToken: model.Commit);

        output.Should().HaveCount(3);
        output.Should().Equal(3, 4, 5);
    }
}
