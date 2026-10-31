// FILE: Basic.DBF.DbfIndex.Ndx.cs

using System.Text;
using Basic.Databases;

namespace Basic.DBF;

/// <summary>dBASE III/IV NDX anchor + page constants (512-byte blocks).</summary>
internal static class NdxFormat
{
    public const int BlockSize = 512;
    public const int ExpressionOffset = 24;
    public const int ExpressionLength = 100;

    public static int GroupLength(int keyLength) => 8 + ((keyLength + 3) / 4) * 4;

    public static ushort MaxKeysPerPage(int keyLength) =>
        (ushort)((BlockSize - 4) / GroupLength(keyLength));

    public static uint BlockOffset(uint blockNumber) => blockNumber * (uint)BlockSize;
}

internal readonly struct NdxAnchor
{
    public uint RootBlock { get; init; }
    public uint EofBlock { get; init; }
    public ushort KeyLength { get; init; }
    public ushort MaxKeys { get; init; }
    public ushort GroupLength { get; init; }
    public bool Unique { get; init; }
    public string Expression { get; init; }

    public static NdxAnchor Parse(ReadOnlySpan<byte> block)
    {
        var exprSpan = block.Slice(NdxFormat.ExpressionOffset, NdxFormat.ExpressionLength);
        var exprLen = exprSpan.IndexOf((byte)0);
        if (exprLen < 0)
        {
            exprLen = NdxFormat.ExpressionLength;
        }

        return new NdxAnchor
        {
            RootBlock = BitConverter.ToUInt32(block),
            EofBlock = BitConverter.ToUInt32(block.Slice(4, 4)),
            KeyLength = BitConverter.ToUInt16(block.Slice(12, 2)),
            MaxKeys = BitConverter.ToUInt16(block.Slice(14, 2)),
            GroupLength = BitConverter.ToUInt16(block.Slice(18, 2)),
            Unique = block[23] != 0,
            Expression = exprLen == 0 ? string.Empty : Encoding.ASCII.GetString(exprSpan[..exprLen]),
        };
    }
}

internal sealed class NdxPage
{
    private readonly byte[] _data;
    private readonly int _groupLen;

    public int ItemCount { get; }

    public NdxPage(byte[] data, int groupLen)
    {
        _data = data;
        _groupLen = groupLen;
        ItemCount = BitConverter.ToUInt16(_data);
    }

    public ReadOnlySpan<byte> GetKey(int index, int keyLength) =>
        _data.AsSpan(4 + (index * _groupLen) + 8, keyLength);

    public uint GetChildBlock(int index) =>
        BitConverter.ToUInt32(_data, 4 + (index * _groupLen));

    public uint GetRecordNumber(int index) =>
        BitConverter.ToUInt32(_data, 4 + (index * _groupLen) + 4);

    public bool IsLeaf => ItemCount > 0 && GetChildBlock(0) == 0;
}

/// <summary>Writes dBASE-compatible NDX files (512-byte B+tree pages).</summary>
internal static class NdxBuilder
{
    public static void Write(
        string path,
        string keyExpression,
        int keyLength,
        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries,
        bool unique)
    {
        var groupLen = NdxFormat.GroupLength(keyLength);
        var maxKeys = NdxFormat.MaxKeysPerPage(keyLength);
        var sorted = entries
            .Select(e => (Key: e.Key.ToArray(), e.Recno))
            .OrderBy(e => e.Key, Comparer<byte[]>.Create(static (a, b) => a.AsSpan().SequenceCompareTo(b)))
            .ToList();

        var dataPages = new List<byte[]>();
        uint rootBlock;

        if (sorted.Count == 0)
        {
            dataPages.Add(BuildLeafPage([], keyLength, groupLen, maxKeys));
            rootBlock = 1;
        }
        else
        {
            var leaves = new List<(uint BlockNum, byte[] FirstKey)>();
            for (var i = 0; i < sorted.Count; i += maxKeys)
            {
                var chunk = sorted.Skip(i).Take(maxKeys)
                    .Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno))
                    .ToList();
                dataPages.Add(BuildLeafPage(chunk, keyLength, groupLen, maxKeys));
                leaves.Add(((uint)dataPages.Count, chunk[0].Item1.ToArray()));
            }

