/* FILE: native/gmb_db_c/src/SPEED/Database.RowCounts.c */
/* Port 1:1 di modules/Database/SPEED/Database.RowCounts.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Database.RowCounts
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases.Speed;
//
//public partial class Database
//{
//    /// <summary>
//    /// Rebuilds manifest row counts from a full table scan. Offline/ops only — O(n) per table.
//    /// ponytail: legacy DBs with untrusted counts; call after migration or before relying on O(1) validate.
//    /// </summary>
//    public void RebuildManifestRowCounts()
//    {
//        lock (_lock)
//        {
//            foreach (var table in Manifest.GetAllTables())
//            {
//                var count = Scan(table.Id).Count(kvp => kvp.Value is not null);
//                Manifest.SetRowCount(table.Id, count);
//            }
//
//            Manifest.Save();
//        }
//    }
//
//    private void AdjustRowCountOnSet(int tableId, byte[] key)
//    {
//        if (Get(tableId, key) is null)
//        {
//            Manifest.AdjustRowCount(tableId, 1);
//        }
//    }
//
//    private void AdjustRowCountOnDelete(int tableId, byte[] key)
//    {
//        if (Get(tableId, key) is not null)
//        {
//            Manifest.AdjustRowCount(tableId, -1);
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione) --- */

void Database_RebuildManifestRowCounts(Database* self)
{
    Manifest* man;
    int i, n, id = 0;
    if (!self) return;
    man = Database_GetManifest(self);
    if (!man) return;
    n = Manifest_TableCount(man);
    for (i = 0; i < n; i++) {
        DatabaseScan* scan;
        int64_t count = 0;
        uint8_t *k = NULL, *v = NULL;
        int kl = 0, vl = 0;
        if (!Manifest_GetTableIdAt(man, i, &id) || id <= 0) continue;
        scan = Database_Scan(self, id);
        if (scan) {
            while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
                if (v && vl > 0) count++;
                free(k);
                free(v);
                k = NULL;
                v = NULL;
            }
            Database_ScanDispose(scan);
        }
        Manifest_SetRowCount(man, id, count);
    }
    Manifest_Save(man);
}

void Database_AdjustRowCountOnSet(Database* self, int tableId, const uint8_t* key, int keyLen)
{
    uint8_t* val = NULL;
    int vlen = 0;
    Manifest* man;
    if (!self || !key) return;
    man = Database_GetManifest(self);
    if (!man) return;
    if (!Database_Get(self, tableId, key, keyLen, &val, &vlen))
        Manifest_AdjustRowCount(man, tableId, 1);
    free(val);
}

void Database_AdjustRowCountOnDelete(Database* self, int tableId, const uint8_t* key, int keyLen)
{
    uint8_t* val = NULL;
    int vlen = 0;
    Manifest* man;
    if (!self || !key) return;
    man = Database_GetManifest(self);
    if (!man) return;
    if (Database_Get(self, tableId, key, keyLen, &val, &vlen))
        Manifest_AdjustRowCount(man, tableId, -1);
    free(val);
}
