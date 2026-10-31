// FILE: Basic.Agents.ReplayRegistry — Orchestrazione agenti (A8): Session Replay.
// Registrazione di una sessione agentica: eventi append-only per sessione, con stato di
// registrazione. REPLAYGET restituisce la sequenza registrata (il "nastro" riproducibile);
// REPLAYLIST elenca le sessioni. Deterministico, nessun LLM.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_replay.c):
// stesse operazioni, stesso formato JSON. Thread-safe. v1 in-memory (persistenza su disco =
// follow-on); la ri-esecuzione automatica degli eventi è un follow-on.
using System.Collections.Concurrent;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe delle sessioni registrate. START/RECORD/STOP scrivono,
/// GET/LIST leggono.</summary>
public static class ReplayRegistry
{
    private sealed class Session
    {
        public string Id = "";
        public bool Recording;
        public readonly List<string> Events = new();
    }

    private static readonly ConcurrentDictionary<string, Session> _sessions =
        new(StringComparer.OrdinalIgnoreCase);
    private static readonly object _lock = new();
    private static string _current = "";

    /// <summary>Avvia (o riavvia) la registrazione di una sessione. 1 = ok, 0 = id vuoto.</summary>
    public static double Start(string sessionId)
    {
        if (string.IsNullOrWhiteSpace(sessionId)) return 0.0;
        lock (_lock)
        {
            var s = new Session { Id = sessionId, Recording = true };
            _sessions[sessionId] = s;
            _current = sessionId;
        }
        return 1.0;
    }

    /// <summary>Registra un evento nella sessione corrente. 1 = ok, 0 = nessuna registrazione attiva.</summary>
    public static double Record(string ev)
    {
        lock (_lock)
        {
            if (string.IsNullOrEmpty(_current) || !_sessions.TryGetValue(_current, out var s) || !s.Recording)
                return 0.0;
            s.Events.Add(ev ?? "");
            return 1.0;
        }
    }

    /// <summary>Ferma la registrazione corrente. 1 = ok, 0 = nessuna attiva.</summary>
    public static double Stop()
    {
        lock (_lock)
        {
            if (string.IsNullOrEmpty(_current) || !_sessions.TryGetValue(_current, out var s)) return 0.0;
            s.Recording = false;
            _current = "";
            return 1.0;
        }
    }

    /// <summary>JSON della sessione ({} se assente).</summary>
    public static string Get(string sessionId)
    {
        if (string.IsNullOrWhiteSpace(sessionId) || !_sessions.TryGetValue(sessionId, out var s)) return "{}";
        var sb = new StringBuilder();
        lock (_lock)
        {
            sb.Append("{\"session\":\"").Append(Escape(s.Id))
              .Append("\",\"recording\":").Append(s.Recording ? "true" : "false")
              .Append(",\"events\":[");
            for (var i = 0; i < s.Events.Count; i++)
            {
                if (i > 0) sb.Append(',');
                sb.Append('"').Append(Escape(s.Events[i])).Append('"');
            }
            sb.Append("]}");
        }
        return sb.ToString();
    }

    /// <summary>JSON di tutte le sessioni, ordinate per id (con conteggio eventi).</summary>
    public static string ListJson()
    {
        var keys = _sessions.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var sb = new StringBuilder();
        sb.Append("{\"sessions\":[");
        lock (_lock)
        {
            for (var i = 0; i < keys.Length; i++)
            {
                if (i > 0) sb.Append(',');
                var s = _sessions[keys[i]];
                sb.Append("{\"session\":\"").Append(Escape(s.Id))
                  .Append("\",\"recording\":").Append(s.Recording ? "true" : "false")
                  .Append(",\"count\":").Append(s.Events.Count).Append('}');
            }
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Replay: riproduce gli eventi di <paramref name="sessionId"/> in una NUOVA sessione
    /// <paramref name="targetId"/> (registrazione). Ritorna il numero di eventi riprodotti;
    /// 0 se la sessione sorgente non esiste.</summary>
    public static double Run(string sessionId, string targetId)
    {
        if (string.IsNullOrWhiteSpace(sessionId) || string.IsNullOrWhiteSpace(targetId)) return 0.0;
        string[] events;
        lock (_lock)
        {
            if (!_sessions.TryGetValue(sessionId, out var src)) return 0.0;
            events = src.Events.ToArray();
        }
        Start(targetId);
        foreach (var ev in events) Record(ev);
        Stop();
        return events.Length;
    }

    /// <summary>Executor degli eventi (impostato dal backend con il valutatore BASIC runtime).
    /// Ritorna "" se l'evento è stato eseguito, "error: …" altrimenti.</summary>
    public static Func<string, string>? Executor { get; set; }

    /// <summary>Nome della sessione degli esiti della riesecuzione automatica.</summary>
    public static string AutoSessionId(string sessionId) => (sessionId ?? "") + "#auto";

    /// <summary>
    /// E1 — **RIESECUZIONE AUTOMATICA** degli eventi di una sessione: ogni evento è valutato col
    /// valutatore runtime (Executor) e l'esito è registrato come evento `"ok: <evento>"` /
    /// `"error: <msg>"` nella sessione `"&lt;sessione&gt;#auto"` (leggibile con `AI.REPLAYGET`).
    /// Mai silenzioso: gli eventi falliti restano tracciati, e senza executor NON si finge nulla
    /// (errore esplicito). Ritorna il numero di eventi rieseguiti con successo.
    /// </summary>
    public static double Auto(string sessionId)
    {
        if (string.IsNullOrWhiteSpace(sessionId)) return 0;
        if (Executor is null)
            throw new InvalidOperationException(
                "AI REPLAY AUTO: nessun executor registrato (valutatore runtime non disponibile su questo backend)");
        string[] events;
        lock (_lock)
        {
            if (!_sessions.TryGetValue(sessionId, out var src)) return 0;
            events = src.Events.ToArray();
        }
        if (events.Length == 0) return 0;
        var target = AutoSessionId(sessionId);
        Start(target);
        var ok = 0;
        foreach (var ev in events)
        {
            string err;
            try { err = Executor(ev) ?? ""; }
            catch (Exception ex) { err = "error: " + ex.Message; }
            if (string.IsNullOrEmpty(err)) { ok++; Record("ok: " + ev); }
            else Record("error: " + ev + " → " + err);
        }
        Stop();
        return ok;
    }

    /// <summary>Svuota il registro. Ritorna il numero di sessioni rimosse.</summary>
    public static double Clear()
    {
        lock (_lock)
        {
            var n = _sessions.Count;
            _sessions.Clear();
            _current = "";
            return n;
        }
    }

    /// <summary>Serializza lo stato: record `session\x1frecording\x1fevent1\x1devent2...` separati
    /// da \x1e (gli eventi sono separati da \x1d).</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        lock (_lock)
        {
            foreach (var kv in _sessions)
            {
                if (!first) sb.Append('\u001e');
                first = false;
                sb.Append(kv.Value.Id).Append('\u001f').Append(kv.Value.Recording ? "1" : "0").Append('\u001f');
                for (var i = 0; i < kv.Value.Events.Count; i++)
                {
                    if (i > 0) sb.Append('\u001d');
                    sb.Append(kv.Value.Events[i]);
                }
            }
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
            if (f.Length < 2) continue;
            Start(f[0]);
            if (f.Length >= 3 && f[2].Length > 0)
                foreach (var ev in f[2].Split('\u001d'))
                    Record(ev);
            if (f[1] != "1") Stop();
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
