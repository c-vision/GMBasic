// FILE: Basic.DBF.DbfIndex.Compound.Cache.cs
// CDX/MDX Page Cache con LRU eviction, dirty tracking, e B-tree incremental write.
// Aggiunge caching ai tag CDX/MDX esistenti senza modificarne l'API pubblica.

using System.Collections.Generic;
using System.IO;

namespace Basic.DBF;

/// <summary>Cache LRU per pagine CDX/MDX (512 byte).</summary>
internal sealed class CdxPageCache
{
    private readonly Dictionary<uint, CdxCacheEntry> _pages = new();
    private readonly LinkedList<uint> _lru = new();  // testa = MRU, coda = LRU
    private readonly int _maxPages;
    private readonly string _filePath;

    public CdxPageCache(string filePath, int maxPages = 10000)
    {
        _filePath = filePath;
        _maxPages = maxPages;
    }

    /// <summary>Legge una pagina dal file o dalla cache.</summary>
    public byte[] GetPage(uint offset)
    {
        if (_pages.TryGetValue(offset, out var entry))
        {
            Touch(offset);
            return entry.Data;
        }

        using var fs = new FileStream(_filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        var data = new byte[CdxFormat.PageSize];
        fs.Position = offset;
        fs.ReadExactly(data);

        if (_pages.Count >= _maxPages)
            EvictLru();

        _pages[offset] = new CdxCacheEntry { Data = data, Dirty = false };
        _lru.AddFirst(offset);
        return data;
    }

    /// <summary>Scrive una pagina nella cache (non su disco).</summary>
    public void PutPage(uint offset, byte[] data, bool dirty = false)
    {
        if (_pages.Count >= _maxPages)
            EvictLru();

        _pages[offset] = new CdxCacheEntry { Data = data, Dirty = dirty };
        Touch(offset);
    }

    /// <summary>Marca una pagina come modificata.</summary>
    public void MarkDirty(uint offset)
    {
        if (_pages.TryGetValue(offset, out var entry))
        {
            entry.Dirty = true;
            Touch(offset);
        }
    }

    /// <summary>Alloca una nuova pagina e la scrive in coda al file.</summary>
    public uint AllocatePage(byte[] data)
    {
        uint offset;
        using (var fs = new FileStream(_filePath, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite))
        {
            offset = (uint)fs.Length;
            fs.Position = offset;
            fs.Write(data);
        }
        PutPage(offset, data, dirty: false);
        return offset;
    }

    /// <summary>Scrive tutte le pagine modificate su disco.</summary>
    public void Flush()
    {
        using var fs = new FileStream(_filePath, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite);
        foreach (var (offset, entry) in _pages)
        {
            if (entry.Dirty)
            {
                fs.Position = offset;
                fs.Write(entry.Data);
                entry.Dirty = false;
            }
        }
    }

    /// <summary>Svuota la cache.</summary>
    public void Clear()
    {
        _pages.Clear();
        _lru.Clear();
    }

    /// <summary>Numero di pagine in cache.</summary>
    public int Count => _pages.Count;

    private void Touch(uint offset)
    {
        _lru.Remove(offset);
        _lru.AddFirst(offset);
    }

    private void EvictLru()
    {
        if (_lru.Count == 0) return;
        var oldest = _lru.Last!.Value;
        _lru.RemoveLast();
        _pages.Remove(oldest);
    }

    private sealed class CdxCacheEntry
    {
        public byte[] Data = null!;
        public bool Dirty;
    }
}

/// <summary>Costruttore di alberi B+ per CDX con split dei nodi.</summary>
internal static class CdxBTreeBuilder
{
    /// <summary>Costruisce un albero CDX da una lista ordinata di entries.
    /// Restituisce (lista pagine, indice root).</summary>
    public static (List<CdxPage> Pages, int RootIndex) Build(
        IReadOnlyList<(byte[] Key, int Recno)> sortedEntries,
        int keyLength)
    {
        if (sortedEntries.Count == 0)
        {
            var empty = CdxPage.NewLeaf(keyLength);
            return (new List<CdxPage> { empty }, 0);
        }

        var maxLeaf = CdxCompactEncoder.MaxKeysPerLeaf(keyLength);
        var pages = new List<CdxPage>();
        var level = new List<(int PageIndex, byte[] PromoteKey)>();

        // Build leaf pages
        for (int i = 0; i < sortedEntries.Count; i += maxLeaf)
        {
            var chunk = sortedEntries.Skip(i).Take(maxLeaf).ToList();
            var (leafData, _) = CdxPage.BuildLeaf(chunk, keyLength);
            pages.Add(new CdxPage { Data = leafData, Offset = 0, IsLeaf = true });
            level.Add((pages.Count - 1, chunk[0].Key));
        }

        if (level.Count == 1)
            return (pages, level[0].PageIndex);

        // Build interior levels
        int maxBranch = Math.Max(1, (CdxFormat.PageSize - CdxFormat.IntNodeHeaderSize) / (keyLength + 4) - 1);
        while (level.Count > 1)
        {
            var parents = new List<(int PageIndex, byte[] PromoteKey)>();
            int idx = 0;
            while (idx < level.Count)
            {
                var childIndices = new List<int>();
                var separators = new List<byte[]>();
                childIndices.Add(level[idx].PageIndex);
                idx++;
                while (idx < level.Count && separators.Count < maxBranch)
                {
                    separators.Add(level[idx].PromoteKey);
                    childIndices.Add(level[idx].PageIndex);
                    idx++;
                }
                var interiorData = CdxPage.BuildInterior(new List<uint>(childIndices.Select(c => (uint)c)), separators, keyLength);
                pages.Add(new CdxPage { Data = interiorData, Offset = 0, IsLeaf = false });
                parents.Add((pages.Count - 1, separators.Count > 0 ? separators[0] : level[0].PromoteKey));
            }
            level = parents;
        }

        return (pages, level[0].PageIndex);
    }

