// FILE: Basic.Connectors.ConnectorHttp
// HTTP connector F1 — get/post/put/patch/delete con auth/headers/cookies/JSON/XML/multipart/retry (SYSTEM.txt §12)

using System.Text.Json;

namespace Basic.Connectors;

public static class ConnectorHttp
{
    public static string Dispatch(string tool, string argsJson)
    {
        var args = TryParseArgs(argsJson);
        return tool.ToLowerInvariant() switch
        {
            "get" => DoRequest("GET", GetArg(args, "url") ?? throw new ArgumentException("url required"), args),
            "post" => DoRequest("POST", GetArg(args, "url") ?? throw new ArgumentException("url required"), args),
            "put" => DoRequest("PUT", GetArg(args, "url") ?? throw new ArgumentException("url required"), args),
            "patch" => DoRequest("PATCH", GetArg(args, "url") ?? throw new ArgumentException("url required"), args),
            "delete" => DoRequest("DELETE", GetArg(args, "url") ?? throw new ArgumentException("url required"), args),
            "fetch" => DoRequest(GetArg(args, "method") ?? "GET", GetArg(args, "url") ?? throw new ArgumentException("url required"), args),
            _ => throw new ArgumentException($"HTTP tool '{tool}' unknown (get/post/put/patch/delete/fetch)")
        };
    }

    private static string DoRequest(string method, string url, Dictionary<string, string> args)
    {
        var helper = new CommonLib.Http.HttpClientHelper();
        var opts = new CommonLib.Http.HttpOptions { Method = method.ToUpperInvariant() };

        // Body / contentType
        if (TryGetArg(args, "body") is string body) opts.Body = body;
        if (TryGetArg(args, "contentType") is string ct) opts.Headers.SetHeader("Content-Type", ct);
        else if (TryGetArg(args, "content_type") is string ct2) opts.Headers.SetHeader("Content-Type", ct2);

        // Headers
        if (TryGetArg(args, "headers") is string hdrJson)
        {
            try
            {
                using var doc = JsonDocument.Parse(hdrJson);
                if (doc.RootElement.ValueKind == JsonValueKind.Object)
                    foreach (var p in doc.RootElement.EnumerateObject())
                        opts.Headers.SetHeader(p.Name, p.Value.GetString() ?? p.Value.GetRawText());
            }
            catch { }
        }
        // Auth: Bearer token da args o da ConnectorAuth (per-connector)
        if (TryGetArg(args, "bearerToken") is string bearer) opts.Headers.SetHeader("Authorization", $"Bearer {bearer}");
        else if (TryGetArg(args, "token") is string tok) opts.Headers.SetHeader("Authorization", $"Bearer {tok}");
        else if (TryGetArg(args, "api_key") is string ak) opts.Headers.SetHeader("Authorization", $"Bearer {ak}");

        // Timeout
        if (TryGetArg(args, "timeout") is string t && int.TryParse(t, out var ms)) opts.Timeout = ms;

        // OAuth per-connector: se l'url è di un host noto (gmail, github), prova a iniettare Bearer da ConnectorAuth
        // L'argomento "connector" può essere passato per risolvere il token (es. gmail)
        if (TryGetArg(args, "connector") is string conn)
        {
            var hdr = ConnectorAuth.GetAuthorizationHeader(conn);
            if (!string.IsNullOrWhiteSpace(hdr)) opts.Headers.SetHeader("Authorization", hdr);
        }

        var result = helper.PerformRequestAsync(url, opts).GetAwaiter().GetResult();
        var resp = new
        {
            url, method,
            statusCode = result.StatusCode,
            isSuccess = result.IsSuccess,
            body = result.Body ?? "",
            headers = result.Headers,
            error = result.ErrorMessage
        };
        return JsonSerializer.Serialize(resp, new JsonSerializerOptions { WriteIndented = true });
    }

    private static Dictionary<string, string> TryParseArgs(string json)
    {
        var d = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (string.IsNullOrWhiteSpace(json)) return d;
        try
        {
            using var doc = JsonDocument.Parse(json);
            if (doc.RootElement.ValueKind != JsonValueKind.Object) return d;
            foreach (var p in doc.RootElement.EnumerateObject())
                d[p.Name] = p.Value.ValueKind == JsonValueKind.String ? p.Value.GetString() ?? "" : p.Value.GetRawText().Trim('"');
        }
        catch { }
        return d;
    }
    private static string? GetArg(Dictionary<string, string> d, string k) => d.TryGetValue(k, out var v) ? v : null;
    private static string? TryGetArg(Dictionary<string, string> d, string k) => d.TryGetValue(k, out var v) ? v : null;
}
