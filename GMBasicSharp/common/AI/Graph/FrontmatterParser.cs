// FILE: Basic.AI.Graph.FrontmatterParser
// Estrazione del frontmatter YAML (--- blocchi in testa ai .md Obsidian) come metadata
// strutturati riusabili da RAG BUILD (filtri category/tags/source) e GRAPH BUILD (nodi).
// Riusabile ovunque: REPL, CLI, BASIC/agenti. Niente dipendenze YAML: solo il sottoinsieme
// chiave:valore (stringhe, liste semplici) che serve a GMBASIC — mai un parser YAML completo.
using System.Text.Json;

namespace Basic.AI.Graph;

/// <summary>Frontmatter YAML estratto da un file markdown Obsidian (blocco --- in testa).</summary>
public sealed record Frontmatter(
    Dictionary<string, List<string>> Fields,
    string Body)
{
    /// <summary>Primo valore di un campo (o null).</summary>
    public string? Get(string key) =>
        Fields.TryGetValue(key, out var v) && v.Count > 0 ? v[0] : null;

    /// <summary>Tutti i valori di un campo (per tag multipli).</summary>
    public IReadOnlyList<string> GetAll(string key) =>
        Fields.TryGetValue(key, out var v) ? v : [];
}

/// <summary>Parser del frontmatter YAML per note Obsidian. Gestisce i campi comuni
/// (title/tags/category/source/year) e qualunque chiave custom, con liste semplici.</summary>
public static class FrontmatterParser
{
    /// <summary>Se il testo inizia con un blocco <c>---</c> (YAML) o <c>+++</c> (TOML), lo estrae
    /// e ritorna frontmatter+body; altrimenti frontmatter vuoto + body = testo intero.</summary>
    public static Frontmatter Parse(string text)
    {
        if (string.IsNullOrEmpty(text))
            return new Frontmatter(new(), string.Empty);

        // Il blocco frontmatter è un --- (YAML) o +++ (TOML) nella PRIMA riga (o dopo BOM).
        var start = text.StartsWith('\uFEFF') ? 1 : 0;
        if (text.Length - start < 6) return new Frontmatter(new(), text);
        var isYaml = text.AsSpan(start).StartsWith("---");
        var isToml = text.AsSpan(start).StartsWith("+++");
        if (!isYaml && !isToml) return new Frontmatter(new(), text);

        var nl = text.IndexOf('\n', start + 3);
        if (nl < 0) return new Frontmatter(new(), text);
        var bodyStart = nl + 1;
        var lines = text.AsSpan(bodyStart).ToString().Split('\n');
        var endIdx = -1;
        for (var i = 0; i < lines.Length; i++)
        {
            var t = lines[i].TrimEnd('\r').Trim();
            if (isYaml && (t == "---" || t == "..."))
            {
                endIdx = i;
                break;
            }
            if (isToml && t == "+++")
            {
                endIdx = i;
                break;
            }
        }
        if (endIdx < 0) return new Frontmatter(new(), text); // apertura senza chiusura: non è frontmatter

        var fields = ParseFields(lines.Take(endIdx));
        var body = string.Join('\n', lines.Skip(endIdx + 1));
        return new Frontmatter(fields, body);
    }

    private static Dictionary<string, List<string>> ParseFields(IEnumerable<string> rawLines)
    {
        var fields = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
        foreach (var line in rawLines)
        {
            var s = line.Trim();
            if (s.Length == 0 || s.StartsWith('#')) continue;
            var colon = s.IndexOf(':');
            if (colon <= 0) continue;
            var key = s[..colon].Trim();
            var val = s[(colon + 1)..].Trim();

            if (val.StartsWith('[') && val.EndsWith(']'))
            {
                // lista inline: [a, b, c]
                var inner = val[1..^1];
                foreach (var item in inner.Split(','))
                {
                    var it = item.Trim().Trim('"', '\'');
                    if (it.Length > 0) Add(fields, key, it);
                }
            }
            else if (val.Length == 0)
            {
                // lista a blocchi (righe successive con "- item")
                // (non gestita qui: il chiamante può leggere Get del campo; per le liste a blocchi
                //  ci si affida al body — caso raro nei vault GMBASIC, quasi sempre liste inline)
            }
            else
            {
                Add(fields, key, val.Trim('"', '\''));
            }
        }
        return fields;
    }

    private static void Add(Dictionary<string, List<string>> fields, string key, string value)
    {
        if (!fields.TryGetValue(key, out var list))
        {
            list = [];
            fields[key] = list;
        }
        list.Add(value);
    }

    /// <summary>Serializza i campi rilevanti come metadata JSON per <c>IndexTextWithMetadata</c>
    /// (filtri AI.SEARCH category/tags/source). Campi sconosciuti inclusi se presenti.</summary>
    public static string ToMetadataJson(Frontmatter fm)
    {
        var dict = new Dictionary<string, object>();
        foreach (var (k, v) in fm.Fields)
        {
            if (v.Count == 1) dict[k] = v[0];
            else dict[k] = v;
        }
        return JsonSerializer.Serialize(dict);
    }
}
