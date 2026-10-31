// FILE: Basic.SQLite.Vector.SqlVectorSchema
using System.Globalization;
using Microsoft.Data.Sqlite;

namespace Basic.SQLite.Vector;

/// <summary>Idempotent DDL for vector tables.</summary>
public static class SqlVectorSchema
{
    public static void EnsureSchema(SqliteConnection connection)
    {
        ArgumentNullException.ThrowIfNull(connection);
        if (connection.State != System.Data.ConnectionState.Open)
            throw new InvalidOperationException("Connection must be open.");

        using var command = connection.CreateCommand();
        command.CommandText = """
            CREATE TABLE IF NOT EXISTS vec_collections (
              id            INTEGER PRIMARY KEY,
              name          TEXT NOT NULL UNIQUE,
              dimensions    INTEGER NOT NULL,
              metric        TEXT NOT NULL CHECK (metric IN ('cosine','l2')),
              model_version TEXT NOT NULL DEFAULT 'fixture-v1',
              model_digest  TEXT,
              format        TEXT NOT NULL DEFAULT 'fp32' CHECK (format IN ('fp32','sq8')),
              created_at    TEXT NOT NULL DEFAULT (datetime('now'))
            );

            CREATE TABLE IF NOT EXISTS vec_documents (
              id              INTEGER PRIMARY KEY,
              collection_id   INTEGER NOT NULL REFERENCES vec_collections(id) ON DELETE CASCADE,
              source_id       TEXT NOT NULL,
              source_hash     TEXT NOT NULL,
              status          TEXT NOT NULL DEFAULT 'current' CHECK (status IN ('draft','current','deprecated')),
              tenant_id       TEXT,
              access_roles    TEXT,
              metadata_json   TEXT,
              created_at      TEXT NOT NULL DEFAULT (datetime('now')),
              updated_at      TEXT NOT NULL DEFAULT (datetime('now')),
              UNIQUE(collection_id, source_id)
            );

            CREATE TABLE IF NOT EXISTS vec_chunks (
              id            INTEGER PRIMARY KEY,
              document_id   INTEGER NOT NULL REFERENCES vec_documents(id) ON DELETE CASCADE,
              chunk_index   INTEGER NOT NULL,
              text          TEXT NOT NULL,
              token_count   INTEGER,
              metadata_json TEXT,
              UNIQUE(document_id, chunk_index)
            );

            CREATE TABLE IF NOT EXISTS vec_embeddings (
              chunk_id             INTEGER NOT NULL REFERENCES vec_chunks(id) ON DELETE CASCADE,
              model_version        TEXT NOT NULL,
              embedding_created_at TEXT NOT NULL DEFAULT (datetime('now')),
              dimensions           INTEGER NOT NULL,
              embedding            BLOB NOT NULL,
              PRIMARY KEY (chunk_id, model_version)
            );

            CREATE INDEX IF NOT EXISTS idx_vec_documents_collection ON vec_documents(collection_id);
            CREATE INDEX IF NOT EXISTS idx_vec_documents_status ON vec_documents(status);

            CREATE TABLE IF NOT EXISTS vec_query_audit (
              id              INTEGER PRIMARY KEY,
              queried_at      TEXT NOT NULL DEFAULT (datetime('now')),
              collection_id   INTEGER,
              query_text      TEXT,
              model_version   TEXT,
              filters_json    TEXT,
              top_k           INTEGER,
              results_json    TEXT,
              latency_ms      INTEGER,
              status          TEXT,
              is_shadow       INTEGER DEFAULT 0
            );
            """;
        command.ExecuteNonQuery();
        MigrateEmbeddingsCompositeKey(connection);
        MigrateAddModelDigestColumn(connection);
        MigrateAddFormatColumn(connection);
        SqlVectorFts.EnsureSchema(connection);
    }

    /// <summary>Migrates legacy single-PK vec_embeddings to composite (chunk_id, model_version).</summary>
    private static void MigrateEmbeddingsCompositeKey(SqliteConnection connection)
    {
        using var info = connection.CreateCommand();
        info.CommandText = "SELECT sql FROM sqlite_master WHERE type='table' AND name='vec_embeddings'";
        var ddl = info.ExecuteScalar() as string;
        if (ddl is null || ddl.Contains("PRIMARY KEY (chunk_id, model_version)", StringComparison.Ordinal))
            return;

        using var migrate = connection.CreateCommand();
        migrate.CommandText = """
            CREATE TABLE vec_embeddings_migrated (
              chunk_id             INTEGER NOT NULL REFERENCES vec_chunks(id) ON DELETE CASCADE,
              model_version        TEXT NOT NULL,
              embedding_created_at TEXT NOT NULL DEFAULT (datetime('now')),
              dimensions           INTEGER NOT NULL,
              embedding            BLOB NOT NULL,
              PRIMARY KEY (chunk_id, model_version)
            );
            INSERT INTO vec_embeddings_migrated
              (chunk_id, model_version, embedding_created_at, dimensions, embedding)
            SELECT chunk_id, model_version, embedding_created_at, dimensions, embedding
            FROM vec_embeddings;
            DROP TABLE vec_embeddings;
            ALTER TABLE vec_embeddings_migrated RENAME TO vec_embeddings;
            """;
        migrate.ExecuteNonQuery();
    }

    /// <summary>Fase 3 (todo Tokenizer-WordPiece): aggiunge model_digest a vec_collections sui DB
    /// creati prima della colonna (idempotente, via pragma_table_info).</summary>
    private static void MigrateAddModelDigestColumn(SqliteConnection connection)
    {
        using var info = connection.CreateCommand();
        info.CommandText = "SELECT COUNT(*) FROM pragma_table_info('vec_collections') WHERE name = 'model_digest'";
        var n = Convert.ToInt64(info.ExecuteScalar() ?? 0, CultureInfo.InvariantCulture);
        if (n > 0)
            return;

        using var migrate = connection.CreateCommand();
        migrate.CommandText = "ALTER TABLE vec_collections ADD COLUMN model_digest TEXT";
        migrate.ExecuteNonQuery();
    }

    /// <summary>Aggiunge format ('fp32'|'sq8', default fp32) a vec_collections sui DB
    /// creati prima della colonna (idempotente, via pragma_table_info — parità C rag_migrate).</summary>
    private static void MigrateAddFormatColumn(SqliteConnection connection)
    {
        using var info = connection.CreateCommand();
        info.CommandText = "SELECT COUNT(*) FROM pragma_table_info('vec_collections') WHERE name = 'format'";
        var n = Convert.ToInt64(info.ExecuteScalar() ?? 0, CultureInfo.InvariantCulture);
        if (n > 0)
            return;

        using var migrate = connection.CreateCommand();
        migrate.CommandText = "ALTER TABLE vec_collections ADD COLUMN format TEXT NOT NULL DEFAULT 'fp32'";
        migrate.ExecuteNonQuery();
    }
}
