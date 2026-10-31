// FILE: Basic.SQLite.Vector.SqlVectorSearch
using System.Diagnostics;
using System.Globalization;
using System.Collections.Concurrent;
using System.Text.Json;
using Microsoft.Data.Sqlite;

namespace Basic.SQLite.Vector;

/// <summary>Vector search with brute-force and lazy in-memory HNSW ANN paths.</summary>
public sealed class SqlVectorSearch
{
    private static readonly JsonSerializerOptions AuditJson = new() { WriteIndented = false };

    private readonly SqliteConnection _connection;
    private readonly IEmbeddingProvider _embeddingProvider;
    private readonly ConcurrentDictionary<long, CachedIndex> _indexCache = new();
    private readonly object _rebuildLock = new();

    /// <summary>Shared per-connection lock (see <see cref="SqlConnectionLocks"/>) — the SAME
    /// lock instance used by SqlVectorIndexer when wrapping the same connection, so that reads
    /// here can't collide with concurrent writer CreateCommand/Dispose calls on the shared
    /// SqliteConnection (ConcurrentSearch_WhileIndexing_DoesNotCrash).</summary>
    private readonly object _connectionLock;

    public SqlVectorSearch(SqliteConnection connection, IEmbeddingProvider? embeddingProvider = null)
    {
        ArgumentNullException.ThrowIfNull(connection);
        _connection = connection;
        // P0a (todo Tokenizer-WordPiece, decisione utente 2026-09-12): senza provider embedding
        // esplicito è ERRORE, mai fallback silenzioso sul fixture (stessa regola di SqlVectorIndexer).
        _embeddingProvider = embeddingProvider
            ?? throw new InvalidOperationException(
                "Nessun provider embedding attivo: registrane uno reale (AI SET ACTIVE %EMBEDDING, alias) o passa un provider esplicito.");
        _connectionLock = SqlConnectionLocks.For(connection);
        // Same shared-lock requirement as SqlVectorIndexer's constructor: DDL on _connection
        // must not run concurrently with another thread's open transaction on it.
        lock (_connectionLock)
        {
            SqlVectorSchema.EnsureSchema(_connection);
        }
    }

    public VectorSearchResponse Search(
        long collectionId,
        ReadOnlySpan<float> queryEmbedding,
        int topK,
        VectorSearchFilter? filter = null,
        VectorSearchOptions? options = null)
    {
        if (topK <= 0)
            throw new ArgumentOutOfRangeException(nameof(topK));

        filter ??= new VectorSearchFilter();
        options ??= new VectorSearchOptions();
        var sw = Stopwatch.StartNew();

        lock (_connectionLock)
        {
            SqlVectorModelGuard.GuardOrBackfill(_connection, collectionId, _embeddingProvider);
            var metric = GetCollectionMetric(collectionId);
            var dimensions = GetCollectionDimensions(collectionId);
            if (queryEmbedding.Length != dimensions)
                throw new ArgumentException($"Query embedding length {queryEmbedding.Length} != collection dimensions {dimensions}.");

            var candidates = LoadCandidates(collectionId, filter);
            var useSq8 = GetCollectionFormatSq8(collectionId);
            var results = ShouldUseApproximateIndex(candidates.Count, options)
                ? SearchApproximate(collectionId, queryEmbedding, metric, topK, filter, options, candidates, useSq8)
                : RankExact(queryEmbedding, metric, topK, filter, candidates, useSq8);

            var status = results.Count == 0 ||
                         (filter.MinScore > float.NegativeInfinity && results[0].Score < filter.MinScore)
                ? VectorSearchStatus.InsufficientEvidence
                : VectorSearchStatus.Success;

            sw.Stop();
            if (options.WriteAudit)
                WriteAudit(collectionId, options.QueryText, filter, topK, status, results, (int)sw.ElapsedMilliseconds);

            return new VectorSearchResponse(status, results);
        }
    }

    public VectorSearchResponse SearchByText(
        long collectionId,
        string queryText,
        int topK,
        VectorSearchFilter? filter = null,
        VectorSearchOptions? options = null)
    {
        ArgumentException.ThrowIfNullOrEmpty(queryText);
        int dimensions;
        lock (_connectionLock)
        {
            SqlVectorModelGuard.GuardOrBackfill(_connection, collectionId, _embeddingProvider);
            dimensions = GetCollectionDimensions(collectionId);
        }
        var query = _embeddingProvider.Embed(queryText, dimensions);
        options = options is null
            ? new VectorSearchOptions(QueryText: queryText)
            : options with { QueryText = options.QueryText ?? queryText };
        return Search(collectionId, query, topK, filter, options);
    }

