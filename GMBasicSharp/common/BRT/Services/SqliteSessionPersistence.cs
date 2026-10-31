// SQLite (RDD-SQL, sempre presente) persistence per le sessioni HTTP — backend durevole
// del CommonLib.Http.BspSessionManager. Layering: BRT reference Databases (pacchetto
// Microsoft.Data.Sqlite + SQLitePCLRaw.bundle_e_sqlite3 via NuGet, nessun binario manuale).
using System.Collections.Concurrent;
using Microsoft.Data.Sqlite;
using CommonLib.Http;

namespace Basic.Runtime.Services;

/// <summary>
/// Persistenza SQLite per le sessioni: tabella `sessions(session_id PK, data, last_access)`.
/// Salvataggio = upsert + rimozione delle chiavi non più presenti (snapshot completo, veloce
/// per pochi KB/sessione); carica allo startup. Accesso per chiave alla lettura.
/// </summary>
public sealed class SqliteSessionPersistence : ISessionPersistence
{
    private readonly string _connectionString;
    private readonly object _sync = new();

    public SqliteSessionPersistence(string filePath)
    {
        var dir = Path.GetDirectoryName(Path.GetFullPath(filePath));
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        _connectionString = "Data Source=" + filePath;
        using var c = Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = """
            CREATE TABLE IF NOT EXISTS sessions (
                session_id   TEXT PRIMARY KEY,
                data         TEXT NOT NULL,
                last_access  INTEGER NOT NULL
            );
            """;
        cmd.ExecuteNonQuery();
    }

    private SqliteConnection Open()
    {
        var c = new SqliteConnection(_connectionString);
        c.Open();
        return c;
    }

    public IReadOnlyDictionary<string, SessionSnapshot> Load()
    {
        lock (_sync)
        {
            var result = new ConcurrentDictionary<string, SessionSnapshot>(StringComparer.Ordinal);
            using var c = Open();
            using var cmd = c.CreateCommand();
            cmd.CommandText = "SELECT session_id, data, last_access FROM sessions;";
            using var r = cmd.ExecuteReader();
            while (r.Read())
                result[r.GetString(0)] = new SessionSnapshot(r.GetString(1), r.GetInt64(2));
            return result;
        }
    }

    public void Save(IReadOnlyDictionary<string, SessionSnapshot> all)
    {
        lock (_sync)
        {
            using var c = Open();
            using var tx = c.BeginTransaction();
            // Rimuove le sessioni scadute/cancellate non più nello snapshot
            using (var del = c.CreateCommand())
            {
                del.Transaction = tx;
                del.CommandText = "DELETE FROM sessions;";
                del.ExecuteNonQuery();
            }
            if (all.Count > 0)
            {
                using var ins = c.CreateCommand();
                ins.Transaction = tx;
                ins.CommandText =
                    "INSERT OR REPLACE INTO sessions(session_id, data, last_access) VALUES($id, $data, $last);";
                var pId = ins.CreateParameter(); pId.ParameterName = "$id";
                var pData = ins.CreateParameter(); pData.ParameterName = "$data";
                var pLast = ins.CreateParameter(); pLast.ParameterName = "$last";
                ins.Parameters.Add(pId); ins.Parameters.Add(pData); ins.Parameters.Add(pLast);
                foreach (var kv in all)
                {
                    pId.Value = kv.Key;
                    pData.Value = kv.Value.DataJson;
                    pLast.Value = kv.Value.LastAccessUtcTicks;
                    ins.ExecuteNonQuery();
                }
            }
            tx.Commit();
        }
    }
}
