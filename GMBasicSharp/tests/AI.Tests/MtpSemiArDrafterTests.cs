// FILE: AI.Tests.MtpSemiArDrafterTests
// Test deterministico (senza modello/GPU) del drafter semi-autoregressivo MTP:
// - Draft produce una catena coerente condizionata su prefisso confermato + token draftati.
// - Il riallineamento dopo un'accettazione PARZIALE (truncate + ri-append) e' corretto.
// - Integrazione con SpeculativeDecode.Generate: output IDENTICO al greedy puro (parita'),
//   indipendentemente da quanti draft vengono accettati.
using Basic.AI.Speculative;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class MtpSemiArDrafterTests
{
    /// <summary>Mock del modello DRAFT: una sessione con KV = lista token. Il greedy alla
    /// posizione assoluta p (lunghezza KV) e' seq[p] (se oltre, ciclico). Append aggiunge;
    /// Generate(1) predice il greedy a posizione corrente e lo appende (semi-AR chain);
    /// Truncate taglia la KV a n.</summary>
    private sealed class MockDraftSession
    {
        public List<int> Kv { get; } = new();
        public int[] Seq { get; }
        public Func<IntPtr, float>? GreedyProb;

        public MockDraftSession(int[] seq) => Seq = seq;
        private int At(int p) => p < Seq.Length ? Seq[p] : Seq[p % Seq.Length];

        public IntPtr Create() { Kv.Clear(); return (IntPtr)1; }
        public void Destroy(IntPtr _) { }
        public int Append(IntPtr _, int[] ids, int n) { for (int i = 0; i < n; i++) Kv.Add(ids[i]); return 0; }
        public int Generate(IntPtr _, int max, int eos, int[] outIds)
        {
            var produced = 0;
            while (produced < max)
            {
                var t = At(Kv.Count);
                if (t == eos) break;
                outIds[produced++] = t;
                Kv.Add(t);
            }
            return produced;
        }
        public int Truncate(IntPtr _, int n) { while (Kv.Count > n) Kv.RemoveAt(Kv.Count - 1); return 0; }

        public MtpDraftSessionOps Ops() => new()
        {
            CreateSession = Create,
            DestroySession = Destroy,
            Append = Append,
            Generate = Generate,
            Truncate = Truncate,
            GreedyProb = GreedyProb,
        };
    }

    [Fact]
    public void Draft_ProposesCoherentSemiArChain()
    {
        // Draft model: greedy assoluti [1,2,3,4,5,6,7,8,9,10]. Seed = [1,2] (posizioni 0,1).
        // Proposta = [3,4,5,...] — ogni token condiziona i precedenti nella catena.
        var sess = new MockDraftSession([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
        using var dr = new MtpSemiArDrafter(sess.Ops(), eosTokenId: -1);
        dr.Seed([1, 2]).Should().BeTrue();

        var block = dr.Draft([1, 2], maxDraft: 5);

        block.Should().Equal(3, 4, 5, 6, 7);
        dr.MirroredLen.Should().Be(2); // nessun token confermato in piu' dopo il draft
    }

    [Fact]
    public void Draft_ResyncAfterPartialAccept_RealignsToConfirmedPrefix()
    {
        // Draft propone 3,4,9 (il suo greedy) ma il TARGET accetta solo [3,4] e committa 5.
        // Riallineamento: truncate (scarta 9), append 5; il prossimo draft deve partire da 5.
        var sess = new MockDraftSession([1, 2, 3, 4, 9, 9, 9, 9]);
        using var dr = new MtpSemiArDrafter(sess.Ops(), eosTokenId: -1);
        dr.Seed([1, 2]).Should().BeTrue();

        var block = dr.Draft([1, 2], maxDraft: 3);
        block.Should().Equal(3, 4, 9);
        sess.Kv.Should().Equal(1, 2, 3, 4, 9); // sessione draft = prefix + guess

        // Il target accetta [3,4] e committa la correzione 5 => prefisso confermato [1,2,3,4,5].
        var next = dr.Draft([1, 2, 3, 4, 5], maxDraft: 2);

        // Riallineato a [1,2,3,4,5] (9 scartato, 5 appeso) -> il draft model (greedy fissi 9)
        // propone 9,9.
        sess.Kv.Should().Equal(1, 2, 3, 4, 5, 9, 9);
        next.Should().Equal(9, 9);
        dr.MirroredLen.Should().Be(5);
    }

    [Fact]
    public void Draft_FullAccept_NoUnnecessaryTruncate()
    {
        var sess = new MockDraftSession([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
        using var dr = new MtpSemiArDrafter(sess.Ops(), eosTokenId: -1);
        dr.Seed([1, 2]).Should().BeTrue();

        var block = dr.Draft([1, 2], maxDraft: 3);
        block.Should().Equal(3, 4, 5);
        var beforeTruncateCalls = sess.Kv.Count;

        // Full accept: prefisso confermato [1,2,3,4,5] == blocco gia' nella sessione draft:
        // niente truncate (la sessione conserva i suoi token identici).
        var next = dr.Draft([1, 2, 3, 4, 5], maxDraft: 2);
        sess.Kv.Should().Equal(1, 2, 3, 4, 5, 6, 7); // 6,7 appesi in coda senza reset
        next.Should().Equal(6, 7);
        _ = beforeTruncateCalls;
    }

    [Fact]
    public void Draft_ConfidenceGate_TruncatesBlockAtLowConfidence()
    {
        // DSpark confidence-scheduled: il blocco viene troncato al primo token con
        // P(greedy|ctx) del draft sotto soglia. Mock: confidenza 0.9 per i primi 2 token,
        // poi 0.3 (< soglia 0.6) -> il blocco deve fermarsi a 2 token e il 3° va scartato
        // (rollback della sessione draft).
        var sess = new MockDraftSession([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
        var probs = new[] { 0.9f, 0.9f, 0.3f }; // per i primi 3 passi di draft
        var probCalls = 0;
        sess.GreedyProb = _ => probCalls < probs.Length ? probs[probCalls++] : 0.9f;
        using var dr = new MtpSemiArDrafter(sess.Ops(), eosTokenId: -1, confidenceThreshold: 0.6f);
        dr.Seed([1, 2]).Should().BeTrue();

        var block = dr.Draft([1, 2], maxDraft: 5);

        block.Should().Equal(3, 4);            // troncato: il 3° token (5) era a confidenza 0.3
        sess.Kv.Should().Equal(1, 2, 3, 4);    // il token incerto e' stato rollbackato
    }

    [Fact]
    public void Draft_NoConfidenceGate_ProposesFullBlock()
    {
        var sess = new MockDraftSession([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
        using var dr = new MtpSemiArDrafter(sess.Ops(), eosTokenId: -1); // soglia 0 = gate off
        dr.Seed([1, 2]).Should().BeTrue();

        var block = dr.Draft([1, 2], maxDraft: 5);

        block.Should().Equal(3, 4, 5, 6, 7);   // nessun gate: blocco pieno
        sess.Kv.Should().Equal(1, 2, 3, 4, 5, 6, 7);
    }

    [Fact]
    public void Generate_WithMtpDrafter_MatchesPureGreedy()
    {
        // Target: greedy [3,4,5,6,7,3,4,5,6,7]. Draft model PERFETTO (stessi greedy) e uno
        // SBAGLIATO (diverge dopo 2). In entrambi i casi l'output == greedy puro (parita').
        var model = new SpeculativeDecodeTests_MockModel([3, 4, 5, 6, 7, 3, 4, 5, 6, 7]);

        // (a) Draft perfetto -> full accept.
        var sessGood = new MockDraftSession([1, 2, 3, 4, 5, 6, 7, 3, 4, 5, 6, 7]);
        using var good = new MtpSemiArDrafter(sessGood.Ops(), eosTokenId: -1);
        good.Seed([1, 2]);
        var outGood = SpeculativeDecode.Generate(
            (p, mx, mn) => { var d = good.Draft(p, mx); return d.Length >= mn ? d : []; },
            null, [1, 2], maxTokens: 10, maxDraft: 4, minDraft: 2, eosTokenId: -1,
            verifyDraftBatch: model.VerifyBatch,
            nextGreedy: model.NextGreedy,
            commitToken: model.Commit);
        outGood.Should().Equal(3, 4, 5, 6, 7, 3, 4, 5, 6, 7);

        // (b) Draft sbagliato dopo il 2° token -> partial accept + mismatch path (truncate).
        model.Position = 0;
        var sessBad = new MockDraftSession([1, 2, 3, 4, 99, 99, 99, 99, 99]);
        using var bad = new MtpSemiArDrafter(sessBad.Ops(), eosTokenId: -1);
        bad.Seed([1, 2]);
        var outBad = SpeculativeDecode.Generate(
            (p, mx, mn) => { var d = bad.Draft(p, mx); return d.Length >= mn ? d : []; },
            null, [1, 2], maxTokens: 10, maxDraft: 4, minDraft: 2, eosTokenId: -1,
            verifyDraftBatch: model.VerifyBatch,
            nextGreedy: model.NextGreedy,
            commitToken: model.Commit);
        outBad.Should().Equal(3, 4, 5, 6, 7, 3, 4, 5, 6, 7);
    }

    /// <summary>MockModel condiviso (Position/VerifyBatch/Commit/NextGreedy) —
    /// replica del modello a greedy ciclico dei test esistenti.</summary>
    private sealed class SpeculativeDecodeTests_MockModel
    {
        private readonly int[] _greedy;
        public int Position;
        public SpeculativeDecodeTests_MockModel(int[] greedy) => _greedy = greedy;
        private int At(int pos) => _greedy[pos % _greedy.Length];
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
}
