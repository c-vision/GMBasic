// FILE: Basic.Mcp.McpClient
// Client MCP verso server esterni (lato consumo). Senza pacchetti esterni: http = POST
// JSON-RPC sull'endpoint (Streamable HTTP); stdio = processo locale già sul PATH.
// Stato per-server mantenuto vivo per l'intero processo:
//   - http:  handshake initialize eseguito UNA volta (i server reali lo richiedono)
//   - stdio: il processo resta in esecuzione (pooling) e viene riusato per le call successive
using System.Text;
using System.Text.Json;

namespace Basic.Mcp;

/// <summary>Client MCP: esegue chiamate JSON-RPC verso un server della lista
/// (<see cref="McpConfig"/>). Mantiene lo stato per-server (handshake / processo).</summary>
public static class McpClient
{
    private sealed class StdioSession
    {
        public required System.Diagnostics.Process Proc { get; init; }
    }

    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, bool> _httpInitialized = new(StringComparer.OrdinalIgnoreCase);
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, StdioSession> _stdio = new(StringComparer.OrdinalIgnoreCase);
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, object> _serverLocks = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>tools/call: invoca <paramref name="tool"/> sul server con gli argomenti
    /// JSON <paramref name="argsJson"/>. Ritorna il testo dei content block (o l'errore).</summary>
    public static string CallTool(McpServerEntry server, string tool, string argsJson)
    {
        ArgumentNullException.ThrowIfNull(server);
        if (!server.Enabled) throw new InvalidOperationException($"Server MCP '{server.Name}' disabilitato.");
        var payload = BuildRequest("tools/call", new { name = tool, arguments = ParseArgs(argsJson) });

        if (server.Kind.Equals("http", StringComparison.OrdinalIgnoreCase))
        {
            var url = server.Url ?? throw new ArgumentException($"Server MCP '{server.Name}': URL mancante.");
            EnsureHttpInitialized(server, url);
            return PostJsonRpc(url, payload, server);
        }
        if (server.Kind.Equals("stdio", StringComparison.OrdinalIgnoreCase))
        {
            var command = server.Command ?? throw new ArgumentException($"Server MCP '{server.Name}': comando mancante.");
            return StdioRequest(server, command, server.Args ?? [], payload);
        }

        throw new NotSupportedException(
            $"Server MCP '{server.Name}': trasporto '{server.Kind}' non supportato (http, stdio).");
    }

    /// <summary>Scopre i tool di un server (initialize + tools/list). Ritorna nome/descrizione
    /// per ciascun tool esposto. Collegato all'aggiunta da marketplace e all'ENABLE.</summary>
    public static List<(string Name, string Description)> DiscoverTools(McpServerEntry server)
    {
        ArgumentNullException.ThrowIfNull(server);
        if (server.Kind.Equals("http", StringComparison.OrdinalIgnoreCase))
        {
            var url = server.Url ?? throw new ArgumentException($"Server MCP '{server.Name}': URL mancante.");
            EnsureHttpInitialized(server, url);
            return ParseTools(PostJsonRpcResult(url, BuildRequest("tools/list", null), server));
        }
        if (server.Kind.Equals("stdio", StringComparison.OrdinalIgnoreCase))
        {
            var command = server.Command ?? throw new ArgumentException($"Server MCP '{server.Name}': comando mancante.");
            return ParseTools(StdioRequest(server, command, server.Args ?? [], BuildRequest("tools/list", null)));
        }
        throw new NotSupportedException($"Server MCP '{server.Name}': trasporto '{server.Kind}' non supportato (http, stdio).");
    }

    /// <summary>Handshake http eseguito una sola volta per server (i server MCP reali
    /// richiedono initialize prima di tools/*).</summary>
    private static void EnsureHttpInitialized(McpServerEntry server, string url)
    {
        if (_httpInitialized.ContainsKey(server.Name)) return;
        var init = BuildRequest("initialize", new { protocolVersion = "2024-11-05", capabilities = new { }, clientInfo = new { name = "gmbasic", version = "1.0" } });
        _ = PostJsonRpcResult(url, init, server);
        _httpInitialized[server.Name] = true;
    }

