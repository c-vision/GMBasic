// FILE: Basic.Connectors.ConnectorGenericHttp
// Fallback HTTP per i 40+ connettori T1-T4 non ancora con handler dedicato (sheets/docs/notion/linear/jira/discord/telegram/teams/dropbox/box/stripe/hubspot/.../qdrant)
// F2-F4: se il connettore ha Bearer/apikey e un API base noto, fa HTTP reale, altrimenti stub con nota F1 pending

using System.Text.Json;

namespace Basic.Connectors;

public static class ConnectorGenericHttp
{
    private static readonly Dictionary<string, string> ApiBase = new(StringComparer.OrdinalIgnoreCase)
    {
        ["google-sheets"] = "https://sheets.googleapis.com/v4/spreadsheets",
        ["google-docs"] = "https://docs.googleapis.com/v1/documents",
        ["onedrive"] = "https://graph.microsoft.com/v1.0/me/drive",
        ["sharepoint"] = "https://graph.microsoft.com/v1.0/sites",
        ["notion"] = "https://api.notion.com/v1/search",
        ["linear"] = "https://api.linear.app/graphql",
        ["jira"] = "https://api.atlassian.com/ex/jira", // placeholder, reale: https://{domain}.atlassian.net/rest/api/3/search
        ["discord"] = "https://discord.com/api/v10",
        ["telegram"] = "https://api.telegram.org",
        ["microsoft-teams"] = "https://graph.microsoft.com/v1.0/teams",
        ["dropbox"] = "https://api.dropboxapi.com/2",
        ["box"] = "https://api.box.com/2.0",
        ["stripe"] = "https://api.stripe.com/v1",
        ["hubspot"] = "https://api.hubapi.com/crm/v3/objects",
        ["salesforce"] = "https://login.salesforce.com/services/data/v59.0",
        ["shopify"] = "https://{shop}.myshopify.com/admin/api/2024-01",
        ["woocommerce"] = "https://example.com/wp-json/wc/v3",
        ["zendesk"] = "https://{domain}.zendesk.com/api/v2",
        ["twilio"] = "https://api.twilio.com/2010-04-01",
        ["sendgrid"] = "https://api.sendgrid.com/v3",
        ["docusign"] = "https://demo.docusign.net/restapi/v2.1",
        ["calendly"] = "https://api.calendly.com",
        ["docker"] = "http://localhost:2375",
        ["kubernetes"] = "https://kubernetes.default.svc",
        ["aws"] = "https://s3.amazonaws.com",
        ["azure"] = "https://management.azure.com",
        ["google-cloud"] = "https://storage.googleapis.com",
        ["cloudflare"] = "https://api.cloudflare.com/client/v4",
        ["sentry"] = "https://sentry.io/api/0",
        ["grafana"] = "https://grafana.example.com/api",
        ["prometheus"] = "http://localhost:9090/api/v1",
        ["gitlab"] = "https://gitlab.com/api/v4",
        ["bitbucket"] = "https://api.bitbucket.org/2.0",
        ["openai"] = "https://api.openai.com/v1",
        ["anthropic"] = "https://api.anthropic.com/v1",
        ["openrouter"] = "https://openrouter.ai/api/v1",
        ["huggingface"] = "https://huggingface.co/api",
        ["weights-biases"] = "https://api.wandb.ai",
        ["qdrant"] = "http://localhost:6333",
        ["chroma"] = "http://localhost:8000/api/v1",
        ["elasticsearch"] = "http://localhost:9200",
        ["neo4j"] = "http://localhost:7474/db/neo4j/tx/commit",
    };

    public static string Dispatch(string connectorName, string tool, string argsJson)
    {
        var auth = ConnectorAuth.GetAuthorizationHeader(connectorName);
        if (string.IsNullOrWhiteSpace(auth))
        {
            var key = ConnectorRegistry.Instance.GetSecret(connectorName, "api_key") ?? ConnectorRegistry.Instance.GetSecret(connectorName, "token");
            if (!string.IsNullOrWhiteSpace(key)) auth = $"Bearer {key}";
        }

        var baseUrl = ApiBase.TryGetValue(connectorName, out var b) ? b : null;
        if (string.IsNullOrWhiteSpace(baseUrl) || string.IsNullOrWhiteSpace(auth))
        {
            // Stub con nota: serve token reale per HTTP, altrimenti F1 pending
            return JsonSerializer.Serialize(new
            {
                connector = connectorName,
                tool,
                args = TryParseJson(argsJson),
                status = "ok",
                note = $"stub — {connectorName}.{tool} F2-F4: serve {(ApiBase.ContainsKey(connectorName) ? "Bearer/apikey" : "implementazione dedicata")} (AI CONNECTOR AUTH/SECRET SET)"
            }, new JsonSerializerOptions { WriteIndented = true });
        }

        // Prova HTTP reale (GET con query, POST per Linear/GraphQL)
        try
        {
            var args = TryParseArgs(argsJson);
            var url = BuildUrl(baseUrl, tool, args);
            var helper = new CommonLib.Http.HttpClientHelper();
            var method = tool.ToLowerInvariant() is "search" or "list" or "get" or "read" ? "GET" : "POST";
            // Linear/Notion usano POST
            if (connectorName is "linear" or "notion") method = "POST";
            var opts = new CommonLib.Http.HttpOptions { Method = method };
            opts.Headers.SetHeader("Authorization", auth);
            if (connectorName == "notion") opts.Headers.SetHeader("Notion-Version", "2022-06-28");
            if (connectorName == "openai" || connectorName == "anthropic") opts.Headers.SetHeader("Content-Type", "application/json");

            // Body per POST
            if (method == "POST" && argsJson.Trim() != "{}")
                opts.Body = argsJson;

            var res = helper.PerformRequestAsync(url, opts).GetAwaiter().GetResult();
            return JsonSerializer.Serialize(new { connector = connectorName, tool, url, statusCode = res.StatusCode, isSuccess = res.IsSuccess, body = res.Body ?? "", error = res.ErrorMessage }, new JsonSerializerOptions { WriteIndented = true });
        }
        catch (Exception ex)
        {
            return JsonSerializer.Serialize(new { connector = connectorName, tool, error = ex.Message, note = "HTTP fallback stub" }, new JsonSerializerOptions { WriteIndented = true });
        }
    }

    private static string BuildUrl(string baseUrl, string tool, Dictionary<string, string> args)
    {
        var t = tool.ToLowerInvariant();
        // Mappature semplici per tool comuni
        if (t is "search" or "list" or "get" or "read")
            return baseUrl;
        if (t is "issues" or "tickets" or "customers" or "contacts" or "products")
            return $"{baseUrl.TrimEnd('/')}/{t}";
        // Fallback: base + /tool
        return $"{baseUrl.TrimEnd('/')}/{t}";
    }

    private static Dictionary<string, string> TryParseArgs(string json)
    {
        var d = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (string.IsNullOrWhiteSpace(json)) return d;
        try { using var doc = JsonDocument.Parse(json); if (doc.RootElement.ValueKind == JsonValueKind.Object) foreach (var p in doc.RootElement.EnumerateObject()) d[p.Name] = p.Value.ValueKind == JsonValueKind.String ? p.Value.GetString() ?? "" : p.Value.GetRawText(); } catch { }
        return d;
    }
    private static object? TryParseJson(string json) { try { return JsonSerializer.Deserialize<object>(json); } catch { return json; } }
}
