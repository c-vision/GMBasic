// FILE: Basic.Mcp.IMcpTransport
namespace Basic.Mcp;

/// <summary>Transport for newline-delimited JSON-RPC MCP messages.</summary>
public interface IMcpTransport : IAsyncDisposable
{
    /// <summary>Read the next JSON-RPC message line.</summary>
    Task<string?> ReadMessageAsync(CancellationToken cancellationToken);

    /// <summary>Write a JSON-RPC message line.</summary>
    Task WriteMessageAsync(string json, CancellationToken cancellationToken);
}
