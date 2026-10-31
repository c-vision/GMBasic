// FILE: CommonLib.Http.RequestParser.cs

using System.Buffers;
using System.Text;
using System.Text.Json.Nodes;

/* 
 * RequestParser.cs: parser HTTP/1.1 ad alte prestazioni basato su macchina a stati (request line, header, body)
 * 
 * ENUM/CLASSI:
 * - ParsingState: stati del parser (RequestLine, Headers, Body, Complete, Invalid)
 * - HttpRequestParser: parser incrementale con Parse(ref buffer) e GetCompletedRequest()
 */

namespace CommonLib.Http;

/// <summary>Stati del parser HTTP durante l'analisi incrementale di una richiesta.</summary>
public enum ParsingState
{
    /// <summary>In attesa della request line (es. "GET /path HTTP/1.1").</summary>
    RequestLine,
    /// <summary>In attesa degli header HTTP (fino al CRLF doppio).</summary>
    Headers,
    /// <summary>In attesa del corpo (in base al Content-Length).</summary>
    Body,
    /// <summary>Richiesta completamente parsata e pronta.</summary>
    Complete,
    /// <summary>Richiesta malformata — parser in stato di errore.</summary>
    Invalid
}

/// <summary>
/// Parser HTTP/1.1 ad alte prestazioni basato su macchina a stati.
/// Progettato per leggere dati in modo incrementale da un ReadOnlySequence&lt;byte&gt;
/// (buffer di rete), senza allocazioni intermedie. Supporta parsing di request line,
/// header (fino al CRLF doppio) e body con Content-Length.
/// </summary>
public sealed class HttpRequestParser
{
    private const byte CR = (byte)'\r';
    private const byte LF = (byte)'\n';
    private static readonly byte[] NewLine = [CR, LF];
    private static readonly byte[] HeadersEnd = [CR, LF, CR, LF];

    /// <summary>Stato corrente del parser nella macchina a stati.</summary>
    public ParsingState State { get; private set; } = ParsingState.RequestLine;

    // Campi di accumulo durante il parsing incrementale
    private string _method = string.Empty;
    private string _path = string.Empty;
    private string _queryString = string.Empty;
    private JsonObject _query = new();
    private JsonObject _headers = new();
    private List<byte> _bodyBuffer = new();
    private long _contentLength;

    /// <summary>Fornisce un buffer di byte al parser per l'analisi incrementale.</summary>
    /// <param name="buffer">Il buffer contenente i nuovi dati dalla rete. Viene avanzato man mano che i dati vengono consumati.</param>
    /// <returns>True se una richiesta completa è stata parsata, False se servono più dati.</returns>
    public bool Parse(ref ReadOnlySequence<byte> buffer)
    {
        if (State is ParsingState.RequestLine or ParsingState.Headers)
        {
            if (!TryParseHeaders(ref buffer)) return false;

            if (_headers.TryGetPropertyValue("Content-Length", out var lengthNode) &&
                long.TryParse(lengthNode?.GetValue<string>(), out _contentLength))
            {
                State = _contentLength > 0 ? ParsingState.Body : ParsingState.Complete;
            }
            else
            {
                State = ParsingState.Complete; // No body — richiesta completa
            }
        }

        if (State == ParsingState.Body)
        {
            if (!TryParseBody(ref buffer)) return false;
            State = ParsingState.Complete;
        }

        return State == ParsingState.Complete;
    }

    /// <summary>Ottiene la richiesta parsata e resetta lo stato per la prossima richiesta (keep-alive).</summary>
    public HttpRequest GetCompletedRequest()
    {
        var request = new HttpRequest
        {
            Method = _method,
            Path = _path,
            Query = _query,
            QueryString = _queryString,
            Headers = _headers,
            BodyBytes = _bodyBuffer.ToArray(),
            Body = Encoding.UTF8.GetString(_bodyBuffer.ToArray())
        };
        Reset();
        return request;
    }

