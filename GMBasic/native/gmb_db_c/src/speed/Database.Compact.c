/* FILE: native/gmb_db_c/src/SPEED/Database.Compact.c */
/* Port 1:1 di modules/Database/SPEED/Database.Compact.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Database.Compact
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases.Speed;
//
//public partial class Database
//{
//    /// <summary>
//    /// Synchronously flushes the active memtable for a table to a new L0 SST.
//    /// ponytail: offline compact / ops — bypasses size threshold; no background compaction chain.
//    /// </summary>
//    /// <param name="tableId">Table identifier.</param>
//    public void ForceFlushTableSync(int tableId)
//    {
//        Memtable? toFlush;
//        WriteAheadLog? oldWal;
//
//        lock (_lock)
//        {
//            if (!_activeMemtables.TryGetValue(tableId, out var memtable) || memtable.KeyCount == 0)
//                return;
//
//            toFlush = memtable;
//            _activeMemtables[tableId] = new Memtable();
//            oldWal = _activeWals.GetValueOrDefault(tableId);
//            _activeWals.Remove(tableId);
//        }
//
//        var tableLock = _tableLocks.GetOrAdd(tableId, _ => new object());
//        lock (tableLock)
//        {
//            var fileNumber = Manifest.GetNextFileNumber();
//            var sstFileName = $"{fileNumber:D6}_{tableId}.sst";
//            var sstPath = Path.Combine(DatabasePath, Config.SstDirectoryName, sstFileName);
//            SstWriter.WriteFromMemtable(sstPath, toFlush);
//            Manifest.AddSstFile(tableId, sstFileName);
//            Manifest.Save();
//            oldWal?.Dispose();
//            if (oldWal is not null)
//            {
//                var walPath = oldWal.GetWalPath();
//                if (File.Exists(walPath))
//                    File.Delete(walPath);
//            }
//        }
//    }
//
//    /// <summary>
//    /// Offline compaction for one main table and its secondary index tables.
//    /// ponytail: L0+L1 merge only — deeper levels unchanged; upgrade path: full LSM major compact.
//    /// </summary>
//    /// <param name="tableName">Main table name.</param>
//    /// <returns>Net SST files removed across main + index tables.</returns>
//    public int ForceCompactTable(string tableName)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(tableName);
//        if (!Manifest.TryGetTable(tableName, out var table) || table is null)
//            throw new InvalidOperationException($"Table '{tableName}' not found.");
//
//        var tableIds = new List<int> { table.Id };
//        foreach (var indexDef in table.Schema.SecondaryKeys)
//        {
//            var indexTableName = QueryOptimizer.GetIndexTableName(table.Name, indexDef.Name);
//            if (Manifest.TryGetTable(indexTableName, out var indexMeta) && indexMeta is not null)
//                tableIds.Add(indexMeta.Id);
//        }
//
//        var totalReduced = 0;
//        foreach (var tableId in tableIds)
//        {
//            if (_compactionTasks.TryGetValue(tableId, out var pending))
//                pending.Wait(TimeSpan.FromSeconds(30));
//
//            ForceFlushTableSync(tableId);
//            totalReduced += Compactor.CompactLevels01Forced(this, tableId, _sstReaderCache);
//        }
//
//        Manifest.Save();
//        return totalReduced;
//    }
//
//    private void MaybeRebuildUntrustedRowCounts()
//    {
//        var needsRebuild = Manifest.GetAllTables().Any(t => !Manifest.IsRowCountTrusted(t.Id));
//        if (!needsRebuild)
//            return;
//
//        RebuildManifestRowCounts();
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C# Database.ForceFlushTableSync / ForceCompactTable / MaybeRebuildUntrustedRowCounts
 * vivono in Database.c (Database_ForceFlushTableSync, Database_ForceCompactTable). */