    /// <summary>Inserisce una entry in un albero CDX (usa la cache).</summary>
    public static uint InsertEntry(
        CdxPageCache cache,
        uint rootOffset,
        byte[] key,
        int recno,
        int keyLength,
        bool unique)
    {
        var root = cache.GetPage(rootOffset);
        var (newRootData, newRootOffset, splitKey, splitChildOffset) =
            InsertRecursive(cache, rootOffset, key, recno, keyLength, unique);

        if (splitKey != null)
        {
            // Root was split — create new root
            var children = new List<int> { (int)newRootOffset, (int)splitChildOffset };
            var separators = new List<byte[]> { splitKey };
            var newRoot = CdxPage.BuildInterior(
                new List<uint>(children.Select(c => (uint)c)),
                separators,
                keyLength);
            newRootOffset = cache.AllocatePage(newRoot);
            cache.MarkDirty(newRootOffset);
        }
        else if (newRootData != null)
        {
            cache.PutPage(rootOffset, newRootData, dirty: true);
            cache.MarkDirty(rootOffset);
            newRootOffset = rootOffset;
        }

        return newRootOffset;
    }

    private static (byte[]? pageData, uint pageOffset, byte[]? splitKey, uint splitChild) InsertRecursive(
        CdxPageCache cache,
        uint offset,
        byte[] key,
        int recno,
        int keyLength,
        bool unique)
    {
        var page = cache.GetPage(offset);
        var isLeaf = (BitConverter.ToUInt16(page, 0) & 2) != 0;

        if (isLeaf)
        {
            return InsertInLeaf(cache, offset, page, key, recno, keyLength, unique);
        }

        // Interior node: find child to descend into
        var keyCount = BitConverter.ToUInt16(page, 2);
        int childIdx = 0;
        for (int i = 0; i < keyCount; i++)
        {
            var sepKey = CdxPage.GetInteriorKey(page, i, keyLength);
            if (key.AsSpan().SequenceCompareTo(sepKey) < 0) break;
            childIdx = i + 1;
        }
        var childOffset = CdxPage.GetInteriorChild(page, childIdx, keyCount, keyLength);
        if (childOffset == 0 || childOffset == uint.MaxValue)
            return (null, offset, null, 0);

        var (childData, childOff, splitKey, splitChild) =
            InsertRecursive(cache, childOffset, key, recno, keyLength, unique);

        if (splitKey == null)
        {
            if (childData != null && childOff != childOffset)
            {
                // Child was rewritten at new offset — update pointer
                cache.PutPage(childOff, childData, dirty: true);
                CdxPage.SetInteriorChild(page, childIdx, keyCount, keyLength, childOff);
                cache.PutPage(offset, page, dirty: true);
            }
            else if (childData != null)
            {
                cache.PutPage(childOff, childData, dirty: true);
            }
            return (null, offset, null, 0);
        }

        // Child was split — insert new separator in this node
        return InsertSplitInInterior(cache, offset, page, childIdx, keyCount,
            splitKey, splitChild, childOff, childData, keyLength);
    }

