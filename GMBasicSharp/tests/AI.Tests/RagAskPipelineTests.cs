// FILE: Sqlite.Tests.Vector.RagAskPipelineTests
using Basic.SQLite.Vector;
using Basic.AI.Rag;
using FluentAssertions;
using Microsoft.Data.Sqlite;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class RagAskPipelineTests : IDisposable
{
    private readonly SqliteConnection _connection = new("Data Source=:memory:");

    public RagAskPipelineTests()
    {
        LlmCompleterResolver.Default = new FixtureLlmCompleter();
        _connection.Open();
        // P0a: fixture esplicito (non più default silenzioso del costruttore).
        _fixture = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.Default = _fixture;
        EmbeddingProviderResolver.ActiveDimensions = 32;
    }

    private readonly FixtureEmbeddingProvider _fixture;

    [Fact]
    public void Ask_ReturnsSnippetFromTopHit_OnIndexedCorpus()
    {
        var indexer = new SqlVectorIndexer(_connection, _fixture);
        var collectionId = indexer.EnsureCollection("kb", 32, VectorMetric.Cosine);
        indexer.IndexText(collectionId, "doc-1",
            "Vector databases enable semantic retrieval over unstructured knowledge bases.");

        var search = new SqlVectorSearch(_connection, _fixture);
        var response = search.HybridSearchByText(collectionId, "semantic retrieval", topK: 3);
        var bundle = RagContextBundle.FromResults(response.Results);

        var answer = RagAskPipeline.Ask(bundle, "What enables semantic retrieval?");

        answer.Should().Contain("semantic retrieval");
        answer.Should().Contain("What enables semantic retrieval?");
    }

    [Fact]
    public void Ask_EmptyCorpus_ReturnsInsufficientMessage()
    {
        var indexer = new SqlVectorIndexer(_connection, _fixture);
        indexer.EnsureCollection("empty", 32, VectorMetric.Cosine);
        var bundle = RagContextBundle.FromResults([]);

        var answer = RagAskPipeline.Ask(bundle, "anything");

        answer.Should().Contain("insufficient evidence");
    }

    [Fact]
    public void VectorSettings_TryLoadFromFile_ReturnsNullWhenMissing()
    {
        VectorSettings.TryLoadFromFile(Path.Combine(Path.GetTempPath(), $"no-settings-{Guid.NewGuid():N}.json"))
            .Should().BeNull();
    }

    public void Dispose() => _connection.Dispose();
}
