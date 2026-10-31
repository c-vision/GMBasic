// FILE: Basic.AI.Agents.LoopDetector
// Second Brain — §3 Loop Detection: 5 meccanismi per fermare gli agenti in loop:
// 1) Action dedup: stessa (tool, args hash) ≥ N volte → stop
// 2) Cycle memory: messaggio "ho già provato X" nel prompt
// 3) State probe: output identico consecutivo → loop
// 4) Budget: limite di token/azioni
// 5) Reflection on loop: nota nel brain quando un loop viene rilevato
using System.Collections.Concurrent;
using System.Security.Cryptography;
using System.Text;

namespace Basic.AI.Agents;

public sealed record LoopVerdict(bool IsLoop, string? Reason, string? Details);

public static class LoopDetector
{
    private const int DefaultActionThreshold = 3;

    // per-agente: (tool, hash) → count
    private static readonly ConcurrentDictionary<string, ConcurrentDictionary<string, int>> _actionCounts = new(StringComparer.OrdinalIgnoreCase);
    // per-agente: ultimo output (per state probe)
    private static readonly ConcurrentDictionary<string, string> _lastOutput = new(StringComparer.OrdinalIgnoreCase);
    // per-agente: token budget spesi
    private static readonly ConcurrentDictionary<string, long> _tokenBudget = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Registra un'azione (tool + args) per l'agente e ritorna il verdetto.
    /// Se la stessa azione è stata eseguita ≥ <paramref name="threshold"/> volte → loop.</summary>
    public static LoopVerdict TrackAction(string agent, string tool, string args, int threshold = DefaultActionThreshold)
    {
        var key = Hash(tool + "|" + NormalizeArgs(args));
        var map = _actionCounts.GetOrAdd(agent, _ => new ConcurrentDictionary<string, int>(StringComparer.Ordinal));
        var count = map.AddOrUpdate(key, 1, (_, v) => v + 1);
        if (count >= threshold)
            return new LoopVerdict(true, $"action-dedup", $"tool '{tool}' eseguito {count}× con stessi argomenti");
        return new LoopVerdict(false, null, null);
    }

    /// <summary>State probe: se l'output coincide col precedente consecutivo → loop.</summary>
    public static LoopVerdict TrackOutput(string agent, string output, int repeatsThreshold = 3)
    {
        var norm = NormalizeArgs(output ?? "");
        if (_lastOutput.TryGetValue(agent, out var prev) && prev == norm)
        {
            var count = _actionCounts.GetOrAdd(agent + ":out", _ => new ConcurrentDictionary<string, int>());
            var c = count.AddOrUpdate("same", 1, (_, v) => v + 1);
            _lastOutput[agent] = norm;
            if (c >= repeatsThreshold)
                return new LoopVerdict(true, "state-probe", $"output identico {c} volte consecutive");
        }
        else
        {
            _actionCounts.GetOrAdd(agent + ":out", _ => new ConcurrentDictionary<string, int>())["same"] = 0;
            _lastOutput[agent] = norm;
        }
        return new LoopVerdict(false, null, null);
    }

    /// <summary>Budget: decrementa il budget token/azioni. Ritorna loop se esaurito.</summary>
    public static LoopVerdict TrackBudget(string agent, long tokens, long maxTokens)
    {
        var spent = _tokenBudget.AddOrUpdate(agent, tokens, (_, v) => v + tokens);
        if (spent >= maxTokens)
            return new LoopVerdict(true, "budget", $"budget esaurito: {spent}/{maxTokens} token");
        return new LoopVerdict(false, null, null);
    }

    /// <summary>Cycle memory: stringa da iniettare nel prompt quando l'agente riprova un'azione già
    /// vista (per agente). Ritorna il testo di avviso, o null.</summary>
    public static string? CycleMemoryPrompt(string agent, string tool, string args)
    {
        var key = Hash(tool + "|" + NormalizeArgs(args));
        if (_actionCounts.TryGetValue(agent, out var map) && map.TryGetValue(key, out var count) && count >= 2)
            return $"[AVVISO LOOP] Hai già provato '{tool}' {count} volte con questi argomenti. Cambia strategia.";
        return null;
    }

    /// <summary>Reflection on loop: registra il loop nel brain (come fatto episodico) per
    /// memoria a lungo termine.</summary>
    public static void RecordLoop(string agent, LoopVerdict verdict)
    {
        try
        {
            Basic.AI.MemoryBrain.Add($"Loop rilevato per agente {agent}: {verdict.Reason} — {verdict.Details}",
                Basic.AI.MemoryBrain.MemoryKind.Episodic, source: $"agent:{agent}");
        }
        catch { /* best effort */ }
    }

    public static void ResetForTests() { _actionCounts.Clear(); _lastOutput.Clear(); _tokenBudget.Clear(); }

    private static string NormalizeArgs(string s) =>
        string.Join(' ', (s ?? "").Trim().Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries)).ToLowerInvariant();

    private static string Hash(string s)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(s));
        return Convert.ToHexString(bytes);
    }
}
