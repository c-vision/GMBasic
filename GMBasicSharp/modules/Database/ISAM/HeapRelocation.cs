// FILE: Basic.ISAM.HeapRelocation

// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.ISAM;

/// <summary>
/// Aggiorna puntatori assoluti dopo lo spostamento dell'area heap (LOB + pagine B+).
/// </summary>
internal static class HeapRelocation
{
    internal const int PageSize = 4096;

    internal static long AlignUp(long value, int alignment) =>
        (value + alignment - 1) / alignment * alignment;

    /// <summary>
    /// Calcola l'offset minimo consentito per l'heap oltre la griglia record allocata.
    /// </summary>
    internal static long MinimumHeapStart(IsamFileHandle handle)
    {
        var header = handle.Header;
        var dataEnd = handle.DataAreaStartOffset + header.LastRecNumber * header.RecordSize;
        return AlignUp(dataEnd, PageSize);
    }

    /// <summary>
    /// Restituisce gli offset (relativi all'inizio record) dei campi JsonType nello schema.
    /// </summary>
    internal static int[] GetJsonFieldOffsets(Config.IsamSchema schema)
    {
        var metadataSize = System.Runtime.InteropServices.Marshal.SizeOf<RecordMetadata>();
        var offsets = new List<int>();
        var cursor = metadataSize;
        foreach (var field in schema.Fields)
        {
            if (field.DataType is JsonType)
            {
                offsets.Add(cursor);
            }

            cursor += SchemaManager.GetFieldStorageSize(field.DataType, field.Length);
        }

        return offsets.ToArray();
    }
}
