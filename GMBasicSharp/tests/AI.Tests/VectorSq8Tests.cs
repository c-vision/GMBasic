// FILE: AI.Tests.VectorSq8Tests
// Storage SQ8 + scoring nel dominio int8 (parità C gmb_vecquant.c §0bis).
using Basic.SQLite.Vector;
using FluentAssertions;
using Microsoft.Data.Sqlite;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class VectorSq8Tests : IDisposable
{
    private readonly SqliteConnection _connection = new("Data Source=:memory:");

    public VectorSq8Tests() => _connection.Open();

    public void Dispose() => _connection.Dispose();

    [Fact]
    public void Sq8Codec_Layout_Scale_Plus_Payload()
    {
        var v = new float[] { 1f, -2f, 0.5f, 0f };
        var blob = VectorMath.ToSq8Blob(v);
        blob.Should().HaveCount(8);
        BitConverter.ToSingle(blob, 0).Should().BeApproximately(2f / 127f, 1e-7f);
        ((sbyte)blob[4]).Should().Be(64);
        ((sbyte)blob[5]).Should().Be(-127);
        ((sbyte)blob[6]).Should().Be(32);
        ((sbyte)blob[7]).Should().Be(0);
        VectorMath.IsSq8Blob(blob, 4).Should().BeTrue();
        VectorMath.IsSq8Blob(VectorMath.ToBlob(v), 4).Should().BeFalse();
    }

    [Fact]
    public void Sq8Codec_ZeroVector_ScaleZero()
    {
        var blob = VectorMath.ToSq8Blob(new float[4]);
        BitConverter.ToSingle(blob, 0).Should().Be(0f);
        blob[4..].Should().OnlyContain(b => b == 0);
    }

    [Fact]
    public void Sq8Cosine_Matches_Float32()
    {
        var rnd = new Random(7);
        for (var t = 0; t < 20; t++)
        {
            var a = Enumerable.Range(0, 32).Select(_ => (float)(rnd.NextDouble() * 2 - 1)).ToArray();
            var b = Enumerable.Range(0, 32).Select(_ => (float)(rnd.NextDouble() * 2 - 1)).ToArray();
            var (qa, _) = VectorMath.Sq8Encode(a);
            var (qb, _) = VectorMath.Sq8Encode(b);
            VectorMath.Sq8Cosine(qa, qb).Should().BeApproximately(VectorMath.CosineSimilarity(a, b), 0.02f);
        }
    }

    [Fact]
    public void Sq8L2_Matches_Float32()
    {
        var a = new float[] { 3f, 4f };
        var (qa, sa) = VectorMath.Sq8Encode(a);
        var (qz, sz) = VectorMath.Sq8Encode(new float[2]);
        VectorMath.Sq8L2(qa, sa, qz, sz).Should().BeApproximately(5f, 0.05f);
    }

    [Fact]
    public void EnsureCollection_Sq8_StoresSq8Blobs_And_SearchParity()
    {
        var indexer = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("sq8-test"));
        var fp = indexer.EnsureCollection("sq8-fp32", 8, VectorMetric.Cosine);
        var sq = indexer.EnsureCollection("sq8-coll", 8, VectorMetric.Cosine, format: "sq8");
        var docs = new[]
        {
            "il motore gestisce la memoria gerarchica",
            "il compilatore emette codice nativo",
            "la rete neurale apprende dai dati",
        };
        for (var i = 0; i < docs.Length; i++)
        {
            indexer.IndexText(fp, $"d{i}", docs[i]);
            indexer.IndexText(sq, $"d{i}", docs[i]);
        }
        // Storage: blob SQ8 = 4+8 byte, FP32 = 32 byte.
        BlobLength("sq8-coll").Should().Be(12);
        BlobLength("sq8-fp32").Should().Be(32);
        // Recall parity: stesso top-1 tra fp32 e sq8.
        var search = new SqlVectorSearch(_connection, new FixtureEmbeddingProvider("sq8-test"));
        var topFp = search.SearchByText(fp, "memoria gerarchica", 1).Results[0].SourceId;
        var topSq = search.SearchByText(sq, "memoria gerarchica", 1).Results[0].SourceId;
        topSq.Should().Be(topFp);
    }

    [Fact]
    public void EnsureCollection_WithoutFormat_KeepsFp32()
    {
        var indexer = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("sq8-test"));
        var coll = indexer.EnsureCollection("plain", 8, VectorMetric.Cosine);
        indexer.IndexText(coll, "d0", "testo di prova per formato default");
        BlobLength("plain").Should().Be(32);
    }

    [Fact]
    public void EnsureCollection_ExplicitFp32_AfterSq8_SwitchesBack()
    {
        var indexer = new SqlVectorIndexer(_connection, new FixtureEmbeddingProvider("sq8-test"));
        var coll = indexer.EnsureCollection("switch", 8, VectorMetric.Cosine, format: "sq8");
        indexer.IndexText(coll, "d0", "primo documento quantizzato");
        BlobLength("switch").Should().Be(12);
        indexer.EnsureCollection("switch", 8, VectorMetric.Cosine, format: "fp32");
        indexer.IndexText(coll, "d1", "secondo documento intero");
        // La riga nuova è fp32 (128 bit = 32 byte); la vecchia resta sq8 (rilevata per lunghezza).
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "SELECT length(e.embedding) FROM vec_embeddings e JOIN vec_chunks c ON c.id=e.chunk_id JOIN vec_documents d ON d.id=c.document_id JOIN vec_collections k ON k.id=d.collection_id WHERE k.name='switch' ORDER BY e.chunk_id";
        var lens = new List<long>();
        using var r = cmd.ExecuteReader();
        while (r.Read()) lens.Add(r.GetInt64(0));
        lens.Should().Equal(12L, 32L);
    }

    private long BlobLength(string collection)
    {
        using var cmd = _connection.CreateCommand();
        cmd.CommandText = "SELECT length(e.embedding) FROM vec_embeddings e JOIN vec_chunks c ON c.id=e.chunk_id JOIN vec_documents d ON d.id=c.document_id JOIN vec_collections k ON k.id=d.collection_id WHERE k.name=$name LIMIT 1";
        cmd.Parameters.AddWithValue("$name", collection);
        return Convert.ToInt64(cmd.ExecuteScalar());
    }
}
