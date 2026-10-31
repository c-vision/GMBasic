// FILE: Sqlite.Tests.Providers.HttpProviderTests
using System.Net;
using System.Text;
using System.Text.Json;
using Basic.Providers;
using Basic.SQLite.Vector;
using FluentAssertions;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class HttpProviderTests : IDisposable
{
    public HttpProviderTests() => ProviderServer.ResetForTests();

    public void Dispose() => ProviderServer.ResetForTests();

    [Fact]
    public void OllamaEmbedding_PostsEmbedAndFitsDimensions()
    {
        var handler = new StubHandler((req, _) =>
        {
            req.Method.Should().Be(HttpMethod.Post);
            req.RequestUri!.AbsolutePath.Should().Be("/api/embed");
            var body = req.Content!.ReadAsStringAsync().GetAwaiter().GetResult();
            body.Should().Contain("nomic-embed-text");
            body.Should().Contain("hello");

            var json = JsonSerializer.Serialize(new { embeddings = new[] { new[] { 1f, 0f, 0f, 0f } } });
            return JsonResponse(json);
        });

        var descriptor = new ProviderDescriptor(
            ProviderRole.Embedding,
            "ollama-emb",
            "ollama",
            new Dictionary<string, string>
            {
                ["baseUrl"] = "http://ollama.test",
                ["model"] = "nomic-embed-text",
                ["dimensions"] = "4"
            });

        var provider = new OllamaEmbeddingProvider(descriptor, new HttpClient(handler));
        var vector = provider.Embed("hello", 4);

        vector.Should().HaveCount(4);
        vector[0].Should().BeApproximately(1f, 0.001f);
        provider.ModelVersion.Should().Be("ollama:nomic-embed-text");
    }

    [Fact]
    public void OllamaCompletion_PostsChatAndReturnsContent()
    {
        var handler = new StubHandler((req, _) =>
        {
            req.RequestUri!.AbsolutePath.Should().Be("/api/chat");
            var json = JsonSerializer.Serialize(new { message = new { content = "Ollama says hi" } });
            return JsonResponse(json);
        });

        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion,
            "ollama-llm",
            "ollama",
            new Dictionary<string, string>
            {
                ["baseUrl"] = "http://ollama.test",
                ["model"] = "llama3"
            });

        var completer = new OllamaLlmCompleter(descriptor, new HttpClient(handler));
        completer.Complete("sys", "user").Should().Be("Ollama says hi");
        completer.ModelName.Should().Be("llama3");
    }

    [Fact]
    public void OpenAiEmbedding_SendsBearerAndParsesData()
    {
        var handler = new StubHandler((req, _) =>
        {
            req.Headers.Authorization!.Scheme.Should().Be("Bearer");
            req.Headers.Authorization!.Parameter.Should().Be("sk-test");
            req.RequestUri!.AbsolutePath.Should().Be("/v1/embeddings");

            var json = JsonSerializer.Serialize(new { data = new[] { new { embedding = new[] { 0f, 1f, 0f, 0f } } } });
            return JsonResponse(json);
        });

        var descriptor = new ProviderDescriptor(
            ProviderRole.Embedding,
            "openai-emb",
            "openai",
            new Dictionary<string, string>
            {
                ["apiKey"] = "sk-test",
                ["model"] = "text-embedding-3-small",
                ["dimensions"] = "4"
            });

        var provider = new OpenAiEmbeddingProvider(descriptor, new HttpClient(handler));
        var vector = provider.Embed("text", 4);
        vector[1].Should().BeApproximately(1f, 0.001f);
    }

    [Fact]
    public void OpenAiCompletion_ParsesChoicesMessage()
    {
        var handler = new StubHandler((req, _) =>
        {
            req.RequestUri!.AbsolutePath.Should().Be("/v1/chat/completions");
            var json = JsonSerializer.Serialize(new
            {
                choices = new[] { new { message = new { content = "OpenAI reply" } } }
            });
            return JsonResponse(json);
        });

        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion,
            "openai-llm",
            "openai",
            new Dictionary<string, string>
            {
                ["apiKey"] = "sk-test",
                ["model"] = "gpt-4o-mini"
            });

        var completer = new OpenAiLlmCompleter(descriptor, new HttpClient(handler));
        completer.Complete("sys", "user").Should().Be("OpenAI reply");
    }

    [Fact]
    public void AnthropicCompletion_SendsApiKeyHeaderAndParsesTextBlock()
    {
        var handler = new StubHandler((req, _) =>
        {
            req.RequestUri!.AbsolutePath.Should().Be("/v1/messages");
            req.Headers.GetValues("x-api-key").Should().ContainSingle().Which.Should().Be("sk-ant-test");
            req.Headers.GetValues("anthropic-version").Should().ContainSingle().Which.Should().Be("2023-06-01");

            var json = JsonSerializer.Serialize(new
            {
                content = new[] { new { type = "text", text = "Claude says hi" } }
            });
            return JsonResponse(json);
        });

        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion,
            "anthropic-llm",
            "anthropic",
            new Dictionary<string, string>
            {
                ["apiKey"] = "sk-ant-test",
                ["model"] = "claude-sonnet-5"
            });

        var completer = new AnthropicLlmCompleter(descriptor, new HttpClient(handler));
        completer.Complete("sys", "user").Should().Be("Claude says hi");
        completer.ModelName.Should().Be("claude-sonnet-5");
    }

    [Fact]
    public void ProviderFactory_CreatesOllamaAndOpenAiKinds()
    {
        var emb = ProviderFactory.CreateEmbedding(new ProviderDescriptor(
            ProviderRole.Embedding, "e", "ollama",
            new Dictionary<string, string> { ["model"] = "m", ["dimensions"] = "8" }));
        emb.Should().BeOfType<OllamaEmbeddingProvider>();

        var llm = ProviderFactory.CreateCompletion(new ProviderDescriptor(
            ProviderRole.Completion, "c", "openai",
            new Dictionary<string, string> { ["model"] = "gpt-4o-mini", ["apiKey"] = "k" }));
        llm.Should().BeOfType<OpenAiLlmCompleter>();

        var anthropic = ProviderFactory.CreateCompletion(new ProviderDescriptor(
            ProviderRole.Completion, "a", "anthropic",
            new Dictionary<string, string> { ["model"] = "claude-sonnet-5", ["apiKey"] = "k" }));
        anthropic.Should().BeOfType<AnthropicLlmCompleter>();

        FluentActions.Invoking(() => ProviderFactory.CreateEmbedding(new ProviderDescriptor(
                ProviderRole.Embedding, "a-emb", "anthropic",
                new Dictionary<string, string> { ["model"] = "claude-sonnet-5", ["apiKey"] = "k" })))
            .Should().Throw<InvalidOperationException>();
    }

    [Fact]
    public void ProviderServer_RegisterHttpKinds_WithoutCallingNetwork()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Embedding, "ollama-demo", "ollama",
            new Dictionary<string, string>
            {
                ["baseUrl"] = "http://127.0.0.1:1",
                ["model"] = "nomic-embed-text",
                ["dimensions"] = "384"
            });
        server.Register(ProviderRole.Completion, "openai-demo", "openai",
            new Dictionary<string, string>
            {
                ["apiKey"] = "sk-demo",
                ["model"] = "gpt-4o-mini"
            });

        server.List(ProviderRole.Embedding).Should().Contain("ollama-demo");
        server.List(ProviderRole.Completion).Should().Contain("openai-demo");
        server.GetActiveName(ProviderRole.Embedding).Should().Be("fixture-embedding");
    }

    [Fact]
    public void OllamaCompletion_StreamsDeltas()
    {
        var handler = new StubHandler((req, _) =>
        {
            req.RequestUri!.AbsolutePath.Should().Be("/api/chat");
            var body = req.Content!.ReadAsStringAsync().GetAwaiter().GetResult();
            body.Should().Contain("\"stream\":true");

            const string sse = "{\"model\":\"llama3\",\"message\":{\"role\":\"assistant\",\"content\":\"Ciao\"},\"done\":false}\n"
                             + "{\"model\":\"llama3\",\"message\":{\"role\":\"assistant\",\"content\":\" mondo\"},\"done\":false}\n"
                             + "{\"model\":\"llama3\",\"message\":{\"role\":\"assistant\",\"content\":\"\"},\"done\":true}\n";
            return JsonResponse(sse);
        });

        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion,
            "ollama-stream",
            "ollama",
            new Dictionary<string, string>
            {
                ["baseUrl"] = "http://ollama.test",
                ["model"] = "llama3"
            });

        var provider = new OllamaLlmCompleter(descriptor, new HttpClient(handler));
        var chunks = provider.CompleteStream("sys", "hello").ToList();

        string.Concat(chunks).Should().Be("Ciao mondo");
        chunks.Should().HaveCount(2); // i delta, non la risposta intera in un chunk
    }

    private static HttpResponseMessage JsonResponse(string json) =>
        new(HttpStatusCode.OK)
        {
            Content = new StringContent(json, Encoding.UTF8, "application/json")
        };

    private sealed class StubHandler(Func<HttpRequestMessage, CancellationToken, HttpResponseMessage> handler)
        : HttpMessageHandler
    {
        protected override Task<HttpResponseMessage> SendAsync(
            HttpRequestMessage request,
            CancellationToken cancellationToken) =>
            Task.FromResult(handler(request, cancellationToken));
    }
}
