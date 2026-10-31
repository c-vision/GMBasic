// FILE: CommonLib.Http.HttpClientHelper.cs

using System.Net;
using System.Net.Http.Headers;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json.Nodes;

/*
 * HttpClientHelper.cs: client HTTP centralizzato con pooling di connessioni e supporto cookie
 *
 * CLASSI:
 * - FetchResult: risultato di un'operazione FETCH (StatusCode, Body, BodyBytes, Headers, ErrorMessage)
 * - HttpClientHelper: client HTTP con PerformRequestAsync(url, options)
 */

namespace CommonLib.Http;

/// <summary>Risultato di un'operazione FETCH HTTP asincrona.
/// Contiene codice di stato, corpo (stringa e byte[]), header della risposta
/// e un eventuale messaggio di errore se la richiesta è fallita.</summary>
public sealed class FetchResult
{
    /// <summary>Codice di stato HTTP (200 per OK, 404 per Not Found, ecc.). 0 se errore.</summary>
    public int StatusCode { get; init; }
    public bool IsSuccess => StatusCode >= 200 && StatusCode < 300;

    /// <summary>Corpo della risposta come stringa. Vuoto se la richiesta è fallita.</summary>
    public string Body { get; init; } = string.Empty;

    /// <summary>Corpo della risposta come byte[], per dati binari.</summary>
    public byte[] BodyBytes { get; init; } = [];

    /// <summary>Dimensione in byte del corpo (BodyBytes.Length), come dichiarato dal server
    /// quando disponibile. Parità 4 backend: il runtime C semina la chiave "ContentLength"
    /// col conteggio byte del body (gmb_http_fetchresult_seed).</summary>
    public long ContentLength => BodyBytes.Length;

    /// <summary>Header della risposta come oggetto JSON.</summary>
    public JsonObject Headers { get; init; } = new();

    /// <summary>URL effettivo della risposta finale (dopo eventuali redirect seguiti). Uguale
    /// all'URL richiesto se non ci sono stati redirect.</summary>
    public string Url { get; init; } = string.Empty;

    /// <summary>Messaggio di errore se la richiesta è fallita. Stringa vuota se OK.</summary>
    public string ErrorMessage { get; init; } = string.Empty;

    /// <summary>
    /// Salva il corpo della risposta (BodyBytes) su file a blocchi, risparmiando memoria.
    /// </summary>
    /// <param name="path">Il percorso del file di destinazione.</param>
    public void SaveToFile(string path)
    {
        System.IO.File.WriteAllBytes(path, BodyBytes ?? System.Array.Empty<byte>());
    }
}

/// <summary>
/// Client HTTP centralizzato con pooling di connessioni e supporto cookie.
/// Utilizza HttpClient lazy e riutilizzabile (best practice .NET) per ridurre
/// il consumo di risorse. Supporta decommpressione automatica (gzip, deflate),
/// timeout configurabile, header custom e corpo stringa/binario.
/// </summary>
public sealed class HttpClientHelper
{
    /// <summary>HttpClient condiviso SENZA supporto cookie.
    /// Inizializzato lazy per ritardare la creazione delle risorse.</summary>
    private readonly Lazy<HttpClient> _sharedClient;

    /// <summary>HttpClient condiviso CON supporto cookie (CookieContainer).
    /// I cookie vengono mantenuti tra richieste successive.</summary>
    private readonly Lazy<HttpClient> _sharedCookieClient;

