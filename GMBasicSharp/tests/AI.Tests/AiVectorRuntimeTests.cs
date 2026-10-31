// FILE: AI.Tests.AiVectorRuntimeTests
using Basic.SQLite;
using Basic.SQLite.Vector;
using Basic.AI.Rag;
using FluentAssertions;
using Microsoft.Data.Sqlite;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
[Collection("BrainGlobals")]
public sealed class AiVectorRuntimeTests : IDisposable
{
    private readonly SqliteConnection _connection = new("Data Source=:memory:");
    private const string Alias = "vec-runtime-test";

    public AiVectorRuntimeTests()
    {
        var ds = new Basic.Databases.Rdd.DataSession();
        AiVectorRuntime.SqlDataSession = ds;
        var driver = new Basic.Databases.Rdd.SqlDriver();
        driver.Open(new Basic.Databases.Rdd.DbOpenInfo { FileName = "Data Source=:memory:", Alias = Alias, Driver = "SQLite" });
        ds.RegisterDirect(ds.AllocateArea(), driver, Alias);
        _connection.Open();
        // P0a2 (decisione utente 2026-09-12): il resolver NON ha più un default installato dal
        // costruttore di PCodeVm — chi usa la pipeline vettoriale dichiara il provider. Questo
        // test suite verifica la pipeline, quindi espone il fixture ESPLICITAMENTE (scelta del
        // test, non più un comportamento silenzioso di produzione).
        EmbeddingProviderResolver.Default = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.ActiveDimensions = 32;
    }

    [Fact]
    public void SearchJson_Explain_True_AddsBackendRanks()
    {
        AiVectorRuntime.IndexText(Alias, "kb5", "exp-doc",
            "reti neurali profonde per il riconoscimento di immagini con profondita");
        var plain = AiVectorRuntime.SearchJson(Alias, "kb5", "reti neurali profonde", 3, hybrid: true);
        plain.Should().Contain("\"vectorRank\":null");   // default: explain assente → campi null

        var explained = AiVectorRuntime.SearchJson(Alias, "kb5", "reti neurali profonde", 3, hybrid: true,
            reranker: null, rerankEvidenceMin: double.NaN, explain: true);
        explained.Should().NotContain("\"vectorRank\":null"); // con explain i rank sono valorizzati
        explained.Should().Contain("\"ftsRank\"");
        explained.Should().Contain("\"rrfScore\"");
        explained.Should().Contain("exp-doc");
    }

    public void Dispose()
    {
        AiVectorRuntime.SqlDataSession?.CloseAll();
        EmbeddingProviderResolver.ResetForTests();
        _connection.Dispose();
    }

    [Fact]
    public void SearchJson_ReturnsHits_AfterIndex()
    {
        AiVectorRuntime.IndexText(Alias, "kb", "doc-1", "semantic retrieval with vector database");
        var json = AiVectorRuntime.SearchJson(Alias, "kb", "vector database", topK: 3, hybrid: true);
        json.Should().Contain("doc-1");
        json.Should().Contain("\"status\"");
    }

    [Fact]
    public void IndexMeta_WithTenant_FiltersSearch()
    {
        AiVectorRuntime.IndexTextWithMetadata(Alias, "kb", "t1-doc",
            "tenant one knowledge base article",
            """{"status":"current","tenant":"tenant-a","role":"analyst"}""");
        AiVectorRuntime.IndexTextWithMetadata(Alias, "kb", "t2-doc",
            "tenant two separate article",
            """{"status":"current","tenant":"tenant-b","role":"analyst"}""");

        var json = AiVectorRuntime.SearchJson(Alias, "kb", "knowledge base", 5, true,
            """{"tenant":"tenant-a"}""");
        json.Should().Contain("t1-doc");
        json.Should().NotContain("t2-doc");
    }

    [Fact]
    public void RemoveSource_AndCount_Work()
    {
        AiVectorRuntime.IndexText(Alias, "kb", "rm-me", "temporary document to remove");
        AiVectorRuntime.CountEmbeddings(Alias, "kb").Should().BeGreaterThan(0);
        AiVectorRuntime.RemoveSource(Alias, "kb", "rm-me");
        var json = AiVectorRuntime.SearchJson(Alias, "kb", "temporary", 3, false);
        json.Should().NotContain("rm-me");
    }

    // --- P1-2: wiring DefaultReranker (opt-in, default OFF) in SearchJson/Ask/RagContext ---

    [Fact]
    public void DefaultReranker_Off_BehavesAsBefore_NoRerankerScores()
    {
        var prev = AiVectorRuntime.DefaultReranker;
        var prevEv = AiVectorRuntime.DefaultRerankEvidenceMin;
        AiVectorRuntime.DefaultReranker = null;
        AiVectorRuntime.DefaultRerankEvidenceMin = double.NegativeInfinity;
        try
        {
            AiVectorRuntime.IndexText(Alias, "kb2", "d1", "le reti neurali profonde sono potenti");
            var json = AiVectorRuntime.SearchJson(Alias, "kb2", "reti neurali profonde", 3, hybrid: true);
            json.Should().Contain("d1");
            json.Should().NotContain("keyword-reranker"); // nessun reranker coinvolto
        }
        finally
        {
            AiVectorRuntime.DefaultReranker = prev;
            AiVectorRuntime.DefaultRerankEvidenceMin = prevEv;
        }
    }

