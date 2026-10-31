// FILE: Basic.Runtime.Services.DatabaseService.cs
// Implementazione di IDatabaseService: SQL (SQLite) e VectorDB stanno in questo file, i
// driver su file (DBF/ISAM/SPEED/Ledger) nella partial DatabaseService.Native.cs.
//
// La classe e' partial e l'unica istanza usata dal runtime implementa TUTTO (vedi
// HeadlessRuntimeHost): non esiste piu' alcuna distinzione di edizione (la matrice
// GMBASIC_COMMUNITY/GMBASIC_COMMERCIAL, l'eccezione CommercialEditionRequired e i due
// servizi separati sono stati rimossi).

using System.Collections.Concurrent;

namespace Basic.Runtime;

public sealed partial class DatabaseService : IDatabaseService, IDisposable
{
    private long _nextHandle = 1;
    private readonly ConcurrentDictionary<long, object> _handles = new();

    // ================================================================
    // SQLite
    // ================================================================
    public long SqlOpen(string path)
    {
        // path può essere un PERCORSO FILE ("Work/app.db") o una CONNECTION STRING completa
        // ("Data Source=Work/app.db", "Data Source=:memory:"): con '=' è già una connection
        // string → NON ri-wrapare (prima diventava "Data Source=Data Source=..." → SQLite creava
        // un file col nome sbagliato, e RDD USE ... VIA "SQL" non vedeva la tabella creata da
        // SQL OPEN — RDD_SQL_CRUD). Stessa logica di IlRuntime.RddUse/SqlOpen.
        try { var conn = new Microsoft.Data.Sqlite.SqliteConnection(path.Contains('=') ? path : $"Data Source={path}"); conn.Open(); var h = _nextHandle++; _handles[h] = conn; return h; }
        catch { return -1; }
    }
    public void SqlClose(long handle) { if (_handles.TryRemove(handle, out var e) && e is Microsoft.Data.Sqlite.SqliteConnection c) { c.Close(); c.Dispose(); } }
    public void SqlExecute(long handle, string sql) { if (Get<Microsoft.Data.Sqlite.SqliteConnection>(handle) is { } c) { using var cmd = c.CreateCommand(); cmd.CommandText = sql; cmd.ExecuteNonQuery(); } }
    public object?[][] SqlQuery(long handle, string sql)
    {
        if (Get<Microsoft.Data.Sqlite.SqliteConnection>(handle) is not { } c) return [];
        try { using var cmd = c.CreateCommand(); cmd.CommandText = sql; using var r = cmd.ExecuteReader(); var res = new List<object?[]>(); while (r.Read()) { var row = new object?[r.FieldCount]; for (var i = 0; i < r.FieldCount; i++) row[i] = r.IsDBNull(i) ? null : r.GetValue(i); res.Add(row); } return [.. res]; }
        catch { return []; }
    }
    public long SqlLastInsertRowId(long handle) { if (Get<Microsoft.Data.Sqlite.SqliteConnection>(handle) is not { } c) return 0; using var cmd = c.CreateCommand(); cmd.CommandText = "SELECT last_insert_rowid()"; return (long)(cmd.ExecuteScalar() ?? 0L); }
    public void SqlBeginTransaction(long handle) { ExecSql(handle, "BEGIN"); }
    public void SqlCommit(long handle) { ExecSql(handle, "COMMIT"); }
    public void SqlRollback(long handle) { ExecSql(handle, "ROLLBACK"); }
    private void ExecSql(long handle, string sql) { if (Get<Microsoft.Data.Sqlite.SqliteConnection>(handle) is { } c) { using var cmd = c.CreateCommand(); cmd.CommandText = sql; cmd.ExecuteNonQuery(); } }

    // ================================================================
    // VectorDB
    // ================================================================
    public long VectorOpen(string path) => -1;
    public void VectorClose(long handle) { DisposeHandle(handle); }
    public void VectorInsert(long handle, float[] vector, object? metadata) { }
    public object?[] VectorSearch(long handle, float[] query, int topK) => [];
    public object?[] VectorSearchText(string dbPath, string collection, string query, int topK) => [];
    public long VectorCount(string dbPath, string collection) => 0;
    public void VectorEnsureCollection(string dbPath, string collection) { }
    public void VectorIndexText(string dbPath, string collection, string docId, string text) { }
    public void VectorLoadDocuments(string dbPath, string dirPath, string collection) { }

    // ================================================================
    // Helpers (usati anche dalla partial nativa)
    // ================================================================
    protected T? Get<T>(long handle) where T : class => _handles.TryGetValue(handle, out var e) ? e as T : null;
    protected void DisposeHandle(long handle) { if (_handles.TryRemove(handle, out var e) && e is IDisposable d) d.Dispose(); }
    protected long NextHandle => _nextHandle++;
    protected void StoreHandle(long handle, object value) => _handles[handle] = value;

    public void Dispose() { foreach (var kv in _handles) if (kv.Value is IDisposable d) d.Dispose(); _handles.Clear(); }
}
