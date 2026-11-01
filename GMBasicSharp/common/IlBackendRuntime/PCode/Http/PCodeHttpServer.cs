// FILE: Basic.IlBackend.PCode.Http.PCodeHttpServer.cs
// Fondamenta di rete per HTTP_SERVER nella P-CODE VM: TcpListener + SslStream, I/O interamente
// asincrono (nessun thread OS dedicato per connessione), parsing tramite CommonLib.Http.HttpRequestParser
// (riusato as-is dall'AST executor, engine-agnostic), scrittura risposta HTTP/1.1 scritta a mano.
//
// Decisione architetturale (vedi piano Obsidian PCode-HTTP-Server-Plan.md): niente libuv (l'AST lo
// usa solo perche' e' single-thread cooperativo, esigenza che questa VM non ha), niente
// System.Net.HttpListener nudo (il suo TLS richiede binding certificato a livello OS, non un file
// .pfx come fa oggi HTTP SERVER porta, cert.pfx, password nell'AST). L'I/O e' async/await puro
// (AcceptTcpClientAsync/ReadAsync/WriteAsync) — stessa classe di scalabilita' di libuv (IOCP/epoll/
// kqueue sotto al thread pool .NET) ma senza interop nativo, coerente con la scelta che Kestrel
// stesso ha fatto abbandonando libuv nel 2019.

using System.Buffers;
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using CommonLib.Http;

namespace Basic.IlBackend.PCode.Http;

/// <summary>Un server HTTP attivo: listener, certificato opzionale, config, connessioni tracciate.</summary>
internal sealed class PCodeHttpServerInstance
{
    public required TcpListener Listener { get; init; }
    public required CancellationTokenSource Cts { get; init; }
    public X509Certificate2? Certificate { get; set; }
    public required HttpConfig Config { get; set; }
    /// <summary>Ritorna true se la richiesta e' stata promossa a WebSocket (l'intera sessione,
    /// incluso il loop di ricezione, e' gia' stata gestita per intero DENTRO Dispatch prima del
    /// ritorno — vedi PCodeProcessor.DispatchHttpRequestAsync/HandleWebSocketUpgradeAsync): in
    /// quel caso il chiamante non deve scrivere una risposta HTTP normale ne' continuare il
    /// loop keep-alive, solo chiudere la connessione.</summary>
    public required Func<HttpRequest, PCodeHttpResponse, Stream, CancellationToken, Task<bool>> Dispatch { get; set; }
    public readonly Dictionary<int, TcpClient> ActiveConnections = new();
    public readonly object ConnectionsLock = new();
    public Task? AcceptLoopTask;
    private int _connIdCounter;

    internal int NextConnectionId() => System.Threading.Interlocked.Increment(ref _connIdCounter);
}

/// <summary>
/// Ciclo di accept e gestione connessione HTTP, interamente asincrono. Nessuna logica di
/// routing qui — quella vive nel router/dispatch passato dal chiamante (PCodeProcessor), questa
/// classe si occupa solo di byte sul filo (accept, TLS, framing HTTP/1.1, keep-alive).
/// </summary>
internal static class PCodeHttpServer
{
    public static X509Certificate2? LoadCertificate(string? path, string? password)
    {
        if (string.IsNullOrEmpty(path)) return null;
        return string.IsNullOrEmpty(password)
            ? X509CertificateLoader.LoadPkcs12FromFile(path, null)
            : X509CertificateLoader.LoadPkcs12FromFile(path, password);
    }

    public static PCodeHttpServerInstance Start(int port, X509Certificate2? certificate, HttpConfig config,
        Func<HttpRequest, PCodeHttpResponse, Stream, CancellationToken, Task<bool>> dispatch)
    {
        var listener = new TcpListener(IPAddress.Any, port);
        listener.Start();
        var cts = new CancellationTokenSource();
        var server = new PCodeHttpServerInstance
        {
            Listener = listener,
            Cts = cts,
            Certificate = certificate,
            Config = config,
            Dispatch = dispatch
        };
        server.AcceptLoopTask = AcceptLoopAsync(server);
        return server;
    }

