// FILE: Basic.Runtime.IServices.IDatabaseService.cs

namespace Basic.Runtime;

public interface IDatabaseService
{
    // === ISAM ===
    long IsamOpen(string path);
    long IsamOpenSchema(string path, string alias, string schemaJson);
    void IsamClose(long handle);
    void IsamInsert(long handle, object[] values);
    void IsamUpdate(long handle, long recno, object[] values);
    void IsamPut(long handle, object[] values);
    void IsamDelete(long handle, long recno);
    object?[] IsamRead(long handle, long recno);
    object?[] IsamReadCurrent(long handle);
    string[] IsamFieldNames(long handle);
    long IsamGetRecordCount(long handle);
    long IsamRecordSize(long handle);
    void IsamUse(long handle, string alias, object? type);
    void IsamSelect(long workArea);
    void IsamGo(long handle, long recno);
    void IsamSkip(long handle, int count);
    void IsamSeek(long handle, object key);
    void IsamSetOrder(long handle, int indexNum);
    void IsamSetFilter(long handle, string? filterExpr);
    void IsamPack(long handle);
    void IsamReindex(long handle);
    bool IsamFound(long handle);
    bool IsamEof(long handle);
    bool IsamBof(long handle);
    void IsamRecall(long handle);
    void IsamSetFilterExpr(long handle, Basic.Expr? filterExpr, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator);
    void IsamCreateIndex(long handle, string indexName, string keyExpression, long keyLength);
    void IsamDropIndex(long handle, string indexName);
    void IsamRepair(long handle);
    void IsamBackup(long handle, string destPath);
    bool IsamValidate(long handle);
    string IsamValidateJson(long handle);
    void IsamCopyTo(long handle, string destPath, Basic.Expr? condition, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator);
    void IsamSortTo(long handle, string destPath, string[] fields, bool[] descending, Basic.Expr? condition, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator);
    void IsamJoin(long handle, long secondaryHandle, string destPath, Basic.Expr condition, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator);
    void IsamSetRelation(long handle, long parentHandle, string keyExpression);
    void IsamLock(long handle, bool isRecordLock);
    void IsamUnlock(long handle, bool isRecordLock);

    // === DBF ===
    long DbfOpen(string path);
    void DbfClose(long handle);
    void DbfAppend(long handle, object[] values);
    /// <summary>`DBF APPEND RECORD {Campo: valore, ...}` (literal UDT): a differenza di
    /// <see cref="DbfAppend"/> (posizionale), i campi arrivano per NOME — riordinati qui
    /// contro lo schema reale del driver (stesso motivo/fix di SpeedInsertByName).</summary>
    void DbfAppendByName(long handle, System.Collections.Generic.Dictionary<string, object> fields);
    /// <summary>Legge il campo NOMINATO del record CORRENTE (late-binding Clipper: campo
    /// nudo, es. `ITEM` dopo `DBF SELECT ORD`). Null se il campo non esiste o l'area non
    /// ha un record corrente.</summary>
    object? DbfFieldByName(long handle, string fieldName);
    int DbfFieldIndex(long handle, string fieldName);
    object?[] DbfRead(long handle, long recno);
    void DbfUpdate(long handle, long recno, object[] values);
    void DbfDelete(long handle, long recno);
    long DbfGetRecordCount(long handle);
    long DbfUse(string path, object?[] indexPaths, int locking, bool unsafeFlag, string? alias, object? typeId, string? typeName);
    void DbfSelectWorkArea(long workArea);
    /// <summary>`DBF SELECT alias`: come <see cref="DbfSelectWorkArea"/> ma per handle
    /// (risolto da IlRuntime tramite l'alias) invece di numero di area diretto.</summary>
    void DbfSelectHandle(long handle);
    object?[] DbfGetRecord(long handle);
    /// <summary>Record corrente come nome campo → valore (snapshot per `DBF GET &lt;var&gt;`
    /// late-binding: la variabile dinamica è un dict letto da DynamicFieldGet).</summary>
    System.Collections.Generic.Dictionary<string, object?> DbfGetRecordNamed(long handle);
    void DbfReplace(long handle, int fieldIndex, object value);
    /// <summary>REPLACE <c>fieldName</c> WITH value per NOME campo — usato quando il Linker
    /// risolve il target tramite CurrentUdtScopeId (UDT reale via `AS Type`), il cui
    /// FieldSymbolId è l'ID GLOBALE del campo nel Linker, non l'indice posizionale che si
    /// aspetta <see cref="DbfReplace"/>. Stesso pattern di <see cref="DbfFieldByName"/>
    /// (già usato per le letture late-binding).</summary>
    void DbfReplaceByName(long handle, string fieldName, object? value);
    void DbfRecall(long handle);
    void DbfPack(long handle);
    void DbfZap(long handle);
    void DbfGo(long handle, long recno);
    void DbfGoBottom(long handle);
    void DbfSkip(long handle, int count);
    bool DbfSeek(long handle, object key);
    void DbfSetOrder(long handle, int indexOrder);
    /// <summary>SET ORDER TO "nome indice": seleziona l'ordine per NOME (il driver risolve).</summary>
    void DbfSetOrderByName(long handle, string name);
    void DbfSetFilter(long handle, string? filterExpr);
    void DbfSetDeleted(long handle, bool on);
    void DbfSetExact(long handle, bool on);
    void DbfAppendBlank(long handle);
    bool DbfEof(long handle);
    bool DbfBof(long handle);
    long DbfRecno(long handle);
    long DbfActiveOrder(long handle);
    bool DbfFound(long handle);
    bool DbfIsDeleted(long handle);
    void DbfCloseAll();
    void DbfReindex(long handle, long unused);
    void DbfIndexOn(long handle, string keyExpr, string file, int unique);
    bool DbfValidate(long handle);
    void DbfList(long handle, string fieldsCsv, string destination, bool toPrint);
    /// <summary>LIST con condizione FOR (evaluator per-record; null = nessun filtro).</summary>
    void DbfListCond(long handle, string fieldsCsv, string destination, bool toPrint, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? condition);
    void DbfReport(long handle, string formFile, string destination, bool toPrint);
    void DbfAggregate(long handle, string keyField, string destPath, string aggregations);
    void DbfCreateView(string viewName);
    void DbfDropView(string viewName);
    void DbfSortTo(long handle, string destPath, string[] fields, bool[] descending);
    void DbfTotalOn(long handle, string destPath, Basic.Expr keyExpr, Basic.Expr[] sumFieldExprs, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator);
    void DbfCopyTo(long handle, string destPath, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator);
    void DbfAppendFrom(long handle, string sourcePath, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator);
    void DbfCopyStructure(long handle, string destPath);
    void DbfSetRelation(long handle, long parentHandle, string keyExpression);
    void DbfJoin(long handle, long secondaryHandle, string destPath, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? evaluator);
    void DbfUpdate(long handle, string keyField, string[] fieldNames, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[] valueEvaluators);
    /// <summary>DBF UPDATE ON &lt;key&gt; FROM &lt;alias&gt; REPLACE …: aggiorna la destinazione
    /// per chiave leggendo dal SORGENTE. I callback ricevono (ctxDestinazione, ctxSorgente).</summary>
    void DbfUpdateOn(long destHandle, long srcHandle, string srcAlias, string keyField, string[] fieldNames,
        System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>,
                    System.Collections.Generic.IReadOnlyDictionary<string, object?>, object?>[] valueEvaluators);
    void DbfSetExclusive(long handle, bool on);
    void DbfSetSchema(long handle, bool on);
    void DbfCreateFile(string path, string fieldDefs);

