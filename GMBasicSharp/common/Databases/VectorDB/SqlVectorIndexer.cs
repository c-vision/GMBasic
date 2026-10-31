// FILE: Basic.SQLite.Vector.SqlVectorIndexer
using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using Microsoft.Data.Sqlite;

namespace Basic.SQLite.Vector;

/// <summary>Ingests documents, chunks, and embeddings into vec_* tables.</summary>
public sealed class SqlVectorIndexer
{
    private readonly SqliteConnection _connection;
    private readonly IEmbeddingProvider _embeddingProvider;

    /// <summary>Serializza tutte le operazioni sulla <see cref="_connection"/> condivisa: un
    /// singolo SqliteConnection non supporta transazioni concorrenti (una sola alla volta a
    /// livello di handle nativo sqlite3) — due thread che chiamano IndexText() sulla stessa
    /// connessione senza questo lock fanno collidere BeginTransaction() con "cannot start a
    /// transaction within a transaction" (bug riprodotto da
    /// ConcurrencyTests.ConcurrentIndexing_SameCollection_ThreadSafe). Anche i metodi senza una
    /// transazione esplicita (IndexChunk, DeleteBySourceId, ecc.) vanno sotto lo stesso lock:
    /// last_insert_rowid() è scoped alla CONNESSIONE, non alla singola INSERT — un INSERT di un
    /// altro thread interposto fra la INSERT e la successiva SELECT last_insert_rowid() di
    /// questo thread farebbe leggere l'id sbagliato, un bug silenzioso di corruzione dati più
    /// subdolo del semplice crash. Il lock è condiviso per-connessione (via
    /// <see cref="SqlConnectionLocks"/>) con SqlVectorSearch, non locale a questa istanza:
    /// altrimenti un reader concorrente su SqlVectorSearch potrebbe comunque collidere su
    /// CreateCommand/Dispose della stessa connessione (ConcurrentSearch_WhileIndexing_DoesNotCrash).</summary>
    private readonly object _writeLock;

    public SqlVectorIndexer(SqliteConnection connection, IEmbeddingProvider? embeddingProvider = null)
    {
        ArgumentNullException.ThrowIfNull(connection);
        _connection = connection;
        // P0a (todo Tokenizer-WordPiece, decisione utente 2026-09-12): senza un provider embedding
        // esplicito è ERRORE, mai fallback silenzioso sul fixture (pseudo-semantica stabile ma
        // priva di significato — gatto e micio distano come gatto e gravitational). Il fixture
        // resta legittimo SOLO se passato esplicitamente (es. dai test). Parità con
        // IlRuntime.BrainVaultBuild e RagIndexService.ValidateEmbeddingProvider.
        _embeddingProvider = embeddingProvider
            ?? throw new InvalidOperationException(
                "Nessun provider embedding attivo: registrane uno reale (AI SET ACTIVE %EMBEDDING, alias) o passa un provider esplicito.");
        _writeLock = SqlConnectionLocks.For(connection);
        // EnsureSchema runs DDL on _connection; must be under the shared lock too, otherwise
        // constructing an indexer/search instance while another thread holds an open
        // transaction on the same connection throws (schema DDL isn't part of that transaction).
        lock (_writeLock)
        {
            SqlVectorSchema.EnsureSchema(_connection);
        }
    }

    public long EnsureCollection(string name, int dimensions, VectorMetric metric, string? modelVersion = null, string? format = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        if (dimensions <= 0)
            throw new ArgumentOutOfRangeException(nameof(dimensions));
        // Formato esplicito ('sq8'|'fp32') cambia lo storage; omesso = conserva l'esistente
        // (mai reset da chiamate senza intento, es. IndexText via WithCollection).
        string? fmt = format == "sq8" ? "sq8" : format == "fp32" ? "fp32" : null;

        lock (_writeLock)
        {
            var metricText = metric == VectorMetric.Cosine ? "cosine" : "l2";
            using var find = _connection.CreateCommand();
            find.CommandText = "SELECT id FROM vec_collections WHERE name = $name";
            find.Parameters.AddWithValue("$name", name);
            var existing = find.ExecuteScalar();
            if (existing is long id)
            {
                GuardOrBackfillModelDigest(id);
                if (fmt is not null) SetCollectionFormat(id, fmt);
                return id;
            }
            if (existing is int i)
            {
                GuardOrBackfillModelDigest(i);
                if (fmt is not null) SetCollectionFormat(i, fmt);
                return i;
            }

            using var insert = _connection.CreateCommand();
            insert.CommandText = """
                INSERT INTO vec_collections (name, dimensions, metric, model_version, model_digest, format)
                VALUES ($name, $dim, $metric, $model, $digest, $format)
                """;
            insert.Parameters.AddWithValue("$name", name);
            insert.Parameters.AddWithValue("$dim", dimensions);
            insert.Parameters.AddWithValue("$metric", metricText);
            insert.Parameters.AddWithValue("$model", modelVersion ?? _embeddingProvider.ModelVersion);
            insert.Parameters.AddWithValue("$digest", ModelDigest(_embeddingProvider, dimensions));
            insert.Parameters.AddWithValue("$format", fmt ?? "fp32");
            insert.ExecuteNonQuery();

            using var last = _connection.CreateCommand();
            last.CommandText = "SELECT last_insert_rowid()";
            return Convert.ToInt64(last.ExecuteScalar(), CultureInfo.InvariantCulture);
        }
    }

