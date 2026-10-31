/* FILE: native/gmb_db_c/src/DBF/DbfIndex.Ndx.c */
/* Port 1:1 di modules/Database/DBF/DbfIndex.Ndx.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfIndex.Ndx.cs
//
//using System.Text;
//using Basic.Databases;
//
//namespace Basic.DBF;
//
///// <summary>dBASE III/IV NDX anchor + page constants (512-byte blocks).</summary>
//internal static class NdxFormat
//{
//    public const int BlockSize = 512;
//    public const int ExpressionOffset = 24;
//    public const int ExpressionLength = 100;
//
//    public static int GroupLength(int keyLength) => 8 + ((keyLength + 3) / 4) * 4;
//
//    public static ushort MaxKeysPerPage(int keyLength) =>
//        (ushort)((BlockSize - 4) / GroupLength(keyLength));
//
//    public static uint BlockOffset(uint blockNumber) => blockNumber * (uint)BlockSize;
//}
//
//internal readonly struct NdxAnchor
//{
//    public uint RootBlock { get; init; }
//    public uint EofBlock { get; init; }
//    public ushort KeyLength { get; init; }
//    public ushort MaxKeys { get; init; }
//    public ushort GroupLength { get; init; }
//    public bool Unique { get; init; }
//    public string Expression { get; init; }
//
//    public static NdxAnchor Parse(ReadOnlySpan<byte> block)
//    {
//        var exprSpan = block.Slice(NdxFormat.ExpressionOffset, NdxFormat.ExpressionLength);
//        var exprLen = exprSpan.IndexOf((byte)0);
//        if (exprLen < 0)
//        {
//            exprLen = NdxFormat.ExpressionLength;
//        }
//
//        return new NdxAnchor
//        {
//            RootBlock = BitConverter.ToUInt32(block),
//            EofBlock = BitConverter.ToUInt32(block.Slice(4, 4)),
//            KeyLength = BitConverter.ToUInt16(block.Slice(12, 2)),
//            MaxKeys = BitConverter.ToUInt16(block.Slice(14, 2)),
//            GroupLength = BitConverter.ToUInt16(block.Slice(18, 2)),
//            Unique = block[23] != 0,
//            Expression = exprLen == 0 ? string.Empty : Encoding.ASCII.GetString(exprSpan[..exprLen]),
//        };
//    }
//}
//
//internal sealed class NdxPage
//{
//    private readonly byte[] _data;
//    private readonly int _groupLen;
//
//    public int ItemCount { get; }
//
//    public NdxPage(byte[] data, int groupLen)
//    {
//        _data = data;
//        _groupLen = groupLen;
//        ItemCount = BitConverter.ToUInt16(_data);
//    }
//
//    public ReadOnlySpan<byte> GetKey(int index, int keyLength) =>
//        _data.AsSpan(4 + (index * _groupLen) + 8, keyLength);
//
//    public uint GetChildBlock(int index) =>
//        BitConverter.ToUInt32(_data, 4 + (index * _groupLen));
//
//    public uint GetRecordNumber(int index) =>
//        BitConverter.ToUInt32(_data, 4 + (index * _groupLen) + 4);
//
//    public bool IsLeaf => ItemCount > 0 && GetChildBlock(0) == 0;
//}
//
///// <summary>Writes dBASE-compatible NDX files (512-byte B+tree pages).</summary>
//internal static class NdxBuilder
//{
//    public static void Write(
//        string path,
//        string keyExpression,
//        int keyLength,
//        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries,
//        bool unique)
//    {
//        var groupLen = NdxFormat.GroupLength(keyLength);
//        var maxKeys = NdxFormat.MaxKeysPerPage(keyLength);
//        var sorted = entries
//            .Select(e => (Key: e.Key.ToArray(), e.Recno))
//            .OrderBy(e => e.Key, Comparer<byte[]>.Create(static (a, b) => a.AsSpan().SequenceCompareTo(b)))
//            .ToList();
//
//        var dataPages = new List<byte[]>();
//        uint rootBlock;
//
//        if (sorted.Count == 0)
//        {
//            dataPages.Add(BuildLeafPage([], keyLength, groupLen, maxKeys));
//            rootBlock = 1;
//        }
//        else
//        {
//            var leaves = new List<(uint BlockNum, byte[] FirstKey)>();
//            for (var i = 0; i < sorted.Count; i += maxKeys)
//            {
//                var chunk = sorted.Skip(i).Take(maxKeys)
//                    .Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno))
//                    .ToList();
//                dataPages.Add(BuildLeafPage(chunk, keyLength, groupLen, maxKeys));
//                leaves.Add(((uint)dataPages.Count, chunk[0].Item1.ToArray()));
//            }
//
//            var level = leaves;
//            while (level.Count > 1)
//            {
//                level = BuildInternalLevel(level, dataPages, keyLength, groupLen, maxKeys);
//            }
//
//            rootBlock = level[0].BlockNum;
//        }
//
//        var anchor = BuildAnchor(keyExpression, keyLength, groupLen, maxKeys, rootBlock,
//            (uint)(dataPages.Count + 1), unique);
//
//        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
//        using var fs = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
//        fs.Write(anchor);
//        foreach (var page in dataPages)
//        {
//            fs.Write(page);
//        }
//    }
//
//    private static List<(uint BlockNum, byte[] FirstKey)> BuildInternalLevel(
//        List<(uint BlockNum, byte[] FirstKey)> children,
//        List<byte[]> dataPages,
//        int keyLength,
//        int groupLen,
//        ushort maxKeys)
//    {
//        var parents = new List<(uint BlockNum, byte[] FirstKey)>();
//        var index = 0;
//
//        while (index < children.Count)
//        {
//            var groupStart = index;
//            var childBlocks = new List<uint> { children[index].BlockNum };
//            var separators = new List<byte[]>();
//            index++;
//
//            while (index < children.Count && separators.Count < maxKeys)
//            {
//                separators.Add(children[index].FirstKey);
//                childBlocks.Add(children[index].BlockNum);
//                index++;
//            }
//
//            dataPages.Add(BuildInternalPage(childBlocks, separators, keyLength, groupLen, maxKeys));
//            var blockNum = (uint)dataPages.Count;
//            var promoteKey = separators.Count > 0 ? separators[0] : children[groupStart].FirstKey;
//            parents.Add((blockNum, promoteKey));
//        }
//
//        return parents;
//    }
//
//    private static byte[] BuildAnchor(
//        string keyExpression,
//        int keyLength,
//        int groupLen,
//        ushort maxKeys,
//        uint rootBlock,
//        uint eofBlock,
//        bool unique)
//    {
//        var anchor = new byte[NdxFormat.BlockSize];
//        BitConverter.GetBytes(rootBlock).CopyTo(anchor, 0);
//        BitConverter.GetBytes(eofBlock).CopyTo(anchor, 4);
//        BitConverter.GetBytes((ushort)keyLength).CopyTo(anchor, 12);
//        BitConverter.GetBytes(maxKeys).CopyTo(anchor, 14);
//        BitConverter.GetBytes((ushort)groupLen).CopyTo(anchor, 18);
//        anchor[23] = unique ? (byte)1 : (byte)0;
//        var exprBytes = Encoding.ASCII.GetBytes(keyExpression.ToLowerInvariant());
//        exprBytes.AsSpan(0, Math.Min(exprBytes.Length, NdxFormat.ExpressionLength))
//            .CopyTo(anchor.AsSpan(NdxFormat.ExpressionOffset));
//        return anchor;
//    }
//
//    private static byte[] BuildLeafPage(
//        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries,
//        int keyLength,
//        int groupLen,
//        ushort maxKeys)
//    {
//        _ = maxKeys;
//        var page = new byte[NdxFormat.BlockSize];
//        BitConverter.GetBytes((ushort)entries.Count).CopyTo(page, 0);
//
//        for (var i = 0; i < entries.Count; i++)
//        {
//            var offset = 4 + (i * groupLen);
//            BitConverter.GetBytes(0u).CopyTo(page, offset);
//            BitConverter.GetBytes((uint)entries[i].Recno).CopyTo(page, offset + 4);
//            entries[i].Key.Span.CopyTo(page.AsSpan(offset + 8, keyLength));
//        }
//
//        return page;
//    }
//
//    private static byte[] BuildInternalPage(
//        IReadOnlyList<uint> childBlocks,
//        IReadOnlyList<byte[]> separatorKeys,
//        int keyLength,
//        int groupLen,
//        ushort maxKeys)
//    {
//        _ = maxKeys;
//        var page = new byte[NdxFormat.BlockSize];
//        BitConverter.GetBytes((ushort)separatorKeys.Count).CopyTo(page, 0);
//
//        for (var i = 0; i < childBlocks.Count; i++)
//        {
//            var offset = 4 + (i * groupLen);
//            BitConverter.GetBytes(childBlocks[i]).CopyTo(page, offset);
//            BitConverter.GetBytes(0u).CopyTo(page, offset + 4);
//            if (i < separatorKeys.Count)
//            {
//                separatorKeys[i].AsSpan(0, Math.Min(separatorKeys[i].Length, keyLength))
//                    .CopyTo(page.AsSpan(offset + 8, keyLength));
//            }
//        }
//
//        return page;
//    }
//}
//
///// <summary>
///// dBASE NDX index with read btree traversal and ponytail in-memory rewrite for mutations.
///// </summary>
//public sealed class NdxFile : IWritableIndexHandle
//{
//    private readonly string _path;
//    private readonly ILockManager? _lockManager;
//    private readonly object _syncRoot = new();
//    private readonly List<(byte[] Key, int Recno)> _entries;
//    private readonly string _keyExpressionString;
//    private readonly bool _writable;
//    private List<int>? _recordOrderCache;
//
//    public string Name { get; }
//    public bool Unique { get; }
//    public bool Descending { get; }
//    public int KeyLength { get; }
//
//    private NdxFile(
//        string path,
//        List<(byte[] Key, int Recno)> entries,
//        string keyExpression,
//        int keyLength,
//        bool unique,
//        ILockManager? lockManager,
//        bool writable)
//    {
//        _path = path;
//        _entries = entries;
//        _keyExpressionString = keyExpression;
//        KeyLength = keyLength;
//        Unique = unique;
//        Descending = false;
//        _lockManager = lockManager;
//        _writable = writable;
//        Name = Path.GetFileNameWithoutExtension(path);
//    }
//
//    public static NdxFile CreateNew(
//        string filePath,
//        Expr keyExpression,
//        int keyLength,
//        bool isUnique,
//        ILockManager lockManager,
//        LockingStrategy lockingStrategy)
//    {
//        _ = lockingStrategy;
//        var exprStr = DbfTotalExpressions.ToPlainString(keyExpression);
//        NdxBuilder.Write(filePath, exprStr, keyLength, [], isUnique);
//        return OpenExisting(filePath, lockManager, lockingStrategy);
//    }
//
//    public static NdxFile OpenExisting(string path, ILockManager? lockManager, LockingStrategy lockingStrategy)
//    {
//        _ = lockingStrategy;
//        var entries = LoadEntriesFromDisk(path);
//        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
//        var anchorBuf = new byte[NdxFormat.BlockSize];
//        fs.ReadExactly(anchorBuf);
//        var anchor = NdxAnchor.Parse(anchorBuf);
//        return new NdxFile(
//            path,
//            entries,
//            anchor.Expression,
//            anchor.KeyLength,
//            anchor.Unique,
//            lockManager,
//            writable: lockManager != null);
//    }
//
//    internal static List<(byte[] Key, int Recno)> LoadEntriesFromDisk(string path)
//    {
//        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
//        var anchorBuf = new byte[NdxFormat.BlockSize];
//        fs.ReadExactly(anchorBuf);
//        var anchor = NdxAnchor.Parse(anchorBuf);
//        if (anchor.RootBlock == 0)
//        {
//            return [];
//        }
//
//        var groupLen = anchor.GroupLength > 0 ? anchor.GroupLength : NdxFormat.GroupLength(anchor.KeyLength);
//        var result = new List<(byte[] Key, int Recno)>();
//        EnumerateBlock(fs, anchor.RootBlock, anchor.KeyLength, groupLen, result);
//        return result;
//    }
//
//    private static void EnumerateBlock(
//        FileStream fs,
//        uint blockNumber,
//        int keyLength,
//        int groupLen,
//        List<(byte[] Key, int Recno)> result)
//    {
//        var page = ReadPage(fs, blockNumber, groupLen);
//        if (page.ItemCount == 0)
//        {
//            return;
//        }
//
//        if (page.IsLeaf)
//        {
//            for (var i = 0; i < page.ItemCount; i++)
//            {
//                result.Add((page.GetKey(i, keyLength).ToArray(), (int)page.GetRecordNumber(i)));
//            }
//
//            return;
//        }
//
//        for (var i = 0; i <= page.ItemCount; i++)
//        {
//            var child = page.GetChildBlock(i);
//            if (child != 0)
//            {
//                EnumerateBlock(fs, child, keyLength, groupLen, result);
//            }
//        }
//    }
//
//    private static NdxPage ReadPage(FileStream fs, uint blockNumber, int groupLen)
//    {
//        var buffer = new byte[NdxFormat.BlockSize];
//        fs.Position = NdxFormat.BlockOffset(blockNumber);
//        fs.ReadExactly(buffer);
//        return new NdxPage(buffer, groupLen);
//    }
//
//    public string GetKeyExpressionString() => _keyExpressionString;
//
//    public int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
//    {
//        AcquireSerializableReadLock(keyBytes, context);
//        lock (_syncRoot)
//        {
//            return SeekInMemory(keyBytes, soft);
//        }
//    }
//
//    public void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
//    {
//        EnsureWritable();
//        AcquireSerializableWriteLock(keyBytes, context);
//        lock (_syncRoot)
//        {
//            InsertUnderLock(keyBytes, recno);
//            FlushToDisk();
//        }
//    }
//
//    public void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context)
//    {
//        EnsureWritable();
//        if (entries.Count == 0)
//        {
//            return;
//        }
//
//        foreach (var (key, _) in entries)
//        {
//            AcquireSerializableWriteLock(key.Span, context);
//        }
//
//        lock (_syncRoot)
//        {
//            foreach (var (key, recno) in entries)
//            {
//                InsertUnderLock(key.Span, recno);
//            }
//
//            FlushToDisk();
//        }
//    }
//
//    public void Delete(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
//    {
//        EnsureWritable();
//        AcquireSerializableWriteLock(keyBytes, context);
//        lock (_syncRoot)
//        {
//            var idx = FindKeyIndex(keyBytes, out var exact);
//            if (!exact)
//            {
//                return;
//            }
//
//            for (var i = idx; i < _entries.Count; i++)
//            {
//                if (_entries[i].Key.AsSpan().SequenceCompareTo(keyBytes) != 0)
//                {
//                    break;
//                }
//
//                if (_entries[i].Recno == recno)
//                {
//                    _entries.RemoveAt(i);
//                    InvalidateCache();
//                    FlushToDisk();
//                    return;
//                }
//            }
//        }
//    }
//
//    public void Update(ReadOnlySpan<byte> oldKey, ReadOnlySpan<byte> newKey, int recno, TransactionContext? context)
//    {
//        Delete(oldKey, recno, context);
//        Insert(newKey, recno, context);
//    }
//
//    public void Rebuild(Func<IEnumerable<(ReadOnlyMemory<byte> key, int recno)>> producer)
//    {
//        EnsureWritable();
//        lock (_syncRoot)
//        {
//            _entries.Clear();
//            foreach (var (key, recno) in producer())
//            {
//                InsertUnderLock(key.Span, recno);
//            }
//
//            InvalidateCache();
//            FlushToDisk();
//        }
//    }
//
//    public int GetFirstRecordNumber()
//    {
//        lock (_syncRoot)
//        {
//            return _entries.Count > 0 ? _entries[0].Recno : 0;
//        }
//    }
//
//    public int GetLastRecordNumber()
//    {
//        lock (_syncRoot)
//        {
//            return _entries.Count > 0 ? _entries[^1].Recno : 0;
//        }
//    }
//
//    public IEnumerable<int> GetAllRecordNumbers()
//    {
//        lock (_syncRoot)
//        {
//            _recordOrderCache ??= _entries.Select(e => e.Recno).ToList();
//            return _recordOrderCache.ToList();
//        }
//    }
//
//    public (int recno, uint pageNum, int indexInLeaf) GetFirstRecordPosition(TransactionContext? context)
//    {
//        lock (_syncRoot)
//        {
//            if (_entries.Count == 0)
//            {
//                return (0, 0, 0);
//            }
//
//            if (context is { IsolationLevel: IsolationLevel.Serializable })
//            {
//                AcquireSerializableReadLock(_entries[0].Key, context);
//            }
//
//            return (_entries[0].Recno, 1, 0);
//        }
//    }
//
//    public (int recno, uint pageNum, int indexInLeaf) GetLastRecordPosition(TransactionContext? context)
//    {
//        lock (_syncRoot)
//        {
//            if (_entries.Count == 0)
//            {
//                return (0, 0, 0);
//            }
//
//            if (context is { IsolationLevel: IsolationLevel.Serializable })
//            {
//                AcquireSerializableReadLock(_entries[^1].Key, context);
//            }
//
//            var last = _entries.Count - 1;
//            return (_entries[last].Recno, 1, last);
//        }
//    }
//
//    public (int recno, uint newPageNum, int newIndexInLeaf) GetNextRecord(
//        uint currentPageNum, int currentIndexInLeaf, int direction, TransactionContext? context)
//    {
//        _ = currentPageNum;
//        lock (_syncRoot)
//        {
//            var newIndex = currentIndexInLeaf + direction;
//            if (newIndex < 0 || newIndex >= _entries.Count)
//            {
//                return (0, 0, 0);
//            }
//
//            if (context is { IsolationLevel: IsolationLevel.Serializable })
//            {
//                AcquireSerializableReadLock(_entries[newIndex].Key, context);
//            }
//
//            return (_entries[newIndex].Recno, 1, newIndex);
//        }
//    }
//
//    public void InvalidateCache()
//    {
//        lock (_syncRoot)
//        {
//            _recordOrderCache = null;
//        }
//    }
//
//    public void Dispose()
//    {
//    }
//
//    private void EnsureWritable()
//    {
//        if (!_writable)
//        {
//            throw new NotSupportedException("L'indice NDX è aperto in sola lettura.");
//        }
//    }
//
//    private int SeekInMemory(ReadOnlySpan<byte> keyBytes, bool soft)
//    {
//        if (_entries.Count == 0)
//        {
//            return -1;
//        }
//
//        var idx = FindKeyIndex(keyBytes, out var exact);
//        if (exact)
//        {
//            return _entries[idx].Recno;
//        }
//
//        return soft && idx < _entries.Count ? _entries[idx].Recno : -1;
//    }
//
//    private void InsertUnderLock(ReadOnlySpan<byte> keyBytes, int recno)
//    {
//        var idx = FindKeyIndex(keyBytes, out var exact);
//        if (Unique && exact)
//        {
//            throw new InvalidOperationException($"Chiave duplicata nell'indice '{Name}'.");
//        }
//
//        _entries.Insert(idx, (keyBytes.ToArray(), recno));
//        InvalidateCache();
//    }
//
//    private int FindKeyIndex(ReadOnlySpan<byte> keyBytes, out bool exactMatch)
//    {
//        exactMatch = false;
//        var lo = 0;
//        var hi = _entries.Count - 1;
//        var insertAt = _entries.Count;
//
//        while (lo <= hi)
//        {
//            var mid = (lo + hi) >> 1;
//            var cmp = keyBytes.SequenceCompareTo(_entries[mid].Key);
//            if (cmp == 0)
//            {
//                exactMatch = true;
//                return mid;
//            }
//
//            if (cmp < 0)
//            {
//                insertAt = mid;
//                hi = mid - 1;
//            }
//            else
//            {
//                lo = mid + 1;
//            }
//        }
//
//        return insertAt;
//    }
//
//    private void FlushToDisk()
//    {
//        var payload = _entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList();
//        NdxBuilder.Write(_path, _keyExpressionString, KeyLength, payload, Unique);
//    }
//
//    private void AcquireSerializableReadLock(ReadOnlySpan<byte> keyBytes, TransactionContext? context)
//    {
//        if (context is not { IsolationLevel: IsolationLevel.Serializable } || _lockManager == null)
//        {
//            return;
//        }
//
//        var lockKey = LockManagerBase.GetKeyLockKey(Name, keyBytes.ToArray());
//        _lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{Name}'");
//        context.ReadLockedKeys.Add(lockKey);
//    }
//
//    private void AcquireSerializableWriteLock(ReadOnlySpan<byte> keyBytes, TransactionContext? context)
//    {
//        if (context is not { IsolationLevel: IsolationLevel.Serializable } || _lockManager == null)
//        {
//            return;
//        }
//
//        var lockKey = LockManagerBase.GetKeyLockKey(Name, keyBytes.ToArray());
//        _lockManager.AcquireWriteLock(lockKey, 5000, $"chiave di indice '{Name}'");
//        context.WriteLockedKeys.Add(lockKey);
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#define NDX_MAX_ENTRIES 4096 /* ponytail: C# List grows; ceiling 4096; upgrade: realloc unbounded */
#define NDX_MAX_PAGES 512
#define NDX_MAX_DEPTH 16