    private static (byte[]? pageData, uint pageOffset, byte[]? splitKey, uint splitChild) InsertInLeaf(
        CdxPageCache cache, uint offset, byte[] page, byte[] key, int recno, int keyLength, bool unique)
    {
        var decoded = CdxCompactDecoder.DecodeLeafKeys(page, BitConverter.ToUInt16(page, 2), keyLength);
        var entries = decoded.Select(d => (d.Key, (int)d.Recno)).ToList();

        // Check for duplicate
        var idx = FindKeyIndex(entries, key);
        if (unique && idx < entries.Count && entries[idx].Key.AsSpan().SequenceEqual(key))
            throw new InvalidOperationException($"Chiave duplicata nell'indice CDX.");

        entries.Insert(idx, (key, recno));

        int maxLeaf = CdxCompactEncoder.MaxKeysPerLeaf(keyLength);
        if (entries.Count <= maxLeaf)
        {
            var (newLeaf, _) = CdxPage.BuildLeaf(entries, keyLength);
            return (newLeaf, offset, null, 0);
        }

        // Split leaf
        int splitAt = entries.Count / 2;
        var leftEntries = entries.Take(splitAt).ToList();
        var rightEntries = entries.Skip(splitAt).ToList();

        var (leftPage, _) = CdxPage.BuildLeaf(leftEntries, keyLength);
        var (rightPage, _) = CdxPage.BuildLeaf(rightEntries, keyLength);

        uint rightOffset = cache.AllocatePage(rightPage);
        cache.PutPage(offset, leftPage, dirty: true);
        cache.MarkDirty(offset);

        return (leftPage, offset, rightEntries[0].Key, rightOffset);
    }

    private static (byte[]? pageData, uint pageOffset, byte[]? splitKey, uint splitChild) InsertSplitInInterior(
        CdxPageCache cache, uint offset, byte[] page, int insertAfter,
        int keyCount, byte[] splitKey, uint splitChildOffset,
        uint childOffset, byte[]? childData, int keyLength)
    {
        // Update child pointer for the split child
        if (childData != null)
        {
            cache.PutPage(childOffset, childData, dirty: true);
        }

        // Build list of child offsets and separators
        var children = new List<uint>();
        var separators = new List<byte[]>();
        for (int i = 0; i <= keyCount; i++)
        {
            children.Add(CdxPage.GetInteriorChild(page, i, keyCount, keyLength));
            if (i < keyCount)
                separators.Add(CdxPage.GetInteriorKey(page, i, keyLength));
            if (i == insertAfter)
            {
                separators.Add(splitKey);
                children.Add(splitChildOffset);
            }
        }

        int maxBranch = Math.Max(1, (CdxFormat.PageSize - CdxFormat.IntNodeHeaderSize) / (keyLength + 4) - 1);
        if (separators.Count <= maxBranch)
        {
            var newPage = CdxPage.BuildInterior(children, separators, keyLength);
            return (newPage, offset, null, 0);
        }

        // Split interior node
        int splitAt = separators.Count / 2;
        var leftChildren = children.Take(splitAt + 1).ToList();
        var rightChildren = children.Skip(splitAt + 1).ToList();
        var leftSeps = separators.Take(splitAt).ToList();
        var rightSeps = separators.Skip(splitAt + 1).ToList();
        var promoteKey = separators[splitAt];

        var leftPage = CdxPage.BuildInterior(leftChildren, leftSeps, keyLength);
        var rightPage = CdxPage.BuildInterior(rightChildren, rightSeps, keyLength);

        uint rightOff = cache.AllocatePage(rightPage);
        cache.PutPage(offset, leftPage, dirty: true);
        cache.MarkDirty(offset);

        return (leftPage, offset, promoteKey, rightOff);
    }

