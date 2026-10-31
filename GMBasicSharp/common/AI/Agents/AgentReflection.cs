// FILE: Basic.Agents.AgentReflection
// Alimentazione automatica del second brain da parte degli agenti (Fase 3): dopo un turno
// l'agente riflette ed estrae FATTI durevoli (kind Fact) + un RIASSUNTO episodico (kind
// Episodic). Gate per-agente: AI AGENT SET nome, memory, ON. Skip dei completer fixture
// (produrrebbero rumore, non fatti).
using Basic.AI;
using Basic.AI.Rag;
using System.Collections.Concurrent;

namespace Basic.Agents;

public static class AgentReflection
{
    private static readonly ConcurrentDictionary<string, List<string>> _stm = new(StringComparer.OrdinalIgnoreCase);
    private static readonly ConcurrentDictionary<string, List<string>> _ltm = new(StringComparer.OrdinalIgnoreCase);
    private const int StmCap = 8;

    private static bool IsFixture(ILlmCompleter completer) =>
        completer.ModelName.Contains("fixture", StringComparison.OrdinalIgnoreCase);

    /// <summary>Riflette su un turno (prompt + risposta dell'agente) e scrive nel brain:
    /// fatti estratti + riassunto episodico. Best-effort: non deve mai rompere il turno.</summary>
    public static void Reflect(AgentRecord agent, ILlmCompleter completer, string turnPrompt)
    {
        if (!agent.MemoryEnabled || IsFixture(completer)) return;
        try
        {
            foreach (var fact in ExtractFacts(completer, agent, turnPrompt))
                MemoryBrain.Add(fact, MemoryBrain.MemoryKind.Fact, source: $"agent:{agent.Name}");

            var summary = Summarize(completer, agent, turnPrompt);
            if (!string.IsNullOrWhiteSpace(summary))
                MemoryBrain.Add(summary, MemoryBrain.MemoryKind.Episodic, source: $"agent:{agent.Name}");

            // MIRROR (9.5): STM per agente (resettabile a subtask ok) + LTM persistente
            StmAdd(agent.Name, turnPrompt);
            LtmAdd(agent.Name, summary);
        }
        catch { /* la reflection non deve mai rompere il turno */ }
    }

    // ── MIRROR STM/LTM (9.5) ─────────────────────────────────────────────────

    /// <summary>Memoria a breve termine per agente (cap 8, FIFO) — resettata a subtask ok.</summary>
    public static void StmAdd(string agent, string entry)
    {
        var list = _stm.GetOrAdd(agent, _ => []);
        lock (list)
        {
            if (entry.Length > 0) list.Add(entry);
            while (list.Count > StmCap) list.RemoveAt(0);
        }
    }

    public static List<string> StmGet(string agent)
        => _stm.TryGetValue(agent, out var l) ? [.. l] : [];

    public static void StmReset(string agent)
    {
        if (_stm.TryGetValue(agent, out var l)) lock (l) l.Clear();
    }

    /// <summary>Memoria a lungo termine per agente (traiettorie: plan/toolSeq/results).</summary>
    public static void LtmAdd(string agent, string trajectory)
    {
        if (string.IsNullOrWhiteSpace(trajectory)) return;
        var list = _ltm.GetOrAdd(agent, _ => []);
        lock (list)
        {
            list.Add(trajectory);
            if (list.Count > 128) list.RemoveAt(0);
        }
    }

    public static List<string> LtmGet(string agent)
        => _ltm.TryGetValue(agent, out var l) ? [.. l] : [];

    /// <summary>ReTreVal prune (9.5): valuta i candidati con un judge (LLM-as-judge, score 1-10)
    /// e mantiene i top-k. Se il judge è fixture, usa la lunghezza come proxy di qualità.</summary>
    public static List<string> Prune(ILlmCompleter? judge, string agent, IEnumerable<string> candidates, int topK)
    {
        var list = candidates.Where(c => !string.IsNullOrWhiteSpace(c)).ToList();
        if (list.Count <= topK) return list;
        var scored = new List<(string C, double S)>();
        foreach (var c in list)
        {
            double s;
            if (judge is null || IsFixture(judge)) s = Math.Min(10, c.Length / 32.0); // proxy deterministico
            else
            {
                try
                {
                    var t = judge.Complete(agent, $"Valuta da 1 a 10 la qualità di questo risultato di sotto-problema:\n{c}") ?? "";
                    var m = System.Text.RegularExpressions.Regex.Match(t, @"\d+(\.\d+)?", System.Text.RegularExpressions.RegexOptions.RightToLeft);
                    s = m.Success && double.TryParse(m.Value, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out var v) ? v : 5.0;
                }
                catch { s = 5.0; }
            }
            scored.Add((c, s));
        }
        return scored.OrderByDescending(x => x.S).Take(topK).Select(x => x.C).ToList();
    }

    public static void ResetForTests() { _stm.Clear(); _ltm.Clear(); }

    private static List<string> ExtractFacts(ILlmCompleter completer, AgentRecord agent, string turnPrompt)
    {
        var prompt =
            "Sei un estrattore di memoria. Dalla conversazione seguente estrai SOLO i fatti " +
            "dichiarativi DUREVOLI (preferenze, decisioni, informazioni sull'utente o sul " +
            "progetto). Formato: un fatto per riga, senza prefissi. Se non ci sono fatti " +
            "durevoli, non scrivere nulla.\n\n" + turnPrompt;
        var text = completer.Complete(agent.ResolveSystemPrompt(), prompt) ?? "";
        return text.Split('\n').Select(l => l.Trim()).Where(l => l.Length > 3).ToList();
    }

    private static string Summarize(ILlmCompleter completer, AgentRecord agent, string turnPrompt)
    {
        var prompt =
            "Riassumi la conversazione seguente in UNA sola frase, per la memoria a lungo " +
            "termine di un agente.\n\n" + turnPrompt;
        return (completer.Complete(agent.ResolveSystemPrompt(), prompt) ?? "").Trim();
    }
}