int NdxFormat_GroupLength(int keyLength)
{
    if (keyLength < 0) keyLength = 0;
    return 8 + ((keyLength + 3) / 4) * 4;
}

uint16_t NdxFormat_MaxKeysPerPage(int keyLength)
{
    int g = NdxFormat_GroupLength(keyLength);
    if (g <= 0) return 0;
    return (uint16_t)((NdxFormat_BlockSize - 4) / g);
}

static void ndx_le16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t ndx_ru16(const uint8_t* p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static void ndx_le32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t ndx_ru32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* C# ReadOnlySpan.SequenceCompareTo */
static int ndx_keycmp(const uint8_t* a, int alen, const uint8_t* b, int blen)
{
    int n = alen < blen ? alen : blen;
    int c = n > 0 ? memcmp(a, b, (size_t)n) : 0;
    if (c != 0) return c;
    return alen - blen;
}

typedef struct {
    uint8_t* Key;
    int KeyLen;
    int Recno;
} NdxEntry;

typedef struct {
    uint32_t RootBlock;
    uint32_t EofBlock;
    uint16_t KeyLength;
    uint16_t MaxKeys;
    uint16_t GroupLength;
    int Unique;
    char Expression[NdxFormat_ExpressionLength + 1];
} NdxAnchor;

static NdxAnchor ndx_parse_anchor(const uint8_t* block)
{
    NdxAnchor a;
    const uint8_t* expr;
    int exprLen, i;
    memset(&a, 0, sizeof(a));
    a.RootBlock = ndx_ru32(block);
    a.EofBlock = ndx_ru32(block + 4);
    a.KeyLength = ndx_ru16(block + 12);
    a.MaxKeys = ndx_ru16(block + 14);
    a.GroupLength = ndx_ru16(block + 18);
    a.Unique = block[23] != 0;
    expr = block + NdxFormat_ExpressionOffset;
    exprLen = NdxFormat_ExpressionLength;
    for (i = 0; i < NdxFormat_ExpressionLength; i++) {
        if (expr[i] == 0) {
            exprLen = i;
            break;
        }
    }
    if (exprLen > 0)
        memcpy(a.Expression, expr, (size_t)exprLen);
    a.Expression[exprLen] = 0;
    return a;
}

static void ndx_build_anchor(
    uint8_t* anchor,
    const char* keyExpression,
    int keyLength,
    int groupLen,
    uint16_t maxKeys,
    uint32_t rootBlock,
    uint32_t eofBlock,
    int unique)
{
    int i, n;
    memset(anchor, 0, NdxFormat_BlockSize);
    ndx_le32(anchor, rootBlock);
    ndx_le32(anchor + 4, eofBlock);
    ndx_le16(anchor + 12, (uint16_t)keyLength);
    ndx_le16(anchor + 14, maxKeys);
    ndx_le16(anchor + 18, (uint16_t)groupLen);
    anchor[23] = unique ? (uint8_t)1 : (uint8_t)0;
    if (keyExpression) {
        n = (int)strlen(keyExpression);
        if (n > NdxFormat_ExpressionLength) n = NdxFormat_ExpressionLength;
        for (i = 0; i < n; i++)
            anchor[NdxFormat_ExpressionOffset + i] = (uint8_t)tolower((unsigned char)keyExpression[i]);
    }
}

static void ndx_build_leaf(
    uint8_t* page,
    const NdxEntry* entries,
    int count,
    int keyLength,
    int groupLen)
{
    int i, copy;
    memset(page, 0, NdxFormat_BlockSize);
    ndx_le16(page, (uint16_t)count);
    for (i = 0; i < count; i++) {
        int offset = 4 + (i * groupLen);
        ndx_le32(page + offset, 0);
        ndx_le32(page + offset + 4, (uint32_t)entries[i].Recno);
        copy = entries[i].KeyLen < keyLength ? entries[i].KeyLen : keyLength;
        if (copy > 0 && entries[i].Key)
            memcpy(page + offset + 8, entries[i].Key, (size_t)copy);
    }
}

static void ndx_build_internal(
    uint8_t* page,
    const uint32_t* childBlocks,
    int childCount,
    uint8_t** separators,
    int sepCount,
    const int* sepLens,
    int keyLength,
    int groupLen)
{
    int i, copy;
    memset(page, 0, NdxFormat_BlockSize);
    ndx_le16(page, (uint16_t)sepCount);
    for (i = 0; i < childCount; i++) {
        int offset = 4 + (i * groupLen);
        ndx_le32(page + offset, childBlocks[i]);
        ndx_le32(page + offset + 4, 0);
        if (i < sepCount && separators[i]) {
            copy = sepLens[i] < keyLength ? sepLens[i] : keyLength;
            if (copy > 0)
                memcpy(page + offset + 8, separators[i], (size_t)copy);
        }
    }
}

static void ndx_sort_entries(NdxEntry* e, int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        NdxEntry t = e[i];
        j = i;
        while (j > 0 && ndx_keycmp(e[j - 1].Key, e[j - 1].KeyLen, t.Key, t.KeyLen) > 0) {
            e[j] = e[j - 1];
            j--;
        }
        e[j] = t;
    }
}

