// FILE: Basic.Providers.OllamaProviders
using System.Text.Json;
using Basic.SQLite.Vector;
using Basic.AI.Rag;
using Basic.Mcp;

namespace Basic.Providers;

internal sealed class OllamaEmbeddingProvider : IEmbeddingProvider
{
    private const string DefaultBaseUrl = "http://localhost:11434";

    private readonly ProviderDescriptor _descriptor;
    private readonly HttpClient _http;
    private readonly string _model;
    private readonly int _dimensions;

    internal OllamaEmbeddingProvider(ProviderDescriptor descriptor, HttpClient? http = null)
    {
        _descriptor = descriptor;
        _http = ProviderHttp.CreateClient(descriptor, http);
        _model = ProviderHttp.RequireConfig(descriptor, "model");
        _dimensions = int.TryParse(descriptor.GetConfig("dimensions"), out var d) && d > 0
            ? d
            : EmbeddingProviderResolver.DefaultDimensions;
    }

    public string ModelVersion => _descriptor.GetConfig("modelVersion") ?? $"ollama:{_model}";

    public float[] Embed(string text, int dimensions)
    {
        ArgumentException.ThrowIfNullOrEmpty(text);
        if (dimensions <= 0)
            throw new ArgumentOutOfRangeException(nameof(dimensions));

        var url = ProviderHttp.BuildUrl(_descriptor, "/api/embed", DefaultBaseUrl);
        var body = new { model = _model, input = text };
        var responseJson = ProviderHttp.PostJson(_http, url, body, _descriptor);
        using var doc = JsonDocument.Parse(responseJson);
        var embedding = doc.RootElement.GetProperty("embeddings")[0];
        var source = new float[embedding.GetArrayLength()];
        for (var i = 0; i < source.Length; i++)
            source[i] = embedding[i].GetSingle();

        return ProviderHttp.FitDimensions(source, dimensions);
    }
}

internal sealed class OllamaLlmCompleter : ILlmCompleter, IStreamingLlmCompleter, IToolCallingCompleter, IVisionCompleter, IAudioCompleter
{
    private const string DefaultBaseUrl = "http://localhost:11434";

    private readonly ProviderDescriptor _descriptor;
    private readonly HttpClient _http;
    private readonly string _model;

    internal OllamaLlmCompleter(ProviderDescriptor descriptor, HttpClient? http = null)
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

    // FASE 5 Parte B: visione via Ollama /api/chat con images (array base64) nel messaggio
    // user -- protocollo nativo Ollama, supportato da modelli multimodali (llava, qwen2-vl,
    // minicpm-v, gemma-vision, ecc.). Stessa forma del payload di Complete(), con il campo
    // images aggiunto.
    public string DescribeImage(byte[] imageBytes, string format, string prompt)
    {
        var b64 = Convert.ToBase64String(imageBytes);
        var messages = new object[]
        {
            new { role = "user", content = prompt, images = new[] { b64 } }
        };
        return Chat(messages);
    }

