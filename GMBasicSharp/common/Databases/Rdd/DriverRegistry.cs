// FILE: Basic.Databases.Rdd.DriverRegistry.cs
// Registro globale dei driver database. Supporta registrazione built-in
// e caricamento dinamico da assembly esterni.

using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace Basic.Databases.Rdd;

/// <summary>Entry nel registro driver.</summary>
public sealed class RegisteredDriver
{
    public string Name { get; }
    public Type DriverType { get; }
    public string[]? Extensions { get; }
    public string? Description { get; }

    public RegisteredDriver(string name, Type driverType, string[]? extensions = null, string? description = null)
    {
        Name = name;
        DriverType = driverType;
        Extensions = extensions;
        Description = description;
    }

    public IDatabaseDriver CreateInstance()
        => (IDatabaseDriver)Activator.CreateInstance(DriverType)!;
}

/// <summary>Registro globale dei driver database disponibili.</summary>
public static class DriverRegistry
{
    private static readonly Dictionary<string, RegisteredDriver> _drivers =
        new(StringComparer.OrdinalIgnoreCase);

    static DriverRegistry()
    {
        // Registra tutti i driver built-in che hanno [DatabaseDriver] attribute
        var driverTypes = typeof(DriverRegistry).Assembly.GetExportedTypes()
            .Where(t => !t.IsAbstract && typeof(IDatabaseDriver).IsAssignableFrom(t));

        foreach (var type in driverTypes)
        {
            var attr = type.GetCustomAttribute<DatabaseDriverAttribute>();
            if (attr != null)
                Register(attr.Name, type, attr.Extensions, attr.Description);
        }
    }

    /// <summary>Registra manualmente un driver.</summary>
    public static void Register(string name, Type driverType, string[]? extensions = null, string? description = null)
    {
        if (!typeof(IDatabaseDriver).IsAssignableFrom(driverType))
            throw new ArgumentException($"Il tipo {driverType.Name} non implementa IDatabaseDriver");

        _drivers[name] = new RegisteredDriver(name, driverType, extensions, description);
    }

    /// <summary>Carica driver da un assembly esterno (.dll).</summary>
    public static void LoadFromAssembly(string assemblyPath)
    {
        var assembly = Assembly.LoadFrom(assemblyPath);
        foreach (var type in assembly.GetExportedTypes())
        {
            if (type.IsAbstract || !typeof(IDatabaseDriver).IsAssignableFrom(type))
                continue;

            var attr = type.GetCustomAttribute<DatabaseDriverAttribute>();
            if (attr != null)
                Register(attr.Name, type, attr.Extensions, attr.Description);
        }
    }

    /// <summary>Crea un'istanza di un driver per nome.</summary>
    public static IDatabaseDriver Create(string driverName)
    {
        if (!_drivers.TryGetValue(driverName, out var reg))
        {
            var available = string.Join(", ", _drivers.Keys);
            throw new ArgumentException(
                $"Driver '{driverName}' non trovato. Driver disponibili: {available}");
        }
        return reg.CreateInstance();
    }

    /// <summary>Trova il driver appropriato per un'estensione file.</summary>
    public static string? FindForExtension(string filePath)
    {
        var ext = Path.GetExtension(filePath);
        foreach (var (name, reg) in _drivers)
        {
            if (reg.Extensions == null) continue;
            if ((!string.IsNullOrEmpty(ext) && reg.Extensions.Contains(ext, StringComparer.OrdinalIgnoreCase))
                || reg.Extensions.Contains(filePath, StringComparer.OrdinalIgnoreCase))
                return name;
        }
        return null;
    }

    /// <summary>Elenco dei nomi dei driver disponibili.</summary>
    public static IReadOnlyList<string> AvailableDrivers => _drivers.Keys.ToList();
}
