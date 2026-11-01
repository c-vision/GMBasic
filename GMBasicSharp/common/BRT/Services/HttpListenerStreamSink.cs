// FILE: Basic.Runtime.Services.HttpListenerStreamSink.cs
// Sink di streaming per il server HttpListener (gamba IL): qui il framing chunked lo fa
// HttpListener (SendChunked), il sink scrive solo il corpo progressivo sull'OutputStream.
// Il protocollo (status/header/Write/Flush/EndStream) resta in PCodeHttpResponse: cambia
// solo il trasporto (CommonLib.Http.HttpStreamSink).

using System.Collections.Generic;
using System.Net;

namespace Basic.Runtime;

/// <summary>
/// Streaming su <see cref="HttpListenerResponse"/>: status e header vengono impostati sulla
/// risposta (SendChunked), il corpo va scritto subito sull'OutputStream. HttpListener manda
/// l'header al PRIMO write, quindi un handler che streamma davvero non aspetta la fine.
/// </summary>
public sealed class HttpListenerStreamSink : CommonLib.Http.HttpStreamSink
{
    private readonly HttpListenerResponse _response;

    public HttpListenerStreamSink(HttpListenerResponse response) => _response = response;

    protected override void BeginCore(int status, string contentType, IReadOnlyList<KeyValuePair<string, string>> headers)
    {
        _response.StatusCode = status;
        _response.ContentType = contentType;
        // 1.1 chunked: HttpListener calcola da sé Content-Length/chunk terminale alla chiusura.
        _response.SendChunked = true;
        _response.KeepAlive = false;
        foreach (var kv in headers)
        {
            if (IsFramingHeader(kv.Key)) continue;
            if (kv.Key.Equals("Set-Cookie", System.StringComparison.OrdinalIgnoreCase))
            {
                _response.Headers.Add("Set-Cookie", kv.Value);
                continue;
            }
            try { _response.Headers[kv.Key] = kv.Value; } catch { /* header riservato dal listener */ }
        }
    }

    protected override void WriteCore(byte[] data, int offset, int count)
    {
        _response.OutputStream.Write(data, offset, count);
        _response.OutputStream.Flush();
    }

    /// <summary>HttpListener chiude lui il corpo chunked: qui serve solo il flush finale.</summary>
    protected override void EndCore() => _response.OutputStream.Flush();
}
