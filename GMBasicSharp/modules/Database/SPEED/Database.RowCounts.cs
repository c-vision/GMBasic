// FILE: Basic.Databases.Speed.Database.RowCounts
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.Databases.Speed;

public partial class Database
{
    /// <summary>
    /// Rebuilds manifest row counts from a full table scan. Offline/ops only — O(n) per table.
    /// ponytail: legacy DBs with untrusted counts; call after migration or before relying on O(1) validate.
    /// </summary>
    public void RebuildManifestRowCounts()
    {
        lock (_lock)
        {
            foreach (var table in Manifest.GetAllTables())
            {
                var count = Scan(table.Id).Count(kvp => kvp.Value is not null);
                Manifest.SetRowCount(table.Id, count);
            }

            Manifest.Save();
        }
    }

    private void AdjustRowCountOnSet(int tableId, byte[] key)
    {
        if (Get(tableId, key) is null)
        {
            Manifest.AdjustRowCount(tableId, 1);
        }
    }

    private void AdjustRowCountOnDelete(int tableId, byte[] key)
    {
        if (Get(tableId, key) is not null)
        {
            Manifest.AdjustRowCount(tableId, -1);
        }
    }
}