    /// <summary>Hybrid vector + FTS5 search merged with reciprocal rank fusion.</summary>
    /// <param name="reranker">Se non null: dopo la fusione RRF, riordina un pool di candidati
    /// con il reranker (cross-encoder/lexical) e prende topK — parità meccanismo di QMD/fnd
    /// (retrieval → rerank → topK). Null (default) = comportamento storico invariato.</param>
    /// <param name="rerankEvidenceMin">Quando è finito e il reranker è attivo: se il miglior
    /// punteggio RERANKED (0..1 per keyword/cross-encoder normalizzati) è sotto soglia →
    /// InsufficientEvidence. -inf (default) = nessun gate aggiuntivo.</param>
    public VectorSearchResponse HybridSearchByText(
        long collectionId,
        string queryText,
        int topK,
        VectorSearchFilter? filter = null,
        VectorSearchOptions? options = null,
        HybridSearchOptions? hybrid = null,
        IReranker? reranker = null,
        double rerankEvidenceMin = double.NegativeInfinity,
        bool includeExplain = false)
    {
        ArgumentException.ThrowIfNullOrEmpty(queryText);
        if (topK <= 0)
            throw new ArgumentOutOfRangeException(nameof(topK));

        filter ??= new VectorSearchFilter();
        options ??= new VectorSearchOptions(QueryText: queryText);
        hybrid ??= new HybridSearchOptions();
        var pool = Math.Max(topK, hybrid.CandidatePoolSize);
        // quando c'è il reranker il pool finale deve dare margine di riordino (≥3× topK)
        var target = reranker != null ? Math.Max(topK * 3, pool) : topK;
        var sw = Stopwatch.StartNew();

        lock (_connectionLock)
        {
            SqlVectorModelGuard.GuardOrBackfill(_connection, collectionId, _embeddingProvider);
            var dimensions = GetCollectionDimensions(collectionId);
            var queryEmbed = _embeddingProvider.Embed(queryText, dimensions);
            var metric = GetCollectionMetric(collectionId);
            var candidates = LoadCandidates(collectionId, filter);
            var byChunk = candidates.ToDictionary(c => c.ChunkId);
            var useSq8 = GetCollectionFormatSq8(collectionId);

            var vectorRanked = RankVector(queryEmbed, metric, filter, candidates, pool, useSq8);
            var ftsRanked = SqlVectorFts.Search(_connection, collectionId, queryText, pool)
                .Where(id => byChunk.ContainsKey(id) && filter.Matches(
                    byChunk[id].Status, byChunk[id].TenantId, byChunk[id].AccessRoles,
                    byChunk[id].ModelVersion, byChunk[id].SourceId, byChunk[id].MetadataJson))
                .ToList();

            // explain (P2-3): rank 1-based per backend per chunk (0 = non presente in quel backend)
            Dictionary<long, (int V, int F)>? ranks = null;
            if (includeExplain)
            {
                ranks = new Dictionary<long, (int, int)>();
                for (var i = 0; i < vectorRanked.Count; i++) ranks[vectorRanked[i]] = (i + 1, ranks.GetValueOrDefault(vectorRanked[i]).F);
                for (var i = 0; i < ftsRanked.Count; i++) ranks[ftsRanked[i]] = (ranks.GetValueOrDefault(ftsRanked[i]).V, i + 1);
            }

            var fused = ReciprocalRankFusion.Merge(vectorRanked, ftsRanked, hybrid);
            var results = new List<VectorSearchResult>();
            foreach (var (chunkId, rrfScore) in fused)
            {
                if (results.Count >= target) break;
                if (!byChunk.TryGetValue(chunkId, out var c)) continue;
                if (rrfScore < filter.MinScore) continue;

                string? explainJson = null;
                if (ranks != null && ranks.TryGetValue(chunkId, out var rk))
                    explainJson = $"{{\"vectorRank\":{rk.V},\"ftsRank\":{rk.F},\"rrfScore\":{rrfScore.ToString("G6", System.Globalization.CultureInfo.InvariantCulture)}}}";

                results.Add(new VectorSearchResult(
                    c.ChunkId,
                    c.DocumentId,
                    c.SourceId,
                    c.Text,
                    (float)rrfScore,
                    c.ModelVersion,
                    c.MetadataJson,
                    explainJson));
            }

            // Rerank (QMD/fnd): riordina il pool col reranker, poi topK. Score finale = del reranker.
            if (reranker != null && results.Count > 0)
                results = RerankerScoring.Rerank(reranker, queryText, results, topK);

            var status = results.Count == 0 ||
                         (reranker != null && double.IsFinite(rerankEvidenceMin) &&
                          results[0].Score < rerankEvidenceMin)
                ? VectorSearchStatus.InsufficientEvidence
                : VectorSearchStatus.Success;
            sw.Stop();
            if (options.WriteAudit)
                WriteAudit(collectionId, options.QueryText, filter, topK, status, results, (int)sw.ElapsedMilliseconds, options.ShadowModelVersion);

            // Shadow search: run with alternative model if configured
            if (!string.IsNullOrEmpty(options.ShadowModelVersion) && options.WriteAudit)
            {
                var shadowProvider = GetShadowProvider(options.ShadowModelVersion);
                if (shadowProvider != null)
                {
                    var shadowEmbed = shadowProvider.Embed(queryText, dimensions);
                    var shadowResults = RankExact(shadowEmbed, metric, topK, filter, candidates, useSq8);
                    if (shadowResults.Count > 0)
                    {
                        WriteAudit(collectionId, options.QueryText, filter, topK, status, shadowResults,
                            (int)sw.ElapsedMilliseconds, options.ShadowModelVersion, isShadow: true);
                    }
                }
            }

            return new VectorSearchResponse(status, results);
        }
    }

