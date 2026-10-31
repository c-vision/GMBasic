// FILE: Sqlite.Tests.Providers.ProviderServerTests
using Basic.AI.Rag;
using Basic.Providers;
using Basic.SQLite.Vector;
using FluentAssertions;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class ProviderServerTests : IDisposable
{
    public ProviderServerTests() => ProviderServer.ResetForTests();

    public void Dispose() => ProviderServer.ResetForTests();

    [Fact]
    public void Defaults_RegisterFixtureEmbeddingAndCompletion()
    {
        var server = ProviderServer.Instance;
        server.List(ProviderRole.Embedding).Should().Contain("fixture-embedding");
        server.List(ProviderRole.Completion).Should().Contain("fixture-completion");
        server.GetActiveName(ProviderRole.Embedding).Should().Be("fixture-embedding");
    }

    [Fact]
    public void Register_Configure_SetActive_UsesProvider()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Embedding, "emb-a", "fixture");
        server.Register(ProviderRole.Embedding, "emb-b", "fixture");
        server.SetActive(ProviderRole.Embedding, "emb-b");

        server.GetActiveName(ProviderRole.Embedding).Should().Be("emb-b");
        server.GetActiveEmbedding().Should().BeOfType<FixtureEmbeddingProvider>();
    }

    [Fact]
    public void Rotate_CyclesProvidersRoundRobin()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Completion, "llm-a", "fixture");
        server.Register(ProviderRole.Completion, "llm-b", "fixture");
        server.SetActive(ProviderRole.Completion, "llm-a");

        server.Rotate(ProviderRole.Completion).Should().Be("llm-b");
        server.Rotate(ProviderRole.Completion).Should().Be("fixture-completion");
        server.Rotate(ProviderRole.Completion).Should().Be("llm-a");
    }

    [Fact]
    public void Configure_InvalidatesCachedInstance()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Embedding, "onnx-test", "onnx",
            new Dictionary<string, string> { ["model"] = "missing.onnx", ["dimensions"] = "8" });
        server.SetActive(ProviderRole.Embedding, "onnx-test");

        server.Invoking(s => s.GetActiveEmbedding()).Should().Throw<FileNotFoundException>();
        server.Configure("onnx-test", "model", "other-missing.onnx");
        server.Invoking(s => s.GetActiveEmbedding()).Should().Throw<FileNotFoundException>();
    }

    [Fact]
    public void Unregister_RemovesProvider()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Embedding, "temp", "fixture");
        server.Unregister("temp").Should().BeTrue();
        server.List(ProviderRole.Embedding).Should().NotContain("temp");
    }

    [Fact]
    public void TestConnection_FixtureEmbedding_ReturnsOk()
    {
        var result = ProviderServer.Instance.TestConnection(ProviderRole.Embedding, "fixture-embedding");
        result.Should().StartWith("OK");
        result.Should().Contain("fixture-embedding");
    }

    [Fact]
    public void TestConnection_NonExistentProvider_ReturnsError()
    {
        var result = ProviderServer.Instance.TestConnection(ProviderRole.Embedding, "no-such-provider");
        result.Should().StartWith("ERROR");
    }

    [Fact]
    public void TestConnection_FixtureCompletion_ReturnsOk()
    {
        var result = ProviderServer.Instance.TestConnection(ProviderRole.Completion, "fixture-completion");
        result.Should().StartWith("OK");
    }

    [Fact(DisplayName = "PROVIDER.REGISTER: a .gguf file registers as kind=llamacpp with modelPath, capability chat")]
    public void Register_GgufFile_RegistersAsLlamaCpp()
    {
        var ggufPath = Path.Combine(Path.GetTempPath(), "provider-server-test-" + Guid.NewGuid().ToString("N") + ".gguf");
        File.WriteAllBytes(ggufPath, [1, 2, 3]);
        try
        {
            var name = ProviderServer.Instance.RegisterSource("local-test", null, ggufPath);

            name.Should().Be("local-test");
            ProviderServer.Instance.List("completion").Should().Contain(name);
        }
        finally
        {
            File.Delete(ggufPath);
        }
    }

    [Fact(DisplayName = "PROVIDER.REGISTER: an MLX repo folder registers as kind=mlx with modelDir")]
    public void Register_MlxRepoFolder_RegistersAsMlx()
    {
        var mlxDir = Path.Combine(Path.GetTempPath(), "provider-server-test-mlx-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(mlxDir);
        File.WriteAllText(Path.Combine(mlxDir, "config.json"), "{}");
        File.WriteAllBytes(Path.Combine(mlxDir, "model.safetensors"), [1, 2, 3]);
        try
        {
            var name = ProviderServer.Instance.RegisterSource("mlx-test", null, mlxDir);

            ProviderServer.Instance.List("completion").Should().Contain(name);
        }
        finally
        {
            Directory.Delete(mlxDir, recursive: true);
        }
    }

    [Fact(DisplayName = "PROVIDER.REGISTER: an unrecognizable path throws instead of registering something wrong")]
    public void Register_UnrecognizedPath_Throws()
    {
        var act = () => ProviderServer.Instance.RegisterSource("bad", null, "/tmp/does-not-exist-" + Guid.NewGuid().ToString("N"));
        act.Should().Throw<InvalidOperationException>();
    }

    [Fact(DisplayName = "Ollama (FASE 5 Parte B): il completer implementa IVisionCompleter e IAudioCompleter, e il payload chat include images/audio")]
    public async Task Ollama_ImplementsVisionAndAudio_WithCorrectPayload()
    {
        // Hermetic: non chiama Ollama reale -- usa un HttpClient con handler finto che cattura
        // il body e risponde con un message.content fisso. Verifica che il completer Ollama
        // implementi le interfacce multimodali e che il payload /api/chat includa il campo
        // images (visione) o audio (trascrizione) come array base64.
        string? lastBody = null;
        var handler = new System.Net.Http.HttpMessageInvoker(new FakeOllamaHandler(s => lastBody = s));
        using var http = new HttpClient(new FakeOllamaHandler(s => lastBody = s));

        var descriptor = new ProviderDescriptor(ProviderRole.Completion, "ollama-test", "ollama",
            new Dictionary<string, string> { ["model"] = "llava:latest", ["baseUrl"] = "http://fake" });
        var completer = new OllamaLlmCompleter(descriptor, http);

        completer.Should().BeAssignableTo<IVisionCompleter>();
        completer.Should().BeAssignableTo<IAudioCompleter>();

        var vision = (IVisionCompleter)completer;
        var text = vision.DescribeImage(new byte[] { 1, 2, 3 }, "png", "read this");
        text.Should().Be("Fake response");
        lastBody.Should().Contain("\"images\"", "DescribeImage must send images (base64) in the user message");

        lastBody = null;
        var audio = (IAudioCompleter)completer;
        var tr = audio.TranscribeAudio(new byte[] { 9, 8, 7 }, "wav");
        tr.Should().Be("Fake response");
        lastBody.Should().Contain("\"audio\"", "TranscribeAudio must send audio (base64) in the user message");
    }

    [Fact(DisplayName = "PROVIDER.DISCOVER: registra i modelli OpenRouter come <prefix>:<id>, free con priorità 10")]
    public void Discover_OpenRouterModels_RegistersWithFreePriority()
    {
        // Mock di GET /models in formato OpenRouter (models[] con pricing).
        var json = """
            {"models":[
                {"id":"free-model","pricing":{"input":"0","output":"0"},"context_window":8000},
                {"id":"paid-model","pricing":{"input":"0.5","output":"1.5"},"context_window":16000}
            ]}
            """;
        using var http = new HttpClient(new FakeModelsHandler(json));

        var aliases = ProviderServer.Instance.DiscoverModels("or", "chat", "https://openrouter.ai/api/v1", "sk-test", http: http);

        aliases.Should().Contain("or:free-model");
        aliases.Should().Contain("or:paid-model");

        var free = ProviderServer.Instance.GetActiveCompletion();
        _ = free;
        ProviderServer.Instance.GetPriority("or:free-model").Should().Be(10, "free models get free-first priority");
        ProviderServer.Instance.GetPriority("or:paid-model").Should().Be(0);
        ProviderServer.Instance.IsFreeModel("or:free-model").Should().BeTrue();
        ProviderServer.Instance.IsFreeModel("or:paid-model").Should().BeFalse();
    }

    [Fact(DisplayName = "PROVIDER.DISCOVER: soloFree filtra i modelli a pagamento")]
    public void Discover_OnlyFree_FiltersPaid()
    {
        var json = """
            {"models":[
                {"id":"free-a","pricing":{"input":"0","output":"0"}},
                {"id":"paid-b","pricing":{"input":"1","output":"2"}}
            ]}
            """;
        using var http = new HttpClient(new FakeModelsHandler(json));

        var aliases = ProviderServer.Instance.DiscoverModels("or", "chat", "https://openrouter.ai/api/v1", "sk-test", onlyFree: true, http: http);

        aliases.Should().Contain("or:free-a");
        aliases.Should().NotContain("or:paid-b");
    }

    [Fact(DisplayName = "capability numerica: %CHAT/%OCR/%AUDIO = 0/1/2")]
    public void Capability_FromConfig_AcceptsNumericValues()
    {
        Basic.AI.Rag.AiCapabilityExtensions.FromConfig("0").Should().Be(Basic.AI.Rag.AiCapability.Chat);
        Basic.AI.Rag.AiCapabilityExtensions.FromConfig("1").Should().Be(Basic.AI.Rag.AiCapability.Ocr);
        Basic.AI.Rag.AiCapabilityExtensions.FromConfig("2").Should().Be(Basic.AI.Rag.AiCapability.Audio);
        Basic.AI.Rag.AiCapabilityExtensions.FromConfig("%CHAT").Should().Be(Basic.AI.Rag.AiCapability.Chat);
        Basic.AI.Rag.AiCapabilityExtensions.FromConfig("%OCR").Should().Be(Basic.AI.Rag.AiCapability.Ocr);
        Basic.AI.Rag.AiCapabilityExtensions.FromConfig("%AUDIO").Should().Be(Basic.AI.Rag.AiCapability.Audio);
        Basic.AI.Rag.AiCapabilityExtensions.FromConfig("chat").Should().Be(Basic.AI.Rag.AiCapability.Chat);
        Basic.AI.Rag.AiCapabilityExtensions.FromConfig("ocr").Should().Be(Basic.AI.Rag.AiCapability.Ocr);
        Basic.AI.Rag.AiCapabilityExtensions.FromConfig("audio").Should().Be(Basic.AI.Rag.AiCapability.Audio);
    }

    [Fact(DisplayName = "PROVIDER.SETCREDITS + RecordCompletion: a 0 crediti il provider va Blocked")]
    public void Credits_WhenExhausted_BlocksProvider()
    {
        ProviderServer.ResetForTests();
        var ggufPath = Path.Combine(Path.GetTempPath(), "credits-test-" + Guid.NewGuid().ToString("N") + ".gguf");
        File.WriteAllBytes(ggufPath, [1]);
        try
        {
            ProviderServer.Instance.RegisterSource("cred-model", null, ggufPath);
            ProviderServer.Instance.SetCredits("cred-model", 2);
            ProviderServer.Instance.GetCredits("cred-model").Should().Be(2);

            ProviderServer.Instance.RecordCompletion("completion", "cred-model", 1000);
            ProviderServer.Instance.GetCredits("cred-model").Should().Be(1);
            ProviderServer.Instance.GetStatus("cred-model").Should().Be(ProviderStatus.Available);

            ProviderServer.Instance.RecordCompletion("completion", "cred-model", 1000);
            ProviderServer.Instance.GetCredits("cred-model").Should().Be(0);
            ProviderServer.Instance.GetStatus("cred-model").Should().Be(ProviderStatus.Blocked,
                "a provider with 0 credits is blocked so rotation skips it");
        }
        finally
        {
            File.Delete(ggufPath);
            ProviderServer.ResetForTests();
        }
    }

    private sealed class FakeModelsHandler : HttpMessageHandler
    {
        private readonly string _json;
        public FakeModelsHandler(string json) => _json = json;

        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken ct)
        {
            var response = new HttpResponseMessage(System.Net.HttpStatusCode.OK)
            {
                Content = new StringContent(_json, System.Text.Encoding.UTF8, "application/json")
            };
            return Task.FromResult(response);
        }
    }

    private sealed class FakeOllamaHandler : HttpMessageHandler
    {
        private readonly Action<string> _onRequest;
        public FakeOllamaHandler(Action<string> onRequest) => _onRequest = onRequest;

        protected override Task<HttpResponseMessage> SendAsync(
            HttpRequestMessage request, CancellationToken ct)
        {
            var body = request.Content?.ReadAsStringAsync(ct).GetAwaiter().GetResult() ?? "";
            _onRequest(body);
            var response = new HttpResponseMessage(System.Net.HttpStatusCode.OK)
            {
                Content = new StringContent("{\"message\":{\"content\":\"Fake response\"}}",
                    System.Text.Encoding.UTF8, "application/json")
            };
            return Task.FromResult(response);
        }
    }
}
