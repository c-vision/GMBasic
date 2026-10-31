// FILE: Basic.Runtime.Services.HttpService.cs
// HTTP client + server usando HttpClient e HttpListener (built-in .NET).

using System.Buffers;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Http;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json;

namespace Basic.Runtime;

public sealed class HttpService : IHttpService, IDisposable
{
    private readonly HttpClient _client = new();
    private readonly ConcurrentDictionary<long, HttpListener> _servers = new();
    private readonly ConcurrentDictionary<long, (System.Net.Sockets.TcpListener Listener, X509Certificate2 Cert)> _tlsServers = new();
    private long _nextHandle = 1000;

    // ================================================================
    // HTTP CLIENT
    // ================================================================
    public string HttpGet(string url)
    {
        try { return _client.GetStringAsync(url).GetAwaiter().GetResult(); }
        catch { return ""; }
    }

    public string HttpPost(string url, string body, string contentType)
    {
        try
        {
            var content = new StringContent(body, Encoding.UTF8, contentType ?? "text/plain");
            var resp = _client.PostAsync(url, content).GetAwaiter().GetResult();
            return resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
        }
        catch { return ""; }
    }

    public string HttpPut(string url, string body, string contentType)
    {
        try
        {
            var content = new StringContent(body, Encoding.UTF8, contentType ?? "text/plain");
            var resp = _client.PutAsync(url, content).GetAwaiter().GetResult();
            return resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
        }
        catch { return ""; }
    }

    public string HttpDelete(string url)
    {
        try
        {
            var resp = _client.DeleteAsync(url).GetAwaiter().GetResult();
            return resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
        }
        catch { return ""; }
    }

    public byte[] HttpDownload(string url)
    {
        try { return _client.GetByteArrayAsync(url).GetAwaiter().GetResult(); }
        catch { return []; }
    }

    public string HttpUpload(string url, string filePath, string fieldName)
    {
        try
        {
            using var form = new MultipartFormDataContent();
            form.Add(new ByteArrayContent(File.ReadAllBytes(filePath)), fieldName, Path.GetFileName(filePath));
            var resp = _client.PostAsync(url, form).GetAwaiter().GetResult();
            return resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
        }
        catch { return ""; }
    }

    // ================================================================
    // HTTP SERVER (via HttpListener)
    // ================================================================
    private readonly ConcurrentDictionary<long, List<RouteEntry>> _routes = new();
    private readonly ConcurrentDictionary<long, (string Prefix, string Directory)> _staticDirs = new();
    private readonly ConcurrentDictionary<long, List<(string PathPrefix, Func<HttpRequest, (bool Continue, HttpResponse? Response)> Middleware)>> _middlewares = new();
    private readonly ConcurrentDictionary<long, List<(string Path, Func<HttpRequest, System.Net.WebSockets.WebSocket, Task> Handler)>> _webSocketRoutes = new();
    private readonly ConcurrentDictionary<long, HttpServerSettings> _settings = new();
    private readonly ConcurrentDictionary<long, RateLimiter> _rateLimiters = new();

    /// <summary>
    /// Hook di iniezione per servire pagine .bsp (Basic Server Pages) — BRT non può referenziare
    /// IlBackend (dipendenza nella direzione opposta, IlBackend -> BRT), quindi non può compilare
    /// né eseguire P-Code direttamente. IlBackend imposta questo delegate statico (una volta,
    /// vedi IlRuntime) puntando alla sua logica di compilazione/esecuzione BSP (Compiler + PCodeVm
    /// interno, common/IlBackend/PCode/). Riceve il path fisico assoluto del file .bsp e la
    /// richiesta; ritorna null se il file non è servibile come BSP (fallback al comportamento
    /// normale), altrimenti la risposta già pronta.
    /// </summary>
    public static Func<string, HttpRequest, HttpResponse?>? GlobalBspPageHandler { get; set; }

    /// <summary>
    /// `Path` può contenere placeholder `{nome}` (es. `/download/{name}/{version}`, estratti in
    /// `Params` — vedi `TryMatch`) o un `*` finale (wildcard, nessun parametro estratto, stesso
    /// comportamento della vecchia `MatchPath` rimossa). Stesso schema di matching già usato in
    /// `common/IlBackend/PCode/Http/PCodeHttpRouter.cs` per l'esecuzione via PCodeVm interno — qui
    /// la controparte per il server HTTP reale (`HttpService`/`IHttpService`), prima assente
    /// (route con placeholder non facevano mai match).
    /// </summary>
    private sealed class RouteEntry
    {
        public string Method { get; }
        public string PathTemplate { get; }
        public Func<HttpRequest, HttpResponse> Handler { get; }
        private readonly System.Text.RegularExpressions.Regex _regex;
        private readonly List<string> _paramNames = new();

