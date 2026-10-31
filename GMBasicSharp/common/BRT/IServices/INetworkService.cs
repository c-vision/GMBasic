// FILE: Basic.Runtime.IServices.INetworkService.cs

namespace Basic.Runtime;

/// <summary>Servizio di rete per il BASIC Runtime (TCP/UDP/SSL).</summary>
public interface INetworkService
{
    // === TCP ===
    long TcpConnect(string host, int port);
    void TcpClose(long handle);
    void TcpSend(long handle, byte[] data);
    byte[]? TcpReceive(long handle, int count);
    byte[]? TcpPeek(long handle, int count);
    long TcpListen(string host, int port);
    long TcpAccept(long listener);
    ValueTask<long> TcpConnectAsync(string host, int port, CancellationToken cancellationToken);
    ValueTask TcpSendAsync(long handle, byte[] data, CancellationToken cancellationToken);
    ValueTask<byte[]?> TcpReceiveAsync(long handle, int count, CancellationToken cancellationToken);
    ValueTask<long> TcpAcceptAsync(long listener, CancellationToken cancellationToken);
    ValueTask<byte[]?> TcpPeekAsync(long handle, int count, CancellationToken cancellationToken);

    // === UDP ===
    long UdpOpen(int port);
    void UdpClose(long handle);
    void UdpSend(long handle, byte[] data, string host, int port);
    (byte[] Data, string Host, int Port)? UdpReceive(long handle);
    ValueTask UdpSendAsync(long handle, byte[] data, string host, int port, CancellationToken cancellationToken);
    ValueTask<(byte[] Data, string Host, int Port)?> UdpReceiveAsync(long handle, CancellationToken cancellationToken);

    // === SSL ===
    long SslConnect(string host, int port, string? certPath);
    long SslListen(int port, string? certPath, string? certPassword);
    void SslClose(long handle);
    void SslSend(long handle, byte[] data);
    byte[]? SslReceive(long handle, int count);
    ValueTask<long> SslConnectAsync(string host, int port, string? certPath, CancellationToken cancellationToken);

    // === Socket Options ===
    void TcpSetOption(long handle, string option, string value);
    void TcpShutdown(long handle, string mode);

    // === Handle resolution (REMOTE/REMOTEPORT/LOCALPORT/RECV) ===
    /// <summary>Risolve un handle numerico all'oggetto NetworkHandle, o null se sconosciuto.</summary>
    CommonLib.Network.NetworkHandle? GetNetworkHandle(long handle);
}
