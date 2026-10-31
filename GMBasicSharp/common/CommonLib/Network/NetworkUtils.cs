// FILE: CommonLib.Network.NetworkUtils.cs

using System.Net;

/* 
 * NetworkUtils.cs: utility statiche per ispezionare lo stato di handle di rete senza eseguire I/O
 * 
 * FUNZIONI:
 * - IsConnected(handle): verifica se un handle di rete è attivo e connesso
 * - GetRemoteAddress(handle): restituisce l'indirizzo IP remoto
 * - GetRemotePort(handle): restituisce la porta remota
 * - GetLocalPort(handle): restituisce la porta locale
 */

namespace CommonLib.Network;

/// <summary>Utility statiche per ispezionare lo stato di handle di rete senza eseguire I/O.</summary>
public static class NetworkUtils
{
    /// <summary>Verifica se un handle di rete è attivo e connesso.</summary>
    /// <param name="handle">Handle di rete da verificare.</param>
    /// <returns>True se l'handle è attivo e connesso.</returns>
    public static bool IsConnected(NetworkHandle? handle) => handle switch
    {
        TcpListenerHandle th => th.Listener.Server.IsBound,
        AsyncTcpListenerHandle ath => ath.Listener.Server.IsBound,
        TcpClientHandle ch => ch.ClientSocket.Connected,
        AsyncTcpClientHandle ach => ach.Client.Connected,
        UdpHandle uh => uh.UdpSocket.IsBound,
        _ => false
    };

    /// <summary>Restituisce l'indirizzo IP remoto di un handle TCP, o stringa vuota.</summary>
    /// <param name="handle">Handle TCP da cui leggere l'indirizzo.</param>
    /// <returns>Indirizzo IP remoto come stringa, o stringa vuota se non disponibile.</returns>
    public static string GetRemoteAddress(NetworkHandle? handle)
    {
        var remoteEndPoint = handle switch
        {
            TcpClientHandle ch => ch.ClientSocket.RemoteEndPoint,
            AsyncTcpClientHandle ach => ach.RemoteEndPoint,
            _ => null
        };
        return (remoteEndPoint as IPEndPoint)?.Address.ToString() ?? string.Empty;
    }

    /// <summary>Restituisce la porta remota di un handle TCP, o 0.</summary>
    /// <param name="handle">Handle TCP da cui leggere la porta.</param>
    /// <returns>Numero di porta remota, o 0 se non disponibile.</returns>
    public static long GetRemotePort(NetworkHandle? handle)
    {
        var remoteEndPoint = handle switch
        {
            TcpClientHandle ch => ch.ClientSocket.RemoteEndPoint,
            AsyncTcpClientHandle ach => ach.RemoteEndPoint,
            _ => null
        };
        return (remoteEndPoint as IPEndPoint)?.Port ?? 0;
    }

    /// <summary>Restituisce la porta locale di un handle TCP/UDP, o 0.</summary>
    /// <param name="handle">Handle di rete da cui leggere la porta locale.</param>
    /// <returns>Numero di porta locale, o 0 se non disponibile.</returns>
    public static long GetLocalPort(NetworkHandle? handle)
    {
        var localEndPoint = handle switch
        {
            TcpListenerHandle th => th.Listener.LocalEndpoint,
            AsyncTcpListenerHandle ath => ath.Listener.LocalEndpoint,
            TcpClientHandle ch => ch.ClientSocket.LocalEndPoint,
            AsyncTcpClientHandle ach => ach.LocalEndPoint,
            UdpHandle uh => uh.UdpSocket.LocalEndPoint,
            _ => null
        };
        return (localEndPoint as IPEndPoint)?.Port ?? 0;
    }
    private static int _lastNetError = 0;
    public static int NetErr { get => _lastNetError; set => _lastNetError = value; }
}
