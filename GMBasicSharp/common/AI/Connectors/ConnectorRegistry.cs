// FILE: Basic.Connectors.ConnectorRegistry
// Runtime registry — singleton in-memory + persistenza via ConnectorConfig + discovery tools.

using System.Collections.Concurrent;
using System.Text.Json.Nodes;
using Basic.Mcp;

namespace Basic.Connectors;

/// <summary>Registry runtime dei connettori (singleton, thread-safe, persistito in config/connectors.json).</summary>
public sealed class ConnectorRegistry
{
    public static ConnectorRegistry Instance { get; } = new();

    private readonly Lock _lock = new();
    // Tool scoperti per connettore: connectorName -> tool -> definition
    private readonly ConcurrentDictionary<string, List<ConnectorTool>> _discovered = new(StringComparer.OrdinalIgnoreCase);
    // Capability index: capability -> connectors
    private readonly ConcurrentDictionary<string, List<string>> _capIndex = new(StringComparer.OrdinalIgnoreCase);

    private ConnectorRegistry() { }

    // ── CRUD connettori (delegano a ConnectorConfig con persistenza) ──

    public bool Register(ConnectorDefinition def)
    {
        ArgumentNullException.ThrowIfNull(def);
        ArgumentException.ThrowIfNullOrWhiteSpace(def.Name);
        lock (_lock)
        {
            if (ConnectorConfig.Get(def.Name) is not null) return false;
            var ok = ConnectorConfig.Add(def);
            if (ok && def.Tools is not null && def.Tools.Count > 0)
            {
                var tools = def.Tools.Select(n => new ConnectorTool(n, $"{def.DisplayName} tool {n}")).ToList();
                RememberTools(def.Name, tools);
            }
            return ok;
        }
    }

