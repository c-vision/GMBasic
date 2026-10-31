// FILE: Basic.Runtime.Services.DatabaseService.cs
// Implementazione CORE (community) di IDatabaseService — parte comunitaria.
// Contiene SOLO i driver community: Sql (SQLite), VectorDB.
// I metodi commerciali (ISAM/DBF/SPEED/Ledger) sono implementati nella partial
// commerciale (modules/Database, solo edizione Commercial) con i motori C# nativi
// (Basic.DBF.DbfEngine, Basic.ISAM.Engine, Basic.Databases.Speed.Database,
// Basic.Ledger.LedgerManager). In Community lanciano CommercialEditionRequiredException.
// La classe è partial: la parte commerciale estende questa in moduli separati.

using System.Collections.Concurrent;

namespace Basic.Runtime;

public class DatabaseService : IDatabaseService, IDisposable
{
    private long _nextHandle = 1;
    private readonly ConcurrentDictionary<long, object> _handles = new();

    // ================================================================
    // ISAM — [COMMERCIALE] implementato in partial commerciale (modules/Database)
    // ================================================================
    public virtual long IsamOpen(string path) => CommercialNotAvailable<long>(nameof(IsamOpen));
    public virtual long IsamOpenSchema(string path, string alias, string schemaJson) => CommercialNotAvailable<long>(nameof(IsamOpenSchema));
    public virtual void IsamClose(long handle) => DisposeHandle(handle);
    public virtual void IsamInsert(long handle, object[] values) => CommercialNotAvailable(nameof(IsamInsert));
    public virtual void IsamUpdate(long handle, long recno, object[] values) => CommercialNotAvailable(nameof(IsamUpdate));
    public virtual void IsamPut(long handle, object[] values) => CommercialNotAvailable(nameof(IsamPut));
    public virtual void IsamDelete(long handle, long recno) => CommercialNotAvailable(nameof(IsamDelete));
    public virtual object?[] IsamRead(long handle, long recno) => CommercialNotAvailable<object?[]>(nameof(IsamRead));
    public virtual object?[] IsamReadCurrent(long handle) => CommercialNotAvailable<object?[]>(nameof(IsamReadCurrent));
    public virtual string[] IsamFieldNames(long handle) => CommercialNotAvailable<string[]>(nameof(IsamFieldNames));
    public virtual long IsamGetRecordCount(long handle) => CommercialNotAvailable<long>(nameof(IsamGetRecordCount));
    public virtual long IsamRecordSize(long handle) => CommercialNotAvailable<long>(nameof(IsamRecordSize));
    public virtual void IsamUse(long handle, string alias, object? type) => CommercialNotAvailable(nameof(IsamUse));
    public virtual void IsamSelect(long workArea) => CommercialNotAvailable(nameof(IsamSelect));
    public virtual void IsamGo(long handle, long recno) => CommercialNotAvailable(nameof(IsamGo));
    public virtual void IsamSkip(long handle, int count) => CommercialNotAvailable(nameof(IsamSkip));
    public virtual void IsamSeek(long handle, object key) => CommercialNotAvailable(nameof(IsamSeek));
    public virtual void IsamSetOrder(long handle, int indexNum) => CommercialNotAvailable(nameof(IsamSetOrder));
    public virtual void IsamSetFilter(long handle, string? filterExpr) => CommercialNotAvailable(nameof(IsamSetFilter));
    public virtual void IsamPack(long handle) => CommercialNotAvailable(nameof(IsamPack));
    public virtual void IsamReindex(long handle) => CommercialNotAvailable(nameof(IsamReindex));
    public virtual bool IsamFound(long handle) => CommercialNotAvailable<bool>(nameof(IsamFound));
    public virtual bool IsamEof(long handle) => CommercialNotAvailable<bool>(nameof(IsamEof));
    public virtual bool IsamBof(long handle) => CommercialNotAvailable<bool>(nameof(IsamBof));
    public virtual void IsamRecall(long handle) => CommercialNotAvailable(nameof(IsamRecall));
    public virtual void IsamSetFilterExpr(long handle, Basic.Expr? filterExpr, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) => CommercialNotAvailable(nameof(IsamSetFilterExpr));
    public virtual void IsamCreateIndex(long handle, string indexName, string keyExpression, long keyLength) => CommercialNotAvailable(nameof(IsamCreateIndex));
    public virtual void IsamDropIndex(long handle, string indexName) => CommercialNotAvailable(nameof(IsamDropIndex));
    public virtual void IsamRepair(long handle) => CommercialNotAvailable(nameof(IsamRepair));
    public virtual void IsamBackup(long handle, string destPath) => CommercialNotAvailable(nameof(IsamBackup));
    public virtual bool IsamValidate(long handle) => CommercialNotAvailable<bool>(nameof(IsamValidate));
    public virtual string IsamValidateJson(long handle) => CommercialNotAvailable<string>(nameof(IsamValidateJson));
    public virtual void IsamCopyTo(long handle, string destPath, Basic.Expr? condition, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) => CommercialNotAvailable(nameof(IsamCopyTo));
    public virtual void IsamSortTo(long handle, string destPath, string[] fields, bool[] descending, Basic.Expr? condition, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) => CommercialNotAvailable(nameof(IsamSortTo));
    public virtual void IsamJoin(long handle, long secondaryHandle, string destPath, Basic.Expr condition, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) => CommercialNotAvailable(nameof(IsamJoin));
    public virtual void IsamSetRelation(long handle, long parentHandle, string keyExpression) => CommercialNotAvailable(nameof(IsamSetRelation));
    public virtual void IsamLock(long handle, bool isRecordLock) => CommercialNotAvailable(nameof(IsamLock));
    public virtual void IsamUnlock(long handle, bool isRecordLock) => CommercialNotAvailable(nameof(IsamUnlock));

