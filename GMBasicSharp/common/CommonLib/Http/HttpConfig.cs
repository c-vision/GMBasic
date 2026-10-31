// FILE: CommonLib.Http.HttpConfig.cs

/* 
 * HttpConfig.cs: opzioni configurabili per il server HTTP (porta, TLS, timeout, rate limiting)
 * 
 * PROPRIETÀ:
 * - Port, CertificatePath, CertificatePassword, RequestTimeoutMs, RateLimitRequestsPerMinute, RateLimitBurstSize
 * - MaxRequestBodySize, DefaultRequestTimeout, EnableKeepAlive
 */

namespace CommonLib.Http;

/// <summary>
/// Rappresenta un set di opzioni configurabili per il server HTTP,
/// esposto al runtime BASIC come tipo di dato built-in (creabile con NEW HTTPCONFIG).
/// Controlla porta, certificato TLS, timeout richieste e rate limiting anti-abuso.
/// </summary>
public sealed class HttpConfig
{
    /// <summary>Porta TCP su cui il server si mette in ascolto. Default: 8080.</summary>
    public int Port { get; set; } = 8080;

    /// <summary>Percorso del file del certificato (es. .pfx) per abilitare HTTPS. Null = HTTP.</summary>
    public string? CertificatePath { get; set; }

    /// <summary>Password per il file del certificato, se protetto.</summary>
    public string? CertificatePassword { get; set; }

    /// <summary>Timeout per le richieste HTTP in millisecondi. Default: 30000 (30s).</summary>
    public int RequestTimeoutMs { get; set; } = 30000;

    /// <summary>Numero massimo di richieste al minuto per singolo indirizzo IP. Default: 60.</summary>
    public int RateLimitRequestsPerMinute { get; set; } = 60;

    /// <summary>Dimensione massima del burst permesso dal rate limiter (token bucket). Default: 120.</summary>
    public int RateLimitBurstSize { get; set; } = 120;

    /// <summary>Memoizzazione della RESA delle pagine BSP, in millisecondi (0 = disattivata).
    /// Il costo per richiesta di una pagina è l'esecuzione (~ms), non l'HTTP: con molti client in
    /// polling la stessa pagina viene rieseguita identica. Impostando &gt; 0 il corpo viene riusato
    /// per quella finestra e servito con ETag/304. Valore consigliato per una dashboard: 500-1500.
    /// Vale solo per le gambe managed (le gambe C non servono BSP).</summary>
    public int BspCacheMs { get; set; }

    /// <summary>Dimensione massima consentita per il body di una richiesta in byte. Default: 10 MB.</summary>
    public long MaxRequestBodySize { get; set; } = 10 * 1024 * 1024;

    /// <summary>Alias per RequestTimeoutMs. Default: 30000 (30s).</summary>
    public int DefaultRequestTimeout { get => RequestTimeoutMs; set => RequestTimeoutMs = value; }

    /// <summary>Abilita il supporto Keep-Alive per connessioni HTTP persistenti. Default: true.</summary>
    public bool EnableKeepAlive { get; set; } = true;

    /// <summary>Header di sicurezza automatici (HSTS/CSP/X-Frame-Options/X-Content-Type-Options/
    /// Referrer-Policy/X-Permitted-Cross-Domain-Policies) su ogni risposta. Default: false.</summary>
    public bool SecurityHeaders { get; set; }

    /// <summary>Allow-list IP (IP singoli o CIDR IPv4, separati da virgola). Vuota = tutti.</summary>
    public string AllowedIPs { get; set; } = "";

    /// <summary>Block-list IP (IP singoli o CIDR IPv4, separati da virgola). Vuota = nessuno.</summary>
    public string BlockedIPs { get; set; } = "";

    /// <summary>Sessioni server-side su route HTTP (REQ("Session")) — opt-in, default false.</summary>
    public bool Sessions { get; set; }

    /// <summary>Percorso del file SQLite per la persistenza sessioni (vuoto = in-memory).</summary>
    public string SessionFile { get; set; } = "";
}