    public bool Unregister(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            _discovered.TryRemove(name, out _);
            // Chiude anche la sessione runtime (CallCount/LastUsedAt): senza questo la
            // sessione resta nel singleton di processo e riappare in STATUS dopo una nuova
            // registrazione (stato non deterministico tra backend nella stessa sessione test).
            ConnectorSessionManager.Instance.Remove(name);
            return ConnectorConfig.Remove(name);
        }
    }

    public ConnectorDefinition? Get(string name) => ConnectorConfig.Get(name);

    public IReadOnlyList<ConnectorDefinition> List() => ConnectorConfig.Connectors;

    public bool SetEnabled(string name, bool enabled) => ConnectorConfig.SetEnabled(name, enabled);

    public IReadOnlyList<ConnectorDefinition> ListEnabled() => List().Where(c => c.Enabled).ToList();

    // ── Tools discovery per-connector ──

    public void RememberTools(string connectorName, IReadOnlyList<ConnectorTool> tools)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(connectorName);
        _discovered[connectorName] = [.. tools];
        // Aggiorna connettore con la lista tool
        var def = Get(connectorName);
        if (def is not null)
        {
            var toolNames = tools.Select(t => t.Name).ToList();
            ConnectorConfig.Update(def with { Tools = toolNames, UpdatedAt = DateTimeOffset.Now });
        }
        // Bridge: registra ogni tool anche nel McpToolRegistry così gli agenti lo vedono
        // Il dispatcher per i connettori è gestito direttamente in AgentToolLoop (check prefix connector)
        // ma la registrazione qui assicura che ResolveAgentTools lo includa via McpToolRegistry.List()
        foreach (var t in tools)
        {
            var qualified = $"{connectorName}.{t.Name}";
            var schema = TryParseSchema(t.InputSchemaJson);
            var mcpDef = new McpToolDefinition
            {
                Name = qualified,
                Description = t.Description,
                QualifiedProcedureName = $"CONNECTOR:{qualified}",
                InputSchema = schema
            };
            McpToolRegistry.Instance.Register(mcpDef);
        }
    }

    private static JsonObject TryParseSchema(string json)
    {
        try
        {
            if (string.IsNullOrWhiteSpace(json)) return new JsonObject();
            var node = JsonNode.Parse(json);
            return node as JsonObject ?? new JsonObject();
        }
        catch { return new JsonObject(); }
    }

    public IReadOnlyList<ConnectorTool>? GetTools(string connectorName)
    {
        if (_discovered.TryGetValue(connectorName, out var t)) return t;
        // Fallback: ricostruisci dai Tools salvati in config se _discovered è vuoto (restart)
        var def = Get(connectorName);
        if (def?.Tools is not null && def.Tools.Count > 0)
        {
            var tools = def.Tools.Select(n => new ConnectorTool(n, $"{def.DisplayName} tool {n}")).ToList();
            _discovered[connectorName] = tools;
            // Registra anche nel McpToolRegistry per gli agenti
            foreach (var ct in tools)
            {
                var qualified = $"{connectorName}.{ct.Name}";
                if (McpToolRegistry.Instance.Get(qualified) is null)
                {
                    var mcpDef = new McpToolDefinition
                    {
                        Name = qualified,
                        Description = ct.Description,
                        QualifiedProcedureName = $"CONNECTOR:{qualified}",
                        InputSchema = new JsonObject()
                    };
                    McpToolRegistry.Instance.Register(mcpDef);
                }
            }
            return tools;
        }
        return null;
    }

    public IReadOnlyList<ConnectorTool> ListAllTools()
    {
        var all = new List<ConnectorTool>();
        foreach (var kv in _discovered) all.AddRange(kv.Value);
        return all;
    }

    // Per gli agenti: ogni tool scoperto diventa connector.tool
    public IReadOnlyList<ConnectorTool> GetAgentToolDefinitions()
    {
        var result = new List<ConnectorTool>();
        foreach (var conn in ListEnabled())
        {
            var tools = GetTools(conn.Name);
            if (tools is null) continue;
            foreach (var t in tools)
                result.Add(t with { Name = $"{conn.Name}.{t.Name}" });
        }
        return result;
    }

    // ── Ricerca connettori per capability/tool ──

    public IReadOnlyList<ConnectorDefinition> Search(string? capability = null, string? kind = null)
    {
        IEnumerable<ConnectorDefinition> q = List();
        if (!string.IsNullOrWhiteSpace(capability))
            q = q.Where(c => c.Capabilities.Any(cap => cap.Contains(capability, StringComparison.OrdinalIgnoreCase))
                || (GetTools(c.Name)?.Any(t => t.Name.Contains(capability, StringComparison.OrdinalIgnoreCase)) ?? false)
                || c.Name.Contains(capability, StringComparison.OrdinalIgnoreCase));
        if (!string.IsNullOrWhiteSpace(kind))
            q = q.Where(c => c.Kind.Equals(kind, StringComparison.OrdinalIgnoreCase));
        return q.ToList();
    }

    // ── Marketplace ──

    public IReadOnlyList<ConnectorDefinition> MarketplaceList() => ConnectorMarketplace.Catalog;

    public ConnectorDefinition? MarketplaceGet(string name) => ConnectorMarketplace.Get(name);

    // ── Policy / Secrets (delegano a ConnectorConfig) ──

    public bool SetPolicy(string connectorName, string tool, string policy) =>
        ConnectorConfig.SetPolicy(connectorName, tool, policy);

    public string? GetPolicy(string connectorName, string tool)
    {
        var def = Get(connectorName);
        if (def?.ToolPolicies is null) return null;
        return def.ToolPolicies.TryGetValue(tool, out var p) ? p : null;
    }

    public bool SetSecret(string connectorName, string key, string value) =>
        ConnectorConfig.SetSecret(connectorName, key, value);

    public string? GetSecret(string connectorName, string key)
    {
        var def = Get(connectorName);
        if (def?.Secrets is null) return null;
        return def.Secrets.TryGetValue(key, out var v) ? v : null;
    }

    // ── Stato JSON per BASIC ──

    public string ToJson(string? name = null)
    {
        if (!string.IsNullOrWhiteSpace(name))
        {
            var def = Get(name);
            return def is null ? "{}" : System.Text.Json.JsonSerializer.Serialize(def, new System.Text.Json.JsonSerializerOptions { WriteIndented = true, PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.CamelCase });
        }
        return System.Text.Json.JsonSerializer.Serialize(List(), new System.Text.Json.JsonSerializerOptions { WriteIndented = true, PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.CamelCase });
    }

    internal void ResetForTests()
    {
        lock (_lock) _discovered.Clear();
        _capIndex.Clear();
    }
}
