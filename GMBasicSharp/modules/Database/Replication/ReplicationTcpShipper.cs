// FILE: Basic.Databases.Replication.ReplicationTcpShipper

using System.Net.Sockets;
using System.Text;

namespace Basic.Databases.Replication;

/// <summary>
/// Primary-side TCP shipper: tails a <c>.repl</c> log and streams new lines to a receiver.
/// </summary>
internal static class ReplicationTcpShipper
{
    public static int ShipOnce(
        string replLogPath,
        string host,
        int port,
        string? shipCheckpointPath = null,
        TimeSpan? connectTimeout = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
        ArgumentException.ThrowIfNullOrWhiteSpace(host);
        if (port is <= 0 or > 65535)
        {
            throw new ArgumentOutOfRangeException(nameof(port), port, "Port must be 1-65535.");
        }

        if (!File.Exists(replLogPath))
        {
            throw new FileNotFoundException("Replication log not found.", replLogPath);
        }

        shipCheckpointPath ??= ReplicationShipState.DefaultCheckpointPath(replLogPath);
        connectTimeout ??= TimeSpan.FromSeconds(10);

        using var client = ConnectWithRetry(host, port, connectTimeout.Value);
        using var stream = client.GetStream();
        using var ackReader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: false, bufferSize: 4096, leaveOpen: true);
        var shipOffset = ReplicationShipState.LoadOffset(shipCheckpointPath);
        ReplicationTcpProtocol.WriteLine(stream, $"{ReplicationTcpProtocol.Hello}\t{shipOffset}");

        var ackLine = ackReader.ReadLine()
                      ?? throw new InvalidDataException("Receiver closed before ACK.");
        if (!ReplicationTcpProtocol.TryParseAck(ackLine, out var receiverOffset))
        {
            throw new InvalidDataException($"Expected ACK, got: {ackLine}");
        }

        var startOffset = ReplicationLogEncoding.AdjustFileOffset(replLogPath, receiverOffset);
        var shipped = 0;
        var byteOffset = startOffset;
        using var fs = new FileStream(replLogPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        fs.Seek(startOffset, SeekOrigin.Begin);
        using var logReader = new StreamReader(fs, Encoding.UTF8, detectEncodingFromByteOrderMarks: false, bufferSize: 4096, leaveOpen: true);
        string? line;
        while ((line = logReader.ReadLine()) is not null)
        {
            if (string.IsNullOrWhiteSpace(line))
            {
                continue;
            }

            ReplicationTcpProtocol.WriteLine(stream, line);
            shipped++;
            byteOffset += ReplicationTcpProtocol.LineByteCount(line);
            ReplicationShipState.SaveOffset(shipCheckpointPath, byteOffset);
        }

        ReplicationTcpProtocol.WriteLine(stream, ReplicationTcpProtocol.Bye);
        stream.Flush();
        return shipped;
    }

    // ponytail: tests start receiver via Task.Run — brief retry avoids connection refused races.
    private static TcpClient ConnectWithRetry(string host, int port, TimeSpan timeout)
    {
        var deadline = DateTime.UtcNow + timeout;
        Exception? lastError = null;
        while (DateTime.UtcNow < deadline)
        {
            var client = new TcpClient();
            try
            {
                var remaining = deadline - DateTime.UtcNow;
                var slice = remaining < TimeSpan.FromMilliseconds(200) ? remaining : TimeSpan.FromMilliseconds(200);
                if (!client.ConnectAsync(host, port).Wait(slice))
                {
                    client.Dispose();
                    Thread.Sleep(25);
                    continue;
                }

                return client;
            }
            catch (Exception ex) when (IsTransientConnectFailure(ex))
            {
                lastError = ex;
                client.Dispose();
                Thread.Sleep(25);
            }
        }

        throw new TimeoutException($"Timed out connecting to {host}:{port}.", lastError);
    }

    private static bool IsTransientConnectFailure(Exception ex) =>
        ex switch
        {
            SocketException { SocketErrorCode: SocketError.ConnectionRefused } => true,
            AggregateException { InnerException: SocketException { SocketErrorCode: SocketError.ConnectionRefused } } => true,
            _ => false,
        };
}
