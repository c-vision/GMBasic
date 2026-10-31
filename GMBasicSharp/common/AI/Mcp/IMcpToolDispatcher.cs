// FILE: Basic.Mcp.IMcpToolDispatcher
namespace Basic.Mcp;

/// <summary>Invokes a registered BASIC procedure as an MCP tool.</summary>
public interface IMcpToolDispatcher
{
    /// <summary>Dispatch tool call with JSON arguments object.</summary>
    /// <param name="qualifiedProcedureName">BASIC qualified name (e.g. <c>MyModule.Add</c>).</param>
    /// <param name="argumentsJson">JSON object of argument values.</param>
    /// <param name="cancellationToken">Cancellation for long-running tools.</param>
  Task<McpToolCallResult> DispatchAsync(
        string qualifiedProcedureName,
        string argumentsJson,
        CancellationToken cancellationToken);
}

/// <summary>Result of a tool invocation for MCP <c>tools/call</c>.</summary>
public sealed record McpToolCallResult(bool IsError, string TextContent, string? StructuredJson = null);