    [Fact]
    public void DefaultReranker_Keyword_Orders_And_Ask_UsesIt()
    {
        var prev = AiVectorRuntime.DefaultReranker;
        var prevEv = AiVectorRuntime.DefaultRerankEvidenceMin;
        AiVectorRuntime.DefaultReranker = new KeywordReranker();
        AiVectorRuntime.DefaultRerankEvidenceMin = double.NegativeInfinity;
        try
        {
            AiVectorRuntime.IndexText(Alias, "kb3", "forte",
                "le reti neurali profonde servono per il riconoscimento di immagini con profondita");
            AiVectorRuntime.IndexText(Alias, "kb3", "debole",
                "il gatto dorme sul divano e sogna tranquillo il suo pesce");

            var ctx = AiVectorRuntime.RagContext(Alias, "kb3", "reti neurali profonde riconoscimento", 3);
            ctx.Should().Contain("forte");

            var json = AiVectorRuntime.SearchJson(Alias, "kb3", "reti neurali profonde", 3, hybrid: true);
            var idxForte = json.IndexOf("forte", System.StringComparison.Ordinal);
            var idxDebole = json.IndexOf("debole", System.StringComparison.Ordinal);
            idxForte.Should().BeGreaterThan(0);
            idxDebole.Should().BeGreaterThan(idxForte, "il reranker keyword deve portare il doc forte prima del debole");
        }
        finally
        {
            AiVectorRuntime.DefaultReranker = prev;
            AiVectorRuntime.DefaultRerankEvidenceMin = prevEv;
        }
    }

    [Fact]
    public void DefaultReranker_EvidenceMin_Ask_ReturnsInsufficient_WhenWeak()
    {
        var prev = AiVectorRuntime.DefaultReranker;
        var prevEv = AiVectorRuntime.DefaultRerankEvidenceMin;
        AiVectorRuntime.DefaultReranker = new KeywordReranker();
        AiVectorRuntime.DefaultRerankEvidenceMin = 0.9; // il doc debole non raggiunge quasi mai 0.9
        try
        {
            AiVectorRuntime.IndexText(Alias, "kb4", "debole",
                "il gatto dorme sul divano e sogna il suo pesce colorato");
            var json = AiVectorRuntime.SearchJson(Alias, "kb4", "reti neurali profonde", 3, hybrid: true);
            json.Should().Contain("\"InsufficientEvidence\"");
        }
        finally
        {
            AiVectorRuntime.DefaultReranker = prev;
            AiVectorRuntime.DefaultRerankEvidenceMin = prevEv;
        }
    }

    [Fact]
    public void EnsureCollection_WithDimensions_CreatesCollection()
    {
        var id = AiVectorRuntime.EnsureCollection(Alias, "dims-test", 16);
        id.Should().BeGreaterThan(0);
    }

    [Fact]
    public void IndexText_ReturnsOutcomeCodes()
    {
        var indexed = AiVectorRuntime.IndexText(Alias, "kb", "outcome-test", "first version");
        indexed.Should().Be(IndexTextOutcome.Indexed);

        var updated = AiVectorRuntime.IndexText(Alias, "kb", "outcome-test", "second version");
        updated.Should().Be(IndexTextOutcome.Updated);

        var skipped = AiVectorRuntime.IndexText(Alias, "kb", "outcome-test", "second version");
        skipped.Should().Be(IndexTextOutcome.Skipped);
    }

    [Fact]
    public void PurgeDeprecated_RemovesEmbeddings()
    {
        AiVectorRuntime.IndexTextWithMetadata(Alias, "kb", "purge-me", "doc to purge",
            """{"status":"deprecated"}""");
        AiVectorRuntime.PurgeDeprecated(Alias, "kb").Should().BeGreaterThan(0);
        var json = AiVectorRuntime.SearchJson(Alias, "kb", "purge", 3, false);
        json.Should().Contain("\"status\":\"InsufficientEvidence\"");
    }

    [Fact]
    public void SearchJson_EmptyCollection_ReturnsInsufficientEvidence()
    {
        var json = AiVectorRuntime.SearchJson(Alias, "empty-coll", "anything", 3, false);
        json.Should().Contain("InsufficientEvidence");
    }

    [Fact]
    public void RagContext_ReturnsFormattedText()
    {
        AiVectorRuntime.IndexText(Alias, "kb", "rag-ctx-1", "first document for context test");
        var ctx = AiVectorRuntime.RagContext(Alias, "kb", "first document", 3);
        ctx.Should().Contain("first");
        ctx.Should().Contain("source_id=rag-ctx-1");
    }

    [Fact]
    public void Ask_WithNoCompleter_ReturnsErrorMessage()
    {
        LlmCompleterResolver.Default = null;
        AiVectorRuntime.IndexText(Alias, "kb", "ask-1", "document to answer from");
        var answer = AiVectorRuntime.Ask(Alias, "kb", "what is this?", 3);
        answer.Should().Contain("Error");
    }

    [Fact]
    public void ListCollections_ReturnsJsonArray()
    {
        AiVectorRuntime.EnsureCollection(Alias, "coll-a", 8);
        AiVectorRuntime.EnsureCollection(Alias, "coll-b", 16);
        var json = AiVectorRuntime.ListCollections(Alias);
        json.Should().Contain("coll-a");
        json.Should().Contain("coll-b");
        json.Should().Contain("\"dimensions\"");
    }

    [Fact]
    public void CrossCollection_Isolation()
    {
        AiVectorRuntime.IndexText(Alias, "coll-x", "doc-x", "data in collection x");
        AiVectorRuntime.IndexText(Alias, "coll-y", "doc-y", "data in collection y");
        var jsonX = AiVectorRuntime.SearchJson(Alias, "coll-y", "collection x", 3, false);
        jsonX.Should().NotContain("doc-x");
    }
}
