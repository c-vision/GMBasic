// FILE: Basic.AI.Speculative.DFlash2Draft
// Pipeline DFlash2 in GMBasic: il modello DRAFT (block diffusion) produce, per ogni posizione
// del blocco, i candidati top-K + i logits U + i codebook embedding A/B + il context gate H.
// La DFlash2Selector traccia poi il path coerente (S_t(a,b)=U_t(b)+<A(a)⊙H,B(b)>) -> blocco.
//
// Il produttore dei blocchi (il forward nativo del modello DFlash2: fc + candidate_selector)
// e' astratto da IDFlash2BlockProducer: una volta disponibile il loader del modello (tappa
// backbone+head), questo drafter e' completo. Finche' il forward non c'e', il producer ritorna
// null / non disponibile e lo speculative ricade sul drafter n-gram o greedy (mai stub).
namespace Basic.AI.Speculative;

/// <summary>Un blocco di candidati per posizione, come prodotto dal forward del modello DFlash2.</summary>
public sealed class DFlash2Block
{
    /// <summary>Token candidati per posizione (top-K; candidateId = token id).</summary>
    public required int[][] Candidates;
    /// <summary>Logits U_t(b) del drafter per ogni candidato.</summary>
    public required float[][] U;
    /// <summary>Context gate H(h_t) per posizione (256-dim).</summary>
    public required float[][] H;
    /// <summary>Embedding A(a) per ogni token (codebook predecessor, 256-dim).</summary>
    public required Func<int, float[]> A;
    /// <summary>Embedding B(b) per ogni token (codebook successor, 256-dim).</summary>
    public required Func<int, float[]> B;
}

/// <summary>Produttore del blocco: esegue il forward del modello DFlash2 (target context feature
/// -> backbone -> fc -> candidate_selector) e produce i candidati/logits/embedding per il path
/// selector. Ritorna null se il forward non e' disponibile (loader non ancora implementato).</summary>
public interface IDFlash2BlockProducer
{
    /// <summary>Produce il blocco di candidati dati l'ultimo token verificato e la feature di
    /// contesto del target. Null = non disponibile (mai stub: il chiamante ricade sul greedy).</summary>
    DFlash2Block? Produce(int lastVerifiedToken, float[] targetContextFeature);
}

/// <summary>Drafter DFlash2: usa il producer (forward del modello) + la path selector per
/// proporre un blocco coerente. Se il producer non e' disponibile, ritorna draft vuoto (il
/// chiamante usa il greedy). Puro C#, testabile con un producer mock.</summary>
public sealed class DFlash2Draft
{
    private readonly IDFlash2BlockProducer _producer;

    public DFlash2Draft(IDFlash2BlockProducer producer) => _producer = producer;

    /// <summary>Propone un blocco di draft token (max maxDraft). Ritorna [] se il producer
    /// non e' disponibile o non produce candidati (greedy fallback).</summary>
    public int[] Draft(int lastVerifiedToken, float[] ctxFeature, int maxDraft)
    {
        var block = _producer.Produce(lastVerifiedToken, ctxFeature);
        if (block is null || block.Candidates.Length == 0) return [];

        // Path selector: greedy walk sul lattice (coerenza).
        var walked = DFlash2Selector.GreedyWalk(
            lastVerifiedToken, block.Candidates, block.U,
            block.A, block.B, block.H);

        var take = Math.Min(walked.Length, maxDraft);
        return take > 0 ? walked[..take] : [];
    }
}
