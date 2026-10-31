// FILE: Basic.Mcp.McpHttpSecurity
using System.Net;
using System.Security.Cryptography;

namespace Basic.Mcp;

/// <summary>API key validation and per-IP rate limiting for MCP HTTP.</summary>
public sealed class McpHttpSecurity
{
    private readonly string? _apiKey;
    private readonly int _requestsPerMinute;
    private readonly Lock _lock = new();
    private readonly Dictionary<string, WindowCounter> _windows = new(StringComparer.Ordinal);

    public McpHttpSecurity(McpHttpOptions options)
    {
        ArgumentNullException.ThrowIfNull(options);
        _apiKey = options.ApiKey;
        _requestsPerMinute = options.RequestsPerMinutePerIp;
    }

    /// <summary>Whether API key enforcement is enabled.</summary>
    public bool RequiresApiKey => !string.IsNullOrEmpty(_apiKey);

    /// <summary>Validate Authorization Bearer or X-API-Key header.</summary>
    public bool TryValidateApiKey(HttpListenerRequest request, out string? failureReason) =>
        TryValidateApiKeyHeaders(request.Headers["Authorization"], request.Headers["X-API-Key"], out failureReason);

    /// <summary>Validate API key headers without an HTTP request object (tests).</summary>
    public bool TryValidateApiKeyHeaders(string? authorizationHeader, string? xApiKeyHeader, out string? failureReason)
    {
        if (!RequiresApiKey)
        {
            failureReason = null;
            return true;
        }

        var provided = ExtractApiKey(authorizationHeader, xApiKeyHeader);
        if (string.IsNullOrEmpty(provided))
        {
            failureReason = "missing_api_key";
            return false;
        }

        if (!FixedTimeEquals(provided, _apiKey!))
        {
            failureReason = "invalid_api_key";
            return false;
        }

        failureReason = null;
        return true;
    }

    /// <summary>Check and increment per-IP request count.</summary>
    public bool TryConsumeRateLimit(string clientId, out string? failureReason)
    {
        if (_requestsPerMinute <= 0)
        {
            failureReason = null;
            return true;
        }

        var now = DateTimeOffset.UtcNow;
        lock (_lock)
        {
            if (!_windows.TryGetValue(clientId, out var window) || now - window.WindowStart >= TimeSpan.FromMinutes(1))
            {
                _windows[clientId] = new WindowCounter(now, 1);
                failureReason = null;
                return true;
            }

            if (window.Count >= _requestsPerMinute)
            {
                failureReason = "rate_limit_exceeded";
                return false;
            }

            _windows[clientId] = window with { Count = window.Count + 1 };
            failureReason = null;
            return true;
        }
    }

    /// <summary>Extract client id from remote endpoint.</summary>
    public static string GetClientId(HttpListenerRequest request) =>
        request.RemoteEndPoint?.Address?.ToString() ?? "unknown";

    private static string? ExtractApiKey(HttpListenerRequest request) =>
        ExtractApiKey(request.Headers["Authorization"], request.Headers["X-API-Key"]);

    private static string? ExtractApiKey(string? authorizationHeader, string? xApiKeyHeader)
    {
        if (!string.IsNullOrWhiteSpace(authorizationHeader) &&
            authorizationHeader.StartsWith("Bearer ", StringComparison.OrdinalIgnoreCase))
        {
            return authorizationHeader["Bearer ".Length..].Trim();
        }

        return string.IsNullOrWhiteSpace(xApiKeyHeader) ? null : xApiKeyHeader.Trim();
    }

    private static bool FixedTimeEquals(string left, string right)
    {
        var leftBytes = System.Text.Encoding.UTF8.GetBytes(left);
        var rightBytes = System.Text.Encoding.UTF8.GetBytes(right);
        return leftBytes.Length == rightBytes.Length &&
               CryptographicOperations.FixedTimeEquals(leftBytes, rightBytes);
    }

    private sealed record WindowCounter(DateTimeOffset WindowStart, int Count);
}
