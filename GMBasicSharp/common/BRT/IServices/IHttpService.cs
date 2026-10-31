// FILE: Basic.Runtime.IServices.IHttpService.cs

namespace Basic.Runtime;

/// <summary>Servizio HTTP per il BASIC Runtime.</summary>
public interface IHttpService
{
    // === Client ===
    string HttpGet(string url);
    string HttpPost(string url, string body, string contentType);
    string HttpPut(string url, string body, string contentType);
    string HttpDelete(string url);
    byte[] HttpDownload(string url);
    string HttpUpload(string url, string filePath, string fieldName);

    // === Server ===
    long HttpServerStart(int port);
    /// <summary>Avvia un server HTTPS su IL (TcpListener + SslStream, certificato .pfx).
    /// Usato quando HTTP CONFIG .CertificatePath è impostato prima di HTTP SERVER.</summary>
    long HttpServerStartTls(int port, string certPath, string certPassword);
    void HttpServerStop(long handle);
    void HttpAddRoute(long handle, string method, string path, Func<HttpRequest, HttpResponse> handler);
    void HttpAddStatic(long handle, string prefix, string directory);
    /// <summary>Registra un middleware per un prefisso di path ("*" per tutte le richieste).
    /// Il delegate riceve la richiesta e ritorna (true, null) se deve continuare la catena
    /// (l'equivalente BASIC di `HTTP USE NEXT`), oppure (false, risposta) se ha gestito la
    /// richiesta direttamente (short-circuit — es. un middleware di autenticazione che nega
    /// l'accesso). Firma volutamente limitata ai tipi di questo assembly (HttpRequest/
    /// HttpResponse, non un tipo P-Code/IlBackend): BRT sta SOTTO IlBackend nel grafo delle
    /// dipendenze, non può referenziarne i tipi — l'eventuale oggetto risposta "vivo" usato
    /// per il dispatch dinamico BASIC (res.Send/SendJSON/ecc.) resta interno al chiamante
    /// (IlRuntime.InvokeHttpMiddlewareHandler), già convertito in HttpResponse qui.</summary>
    void HttpAddMiddleware(long handle, string pathPrefix, Func<HttpRequest, (bool Continue, HttpResponse? Response)> middleware);

    /// <summary>Registra una route WebSocket (upgrade HTTP). Se il client manda una richiesta con
    /// `Upgrade: websocket` (case-insensitive) su un path che combacia, HttpService esegue
    /// l'handshake 101 (HttpListenerContext.AcceptWebSocketAsync) e invoca il handler col
    /// <see cref="System.Net.WebSockets.WebSocket"/> già accettato; la sessione (loop di ricezione,
    /// send, chiusura) resta compito del chiamante (IlBackend → IlWebSocketHandle). Path con
    /// match esatto o `*` finale, coerente con HttpAddRoute. Una richiesta NON di upgrade su una
    /// route WebSocket cade nel dispatch normale (404).</summary>
    void HttpAddWebSocketRoute(long handle, string path, Func<HttpRequest, System.Net.WebSockets.WebSocket, Task> handler);

    /// <summary>Applica le impostazioni di servizio a un server HTTP già avviato (HTTP CONFIG):
    /// timeout per richiesta, dimensione massima del body, rate-limit (token bucket) e
    /// keep-alive della connessione. Valori 0 = non limitare / default. La porta è decisa da
    /// HttpServerStart e il TLS NON è supportato da HttpListener (il percorso HTTPS resta la
    /// PCodeVM/PCodeHttpServer): quelle parti di HTTP CONFIG si applicano solo lì.</summary>
    void HttpConfigure(long handle, HttpServerSettings settings);
}