typedef struct {
    uint32_t BlockNum;
    uint8_t* FirstKey;
    int FirstKeyLen;
} NdxLevelItem;

static int ndx_builder_write(
    const char* path,
    const char* keyExpression,
    int keyLength,
    const NdxEntry* entries,
    int nEntries,
    int unique)
{
    int groupLen = NdxFormat_GroupLength(keyLength);
    uint16_t maxKeys = NdxFormat_MaxKeysPerPage(keyLength);
    uint8_t pages[NDX_MAX_PAGES][NdxFormat_BlockSize];
    int nPages = 0;
    uint32_t rootBlock;
    uint8_t anchor[NdxFormat_BlockSize];
    NdxEntry* sorted = NULL;
    FILE* fs;
    int p;

    if (!path || keyLength <= 0 || groupLen <= 0) return GMB_DB_ARG;

    if (nEntries > 0) {
        sorted = (NdxEntry*)malloc((size_t)nEntries * sizeof(NdxEntry));
        if (!sorted) return GMB_DB_ERR;
        memcpy(sorted, entries, (size_t)nEntries * sizeof(NdxEntry));
        ndx_sort_entries(sorted, nEntries);
    }

    if (nEntries <= 0) {
        ndx_build_leaf(pages[0], NULL, 0, keyLength, groupLen);
        nPages = 1;
        rootBlock = 1;
    } else {
        NdxLevelItem leaves[NDX_MAX_PAGES];
        int nLeaves = 0;
        int idx = 0;
        NdxLevelItem level[NDX_MAX_PAGES];
        int nLevel;
        while (idx < nEntries) {
            int take = nEntries - idx;
            if (take > (int)maxKeys) take = (int)maxKeys;
            if (nPages >= NDX_MAX_PAGES) {
                free(sorted);
                return GMB_DB_ERR;
            }
            ndx_build_leaf(pages[nPages], sorted + idx, take, keyLength, groupLen);
            nPages++;
            leaves[nLeaves].BlockNum = (uint32_t)nPages;
            leaves[nLeaves].FirstKey = sorted[idx].Key;
            leaves[nLeaves].FirstKeyLen = sorted[idx].KeyLen;
            nLeaves++;
            idx += take;
        }
        nLevel = nLeaves;
        memcpy(level, leaves, (size_t)nLeaves * sizeof(NdxLevelItem));
        while (nLevel > 1) {
            NdxLevelItem parents[NDX_MAX_PAGES];
            int nParents = 0;
            int index = 0;
            while (index < nLevel) {
                uint32_t childBlocks[256];
                uint8_t* seps[256];
                int sepLens[256];
                int nChild = 0;
                int nSep = 0;
                int groupStart = index;
                childBlocks[nChild++] = level[index].BlockNum;
                index++;
                while (index < nLevel && nSep < (int)maxKeys && nChild < 256) {
                    seps[nSep] = level[index].FirstKey;
                    sepLens[nSep] = level[index].FirstKeyLen;
                    nSep++;
                    childBlocks[nChild++] = level[index].BlockNum;
                    index++;
                }
                if (nPages >= NDX_MAX_PAGES) {
                    free(sorted);
                    return GMB_DB_ERR;
                }
                ndx_build_internal(pages[nPages], childBlocks, nChild, seps, nSep, sepLens, keyLength, groupLen);
                nPages++;
                parents[nParents].BlockNum = (uint32_t)nPages;
                if (nSep > 0) {
                    parents[nParents].FirstKey = seps[0];
                    parents[nParents].FirstKeyLen = sepLens[0];
                } else {
                    parents[nParents].FirstKey = level[groupStart].FirstKey;
                    parents[nParents].FirstKeyLen = level[groupStart].FirstKeyLen;
                }
                nParents++;
            }
            nLevel = nParents;
            memcpy(level, parents, (size_t)nParents * sizeof(NdxLevelItem));
        }
        rootBlock = level[0].BlockNum;
    }

    ndx_build_anchor(anchor, keyExpression, keyLength, groupLen, maxKeys, rootBlock, (uint32_t)(nPages + 1), unique);

    {
        char dirbuf[PATH_MAX];
        char* slash;
        snprintf(dirbuf, sizeof(dirbuf), "%s", path);
        slash = strrchr(dirbuf, '/');
        if (slash && slash != dirbuf) {
            *slash = 0;
            if (mkdir(dirbuf, 0755) != 0 && errno != EEXIST) {
                free(sorted);
                return GMB_DB_ERR;
            }
        }
    }

    fs = fopen(path, "wb");
    if (!fs) {
        free(sorted);
        return GMB_DB_ERR;
    }
    if (fwrite(anchor, 1, NdxFormat_BlockSize, fs) != NdxFormat_BlockSize) {
        fclose(fs);
        free(sorted);
        return GMB_DB_ERR;
    }
    for (p = 0; p < nPages; p++) {
        if (fwrite(pages[p], 1, NdxFormat_BlockSize, fs) != NdxFormat_BlockSize) {
            fclose(fs);
            free(sorted);
            return GMB_DB_ERR;
        }
    }
    fclose(fs);
    free(sorted);
    return GMB_DB_OK;
}