        public RouteEntry(string method, string pathTemplate, Func<HttpRequest, HttpResponse> handler)
        {
            Method = method;
            PathTemplate = pathTemplate;
            Handler = handler;
            var escaped = System.Text.RegularExpressions.Regex.Escape(pathTemplate);
            var pattern = System.Text.RegularExpressions.Regex.Replace(escaped,
                @"\\\{([a-zA-Z_][a-zA-Z0-9_]*)\}",
                m => { _paramNames.Add(m.Groups[1].Value); return "([^/]+)"; });
            // '*' finale: Regex.Escape lo lascia intatto (non un metacarattere da escapare) — va
            // tradotto in ".*" DOPO la sostituzione dei placeholder, altrimenti l'ancora "/?$"
            // sotto renderebbe il match sempre esatto anche per pattern come "/static/*".
            if (pattern.EndsWith('*'))
                pattern = pattern[..^1] + ".*";
            _regex = new System.Text.RegularExpressions.Regex("^" + pattern + "/?$",
                System.Text.RegularExpressions.RegexOptions.IgnoreCase | System.Text.RegularExpressions.RegexOptions.Compiled);
        }

        public bool TryMatch(string path, out Dictionary<string, string> parameters)
        {
            parameters = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var m = _regex.Match(path);
            if (!m.Success) return false;
            for (var i = 0; i < _paramNames.Count; i++)
                parameters[_paramNames[i]] = Uri.UnescapeDataString(m.Groups[i + 1].Value);
            return true;
        }
    }

    public long HttpServerStart(int port)
    {
        try
        {
            var listener = new HttpListener();
            listener.Prefixes.Add($"http://localhost:{port}/");
            // Le pagine BSP (e i client locali) possono puntare a 127.0.0.1 (RegistryBsp:
            // FETCH("http://127.0.0.1:{port}/api/...")) — senza il prefisso esplicito il
            // listener risponde 404/rifiuta su quell'host e la pagina resta vuota/pende.
            try { listener.Prefixes.Add($"http://127.0.0.1:{port}/"); } catch { /* già coperto */ }
            listener.Start();
            var h = _nextHandle++;
            _servers[h] = listener;
            _routes[h] = new List<RouteEntry>();
            _middlewares[h] = new List<(string, Func<HttpRequest, (bool, HttpResponse?)>)>();
            _webSocketRoutes[h] = new List<(string, Func<HttpRequest, System.Net.WebSockets.WebSocket, Task>)>();

            // Start listener in background (async: gestisce anche upgrade WebSocket).
            // NON usare Task.Run: l'accept loop è async (GetContextAsync, mai bloccante) e parte
            // subito sul thread corrente fino al primo await — un Task.Run in coda al threadpool
            // può non essere schedulato a tempo dopo test pesanti (pool saturo) → listener
            // Started ma nessun accept → il client non riesce a connettersi (bug HttpListener).
            var listenerHandle = h;
            _ = ServerLoopAsync(listenerHandle, listener);
            return h;
        }
        catch { return -1; }
    }