    // ================================================================
    // DBF — [COMMERCIALE] implementato in partial commerciale (modules/Database)
    // ================================================================
    public virtual long DbfOpen(string path) => CommercialNotAvailable<long>(nameof(DbfOpen));
    public virtual void DbfClose(long handle) => DisposeHandle(handle);
    public virtual void DbfAppend(long handle, object[] values) => CommercialNotAvailable(nameof(DbfAppend));
    public virtual void DbfAppendByName(long handle, System.Collections.Generic.Dictionary<string, object> fields) => CommercialNotAvailable(nameof(DbfAppendByName));
    public virtual object? DbfFieldByName(long handle, string fieldName) => CommercialNotAvailable<object?>(nameof(DbfFieldByName));
    public virtual int DbfFieldIndex(long handle, string fieldName) => CommercialNotAvailable<int>(nameof(DbfFieldIndex));
    public virtual object?[] DbfRead(long handle, long recno) => CommercialNotAvailable<object?[]>(nameof(DbfRead));
    public virtual void DbfUpdate(long handle, long recno, object[] values) => CommercialNotAvailable(nameof(DbfUpdate));
    public virtual void DbfDelete(long handle, long recno) => CommercialNotAvailable(nameof(DbfDelete));
    public virtual long DbfGetRecordCount(long handle) => CommercialNotAvailable<long>(nameof(DbfGetRecordCount));
    public virtual long DbfUse(string path, object?[] indexPaths, int locking, bool unsafeFlag, string? alias, object? typeId, string? typeName)
        => CommercialNotAvailable<long>(nameof(DbfUse));
    public virtual void DbfSelectWorkArea(long workArea) => CommercialNotAvailable(nameof(DbfSelectWorkArea));
    public virtual void DbfSelectHandle(long handle) => CommercialNotAvailable(nameof(DbfSelectHandle));
    public virtual object?[] DbfGetRecord(long handle) => CommercialNotAvailable<object?[]>(nameof(DbfGetRecord));
    public virtual System.Collections.Generic.Dictionary<string, object?> DbfGetRecordNamed(long handle) => CommercialNotAvailable<System.Collections.Generic.Dictionary<string, object?>>(nameof(DbfGetRecordNamed));
    public virtual void DbfReplace(long handle, int fieldIndex, object value) => CommercialNotAvailable(nameof(DbfReplace));
    public virtual void DbfReplaceByName(long handle, string fieldName, object? value) => CommercialNotAvailable(nameof(DbfReplaceByName));
    public virtual void DbfRecall(long handle) => CommercialNotAvailable(nameof(DbfRecall));
    public virtual void DbfPack(long handle) => CommercialNotAvailable(nameof(DbfPack));
    public virtual void DbfZap(long handle) => CommercialNotAvailable(nameof(DbfZap));
    public virtual void DbfGo(long handle, long recno) => CommercialNotAvailable(nameof(DbfGo));
    public virtual void DbfGoBottom(long handle) => CommercialNotAvailable(nameof(DbfGoBottom));
    public virtual void DbfSkip(long handle, int count) => CommercialNotAvailable(nameof(DbfSkip));
    public virtual bool DbfSeek(long handle, object key) => CommercialNotAvailable<bool>(nameof(DbfSeek));
    public virtual void DbfSetOrder(long handle, int indexOrder) => CommercialNotAvailable(nameof(DbfSetOrder));
    public virtual void DbfSetOrderByName(long handle, string name) => CommercialNotAvailable(nameof(DbfSetOrderByName));
    public virtual void DbfSetFilter(long handle, string? filterExpr) => CommercialNotAvailable(nameof(DbfSetFilter));
    public virtual void DbfSetDeleted(long handle, bool on) => CommercialNotAvailable(nameof(DbfSetDeleted));
    public virtual void DbfSetExact(long handle, bool on) => CommercialNotAvailable(nameof(DbfSetExact));
    public virtual void DbfAppendBlank(long handle) => CommercialNotAvailable(nameof(DbfAppendBlank));
    public virtual bool DbfEof(long handle) => CommercialNotAvailable<bool>(nameof(DbfEof));
    public virtual bool DbfBof(long handle) => CommercialNotAvailable<bool>(nameof(DbfBof));
    public virtual long DbfRecno(long handle) => CommercialNotAvailable<long>(nameof(DbfRecno));
    public virtual long DbfActiveOrder(long handle) => CommercialNotAvailable<long>(nameof(DbfActiveOrder));
    public virtual bool DbfFound(long handle) => CommercialNotAvailable<bool>(nameof(DbfFound));
    public virtual bool DbfIsDeleted(long handle) => CommercialNotAvailable<bool>(nameof(DbfIsDeleted));
    public virtual void DbfCloseAll() { }
    public virtual void DbfReindex(long handle, long unused) => CommercialNotAvailable(nameof(DbfReindex));
    public virtual void DbfIndexOn(long handle, string keyExpr, string file, int unique) => CommercialNotAvailable(nameof(DbfIndexOn));
    public virtual bool DbfValidate(long handle) => CommercialNotAvailable<bool>(nameof(DbfValidate));
    public virtual void DbfList(long handle, string fieldsCsv, string destination, bool toPrint) => CommercialNotAvailable(nameof(DbfList));
    public virtual void DbfListCond(long handle, string fieldsCsv, string destination, bool toPrint, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? condition) => CommercialNotAvailable(nameof(DbfListCond));
    public virtual void DbfReport(long handle, string formFile, string destination, bool toPrint) => CommercialNotAvailable(nameof(DbfReport));
    public virtual void DbfAggregate(long handle, string keyField, string destPath, string aggregations) => CommercialNotAvailable(nameof(DbfAggregate));
    public virtual void DbfCreateView(string viewName) => CommercialNotAvailable(nameof(DbfCreateView));
    public virtual void DbfDropView(string viewName) => CommercialNotAvailable(nameof(DbfDropView));
    public virtual void DbfSortTo(long handle, string destPath, string[] fields, bool[] descending) => CommercialNotAvailable(nameof(DbfSortTo));
    public virtual void DbfTotalOn(long handle, string destPath, Basic.Expr keyExpr, Basic.Expr[] sumFieldExprs, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) => CommercialNotAvailable(nameof(DbfTotalOn));
    public virtual void DbfCopyTo(long handle, string destPath, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) => CommercialNotAvailable(nameof(DbfCopyTo));
    public virtual void DbfAppendFrom(long handle, string sourcePath, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) => CommercialNotAvailable(nameof(DbfAppendFrom));
    public virtual void DbfCopyStructure(long handle, string destPath) => CommercialNotAvailable(nameof(DbfCopyStructure));
    public virtual void DbfSetRelation(long handle, long parentHandle, string keyExpression) => CommercialNotAvailable(nameof(DbfSetRelation));
    public virtual void DbfJoin(long handle, long secondaryHandle, string destPath, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator) => CommercialNotAvailable(nameof(DbfJoin));
    public virtual void DbfUpdate(long handle, string keyField, string[] fieldNames, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[] valueEvaluators) => CommercialNotAvailable(nameof(DbfUpdate));
    public virtual void DbfUpdateOn(long destHandle, long srcHandle, string srcAlias, string keyField, string[] fieldNames,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>,
                    System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[] valueEvaluators) => CommercialNotAvailable(nameof(DbfUpdateOn));
    public virtual void DbfSetExclusive(long handle, bool on) => CommercialNotAvailable(nameof(DbfSetExclusive));
    public virtual void DbfSetSchema(long handle, bool on) => CommercialNotAvailable(nameof(DbfSetSchema));
    public virtual void DbfCreateFile(string path, string fieldDefs) => CommercialNotAvailable(nameof(DbfCreateFile));

