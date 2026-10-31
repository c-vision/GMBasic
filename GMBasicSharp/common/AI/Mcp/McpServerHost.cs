// FILE: Basic.Mcp.McpServerHost
namespace Basic.Mcp;

/// <summary>Runs the MCP server read/write loop over a transport.</summary>
public sealed class McpServerHost
{
    private readonly McpServer _server;
    private readonly IMcpTransport _transport;

    public McpServerHost(McpServer server, IMcpTransport transport)
    {
        _server = server ?? throw new ArgumentNullException(nameof(server));
        _transport = transport ?? throw new ArgumentNullException(nameof(transport));
    }

    /// <summary>Block until transport closes, processing JSON-RPC messages.</summary>
    public async Task RunAsync(CancellationToken cancellationToken = default)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            var line = await _transport.ReadMessageAsync(cancellationToken).ConfigureAwait(false);
            if (line is null)
                break;

            if (string.IsNullOrWhiteSpace(line))
                continue;

            var response = await _server.HandleRequestAsync(line, cancellationToken).ConfigureAwait(false);
            if (response is not null)
                await _transport.WriteMessageAsync(response, cancellationToken).ConfigureAwait(false);
        }
    }
}
