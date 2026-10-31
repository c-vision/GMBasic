// FILE: Basic.Agents.MetricsRegistry — Orchestrazione agenti (B6): metriche fleet-level.
// Aggregazione di metriche per-agente (token, costo, latenza, successi/fallimenti) in un
// registro unico, con FLEETSTATS che somma per agente e per chiave su tutta la flotta.
// Deterministico (nessun LLM): le metriche sono registrate esplicitamente via METRICRECORD.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_metrics.c):
// stesse operazioni, stesso formato JSON. Thread-safe.
using System.Collections.Concurrent;
using System.Globalization;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe delle metriche fleet. RECORD/RESET scrivono,
/// GET/LIST/FLEETSTATS leggono.</summary>
public static class MetricsRegistry
{
    // agent -> (key -> somma)
    private static readonly ConcurrentDictionary<string, ConcurrentDictionary<string, double>> _m =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Registra/accumula una metrica per un agente. 1 = ok, 0 = agente/chiave vuoti.</summary>
    public static double Record(string agent, string key, double value)
    {
        if (string.IsNullOrWhiteSpace(agent) || string.IsNullOrWhiteSpace(key)) return 0.0;
        var byKey = _m.GetOrAdd(agent, _ => new ConcurrentDictionary<string, double>(StringComparer.OrdinalIgnoreCase));
        byKey.AddOrUpdate(key, value, (_, old) => old + value);
        return 1.0;
    }

    /// <summary>Somma della metrica per agente+chiave (0 se assente).</summary>
    public static double Get(string agent, string key)
        => !string.IsNullOrWhiteSpace(agent) && _m.TryGetValue(agent, out var byKey) &&
           byKey.TryGetValue(key, out var v) ? v : 0.0;

    /// <summary>§3.6 PARSEC — metriche TASK standard (tokens/ttft/tool_calls/llm_calls/latency)
    /// come JSON. Stesse chiavi/formato del C (gmb_task_metrics).</summary>
    public static string TaskMetrics(string task)
    {
        string[] k = { "tokens", "ttft", "tool_calls", "llm_calls", "latency" };
        var sb = new StringBuilder("{");
        for (int i = 0; i < k.Length; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append('"').Append(k[i]).Append("\":")
              .Append(((long)System.Math.Round(Get(task, k[i]))).ToString(CultureInfo.InvariantCulture));
        }
        sb.Append('}');
        return sb.ToString();
    }

    /// <summary>JSON di tutte le metriche: {"metrics":{"<agent>":{"<key>":v}}}.</summary>
    public static string ListJson()
    {
        var sb = new StringBuilder();
        sb.Append("{\"metrics\":{");
        AppendAgents(sb);
        sb.Append("}}");
        return sb.ToString();
    }

    /// <summary>JSON fleet: {"agents":{...},"totals":{"<key>":somma},"count":n}.
    /// `totals` somma la stessa chiave su tutti gli agenti.</summary>
    public static string FleetStats()
    {
        var totals = new SortedDictionary<string, double>(StringComparer.OrdinalIgnoreCase);
        foreach (var byKey in _m.Values)
            foreach (var kv in byKey)
                totals[kv.Key] = totals.TryGetValue(kv.Key, out var cur) ? cur + kv.Value : kv.Value;
        var sb = new StringBuilder();
        sb.Append("{\"agents\":{");
        AppendAgents(sb);
        sb.Append("},\"totals\":{");
        var first = true;
        foreach (var kv in totals)
        {
            if (!first) sb.Append(',');
            first = false;
            sb.Append('"').Append(Escape(kv.Key)).Append("\":").Append(Num(kv.Value));
        }
        sb.Append("},\"count\":").Append(_m.Count).Append('}');
        return sb.ToString();
    }

    /// <summary>Svuota il registro. Ritorna il numero di agenti rimossi.</summary>
    public static double Reset()
    {
        var n = _m.Count;
        _m.Clear();
        return n;
    }

    /// <summary>Serializza lo stato: record `agent\x1fkey\x1fvalue` separati da \x1e.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        foreach (var kv in _m)
            foreach (var kv2 in kv.Value)
            {
                if (!first) sb.Append('\u001e');
                first = false;
                sb.Append(kv.Key).Append('\u001f').Append(kv2.Key).Append('\u001f').Append(Num(kv2.Value));
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
            if (f.Length >= 3 && double.TryParse(f[2], NumberStyles.Float, CultureInfo.InvariantCulture, out var v))
                Record(f[0], f[1], v);
        }
    }

    private static void AppendAgents(StringBuilder sb)
    {
        var agents = _m.Keys.ToArray();
        System.Array.Sort(agents, StringComparer.OrdinalIgnoreCase);
        for (var i = 0; i < agents.Length; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append('"').Append(Escape(agents[i])).Append("\":{");
            var keys = _m[agents[i]].Keys.ToArray();
            System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
            for (var j = 0; j < keys.Length; j++)
            {
                if (j > 0) sb.Append(',');
                sb.Append('"').Append(Escape(keys[j])).Append("\":").Append(Num(_m[agents[i]][keys[j]]));
            }
            sb.Append('}');
        }
    }

    private static string Num(double d)
        => d.ToString("0.################", CultureInfo.InvariantCulture);

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