static int ndx_read_page(FILE* fs, uint32_t blockNumber, uint8_t* buffer)
{
    if (fseek(fs, (long)blockNumber * NdxFormat_BlockSize, SEEK_SET) != 0) return -1;
    if (fread(buffer, 1, NdxFormat_BlockSize, fs) != NdxFormat_BlockSize) return -1;
    return 0;
}

static void ndx_enumerate(
    FILE* fs,
    uint32_t blockNumber,
    int keyLength,
    int groupLen,
    NdxEntry* result,
    int* nResult,
    int depth)
{
    uint8_t page[NdxFormat_BlockSize];
    uint16_t itemCount;
    uint32_t child0;
    int i, isLeaf;
    if (depth > NDX_MAX_DEPTH || *nResult >= NDX_MAX_ENTRIES) return;
    if (ndx_read_page(fs, blockNumber, page) != 0) return;
    itemCount = ndx_ru16(page);
    if (itemCount == 0) return;
    child0 = ndx_ru32(page + 4);
    isLeaf = child0 == 0;
    if (isLeaf) {
        for (i = 0; i < (int)itemCount && *nResult < NDX_MAX_ENTRIES; i++) {
            int offset = 4 + (i * groupLen);
            NdxEntry* e = &result[*nResult];
            e->KeyLen = keyLength;
            e->Key = (uint8_t*)malloc((size_t)keyLength);
            if (!e->Key) return;
            memcpy(e->Key, page + offset + 8, (size_t)keyLength);
            e->Recno = (int)ndx_ru32(page + offset + 4);
            (*nResult)++;
        }
        return;
    }
    for (i = 0; i <= (int)itemCount; i++) {
        uint32_t child = ndx_ru32(page + 4 + (i * groupLen));
        if (child != 0)
            ndx_enumerate(fs, child, keyLength, groupLen, result, nResult, depth + 1);
    }
}

