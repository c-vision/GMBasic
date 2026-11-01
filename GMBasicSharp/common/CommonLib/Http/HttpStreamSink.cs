// FILE: CommonLib.Http.HttpStreamSink.cs
// Sink di una risposta HTTP in STREAMING (chunked): il trasporto (socket grezzo HTTP/1.1,
// SslStream del server TLS, HttpListenerResponse) implementa Begin/Write/End; la logica di
// che cosa scrivere (status, header, corpo progressivo) resta UNA sola:
// PCodeHttpResponse.StartStreaming/WriteStream/FlushStream/CompleteStream.
//
// Perché serve l'astrazione: il framing chunked è identico su tutte le gambe native
// (gmb_network.c: gmb_http_response_stream_*) e gestito dal client HTTP sulle gambe managed;
// quello che cambia è SOLO dove finiscono i byte (NetworkStream, SslStream, OutputStream di
// HttpListener). Un unico punto per il protocollo, un sink per trasporto.

using System.Collections.Generic;
using System.IO;
using System.Text;

namespace CommonLib.Http;

/// <summary>
/// Trasporto di una risposta in streaming. Lo stato (Begin/End una volta sola) è gestito qui,
/// così ogni trasporto scrive solo i propri byte.
/// </summary>
public abstract class HttpStreamSink
{
    private int _begun;
    private int _ended;

    /// <summary>True dopo il primo <see cref="Begin"/> (la risposta è già partita: header inviati).</summary>
    public bool Begun => System.Threading.Volatile.Read(ref _begun) != 0;

    /// <summary>True dopo <see cref="End"/> (framing chiuso).</summary>
    public bool Ended => System.Threading.Volatile.Read(ref _ended) != 0;

    /// <summary>Header di framing: li decide il trasporto, replicarli rompe il protocollo.</summary>
    protected static bool IsFramingHeader(string name) =>
        name.Equals("Content-Length", System.StringComparison.OrdinalIgnoreCase)
        || name.Equals("Transfer-Encoding", System.StringComparison.OrdinalIgnoreCase)
        || name.Equals("Connection", System.StringComparison.OrdinalIgnoreCase)
        || name.Equals("Content-Type", System.StringComparison.OrdinalIgnoreCase)
        || name.Equals("Cache-Control", System.StringComparison.OrdinalIgnoreCase);

    public void Begin(int status, string contentType, IReadOnlyList<KeyValuePair<string, string>> headers)
    {
        if (System.Threading.Interlocked.Exchange(ref _begun, 1) != 0) return;
        try { BeginCore(status, contentType, headers); }
        catch { /* client andato durante l'header: lo streaming resta "iniziato" (niente retry) */ }
    }

    public void Write(byte[] data, int offset, int count)
    {
        if (count <= 0) return;
        try { WriteCore(data, offset, count); }
        catch { /* client andato: le scritture successive sono no-op lato socket */ }
    }

    public void End()
    {
        if (System.Threading.Interlocked.Exchange(ref _ended, 1) != 0) return;
        try { EndCore(); }
        catch { }
    }

    protected abstract void BeginCore(int status, string contentType, IReadOnlyList<KeyValuePair<string, string>> headers);
    protected abstract void WriteCore(byte[] data, int offset, int count);
    protected abstract void EndCore();

    /// <summary>Frase di stato per la status-line (il corpo resta invariato).</summary>
    protected static string ReasonPhrase(int status) => status switch
    {
        200 => "OK",
        201 => "Created",
        202 => "Accepted",
        204 => "No Content",
        206 => "Partial Content",
        301 => "Moved Permanently",
        302 => "Found",
        304 => "Not Modified",
        400 => "Bad Request",
        401 => "Unauthorized",
        403 => "Forbidden",
        404 => "Not Found",
        405 => "Method Not Allowed",
        409 => "Conflict",
        413 => "Payload Too Large",
        418 => "I'm a teapot",
        429 => "Too Many Requests",
        500 => "Internal Server Error",
        501 => "Not Implemented",
        502 => "Bad Gateway",
        503 => "Service Unavailable",
        504 => "Gateway Timeout",
        _ => "OK",
    };
}

/// <summary>
/// Streaming su un flusso HTTP/1.1 GREZZO (socket del server PCode, SslStream del server TLS):
/// il framing chunked lo scrive questo sink. Usato sia dal server TcpListener sia da quello TLS,
/// che quindi non duplicano il protocollo.
/// </summary>
public sealed class ChunkedStreamSink : HttpStreamSink
{    private readonly Stream _stream;
    private readonly bool _keepAlive;
    private readonly object _lock = new();

    public ChunkedStreamSink(Stream stream, bool keepAlive = false)
    {
        _stream = stream;
        _keepAlive = keepAlive;
    }

    protected override void BeginCore(int status, string contentType, IReadOnlyList<KeyValuePair<string, string>> headers)
    {
        var sb = new StringBuilder();
        sb.Append("HTTP/1.1 ").Append(status).Append(' ').Append(ReasonPhrase(status)).Append("\r\n");
        sb.Append("Content-Type: ").Append(contentType).Append("\r\n");
        foreach (var kv in headers)
        {
            if (IsFramingHeader(kv.Key)) continue;
            sb.Append(kv.Key).Append(": ").Append(kv.Value).Append("\r\n");
        }
        sb.Append("Transfer-Encoding: chunked\r\n");
        sb.Append("Cache-Control: no-store\r\n");
        sb.Append("Connection: ").Append(_keepAlive ? "keep-alive" : "close").Append("\r\n\r\n");
        var head = Encoding.ASCII.GetBytes(sb.ToString());
        lock (_lock)
        {
            _stream.Write(head, 0, head.Length);
            _stream.Flush();
        }
    }

    protected override void WriteCore(byte[] data, int offset, int count)
    {
        var hdr = Encoding.ASCII.GetBytes($"{count:x}\r\n");
        lock (_lock)
        {
            _stream.Write(hdr, 0, hdr.Length);
            _stream.Write(data, offset, count);
            _stream.Write([(byte)'\r', (byte)'\n'], 0, 2);
            _stream.Flush();
        }
    }

    protected override void EndCore()
    {
        lock (_lock)
        {
            _stream.Write([(byte)'0', (byte)'\r', (byte)'\n', (byte)'\r', (byte)'\n'], 0, 5);
            _stream.Flush();
        }
    }
}

/// <summary>
/// Sink "ambient" del thread che esegue il DISPATCH di una richiesta.
/// Serve alle gambe in cui la risposta non è creabile dal server (gamba IL: la
/// <c>PCodeHttpResponse</c> nasce dentro il runtime del handler): il server dichiara qui il
/// trasporto della connessione, chi costruisce la risposta lo adotta. Vive in CommonLib perché
/// BRT (server) e IlBackendRuntime (risposta) sono su due livelli diversi e solo CommonLib è
/// visibile a entrambi — nessuna dipendenza inversa.
/// </summary>
public static class HttpStreamAmbient
{
    [System.ThreadStatic] private static HttpStreamSink? _current;

    /// <summary>Trasporto corrente del thread (null fuori da un dispatch HTTP).</summary>
    public static HttpStreamSink? Current => _current;

    /// <summary>Rende <paramref name="sink"/> il trasporto corrente del thread fino al Dispose.</summary>
    public static System.IDisposable Use(HttpStreamSink? sink)
    {
        var prev = _current;
        _current = sink;
        return new Scope(prev);
    }

    private sealed class Scope : System.IDisposable
    {
        private readonly HttpStreamSink? _prev;
        public Scope(HttpStreamSink? prev) => _prev = prev;
        public void Dispose() => _current = _prev;
    }
}
