// FILE: Basic.Providers.ProviderHttp
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using Basic.SQLite.Vector;

namespace Basic.Providers;

internal static class ProviderHttp
{
    private static readonly Dictionary<string, ProviderCircuitBreaker> CircuitBreakers = new(StringComparer.OrdinalIgnoreCase);

    internal static ProviderCircuitBreaker GetCircuitBreaker(string providerName)
    {
        lock (CircuitBreakers)
        {
            if (!CircuitBreakers.TryGetValue(providerName, out var cb))
            {
                cb = new ProviderCircuitBreaker();
                CircuitBreakers[providerName] = cb;
            }
            return cb;
        }
    }

    internal static string RequireConfig(ProviderDescriptor descriptor, string key) =>
        descriptor.GetConfig(key)
        ?? throw new InvalidOperationException($"Provider '{descriptor.Name}' requires config key '{key}'.");

    internal static int ParseTimeoutMs(ProviderDescriptor descriptor, int defaultMs = 60_000) =>
        int.TryParse(descriptor.GetConfig("timeout"), out var ms) && ms > 0 ? ms : defaultMs;

    internal static HttpClient CreateClient(ProviderDescriptor descriptor, HttpClient? httpOverride = null)
    {
        var client = httpOverride ?? new HttpClient
        {
            Timeout = TimeSpan.FromMilliseconds(ParseTimeoutMs(descriptor))
        };

        var apiKey = descriptor.GetConfig("apiKey") ?? "";
        if (!string.IsNullOrWhiteSpace(apiKey) && client.DefaultRequestHeaders.Authorization is null)
            client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", apiKey);

        // Extra headers for providers like OpenRouter
        if (descriptor.GetConfig("httpReferer") is {} referer && !client.DefaultRequestHeaders.Contains("HTTP-Referer"))
            client.DefaultRequestHeaders.Add("HTTP-Referer", referer);
        if (descriptor.GetConfig("xTitle") is {} xtitle && !client.DefaultRequestHeaders.Contains("X-Title"))
            client.DefaultRequestHeaders.Add("X-Title", xtitle);

        return client;
    }

    internal static string BuildUrl(ProviderDescriptor descriptor, string path, string defaultBaseUrl)
    {
        var baseUrl = (descriptor.ApiBaseUrl ?? descriptor.GetConfig("baseUrl") ?? defaultBaseUrl).TrimEnd('/');
        return $"{baseUrl}{path}";
    }

    /// <remarks>
    /// ponytail: sync-over-async until IAsyncEmbeddingProvider / AWAIT in runtime; ceiling: blocks caller thread.
    /// </remarks>
    internal static string PostJson(
        HttpClient http,
        string url,
        object body,
        ProviderDescriptor? descriptor = null,
        CancellationToken cancellationToken = default)
    {        var cb = descriptor is not null ? GetCircuitBreaker(descriptor.Name) : null;

        if (cb is { IsOpen: true })
            throw new InvalidOperationException($"Circuit breaker open for provider '{descriptor!.Name}'. Try again later.");

        var json = JsonSerializer.Serialize(body);
        using var content = new StringContent(json, Encoding.UTF8, "application/json");
        try
        {
            using var response = http.PostAsync(url, content, cancellationToken).ConfigureAwait(false).GetAwaiter().GetResult();
            var responseBody = response.Content.ReadAsStringAsync(cancellationToken).ConfigureAwait(false).GetAwaiter().GetResult();
            if (!response.IsSuccessStatusCode)
                throw new HttpRequestException($"HTTP {(int)response.StatusCode}: {responseBody}");

            cb?.RecordSuccess();
            if (descriptor is not null)
                ProviderServer.Instance.ReportHealthResult(descriptor.Name, healthy: true);
            return responseBody;
        }
        catch (Exception)
        {
            cb?.RecordFailure();
            if (descriptor is not null)
                ProviderServer.Instance.ReportHealthResult(descriptor.Name, healthy: false);
            throw;
        }
    }

