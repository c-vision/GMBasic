// FILE: Basic.Databases.Speed.Database.Indexes
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.Databases.Speed;

public partial class Database
{
    /// <summary>
    /// Creates a secondary index table, registers it in the main table schema, and populates it.
    /// </summary>
    /// <param name="mainTableId">Main table identifier.</param>
    /// <param name="indexDef">Secondary index definition.</param>
    /// <param name="keyEvaluator">Optional evaluator for complex key expressions.</param>
    /// <returns>Metadata for the new index table.</returns>
    public TableMetadata CreateSecondaryIndex(int mainTableId, Config.SpeedIndexDefinition indexDef,
        ExpressionEvaluator? keyEvaluator = null)
    {
        ArgumentNullException.ThrowIfNull(indexDef);
        if (!Manifest.TryGetTable(mainTableId, out var mainMeta) || mainMeta is null)
            throw new InvalidOperationException($"Tabella con ID {mainTableId} non trovata.");

        if (keyEvaluator is not null)
            _indexKeyEvaluators[mainTableId] = keyEvaluator;

        var indexTableName = GetIndexTableName(mainMeta.Name, indexDef.Name);
        if (Manifest.TryGetTable(indexTableName, out var existing) && existing is not null)
            return existing;

        var indexSchema = BuildIndexTableSchema(mainMeta.Name, indexDef);
        lock (_lock)
        {
            Manifest.AddSecondaryIndex(mainMeta.Name, indexDef);
            var indexMeta = Manifest.CreateTable(indexSchema);
            _tableLocks.TryAdd(indexMeta.Id, new object());
            PopulateIndexTable(mainTableId, indexMeta.Id, indexDef);
            Manifest.Save();
            return indexMeta;
        }
    }

    /// <summary>
    /// Validates manifest readability and SST file presence; optionally compares index row counts.
    /// ponytail: index row count vs main table is O(n) scan — skipped when main table exceeds 10k rows.
    /// </summary>
    /// <returns>Validation outcome.</returns>
    public (bool IsValid, string Message) ValidateIntegrity() =>
        ValidateSecondaryIndexCounts(enforceAllTableSizes: false);

    /// <summary>
    /// Offline/ops integrity: SST footers + secondary index row counts without the 10k cap.
    /// ponytail: O(n) per table — CLI <c>validate --full</c> only; runtime uses <see cref="ValidateIntegrityQuick"/>.
    /// </summary>
    /// <returns>Validation outcome.</returns>
    public (bool IsValid, string Message) ValidateIntegrityFull()
    {
        var quick = ValidateIntegrityQuick();
        if (!quick.IsValid)
        {
            return quick;
        }

        return ValidateSecondaryIndexCounts(enforceAllTableSizes: true);
    }

    private (bool IsValid, string Message) ValidateSecondaryIndexCounts(bool enforceAllTableSizes)
    {
        var errors = new List<string>();
        try
        {
            foreach (var table in Manifest.GetAllTables())
            {
                foreach (var level in table.SstFilesByLevel.Keys.OrderBy(k => k))
                {
                    foreach (var fileName in table.GetFilesForLevel(level))
                    {
                        var path = Path.Combine(DatabasePath, Config.SstDirectoryName, fileName);
                        if (!File.Exists(path))
                        {
                            errors.Add($"SST mancante per tabella '{table.Name}': {fileName}");
                        }
                    }
                }
            }

            foreach (var table in Manifest.GetAllTables().Where(t => !IsIndexTableName(t.Name)))
            {
                if (table.Schema.SecondaryKeys.Count == 0) continue;
                var mainCount = Scan(table.Id).Count(kvp => kvp.Value is not null);
                if (!enforceAllTableSizes && mainCount > 10_000) continue;

                foreach (var indexDef in table.Schema.SecondaryKeys)
                {
                    var indexTableName = GetIndexTableName(table.Name, indexDef.Name);
                    if (!Manifest.TryGetTable(indexTableName, out var indexMeta) || indexMeta is null)
                    {
                        errors.Add($"Tabella-indice mancante: {indexTableName}");
                        continue;
                    }

                    var indexCount = Scan(indexMeta.Id).Count(kvp => kvp.Value is not null);
                    if (indexCount != mainCount)
                    {
                        errors.Add(
                            $"Conteggio indice '{indexDef.Name}' ({indexCount}) != main '{table.Name}' ({mainCount})");
                    }
                }
            }
        }
        catch (Exception ex)
        {
            return (false, ex.Message);
        }

        return errors.Count == 0 ? (true, "OK") : (false, string.Join("; ", errors));
    }

