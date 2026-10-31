// FILE: Basic.SQLite.Vector.ReciprocalRankFusion
namespace Basic.SQLite.Vector;

/// <summary>RRF merge for hybrid retrieval lists.</summary>
public static class ReciprocalRankFusion
{
    public static IReadOnlyList<(long ChunkId, double Score)> Merge(
        IReadOnlyList<long> vectorRankedChunkIds,
        IReadOnlyList<long> ftsRankedChunkIds,
        HybridSearchOptions options)
    {
        ArgumentNullException.ThrowIfNull(options);
        var scores = new Dictionary<long, double>();

        for (var i = 0; i < vectorRankedChunkIds.Count; i++)
        {
            var id = vectorRankedChunkIds[i];
            scores[id] = scores.GetValueOrDefault(id) + options.VectorWeight / (options.RrfK + i + 1);
        }

        for (var i = 0; i < ftsRankedChunkIds.Count; i++)
        {
            var id = ftsRankedChunkIds[i];
            scores[id] = scores.GetValueOrDefault(id) + options.FtsWeight / (options.RrfK + i + 1);
        }

        return scores
            .OrderByDescending(kv => kv.Value)
            .Select(kv => (kv.Key, kv.Value))
            .ToList();
    }
}