    // === SPEED ===
    long SpeedOpen(string path);
    /// <summary>`SPEED USE tableName FROM dbAlias OF Type ALIAS tableAlias [INDEXED BY key]`:
    /// lega l'alias della TABELLA (non del database, già aperto via SpeedOpen — `dbHandle` è
    /// l'handle di QUELL'apertura, risolto dal chiamante) a una tabella specifica per nome, con
    /// lo schema derivato dal tipo UDT (stesso formato JSON di <see cref="IsamOpenSchema"/> —
    /// TypeName/Fields/PrimaryKey/Indexes). Se la tabella non esiste ancora nel database la
    /// crea; se esiste già la riusa (schema del manifest, quello passato qui viene ignorato in
    /// quel caso).</summary>
    long SpeedOpenSchema(string tableName, long dbHandle, string tableAlias, string schemaJson);
    void SpeedClose(long handle);
    void SpeedInsert(long handle, string key, object[] values);
    /// <summary>`SPEED PUT alias, {Campo: valore, ...}` (literal UDT): a differenza di
    /// <see cref="SpeedInsert"/> (posizionale, l'ordine di `values` DEVE già combaciare con lo
    /// schema), qui i campi sono per NOME — riordinati qui contro lo schema REALE della
    /// tabella (noto solo a runtime, non a compile-time per un dict-literal).</summary>
    void SpeedInsertByName(long handle, System.Collections.Generic.Dictionary<string, object> fields);
    object?[] SpeedGet(long handle, string key);
    /// <summary>`SPEED GET alias, record`: legge il record CORRENTE (posizionato da GO/SEEK/
    /// SKIP), nessun Seek implicito — a differenza di <see cref="SpeedGet"/> che cerca sempre
    /// per chiave.</summary>
    object?[] SpeedGetCurrent(long handle);
    void SpeedDelete(long handle, string key);
    void SpeedSeek(long handle, string key);
    bool SpeedEof(long handle);
    void SpeedSkip(long handle, int count);
    void SpeedGo(long handle, long position);
    void SpeedSetOrder(long handle, string indexName);
    void SpeedSetFilter(long handle, string? filterExpr);
    /// <summary>`handle` = driver FIGLIO (chi riceve la relazione), `parentHandle` = driver
    /// PADRE (stesso contratto di <see cref="DbfSetRelation"/>). `keyExpression` vuota →
    /// rimuove la relazione uscente da `handle` (usato anche come padre in quel caso).</summary>
    void SpeedSetRelation(long handle, long parentHandle, string keyExpression);
    void SpeedCreateIndex(long handle, string indexName, string keyExpression);
    void SpeedDropIndex(long handle, string indexName);
    /// <summary>Bridge verso il motore SPEED grezzo (Basic.Databases.Speed.Database) per
    /// CREATE/REFRESH/DROP VIEW e SELECT — queste feature orchestrano query multi-tabella
    /// (JOIN/GROUP BY/aggregati) direttamente sul motore, che IlBackend può referenziare
    /// (BRT -> modules/Database, transitivo), a differenza delle operazioni CRUD/navigazione
    /// che passano sempre per singolo handle/driver.</summary>
    Basic.Databases.Speed.Database? SpeedGetDatabaseFor(long handle);
    /// <summary>Nome REALE della tabella nel manifest SPEED per l'alias/handle dato (può
    /// differire dall'alias runtime, es. `SPEED USE "Clienti" ... ALIAS Cl`) — serve a
    /// costruire il QuerySource.TableName di una query (Database.ExecuteQuery cerca la
    /// tabella per nome-manifest, non per alias).</summary>
    string? SpeedGetTableNameFor(long handle);
    /// <summary>Crea (find-or-create, vedi Database.CreateTable) la tabella fisica per una
    /// VIEW materializzata SPEED, dallo stesso formato schema JSON di
    /// <see cref="SpeedOpenSchema"/> (Fields/PrimaryKey) — usato da CREATE VIEW, che risolve
    /// il TYPE di destinazione a compile-time (IlBackend) ma delega qui il parsing
    /// DataType-da-stringa (già presente per SpeedOpenSchema, non duplicato in IlRuntime).</summary>
    bool SpeedCreateViewTable(long dbHandle, string viewName, string schemaJson);

