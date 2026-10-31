// FILE: Basic.Mcp.McpToolDefinition
using System.Text.Json.Nodes;

namespace Basic.Mcp;

/// <summary>Registered MCP tool backed by a BASIC procedure.</summary>
public sealed class McpToolDefinition
{
    public required string Name { get; init; }

    public string Description { get; init; } = string.Empty;

    public required string QualifiedProcedureName { get; init; }

    public required JsonObject InputSchema { get; init; }

    /// <summary>Optional <c>outputSchema</c> (MCP tool result metadata). Null = not declared.</summary>
    public JsonObject? OutputSchema { get; init; }

    /// <summary>Classification MCP-001-style: a read-only tool consults data/executes pure
    /// calculations and never mutates state; a mutating tool is explicitly flagged. Default
    /// <c>false</c> (mutating) — read-only is an explicit declaration, not an inference.</summary>
    public bool ReadOnly { get; init; }
}