    public int CountAuditRows()
    {
        lock (_connectionLock)
        {
            return ScalarInt("SELECT COUNT(*) FROM vec_query_audit");
        }
    }

    private static bool ShouldUseApproximateIndex(int count, VectorSearchOptions options) =>
        options.UseApproximateIndex ?? count >= options.ApproximateIndexMinCount;

    private List<VectorSearchResult> SearchApproximate(
        long collectionId,
        ReadOnlySpan<float> queryEmbedding,
        VectorMetric metric,
        int topK,
        VectorSearchFilter filter,
        VectorSearchOptions options,
        List<CandidateRow> candidates,
        bool useSq8 = false)
    {
        if (candidates.Count == 0)
            return [];

        var index = GetOrBuildIndex(collectionId, metric, candidates, queryEmbedding.Length);
        var oversample = Math.Max(topK * 3, topK + 10);
        var ef = Math.Max(options.ApproximateEfSearch, oversample);
        var chunkIds = index.Search(queryEmbedding, oversample, ef);
        var byChunk = candidates.ToDictionary(c => c.ChunkId);
        var scored = new List<VectorSearchResult>(chunkIds.Count);
        sbyte[]? qq = null;
        float qs = 0f;
        if (useSq8) (qq, qs) = VectorMath.Sq8EncodeQuery(queryEmbedding);
        var dims = queryEmbedding.Length;

        foreach (var chunkId in chunkIds)
        {
            if (!byChunk.TryGetValue(chunkId, out var c))
                continue;
            if (!filter.Matches(c.Status, c.TenantId, c.AccessRoles, c.ModelVersion, c.SourceId, c.MetadataJson))
                continue;

            float score = useSq8 && qq is not null && VectorMath.IsSq8Blob(c.Embedding, dims)
                ? VectorMath.ScoreSq8(metric, qq, qs, c.Embedding, dims)
                : VectorMath.Score(metric, queryEmbedding, VectorMath.FromBlob(c.Embedding));
            if (score < filter.MinScore)
                continue;

            scored.Add(ToResult(c, score));
        }

        if (scored.Count < topK)
            return RankExact(queryEmbedding, metric, topK, filter, candidates);

        scored.Sort((a, b) => b.Score.CompareTo(a.Score));
        return scored.Count <= topK ? scored : scored.GetRange(0, topK);
    }

    private List<VectorSearchResult> RankExact(
        ReadOnlySpan<float> queryEmbedding,
        VectorMetric metric,
        int topK,
        VectorSearchFilter filter,
        List<CandidateRow> candidates,
        bool useSq8 = false)
    {
        var scored = new List<VectorSearchResult>(candidates.Count);
        sbyte[]? qq = null;
        float qs = 0f;
        if (useSq8) (qq, qs) = VectorMath.Sq8EncodeQuery(queryEmbedding);
        var dims = queryEmbedding.Length;
        foreach (var c in candidates)
        {
            if (!filter.Matches(c.Status, c.TenantId, c.AccessRoles, c.ModelVersion, c.SourceId, c.MetadataJson))
                continue;

            float score = useSq8 && qq is not null && VectorMath.IsSq8Blob(c.Embedding, dims)
                ? VectorMath.ScoreSq8(metric, qq, qs, c.Embedding, dims)
                : VectorMath.Score(metric, queryEmbedding, VectorMath.FromBlob(c.Embedding));
            if (score < filter.MinScore)
                continue;

            scored.Add(ToResult(c, score));
        }

        scored.Sort((a, b) => b.Score.CompareTo(a.Score));
        return scored.Count <= topK ? scored : scored.GetRange(0, topK);
    }

