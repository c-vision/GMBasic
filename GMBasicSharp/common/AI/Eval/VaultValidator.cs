// FILE: Basic.AI.Eval.VaultValidator — T4.1 (arXiv:2608.12970), 2026-09-09.
// CORE deterministico di "Automated UAT via Vault": valida un RISULTATO contro i DOCUMENTI DI
// REQUISITO di una cartella del vault. v1 senza LLM/embeddings (oracolo a TERMINI): estrae le
// "requirement line" dai doc .md/.txt (bullet '-'/'*'/numerati o righe con MUST/REQUIREMENT),
// per ognuna ricava i termini significativi (alpha, >=4 char, no stopword) e verifica la loro
// presenza nel risultato. Verdetto di conformità esplicito (quali requisiti soddisfatti e quali
// no). L'integrazione RAG/embeddings e la superficie BASIC `AI EVAL VALIDATE ... AGAINST VAULT`
// restano la parte applicativa (aperta).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

namespace Basic.AI.Eval;

public sealed record EvalRequirement(string Text, string[] Terms);
public sealed record EvalVerdict(bool Ok, List<string> Passed, List<string> Failed, double Coverage);

public static class VaultValidator
{
    private static readonly string[] Stopwords =
    {
        "il", "lo", "la", "le", "gli", "un", "una", "uno", "e", "ed", "di", "da", "con", "per",
        "che", "deve", "dovra", "essere", "del", "della", "dei", "non", "ma", "se", "o", "in",
        "su", "tra", "fra", "nel", "nella", "the", "a", "an", "and", "of", "to", "for", "with",
        "shall", "must", "should", "will", "be", "is", "are", "the", "this", "that", "from", "on", "at", "by",
    };

    private static readonly Regex Bullet = new(@"^\s*(?:[-*•]|\d+[.)])\s+(.+)$", RegexOptions.Compiled);
    private static readonly Regex MustLine = new(@"(?:MUST|SHOULD|REQUIREMENT)\s*[:：]", RegexOptions.Compiled | RegexOptions.IgnoreCase);

    public static string[] TermsOf(string text)
    {
        if (string.IsNullOrEmpty(text)) return [];
        var words = Regex.Matches(text.ToLowerInvariant(), "[a-zàèéìòù0-9]{3,}")
            .Cast<Match>().Select(m => m.Value).Where(w => w.Length >= 4 && !Stopwords.Contains(w));
        return words.Distinct().ToArray();
    }

    /// <summary>Estrae le requirement-line dai doc .md/.txt della cartella (ricorsivo).</summary>
    public static List<EvalRequirement> ExtractRequirements(string folder)
    {
        var reqs = new List<EvalRequirement>();
        if (!Directory.Exists(folder)) return reqs;
        foreach (var file in Directory.EnumerateFiles(folder, "*", SearchOption.AllDirectories)
                     .Where(f => f.EndsWith(".md", StringComparison.OrdinalIgnoreCase)
                              || f.EndsWith(".txt", StringComparison.OrdinalIgnoreCase))
                     .OrderBy(f => f, StringComparer.Ordinal))
        {
            foreach (var line in File.ReadAllLines(file))
            {
                var trimmed = line.Trim();
                string? candidate = null;
                if (Bullet.IsMatch(line)) candidate = Bullet.Match(line).Groups[1].Value;
                else if (MustLine.IsMatch(trimmed)) candidate = trimmed;
                if (candidate is null) continue;
                var terms = TermsOf(candidate);
                if (terms.Length > 0)
                    reqs.Add(new EvalRequirement(trimmed, terms));
            }
        }
        return reqs;
    }

    /// <summary>Valida <paramref name="result"/> contro i requisiti dei doc in <paramref name="folder"/>.
    /// <paramref name="termRatio"/> = frazione di termini che deve comparire nel risultato per
    /// considerare soddisfatto un requisito (default 0.5). Verdetto Ok se TUTTI i requisiti passano.</summary>
    public static EvalVerdict Validate(string result, string folder, double termRatio = 0.5)
        => ValidateLines(result, ExtractRequirements(folder).Select(r => r.Text), termRatio);

    /// <summary>Verdetto su requirement-line GIÀ RECUPERATE (seam per i brain/RAG esistenti:
    /// vault, SecondBrain, RAG-KB — passare qui le righe estratte da un retrieval, es.
    /// HybridSearch/RagAsk). Stesso motore di conformità del percorso doc, deterministico.</summary>
    public static EvalVerdict ValidateLines(string result, IEnumerable<string> requirementLines,
        double termRatio = 0.5)
    {
        var reqs = requirementLines
            .Select(r => new EvalRequirement(r, TermsOf(r)))
            .Where(r => r.Terms.Length > 0)
            .ToList();
        var lowerResult = (result ?? "").ToLowerInvariant();
        var passed = new List<string>();
        var failed = new List<string>();
        var covered = 0;
        foreach (var req in reqs)
        {
            var hits = req.Terms.Count(t => lowerResult.Contains(t, StringComparison.Ordinal));
            var ratio = req.Terms.Length == 0 ? 1.0 : (double)hits / req.Terms.Length;
            covered += req.Terms.Length == 0 ? 0 : hits;
            if (ratio >= termRatio) passed.Add(req.Text);
            else failed.Add(req.Text);
        }
        var coverage = reqs.Sum(r => r.Terms.Length) == 0
            ? 1.0
            : (double)covered / reqs.Sum(r => r.Terms.Length);
        return new EvalVerdict(failed.Count == 0 && reqs.Count > 0, passed, failed, coverage);
    }

    /// <summary>Rappresenta il verdetto come JSON (pronto per l'esito della superficie BASIC
    /// `INTO`/ritorno): {"ok":bool,"passed":[...],"failed":[...],"coverage":0.0}.</summary>
    public static string ToJson(EvalVerdict v)
    {
        var sb = new System.Text.StringBuilder();
        sb.Append("{\"ok\":").Append(v.Ok ? "true" : "false");
        sb.Append(",\"passed\":").Append(JsonArr(v.Passed));
        sb.Append(",\"failed\":").Append(JsonArr(v.Failed));
        sb.Append(",\"coverage\":").Append(v.Coverage.ToString("0.###", System.Globalization.CultureInfo.InvariantCulture));
        sb.Append('}');
        return sb.ToString();
    }

    private static string JsonArr(IReadOnlyList<string> items)
    {
        var sb = new System.Text.StringBuilder("[");
        for (var i = 0; i < items.Count; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append('"').Append(items[i].Replace("\\", "\\\\").Replace("\"", "\\\"")).Append('"');
        }
        sb.Append(']');
        return sb.ToString();
    }
}
