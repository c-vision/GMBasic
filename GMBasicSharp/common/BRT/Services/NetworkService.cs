// FILE: Basic.Runtime.Services.NetworkService.cs

using System.Net;
using System.Net.Sockets;
using System.Collections.Concurrent;

namespace Basic.Runtime.Services;

public sealed class NetworkService : INetworkService
{
    private readonly ConcurrentDictionary<long, TcpClient> _tcpClients = new();
    private readonly ConcurrentDictionary<long, TcpListener> _tcpListeners = new();
    private long _nextHandle;

    /// <summary>Handle numerico → oggetto NetworkHandle, per REMOTE/REMOTEPORT/LOCALPORT/RECV.</summary>
    private readonly ConcurrentDictionary<long, CommonLib.Network.NetworkHandle> _handles = new();

    private long NextHandle() => Interlocked.Increment(ref _nextHandle);

    public long TcpConnect(string host, int port)
    {
        // Prova TUTTI gli indirizzi risolti (IPv6 e IPv4): su macOS "localhost" risolve
        // in ::1, ma i listener TCP OPEN ascoltano su IPAddress.Any (IPv4) — una
        // connessione diretta a ::1 fallisce. Fallback su 127.0.0.1 se il primo fallisce.
        var client = new TcpClient();
        try
        {
            var addresses = Dns.GetHostAddresses(host);
            if (addresses.Length > 1)
            {
                var lastEx = (Exception?)null;
                foreach (var addr in addresses)
                {
                    try
                    {
                        client = new TcpClient();
                        client.Connect(addr, port);
                        var h = NextHandle();
                        _tcpClients[h] = client;
                        _handles[h] = new CommonLib.Network.TcpClientHandle(client.Client, CommonLib.Network.NetworkEngine.Standard);
                        return h;
                    }
                    catch (Exception ex) { lastEx = ex; }
                }
                if (lastEx != null) throw lastEx;
            }
            client.Connect(host, port);
        }
        catch
        {
            // Ultimo tentativo: 127.0.0.1 esplicito (DNS di "localhost" può dare solo ::1,
            // ma i listener BASIC ascoltano su IPAddress.Any = IPv4). RETRY per un breve
            // lasso se il server non è ancora in ascolto (race di avvio sotto GOMAXPROCS=1:
            // il goroutine server può non aver completato TCP OPEN+ACCEPT quando il client
            // si connette → connection refused immediato → flake NetTcpAdvanced).
            var deadline = Environment.TickCount64 + 2000;
            while (true)
            {
                try
                {
                    client = new TcpClient();
                    client.Connect("127.0.0.1", port);
                    break;
                }
                catch
                {
                    if (Environment.TickCount64 >= deadline) return -1;
                    Thread.Sleep(25);
                }
            }
        }
        var hh = NextHandle();
        _tcpClients[hh] = client;
        _handles[hh] = new CommonLib.Network.TcpClientHandle(client.Client, CommonLib.Network.NetworkEngine.Standard);
        return hh;
    }

    public async ValueTask<long> TcpConnectAsync(string host, int port, CancellationToken cancellationToken)
    {
        // Come TcpConnect (sync): prova TUTTI gli indirizzi risolti (IPv6 e IPv4) — su macOS
        // "localhost" risolve in ::1, ma i listener BASIC TCP OPEN ascoltano su IPAddress.Any
        // (IPv4): la ConnectAsync diretta a ::1 fallisce → flake NetTcpAdvanced. Fallback su
        // 127.0.0.1 esplicito.
        TcpClient? client = null;
        Exception? lastEx = null;
        try
        {
            var addresses = Dns.GetHostAddresses(host);
            foreach (var addr in addresses)
            {
                cancellationToken.ThrowIfCancellationRequested();
                client?.Dispose();
                client = new TcpClient();
                try
                {
                    await client.ConnectAsync(addr, port, cancellationToken).ConfigureAwait(false);
                    var handle = NextHandle();
                    _tcpClients[handle] = client;
                    _handles[handle] = new CommonLib.Network.TcpClientHandle(client.Client, CommonLib.Network.NetworkEngine.Standard);
                    return handle;
                }
                catch (Exception ex) { lastEx = ex; }
            }
        }
        catch (OperationCanceledException)
        {
            client?.Dispose();
            throw;
        }
        catch (Exception ex) { lastEx = ex; }
        // Fallback: 127.0.0.1 esplicito (DNS di "localhost" può dare solo ::1, i listener
        // BASIC ascoltano su IPv4). Bounded retry per la race di avvio del server.
        var deadline = Environment.TickCount64 + 2000;
        while (true)
        {
            cancellationToken.ThrowIfCancellationRequested();
            client?.Dispose();
            client = new TcpClient();
            try
            {
                await client.ConnectAsync("127.0.0.1", port, cancellationToken).ConfigureAwait(false);
                var handle = NextHandle();
                _tcpClients[handle] = client;
                _handles[handle] = new CommonLib.Network.TcpClientHandle(client.Client, CommonLib.Network.NetworkEngine.Standard);
                return handle;
            }
            catch (Exception ex)
            {
                lastEx = ex;
                if (Environment.TickCount64 >= deadline) break;
                await Task.Delay(25, cancellationToken).ConfigureAwait(false);
            }
        }
        client?.Dispose();
        if (lastEx is OperationCanceledException) throw lastEx;
        return -1;
    }

