// FILE: Basic.SQLite.Vector.IndexTextOutcome
namespace Basic.SQLite.Vector;

/// <summary>Result of an <see cref="SqlVectorIndexer.IndexText"/> call.</summary>
public enum IndexTextOutcome
{
    Indexed,
    Updated,
    Skipped
}
