// FILE: AI.Tests.Vector.SqlVectorFtsDiacriticsTests
// LOCK di comportamento del tokenizer FTS5 (todo Tokenizer-WordPiece §4, chiuso come
// «non applicabile, documentato» — misura 2026-09-13): su SQLite 3.50/3.51 `unicode61`
// rimuove GIÀ i diacritici di default (livello 1). Questi test fissano la premessa misurata:
// se un aggiornamento di SQLite o una modifica al DDL cambiasse il default, il test fallisce
// e la decisione di fase 2 va riesaminata (in quel caso: via 1 `remove_diacritics 2`, che
// aggiunge solo i non-scomponibili NFD come ø/ł/đ, e va allineato anche gmb_rag.c:129).
using Basic.SQLite.Vector;
using FluentAssertions;
using Microsoft.Data.Sqlite;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class SqlVectorFtsDiacriticsTests : IDisposable
{
    private readonly string _dbPath =
        Path.Combine(Path.GetTempPath(), $"gmb_fts_diacritics_{Guid.NewGuid():N}.db");

    private SqliteConnection Open()
    {
        var conn = new SqliteConnection($"Data Source={_dbPath}");
        conn.Open();
        return conn;
    }

    public void Dispose()
    {
        SqliteConnection.ClearAllPools();
        try { File.Delete(_dbPath); } catch { /* best effort */ }
    }

    [Fact(DisplayName = "FTS5 unicode61 default: query senza accenti matcha testo accentato (livello 1 di serie)")]
    public void DefaultUnicode61_MatchesAccentsWithoutDiacritics()
    {
        using var conn = Open();
        if (!SqlVectorFts.IsAvailable(conn))
            return;   // ambiente senza FTS5: skip esplicito (nessun falso verde)

        SqlVectorFts.EnsureSchema(conn);   // DDL di produzione: tokenize='unicode61'
        SqlVectorFts.IndexChunk(conn, 1, 10, "Il caffè di oggi: résumé e naïveté");

        SqlVectorFts.Search(conn, 10, "resume", 10).Should().Contain(1,
            "il default FTS5 di unicode61 rimuove i diacritici: 'resume' matcha 'résumé'");
        SqlVectorFts.Search(conn, 10, "caffe", 10).Should().Contain(1,
            "'caffè' → 'caffe' (doppia f)");
        SqlVectorFts.Search(conn, 10, "naivete", 10).Should().Contain(1);
        SqlVectorFts.Search(conn, 10, "résumé", 10).Should().Contain(1,
            "la query accentata matcha (normalizzate entrambe le parti)");
    }

    [Fact(DisplayName = "Premessa misurata: solo remove_diacritics=0 esplicito preserva i diacritici (default ≠ 0)")]
    public void ExplicitLevel0_KeepsDiacritics_ProvingDefaultIsNotZero()
    {
        using var conn = Open();
        if (!SqlVectorFts.IsAvailable(conn))
            return;

        using (var ddl = conn.CreateCommand())
        {
            ddl.CommandText = """
                CREATE VIRTUAL TABLE t0 USING fts5(x, tokenize='unicode61 remove_diacritics 0');
                INSERT INTO t0(rowid, x) VALUES (1, 'Résumé nel vecchio indice');
                CREATE VIRTUAL TABLE t1 USING fts5(x, tokenize='unicode61');
                INSERT INTO t1(rowid, x) VALUES (1, 'Résumé nel vecchio indice');
                """;
            ddl.ExecuteNonQuery();
        }

        long Matches(string table, string phrase)
        {
            using var q = conn.CreateCommand();
            q.CommandText = $"SELECT count(*) FROM {table} WHERE {table} MATCH '\"{phrase}\"'";
            return (long)q.ExecuteScalar()!;
        }

        Matches("t0", "resume").Should().Be(0,
            "remove_diacritics=0 esplicito NON deve matchare: dimostra che il default non è 0");
        Matches("t1", "resume").Should().Be(1,
            "il default (DDL di produzione) rimuove i diacritici scomponibili NFD");
        // Nota misurata (e_sqlite3 3.50.4): `remove_diacritics 2` NON piega comunque i
        // non-scomponibili come 'ø' ('Øslo' non matcha 'oslo' nemmeno a livello 2) — il
        // delta fra default e livello 2 è quindi nullo anche per i corpus nordici su questo
        // motore. Dettagli nel todo Tokenizer-WordPiece §4 (chiuso "non applicabile").
    }
}
