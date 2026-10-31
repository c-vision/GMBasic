// FILE: Basic.SQLite.Vector.KeywordReranker
namespace Basic.SQLite.Vector;

/// <summary>Deterministic, zero-dependency reranker: boosts documents containing more of the
/// query's terms. No model, no network — the always-available default (bootstrap-registered,
/// same role "fixture" default plays for embedding/completion), and a reasonable fallback on
/// machines that can't or don't want to run a real cross-encoder.</summary>
/// <remarks>ponytail: not semantically meaningful (pure lexical overlap) — upgrade path:
/// <see cref="OnnxCrossEncoderReranker"/> for real relevance judgments.</remarks>
public sealed class KeywordReranker : IReranker
{
    public string ModelName => "keyword-reranker-v1";

    public float Score(string query, string document)
    {
        ArgumentNullException.ThrowIfNull(query);
        ArgumentNullException.ThrowIfNull(document);

        var terms = Tokenize(query);
        if (terms.Length == 0)
            return 0f;

        var text = document.ToLowerInvariant();
        var matches = terms.Count(t => text.Contains(t, StringComparison.Ordinal));
        return (float)matches / terms.Length;
    }

    private static string[] Tokenize(string text) =>
        text.Split([' ', ',', '.', '!', '?', ';', ':'], StringSplitOptions.RemoveEmptyEntries)
            .Select(t => t.ToLowerInvariant())
            .Where(t => t.Length > 2)
            .Distinct()
            .ToArray();
}
