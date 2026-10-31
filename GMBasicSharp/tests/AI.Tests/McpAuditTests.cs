// FILE: Mcp.Tests.McpAuditTests
using Basic.Mcp;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class McpAuditTests
{
    [Fact]
    public void FileAuditSink_WritesEntry()
    {
        var path = Path.Combine(Path.GetTempPath(), $"mcp-audit-{Guid.NewGuid():N}.log");
        var sink = new McpFileAuditSink(path);
        sink.Record(new McpAuditEntry(
            DateTimeOffset.UtcNow,
            "tool_call",
            "add_numbers",
            "AddNumbers",
            "127.0.0.1",
            true,
            12,
            "42"));

        File.Exists(path).Should().BeTrue();
        var text = File.ReadAllText(path);
        text.Should().Contain("event=tool_call");
        text.Should().Contain("add_numbers");
    }
}