    /// <summary>Indexes text; skips re-embed when <paramref name="text"/> hash is unchanged.</summary>
    public IndexTextOutcome IndexText(long collectionId, string sourceId, string text, DocumentMetadata? metadata = null,
        ChunkingOptions? chunking = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceId);
        ArgumentNullException.ThrowIfNull(text);
        metadata ??= new DocumentMetadata();

        lock (_writeLock)
        {
            // Fase 3 (todo Tokenizer-WordPiece): guardia d'integrità del modello — prima di
            // qualunque scrittura, la collection deve appartenere al provider attivo.
            GuardOrBackfillModelDigest(collectionId);
            var sourceHash = ComputeHash(text);
            var existingHash = TryGetSourceHash(collectionId, sourceId);
            if (existingHash is not null && existingHash == sourceHash)
                return IndexTextOutcome.Skipped;

            using var tx = _connection.BeginTransaction();
            try
            {
                var isUpdate = existingHash is not null;
                var dimensions = GetCollectionDimensions(collectionId);
                DeleteBySourceId(collectionId, sourceId);

                var now = DateTime.UtcNow.ToString("O", CultureInfo.InvariantCulture);
                var documentId = InsertDocument(collectionId, sourceId, sourceHash, metadata, now);

                // il chunking avviene QUI su tutto il testo (una sola chiamata per documento):
                // chi chiama IndexText con un testo intero NON deve pre-splittare (il fix del
                // doppio chunking + DeleteBySourceId ricorsivo, 2026-09-06 — RAG BUILD su un
                // libro 725KB produceva 2 chunk invece di ~400 perché ogni chiamata per-chunk
                // cancellava il documento e i chunk della chiamata precedente).
                var chunks = TextChunker.Chunk(text, chunking);
                if (chunks.Count == 0)
                    chunks = [text];

                for (var i = 0; i < chunks.Count; i++)
                {
                    var chunkId = InsertChunk(documentId, collectionId, i, chunks[i]);
                    var embedding = _embeddingProvider.Embed(chunks[i], dimensions);
                    UpsertEmbedding(chunkId, collectionId, embedding, _embeddingProvider.ModelVersion, dimensions);
                }

                tx.Commit();
                return isUpdate ? IndexTextOutcome.Updated : IndexTextOutcome.Indexed;
            }
            catch
            {
                tx.Rollback();
                throw;
            }
        }
    }

    /// <summary>Indexes a single chunk with an explicit embedding (tests / precomputed vectors).</summary>
    public void IndexChunk(
        long collectionId,
        string sourceId,
        string chunkText,
        ReadOnlySpan<float> embedding,
        DocumentMetadata? metadata = null,
        int chunkIndex = 0,
        string? modelVersion = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceId);
        ArgumentException.ThrowIfNullOrEmpty(chunkText);
        metadata ??= new DocumentMetadata();

        lock (_writeLock)
        {
            var dimensions = GetCollectionDimensions(collectionId);
            if (embedding.Length != dimensions)
                throw new ArgumentException($"Embedding length {embedding.Length} does not match collection dimensions {dimensions}.");

            DeleteBySourceId(collectionId, sourceId);
            var sourceHash = ComputeHash(chunkText);
            var now = DateTime.UtcNow.ToString("O", CultureInfo.InvariantCulture);
            var documentId = InsertDocument(collectionId, sourceId, sourceHash, metadata, now);
            var chunkId = InsertChunk(documentId, collectionId, chunkIndex, chunkText);
            UpsertEmbedding(chunkId, collectionId, embedding, modelVersion ?? _embeddingProvider.ModelVersion, dimensions);
        }
    }

    /// <summary>Dual-write: upserts embedding for <paramref name="modelVersion"/> without removing other model versions.</summary>
    public void UpsertChunkEmbedding(
        long collectionId,
        string sourceId,
        string chunkText,
        ReadOnlySpan<float> embedding,
        string modelVersion,
        DocumentMetadata? metadata = null,
        int chunkIndex = 0)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceId);
        ArgumentException.ThrowIfNullOrEmpty(chunkText);
        ArgumentException.ThrowIfNullOrWhiteSpace(modelVersion);
        metadata ??= new DocumentMetadata();

        lock (_writeLock)
        {
            var dimensions = GetCollectionDimensions(collectionId);
            if (embedding.Length != dimensions)
                throw new ArgumentException($"Embedding length {embedding.Length} does not match collection dimensions {dimensions}.");

            var documentId = EnsureDocument(collectionId, sourceId, ComputeHash(chunkText), metadata);
            var chunkId = EnsureChunk(documentId, collectionId, chunkIndex, chunkText);
            UpsertEmbedding(chunkId, collectionId, embedding, modelVersion, dimensions);
        }
    }

    public void DeleteBySourceId(long collectionId, string sourceId)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceId);
        lock (_writeLock)
        {
            SqlVectorFts.DeleteForSource(_connection, collectionId, sourceId);
            using var command = _connection.CreateCommand();
            command.CommandText = """
                DELETE FROM vec_documents
                WHERE collection_id = $cid AND source_id = $sid
                """;
            command.Parameters.AddWithValue("$cid", collectionId);
            command.Parameters.AddWithValue("$sid", sourceId);
            command.ExecuteNonQuery();
        }
    }

    /// <summary>Removes all documents marked <c>deprecated</c> and their embeddings.</summary>
    public int PurgeDeprecated(long collectionId)
    {
        lock (_writeLock)
        {
            SqlVectorFts.DeleteDeprecated(_connection, collectionId);
            using var command = _connection.CreateCommand();
            command.CommandText = """
                DELETE FROM vec_documents
                WHERE collection_id = $cid AND status = 'deprecated'
                """;
            command.Parameters.AddWithValue("$cid", collectionId);
            return command.ExecuteNonQuery();
        }
    }

    public int CountEmbeddings(long collectionId)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = """
            SELECT COUNT(*)
            FROM vec_embeddings e
            JOIN vec_chunks c ON c.id = e.chunk_id
            JOIN vec_documents d ON d.id = c.document_id
            WHERE d.collection_id = $cid
            """;
        command.Parameters.AddWithValue("$cid", collectionId);
        return Convert.ToInt32(command.ExecuteScalar(), CultureInfo.InvariantCulture);
    }

    private string? TryGetSourceHash(long collectionId, string sourceId)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = """
            SELECT source_hash FROM vec_documents
            WHERE collection_id = $cid AND source_id = $sid
            """;
        command.Parameters.AddWithValue("$cid", collectionId);
        command.Parameters.AddWithValue("$sid", sourceId);
        return command.ExecuteScalar() as string;
    }

    private long InsertDocument(long collectionId, string sourceId, string sourceHash, DocumentMetadata metadata, string now)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = """
            INSERT INTO vec_documents
              (collection_id, source_id, source_hash, status, tenant_id, access_roles, metadata_json, created_at, updated_at)
            VALUES ($cid, $sid, $hash, $status, $tenant, $roles, $meta, $now, $now)
            """;
        command.Parameters.AddWithValue("$cid", collectionId);
        command.Parameters.AddWithValue("$sid", sourceId);
        command.Parameters.AddWithValue("$hash", sourceHash);
        command.Parameters.AddWithValue("$status", metadata.Status);
        command.Parameters.AddWithValue("$tenant", (object?)metadata.TenantId ?? DBNull.Value);
        command.Parameters.AddWithValue("$roles", (object?)metadata.AccessRoles ?? DBNull.Value);
        command.Parameters.AddWithValue("$meta", (object?)metadata.MetadataJson ?? DBNull.Value);
        command.Parameters.AddWithValue("$now", now);
        command.ExecuteNonQuery();

        using var last = _connection.CreateCommand();
        last.CommandText = "SELECT last_insert_rowid()";
        return Convert.ToInt64(last.ExecuteScalar(), CultureInfo.InvariantCulture);
    }

    private long InsertChunk(long documentId, long collectionId, int chunkIndex, string text)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = """
            INSERT INTO vec_chunks (document_id, chunk_index, text, token_count)
            VALUES ($doc, $idx, $text, $tokens)
            """;
        command.Parameters.AddWithValue("$doc", documentId);
        command.Parameters.AddWithValue("$idx", chunkIndex);
        command.Parameters.AddWithValue("$text", text);
        command.Parameters.AddWithValue("$tokens", text.Length);
        command.ExecuteNonQuery();

        using var last = _connection.CreateCommand();
        last.CommandText = "SELECT last_insert_rowid()";
        var chunkId = Convert.ToInt64(last.ExecuteScalar(), CultureInfo.InvariantCulture);
        SqlVectorFts.IndexChunk(_connection, chunkId, collectionId, text);
        return chunkId;
    }

    private long EnsureDocument(long collectionId, string sourceId, string sourceHash, DocumentMetadata metadata)
    {
        using var find = _connection.CreateCommand();
        find.CommandText = """
            SELECT id FROM vec_documents
            WHERE collection_id = $cid AND source_id = $sid
            """;
        find.Parameters.AddWithValue("$cid", collectionId);
        find.Parameters.AddWithValue("$sid", sourceId);
        var existing = find.ExecuteScalar();
        if (existing is long id)
            return id;
        if (existing is int i)
            return i;

        var now = DateTime.UtcNow.ToString("O", CultureInfo.InvariantCulture);
        return InsertDocument(collectionId, sourceId, sourceHash, metadata, now);
    }

    private long EnsureChunk(long documentId, long collectionId, int chunkIndex, string text)
    {
        using var find = _connection.CreateCommand();
        find.CommandText = """
            SELECT id FROM vec_chunks
            WHERE document_id = $doc AND chunk_index = $idx
            """;
        find.Parameters.AddWithValue("$doc", documentId);
        find.Parameters.AddWithValue("$idx", chunkIndex);
        var existing = find.ExecuteScalar();
        if (existing is long id)
            return id;
        if (existing is int i)
            return i;

        return InsertChunk(documentId, collectionId, chunkIndex, text);
    }

    private void UpsertEmbedding(long chunkId, long collectionId, ReadOnlySpan<float> embedding, string modelVersion, int dimensions)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = """
            INSERT INTO vec_embeddings (chunk_id, model_version, dimensions, embedding)
            VALUES ($chunk, $model, $dim, $blob)
            ON CONFLICT (chunk_id, model_version) DO UPDATE SET
              dimensions = excluded.dimensions,
              embedding = excluded.embedding,
              embedding_created_at = datetime('now')
            """;
        command.Parameters.AddWithValue("$chunk", chunkId);
        command.Parameters.AddWithValue("$model", modelVersion);
        command.Parameters.AddWithValue("$dim", dimensions);
        command.Parameters.AddWithValue("$blob", ToStorageBlob(collectionId, embedding));
        command.ExecuteNonQuery();
    }

    /// <summary>True se la collection usa storage SQ8 (default fp32, anche su DB migrati).</summary>
    public bool IsSq8Collection(long collectionId)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = "SELECT format FROM vec_collections WHERE id = $id";
        command.Parameters.AddWithValue("$id", collectionId);
        var value = command.ExecuteScalar() as string;
        return value == "sq8";
    }

    private void SetCollectionFormat(long collectionId, string format)
    {
        using var command = _connection.CreateCommand();
        command.CommandText = "UPDATE vec_collections SET format = $format WHERE id = $id";
        command.Parameters.AddWithValue("$format", format);
        command.Parameters.AddWithValue("$id", collectionId);
        command.ExecuteNonQuery();
    }

    private byte[] ToStorageBlob(long collectionId, ReadOnlySpan<float> embedding) =>
        IsSq8Collection(collectionId) ? VectorMath.ToSq8Blob(embedding) : VectorMath.ToBlob(embedding);

    private void InsertEmbedding(long chunkId, long collectionId, ReadOnlySpan<float> embedding, string modelVersion, int dimensions) =>
        UpsertEmbedding(chunkId, collectionId, embedding, modelVersion, dimensions);

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

    private static string ComputeHash(string text)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(text));
        return Convert.ToHexString(bytes).ToLowerInvariant();
    }

    /// <summary>Fase 3: digest dell'identità del provider per la collection (delega al guard
    /// condiviso con SqlVectorSearch — una sola implementazione per scritture e letture).</summary>
    private static string ModelDigest(IEmbeddingProvider provider, int dimensions) =>
        SqlVectorModelGuard.ComputeDigest(provider, dimensions);

    /// <summary>Fase 3: guardia d'integrità del modello all'accesso a una collection esistente
    /// (backfill DB pre-migrazione / rifiuto con i due digest — vedi SqlVectorModelGuard).</summary>
    private void GuardOrBackfillModelDigest(long collectionId) =>
        SqlVectorModelGuard.GuardOrBackfill(_connection, collectionId, _embeddingProvider);
}
