// FILE: Basic.Agents.AgentInsertTrigger
// C2 — ON INSERT EXECUTE AGENT: trigger su insert/update di documenti in una collection RAG.
// Registri un trigger (collection → agente + template prompt); quando AiVectorRuntime indicizza
// (IndexTextWithMetadata outcome Indexed/Updated) l'agente viene eseguito (Ask) sul testo.
// Fire avviene su un task (non blocca l'insert); la storia dell'esecuzione è tracciata via
// AgentEventLog (kind Asked). Mai stub: registro con getter espliciti.

using System.Collections.Concurrent;

namespace Basic.Agents;

public sealed record InsertTrigger(string Db, string Collection, string AgentName, string PromptTemplate);

public static class AgentInsertTrigger
{
    private static readonly ConcurrentDictionary<string, InsertTrigger> Triggers = new(StringComparer.OrdinalIgnoreCase);
    private static readonly ConcurrentDictionary<string, long> FireCount = new(StringComparer.OrdinalIgnoreCase);

    public static string Key(string db, string collection) => $"{db}|{collection}";

    /// <summary>Registra un trigger: dopo ogni insert in (db,collection) esegue
    /// AgentServer.Ask(agentName, template con {sourceId} e {text}).</summary>
    public static void Register(string db, string collection, string agentName, string promptTemplate)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(db);
        ArgumentException.ThrowIfNullOrWhiteSpace(collection);
        ArgumentException.ThrowIfNullOrWhiteSpace(agentName);
        if (!AgentServer.Instance.List().Any(a => a.Name.Equals(agentName, StringComparison.OrdinalIgnoreCase)))
            throw new KeyNotFoundException($"Agent '{agentName}' non esiste — registra l'agente prima del trigger");
        Triggers[Key(db, collection)] = new InsertTrigger(db, collection, agentName, promptTemplate ?? "Elabora il documento inserito: {sourceId}");
    }

    public static bool Unregister(string db, string collection) => Triggers.TryRemove(Key(db, collection), out _);
    public static void Clear() { Triggers.Clear(); FireCount.Clear(); }
    public static InsertTrigger? Get(string db, string collection) =>
        Triggers.TryGetValue(Key(db, collection), out var t) ? t : null;
    public static List<InsertTrigger> List() => [.. Triggers.Values];
    public static long Fired(string db, string collection) =>
        FireCount.TryGetValue(Key(db, collection), out var n) ? n : 0;

    /// <summary>Chiamato da AiVectorRuntime dopo un insert/update riuscito. Esegue l'agente su
    /// un task in background (mai sul thread dell'insert); conteggia i fire.</summary>
    public static void OnInserted(string db, string collection, string sourceId, string text, object outcome)
    {
        var trigger = Get(db, collection);
        if (trigger is null) return;
        FireCount.AddOrUpdate(Key(db, collection), 1, (_, v) => v + 1);
        var prompt = (trigger.PromptTemplate ?? "Elabora il documento: {sourceId}")
            .Replace("{sourceId}", sourceId).Replace("{text}", (text ?? "").Length > 4000 ? text[..4000] : text ?? "");
        _ = System.Threading.Tasks.Task.Run(() =>
        {
            try
            {
                AgentServer.Instance.Ask(trigger.AgentName, prompt);
            }
            catch { /* il trigger non deve rompere l'insert */ }
        });
    }
}