    /// <summary>
    /// Fast integrity check: manifest SST presence + readable SST footers + O(1) manifest row-count sanity when trusted.
    /// ponytail: no O(n) scan — use <see cref="ValidateIntegrityFull"/> for full index parity offline.
    /// </summary>
    /// <returns>Validation outcome.</returns>
    public (bool IsValid, string Message) ValidateIntegrityQuick()
    {
        var errors = new List<string>();
        try
        {
            foreach (var table in Manifest.GetAllTables())
            {
                foreach (var level in table.SstFilesByLevel.Keys.OrderBy(k => k))
                {
                    foreach (var fileName in table.GetFilesForLevel(level))
                    {
                        var path = Path.Combine(DatabasePath, Config.SstDirectoryName, fileName);
                        if (!File.Exists(path))
                        {
                            errors.Add($"SST mancante per tabella '{table.Name}': {fileName}");
                            continue;
                        }

                        if (!SstReader.TryValidateFooter(path, out var footerError))
                        {
                            errors.Add($"SST non leggibile '{fileName}': {footerError}");
                        }
                    }
                }
            }

            foreach (var table in Manifest.GetAllTables().Where(t => !IsIndexTableName(t.Name)))
            {
                if (table.Schema.SecondaryKeys.Count == 0) continue;
                if (!Manifest.IsRowCountTrusted(table.Id)) continue;

                var mainCount = Manifest.GetRowCount(table.Id);
                foreach (var indexDef in table.Schema.SecondaryKeys)
                {
                    var indexTableName = GetIndexTableName(table.Name, indexDef.Name);
                    if (!Manifest.TryGetTable(indexTableName, out var indexMeta) || indexMeta is null)
                    {
                        errors.Add($"Tabella-indice mancante: {indexTableName}");
                        continue;
                    }

                    if (!Manifest.IsRowCountTrusted(indexMeta.Id)) continue;

                    var indexCount = Manifest.GetRowCount(indexMeta.Id);
                    if (indexCount != mainCount)
                    {
                        errors.Add(
                            $"Conteggio manifest indice '{indexDef.Name}' ({indexCount}) != main '{table.Name}' ({mainCount})");
                    }
                }
            }
        }
        catch (Exception ex)
        {
            return (false, ex.Message);
        }

        return errors.Count == 0 ? (true, "OK") : (false, string.Join("; ", errors));
    }

    private void EnsureSecondaryIndexTables(TableMetadata mainMeta)
    {
        if (mainMeta.Schema.SecondaryKeys.Count == 0) return;

        lock (_lock)
        {
            foreach (var indexDef in mainMeta.Schema.SecondaryKeys)
            {
                var indexTableName = GetIndexTableName(mainMeta.Name, indexDef.Name);
                if (Manifest.TryGetTable(indexTableName, out _))
                    continue;

                var indexSchema = BuildIndexTableSchema(mainMeta.Name, indexDef);
                var indexMeta = Manifest.CreateTable(indexSchema);
                _tableLocks.TryAdd(indexMeta.Id, new object());
                PopulateIndexTable(mainMeta.Id, indexMeta.Id, indexDef);
            }

            Manifest.Save();
        }
    }

