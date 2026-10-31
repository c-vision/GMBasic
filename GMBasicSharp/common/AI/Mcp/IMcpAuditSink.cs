// FILE: Basic.Mcp.IMcpAuditSink
namespace Basic.Mcp;

/// <summary>Structured MCP audit event.</summary>
/// <param name="Timestamp">UTC timestamp.</param>
/// <param name="EventType">Event kind (tool_call, auth_failure).</param>
/// <param name="ToolName">Tool name when applicable.</param>
/// <param name="QualifiedProcedure">BASIC procedure when applicable.</param>
/// <param name="ClientId">Remote client or agent identifier.</param>
/// <param name="Success">Whether the operation succeeded.</param>
/// <param name="DurationMs">Elapsed milliseconds (0 for auth failures).</param>
/// <param name="Detail">Optional detail or error message.</param>
public sealed record McpAuditEntry(
    DateTimeOffset Timestamp,
    string EventType,
    string? ToolName,
    string? QualifiedProcedure,
    string? ClientId,
    bool Success,
    int DurationMs,
    string? Detail);

/// <summary>Sink for MCP audit records (file, Ledger journal, tests).</summary>
public interface IMcpAuditSink
{
    /// <summary>Append one audit record.</summary>
    void Record(McpAuditEntry entry);
}
