// FILE: Basic.DBF.DbfIndex.Clipper.cs

using System.Text;
using Basic.Databases;

// ReSharper disable InconsistentNaming

namespace Basic.DBF;

#region Clipper-Specific Structures

/// <summary>
/// Parsed Clipper 5.x NTX file header (1024-byte page 0).
/// Layout per Harbour DBFNTX / Clickety Click NTX spec — not GM's writable NtxFile format.
/// </summary>
internal readonly struct ClipperNtxHeader
{
    public const int PageSize = 1024;
    public const int KeyExpressionOffset = 22;
    public const int UniqueFlagOffset = 278;
    public const int DescendingFlagOffset = 280;

    public ushort Type { get; init; }
    public ushort Version { get; init; }
    public uint RootBlock { get; init; }
    public uint NextPage { get; init; }
    public ushort ItemSize { get; init; }
    public ushort KeyLength { get; init; }
    public ushort KeyDecimals { get; init; }
    public ushort MaxItems { get; init; }
    public ushort HalfPage { get; init; }

    /// <summary>
    /// True when the type word matches Clipper 5.x (0x0006) or Clipper 87 (0x0003).
    /// </summary>
    public static bool IsClipperType(ushort type)
    {
        var baseType = (ushort)(type & 0x87FF);
        return baseType is 0x0006 or 0x0003;
    }

    public static uint ResolveBlockAddress(uint block) =>
        block is > 0 and < PageSize ? block * PageSize : block;

    public static ClipperNtxHeader Parse(ReadOnlySpan<byte> buffer)
    {
        if (buffer.Length < PageSize)
            throw new InvalidDataException("Clipper NTX header must be 1024 bytes.");

        return new ClipperNtxHeader
        {
            Type = BitConverter.ToUInt16(buffer),
            Version = BitConverter.ToUInt16(buffer.Slice(2, 2)),
            RootBlock = BitConverter.ToUInt32(buffer.Slice(4, 4)),
            NextPage = BitConverter.ToUInt32(buffer.Slice(8, 4)),
            ItemSize = BitConverter.ToUInt16(buffer.Slice(12, 2)),
            KeyLength = BitConverter.ToUInt16(buffer.Slice(14, 2)),
            KeyDecimals = BitConverter.ToUInt16(buffer.Slice(16, 2)),
            MaxItems = BitConverter.ToUInt16(buffer.Slice(18, 2)),
            HalfPage = BitConverter.ToUInt16(buffer.Slice(20, 2)),
        };
    }
}

/// <summary>
/// Clipper NTX page with offset table (Harbour hb_ntxGetKeyOffset layout).
/// Each key entry: [child page uint32][recno uint32][key bytes].
/// </summary>
internal sealed class ClipperPage
{
    private readonly byte[] _data;
    private readonly int _keyLength;

    public int ItemCount { get; }

    public ClipperPage(byte[] data, int keyLength)
    {
        _data = data;
        _keyLength = keyLength;
        ItemCount = BitConverter.ToUInt16(_data, 0);
    }

    private int EntryOffset(int index) => BitConverter.ToUInt16(_data, 2 + (index << 1));

    public ReadOnlySpan<byte> GetKey(int index) =>
        _data.AsSpan(EntryOffset(index) + 8, _keyLength);

    public uint GetChildPage(int index) =>
        BitConverter.ToUInt32(_data, EntryOffset(index));

    public uint GetRecordNumber(int index) =>
        BitConverter.ToUInt32(_data, EntryOffset(index) + 4);

    public bool IsLeaf => ItemCount > 0 && GetChildPage(0) == 0;
}

#endregion

/// <summary>
/// Read-only Clipper 5.x NTX index (signature 0x0006 at header offset 0).
/// </summary>
internal sealed class ClipperNtxFile : IIndexHandle
{
    private readonly FileStream _fs;
    private readonly ClipperNtxHeader _header;
    private readonly byte[] _headerBuffer;
    private readonly string _keyExpressionString;
    private List<int>? _recordOrderCache;
    private readonly Lock _cacheLock = new();

    public string Name { get; }
    public bool Unique { get; }
    public bool Descending { get; }
    public int KeyLength => _header.KeyLength;

    public ClipperNtxFile(string path)
    {
        _fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        Name = Path.GetFileNameWithoutExtension(path);

        _headerBuffer = new byte[ClipperNtxHeader.PageSize];
        _fs.ReadExactly(_headerBuffer);
        _header = ClipperNtxHeader.Parse(_headerBuffer);

        if (!ClipperNtxHeader.IsClipperType(_header.Type))
            throw new InvalidDataException($"File '{path}' non è un indice Clipper valido (type=0x{_header.Type:X4}).");

        Unique = _headerBuffer[ClipperNtxHeader.UniqueFlagOffset] != 0;
        Descending = _headerBuffer[ClipperNtxHeader.DescendingFlagOffset] != 0;

        _keyExpressionString = ReadKeyExpression();
    }

