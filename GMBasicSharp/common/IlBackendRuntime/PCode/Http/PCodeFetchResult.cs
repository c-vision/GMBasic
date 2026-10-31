// FILE: Basic.IlBackend.PCode.Http.PCodeFetchResult.cs
// Wrapper P-CODE-friendly attorno a CommonLib.Http.FetchResult (risultato di FETCH), esposto al
// codice BASIC via OP_LOAD_FIELD (StatusCode/Body/Headers/ContentLength/Url/IsSuccess) e
// OP_CALL_MEMBER (SaveToFile/GetBodyBytes) — stesso pattern di dispatch gia' usato per
// PCodeHttpResponse.

namespace Basic.IlBackend.PCode.Http;

/// <summary>
/// Risultato di una FETCH, restituito a BASIC come FETCHRESULT. Immutabile: costruito una sola
/// volta quando la richiesta HTTP completa (vedi PCodeProcessor.OP_FETCH).
/// </summary>
internal sealed class PCodeFetchResult
{
    private readonly CommonLib.Http.FetchResult _result;

    /// <summary>URL della richiesta originale (CommonLib.Http.FetchResult non lo porta con se').</summary>
    public string Url { get; }

    public PCodeFetchResult(CommonLib.Http.FetchResult result, string url)
    {
        _result = result;
        Url = url;
    }

    public int StatusCode => _result.StatusCode;
    public string Body => _result.Body;
    public byte[] BodyBytes => _result.BodyBytes;
    public bool IsSuccess => _result.IsSuccess;
    public string ErrorMessage => _result.ErrorMessage;

    /// <summary>Header della risposta come Dictionary BASIC-accessibile (stesso pattern di
    /// BuildRequestDict per le richieste HTTP server-side).</summary>
    public Dictionary<string, object?> HeadersAsDict()
    {
        var dict = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var kv in _result.Headers)
            dict[kv.Key] = kv.Value?.ToString();
        return dict;
    }

    public void SaveToFile(string path) => _result.SaveToFile(path);
}