    // === SQLite ===
    long SqlOpen(string path);
    void SqlClose(long handle);
    void SqlExecute(long handle, string sql);
    object?[][] SqlQuery(long handle, string sql);
    long SqlLastInsertRowId(long handle);
    void SqlBeginTransaction(long handle);
    void SqlCommit(long handle);
    void SqlRollback(long handle);

    // === Ledger ===
    long LedgerOpen(string path);
    void LedgerClose(long handle);
    long LedgerAppend(long handle, string category, object[] values);
    long LedgerUpdate(long handle, long id, object[] values);
    object?[] LedgerRead(long handle, long recno);
    long LedgerGetCount(long handle);
    void LedgerGoTop(long handle);
    void LedgerGoBottom(long handle);
    void LedgerGo(long handle, long recno);
    void LedgerSkip(long handle, int count);
    bool LedgerBof(long handle);
    bool LedgerEof(long handle);
    long LedgerRecno(long handle);
    bool LedgerFound(long handle);
    bool LedgerValidate(long handle);
    void LedgerDelete(long handle, long id);
    void LedgerPack(long handle);
    void LedgerSave(long handle);
    object?[] LedgerRegister(long handle, string query);
    object?[] LedgerBalance(long handle, string query);
    object?[] LedgerGet(long handle, long id);
    string[] LedgerAccounts(long handle, string? root);
    void LedgerBudget(long handle, string account, decimal amount, string period);
    object? LedgerVariance(long handle, string account, string period);
    object? LedgerPlot(long handle, string query, string chartType);
    double LedgerImport(long handle, string csvPath, string mappingRules);


    // === VectorDB ===
    long VectorOpen(string path);
    void VectorClose(long handle);
    void VectorInsert(long handle, float[] vector, object? metadata);
    object?[] VectorSearch(long handle, float[] query, int topK);
    object?[] VectorSearchText(string dbPath, string collection, string query, int topK);
    long VectorCount(string dbPath, string collection);
    void VectorEnsureCollection(string dbPath, string collection);
    void VectorIndexText(string dbPath, string collection, string docId, string text);
    void VectorLoadDocuments(string dbPath, string dirPath, string collection);

    void DbfSetDurability(long handle, string mode);
    void DbfFlushDurability(long handle);
    void IsamSetReplication(long handle, string logPath);
    void SpeedSetDurability(long handle, string mode);
    void SpeedFlushDurability(long handle);
    void SpeedBeginGroupCommit(long handle);
    void SpeedEndGroupCommit(long handle);
    string DbfSetReplication(long handle, string logPath);
    void DbfBeginGroupCommit(long handle);
    void DbfEndGroupCommit(long handle);
    int DbfApplyReplication(long handle, string replLogPath);
    int DbfFollowReplication(long handle, string replLogPath);
    long DbfAppendMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records);
    long IsamAppendMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records);
    int IsamApplyReplication(long handle, string replLogPath);
    long SpeedSetMany(long handle, System.Collections.Generic.List<System.Collections.Generic.Dictionary<string, object?>> records);
}
