// FILE: Basic.Mcp.McpToolRegistry
using System.Text.Json.Nodes;

namespace Basic.Mcp;

/// <summary>In-process registry of MCP tools exposed by the BASIC runtime.</summary>
public sealed class McpToolRegistry
{
    public static McpToolRegistry Instance { get; } = new();

    private readonly Lock _lock = new();
    private readonly Dictionary<string, McpToolDefinition> _tools = new(StringComparer.OrdinalIgnoreCase);

    private McpToolRegistry()
    {
    }

    /// <summary>Register or replace a tool definition.</summary>
    public void Register(McpToolDefinition tool)
    {
        ArgumentNullException.ThrowIfNull(tool);
        ArgumentException.ThrowIfNullOrWhiteSpace(tool.Name);
        lock (_lock)
            _tools[tool.Name] = tool;
    }

    /// <summary>Remove a tool by name.</summary>
    public bool Unregister(string toolName)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(toolName);
        lock (_lock)
            return _tools.Remove(toolName);
    }

    /// <summary>Get a tool by name.</summary>
    public McpToolDefinition? Get(string toolName)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(toolName);
        lock (_lock)
            return _tools.TryGetValue(toolName, out var tool) ? tool : null;
    }

    /// <summary>List all registered tools.</summary>
    public IReadOnlyList<McpToolDefinition> List()
    {
        lock (_lock)
            return _tools.Values.ToList();
    }

    /// <summary>Build MCP <c>tools/list</c> result objects.</summary>
    public JsonArray ToMcpToolList()
    {
        var array = new JsonArray();
        foreach (var tool in List().OrderBy(t => t.Name, StringComparer.OrdinalIgnoreCase))
        {
            var entry = new JsonObject
            {
                ["name"] = tool.Name,
                ["description"] = string.IsNullOrWhiteSpace(tool.Description)
                    ? $"BASIC tool {tool.QualifiedProcedureName}"
                    : tool.Description,
                ["inputSchema"] = tool.InputSchema.DeepClone()
            };
            if (tool.OutputSchema is { } outputSchema)
                entry["outputSchema"] = outputSchema.DeepClone();
            if (tool.ReadOnly)
                entry["readOnly"] = true;
            array.Add(entry);
        }

        return array;
    }

    internal static void ResetForTests()
    {
        lock (Instance._lock)
            Instance._tools.Clear();
    }
}
