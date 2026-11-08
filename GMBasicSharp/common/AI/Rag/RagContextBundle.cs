// FILE: Basic.AI.Rag.RagContextBundle
using System.Text;
using Basic.SQLite.Vector;

namespace Basic.AI.Rag;

/// <summary>Source attribution for grounded RAG context.</summary>
public sealed record RagSourceAttribution(string SourceId, float Score, string Snippet);

/// <summary>Formatted RAG context with per-source attributions.</summary>
public sealed record RagContextBundle(string Context, IReadOnlyList<RagSourceAttribution> Attributions)
{
    /// <summary>Builds grounded context from vector search hits.</summary>
    public static RagContextBundle FromResults(
        IReadOnlyList<VectorSearchResult> results,
        int maxSnippetChars = 240)
    {
        ArgumentNullException.ThrowIfNull(results);
        if (maxSnippetChars <= 0)
            throw new ArgumentOutOfRangeException(nameof(maxSnippetChars));

        if (results.Count == 0)
            return new RagContextBundle(string.Empty, []);

        var sb = new StringBuilder();
        var attributions = new List<RagSourceAttribution>(results.Count);
        for (var i = 0; i < results.Count; i++)
        {
            var hit = results[i];
            var snippet = Truncate(hit.Text, maxSnippetChars);
            attributions.Add(new RagSourceAttribution(hit.SourceId, hit.Score, snippet));
            sb.Append('[').Append(i + 1).Append("] (source_id=")
                .Append(hit.SourceId).Append(", score=")
                .Append(hit.Score.ToString("F4"))
                .Append(") ").AppendLine(snippet);
        }

        return new RagContextBundle(sb.ToString().TrimEnd(), attributions);
    }

    private static string Truncate(string text, int maxChars)
    {
        if (string.IsNullOrEmpty(text) || text.Length <= maxChars)
            return text;

        return text[..maxChars] + "…";
    }
}
