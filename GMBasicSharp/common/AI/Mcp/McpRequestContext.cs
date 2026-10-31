// FILE: Basic.Mcp.McpRequestContext
namespace Basic.Mcp;

/// <summary>Per-request MCP metadata (client id for audit).</summary>
public static class McpRequestContext
{
    private static readonly AsyncLocal<string?> ClientId = new();

    /// <summary>Current client identifier (HTTP remote IP or stdio label).</summary>
    public static string? CurrentClientId => ClientId.Value;

    /// <summary>Push client id for the duration of a scope.</summary>
    public static IDisposable PushClient(string? clientId) => new Scope(clientId);

    private sealed class Scope : IDisposable
    {
        private readonly string? _previous;

        public Scope(string? clientId)
        {
            _previous = ClientId.Value;
            ClientId.Value = clientId;
        }

        public void Dispose() => ClientId.Value = _previous;
    }
}
