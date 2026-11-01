// FILE: Basic.IlBackend.PCode.Http.PCodeHttpResponse.cs
// Handle di risposta HTTP esposto al codice BASIC via OP_CALL_MEMBER (vedi PCodeProcessor.cs).
// Non un Dictionary come il vecchio prototipo GMBasic: una classe dedicata con metodi nativi
// (Send/SendJson/SendFile/SetStatus/SetHeader/SetCookie/Redirect), stesso pattern di dispatch
// gia' usato per IList/IDictionary in OP_CALL_MEMBER.

using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Basic.IlBackend.PCode.Http;

/// <summary>
/// System.Text.Json non sa nulla di <see cref="PCodeProcessor.DateValue"/> (il wrapper interno
/// per NOW()/date letterali nella P-CODE VM, che porta solo un double OADate) — senza questo
/// converter, JsonSerializer.Serialize lo trattava come un oggetto POCO qualunque, serializzando
/// la sua unica proprieta' pubblica cosi' com'e': <c>{"OADate": 46237.06...}</c> invece del valore
/// data/ora leggibile che ci si aspetterebbe in un body JSON (bug reale, trovato confrontando
/// l'output P-CODE contro l'AST executor per Http/HttpServer.bas: l'AST usa System.DateTime
/// nativo con un case dedicato DateTime → stringa ISO 8601 in Gpm.Helpers.cs, la P-CODE VM no).
/// Stessa scelta di formato ("o", round-trippabile) per coerenza diretta con quel case AST.
/// </summary>
internal sealed class PCodeDateValueJsonConverter : JsonConverter<PCodeProcessor.DateValue>
{
    public override PCodeProcessor.DateValue Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
        => new(DateTime.Parse(reader.GetString() ?? "", CultureInfo.InvariantCulture).ToOADate());

    public override void Write(Utf8JsonWriter writer, PCodeProcessor.DateValue value, JsonSerializerOptions options)
    {
        // DateTime.FromOADate ritorna sempre Kind=Unspecified (l'OADate e' un numero di giorni
        // "ingenuo", nessuna informazione di fuso oraria sopravvive al round-trip) — con "o" su un
        // DateTime Unspecified, ToString omette del tutto l'offset UTC finale (differenza di
        // formato pura rispetto all'AST, che serializza System.DateTime.Now nativo, Kind=Local,
        // offset sempre presente). Le date/ore in questa VM sono sempre in orario locale (vedi
        // NOW() → DateTime.Now.ToOADate()), quindi SpecifyKind(Local) qui e' corretto, non un
        // workaround — ripristina l'offset nel formato, non solo l'aspetto cosmetico.
        var local = DateTime.SpecifyKind(DateTime.FromOADate(value.OADate), DateTimeKind.Local);
        writer.WriteStringValue(local.ToString("o", CultureInfo.InvariantCulture));
    }
}

/// <summary>
/// Risposta HTTP mutabile associata a una richiesta in corso. Send*/Redirect sono idempotenti
/// (come HttpResponse.SendResponse nell'AST: la prima chiamata vince, le successive sono no-op)
/// e completano l'attesa del loop di connessione tramite il callback registrato con OnSent.
/// </summary>
internal sealed class PCodeHttpResponse
{
    private readonly object _lock = new();
    private Action? _onSent;

    public int StatusCode { get; private set; } = 200;
    public Dictionary<string, string> Headers { get; } = new(StringComparer.OrdinalIgnoreCase);
    public List<string> RawSetCookieHeaders { get; } = new();
    public byte[] Body { get; private set; } = [];
    public bool Sent { get; private set; }

    /// <summary>
    /// Registra il callback da invocare quando la risposta viene inviata. Se e' gia' stata
    /// inviata (il codice BASIC ha chiamato Send* prima che il loop di connessione arrivasse a
    /// registrarsi), invoca subito — evita la finestra di race "Sent diventa true fra il
    /// controllo e la registrazione" con lo stesso pattern try-then-park del resto della VM.
    /// </summary>
    internal void OnSent(Action callback)
    {
        lock (_lock)
        {
            if (Sent) { callback(); return; }
            _onSent = callback;
        }
    }

    private void Complete(byte[] body, string? contentType, int? statusCode = null)
    {
        Action? cb;
        lock (_lock)
        {
            if (Sent) return;
            Body = body;
            if (contentType != null && !Headers.ContainsKey("Content-Type")) Headers["Content-Type"] = contentType;
            if (statusCode.HasValue) StatusCode = statusCode.Value;
            Sent = true;
            cb = _onSent;
            _onSent = null;
        }
        cb?.Invoke();
    }

