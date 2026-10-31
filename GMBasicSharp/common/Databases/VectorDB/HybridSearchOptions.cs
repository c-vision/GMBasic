// FILE: Basic.SQLite.Vector.HybridSearchOptions
namespace Basic.SQLite.Vector;

/// <summary>RRF hybrid search tuning (vector + FTS5 BM25).</summary>
public sealed record HybridSearchOptions(
    int CandidatePoolSize = 50,
    int RrfK = 60,
    double VectorWeight = 1.0,
    double FtsWeight = 1.0);
