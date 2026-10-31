// FILE: Basic.Connectors.ConnectorOpenApi
// OpenAPI auto-gen F4 — genera un connettore da openapi.json (SYSTEM.txt §12, §34 F4)

using System.Text.Json;

namespace Basic.Connectors;

public static class ConnectorOpenApi
{
    public static string CreateFromUrl(string connectorName, string openApiUrl)
    {
        if (string.IsNullOrWhiteSpace(openApiUrl)) throw new ArgumentException("openApiUrl required");
        var helper = new CommonLib.Http.HttpClientHelper();
        var opts = new CommonLib.Http.HttpOptions { Method = "GET" };
        var res = helper.PerformRequestAsync(openApiUrl, opts).GetAwaiter().GetResult();
        if (!res.IsSuccess) throw new InvalidOperationException($"OpenAPI fetch failed {res.StatusCode}: {res.ErrorMessage} {res.Body}");

        using var doc = JsonDocument.Parse(res.Body ?? "{}");
        var tools = new List<ConnectorTool>();
        if (doc.RootElement.TryGetProperty("paths", out var paths) && paths.ValueKind == JsonValueKind.Object)
        {
            foreach (var pathProp in paths.EnumerateObject())
            {
                var path = pathProp.Name;
                if (pathProp.Value.ValueKind != JsonValueKind.Object) continue;
                foreach (var methodProp in pathProp.Value.EnumerateObject())
                {
                    var method = methodProp.Name.ToLowerInvariant();
                    if (method is not ("get" or "post" or "put" or "patch" or "delete")) continue;
                    var opId = methodProp.Value.TryGetProperty("operationId", out var oid) ? oid.GetString() ?? $"{method}_{path.Trim('/').Replace("/", "_")}" : $"{method}_{path.Trim('/').Replace("/", "_")}";
                    var summary = methodProp.Value.TryGetProperty("summary", out var s) ? s.GetString() ?? "" : "";
                    var desc = string.IsNullOrWhiteSpace(summary) ? $"{method.ToUpperInvariant()} {path}" : summary;
                    // Input schema dal requestBody/properties (semplificato)
                    var schema = "{}";
                    if (methodProp.Value.TryGetProperty("requestBody", out var rb)) schema = rb.GetRawText();
                    tools.Add(new ConnectorTool(opId, desc, schema));
                }
            }
        }

        var def = new ConnectorDefinition(
            Name: connectorName,
            Kind: "openapi",
            DisplayName: connectorName,
            Version: "1.0",
            AuthKind: "none",
            Capabilities: tools.Select(t => t.Name).ToList(),
            Tools: tools.Select(t => t.Name).ToList(),
            Description: $"OpenAPI connector from {openApiUrl} ({tools.Count} ops)",
            Url: openApiUrl
        );

        if (!ConnectorRegistry.Instance.Register(def))
            ConnectorRegistry.Instance.Get(connectorName); // already exists

        if (tools.Count > 0) ConnectorRegistry.Instance.RememberTools(connectorName, tools);

        return JsonSerializer.Serialize(new { connector = connectorName, url = openApiUrl, tools = tools.Select(t => new { t.Name, t.Description }), count = tools.Count }, new JsonSerializerOptions { WriteIndented = true });
    }

    public static string Dispatch(string connectorName, string tool, string argsJson)
    {
        var def = ConnectorRegistry.Instance.Get(connectorName) ?? throw new ArgumentException($"Connector '{connectorName}' not found");
        var baseUrl = def.Url ?? throw new InvalidOperationException($"OpenAPI connector '{connectorName}' has no Url (openApiUrl)");
        // Costruisci URL base dall'openApiUrl (es. https://petstore.swagger.io/v2/swagger.json → https://petstore.swagger.io/v2)
        var uri = new Uri(baseUrl);
        var apiBase = $"{uri.Scheme}://{uri.Host}{(uri.IsDefaultPort ? "" : $":{uri.Port}")}";
        // Per semplicità, chiama {apiBase}/{tool} con args come query/body
        var helper = new CommonLib.Http.HttpClientHelper();
        var opts = new CommonLib.Http.HttpOptions { Method = "POST", Body = argsJson };
        opts.Headers.SetHeader("Content-Type", "application/json");
        var url = $"{apiBase}/{tool}";
        var res = helper.PerformRequestAsync(url, opts).GetAwaiter().GetResult();
        return JsonSerializer.Serialize(new { connector = connectorName, tool, url, statusCode = res.StatusCode, body = res.Body ?? "", error = res.ErrorMessage }, new JsonSerializerOptions { WriteIndented = true });
    }
}
