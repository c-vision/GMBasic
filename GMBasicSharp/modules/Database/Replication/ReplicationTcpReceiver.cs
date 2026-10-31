// FILE: Basic.Databases.Replication.ReplicationTcpReceiver

using System.Net;
using System.Net.Sockets;
using System.Text;

namespace Basic.Databases.Replication;

/// <summary>
/// Secondary-side TCP receiver: accepts one connection, applies replication lines via callback.
/// </summary>
internal static class ReplicationTcpReceiver
{
    public static int ReceiveOnce(
        int port,
        Func<ReplicationApplyState, string, bool> applyLine,
        string? checkpointPath = null,
        TimeSpan? acceptTimeout = null)
    {
        ArgumentNullException.ThrowIfNull(applyLine);
        if (port is <= 0 or > 65535)
        {
            throw new ArgumentOutOfRangeException(nameof(port), port, "Port must be 1-65535.");
        }

        acceptTimeout ??= TimeSpan.FromSeconds(30);
        using var listener = new TcpListener(IPAddress.Loopback, port);
        listener.Server.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
        listener.Start();
        try
        {
            using var acceptCts = new CancellationTokenSource(acceptTimeout.Value);
            using var client = listener.AcceptTcpClientAsync(acceptCts.Token).GetAwaiter().GetResult();
            using var stream = client.GetStream();
            return ReceiveSession(stream, applyLine, checkpointPath);
        }
        finally
        {
            listener.Stop();
        }
    }

    internal static int ReceiveSession(
        Stream stream,
        Func<ReplicationApplyState, string, bool> applyLine,
        string? checkpointPath)
    {
        ArgumentNullException.ThrowIfNull(stream);
        ArgumentNullException.ThrowIfNull(applyLine);

        using var lineReader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: false, bufferSize: 4096, leaveOpen: true);
        var helloLine = lineReader.ReadLine()
                        ?? throw new InvalidDataException("Shipper closed before HELLO.");
        if (!ReplicationTcpProtocol.TryParseHello(helloLine, out _))
        {
            throw new InvalidDataException($"Expected HELLO, got: {helloLine}");
        }

        checkpointPath ??= string.Empty;
        var (byteOffset, state) = string.IsNullOrWhiteSpace(checkpointPath)
            ? (0L, new ReplicationApplyState())
            : ReplicationApplyState.LoadCheckpoint(checkpointPath);

        ReplicationTcpProtocol.WriteLine(stream, $"{ReplicationTcpProtocol.Ack}\t{byteOffset}");

        var applied = 0;
        string? line;
        while ((line = lineReader.ReadLine()) is not null)
        {
            if (ReplicationTcpProtocol.IsBye(line))
            {
                break;
            }

            if (ReplicationTcpProtocol.IsPing(line))
            {
                ReplicationTcpProtocol.WriteLine(stream, ReplicationTcpProtocol.Pong);
                continue;
            }

            if (ReplicationTcpProtocol.IsControlLine(line))
            {
                continue;
            }

            if (string.IsNullOrWhiteSpace(line))
            {
                continue;
            }

            if (applyLine(state, line))
            {
                applied++;
            }

            byteOffset += ReplicationTcpProtocol.LineByteCount(line);
            if (!string.IsNullOrWhiteSpace(checkpointPath))
            {
                state.SaveCheckpoint(checkpointPath, byteOffset);
            }
        }

        return applied;
    }
}