    public void SetStatus(int code) { lock (_lock) { if (!Sent) StatusCode = code; } }

    public void SetHeader(string name, string value) { lock (_lock) { if (!Sent) Headers[name] = value; } }

    public void SetCookie(string name, string value, string? path = null, bool httpOnly = true, bool secure = false)
    {
        var sb = new StringBuilder();
        sb.Append(name).Append('=').Append(value);
        sb.Append("; Path=").Append(string.IsNullOrEmpty(path) ? "/" : path);
        if (httpOnly) sb.Append("; HttpOnly");
        if (secure) sb.Append("; Secure");
        lock (_lock) { if (!Sent) RawSetCookieHeaders.Add(sb.ToString()); }
    }

    public void Send(string text) => Complete(Encoding.UTF8.GetBytes(text), "text/plain; charset=utf-8");

    public void SendBytes(byte[] data, string contentType = "application/octet-stream") => Complete(data, contentType);

    /// <summary>res.SendFile(path [, contentType]) — invia il contenuto di un file.
    /// Era DICHIARATO nello schema del tipo (StdLib.Http.cs) ma non implementato su nessun
    /// backend: la chiamata era un no-op silenzioso (200 con corpo vuoto). È il caso di
    /// PackageServer.bas HandleDownload, che fa res.SendFile(tmp) dopo COMPRESSION.ZIP: il
    /// registry rispondeva 200 con 0 byte e il client non poteva installare nessun pacchetto.
    /// Il file e' letto SUBITO (non in modo lazy): cosi' chi lo cancella subito dopo
    /// (KILL tmp) non svuota la risposta.</summary>
    public void SendFile(string path, string? contentType = null)
    {
        byte[] bytes;
        try { bytes = File.ReadAllBytes(path); }
        catch (Exception)
        {
            /* Messaggio FISSO (parità bit-exact con il runtime C gmb_http_response_send_file e
             * con il VM/C inline): il testo dell'eccezione .NET divergerebbe fra le gambe. */
            Complete(Encoding.UTF8.GetBytes("500 SendFile: file non leggibile"), "text/plain; charset=utf-8", 500);
            return;
        }
        Complete(bytes, contentType ?? GuessContentType(path));
    }

    /// <summary>Content-type minimo per estensione (ZIP del registry, HTML delle BSP, ...).</summary>
    internal static string GuessContentType(string path)
    {
        var ext = Path.GetExtension(path).ToLowerInvariant();
        return ext switch
        {
            ".zip" => "application/zip",
            ".json" => "application/json",
            ".html" or ".htm" => "text/html; charset=utf-8",
            ".txt" => "text/plain; charset=utf-8",
            ".css" => "text/css; charset=utf-8",
            ".js" => "application/javascript",
            ".png" => "image/png",
            ".jpg" or ".jpeg" => "image/jpeg",
            ".gif" => "image/gif",
            ".svg" => "image/svg+xml",
            _ => "application/octet-stream",
        };
    }

    private static readonly JsonSerializerOptions JsonOptions = new() { Converters = { new PCodeDateValueJsonConverter() } };

    /* Normalizza come JSON.STRINGIFY (IlRuntime.NormalizeForJson): cosi' un datetime
     * (System.DateTime su IL o PCode.DateValue) esce come stringa ISO-8601 identica, invece di
     * "" (DateTime crudo) / "o con offset" (DateValue). Formato unico: yyyy-MM-ddTHH:mm:ss. */
    public void SendJson(object? value) => Complete(JsonSerializer.SerializeToUtf8Bytes(Basic.IlBackend.IlRuntime.NormalizeForJson(value), JsonOptions), "application/json");

    public void Redirect(string url, int statusCode = 302)
    {
        lock (_lock) { if (!Sent) Headers["Location"] = url; }
        Complete([], null, statusCode);
    }

    /// <summary>Invocato dal loop di connessione (Fase A) se nessun codice BASIC invia mai una
    /// risposta entro il timeout configurato — coerente con la deviazione intenzionale rispetto
    /// all'AST (che resta appeso per sempre) documentata nel piano.</summary>
    internal void CompleteAsTimeout()
    {
        Complete(Encoding.UTF8.GetBytes("504 Gateway Timeout"), "text/plain; charset=utf-8", 504);
    }

    internal void CompleteAsServerError(string message)
    {
        Complete(Encoding.UTF8.GetBytes(message), "text/plain; charset=utf-8", 500);
    }

    internal void CompleteAsNotFound()
    {
        Complete(Encoding.UTF8.GetBytes("404 Not Found"), "text/plain; charset=utf-8", 404);
    }
}