            var level = leaves;
            while (level.Count > 1)
            {
                level = BuildInternalLevel(level, dataPages, keyLength, groupLen, maxKeys);
            }

            rootBlock = level[0].BlockNum;
        }

        var anchor = BuildAnchor(keyExpression, keyLength, groupLen, maxKeys, rootBlock,
            (uint)(dataPages.Count + 1), unique);

        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using var fs = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
        fs.Write(anchor);
        foreach (var page in dataPages)
        {
            fs.Write(page);
        }
    }

    private static List<(uint BlockNum, byte[] FirstKey)> BuildInternalLevel(
        List<(uint BlockNum, byte[] FirstKey)> children,
        List<byte[]> dataPages,
        int keyLength,
        int groupLen,
        ushort maxKeys)
    {
        var parents = new List<(uint BlockNum, byte[] FirstKey)>();
        var index = 0;

        while (index < children.Count)
        {
            var groupStart = index;
            var childBlocks = new List<uint> { children[index].BlockNum };
            var separators = new List<byte[]>();
            index++;

            while (index < children.Count && separators.Count < maxKeys)
            {
                separators.Add(children[index].FirstKey);
                childBlocks.Add(children[index].BlockNum);
                index++;
            }

            dataPages.Add(BuildInternalPage(childBlocks, separators, keyLength, groupLen, maxKeys));
            var blockNum = (uint)dataPages.Count;
            var promoteKey = separators.Count > 0 ? separators[0] : children[groupStart].FirstKey;
            parents.Add((blockNum, promoteKey));
        }

        return parents;
    }

    private static byte[] BuildAnchor(
        string keyExpression,
        int keyLength,
        int groupLen,
        ushort maxKeys,
        uint rootBlock,
        uint eofBlock,
        bool unique)
    {
        var anchor = new byte[NdxFormat.BlockSize];
        BitConverter.GetBytes(rootBlock).CopyTo(anchor, 0);
        BitConverter.GetBytes(eofBlock).CopyTo(anchor, 4);
        BitConverter.GetBytes((ushort)keyLength).CopyTo(anchor, 12);
        BitConverter.GetBytes(maxKeys).CopyTo(anchor, 14);
        BitConverter.GetBytes((ushort)groupLen).CopyTo(anchor, 18);
        anchor[23] = unique ? (byte)1 : (byte)0;
        var exprBytes = Encoding.ASCII.GetBytes(keyExpression.ToLowerInvariant());
        exprBytes.AsSpan(0, Math.Min(exprBytes.Length, NdxFormat.ExpressionLength))
            .CopyTo(anchor.AsSpan(NdxFormat.ExpressionOffset));
        return anchor;
    }

    private static byte[] BuildLeafPage(
        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries,
        int keyLength,
        int groupLen,
        ushort maxKeys)
    {
        _ = maxKeys;
        var page = new byte[NdxFormat.BlockSize];
        BitConverter.GetBytes((ushort)entries.Count).CopyTo(page, 0);

        for (var i = 0; i < entries.Count; i++)
        {
            var offset = 4 + (i * groupLen);
            BitConverter.GetBytes(0u).CopyTo(page, offset);
            BitConverter.GetBytes((uint)entries[i].Recno).CopyTo(page, offset + 4);
            entries[i].Key.Span.CopyTo(page.AsSpan(offset + 8, keyLength));
        }

        return page;
    }

    private static byte[] BuildInternalPage(
        IReadOnlyList<uint> childBlocks,
        IReadOnlyList<byte[]> separatorKeys,
        int keyLength,
        int groupLen,
        ushort maxKeys)
    {
        _ = maxKeys;
        var page = new byte[NdxFormat.BlockSize];
        BitConverter.GetBytes((ushort)separatorKeys.Count).CopyTo(page, 0);

        for (var i = 0; i < childBlocks.Count; i++)
        {
            var offset = 4 + (i * groupLen);
            BitConverter.GetBytes(childBlocks[i]).CopyTo(page, offset);
            BitConverter.GetBytes(0u).CopyTo(page, offset + 4);
            if (i < separatorKeys.Count)
            {
                separatorKeys[i].AsSpan(0, Math.Min(separatorKeys[i].Length, keyLength))
                    .CopyTo(page.AsSpan(offset + 8, keyLength));
            }
        }

        return page;
    }
}

