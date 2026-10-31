// FILE: Basic.Mcp.McpFileAuditSink
namespace Basic.Mcp;

/// <summary>Append-only file audit sink for MCP events.</summary>
public sealed class McpFileAuditSink : IMcpAuditSink
{
    private readonly Lock _lock = new();
    private readonly string _path;

    public McpFileAuditSink(string? path = null) =>
        _path = path ?? Path.Combine(Path.GetTempPath(), "gmbasic-mcp-audit.log");

    /// <summary>Path of the audit log file.</summary>
    public string LogPath => _path;

    /// <inheritdoc />
    public void Record(McpAuditEntry entry)
    {
        var line =
            $"{entry.Timestamp:O}\tevent={entry.EventType}\ttool={entry.ToolName ?? string.Empty}\t" +
            $"proc={entry.QualifiedProcedure ?? string.Empty}\tclient={entry.ClientId ?? string.Empty}\t" +
            $"ok={entry.Success}\tduration_ms={entry.DurationMs}\t{entry.Detail ?? string.Empty}";
        lock (_lock)
            File.AppendAllText(_path, line + Environment.NewLine);
    }
}
