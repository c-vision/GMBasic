// FILE: Basic.Agents.Reasoning — T1.1 Thinking Mode (Reasoning Regimes), 2026-09-09.
// Best-of-N sampling+voting per AI-Research-Plan (arXiv:2609.02042 SPACE è T2.1; qui il
// regime "thinking" con campionamento): quando l'agente ha ThinkingEnabled e ReasoningSamples>1
// si campiona N volte la stessa completion e si sceglie il vincitore col VOTO di maggioranza.
// Se un candidato è vuoto viene scartato. I tie si rompono a favore del primo candidato che ha
// raggiunto la frequenza massima (ordine di campionamento stabile).
using System.Collections.Generic;
using System.Linq;

namespace Basic.Agents;

public static class Reasoning
{
    /// <summary>Campiona N volte <paramref name="completeOnce"/> e sceglie il vincitore per
    /// voto di maggioranza (Best-of-N). Candidati null/vuoti scartati. Se N&gt;1 e NESSUN
    /// candidato è valido → errore ESPLICITO (mai silenzioso, parità §0bis/AgentVerifier).</summary>
    public static string BestOfN(Func<string> completeOnce, int n)
    {
        if (n < 1) n = 1;
        var candidates = new List<string>(n);
        for (var i = 0; i < n; i++)
        {
            var sample = completeOnce() ?? "";
            if (sample.Length > 0) candidates.Add(sample);
        }
        if (n > 1 && candidates.Count == 0)
            throw new InvalidOperationException(
                "AI thinking (Best-of-N): nessun campione valido su " + n + " tentativi.");
        return PickWinner(candidates);
    }

    /// <summary>Voto di maggioranza su una lista di candidati. Tie → primo candidato con la
    /// frequenza massima (ordine di campionamento stabile). Vuota → "".</summary>
    public static string PickWinner(IReadOnlyList<string> candidates)
    {
        if (candidates is null || candidates.Count == 0) return "";
        var counts = new Dictionary<string, int>(System.StringComparer.Ordinal);
        string? best = null;
        var bestCount = 0;
        foreach (var c in candidates)
        {
            if (string.IsNullOrEmpty(c)) continue;
            var k = c;
            counts[k] = counts.TryGetValue(k, out var cur) ? cur + 1 : 1;
            if (counts[k] > bestCount)
            {
                bestCount = counts[k];
                best = k;
            }
        }
        return best ?? "";
    }

    /// <summary>T1.1 Sequential Scaling (re-iniezione del verifier, 2026-09-09): esegue fino a
    /// <paramref name="maxRounds"/> tentativi; ogni tentativo riceve l'ERRORE del precedente come
    /// feedback (<paramref name="attempt"/>(roundNumber, feedback), feedback="" al primo).
    /// <paramref name="validate"/> accetta il risultato (ok) oppure produce l'errore da
    /// re-iniettare. Ritorna il primo risultato accettato; se nessuno passa → errore ESPLICITO
    /// (mai silenzioso). Modello: AgentVerifier.Verify + CodeGenLoop (re-iniezione nel prompt).</summary>
    public static string SequentialScale(
        Func<int, string, string> attempt,
        Func<string, (bool Ok, string? Error)> validate,
        int maxRounds = 3)
    {
        if (maxRounds < 1) maxRounds = 1;
        ArgumentNullException.ThrowIfNull(attempt);
        ArgumentNullException.ThrowIfNull(validate);
        var feedback = "";
        for (var round = 1; round <= maxRounds; round++)
        {
            var result = attempt(round, feedback) ?? "";
            if (result.Length == 0)
            {
                feedback = $"Tentativo {round}: risultato vuoto. Correggi e riprova.";
                continue;
            }
            var (ok, error) = validate(result);
            if (ok) return result;
            feedback = error is { Length: > 0 }
                ? $"Tentativo {round}: {error}. Correggi e riprova."
                : $"Tentativo {round}: risultato non valido. Correggi e riprova.";
        }
        throw new InvalidOperationException(
            "Sequential scaling: nessun risultato accettato dopo " + maxRounds + " tentativi. Ultimo feedback: " + feedback);
    }
}
