// FILE: Mcp.Tests.CapturingAuditSink
using Basic.Mcp;

namespace AI.Tests;

internal sealed class CapturingAuditSink : IMcpAuditSink
{
    public List<McpAuditEntry> Entries { get; } = [];

    public void Record(McpAuditEntry entry) => Entries.Add(entry);
}
