// FILE: Basic.Mcp.McpHttpOptions
namespace Basic.Mcp;

/// <summary>Configuration for the MCP HTTP listener.</summary>
public sealed class McpHttpOptions
{
    /// <summary>Default MCP HTTP port.</summary>
    public const int DefaultPort = 8765;

    /// <summary>TCP port to bind.</summary>
    public int Port { get; init; } = DefaultPort;

    /// <summary>HTTP path for JSON-RPC POST.</summary>
    public string Path { get; init; } = "/mcp";

    /// <summary>API key required for Authorization Bearer or X-API-Key.</summary>
    public string? ApiKey { get; init; }

    /// <summary>Maximum request body size in bytes.</summary>
    public int MaxBodyBytes { get; init; } = McpConstants.DefaultMaxMessageBytes;

    /// <summary>Maximum JSON-RPC requests per minute per client IP.</summary>
    /// <remarks>ponytail: fixed window counter; upgrade path: sliding window + shared store.</remarks>
    public int RequestsPerMinutePerIp { get; init; } = 120;

    /// <summary>Request read timeout.</summary>
    public TimeSpan RequestTimeout { get; init; } = TimeSpan.FromSeconds(30);

    /// <summary>Build options from environment variables.</summary>
    public static McpHttpOptions FromEnvironment(int port = DefaultPort) =>
        new()
        {
            Port = port,
            ApiKey = Environment.GetEnvironmentVariable(McpConstants.EnvApiKey),
            MaxBodyBytes = ParseIntOrDefault(
                Environment.GetEnvironmentVariable(McpConstants.EnvHttpMaxBodyBytes),
                McpConstants.DefaultMaxMessageBytes),
            RequestsPerMinutePerIp = ParseIntOrDefault(
                Environment.GetEnvironmentVariable(McpConstants.EnvHttpRateLimitPerMinute),
                120)
        };

    private static int ParseIntOrDefault(string? value, int fallback) =>
        int.TryParse(value, out var parsed) && parsed > 0 ? parsed : fallback;
}
