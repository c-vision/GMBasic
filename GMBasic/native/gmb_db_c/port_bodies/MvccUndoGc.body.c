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
