// FILE: CommonLib.Http.HttpOptions.cs

using System.Text.Json.Nodes;

/* 
 * HttpOptions.cs: opzioni per richieste HTTP (metodo, header, corpo, timeout, cookie)
 * 
 * CLASSI:
 * - HttpHeaders: gestione tipizzata degli header HTTP (ContentType, Authorization, UserAgent)
 * - HttpOptions: opzioni richiesta (Method, Headers, Body, Timeout, UseCookies)
 */

namespace CommonLib.Http;

/// <summary>
/// Gestione tipizzata degli header HTTP per richieste e risposte.
/// Fornisce accesso rapido agli header comuni (ContentType, Authorization, UserAgent)
/// e un dizionario generico per header custom. Case-insensitive.
/// Usato da HttpOptions per configurare le richieste HTTP FETCH.
/// </summary>
public sealed class HttpHeaders
{
    /// <summary>Dizionario interno case-insensitive per tutti gli header.</summary>
    private readonly Dictionary<string, string> _customHeaders = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Header Content-Type (es. "application/json").</summary>
    public string ContentType
    {
        get => GetHeader("Content-Type");
        set => SetHeader("Content-Type", value);
    }

    /// <summary>Header Authorization (es. "Bearer token123").</summary>
    public string Authorization
    {
        get => GetHeader("Authorization");
        set => SetHeader("Authorization", value);
    }

    /// <summary>Header User-Agent (es. "GM-BASIC/1.0").</summary>
    public string UserAgent
    {
        get => GetHeader("User-Agent");
        set => SetHeader("User-Agent", value);
    }

    /// <summary>Imposta o aggiorna un header generico.</summary>
    public void SetHeader(string key, string value) => _customHeaders[key] = value;

    /// <summary>Recupera il valore di un header. Stringa vuota se non presente.</summary>
    public string GetHeader(string key) =>
        _customHeaders.TryGetValue(key, out var val) ? val : string.Empty;

    /// <summary>Restituisce tutti gli header come dizionario per l'invio HTTP.</summary>
    public IDictionary<string, string> GetAll() => _customHeaders;
}

/// <summary>
/// Rappresenta un set di opzioni per una richiesta HTTP/HTTPS,
/// esposto al runtime BASIC come tipo built-in (creabile con NEW HTTPOPTIONS).
/// Include metodo, header, corpo, timeout e gestione cookie.
/// </summary>
public sealed class HttpOptions
{
    /// <summary>Metodo HTTP (GET, POST, PUT, DELETE, ecc.). Default: "GET".</summary>
    public string Method { get; set; } = "GET";

    /// <summary>Header della richiesta (Content-Type, Authorization, ecc.).</summary>
    public HttpHeaders Headers { get; set; } = new();

    /// <summary>Corpo della richiesta: stringa (testo) o byte[] (binario).</summary>
    public object? Body { get; set; }

    /// <summary>Timeout in millisecondi. Default: 30000 (30s).</summary>
    public int Timeout { get; set; } = 30000;

    /// <summary>Se true, gestisce automaticamente i cookie di sessione tra richieste successive. Default: true.</summary>
    public bool UseCookies { get; set; } = true;

    /// <summary>Cookie jar dedicato per questa richiesta (sessione indipendente). Se null, usa il
    /// jar globale condiviso (comportamento di default, invariato) quando <see cref="UseCookies"/>
    /// è true.</summary>
    public CookieJar? CookieJar { get; set; }

    /// <summary>Nome utente per Basic Authentication. Se impostato insieme a <see cref="Password"/>
    /// (e <see cref="Headers"/>.Authorization non è già impostato esplicitamente), viene inviato
    /// automaticamente come header "Authorization: Basic base64(user:pass)".</summary>
    public string? Username { get; set; }

    /// <summary>Password per Basic Authentication. Vedi <see cref="Username"/>.</summary>
    public string? Password { get; set; }

