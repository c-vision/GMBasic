// FILE: Basic.AI.Speculative.SpeculativeDecode
// Loop speculative PARITY-GUARANTEED: ogni token emesso viene committato in KV
// ESATTAMENTE una volta, in ordine greedy -> output identico al decode greedy, senza duplicati.
//
// Il DRAFT e' astratto come delegate (prefix, maxDraft, minDraft) -> token candidati:
//   - T2: NGramDrafter (lookup hash a 3 livelli, niente modello).
//   - T3: modello DRAFT (DFlash2 / piccolo) che propone un blocco di token via forward.
// Il learning (cache n-gram) e' opzionale: il modello draft non impara.
// Primitive di verifica del target native, esposte come delegate:
//   - verifyDraftBatch(draft): forward batch dei K draft -> numero di token accettati (committati
//     in KV dal backend); il backend lascia pending_next_id = token corretto al primo mismatch
//     (o il nuovo greedy dopo il draft se tutti accettati).
//   - nextGreedy(): greedy corrente della sessione senza avanzare (pending_next_id), o <0 errore.
//   - commitToken(token): avanza la KV di 1 col token dato (deve coincidere col greedy corrente).
//
// La verifica e' SEMPRE del target (authoritative): il drafter propone solo per accelerare. Il
// pattern qui (a differenza del loop generico SpeculativeEngine) committa esplicitamente il token
// corretto al mismatch invece di lasciarlo ri-verificare, garantendo parita' col greedy anche con
// accepted==0 (niente ri-emissione duplicata dello stesso token senza avanzare la KV).
namespace Basic.AI.Speculative;

public static class SpeculativeDecode
{
    public static int[] Generate(
        Func<int[], int, int, int[]> draft,   // (prefix, maxDraft, minDraft) -> token candidati
        Action<int[]>? learn,                  // learning opzionale (n-gram cache); null = modello draft
        int[] prefix,
        int maxTokens,
        int maxDraft,
        int minDraft,
        int eosTokenId,
        Func<int[], int> verifyDraftBatch,
        Func<int> nextGreedy,
        Func<int, bool> commitToken)
    {
        var result = new List<int>();
        if (prefix is null || prefix.Length == 0 || maxTokens <= 0) return [];

        while (result.Count < maxTokens)
        {
            var proposed = draft(prefix, maxDraft, minDraft);
            if (proposed.Length == 0)
            {
                // Nessun draft: emette il greedy corrente e lo committa (avanza KV di 1).
                // L'EOS NON va incluso nell'output (come session_generate): stop prima di emetterlo.
                var greedy = nextGreedy();
                if (greedy < 0) break;
                if (greedy == eosTokenId) break;
                if (!commitToken(greedy)) break;
                result.Add(greedy);
                prefix = Append(prefix, greedy);
                learn?.Invoke(prefix[^Math.Min(NGramDrafter.ContextWindow, prefix.Length)..]);
                continue;
            }

            var accepted = verifyDraftBatch(proposed);
            if (accepted < 0) break;

            // Token accettati (gia' in KV via batch): emettili, fermandoti a maxTokens. Se un
            // token accettato e' l'EOS, la generazione termina PRIMA di emetterlo (come il
            // decode greedy).
            for (var i = 0; i < accepted && result.Count < maxTokens; i++)
            {
                if (proposed[i] == eosTokenId) return result.ToArray();
                result.Add(proposed[i]);
                prefix = Append(prefix, proposed[i]);
            }
            if (result.Count >= maxTokens) break;

            if (accepted == proposed.Length)
            {
                // Tutto il draft accettato: il greedy dopo il draft e' il primo del prossimo giro.
                learn?.Invoke(proposed[..accepted]);
                continue;
            }

            // Mismatch parziale (incl. accepted==0): il token corretto e' il greedy corrente
            // della sessione (pending_next_id gia' allineato dal backend dopo i token accettati).
            // Lo committiamo esplicitamente -> avanza la KV di 1, niente duplicati, parita' greedy.
            var correct = nextGreedy();
            if (correct < 0) break;
            if (correct == eosTokenId) break;
            if (!commitToken(correct)) break;
            result.Add(correct);
            prefix = Append(prefix, correct);
            learn?.Invoke(proposed[..Math.Min(accepted, proposed.Length)]);
        }
        return result.ToArray();
    }

    /// <summary>Convenience overload per il drafter n-gram (T2): adatta NGramDrafter al
    /// delegate draft + learning (la cache di sessione e' calcolata qui). Mantiene i test.</summary>
    public static int[] Generate(
        NGramDrafter drafter,
        int[] prefix,
        int maxTokens,
        int maxDraft,
        int minDraft,
        int eosTokenId,
        Func<int[], int> verifyDraftBatch,
        Func<int> nextGreedy,
        Func<int, bool> commitToken)
    {
        if (prefix is null || prefix.Length == 0) return [];
        var sessionKey = drafter.MakeSessionKey(prefix[^Math.Min(NGramDrafter.ContextWindow, prefix.Length)..]);
        return Generate(
            (p, mx, mn) => drafter.Draft(p, mx, mn),
            acc => drafter.UpdateCache(sessionKey, acc),
            prefix, maxTokens, maxDraft, minDraft, eosTokenId,
            verifyDraftBatch, nextGreedy, commitToken);
    }

    /// <summary>Convenience overload per il drafter a suffissi (suffix decoding, T8):
    /// semina l'indice col prefisso iniziale; il drafter si auto-sincronizza dal
    /// prefisso crescente nel delegate draft (learn non serve).</summary>
    public static int[] Generate(
        SuffixDrafter drafter,
        int[] prefix,
        int maxTokens,
        int maxDraft,
        int minDraft,
        int eosTokenId,
        Func<int[], int> verifyDraftBatch,
        Func<int> nextGreedy,
        Func<int, bool> commitToken)
    {
        if (drafter is null || prefix is null || prefix.Length == 0) return [];
        drafter.AddPromptTokens(prefix);
        return Generate(
            (p, mx, mn) => drafter.Draft(p, mx, mn),
            learn: null,
            prefix, maxTokens, maxDraft, minDraft, eosTokenId,
            verifyDraftBatch, nextGreedy, commitToken);
    }

    private static int[] Append(int[] arr, int t)
    {
        var r = new int[arr.Length + 1];
        Array.Copy(arr, r, arr.Length);
        r[^1] = t;
        return r;
    }
}
