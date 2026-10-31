// FILE: Basic.AI.Providers.DifferentialRouter — T3.1 (arXiv:2608.30224), 2026-09-09.
// CORE euristico deterministico del "Differential Reasoning Router": calcola il Token Economy
// Score (TES) di un task e decide se può andare su un modello locale/piccolo o se deve scalare al
// frontier. TES alto = task corto e a bassa difficoltà → locale conviene; TES basso = task lungo
// o di ragionamento → frontier. Euristica v1 documentata: promptTokens ≈ len/3.2, costo vs una
// soglia di ~3500 token, penalità di difficoltà (keyword reasoning/analisi). Nessuna dipendenza
// da LLM: pura funzione, testabile in modo deterministico. L'integrazione nel router reale
// (LlmCompleterResolver) resta la superficie applicativa (aperta).
using System;
using System.Collections.Generic;

namespace Basic.AI.Providers;

public enum RouterTier { Local, Frontier }

public sealed record RoutingDecision(RouterTier Tier, double Tes, string Reason);

public static class DifferentialRouter
{
    public const double LocalThreshold = 0.5;

    private static readonly string[] HardKeywords =
    {
        // ragionamento/analisi/confronto/critica/riassunto critico
        "analizza", "analizza", "confronta", "sintetizza", "critica", "valuta", "spiega",
        "motiva", "ragiona", "reason", "analyze", "evaluate", "compare", "synthesize",
        "explain", "debug", "debugga", "bug", "refactor", "review", "legal", "lawsuit",
        "matematica", "prova", "theorem", "derive", "derivazione", "ottimizza", "optimize",
    };

    /// <summary>Difficoltà euristica 0..1: conteggio keyword di ragionamento normalizzato con
    /// 1-exp(-k) (k = occorrenze, soglia pratica ~5).</summary>
    public static double DifficultyOf(string? prompt)
    {
        if (string.IsNullOrEmpty(prompt)) return 0.0;
        var lower = prompt.ToLowerInvariant();
        var hits = 0;
        foreach (var kw in HardKeywords)
        {
            var idx = 0;
            while ((idx = lower.IndexOf(kw, idx, StringComparison.Ordinal)) >= 0)
            {
                hits++;
                idx += kw.Length;
            }
        }
        return 1.0 - Math.Exp(-Math.Min(hits, 12));
    }

    /// <summary>Token Economy Score 0..1: economia di lasciare il task al modello piccolo.
    /// Costo di contesto (prompt+stima risposta) vs ~3500 token, penalizzato dalla difficoltà.</summary>
    public static double TokenEconomyScore(string? prompt, int estAnswerTokens = 256)
    {
        var promptTokens = Math.Ceiling((prompt?.Length ?? 0) / 3.2);
        var total = Math.Max(1.0, promptTokens + Math.Max(estAnswerTokens, 1));
        var cost = Math.Min(1.0, 3500.0 / total);
        var difficulty = DifficultyOf(prompt);
        // Penalità di difficoltà 0.55: un task di ragionamento non deve restare sul piccolo
        // nemmeno se il contesto è corto (il costo dell'errore > risparmio del token).
        return Math.Clamp(cost - difficulty * 0.55, 0.0, 1.0);
    }

    /// <summary>Decide il tier: TES ≥ <see cref="LocalThreshold"/> → Local, altrimenti Frontier.</summary>
    public static RoutingDecision Decide(string? prompt, int estAnswerTokens = 256)
    {
        var tes = TokenEconomyScore(prompt, estAnswerTokens);
        if (tes >= LocalThreshold)
            return new RoutingDecision(RouterTier.Local, tes,
                "task corto e a bassa difficoltà: il modello locale basta (TES " + tes.ToString("0.00") + ")");
        return new RoutingDecision(RouterTier.Frontier, tes,
            "task lungo o di ragionamento: scala al modello grande (TES " + tes.ToString("0.00") + " < " + LocalThreshold + ")");
    }
}