    /// <summary>
    /// ponytail: Clipper stores page index (&lt;1024) or byte offset (≥1024); Harbour uses page index × 1024.
    /// </summary>
    private static uint ResolveBlockAddress(uint block) =>
        ClipperNtxHeader.ResolveBlockAddress(block);

    /// <summary>Exports all (key, recno) pairs for writable reopen.</summary>
    internal IEnumerable<(byte[] Key, int Recno)> EnumerateEntries()
    {
        if (_header.RootBlock == 0)
        {
            yield break;
        }

        foreach (var entry in EnumerateBlock(_header.RootBlock))
        {
            yield return entry;
        }
    }

    private IEnumerable<(byte[] Key, int Recno)> EnumerateBlock(uint blockAddress)
    {
        var page = ReadPage(ResolveBlockAddress(blockAddress));
        if (page.ItemCount == 0)
        {
            yield break;
        }

        if (page.IsLeaf)
        {
            for (var i = 0; i < page.ItemCount; i++)
            {
                yield return (page.GetKey(i).ToArray(), (int)page.GetRecordNumber(i));
            }

            yield break;
        }

        for (var i = 0; i <= page.ItemCount; i++)
        {
            var child = page.GetChildPage(i);
            if (child == 0)
            {
                continue;
            }

            foreach (var entry in EnumerateBlock(child))
            {
                yield return entry;
            }
        }
    }

    private string ReadKeyExpression()
    {
        var span = _headerBuffer.AsSpan(ClipperNtxHeader.KeyExpressionOffset, 256);
        var length = span.IndexOf((byte)0);
        if (length < 0) length = span.Length;
        return length == 0 ? string.Empty : Encoding.ASCII.GetString(span[..length]);
    }

    private ClipperPage ReadPage(uint fileOffset)
    {
        var buffer = new byte[ClipperNtxHeader.PageSize];
        _fs.Position = fileOffset;
        _fs.ReadExactly(buffer);
        return new ClipperPage(buffer, _header.KeyLength);
    }

    private static int FindKeyIndex(ClipperPage page, ReadOnlySpan<byte> keyBytes, out bool exactMatch)
    {
        exactMatch = false;
        var last = -1;
        var begin = 0;
        var end = page.ItemCount - 1;

        while (begin <= end)
        {
            var mid = (begin + end) >> 1;
            var cmp = keyBytes.SequenceCompareTo(page.GetKey(mid));
            if (cmp == 0)
            {
                exactMatch = true;
                return mid;
            }

            if (cmp > 0)
            {
                begin = mid + 1;
            }
            else
            {
                last = mid;
                end = mid - 1;
            }
        }

        return last >= 0 ? last : page.ItemCount;
    }

    private static int FindChildIndex(ClipperPage page, ReadOnlySpan<byte> keyBytes)
    {
        for (var i = 0; i < page.ItemCount; i++)
        {
            if (keyBytes.SequenceCompareTo(page.GetKey(i)) < 0)
            {
                return i;
            }
        }

        return page.ItemCount;
    }

    private int SeekInPage(uint blockAddress, ReadOnlySpan<byte> keyBytes, bool soft)
    {
        var pageAddress = ResolveBlockAddress(blockAddress);

        while (true)
        {
            var page = ReadPage(pageAddress);
            if (page.ItemCount == 0)
                return -1;

            if (!page.IsLeaf)
            {
                var childIndex = FindChildIndex(page, keyBytes);
                var childBlock = page.GetChildPage(childIndex);
                if (childBlock == 0)
                {
                    return -1;
                }

                pageAddress = ResolveBlockAddress(childBlock);
                continue;
            }

            var keyIndex = FindKeyIndex(page, keyBytes, out var exactMatch);
            if (exactMatch)
                return (int)page.GetRecordNumber(keyIndex);

            if (soft && keyIndex < page.ItemCount)
                return (int)page.GetRecordNumber(keyIndex);

            return -1;
        }
    }

    private (int Recno, byte[]? KeyBytes) SeekInPageWithKey(uint blockAddress, ReadOnlySpan<byte> keyBytes, bool soft)
    {
        var pageAddress = ResolveBlockAddress(blockAddress);

        while (true)
        {
            var page = ReadPage(pageAddress);
            if (page.ItemCount == 0)
                return (-1, null);

            if (!page.IsLeaf)
            {
                var childIndex = FindChildIndex(page, keyBytes);
                var childBlock = page.GetChildPage(childIndex);
                if (childBlock == 0)
                {
                    return (-1, null);
                }

                pageAddress = ResolveBlockAddress(childBlock);
                continue;
            }

            var keyIndex = FindKeyIndex(page, keyBytes, out var exactMatch);
            if (exactMatch)
                return ((int)page.GetRecordNumber(keyIndex), page.GetKey(keyIndex).ToArray());

            if (soft && keyIndex < page.ItemCount)
                return ((int)page.GetRecordNumber(keyIndex), page.GetKey(keyIndex).ToArray());

            return (-1, null);
        }
    }

