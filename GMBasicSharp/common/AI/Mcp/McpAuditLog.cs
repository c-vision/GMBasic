// FILE: Basic.Mcp.McpAuditLog
namespace Basic.Mcp;

/// <summary>Composite audit log for MCP tool invocations and auth events.</summary>
public sealed class McpAuditLog
{
    private readonly IMcpAuditSink[] _sinks;

    public McpAuditLog(params IMcpAuditSink[] sinks)
    {
        _sinks = sinks is { Length: > 0 } ? sinks : [new McpFileAuditSink()];
    }

    /// <summary>Primary file sink path when configured.</summary>
    public string? LogPath => _sinks.OfType<McpFileAuditSink>().FirstOrDefault()?.LogPath;

    /// <summary>Build default sinks: file append plus optional Ledger journal from env.</summary>
    public static McpAuditLog CreateDefault()
    {
        var sinks = new List<IMcpAuditSink> { new McpFileAuditSink() };
        foreach (var extra in McpRuntimeBridge.ExtraAuditSinks)
            sinks.Add(extra);
        return new McpAuditLog(sinks.ToArray());
    }

    /// <summary>Record a tool invocation.</summary>
    public void Record(string toolName, string qualifiedProcedure, bool success, string? detail = null,
        int durationMs = 0, string? clientId = null) =>
        Record(new McpAuditEntry(
            DateTimeOffset.UtcNow,
            "tool_call",
            toolName,
            qualifiedProcedure,
            clientId ?? McpRequestContext.CurrentClientId,
            success,
            durationMs,
            detail));

    /// <summary>Record an HTTP authentication failure.</summary>
    public void RecordAuthFailure(string clientId, string reason) =>
        Record(new McpAuditEntry(
            DateTimeOffset.UtcNow,
            "auth_failure",
            null,
            null,
            clientId,
            false,
            0,
            reason));

    /// <summary>Record a structured audit entry to all sinks.</summary>
    public void Record(McpAuditEntry entry)
    {
        foreach (var sink in _sinks)
            sink.Record(entry);
    }
}
