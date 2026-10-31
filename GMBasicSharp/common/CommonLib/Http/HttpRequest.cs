// FILE: CommonLib.Http.HttpRequest.cs

using System.Text.Json.Nodes;

/* 
 * HttpRequest.cs: richiesta HTTP in entrata o in uscita (Method, Path, Query, Params, Headers, Body)
 * 
 * PROPRIETÀ:
 * - Method, Path, Query, Params, Headers, Body, BodyBytes, RemoteIpAddress
 * - BodyJson: auto-parsed JSON body
 * - ParseMultipartForm(): parsifica il corpo come multipart/form-data
 * - ParseForm(): parsifica il corpo come application/x-www-form-urlencoded
 */

namespace CommonLib.Http;

/// <summary>
/// Rappresenta una richiesta HTTP in entrata (lato server) o in uscita (lato client).
/// Record immutabile con init-only properties. Passato alla subroutine di gestione BASIC
/// per ispezionare metodo, path, parametri, header e corpo della richiesta.
/// Supporta parsing multipart/form-data tramite <see cref="ParseMultipartForm"/>.
/// </summary>
public record HttpRequest
{
    /// <summary>Metodo HTTP della richiesta (GET, POST, PUT, DELETE, ecc.).</summary>
    public string Method { get; init; } = "GET";

    /// <summary>Percorso (path) dell'URL richiesto (es. "/users/123").</summary>
    public string Path { get; init; } = "/";

    /// <summary>Parametri della query string parsati come JSON (es. {"page": "1"}).</summary>
    public JsonObject Query { get; init; } = new();

    /// <summary>Raw query string (es. "page=1&amp;sort=asc"). Accessible from BASIC.</summary>
    public string QueryString { get; init; } = string.Empty;

    /// <summary>Parametri estratti dal pattern della route (es. /users/{id} → {"id": "123"}).</summary>
    public JsonObject Params { get; init; } = new();

    /// <summary>Header della richiesta come oggetto JSON (es. {"Content-Type": "text/html"}).</summary>
    public JsonObject Headers { get; init; } = new();

    /// <summary>Corpo (body) della richiesta come stringa. Vuoto se assente.</summary>
    public string Body { get; init; } = string.Empty;

    /// <summary>Corpo (body) della richiesta come array di byte, per dati binari.</summary>
    public byte[] BodyBytes { get; init; } = [];

    /// <summary>Indirizzo IP del client che ha effettuato la richiesta (lato server).</summary>
    public string RemoteIpAddress { get; init; } = string.Empty;

    /// <summary>Corpo JSON parsato automaticamente se Content-Type è application/json. Null altrimenti.</summary>
    public JsonObject? BodyJson { get; init; }

    /// <summary>ID della sessione estratta dal cookie, o null.</summary>
    public string? SessionId { get; init; }

    /// <summary>
    /// Parsa il corpo della richiesta come multipart/form-data.
    /// </summary>
    /// <returns>HttpPostedData con i dati parsati, o null se il Content-Type non è multipart.</returns>
    public HttpPostedData? ParseMultipartForm()
    {
        return RequestBodyParser.ParseMultipartData(this);
    }

    /// <summary>
    /// Parsa il corpo come application/x-www-form-urlencoded.
    /// Restituisce un JsonObject con i campi del form.
    /// </summary>
    public JsonObject? ParseForm()
    {
        var dict = RequestBodyParser.ParseFormData(Body);
        if (dict is null) return null;
        var jo = new JsonObject();
        foreach (var kv in dict) jo[kv.Key] = kv.Value;
        return jo;
    }

    /// <summary>
    /// Core UNICO di parsing del form body usato dai backend IL e PCode per seminare il
    /// keyword-scope FORM (e riempire FORM("chiave")): gate sul Content-Type
    /// application/x-www-form-urlencoded (mai per body JSON o GET), valori SEMPRE stringa.
    /// Delega a CommonLib.Http.RequestBodyParser.ParseFormDictionary (implementazione condivisa
    /// con il tipo HttpRequest del BRT — allineamento 2026-09-02, prima due copie divergenti).
    /// </summary>
    public Dictionary<string, object?> ParseFormDictionary()
    {
        var ct = Headers.TryGetPropertyValue("Content-Type", out var n) ? n?.ToString() : null;
        return RequestBodyParser.ParseFormDictionary(ct, Body);
    }
}