/// <summary>
/// dBASE NDX index with read btree traversal and ponytail in-memory rewrite for mutations.
/// </summary>
public sealed class NdxFile : IWritableIndexHandle
{
    private readonly string _path;
    private readonly ILockManager? _lockManager;
    private readonly object _syncRoot = new();
    private readonly List<(byte[] Key, int Recno)> _entries;
    private readonly string _keyExpressionString;
    private readonly bool _writable;
    private List<int>? _recordOrderCache;

    public string Name { get; }
    public bool Unique { get; }
    public bool Descending { get; }
    public int KeyLength { get; }

    private NdxFile(
        string path,
        List<(byte[] Key, int Recno)> entries,
        string keyExpression,
        int keyLength,
        bool unique,
        ILockManager? lockManager,
        bool writable)
    {
        _path = path;
        _entries = entries;
        _keyExpressionString = keyExpression;
        KeyLength = keyLength;
        Unique = unique;
        Descending = false;
        _lockManager = lockManager;
        _writable = writable;
        Name = Path.GetFileNameWithoutExtension(path);
    }

    public static NdxFile CreateNew(
        string filePath,
        Expr keyExpression,
        int keyLength,
        bool isUnique,
        ILockManager lockManager,
        LockingStrategy lockingStrategy)
    {
        _ = lockingStrategy;
        var exprStr = DbfTotalExpressions.ToPlainString(keyExpression);
        NdxBuilder.Write(filePath, exprStr, keyLength, [], isUnique);
        return OpenExisting(filePath, lockManager, lockingStrategy);
    }

    public static NdxFile OpenExisting(string path, ILockManager? lockManager, LockingStrategy lockingStrategy)
    {
        _ = lockingStrategy;
        var entries = LoadEntriesFromDisk(path);
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        var anchorBuf = new byte[NdxFormat.BlockSize];
        fs.ReadExactly(anchorBuf);
        var anchor = NdxAnchor.Parse(anchorBuf);
        return new NdxFile(
            path,
            entries,
            anchor.Expression,
            anchor.KeyLength,
            anchor.Unique,
            lockManager,
            writable: lockManager != null);
    }

    internal static List<(byte[] Key, int Recno)> LoadEntriesFromDisk(string path)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        var anchorBuf = new byte[NdxFormat.BlockSize];
        fs.ReadExactly(anchorBuf);
        var anchor = NdxAnchor.Parse(anchorBuf);
        if (anchor.RootBlock == 0)
        {
            return [];
        }

