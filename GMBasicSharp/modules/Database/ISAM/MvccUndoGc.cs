// FILE: Basic.ISAM.MvccUndoGc

using Basic.ISAM;

namespace Basic.ISAM;

/// <summary>
/// Policy GC catene undo MVCC: tronca profondità e recupera slot su rollback.
/// ponytail: reclaim heap completo resta su PACK (file nuovo senza orfani heap).
/// </summary>
internal static class MvccUndoGc
{
    /// <summary>Profondità massima catena undo dopo commit/auto-commit.</summary>
    internal const int MaxRetainedChainDepth = 1;

    /// <summary>
    /// Dopo scrittura su offset griglia, tronca la catena undo oltre <see cref="MaxRetainedChainDepth"/>.
    /// </summary>
    internal static void PruneAfterGridWrite(IsamFileHandle handle, long gridOffset)
    {
        var meta = handle.ReadRecordMetadata(gridOffset, handle.FileEncoding);
        PruneChainTail(handle, meta.PreviousVersionOffset);
    }

    /// <summary>
    /// Segna slot undo transazionali orfani dopo rollback (spazio recuperato da PACK).
    /// </summary>
    internal static void ReclaimTransactionalUndoSlots(IsamFileHandle handle, IEnumerable<long> undoOffsets)
    {
        foreach (var offset in undoOffsets)
        {
            if (offset < 0)
            {
                continue;
            }

            var meta = handle.ReadRecordMetadata(offset, handle.FileEncoding);
            meta.Status = Config.RecordStatusDeleted;
            meta.PreviousVersionOffset = -1;
            handle.WriteRecordMetadata(offset, meta);
        }
    }

    internal static int MeasureChainDepth(IsamFileHandle handle, long startOffset)
    {
        var depth = 0;
        var offset = startOffset;
        while (offset != -1)
        {
            depth++;
            var meta = handle.ReadRecordMetadata(offset, handle.FileEncoding);
            offset = meta.PreviousVersionOffset;
        }

        return depth;
    }

    private static void PruneChainTail(IsamFileHandle handle, long headOffset)
    {
        if (headOffset < 0)
        {
            return;
        }

        var depth = 0;
        var offset = headOffset;
        long lastKept = -1;

        while (offset != -1)
        {
            depth++;
            if (depth > MaxRetainedChainDepth && lastKept >= 0)
            {
                var keptMeta = handle.ReadRecordMetadata(lastKept, handle.FileEncoding);
                keptMeta.PreviousVersionOffset = -1;
                handle.WriteRecordMetadata(lastKept, keptMeta);
                break;
            }

            lastKept = offset;
            var meta = handle.ReadRecordMetadata(offset, handle.FileEncoding);
            offset = meta.PreviousVersionOffset;
        }
    }
}