    // FASE 5 Parte B: audio via Ollama /api/chat con audio (array base64) nel messaggio user.
    // Ollama 0.31+ supporta il campo audio per i modelli che lo accettano (whisper, e simili
    // convertiti); il testo trascritto arriva come message.content. Nota: non esiste un
    // /audio/transcriptions in Ollama -- la trascrizione passa dal chat con audio in input.
    public string TranscribeAudio(byte[] audioBytes, string format)
    {
        var b64 = Convert.ToBase64String(audioBytes);
        var prompt = _descriptor.GetConfig("audioPrompt") ?? "Transcribe the audio.";
        var messages = new object[]
        {
            new { role = "user", content = prompt, audio = new[] { b64 } }
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
        var url = ProviderHttp.BuildUrl(_descriptor, "/api/chat", DefaultBaseUrl);
        var body = new Dictionary<string, object?> { ["model"] = _model, ["messages"] = messages, ["stream"] = true };
        if (float.TryParse(_descriptor.GetConfig("temperature"), out var t)) body["temperature"] = t;
        foreach (var delta in ProviderHttp.PostJsonStream(_http, url, body, _descriptor,
            static root =>
            {
                if (root.TryGetProperty("message", out var message)
                    && message.TryGetProperty("content", out var content)
                    && content.ValueKind == System.Text.Json.JsonValueKind.String)
                    return content.GetString();
                return null;
            }, dataPrefix: false))
            yield return delta;
    }

    private string Chat(object[] messages)
    {
        var url = ProviderHttp.BuildUrl(_descriptor, "/api/chat", DefaultBaseUrl);
        var body = new Dictionary<string, object?> { ["model"] = _model, ["messages"] = messages, ["stream"] = false };
        if (float.TryParse(_descriptor.GetConfig("temperature"), out var t)) body["temperature"] = t;
        var responseJson = ProviderHttp.PostJson(_http, url, body, _descriptor);
        return ProviderHttp.SafeExtractOllama(responseJson);
    }

    public ToolCallOutcome CompleteWithTools(string systemPrompt, string userPrompt, IReadOnlyList<McpToolDefinition> tools)
    {
        var messages = new object[]
        {
            new { role = "system", content = systemPrompt ?? "" },
            new { role = "user", content = userPrompt }
        };
        var url = ProviderHttp.BuildUrl(_descriptor, "/api/chat", DefaultBaseUrl);
        var dict = new Dictionary<string, object?>
        {
            ["model"] = _model,
            ["messages"] = messages,
            ["stream"] = false,
            ["tools"] = tools.Select(ToOllamaTool).ToArray()
        };
        if (float.TryParse(_descriptor.GetConfig("temperature"), out var t)) dict["temperature"] = t;

        var json = ProviderHttp.PostJson(_http, url, dict, _descriptor);
        return ParseOllamaToolCallOutcome(json);
    }

    private static object ToOllamaTool(McpToolDefinition t) => new
    {
        type = "function",
        function = new { name = t.Name, description = t.Description, parameters = t.InputSchema }
    };

    private static ToolCallOutcome ParseOllamaToolCallOutcome(string json)
    {
        try
        {
            using var doc = JsonDocument.Parse(json);
            if (doc.RootElement.TryGetProperty("message", out var msg))
            {
                // Ollama native: message.tool_calls = [{function:{name, arguments}}]
                if (msg.TryGetProperty("tool_calls", out var calls) && calls.ValueKind == JsonValueKind.Array && calls.GetArrayLength() > 0)
                {
                    var list = new List<ToolCallRequest>();
                    foreach (var c in calls.EnumerateArray())
                    {
                        if (!c.TryGetProperty("function", out var fn)) continue;
                        var name = fn.TryGetProperty("name", out var n) ? n.GetString() : null;
                        if (string.IsNullOrWhiteSpace(name)) continue;
                        string argsJson = "{}";
                        if (fn.TryGetProperty("arguments", out var args))
                        {
                            // Ollama may return arguments as object or stringified JSON
                            if (args.ValueKind == JsonValueKind.String)
                                argsJson = args.GetString() ?? "{}";
                            else if (args.ValueKind == JsonValueKind.Object)
                                argsJson = args.GetRawText();
                            else
                                argsJson = args.GetRawText();
                        }
                        list.Add(new ToolCallRequest("", name!, argsJson));
                    }
                    if (list.Count > 0) return new ToolCallOutcome(null, list);
                }
                // Fallback: Ollama may also return OpenAI-compatible choices[0].message.tool_calls
                if (msg.TryGetProperty("content", out var content) && content.ValueKind == JsonValueKind.String)
                {
                    var text = content.GetString();
                    if (!string.IsNullOrWhiteSpace(text))
                    {
                        var parsed = ToolCallParser.TryParse(text!);
                        if (parsed is not null && parsed.Count > 0) return new ToolCallOutcome(null, parsed);
                        return new ToolCallOutcome(text, null);
                    }
                }
            }
            // OpenAI-compatible fallback: choices[0].message.tool_calls
            if (doc.RootElement.TryGetProperty("choices", out var choices)
                && choices.ValueKind == JsonValueKind.Array && choices.GetArrayLength() > 0)
            {
                var m = choices[0].GetProperty("message");
                if (m.TryGetProperty("tool_calls", out var ocalls) && ocalls.ValueKind == JsonValueKind.Array)
                {
                    var list = new List<ToolCallRequest>();
                    foreach (var c in ocalls.EnumerateArray())
                    {
                        if (!c.TryGetProperty("function", out var fn)) continue;
                        var name = fn.TryGetProperty("name", out var n) ? n.GetString() : null;
                        if (string.IsNullOrWhiteSpace(name)) continue;
                        var args = fn.TryGetProperty("arguments", out var a) ? a.GetString() : null;
                        list.Add(new ToolCallRequest("", name!, args ?? "{}"));
                    }
                    if (list.Count > 0) return new ToolCallOutcome(null, list);
                }
            }
        }
        catch { }
        return new ToolCallOutcome(ProviderHttp.SafeExtractOllama(json), null);
    }
}