    /// <summary>POST JSON in STREAMING (SSE). Il body DEVE già contenere <c>stream: true</c>.
    /// <summary>POST JSON in STREAMING (SSE, formato OpenAI). <see cref="PostJsonStream(HttpClient, string, object, ProviderDescriptor?, Func{JsonElement, string?}, bool)"/>
    /// con il selettore OpenAI <c>choices[0].delta.content</c> e righe <c>data:</c> con
    /// terminatore <c>[DONE]</c>. Le eccezioni propagano al consumatore.</summary>
    internal static IEnumerable<string> PostJsonStream(
        HttpClient http,
        string url,
        object body,
        ProviderDescriptor? descriptor = null) =>
        PostJsonStream(http, url, body, descriptor, ExtractOpenAiDelta, dataPrefix: true);

    /// <summary>POST JSON in STREAMING (SSE). Il body DEVE già contenere <c>stream: true</c>.
    /// Legge la risposta riga per riga e yielda i delta di contenuto estratti da
    /// <paramref name="extractDelta"/> (che riceve il payload JSON della riga e ritorna il
    /// testo del delta, o null se la riga non ne ha). Con <paramref name="dataPrefix"/> le
    /// righe attese hanno il prefisso <c>data:</c> e il terminatore <c>[DONE]</c> (OpenAI);
    /// altrimenti ogni riga non vuota è un payload JSON (Ollama nativo). Le eccezioni
    /// propagano al consumatore (l'iteratore C# non può combinare yield e try/catch).</summary>
    internal static IEnumerable<string> PostJsonStream(
        HttpClient http,
        string url,
        object body,
        ProviderDescriptor? descriptor,
        Func<JsonElement, string?> extractDelta,
        bool dataPrefix)
    {
        var cb = descriptor is not null ? GetCircuitBreaker(descriptor.Name) : null;

        if (cb is { IsOpen: true })
            throw new InvalidOperationException($"Circuit breaker open for provider '{descriptor!.Name}'. Try again later.");

        var json = JsonSerializer.Serialize(body);
        using var content = new StringContent(json, Encoding.UTF8, "application/json");
        var request = new HttpRequestMessage(HttpMethod.Post, url) { Content = content };
        using var response = http.SendAsync(request, HttpCompletionOption.ResponseHeadersRead)
            .GetAwaiter().GetResult();
        if (!response.IsSuccessStatusCode)
            throw new HttpRequestException($"HTTP {(int)response.StatusCode}: {response.Content.ReadAsStringAsync().GetAwaiter().GetResult()}");

        using var stream = response.Content.ReadAsStream();
        using var reader = new StreamReader(stream, Encoding.UTF8);
        string? line;
        while ((line = reader.ReadLine()) is not null)
        {
            string payloadStr;
            if (dataPrefix)
            {
                if (!line.StartsWith("data:", StringComparison.Ordinal)) continue;
                payloadStr = line.AsSpan(5).Trim().ToString();
                if (payloadStr.Length == 0) continue;
                if (payloadStr == "[DONE]") break;
            }
            else
            {
                payloadStr = line.Trim();
                if (payloadStr.Length == 0) continue;
            }

            using var doc = JsonDocument.Parse(payloadStr);
            var text = extractDelta(doc.RootElement);
            if (!string.IsNullOrEmpty(text)) yield return text;
        }

        cb?.RecordSuccess();
        if (descriptor is not null)
            ProviderServer.Instance.ReportHealthResult(descriptor.Name, healthy: true);
    }

    private static string? ExtractOpenAiDelta(JsonElement root)
    {
        if (root.TryGetProperty("choices", out var choices)
            && choices.ValueKind == JsonValueKind.Array
            && choices.GetArrayLength() > 0
            && choices[0].TryGetProperty("delta", out var delta)
            && delta.TryGetProperty("content", out var contentProp)
            && contentProp.ValueKind == JsonValueKind.String)
        {
            return contentProp.GetString();
        }
        return null;
    }

