// FILE: AI.Tests.SuffixDrafterTests
using Basic.AI.Speculative;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

/// <summary>Verifica deterministica (senza modello/GPU) del SuffixDrafter (port di
/// suffix_decoding.py rapidMLX, SuffixDecoding NeurIPS 2025): voto maggioritario
/// sulle occorrenze precedenti del suffisso corrente + troncamento per confidenza,
/// e del loop speculative parity-guaranteed quando usato con SpeculativeDecode.</summary>
[Trait("Category", "Fast")]
public sealed class SuffixDrafterTests
{
    [Fact]
    public void Seed_ThenDraft_ReturnsMajorityContinuation()
    {
        // La storia termina con [10,20,30,40,50] che e' gia' occorso PRIMA, seguito da
        // 60,70,80. Il drafter deve proporre la continuazione [60,70,80] per voto.
        var d = new SuffixDrafter();
        d.AddPromptTokens([10, 20, 30, 40, 50, 60, 70, 80, 10, 20, 30, 40, 50]);

        var draft = d.Draft([10, 20, 30, 40, 50], maxDraft: 8, minDraft: 2);

        draft.Should().NotBeEmpty();
        draft.Should().ContainInOrder(60, 70, 80);
    }

    [Fact]
    public void Draft_NoMatch_ReturnsEmpty()
    {
        var d = new SuffixDrafter();
        d.AddPromptTokens([1, 2, 3, 4]);

        var draft = d.Draft([99, 98, 97, 96, 95], maxDraft: 8, minDraft: 2);

        draft.Should().BeEmpty();
    }

    [Fact]
    public void Seed_ThenDraft_AmbiguousContinuation_PicksMajority()
    {
        // La storia termina con [1,2,3], occorso PRIMA due volte con continuazioni
        // diverse (4 e 9): il voto maggioritario propone uno dei due (confidenza 0.5
        // >= soglia), non una corsa univoca.
        var d = new SuffixDrafter { MaxSuffixLen = 4, MinConfidence = 0.3 };
        d.AddPromptTokens([1, 2, 3, 4, 1, 2, 3, 9, 1, 2, 3]);

        var draft = d.Draft([1, 2, 3], maxDraft: 4, minDraft: 1);

        draft.Should().NotBeEmpty();
        draft[0].Should().BeOneOf(4, 9);
    }

    /// <summary>Mock del motore: greedy deterministico ciclico (stesso di SpeculativeDecodeTests).</summary>
    private sealed class MockModel
    {
        private readonly int[] _greedy;
        public int Position;

        public MockModel(int[] greedy) => _greedy = greedy;
        private int At(int pos) => _greedy[pos % _greedy.Length];

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
    public void Generate_WithSuffixDrafter_MatchesPureGreedy()
    {
        // Prompt [1,2], greedy ciclico [3,4,5,6,7]. Il SuffixDrafter (seminato col
        // prompt dall'overload) propone dai suffissi osservati; la parita' e' garantita
        // dal verify indipendentemente dalla qualita' dei draft.
        var model = new MockModel([3, 4, 5, 6, 7, 3, 4, 5, 6, 7]);
        var drafter = new SuffixDrafter();

        var output = SpeculativeDecode.Generate(
            drafter, [1, 2], maxTokens: 10, maxDraft: 8, minDraft: 2, eosTokenId: -1,
            verifyDraftBatch: model.VerifyBatch,
            nextGreedy: model.NextGreedy,
            commitToken: model.Commit);

        output.Should().Equal(model.PureGreedy(10));
    }

    [Fact]
    public void Generate_WithSeededSuffixDrafter_AcceptsCorrectDraft()
    {
        // Semina il drafter con una storia che contiene la continuazione corretta del
        // greedy: il suffisso [3,4,5,6,7] occorre PRIMA, seguito da 3 -> il draft
        // proposto coincide col greedy e viene accettato (accettazione > 0).
        var model = new MockModel([3, 4, 5, 6, 7, 3, 4, 5, 6, 7]);
        var drafter = new SuffixDrafter();
        drafter.AddPromptTokens([1, 2, 3, 4, 5, 6, 7, 3, 4, 5, 6, 7]);

        var acceptedDrafts = 0;
        var output = SpeculativeDecode.Generate(
            drafter, [1, 2], maxTokens: 10, maxDraft: 8, minDraft: 2, eosTokenId: -1,
            verifyDraftBatch: draft =>
            {
                var acc = model.VerifyBatch(draft);
                if (acc > 0) acceptedDrafts++;
                return acc;
            },
            nextGreedy: model.NextGreedy,
            commitToken: model.Commit);

        output.Should().Equal(model.PureGreedy(10));
        acceptedDrafts.Should().BeGreaterThan(0, "la storia seminata deve produrre draft accettati");
    }
}
