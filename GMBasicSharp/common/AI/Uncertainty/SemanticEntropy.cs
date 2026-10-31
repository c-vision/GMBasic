// FILE: Basic.AI.Uncertainty.SemanticEntropy
//
// Self-Consistency / Semantic Entropy (Spanda R_sc) — implementazione C# PURA per i
// backend GESTITI (IL, PCodeVM).
//
// REGOLA (CLAUDE.md): il C non chiama MAI il C#; il P/Invoke C# → C è AMMESSO (refactoring voluto).
// Questa implementazione resta oggi in C# puro (scelta storica, non un obbligo): lo STESSO algoritmo
// del runtime C (`gmb_uncertainty.c`). La parità di risultato è verificata dai MiniTest
// (stesso input → stesso output sui 4 backend).
//
// Sorgente di riferimento per il comportamento: `native/gmb_runtime_c/src/gmb_uncertainty.c`
// (pipeline: strip co-markers → trim filler prefixes → normalize numbers → canonicalizza →
// cluster exact-match → Shannon log2 → risk = H/log2(K)).

namespace Basic.AI.Uncertainty;

public static class SemanticEntropy
{
    public const int MaxResponses = 32;
    public const int MaxBuckets = 32;

    /// <summary>Dettaglio di un calcolo (specchio di gmb_consistency_detail_t).</summary>
    public sealed record Detail(
        int K, int NBuckets, double Entropy, double Risk,
        IReadOnlyList<int> BucketSizes,
        IReadOnlyList<string> BucketReps,
        bool CollapseWarning);

    private static readonly string[] FillerPrefixes =
    [
        "the answer is ", "answer: ", "result: ", "the result is ",
        "final answer: ", "final answer is ",
    ];

    private static readonly Dictionary<string, string> WordNumbers = new(StringComparer.Ordinal)
    {
        ["zero"] = "0", ["one"] = "1", ["two"] = "2", ["three"] = "3", ["four"] = "4",
        ["five"] = "5", ["six"] = "6", ["seven"] = "7", ["eight"] = "8", ["nine"] = "9",
        ["ten"] = "10", ["eleven"] = "11", ["twelve"] = "12", ["thirteen"] = "13",
        ["fourteen"] = "14", ["fifteen"] = "15", ["sixteen"] = "16", ["seventeen"] = "17",
        ["eighteen"] = "18", ["nineteen"] = "19", ["twenty"] = "20", ["thirty"] = "30",
        ["forty"] = "40", ["fifty"] = "50", ["sixty"] = "60", ["seventy"] = "70",
        ["eighty"] = "80", ["ninety"] = "90", ["hundred"] = "100", ["thousand"] = "1000",
        ["million"] = "1000000",
    };

    /// <summary>Normalizzazione deterministica di una risposta (stesse regole del C).</summary>
    public static string Normalize(string? input)
    {
        var s = input ?? string.Empty;
        s = StripCoMarkers(s);
        s = TrimFillerPrefix(s);
        s = NormalizeNumbers(s);
        return CanonicalizeWhitespace(s);
    }

    private static string StripCoMarkers(string s)
    {
        var sb = new System.Text.StringBuilder(s.Length);
        for (var i = 0; i < s.Length;)
        {
            // "####"
            if (i + 3 < s.Length && s[i] == '#' && s[i + 1] == '#' && s[i + 2] == '#' && s[i + 3] == '#')
            {
                i += 4;
                continue;
            }
            // "\boxed{...}" → estrae il contenuto (non lo rimuove)
            if (i + 7 < s.Length && s.AsSpan(i).StartsWith("\\boxed{"))
            {
                var j = i + 7;
                var depth = 1;
                var start = j;
                while (j < s.Length && depth > 0)
                {
                    if (s[j] == '{') depth++;
                    else if (s[j] == '}') depth--;
                    j++;
                }
                var contentLen = Math.Max(0, j - start - 1);
                if (contentLen > 0) sb.Append(s, start, contentLen);
                i = j;
                continue;
            }
            sb.Append(s[i]);
            i++;
        }
        return sb.ToString();
    }

    private static string TrimFillerPrefix(string s)
    {
        var t = s.TrimStart();
        foreach (var pref in FillerPrefixes)
            if (t.StartsWith(pref, StringComparison.OrdinalIgnoreCase))
                return t[pref.Length..];
        return t;
    }

    private static string NormalizeNumbers(string s)
    {
        var sb = new System.Text.StringBuilder(s.Length);
        foreach (var c in s)
            if (c != '$' && c != ',') sb.Append(c);
        var r = sb.ToString();
        if (r.Length >= 2 && r[^2] == '.' && r[^1] == '0') r = r[..^2];
        // mapping parola→numero solo se l'INTERA stringa è la parola (come il C)
        return WordNumbers.TryGetValue(r, out var num) ? num : r;
    }