    public void TcpClose(long handle)
    {
        if (_tcpClients.TryRemove(handle, out var c)) c.Close();
        if (_tcpListeners.TryRemove(handle, out var l)) l.Stop();
        _handles.TryRemove(handle, out _);
    }

    public CommonLib.Network.NetworkHandle? GetNetworkHandle(long handle)
        => _handles.TryGetValue(handle, out var nh) ? nh : null;

    public void TcpSend(long handle, byte[] data)
    {
        // SSL handle (SSL CONNECT/ACCEPT) viaggia sullo SslStream.
        if (_sslClients.TryGetValue(handle, out var sc))
        {
            try { sc.Stream.Write(data, 0, data.Length); sc.Stream.Flush(); } catch { }
            return;
        }
        if (_tcpClients.TryGetValue(handle, out var c))
        {
            var s = c.GetStream();
            s.Write(data, 0, data.Length);
            s.Flush();
        }
    }

    public async ValueTask TcpSendAsync(long handle, byte[] data, CancellationToken cancellationToken)
    {
        if (_sslClients.TryGetValue(handle, out var secure))
        {
            await secure.Stream.WriteAsync(data, cancellationToken).ConfigureAwait(false);
            await secure.Stream.FlushAsync(cancellationToken).ConfigureAwait(false);
            return;
        }
        if (!_tcpClients.TryGetValue(handle, out var client)) return;
        var stream = client.GetStream();
        await stream.WriteAsync(data, cancellationToken).ConfigureAwait(false);
        await stream.FlushAsync(cancellationToken).ConfigureAwait(false);
    }

    public byte[]? TcpReceive(long handle, int count)
    {
        if (_sslClients.TryGetValue(handle, out var sc))
        {
            try
            {
                var buf = new byte[count];
                var r = sc.Stream.Read(buf, 0, count);
                return r > 0 ? buf.AsSpan(0, r).ToArray() : null;
            }
            catch { return null; }
        }
        if (!_tcpClients.TryGetValue(handle, out var c)) return null;
        var buffer = new byte[count];
        var read = c.GetStream().Read(buffer, 0, count);
        return buffer.AsSpan(0, read).ToArray();
    }

    /// <summary>Sbircia fino a <paramref name="count"/> byte dal socket TCP senza rimuoverli
    /// (parità C99Native gmb_net_tcp_peek / recv MSG_PEEK). Handle invalido → null.</summary>
    public byte[]? TcpPeek(long handle, int count)
    {
        if (!_tcpClients.TryGetValue(handle, out var c)) return null;
        try
        {
            var buffer = new byte[count];
            // Bugs/(open) - NET-TcpPeek-Bloccante-Hang-Test-Goroutine.md: Receive sincrono
            // SENZA timeout sul thread chiamante — dal task PCode quel thread è un worker
            // GPM, e con GMBASIC_GOMAXPROCS=1 l'unico worker restava bloccato qui per
            // sempre (nessun peer che invia, nessun disconnect) → WaitForAllTasks non
            // ritornava mai, hang permanente (osservato: run vstest orfano 30h). Fix
            // minimo (non il park-based completo, che richiede spostare NET.PEEK dal
            // dispatch BuiltInFunction sincrono al dispatch NetSubOpCode/ParkAsync già
            // usato da TCP_RECV/TCP_ACCEPT — riprogettazione più ampia, fuori scope):
            // stesso "tetto di sicurezza" già applicato a UdpReceive (ReceiveTimeout=10000,
            // vedi sopra) — converte l'hang infinito in un fallimento limitato e
            // recuperabile (SocketException su timeout → catch sotto → null), non un vero
            // park cooperativo (il worker resta comunque occupato fino a 10s), ma elimina
            // il sintomo osservato (hang PERMANENTE).
            c.Client.ReceiveTimeout = 10000;
            var read = c.Client.Receive(buffer, 0, count, System.Net.Sockets.SocketFlags.Peek);
            return read > 0 ? buffer.AsSpan(0, read).ToArray() : null;
        }
        catch { return null; }
    }

