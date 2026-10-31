// FILE: Basic.Databases.Rdd.DatabaseWorkArea.cs — metadata bag

namespace Basic.Databases.Rdd;

public sealed class DatabaseWorkArea
{
    public int Area { get; init; }
    public IDatabaseDriver? Driver { get; set; }
    public string Alias { get; set; } = "";
    /// <summary>Driver-specific metadata (es. ISAM: UDT, Schema, FieldSymbolIds).</summary>
    public Dictionary<string, object?> Meta { get; } = new();

    public bool IsEmpty => Driver == null;
}
