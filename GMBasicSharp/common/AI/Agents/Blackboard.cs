// FILE: Basic.Agents.Blackboard — Fase 2 GMBasic-Agent-Runtime: area di scambio strutturata
// chiave/valore per coordinamento in tempo reale tra task/agenti paralleli. DISTINTA da MemoryBrain
// (memoria semantica a lungo termine): qui è scrittura concorrente + lettura "quando pronto", nessun
// retrieval semantico. Thread-safe (lock per chiave via ConcurrentDictionary + TaskCompletionSource).
using System.Collections.Concurrent;

namespace Basic.Agents;

/// <summary>Blackboard chiave/valore condiviso, concorrente. Scrittura last-write-wins; READ legge
/// lo stato corrente; WAIT si sospende finché una chiave non ha valore (o cambia), con timeout.</summary>
public static class Blackboard
{
    private static readonly ConcurrentDictionary<string, string> _kv = new(StringComparer.OrdinalIgnoreCase);
    private static readonly ConcurrentDictionary<string, System.Threading.Tasks.TaskCompletionSource<string>> _waiters = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Scrive (o aggiorna) la chiave e sveglia i WAIT in attesa. 1 = ok.</summary>
    public static double Write(string key, string value)
    {
        if (string.IsNullOrWhiteSpace(key)) return 0.0;
        _kv[key] = value ?? "";
        if (_waiters.TryRemove(key, out var tcs)) tcs.TrySetResult(value ?? "");
        return 1.0;
    }

    /// <summary>Legge il valore corrente della chiave ("" se assente).</summary>
    public static string Read(string key)
        => !string.IsNullOrWhiteSpace(key) && _kv.TryGetValue(key, out var v) ? v : "";

    /// <summary>Bloccante: attende che la chiave abbia un valore (o cambi), fino a timeoutMs.
    /// Ritorna il valore, o "" se timeout. NB: chiamare fuori da un worker GPM concorrente (parità
    /// col modello blocking esistente); la versione park-based GPM è un follow-on.</summary>
    public static string Wait(string key, int timeoutMs = 10000)
    {
        if (string.IsNullOrWhiteSpace(key)) return "";
        GpmWorkerGuard.Check("Blackboard.Wait (AI.BLACKBOARDWAIT)");
        if (_kv.TryGetValue(key, out var cur)) return cur;   // già pronta
        var tcs = _waiters.GetOrAdd(key, _ => new System.Threading.Tasks.TaskCompletionSource<string>(
            System.Threading.Tasks.TaskCreationOptions.RunContinuationsAsynchronously));
        try
        {
            return tcs.Task.Wait(timeoutMs) ? tcs.Task.Result : "";
        }
        catch { return ""; }
    }

    /// <summary>Rimuove una chiave. 1 = rimossa, 0 = assente.</summary>
    public static double Remove(string key)
        => !string.IsNullOrWhiteSpace(key) && _kv.TryRemove(key, out _) ? 1.0 : 0.0;

    /// <summary>Svuota il blackboard (test/harness).</summary>
    public static void Clear() { _kv.Clear(); _waiters.Clear(); }

    /// <summary>Numero di chiavi (diagnostica).</summary>
    public static int Count => _kv.Count;
}
