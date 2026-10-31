// FILE: Basic.Mcp.McpSchemaBuilder
using System.Text.Json.Nodes;

namespace Basic.Mcp;

/// <summary>Builds MCP JSON Schema from reflection-style parameter metadata.</summary>
public static class McpSchemaBuilder
{
    /// <summary>Creates an MCP <c>inputSchema</c> from parameter name/type pairs.</summary>
    public static JsonObject FromParameters(IEnumerable<(string Name, string TypeName, bool Optional)> parameters)
    {
        var properties = new JsonObject();
        var required = new JsonArray();

        foreach (var (name, typeName, optional) in parameters)
        {
            properties[name] = MapBasicTypeToJsonSchema(typeName);
            if (!optional)
                required.Add(name);
        }

        return new JsonObject
        {
            ["type"] = "object",
            ["properties"] = properties,
            ["required"] = required
        };
    }

    private static JsonObject MapBasicTypeToJsonSchema(string typeName)
    {
        var upper = typeName.Trim().ToUpperInvariant();
        if (upper.StartsWith("ARRAY OF ", StringComparison.Ordinal) ||
            upper.StartsWith("LIST OF ", StringComparison.Ordinal))
        {
            var element = upper.Contains("STRING", StringComparison.Ordinal) ? "string" : "number";
            return new JsonObject
            {
                ["type"] = "array",
                ["items"] = new JsonObject { ["type"] = element }
            };
        }

        return upper switch
        {
            "STRING" => new JsonObject { ["type"] = "string" },
            "BIT" or "BOOLEAN" or "BOOL" => new JsonObject { ["type"] = "boolean" },
            "INTEGER" or "SHORT" or "INT" or "LONG" or "QUAD" or "BYTE" or "SBYTE" or "UINTEGER" or "ULONG" or "UQUAD" =>
                new JsonObject { ["type"] = "integer" },
            "SINGLE" or "DOUBLE" or "NUMBER" or "CURRENCY" => new JsonObject { ["type"] = "number" },
            "JSON" => new JsonObject { ["type"] = "object" },
            _ => new JsonObject { ["type"] = "string", ["description"] = upper }
        };
    }
}
