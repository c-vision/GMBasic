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

    /* ── Streaming (chunked) ────────────────────────────────────────────────────
     * Una risposta può essere inviata PROGRESSIVAMENTE: `Stream(contentType)` scrive subito
     * status+header con `Transfer-Encoding: chunked` e da quel momento `Write`/`Flush` mandano
     * i byte al client mentre il codice BASIC continua a girare (SSE, pagine lunghe, progress).
     * Il TRASPORTO (socket grezzo, SslStream, OutputStream di HttpListener) è un
     * HttpStreamSink: la logica della risposta è una sola per tutte le gambe.
     * Con lo streaming attivo NON si applicano Content-Length, gzip, ETag e render cache
     * (sono semantiche di una risposta completa): chi streamma non li riceve. */
    private CommonLib.Http.HttpStreamSink? _sink;
    private bool _streaming;
    private readonly MemoryStream _pending = new();
    public bool Streaming { get { lock (_lock) return _streaming; } }

    /* Sink "ambient": le gambe che NON possono agganciare il sink all'oggetto (la gamba IL crea
     * la risposta dentro il runtime del handler, non nel server) lo impostano con UseSink sul
     * thread che esegue il dispatch; il costruttore lo adotta. Il trasporto resta in CommonLib. */
    public PCodeHttpResponse() => _sink = CommonLib.Http.HttpStreamAmbient.Current;

    /// <summary>Rende <paramref name="sink"/> il trasporto delle risposte create su questo
    /// thread finché lo scope non è chiuso (gamba IL: HttpListener/TLS).</summary>
    public static IDisposable UseSink(CommonLib.Http.HttpStreamSink? sink)
        => CommonLib.Http.HttpStreamAmbient.Use(sink);

    /* Range della RICHIESTA (per res.SendFile): il download di un archivio deve poter
     * riprendere da dove si era interrotto. Lo deposita il dispatch, come il sink di
     * streaming — la risposta non vede la richiesta. */
    private string? _requestRange;

    internal void AttachRequestInfo(string? rangeHeader) { _requestRange = rangeHeader; }

    /// <summary>Aggancia il trasporto della connessione (il loop lo fa prima del dispatch).</summary>
    internal void AttachSink(CommonLib.Http.HttpStreamSink sink) => _sink = sink;

    /// <summary>Aggancia il socket della connessione (HTTP/1.1 grezzo, framing chunked).</summary>
    internal void AttachStream(Stream sink, CancellationToken ct, bool keepAlive)
        => _sink = new CommonLib.Http.ChunkedStreamSink(sink, keepAlive);

    private void StreamFlushChunk()
    {
        byte[] payload;
        lock (_lock)
        {
            if (_pending.Length == 0) return;
            payload = _pending.ToArray();
            _pending.SetLength(0);
        }
        _sink?.Write(payload, 0, payload.Length);
    }

    /// <summary>
    /// Avvia la risposta in streaming: status + header scritti ORA, poi ogni Write/Flush manda
    /// i byte. Content-Type di default: text/plain; charset=utf-8. Senza trasporto (uso fuori
    /// da un server) la risposta resta bufferizzata: Write accoda nel Body.
    /// </summary>
    public void StartStreaming(string? contentType)
    {
        CommonLib.Http.HttpStreamSink? sink;
        string ctype;
        List<KeyValuePair<string, string>> hdrs;
        int status;
        lock (_lock)
        {
            if (_streaming || Sent || _sink == null) return;
            if (contentType != null && !Headers.ContainsKey("Content-Type")) Headers["Content-Type"] = contentType;
            if (!Headers.ContainsKey("Content-Type")) Headers["Content-Type"] = "text/plain; charset=utf-8";
            _streaming = true;
            /* NON si imposta Sent: per lo streaming "risposta pronta" = handler FINITO
             * (il dispatch attende la fine del task, altrimenti il server chiuderebbe il
             * chunked mentre l'handler continua a scrivere). Sent lo mette CompleteStream. */
            sink = _sink;
            ctype = Headers["Content-Type"];
            status = StatusCode;
            hdrs = new List<KeyValuePair<string, string>>(Headers.Count + RawSetCookieHeaders.Count);
            foreach (var kv in Headers) hdrs.Add(kv);
            foreach (var c in RawSetCookieHeaders) hdrs.Add(new KeyValuePair<string, string>("Set-Cookie", c));
        }
        sink.Begin(status, ctype, hdrs);
    }

    /// <summary>Accoda testo alla risposta in streaming (se non è iniziato, lo avvia).</summary>
    public void WriteStream(string text)
    {
        if (!Streaming) StartStreaming(null);
        var bytes = Encoding.UTF8.GetBytes(text ?? "");
        if (!Streaming) { AppendBodyFallback(bytes); return; }
        lock (_lock) _pending.Write(bytes, 0, bytes.Length);
        if (_pending.Length >= 8192) StreamFlushChunk();
    }

    public void WriteStreamBytes(byte[] data)
    {
        if (!Streaming) StartStreaming(null);
        if (!Streaming) { AppendBodyFallback(data); return; }
        lock (_lock) _pending.Write(data, 0, data.Length);
        if (_pending.Length >= 8192) StreamFlushChunk();
    }

    /// <summary>Nessun trasporto (risposta costruita fuori da un server): i byte finiscono nel
    /// Body bufferizzato, così un handler non perde silenziosamente l'output.</summary>
    private void AppendBodyFallback(byte[] data)
    {
        lock (_lock)
        {
            var merged = new byte[Body.Length + data.Length];
            Buffer.BlockCopy(Body, 0, merged, 0, Body.Length);
            Buffer.BlockCopy(data, 0, merged, Body.Length, data.Length);
            Body = merged;
        }
    }

    /// <summary>Manda ORA al client quello che è in coda (senza chiudere la risposta).</summary>
    public void FlushStream()
    {
        if (!Streaming) { StartStreaming(null); return; }
        StreamFlushChunk();
    }

    /// <summary>Chiude lo streaming: flush del residuo + chunk terminale, poi marca la
    /// risposta come inviata (sblocca il dispatch).</summary>
    public void CompleteStream()
    {
        if (!Streaming) return;
        StreamFlushChunk();
        _sink?.End();
        Action? cb;
        lock (_lock)
        {
            if (Sent) return;
            Sent = true;
            cb = _onSent;
            _onSent = null;
        }
        cb?.Invoke();
    }

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

    public void Send(string text)
    {
        if (Streaming) { WriteStream(text); return; }
        Complete(Encoding.UTF8.GetBytes(text), "text/plain; charset=utf-8");
    }

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
        long total;
        try
        {
            total = new FileInfo(path).Length;
            bytes = File.ReadAllBytes(path);
        }
        catch (Exception)
        {
            /* Messaggio FISSO (parità bit-exact con il runtime C gmb_http_response_send_file e
             * con il VM/C inline): il testo dell'eccezione .NET divergerebbe fra le gambe. */
            Complete(Encoding.UTF8.GetBytes("500 SendFile: file non leggibile"), "text/plain; charset=utf-8", 500);
            return;
        }
        var ct = contentType ?? GuessContentType(path);
        // Accept-Ranges sempre: dice al client che il download è riprendibile.
        SetHeader("Accept-Ranges", "bytes");
        // Range (RFC 9110): stessa logica managed dei file statici (CommonLib.Http.HttpRange).
        var kind = CommonLib.Http.HttpRange.Parse(_requestRange, total, out var start, out var end);
        if (kind == CommonLib.Http.HttpRangeKind.Unsatisfiable)
        {
            SetHeader("Content-Range", $"bytes */{total}");
            Complete([], "text/plain; charset=utf-8", 416);
            return;
        }
        if (kind == CommonLib.Http.HttpRangeKind.Valid)
        {
            SetHeader("Content-Range", $"bytes {start}-{end}/{total}");
            var slice = new byte[end - start + 1];
            Array.Copy(bytes, start, slice, 0, slice.Length);
            Complete(slice, ct, 206);
            return;
        }
        Complete(bytes, ct);
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
        /* Corpo canonico = C# HttpService (404 => "Not Found"): prima "404 Not Found",
         * divergente da IL/VM-C/C99 (e dallo stesso HttpService). */
        Complete(Encoding.UTF8.GetBytes("Not Found"), "text/plain; charset=utf-8", 404);
    }
}
