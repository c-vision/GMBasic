// FILE: Basic.Mcp.McpHttpListenerHost
using System.Net;
using System.Text;

namespace Basic.Mcp;

/// <summary>Minimal secure HTTP transport for MCP JSON-RPC (POST /mcp).</summary>
/// <remarks>ponytail: HttpListener single-thread accept + per-request tasks; upgrade path: ASP.NET Core Kestrel.</remarks>
public sealed class McpHttpListenerHost : IAsyncDisposable
{
    private readonly McpServer _server;
    private readonly McpHttpOptions _options;
    private readonly McpHttpSecurity _security;
    private readonly McpAuditLog _audit;
    private readonly HttpListener _listener = new();
    private readonly string _prefix;

    public McpHttpListenerHost(
        McpServer server,
        McpHttpOptions options,
        McpAuditLog? audit = null)
    {
        _server = server ?? throw new ArgumentNullException(nameof(server));
        _options = options ?? throw new ArgumentNullException(nameof(options));
        _security = new McpHttpSecurity(options);
        _audit = audit ?? new McpAuditLog();
        _prefix = $"http://127.0.0.1:{options.Port}/";
        _listener.Prefixes.Add(_prefix);
    }

    /// <summary>Bound URL prefix.</summary>
    public string Prefix => _prefix;

    /// <summary>Start accepting HTTP MCP requests until cancelled.</summary>
    public async Task RunAsync(CancellationToken cancellationToken = default)
    {
        if (_security.RequiresApiKey && string.IsNullOrEmpty(_options.ApiKey))
            throw new InvalidOperationException($"HTTP MCP requires {McpConstants.EnvApiKey} when API key auth is enabled.");

        _listener.Start();
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                HttpListenerContext context;
                try
                {
                    context = await _listener.GetContextAsync().WaitAsync(cancellationToken).ConfigureAwait(false);
                }
                catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
                {
                    break;
                }

                _ = Task.Run(() => HandleRequestAsync(context), CancellationToken.None);
            }
        }
        finally
        {
            if (_listener.IsListening)
                _listener.Stop();
        }
    }

    internal async Task HandleRequestAsync(HttpListenerContext context)
    {
        var clientId = McpHttpSecurity.GetClientId(context.Request);
        try
        {
            using var timeoutCts = new CancellationTokenSource(_options.RequestTimeout);
            await HandleRequestCoreAsync(context, clientId, timeoutCts.Token).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            await WriteJsonAsync(context.Response, 500,
                """{"jsonrpc":"2.0","id":null,"error":{"code":-32603,"message":"Internal error","data":""" +
                JsonEscape(ex.Message) + "\"}}").ConfigureAwait(false);
        }
    }

    private async Task HandleRequestCoreAsync(HttpListenerContext context, string clientId, CancellationToken cancellationToken)
    {
        var request = context.Request;
        var response = context.Response;
        var normalizedPath = request.Url?.AbsolutePath ?? "/";

        if (!string.Equals(normalizedPath, _options.Path, StringComparison.OrdinalIgnoreCase))
        {
            await WritePlainAsync(response, 404, "Not Found").ConfigureAwait(false);
            return;
        }

        if (!string.Equals(request.HttpMethod, "POST", StringComparison.OrdinalIgnoreCase))
        {
            await WritePlainAsync(response, 405, "Method Not Allowed").ConfigureAwait(false);
            return;
        }

        if (!_security.TryValidateApiKey(request, out var authFailure))
        {
            _audit.RecordAuthFailure(clientId, authFailure ?? "auth_failed");
            await WriteJsonAsync(response, 401,
                """{"jsonrpc":"2.0","id":null,"error":{"code":-32001,"message":"Unauthorized"}}""").ConfigureAwait(false);
            return;
        }

        if (!_security.TryConsumeRateLimit(clientId, out var rateFailure))
        {
            _audit.RecordAuthFailure(clientId, rateFailure ?? "rate_limited");
            await WriteJsonAsync(response, 429,
                """{"jsonrpc":"2.0","id":null,"error":{"code":-32002,"message":"Too Many Requests"}}""").ConfigureAwait(false);
            return;
        }

        string body;
        try
        {
            body = await ReadBodyAsync(request.InputStream, request.ContentLength64, cancellationToken)
                .ConfigureAwait(false);
        }
        catch (InvalidOperationException ex)
        {
            await WriteJsonAsync(response, 413,
                "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32600,\"message\":\"" +
                JsonEscape(ex.Message) + "\"}}").ConfigureAwait(false);
            return;
        }

        string? jsonResponse;
        using (McpRequestContext.PushClient(clientId))
            jsonResponse = await _server.HandleRequestAsync(body, cancellationToken).ConfigureAwait(false);

        if (jsonResponse is null)
        {
            response.StatusCode = 204;
            response.Close();
            return;
        }

        await WriteJsonAsync(response, 200, jsonResponse).ConfigureAwait(false);
    }

    private async Task<string> ReadBodyAsync(Stream stream, long contentLength, CancellationToken cancellationToken)
    {
        if (contentLength < 0)
            contentLength = 0;

        if (contentLength > _options.MaxBodyBytes)
            throw new InvalidOperationException($"Request body exceeds max size ({_options.MaxBodyBytes} bytes).");

        using var ms = new MemoryStream();
        var buffer = new byte[8192];
        while (ms.Length < contentLength || contentLength == 0)
        {
            var read = await stream.ReadAsync(buffer.AsMemory(0, buffer.Length), cancellationToken)
                .ConfigureAwait(false);
            if (read == 0)
                break;
            ms.Write(buffer, 0, read);
            if (ms.Length > _options.MaxBodyBytes)
                throw new InvalidOperationException($"Request body exceeds max size ({_options.MaxBodyBytes} bytes).");
            if (contentLength > 0 && ms.Length >= contentLength)
                break;
        }

        return Encoding.UTF8.GetString(ms.ToArray());
    }

    private static async Task WriteJsonAsync(HttpListenerResponse response, int statusCode, string json)
    {
        var bytes = Encoding.UTF8.GetBytes(json);
        response.StatusCode = statusCode;
        response.ContentType = "application/json; charset=utf-8";
        response.ContentLength64 = bytes.Length;
        await response.OutputStream.WriteAsync(bytes).ConfigureAwait(false);
        response.Close();
    }

    private static async Task WritePlainAsync(HttpListenerResponse response, int statusCode, string message)
    {
        var bytes = Encoding.UTF8.GetBytes(message);
        response.StatusCode = statusCode;
        response.ContentType = "text/plain; charset=utf-8";
        response.ContentLength64 = bytes.Length;
        await response.OutputStream.WriteAsync(bytes).ConfigureAwait(false);
        response.Close();
    }

    private static string JsonEscape(string value) =>
        value.Replace("\\", "\\\\", StringComparison.Ordinal).Replace("\"", "\\\"", StringComparison.Ordinal);

    /// <inheritdoc />
    public async ValueTask DisposeAsync()
    {
        if (_listener.IsListening)
            _listener.Stop();
        _listener.Close();
        await Task.CompletedTask.ConfigureAwait(false);
    }
}