    /// <summary>Inizializza il client con pool di connessioni lazy.</summary>
    public HttpClientHelper()
    {
        _sharedClient = new Lazy<HttpClient>(() =>
        {
            var handler = new HttpClientHandler
            {
                AutomaticDecompression = DecompressionMethods.All,
                UseCookies = false
            };
            var client = new HttpClient(handler);
            client.DefaultRequestHeaders.UserAgent.ParseAdd(
                $"GM-BASIC/{typeof(HttpClientHelper).Assembly.GetName().Version}");
            return client;
        });

        _sharedCookieClient = new Lazy<HttpClient>(() =>
        {
            var cookieContainer = new CookieContainer();
            var handler = new HttpClientHandler
            {
                AutomaticDecompression = DecompressionMethods.All,
                UseCookies = true,
                CookieContainer = cookieContainer
            };
            var client = new HttpClient(handler);
            client.DefaultRequestHeaders.UserAgent.ParseAdd(
                $"GM-BASIC/{typeof(HttpClientHelper).Assembly.GetName().Version}");
            return client;
        });
    }

    /// <summary>Esegue una richiesta HTTP/HTTPS asincrona.</summary>
    /// <param name="url">L'URL completo della richiesta.</param>
    /// <param name="options">Opzioni: metodo, header, corpo, timeout, cookie, auth, proxy, certificato client, redirect, TLS.</param>
    /// <returns>FetchResult con status, corpo e header della risposta.</returns>
    public async Task<FetchResult> PerformRequestAsync(
        string url, HttpOptions options, CancellationToken cancellationToken = default)
    {
        // Le due HttpClient condivise lazy (fast path, nessuna opzione avanzata) bastano per il
        // caso comune. Proxy/certificato client/cookie-jar dedicato/redirect non standard/TLS
        // insicuro richiedono un handler configurato per QUESTA richiesta — costruito al volo,
        // non condiviso: sono opzioni potenzialmente diverse a ogni chiamata, un HttpClient
        // condiviso multi-richiesta con queste impostazioni fisse non avrebbe senso.
        var needsScopedHandler = options.CookieJar != null
            || !string.IsNullOrEmpty(options.ProxyUrl)
            || !string.IsNullOrEmpty(options.ClientCertificatePath)
            || options.AllowInsecureSSL
            || !options.AllowRedirects
            || options.MaxRedirects != 10
            || !string.IsNullOrEmpty(options.PinnedCertificateSha256);

        HttpClient client;
        SocketsHttpHandler? scopedHandler = null;
        if (needsScopedHandler)
        {
            scopedHandler = new SocketsHttpHandler
            {
                AutomaticDecompression = DecompressionMethods.All,
                UseCookies = options.CookieJar != null || options.UseCookies,
                CookieContainer = options.CookieJar?.Container ?? new CookieContainer(),
                AllowAutoRedirect = options.AllowRedirects,
                MaxAutomaticRedirections = Math.Max(1, options.MaxRedirects),
            };

            if (options.AllowInsecureSSL || !string.IsNullOrEmpty(options.ClientCertificatePath)
                || !string.IsNullOrEmpty(options.PinnedCertificateSha256))
            {
                var sslOptions = new SslClientAuthenticationOptions();
                if (!string.IsNullOrEmpty(options.PinnedCertificateSha256))
                {
                    // Certificate pinning: ignora la catena di fiducia del sistema (un pin
                    // esplicito E' la decisione di fiducia — indipendente da AllowInsecureSSL,
                    // vedi HttpOptions.PinnedCertificateSha256), accetta SOLO se l'impronta
                    // SHA-256 del certificato presentato corrisponde esattamente a quella attesa.
                    var expectedPin = NormalizeHexFingerprint(options.PinnedCertificateSha256);
                    sslOptions.RemoteCertificateValidationCallback = (_, cert, _, _) =>
                    {
                        if (cert == null) return false;
                        var actualPin = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(cert.GetRawCertData()));
                        return string.Equals(actualPin, expectedPin, StringComparison.OrdinalIgnoreCase);
                    };
                }
                else if (options.AllowInsecureSSL)
                {
                    sslOptions.RemoteCertificateValidationCallback = (_, _, _, _) => true;
                }
                if (!string.IsNullOrEmpty(options.ClientCertificatePath))
                {
                    var clientCert = string.IsNullOrEmpty(options.ClientCertificatePassword)
                        ? X509CertificateLoader.LoadPkcs12FromFile(options.ClientCertificatePath, null)
                        : X509CertificateLoader.LoadPkcs12FromFile(options.ClientCertificatePath, options.ClientCertificatePassword);
                    sslOptions.ClientCertificates = [clientCert];
                }
                scopedHandler.SslOptions = sslOptions;
            }

            if (!string.IsNullOrEmpty(options.ProxyUrl))
            {
                var proxy = new WebProxy(options.ProxyUrl);
                if (!string.IsNullOrEmpty(options.ProxyUsername))
                    proxy.Credentials = new NetworkCredential(options.ProxyUsername, options.ProxyPassword ?? "");
                scopedHandler.Proxy = proxy;
                scopedHandler.UseProxy = true;
            }

            client = new HttpClient(scopedHandler);
            // Con un preset browser attivo, lo User-Agent arriva dal preset stesso (applicato
            // a livello di richiesta sotto, insieme al resto dei suoi header nell'ordine giusto)
            // — impostarlo qui ANCHE a livello di client duplicherebbe l'header sul wire
            // (HttpClient combina DefaultRequestHeaders con quelli della richiesta, non li
            // sovrascrive, per header multi-valore come User-Agent).
            if (string.IsNullOrEmpty(options.BrowserPreset))
                client.DefaultRequestHeaders.UserAgent.ParseAdd(
                    $"GM-BASIC/{typeof(HttpClientHelper).Assembly.GetName().Version}");
        }
        else
        {
            client = options.UseCookies ? _sharedCookieClient.Value : _sharedClient.Value;
        }