    private static string CanonicalizeWhitespace(string s)
    {
        var sb = new System.Text.StringBuilder(s.Length);
        var inSpace = false;
        foreach (var c in s)
        {
            if (char.IsWhiteSpace(c))
            {
                if (!inSpace) { sb.Append(' '); inSpace = true; }
            }
            else
            {
                sb.Append(char.ToLowerInvariant(c));
                inSpace = false;
            }
        }
        return sb.ToString().Trim();
    }

    /// <summary>Pipeline completa su K risposte RAW (normalizza → cluster → entropia → risk).</summary>
    public static Detail Compute(IReadOnlyList<string?> responses)
    {
        var k = responses.Count;
        if (k < 1) throw new ArgumentException("almeno 1 risposta", nameof(responses));
        if (k > MaxResponses) k = MaxResponses;

        var norm = new string[k];
        for (var i = 0; i < k; i++) norm[i] = Normalize(responses[i]);

        // cluster exact-match
        var reps = new List<string>();
        var counts = new List<int>();
        for (var i = 0; i < k; i++)
        {
            var idx = reps.IndexOf(norm[i]);
            if (idx >= 0) counts[idx]++;
            else { reps.Add(norm[i]); counts.Add(1); }
            if (reps.Count > MaxBuckets) break;
        }

        var total = counts.Sum();
        double entropy = 0;
        foreach (var c in counts)
        {
            if (c <= 0) continue;
            var p = (double)c / total;
            entropy -= p * Math.Log2(p);
        }

        double risk;
        if (k == 1) risk = 0;
        else
        {
            var maxEntropy = Math.Log2(k);
            risk = maxEntropy <= 0 ? 0 : entropy / maxEntropy;
        }

        // Heuristic collapse (identica al C): risk ≈ 0 con K ≥ 3
        var collapse = risk < 0.05 && k >= 3;
        return new Detail(k, counts.Count, entropy, risk, counts, reps, collapse);
    }

    private static Detail? _last;

    /// <summary>AI.LEXICALENTROPY(responses$): entropia di Shannon (bit) sulle risposte
    /// separate da ';' o newline. Deterministica, nessun provider. Aggiorna l'ultimo
    /// dettaglio (per AI.CONSISTENCYDETAIL$()).</summary>
    public static double LexicalEntropy(string? responses)
    {
        var parts = SplitResponses(responses);
        if (parts.Count == 0) { _last = null; return 0.0; }
        var d = Compute(parts);
        _last = d;
        return d.Entropy;
    }

    /// <summary>Calcola il risk da K risposte già ottenute (usato da AI.CONSISTENCY).</summary>
    public static double ConsistencyRisk(IReadOnlyList<string?> responses)
    {
        if (responses.Count == 0) { _last = null; return double.NaN; }
        var d = Compute(responses);
        _last = d;
        return d.Risk;
    }

    /// <summary>Invalida l'ultimo dettaglio (nessun calcolo valido).</summary>
    public static void Invalidate() => _last = null;

    /// <summary>AI.CONSISTENCYDETAIL$(): JSON dell'ultimo calcolo, "{}" se nessuno.</summary>
    public static string LastDetailJson()
        => _last is null ? "{}" : DetailToJson(_last);

    public static string DetailToJson(Detail d)
    {
        var sb = new System.Text.StringBuilder();
        sb.Append("{\"k\":").Append(d.K)
          .Append(",\"n_buckets\":").Append(d.NBuckets)
          .Append(",\"entropy\":").Append(d.Entropy.ToString("F6", System.Globalization.CultureInfo.InvariantCulture))
          .Append(",\"risk\":").Append(d.Risk.ToString("F6", System.Globalization.CultureInfo.InvariantCulture))
          .Append(",\"collapse_warning\":").Append(d.CollapseWarning ? "true" : "false")
          .Append(",\"buckets\":[");
        for (int i = 0; i < d.NBuckets; i++)
        {
            if (i > 0) sb.Append(",");
            var repr = d.BucketReps[i] ?? "";
            // Match VMC behavior: no JSON escaping (raw string in repr)
            sb.Append("{\"size\":").Append(d.BucketSizes[i])
              .Append(",\"repr\":\"").Append(repr).Append("\"}");
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Split IDENTICO a `uncertainty_split` del runtime C: separatori ';' e
    /// newline, trim di spazi/tab ai bordi, campi VUOTI MANTENUTI (una stringa vuota dà
    /// 1 risposta vuota, non 0), al più MaxResponses.</summary>
    private static List<string> SplitResponses(string? responses)
    {
        var list = new List<string>();
        var s = responses ?? string.Empty;
        var start = 0;
        for (var i = 0; i <= s.Length; i++)
        {
            if (i == s.Length || s[i] == ';' || s[i] == '\n' || s[i] == '\r')
            {
                var field = s[start..i].Trim(' ', '\t');
                list.Add(field);
                if (list.Count >= MaxResponses) break;
                start = i + 1;
            }
        }
        return list;
    }
}
