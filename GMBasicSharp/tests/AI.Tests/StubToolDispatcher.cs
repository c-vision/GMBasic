// FILE: Mcp.Tests.StubToolDispatcher
using Basic.Mcp;

namespace AI.Tests;

internal sealed class StubToolDispatcher(string responseText = "42") : IMcpToolDispatcher
{
    public string? LastProcedure { get; private set; }

    public string? LastArguments { get; private set; }

    public int Calls { get; private set; }

    public Task<McpToolCallResult> DispatchAsync(string qualifiedProcedureName, string argumentsJson,
        CancellationToken cancellationToken)
    {
        Calls++;
        LastProcedure = qualifiedProcedureName;
        LastArguments = argumentsJson;
        return Task.FromResult(new McpToolCallResult(false, responseText));
    }
}
