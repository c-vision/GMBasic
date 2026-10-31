// FILE: Basic.SQLite.Vector.VectorMetric
namespace Basic.SQLite.Vector;

/// <summary>Similarity metric for vector search.</summary>
public enum VectorMetric
{
    /// <summary>Cosine similarity — higher score is better.</summary>
    Cosine,

    /// <summary>L2 distance — stored score is negated distance (higher is better).</summary>
    L2
}