    /// <summary>Token per Bearer Authentication. Se impostato (e Authorization non è già impostato
    /// esplicitamente), viene inviato come header "Authorization: Bearer {token}".</summary>
    public string? BearerToken { get; set; }

    /// <summary>URL del proxy HTTP/HTTPS da usare per questa richiesta (es. "http://host:porta").
    /// Se null, usa il proxy di sistema di default (.NET).</summary>
    public string? ProxyUrl { get; set; }

    /// <summary>Nome utente per l'autenticazione al proxy (opzionale).</summary>
    public string? ProxyUsername { get; set; }

    /// <summary>Password per l'autenticazione al proxy (opzionale).</summary>
    public string? ProxyPassword { get; set; }

    /// <summary>Percorso di un file di certificato client .pfx/.p12 per mutual TLS (opzionale).
    /// Stesso formato già usato da HTTP SERVER per il certificato server.</summary>
    public string? ClientCertificatePath { get; set; }

    /// <summary>Password del certificato client indicato da <see cref="ClientCertificatePath"/>.</summary>
    public string? ClientCertificatePassword { get; set; }

    /// <summary>Se true (default), segue automaticamente i redirect HTTP (3xx). Se false, la
    /// risposta 3xx viene restituita così com'è, senza seguire il redirect.</summary>
    public bool AllowRedirects { get; set; } = true;

    /// <summary>Numero massimo di redirect da seguire automaticamente. Default: 10.</summary>
    public int MaxRedirects { get; set; } = 10;

    /// <summary>Se true, disabilita la validazione del certificato TLS del server (accetta anche
    /// certificati self-signed/scaduti/non fidati). Default: false (validazione sempre attiva) —
    /// da usare solo in sviluppo/test, MAI in produzione verso endpoint non fidati.</summary>
    public bool AllowInsecureSSL { get; set; }

    /// <summary>Nome di un preset di header realistici da inviare come farebbe un browser reale
    /// (User-Agent/Accept/Accept-Language/Sec-Fetch-*/ecc., nell'ordine in cui quel browser li
    /// invia davvero — vedi <see cref="BrowserPresets"/>). Valori riconosciuti: "chrome",
    /// "firefox", "safari" (case-insensitive). Null/vuoto = nessun preset (comportamento
    /// invariato). Gli header espliciti in <see cref="Headers"/> hanno sempre la precedenza sui
    /// valori del preset per lo stesso nome.</summary>
    public string? BrowserPreset { get; set; }

    /// <summary>Se impostato, il certificato TLS del server DEVE avere questa esatta impronta
    /// SHA-256 (formato esadecimale, case-insensitive, con o senza separatori ':'/spazi) —
    /// certificate pinning: la richiesta fallisce se il certificato presentato non corrisponde,
    /// anche se altrimenti valido/fidato dalla catena di certificazione del sistema. Indipendente
    /// da <see cref="AllowInsecureSSL"/> (i due si possono combinare: accetta SOLO quel
    /// certificato specifico, anche se self-signed).</summary>
    public string? PinnedCertificateSha256 { get; set; }
}

/// <summary>
/// Cookie jar dedicato, per mantenere sessioni HTTP indipendenti fra loro (es. simulare più
/// utenti/tab di un browser in parallelo). Esposto al runtime BASIC come tipo built-in
/// (creabile con NEW COOKIEJAR). Wrapper sottile su System.Net.CookieContainer.
/// </summary>
public sealed class CookieJar
{
    public System.Net.CookieContainer Container { get; } = new();

    /// <summary>Rimuove tutti i cookie memorizzati in questo jar.</summary>
    public void Clear()
    {
        // CookieContainer non espone un Clear() diretto: GetAllCookies() (.NET 4.7.2+) enumera
        // ogni cookie di ogni dominio noto — marcarli Expired li rimuove effettivamente dal jar.
        foreach (System.Net.Cookie cookie in Container.GetAllCookies())
            cookie.Expired = true;
    }
}
