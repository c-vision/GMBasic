/* FILE: native/gmb_db_c/src/ISAM/MvccUndoGc.c */
/* Port 1:1 di modules/Database/ISAM/MvccUndoGc.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.MvccUndoGc
//
//using Basic.ISAM;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Policy GC catene undo MVCC: tronca profondità e recupera slot su rollback.
///// ponytail: reclaim heap completo resta su PACK (file nuovo senza orfani heap).
///// </summary>
//internal static class MvccUndoGc
//{
//    /// <summary>Profondità massima catena undo dopo commit/auto-commit.</summary>
//    internal const int MaxRetainedChainDepth = 1;
//
//    /// <summary>
//    /// Dopo scrittura su offset griglia, tronca la catena undo oltre <see cref="MaxRetainedChainDepth"/>.
//    /// </summary>
//    internal static void PruneAfterGridWrite(IsamFileHandle handle, long gridOffset)
//    {
//        var meta = handle.ReadRecordMetadata(gridOffset, handle.FileEncoding);
//        PruneChainTail(handle, meta.PreviousVersionOffset);
//    }
//
//    /// <summary>
//    /// Segna slot undo transazionali orfani dopo rollback (spazio recuperato da PACK).
//    /// </summary>
//    internal static void ReclaimTransactionalUndoSlots(IsamFileHandle handle, IEnumerable<long> undoOffsets)
//    {
//        foreach (var offset in undoOffsets)
//        {
//            if (offset < 0)
//            {
//                continue;
//            }
//
//            var meta = handle.ReadRecordMetadata(offset, handle.FileEncoding);
//            meta.Status = Config.RecordStatusDeleted;
//            meta.PreviousVersionOffset = -1;
//            handle.WriteRecordMetadata(offset, meta);
//        }
//    }
//
//    internal static int MeasureChainDepth(IsamFileHandle handle, long startOffset)
//    {
//        var depth = 0;
//        var offset = startOffset;
//        while (offset != -1)
//        {
//            depth++;
//            var meta = handle.ReadRecordMetadata(offset, handle.FileEncoding);
//            offset = meta.PreviousVersionOffset;
//        }
//
//        return depth;
//    }
//
//    private static void PruneChainTail(IsamFileHandle handle, long headOffset)
//    {
//        if (headOffset < 0)
//        {
//            return;
//        }
//
//        var depth = 0;
//        var offset = headOffset;
//        long lastKept = -1;
//
//        while (offset != -1)
//        {
//            depth++;
//            if (depth > MaxRetainedChainDepth && lastKept >= 0)
//            {
//                var keptMeta = handle.ReadRecordMetadata(lastKept, handle.FileEncoding);
//                keptMeta.PreviousVersionOffset = -1;
//                handle.WriteRecordMetadata(lastKept, keptMeta);
//                break;
//            }
//
//            lastKept = offset;
//            var meta = handle.ReadRecordMetadata(offset, handle.FileEncoding);
//            offset = meta.PreviousVersionOffset;
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

static void MvccUndoGc_PruneChainTail(IsamFileHandle* handle, int64_t headOffset)
{
    int depth = 0;
    int64_t offset = headOffset;
    int64_t lastKept = -1;
    if (headOffset < 0) return;
    while (offset != -1) {
        RecordMetadata meta;
        depth++;
        if (depth > MvccUndoGc_MaxRetainedChainDepth && lastKept >= 0) {
            RecordMetadata kept;
            if (IsamFileHandle_ReadRecordMetadata(handle, lastKept, &kept) != GMB_DB_OK) return;
            kept.PreviousVersionOffset = -1;
            IsamFileHandle_WriteRecordMetadata(handle, lastKept, &kept);
            break;
        }
        lastKept = offset;
        if (IsamFileHandle_ReadRecordMetadata(handle, offset, &meta) != GMB_DB_OK) return;
        offset = meta.PreviousVersionOffset;
    }
}

void MvccUndoGc_PruneAfterGridWrite(IsamFileHandle* handle, int64_t gridOffset)
{
    RecordMetadata meta;
    if (!handle || gridOffset < 0) return;
    if (IsamFileHandle_ReadRecordMetadata(handle, gridOffset, &meta) != GMB_DB_OK) return;
    MvccUndoGc_PruneChainTail(handle, meta.PreviousVersionOffset);
}

void MvccUndoGc_ReclaimTransactionalUndoSlots(IsamFileHandle* handle, const int64_t* undoOffsets, int count)
{
    int i;
    if (!handle || !undoOffsets || count <= 0) return;
    for (i = 0; i < count; i++) {
        RecordMetadata meta;
        if (undoOffsets[i] < 0) continue;
        if (IsamFileHandle_ReadRecordMetadata(handle, undoOffsets[i], &meta) != GMB_DB_OK) continue;
        meta.Status = Config_RecordStatusDeleted;
        meta.PreviousVersionOffset = -1;
        IsamFileHandle_WriteRecordMetadata(handle, undoOffsets[i], &meta);
    }
}
