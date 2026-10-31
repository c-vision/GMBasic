// FILE: AI.Tests.RerankerTests
using Basic.Providers;
using Basic.SQLite.Vector;
using FluentAssertions;
using Microsoft.Data.Sqlite;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class RerankerTests : IDisposable
{
    public RerankerTests() => ProviderServer.ResetForTests();

    public void Dispose() => ProviderServer.ResetForTests();

    [Fact]
    public void KeywordReranker_ScoresHigherOnMoreTermOverlap()
    {
        var reranker = new KeywordReranker();

        var high = reranker.Score("reti neurali profonde", "le reti neurali profonde sono potenti");
        var low = reranker.Score("reti neurali profonde", "il gatto dorme sul divano");

        high.Should().BeGreaterThan(low);
    }

    [Fact]
    public void KeywordReranker_EmptyQuery_ReturnsZero()
    {
        new KeywordReranker().Score("", "qualsiasi testo").Should().Be(0f);
    }

    [Fact]
    public void RerankerScoring_Rerank_OrdersByScoreDescendingAndRespectsTopK()
    {
        var results = new List<VectorSearchResult>
        {
            new(1, 1, "a", "gatto cane", 0.1f, "v1", null),
            new(2, 2, "b", "reti neurali profonde", 0.1f, "v1", null),
            new(3, 3, "c", "reti neurali", 0.1f, "v1", null),
        };

        var reranked = RerankerScoring.Rerank(new KeywordReranker(), "reti neurali profonde", results, 2);

        reranked.Should().HaveCount(2);
        reranked[0].SourceId.Should().Be("b");
        reranked[1].SourceId.Should().Be("c");
    }

    [Fact]
    public void RerankerScoring_EmptyQuery_ReturnsFirstTopKUnchanged()
    {
        var results = new List<VectorSearchResult>
        {
            new(1, 1, "a", "x", 0.9f, "v1", null),
            new(2, 2, "b", "y", 0.1f, "v1", null),
        };

        var reranked = RerankerScoring.Rerank(new KeywordReranker(), "", results, 1);

        reranked.Should().HaveCount(1);
        reranked[0].SourceId.Should().Be("a");
    }

    [Fact]
    public void ProviderFactory_CreatesKeywordReranker()
    {
        var reranker = ProviderFactory.CreateReranker(new ProviderDescriptor(
            ProviderRole.Reranker, "kw", "keyword"));
        reranker.Should().BeOfType<KeywordReranker>();
    }

    [Fact]
    public void ProviderFactory_UnsupportedRerankerKind_Throws()
    {
        var act = () => ProviderFactory.CreateReranker(new ProviderDescriptor(
            ProviderRole.Reranker, "bogus", "bogus-kind"));
        act.Should().Throw<InvalidOperationException>();
    }

    [Fact]
    public void ProviderFactory_OnnxRerankerWithoutModelConfig_Throws()
    {
        var act = () => ProviderFactory.CreateReranker(new ProviderDescriptor(
            ProviderRole.Reranker, "onnx-r", "onnx"));
        act.Should().Throw<InvalidOperationException>();
    }

    [Fact]
    public void ProviderServer_BootstrapsDefaultKeywordReranker()
    {
        var server = ProviderServer.Instance;
        server.List(ProviderRole.Reranker).Should().Contain("keyword-reranker");
        server.GetActiveReranker().Should().BeOfType<KeywordReranker>();
    }

    [Fact]
    public void ProviderServer_RegisterAndActivateCustomReranker_ParticipatesInRotationByPriority()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Reranker, "custom-reranker", "keyword");
        server.SetPriority("custom-reranker", 100);

        server.GetActiveName(ProviderRole.Reranker).Should().Be("custom-reranker");
    }

    [Fact]
    public void ProviderServer_BlockedReranker_IsSkippedByActiveSelection()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Reranker, "custom-reranker", "keyword");
        server.SetPriority("custom-reranker", 100);
        server.SetStatus("custom-reranker", ProviderStatus.Blocked);

        server.GetActiveName(ProviderRole.Reranker).Should().Be("keyword-reranker");
    }

    [Fact]
    public void ProviderServer_TestConnection_ReportsRerankerScore()
    {
        var result = ProviderServer.Instance.TestConnection(ProviderRole.Reranker, "keyword-reranker");
        result.Should().StartWith("OK");
        result.Should().Contain("reranker");
    }

    // --- HybridSearchByText + reranker + evidence gating (P1-1 Second-Brain-Retrieval-Upgrades) ---

    private static VectorSearchResponse Hybrid(string query, int topK, IReranker? reranker,
        double evidenceMin, params string[] texts)
    {
        using var conn = new SqliteConnection("Data Source=:memory:");
        conn.Open();
        // P0a: il fixture non è più un default — nei test va passato ESPPLICITAMENTE.
        var fixture = new FixtureEmbeddingProvider();
        var indexer = new SqlVectorIndexer(conn, fixture);
        var collectionId = indexer.EnsureCollection("kb", 32, VectorMetric.Cosine);
        for (var i = 0; i < texts.Length; i++)
            indexer.IndexText(collectionId, $"doc-{i}", texts[i]);
        var search = new SqlVectorSearch(conn, fixture);
        return search.HybridSearchByText(collectionId, query, topK,
            reranker: reranker, rerankEvidenceMin: evidenceMin);
    }

    [Fact]
    public void Hybrid_Rerank_MovesLexicallyRelevant_AheadOfVectorOnly()
    {
        // doc-0 ha poche parole in comune ma potrebbe vincere per embedding sulla fixture;
        // doc-1 condivide quasi tutti i termini della query. Col reranker keyword, doc-1 va in testa.
        var plain = Hybrid("reti neurali profonde", 2, reranker: null, evidenceMin: double.NegativeInfinity,
            "il gatto dorme sul divano e sogna il pesce",
            "le reti neurali profonde servono per il riconoscimento di immagini",
            "le reti neurali profonde sono potenti e profonde");
        var reranked = Hybrid("reti neurali profonde", 2, reranker: new KeywordReranker(), evidenceMin: double.NegativeInfinity,
            "il gatto dorme sul divano e sogna il pesce",
            "le reti neurali profonde servono per il riconoscimento di immagini",
            "le reti neurali profonde sono potenti e profonde");

        plain.Status.Should().Be(VectorSearchStatus.Success);
        reranked.Status.Should().Be(VectorSearchStatus.Success);
        reranked.Results.Should().NotBeEmpty();
        // il top1 rerankato deve contenere i termini della query (doc che condividono i termini)
        var topLexical = reranked.Results.First();
        topLexical.Text.Contains("reti neurali profonde", StringComparison.Ordinal)
            .Should().BeTrue();
    }

    [Fact]
    public void Hybrid_RerankEvidenceMin_MarksInsufficient_WhenTopBelowThreshold()
    {
        var reranked = Hybrid("xyz abc", 2, reranker: new KeywordReranker(), evidenceMin: 0.9,
            "testo che non contiene alcun termine della query di ricerca",
            "altro contenuto completamente scollegato");

        // keyword score = matches/terms → 0/2 = 0 → sotto la soglia 0.9
        reranked.Status.Should().Be(VectorSearchStatus.InsufficientEvidence);
        reranked.Results.Should().NotBeEmpty(); // i risultati restano, ma lo stato segnala evidenza debole
    }

    [Fact]
    public void Hybrid_RerankEvidenceBelow_KeepsSuccess()
    {
        var reranked = Hybrid("reti neurali profonde", 2, reranker: new KeywordReranker(), evidenceMin: 0.1,
            "le reti neurali profonde servono per il riconoscimento di immagini",
            "altre parole qualsiasi qui dentro");

        reranked.Status.Should().Be(VectorSearchStatus.Success);
        reranked.Results.First().Text.Contains("reti neurali profonde", StringComparison.Ordinal)
            .Should().BeTrue();
    }

    [Fact]
    public void Hybrid_Explain_ReportsBackendRanks()
    {
        // includeExplain non è esposto dalla helper Hybrid: lo chiamo direttamente
        using var conn = new SqliteConnection("Data Source=:memory:");
        conn.Open();
        // P0a: fixture esplicito (non più default silenzioso).
        var fixture = new FixtureEmbeddingProvider();
        var indexer = new SqlVectorIndexer(conn, fixture);
        var collectionId = indexer.EnsureCollection("kb-exp", 32, VectorMetric.Cosine);
        indexer.IndexText(collectionId, "doc-1", "reti neurali profonde per il riconoscimento di immagini");
        indexer.IndexText(collectionId, "doc-2", "il gatto dorme sul divano e sogna il pesce");
        var search = new SqlVectorSearch(conn, fixture);
        var plain = search.HybridSearchByText(collectionId, "reti neurali profonde", 3);
        plain.Results.All(r => r.ExplainJson == null).Should().BeTrue("default: nessun explain");

        var exp = search.HybridSearchByText(collectionId, "reti neurali profonde", 3, includeExplain: true);
        exp.Status.Should().Be(VectorSearchStatus.Success);
        exp.Results.Should().NotBeEmpty();
        exp.Results.First().ExplainJson.Should().NotBeNull();
        exp.Results.First().ExplainJson!.Should().Contain("vectorRank");
        exp.Results.First().ExplainJson!.Should().Contain("ftsRank");
        exp.Results.First().ExplainJson!.Should().Contain("rrfScore");
    }
}
