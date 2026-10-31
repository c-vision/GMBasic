// FILE: Basic.SQLite.Vector.EmbeddingProviderResolver
namespace Basic.SQLite.Vector;

/// <summary>Resolves the active embedding provider.</summary>
public static class EmbeddingProviderResolver
{
    public const int DefaultDimensions = 384;

    /// <summary>Active embedding provider (set by runtime/Interpreter).</summary>
    public static IEmbeddingProvider? Default { get; set; }

    /// <summary>Embedding width for new collections when BASIC omits dimensions.</summary>
    public static int ActiveDimensions { get; set; } = DefaultDimensions;

    /// <summary>Test hook — reset to defaults.</summary>
    internal static void ResetForTests()
    {
        Default = null;
        ActiveDimensions = DefaultDimensions;
    }
}
