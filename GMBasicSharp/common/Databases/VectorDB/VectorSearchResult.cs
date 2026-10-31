// FILE: Basic.SQLite.Vector.VectorSearchResult
namespace Basic.SQLite.Vector;

/// <summary>Outcome of a vector search request.</summary>
public enum VectorSearchStatus
{
    Success,
    InsufficientEvidence
}

/// <summary>Single ranked chunk from vector search.</summary>
public sealed record VectorSearchResult(
    long ChunkId,
    long DocumentId,
    string SourceId,
    string Text,
    float Score,
    string ModelVersion,
    string? MetadataJson,
    string? ExplainJson = null);

/// <summary>Vector search response with status and ranked hits.</summary>
public sealed record VectorSearchResponse(
    VectorSearchStatus Status,
    IReadOnlyList<VectorSearchResult> Results);
