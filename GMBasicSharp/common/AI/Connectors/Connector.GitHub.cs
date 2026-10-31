// FILE: Basic.Connectors.ConnectorGitHub
// GitHub connector F1 — repository/issues/PR/commits/actions via GitHub REST (OAuth Bearer / api_key)

using System.Text.Json;

namespace Basic.Connectors;

public static class ConnectorGitHub
{
    public static string Dispatch(string tool, string argsJson, string connectorName)
    {
        var auth = ConnectorAuth.GetAuthorizationHeader(connectorName);
        // GitHub accetta anche api_key come Bearer
        if (string.IsNullOrWhiteSpace(auth))
        {
            var key = ConnectorRegistry.Instance.GetSecret(connectorName, "api_key") ?? ConnectorRegistry.Instance.GetSecret(connectorName, "token");
            if (!string.IsNullOrWhiteSpace(key)) auth = $"Bearer {key}";
        }
        if (string.IsNullOrWhiteSpace(auth))
            return Stub(connectorName, tool, argsJson, "github — serve OAuth/api_key: AI CONNECTOR SECRET SET \"github\", \"api_key\", \"ghp_...\"");

        var args = TryParseArgs(argsJson);
        var apiBase = "https://api.github.com";
        return tool.ToLowerInvariant() switch
        {
            "repository" or "repo" => HttpGet($"{apiBase}/repos/{GetArg(args, "name") ?? GetArg(args, "repo") ?? "octocat/Hello-World"}", auth),
            "issues" => HttpGet($"{apiBase}/repos/{GetArg(args, "repo") ?? "octocat/Hello-World"}/issues?state={GetArg(args, "state") ?? "open"}", auth),
            "pull_requests" or "prs" => HttpGet($"{apiBase}/repos/{GetArg(args, "repo") ?? "octocat/Hello-World"}/pulls", auth),
            "commits" => HttpGet($"{apiBase}/repos/{GetArg(args, "repo") ?? "octocat/Hello-World"}/commits?sha={GetArg(args, "branch") ?? "main"}", auth),
            "branches" => HttpGet($"{apiBase}/repos/{GetArg(args, "repo") ?? "octocat/Hello-World"}/branches", auth),
            "releases" => HttpGet($"{apiBase}/repos/{GetArg(args, "repo") ?? "octocat/Hello-World"}/releases", auth),
            "actions" or "workflows" => HttpGet($"{apiBase}/repos/{GetArg(args, "repo") ?? "octocat/Hello-World"}/actions/workflows", auth),
            _ => Stub(connectorName, tool, argsJson, "github tools: repository/issues/pull_requests/branches/commits/releases/actions")
        };
    }

    private static string HttpGet(string url, string auth)
    {
        var helper = new CommonLib.Http.HttpClientHelper();
        var opts = new CommonLib.Http.HttpOptions { Method = "GET" };
        opts.Headers.SetHeader("Authorization", auth.StartsWith("Bearer", StringComparison.OrdinalIgnoreCase) ? auth : $"Bearer {auth}");
        opts.Headers.SetHeader("Accept", "application/vnd.github+json");
        opts.Headers.SetHeader("User-Agent", "GMBasic-Connector/1.0");
        var res = helper.PerformRequestAsync(url, opts).GetAwaiter().GetResult();
        return JsonSerializer.Serialize(new { url, statusCode = res.StatusCode, isSuccess = res.IsSuccess, body = res.Body ?? "", error = res.ErrorMessage }, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string Stub(string conn, string tool, string argsJson, string note) =>
        JsonSerializer.Serialize(new { connector = conn, tool, args = TryParseJson(argsJson), status = "ok", note }, new JsonSerializerOptions { WriteIndented = true });

    private static Dictionary<string, string> TryParseArgs(string json)
    {
        var d = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (string.IsNullOrWhiteSpace(json)) return d;
        try { using var doc = JsonDocument.Parse(json); if (doc.RootElement.ValueKind == JsonValueKind.Object) foreach (var p in doc.RootElement.EnumerateObject()) d[p.Name] = p.Value.ValueKind == JsonValueKind.String ? p.Value.GetString() ?? "" : p.Value.GetRawText(); } catch { }
        return d;
    }
    private static string? GetArg(Dictionary<string, string> d, string k) => d.TryGetValue(k, out var v) ? v : null;
    private static object? TryParseJson(string json) { try { return JsonSerializer.Deserialize<object>(json); } catch { return json; } }
}
