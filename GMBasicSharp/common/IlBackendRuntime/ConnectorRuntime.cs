// FILE: Basic.IlBackend.ConnectorRuntime
// Runtime per Connector (F0) — wrappa Basic.Connectors.* per il backend IL/PCode.

using Basic.Connectors;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Basic.IlBackend;

public static class ConnectorRuntime
{
    public static string ConnectorList()
    {
        var list = ConnectorRegistry.Instance.List();
        return JsonSerializer.Serialize(list, new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase, DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull });
    }

    public static string ConnectorStatus(string name)
    {
        var def = ConnectorRegistry.Instance.Get(name);
        if (def is null) return $"{{\"error\":\"connector '{name}' not found\"}}";
        var session = ConnectorSessionManager.Instance.Get(name);
        var obj = new
        {
            def.Name, def.Kind, def.DisplayName, def.Version, def.AuthKind,
            def.Enabled, def.Description,
            capabilities = def.Capabilities,
            tools = def.Tools,
            session = session is null ? null : new { session.CallCount, session.ErrorCount, session.IsConnected, session.LastUsedAt, session.LastError },
        };
        return JsonSerializer.Serialize(obj, new JsonSerializerOptions { WriteIndented = true, PropertyNamingPolicy = JsonNamingPolicy.CamelCase });
    }

    public static string ConnectorTools(string name)
    {
        var tools = ConnectorRegistry.Instance.GetTools(name);
        if (tools is null) return "[]";
        return JsonSerializer.Serialize(tools, new JsonSerializerOptions { WriteIndented = true, PropertyNamingPolicy = JsonNamingPolicy.CamelCase });
    }

    public static string ConnectorSearch(string capability)
    {
        var found = ConnectorRegistry.Instance.Search(capability);
        return JsonSerializer.Serialize(found, new JsonSerializerOptions { WriteIndented = true, PropertyNamingPolicy = JsonNamingPolicy.CamelCase });
    }

    public static string ConnectorMarketplaceList()
    {
        var catalog = ConnectorRegistry.Instance.MarketplaceList();
        return JsonSerializer.Serialize(catalog, new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase, DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull });
    }

    public static string ConnectorMarketplaceGet(string name)
    {
        var entry = ConnectorRegistry.Instance.MarketplaceGet(name)
            ?? throw new ArgumentException($"Connector '{name}' not in marketplace (use CONNECTOR MARKETPLACE LIST).");
        // Auto-register if not exists (like MCP marketplace get)
        if (ConnectorRegistry.Instance.Get(name) is null)
        {
            var def = new ConnectorDefinition(Name: entry.Name, Kind: entry.Kind, DisplayName: entry.DisplayName, Version: entry.Version, AuthKind: entry.AuthKind, Capabilities: entry.Capabilities.ToList(), Tools: entry.Tools.ToList(), Description: entry.Description, Url: entry.Url, Command: entry.Command, Args: entry.Args?.ToList(), Enabled: entry.Enabled);
            ConnectorRegistry.Instance.Register(def);
        }
        return JsonSerializer.Serialize(entry, new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase, DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull });
    }

    public static double ConnectorRegister(string name, string kind, string? configJson = null)
    {
        // F4 OpenAPI: se kind è openapi e configJson è un URL http, genera il connettore da openapi.json
        if (kind.Equals("openapi", StringComparison.OrdinalIgnoreCase) && !string.IsNullOrWhiteSpace(configJson) && configJson.Trim().StartsWith("http", StringComparison.OrdinalIgnoreCase))
        {
            try { Basic.Connectors.ConnectorOpenApi.CreateFromUrl(name, configJson.Trim().Trim('"')); return 1.0; }
            catch { /* fallback a registrazione normale */ }
        }
        var existing = ConnectorMarketplace.Get(name);
        var def = existing is not null
            ? new ConnectorDefinition(Name: name, Kind: kind, DisplayName: existing.DisplayName, Version: existing.Version, AuthKind: existing.AuthKind, Capabilities: existing.Capabilities.ToList(), Tools: existing.Tools.ToList(), Description: existing.Description, Url: existing.Url, Command: existing.Command, Args: existing.Args?.ToList(), Enabled: true)
            : new ConnectorDefinition(Name: name, Kind: kind, DisplayName: name, Version: "1.0", AuthKind: "none", Capabilities: [kind], Tools: [kind], Description: $"Connector {name} ({kind})", Enabled: true);
        // Merge configJson if provided
        if (!string.IsNullOrWhiteSpace(configJson))
        {
            try
            {
                using var doc = JsonDocument.Parse(configJson);
                if (doc.RootElement.TryGetProperty("auth", out var a)) def = def with { AuthKind = a.GetString() ?? def.AuthKind };
                if (doc.RootElement.TryGetProperty("description", out var d)) def = def with { Description = d.GetString() };
            }
            catch { /* best-effort */ }
        }
        // If already exists, update
        var current = ConnectorRegistry.Instance.Get(name);
        if (current is not null) return ConnectorRegistry.Instance.Get(name) is not null && ConnectorConfig.Update(def) ? 1.0 : 0.0;
        return ConnectorRegistry.Instance.Register(def) ? 1.0 : 0.0;
    }

    public static double ConnectorUnregister(string name) => ConnectorRegistry.Instance.Unregister(name) ? 1.0 : 0.0;

    public static double ConnectorLoad(string name)
    {
        var entry = ConnectorMarketplace.Get(name);
        if (entry is null) return 0.0;
        var def = new ConnectorDefinition(Name: entry.Name, Kind: entry.Kind, DisplayName: entry.DisplayName, Version: entry.Version, AuthKind: entry.AuthKind, Capabilities: entry.Capabilities.ToList(), Tools: entry.Tools.ToList(), Description: entry.Description, Url: entry.Url, Command: entry.Command, Args: entry.Args?.ToList(), Enabled: true);
        var current = ConnectorRegistry.Instance.Get(name);
        if (current is not null)
        {
            // Già abilitato → idempotente: NON riscrivere UpdatedAt (evita di sporcare lo stato
            // persistito con un timestamp non deterministico a ogni LOAD).
            if (current.Enabled) return 1.0;
            return ConnectorRegistry.Instance.SetEnabled(name, true) ? 1.0 : 0.0;
        }
        return ConnectorRegistry.Instance.Register(def) ? 1.0 : 0.0;
    }

    public static double ConnectorEnable(string name) => ConnectorRegistry.Instance.SetEnabled(name, true) ? 1.0 : 0.0;
    public static double ConnectorDisable(string name) => ConnectorRegistry.Instance.SetEnabled(name, false) ? 1.0 : 0.0;

    public static string ConnectorCall(string connectorName, string tool, string argsJson)
    {
        var def = ConnectorRegistry.Instance.Get(connectorName)
            ?? throw new ArgumentException($"Connector '{connectorName}' not found. Use CONNECTOR LIST or MARKETPLACE GET.");
        if (!def.Enabled) throw new InvalidOperationException($"Connector '{connectorName}' disabled. Use CONNECTOR ENABLE.");
        // Policy gate
        var verdict = ConnectorPolicy.Evaluate(connectorName, tool, Basic.Agents.AgentPermissionLevel.YOLO, ConnectorRegistry.Instance.GetPolicy(connectorName, tool));
        if (verdict == Basic.Agents.AgentVerdict.Deny) throw new InvalidOperationException($"Policy denied: {connectorName}.{tool}");
        if (verdict == Basic.Agents.AgentVerdict.Approve) throw new InvalidOperationException($"Policy requires approval: {connectorName}.{tool} (use CONNECTOR POLICY SET {connectorName}, {tool}, allow)");

        // Rate limit
        if (!ConnectorSessionManager.Instance.TryAcquire(connectorName))
            throw new InvalidOperationException($"Rate limit exceeded for connector '{connectorName}'.");

        // Dispatch: try MCP client ONLY if connector is NOT one of the 52 native connectors (our marketplace)
        // e se è un MCP server esplicito. Così filesystem/http etc. nativi non collidono con l'MCP server-filesystem di default.
        var isNative = ConnectorMarketplace.Get(connectorName) is not null || ConnectorRegistry.Instance.Get(connectorName)?.Kind != "mcp";
        var mcpEntry = Basic.Mcp.McpConfig.Get(connectorName);
        if (!isNative && mcpEntry is not null)
        {
            try
            {
                var result = Basic.Mcp.McpClient.CallTool(mcpEntry, tool, argsJson);
                ConnectorSessionManager.Instance.RecordCall(connectorName, true);
                return result;
            }
            catch (Exception ex)
            {
                ConnectorSessionManager.Instance.RecordCall(connectorName, false, ex.Message);
                throw;
            }
        }
        // Se è nativo ma ha anche un backing MCP (es. filesystem), prova comunque il fallback MCP solo se il tool è noto MCP
        if (isNative && mcpEntry is not null)
        {
            var mcpTools = Basic.Mcp.McpMarketplace.GetTools(connectorName);
            if (mcpTools is not null && mcpTools.Any(t => t.Name.Equals(tool, StringComparison.OrdinalIgnoreCase)))
            {
                try
                {
                    var result = Basic.Mcp.McpClient.CallTool(mcpEntry, tool, argsJson);
                    ConnectorSessionManager.Instance.RecordCall(connectorName, true);
                    return result;
                }
                catch { /* fallback to native stub */ }
            }
        }
        // F1 native implementations (filesystem/http/database) — dispatch per kind/name
        try
        {
            var nativeResult = TryDispatchNative(connectorName, def.Kind, tool, argsJson);
            if (nativeResult is not null)
            {
                ConnectorSessionManager.Instance.RecordCall(connectorName, true);
                return nativeResult;
            }
        }
        catch (Exception ex)
        {
            ConnectorSessionManager.Instance.RecordCall(connectorName, false, ex.Message);
            throw;
        }
        // Fallback stub — per gmail/drive/github etc. (F1 pending)
        var payload = new { connector = connectorName, tool, args = TryParseJson(argsJson), status = "ok", note = $"native connector '{connectorName}' stub — F1 pending ({def.Kind}), use CONNECTOR MARKETPLACE GET to load full impl" };
        ConnectorSessionManager.Instance.RecordCall(connectorName, true);
        return JsonSerializer.Serialize(payload, new JsonSerializerOptions { WriteIndented = true });
    }

    public static double ConnectorRefresh(string name)
    {
        // For MCP-backed connectors, rediscover tools
        var mcpEntry = Basic.Mcp.McpConfig.Get(name);
        if (mcpEntry is not null)
        {
            try
            {
                var tools = Basic.Mcp.McpClient.DiscoverTools(mcpEntry);
                var ctools = tools.Select(t => new ConnectorTool(t.Name, t.Description)).ToList();
                ConnectorRegistry.Instance.RememberTools(name, ctools);
                Basic.Mcp.McpMarketplace.RememberTools(name, tools);
            }
            catch { /* best-effort */ }
        }
        ConnectorSessionManager.Instance.GetOrCreate(name);
        return 1.0;
    }

    public static string ConnectorPolicyGet(string connector, string tool)
    {
        var p = ConnectorRegistry.Instance.GetPolicy(connector, tool);
        return p ?? "allow";
    }

    public static double ConnectorPolicySet(string connector, string tool, string policy)
    {
        return ConnectorRegistry.Instance.SetPolicy(connector, tool, policy) ? 1.0 : 0.0;
    }

    public static string ConnectorSecretGet(string connector, string key)
    {
        var v = ConnectorRegistry.Instance.GetSecret(connector, key);
        return v ?? "";
    }

    public static double ConnectorSecretSet(string connector, string key, string value)
    {
        return ConnectorRegistry.Instance.SetSecret(connector, key, value) ? 1.0 : 0.0;
    }

    public static double ConnectorAuth(string connector, string authJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(authJson);
            if (doc.RootElement.TryGetProperty("api_key", out var ak) || doc.RootElement.TryGetProperty("apiKey", out ak))
                ConnectorRegistry.Instance.SetSecret(connector, "api_key", ak.GetString() ?? "");
            if (doc.RootElement.TryGetProperty("access_token", out var at))
                ConnectorRegistry.Instance.SetSecret(connector, "access_token", at.GetString() ?? "");
            if (doc.RootElement.TryGetProperty("refresh_token", out var rt))
                ConnectorRegistry.Instance.SetSecret(connector, "refresh_token", rt.GetString() ?? "");
            if (doc.RootElement.TryGetProperty("client_id", out var ci))
                ConnectorRegistry.Instance.SetSecret(connector, "client_id", ci.GetString() ?? "");
            // Persist full auth JSON as oauth_token if contains access_token
            if (doc.RootElement.TryGetProperty("access_token", out _))
                ConnectorRegistry.Instance.SetSecret(connector, "oauth_token", authJson);
        }
        catch { return 0.0; }
        return 1.0;
    }

    public static string ConnectorResources(string name)
    {
        // For now, delegate to MCP resources/list if MCP backed, else return connector tools as resources
        var mcpEntry = Basic.Mcp.McpConfig.Get(name);
        if (mcpEntry is not null)
        {
            try
            {
                // Use McpClient internal? Fallback to tool list as resources
                var tools = Basic.Mcp.McpMarketplace.GetTools(name);
                if (tools is not null)
                {
                    var res = tools.Select(t => new { uri = $"connector://{name}/{t.Name}", name = t.Name, description = t.Description });
                    return JsonSerializer.Serialize(res, new JsonSerializerOptions { WriteIndented = true });
                }
            }
            catch { }
        }
        var ctools = ConnectorRegistry.Instance.GetTools(name);
        if (ctools is null) return "[]";
        var arr = ctools.Select(t => new { uri = $"connector://{name}/{t.Name}", name = t.Name, description = t.Description });
        return JsonSerializer.Serialize(arr, new JsonSerializerOptions { WriteIndented = true });
    }

    public static string ConnectorPrompts(string name)
    {
        // Mirror AI agents as prompts for connector context (like McpServer prompts/list)
        var agents = Basic.Agents.AgentServer.Instance.List();
        var prompts = agents.Select(a => new { name = $"chat-with-{a.Name}", description = $"Chat with agent {a.Name} via connector {name}" });
        return JsonSerializer.Serialize(prompts, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string? TryDispatchNative(string connectorName, string kind, string tool, string argsJson)
    {
        var k = kind.ToLowerInvariant();
        var n = connectorName.ToLowerInvariant();
        // Filesystem (T0) — sandbox + hash/diff
        if (k == "filesystem" || n == "filesystem" || n == "fs")
            return Basic.Connectors.ConnectorFilesystem.Dispatch(tool, argsJson);
        // HTTP / Web (T0) — get/post/put/patch/delete/fetch
        if (k == "http" || k == "web" || n == "http" || n == "web" || n == "fetch")
            return Basic.Connectors.ConnectorHttp.Dispatch(tool, argsJson);
        // Database (T0) — postgres/sqlite/mysql/mariadb/duckdb via common API
        if (k is "postgres" or "sqlite" or "mysql" or "mariadb" or "duckdb" or "sql" || n is "postgres" or "sqlite" || n == "postgres-db" || n == "sqlite-db")
            return Basic.Connectors.ConnectorDatabase.Dispatch(connectorName, tool, argsJson);
        if (k.Contains("postgres") || k.Contains("sqlite") || k.Contains("mysql"))
            return Basic.Connectors.ConnectorDatabase.Dispatch(connectorName, tool, argsJson);
        // Google (gmail/drive/calendar) — via Google APIs con OAuth
        if (k is "gmail" or "drive" or "calendar" || n is "gmail" or "google-drive" or "drive" or "google-calendar" or "calendar" || n.Contains("gmail") || n.Contains("drive") || n.Contains("calendar"))
            return Basic.Connectors.ConnectorGoogle.Dispatch(connectorName, tool, argsJson);
        // GitHub
        if (k == "github" || n == "github" || n.Contains("github") || n.Contains("git"))
            return Basic.Connectors.ConnectorGitHub.Dispatch(tool, argsJson, connectorName);
        // Fallback generico T1-T4 (sheets/docs/notion/linear/jira/discord/telegram/teams/dropbox/box/stripe/hubspot/.../qdrant)
        // Se il connettore è nel marketplace 52, prova HTTP generico (se ha ApiBase e Bearer, fa HTTP reale, altrimenti stub)
        if (Basic.Connectors.ConnectorMarketplace.Get(connectorName) is not null || Basic.Connectors.ConnectorMarketplace.Get(kind) is not null)
            return Basic.Connectors.ConnectorGenericHttp.Dispatch(connectorName, tool, argsJson);
        return null;
    }

    private static object? TryParseJson(string json)
    {
        if (string.IsNullOrWhiteSpace(json)) return null;
        try { return JsonSerializer.Deserialize<object>(json); }
        catch { return json; }
    }
}
