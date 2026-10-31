// FILE: Basic.SQLite.Vector.RerankerScoring
namespace Basic.SQLite.Vector;

/// <summary>Shared sort+take-topK policy for any <see cref="IReranker"/> — keeps the "how do we
/// pick the winners" logic in one place instead of duplicated per reranker kind.</summary>
public static class RerankerScoring
{
    public static List<VectorSearchResult> Rerank(
        IReranker reranker,
        string query,
        IReadOnlyList<VectorSearchResult> results,
        int topK)
    {
        ArgumentNullException.ThrowIfNull(reranker);
        if (string.IsNullOrWhiteSpace(query) || results.Count == 0)
            return results.Take(topK).ToList();

        return results
            .Select(r => r with { Score = reranker.Score(query, r.Text) })
            .OrderByDescending(r => r.Score)
            .Take(topK)
            .ToList();
    }
}
