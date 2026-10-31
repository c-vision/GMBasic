// FILE: Basic.Agents.HookRegistry — Orchestrazione agenti (A3): Lifecycle Hooks.
// Registro degli hook per evento (before_task/after_task/before_agent/.../on_handoff):
// ogni evento può avere più handler (nomi di SUB/FUNCTION). La FIRE risolve gli handler
// registrati per l'evento; l'invocazione automatica ai lifecycle point (GOSUB con symbol
// resolution, come ON KEY) è il follow-on — qui si fornisce la fondazione deterministica.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_hook.c):
// stessa tassonomia di eventi, stesso formato JSON.
using System.Collections.Concurrent;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe degli hook di lifecycle. REGISTER/UNREGISTER scrivono,
/// LIST/EVENTS/FIRE leggono. Eventi validati contro la tassonomia canonica.</summary>
public static class HookRegistry
{
    /// <summary>Tassonomia canonica degli eventi (ordine stabile, parita' con il C).</summary>
    public static readonly string[] Events =
    {
        "before_task", "after_task",
        "before_agent", "after_agent",
        "before_tool", "after_tool",
        "before_model", "after_model",
        "on_error", "on_retry", "on_complete", "on_verify", "on_handoff",
    };

    private static readonly ConcurrentDictionary<string, ConcurrentDictionary<string, bool>> _hooks =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>True se l'evento è nella tassonomia canonica (case-insensitive).</summary>
    public static bool IsKnownEvent(string ev)
    {
        foreach (var e in Events)
            if (string.Equals(e, ev, StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }

    /// <summary>Registra un handler per un evento. 1 = ok, 0 = evento sconosciuto/handler vuoto.</summary>
    public static double Register(string ev, string handler)
    {
        if (string.IsNullOrWhiteSpace(ev) || string.IsNullOrWhiteSpace(handler)) return 0.0;
        if (!IsKnownEvent(ev)) return 0.0;
        var set = _hooks.GetOrAdd(ev, _ => new ConcurrentDictionary<string, bool>(StringComparer.OrdinalIgnoreCase));
        set[handler] = true;
        return 1.0;
    }

    /// <summary>Rimuove un handler da un evento. 1 = rimosso, 0 = assente/evento sconosciuto.</summary>
    public static double Unregister(string ev, string handler)
    {
        if (string.IsNullOrWhiteSpace(ev) || string.IsNullOrWhiteSpace(handler)) return 0.0;
        if (!_hooks.TryGetValue(ev, out var set)) return 0.0;
        return set.TryRemove(handler, out _) ? 1.0 : 0.0;
    }

    /// <summary>JSON di tutti gli hook: {"hooks":{"<event>":["h1","h2"],...}} (eventi noti, ordinati).</summary>
    public static string ListJson()
    {
        var sb = new StringBuilder();
        sb.Append("{\"hooks\":{");
        var first = true;
        foreach (var ev in Events)
        {
            if (!_hooks.TryGetValue(ev, out var set) || set.IsEmpty) continue;
            if (!first) sb.Append(',');
            first = false;
            sb.Append('"').Append(Escape(ev)).Append("\":[");
            var handlers = set.Keys.ToArray();
            System.Array.Sort(handlers, StringComparer.OrdinalIgnoreCase);
            for (var i = 0; i < handlers.Length; i++)
            {
                if (i > 0) sb.Append(',');
                sb.Append('"').Append(Escape(handlers[i])).Append('"');
            }
            sb.Append(']');
        }
        sb.Append("}}");
        return sb.ToString();
    }

    /// <summary>JSON dell'elenco degli eventi noti: {"events":["before_task",...]}.</summary>
    public static string EventsJson()
    {
        var sb = new StringBuilder();
        sb.Append("{\"events\":[");
        for (var i = 0; i < Events.Length; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append('"').Append(Escape(Events[i])).Append('"');
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Risolve gli handler per un evento e DISPACCA quelli che sono agenti (GPM):
    /// {"event":..,"handlers":[...],"agents":{"<nome>":"<risposta>"},"payload":..}.
    /// Handler non-agente resta in "handlers" (SUB, Fase 2). Evento sconosciuto → {"error":..}.</summary>
    public static string Fire(string ev, string payload)
    {
        if (string.IsNullOrWhiteSpace(ev) || !IsKnownEvent(ev))
            return "{\"error\":\"evento hook sconosciuto\"}";
        var handlers = new List<string>();
        if (_hooks.TryGetValue(ev, out var set) && !set.IsEmpty)
        {
            handlers.AddRange(set.Keys);
            handlers.Sort(StringComparer.OrdinalIgnoreCase);
        }
        var sb = new StringBuilder();
        sb.Append("{\"event\":\"").Append(Escape(ev)).Append("\",\"handlers\":[");
        for (var i = 0; i < handlers.Count; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append('"').Append(Escape(handlers[i])).Append('"');
        }
        sb.Append("],\"agents\":{");
        var first = true;
        foreach (var h in handlers)
        {
            if (!AgentServer.Instance.Exists(h)) continue;
            string reply;
            try { reply = AgentServer.Instance.Ask(h, payload ?? ""); }
            catch (Exception ex) { reply = "error: " + ex.Message; }
            if (!first) sb.Append(',');
            first = false;
            sb.Append('"').Append(Escape(h)).Append("\":\"").Append(Escape(reply)).Append('"');
        }
        sb.Append("},\"payload\":\"").Append(Escape(payload ?? "")).Append("\"}");
        return sb.ToString();
    }

    /// <summary>Svuota il registro (test/harness).</summary>
    public static void Clear() => _hooks.Clear();

    /// <summary>Persistenza: `event\x1fhandler` separati da \x1e.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        foreach (var kv in _hooks)
            foreach (var h in kv.Value.Keys)
            {
                if (!first) sb.Append('\u001e');
                first = false;
                sb.Append(kv.Key).Append('\u001f').Append(h);
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
            if (i > 0) Register(rec[..i], rec[(i + 1)..]);
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
