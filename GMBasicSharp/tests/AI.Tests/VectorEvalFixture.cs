// FILE: Sqlite.Tests.Vector.VectorEvalFixture
using Basic.SQLite.Vector;

namespace AI.Tests;

/// <summary>Labeled query → chunk pairs for Recall@K evaluation.</summary>
public static class VectorEvalFixture
{
    public sealed record EvalCase(string Query, string ChunkText, string SourceId);

    public static IReadOnlyList<EvalCase> Cases { get; } =
    [
        new("revenue management optimization for airline networks", "revenue management optimization for airline networks", "doc-rm"),
        new("dynamic pricing engine adjusts fares in real time", "dynamic pricing engine adjusts fares in real time", "doc-dp"),
        new("NDC order management retailing standard", "NDC order management retailing standard", "doc-ndc"),
        new("WAL transaction durability on embedded engines", "WAL transaction durability on embedded engines", "doc-wal"),
        new("B+ tree page split under concurrent inserts", "B+ tree page split under concurrent inserts", "doc-btree"),
        new("vector cosine similarity for semantic retrieval", "vector cosine similarity for semantic retrieval", "doc-vec"),
        new("green thread scheduler cooperative multitasking", "green thread scheduler cooperative multitasking", "doc-gpm"),
        new("Clipper DBF memo field DBT format", "Clipper DBF memo field DBT format", "doc-dbf"),
        new("ledger double entry journal posting", "ledger double entry journal posting", "doc-ledger"),
        new("LSM compaction policy for SPEED engine", "LSM compaction policy for SPEED engine", "doc-speed"),
        new("SQL foreign key pragma enforcement", "SQL foreign key pragma enforcement", "doc-sql"),
        new("agent memory persistence across sessions", "agent memory persistence across sessions", "doc-agent"),
        new("MCP tool registration JSON schema", "MCP tool registration JSON schema", "doc-mcp"),
        new("embedding model version lineage tracking", "embedding model version lineage tracking", "doc-embed"),
        new("chunk overlap strategy for long documents", "chunk overlap strategy for long documents", "doc-chunk"),
        new("metadata access control role filtering", "metadata access control role filtering", "doc-rbac"),
        new("hybrid FTS search with vector RRF", "hybrid FTS search with vector RRF", "doc-fts"),
        new("audit query logging for compliance", "audit query logging for compliance", "doc-audit"),
        new("insufficient evidence threshold on low scores", "insufficient evidence threshold on low scores", "doc-thresh"),
        new("incremental index upsert skip unchanged hash", "incremental index upsert skip unchanged hash", "doc-upsert")
    ];

    public static void SeedCorpus(SqlVectorIndexer indexer, long collectionId, IEmbeddingProvider provider)
    {
        foreach (var c in Cases)
            indexer.IndexText(collectionId, c.SourceId, c.ChunkText);

        // Distractors — similar length, different topics
        for (var i = 0; i < 30; i++)
            indexer.IndexText(collectionId, $"noise-{i}", $"filler document topic {i} unrelated content padding");
    }

    public static double MeasureRecallAtK(SqlVectorSearch search, long collectionId, int k)
    {
        var hits = 0;
        foreach (var eval in Cases)
        {
            var response = search.SearchByText(collectionId, eval.Query, k,
                options: new VectorSearchOptions(WriteAudit: false));
            if (response.Results.Any(r => r.SourceId == eval.SourceId))
                hits++;
        }

        return (double)hits / Cases.Count;
    }
}
