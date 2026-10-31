// FILE: Basic.SQLite.Vector.SqlVectorFts
using System.Collections.Concurrent;
using System.Text;
using Microsoft.Data.Sqlite;

namespace Basic.SQLite.Vector;

/// <summary>FTS5 side index for hybrid keyword + vector search.</summary>
public static class SqlVectorFts
{
    private static readonly ConcurrentDictionary<string, bool> FtsAvailableCache = new();

    public static bool IsAvailable(SqliteConnection connection)
    {
        ArgumentNullException.ThrowIfNull(connection);
        var key = connection.DataSource ?? connection.ConnectionString ?? string.Empty;
        if (FtsAvailableCache.TryGetValue(key, out var cached))
            return cached;

        try
        {
            using var probe = connection.CreateCommand();
            probe.CommandText = "CREATE VIRTUAL TABLE IF NOT EXISTS __fts_probe USING fts5(x)";
            probe.ExecuteNonQuery();
            using var drop = connection.CreateCommand();
            drop.CommandText = "DROP TABLE IF EXISTS __fts_probe";
            drop.ExecuteNonQuery();
            FtsAvailableCache[key] = true;
            return true;
        }
        catch (SqliteException)
        {
            FtsAvailableCache[key] = false;
            return false;
        }
    }

    public static void EnsureSchema(SqliteConnection connection)
    {
        if (!IsAvailable(connection))
            return;

        // NOTA (todo Tokenizer-WordPiece §4, misura 2026-09-13): `unicode61` in FTS5 su
        // SQLite 3.50/3.51 rimuove GIÀ i diacritici di default (livello 1 — misurato su
        // e_sqlite3 3.50.4 e sqlite3 3.51.0: MATCH '"resume"' trova 'Résumé'; solo
        // `remove_diacritics 0` esplicito li preserva). `remove_diacritics 2` aggiungerebbe
        // solo i non-scomponibili NFD (ø/ł/đ): margine nullo per il corpus del progetto
        // (italiano, è/à/ù tutti scomponibili) a fronte di una divergenza C#/C (gmb_rag.c:129
        // resta 'unicode61') e di un rebuild degli indici esistenti. Fase 2 chiusa come
        // «non applicabile, documentato» — questo test di comportamento: SqlVectorFtsDiacriticsTests.
        using var command = connection.CreateCommand();
        command.CommandText = """
            CREATE VIRTUAL TABLE IF NOT EXISTS fts_chunks USING fts5(
              text,
              chunk_id UNINDEXED,
              collection_id UNINDEXED,
              tokenize='unicode61'
            );
            """;
        command.ExecuteNonQuery();
    }

    public static void IndexChunk(SqliteConnection connection, long chunkId, long collectionId, string text)
    {
        if (!IsAvailable(connection))
            return;

        DeleteChunk(connection, chunkId);
        using var command = connection.CreateCommand();
        command.CommandText = """
            INSERT INTO fts_chunks(rowid, text, chunk_id, collection_id)
            VALUES ($rowid, $text, $chunk, $cid)
            """;
        command.Parameters.AddWithValue("$rowid", chunkId);
        command.Parameters.AddWithValue("$text", text);
        command.Parameters.AddWithValue("$chunk", chunkId);
        command.Parameters.AddWithValue("$cid", collectionId);
        command.ExecuteNonQuery();
    }

    public static void DeleteChunk(SqliteConnection connection, long chunkId)
    {
        if (!IsAvailable(connection))
            return;

        using var command = connection.CreateCommand();
        command.CommandText = "DELETE FROM fts_chunks WHERE rowid = $id";
        command.Parameters.AddWithValue("$id", chunkId);
        command.ExecuteNonQuery();
    }

    public static void DeleteForSource(SqliteConnection connection, long collectionId, string sourceId)
    {
        if (!IsAvailable(connection))
            return;

        using var command = connection.CreateCommand();
        command.CommandText = """
            DELETE FROM fts_chunks
            WHERE chunk_id IN (
              SELECT c.id FROM vec_chunks c
              JOIN vec_documents d ON d.id = c.document_id
              WHERE d.collection_id = $cid AND d.source_id = $sid
            )
            """;
        command.Parameters.AddWithValue("$cid", collectionId);
        command.Parameters.AddWithValue("$sid", sourceId);
        command.ExecuteNonQuery();
    }

    public static void DeleteDeprecated(SqliteConnection connection, long collectionId)
    {
        if (!IsAvailable(connection))
            return;

        using var command = connection.CreateCommand();
        command.CommandText = """
            DELETE FROM fts_chunks
            WHERE chunk_id IN (
              SELECT c.id FROM vec_chunks c
              JOIN vec_documents d ON d.id = c.document_id
              WHERE d.collection_id = $cid AND d.status = 'deprecated'
            )
            """;
        command.Parameters.AddWithValue("$cid", collectionId);
        command.ExecuteNonQuery();
    }

  /// <summary>BM25-ranked chunk ids (lower bm25 is better — order preserved).</summary>
    public static IReadOnlyList<long> Search(SqliteConnection connection, long collectionId, string queryText, int limit)
    {
        if (!IsAvailable(connection) || limit <= 0)
            return [];

        var match = ToMatchQuery(queryText);
        if (match.Length == 0)
            return [];

        using var command = connection.CreateCommand();
        command.CommandText = """
            SELECT chunk_id, bm25(fts_chunks) AS rank
            FROM fts_chunks
            WHERE fts_chunks MATCH $match AND collection_id = $cid
            ORDER BY rank
            LIMIT $lim
            """;
        command.Parameters.AddWithValue("$match", match);
        command.Parameters.AddWithValue("$cid", collectionId);
        command.Parameters.AddWithValue("$lim", limit);

        var ids = new List<long>();
        using var reader = command.ExecuteReader();
        while (reader.Read())
            ids.Add(reader.GetInt64(0));
        return ids;
    }

    internal static string ToMatchQuery(string queryText)
    {
        var terms = queryText.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (terms.Length == 0)
            return string.Empty;

        var sb = new StringBuilder();
        for (var i = 0; i < terms.Length; i++)
        {
            if (i > 0) sb.Append(' ');
            sb.Append('"');
            sb.Append(terms[i].Replace("\"", "\"\"", StringComparison.Ordinal));
            sb.Append('"');
        }

        return sb.ToString();
    }
}