    private static int FindKeyIndex(List<(byte[] Key, int Recno)> entries, byte[] key)
    {
        int lo = 0, hi = entries.Count - 1, insertAt = entries.Count;
        while (lo <= hi)
        {
            int mid = (lo + hi) >> 1;
            int cmp = key.AsSpan().SequenceCompareTo(entries[mid].Key);
            if (cmp == 0) return mid;
            if (cmp < 0) { insertAt = mid; hi = mid - 1; }
            else lo = mid + 1;
        }
        return insertAt;
    }
}

/// <summary>Rappresenta una pagina CDX (512 byte) con metadati.</summary>
internal sealed class CdxPage
{
    public byte[] Data = null!;
    public uint Offset;
    public bool IsLeaf;
    public bool Dirty;

    public static CdxPage NewLeaf(int keyLength)
    {
        var data = new byte[CdxFormat.PageSize];
        BitConverter.GetBytes((ushort)3).CopyTo(data, 0); // leaf flag
        BitConverter.GetBytes((ushort)0).CopyTo(data, 2); // 0 keys
        BitConverter.GetBytes(uint.MaxValue).CopyTo(data, 4);
        BitConverter.GetBytes(uint.MaxValue).CopyTo(data, 8);
        _ = keyLength;
        return new CdxPage { Data = data, IsLeaf = true };
    }

    public static (byte[] Data, int KeyCount) BuildLeaf(
        IReadOnlyList<(byte[] Key, int Recno)> entries, int keyLength)
    {
        var data = CdxCompactEncoder.BuildLeaf(
            entries.Select(e => (e.Key, e.Recno)).ToList(), keyLength);
        return (data, entries.Count);
    }

    public static byte[] BuildInterior(
        IReadOnlyList<uint> children, IReadOnlyList<byte[]> separators, int keyLength)
    {
        return CdxCompactEncoder.BuildInteriorPage(children, separators, keyLength);
    }

    public static byte[] GetInteriorKey(byte[] page, int index, int keyLength)
    {
        int keyCount = BitConverter.ToUInt16(page, 2);
        int slotStart = CdxFormat.PageSize - ((keyCount + 1) * (keyLength + 4));
        int offset = slotStart + (index * (keyLength + 4));
        return page.AsSpan(offset, keyLength).ToArray();
    }

    public static uint GetInteriorChild(byte[] page, int index, int keyCount, int keyLength)
    {
        return CdxCompactDecoder.GetInteriorChild(page, index, keyCount, keyLength);
    }

    public static void SetInteriorChild(byte[] page, int index, int keyCount, int keyLength, uint newChild)
    {
        int slotStart = CdxFormat.PageSize - ((keyCount + 1) * (keyLength + 4));
        int offset = slotStart + (index * (keyLength + 4)) + keyLength;
        BitConverter.GetBytes(newChild).CopyTo(page, offset);
    }
}

// ====================================================================
// CdxTagIndex: page-cache aware extension (partial class)
// ====================================================================

internal sealed partial class CdxTagIndex
{
    private CdxPageCache? _pageCache;
    private uint _cachedRootOffset;

