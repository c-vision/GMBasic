// FILE: Basic.AI.Speculative.DFlash2Selector
// DFlash2 path selector — sceglie un path COERENTE attraverso la selector lattice del
// drafter block-diffusion. Algoritmo dal blog Inco AI "DFlash 2: Keep Drafting Parallel"
// e dal paper DFlash (arXiv 2602.06036).
//
// DFlash predice ogni posizione in parallelo, mantenendo i top-K candidati. Il path selector
// scora OGNI coppia adiacente (predecessore a, candidato b) con:
//     S_t(a,b) = U_t(b) + <A(a) ⊙ H(h_t), B(b)>
//   - U_t(b) : logit del drafter per il candidato b alla posizione t (quanto lo "gradisce").
//   - A(a), B(b): embedding compatti (256-dim) dei token (codebook predecessor/successor).
//   - H(h_t): context gate (256-dim) che decide quali parti del match contano.
//   - <·>: prodotto interno -> attenzione bilineare low-rank su coppie adiacenti.
// Poi un greedy walk sui punteggi precalcolati (unico step sequenziale): partendo dall'ultimo
// token verificato, segue il miglior successore. E' completamente parallelo (scoring) + greedy.
// Puro C# (nessuna dipendenza nativa) -> testabile senza modello.
namespace Basic.AI.Speculative;

/// <summary>Implementazione della path selector DFlash2. Prende le liste di candidati e i
/// logits dal drafter block-diffusion e produce un blocco di token coerente (max blockSize-1).</summary>
public sealed class DFlash2Selector
{
    /// <summary>Dimensione embedding codebook (A/B). 256 (paper: "compact 256-dimensional").</summary>
    public const int EmbDim = 256;

    /// <summary>Score della coppia adiacente (predecessore a, candidato b) alla posizione t.</summary>
    public static float Score(float uB, float[] aEmb, float[] hGate, float[] bEmb)
    {
        // S_t(a,b) = U_t(b) + <A(a) ⊙ H(h_t), B(b)>
        var dot = 0.0f;
        for (var i = 0; i < EmbDim; i++)
            dot += (aEmb[i] * hGate[i]) * bEmb[i];
        return uB + dot;
    }

    /// <summary>Greedy walk sul lattice: partendo dall'ultimo token verificato, ad ogni posizione
    /// sceglie il candidato b che massimizza S_t(a,b) tra i successori del predecessore a.</summary>
    public static int[] GreedyWalk(
        int lastVerifiedToken,
        IReadOnlyList<int[]> candidates,   // [pos][candidateId] ; candidateId = token id
        IReadOnlyList<float[]> uLogits,    // [pos][candidate] = U_t(b)
        Func<int, float[]> predecessorEmb, // A(a)
        Func<int, float[]> successorEmb,   // B(b)
        IReadOnlyList<float[]> hGates)     // [pos] = H(h_t)
    {
        var block = new List<int>();
        var a = lastVerifiedToken;
        for (var t = 0; t < candidates.Count; t++)
        {
            var cands = candidates[t];
            var Us = uLogits[t];
            var H = hGates[t];
            var aEmb = predecessorEmb(a);
            int best = -1;
            float bestScore = float.NegativeInfinity;
            for (var i = 0; i < cands.Length; i++)
            {
                var b = cands[i];
                var s = Score(Us[i], aEmb, H, successorEmb(b));
                if (s > bestScore) { bestScore = s; best = b; }
            }
            if (best < 0) break;
            block.Add(best);
            a = best;
        }
        return block.ToArray();
    }
}