        var groupLen = anchor.GroupLength > 0 ? anchor.GroupLength : NdxFormat.GroupLength(anchor.KeyLength);
        var result = new List<(byte[] Key, int Recno)>();
        EnumerateBlock(fs, anchor.RootBlock, anchor.KeyLength, groupLen, result);
        return result;
    }

    private static void EnumerateBlock(
        FileStream fs,
        uint blockNumber,
        int keyLength,
        int groupLen,
        List<(byte[] Key, int Recno)> result)
    {
        var page = ReadPage(fs, blockNumber, groupLen);
        if (page.ItemCount == 0)
        {
            return;
        }

        if (page.IsLeaf)
        {
            for (var i = 0; i < page.ItemCount; i++)
            {
                result.Add((page.GetKey(i, keyLength).ToArray(), (int)page.GetRecordNumber(i)));
            }

            return;
        }

        for (var i = 0; i <= page.ItemCount; i++)
        {
            var child = page.GetChildBlock(i);
            if (child != 0)
            {
                EnumerateBlock(fs, child, keyLength, groupLen, result);
            }
        }
    }

    private static NdxPage ReadPage(FileStream fs, uint blockNumber, int groupLen)
    {
        var buffer = new byte[NdxFormat.BlockSize];
        fs.Position = NdxFormat.BlockOffset(blockNumber);
        fs.ReadExactly(buffer);
        return new NdxPage(buffer, groupLen);
    }

    public string GetKeyExpressionString() => _keyExpressionString;

    public int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
    {
        AcquireSerializableReadLock(keyBytes, context);
        lock (_syncRoot)
        {
            return SeekInMemory(keyBytes, soft);
        }
    }

    public void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
    {
        EnsureWritable();
        AcquireSerializableWriteLock(keyBytes, context);
        lock (_syncRoot)
        {
            InsertUnderLock(keyBytes, recno);
            FlushToDisk();
        }
    }

    public void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context)
    {
        EnsureWritable();
        if (entries.Count == 0)
        {
            return;
        }

        foreach (var (key, _) in entries)
        {
            AcquireSerializableWriteLock(key.Span, context);
        }

        lock (_syncRoot)
        {
            foreach (var (key, recno) in entries)
            {
                InsertUnderLock(key.Span, recno);
            }

            FlushToDisk();
        }
    }

    public void Delete(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
    {
        EnsureWritable();
        AcquireSerializableWriteLock(keyBytes, context);
        lock (_syncRoot)
        {
            var idx = FindKeyIndex(keyBytes, out var exact);
            if (!exact)
            {
                return;
            }

            for (var i = idx; i < _entries.Count; i++)
            {
                if (_entries[i].Key.AsSpan().SequenceCompareTo(keyBytes) != 0)
                {
                    break;
                }

                if (_entries[i].Recno == recno)
                {
                    _entries.RemoveAt(i);
                    InvalidateCache();
                    FlushToDisk();
                    return;
                }
            }
        }
    }

    public void Update(ReadOnlySpan<byte> oldKey, ReadOnlySpan<byte> newKey, int recno, TransactionContext? context)
    {
        Delete(oldKey, recno, context);
        Insert(newKey, recno, context);
    }

    public void Rebuild(Func<IEnumerable<(ReadOnlyMemory<byte> key, int recno)>> producer)
    {
        EnsureWritable();
        lock (_syncRoot)
        {
            _entries.Clear();
            foreach (var (key, recno) in producer())
            {
                InsertUnderLock(key.Span, recno);
            }

            InvalidateCache();
            FlushToDisk();
        }
    }

    public int GetFirstRecordNumber()
    {
        lock (_syncRoot)
        {
            return _entries.Count > 0 ? _entries[0].Recno : 0;
        }
    }

    public int GetLastRecordNumber()
    {
        lock (_syncRoot)
        {
            return _entries.Count > 0 ? _entries[^1].Recno : 0;
        }
    }

    public IEnumerable<int> GetAllRecordNumbers()
    {
        lock (_syncRoot)
        {
            _recordOrderCache ??= _entries.Select(e => e.Recno).ToList();
            return _recordOrderCache.ToList();
        }
    }

    public (int recno, uint pageNum, int indexInLeaf) GetFirstRecordPosition(TransactionContext? context)
    {
        lock (_syncRoot)
        {
            if (_entries.Count == 0)
            {
                return (0, 0, 0);
            }

            if (context is { IsolationLevel: IsolationLevel.Serializable })
            {
                AcquireSerializableReadLock(_entries[0].Key, context);
            }

            return (_entries[0].Recno, 1, 0);
        }
    }

    public (int recno, uint pageNum, int indexInLeaf) GetLastRecordPosition(TransactionContext? context)
    {
        lock (_syncRoot)
        {
            if (_entries.Count == 0)
            {
                return (0, 0, 0);
            }

            if (context is { IsolationLevel: IsolationLevel.Serializable })
            {
                AcquireSerializableReadLock(_entries[^1].Key, context);
            }

            var last = _entries.Count - 1;
            return (_entries[last].Recno, 1, last);
        }
    }

    public (int recno, uint newPageNum, int newIndexInLeaf) GetNextRecord(
        uint currentPageNum, int currentIndexInLeaf, int direction, TransactionContext? context)
    {
        _ = currentPageNum;
        lock (_syncRoot)
        {
            var newIndex = currentIndexInLeaf + direction;
            if (newIndex < 0 || newIndex >= _entries.Count)
            {
                return (0, 0, 0);
            }

            if (context is { IsolationLevel: IsolationLevel.Serializable })
            {
                AcquireSerializableReadLock(_entries[newIndex].Key, context);
            }

            return (_entries[newIndex].Recno, 1, newIndex);
        }
    }

    public void InvalidateCache()
    {
        lock (_syncRoot)
        {
            _recordOrderCache = null;
        }
    }

    public void Dispose()
    {
    }

    private void EnsureWritable()
    {
        if (!_writable)
        {
            throw new NotSupportedException("L'indice NDX è aperto in sola lettura.");
        }
    }

    private int SeekInMemory(ReadOnlySpan<byte> keyBytes, bool soft)
    {
        if (_entries.Count == 0)
        {
            return -1;
        }

        var idx = FindKeyIndex(keyBytes, out var exact);
        if (exact)
        {
            return _entries[idx].Recno;
        }

        return soft && idx < _entries.Count ? _entries[idx].Recno : -1;
    }

    private void InsertUnderLock(ReadOnlySpan<byte> keyBytes, int recno)
    {
        var idx = FindKeyIndex(keyBytes, out var exact);
        if (Unique && exact)
        {
            throw new InvalidOperationException($"Chiave duplicata nell'indice '{Name}'.");
        }

        _entries.Insert(idx, (keyBytes.ToArray(), recno));
        InvalidateCache();
    }

    private int FindKeyIndex(ReadOnlySpan<byte> keyBytes, out bool exactMatch)
    {
        exactMatch = false;
        var lo = 0;
        var hi = _entries.Count - 1;
        var insertAt = _entries.Count;

        while (lo <= hi)
        {
            var mid = (lo + hi) >> 1;
            var cmp = keyBytes.SequenceCompareTo(_entries[mid].Key);
            if (cmp == 0)
            {
                exactMatch = true;
                return mid;
            }

            if (cmp < 0)
            {
                insertAt = mid;
                hi = mid - 1;
            }
            else
            {
                lo = mid + 1;
            }
        }

        return insertAt;
    }

    private void FlushToDisk()
    {
        var payload = _entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList();
        NdxBuilder.Write(_path, _keyExpressionString, KeyLength, payload, Unique);
    }

    private void AcquireSerializableReadLock(ReadOnlySpan<byte> keyBytes, TransactionContext? context)
    {
        if (context is not { IsolationLevel: IsolationLevel.Serializable } || _lockManager == null)
        {
            return;
        }

        var lockKey = LockManagerBase.GetKeyLockKey(Name, keyBytes.ToArray());
        _lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{Name}'");
        context.ReadLockedKeys.Add(lockKey);
    }

    private void AcquireSerializableWriteLock(ReadOnlySpan<byte> keyBytes, TransactionContext? context)
    {
        if (context is not { IsolationLevel: IsolationLevel.Serializable } || _lockManager == null)
        {
            return;
        }

        var lockKey = LockManagerBase.GetKeyLockKey(Name, keyBytes.ToArray());
        _lockManager.AcquireWriteLock(lockKey, 5000, $"chiave di indice '{Name}'");
        context.WriteLockedKeys.Add(lockKey);
    }
}
