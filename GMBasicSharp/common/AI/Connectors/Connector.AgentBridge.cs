// FILE: Basic.Connectors.ConnectorAgentBridge
// Bridge connettori → agenti (ogni agente creato sa dei connettori)

using Basic.Agents;
using System.Text.Json;

namespace Basic.Connectors;

public static class ConnectorRuntimeHelper
{
    public static string CallForAgent(string connectorName, string tool, string argsJson, AgentRecord? agent = null)
    {
        var def = ConnectorRegistry.Instance.Get(connectorName)
            ?? throw new ArgumentException($"Connector '{connectorName}' not found.");
        if (!def.Enabled) throw new InvalidOperationException($"Connector '{connectorName}' disabled.");

        // Policy gate con il livello dell'agente (se disponibile) o YOLO
        var level = agent?.PermissionLevel ?? AgentPermissionLevel.YOLO;
        var policy = ConnectorRegistry.Instance.GetPolicy(connectorName, tool);
        var verdict = ConnectorPolicy.Evaluate(connectorName, tool, level, policy);
        if (verdict == AgentVerdict.Deny) throw new InvalidOperationException($"Policy denied: {connectorName}.{tool}");
        if (verdict == AgentVerdict.Approve)
        {
            if (agent is null) throw new InvalidOperationException($"Policy requires approval: {connectorName}.{tool}");
            var approved = AgentPermissionService.Instance.RequestApprovalAndWait(agent, $"{connectorName}.{tool}");
            if (!approved) throw new InvalidOperationException($"Policy approval denied/timeout: {connectorName}.{tool}");
        }

        if (!ConnectorSessionManager.Instance.TryAcquire(connectorName))
            throw new InvalidOperationException($"Rate limit exceeded for connector '{connectorName}'.");

        // Dispatch nativo (filesystem/http/database/google/github) — riusa la stessa logica di ConnectorRuntime
        // Evita dipendenza da IlBackend: chiama direttamente i dispatcher dei connettori
        var k = def.Kind.ToLowerInvariant();
        var n = connectorName.ToLowerInvariant();
        string? result = null;
        if (k == "filesystem" || n == "filesystem" || n == "fs")
            result = ConnectorFilesystem.Dispatch(tool, argsJson);
        else if (k == "http" || k == "web" || n == "http" || n == "web")
            result = ConnectorHttp.Dispatch(tool, argsJson);
        else if (k is "postgres" or "sqlite" or "mysql" or "mariadb" || n is "postgres" or "sqlite" || k.Contains("postgres") || k.Contains("sqlite"))
            result = ConnectorDatabase.Dispatch(connectorName, tool, argsJson);
        else if (k is "gmail" or "drive" or "calendar" || n.Contains("gmail") || n.Contains("drive") || n.Contains("calendar"))
            result = ConnectorGoogle.Dispatch(connectorName, tool, argsJson);
        else if (k == "github" || n.Contains("github"))
            result = ConnectorGitHub.Dispatch(tool, argsJson, connectorName);
        else if (ConnectorMarketplace.Get(connectorName) is not null)
            result = ConnectorGenericHttp.Dispatch(connectorName, tool, argsJson);

        if (result is not null)
        {
            ConnectorSessionManager.Instance.RecordCall(connectorName, true);
            return result;
        }

        // Fallback stub per gli altri 40+ connettori (T1-T4) non ancora in F1
        var payload = new { connector = connectorName, tool, args = TryParseJson(argsJson), status = "ok", note = $"native connector '{connectorName}' stub — F1 pending ({def.Kind})" };
        ConnectorSessionManager.Instance.RecordCall(connectorName, true);
        return JsonSerializer.Serialize(payload, new JsonSerializerOptions { WriteIndented = true });
    }

    private static object? TryParseJson(string json) { try { return JsonSerializer.Deserialize<object>(json); } catch { return json; } }
}
