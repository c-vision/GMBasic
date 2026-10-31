// FILE: Basic.Agents.Recursive.LatentWorkingMemory
// Estensione 9.1 (LatentMAS) — working memory latente condivisa tra agenti del team.
// In modalità `TEAM SET LATENT kv` il contesto latente (hidden proiettato) NON viene
// ricalcolato ad ogni membro: viene messo nella working memory e trasferito al membro
// successivo (TransferKv) come fingerprint compatta. Il KV-transfer REALE tra sessioni
// (llama_state_get_data/set_data, in-RAM) è esposto via IKvTransfer.

using System.Collections.Concurrent;
using Basic.AI.Rag;

namespace Basic.Agents.Recursive;

public static class LatentWorkingMemory
{
    // team -> working memory (ultimo contesto latente del team)
    private static readonly ConcurrentDictionary<string, string> _working = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Pone il contesto latente corrente del team (MIRROR del membro appena eseguito).</summary>
    public static void Put(string team, string latentContext)
    {
        if (string.IsNullOrWhiteSpace(team)) return;
        _working[team] = latentContext ?? "";
    }

    /// <summary>Legge il contesto latente del team, o null se non presente.</summary>
    public static string? Get(string team)
        => _working.TryGetValue(team, out var v) ? v : null;

    /// <summary>TransferKv(src, dst): copia il contesto latente dal team sorgente a quello
    /// destinazione (working-memory handoff tra agenti/team, training-free).</summary>
    public static void TransferKv(string srcTeam, string dstTeam)
    {
        if (string.IsNullOrWhiteSpace(srcTeam) || string.IsNullOrWhiteSpace(dstTeam)) return;
        if (_working.TryGetValue(srcTeam, out var v))
            _working[dstTeam] = v;
    }

    /// <summary>TransferKv REALE (LatentMAS 9.1): copia lo stato KV+token della sessione sorgente
    /// nella destinazione via IKvTransfer (llama_state_get_data/set_data, in-RAM, niente I/O).
    /// Ritorna false se il completer non supporta il trasferimento KV.</summary>
    public static bool TransferKv(ILatentCompleter completer, int srcSessionId, int dstSessionId)
        => completer is IKvTransfer kv && kv.TransferKv(srcSessionId, dstSessionId);

    public static void ResetForTests() => _working.Clear();
}