struct NdxFile {
    char Path[PATH_MAX];
    char Name[256];
    char KeyExpression[NdxFormat_ExpressionLength + 1];
    int KeyLength;
    int Unique;
    int Descending;
    int Writable;
    int nEntries;
    NdxEntry Entries[NDX_MAX_ENTRIES];
    pthread_mutex_t Lock;
    int LockInited;
};

static void ndx_free_entries(NdxFile* self)
{
    int i;
    for (i = 0; i < self->nEntries; i++) {
        free(self->Entries[i].Key);
        self->Entries[i].Key = NULL;
    }
    self->nEntries = 0;
}

static int ndx_load_entries(const char* path, NdxEntry* out, int* nOut)
{
    FILE* fs;
    uint8_t anchorBuf[NdxFormat_BlockSize];
    NdxAnchor anchor;
    int groupLen;
    *nOut = 0;
    fs = fopen(path, "rb");
    if (!fs) return GMB_DB_ERR;
    if (fread(anchorBuf, 1, NdxFormat_BlockSize, fs) != NdxFormat_BlockSize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    anchor = ndx_parse_anchor(anchorBuf);
    if (anchor.RootBlock == 0) {
        fclose(fs);
        return GMB_DB_OK;
    }
    groupLen = anchor.GroupLength > 0 ? (int)anchor.GroupLength : NdxFormat_GroupLength(anchor.KeyLength);
    ndx_enumerate(fs, anchor.RootBlock, (int)anchor.KeyLength, groupLen, out, nOut, 0);
    fclose(fs);
    return GMB_DB_OK;
}

static NdxFile* ndx_open_existing(const char* path, int writable)
{
    NdxFile* self;
    FILE* fs;
    uint8_t anchorBuf[NdxFormat_BlockSize];
    NdxAnchor anchor;
    const char* slash;
    const char* base;
    const char* dot;
    size_t n;
    if (!path) return NULL;
    fs = fopen(path, "rb");
    if (!fs) return NULL;
    if (fread(anchorBuf, 1, NdxFormat_BlockSize, fs) != NdxFormat_BlockSize) {
        fclose(fs);
        return NULL;
    }
    fclose(fs);
    anchor = ndx_parse_anchor(anchorBuf);
    self = (NdxFile*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->Path, sizeof(self->Path), "%s", path);
    snprintf(self->KeyExpression, sizeof(self->KeyExpression), "%s", anchor.Expression);
    self->KeyLength = (int)anchor.KeyLength;
    self->Unique = anchor.Unique;
    self->Descending = 0;
    self->Writable = writable;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    n = dot ? (size_t)(dot - base) : strlen(base);
    if (n >= sizeof(self->Name)) n = sizeof(self->Name) - 1;
    memcpy(self->Name, base, n);
    self->Name[n] = 0;
    if (ndx_load_entries(path, self->Entries, &self->nEntries) != GMB_DB_OK) {
        NdxFile_Dispose(self);
        return NULL;
    }
    if (pthread_mutex_init(&self->Lock, NULL) == 0)
        self->LockInited = 1;
    return self;
}

NdxFile* NdxFile_CreateNew(const char* filePath, const char* keyExpression, int keyLength, int isUnique)
{
    if (!filePath || keyLength <= 0) return NULL;
    if (ndx_builder_write(filePath, keyExpression ? keyExpression : "", keyLength, NULL, 0, isUnique) != GMB_DB_OK)
        return NULL;
    return ndx_open_existing(filePath, 1);
}

NdxFile* NdxFile_OpenExisting(const char* path, int writable)
{
    return ndx_open_existing(path, writable);
}

void NdxFile_Dispose(NdxFile* self)
{
    if (!self) return;
    ndx_free_entries(self);
    if (self->LockInited) pthread_mutex_destroy(&self->Lock);
    free(self);
}

const char* NdxFile_GetKeyExpressionString(const NdxFile* self)
{
    return self ? self->KeyExpression : NULL;
}

int NdxFile_get_KeyLength(const NdxFile* self)
{
    return self ? self->KeyLength : 0;
}

int NdxFile_get_Unique(const NdxFile* self)
{
    return self ? self->Unique : 0;
}

const char* NdxFile_get_Name(const NdxFile* self)
{
    return self ? self->Name : "";
}

int NdxFile_get_EntryCount(const NdxFile* self)
{
    return self ? self->nEntries : 0;
}

int NdxFile_get_RecnoAt(const NdxFile* self, int index)
{
    if (!self || index < 0 || index >= self->nEntries) return 0;
    return self->Entries[index].Recno;
}

static int ndx_find_key(NdxFile* self, const uint8_t* keyBytes, int keyLen, int* exactMatch)
{
    int lo = 0;
    int hi = self->nEntries - 1;
    int insertAt = self->nEntries;
    *exactMatch = 0;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = ndx_keycmp(keyBytes, keyLen, self->Entries[mid].Key, self->Entries[mid].KeyLen);
        if (cmp == 0) {
            *exactMatch = 1;
            return mid;
        }
        if (cmp < 0) {
            insertAt = mid;
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return insertAt;
}

static int ndx_flush(NdxFile* self)
{
    return ndx_builder_write(self->Path, self->KeyExpression, self->KeyLength, self->Entries, self->nEntries, self->Unique);
}

int NdxFile_Seek(NdxFile* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    int exact, idx, rec;
    if (!self || !keyBytes || keyLen < 0) return -1;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->nEntries == 0) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return -1;
    }
    idx = ndx_find_key(self, keyBytes, keyLen, &exact);
    if (exact)
        rec = self->Entries[idx].Recno;
    else
        rec = (soft && idx < self->nEntries) ? self->Entries[idx].Recno : -1;
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rec;
}