    private VectorHnswIndex GetOrBuildIndex(long collectionId, VectorMetric metric, List<CandidateRow> candidates, int dimensions)
    {
        // Use distinct chunk count to detect insertions (avoids full rebuild on duplicate model_version rows)
        var distinctCount = candidates.Select(c => c.ChunkId).Distinct().Count();

        // Fast path: cache hit with matching count
        if (_indexCache.TryGetValue(collectionId, out var cached) &&
            cached.Count == distinctCount &&
            cached.Metric == metric)
            return cached.Index;

        // Rebuild under lock to prevent concurrent rebuilds
        lock (_rebuildLock)
        {
            // Double-check after acquiring lock
            if (_indexCache.TryGetValue(collectionId, out var recheck) &&
                recheck.Count == distinctCount &&
                recheck.Metric == metric)
                return recheck.Index;

            var index = new VectorHnswIndex(metric);
            foreach (var c in candidates)
                // Navigazione HNSW resta FP32 (decode una tantum in build); lo scoring
                // finale usa ScoreSq8 sui blob — rescore quantizzato, non navigazione.
                index.Add(c.ChunkId, VectorMath.FromBlobAny(c.Embedding, dimensions));
            _indexCache[collectionId] = new CachedIndex(distinctCount, metric, index);
            return index;
        }
    }

    private static List<long> RankVector(
        ReadOnlySpan<float> queryEmbedding,
        VectorMetric metric,
        VectorSearchFilter filter,
        List<CandidateRow> candidates,
        int pool,
        bool useSq8 = false)
    {
        var scored = new List<(long ChunkId, float Score)>(candidates.Count);
        sbyte[]? qq = null;
        float qs = 0f;
        if (useSq8) (qq, qs) = VectorMath.Sq8EncodeQuery(queryEmbedding);
        var dims = queryEmbedding.Length;
        foreach (var c in candidates)
        {
            if (!filter.Matches(c.Status, c.TenantId, c.AccessRoles, c.ModelVersion, c.SourceId, c.MetadataJson))
                continue;
            float score = useSq8 && qq is not null && VectorMath.IsSq8Blob(c.Embedding, dims)
                ? VectorMath.ScoreSq8(metric, qq, qs, c.Embedding, dims)
                : VectorMath.Score(metric, queryEmbedding, VectorMath.FromBlob(c.Embedding));
            if (score < filter.MinScore)
                continue;
            scored.Add((c.ChunkId, score));
        }

        scored.Sort((a, b) => b.Score.CompareTo(a.Score));
        return scored.Take(pool).Select(s => s.ChunkId).ToList();
    }

    private static VectorSearchResult ToResult(CandidateRow c, float score) =>
        new(c.ChunkId, c.DocumentId, c.SourceId, c.Text, score, c.ModelVersion, c.MetadataJson);

    private void WriteAudit(
        long collectionId,
        string? queryText,
        VectorSearchFilter filter,
        int topK,
        VectorSearchStatus status,
        IReadOnlyList<VectorSearchResult> results,
        int latencyMs,
        string? shadowModelVersion = null,
        bool isShadow = false)
    {
        var resultsJson = SerializeAuditResults(results);
        var filtersJson = JsonSerializer.Serialize(new
        {
            filter.RequiredStatus,
            filter.TenantId,
            filter.RequiredRole,
            filter.ModelVersion,
            MinScore = float.IsFinite(filter.MinScore) ? filter.MinScore : (float?)null
        }, AuditJson);

        using var command = _connection.CreateCommand();
        command.CommandText = """
            INSERT INTO vec_query_audit
              (collection_id, query_text, model_version, filters_json, top_k, results_json, latency_ms, status, is_shadow)
            VALUES ($cid, $q, $model, $filters, $k, $results, $ms, $status, $shadow)
            """;
        command.Parameters.AddWithValue("$cid", collectionId);
        command.Parameters.AddWithValue("$q", (object?)queryText ?? DBNull.Value);
        command.Parameters.AddWithValue("$model", (object?)(shadowModelVersion ?? filter.ModelVersion) ?? DBNull.Value);
        command.Parameters.AddWithValue("$filters", filtersJson);
        command.Parameters.AddWithValue("$k", topK);
        command.Parameters.AddWithValue("$results", resultsJson);
        command.Parameters.AddWithValue("$ms", latencyMs);
        command.Parameters.AddWithValue("$status", isShadow ? "Shadow" : status.ToString());
        command.Parameters.AddWithValue("$shadow", isShadow);
        command.ExecuteNonQuery();
    }