    /// <summary>Richiesta su un server stdio riusando il processo (pooling): lo avvia alla
    /// prima richiesta (con handshake initialize) e lo tiene vivo per le successive.</summary>
    private static string StdioRequest(McpServerEntry server, string command, List<string> args, string payload)
    {
        var gate = _serverLocks.GetOrAdd(server.Name, _ => new object());
        lock (gate)
        {
            var session = _stdio.GetOrAdd(server.Name, _ => StartStdio(server, command, args));
            WriteLineAndFlush(session.Proc, payload);
            var responseJson = ReadJsonRpcResponse(session.Proc, server.Name);
            return ExtractResult(responseJson, server.Name);
        }
    }

    private static StdioSession StartStdio(McpServerEntry server, string command, List<string> args)
    {
        var psi = new System.Diagnostics.ProcessStartInfo(command)
        {
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false
        };
        foreach (var a in args) psi.ArgumentList.Add(a);
        if (server.Env is { } env)
            foreach (var kv in env) psi.Environment[kv.Key] = kv.Value;

        var proc = System.Diagnostics.Process.Start(psi)
            ?? throw new InvalidOperationException($"MCP server '{server.Name}': impossibile avviare '{command}'.");
        WriteLineAndFlush(proc, BuildRequest("initialize", new { protocolVersion = "2024-11-05", capabilities = new { }, clientInfo = new { name = "gmbasic", version = "1.0" } }));
        ReadJsonRpcResponse(proc, server.Name);
        return new StdioSession { Proc = proc };
    }

    private static void WriteLineAndFlush(System.Diagnostics.Process proc, string json)
    {
        proc.StandardInput.WriteLine(json);
        proc.StandardInput.Flush();
    }