int NdxFile_Insert(NdxFile* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, rc;
    uint8_t* copy;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes || keyLen < 0) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->nEntries >= NDX_MAX_ENTRIES) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    idx = ndx_find_key(self, keyBytes, keyLen, &exact);
    if (self->Unique && exact) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    copy = (uint8_t*)malloc((size_t)keyLen);
    if (!copy) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    if (keyLen > 0) memcpy(copy, keyBytes, (size_t)keyLen);
    memmove(&self->Entries[idx + 1], &self->Entries[idx], (size_t)(self->nEntries - idx) * sizeof(NdxEntry));
    self->Entries[idx].Key = copy;
    self->Entries[idx].KeyLen = keyLen;
    self->Entries[idx].Recno = recno;
    self->nEntries++;
    rc = ndx_flush(self);
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rc;
}

int NdxFile_Delete(NdxFile* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, i;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    idx = ndx_find_key(self, keyBytes, keyLen, &exact);
    if (!exact) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_OK;
    }
    for (i = idx; i < self->nEntries; i++) {
        if (ndx_keycmp(self->Entries[i].Key, self->Entries[i].KeyLen, keyBytes, keyLen) != 0)
            break;
        if (self->Entries[i].Recno == recno) {
            free(self->Entries[i].Key);
            memmove(&self->Entries[i], &self->Entries[i + 1],
                    (size_t)(self->nEntries - i - 1) * sizeof(NdxEntry));
            self->nEntries--;
            ndx_flush(self);
            break;
        }
    }
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return GMB_DB_OK;
}

