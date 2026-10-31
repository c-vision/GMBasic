// FILE: Basic.Mcp.McpRuntimeBridge
namespace Basic.Mcp;

/// <summary>Bridge from MCP server to the active BASIC Executor tool dispatcher.</summary>
public static class McpRuntimeBridge
{
    /// <summary>Dispatcher set by Executor on startup; null in headless tests until wired.</summary>
    public static IMcpToolDispatcher? Dispatcher { get; set; }

    /// <summary>Additional audit sinks registered by the runtime (tests, custom Ledger wiring).</summary>
    public static IReadOnlyList<IMcpAuditSink> ExtraAuditSinks => _extraAuditSinks;

    private static readonly List<IMcpAuditSink> _extraAuditSinks = [];

    /// <summary>Register an extra audit sink for subsequent MCP servers.</summary>
    public static void RegisterAuditSink(IMcpAuditSink sink)
    {
        ArgumentNullException.ThrowIfNull(sink);
        lock (_extraAuditSinks)
            _extraAuditSinks.Add(sink);
    }

    /// <summary>Clear bridge (tests).</summary>
    public static void ResetForTests()
    {
        Dispatcher = null;
        lock (_extraAuditSinks)
            _extraAuditSinks.Clear();
    }
}