    private void PopulateIndexTable(int mainTableId, int indexTableId, Config.SpeedIndexDefinition indexDef)
    {
        if (!Manifest.TryGetTable(mainTableId, out var mainMeta) || mainMeta is null) return;

        var count = 0L;
        foreach (var record in Scan(mainTableId))
        {
            if (record.Value is null) continue;
            var row = DeserializeRecord(record.Value, mainMeta.Schema);
            var indexKey = ComputeIndexKeyBytes(mainTableId, indexDef, row, mainMeta.Schema);
            if (indexKey is null) continue;
            WriteIndexEntry(indexTableId, indexKey, record.Key);
            count++;
        }

        Manifest.SetRowCount(indexTableId, count);
    }

    private void MaintainSecondaryIndexesOnPut(int mainTableId, byte[] pk, byte[]? oldRecordBytes, byte[] newRecordBytes)
    {
        if (!Manifest.TryGetTable(mainTableId, out var mainMeta) || mainMeta is null) return;
        if (mainMeta.Schema.SecondaryKeys.Count == 0) return;

        var newRow = DeserializeRecord(newRecordBytes, mainMeta.Schema);
        Dictionary<string, object?>? oldRow = oldRecordBytes is not null
            ? DeserializeRecord(oldRecordBytes, mainMeta.Schema)
            : null;

        foreach (var indexDef in mainMeta.Schema.SecondaryKeys)
        {
            if (!TryResolveIndexTableId(mainMeta.Name, indexDef.Name, out var indexTableId)) continue;

            if (oldRow is not null)
            {
                var oldIndexKey = ComputeIndexKeyBytes(mainTableId, indexDef, oldRow, mainMeta.Schema);
                if (oldIndexKey is not null)
                    WriteIndexDelete(indexTableId, oldIndexKey);
            }

            var newIndexKey = ComputeIndexKeyBytes(mainTableId, indexDef, newRow, mainMeta.Schema);
            if (newIndexKey is not null)
                WriteIndexEntry(indexTableId, newIndexKey, pk);
        }
    }

    private void MaintainSecondaryIndexesOnDelete(int mainTableId, byte[] pk, byte[]? oldRecordBytes)
    {
        if (oldRecordBytes is null) return;
        if (!Manifest.TryGetTable(mainTableId, out var mainMeta) || mainMeta is null) return;

        var oldRow = DeserializeRecord(oldRecordBytes, mainMeta.Schema);
        foreach (var indexDef in mainMeta.Schema.SecondaryKeys)
        {
            if (!TryResolveIndexTableId(mainMeta.Name, indexDef.Name, out var indexTableId)) continue;
            var oldIndexKey = ComputeIndexKeyBytes(mainTableId, indexDef, oldRow, mainMeta.Schema);
            if (oldIndexKey is not null)
                WriteIndexDelete(indexTableId, oldIndexKey);
        }
    }

    private void WriteIndexEntry(int indexTableId, byte[] indexKey, byte[] primaryKey)
    {
        AdjustRowCountOnSet(indexTableId, indexKey);
        var wal = GetOrCreateWal(indexTableId);
        wal.LogSet(indexTableId, indexKey, primaryKey);
        MaybeFlushWal(wal);
        GetOrCreateMemtable(indexTableId).Set(indexKey, primaryKey);
    }

    private void WriteIndexDelete(int indexTableId, byte[] indexKey)
    {
        AdjustRowCountOnDelete(indexTableId, indexKey);
        var wal = GetOrCreateWal(indexTableId);
        wal.LogDelete(indexTableId, indexKey);
        MaybeFlushWal(wal);
        GetOrCreateMemtable(indexTableId).Delete(indexKey);
    }