    // ================================================================
    // SPEED — [COMMERCIALE] implementato in partial commerciale (modules/Database)
    // ================================================================
    public virtual long SpeedOpen(string path) => CommercialNotAvailable<long>(nameof(SpeedOpen));
    public virtual long SpeedOpenSchema(string tableName, long dbHandle, string tableAlias, string schemaJson) => CommercialNotAvailable<long>(nameof(SpeedOpenSchema));
    public virtual void SpeedClose(long handle) => DisposeHandle(handle);
    public virtual void SpeedInsert(long handle, string key, object[] values) => CommercialNotAvailable(nameof(SpeedInsert));
    public virtual void SpeedInsertByName(long handle, System.Collections.Generic.Dictionary<string, object> fields) => CommercialNotAvailable(nameof(SpeedInsertByName));
    public virtual object?[] SpeedGet(long handle, string key) => CommercialNotAvailable<object?[]>(nameof(SpeedGet));
    public virtual object?[] SpeedGetCurrent(long handle) => CommercialNotAvailable<object?[]>(nameof(SpeedGetCurrent));
    public virtual void SpeedDelete(long handle, string key) => CommercialNotAvailable(nameof(SpeedDelete));
    public virtual void SpeedSeek(long handle, string key) => CommercialNotAvailable(nameof(SpeedSeek));
    public virtual bool SpeedEof(long handle) => CommercialNotAvailable<bool>(nameof(SpeedEof));
    public virtual void SpeedSkip(long handle, int count) => CommercialNotAvailable(nameof(SpeedSkip));
    public virtual void SpeedGo(long handle, long position) => CommercialNotAvailable(nameof(SpeedGo));
    public virtual void SpeedSetOrder(long handle, string indexName) => CommercialNotAvailable(nameof(SpeedSetOrder));
    public virtual void SpeedSetFilter(long handle, string? filterExpr) => CommercialNotAvailable(nameof(SpeedSetFilter));
    public virtual void SpeedSetRelation(long handle, long parentHandle, string keyExpression) => CommercialNotAvailable(nameof(SpeedSetRelation));
    public virtual void SpeedCreateIndex(long handle, string indexName, string keyExpression) => CommercialNotAvailable(nameof(SpeedCreateIndex));
    public virtual void SpeedDropIndex(long handle, string indexName) => CommercialNotAvailable(nameof(SpeedDropIndex));
    public virtual Basic.Databases.Speed.Database? SpeedGetDatabaseFor(long handle) => null;
    public virtual string? SpeedGetTableNameFor(long handle) => null;
    public virtual bool SpeedCreateViewTable(long dbHandle, string viewName, string schemaJson) => false;

