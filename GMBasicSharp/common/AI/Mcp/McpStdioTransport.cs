// FILE: Basic.Mcp.McpStdioTransport
using System.Text;

namespace Basic.Mcp;

/// <summary>Newline-delimited JSON-RPC over stdio (MCP 2024-11-05).</summary>
public sealed class McpStdioTransport : IMcpTransport
{
    private readonly TextReader _input;
    private readonly TextWriter _output;
    private readonly int _maxMessageBytes;
    private readonly SemaphoreSlim _writeLock = new(1, 1);

    public McpStdioTransport(TextReader? input = null, TextWriter? output = null, int maxMessageBytes = McpConstants.DefaultMaxMessageBytes)
    {
        _input = input ?? Console.In;
        _output = output ?? Console.Out;
        _maxMessageBytes = maxMessageBytes;
    }

    /// <inheritdoc />
    public async Task<string?> ReadMessageAsync(CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        var line = await _input.ReadLineAsync(cancellationToken).ConfigureAwait(false);
        if (line is null)
            return null;

        if (Encoding.UTF8.GetByteCount(line) > _maxMessageBytes)
            throw new InvalidOperationException($"MCP message exceeds max size ({_maxMessageBytes} bytes).");

        return line;
    }

    /// <inheritdoc />
    public async Task WriteMessageAsync(string json, CancellationToken cancellationToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(json);
        if (Encoding.UTF8.GetByteCount(json) > _maxMessageBytes)
            throw new InvalidOperationException($"MCP message exceeds max size ({_maxMessageBytes} bytes).");

        await _writeLock.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            await _output.WriteLineAsync(json.AsMemory(), cancellationToken).ConfigureAwait(false);
            await _output.FlushAsync(cancellationToken).ConfigureAwait(false);
        }
        finally
        {
            _writeLock.Release();
        }
    }

    /// <inheritdoc />
    public ValueTask DisposeAsync()
    {
        _writeLock.Dispose();
        return ValueTask.CompletedTask;
    }
}