    /// <summary>Ferma il listener, chiude tutte le connessioni tracciate, attende la terminazione
    /// pulita dell'accept loop con un timeout breve (vedi Fase E, HTTP SERVER STOP).</summary>
    public static async Task StopAsync(PCodeHttpServerInstance server)
    {
        server.Cts.Cancel();
        try { server.Listener.Stop(); } catch { /* gia' chiuso */ }

        // Grace period breve: da' alle connessioni WebSocket in corso (vedi
        // PCodeProcessor.HandleWebSocketUpgradeAsync) il tempo di osservare la cancellazione di
        // server.Cts (che sblocca una ReceiveAsync in attesa con OperationCanceledException) e
        // inviare un frame di Close prima di essere chiuse a forza dal loop sotto — senza questa
        // pausa, il c.Close() forzato TCP-level vince quasi sempre la corsa contro il tentativo di
        // chiusura pulita a livello WebSocket (bug reale: anche con HandleWebSocketUpgradeAsync
        // corretto per inviare un Close frame alla cancellazione, HTTP SERVER STOP continuava a
        // produrre "abnormal closure" code 1006 lato client, perche' il socket TCP sottostante
        // veniva chiuso a forza QUI prima che il frame di Close potesse uscire sul filo — vedi
        // TODO-PCODE-BUGS.md BUG 25). Le connessioni che completano la chiusura pulita entro la
        // pausa si rimuovono gia' da sole da ActiveConnections (HandleConnectionAsync, finally),
        // quindi lo snapshot preso sotto le esclude automaticamente.
        await Task.Delay(250).ConfigureAwait(false);

        TcpClient[] toClose;
        lock (server.ConnectionsLock) { toClose = [.. server.ActiveConnections.Values]; server.ActiveConnections.Clear(); }
        foreach (var c in toClose) { try { c.Close(); } catch { } }

        if (server.AcceptLoopTask != null)
        {
            try { await server.AcceptLoopTask.WaitAsync(TimeSpan.FromSeconds(5)); }
            catch { /* timeout o cancellazione: usciamo comunque */ }
        }
    }

    private static async Task AcceptLoopAsync(PCodeHttpServerInstance server)
    {
        var token = server.Cts.Token;
        while (!token.IsCancellationRequested)
        {
            TcpClient client;
            try
            {
                client = await server.Listener.AcceptTcpClientAsync(token).ConfigureAwait(false);
            }
            catch (OperationCanceledException) { break; }
            catch (ObjectDisposedException) { break; }
            catch (SocketException) { break; }

            var connId = server.NextConnectionId();
            lock (server.ConnectionsLock) server.ActiveConnections[connId] = client;
            _ = HandleConnectionAsync(server, client, connId, token);
        }
    }

