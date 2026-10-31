// AI/Providers/OpenAiProviders.cs
using System.Text.Json;
using Basic.SQLite.Vector;
using Basic.AI.Rag;

namespace Basic.Providers;

internal sealed class OpenAiEmbeddingProvider : IEmbeddingProvider
{
    private const string DefaultBaseUrl = "https://api.openai.com/v1";
    private readonly ProviderDescriptor _descriptor;
    private readonly HttpClient _http;
    private readonly string _model;
    private readonly int _dimensions;

    internal OpenAiEmbeddingProvider(ProviderDescriptor descriptor, HttpClient? http = null)
    {
        _descriptor = descriptor;
        _http = ProviderHttp.CreateClient(descriptor, http);
        _model = ProviderHttp.RequireConfig(descriptor, "model");
        _dimensions = int.TryParse(descriptor.GetConfig("dimensions"), out var d) && d > 0 ? d : EmbeddingProviderResolver.DefaultDimensions;
    }

    public string ModelVersion => _descriptor.GetConfig("modelVersion") ?? $"openai:{_model}";

    public float[] Embed(string text, int dimensions)
    {
        ArgumentException.ThrowIfNullOrEmpty(text);
        if (dimensions <= 0) throw new ArgumentOutOfRangeException(nameof(dimensions));
        var url = ProviderHttp.BuildUrl(_descriptor, "/embeddings", DefaultBaseUrl);
        var body = new { model = _model, input = text };
        var responseJson = ProviderHttp.PostJson(_http, url, body, _descriptor);
        using var doc = JsonDocument.Parse(responseJson);
        var embedding = doc.RootElement.GetProperty("data")[0].GetProperty("embedding");
        var source = new float[embedding.GetArrayLength()];
        for (var i = 0; i < source.Length; i++) source[i] = embedding[i].GetSingle();
        return ProviderHttp.FitDimensions(source, dimensions);
    }
}

internal sealed class OpenAiLlmCompleter : ILlmCompleter, IStreamingLlmCompleter, IToolCallingCompleter, IVisionCompleter, IAudioCompleter
{
    private const string DefaultBaseUrl = "https://api.openai.com/v1";
    private readonly ProviderDescriptor _descriptor;
    private readonly HttpClient _http;
    private readonly string _model;

    internal OpenAiLlmCompleter(ProviderDescriptor descriptor, HttpClient? http = null)
    {
        _descriptor = descriptor;
        _http = ProviderHttp.CreateClient(descriptor, http);
        _model = ProviderHttp.RequireConfig(descriptor, "model");
    }

    public string ModelName => _model;

    public string Complete(string systemPrompt, string userPrompt)
    {
        var messages = new object[]
        {
            new { role = "system", content = systemPrompt ?? "" },
            new { role = "user", content = userPrompt }
        };
        return Chat(messages);
    }

    public IEnumerable<string> CompleteStream(string systemPrompt, string userPrompt)
    {
        var messages = new object[]
        {
            new { role = "system", content = systemPrompt ?? "" },
            new { role = "user", content = userPrompt }
        };
        return ChatStream(messages);
    }

    private IEnumerable<string> ChatStream(object[] messages)
    {
        var url = ProviderHttp.BuildUrl(_descriptor, "/chat/completions", DefaultBaseUrl);
        var dict = BuildBody(messages);
        dict["stream"] = true;
        foreach (var delta in ProviderHttp.PostJsonStream(_http, url, dict, _descriptor))
            yield return delta;
    }

    public string DescribeImage(byte[] imageBytes, string format, string prompt)
    {        var b64 = Convert.ToBase64String(imageBytes);
        var mime = format.ToLowerInvariant() switch
        {
            "png" => "image/png", "jpg" or "jpeg" => "image/jpeg",
            "gif" => "image/gif", "webp" => "image/webp", _ => "image/png"
        };
        var messages = new object[]
        {
            new { role = "user", content = new object[]
            {
                new { type = "text", text = prompt },
                new { type = "image_url", image_url = new { url = $"data:{mime};base64,{b64}" } }
            }}
        };
        return Chat(messages);
    }