    private int GetRecordNumberAtEdge(uint blockAddress, int direction)
    {
        var pageAddress = ResolveBlockAddress(blockAddress);

        while (true)
        {
            var page = ReadPage(pageAddress);
            if (page.ItemCount == 0)
                return 0;

            var childIndex = direction < 0 ? 0 : page.ItemCount;
            var childBlock = page.GetChildPage(childIndex);

            if (childBlock == 0)
            {
                var keyIndex = direction < 0 ? 0 : page.ItemCount - 1;
                return (int)page.GetRecordNumber(keyIndex);
            }

            pageAddress = ResolveBlockAddress(childBlock);
        }
    }

    private List<int> GetRecordOrderCache()
    {
        lock (_cacheLock)
        {
            if (_recordOrderCache != null) return _recordOrderCache;
            _recordOrderCache = [];
            if (_header.RootBlock != 0)
                PopulateCacheRecursive(_header.RootBlock);
            return _recordOrderCache;
        }
    }

    private void PopulateCacheRecursive(uint blockAddress)
    {
        var page = ReadPage(ResolveBlockAddress(blockAddress));
        if (page.ItemCount == 0)
            return;

        if (page.IsLeaf)
        {
            for (var i = 0; i < page.ItemCount; i++)
                _recordOrderCache!.Add((int)page.GetRecordNumber(i));
            return;
        }

        for (var i = 0; i <= page.ItemCount; i++)
        {
            var child = page.GetChildPage(i);
            if (child != 0)
                PopulateCacheRecursive(child);
        }
    }

    public string GetKeyExpressionString() => _keyExpressionString;

    public int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
    {
        if (_header.RootBlock == 0) return -1;
        return SeekInPage(_header.RootBlock, keyBytes, soft);
    }

    public (int Recno, byte[]? KeyBytes) SeekWithKeyBytes(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
    {
        if (_header.RootBlock == 0) return (-1, null);
        return SeekInPageWithKey(_header.RootBlock, keyBytes, soft);
    }

    public void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context) =>
        throw new NotSupportedException("Gli indici Clipper sono aperti in sola lettura.");

    public void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context) =>
        throw new NotSupportedException("Gli indici Clipper sono aperti in sola lettura.");

    public void Delete(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context) =>
        throw new NotSupportedException("Gli indici Clipper sono aperti in sola lettura.");

    public void Update(ReadOnlySpan<byte> oldKey, ReadOnlySpan<byte> newKey, int recno, TransactionContext? context) =>
        throw new NotSupportedException("Gli indici Clipper sono aperti in sola lettura.");

    public void Rebuild(Func<IEnumerable<(ReadOnlyMemory<byte> key, int recno)>> producer) =>
        throw new NotSupportedException("Gli indici Clipper sono aperti in sola lettura.");

    public int GetFirstRecordNumber()
    {
        if (_header.RootBlock == 0) return 0;
        return GetRecordNumberAtEdge(_header.RootBlock, -1);
    }

    public int GetLastRecordNumber()
    {
        if (_header.RootBlock == 0) return 0;
        return GetRecordNumberAtEdge(_header.RootBlock, 1);
    }

    public IEnumerable<int> GetAllRecordNumbers() => GetRecordOrderCache();

    public (int recno, uint pageNum, int indexInLeaf) GetFirstRecordPosition(TransactionContext? context)
    {
        var cache = GetRecordOrderCache();
        if (cache.Count == 0) return (0, 0, 0);
        return (cache[0], 1, 0);
    }

    public (int recno, uint pageNum, int indexInLeaf) GetLastRecordPosition(TransactionContext? context)
    {
        var cache = GetRecordOrderCache();
        if (cache.Count == 0) return (0, 0, 0);
        var lastIndex = cache.Count - 1;
        return (cache[lastIndex], 1, lastIndex);
    }

    public (int recno, uint newPageNum, int newIndexInLeaf) GetNextRecord(
        uint currentPageNum, int currentIndexInLeaf, int direction, TransactionContext? context)
    {
        var cache = GetRecordOrderCache();
        var newIndex = currentIndexInLeaf + direction;

        if (newIndex >= 0 && newIndex < cache.Count)
            return (cache[newIndex], 1, newIndex);

        return (0, 0, 0);
    }

    public void InvalidateCache()
    {
        lock (_cacheLock)
        {
            _recordOrderCache = null;
        }
    }

    public void Dispose() => _fs.Dispose();
}
