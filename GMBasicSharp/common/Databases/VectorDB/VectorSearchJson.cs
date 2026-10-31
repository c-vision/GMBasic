// FILE: Basic.SQLite.Vector.VectorSearchJson
using System.Text.Json;

namespace Basic.SQLite.Vector;

/// <summary>JSON serialization of vector search responses for BASIC <c>JSON.PARSE</c>.</summary>
public static class VectorSearchJson
{
    private static readonly JsonSerializerOptions Options = new() { WriteIndented = false };

    public static string Serialize(VectorSearchResponse response)
    {
        ArgumentNullException.ThrowIfNull(response);
        var payload = new
        {
            status = response.Status.ToString(),
            hits = response.Results.Select(static r => new
            {
                chunkId = r.ChunkId,
                documentId = r.DocumentId,
                sourceId = r.SourceId,
                score = r.Score,
                text = r.Text,
                modelVersion = r.ModelVersion,
                vectorRank = ExplainRank(r, "vectorRank"),
                ftsRank = ExplainRank(r, "ftsRank"),
                rrfScore = ExplainScore(r),
            }).ToArray()
        };
        return JsonSerializer.Serialize(payload, Options);
    }

    /// <summary>Ritorna il rank (1-based) del chunk nel backend indicato, se ExplainJson è presente.</summary>
    private static int? ExplainRank(VectorSearchResult r, string field)
    {
        if (r.ExplainJson == null) return null;
        using var doc = JsonDocument.Parse(r.ExplainJson);
        return doc.RootElement.TryGetProperty(field, out var p) && p.TryGetInt32(out var v) ? v : null;
    }

    private static double? ExplainScore(VectorSearchResult r)
    {
        if (r.ExplainJson == null) return null;
        using var doc = JsonDocument.Parse(r.ExplainJson);
        return doc.RootElement.TryGetProperty("rrfScore", out var p) && p.TryGetDouble(out var v) ? v : null;
    }
}
