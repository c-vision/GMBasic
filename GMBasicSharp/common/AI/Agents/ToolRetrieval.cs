// FILE: Basic.Agents.ToolRetrieval — N1 Needle3 (tool router a retrieval).
using System;
using System.Collections.Generic;
using Basic.Mcp;

namespace Basic.Agents;

/// <summary>Tool con punteggio di rilevanza 0..1 (N1, Needle3).</summary>
public sealed record RankedTool(McpToolDefinition Tool, double Score);

/// <summary>Ranker di tool sostituibile (N1, Needle3). L'implementazione lessicale è il default
/// deterministico (nessun modello); un ranker a embedding può sostituirla via questa interfaccia.</summary>
public interface IToolRanker
{
    IReadOnlyList<RankedTool> Rank(string prompt, IReadOnlyList<McpToolDefinition> tools, int topK);
}

/// <summary>Ranker lessicale deterministico (N1, Needle3): overlap di token tra prompt e
/// nome+descrizione del tool. Score = token del prompt coperti / token del prompt.
/// Sostituibile con un ranker a embedding senza cambiare il resto del pipeline.</summary>
public sealed class LexicalToolRanker : IToolRanker
{
    public IReadOnlyList<RankedTool> Rank(string prompt, IReadOnlyList<McpToolDefinition> tools, int topK)
    {
        var promptTokens = Tokenize(prompt);
        var scored = new List<RankedTool>(tools.Count);
        foreach (var tool in tools)
        {
            if (tool is null) continue;
            var toolTokens = Tokenize(tool.Name + " " + (tool.Description ?? string.Empty));
            double score = 0.0;
            if (promptTokens.Count > 0 && toolTokens.Count > 0)
            {
                int hit = 0;
                foreach (var tok in promptTokens)
                    if (toolTokens.Contains(tok)) hit++;
                score = (double)hit / promptTokens.Count;
            }
            scored.Add(new RankedTool(tool, score));
        }
        scored.Sort((a, b) => b.Score.CompareTo(a.Score));
        if (topK < scored.Count) scored.RemoveRange(topK, scored.Count - topK);
        return scored;
    }

    internal static HashSet<string> Tokenize(string text)
    {
        var set = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (string.IsNullOrEmpty(text)) return set;
        int start = -1;
        for (int i = 0; i <= text.Length; i++)
        {
            bool isWord = i < text.Length && (char.IsLetterOrDigit(text[i]) || text[i] == '_');
            if (isWord && start < 0) start = i;
            if (!isWord && start >= 0)
            {
                if (i - start >= 2) set.Add(text.Substring(start, i - start).ToLowerInvariant());
                start = -1;
            }
        }
        return set;
    }
}

/// <summary>Risultato della selezione tool (N1, Needle3).</summary>
public sealed class ToolSelection
{
    public IReadOnlyList<McpToolDefinition> Tools { get; init; } = Array.Empty<McpToolDefinition>();
    public bool Escalated { get; init; }
    public EscalationLevel EscalationLevel { get; init; } = EscalationLevel.ToolModel;
    public EscalationReason EscalationReason { get; init; } = EscalationReason.None;
}

/// <summary>Router a retrieval per il catalogo tool (N1, Needle3): retrieve → rank → select →
/// validate → execute → escalate. Il catalogo NON entra nel prompt: solo i top-k candidati.
/// Opt-in spento di default: finché non è abilitato, il comportamento resta quello attuale
/// (tutto il catalogo nel prompt). Quando il recupero non trova candidati sopra soglia, il
/// pipeline escala con motivo NO_TOOL (ramo testato offline); il gancio live ripiega sulla
/// lista completa per non regredire (nessun comportamento nuovo senza copertura).</summary>
public static class ToolRetrieval
{
    public static int TopK { get; set; } = 8;
    public static int MinCatalogSize { get; set; } = 16;
    public static double MinScore { get; set; } = 0.05;
    public static IToolRanker DefaultRanker { get; set; } = new LexicalToolRanker();

    public static ToolSelection SelectTools(
        string prompt,
        IReadOnlyList<McpToolDefinition> tools,
        EscalationPolicy? policy = null,
        IToolRanker? ranker = null)
    {
        policy ??= new EscalationPolicy();
        ranker ??= DefaultRanker;
        if (tools is null || tools.Count == 0)
            return Escalate(policy, EscalationReason.NoTool);

        var ranked = ranker.Rank(prompt ?? string.Empty, tools, TopK);
        var kept = new List<McpToolDefinition>();
        foreach (var r in ranked)
        {
            if (r is not null && r.Score >= MinScore) kept.Add(r.Tool);
        }
        if (kept.Count == 0)
            return Escalate(policy, EscalationReason.NoTool);

        return new ToolSelection { Tools = kept };
    }

    private static ToolSelection Escalate(EscalationPolicy policy, EscalationReason reason)
    {
        var stub = new DecisionResult
        {
            Confidence = 0.0,
            Source = ConfidenceSource.Heuristic,
            Calibration = CalibrationStatus.Uncalibrated,
        };
        var (level, r) = policy.Decide(EscalationLevel.ToolModel, stub, reason);
        return new ToolSelection
        {
            Tools = Array.Empty<McpToolDefinition>(),
            Escalated = true,
            EscalationLevel = level,
            EscalationReason = r,
        };
    }
}