int NdxFile_GetFirstRecordNumber(NdxFile* self)
{
    int rec;
    if (!self) return 0;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    rec = self->nEntries > 0 ? self->Entries[0].Recno : 0;
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rec;
}

int NdxFile_GetLastRecordNumber(NdxFile* self)
{
    int rec;
    if (!self) return 0;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    rec = self->nEntries > 0 ? self->Entries[self->nEntries - 1].Recno : 0;
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rec;
}

int NdxBuilder_Write(const char* path, const char* keyExpression, int keyLength, int unique,
                     const uint8_t* const* keys, const int* keyLens, const int* recnos, int n)
{
    NdxEntry* e = NULL;
    int i, rc;
    if (n < 0) return GMB_DB_ARG;
    if (n > 0) {
        if (!keys || !recnos) return GMB_DB_ARG;
        e = (NdxEntry*)calloc((size_t)n, sizeof(*e));
        if (!e) return GMB_DB_ERR;
        for (i = 0; i < n; i++) {
            e[i].Key = (uint8_t*)keys[i];
            e[i].KeyLen = keyLens ? keyLens[i] : keyLength;
            e[i].Recno = recnos[i];
        }
    }
    rc = ndx_builder_write(path, keyExpression ? keyExpression : "", keyLength, e, n, unique);
    free(e);
    return rc;
}

