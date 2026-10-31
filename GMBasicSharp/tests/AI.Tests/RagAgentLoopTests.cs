// FILE: AI.Tests.RagAgentLoopTests
// RagAgentLoop (2026-09-07): tool rag.* esposti agli agenti (search/context/ask sul retrieval
// vettoriale multi-brain). Il dispatch è NATIVO C# verso MultiBrainSearch — testa Execute su
// brain RAG reali su file (fixture embedding deterministico), più la registrazione nel
// registry MCP. Isolato (save/restore dei singleton globali, DataSession dedicato) per non
// interferire con la parallelizzazione della suite (pattern MultiBrainTests).
using Basic.AI;
using Basic.AI.Rag;
using Basic.AI.Knowledge;
using Basic.Agents;
using Basic.Mcp;
using Basic.SQLite.Vector;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class RagAgentLoopTests : IDisposable
{
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _brainsRoot = Path.Combine(Path.GetTempPath(), "gmbasic-brains-" + Guid.NewGuid().ToString("N"));
    private readonly string _registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-brains-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _ragDir = Path.Combine(Path.GetTempPath(), "gmbasic-rag-" + Guid.NewGuid().ToString("N"));
    private readonly IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly Basic.Databases.Rdd.DataSession? _prevSqlSession;
    private readonly Basic.Databases.Rdd.DataSession _ds;

    public RagAgentLoopTests()
    {
        AiConfig.OverridePathForTests(_configPath);
        AiConfig.BrainsDefaultDir = _brainsRoot;
        AiConfig.RagDefaultDir = _ragDir;
        BrainRegistry.OverridePathForTests(_registryPath);
        _prevEmb = EmbeddingProviderResolver.Default;
        _prevDims = EmbeddingProviderResolver.ActiveDimensions;
        _prevSqlSession = AiVectorRuntime.SqlDataSession;
        _ds = new Basic.Databases.Rdd.DataSession();
        AiVectorRuntime.SqlDataSession = _ds;
        McpToolRegistry.ResetForTests();
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        EmbeddingProviderResolver.Default = _prevEmb;
        EmbeddingProviderResolver.ActiveDimensions = _prevDims;
        AiVectorRuntime.SqlDataSession = _prevSqlSession;
        _ds.CloseAll();
        McpToolRegistry.ResetForTests();
        try { Directory.Delete(_brainsRoot, recursive: true); } catch { }
        try { Directory.Delete(_ragDir, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    private void MakeRagBrain(string name, string content, string collection = "wiki")
    {
        var dbPath = Path.Combine(_ragDir, name + ".db");
        var fixture = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.Default = fixture;
        EmbeddingProviderResolver.ActiveDimensions = 8;
        RagDbSession.Open(dbPath, name);
        AiVectorRuntime.EnsureCollection(name, collection, 8);
        AiVectorRuntime.IndexText(name, collection, "/src/" + name + ".md", content);
        BrainRegistry.Register(name, mode: "rag");
    }

    [Fact(DisplayName = "RagAgentLoop.Register: espone i tool rag.* nel registry")]
    public void Register_ExposesRagTools()
    {
        RagAgentLoop.Register();
        var names = McpToolRegistry.Instance.List().Select(t => t.Name).ToList();
        names.Should().Contain("rag.search");
        names.Should().Contain("rag.context");
        names.Should().Contain("rag.ask");
    }

    [Fact(DisplayName = "RagAgentLoop.Execute: rag.search ritorna hit multi-brain")]
    public void Execute_Search_ReturnsHits()
    {
        MakeRagBrain("ra", "risk parity allocates bonds and equities");
        MakeRagBrain("rb", "momentum factor for intraday trading");

        var res = RagAgentLoop.Execute("rag.search", """{"query":"risk parity","topK":3,"brain":"ra"}""");
        res.Should().NotStartWith("error:");
        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("hits");
    }

    [Fact(DisplayName = "RagAgentLoop.Execute: rag.search senza brain collegati → errore esplicito")]
    public void Execute_Search_NoBrains_Error()
    {
        var res = RagAgentLoop.Execute("rag.search", """{"query":"anything"}""");
        res.Should().Contain("InsufficientEvidence");
    }

    [Fact(DisplayName = "RagAgentLoop.Execute: rag.context ritorna testo grounded con attribuzione")]
    public void Execute_Context_ReturnsGroundedText()
    {
        MakeRagBrain("rc", "risk parity allocates bonds and equities");
        BrainRegistry.Use("rc");

        var res = RagAgentLoop.Execute("rag.context", """{"query":"risk parity"}""");
        res.Should().NotStartWith("error:");
        res.Should().Contain("risk parity");
    }

    [Fact(DisplayName = "RagAgentLoop.Execute: tool sconosciuto / argomenti mancanti → errore esplicito")]
    public void Execute_Invalid_ReturnsError()
    {
        RagAgentLoop.Execute("rag.nope", "{}").Should().StartWith("error:");
        RagAgentLoop.Execute("rag.search", "{}").Should().StartWith("error:");
    }

    [Fact(DisplayName = "RagAgentLoop.Dispatcher: McpToolCallResult coerente")]
    public void Dispatcher_ReturnsToolCallResult()
    {
        MakeRagBrain("rd", "value investing focuses on intrinsic value");
        BrainRegistry.Use("rd");

        var dispatcher = new RagAgentLoop.Dispatcher();
        var result = dispatcher.DispatchAsync("rag.search", """{"query":"value"}""", CancellationToken.None)
            .GetAwaiter().GetResult();
        result.IsError.Should().BeFalse();
        result.TextContent.Should().Contain("hits");
    }
}
