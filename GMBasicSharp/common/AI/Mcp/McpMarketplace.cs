// FILE: Basic.Mcp.McpMarketplace
// Catalogo dei server MCP piu' comuni (definizioni, non installer). I server stdio usano
// il comando noto (npx/uvx); l'eseguibile deve essere gia' sul PATH dell'utente — il
// meccanismo non installa pacchetti esterni. L'aggiunta da marketplace scopre i tool
// (initialize + tools/list) e li rende disponibili a AI.MCPLISTTOOLSSERVER / AI.MCPCALL.

namespace Basic.Mcp;

public static class McpMarketplace
{
    /// <summary>Server di riferimento del marketplace ufficiale (modelcontextprotocol/servers),
    /// con i comandi di avvio standard. Lista estendibile da un manifest remoto.</summary>
    public static IReadOnlyList<McpServerEntry> Catalog { get; } =
    [
        new McpServerEntry("filesystem", "stdio",
            Command: "npx", Args: ["-y", "@modelcontextprotocol/server-filesystem"]),
        new McpServerEntry("fetch", "stdio",
            Command: "npx", Args: ["-y", "@modelcontextprotocol/server-fetch"]),
        new McpServerEntry("memory", "stdio",
            Command: "npx", Args: ["-y", "@modelcontextprotocol/server-memory"]),
        new McpServerEntry("git", "stdio",
            Command: "npx", Args: ["-y", "@modelcontextprotocol/server-git"]),
        new McpServerEntry("time", "stdio",
            Command: "npx", Args: ["-y", "@modelcontextprotocol/server-time"]),
        new McpServerEntry("sequential-thinking", "stdio",
            Command: "npx", Args: ["-y", "@modelcontextprotocol/server-sequential-thinking"]),
    ];

    public static McpServerEntry? Get(string name) =>
        Catalog.FirstOrDefault(s => s.Name.Equals(name, StringComparison.OrdinalIgnoreCase));

    /// <summary>Tool scoperti per server (cache in-memory, popolata da AddServer/Discover).</summary>
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, List<(string Name, string Description)>>
        _discovered = new(StringComparer.OrdinalIgnoreCase);

    public static void RememberTools(string server, IReadOnlyList<(string Name, string Description)> tools) =>
        _discovered[server] = [.. tools];

    public static IReadOnlyList<(string Name, string Description)>? GetTools(string server) =>
        _discovered.TryGetValue(server, out var t) ? t : null;

    internal static void ResetForTests() => _discovered.Clear();

    /// <summary>Client tools dei server abilitati nella lista client (auto-discovery per gli
    /// agenti): ogni tool scoperto diventa <c>server.tool</c>.</summary>
    public static List<McpToolDefinition> GetClientToolDefinitions()
    {
        var result = new List<McpToolDefinition>();
        foreach (var server in McpConfig.Servers.Where(s => s.Enabled))
        {
            var tools = GetTools(server.Name);
            if (tools is null) continue;
            foreach (var (tool, desc) in tools)
                result.Add(new McpToolDefinition
                {
                    Name = $"{server.Name}.{tool}",
                    Description = $"{desc} (server {server.Name})",
                    QualifiedProcedureName = $"{server.Name}.{tool}",
                    InputSchema = new System.Text.Json.Nodes.JsonObject()
                });
        }
        return result;
    }
}
