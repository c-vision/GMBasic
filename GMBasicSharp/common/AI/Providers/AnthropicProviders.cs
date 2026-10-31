// AI/Providers/AnthropicProviders.cs
using System.Net.Http.Headers;
using System.Text.Json;
using Basic.AI.Rag;

namespace Basic.Providers;

internal sealed class AnthropicLlmCompleter : ILlmCompleter, IStreamingLlmCompleter, IToolCallingCompleter, IToolResultAwareCompleter, IVisionCompleter
{
    private const string DefaultBaseUrl = "https://api.anthropic.com/v1";
    private const string AnthropicVersion = "2023-06-01";
    private const int DefaultMaxTokens = 1024;

    private readonly ProviderDescriptor _descriptor;
    private readonly HttpClient _http;
    private readonly string _model;
    private readonly int _maxTokens;

    internal AnthropicLlmCompleter(ProviderDescriptor descriptor, HttpClient? http = null)
    {
        _descriptor = descriptor;
        _model = ProviderHttp.RequireConfig(descriptor, "model");
        var apiKey = descriptor.GetConfig("apiKey") ?? "";
        _maxTokens = int.TryParse(descriptor.GetConfig("maxTokens"), out var mt) && mt > 0 ? mt : DefaultMaxTokens;

        _http = http ?? new HttpClient { Timeout = TimeSpan.FromMilliseconds(ProviderHttp.ParseTimeoutMs(descriptor)) };
        if (!string.IsNullOrWhiteSpace(apiKey) && !_http.DefaultRequestHeaders.Contains("x-api-key"))
            _http.DefaultRequestHeaders.Add("x-api-key", apiKey);
        if (!_http.DefaultRequestHeaders.Contains("anthropic-version"))
            _http.DefaultRequestHeaders.Add("anthropic-version", AnthropicVersion);
        _http.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
    }

    public string ModelName => _model;

    public string Complete(string systemPrompt, string userPrompt)
    {
        var messages = new object[] { new { role = "user", content = userPrompt } };
        var body = string.IsNullOrEmpty(systemPrompt)
            ? (object)new { model = _model, max_tokens = _maxTokens, messages }
            : new { model = _model, max_tokens = _maxTokens, system = systemPrompt, messages };
        return Post(body);
    }

    public string DescribeImage(byte[] imageBytes, string format, string prompt)
    {
        var b64 = Convert.ToBase64String(imageBytes);
        var mime = format.ToLowerInvariant() switch
        {
            "png" => "image/png", "jpg" or "jpeg" => "image/jpeg",
            "gif" => "image/gif", "webp" => "image/webp", _ => "image/png"
        };
        var messages = new object[]
        {
            new { role = "user", content = new object[]
            {
                new { type = "image", source = new { type = "base64", media_type = mime, data = b64 } },
                new { type = "text", text = prompt }
            }}
        };
        var body = new { model = _model, max_tokens = _maxTokens, messages };
        return Post(body);
    }

    public IEnumerable<string> CompleteStream(string systemPrompt, string userPrompt)
    {
        var messages = new object[] { new { role = "user", content = userPrompt } };
        var body = new Dictionary<string, object?>
        {
            ["model"] = _model,
            ["max_tokens"] = _maxTokens,
            ["messages"] = messages,
            ["stream"] = true
        };
        if (!string.IsNullOrEmpty(systemPrompt)) body["system"] = systemPrompt;
        return PostStream(body);
    }

    private IEnumerable<string> PostStream(object body)
    {
        var url = ProviderHttp.BuildUrl(_descriptor, "/messages", DefaultBaseUrl);
        foreach (var delta in ProviderHttp.PostJsonStream(_http, url, body, _descriptor,
            static root =>
            {
                if (root.TryGetProperty("type", out var type) && type.GetString() == "content_block_delta"
                    && root.TryGetProperty("delta", out var delta)
                    && delta.TryGetProperty("text", out var text)
                    && text.ValueKind == System.Text.Json.JsonValueKind.String)
                    return text.GetString();
                return null;
            }, dataPrefix: true))
            yield return delta;
    }
    private string Post(object body)
    {
        var url = ProviderHttp.BuildUrl(_descriptor, "/messages", DefaultBaseUrl);
        var json = ProviderHttp.PostJson(_http, url, body, _descriptor);
        try
        {
            using var doc = JsonDocument.Parse(json);
            var content = doc.RootElement.GetProperty("content");
            foreach (var block in content.EnumerateArray())
                if (block.TryGetProperty("type", out var t) && t.GetString() == "text")
                    return block.GetProperty("text").GetString() ?? "";
            return "";
        }
        catch { return ""; }
    }

