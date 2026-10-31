/* FILE: native/gmb_db_c/src/SPEED/Database.Indexes.c */
/* Port 1:1 di modules/Database/SPEED/Database.Indexes.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"
#include <unistd.h>

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Database.Indexes
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases.Speed;
//
//public partial class Database
//{
//    /// <summary>
//    /// Creates a secondary index table, registers it in the main table schema, and populates it.
//    /// </summary>
//    /// <param name="mainTableId">Main table identifier.</param>
//    /// <param name="indexDef">Secondary index definition.</param>
//    /// <param name="keyEvaluator">Optional evaluator for complex key expressions.</param>
//    /// <returns>Metadata for the new index table.</returns>
//    public TableMetadata CreateSecondaryIndex(int mainTableId, Config.SpeedIndexDefinition indexDef,
//        ExpressionEvaluator? keyEvaluator = null)
//    {
//        ArgumentNullException.ThrowIfNull(indexDef);
//        if (!Manifest.TryGetTable(mainTableId, out var mainMeta) || mainMeta is null)
//            throw new InvalidOperationException($"Tabella con ID {mainTableId} non trovata.");
//
//        if (keyEvaluator is not null)
//            _indexKeyEvaluators[mainTableId] = keyEvaluator;
//
//        var indexTableName = GetIndexTableName(mainMeta.Name, indexDef.Name);
//        if (Manifest.TryGetTable(indexTableName, out var existing) && existing is not null)
//            return existing;
//
//        var indexSchema = BuildIndexTableSchema(mainMeta.Name, indexDef);
//        lock (_lock)
//        {
//            Manifest.AddSecondaryIndex(mainMeta.Name, indexDef);
//            var indexMeta = Manifest.CreateTable(indexSchema);
//            _tableLocks.TryAdd(indexMeta.Id, new object());
//            PopulateIndexTable(mainTableId, indexMeta.Id, indexDef);
//            Manifest.Save();
//            return indexMeta;
//        }
//    }
//
//    /// <summary>
//    /// Validates manifest readability and SST file presence; optionally compares index row counts.
//    /// ponytail: index row count vs main table is O(n) scan — skipped when main table exceeds 10k rows.
//    /// </summary>
//    /// <returns>Validation outcome.</returns>
//    public (bool IsValid, string Message) ValidateIntegrity() =>
//        ValidateSecondaryIndexCounts(enforceAllTableSizes: false);
//
//    /// <summary>
//    /// Offline/ops integrity: SST footers + secondary index row counts without the 10k cap.
//    /// ponytail: O(n) per table — CLI <c>validate --full</c> only; runtime uses <see cref="ValidateIntegrityQuick"/>.
//    /// </summary>
//    /// <returns>Validation outcome.</returns>
//    public (bool IsValid, string Message) ValidateIntegrityFull()
//    {
//        var quick = ValidateIntegrityQuick();
//        if (!quick.IsValid)
//        {
//            return quick;
//        }
//
//        return ValidateSecondaryIndexCounts(enforceAllTableSizes: true);
//    }
//
//    private (bool IsValid, string Message) ValidateSecondaryIndexCounts(bool enforceAllTableSizes)
//    {
//        var errors = new List<string>();
//        try
//        {
//            foreach (var table in Manifest.GetAllTables())
//            {
//                foreach (var level in table.SstFilesByLevel.Keys.OrderBy(k => k))
//                {
//                    foreach (var fileName in table.GetFilesForLevel(level))
//                    {
//                        var path = Path.Combine(DatabasePath, Config.SstDirectoryName, fileName);
//                        if (!File.Exists(path))
//                        {
//                            errors.Add($"SST mancante per tabella '{table.Name}': {fileName}");
//                        }
//                    }
//                }
//            }
//
//            foreach (var table in Manifest.GetAllTables().Where(t => !IsIndexTableName(t.Name)))
//            {
//                if (table.Schema.SecondaryKeys.Count == 0) continue;
//                var mainCount = Scan(table.Id).Count(kvp => kvp.Value is not null);
//                if (!enforceAllTableSizes && mainCount > 10_000) continue;
//
//                foreach (var indexDef in table.Schema.SecondaryKeys)
//                {
//                    var indexTableName = GetIndexTableName(table.Name, indexDef.Name);
//                    if (!Manifest.TryGetTable(indexTableName, out var indexMeta) || indexMeta is null)
//                    {
//                        errors.Add($"Tabella-indice mancante: {indexTableName}");
//                        continue;
//                    }
//
//                    var indexCount = Scan(indexMeta.Id).Count(kvp => kvp.Value is not null);
//                    if (indexCount != mainCount)
//                    {
//                        errors.Add(
//                            $"Conteggio indice '{indexDef.Name}' ({indexCount}) != main '{table.Name}' ({mainCount})");
//                    }
//                }
//            }
//        }
//        catch (Exception ex)
//        {
//            return (false, ex.Message);
//        }
//
//        return errors.Count == 0 ? (true, "OK") : (false, string.Join("; ", errors));
//    }
//
//    /// <summary>
//    /// Fast integrity check: manifest SST presence + readable SST footers + O(1) manifest row-count sanity when trusted.
//    /// ponytail: no O(n) scan — use <see cref="ValidateIntegrityFull"/> for full index parity offline.
//    /// </summary>
//    /// <returns>Validation outcome.</returns>
//    public (bool IsValid, string Message) ValidateIntegrityQuick()
//    {
//        var errors = new List<string>();
//        try
//        {
//            foreach (var table in Manifest.GetAllTables())
//            {
//                foreach (var level in table.SstFilesByLevel.Keys.OrderBy(k => k))
//                {
//                    foreach (var fileName in table.GetFilesForLevel(level))
//                    {
//                        var path = Path.Combine(DatabasePath, Config.SstDirectoryName, fileName);
//                        if (!File.Exists(path))
//                        {
//                            errors.Add($"SST mancante per tabella '{table.Name}': {fileName}");
//                            continue;
//                        }
//
//                        if (!SstReader.TryValidateFooter(path, out var footerError))
//                        {
//                            errors.Add($"SST non leggibile '{fileName}': {footerError}");
//                        }
//                    }
//                }
//            }
//
//            foreach (var table in Manifest.GetAllTables().Where(t => !IsIndexTableName(t.Name)))
//            {
//                if (table.Schema.SecondaryKeys.Count == 0) continue;
//                if (!Manifest.IsRowCountTrusted(table.Id)) continue;
//
//                var mainCount = Manifest.GetRowCount(table.Id);
//                foreach (var indexDef in table.Schema.SecondaryKeys)
//                {
//                    var indexTableName = GetIndexTableName(table.Name, indexDef.Name);
//                    if (!Manifest.TryGetTable(indexTableName, out var indexMeta) || indexMeta is null)
//                    {
//                        errors.Add($"Tabella-indice mancante: {indexTableName}");
//                        continue;
//                    }
//
//                    if (!Manifest.IsRowCountTrusted(indexMeta.Id)) continue;
//
//                    var indexCount = Manifest.GetRowCount(indexMeta.Id);
//                    if (indexCount != mainCount)
//                    {
//                        errors.Add(
//                            $"Conteggio manifest indice '{indexDef.Name}' ({indexCount}) != main '{table.Name}' ({mainCount})");
//                    }
//                }
//            }
//        }
//        catch (Exception ex)
//        {
//            return (false, ex.Message);
//        }
//
//        return errors.Count == 0 ? (true, "OK") : (false, string.Join("; ", errors));
//    }
//
//    private void EnsureSecondaryIndexTables(TableMetadata mainMeta)
//    {
//        if (mainMeta.Schema.SecondaryKeys.Count == 0) return;
//
//        lock (_lock)
//        {
//            foreach (var indexDef in mainMeta.Schema.SecondaryKeys)
//            {
//                var indexTableName = GetIndexTableName(mainMeta.Name, indexDef.Name);
//                if (Manifest.TryGetTable(indexTableName, out _))
//                    continue;
//
//                var indexSchema = BuildIndexTableSchema(mainMeta.Name, indexDef);
//                var indexMeta = Manifest.CreateTable(indexSchema);
//                _tableLocks.TryAdd(indexMeta.Id, new object());
//                PopulateIndexTable(mainMeta.Id, indexMeta.Id, indexDef);
//            }
//
//            Manifest.Save();
//        }
//    }
//
//    private void PopulateIndexTable(int mainTableId, int indexTableId, Config.SpeedIndexDefinition indexDef)
//    {
//        if (!Manifest.TryGetTable(mainTableId, out var mainMeta) || mainMeta is null) return;
//
//        var count = 0L;
//        foreach (var record in Scan(mainTableId))
//        {
//            if (record.Value is null) continue;
//            var row = DeserializeRecord(record.Value, mainMeta.Schema);
//            var indexKey = ComputeIndexKeyBytes(mainTableId, indexDef, row, mainMeta.Schema);
//            if (indexKey is null) continue;
//            WriteIndexEntry(indexTableId, indexKey, record.Key);
//            count++;
//        }
//
//        Manifest.SetRowCount(indexTableId, count);
//    }
//
//    private void MaintainSecondaryIndexesOnPut(int mainTableId, byte[] pk, byte[]? oldRecordBytes, byte[] newRecordBytes)
//    {
//        if (!Manifest.TryGetTable(mainTableId, out var mainMeta) || mainMeta is null) return;
//        if (mainMeta.Schema.SecondaryKeys.Count == 0) return;
//
//        var newRow = DeserializeRecord(newRecordBytes, mainMeta.Schema);
//        Dictionary<string, object?>? oldRow = oldRecordBytes is not null
//            ? DeserializeRecord(oldRecordBytes, mainMeta.Schema)
//            : null;
//
//        foreach (var indexDef in mainMeta.Schema.SecondaryKeys)
//        {
//            if (!TryResolveIndexTableId(mainMeta.Name, indexDef.Name, out var indexTableId)) continue;
//
//            if (oldRow is not null)
//            {
//                var oldIndexKey = ComputeIndexKeyBytes(mainTableId, indexDef, oldRow, mainMeta.Schema);
//                if (oldIndexKey is not null)
//                    WriteIndexDelete(indexTableId, oldIndexKey);
//            }
//
//            var newIndexKey = ComputeIndexKeyBytes(mainTableId, indexDef, newRow, mainMeta.Schema);
//            if (newIndexKey is not null)
//                WriteIndexEntry(indexTableId, newIndexKey, pk);
//        }
//    }
//
//    private void MaintainSecondaryIndexesOnDelete(int mainTableId, byte[] pk, byte[]? oldRecordBytes)
//    {
//        if (oldRecordBytes is null) return;
//        if (!Manifest.TryGetTable(mainTableId, out var mainMeta) || mainMeta is null) return;
//
//        var oldRow = DeserializeRecord(oldRecordBytes, mainMeta.Schema);
//        foreach (var indexDef in mainMeta.Schema.SecondaryKeys)
//        {
//            if (!TryResolveIndexTableId(mainMeta.Name, indexDef.Name, out var indexTableId)) continue;
//            var oldIndexKey = ComputeIndexKeyBytes(mainTableId, indexDef, oldRow, mainMeta.Schema);
//            if (oldIndexKey is not null)
//                WriteIndexDelete(indexTableId, oldIndexKey);
//        }
//    }
//
//    private void WriteIndexEntry(int indexTableId, byte[] indexKey, byte[] primaryKey)
//    {
//        AdjustRowCountOnSet(indexTableId, indexKey);
//        var wal = GetOrCreateWal(indexTableId);
//        wal.LogSet(indexTableId, indexKey, primaryKey);
//        MaybeFlushWal(wal);
//        GetOrCreateMemtable(indexTableId).Set(indexKey, primaryKey);
//    }
//
//    private void WriteIndexDelete(int indexTableId, byte[] indexKey)
//    {
//        AdjustRowCountOnDelete(indexTableId, indexKey);
//        var wal = GetOrCreateWal(indexTableId);
//        wal.LogDelete(indexTableId, indexKey);
//        MaybeFlushWal(wal);
//        GetOrCreateMemtable(indexTableId).Delete(indexKey);
//    }
//
//    private byte[]? ComputeIndexKeyBytes(int mainTableId, Config.SpeedIndexDefinition indexDef,
//        Dictionary<string, object?> row, Config.SpeedSchema schema)
//    {
//        object? keyObject;
//        if (_indexKeyEvaluators.TryGetValue(mainTableId, out var evaluator))
//        {
//            keyObject = evaluator(indexDef.KeyExpression, row);
//        }
//        else if (!TryGetFieldValue(row, indexDef.KeyExpression, out keyObject) &&
//                 !TryGetFieldValue(row, indexDef.Name, out keyObject))
//        {
//            // ponytail: complex expressions require SetIndexKeyEvaluator from GPM
//            return null;
//        }
//
//        return SpeedMarshaller.ToBytes(keyObject, indexDef.DataType, schema);
//    }
//
//    private static bool TryGetFieldValue(Dictionary<string, object?> row, string fieldName, out object? value) =>
//        row.TryGetValue(fieldName, out value);
//
//    private static Dictionary<string, object?> DeserializeRecord(byte[] recordBytes, Config.SpeedSchema schema) =>
//        (Dictionary<string, object?>)SpeedMarshaller.FromBytes(recordBytes, schema);
//
//    private static Config.SpeedSchema BuildIndexTableSchema(string mainTableName, Config.SpeedIndexDefinition indexDef)
//    {
//        return new Config.SpeedSchema(
//            GetIndexTableName(mainTableName, indexDef.Name),
//            [new Config.SpeedFieldSchema("PK", indexDef.DataType, indexDef.KeyLength)],
//            new Config.SpeedIndexDefinition(
//                indexDef.Name,
//                indexDef.KeyExpression,
//                indexDef.DataType,
//                indexDef.KeyLength,
//                true,
//                indexDef.IsUnsafe),
//            []);
//    }
//
//    private static string GetIndexTableName(string mainTableName, string indexName) =>
//        $"_idx_{mainTableName}_{indexName}";
//
//    private bool TryResolveIndexTableId(string mainTableName, string indexName, out int indexTableId)
//    {
//        indexTableId = -1;
//        var indexTableName = GetIndexTableName(mainTableName, indexName);
//        if (!Manifest.TryGetTable(indexTableName, out var indexMeta) || indexMeta is null)
//            return false;
//        indexTableId = indexMeta.Id;
//        return true;
//    }
//
//    private bool IsMainDataTable(int tableId)
//    {
//        if (!Manifest.TryGetTable(tableId, out var meta) || meta is null) return false;
//        return !IsIndexTableName(meta.Name);
//    }
//
//    private static bool IsIndexTableName(string tableName) =>
//        tableName.StartsWith("_idx_", StringComparison.Ordinal);
//
//    private bool HasSecondaryIndexes(int tableId)
//    {
//        if (!Manifest.TryGetTable(tableId, out var meta) || meta is null)
//        {
//            return false;
//        }
//
//        return meta.Schema.SecondaryKeys.Count > 0;
//    }
//
//    /// <summary>
//    /// Recupera il valore precedente per manutenzione indici: memtable prima, SST solo se assente in memoria.
//    /// ponytail: insert puro senza chiave in memtable evita scan SST (oldValue null).
//    /// </summary>
//    private byte[]? TryGetOldValueForIndex(int tableId, byte[] key)
//    {
//        var memtable = GetOrCreateMemtable(tableId);
//        if (memtable.Get(key, out var memValue))
//        {
//            return memValue;
//        }
//
//        return GetCommitted(tableId, key);
//    }
//
//    /// <summary>Reads committed state, bypassing the active transaction buffer.</summary>
//    private byte[]? GetCommitted(int tableId, byte[] key)
//    {
//        Memtable currentMemtable = GetOrCreateMemtable(tableId);
//        if (currentMemtable.Get(key, out var memValue)) return memValue;
//
//        if (Manifest.TryGetTable(tableId, out var tableMetadata) && tableMetadata != null)
//        {
//            foreach (var sstFileName in tableMetadata.SstFilesByLevel.Keys.OrderBy(k => k)
//                         .SelectMany(level => tableMetadata.GetFilesForLevel(level).Reverse()))
//            {
//                var reader = _sstReaderCache.GetOrAdd(Path.Combine(DatabasePath, Config.SstDirectoryName, sstFileName),
//                    path => new SstReader(path));
//                if (reader.TryFind(key, out var sstValue)) return sstValue;
//            }
//        }
//
//        return null;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>

static int db_idx_is_index_name(const char* name)
{
    return name && strncmp(name, "_idx_", 5) == 0;
}

int Database_IsMainDataTable(Database* self, int tableId)
{
    char name[128];
    if (!self || !Database_GetManifest(self)) return 0;
    if (!Manifest_TryGetTable(Database_GetManifest(self), tableId, name, (int)sizeof(name))) return 0;
    return !db_idx_is_index_name(name);
}

static int db_idx_resolve(Database* self, int mainTableId, const char* indexName, int* indexTableId)
{
    char mainName[128], idxName[256];
    if (!self || !indexName || !indexTableId) return 0;
    if (!Manifest_TryGetTable(Database_GetManifest(self), mainTableId, mainName, (int)sizeof(mainName)))
        return 0;
    Manifest_IndexTableName(mainName, indexName, idxName, (int)sizeof(idxName));
    return Manifest_TryGetTableByName(Database_GetManifest(self), idxName, indexTableId);
}

static const uint8_t* db_idx_key(const uint8_t* value, int valueLen, int keyLength, int* outLen)
{
    if (!value || valueLen <= 0) {
        *outLen = 0;
        return NULL;
    }
    if (keyLength > 0 && keyLength < valueLen) {
        *outLen = keyLength;
        return value;
    }
    *outLen = valueLen;
    return value;
}

static uint8_t* db_idx_materialize_key(Database* self, int tableId, const uint8_t* value, int valueLen,
                                       const char* keyExpression, int keyLength, int* outLen)
{
    uint8_t* extracted = NULL;
    int elen = 0;
    const uint8_t* k;
    uint8_t* copy;
    if (outLen) *outLen = 0;
    if (!self || !value || valueLen <= 0) return NULL;
    if (keyExpression && keyExpression[0] &&
        Database_EvalIndexKey(self, tableId, keyExpression, value, valueLen, &extracted, &elen)) {
        if (keyLength > 0 && keyLength < elen) elen = keyLength;
        if (outLen) *outLen = elen;
        return extracted;
    }
    if (keyExpression && keyExpression[0] &&
        Manifest_FieldCount(Database_GetManifest(self), tableId) > 0 &&
        SpeedMarshaller_ExtractField(Database_GetManifest(self), tableId, value, valueLen,
                                     keyExpression, &extracted, &elen) && extracted) {
        if (keyLength > 0 && keyLength < elen) elen = keyLength;
        if (outLen) *outLen = elen;
        return extracted;
    }
    free(extracted);
    k = db_idx_key(value, valueLen, keyLength, outLen);
    if (!k || !outLen || *outLen <= 0) return NULL;
    copy = (uint8_t*)malloc((size_t)*outLen);
    if (!copy) return NULL;
    memcpy(copy, k, (size_t)*outLen);
    return copy;
}

static void db_idx_write_entry(Database* self, int indexTableId, const uint8_t* indexKey, int indexKeyLen,
                              const uint8_t* pk, int pkLen)
{
    if (!self || !indexKey || indexKeyLen <= 0 || !pk || pkLen <= 0) return;
    Database_Set(self, indexTableId, indexKey, indexKeyLen, pk, pkLen);
}

static void db_idx_write_delete(Database* self, int indexTableId, const uint8_t* indexKey, int indexKeyLen)
{
    if (!self || !indexKey || indexKeyLen <= 0) return;
    Database_Delete(self, indexTableId, indexKey, indexKeyLen);
}

void Database_MaintainSecondaryIndexesOnPut(Database* self, int tableId,
                                            const uint8_t* pk, int pkLen,
                                            const uint8_t* oldVal, int oldLen,
                                            const uint8_t* newVal, int newLen)
{
    int n, i;
    if (!self || !Database_IsMainDataTable(self, tableId)) return;
    n = Manifest_SecondaryKeyCount(Database_GetManifest(self), tableId);
    for (i = 0; i < n; i++) {
        char iname[64], expr[64];
        int keyLength = 0, indexTableId = 0, klen = 0;
        const uint8_t* k;
        if (!Manifest_GetSecondaryKey(Database_GetManifest(self), tableId, i, iname, (int)sizeof(iname),
                                      expr, (int)sizeof(expr), &keyLength))
            continue;
        if (!db_idx_resolve(self, tableId, iname, &indexTableId)) continue;
        if (oldVal && oldLen > 0) {
            k = db_idx_materialize_key(self, tableId, oldVal, oldLen, expr, keyLength, &klen);
            if (k) { db_idx_write_delete(self, indexTableId, k, klen); free((void*)k); }
        }
        k = db_idx_materialize_key(self, tableId, newVal, newLen, expr, keyLength, &klen);
        if (k) { db_idx_write_entry(self, indexTableId, k, klen, pk, pkLen); free((void*)k); }
    }
}

void Database_MaintainSecondaryIndexesOnDelete(Database* self, int tableId,
                                               const uint8_t* pk, int pkLen,
                                               const uint8_t* oldVal, int oldLen)
{
    int n, i;
    (void)pk;
    (void)pkLen;
    if (!self || !oldVal || oldLen <= 0 || !Database_IsMainDataTable(self, tableId)) return;
    n = Manifest_SecondaryKeyCount(Database_GetManifest(self), tableId);
    for (i = 0; i < n; i++) {
        char iname[64], expr[64];
        int keyLength = 0, indexTableId = 0, klen = 0;
        const uint8_t* k;
        if (!Manifest_GetSecondaryKey(Database_GetManifest(self), tableId, i, iname, (int)sizeof(iname),
                                      expr, (int)sizeof(expr), &keyLength))
            continue;
        if (!db_idx_resolve(self, tableId, iname, &indexTableId)) continue;
        k = db_idx_materialize_key(self, tableId, oldVal, oldLen, expr, keyLength, &klen);
        if (k) { db_idx_write_delete(self, indexTableId, k, klen); free((void*)k); }
    }
}

int Database_CreateSecondaryIndex(Database* self, int mainTableId, const char* indexName,
                                  const char* keyExpression, int keyLength)
{
    char mainName[128], idxName[256];
    int existing = 0, indexTableId, keyLenDef = 0;
    DatabaseScan* scan;
    Manifest* man;
    if (!self || !indexName || !indexName[0]) return -1;
    man = Database_GetManifest(self);
    if (!man || !Manifest_TryGetTable(man, mainTableId, mainName, (int)sizeof(mainName))) return -1;
    if (db_idx_is_index_name(mainName)) return -1;
    Manifest_IndexTableName(mainName, indexName, idxName, (int)sizeof(idxName));
    if (Manifest_TryGetTableByName(man, idxName, &existing) && existing > 0) return existing;
    if (Manifest_AddSecondaryIndex(man, mainName, indexName, keyExpression, keyLength) != GMB_DB_OK)
        return -1;
    indexTableId = Database_CreateTable(self, idxName);
    if (indexTableId <= 0) return -1;
    keyLenDef = keyLength;
    scan = Database_Scan(self, mainTableId);
    if (scan) {
        uint8_t *k = NULL, *v = NULL;
        int kl = 0, vl = 0;
        while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
            int ikl = 0;
            uint8_t* ik;
            if (v && vl > 0) {
                ik = db_idx_materialize_key(self, mainTableId, v, vl, keyExpression, keyLenDef, &ikl);
                if (ik) { db_idx_write_entry(self, indexTableId, ik, ikl, k, kl); free(ik); }
            }
            free(k);
            free(v);
        }
        Database_ScanDispose(scan);
    }
    Manifest_Save(man);
    return indexTableId;
}

static int db_idx_append_err(char* msg, int cap, const char* add)
{
    size_t n;
    if (!msg || cap <= 0 || !add) return 0;
    n = strlen(msg);
    if (n == 0) {
        snprintf(msg, (size_t)cap, "%s", add);
        return 1;
    }
    snprintf(msg + n, (size_t)(cap - (int)n), "; %s", add);
    return 1;
}

static int64_t db_idx_scan_live(Database* self, int tableId)
{
    DatabaseScan* scan;
    int64_t count = 0;
    uint8_t *k = NULL, *v = NULL;
    int kl = 0, vl = 0;
    scan = Database_Scan(self, tableId);
    if (!scan) return 0;
    while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
        if (v && vl > 0) count++;
        free(k);
        free(v);
    }
    Database_ScanDispose(scan);
    return count;
}

static int db_idx_check_sst(Database* self, char* msg, int cap)
{
    Manifest* man;
    int t, n, level, nf, ok = 1;
    const char* pathRoot;
    if (!self) return 0;
    man = Database_GetManifest(self);
    pathRoot = Database_GetPath(self);
    n = Manifest_TableCount(man);
    for (t = 0; t < n; t++) {
        int id = 0;
        char tname[128];
        if (!Manifest_GetTableIdAt(man, t, &id) || id <= 0) continue;
        if (!Manifest_TryGetTable(man, id, tname, (int)sizeof(tname))) continue;
        for (level = 0; level < 8; level++) {
            char names[64][256];
            int i;
            nf = Manifest_GetFilesForLevel(man, id, level, names, 64);
            for (i = 0; i < nf; i++) {
                char fpath[1100], err[128], line[320];
                snprintf(fpath, sizeof(fpath), "%s/%s/%s", pathRoot, Config_SstDirectoryName, names[i]);
                if (access(fpath, F_OK) != 0) {
                    snprintf(line, sizeof(line), "SST mancante per tabella '%s': %s", tname, names[i]);
                    db_idx_append_err(msg, cap, line);
                    ok = 0;
                    continue;
                }
                if (!SstReader_TryValidateFooter(fpath, err, (int)sizeof(err))) {
                    snprintf(line, sizeof(line), "SST non leggibile '%s': %s", names[i], err);
                    db_idx_append_err(msg, cap, line);
                    ok = 0;
                }
            }
        }
    }
    return ok;
}

static int db_idx_check_index_manifest_counts(Database* self, char* msg, int cap)
{
    Manifest* man;
    int t, n, ok = 1;
    if (!self) return 0;
    man = Database_GetManifest(self);
    n = Manifest_TableCount(man);
    for (t = 0; t < n; t++) {
        int id = 0, secN, s;
        char tname[128];
        int64_t mainCount;
        if (!Manifest_GetTableIdAt(man, t, &id) || id <= 0) continue;
        if (!Manifest_TryGetTable(man, id, tname, (int)sizeof(tname))) continue;
        if (strncmp(tname, "_idx_", 5) == 0) continue;
        secN = Manifest_SecondaryKeyCount(man, id);
        if (secN <= 0) continue;
        if (!Manifest_IsRowCountTrusted(man, id)) continue;
        mainCount = Manifest_GetRowCount(man, id);
        for (s = 0; s < secN; s++) {
            char iname[64], idxName[256], line[320];
            int idxId = 0;
            int64_t indexCount;
            if (!Manifest_GetSecondaryKey(man, id, s, iname, (int)sizeof(iname), NULL, 0, NULL))
                continue;
            Manifest_IndexTableName(tname, iname, idxName, (int)sizeof(idxName));
            if (!Manifest_TryGetTableByName(man, idxName, &idxId) || idxId <= 0) {
                snprintf(line, sizeof(line), "Tabella-indice mancante: %s", idxName);
                db_idx_append_err(msg, cap, line);
                ok = 0;
                continue;
            }
            if (!Manifest_IsRowCountTrusted(man, idxId)) continue;
            indexCount = Manifest_GetRowCount(man, idxId);
            if (indexCount != mainCount) {
                snprintf(line, sizeof(line),
                         "Conteggio manifest indice '%s' (%lld) != main '%s' (%lld)",
                         iname, (long long)indexCount, tname, (long long)mainCount);
                db_idx_append_err(msg, cap, line);
                ok = 0;
            }
        }
    }
    return ok;
}

static int db_idx_check_index_scan_counts(Database* self, int enforceAll, char* msg, int cap)
{
    Manifest* man;
    int t, n, ok = 1;
    if (!self) return 0;
    man = Database_GetManifest(self);
    n = Manifest_TableCount(man);
    for (t = 0; t < n; t++) {
        int id = 0, secN, s;
        char tname[128];
        int64_t mainCount;
        if (!Manifest_GetTableIdAt(man, t, &id) || id <= 0) continue;
        if (!Manifest_TryGetTable(man, id, tname, (int)sizeof(tname))) continue;
        if (strncmp(tname, "_idx_", 5) == 0) continue;
        secN = Manifest_SecondaryKeyCount(man, id);
        if (secN <= 0) continue;
        mainCount = db_idx_scan_live(self, id);
        if (!enforceAll && mainCount > 10000) continue;
        for (s = 0; s < secN; s++) {
            char iname[64], idxName[256], line[320];
            int idxId = 0;
            int64_t indexCount;
            if (!Manifest_GetSecondaryKey(man, id, s, iname, (int)sizeof(iname), NULL, 0, NULL))
                continue;
            Manifest_IndexTableName(tname, iname, idxName, (int)sizeof(idxName));
            if (!Manifest_TryGetTableByName(man, idxName, &idxId) || idxId <= 0) {
                snprintf(line, sizeof(line), "Tabella-indice mancante: %s", idxName);
                db_idx_append_err(msg, cap, line);
                ok = 0;
                continue;
            }
            indexCount = db_idx_scan_live(self, idxId);
            if (indexCount != mainCount) {
                snprintf(line, sizeof(line),
                         "Conteggio indice '%s' (%lld) != main '%s' (%lld)",
                         iname, (long long)indexCount, tname, (long long)mainCount);
                db_idx_append_err(msg, cap, line);
                ok = 0;
            }
        }
    }
    return ok;
}

int Database_ValidateIntegrityQuick(Database* self, char* message, int messageCap)
{
    int ok;
    if (message && messageCap > 0) message[0] = 0;
    if (!self) {
        if (message && messageCap) snprintf(message, (size_t)messageCap, "null");
        return 0;
    }
    ok = db_idx_check_sst(self, message, messageCap);
    if (!db_idx_check_index_manifest_counts(self, message, messageCap)) ok = 0;
    if (ok && message && messageCap) snprintf(message, (size_t)messageCap, "OK");
    return ok;
}

int Database_ValidateIntegrity(Database* self, char* message, int messageCap)
{
    int ok;
    if (message && messageCap > 0) message[0] = 0;
    if (!self) {
        if (message && messageCap) snprintf(message, (size_t)messageCap, "null");
        return 0;
    }
    ok = db_idx_check_sst(self, message, messageCap);
    if (!db_idx_check_index_scan_counts(self, 0, message, messageCap)) ok = 0;
    if (ok && message && messageCap) snprintf(message, (size_t)messageCap, "OK");
    return ok;
}

int Database_ValidateIntegrityFull(Database* self, char* message, int messageCap)
{
    if (!Database_ValidateIntegrityQuick(self, message, messageCap)) return 0;
    if (message && messageCap > 0) message[0] = 0;
    if (!db_idx_check_index_scan_counts(self, 1, message, messageCap)) return 0;
    if (message && messageCap) snprintf(message, (size_t)messageCap, "OK");
    return 1;
}

int Database_Indexes_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/speedidx_XXXXXX";
    Database* db;
    int tableId, idxId, vlen, found, nsec;
    uint8_t *val;
    char iname[64];
    if (!mkdtemp(dir)) return -60;
    db = Database_Open(dir);
    if (!db) return -61;
    tableId = Database_CreateTable(db, "Person");
    if (tableId <= 0) { Database_Dispose(db); return -62; }
    if (Database_Set(db, tableId, (const uint8_t*)"k001", 4, (const uint8_t*)"alice", 5) != GMB_DB_OK) {
        Database_Dispose(db); return -63;
    }
    if (Database_Set(db, tableId, (const uint8_t*)"k002", 4, (const uint8_t*)"bobxx", 5) != GMB_DB_OK) {
        Database_Dispose(db); return -63;
    }
    idxId = Database_CreateSecondaryIndex(db, tableId, "ByName", "Name", 0);
    if (idxId <= 0) { Database_Dispose(db); return -64; }
    found = Database_Get(db, idxId, (const uint8_t*)"alice", 5, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "k001", 4) != 0) {
        free(val); Database_Dispose(db); return -65;
    }
    free(val);
    if (Database_Set(db, tableId, (const uint8_t*)"k001", 4, (const uint8_t*)"carol", 5) != GMB_DB_OK) {
        Database_Dispose(db); return -66;
    }
    if (Database_Get(db, idxId, (const uint8_t*)"alice", 5, &val, &vlen)) {
        free(val); Database_Dispose(db); return -67;
    }
    found = Database_Get(db, idxId, (const uint8_t*)"carol", 5, &val, &vlen);
    if (!found || memcmp(val, "k001", 4) != 0) { free(val); Database_Dispose(db); return -68; }
    free(val);
    if (Database_Delete(db, tableId, (const uint8_t*)"k002", 4) != GMB_DB_OK) {
        Database_Dispose(db); return -69;
    }
    if (Database_Get(db, idxId, (const uint8_t*)"bobxx", 5, &val, &vlen)) {
        free(val); Database_Dispose(db); return -70;
    }
    nsec = Manifest_SecondaryKeyCount(Database_GetManifest(db), tableId);
    if (nsec != 1 || !Manifest_GetSecondaryKey(Database_GetManifest(db), tableId, 0, iname, (int)sizeof(iname),
                                               NULL, 0, NULL) || strcmp(iname, "ByName") != 0) {
        Database_Dispose(db); return -71;
    }
    Database_Dispose(db);
    db = Database_Open(dir);
    if (!db) return -72;
    if (!Manifest_TryGetTableByName(Database_GetManifest(db), "_idx_Person_ByName", &idxId) || idxId <= 0) {
        Database_Dispose(db); return -73;
    }
    found = Database_Get(db, idxId, (const uint8_t*)"carol", 5, &val, &vlen);
    if (!found || memcmp(val, "k001", 4) != 0) { free(val); Database_Dispose(db); return -74; }
    free(val);
    Database_RebuildManifestRowCounts(db);
    if (Manifest_GetRowCount(Database_GetManifest(db), tableId) != 1) {
        Database_Dispose(db); return -75;
    }
    if (Database_DropIndex(db, tableId, "ByName") != GMB_DB_OK) {
        Database_Dispose(db); return -76;
    }
    if (Manifest_SecondaryKeyCount(Database_GetManifest(db), tableId) != 0) {
        Database_Dispose(db); return -77;
    }
    if (Manifest_TryGetTableByName(Database_GetManifest(db), "_idx_Person_ByName", &idxId)) {
        Database_Dispose(db); return -78;
    }
    Database_SetIndexKeyEvaluator(db, tableId, SpeedExpressionEvaluator_Dummy, NULL);
    {
        char vmsg[256];
        if (!Database_ValidateIntegrityQuick(db, vmsg, (int)sizeof(vmsg))) {
            Database_Dispose(db); return -79;
        }
    }
    Database_Dispose(db);
    return GMB_DB_OK;
}
