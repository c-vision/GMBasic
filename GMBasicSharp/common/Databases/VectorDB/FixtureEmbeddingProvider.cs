// FILE: Basic.SQLite.Vector.FixtureEmbeddingProvider
namespace Basic.SQLite.Vector;

/// <summary>Deterministic embeddings for xUnit — no network, no model files.</summary>
public sealed class FixtureEmbeddingProvider : IEmbeddingProvider
{
    public string ModelVersion { get; }

    public FixtureEmbeddingProvider(string modelVersion = "fixture-v1")
    {
        ModelVersion = modelVersion;
    }

    public float[] Embed(string text, int dimensions)
    {
        ArgumentException.ThrowIfNullOrEmpty(text);
        if (dimensions <= 0)
            throw new ArgumentOutOfRangeException(nameof(dimensions));

        var vector = new float[dimensions];
        // Deterministic FNV-1a hash (GetHashCode non è stabile tra esecuzioni .NET)
        uint hash = 2166136261;
        foreach (char c in text) { hash ^= c; hash *= 16777619; }
        int seed = (int)hash;
        for (var i = 0; i < dimensions; i++)
            vector[i] = (float)Math.Sin(seed * (i + 1) * 0.131);

        VectorMath.Normalize(vector);
        return vector;
    }
}