    /// <summary>GET JSON (sync-over-async, stesso pattern di <see cref="PostJson"/>).
    /// Usato da PROVIDER.DISCOVER per interroga <c>GET {baseUrl}/models</c> del provider
    /// (formato OpenAI <c>data[]</c> o OpenRouter <c>models[]</c>). Applica il circuit
    /// breaker e il health-report come PostJson.</summary>
    internal static string GetJson(
        HttpClient http,
        string url,
        ProviderDescriptor? descriptor = null,
        CancellationToken cancellationToken = default)
    {
        var cb = descriptor is not null ? GetCircuitBreaker(descriptor.Name) : null;

        if (cb is { IsOpen: true })
            throw new InvalidOperationException($"Circuit breaker open for provider '{descriptor!.Name}'. Try again later.");

        try
        {
            using var response = http.GetAsync(url, cancellationToken).ConfigureAwait(false).GetAwaiter().GetResult();
            var responseBody = response.Content.ReadAsStringAsync(cancellationToken).ConfigureAwait(false).GetAwaiter().GetResult();
            if (!response.IsSuccessStatusCode)
                throw new HttpRequestException($"HTTP {(int)response.StatusCode}: {responseBody}");

            cb?.RecordSuccess();
            if (descriptor is not null)
                ProviderServer.Instance.ReportHealthResult(descriptor.Name, healthy: true);
            return responseBody;
        }
        catch (Exception)
        {
            cb?.RecordFailure();
            if (descriptor is not null)
                ProviderServer.Instance.ReportHealthResult(descriptor.Name, healthy: false);
            throw;
        }
    }

    internal static float[] FitDimensions(ReadOnlySpan<float> source, int dimensions)
    {
        var vector = new float[dimensions];
        source[..Math.Min(source.Length, dimensions)].CopyTo(vector);
        VectorMath.Normalize(vector);
        return vector;
    }

    /// <summary>Safely extracts <c>message.content</c> from an Ollama response.</summary>
    internal static string SafeExtractOllama(string responseJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(responseJson);
            if (doc.RootElement.TryGetProperty("message", out var msg) &&
                msg.TryGetProperty("content", out var ct) && ct.ValueKind == JsonValueKind.String)
                return ct.GetString() ?? "";
            return "";
        }
        catch { return ""; }
    }
    internal static string SafeExtractContent(string responseJson)
    {
        try
        {
            using var doc = JsonDocument.Parse(responseJson);
            if (!doc.RootElement.TryGetProperty("choices", out var choices)) return "";
            if (choices.ValueKind != JsonValueKind.Array || choices.GetArrayLength() == 0) return "";
            var first = choices[0];
            if (!first.TryGetProperty("message", out var msg) || msg.ValueKind != JsonValueKind.Object) return "";
            if (msg.TryGetProperty("content", out var content) && content.ValueKind == JsonValueKind.String)
                return content.GetString() ?? "";
            return "";
        }
        catch { return ""; }
    }

    /// <summary>POST multipart form data (for audio transcription).</summary>
    internal static string PostMultipart(HttpClient http, string url, MultipartFormDataContent content,
        ProviderDescriptor? descriptor = null, CancellationToken ct = default)
    {
        var cb = descriptor is not null ? GetCircuitBreaker(descriptor.Name) : null;
        if (cb is { IsOpen: true })
            throw new InvalidOperationException($"Circuit breaker open for provider '{descriptor!.Name}'.");

        try
        {
            using var response = http.PostAsync(url, content, ct).ConfigureAwait(false).GetAwaiter().GetResult();
            var body = response.Content.ReadAsStringAsync(ct).ConfigureAwait(false).GetAwaiter().GetResult();
            if (!response.IsSuccessStatusCode)
                throw new HttpRequestException($"HTTP {(int)response.StatusCode}: {body}");

            cb?.RecordSuccess();
            if (descriptor is not null)
                ProviderServer.Instance.ReportHealthResult(descriptor.Name, healthy: true);
            return body;
        }
        catch (Exception)
        {
            cb?.RecordFailure();
            if (descriptor is not null)
                ProviderServer.Instance.ReportHealthResult(descriptor.Name, healthy: false);
            throw;
        }
    }
}
