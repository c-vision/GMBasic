// FILE: AI.Tests.EnricherTests
// §2 Second Brain — Enricher: SUMMARIZATION/TAGGING/ENTITY_EXTRACTION via LLM attivo
// (FixtureLlmCompleter deterministica), QA.ANSWER grounded con citations da un brain RAG.
// Isolato da config utente reale (OverridePathForTests + DataSession isolato).
using Basic.AI;
using Basic.AI.Knowledge;
using Basic.AI.Rag;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class EnricherTests : IDisposable
{
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _ragDir = Path.Combine(Path.GetTempPath(), "gmbasic-enricher-" + Guid.NewGuid().ToString("N"));
    private readonly string _registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-enricher-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly Basic.Databases.Rdd.DataSession _ds;
    private readonly Basic.Databases.Rdd.DataSession? _prevSqlSession;
    private readonly Basic.SQLite.Vector.IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly ILlmCompleter? _prevCompleter;

    public EnricherTests()
    {
        AiConfig.OverridePathForTests(_configPath);
        AiConfig.RagDefaultDir = _ragDir;
        BrainRegistry.OverridePathForTests(_registryPath);
        _ds = new Basic.Databases.Rdd.DataSession();
        _prevSqlSession = AiVectorRuntime.SqlDataSession;
        _prevEmb = Basic.SQLite.Vector.EmbeddingProviderResolver.Default;
        _prevDims = Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions;
        _prevCompleter = LlmCompleterResolver.Default;
        AiVectorRuntime.SqlDataSession = _ds;
        LlmCompleterResolver.Default = new FixtureLlmCompleter();
    }

    public void Dispose()
    {
        // Ripristina lo stato globale PRE-ESISTENTE (il singleton è condiviso tra le classi
        // di test: azzerare a null rompe i test che girano dopo e si aspettano il valore).
        LlmCompleterResolver.Default = _prevCompleter;
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = _prevEmb;
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = _prevDims;
        AiVectorRuntime.SqlDataSession = _prevSqlSession;
        _ds.CloseAll();
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        try { Directory.Delete(_ragDir, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    private void MakeRagBrain(string name, string content, string collection = "wiki")
    {
        var dbPath = Path.Combine(_ragDir, name + ".db");
        var fixture = new Basic.SQLite.Vector.FixtureEmbeddingProvider();
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = fixture;
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = 8;
        Basic.AI.Rag.RagDbSession.Open(dbPath, name);
        AiVectorRuntime.EnsureCollection(name, collection, 8);
        AiVectorRuntime.IndexText(name, collection, "/src/" + name + ".md", content);
        BrainRegistry.Register(name, mode: "rag");
    }

    [Fact]
    public void Enrich_NoCompleter_ReturnsExplicitError()
    {
        LlmCompleterResolver.Default = null;
        var r = Enricher.Enrich("testo", EnrichMode.Summarization);
        r.Should().Contain("nessun provider LLM");
    }

    [Fact]
    public void Enrich_Summarization_ReturnsNonEmptyWithCompleter()
    {
        var r = Enricher.Enrich("Vector databases enable semantic retrieval over unstructured knowledge.", EnrichMode.Summarization);
        r.Should().NotBeNullOrWhiteSpace();
    }

    [Fact]
    public void Enrich_EmptyText_ReturnsEmpty()
    {
        Enricher.Enrich("   ", EnrichMode.Tagging).Should().Be("");
    }

    [Fact]
    public void SuggestTags_ReturnsNonEmpty()
    {
        var r = Enricher.SuggestTags("trading strategy with RSI and momentum indicators on futures");
        r.Should().NotBeNullOrWhiteSpace();
    }

    [Fact]
    public void QaAnswer_BrainNotAvailable_ReturnsErrorJson()
    {
        var r = Enricher.QaAnswer("what?", "missing-brain");
        r.Should().Contain("\"Error\"");
        r.Should().Contain("non disponibile come rag");
    }

    [Fact]
    public void QaAnswer_GroundedAnswerWithCitations()
    {
        MakeRagBrain("kb", "The capital of France is Paris. The Eiffel Tower is in Paris.");
        var r = Enricher.QaAnswer("What is the capital of France?", "kb", contextWindow: 3);
        r.Should().Contain("\"Success\"");
        r.Should().Contain("capital of France");
        r.Should().Contain("citations");
    }

    [Fact]
    public void QaAnswer_NoRelevantChunk_ReturnsInsufficient()
    {
        // Collezione VUOTA (nessun chunk indicizzato) → hybrid search 0 risultati.
        var dbPath = Path.Combine(_ragDir, "empty-kb.db");
        var fixture = new Basic.SQLite.Vector.FixtureEmbeddingProvider();
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = fixture;
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = 8;
        Basic.AI.Rag.RagDbSession.Open(dbPath, "empty-kb");
        AiVectorRuntime.EnsureCollection("empty-kb", "wiki", 8);
        BrainRegistry.Register("empty-kb", mode: "rag");

        var r = Enricher.QaAnswer("quantum computing algebra", "empty-kb", contextWindow: 1);
        r.Should().Contain("InsufficientEvidence");
    }
}