    /// <summary>Parsa la request line e tutti gli header fino al CRLF doppio.</summary>
    private bool TryParseHeaders(ref ReadOnlySequence<byte> buffer)
    {
        var reader = new SequenceReader<byte>(buffer);
        if (!reader.TryReadTo(out ReadOnlySequence<byte> headersBlock, HeadersEnd))
            return false; // Non abbiamo ancora tutti gli header

        var headersReader = new SequenceReader<byte>(headersBlock);

        // Request Line: "GET /path HTTP/1.1"
        if (headersReader.TryReadTo(out ReadOnlySpan<byte> requestLineSpan, NewLine))
        {
            var lineStr = Encoding.ASCII.GetString(requestLineSpan);
            var parts = lineStr.Split(' ', 3);
            if (parts.Length < 2) { State = ParsingState.Invalid; return false; }

            _method = parts[0];
            var fullPath = parts[1];
            var queryIndex = fullPath.IndexOf('?');
            if (queryIndex != -1)
            {
                _path = fullPath[..queryIndex];
                _queryString = fullPath[(queryIndex + 1)..];
                ParseQueryString(_queryString);
            }
            else
            {
                _path = fullPath;
            }
        }

        // Header: righe "Chiave: Valore"
        while (headersReader.TryReadTo(out ReadOnlySpan<byte> headerLineSpan, NewLine))
        {
            if (headerLineSpan.Length == 0) continue;
            var headerStr = Encoding.ASCII.GetString(headerLineSpan);
            var parts = headerStr.Split(':', 2);
            if (parts.Length == 2)
                _headers[parts[0].Trim()] = parts[1].Trim();
        }

        // L'ULTIMA riga di header in headersBlock non ha un proprio \r\n finale:
        // quel \r\n e' stato consumato come prima meta' del delimitatore
        // HeadersEnd (\r\n\r\n) che separa gli header dal body, quindi TryReadTo
        // non riesce mai a trovarne il terminatore e la riga resta nel reader
        // come dati "non letti". Senza questo, l'ultimo header inviato dal
        // client (es. Content-Length o Content-Type, a seconda dell'ordine)
        // viene silenziosamente perso.
        if (headersReader.Remaining > 0)
        {
            var lastLineBytes = headersReader.UnreadSequence.ToArray();
            if (lastLineBytes.Length > 0)
            {
                var headerStr = Encoding.ASCII.GetString(lastLineBytes);
                var parts = headerStr.Split(':', 2);
                if (parts.Length == 2)
                    _headers[parts[0].Trim()] = parts[1].Trim();
            }
        }

        buffer = buffer.Slice(reader.Position);
        return true;
    }

    /// <summary>Accumula il corpo della richiesta fino a raggiungere Content-Length.</summary>
    private bool TryParseBody(ref ReadOnlySequence<byte> buffer)
    {
        var bytesNeeded = _contentLength - _bodyBuffer.Count;
        var bytesAvailable = buffer.Length;
        var bytesToConsume = (int)Math.Min(bytesNeeded, bytesAvailable);

        if (bytesToConsume > 0)
        {
            _bodyBuffer.AddRange(buffer.Slice(0, bytesToConsume).ToArray());
            buffer = buffer.Slice(bytesToConsume);
        }

        return _bodyBuffer.Count >= _contentLength;
    }

    /// <summary>Parsa la query string in un JsonObject (es. "?page=1&sort=asc").</summary>
    private void ParseQueryString(string queryString)
    {
        if (string.IsNullOrEmpty(queryString)) return;
        _query = new JsonObject();
        foreach (var pair in queryString.Split('&'))
        {
            var parts = pair.Split('=', 2);
            if (parts.Length > 0 && !string.IsNullOrEmpty(parts[0]))
                _query[Uri.UnescapeDataString(parts[0])] =
                    parts.Length > 1 ? Uri.UnescapeDataString(parts[1]) : string.Empty;
        }
    }

    /// <summary>Resetta il parser per gestire una nuova richiesta (keep-alive HTTP).</summary>
    public void Reset()
    {
        State = ParsingState.RequestLine;
        _method = string.Empty;
        _path = string.Empty;
        _queryString = string.Empty;
        _query = new JsonObject();
        _headers = new JsonObject();
        _bodyBuffer.Clear();
        _contentLength = 0;
    }
}