    private static async Task HandleConnectionAsync(PCodeHttpServerInstance server, TcpClient client, int connId, CancellationToken serverToken)
    {
        try
        {
            client.NoDelay = true;
            Stream stream = client.GetStream();
            if (server.Certificate != null)
            {
                var ssl = new SslStream(stream, false);
                await ssl.AuthenticateAsServerAsync(new SslServerAuthenticationOptions
                {
                    ServerCertificate = server.Certificate,
                    EnabledSslProtocols = SslProtocols.None // negozia la versione piu' recente supportata dal sistema
                }, serverToken).ConfigureAwait(false);
                stream = ssl;
            }

            var remoteIp = (client.Client.RemoteEndPoint as IPEndPoint)?.Address.ToString() ?? "";
            var parser = new HttpRequestParser();
            var accum = new List<byte>();
            var keepAlive = true;

            while (keepAlive && !serverToken.IsCancellationRequested)
            {
                var request = await ReadOneRequestAsync(stream, parser, accum, serverToken).ConfigureAwait(false);
                if (request == null) break; // connessione chiusa dal client o dati malformati

                request = request with { RemoteIpAddress = remoteIp };
                var response = new PCodeHttpResponse();
                // Streaming (chunked): il codice BASIC scrive sul socket mentre gira.
                response.AttachStream(stream, serverToken, server.Config.EnableKeepAlive);
                // Range della richiesta: serve a res.SendFile per servire una fetta (206).
                response.AttachRequestInfo(
                    request.Headers.TryGetPropertyValue("Range", out var reqRange) ? reqRange?.ToString() : null);

                // IP allow/block (HTTP CONFIG AllowedIPs/BlockedIPs — CIDR IPv4/IP singoli)
                var blocked = !CommonLib.Http.HttpSecurity.IpAllowed(remoteIp, server.Config.AllowedIPs, server.Config.BlockedIPs);

                // Body size cap (HTTP CONFIG MaxRequestBodySize → 413; prima enforce solo in IL)
                var bodyTooLarge = false;
                if (server.Config.MaxRequestBodySize > 0 &&
                    request.Headers.TryGetPropertyValue("Content-Length", out var clNode) &&
                    long.TryParse(clNode?.ToString(), out var cl))
                    bodyTooLarge = cl > server.Config.MaxRequestBodySize;

                if (blocked || bodyTooLarge)
                {
                    response.SetStatus(bodyTooLarge ? 413 : 403);
                    response.Send(bodyTooLarge ? "413 Payload Too Large" : "403 Forbidden");
                }
                else
                {
                    var upgraded = await server.Dispatch(request, response, stream, serverToken).ConfigureAwait(false);
                    if (upgraded) return; // sessione WebSocket gia' conclusa per intero dentro Dispatch
                }

                if (!response.Sent && !response.Streaming)
                    response.CompleteAsServerError("500 Internal Server Error: no response was ever sent");

                // Header di sicurezza automatici (HTTP CONFIG SecurityHeaders) — non
                // sovrascrivono header gia' impostati esplicitamente da route/middleware.
                if (server.Config.SecurityHeaders)
                {
                    foreach (var (key, value) in CommonLib.Http.HttpSecurity.DefaultSecurityHeaders())
                        if (!response.Headers.ContainsKey(key))
                            response.Headers[key] = value;
                }

                keepAlive = ShouldKeepAlive(request, response, server.Config);
                if (response.Streaming)
                {
                    /* Risposta già inviata in chunk: chiude con il chunk terminale. */
                    response.CompleteStream();
                    keepAlive = false;   /* dopo uno stream la connessione si chiude (semantica SSE) */
                }
                else
                {
                    await WriteResponseAsync(stream, response, keepAlive, serverToken,
                        string.Equals(request.Method, "HEAD", StringComparison.OrdinalIgnoreCase)).ConfigureAwait(false);
                }
            }
        }
        catch { /* connessione caduta: nessuna azione, si chiude sotto */ }
        finally
        {
            lock (server.ConnectionsLock) server.ActiveConnections.Remove(connId);
            try { client.Close(); } catch { }
        }
    }