    private IEmbeddingProvider? GetShadowProvider(string modelVersion)
    {
        // Try to get shadow provider from active embedding provider
        try
        {
            var active = EmbeddingProviderResolver.Default;
            return active ?? null;
        }
        catch (InvalidOperationException)
        {
            return null;
        }
        catch (ArgumentException)
        {
            return null;
        }
    }

    private static string SerializeAuditResults(IReadOnlyList<VectorSearchResult> results)
    {
        var rows = results.Select(r => new
        {
            r.ChunkId,
            r.SourceId,
            Score = float.IsFinite(r.Score) ? r.Score : 0f
        });
        return JsonSerializer.Serialize(rows, AuditJson);
    }

    private int ScalarInt(string sql)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = sql;
        return Convert.ToInt32(command.ExecuteScalar(), CultureInfo.InvariantCulture);
    }

    private VectorMetric GetCollectionMetric(long collectionId)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = "SELECT metric FROM vec_collections WHERE id = $id";
        command.Parameters.AddWithValue("$id", collectionId);
        var value = Convert.ToString(command.ExecuteScalar(), CultureInfo.InvariantCulture);
        if (value is null)
            throw new InvalidOperationException($"Collection {collectionId} not found.");
        return value == "l2" ? VectorMetric.L2 : VectorMetric.Cosine;
    }

    private int GetCollectionDimensions(long collectionId)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = "SELECT dimensions FROM vec_collections WHERE id = $id";
        command.Parameters.AddWithValue("$id", collectionId);
        var value = command.ExecuteScalar();
        if (value is null)
            throw new InvalidOperationException($"Collection {collectionId} not found.");
        return Convert.ToInt32(value, CultureInfo.InvariantCulture);
    }

    /// <summary>True se la collection usa storage SQ8 (default fp32, anche su DB migrati).</summary>
    private bool GetCollectionFormatSq8(long collectionId)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = "SELECT format FROM vec_collections WHERE id = $id";
        command.Parameters.AddWithValue("$id", collectionId);
        var value = Convert.ToString(command.ExecuteScalar(), CultureInfo.InvariantCulture);
        return value == "sq8";
    }

    private List<CandidateRow> LoadCandidates(long collectionId, VectorSearchFilter filter)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = """
            SELECT e.chunk_id, c.document_id, d.source_id, c.text, e.model_version, d.metadata_json,
                   d.status, d.tenant_id, d.access_roles, e.embedding
            FROM vec_embeddings e
            JOIN vec_chunks c ON c.id = e.chunk_id
            JOIN vec_documents d ON d.id = c.document_id
            WHERE d.collection_id = $cid
            """;
        command.Parameters.AddWithValue("$cid", collectionId);
        if (filter.ModelVersion is not null)
        {
            command.CommandText += " AND e.model_version = $model";
            command.Parameters.AddWithValue("$model", filter.ModelVersion);
        }
        if (filter.RequiredStatus is not null)
        {
            command.CommandText += " AND d.status = $status";
            command.Parameters.AddWithValue("$status", filter.RequiredStatus);
        }
        if (filter.TenantId is not null)
        {
            command.CommandText += " AND d.tenant_id = $tenant";
            command.Parameters.AddWithValue("$tenant", filter.TenantId);
        }

        var list = new List<CandidateRow>();
        using var reader = command.ExecuteReader();
        while (reader.Read())
        {
            list.Add(new CandidateRow(
                reader.GetInt64(0),
                reader.GetInt64(1),
                reader.GetString(2),
                reader.GetString(3),
                reader.GetString(4),
                reader.IsDBNull(5) ? null : reader.GetString(5),
                reader.GetString(6),
                reader.IsDBNull(7) ? null : reader.GetString(7),
                reader.IsDBNull(8) ? null : reader.GetString(8),
                (byte[])reader.GetValue(9)));
        }

        return list;
    }

    private sealed record CachedIndex(int Count, VectorMetric Metric, VectorHnswIndex Index);

    private sealed record CandidateRow(
        long ChunkId,
        long DocumentId,
        string SourceId,
        string Text,
        string ModelVersion,
        string? MetadataJson,
        string Status,
        string? TenantId,
        string? AccessRoles,
        byte[] Embedding);
}
