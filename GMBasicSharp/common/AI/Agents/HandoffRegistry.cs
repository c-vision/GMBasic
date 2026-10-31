// FILE: Basic.Agents.HandoffRegistry — Orchestrazione agenti (A9): Handoff.
// Trasferimento di un task da un agente a un altro: registro (task → agente destinatario) con
// stato (pending/done) e risultato. Il risultato può essere fornito dall'agente destinatario
// (HANDOFFCOMPLETE) oppure calcolato dal runtime. Nessuna duplicazione di contenuto.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_handoff.c):
// stesse operazioni, stesso formato JSON. Thread-safe.
using System.Collections.Concurrent;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe degli handoff. HANDOFF/COMPLETE/CLEAR scrivono,
/// STATUS/LIST leggono.</summary>
public static class HandoffRegistry
{
    private sealed class Entry
    {
        public string Task = "";
        public string To = "";
        public string Status = "pending";
        public string Result = "";
    }

    private static readonly ConcurrentDictionary<string, Entry> _h =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Registra/aggiorna un handoff: task → agente destinatario (stato pending).
    /// 1 = ok, 0 = task/agente vuoti.</summary>
    public static double Handoff(string taskId, string toAgent)
    {
        if (string.IsNullOrWhiteSpace(taskId) || string.IsNullOrWhiteSpace(toAgent)) return 0.0;
        _h[taskId] = new Entry { Task = taskId, To = toAgent, Status = "pending", Result = "" };
        return 1.0;
    }

    /// <summary>Marca l'handoff come completato col risultato. 1 = ok, 0 = task inesistente.</summary>
    public static double Complete(string taskId, string result)
    {
        if (string.IsNullOrWhiteSpace(taskId) || !_h.TryGetValue(taskId, out var h)) return 0.0;
        h.Status = "done";
        h.Result = result ?? "";
        return 1.0;
    }

    /// <summary>JSON dell'handoff ({} se assente).</summary>
    public static string Status(string taskId)
        => !string.IsNullOrWhiteSpace(taskId) && _h.TryGetValue(taskId, out var h) ? Json(h) : "{}";

    /// <summary>JSON di tutti gli handoff, ordinati per task.</summary>
    public static string ListJson()
    {
        var keys = _h.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var sb = new StringBuilder();
        sb.Append("{\"handoffs\":[");
        for (var i = 0; i < keys.Length; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append(Json(_h[keys[i]]));
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Svuota il registro. Ritorna il numero di handoff rimossi.</summary>
    public static double Clear()
    {
        var n = _h.Count;
        _h.Clear();
        return n;
    }

    /// <summary>Serializza lo stato: record `task\x1fto\x1fstatus\x1fresult` separati da \x1e.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        foreach (var kv in _h)
        {
            if (!first) sb.Append('\u001e');
            first = false;
            sb.Append(kv.Value.Task).Append('\u001f').Append(kv.Value.To).Append('\u001f')
              .Append(kv.Value.Status).Append('\u001f').Append(kv.Value.Result);
        }
        return sb.ToString();
    }

    /// <summary>Ripristina lo stato da <see cref="ExportState"/>.</summary>
    public static void ImportState(string state)
    {
        if (string.IsNullOrEmpty(state)) return;
        foreach (var rec in state.Split('\u001e'))
        {
            var f = rec.Split('\u001f');
            if (f.Length < 3) continue;
            Handoff(f[0], f[1]);
            if (f[2] == "done") Complete(f[0], f.Length >= 4 ? f[3] : "");
        }
    }

    private static string Json(Entry h)
    {
        var sb = new StringBuilder();
        sb.Append("{\"task\":\"").Append(Escape(h.Task))
          .Append("\",\"to\":\"").Append(Escape(h.To))
          .Append("\",\"status\":\"").Append(Escape(h.Status))
          .Append("\",\"result\":\"").Append(Escape(h.Result)).Append("\"}");
        return sb.ToString();
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