    public ToolCallOutcome CompleteWithTools(string systemPrompt, string userPrompt, IReadOnlyList<Basic.Mcp.McpToolDefinition> tools)
    {
        var messages = new object[] { new { role = "user", content = userPrompt } };
        var dict = new Dictionary<string, object?>
        {
            ["model"] = _model,
            ["max_tokens"] = _maxTokens,
            ["messages"] = messages,
            ["tools"] = tools.Select(ToAnthropicTool).ToArray(),
            ["tool_choice"] = new { type = "auto" }
        };
        if (!string.IsNullOrEmpty(systemPrompt)) dict["system"] = systemPrompt;

        var url = ProviderHttp.BuildUrl(_descriptor, "/messages", DefaultBaseUrl);
        var json = ProviderHttp.PostJson(_http, url, dict, _descriptor);
        return ParseToolCallOutcome(json);
    }

    public ToolCallOutcome CompleteWithToolResults(string systemPrompt, string userPrompt, IReadOnlyList<Basic.Mcp.McpToolDefinition> tools, IReadOnlyList<ToolExecution> executed)
    {
        // Multi-round NATIVO: i risultati dei tool rientrano come content block tool_result
        // (con tool_use_id), non come testo nel transcript.
        var resultBlocks = executed
            .Select(e => new { type = "tool_result", tool_use_id = e.ToolUseId, content = e.Result })
            .ToArray();
        var messages = new object[]
        {
            new { role = "user", content = userPrompt },
            new { role = "user", content = resultBlocks }
        };
        var dict = new Dictionary<string, object?>
        {
            ["model"] = _model,
            ["max_tokens"] = _maxTokens,
            ["messages"] = messages,
            ["tools"] = tools.Select(ToAnthropicTool).ToArray(),
            ["tool_choice"] = new { type = "auto" }
        };
        if (!string.IsNullOrEmpty(systemPrompt)) dict["system"] = systemPrompt;

        var url = ProviderHttp.BuildUrl(_descriptor, "/messages", DefaultBaseUrl);
        var json = ProviderHttp.PostJson(_http, url, dict, _descriptor);
        return ParseToolCallOutcome(json);
    }

    private static object ToAnthropicTool(Basic.Mcp.McpToolDefinition t) => new
    {
        name = t.Name,
        description = t.Description,
        input_schema = t.InputSchema
    };

    /// <summary>Anthropic: i tool call arrivano come content block <c>tool_use</c> (id, name,
    /// input) dentro <c>content</c>, non in un campo dedicato. Il testo finale e' il blocco
    /// <c>text</c>. Il tool-loop inietta i risultati nel transcript per il round successivo
    /// (coerente con OpenAI); un multi-round con content block tool_result e' un'evoluzione.</summary>
    private static ToolCallOutcome ParseToolCallOutcome(string json)
    {
        using var doc = JsonDocument.Parse(json);
        if (!doc.RootElement.TryGetProperty("content", out var content) || content.ValueKind != JsonValueKind.Array)
            return new ToolCallOutcome("", null);

        var calls = new List<Basic.Mcp.ToolCallRequest>();
        var text = new System.Text.StringBuilder();
        foreach (var block in content.EnumerateArray())
        {
            var type = block.TryGetProperty("type", out var tp) ? tp.GetString() : null;
            if (type == "text" && block.TryGetProperty("text", out var bt) && bt.ValueKind == JsonValueKind.String)
                text.Append(bt.GetString());
            else if (type == "tool_use")
            {
                var id = block.TryGetProperty("id", out var bid) ? bid.GetString() : "";
                var name = block.TryGetProperty("name", out var bn) ? bn.GetString() : null;
                if (string.IsNullOrWhiteSpace(name)) continue;
                var input = block.TryGetProperty("input", out var inp) ? inp.GetRawText() : "{}";
                calls.Add(new Basic.Mcp.ToolCallRequest(id ?? "", name!, input));
            }
        }
        if (calls.Count > 0) return new ToolCallOutcome(null, calls);
        return new ToolCallOutcome(text.ToString(), null);
    }
}
