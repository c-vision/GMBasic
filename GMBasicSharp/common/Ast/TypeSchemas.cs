// FILE: Ast.TypeSchemas.cs

namespace Basic;

/// <summary>Definisce lo schema (membri e metodi) per un tipo built-in complesso non-UDT.</summary>
public sealed record BuiltinTypeSchema(
    IReadOnlyDictionary<string, DataType> Fields,
    IReadOnlyDictionary<string, DataType> Methods
);

/// <summary>Registro centrale degli schemi dei tipi built-in, popolato da StdLib e usato dal Linker.</summary>
public static class TypeSchemaRegistry
{
    private static readonly Dictionary<Type, BuiltinTypeSchema> _schemas = new();

    public static void Register(Type astNodeType, BuiltinTypeSchema schema) => _schemas[astNodeType] = schema;

    public static bool TryGetValue(Type type, out BuiltinTypeSchema? schema) => _schemas.TryGetValue(type, out schema);

    public static bool ContainsKey(Type type) => _schemas.ContainsKey(type);

    /// <summary>Enumera tutte le coppie tipo→schema registrate (per METHODINFO/PROPERTYINFO).</summary>
    public static IEnumerable<KeyValuePair<Type, BuiltinTypeSchema>> Enumerate()
    {
        foreach (var kv in _schemas) yield return kv;
    }
}
