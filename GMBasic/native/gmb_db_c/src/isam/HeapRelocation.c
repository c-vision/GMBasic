/* FILE: native/gmb_db_c/src/ISAM/HeapRelocation.c */
/* Port 1:1 di modules/Database/ISAM/HeapRelocation.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.HeapRelocation
//
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.ISAM;
//
///// <summary>
///// Aggiorna puntatori assoluti dopo lo spostamento dell'area heap (LOB + pagine B+).
///// </summary>
//internal static class HeapRelocation
//{
//    internal const int PageSize = 4096;
//
//    internal static long AlignUp(long value, int alignment) =>
//        (value + alignment - 1) / alignment * alignment;
//
//    /// <summary>
//    /// Calcola l'offset minimo consentito per l'heap oltre la griglia record allocata.
//    /// </summary>
//    internal static long MinimumHeapStart(IsamFileHandle handle)
//    {
//        var header = handle.Header;
//        var dataEnd = handle.DataAreaStartOffset + header.LastRecNumber * header.RecordSize;
//        return AlignUp(dataEnd, PageSize);
//    }
//
//    /// <summary>
//    /// Restituisce gli offset (relativi all'inizio record) dei campi JsonType nello schema.
//    /// </summary>
//    internal static int[] GetJsonFieldOffsets(Config.IsamSchema schema)
//    {
//        var metadataSize = System.Runtime.InteropServices.Marshal.SizeOf<RecordMetadata>();
//        var offsets = new List<int>();
//        var cursor = metadataSize;
//        foreach (var field in schema.Fields)
//        {
//            if (field.DataType is JsonType)
//            {
//                offsets.Add(cursor);
//            }
//
//            cursor += SchemaManager.GetFieldStorageSize(field.DataType, field.Length);
//        }
//
//        return offsets.ToArray();
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <pthread.h>
#include <time.h>
int64_t HeapRelocation_AlignUp(int64_t value, int alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}


/* Port di HeapRelocation.GetJsonFieldOffsets: offset (relativi all'inizio record) dei
 * campi JsonType, calcolati con lo stesso layout del marshaller (meta + campi). */
int HeapRelocation_GetJsonFieldOffsets(const IsamSchema* schema, int64_t* out, int cap)
{
    int cursor = RecordMetadata_Size;
    int count = 0;
    int i;
    if (!schema || !out) return 0;
    for (i = 0; i < schema->FieldCount && count < cap; i++) {
        if (strcmp(schema->Fields[i].DataTypeCode, "JsonType") == 0)
            out[count++] = cursor;
        cursor += SchemaManager_GetFieldSize(schema->Fields[i].DataTypeCode, schema->Fields[i].Length);
    }
    return count;
}

