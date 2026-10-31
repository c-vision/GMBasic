// FILE: Ast.RuntimeRecords.cs

using System.Text.Json;
using System.Text.Json.Nodes;

namespace Basic;

/// <summary>
/// Istanza runtime di un tipo definito dall'utente (TYPE...END TYPE).
/// </summary>
public sealed record UdtInstance(UserDefinedType Definition, Dictionary<int, object?> Fields);

/// <summary>
/// Incapsula un nodo JSON per la serializzazione e il runtime.
/// </summary>
public sealed record JsonWrapper(JsonNode Node)
{
    /// <inheritdoc />
    public override string ToString() =>
        Node.ToJsonString(new JsonSerializerOptions { WriteIndented = true });
}