    // ================================================================
    // SQLite — COMMUNITY
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

    // VectorDB — COMMUNITY
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
    // Ledger — [COMMERCIALE] implementato in partial commerciale (modules/Database)
    // ================================================================
    public virtual long LedgerOpen(string path) => CommercialNotAvailable<long>(nameof(LedgerOpen));
    public virtual void LedgerClose(long handle) => DisposeHandle(handle);
    public virtual long LedgerAppend(long handle, string category, object[] values) => CommercialNotAvailable<long>(nameof(LedgerAppend));
    public virtual long LedgerUpdate(long handle, long id, object[] values) => CommercialNotAvailable<long>(nameof(LedgerUpdate));
    public virtual object?[] LedgerRead(long handle, long recno) => CommercialNotAvailable<object?[]>(nameof(LedgerRead));
    public virtual long LedgerGetCount(long handle) => CommercialNotAvailable<long>(nameof(LedgerGetCount));
    public virtual void LedgerGoTop(long handle) => CommercialNotAvailable(nameof(LedgerGoTop));
    public virtual void LedgerGoBottom(long handle) => CommercialNotAvailable(nameof(LedgerGoBottom));
    public virtual void LedgerGo(long handle, long recno) => CommercialNotAvailable(nameof(LedgerGo));
    public virtual void LedgerSkip(long handle, int count) => CommercialNotAvailable(nameof(LedgerSkip));
    public virtual bool LedgerBof(long handle) => CommercialNotAvailable<bool>(nameof(LedgerBof));
    public virtual bool LedgerEof(long handle) => CommercialNotAvailable<bool>(nameof(LedgerEof));
    public virtual long LedgerRecno(long handle) => CommercialNotAvailable<long>(nameof(LedgerRecno));
    public virtual bool LedgerFound(long handle) => CommercialNotAvailable<bool>(nameof(LedgerFound));
    public virtual bool LedgerValidate(long handle) => CommercialNotAvailable<bool>(nameof(LedgerValidate));
    public virtual void LedgerDelete(long handle, long id) => CommercialNotAvailable(nameof(LedgerDelete));
    public virtual void LedgerPack(long handle) => CommercialNotAvailable(nameof(LedgerPack));
    public virtual void LedgerSave(long handle) => CommercialNotAvailable(nameof(LedgerSave));
    public virtual object?[] LedgerRegister(long handle, string query) => CommercialNotAvailable<object?[]>(nameof(LedgerRegister));
    public virtual object?[] LedgerBalance(long handle, string query) => CommercialNotAvailable<object?[]>(nameof(LedgerBalance));
    public virtual object?[] LedgerGet(long handle, long id) => CommercialNotAvailable<object?[]>(nameof(LedgerGet));
    public virtual string[] LedgerAccounts(long handle, string? root) => CommercialNotAvailable<string[]>(nameof(LedgerAccounts));
    public virtual void LedgerBudget(long handle, string account, decimal amount, string period) => CommercialNotAvailable(nameof(LedgerBudget));
    public virtual object? LedgerVariance(long handle, string account, string period) => CommercialNotAvailable<object?>(nameof(LedgerVariance));
    public virtual object? LedgerPlot(long handle, string query, string chartType) => CommercialNotAvailable<object?>(nameof(LedgerPlot));
    public virtual double LedgerImport(long handle, string csvPath, string mappingRules) => CommercialNotAvailable<double>(nameof(LedgerImport));

