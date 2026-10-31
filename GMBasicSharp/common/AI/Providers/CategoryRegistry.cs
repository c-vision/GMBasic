// FILE: Basic.AI.Providers.CategoryRegistry — Orchestrazione agenti (A1): tassonomia di
// categorie di task con routing esplicito categoria→provider e fallback. Estende il modello
// del DifferentialRouter (Local/Frontier) con categorie NOMINATE (deep/quick/ultrabrain/
// research/coding/testing/visual) senza vincolare il runtime a un singolo provider.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_category.c):
// stesse operazioni, stesso formato JSON di ListJson/List.
//
// Regola: la conoscenza delle categorie e' separata dalla loro esecuzione; questo registro
// e' solo policy (categoria → provider/modello). Il dispatch effettivo resta a ProviderServer.
using System.Collections.Concurrent;
using System.Text;

namespace Basic.AI.Providers;

/// <summary>Registro thread-safe delle categorie di task → provider/modello (con fallback).
/// Scrittura last-write-wins; PICK ritorna il provider associato ("" se assente).</summary>
public static class CategoryRegistry
{
    private sealed record Entry(string Provider, string Fallback);

    private static readonly ConcurrentDictionary<string, Entry> _categories =
        new(StringComparer.OrdinalIgnoreCase);

    private static volatile bool _auto;

    /// <summary>Associa una categoria a un provider/modello. 1 = ok, 0 = errore (categoria o provider vuoti).</summary>
    public static double Set(string category, string provider)
    {
        if (string.IsNullOrWhiteSpace(category) || string.IsNullOrWhiteSpace(provider)) return 0.0;
        _categories.AddOrUpdate(category,
            _ => new Entry(provider, ""),
            (_, old) => old with { Provider = provider });
        return 1.0;
    }

    /// <summary>Imposta il provider di fallback per una categoria. 1 = ok, 0 = errore.</summary>
    public static double SetFallback(string category, string provider)
    {
        if (string.IsNullOrWhiteSpace(category)) return 0.0;
        _categories.AddOrUpdate(category,
            _ => new Entry("", provider),
            (_, old) => old with { Fallback = provider });
        return 1.0;
    }

    /// <summary>Abilita/disabilita il routing automatico per categoria. Ritorna il nuovo stato (1/0).</summary>
    public static double SetAuto(double flag)
    {
        _auto = flag != 0.0;
        return _auto ? 1.0 : 0.0;
    }

    /// <summary>Provider associato alla categoria ("" se assente o se il routing e' disabilitato).</summary>
    public static string Pick(string category)
    {
        if (!_auto || string.IsNullOrWhiteSpace(category)) return "";
        return _categories.TryGetValue(category, out var e) ? e.Provider : "";
    }

    /// <summary>Fallback associato alla categoria ("" se assente).</summary>
    public static string PickFallback(string category)
    {
        if (string.IsNullOrWhiteSpace(category)) return "";
        return _categories.TryGetValue(category, out var e) ? e.Fallback : "";
    }

    /// <summary>True se il routing automatico e' abilitato.</summary>
    public static bool AutoEnabled => _auto;

    /// <summary>JSON deterministico delle categorie registrate, ordinate per nome (parita' con il C).</summary>
    public static string ListJson()
    {
        var keys = _categories.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var sb = new StringBuilder();
        sb.Append("{\"auto\":").Append(_auto ? "true" : "false").Append(",\"categories\":{");
        for (var i = 0; i < keys.Length; i++)
        {
            if (i > 0) sb.Append(',');
            var e = _categories[keys[i]];
            sb.Append('"').Append(Escape(keys[i])).Append("\":{\"provider\":\"")
              .Append(Escape(e.Provider)).Append("\",\"fallback\":\"")
              .Append(Escape(e.Fallback)).Append("\"}");
        }
        sb.Append("}}");
        return sb.ToString();
    }

    /// <summary>Svuota il registro (test/harness).</summary>
    public static void Clear() { _categories.Clear(); _auto = false; }

    /// <summary>Persistenza: `auto\x1ecat\x1fprovider\x1ffallback...`.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        sb.Append(_auto ? "1" : "0");
        foreach (var kv in _categories)
            sb.Append('\u001e').Append(kv.Key).Append('\u001f').Append(kv.Value.Provider).Append('\u001f').Append(kv.Value.Fallback);
        return sb.ToString();
    }

    /// <summary>Ripristina lo stato da <see cref="ExportState"/>.</summary>
    public static void ImportState(string state)
    {
        if (string.IsNullOrEmpty(state)) return;
        var parts = state.Split('\u001e');
        _auto = parts[0] == "1";
        for (var i = 1; i < parts.Length; i++)
        {
            var f = parts[i].Split('\u001f');
            if (f.Length >= 3) { Set(f[0], f[1]); SetFallback(f[0], f[2]); }
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
