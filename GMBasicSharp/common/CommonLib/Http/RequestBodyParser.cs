// FILE: CommonLib.Http.RequestBodyParser.cs

using HttpMultipartParser;
using System.Text;

/* 
 * RequestBodyParser.cs: parsing del corpo di richieste HTTP (form URL-encoded e multipart/form-data)
 * 
 * FUNZIONI:
 * - ParseFormData(body): parsifica dati form URL-encoded
 * - ParseMultipartData(request): parsifica richiesta multipart/form-data
 */

namespace CommonLib.Http;

/// <summary>
/// Fornisce metodi statici per il parsing del corpo di una richiesta HTTP.
/// Supporta application/x-www-form-urlencoded (ParseFormData)
/// e multipart/form-data (ParseMultipartData) tramite la libreria HttpMultipartParser.
/// </summary>
public static class RequestBodyParser
{
    /// <summary>Parsa dati form URL-encoded dal corpo di una richiesta (es. "name=John&age=30").</summary>
    /// <param name="body">La stringa del corpo della richiesta.</param>
    /// <returns>Dizionario chiave-valore con i campi del form.</returns>
    public static Dictionary<string, string> ParseFormData(string body)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (string.IsNullOrEmpty(body)) return result;

        foreach (var pair in body.Split('&'))
        {
            var parts = pair.Split('=', 2);
            if (parts.Length == 2)
                result[Uri.UnescapeDataString(parts[0])] = Uri.UnescapeDataString(parts[1]);
        }
        return result;
    }

    /// <summary>
    /// Core UNICO di parsing del form body (allineamento IL/PCode 2026-09-02): gate sul
    /// Content-Type application/x-www-form-urlencoded (mai per body JSON o GET) e valori SEMPRE
    /// stringa. Chiamato dai due tipi di HttpRequest (BRT e CommonLib.Http) — prima ognuno aveva
    /// la propria copia locale divergente.
    /// </summary>
    public static Dictionary<string, object?> ParseFormDictionary(string? contentType, string? body)
    {
        var result = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        if (string.IsNullOrEmpty(contentType) ||
            !contentType.StartsWith("application/x-www-form-urlencoded", StringComparison.OrdinalIgnoreCase))
            return result;
        foreach (var kv in ParseFormData(body ?? "")) result[kv.Key] = kv.Value;
        return result;
    }

    /// <summary>Parsa una richiesta multipart/form-data usando HttpMultipartParser.</summary>
    /// <param name="request">L'oggetto HttpRequest contenente il body da parsare.</param>
    /// <returns>HttpPostedData con campi e file, o null se Content-Type non è multipart.</returns>
    public static HttpPostedData? ParseMultipartData(HttpRequest request)
    {
        if (!request.Headers.TryGetPropertyValue("Content-Type", out var contentTypeNode))
            return null;

        var contentType = contentTypeNode?.GetValue<string>() ?? string.Empty;
        if (!contentType.StartsWith("multipart/form-data", StringComparison.OrdinalIgnoreCase))
            return null;

        // HttpMultipartParser vuole solo il VALORE del boundary (es.
        // "----abc123"), non l'intero header Content-Type — passarglielo
        // per intero fa fallire la ricerca del delimitatore nel body con
        // "Could not find expected boundary".
        var boundaryIndex = contentType.IndexOf("boundary=", StringComparison.OrdinalIgnoreCase);
        if (boundaryIndex < 0) return null;
        var boundary = contentType[(boundaryIndex + "boundary=".Length)..].Trim('"');

        using var stream = new MemoryStream(request.BodyBytes);
        var parser = MultipartFormDataParser.Parse(stream, boundary, Encoding.UTF8);

        var formFields = parser.Parameters
            .Select(p => new FormField(p.Name, p.Data))
            .ToList();

        var fileFields = parser.Files
            .Select(f =>
            {
                // FileField.Data = byte CRUDI (non lo Stream del section): parità coi backend C
                // (gmb_net_http_parse_multipart) e con lo schema ARRAY OF BYTE.
                // MultipartSection.Data è il sub-stream della parte già posizionato a 0.
                byte[] bytes;
                using (var ms = new MemoryStream())
                {
                    f.Data.CopyTo(ms);
                    bytes = ms.ToArray();
                }
                return new FileField(f.Name, f.FileName, f.ContentType, bytes);
            })
            .ToList();

        return new HttpPostedData(formFields, fileFields);
    }
}