    public async ValueTask<byte[]?> TcpReceiveAsync(long handle, int count, CancellationToken cancellationToken)
    {
        System.IO.Stream? stream = null;
        if (_sslClients.TryGetValue(handle, out var secure)) stream = secure.Stream;
        else if (_tcpClients.TryGetValue(handle, out var client)) stream = client.GetStream();
        if (stream == null) return null;

        var buffer = new byte[Math.Max(0, count)];
        var read = await stream.ReadAsync(buffer, cancellationToken).ConfigureAwait(false);
        return read > 0 ? buffer.AsSpan(0, read).ToArray() : null;
    }

    /// <summary>Fix park-based per Bugs/(solved) - NET-TcpPeek-Bloccante-Hang-Test-Goroutine.md:
    /// controparte async di <see cref="TcpPeek"/>, usata dal backend PCode via ParkAsync
    /// (NetSubOpCode.TCP_PEEK) invece del dispatch BuiltInFunction sincrono. Socket.ReceiveAsync
    /// con SocketFlags.Peek è I/O asincrono reale (nessun thread dedicato bloccato in attesa,
    /// a differenza di un ipotetico Task.Run su TcpPeek sincrono): il worker GPM chiamante non
    /// resta occupato per la durata dell'attesa, qualunque essa sia — nessun timeout necessario
    /// qui (a differenza del "tetto di sicurezza" su TcpPeek sincrono, ancora usato dal backend
    /// IL che non passa da questo park). Solo TCP semplice (non SSL), stessa limitazione di
    /// TcpPeek sincrono: _sslClients non espone un vero peek senza consumare byte decifrati.</summary>
    public async ValueTask<byte[]?> TcpPeekAsync(long handle, int count, CancellationToken cancellationToken)
    {
        if (!_tcpClients.TryGetValue(handle, out var c)) return null;
        var buffer = new byte[Math.Max(0, count)];
        try
        {
            var read = await c.Client.ReceiveAsync(buffer, System.Net.Sockets.SocketFlags.Peek, cancellationToken).ConfigureAwait(false);
            return read > 0 ? buffer.AsSpan(0, read).ToArray() : null;
        }
        catch (OperationCanceledException) { throw; }
        catch { return null; }
    }

    public long TcpListen(string host, int port)
    {
        var listener = new TcpListener(IPAddress.Any, port);
        listener.Start();
        var h = NextHandle();
        _tcpListeners[h] = listener;
        return h;
    }

    public long TcpAccept(long listener)
    {
        // TCP ACCEPT su un listener SSL (SSL OPEN ... poi TCP ACCEPT): handshake TLS completo.
        if (_sslListeners.ContainsKey(listener))
            return SslAccept(listener);
        if (!_tcpListeners.TryGetValue(listener, out var l)) return -1;
        var client = l.AcceptTcpClient();
        var h = NextHandle();
        _tcpClients[h] = client;
        _handles[h] = new CommonLib.Network.TcpClientHandle(client.Client, CommonLib.Network.NetworkEngine.Standard);
        return h;
    }

