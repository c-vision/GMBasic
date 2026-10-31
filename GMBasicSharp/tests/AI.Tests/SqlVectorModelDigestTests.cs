// FILE: AI.Tests.SqlVectorModelDigestTests
// Fase 3 del todo Tokenizer-WordPiece-Unicode-e-Retrieval-Sparse-Learned: guardia d'integrità
// del modello per-collection. Un vettore scritto col modello B non può entrare (né essere letto)
// in una collection dichiarata col modello A: l'indexer e lo search devono RIFIUTARE con errore
// esplicito che mostra i due digest — mai distanza cosine tra modelli diversi, in silenzio.
using Basic.SQLite.Vector;
using FluentAssertions;
using Microsoft.Data.Sqlite;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class SqlVectorModelDigestTests : IDisposable
{
    private readonly SqliteConnection _connection = new("Data Source=:memory:");

    public SqlVectorModelDigestTests() => _connection.Open();

    public void Dispose() => _connection.Dispose();

    private static long FindCollectionId(SqliteConnection connection, string name)
    {
        using var find = connection.CreateCommand();
        find.CommandText = "SELECT id FROM vec_collections WHERE name = $name";
        find.Parameters.AddWithValue("$name", name);
        var v = find.ExecuteScalar();
        if (v is long l) return l;
        if (v is int i) return i;
        throw new InvalidOperationException($"collection '{name}' non trovata");
    }

    [Fact]
    public void SameModel_IndexesAndDiscriminatesSources()
    {
        var indexer = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("model-a"));
        var coll = indexer.EnsureCollection("docs", 8, VectorMetric.Cosine);
        indexer.IndexText(coll, "s1", "hello world").Should().Be(IndexTextOutcome.Indexed);
        indexer.IndexText(coll, "s2", "goodbye world").Should().Be(IndexTextOutcome.Indexed);
        indexer.IndexText(coll, "s1", "hello world").Should().Be(IndexTextOutcome.Skipped);   // stesso source, hash invariato
    }

    [Fact]
    public void DifferentModel_IndexText_RejectedWithBothDigestsAndVersions()
    {
        var indexerA = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("model-a"));
        var coll = indexerA.EnsureCollection("docs", 8, VectorMetric.Cosine);
        indexerA.IndexText(coll, "s1", "hello world");

        var indexerB = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("model-b"));
        var ex = Assert.Throws<InvalidOperationException>(() => indexerB.IndexText(coll, "s2", "goodbye world"));
        ex.Message.Should().Contain("model-a");
        ex.Message.Should().Contain("model-b");
        ex.Message.Should().Contain("digest");
        ex.Message.Should().Contain("Rifiuto");
        // nessuna riga scritta dal modello B
        indexerA.CountEmbeddings(coll).Should().Be(1);
    }

    [Fact]
    public void DifferentModel_EnsureCollectionExisting_Rejected()
    {
        var indexerA = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("model-a"));
        indexerA.EnsureCollection("docs", 8, VectorMetric.Cosine);

        var indexerB = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("model-b"));
        Assert.Throws<InvalidOperationException>(() => indexerB.EnsureCollection("docs", 8, VectorMetric.Cosine));
    }

    [Fact]
    public void DifferentModel_Search_Rejected_NotSilentCosine()
    {
        // nota §3: leggere con il modello B una collection scritta col modello A NON deve
        // restituire cosine mute — deve partire l'errore col digest.
        var indexerA = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("model-a"));
        var coll = indexerA.EnsureCollection("docs", 8, VectorMetric.Cosine);
        indexerA.IndexText(coll, "s1", "the cat sat on the mat");

        var searchB = new SqlVectorSearch(_connection, new FixtureEmbeddingProvider("model-b"));
        Assert.Throws<InvalidOperationException>(() => searchB.SearchByText(coll, "cat", 2));
    }

    [Fact]
    public void LegacyDb_WithoutDigestColumn_IsMigrated_Backfilled_ThenEnforced()
    {
        // DB creati prima della fase 3: la colonna non esiste → EnsureSchema la aggiunge; il primo
        // accesso backfilla col provider attuale (nessun falso rifiuto), poi l'enforcement vale.
        using (var setup = _connection.CreateCommand())
        {
            setup.CommandText = """
                CREATE TABLE vec_collections (
                  id INTEGER PRIMARY KEY,
                  name TEXT NOT NULL UNIQUE,
                  dimensions INTEGER NOT NULL,
                  metric TEXT NOT NULL CHECK (metric IN ('cosine','l2')),
                  model_version TEXT NOT NULL DEFAULT 'fixture-v1',
                  created_at TEXT NOT NULL DEFAULT (datetime('now'))
                );
                INSERT INTO vec_collections (name, dimensions, metric, model_version)
                VALUES ('legacy', 8, 'cosine', 'legacy-model');
                """;
            setup.ExecuteNonQuery();
        }

        SqlVectorSchema.EnsureSchema(_connection);   // migrazione: aggiunge model_digest

        var id = FindCollectionId(_connection, "legacy");
        var indexer = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("model-a"));
        indexer.IndexText(id, "s1", "backfill and index").Should().Be(IndexTextOutcome.Indexed);

        // ora che il digest è stato backfillato, un altro modello viene RIFIUTATO
        var indexerB = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("model-b"));
        Assert.Throws<InvalidOperationException>(() => indexerB.IndexText(id, "s2", "no"));
    }
}