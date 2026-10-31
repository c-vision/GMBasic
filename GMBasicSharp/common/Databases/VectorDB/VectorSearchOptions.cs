// FILE: Basic.SQLite.Vector.VectorSearchOptions
namespace Basic.SQLite.Vector;

/// <summary>Query-time options (audit, attribution, ANN index).</summary>
public sealed record VectorSearchOptions(
    bool WriteAudit = true,
    string? QueryText = null,
    bool? UseApproximateIndex = null,
    int ApproximateIndexMinCount = 1000,
    int ApproximateEfSearch = 64,
    string? ShadowModelVersion = null);
