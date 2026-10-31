// FILE: Basic.Connectors.ConnectorGoogle
// Google connectors F1 — gmail, drive, calendar via Google APIs (Gmail/Drive/Calendar, OAuth Bearer)

using System.Text.Json;

namespace Basic.Connectors;

public static class ConnectorGoogle
{
    public static string Dispatch(string connectorName, string tool, string argsJson)
    {
        var kind = connectorName.ToLowerInvariant();
        // Gmail
        if (kind == "gmail" || kind == "google-mail")
            return GmailDispatch(tool, argsJson, connectorName);
        if (kind == "google-drive" || kind == "drive")
            return DriveDispatch(tool, argsJson, connectorName);
        if (kind == "google-calendar" || kind == "calendar")
            return CalendarDispatch(tool, argsJson, connectorName);
        throw new ArgumentException($"Google connector '{connectorName}' unknown tool '{tool}'");
    }

    private static string GmailDispatch(string tool, string argsJson, string connectorName)
    {
        var auth = ConnectorAuth.GetAuthorizationHeader(connectorName);
        if (string.IsNullOrWhiteSpace(auth))
            return Stub(connectorName, tool, argsJson, "gmail — serve OAuth: AI CONNECTOR AUTH \"gmail\", '{\"access_token\":\"ya29...\"}'");

        var args = TryParseArgs(argsJson);
        var baseUrl = "https://gmail.googleapis.com/gmail/v1/users/me";
        return tool.ToLowerInvariant() switch
        {
            "search" => HttpGet($"{baseUrl}/messages?q={Uri.EscapeDataString(GetArg(args, "query") ?? GetArg(args, "q") ?? "")}", auth),
            "read" => HttpGet($"{baseUrl}/messages/{Uri.EscapeDataString(GetArg(args, "id") ?? GetArg(args, "messageId") ?? "")}?format=full", auth),
            "send" => HttpPost($"{baseUrl}/messages/send", $"{{\"raw\":\"{Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(GetArg(args, "raw") ?? ""))}\"}}", auth),
            "labels" => HttpGet($"{baseUrl}/labels", auth),
            _ => Stub(connectorName, tool, argsJson, "gmail tools: search/read/send/labels")
        };
    }

    private static string DriveDispatch(string tool, string argsJson, string connectorName)
    {
        var auth = ConnectorAuth.GetAuthorizationHeader(connectorName);
        if (string.IsNullOrWhiteSpace(auth))
            return Stub(connectorName, tool, argsJson, "drive — serve OAuth");

        var args = TryParseArgs(argsJson);
        var baseUrl = "https://www.googleapis.com/drive/v3";
        return tool.ToLowerInvariant() switch
        {
            "search" => HttpGet($"{baseUrl}/files?q={Uri.EscapeDataString(GetArg(args, "query") ?? GetArg(args, "q") ?? "trashed=false")}&fields=files(id,name,mimeType)", auth),
            "read" => HttpGet($"{baseUrl}/files/{Uri.EscapeDataString(GetArg(args, "id") ?? GetArg(args, "fileId") ?? "")}?alt=media", auth),
            "list" => HttpGet($"{baseUrl}/files?fields=files(id,name)", auth),
            _ => Stub(connectorName, tool, argsJson, "drive tools: search/read/list")
        };
    }

    private static string CalendarDispatch(string tool, string argsJson, string connectorName)
    {
        var auth = ConnectorAuth.GetAuthorizationHeader(connectorName);
        if (string.IsNullOrWhiteSpace(auth))
            return Stub(connectorName, tool, argsJson, "calendar — serve OAuth");

        var args = TryParseArgs(argsJson);
        var baseUrl = "https://www.googleapis.com/calendar/v3";
        return tool.ToLowerInvariant() switch
        {
            "events" => HttpGet($"{baseUrl}/calendars/primary/events?maxResults=10", auth),
            "list" => HttpGet($"{baseUrl}/users/me/calendarList", auth),
            _ => Stub(connectorName, tool, argsJson, "calendar tools: events/list")
        };
    }

    private static string HttpGet(string url, string auth)
    {
        var helper = new CommonLib.Http.HttpClientHelper();
        var opts = new CommonLib.Http.HttpOptions { Method = "GET" };
        opts.Headers.SetHeader("Authorization", auth);
        opts.Headers.SetHeader("Accept", "application/json");
        var res = helper.PerformRequestAsync(url, opts).GetAwaiter().GetResult();
        return JsonSerializer.Serialize(new { url, statusCode = res.StatusCode, body = res.Body ?? "", isSuccess = res.IsSuccess, error = res.ErrorMessage }, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string HttpPost(string url, string body, string auth)
    {
        var helper = new CommonLib.Http.HttpClientHelper();
        var opts = new CommonLib.Http.HttpOptions { Method = "POST", Body = body };
        opts.Headers.SetHeader("Authorization", auth);
        opts.Headers.SetHeader("Content-Type", "application/json");
        var res = helper.PerformRequestAsync(url, opts).GetAwaiter().GetResult();
        return JsonSerializer.Serialize(new { url, statusCode = res.StatusCode, body = res.Body ?? "", isSuccess = res.IsSuccess, error = res.ErrorMessage }, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string Stub(string conn, string tool, string argsJson, string note) =>
        JsonSerializer.Serialize(new { connector = conn, tool, args = TryParseJson(argsJson), status = "ok", note = $"stub — {note} (F1: serve token reale)" }, new JsonSerializerOptions { WriteIndented = true });

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