    // ================================================================

    // ================================================================
    // Helpers (usati anche dalla partial commerciale)
    // ================================================================
    protected T? Get<T>(long handle) where T : class => _handles.TryGetValue(handle, out var e) ? e as T : null;
    protected void DisposeHandle(long handle) { if (_handles.TryRemove(handle, out var e) && e is IDisposable d) d.Dispose(); }
    protected long NextHandle => _nextHandle++;
    protected void StoreHandle(long handle, object value) => _handles[handle] = value;

    // Errore chiaro per i comandi commerciali in Community (D11)
    protected static void CommercialNotAvailable(string command) =>
        throw new CommercialEditionRequiredException(command);

    protected static T CommercialNotAvailable<T>(string command) =>
        throw new CommercialEditionRequiredException(command);

    public void Dispose() { foreach (var kv in _handles) if (kv.Value is IDisposable d) d.Dispose(); _handles.Clear(); }
    public virtual void DbfSetDurability(long handle, string mode) => CommercialNotAvailable(nameof(DbfSetDurability));
    public virtual void DbfFlushDurability(long handle) => CommercialNotAvailable(nameof(DbfFlushDurability));
    public virtual void IsamSetReplication(long handle, string logPath) => CommercialNotAvailable(nameof(IsamSetReplication));
    public virtual void SpeedSetDurability(long handle, string mode) => CommercialNotAvailable(nameof(SpeedSetDurability));
    public virtual void SpeedFlushDurability(long handle) => CommercialNotAvailable(nameof(SpeedFlushDurability));
    public virtual void SpeedBeginGroupCommit(long handle) => CommercialNotAvailable(nameof(SpeedBeginGroupCommit));
    public virtual void SpeedEndGroupCommit(long handle) => CommercialNotAvailable(nameof(SpeedEndGroupCommit));
    public virtual string DbfSetReplication(long handle, string logPath) => CommercialNotAvailable<string>(nameof(DbfSetReplication));
    public virtual void DbfBeginGroupCommit(long handle) => CommercialNotAvailable(nameof(DbfBeginGroupCommit));
    public virtual void DbfEndGroupCommit(long handle) => CommercialNotAvailable(nameof(DbfEndGroupCommit));
    public virtual int DbfApplyReplication(long handle, string replLogPath) => CommercialNotAvailable<int>(nameof(DbfApplyReplication));
    public virtual int DbfFollowReplication(long handle, string replLogPath) => CommercialNotAvailable<int>(nameof(DbfFollowReplication));
    public virtual long DbfAppendMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records) => CommercialNotAvailable<long>(nameof(DbfAppendMany));
    public virtual long IsamAppendMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records) => CommercialNotAvailable<long>(nameof(IsamAppendMany));
    public virtual int IsamApplyReplication(long handle, string replLogPath) => CommercialNotAvailable<int>(nameof(IsamApplyReplication));
    public virtual long SpeedSetMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records) => CommercialNotAvailable<long>(nameof(SpeedSetMany));

}