    public async ValueTask<long> TcpAcceptAsync(long listener, CancellationToken cancellationToken)
    {
        if (_sslListeners.ContainsKey(listener))
        {
            if (!_sslListeners.TryGetValue(listener, out var sslListener) ||
                !_sslCerts.TryGetValue(listener, out var certificate)) return -1;
            var sslClient = await sslListener.AcceptTcpClientAsync(cancellationToken).ConfigureAwait(false);
            var ssl = new System.Net.Security.SslStream(sslClient.GetStream(), false);
            try
            {
                await ssl.AuthenticateAsServerAsync(new System.Net.Security.SslServerAuthenticationOptions
                {
                    ServerCertificate = certificate
                }, cancellationToken).ConfigureAwait(false);
            }
            catch
            {
                ssl.Dispose();
                sslClient.Dispose();
                throw;
            }
            var sslHandle = NextHandle();
            _sslClients[sslHandle] = (sslClient, ssl);
            return sslHandle;
        }

        if (!_tcpListeners.TryGetValue(listener, out var tcpListener)) return -1;
        var client = await tcpListener.AcceptTcpClientAsync(cancellationToken).ConfigureAwait(false);
        var handle = NextHandle();
        _tcpClients[handle] = client;
        _handles[handle] = new CommonLib.Network.TcpClientHandle(client.Client, CommonLib.Network.NetworkEngine.Standard);
        return handle;
    }

    private readonly ConcurrentDictionary<long, CommonLib.Network.UdpHandle> _udpClients = new();
    private readonly ConcurrentDictionary<long, (TcpClient Client, System.IO.Stream Stream)> _sslClients = new();
    private readonly ConcurrentDictionary<long, TcpListener> _sslListeners = new();
    private readonly ConcurrentDictionary<long, System.Security.Cryptography.X509Certificates.X509Certificate2> _sslCerts = new();

    public long UdpOpen(int port)
    {
        try
        {
            var udp = new CommonLib.Network.UdpHandle(port, CommonLib.Network.NetworkEngine.Standard);
            var h = NextHandle();
            _udpClients[h] = udp;
            _handles[h] = udp;
            return h;
        }
        catch { return -1; }
    }

    public void UdpClose(long handle)
    {
        if (_udpClients.TryRemove(handle, out var u)) { try { u.Dispose(); } catch { } }
        _handles.TryRemove(handle, out _);
    }

    public void UdpSend(long handle, byte[] data, string host, int port)
    {
        if (!_udpClients.TryGetValue(handle, out var u)) return;
        try
        {
            var addresses = Dns.GetHostAddresses(host);
            if (addresses.Length == 0) return;
            u.UdpSocket.SendTo(data, SocketFlags.None, new IPEndPoint(addresses[0], port));
        }
        catch { /* best effort — UDP non ha handshake */ }
    }

    public async ValueTask UdpSendAsync(long handle, byte[] data, string host, int port, CancellationToken cancellationToken)
    {
        if (!_udpClients.TryGetValue(handle, out var udp)) return;
        var addresses = await Dns.GetHostAddressesAsync(host, cancellationToken).ConfigureAwait(false);
        if (addresses.Length == 0) return;
        await udp.UdpSocket.SendToAsync(data, SocketFlags.None, new IPEndPoint(addresses[0], port), cancellationToken)
            .ConfigureAwait(false);
    }

    public (byte[] Data, string Host, int Port)? UdpReceive(long handle)
    {
        if (!_udpClients.TryGetValue(handle, out var u)) return null;
        try
        {
            var buffer = new byte[65535];
            EndPoint? remote = new IPEndPoint(IPAddress.Any, 0);
            u.UdpSocket.ReceiveTimeout = 10000; // 10s max — il VM passa il timeout BASIC, qui un tetto di sicurezza
            var read = u.UdpSocket.ReceiveFrom(buffer, SocketFlags.None, ref remote);
            if (read <= 0 || remote is not IPEndPoint rip) return null;
            return (buffer.AsSpan(0, read).ToArray(), rip.Address.ToString(), rip.Port);
        }
        catch { return null; }
    }

    public async ValueTask<(byte[] Data, string Host, int Port)?> UdpReceiveAsync(long handle, CancellationToken cancellationToken)
    {
        if (!_udpClients.TryGetValue(handle, out var udp)) return null;
        var buffer = new byte[65535];
        var result = await udp.UdpSocket.ReceiveFromAsync(
            buffer, SocketFlags.None, new IPEndPoint(IPAddress.Any, 0), cancellationToken).ConfigureAwait(false);
        if (result.ReceivedBytes <= 0 || result.RemoteEndPoint is not IPEndPoint remote) return null;
        return (buffer.AsSpan(0, result.ReceivedBytes).ToArray(), remote.Address.ToString(), remote.Port);
    }