    /// <summary>Legge da stdout finché trova una riga JSON-RPC valida (alcuni server
    /// loggano su stdout righe non-JSON). Ritorna la risposta JSON.</summary>
    private static string ReadJsonRpcResponse(System.Diagnostics.Process proc, string serverName)
    {
        string? line;
        while ((line = proc.StandardOutput.ReadLine()) is not null)
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            try
            {
                using var doc = JsonDocument.Parse(line);
                if (doc.RootElement.TryGetProperty("jsonrpc", out _)) return line;
            }
            catch (JsonException) { /* riga di log, continua */ }
        }
        var err = proc.StandardError.ReadToEnd();
        throw new InvalidOperationException($"MCP server '{serverName}': nessuna risposta JSON-RPC. stderr={err}");
    }

    private static string ExtractResult(string responseJson, string serverName)
    {
        using var doc = JsonDocument.Parse(responseJson);
        if (doc.RootElement.TryGetProperty("error", out var err))
            throw new InvalidOperationException($"MCP server '{serverName}': {err.GetString() ?? err.ToString()}");
        if (!doc.RootElement.TryGetProperty("result", out var res)) return responseJson;
        if (res.TryGetProperty("isError", out var isErr) && isErr.GetBoolean())
            throw new InvalidOperationException($"MCP tool error: {ExtractText(res)}");
        return ExtractText(res);
    }

    private static string BuildRequest(string method, object? parameters)
    {
        using var ms = new MemoryStream();
        using (var w = new Utf8JsonWriter(ms))
        {
            w.WriteStartObject();
            w.WriteString("jsonrpc", "2.0");
            w.WriteNumber("id", 1);
            w.WriteString("method", method);
            if (parameters is not null)
            {
                w.WritePropertyName("params");
                JsonSerializer.Serialize(w, parameters, options: null);
            }
            w.WriteEndObject();
        }
        return Encoding.UTF8.GetString(ms.ToArray());
    }

    private static object? ParseArgs(string argsJson)
    {
        if (string.IsNullOrWhiteSpace(argsJson)) return null;
        return JsonSerializer.Deserialize<object>(argsJson);
    }

    /// <summary>Estrae l'array tools dal JSON del <c>result</c>: [{name, description}].</summary>
    private static List<(string Name, string Description)> ParseTools(string resultJson)
    {
        var list = new List<(string, string)>();
        using var doc = JsonDocument.Parse(resultJson);
        if (!doc.RootElement.TryGetProperty("tools", out var tools) || tools.ValueKind != JsonValueKind.Array) return list;
        foreach (var t in tools.EnumerateArray())
        {
            var name = t.TryGetProperty("name", out var n) ? n.GetString() : null;
            if (string.IsNullOrWhiteSpace(name)) continue;
            var desc = t.TryGetProperty("description", out var d) ? d.GetString() : null;
            list.Add((name!, desc ?? ""));
        }
        return list;
    }

    /// <summary>POST JSON-RPC che ritorna il <c>result</c> come JSON grezzo (per tools/list).</summary>
    private static string PostJsonRpcResult(string url, string payload, McpServerEntry server)
    {
        var raw = PostJsonRpcRaw(url, payload, server);
        using var doc = JsonDocument.Parse(raw);
        return doc.RootElement.TryGetProperty("result", out var res) ? res.GetRawText() : raw;
    }

    private static string PostJsonRpcRaw(string url, string payload, McpServerEntry server)
    {
        var helper = new CommonLib.Http.HttpClientHelper();
        var opts = new CommonLib.Http.HttpOptions { Method = "POST", Body = payload };
        opts.Headers.SetHeader("Content-Type", "application/json");
        opts.Headers.SetHeader("Accept", "application/json, text/event-stream");
        // OAuth per-connector (F0): ConnectorAuth Bearer ha priorità su ApiKey
        try
        {
            var oauthHeader = Basic.Connectors.ConnectorAuth.GetAuthorizationHeader(server.Name);
            if (!string.IsNullOrWhiteSpace(oauthHeader))
                opts.Headers.SetHeader("Authorization", oauthHeader);
            else if (server.Env is { } env && env.TryGetValue("ApiKey", out var key))
                opts.Headers.SetHeader("Authorization", $"Bearer {key}");
        }
        catch { if (server.Env is { } env2 && env2.TryGetValue("ApiKey", out var key2)) opts.Headers.SetHeader("Authorization", $"Bearer {key2}"); }

        var result = helper.PerformRequestAsync(url, opts).GetAwaiter().GetResult();
        if (!string.IsNullOrEmpty(result.ErrorMessage))
            throw new InvalidOperationException($"MCP server '{server.Name}': {result.ErrorMessage}");
        if (string.IsNullOrWhiteSpace(result.Body)) return "{}";
        return result.Body;
    }

    private static string PostJsonRpc(string url, string payload, McpServerEntry server)
    {
        var raw = PostJsonRpcRaw(url, payload, server);
        using var doc = JsonDocument.Parse(raw);
        if (doc.RootElement.TryGetProperty("error", out var err))
            throw new InvalidOperationException($"MCP server '{server.Name}': {err.GetString() ?? err.ToString()}");
        if (!doc.RootElement.TryGetProperty("result", out var res)) return raw;
        if (res.TryGetProperty("isError", out var isErr) && isErr.GetBoolean())
        {
            var msg = ExtractText(res);
            throw new InvalidOperationException($"MCP tool error: {msg}");
        }
        return ExtractText(res);
    }

    /// <summary>Concatena i content block testuali del risultato MCP.</summary>
    private static string ExtractText(JsonElement res)
    {
        if (!res.TryGetProperty("content", out var content) || content.ValueKind != JsonValueKind.Array)
            return res.GetRawText();
        var sb = new StringBuilder();
        foreach (var block in content.EnumerateArray())
        {
            if (block.TryGetProperty("text", out var text) && text.ValueKind == JsonValueKind.String)
            {
                if (sb.Length > 0) sb.Append('\n');
                sb.Append(text.GetString());
            }
        }
        return sb.ToString();
    }

    /// <summary>Killa i processi stdio e azzera lo stato (test).</summary>
    internal static void ResetForTests()
    {
        _httpInitialized.Clear();
        foreach (var session in _stdio.Values)
        {
            try { session.Proc.Kill(entireProcessTree: true); } catch { }
            try { session.Proc.Dispose(); } catch { }
        }
        _stdio.Clear();
        _serverLocks.Clear();
    }
}
