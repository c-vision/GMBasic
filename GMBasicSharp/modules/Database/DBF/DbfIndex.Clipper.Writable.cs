// FILE: Basic.DBF.DbfIndex.Clipper.Writable.cs

using System.Text;
using Basic.Databases;

namespace Basic.DBF;

/// <summary>
/// Writable Clipper 5.x NTX index (0x0006). ponytail: in-memory sorted entries + full rewrite on each mutation.
/// </summary>
public sealed class ClipperNtxWritable : IWritableIndexHandle
{
    private readonly string _path;
    private readonly ILockManager _lockManager;
    private readonly object _syncRoot = new();
    private readonly List<(byte[] Key, int Recno)> _entries;
    private readonly string _keyExpressionString;
    private List<int>? _recordOrderCache;

    public string Name { get; }
    public bool Unique { get; }
    public bool Descending { get; }
    public int KeyLength { get; }

    private ClipperNtxWritable(
        string path,
        List<(byte[] Key, int Recno)> entries,
        string keyExpression,
        int keyLength,
        bool unique,
        bool descending,
        ILockManager lockManager)
    {
        _path = path;
        _entries = entries;
        _keyExpressionString = keyExpression;
        KeyLength = keyLength;
        Unique = unique;
        Descending = descending;
        _lockManager = lockManager;
        Name = Path.GetFileNameWithoutExtension(path);
    }

    public static ClipperNtxWritable CreateNew(
        string filePath,
        Expr keyExpression,
        int keyLength,
        bool isUnique,
        ILockManager lockManager,
        LockingStrategy lockingStrategy)
    {
        _ = lockingStrategy;
        var exprStr = DbfTotalExpressions.ToPlainString(keyExpression);
        ClipperNtxBuilder.Write(filePath, exprStr, keyLength, [], isUnique);
        return OpenExisting(filePath, lockManager, lockingStrategy);
    }

    public static ClipperNtxWritable OpenExisting(
        string path,
        ILockManager lockManager,
        LockingStrategy lockingStrategy)
    {
        _ = lockingStrategy;
        using var reader = new ClipperNtxFile(path);
        var entries = reader.EnumerateEntries()
            .Select(e => (e.Key, e.Recno))
            .ToList();
        return new ClipperNtxWritable(
            path,
            entries,
            reader.GetKeyExpressionString(),
            reader.KeyLength,
            reader.Unique,
            reader.Descending,
            lockManager);
    }

    public string GetKeyExpressionString() => _keyExpressionString;

    public int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
    {
        if (context is { IsolationLevel: IsolationLevel.Serializable })
        {
            var lockKey = LockManagerBase.GetKeyLockKey(Name, keyBytes.ToArray());
            _lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{Name}'");
            context.ReadLockedKeys.Add(lockKey);
        }

        lock (_syncRoot)
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

            if (soft && idx < _entries.Count)
            {
                return _entries[idx].Recno;
            }

            return -1;
        }
    }

    public void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
    {
        AcquireSerializableWriteLock(keyBytes, context);
        lock (_syncRoot)
        {
            InsertUnderLock(keyBytes, recno);
            FlushToDisk();
        }
    }

    public void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context)
    {
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
        if (context is { IsolationLevel: IsolationLevel.Serializable } && _entries.Count > 0)
        {
            var lockKey = LockManagerBase.GetKeyLockKey(Name, _entries[0].Key);
            _lockManager.AcquireReadLock(lockKey, 5000, $"prima chiave dell'indice '{Name}'");
            context.ReadLockedKeys.Add(lockKey);
        }

        lock (_syncRoot)
        {
            return _entries.Count == 0 ? (0, 0u, 0) : (_entries[0].Recno, 1u, 0);
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
                var lockKey = LockManagerBase.GetKeyLockKey(Name, _entries[^1].Key);
                _lockManager.AcquireReadLock(lockKey, 5000, $"ultima chiave dell'indice '{Name}'");
                context.ReadLockedKeys.Add(lockKey);
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
                var lockKey = LockManagerBase.GetKeyLockKey(Name, _entries[newIndex].Key);
                _lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{Name}' durante la scansione");
                context.ReadLockedKeys.Add(lockKey);
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

    private void InsertUnderLock(ReadOnlySpan<byte> keyBytes, int recno)
    {
        var idx = FindKeyIndex(keyBytes, out var exact);
        if (Unique && exact)
        {
            throw new InvalidOperationException($"Chiave duplicata nell'indice '{Name}'.");
        }

        var keyCopy = keyBytes.ToArray();
        _entries.Insert(idx, (keyCopy, recno));
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
        var payload = _entries
            .Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno))
            .ToList();
        ClipperNtxBuilder.Write(_path, _keyExpressionString, KeyLength, payload, Unique, Descending);
    }

    private void AcquireSerializableWriteLock(ReadOnlySpan<byte> keyBytes, TransactionContext? context)
    {
        if (context is not { IsolationLevel: IsolationLevel.Serializable })
        {
            return;
        }

        var lockKey = LockManagerBase.GetKeyLockKey(Name, keyBytes.ToArray());
        _lockManager.AcquireWriteLock(lockKey, 5000, $"chiave di indice '{Name}'");
        context.WriteLockedKeys.Add(lockKey);
    }
}