    /// <summary>Attiva la page cache per accessi incrementali. Da chiamare dopo il costruttore.</summary>
    public void EnablePageCache()
    {
        if (!_writable) return; // Read-only indexes don't need cache
        _pageCache = new CdxPageCache(_compoundPath);
        _cachedRootOffset = _descriptor.RootOffset;
        _entries.Clear(); // Free memory - entries will be loaded on demand
    }

    /// <summary>Carica entries on-demand usando la page cache (per operazioni di ricerca).</summary>
    private void EnsureEntriesLoaded()
    {
        if (_pageCache == null) return; // Old path: entries already loaded
        if (_entries.Count > 0) return; // Already loaded

        // Lazy load: walk the tree using page cache
        if (_cachedRootOffset == 0) return;
        LoadEntriesFromCache(_cachedRootOffset);
    }

    private void LoadEntriesFromCache(uint offset)
    {
        var page = _pageCache!.GetPage(offset);
        var attr = BitConverter.ToUInt16(page);
        var keyCount = BitConverter.ToUInt16(page, 2);
        var isLeaf = (attr & 2) != 0;

        if (isLeaf)
        {
            var decoded = CdxCompactDecoder.DecodeLeafKeys(page, keyCount, KeyLength);
            foreach (var d in decoded)
                _entries.Add((d.Key, (int)d.Recno));
            return;
        }

        for (var i = 0; i <= keyCount; i++)
        {
            var child = CdxCompactDecoder.GetInteriorChild(page, i, keyCount, KeyLength);
            if (child != 0 && child != uint.MaxValue)
                LoadEntriesFromCache(child);
        }
    }

    /// <summary>Insert con page cache: B-tree split invece di riscrivere tutto.</summary>
    private void InsertWithCache(byte[] keyBytes, int recno)
    {
        if (_pageCache == null)
        {
            InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, keyBytes, recno);
            return;
        }

        _cachedRootOffset = CdxBTreeBuilder.InsertEntry(
            _pageCache, _cachedRootOffset, keyBytes, recno, KeyLength, Unique);
        _pageCache.Flush();
        _recordOrderCache = null;
    }

    /// <summary>Delete con page cache: carica entries, rimuovi, ricostruisci albero.</summary>
    private void DeleteWithCache(byte[] keyBytes, int recno)
    {
        if (_pageCache == null)
        {
            InMemoryIndexOps.Delete(_entries, _syncRoot, keyBytes, recno);
            return;
        }

        EnsureEntriesLoaded();
        InMemoryIndexOps.Delete(_entries, _syncRoot, keyBytes, recno);

        // Ricostruisci albero dalle entries rimanenti
        var sorted = _entries.OrderBy(e => e.Key, 
            Comparer<byte[]>.Create((a, b) => a.AsSpan().SequenceCompareTo(b))).ToList();
        var (pages, rootIdx) = CdxBTreeBuilder.Build(
            sorted.Select(e => (e.Key, e.Recno)).ToList(), KeyLength);

        _pageCache.Clear();
        for (int i = 0; i < pages.Count; i++)
        {
            pages[i].Offset = _pageCache.AllocatePage(pages[i].Data);
        }
        _cachedRootOffset = pages[rootIdx].Offset;
        _pageCache.Flush();
        _recordOrderCache = null;
    }

    /// <summary>Incremental flush: scrive solo pagine modificate (non ricostruisce il file).</summary>
    private void FlushCompoundIncremental()
    {
        if (_pageCache == null)
        {
            RewriteCompound(); // Old path: full rebuild
            return;
        }

        // Update the compound header to point to the new root
        var header = new byte[CdxFormat.HeaderSize];
        using (var fs = new FileStream(_compoundPath, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite))
        {
            fs.ReadExactly(header);
            BitConverter.GetBytes(_cachedRootOffset).CopyTo(header, 0);
            fs.Position = 0;
            fs.Write(header);
        }

        _pageCache.Flush();
    }
}