    private byte[]? ComputeIndexKeyBytes(int mainTableId, Config.SpeedIndexDefinition indexDef,
        Dictionary<string, object?> row, Config.SpeedSchema schema)
    {
        object? keyObject;
        if (_indexKeyEvaluators.TryGetValue(mainTableId, out var evaluator))
        {
            keyObject = evaluator(indexDef.KeyExpression, row);
        }
        else if (!TryGetFieldValue(row, indexDef.KeyExpression, out keyObject) &&
                 !TryGetFieldValue(row, indexDef.Name, out keyObject))
        {
            // ponytail: complex expressions require SetIndexKeyEvaluator from GPM
            return null;
        }

        return SpeedMarshaller.ToBytes(keyObject, indexDef.DataType, schema);
    }

    private static bool TryGetFieldValue(Dictionary<string, object?> row, string fieldName, out object? value) =>
        row.TryGetValue(fieldName, out value);

    private static Dictionary<string, object?> DeserializeRecord(byte[] recordBytes, Config.SpeedSchema schema) =>
        (Dictionary<string, object?>)SpeedMarshaller.FromBytes(recordBytes, schema);

    private static Config.SpeedSchema BuildIndexTableSchema(string mainTableName, Config.SpeedIndexDefinition indexDef)
    {
        return new Config.SpeedSchema(
            GetIndexTableName(mainTableName, indexDef.Name),
            [new Config.SpeedFieldSchema("PK", indexDef.DataType, indexDef.KeyLength)],
            new Config.SpeedIndexDefinition(
                indexDef.Name,
                indexDef.KeyExpression,
                indexDef.DataType,
                indexDef.KeyLength,
                true,
                indexDef.IsUnsafe),
            []);
    }

    private static string GetIndexTableName(string mainTableName, string indexName) =>
        $"_idx_{mainTableName}_{indexName}";

    private bool TryResolveIndexTableId(string mainTableName, string indexName, out int indexTableId)
    {
        indexTableId = -1;
        var indexTableName = GetIndexTableName(mainTableName, indexName);
        if (!Manifest.TryGetTable(indexTableName, out var indexMeta) || indexMeta is null)
            return false;
        indexTableId = indexMeta.Id;
        return true;
    }

    private bool IsMainDataTable(int tableId)
    {
        if (!Manifest.TryGetTable(tableId, out var meta) || meta is null) return false;
        return !IsIndexTableName(meta.Name);
    }

    private static bool IsIndexTableName(string tableName) =>
        tableName.StartsWith("_idx_", StringComparison.Ordinal);

    private bool HasSecondaryIndexes(int tableId)
    {
        if (!Manifest.TryGetTable(tableId, out var meta) || meta is null)
        {
            return false;
        }

        return meta.Schema.SecondaryKeys.Count > 0;
    }

    /// <summary>
    /// Recupera il valore precedente per manutenzione indici: memtable prima, SST solo se assente in memoria.
    /// ponytail: insert puro senza chiave in memtable evita scan SST (oldValue null).
    /// </summary>
    private byte[]? TryGetOldValueForIndex(int tableId, byte[] key)
    {
        var memtable = GetOrCreateMemtable(tableId);
        if (memtable.Get(key, out var memValue))
        {
            return memValue;
        }

        return GetCommitted(tableId, key);
    }

    /// <summary>Reads committed state, bypassing the active transaction buffer.</summary>
    private byte[]? GetCommitted(int tableId, byte[] key)
    {
        Memtable currentMemtable = GetOrCreateMemtable(tableId);
        if (currentMemtable.Get(key, out var memValue)) return memValue;

        if (Manifest.TryGetTable(tableId, out var tableMetadata) && tableMetadata != null)
        {
            foreach (var sstFileName in tableMetadata.SstFilesByLevel.Keys.OrderBy(k => k)
                         .SelectMany(level => tableMetadata.GetFilesForLevel(level).Reverse()))
            {
                var reader = _sstReaderCache.GetOrAdd(Path.Combine(DatabasePath, Config.SstDirectoryName, sstFileName),
                    path => new SstReader(path));
                if (reader.TryFind(key, out var sstValue)) return sstValue;
            }
        }

        return null;
    }
}