    /// <summary>
    /// HTTPS su IL (fix 2026-09-02): HttpListener NON supporta TLS su .NET multipiattaforma.
    /// Trasporto alternativo TcpListener + SslStream (speculare al PCodeHttpServer): parsing raw
    /// HTTP/1.1 con CommonLib.Http.HttpRequestParser, poi lo STESSO Dispatch dei route/middleware/
    /// BSP. Prima versione: una risposta per connessione (Connection: close), WebSocket upgrade
    /// non supportato (501) — il path plain (HttpListener) resta invariato.
    /// </summary>
    public long HttpServerStartTls(int port, string certPath, string certPassword)
    {
        try
        {
            var cert = string.IsNullOrEmpty(certPath) ? null : new X509Certificate2(certPath, certPassword, X509KeyStorageFlags.Exportable);
            if (cert is null) return -1;
            var listener = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback, port);
            listener.Start();
            var h = _nextHandle++;
            _tlsServers[h] = (listener, cert);
            _routes[h] = new List<RouteEntry>();
            _middlewares[h] = new List<(string, Func<HttpRequest, (bool, HttpResponse?)>)>();
            _webSocketRoutes[h] = new List<(string, Func<HttpRequest, System.Net.WebSockets.WebSocket, Task>)>();
            var lh = h;
            _ = TlsServerLoopAsync(lh, listener, cert);
            return h;
        }
        catch { return -1; }
    }

    /// <summary>Loop di accettazione per il server HTTPS (TcpListener).</summary>
    private async Task TlsServerLoopAsync(long handle, System.Net.Sockets.TcpListener listener, X509Certificate2 cert)
    {
        while (true)
        {
            System.Net.Sockets.TcpClient client;
            try { client = await listener.AcceptTcpClientAsync().ConfigureAwait(false); }
            catch { break; }
            _ = Task.Run(() => HandleTlsClientAsync(handle, client, cert));
        }
    }

    /// <summary>Serve UNA connessione HTTPS: handshake TLS, lettura raw HTTP/1.1, dispatch.</summary>
    private async Task HandleTlsClientAsync(long handle, System.Net.Sockets.TcpClient client, X509Certificate2 cert)
    {
        try
        {
            using (client)
            {
                client.NoDelay = true;
                var ssl = new SslStream(client.GetStream(), false);
                await ssl.AuthenticateAsServerAsync(new SslServerAuthenticationOptions
                {
                    ServerCertificate = cert,
                    EnabledSslProtocols = SslProtocols.None
                }).ConfigureAwait(false);

                var settings = _settings.TryGetValue(handle, out var s) ? s : new HttpServerSettings();
                var remoteIp = (client.Client.RemoteEndPoint as IPEndPoint)?.Address?.ToString() ?? "";

                // IP allow/block (HTTP CONFIG)
                if (!CommonLib.Http.HttpSecurity.IpAllowed(remoteIp, settings.AllowedIPs, settings.BlockedIPs))
                {
                    await WriteTlsResponseAsync(ssl, new HttpResponse { StatusCode = 403, ContentType = "text/plain", Body = "403 Forbidden" }, settings).ConfigureAwait(false);
                    return;
                }

                var parser = new CommonLib.Http.HttpRequestParser();
                var accum = new List<byte>();
                var request = await ReadTlsRequestAsync(ssl, parser, accum).ConfigureAwait(false);
                if (request == null) return;
                var req = MapRequest(request);
                req.RemoteIpAddress = remoteIp;

                // WebSocket upgrade non supportato su questo trasporto TLS (prima versione)
                if (req.Headers.TryGetValue("Upgrade", out var up) &&
                    up.Contains("websocket", StringComparison.OrdinalIgnoreCase))
                {
                    await WriteTlsResponseAsync(ssl, new HttpResponse { StatusCode = 501, ContentType = "text/plain", Body = "501 WebSocket over TLS not implemented" }, settings).ConfigureAwait(false);
                    return;
                }

                // Body size cap (HTTP CONFIG MaxRequestBodySize)
                if (settings.MaxRequestBodySize > 0 &&
                    req.Headers.TryGetValue("Content-Length", out var cl) &&
                    long.TryParse(cl, out var len) && len > settings.MaxRequestBodySize)
                {
                    await WriteTlsResponseAsync(ssl, new HttpResponse { StatusCode = 413, ContentType = "text/plain", Body = "413 Payload Too Large" }, settings).ConfigureAwait(false);
                    return;
                }

                // Rate-limit (HTTP CONFIG) — stesso token bucket del path plain
                if (settings.RateLimitRequestsPerMinute > 0)
                {
                    var limiter = _rateLimiters.GetOrAdd(handle, _ => new RateLimiter(settings.RateLimitRequestsPerMinute, settings.RateLimitBurstSize));
                    if (!limiter.TryAcquire())
                    {
                        await WriteTlsResponseAsync(ssl, new HttpResponse { StatusCode = 429, ContentType = "text/plain", Body = "429 Too Many Requests" }, settings).ConfigureAwait(false);
                        return;
                    }
                }

                // Sessioni route HTTP (HTTP CONFIG .Sessions) — prima del dispatch
                if (settings.Sessions && req.SessionData == null)
                {
                    var cookieHeader = req.Headers.TryGetValue("Cookie", out var ck) ? ck : null;
                    var (setCookie, data) = CommonLib.Http.HttpSessions.Ensure(cookieHeader);
                    req.SessionData = data;
                    req.SessionSetCookie = setCookie;
                }

                HttpResponse resp;
                try { resp = Dispatch(handle, req) ?? new HttpResponse { StatusCode = 500, ContentType = "text/plain", Body = "500 No response" }; }
                catch { resp = new HttpResponse { StatusCode = 500, ContentType = "text/plain", Body = "500 Internal Server Error" }; }

                if (req.SessionSetCookie != null && !resp.Headers.ContainsKey("Set-Cookie"))
                    resp.Headers["Set-Cookie"] = req.SessionSetCookie;

                await WriteTlsResponseAsync(ssl, resp, settings).ConfigureAwait(false);
            }
        }
        catch { /* connessione caduta / handshake fallito */ }
    }

    /// <summary>Legge dallo stream TLS finché HttpRequestParser ha una richiesta completa.</summary>
    private static async Task<CommonLib.Http.HttpRequest?> ReadTlsRequestAsync(Stream stream, CommonLib.Http.HttpRequestParser parser, List<byte> accum)
    {
        var chunk = new byte[16384];
        while (true)
        {
            if (accum.Count > 0)
            {
                var seq = new ReadOnlySequence<byte>(accum.ToArray());
                if (parser.Parse(ref seq)) return parser.GetCompletedRequest();
            }
            int n;
            try { n = await stream.ReadAsync(chunk).ConfigureAwait(false); }
            catch { return null; }
            if (n == 0) return null;
            accum.AddRange(chunk.AsSpan(0, n).ToArray());
        }
    }

    /// <summary>Mappa la CommonLib.Http.HttpRequest (JsonObject headers) sul tipo HttpRequest del BRT.</summary>
    private static HttpRequest MapRequest(CommonLib.Http.HttpRequest req)
    {
        var headers = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in req.Headers)
            headers[kv.Key] = kv.Value?.ToString() ?? "";
        var query = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in req.Query)
            query[kv.Key] = kv.Value?.ToString() ?? "";
        return new HttpRequest
        {
            Method = req.Method,
            Path = req.Path,
            Body = req.Body,
            Headers = headers,
            Query = query
        };
    }

    /// <summary>Scrive una risposta HTTP/1.1 su stream TLS (header di sicurezza se configurati).</summary>
    private static async Task WriteTlsResponseAsync(Stream stream, HttpResponse resp, HttpServerSettings settings)
    {
        var sb = new StringBuilder();
        var code = resp.StatusCode;
        sb.Append("HTTP/1.1 ").Append(code).Append(' ').Append(ReasonPhrase(code)).Append("\r\n");
        // Corpo binario se presente (res.SendFile, ZIP, immagini): i byte grezzi, altrimenti il
        // solo testo UTF-8 come prima. Content-Length deve coincidere con i byte inviati.
        var rawBody = resp.BodyBytes;
        var body = rawBody is null ? (resp.Body ?? "") : null;
        foreach (var kv in resp.Headers)
        {
            if (kv.Key.Equals("Content-Length", StringComparison.OrdinalIgnoreCase)) continue;
            if (kv.Key.Equals("Connection", StringComparison.OrdinalIgnoreCase)) continue;
            sb.Append(kv.Key).Append(": ").Append(kv.Value).Append("\r\n");
        }
        if (settings?.SecurityHeaders == true)
        {
            foreach (var (key, value) in CommonLib.Http.HttpSecurity.DefaultSecurityHeaders())
                if (!resp.Headers.ContainsKey(key))
                    sb.Append(key).Append(": ").Append(value).Append("\r\n");
        }
        sb.Append("Content-Type: ").Append(string.IsNullOrEmpty(resp.ContentType) ? "text/plain" : resp.ContentType).Append("\r\n");
        var bodyBytes = rawBody ?? Encoding.UTF8.GetBytes(body!);
        sb.Append("Content-Length: ").Append(bodyBytes.Length).Append("\r\n");
        sb.Append("Connection: close\r\n\r\n");
        var bytes = Encoding.UTF8.GetBytes(sb.ToString());
        await stream.WriteAsync(bytes).ConfigureAwait(false);
        if (bodyBytes.Length > 0)
            await stream.WriteAsync(bodyBytes).ConfigureAwait(false);
        await stream.FlushAsync().ConfigureAwait(false);
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
        413 => "Payload Too Large",
        429 => "Too Many Requests",
        500 => "Internal Server Error",
        501 => "Not Implemented",
        504 => "Gateway Timeout",
        _ => "Unknown"
    };

    public void HttpServerStop(long handle)
    {
        if (_tlsServers.TryRemove(handle, out var tls))
        {
            try { tls.Listener.Stop(); } catch { }
            try { tls.Listener.Server?.Close(); } catch { }
        }
        if (_servers.TryRemove(handle, out var listener))
        {
            try { listener.Stop(); } catch { }
            try { listener.Close(); } catch { }
        }
        _routes.TryRemove(handle, out _);
        _staticDirs.TryRemove(handle, out _);
        _middlewares.TryRemove(handle, out _);
        _webSocketRoutes.TryRemove(handle, out _);
        _settings.TryRemove(handle, out _);
        _rateLimiters.TryRemove(handle, out _);
    }

    public void HttpAddRoute(long handle, string method, string path, Func<HttpRequest, HttpResponse> handler)
    {
        if (_routes.TryGetValue(handle, out var routes))
            routes.Add(new RouteEntry(method.ToUpperInvariant(), path, handler));
    }

    /// <summary>
    /// Prima ignorava `prefix` (salvava solo `directory`) — Dispatch combinava la directory con
    /// il PATH COMPLETO della richiesta, prefisso incluso (es. `HTTP STATIC "/static",
    /// "./wwwroot"` + richiesta `/static/index.html` cercava `./wwwroot/static/index.html`, MAI
    /// `./wwwroot/index.html`): ogni file serviva 404 a meno di duplicare il nome del prefisso
    /// come sottocartella fisica. Scoperto verificando dal vivo con un server reale + curl se le
    /// pagine BSP fossero davvero raggiungibili (non solo compilabili) — lo stesso bug rendeva
    /// non funzionante anche `samples/Http/HttpStatic.bas`, mai eseguito per verifica.
    /// </summary>
    public void HttpAddStatic(long handle, string prefix, string directory)
    {
        _staticDirs[handle] = (prefix.TrimEnd('/'), directory);
    }

    /// <summary>
    /// Prima era un no-op esplicito ("non implementato in HttpListener") — nessun middleware
    /// registrato riceveva mai traffico reale, stesso identico gap di HTTP STATIC/ROUTE prima dei
    /// loro fix gemelli. In ordine di registrazione, eseguiti da <see cref="Dispatch"/> PRIMA di
    /// route/file statici per ogni richiesta il cui path combacia col prefisso.
    /// </summary>
    public void HttpAddMiddleware(long handle, string pathPrefix, Func<HttpRequest, (bool Continue, HttpResponse? Response)> middleware)
    {
        if (_middlewares.TryGetValue(handle, out var list))
            list.Add((pathPrefix, middleware));
    }

    /// <summary>Registra una route WebSocket (upgrade HTTP) — vedi <see cref="IHttpService.HttpAddWebSocketRoute"/>.</summary>
    public void HttpAddWebSocketRoute(long handle, string path, Func<HttpRequest, System.Net.WebSockets.WebSocket, Task> handler)
    {
        if (_webSocketRoutes.TryGetValue(handle, out var list))
            list.Add((path, handler));
    }

    /// <summary>Applica le impostazioni di servizio (HTTP CONFIG) — vedi <see cref="HttpServerSettings"/>.</summary>
    public void HttpConfigure(long handle, HttpServerSettings settings)
    {
        _settings[handle] = settings ?? new HttpServerSettings();
        if (!string.IsNullOrEmpty(settings?.SessionFile))
            Basic.Runtime.Services.SessionStorage.ConfigureFile(settings.SessionFile);
    }

    private static bool MatchWsPath(string pattern, string path)
    {
        if (pattern == path) return true;
        if (pattern.EndsWith('*') && path.StartsWith(pattern.TrimEnd('*'), StringComparison.OrdinalIgnoreCase))
            return true;
        return false;
    }

    /// <summary>Token bucket semplice per il rate-limit HTTP CONFIG (RateLimitRequestsPerMinute /
    /// RateLimitBurstSize). Non [ThreadStatic]: il ServerLoop è un singolo thread.</summary>
    private sealed class RateLimiter
    {
        private readonly int _perMinute;
        private readonly int _burst;
        private int _tokens;
        private DateTime _refillAt = DateTime.UtcNow;
        private readonly object _lock = new();

        public RateLimiter(int perMinute, int burst)
        {
            _perMinute = perMinute;
            _burst = Math.Max(1, burst);
            _tokens = _burst;
        }

        public bool TryAcquire()
        {
            lock (_lock)
            {
                var now = DateTime.UtcNow;
                var elapsedMs = (now - _refillAt).TotalMilliseconds;
                if (elapsedMs > 0)
                {
                    var add = (int)(_perMinute * elapsedMs / 60000.0);
                    if (add > 0)
                    {
                        _tokens = Math.Min(_burst, _tokens + add);
                        _refillAt = now;
                    }
                }
                if (_tokens <= 0) return false;
                _tokens--;
                return true;
            }
        }
    }

    /// <summary>
    /// Loop di accettazione asincrono: gestisce anche gli upgrade WebSocket (una richiesta con
    /// `Upgrade: websocket` su una route WS viene passata al handler col WebSocket accettato,
    /// e la sessione gira in background senza bloccare l'accept loop), il rate-limit e il
    /// timeout per richiesta di HTTP CONFIG. ACCEPT ASINCRONO (GetContextAsync): nessun thread
    /// di pool resta bloccato in attesa — parità col PCodeHttpServer (TcpListener async), che
    /// dopo test pesanti non soffre della starvation dell'HttpListener sincrono.
    /// </summary>
    private async Task ServerLoopAsync(long handle, HttpListener listener)
    {
        while (listener.IsListening)
        {
            HttpListenerContext context;
            try { context = await listener.GetContextAsync().ConfigureAwait(false); }
            catch (HttpListenerException) { break; }
            catch { continue; }

            var settings = _settings.TryGetValue(handle, out var s) ? s : new HttpServerSettings();

            // Ogni richiesta è processata CONCORRENTEMENTE (fire-and-forget): il loop torna subito a
            // GetContext() e accetta le richieste successive mentre la corrente è ancora in corso.
            // Prima il loop attendeva `process.Result` per richiesta → SINGLE-REQUEST-AT-A-TIME: una
            // pagina BSP che FETCHa lo STESSO server (es. index.bsp → /api/packages su 127.0.0.1) non
            // veniva mai accettata (il loop era bloccato sulla pagina) → deadlock (curl timeout) e
            // il processo moriva. Il VM/C serve già per-request in VM separate.
            _ = Task.Run(() => ProcessRequestAsync(handle, context, settings));
        }
    }

    /// <summary>Elabora UNA richiesta in background (fire-and-forget): body-size check, upgrade
    /// WebSocket, rate-limit, dispatch con timeout e invio risposta. Le eccezioni sono isolate
    /// per richiesta — un errore (es. client che si scollega durante la risposta) non deve mai
    /// uccidere l'accept loop né il processo.</summary>
    private async Task ProcessRequestAsync(long handle, HttpListenerContext context, HttpServerSettings settings)
    {
        try
        {
            // IP allow/block (HTTP CONFIG AllowedIPs/BlockedIPs — CIDR IPv4/IP singoli)
            var remoteIp = context.Request.RemoteEndPoint?.Address?.ToString() ?? "";
            if (!CommonLib.Http.HttpSecurity.IpAllowed(remoteIp, settings.AllowedIPs, settings.BlockedIPs))
            {
                SendResponse(context.Response, new HttpResponse { StatusCode = 403, ContentType = "text/plain", Body = "403 Forbidden" }, settings);
                return;
            }

            // Dimensione massima del body (HTTP CONFIG MaxRequestBodySize)
            if (settings.MaxRequestBodySize > 0 && context.Request.ContentLength64 > settings.MaxRequestBodySize)
            {
                SendResponse(context.Response, new HttpResponse { StatusCode = 413, Body = "413 Payload Too Large" }, settings);
                return;
            }

            // Upgrade WebSocket: handshake 101 + sessione in background. Attenzione a NON
            // leggere request.InputStream PRIMA di AcceptWebSocketAsync: per una richiesta di
            // upgrade il body non esiste e ReadToEnd (in ConvertRequest) resterebbe bloccato
            // per sempre finché il client non chiude — hang della richiesta.
            var path = context.Request.Url?.AbsolutePath ?? "/";
            var isUpgrade = string.Equals(context.Request.Headers["Upgrade"], "websocket", StringComparison.OrdinalIgnoreCase);
            if (isUpgrade && _webSocketRoutes.TryGetValue(handle, out var wsRoutes))
            {
                var wsMatch = wsRoutes.FirstOrDefault(r => MatchWsPath(r.Path, path));
                if (wsMatch.Handler != null)
                {
                    var wsContext = await context.AcceptWebSocketAsync(null).ConfigureAwait(false);
                    var wsReq = new HttpRequest
                    {
                        Method = context.Request.HttpMethod,
                        Path = path,
                        Headers = context.Request.Headers.AllKeys.ToDictionary(k => k ?? "", k => context.Request.Headers[k] ?? ""),
                        Query = (context.Request.Url?.Query ?? "").TrimStart('?').Split('&')
                            .Where(s => s.Contains('='))
                            .Select(s => s.Split('=', 2))
                            .ToDictionary(p => p[0], p => Uri.UnescapeDataString(p[1])),
                    };
                    _ = Task.Run(() => wsMatch.Handler(wsReq, wsContext.WebSocket));
                    return;
                }
            }

            // Rate-limit (HTTP CONFIG)
            if (settings.RateLimitRequestsPerMinute > 0)
            {
                var limiter = _rateLimiters.GetOrAdd(handle, _ => new RateLimiter(settings.RateLimitRequestsPerMinute, settings.RateLimitBurstSize));
                if (!limiter.TryAcquire())
                {
                    SendResponse(context.Response, new HttpResponse { StatusCode = 429, Body = "429 Too Many Requests" }, settings);
                    return;
                }
            }

            // Timeout per richiesta (HTTP CONFIG RequestTimeoutMs): legge+dispatches in un
            // task, se non completa entro il timeout → 503 (il task prosegue in background).
            var process = Task.Run(() =>
            {
                var request = ConvertRequest(context.Request, settings);
                // Sessioni route HTTP (HTTP CONFIG .Sessions): data VIVO + cookie prima risposta
                if (settings.Sessions && request.SessionData == null)
                {
                    var cookieHeader = request.Headers.TryGetValue("Cookie", out var ck) ? ck : null;
                    var (setCookie, data) = CommonLib.Http.HttpSessions.Ensure(cookieHeader);
                    request.SessionData = data;
                    request.SessionSetCookie = setCookie;
                }
                var dispatchResp = Dispatch(handle, request);
                if (request.SessionSetCookie != null && !dispatchResp.Headers.ContainsKey("Set-Cookie"))
                    dispatchResp.Headers["Set-Cookie"] = request.SessionSetCookie;
                return dispatchResp;
            });
            HttpResponse resp;
            if (settings.RequestTimeoutMs > 0)
            {
                var completed = await Task.WhenAny(process, Task.Delay(settings.RequestTimeoutMs)).ConfigureAwait(false) == process;
                resp = completed ? process.Result : new HttpResponse { StatusCode = 503, ContentType = "text/plain", Body = "503 Request Timeout" };
            }
            else
            {
                resp = await process.ConfigureAwait(false);
            }
            SendResponse(context.Response, resp, settings);
        }
        catch (HttpListenerException) { }
        catch { }
    }

    private HttpRequest ConvertRequest(HttpListenerRequest request, HttpServerSettings settings)
    {
        var maxBody = settings?.MaxRequestBodySize ?? 0;
        string body;
        if (maxBody > 0)
        {
            // Cap difensivo anche per body chunked (senza Content-Length): mai leggere oltre
            // maxBody. La risposta 413 per il caso con Content-Length è già gestita nel loop.
            var limit = (int)Math.Min(maxBody, int.MaxValue - 1);
            var buffer = new byte[limit + 1];
            var read = 0;
            using (var input = request.InputStream)
            {
                while (read < buffer.Length)
                {
                    var n = input.Read(buffer, read, buffer.Length - read);
                    if (n <= 0) break;
                    read += n;
                }
            }
            body = Encoding.UTF8.GetString(buffer, 0, Math.Min(read, limit));
        }
        else
        {
            using var reader = new StreamReader(request.InputStream);
            body = reader.ReadToEnd();
        }
        return new HttpRequest
        {
            Method = request.HttpMethod,
            Path = request.Url?.AbsolutePath ?? "/",
            Body = body,
            Headers = request.Headers.AllKeys.ToDictionary(k => k ?? "", k => request.Headers[k] ?? ""),
            Query = (request.Url?.Query ?? "").TrimStart('?').Split('&')
                .Where(s => s.Contains('='))
                .Select(s => s.Split('=', 2))
                .ToDictionary(p => p[0], p => Uri.UnescapeDataString(p[1])),
            RemoteIpAddress = request.RemoteEndPoint?.Address?.ToString() ?? string.Empty,
        };
    }

    private HttpResponse Dispatch(long handle, HttpRequest req)
    {
        // Middleware (HTTP USE): in ordine di registrazione, solo quelli il cui prefisso combacia
        // col path richiesto ("*" = tutte le richieste). Il primo che risponde direttamente
        // (Continue=false, es. autenticazione fallita) ferma la catena qui — route/file statici
        // non vengono mai raggiunti per quella richiesta.
        if (_middlewares.TryGetValue(handle, out var middlewares))
        {
            foreach (var (pathPrefix, middleware) in middlewares)
            {
                if (pathPrefix != "*" && !req.Path.StartsWith(pathPrefix, StringComparison.OrdinalIgnoreCase))
                    continue;
                var (shouldContinue, response) = middleware(req);
                if (!shouldContinue)
                    return response ?? new HttpResponse { StatusCode = 500, Body = "500 Middleware non ha prodotto una risposta." };
            }
        }

        // Check routes
        if (_routes.TryGetValue(handle, out var routes))
        {
            foreach (var route in routes)
            {
                if (route.Method == req.Method && route.TryMatch(req.Path, out var routeParams))
                {
                    req.Params = routeParams;
                    return route.Handler(req);
                }
            }
        }

        // Check static files
        if (_staticDirs.TryGetValue(handle, out var mount) &&
            req.Path.StartsWith(mount.Prefix, StringComparison.Ordinal))
        {
            var relativePath = req.Path[mount.Prefix.Length..].TrimStart('/');
            var filePath = Path.Combine(mount.Directory, relativePath);
            if (File.Exists(filePath))
            {
                var ext = Path.GetExtension(filePath).ToLower();
                if (ext == ".bsp" && GlobalBspPageHandler != null)
                {
                    var bspResponse = GlobalBspPageHandler(filePath, req);
                    if (bspResponse != null) return bspResponse;
                }
                var contentType = ext switch
                {
                    ".html" => "text/html",
                    ".css" => "text/css",
                    ".js" => "application/javascript",
                    ".png" => "image/png",
                    ".jpg" or ".jpeg" => "image/jpeg",
                    ".gif" => "image/gif",
                    ".svg" => "image/svg+xml",
                    ".json" => "application/json",
                    _ => "application/octet-stream",
                };
                return new HttpResponse { Body = File.ReadAllText(filePath), ContentType = contentType };
            }
        }

        return new HttpResponse { StatusCode = 404, Body = "Not Found" };
    }

    private static void SendResponse(HttpListenerResponse response, HttpResponse resp, HttpServerSettings settings)
    {
        response.StatusCode = resp.StatusCode;
        response.ContentType = resp.ContentType;
        // Keep-Alive configurabile (HTTP CONFIG EnableKeepAlive); default true.
        response.KeepAlive = settings?.EnableKeepAlive ?? true;
        // Prima: gli header custom (resp.Headers) non venivano mai scritti sulla risposta HTTP
        // reale — silenziosamente persi. Scoperto mentre si collegava la persistenza sessione
        // BSP (Set-Cookie: GMBSESSION=...): il cookie veniva calcolato correttamente lato server
        // ma non avrebbe mai raggiunto il client, facendo sembrare "funzionante" un fix che in
        // realtà non persisteva nulla al primo giro reale.
        foreach (var (key, value) in resp.Headers)
            response.Headers[key] = value;
        // Header di sicurezza automatici (HTTP CONFIG SecurityHeaders) — non sovrascrivono
        // header già impostati esplicitamente dalla route/middleware (defense in depth ma
        // lascia spazio a configurazione più specifica).
        if (settings?.SecurityHeaders == true)
        {
            foreach (var (key, value) in CommonLib.Http.HttpSecurity.DefaultSecurityHeaders())
                if (response.Headers[key] == null)
                    response.Headers[key] = value;
        }
        // Corpo binario se presente, testo altrimenti (vedi WriteTlsResponseAsync).
        var outBytes = resp.BodyBytes
            ?? (resp.Body != null ? Encoding.UTF8.GetBytes(resp.Body) : null);
        if (outBytes != null)
        {
            response.ContentLength64 = outBytes.Length;
            response.OutputStream.Write(outBytes, 0, outBytes.Length);
        }
        response.OutputStream.Close();
    }

    // ================================================================
    public void Dispose()
    {
        foreach (var kv in _servers)
        {
            try { kv.Value.Stop(); } catch { }
            try { kv.Value.Close(); } catch { }
        }
        _client.Dispose();
    }
}