static void ndx_sc_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int NdxFile_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ndxsc_XXXXXX";
    char path[PATH_MAX];
    char uniq[PATH_MAX];
    NdxFile *idx, *reopen;
    uint8_t key[10];
    uint8_t head[64];
    FILE* f;
    long flen;
    int i, rec;
    uint16_t maxKeys;

    if (NdxFormat_GroupLength(10) != 20) return -1;
    maxKeys = NdxFormat_MaxKeysPerPage(10);
    if (maxKeys != 25) return -1;

    if (!mkdtemp(dir)) return -2;
    snprintf(path, sizeof(path), "%s/t.ndx", dir);

    idx = NdxFile_CreateNew(path, "NAME", 10, 0);
    if (!idx) return -3;
    if (NdxFile_get_KeyLength(idx) != 10 || NdxFile_get_Unique(idx) != 0) {
        NdxFile_Dispose(idx);
        return -4;
    }
    if (!NdxFile_GetKeyExpressionString(idx) || strcmp(NdxFile_GetKeyExpressionString(idx), "name") != 0) {
        NdxFile_Dispose(idx);
        return -5;
    }

    f = fopen(path, "rb");
    if (!f) {
        NdxFile_Dispose(idx);
        return -6;
    }
    if (fread(head, 1, 32, f) != 32) {
        fclose(f);
        NdxFile_Dispose(idx);
        return -7;
    }
    fseek(f, 0, SEEK_END);
    flen = ftell(f);
    fclose(f);
    /* empty: root=1 LE, eof=2, keyLen=10, maxKeys=25, group=20, unique=0, expr "name" */
    if (flen != 1024
        || ndx_ru32(head) != 1
        || ndx_ru32(head + 4) != 2
        || ndx_ru16(head + 12) != 10
        || ndx_ru16(head + 14) != 25
        || ndx_ru16(head + 18) != 20
        || head[23] != 0) {
        NdxFile_Dispose(idx);
        return -8;
    }

    if (NdxFile_GetFirstRecordNumber(idx) != 0 || NdxFile_GetLastRecordNumber(idx) != 0) {
        NdxFile_Dispose(idx);
        return -9;
    }

    ndx_sc_key(key, 1);
    if (NdxFile_Insert(idx, key, 10, 101) != GMB_DB_OK) {
        NdxFile_Dispose(idx);
        return -10;
    }
    ndx_sc_key(key, 0);
    if (NdxFile_Insert(idx, key, 10, 100) != GMB_DB_OK) {
        NdxFile_Dispose(idx);
        return -11;
    }
    ndx_sc_key(key, 0);
    if (NdxFile_Seek(idx, key, 10, 0) != 100) {
        NdxFile_Dispose(idx);
        return -12;
    }
    ndx_sc_key(key, 1);
    if (NdxFile_Seek(idx, key, 10, 0) != 101) {
        NdxFile_Dispose(idx);
        return -13;
    }
    if (NdxFile_GetFirstRecordNumber(idx) != 100 || NdxFile_GetLastRecordNumber(idx) != 101) {
        NdxFile_Dispose(idx);
        return -14;
    }

    for (i = 2; i < 40; i++) {
        ndx_sc_key(key, i);
        if (NdxFile_Insert(idx, key, 10, 100 + i) != GMB_DB_OK) {
            NdxFile_Dispose(idx);
            return -20 - i;
        }
    }
    for (i = 0; i < 40; i++) {
        ndx_sc_key(key, i);
        rec = NdxFile_Seek(idx, key, 10, 0);
        if (rec != 100 + i) {
            NdxFile_Dispose(idx);
            return -100 - i;
        }
    }

    ndx_sc_key(key, 7);
    if (NdxFile_Delete(idx, key, 10, 107) != GMB_DB_OK) {
        NdxFile_Dispose(idx);
        return -15;
    }
    ndx_sc_key(key, 7);
    if (NdxFile_Seek(idx, key, 10, 0) != -1) {
        NdxFile_Dispose(idx);
        return -16;
    }
    ndx_sc_key(key, 8);
    if (NdxFile_Seek(idx, key, 10, 0) != 108) {
        NdxFile_Dispose(idx);
        return -17;
    }

    /* soft seek: missing K007 → next key K008 */
    ndx_sc_key(key, 7);
    if (NdxFile_Seek(idx, key, 10, 1) != 108) {
        NdxFile_Dispose(idx);
        return -18;
    }

    NdxFile_Dispose(idx);
    reopen = NdxFile_OpenExisting(path, 0);
    if (!reopen) return -19;
    ndx_sc_key(key, 0);
    if (NdxFile_Seek(reopen, key, 10, 0) != 100) {
        NdxFile_Dispose(reopen);
        return -21;
    }
    ndx_sc_key(key, 39);
    if (NdxFile_Seek(reopen, key, 10, 0) != 139) {
        NdxFile_Dispose(reopen);
        return -22;
    }
    ndx_sc_key(key, 7);
    if (NdxFile_Seek(reopen, key, 10, 0) != -1) {
        NdxFile_Dispose(reopen);
        return -23;
    }
    if (NdxFile_Insert(reopen, key, 10, 1) != GMB_DB_ERR) {
        NdxFile_Dispose(reopen);
        return -24;
    }
    NdxFile_Dispose(reopen);

    snprintf(uniq, sizeof(uniq), "%s/u.ndx", dir);
    idx = NdxFile_CreateNew(uniq, "ID", 10, 1);
    if (!idx) return -25;
    ndx_sc_key(key, 0);
    if (NdxFile_Insert(idx, key, 10, 1) != GMB_DB_OK) {
        NdxFile_Dispose(idx);
        return -26;
    }
    if (NdxFile_Insert(idx, key, 10, 2) != GMB_DB_ERR) {
        NdxFile_Dispose(idx);
        return -27;
    }
    if (NdxFile_Seek(idx, key, 10, 0) != 1) {
        NdxFile_Dispose(idx);
        return -28;
    }
    NdxFile_Dispose(idx);

    unlink(path);
    unlink(uniq);
    rmdir(dir);
    return GMB_DB_OK;
}