        try
        {
            using var cts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            cts.CancelAfter(options.Timeout);
            using var requestMessage = new HttpRequestMessage(new HttpMethod(options.Method), url);

            // Basic/Bearer auth: solo se l'utente non ha già impostato Authorization
            // esplicitamente (Headers.Authorization ha sempre la precedenza — coerente col fatto
            // che era già possibile impostarlo a mano prima che queste opzioni dedicate
            // esistessero). Calcolato localmente (non scritto su options.Headers) per non mutare
            // un oggetto HttpOptions che il chiamante potrebbe riusare/condividere fra più FETCH
            // concorrenti.
            var authHeader = options.Headers.Authorization;
            if (string.IsNullOrEmpty(authHeader))
            {
                if (!string.IsNullOrEmpty(options.Username))
                {
                    var raw = $"{options.Username}:{options.Password ?? ""}";
                    authHeader = "Basic " + Convert.ToBase64String(Encoding.UTF8.GetBytes(raw));
                }
                else if (!string.IsNullOrEmpty(options.BearerToken))
                {
                    authHeader = "Bearer " + options.BearerToken;
                }
                if (!string.IsNullOrEmpty(authHeader))
                    requestMessage.Headers.TryAddWithoutValidation("Authorization", authHeader);
            }

            // Preset browser (simulazione): applicato PRIMA degli header espliciti, nell'ordine
            // esatto del preset — così un header con lo stesso nome in options.Headers (sotto)
            // sostituisce il valore del preset invece di aggiungersi ad esso, mantenendo la
            // precedenza "esplicito vince sempre" documentata su HttpOptions.BrowserPreset.
            foreach (var (name, value) in BrowserPresets.GetHeaders(options.BrowserPreset))
                requestMessage.Headers.TryAddWithoutValidation(name, value);

            // Imposta gli header (Content-Type escluso, va sul Content)
            foreach (var (key, value) in options.Headers.GetAll())
            {
                if (key.Equals("Content-Type", StringComparison.OrdinalIgnoreCase)) continue;
                if (key.Equals("Connection", StringComparison.OrdinalIgnoreCase) &&
                    value.Contains("close", StringComparison.OrdinalIgnoreCase))
                {
                    // TryAddWithoutValidation scrive l'header grezzo sul wire, ma non avvisa il
                    // pool di connessioni di HttpClient: senza impostare la property tipizzata
                    // ConnectionClose, il client crede comunque la connessione riutilizzabile e
                    // la ripesca dal pool per la richiesta successiva — che poi fallisce con "An
                    // error occurred while sending the request" perché il SERVER l'ha già chiusa
                    // (onora lui correttamente questo stesso header, vedi Gpm.Http.Server.cs).
                    requestMessage.Headers.ConnectionClose = true;
                    continue;
                }
                // Rimuove un eventuale valore gia' presente per lo stesso nome (es. dal preset
                // browser sopra) prima di aggiungere quello esplicito — TryAddWithoutValidation
                // da solo AGGIUNGEREBBE un secondo valore invece di sostituirlo (gli header HTTP
                // supportano valori multipli), producendo un header duplicato/concatenato invece
                // del semplice override "esplicito vince" documentato.
                requestMessage.Headers.Remove(key);
                requestMessage.Headers.TryAddWithoutValidation(key, value);
            }

            // Imposta il corpo della richiesta
            if (options.Body is not null)
            {
                requestMessage.Content = options.Body is byte[] bodyBytes
                    ? new ByteArrayContent(bodyBytes)
                    : new StringContent(options.Body.ToString() ?? "", Encoding.UTF8);

                var contentType = options.Headers.ContentType;
                if (!string.IsNullOrEmpty(contentType))
                {
                    // Il costruttore new MediaTypeHeaderValue(string) accetta solo un bare
                    // "type/subtype", senza parametri — un valore con "; boundary=..." (necessario
                    // per multipart/form-data) lo fa fallire con "invalid format". Parse() gestisce
                    // correttamente la sintassi completa RFC (tipo + parametri).
                    requestMessage.Content.Headers.ContentType = MediaTypeHeaderValue.Parse(contentType);
                }
            }

            using var response = await client.SendAsync(requestMessage, cts.Token).ConfigureAwait(false);

            var responseHeaders = new JsonObject();
            foreach (var header in response.Headers.Concat(response.Content.Headers))
                responseHeaders[header.Key] = string.Join(", ", header.Value);

            var responseBytes = await response.Content.ReadAsByteArrayAsync(cts.Token).ConfigureAwait(false);

            return new FetchResult
            {
                StatusCode = (int)response.StatusCode,
                BodyBytes = responseBytes,
                Body = Encoding.UTF8.GetString(responseBytes),
                Headers = responseHeaders,
                Url = response.RequestMessage?.RequestUri?.ToString() ?? url
            };
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            // La cancellazione del chiamante e' controllo di flusso del GPM: deve arrivare al
            // wait registration, che decide atomicamente se cancellazione o completamento ha
            // vinto. Convertirla in FetchResult(Error) risveglierebbe invece un task gia'
            // cancellato e renderebbe osservabile un completamento tardivo.
            throw;
        }
        catch (Exception ex)
        {
            var message = ex.InnerException is not null ? $"{ex.Message} ({ex.InnerException.Message})" : ex.Message;
            return new FetchResult { StatusCode = 0, ErrorMessage = message, Url = url };
        }
        finally
        {
            // L'handler/client scoped e' stato creato SOLO per questa richiesta (vedi
            // needsScopedHandler sopra) — va smaltito qui, a differenza dei due HttpClient
            // condivisi lazy (_sharedClient/_sharedCookieClient), che vivono per l'intera durata
            // del processo e non vanno mai Dispose-ati.
            if (scopedHandler != null)
            {
                client.Dispose();
                scopedHandler.Dispose();
            }
        }
    }

    /// <summary>Normalizza un'impronta esadecimale (rimuove ':'/spazi/trattini, uppercase) cosi'
    /// che "AA:BB:CC", "aa bb cc" e "AABBCC" siano tutti riconosciuti come lo stesso pin — stessi
    /// formati con cui i certificati vengono tipicamente copiati da un browser/openssl.</summary>
    private static string NormalizeHexFingerprint(string fingerprint) =>
        new string(fingerprint.Where(Uri.IsHexDigit).ToArray()).ToUpperInvariant();
}