    public string TranscribeAudio(byte[] audioBytes, string format)
    {
        var mime = format.ToLowerInvariant() switch
        {
            "mp3" => "audio/mpeg", "wav" => "audio/wav",
            "m4a" => "audio/mp4", "ogg" => "audio/ogg", _ => "audio/mpeg"
        };
        using var content = new MultipartFormDataContent();
        var audioContent = new ByteArrayContent(audioBytes);
        audioContent.Headers.ContentType = new System.Net.Http.Headers.MediaTypeHeaderValue(mime);
        content.Add(audioContent, "file", $"audio.{format}");
        content.Add(new StringContent(_model), "model");
        content.Add(new StringContent("text"), "response_format");

        var url = ProviderHttp.BuildUrl(_descriptor, "/audio/transcriptions", DefaultBaseUrl);
        var json = ProviderHttp.PostMultipart(_http, url, content, _descriptor);
        using var doc = JsonDocument.Parse(json);
        return doc.RootElement.TryGetProperty("text", out var t) ? t.GetString() ?? "" : "";
    }

    private string Chat(object[] messages)
    {
        var url = ProviderHttp.BuildUrl(_descriptor, "/chat/completions", DefaultBaseUrl);
        var body = BuildBody(messages);
        var json = ProviderHttp.PostJson(_http, url, body, _descriptor);
        return ProviderHttp.SafeExtractContent(json);
    }

    public ToolCallOutcome CompleteWithTools(string systemPrompt, string userPrompt, IReadOnlyList<Basic.Mcp.McpToolDefinition> tools)
    {
        var messages = new object[]
        {
            new { role = "system", content = systemPrompt ?? "" },
            new { role = "user", content = userPrompt }
        };
        var url = ProviderHttp.BuildUrl(_descriptor, "/chat/completions", DefaultBaseUrl);
        var dict = BuildBody(messages);
        dict["tools"] = tools.Select(ToOpenAiTool).ToArray();
        dict["tool_choice"] = "auto";

        var json = ProviderHttp.PostJson(_http, url, dict, _descriptor);
        return ParseToolCallOutcome(json);
    }

    private static object ToOpenAiTool(Basic.Mcp.McpToolDefinition t) => new
    {
        type = "function",
        function = new { name = t.Name, description = t.Description, parameters = t.InputSchema }
    };

    private static ToolCallOutcome ParseToolCallOutcome(string json)
    {
        using var doc = System.Text.Json.JsonDocument.Parse(json);
        if (doc.RootElement.TryGetProperty("choices", out var choices)
            && choices.ValueKind == System.Text.Json.JsonValueKind.Array
            && choices.GetArrayLength() > 0)
        {
            var msg = choices[0].GetProperty("message");
            if (msg.TryGetProperty("tool_calls", out var calls) && calls.ValueKind == System.Text.Json.JsonValueKind.Array)
            {
                var list = new List<Basic.Mcp.ToolCallRequest>();
                foreach (var c in calls.EnumerateArray())
                {
                    if (!c.TryGetProperty("function", out var fn)) continue;
                    var name = fn.TryGetProperty("name", out var n) ? n.GetString() : null;
                    if (string.IsNullOrWhiteSpace(name)) continue;
                    var args = fn.TryGetProperty("arguments", out var a) ? a.GetString() : null;
                    list.Add(new Basic.Mcp.ToolCallRequest("", name!, args ?? "{}"));
                }
                if (list.Count > 0) return new ToolCallOutcome(null, list);
            }
            if (msg.TryGetProperty("content", out var content) && content.ValueKind == System.Text.Json.JsonValueKind.String)
                return new ToolCallOutcome(content.GetString(), null);
        }
        return new ToolCallOutcome(ProviderHttp.SafeExtractContent(json), null);
    }

    private Dictionary<string, object?> BuildBody(object[] messages)
    {
        var dict = new Dictionary<string, object?> { ["model"] = _model, ["messages"] = messages };
        if (float.TryParse(_descriptor.GetConfig("temperature"), out var t)) dict["temperature"] = t;
        if (int.TryParse(_descriptor.GetConfig("maxTokens"), out var mt) && mt > 0) dict["max_tokens"] = mt;
        if (float.TryParse(_descriptor.GetConfig("topP"), out var tp)) dict["top_p"] = tp;
        return dict;
    }
}
