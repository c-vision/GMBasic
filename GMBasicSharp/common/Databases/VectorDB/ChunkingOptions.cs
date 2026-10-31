// FILE: Basic.SQLite.Vector.ChunkingOptions
namespace Basic.SQLite.Vector;

/// <summary>Text chunking configuration.</summary>
public sealed record ChunkingOptions(int MaxChars = 512, int OverlapChars = 64);
