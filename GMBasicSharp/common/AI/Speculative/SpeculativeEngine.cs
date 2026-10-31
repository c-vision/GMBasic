// FILE: Basic.AI.Speculative.SpeculativeEngine
// Speculative decoding: DRAFT (NGramDrafter) -> TARGET LLM verify -> accept/rollback.
// Backend-agnostico: il target espone ISpeculativeTarget (verify di K token proposti).
//
// Loop (spec Vision L4):
//   TOKENIZER -> NGRAM DRAFTER (4-16 token) -> TARGET.Verify(draft) -> accetta il piu'
//   lungo prefisso confermato; sul primo mismatch usa il token del target. UpdateCache.
//
// Compiler-aware: se il drafter ha CompilerAware, le proposte privilegiano token
// strutturali (SUB/FUNCTION dentro un body, DBF/ISAM/TORCH dopo IMPORT).
namespace Basic.AI.Speculative;

/// <summary>Il target LLM verifica K token proposti in UN passo. Ritorna il numero di token
/// accettati (0..draft.Length) — i primi `accepted` del draft sono validi, il successivo
/// (se c'e') e' il token corretto del target.</summary>
public interface ISpeculativeTarget
{
    /// <summary>Verifica il draft: ritorna il numero di token accettati (prefisso piu'
    /// lungo coerente col target). Deve anche avanzare lo stato interno dei token accettati.</summary>
    int VerifyDraft(int[] draft, out int nextToken);
}

/// <summary>Estensione opzionale: verify BATCH dei K draft (tappa 2, lo speedup) su una
/// sessione esplicita. Chi non lo implementa (es. mock) ricade su VerifyDraft iterativo.</summary>
public interface ISpeculativeBatchTarget
{
    /// <summary>Verifica i K draft in UN forward batch. Ritorna il numero di token accettati
    /// e il token corretto del modello al primo mismatch (o il nuovo greedy dopo il draft).
    /// Con rollback KV se non tutti i K sono accettati.</summary>
    int VerifyDraftBatch(int sessionId, int[] draft, out int nextToken);
}

/// <summary>Engine speculative: drafter + target + loop accept/rollback. Non tocca il motore
/// nativo direttamente: delega la verifica al target (che puo' essere MLX, llama.cpp, mock).</summary>
public sealed class SpeculativeEngine
{
    private readonly NGramDrafter _drafter;
    private readonly ISpeculativeTarget _target;

    public SpeculativeEngine(NGramDrafter drafter, ISpeculativeTarget target)
    {
        _drafter = drafter;
        _target = target;
    }

    /// <summary>Genera fino a <paramref name="maxTokens"/> token, usando il draft n-gram e la
    /// verifica del target. Ritorna i token accettati (escluso il token "corretto" al primo
    /// mismatch, che e' gia' il primo token del prossimo giro).</summary>
    public int[] Generate(int[] initialPrefix, int maxTokens, int maxDraft = NGramDrafter.DefaultMaxDraft)
    {
        var result = new List<int>();
        var prefix = (int[])initialPrefix.Clone();
        var sessionKey = _drafter.MakeSessionKey(prefix[^Math.Min(NGramDrafter.ContextWindow, prefix.Length)..]);

        while (result.Count < maxTokens)
        {
            var draft = _drafter.Draft(prefix, maxDraft);
            if (draft.Length == 0)
            {
                // Niente draft: chiedi al target il token singolo (draft vuoto -> verify di 1).
                var n = _target.VerifyDraft([], out var single);
                if (n < 0) break; // target esaurito/errore
                result.Add(single);
                prefix = Append(prefix, single);
                _drafter.UpdateCache(sessionKey, prefix[^Math.Min(NGramDrafter.ContextWindow, prefix.Length)..]);
                continue;
            }

            var accepted = _target.VerifyDraft(draft, out var nextToken);
            if (accepted < 0) break; // errore

            // Accetta i token confermati dal target.
            var take = Math.Min(accepted, maxTokens - result.Count);
            for (var i = 0; i < take; i++)
            {
                result.Add(draft[i]);
                prefix = Append(prefix, draft[i]);
            }
            _drafter.UpdateCache(sessionKey, draft[..Math.Min(accepted, draft.Length)]);

            if (accepted < draft.Length && result.Count < maxTokens)
            {
                // Mismatch: il target propone il token corretto al posto del draft[accepted].
                result.Add(nextToken);
                prefix = Append(prefix, nextToken);
            }

            if (accepted == 0 && draft.Length > 0)
            {
                // Nessun token accettato: il target ha dato il token giusto; fallback anti-loop.
                if (result.Count < maxTokens && draft.Length > 0)
                {
                    // Evita loop se nextToken e' gia' in coda.
                    if (result.Count == 0 || result[^1] != nextToken)
                    {
                        result.Add(nextToken);
                        prefix = Append(prefix, nextToken);
                    }
                }
            }
        }

        return result.ToArray();
    }

    private static int[] Append(int[] arr, int t)
    {
        var r = new int[arr.Length + 1];
        Array.Copy(arr, r, arr.Length);
        r[^1] = t;
        return r;
    }
}