/// <summary>Impostazioni di servizio applicabili a un server HTTP (HTTP CONFIG). Valori 0 =
/// non limitare / default; le proprietà non usate da HttpListener (es. porta/certificato TLS)
/// sono gestite altrove.</summary>
public sealed class HttpServerSettings
{
    /// <summary>Timeout per singola richiesta (ms). 0 = nessun timeout.</summary>
    public int RequestTimeoutMs { get; set; }
    /// <summary>Dimensione massima del body di una richiesta (byte). 0 = illimitato.</summary>
    public long MaxRequestBodySize { get; set; }
    /// <summary>Rate-limit: richieste consentite per minuto. 0 = nessun limite.</summary>
    public int RateLimitRequestsPerMinute { get; set; }
    /// <summary>Rate-limit: dimensione del burst (token bucket). 0 = default 1.</summary>
    public int RateLimitBurstSize { get; set; }
    /// <summary>Keep-Alive della connessione (header Connection: keep-alive). Default true.</summary>
    public bool EnableKeepAlive { get; set; } = true;
    /// <summary>Header di sicurezza automatici (HSTS/CSP/X-Frame-Options/X-Content-Type-Options/
    /// Referrer-Policy/X-Permitted-Cross-Domain-Policies) su ogni risposta. Default false.</summary>
    public bool SecurityHeaders { get; set; }
    /// <summary>Allow-list IP (IP singoli o CIDR IPv4, separati da virgola). Vuota = tutti.</summary>
    public string AllowedIPs { get; set; } = "";
    /// <summary>Block-list IP (IP singoli o CIDR IPv4, separati da virgola). Vuota = nessuno.</summary>
    public string BlockedIPs { get; set; } = "";
    /// <summary>Percorso del certificato TLS (.pfx). Non vuoto = server HTTPS su IL.</summary>
    public string CertificatePath { get; set; } = "";
    /// <summary>Password del certificato TLS.</summary>
    public string CertificatePassword { get; set; } = "";
    /// <summary>Sessioni server-side su route HTTP (REQ("Session")) — opt-in, default off.</summary>
    public bool Sessions { get; set; }
    /// <summary>Percorso del file SQLite per la persistenza sessioni (vuoto = in-memory).</summary>
    public string SessionFile { get; set; } = "";
}


/// <summary>Richiesta HTTP minimale.</summary>
public class HttpRequest
{
    public string Method { get; set; } = "GET";
    public string Path { get; set; } = "/";
    public string? Body { get; set; }
    public Dictionary<string, string> Headers { get; set; } = new();
    public Dictionary<string, string> Query { get; set; } = new();

    /// <summary>Query string GREZZA della richiesta ("a=1&amp;b=2", senza '?'). In BASIC:
    /// <c>req.QueryString</c>. Il PCode la popolava, il backend IL no: un handler che ricostruisce
    /// un URL (proxy /api/* della webapp) perdeva i parametri. Parità fra le gambe.</summary>
    public string QueryString { get; set; } = string.Empty;

    /// <summary>Parametri estratti dal path template della route che ha fatto match (es.
    /// "/download/{name}/{version}" → Params["name"]/Params["version"]). Vuoto per route senza
    /// placeholder o quando il dispatch avviene fuori da HttpService.Dispatch (es. richieste
    /// dirette a HttpAddStatic).</summary>
    public Dictionary<string, string> Params { get; set; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Indirizzo IP del client che ha effettuato la richiesta (lato server).
    /// Popolato dal listener HTTP; vuoto se non disponibile. In BASIC: <c>req.RemoteIp</c>.</summary>
    public string RemoteIpAddress { get; set; } = string.Empty;

    /// <summary>Dati sessione per-request (REQ("Session")) — JsonObject VIVO dello store, popolato
    /// dal server quando HTTP CONFIG .Sessions = TRUE. Null = sessioni disattivate.</summary>
    public object? SessionData { get; set; }

    /// <summary>Set-Cookie GMBSESSION da aggiungere alla risposta se la sessione è nuova.</summary>
    public string? SessionSetCookie { get; set; }
}

/// <summary>Risposta HTTP minimale.</summary>
public class HttpResponse
{
    public int StatusCode { get; set; } = 200;
    public string? Body { get; set; }

    /// <summary>Corpo BINARIO grezzo. Se valorizzato, gli scrittori HTTP mandano questi byte
    /// e il Content-Length e' la loro lunghezza; `Body` resta la vista testuale per chi legge la
    /// risposta come stringa. Serve per le risposte non testuali (res.SendFile, ZIP del registry,
    /// immagini, PDF): passarle solo da `Body` le distruggerebbe, perche' una stringa non puo'
    /// contenere byte arbitrari (ogni sequenza non UTF-8 diventerebbe U+FFFD).</summary>
    public byte[]? BodyBytes { get; set; }

    public string ContentType { get; set; } = "text/plain";
    public Dictionary<string, string> Headers { get; set; } = new();
}
