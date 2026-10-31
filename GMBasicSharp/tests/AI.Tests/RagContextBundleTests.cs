// FILE: Sqlite.Tests.Vector.RagContextBundleTests

using Basic.SQLite;
using Basic.SQLite.Vector;
using Basic.AI.Rag;
using FluentAssertions;
using Microsoft.Data.Sqlite;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class RagContextBundleTests : IDisposable
{
    private readonly SqliteConnection _connection;

    public RagContextBundleTests()
    {
        _connection = new SqliteConnection("Data Source=:memory:");
        _connection.Open();
        using var pragma = _connection.CreateCommand();
        pragma.CommandText = "PRAGMA foreign_keys=ON";
        pragma.ExecuteNonQuery();
    }

    public void Dispose() => _connection.Dispose();

    [Fact]
    public void FromResults_ContainsExpectedSourceId()
    {
        var provider = new FixtureEmbeddingProvider();
        var indexer = new SqlVectorIndexer(_connection, provider);
        var collectionId = indexer.EnsureCollection("rag", 32, VectorMetric.Cosine);
        VectorEvalFixture.SeedCorpus(indexer, collectionId, provider);

        const string query = "vector cosine similarity for semantic retrieval";
        var search = new SqlVectorSearch(_connection, provider);
        var response = search.SearchByText(collectionId, query, topK: 3,
            options: new VectorSearchOptions(WriteAudit: false));

        var bundle = RagContextBundle.FromResults(response.Results);

        bundle.Attributions.Should().NotBeEmpty();
        bundle.Attributions.Should().Contain(a => a.SourceId == "doc-vec");
        bundle.Context.Should().Contain("doc-vec");
        bundle.Context.Should().Contain("source_id=");
    }
}
