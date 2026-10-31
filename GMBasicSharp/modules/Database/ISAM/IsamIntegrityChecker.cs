// FILE: Basic.ISAM.IsamIntegrityChecker

using Basic.ISAM;

namespace Basic.ISAM;

/// <summary>
/// Controlli di integrità strutturale estesi oltre la validazione B+ base.
/// </summary>
internal static class IsamIntegrityChecker
{
    internal static IEnumerable<string> ValidatePageFreeList(IsamFileHandle handle)
    {
        var header = handle.Header;
        if (header.PageFreeListHeadOffset == -1)
        {
            yield break;
        }

        var visited = new HashSet<long>();
        var current = header.PageFreeListHeadOffset;
        var heapStart = header.LobHeapStartOffset > 0 ? header.LobHeapStartOffset : Config.FileHeaderFieldsPrefixSize;
        var heapEnd = header.NextAvailablePageOffset;
        Span<byte> nextPtr = stackalloc byte[sizeof(long)];

        while (current != -1)
        {
            if (current < heapStart || current >= heapEnd)
            {
                yield return $"Free list pagine: offset {current} fuori dall'area heap [{heapStart}, {heapEnd}).";
                yield break;
            }

            if (!visited.Add(current))
            {
                yield return $"Free list pagine: ciclo rilevato all'offset {current}.";
                yield break;
            }

            handle.ReadBytesAt(current, nextPtr);
            var next = BitConverter.ToInt64(nextPtr);
            if (next != -1 && (next <= 0 || next % IsamPageChecksum.PageSize != 0))
            {
                yield return $"Free list pagine: puntatore successivo non valido ({next}) a offset {current}.";
                yield break;
            }

            current = next;
        }
    }

    internal static IEnumerable<string> ValidateActiveRecordPointers(IsamFileHandle handle, BPlusTreeManager primaryIndex)
    {
        if (primaryIndex.GetRootOffset() == -1)
        {
            yield break;
        }

        var dataStart = handle.DataAreaStartOffset;
        var seenOffsets = new HashSet<long>();
        var firstLeaf = primaryIndex.FindFirstLeaf();
        if (firstLeaf.KeyCount == 0)
        {
            yield break;
        }

        var currentLeaf = firstLeaf;
        while (currentLeaf != null)
        {
            foreach (var offsetList in currentLeaf.RecordOffsets)
            {
                foreach (var offset in offsetList)
                {
                    if (!seenOffsets.Add(offset))
                    {
                        continue;
                    }

                    if (offset < dataStart)
                    {
                        yield return $"Offset record {offset} precede l'area dati ({dataStart}).";
                        continue;
                    }

                    var metadata = handle.ReadRecordMetadata(offset, handle.FileEncoding);
                    if (metadata.Status != Config.RecordStatusActive &&
                        metadata.Status != Config.RecordStatusDeleted)
                    {
                        yield return
                            $"Offset {offset}: stato record non valido 0x{metadata.Status:X2}.";
                    }

                    foreach (var message in ValidateJsonLobPointers(handle, offset))
                    {
                        yield return message;
                    }
                }
            }

            currentLeaf = currentLeaf.NextLeafOffset != -1
                ? (BPlusLeafNode)primaryIndex.LoadNode(currentLeaf.NextLeafOffset)
                : null;
        }
    }

    private static IEnumerable<string> ValidateJsonLobPointers(IsamFileHandle handle, long recordOffset)
    {
        var jsonFieldOffsets = HeapRelocation.GetJsonFieldOffsets(handle.Schema);
        if (jsonFieldOffsets.Length == 0)
        {
            yield break;
        }

        var recordSize = handle.Header.RecordSize;
        var buffer = new byte[recordSize];
        handle.ReadRecordBytes(recordOffset, buffer);

        var heapStart = handle.Header.LobHeapStartOffset > 0
            ? handle.Header.LobHeapStartOffset
            : handle.Header.NextAvailablePageOffset;
        var heapEnd = handle.Header.NextAvailablePageOffset;

        foreach (var fieldOffset in jsonFieldOffsets)
        {
            if (fieldOffset + sizeof(long) > buffer.Length)
            {
                yield return $"Record {recordOffset}: offset campo JSON {fieldOffset} fuori dal record.";
                continue;
            }

            var lobOffset = BitConverter.ToInt64(buffer, fieldOffset);
            if (lobOffset == 0)
            {
                continue;
            }

            if (lobOffset < heapStart || lobOffset >= heapEnd)
            {
                yield return
                    $"Record {recordOffset}: puntatore LOB JSON {lobOffset} fuori heap [{heapStart}, {heapEnd}).";
                continue;
            }

            string? lobError = null;
            try
            {
                _ = handle.ReadLob(lobOffset);
            }
            catch (Exception ex)
            {
                lobError =
                    $"Record {recordOffset}: LOB JSON a {lobOffset} illeggibile ({ex.Message}).";
            }

            if (lobError != null)
            {
                yield return lobError;
            }
        }
    }
}
