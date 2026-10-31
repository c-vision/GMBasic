// FILE: Basic.Connectors.ConnectorDefinition
// Connector Runtime F0 — definizione uniforme per i 52 connettori (spec MCP SYSTEM.txt §2-§3).
// Un connettore astrae: authentication | capabilities | tools | resources | schemas | permissions | rate limits | state | audit log
// L'agente vede gmail.search() non gmail.googleapis.com — il connettore nasconde REST/pagination/MIME/OAuth refresh.

using System.Text.Json.Serialization;

namespace Basic.Connectors;

/// <summary>Definizione di un connettore (manifest + stato runtime).</summary>
public sealed record ConnectorDefinition(
    string Name,
    string Kind,
    string? DisplayName = null,
    string Version = "1.0",
    string AuthKind = "none",
    List<string>? Capabilities = null,
    List<string>? Tools = null,
    string? Description = null,
    string? Url = null,
    string? Command = null,
    List<string>? Args = null,
    Dictionary<string, string>? Env = null,
    Dictionary<string, string>? Secrets = null,
    Dictionary<string, string>? ToolPolicies = null,
    bool Enabled = true,
    DateTimeOffset? CreatedAt = null,
    DateTimeOffset? UpdatedAt = null)
{
    [JsonIgnore] public string EffectiveDisplayName => DisplayName ?? Name;
    [JsonIgnore] public IReadOnlyList<string> EffectiveCapabilities => Capabilities ?? [];
    [JsonIgnore] public IReadOnlyList<string> EffectiveTools => Tools ?? [];
    [JsonIgnore] public bool IsOAuth => AuthKind.Equals("oauth2", StringComparison.OrdinalIgnoreCase);
    [JsonIgnore] public bool IsApiKey => AuthKind.Equals("apikey", StringComparison.OrdinalIgnoreCase);
}

/// <summary>Capability di un connettore (es. mail.read vs mail.send).</summary>
public sealed record ConnectorCapability(
    string Name,
    string Level,
    string? Description = null)
{
    public bool IsRead => Level.Equals("READ", StringComparison.OrdinalIgnoreCase);
    public bool IsWrite => Level.Equals("WRITE", StringComparison.OrdinalIgnoreCase);
    public bool IsDestructive => Level.Equals("DESTRUCTIVE", StringComparison.OrdinalIgnoreCase);
    public bool IsHighRisk => IsDestructive || Name.Contains("delete", StringComparison.OrdinalIgnoreCase)
        || Name.Contains("send", StringComparison.OrdinalIgnoreCase);
}

/// <summary>Tool esposto da un connettore (metadata per discovery).</summary>
public sealed record ConnectorTool(
    string Name,
    string Description,
    string InputSchemaJson = "{}",
    string OutputSchemaJson = "{}",
    string Permission = "read",
    string RiskLevel = "LOW",
    bool RequiresApproval = false)
{
    public bool IsHighRisk => RiskLevel.Equals("HIGH", StringComparison.OrdinalIgnoreCase)
        || RiskLevel.Equals("DESTRUCTIVE", StringComparison.OrdinalIgnoreCase);
}
