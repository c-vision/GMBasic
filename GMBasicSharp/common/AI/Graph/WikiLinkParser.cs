// FILE: Basic.AI.Graph.WikiLinkParser
// Estrazione e risoluzione dei wikilink Obsidian ([[path|alias]], [[path#heading]],
// [[path#heading|alias]]) per costruire il grafo wiki. I path sono RELATIVI al vault
// (spesso con slash, case-insensitive su macOS) — si risolvono contro l'indice dei file reali,
// con fallback "dangling link" per i path inesistenti (comune in Obsidian).
using System.Text.RegularExpressions;

namespace Basic.AI.Graph;

/// <summary>Un wikilink estratto dal testo.</summary>
public sealed record WikiLink(string Target, string? Alias, string? Heading);

/// <summary>Un nodo del grafo (file .md) con i suoi link risolti.</summary>
public sealed record GraphNode(string Path, string Title, Dictionary<string, List<string>> Frontmatter, IReadOnlyList<WikiLink> Links);

/// <summary>Parser e resolver dei wikilink Obsidian.</summary>
public static partial class WikiLinkParser
{
    // [[target|alias]]  /  [[target#heading]]  /  [[target#heading|alias]]
    [GeneratedRegex(@"\[\[([^\]|#]+)(?:#([^\]|]+))?(?:\|([^\]]+))?\]\]", RegexOptions.Compiled)]
    private static partial Regex WikiLinkRegex();

    public static IReadOnlyList<WikiLink> Extract(string text)
    {
        var result = new List<WikiLink>();
        if (string.IsNullOrEmpty(text)) return result;
        foreach (Match m in WikiLinkRegex().Matches(text))
        {
            var target = m.Groups[1].Value.Trim();
            var heading = m.Groups[2].Success ? m.Groups[2].Value.Trim() : null;
            var alias = m.Groups[3].Success ? m.Groups[3].Value.Trim() : null;
            if (target.Length > 0) result.Add(new WikiLink(target, string.IsNullOrEmpty(alias) ? null : alias,
                string.IsNullOrEmpty(heading) ? null : heading));
        }
        return result;
    }

    /// <summary>Risolutore di path: normalizza (slash, trim, case) e lo confronta contro i path
    /// reali del vault. Ritorna il path relativo normalizzato se risolto, altrimenti null
    /// (dangling). <paramref name="index"/> è l'insieme dei path relativi reali (già normalizzati).</summary>
    public static string? Resolve(string target, IReadOnlySet<string> index)
    {
        var norm = Normalize(target);
        if (index.Contains(norm)) return norm;

        // Fallback: senza estensione .md o con alias → prova con .md
        if (!norm.EndsWith(".md", StringComparison.OrdinalIgnoreCase))
        {
            var withMd = norm + ".md";
            if (index.Contains(withMd)) return withMd;
        }
        // Fallback case-insensitive (macOS/Windows) — confronto per nome base (ultimo segmento),
        // ignorando l'estensione .md sia nel target sia nel candidato.
        var baseName = norm.Contains('/') ? norm[(norm.LastIndexOf('/') + 1)..] : norm;
        if (baseName.EndsWith(".md", StringComparison.OrdinalIgnoreCase)) baseName = baseName[..^3];
        foreach (var candidate in index)
        {
            var cb = candidate.Contains('/') ? candidate[(candidate.LastIndexOf('/') + 1)..] : candidate;
            if (cb.EndsWith(".md", StringComparison.OrdinalIgnoreCase)) cb = cb[..^3];
            if (string.Equals(cb, baseName, StringComparison.OrdinalIgnoreCase)) return candidate;
        }
        return null; // dangling
    }

    /// <summary>Normalizza un path relativo: slash uniformi, trim di spazi/barre, niente
    /// estensioni doppie, case preservato (il resolver fa il case-insensitive per nome).</summary>
    public static string Normalize(string path)
    {
        var p = path.Replace('\\', '/').Trim().Trim('/');
        // rimuovi eventuale "./" iniziale
        while (p.StartsWith("./", StringComparison.Ordinal)) p = p[2..];
        return p;
    }

    /// <summary>Estrae il titolo di un nodo: alias se presente, altrimenti nome file senza
    /// estensione (dell'ultimo segmento del path).</summary>
    public static string TitleOf(string path, string? alias)
    {
        if (!string.IsNullOrWhiteSpace(alias)) return alias!;
        var p = path.Contains('/') ? path[(path.LastIndexOf('/') + 1)..] : path;
        if (p.EndsWith(".md", StringComparison.OrdinalIgnoreCase)) p = p[..^3];
        return p;
    }
}