    /// <summary>
    /// Legge dallo stream finche' HttpRequestParser non segnala una richiesta completa.
    /// Implementazione volutamente semplice (non zero-copy): per un server HTTP di un
    /// interprete BASIC non pensato per carichi da milioni di richieste/secondo, la chiarezza
    /// vale piu' di qualche allocazione in piu' per richiesta.
    /// </summary>
    private static async Task<HttpRequest?> ReadOneRequestAsync(Stream stream, HttpRequestParser parser, List<byte> accum, CancellationToken ct)
    {
        var chunk = new byte[8192];
        while (true)
        {
            if (accum.Count > 0)
            {
                var seq = new ReadOnlySequence<byte>(accum.ToArray());
                if (parser.Parse(ref seq))
                {
                    var consumed = accum.Count - (int)seq.Length;
                    if (consumed > 0) accum.RemoveRange(0, consumed);
                    return parser.GetCompletedRequest();
                }
                if (parser.State == ParsingState.Invalid) return null;

                // BUG FIX (2026-09-01): per i body che richiedono PIÙ letture, il parser avanza
                // `seq` (consuma request-line+header+parte del body) ma `accum` NON veniva
                // ridotto finché il parse non era completo. Al giro successivo il parser veniva
                // richiamato su accum DALL'INIZIO (header ri-parse + body buffer già pieno) →
                // per body grandi (es. publish base64 > ~400KB) il Body risultante conteneva
                // anche gli header HTTP e JSON.PARSE falliva. Rimuoviamo i byte già consumati a
                // ogni iterazione (il parser mantiene lo stato interno in _bodyBuffer).
                var consumedNow = accum.Count - (int)seq.Length;
                if (consumedNow > 0) accum.RemoveRange(0, consumedNow);
            }

            int n;
            try { n = await stream.ReadAsync(chunk, ct).ConfigureAwait(false); }
            catch { return null; }
            if (n == 0) return null; // connessione chiusa dal client
            accum.AddRange(chunk.AsSpan(0, n).ToArray());
        }
    }

    private static bool ShouldKeepAlive(HttpRequest request, PCodeHttpResponse response, HttpConfig config)
    {
        if (!config.EnableKeepAlive) return false;
        if (response.Headers.TryGetValue("Connection", out var respConn) &&
            respConn.Contains("close", StringComparison.OrdinalIgnoreCase)) return false;
        if (request.Headers.TryGetPropertyValue("Connection", out var connNode))
        {
            var v = connNode?.GetValue<string>() ?? "";
            if (v.Contains("close", StringComparison.OrdinalIgnoreCase)) return false;
        }
        return true;
    }

    private static async Task WriteResponseAsync(Stream stream, PCodeHttpResponse response, bool keepAlive, CancellationToken ct, bool headOnly = false)
    {
        var sb = new StringBuilder();
        sb.Append("HTTP/1.1 ").Append(response.StatusCode).Append(' ').Append(ReasonPhrase(response.StatusCode)).Append("\r\n");
        foreach (var kv in response.Headers)
        {
            if (kv.Key.Equals("Content-Length", StringComparison.OrdinalIgnoreCase)) continue;
            if (kv.Key.Equals("Connection", StringComparison.OrdinalIgnoreCase)) continue;
            sb.Append(kv.Key).Append(": ").Append(kv.Value).Append("\r\n");
        }
        foreach (var cookie in response.RawSetCookieHeaders)
            sb.Append("Set-Cookie: ").Append(cookie).Append("\r\n");
        sb.Append("Content-Length: ").Append(response.Body.Length).Append("\r\n");
        sb.Append("Connection: ").Append(keepAlive ? "keep-alive" : "close").Append("\r\n");
        sb.Append("\r\n");

        var headerBytes = Encoding.ASCII.GetBytes(sb.ToString());
        await stream.WriteAsync(headerBytes, ct).ConfigureAwait(false);
        // HEAD: header completi (Content-Length compreso), nessun corpo.
        if (response.Body.Length > 0 && !headOnly) await stream.WriteAsync(response.Body, ct).ConfigureAwait(false);
        await stream.FlushAsync(ct).ConfigureAwait(false);
    }

    private static string ReasonPhrase(int statusCode) => statusCode switch
    {
        200 => "OK",
        201 => "Created",
        204 => "No Content",
        301 => "Moved Permanently",
        302 => "Found",
        304 => "Not Modified",
        400 => "Bad Request",
        401 => "Unauthorized",
        403 => "Forbidden",
        404 => "Not Found",
        405 => "Method Not Allowed",
        429 => "Too Many Requests",
        500 => "Internal Server Error",
        501 => "Not Implemented",
        504 => "Gateway Timeout",
        _ => "Unknown"
    };
}
