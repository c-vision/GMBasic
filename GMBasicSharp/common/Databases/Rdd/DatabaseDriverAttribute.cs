// FILE: Basic.Databases.Rdd.DatabaseDriverAttribute.cs

namespace Basic.Databases.Rdd;

/// <summary>Attributo per registrare un driver database. Usato da DriverRegistry
/// per la scoperta automatica dei driver disponibili.</summary>
[AttributeUsage(AttributeTargets.Class, Inherited = false)]
public sealed class DatabaseDriverAttribute : Attribute
{
    /// <summary>Nome del driver (es. "CSV", "DBFCDX").</summary>
    public string Name { get; }

    /// <summary>Estensioni file associate (es. ".csv", ".tsv").</summary>
    public string[]? Extensions { get; set; }

    /// <summary>Descrizione human-readable del driver.</summary>
    public string? Description { get; set; }

    public DatabaseDriverAttribute(string name)
    {
        Name = name;
    }
}
