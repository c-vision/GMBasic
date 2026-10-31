// FILE: Basic.Connectors.ConnectorPolicy
// Policy per-tool (READ/WRITE/DESTRUCTIVE, HIGH_RISK, approval) — riusa AgentPermissionService come gate unico.

using Basic.Agents;

namespace Basic.Connectors;

/// <summary>Livello di rischio per un tool di un connettore.</summary>
public enum ConnectorRiskLevel { Low, Medium, High, Destructive }

/// <summary>Policy di un tool (allow/deny/approve).</summary>
public sealed record ConnectorToolPolicy(
    string ToolName,
    string Policy,
    ConnectorRiskLevel Risk = ConnectorRiskLevel.Low,
    bool RequiresApproval = false)
{
    public bool IsHighRisk => Risk is ConnectorRiskLevel.High or ConnectorRiskLevel.Destructive || RequiresApproval;
}

public static class ConnectorPolicy
{
    // Tool ad alto rischio (documento §19 — HIGH_RISK connector: payment, delete, send_email, production_deploy, git_push)
    private static readonly HashSet<string> HighRiskTools = new(StringComparer.OrdinalIgnoreCase)
    {
        "send", "delete", "payment", "refund", "production_deploy", "git_push", "git.push",
        "mail.send", "mail.delete", "drive.delete", "database.delete", "stripe.payment",
        "stripe.refund", "docusign.envelopes", "gmail.send", "slack.send", "teams.send",
    };

    private static readonly HashSet<string> DestructiveTools = new(StringComparer.OrdinalIgnoreCase)
    {
        "delete", "destroy", "drop", "truncate", "remove", "purge", "zap",
    };

    public static ConnectorRiskLevel Classify(string connectorName, string toolName)
    {
        var full = $"{connectorName}.{toolName}".ToLowerInvariant();
        var bare = toolName.ToLowerInvariant();
        if (DestructiveTools.Any(d => bare.Contains(d) || full.Contains(d))) return ConnectorRiskLevel.Destructive;
        if (HighRiskTools.Contains(bare) || HighRiskTools.Contains(full) || HighRiskTools.Contains(toolName))
            return ConnectorRiskLevel.High;
        if (bare.Contains("write") || bare.Contains("create") || bare.Contains("update") || bare.Contains("send"))
            return ConnectorRiskLevel.Medium;
        return ConnectorRiskLevel.Low;
    }

    public static bool IsHighRisk(string connectorName, string toolName) =>
        Classify(connectorName, toolName) is ConnectorRiskLevel.High or ConnectorRiskLevel.Destructive;

    /// <summary>Valuta la policy per un connettore/tool con il livello agente (Sandbox/Supervised/YOLO).</summary>
    public static AgentVerdict Evaluate(string connectorName, string toolName, AgentPermissionLevel level, string? perToolPolicy = null)
    {
        if (!string.IsNullOrWhiteSpace(perToolPolicy))
            return perToolPolicy.ToLowerInvariant() switch
            {
                "allow" => AgentVerdict.Allow,
                "deny" => AgentVerdict.Deny,
                "approve" => AgentVerdict.Approve,
                _ => AgentVerdict.Allow
            };
        // Check per-connector policy dal registry
        var stored = ConnectorRegistry.Instance.GetPolicy(connectorName, toolName)
            ?? ConnectorRegistry.Instance.GetPolicy(connectorName, $"{connectorName}.{toolName}");
        if (!string.IsNullOrWhiteSpace(stored))
            return stored.ToLowerInvariant() switch
            {
                "allow" => AgentVerdict.Allow,
                "deny" => AgentVerdict.Deny,
                "approve" => AgentVerdict.Approve,
                _ => AgentVerdict.Allow
            };
        var risk = Classify(connectorName, toolName);
        if (risk is ConnectorRiskLevel.Low) return AgentVerdict.Allow;
        return level switch
        {
            AgentPermissionLevel.YOLO => AgentVerdict.Allow,
            AgentPermissionLevel.Sandbox => AgentVerdict.Deny,
            AgentPermissionLevel.Supervised => AgentVerdict.Approve,
            _ => AgentVerdict.Allow
        };
    }
}
