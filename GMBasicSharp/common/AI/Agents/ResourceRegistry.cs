// FILE: Basic.Agents.ResourceRegistry — Orchestrazione agenti (A7): Resource Manager.
// Registro delle risorse/limiti dell'orchestrazione (parallel agent limits, model slots,
// context budget, connector limits, cpu/gpu/ram) + dynamic swarm sizing: il numero effettivo
// di agenti è min(richiesti, max_agents). Policy deterministica (nessuna lettura di sistema).
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_resource.c):
// stesse operazioni, stesso formato JSON. Thread-safe.
using System.Collections.Concurrent;
using System.Globalization;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe delle risorse/limiti. SET scrive, GET/AVAILABLE/LIST leggono,
/// SWARMSIZE calcola la dimensione effettiva dello swarm.</summary>
public static class ResourceRegistry
{
    private static readonly ConcurrentDictionary<string, string> _res =
        new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Imposta una risorsa/limite. 1 = ok, 0 = chiave vuota.</summary>
    public static double Set(string key, string value)
    {
        if (string.IsNullOrWhiteSpace(key)) return 0.0;
        _res[key] = value ?? "";
        return 1.0;
    }

    /// <summary>Valore della risorsa ("" se assente).</summary>
    public static string Get(string key)
        => !string.IsNullOrWhiteSpace(key) && _res.TryGetValue(key, out var v) ? v : "";

    /// <summary>1 se la risorsa è disponibile: impostata e (numerica > 0 oppure stringa
    /// non vuota diversa da 0/false/off/no). 0 altrimenti.</summary>
    public static double Available(string key)
    {
        if (string.IsNullOrWhiteSpace(key) || !_res.TryGetValue(key, out var v)) return 0.0;
        if (double.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out var d)) return d > 0 ? 1.0 : 0.0;
        if (v.Equals("false", StringComparison.OrdinalIgnoreCase) ||
            v.Equals("off", StringComparison.OrdinalIgnoreCase) ||
            v.Equals("no", StringComparison.OrdinalIgnoreCase)) return 0.0;
        return v.Length > 0 ? 1.0 : 0.0;
    }

    /// <summary>JSON di tutte le risorse, ordinate per chiave.</summary>
    public static string ListJson()
    {
        var keys = _res.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var sb = new StringBuilder();
        sb.Append("{\"resources\":{");
        for (var i = 0; i < keys.Length; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append('"').Append(Escape(keys[i])).Append("\":\"")
              .Append(Escape(_res[keys[i]])).Append('"');
        }
        sb.Append("}}");
        return sb.ToString();
    }

    /// <summary>Dimensione effettiva dello swarm: min(n, max_agents) se max_agents è un numero
    /// positivo, altrimenti n. n < 0 → 0.</summary>
    public static double SwarmSize(double n)
    {
        if (n < 0) n = 0;
        if (_res.TryGetValue("max_agents", out var v) &&
            double.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out var cap) && cap > 0)
            return System.Math.Min(n, System.Math.Floor(cap));
        return n;
    }

    /// <summary>Svuota il registro (test/harness).</summary>
    public static void Clear() => _res.Clear();

    /// <summary>Persistenza: `key\x1fvalue` separati da \x1e.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        foreach (var kv in _res)
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
            if (i > 0) Set(rec[..i], rec[(i + 1)..]);
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
