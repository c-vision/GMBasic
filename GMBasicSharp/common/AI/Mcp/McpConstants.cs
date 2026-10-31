// FILE: Basic.Mcp.McpConstants
namespace Basic.Mcp;

/// <summary>MCP protocol constants (2024-11-05 subset + Connector Runtime extensions).</summary>
public static class McpConstants
{
    public const string ProtocolVersion = "2024-11-05";

    public const string MethodInitialize = "initialize";
    public const string MethodInitialized = "notifications/initialized";
    public const string MethodToolsList = "tools/list";
    public const string MethodToolsCall = "tools/call";
    public const string MethodResourcesList = "resources/list";
    public const string MethodResourcesRead = "resources/read";
    public const string MethodResourcesSubscribe = "resources/subscribe";
    public const string MethodResourcesUnsubscribe = "resources/unsubscribe";
    public const string MethodPromptsList = "prompts/list";
    public const string MethodPromptsGet = "prompts/get";
    public const string MethodPing = "ping";
    public const string MethodSamplingCreateMessage = "sampling/createMessage";

    public const int DefaultMaxMessageBytes = 4 * 1024 * 1024;
    public const int DefaultToolTimeoutMs = 30_000;

    public const string EnvApiKey = "GMBASIC_MCP_API_KEY";
    public const string EnvAuditLedgerPath = "GMBASIC_MCP_AUDIT_LEDGER";
    public const string EnvHttpMaxBodyBytes = "GMBASIC_MCP_HTTP_MAX_BODY_BYTES";
    public const string EnvHttpRateLimitPerMinute = "GMBASIC_MCP_HTTP_RATE_LIMIT_PER_MINUTE";
}
