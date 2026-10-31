// FILE: Basic.Databases.Replication.ReplicationTcpProtocol

using System.Net;
using System.Net.Sockets;
using System.Text;

namespace Basic.Databases.Replication;

/// <summary>
/// Newline-delimited UTF-8 control + data lines for replication over TCP.
/// </summary>
/// <remarks>
/// ponytail: no TLS, no length prefix — upgrade: TLS wrapper or framed binary protocol.
/// </remarks>
internal static class ReplicationTcpProtocol
{
    public const string Hello = "HELLO";
    public const string Ack = "ACK";
    public const string Ping = "PING";
    public const string Pong = "PONG";
    public const string Bye = "BYE";

    public static int AllocateLoopbackPort()
    {
        var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        try
        {
            return ((IPEndPoint)listener.LocalEndpoint).Port;
        }
        finally
        {
            listener.Stop();
        }
    }

    public static long LineByteCount(string line) =>
        Encoding.UTF8.GetByteCount(line) + 1;

    public static void WriteLine(Stream stream, string line)
    {
        ArgumentNullException.ThrowIfNull(stream);
        ArgumentNullException.ThrowIfNull(line);
        var bytes = Encoding.UTF8.GetBytes(line + "\n");
        stream.Write(bytes, 0, bytes.Length);
        stream.Flush();
    }

    public static string? ReadLine(Stream stream)
    {
        ArgumentNullException.ThrowIfNull(stream);
        using var reader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: false, bufferSize: 4096, leaveOpen: true);
        return reader.ReadLine();
    }

    public static bool TryParseHello(string line, out long byteOffset) =>
        TryParseOffsetLine(line, Hello, out byteOffset);

    public static bool TryParseAck(string line, out long byteOffset) =>
        TryParseOffsetLine(line, Ack, out byteOffset);

    public static bool IsPing(string line) =>
        string.Equals(line, Ping, StringComparison.Ordinal);

    public static bool IsBye(string line) =>
        string.Equals(line, Bye, StringComparison.Ordinal);

    public static bool IsControlLine(string line) =>
        line.StartsWith(Hello + '\t', StringComparison.Ordinal)
        || line.StartsWith(Ack + '\t', StringComparison.Ordinal)
        || string.Equals(line, Ping, StringComparison.Ordinal)
        || string.Equals(line, Pong, StringComparison.Ordinal)
        || string.Equals(line, Bye, StringComparison.Ordinal);

    private static bool TryParseOffsetLine(string line, string prefix, out long byteOffset)
    {
        byteOffset = 0;
        if (!line.StartsWith(prefix + '\t', StringComparison.Ordinal))
        {
            return false;
        }

        return long.TryParse(line.AsSpan(prefix.Length + 1), out byteOffset);
    }
}
