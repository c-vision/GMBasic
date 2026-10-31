// FILE: Basic.Agents.CheckpointRegistry — Orchestrazione agenti (B7): checkpoint/recovery.
// Salvataggio di uno stato di sessione (checkpoint) e ripresa (RESUME): consente di riprendere
// un lavoro agentico interrotto. Deterministico, in-memory.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_checkpoint.c):
// stesse operazioni, stesso formato JSON. Thread-safe. Persistenza su disco = follow-on.
using System.Collections.Concurrent;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe dei checkpoint di sessione. CHECKPOINT/CLEAR scrivono,
/// RESUME/LIST leggono.</summary>
public static class CheckpointRegistry
{
    private static readonly ConcurrentDictionary<string, string> _cp =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Salva (o aggiorna) il checkpoint di una sessione. 1 = ok, 0 = id vuoto.</summary>
    public static double Checkpoint(string sessionId, string state)
    {
        if (string.IsNullOrWhiteSpace(sessionId)) return 0.0;
        _cp[sessionId] = state ?? "";
        return 1.0;
    }

    /// <summary>Stato del checkpoint ("" se assente).</summary>
    public static string Resume(string sessionId)
        => !string.IsNullOrWhiteSpace(sessionId) && _cp.TryGetValue(sessionId, out var s) ? s : "";

    /// <summary>JSON di tutti i checkpoint, ordinati per id.</summary>
    public static string ListJson()
    {
        var keys = _cp.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var sb = new StringBuilder();
        sb.Append("{\"checkpoints\":[");
        for (var i = 0; i < keys.Length; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append("{\"session\":\"").Append(Escape(keys[i]))
              .Append("\",\"state\":\"").Append(Escape(_cp[keys[i]])).Append("\"}");
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Svuota il registro. Ritorna il numero di checkpoint rimossi.</summary>
    public static double Clear()
    {
        var n = _cp.Count;
        _cp.Clear();
        return n;
    }

    /// <summary>Serializza lo stato: record `session\x1fstate` separati da \x1e.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        foreach (var kv in _cp)
        {
            if (!first) sb.Append('\u001e');
            first = false;
            sb.Append(kv.Key).Append('\u001f').Append(kv.Value);
        }
        return sb.ToString();
    }

    /// <summary>Ripristina lo stato da <see cref="ExportState"/>.</summary>
    public static void ImportState(string state)
    {
        if (string.IsNullOrEmpty(state)) return;
        foreach (var rec in state.Split('\u001e'))
        {
            var i = rec.IndexOf('\u001f');
            if (i > 0) Checkpoint(rec[..i], rec[(i + 1)..]);
        }
    }

    private static string Escape(string s)
    {
        var sb = new StringBuilder(s.Length + 2);
        foreach (var c in s)
        {
            switch (c)
            {
                case '"': sb.Append("\\\""); break;
                case '\\': sb.Append("\\\\"); break;
                case '\n': sb.Append("\\n"); break;
                case '\r': sb.Append("\\r"); break;
                case '\t': sb.Append("\\t"); break;
                default: sb.Append(c); break;
            }
        }
        return sb.ToString();
    }
}