    public long SslConnect(string host, int port, string? certPath)
    {
        try
        {
            var client = new TcpClient();
            client.Connect(host, port);
            var ssl = new System.Net.Security.SslStream(client.GetStream(), false,
                (_, _, _, _) => true); // skip certificate validation for local dev / self-signed
            ssl.AuthenticateAsClient(host);
            var h = NextHandle();
            _sslClients[h] = (client, ssl);
            return h;
        }
        catch { return -1; }
    }

    public async ValueTask<long> SslConnectAsync(string host, int port, string? certPath, CancellationToken cancellationToken)
    {
        var client = new TcpClient();
        try
        {
            await client.ConnectAsync(host, port, cancellationToken).ConfigureAwait(false);
            var ssl = new System.Net.Security.SslStream(client.GetStream(), false, (_, _, _, _) => true);
            await ssl.AuthenticateAsClientAsync(new System.Net.Security.SslClientAuthenticationOptions
            {
                TargetHost = host
            }, cancellationToken).ConfigureAwait(false);
            var handle = NextHandle();
            _sslClients[handle] = (client, ssl);
            return handle;
        }
        catch (OperationCanceledException)
        {
            client.Dispose();
            throw;
        }
        catch
        {
            client.Dispose();
            return -1;
        }
    }

    public long SslListen(int port, string? certPath, string? certPassword)
    {
        try
        {
            var cert = System.Security.Cryptography.X509Certificates.X509CertificateLoader.LoadPkcs12FromFile(certPath ?? "", certPassword ?? "");
            var listener = new TcpListener(IPAddress.Any, port);
            listener.Start();
            var h = NextHandle();
            _sslListeners[h] = listener;
            _sslCerts[h] = cert;
            return h;
        }
        catch { return -1; }
    }

    /// <summary>Accetta una connessione su un listener SSL e completa l'handshake TLS (AuthenticateAsServer).</summary>
    public long SslAccept(long listener)
    {
        if (!_sslListeners.TryGetValue(listener, out var l)) return -1;
        if (!_sslCerts.TryGetValue(listener, out var cert)) return -1;
        try
        {
            var client = l.AcceptTcpClient();
            var ssl = new System.Net.Security.SslStream(client.GetStream(), false);
            ssl.AuthenticateAsServer(cert);
            var h = NextHandle();
            _sslClients[h] = (client, ssl);
            return h;
        }
        catch { return -1; }
    }

    public void SslClose(long handle)
    {
        if (_sslClients.TryRemove(handle, out var sc)) { try { sc.Client.Close(); } catch { } }
        if (_sslListeners.TryRemove(handle, out var l)) { try { l.Stop(); } catch { } }
        _handles.TryRemove(handle, out _);
    }

    public void SslSend(long handle, byte[] data)
    {
        if (!_sslClients.TryGetValue(handle, out var sc)) return;
        try { sc.Stream.Write(data, 0, data.Length); sc.Stream.Flush(); } catch { }
    }

    public byte[]? SslReceive(long handle, int count)
    {
        if (!_sslClients.TryGetValue(handle, out var sc)) return null;
        try
        {
            var buffer = new byte[count];
            var read = sc.Stream.Read(buffer, 0, count);
            return read > 0 ? buffer.AsSpan(0, read).ToArray() : null;
        }
        catch { return null; }
    }

    public void TcpSetOption(long handle, string option, string value)
    {
        if (_tcpClients.TryGetValue(handle, out var c))
        {
            try
            {
                switch (option.ToUpperInvariant())
                {
                    case "TCP_NODELAY":
                        c.NoDelay = string.Equals(value, "1", StringComparison.Ordinal) || string.Equals(value, "true", StringComparison.OrdinalIgnoreCase);
                        break;
                    case "SO_REUSEADDR":
                        c.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, value == "1");
                        break;
                    case "SO_KEEPALIVE":
                        c.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, value == "1");
                        break;
                }
            }
            catch { }
        }
    }

    public void TcpShutdown(long handle, string mode)
    {
        if (_tcpClients.TryGetValue(handle, out var c))
        {
            try
            {
                var how = mode.ToUpperInvariant() switch
                {
                    "SEND" or "1" => SocketShutdown.Send,
                    "RECEIVE" or "RECV" or "2" => SocketShutdown.Receive,
                    "BOTH" or "0" => SocketShutdown.Both,
                    _ => SocketShutdown.Both
                };
                c.Client.Shutdown(how);
            }
            catch { }
        }
    }
}
