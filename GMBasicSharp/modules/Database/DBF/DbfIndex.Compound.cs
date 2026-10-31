// FILE: Basic.DBF.DbfIndex.Compound.cs

using System.Text;
using Basic.Databases;

namespace Basic.DBF;

#region MDX (dBASE IV compound)

internal static class MdxFormat
{
    public const int HeaderSize = 512;
    public const int TagTableOffset = 544;
    public const int TagEntrySize = 48;
    public const int TagNameLength = 10;
}

internal readonly record struct MdxTagDescriptor(string Name, uint HeaderPage, uint RootPage, int KeyLength, string Expression, bool Unique);

/// <summary>Parses dBASE IV MDX compound files and exposes per-tag indexes.</summary>
internal static class MdxCompound
{
    public static IReadOnlyList<MdxTagDescriptor> ReadTagTable(string path)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        if (fs.Length < MdxFormat.TagTableOffset + MdxFormat.TagEntrySize)
        {
            return [];
        }

        var header = new byte[MdxFormat.HeaderSize];
        fs.ReadExactly(header);
        var tagCount = header[28];
        if (tagCount == 0)
        {
            return [];
        }

        fs.Position = MdxFormat.TagTableOffset;
        var table = new byte[tagCount * MdxFormat.TagEntrySize];
        fs.ReadExactly(table);

        var tags = new List<MdxTagDescriptor>();
        for (var i = 0; i < tagCount; i++)
        {
            var entry = table.AsSpan(i * MdxFormat.TagEntrySize, MdxFormat.TagEntrySize);
            var headerPage = BitConverter.ToUInt32(entry);
            if (headerPage == 0)
            {
                continue;
            }

            var name = ReadTagName(entry.Slice(4, MdxFormat.TagNameLength));
            var tagHeader = ReadBlock(fs, headerPage);
            var rootPage = BitConverter.ToUInt32(tagHeader);
            var keyLength = BitConverter.ToUInt16(tagHeader, 12);
            var unique = tagHeader[23] != 0;
            var expr = ReadExpression(tagHeader);
            tags.Add(new MdxTagDescriptor(name, headerPage, rootPage, keyLength, expr, unique));
        }

        return tags;
    }

    public static void WriteCompound(
        string path,
        string dbfBaseName,
        IReadOnlyList<(string TagName, string Expression, int KeyLength, bool Unique,
            IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> Entries)> tags)
    {
        var tagTable = new byte[tags.Count * MdxFormat.TagEntrySize];
        var dataPages = new List<byte[]>();
        var dataStartOffset = AlignUp(MdxFormat.TagTableOffset + tagTable.Length, NdxFormat.BlockSize);
        var dataStartBlock = dataStartOffset / NdxFormat.BlockSize;

        for (var i = 0; i < tags.Count; i++)
        {
            var tag = tags[i];
            var headerPageNum = dataStartBlock + (uint)dataPages.Count;
            dataPages.Add(BuildTagHeaderPage(tag.Expression, tag.KeyLength, tag.Unique, 0));

            var ndxPages = BuildNdxPages(tag.KeyLength, tag.Entries);
            var rootPage = dataStartBlock + (uint)dataPages.Count;
            dataPages.AddRange(ndxPages);

            var tagHeader = dataPages[(int)(headerPageNum - dataStartBlock)];
            BitConverter.GetBytes(rootPage).CopyTo(tagHeader, 0);
            BitConverter.GetBytes(dataStartBlock + (uint)dataPages.Count).CopyTo(tagHeader, 4);

            var entryOffset = i * MdxFormat.TagEntrySize;
            BitConverter.GetBytes(headerPageNum).CopyTo(tagTable, entryOffset);
            WriteTagName(tagTable.AsSpan(entryOffset + 4, MdxFormat.TagNameLength), tag.TagName);
        }

        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using var fs = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
        fs.Write(BuildMdxHeader(dbfBaseName, (byte)tags.Count));
        fs.Write(new byte[MdxFormat.TagTableOffset - MdxFormat.HeaderSize]);
        fs.Write(tagTable);
        var pad = dataStartOffset - (MdxFormat.TagTableOffset + tagTable.Length);
        if (pad > 0)
        {
            fs.Write(new byte[pad]);
        }

        foreach (var page in dataPages)
        {
            fs.Write(page);
        }
    }

    private static int AlignUp(int value, int alignment) =>
        (value + alignment - 1) / alignment * alignment;

    private static byte[] BuildMdxHeader(string dbfBaseName, byte tagCount)
    {
        var header = new byte[MdxFormat.HeaderSize];
        header[0] = 2;
        header[20] = 2;
        header[28] = tagCount;
        header[29] = MdxFormat.TagNameLength;
        var nameBytes = Encoding.ASCII.GetBytes(Path.GetFileNameWithoutExtension(dbfBaseName));
        nameBytes.AsSpan(0, Math.Min(nameBytes.Length, 16)).CopyTo(header.AsSpan(4));
        return header;
    }

    private static byte[] BuildTagHeaderPage(string expression, int keyLength, bool unique, uint rootPage)
    {
        var page = new byte[NdxFormat.BlockSize];
        BitConverter.GetBytes(rootPage).CopyTo(page, 0);
        BitConverter.GetBytes((ushort)keyLength).CopyTo(page, 12);
        BitConverter.GetBytes(NdxFormat.MaxKeysPerPage(keyLength)).CopyTo(page, 14);
        BitConverter.GetBytes((ushort)NdxFormat.GroupLength(keyLength)).CopyTo(page, 18);
        page[23] = unique ? (byte)1 : (byte)0;
        var exprBytes = Encoding.ASCII.GetBytes(expression.ToLowerInvariant());
        exprBytes.AsSpan(0, Math.Min(exprBytes.Length, NdxFormat.ExpressionLength))
            .CopyTo(page.AsSpan(NdxFormat.ExpressionOffset));
        return page;
    }

    private static List<byte[]> BuildNdxPages(int keyLength, IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries)
    {
        var tempPath = Path.Combine(Path.GetTempPath(), $"mdx_{Guid.NewGuid():N}.ndx");
        try
        {
            NdxBuilder.Write(tempPath, string.Empty, keyLength, entries, unique: false);
            using var fs = new FileStream(tempPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            fs.Position = NdxFormat.BlockSize;
            var pages = new List<byte[]>();
            var buf = new byte[NdxFormat.BlockSize];
            while (fs.Read(buf) == NdxFormat.BlockSize)
            {
                pages.Add(buf.ToArray());
                buf = new byte[NdxFormat.BlockSize];
            }

            return pages;
        }
        finally
        {
            if (File.Exists(tempPath))
            {
                File.Delete(tempPath);
            }
        }
    }

    private static byte[] ReadBlock(FileStream fs, uint pageNumber)
    {
        var buffer = new byte[NdxFormat.BlockSize];
        fs.Position = NdxFormat.BlockOffset(pageNumber);
        fs.ReadExactly(buffer);
        return buffer;
    }

    private static string ReadTagName(ReadOnlySpan<byte> span)
    {
        var len = span.IndexOf((byte)0);
        if (len < 0)
        {
            len = span.Length;
        }

        return len == 0 ? string.Empty : Encoding.ASCII.GetString(span[..len]).Trim();
    }

    private static void WriteTagName(Span<byte> dest, string name)
    {
        dest.Clear();
        var bytes = Encoding.ASCII.GetBytes(name);
        bytes.AsSpan(0, Math.Min(bytes.Length, dest.Length)).CopyTo(dest);
    }

    private static string ReadExpression(ReadOnlySpan<byte> tagHeader) =>
        ReadTagName(tagHeader.Slice(NdxFormat.ExpressionOffset, NdxFormat.ExpressionLength));
}

/// <summary>Single tag inside an MDX compound file.</summary>
internal sealed partial class MdxTagIndex : IWritableIndexHandle
{
    private readonly string _compoundPath;
    private readonly MdxTagDescriptor _descriptor;
    private readonly ILockManager? _lockManager;
    private readonly object _syncRoot = new();
    private readonly List<(byte[] Key, int Recno)> _entries;
    private readonly bool _writable;
    private List<int>? _recordOrderCache;

    public string Name { get; }
    public bool Unique { get; }
    public bool Descending { get; }
    public int KeyLength { get; }

    public MdxTagIndex(string compoundPath, MdxTagDescriptor descriptor, ILockManager? lockManager)
    {
        _compoundPath = compoundPath;
        _descriptor = descriptor;
        _lockManager = lockManager;
        _writable = lockManager != null;
        Name = descriptor.Name;
        Unique = descriptor.Unique;
        Descending = false;
        KeyLength = descriptor.KeyLength;
        _entries = LoadEntries();
    }

    private List<(byte[] Key, int Recno)> LoadEntries()
    {
        if (_descriptor.RootPage == 0)
        {
            return [];
        }

        using var fs = new FileStream(_compoundPath, FileMode.Open, FileAccess.Read, FileShare.Read);
        var groupLen = NdxFormat.GroupLength(KeyLength);
        var result = new List<(byte[] Key, int Recno)>();
        EnumerateBlock(fs, _descriptor.RootPage, _descriptor.KeyLength, groupLen, result);
        return result;
    }

    private static void EnumerateBlock(
        FileStream fs,
        uint blockNumber,
        int keyLength,
        int groupLen,
        List<(byte[] Key, int Recno)> result)
    {
        var buffer = new byte[NdxFormat.BlockSize];
        fs.Position = NdxFormat.BlockOffset(blockNumber);
        fs.ReadExactly(buffer);
        var page = new NdxPage(buffer, groupLen);
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

    public string GetKeyExpressionString() => _descriptor.Expression;

    public int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context) =>
        InMemoryIndexOps.Seek(_entries, _syncRoot, _lockManager, Name, keyBytes, soft, context);

    public (int Recno, byte[]? KeyBytes) SeekWithKeyBytes(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context) =>
        InMemoryIndexOps.SeekWithKey(_entries, _syncRoot, _lockManager, Name, keyBytes, soft, context);

    public void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
    {
        var keyCopy = keyBytes.ToArray();
        Mutate(context, () => InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, keyCopy, recno));
    }

    public void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context) =>
        Mutate(context, () =>
        {
            foreach (var (key, recno) in entries)
            {
                InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, key.ToArray(), recno);
            }
        });

    public void Delete(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
    {
        var keyCopy = keyBytes.ToArray();
        Mutate(context, () => InMemoryIndexOps.Delete(_entries, _syncRoot, keyCopy, recno));
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
                InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, key.ToArray(), recno);
            }

            _recordOrderCache = null;
            RewriteCompound();
        }
    }

    public int GetFirstRecordNumber() => InMemoryIndexOps.GetFirst(_entries, _syncRoot);
    public int GetLastRecordNumber() => InMemoryIndexOps.GetLast(_entries, _syncRoot);
    public IEnumerable<int> GetAllRecordNumbers() => InMemoryIndexOps.GetAll(_entries, _syncRoot, ref _recordOrderCache);

    public (int recno, uint pageNum, int indexInLeaf) GetFirstRecordPosition(TransactionContext? context) =>
        InMemoryIndexOps.FirstPosition(_entries, _syncRoot, _lockManager, Name, context);

    public (int recno, uint pageNum, int indexInLeaf) GetLastRecordPosition(TransactionContext? context) =>
        InMemoryIndexOps.LastPosition(_entries, _syncRoot, _lockManager, Name, context);

    public (int recno, uint newPageNum, int newIndexInLeaf) GetNextRecord(
        uint currentPageNum, int currentIndexInLeaf, int direction, TransactionContext? context) =>
        InMemoryIndexOps.NextRecord(_entries, _syncRoot, _lockManager, Name, currentPageNum, currentIndexInLeaf, direction, context);

    public void InvalidateCache() => InMemoryIndexOps.Invalidate(ref _recordOrderCache, _syncRoot);

    public void Dispose()
    {
    }

    private void Mutate(TransactionContext? context, Action action)
    {
        EnsureWritable();
        lock (_syncRoot)
        {
            action();
            _recordOrderCache = null;
            RewriteCompound();
        }
    }

    private void RewriteCompound()
    {
        // ponytail: rebuild entire MDX from all tags — reload others from disk first
        var allTags = MdxCompound.ReadTagTable(_compoundPath);
        var rebuilt = new List<(string, string, int, bool, IReadOnlyList<(ReadOnlyMemory<byte>, int)>)>();
        foreach (var tag in allTags)
        {
            if (tag.Name.Equals(Name, StringComparison.OrdinalIgnoreCase))
            {
                rebuilt.Add((Name, _descriptor.Expression, KeyLength, Unique,
                    _entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
            }
            else
            {
                var other = new MdxTagIndex(_compoundPath, tag, null);
                rebuilt.Add((tag.Name, tag.Expression, tag.KeyLength, tag.Unique,
                    other._entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
            }
        }

        var dbfName = Path.GetFileNameWithoutExtension(_compoundPath);
        MdxCompound.WriteCompound(_compoundPath, dbfName, rebuilt);
    }

    private void EnsureWritable()
    {
        if (!_writable)
        {
            throw new NotSupportedException($"Il tag MDX '{Name}' è aperto in sola lettura.");
        }
    }
}

#endregion

#region CDX (FoxPro compound compact)

internal static class CdxFormat
{
    public const int PageSize = 512;
    public const int HeaderSize = 512;
    public const int ExtNodeHeaderSize = 24;
    public const int IntNodeHeaderSize = 12;
    public const byte TypeCompound = 0x40;
    public const byte TypeCompact = 0x20;
    public const int TagNameLength = 10;
}

internal readonly record struct CdxTagDescriptor(
    string Name, uint RootOffset, int KeyLength, string Expression, bool Unique, bool Descending);

/// <summary>Parses FoxPro CDX compound files.</summary>
internal static class CdxCompound
{
    public static IReadOnlyList<CdxTagDescriptor> ReadTagTable(string path)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        if (fs.Length < CdxFormat.HeaderSize)
        {
            return [];
        }

        var header = new byte[CdxFormat.HeaderSize];
        fs.ReadExactly(header);
        if ((header[14] & CdxFormat.TypeCompound) == 0)
        {
            return [];
        }

        var rootOffset = BitConverter.ToUInt32(header);
        if (rootOffset == 0 || rootOffset >= fs.Length)
        {
            return [];
        }

        return EnumerateCatalog(fs, rootOffset);
    }

    private static List<CdxTagDescriptor> EnumerateCatalog(FileStream fs, uint rootOffset)
    {
        var tags = new List<CdxTagDescriptor>();
        WalkCatalogNode(fs, rootOffset, tags);
        return tags;
    }

    private static void WalkCatalogNode(FileStream fs, uint offset, List<CdxTagDescriptor> tags)
    {
        var page = ReadPage(fs, offset);
        var attr = BitConverter.ToUInt16(page);
        var keyCount = BitConverter.ToUInt16(page, 2);
        var isLeaf = (attr & 2) != 0;

        if (isLeaf)
        {
            var decoded = CdxCompactDecoder.DecodeLeafKeys(page, keyCount, CdxFormat.TagNameLength);
            foreach (var entry in decoded)
            {
                var name = Encoding.ASCII.GetString(entry.Key).TrimEnd('\0', ' ');
                if (string.IsNullOrEmpty(name))
                {
                    continue;
                }

                var tagHeader = ReadPage(fs, entry.Recno);
                var tagRootOffset = BitConverter.ToUInt32(tagHeader);
                var tagKeyLen = BitConverter.ToUInt16(tagHeader, 12);
                var unique = (tagHeader[14] & 0x01) != 0;
                var descending = tagHeader[502] != 0;
                var expr = ReadCdxExpression(tagHeader);
                tags.Add(new CdxTagDescriptor(name, tagRootOffset, tagKeyLen, expr, unique, descending));
            }

            return;
        }

        for (var i = 0; i < keyCount; i++)
        {
            var childOffset = CdxCompactDecoder.GetInteriorChild(page, i, keyCount, CdxFormat.TagNameLength);
            if (childOffset != 0 && childOffset != uint.MaxValue)
            {
                WalkCatalogNode(fs, childOffset, tags);
            }
        }

        var rightChild = CdxCompactDecoder.GetInteriorChild(page, keyCount, keyCount, CdxFormat.TagNameLength);
        if (rightChild != 0 && rightChild != uint.MaxValue)
        {
            WalkCatalogNode(fs, rightChild, tags);
        }
    }

    public static void WriteCompound(
        string path,
        IReadOnlyList<(string TagName, string Expression, int KeyLength, bool Unique, bool Descending,
            IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> Entries)> tags)
    {
        var pages = new List<byte[]>();
        var catalogLeaves = new List<(string Name, uint TagHeaderOffset)>();

        foreach (var tag in tags)
        {
            var tagHeaderOffset = (uint)(pages.Count * CdxFormat.PageSize + CdxFormat.HeaderSize);
            pages.Add(BuildCdxTagHeader(tag.Expression, tag.KeyLength, tag.Unique, tag.Descending, 0));

            var treeBase = tagHeaderOffset + CdxFormat.PageSize;
            var (tagPages, tagRootPageIndex) = BuildCdxTagPages(tag.KeyLength, tag.Entries, treeBase);
            var tagRoot = treeBase + (uint)(tagRootPageIndex * CdxFormat.PageSize);
            pages.AddRange(tagPages);
            var tagHeader = pages[(int)((tagHeaderOffset - CdxFormat.HeaderSize) / CdxFormat.PageSize)];
            BitConverter.GetBytes(tagRoot).CopyTo(tagHeader, 0);
            catalogLeaves.Add((tag.TagName, tagHeaderOffset));
        }

        var catalogRoot = BuildCatalogTree(pages, catalogLeaves);
        var header = BuildCdxCompoundHeader(catalogRoot, tags.FirstOrDefault().KeyLength);

        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using var fs = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
        fs.Write(header);
        foreach (var page in pages)
        {
            fs.Write(page);
        }
    }

    private static byte[] BuildCdxCompoundHeader(uint catalogRoot, int keyLength)
    {
        var header = new byte[CdxFormat.HeaderSize];
        BitConverter.GetBytes(catalogRoot).CopyTo(header, 0);
        BitConverter.GetBytes((ushort)keyLength).CopyTo(header, 12);
        header[14] = (byte)(CdxFormat.TypeCompact | CdxFormat.TypeCompound);
        return header;
    }

    private static byte[] BuildCdxTagHeader(string expression, int keyLength, bool unique, bool descending, uint rootOffset)
    {
        const int exprOffset = 16;
        var header = new byte[CdxFormat.PageSize];
        BitConverter.GetBytes(rootOffset).CopyTo(header, 0);
        BitConverter.GetBytes((ushort)keyLength).CopyTo(header, 12);
        header[14] = (byte)((unique ? 0x01 : 0x00) | CdxFormat.TypeCompact);
        header[502] = descending ? (byte)1 : (byte)0;
        var exprBytes = Encoding.ASCII.GetBytes(expression);
        var keyExpLen = Math.Min(exprBytes.Length, 510 - exprOffset);
        BitConverter.GetBytes((ushort)keyExpLen).CopyTo(header, 510);
        exprBytes.AsSpan(0, keyExpLen).CopyTo(header.AsSpan(exprOffset));
        return header;
    }

    private static (List<byte[]> Pages, int RootPageIndex) BuildCdxTagPages(
        int keyLength,
        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries,
        uint treeBaseOffset)
    {
        var sorted = entries
            .Select(e => (Key: CdxCompactEncoder.PadIndexKey(e.Key.ToArray(), keyLength), e.Recno))
            .OrderBy(e => e.Key, Comparer<byte[]>.Create(static (a, b) => a.AsSpan().SequenceCompareTo(b)))
            .ToList();

        if (sorted.Count == 0)
        {
            return ([CdxCompactEncoder.BuildEmptyLeaf(keyLength)], 0);
        }

        var maxLeaf = CdxCompactEncoder.MaxKeysPerLeaf(keyLength);
        var maxBranch = CdxCompactEncoder.MaxKeysPerInterior(keyLength);
        var pages = new List<byte[]>();
        var leaves = new List<(int PageIndex, byte[] PromoteKey)>();

        for (var i = 0; i < sorted.Count; i += maxLeaf)
        {
            var chunk = sorted.Skip(i).Take(maxLeaf).ToList();
            pages.Add(CdxCompactEncoder.BuildLeaf(chunk, keyLength));
            leaves.Add((pages.Count - 1, chunk[0].Key));
        }

        if (leaves.Count == 1)
        {
            return (pages, leaves[0].PageIndex);
        }

        var level = leaves;
        while (level.Count > 1)
        {
            level = BuildCdxInteriorLevel(level, pages, keyLength, maxBranch, treeBaseOffset);
        }

        return (pages, level[0].PageIndex);
    }

    private static List<(int PageIndex, byte[] PromoteKey)> BuildCdxInteriorLevel(
        List<(int PageIndex, byte[] PromoteKey)> children,
        List<byte[]> pages,
        int keyLength,
        int maxBranchKeys,
        uint treeBaseOffset)
    {
        var parents = new List<(int PageIndex, byte[] PromoteKey)>();
        var index = 0;

        while (index < children.Count)
        {
            var groupStart = index;
            var childPageIndices = new List<int> { children[index].PageIndex };
            var separators = new List<byte[]>();
            index++;

            while (index < children.Count && separators.Count < maxBranchKeys)
            {
                separators.Add(children[index].PromoteKey);
                childPageIndices.Add(children[index].PageIndex);
                index++;
            }

            var childOffsets = childPageIndices
                .Select(i => treeBaseOffset + (uint)(i * CdxFormat.PageSize))
                .ToList();
            pages.Add(CdxCompactEncoder.BuildInteriorPage(childOffsets, separators, keyLength));
            parents.Add((pages.Count - 1, separators.Count > 0 ? separators[0] : children[groupStart].PromoteKey));
        }

        return parents;
    }

    private static uint BuildCatalogTree(List<byte[]> pages, List<(string Name, uint TagHeaderOffset)> leaves)
    {
        var leafPage = CdxCompactEncoder.BuildCatalogLeaf(leaves);
        pages.Add(leafPage);
        return (uint)(CdxFormat.HeaderSize + (pages.Count - 1) * CdxFormat.PageSize);
    }

    private static IEnumerable<(string Name, uint TagHeaderOffset)> ReadCatalogLeaf(byte[] page, int keyCount)
    {
        var keyLen = CdxFormat.TagNameLength;
        var keysStart = CdxFormat.PageSize - ((keyCount + 1) * (keyLen + 4));
        for (var i = 0; i < keyCount; i++)
        {
            var offset = keysStart + (i * (keyLen + 4));
            var name = Encoding.ASCII.GetString(page, offset, keyLen).TrimEnd('\0', ' ');
            var tagHeaderOffset = BitConverter.ToUInt32(page, offset + keyLen);
            yield return (name, tagHeaderOffset);
        }
    }

    private static byte[] ReadPage(FileStream fs, uint offset)
    {
        var buffer = new byte[CdxFormat.PageSize];
        fs.Position = offset;
        fs.ReadExactly(buffer);
        return buffer;
    }

    private static string ReadCdxExpression(ReadOnlySpan<byte> tagHeader)
    {
        const int exprOffset = 16;
        var keyExpLen = BitConverter.ToUInt16(tagHeader.Slice(510, 2));
        if (keyExpLen == 0)
        {
            return string.Empty;
        }

        return Encoding.ASCII.GetString(tagHeader.Slice(exprOffset, Math.Min((int)keyExpLen, 510 - exprOffset)));
    }
}

/// <summary>Compact CDX key decode/encode helpers.</summary>
internal static class CdxCompactDecoder
{
    internal readonly record struct DecodedKey(byte[] Key, uint Recno);

    public static List<DecodedKey> DecodeLeafKeys(byte[] page, int keyCount, int keyLength)
    {
        var recBits = page[20];
        var dupBits = page[21];
        var trlBits = page[22];
        var reqByte = page[23];
        if (reqByte == 0)
        {
            reqByte = 4;
        }

        var dcMask = (ushort)((1 << dupBits) - 1);
        var tcMask = (ushort)((1 << trlBits) - 1);
        var pool = page.AsSpan(CdxFormat.ExtNodeHeaderSize);
        var keyPos = CdxFormat.PageSize;
        var prevKey = new byte[keyLength];
        var results = new List<DecodedKey>();

        for (var i = 0; i < keyCount; i++)
        {
            var pos = i * reqByte;
            var packed = BitConverter.ToUInt32(pool.Slice(pos, Math.Min((int)reqByte, 4)));
            var dup = dupBits > 0 ? (int)(packed & dcMask) : 0;
            var trail = trlBits > 0 ? (int)((packed >> dupBits) & tcMask) : 0;
            var recno = recBits > 0 ? packed >> (dupBits + trlBits) : 0u;

            var varLen = keyLength - dup - trail;
            keyPos -= varLen;
            var key = new byte[keyLength];
            prevKey.AsSpan(0, dup).CopyTo(key);
            if (varLen > 0)
            {
                page.AsSpan(keyPos, varLen).CopyTo(key.AsSpan(dup));
            }

            if (trail > 0)
            {
                key.AsSpan(keyLength - trail, trail).Fill((byte)' ');
            }

            prevKey = key;
            results.Add(new DecodedKey(key, recno));
        }

        return results;
    }

    public static uint GetInteriorChild(byte[] page, int keyIndex, int keyCount, int keyLength)
    {
        var keysStart = CdxFormat.PageSize - ((keyCount + 1) * (keyLength + 4));
        var offset = keysStart + (keyIndex * (keyLength + 4)) + keyLength;
        if (offset + 4 > CdxFormat.PageSize)
        {
            return 0;
        }

        return BitConverter.ToUInt32(page, offset);
    }
}

internal static class CdxCompactEncoder
{
    public static byte[] BuildEmptyLeaf(int keyLength)
    {
        var page = new byte[CdxFormat.PageSize];
        BitConverter.GetBytes((ushort)2).CopyTo(page, 0);
        BitConverter.GetBytes((ushort)0).CopyTo(page, 2);
        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 4);
        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 8);
        page[20] = 20;
        page[21] = 4;
        page[22] = 4;
        page[23] = 4;
        _ = keyLength;
        return page;
    }

    public static byte[] BuildLeaf(List<(byte[] Key, int Recno)> entries, int keyLength)
    {
        var page = new byte[CdxFormat.PageSize];
        BitConverter.GetBytes((ushort)3).CopyTo(page, 0);
        BitConverter.GetBytes((ushort)entries.Count).CopyTo(page, 2);
        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 4);
        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 8);
        page[20] = 20;
        page[21] = 4;
        page[22] = 4;
        page[23] = 4;

        var prevKey = new byte[keyLength];
        var keyPos = CdxFormat.PageSize;
        var poolOffset = CdxFormat.ExtNodeHeaderSize;

        for (var i = 0; i < entries.Count; i++)
        {
            var (keyRaw, recno) = entries[i];
            var key = PadIndexKey(keyRaw, keyLength);
            var dup = CountDuplicatePrefix(prevKey, key);
            var trail = CountTrailingSpaces(key, keyLength, dup);
            var varLen = keyLength - dup - trail;
            if (varLen < 0 || dup + varLen > key.Length)
            {
                throw new InvalidOperationException(
                    $"CDX compact leaf key encoding overflow (keyLength={keyLength}, dup={dup}, trail={trail}).");
            }

            keyPos -= varLen;
            if (keyPos < poolOffset + (entries.Count * 4))
            {
                throw new InvalidOperationException(
                    $"CDX compact leaf page overflow ({entries.Count} keys, keyLength={keyLength}).");
            }

            key.AsSpan(dup, varLen).CopyTo(page.AsSpan(keyPos));

            var packed = (uint)recno << 8 | (uint)(trail << 4) | (uint)dup;
            BitConverter.GetBytes(packed).CopyTo(page, poolOffset + (i * 4));
            prevKey = key;
        }

        return page;
    }

    public static int MaxKeysPerLeaf(int keyLength)
    {
        var lo = 1;
        var hi = 256;
        while (lo < hi)
        {
            var mid = (lo + hi + 1) / 2;
            if (CanFitLeaf(mid, keyLength))
            {
                lo = mid;
            }
            else
            {
                hi = mid - 1;
            }
        }

        return lo;
    }

    public static int MaxKeysPerInterior(int keyLength)
    {
        var slot = keyLength + 4;
        return Math.Max(1, (CdxFormat.PageSize - CdxFormat.IntNodeHeaderSize) / slot - 1);
    }

    public static byte[] BuildInteriorPage(
        IReadOnlyList<uint> childOffsets,
        IReadOnlyList<byte[]> separatorKeys,
        int keyLength)
    {
        if (childOffsets.Count != separatorKeys.Count + 1)
        {
            throw new ArgumentException("CDX interior page requires one more child than separator keys.");
        }

        var keyCount = separatorKeys.Count;
        var page = new byte[CdxFormat.PageSize];
        BitConverter.GetBytes((ushort)1).CopyTo(page, 0);
        BitConverter.GetBytes((ushort)keyCount).CopyTo(page, 2);
        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 4);
        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 8);

        var keysStart = CdxFormat.PageSize - ((keyCount + 1) * (keyLength + 4));
        for (var i = 0; i < keyCount; i++)
        {
            var slot = keysStart + (i * (keyLength + 4));
            WritePaddedKey(page.AsSpan(slot, keyLength), separatorKeys[i], keyLength);
            BitConverter.GetBytes(childOffsets[i]).CopyTo(page, slot + keyLength);
        }

        BitConverter.GetBytes(childOffsets[keyCount]).CopyTo(page, keysStart + (keyCount * (keyLength + 4)) + keyLength);
        return page;
    }

    private static bool CanFitLeaf(int count, int keyLength)
    {
        var keyPos = CdxFormat.PageSize;
        var poolEnd = CdxFormat.ExtNodeHeaderSize + (count * 4);
        for (var i = 0; i < count; i++)
        {
            keyPos -= keyLength;
            if (keyPos < poolEnd)
            {
                return false;
            }
        }

        return true;
    }

    private static void WritePaddedKey(Span<byte> dest, byte[] key, int keyLength)
    {
        dest.Clear();
        var copyLen = Math.Min(key.Length, keyLength);
        key.AsSpan(0, copyLen).CopyTo(dest);
    }

    public static byte[] BuildCatalogLeaf(List<(string Name, uint TagHeaderOffset)> tags)
    {
        var entries = tags
            .Select(t =>
            {
                var key = new byte[CdxFormat.TagNameLength];
                Encoding.ASCII.GetBytes(t.Name).AsSpan(0, Math.Min(t.Name.Length, key.Length)).CopyTo(key);
                return (Key: key, Recno: (int)t.TagHeaderOffset);
            })
            .OrderBy(e => e.Key, Comparer<byte[]>.Create(static (a, b) => a.AsSpan().SequenceCompareTo(b)))
            .Select(e => (e.Key, e.Recno))
            .ToList();

        return BuildLeaf(entries, CdxFormat.TagNameLength);
    }

    private static int CountDuplicatePrefix(byte[] prev, byte[] key)
    {
        var max = Math.Min(prev.Length, key.Length);
        var i = 0;
        while (i < max && prev[i] == key[i])
        {
            i++;
        }

        return i;
    }

    private static int CountTrailingSpaces(byte[] key, int keyLength, int dup = 0)
    {
        var trail = 0;
        var scanEnd = Math.Min(key.Length, keyLength);
        for (var i = scanEnd - 1; i >= dup; i--)
        {
            if (key[i] != (byte)' ')
            {
                break;
            }

            trail++;
        }

        if (key.Length < keyLength)
        {
            trail += keyLength - Math.Max(key.Length, dup);
        }

        return trail;
    }

    internal static byte[] PadIndexKey(byte[] key, int keyLength)
    {
        if (key.Length == keyLength)
        {
            return key;
        }

        var padded = new byte[keyLength];
        var copyLen = Math.Min(key.Length, keyLength);
        key.AsSpan(0, copyLen).CopyTo(padded);
        if (copyLen < keyLength)
        {
            padded.AsSpan(copyLen).Fill((byte)' ');
        }

        return padded;
    }
}

/// <summary>Single tag inside a FoxPro CDX compound file.</summary>
internal sealed partial class CdxTagIndex : IWritableIndexHandle
{
    private readonly string _compoundPath;
    private readonly CdxTagDescriptor _descriptor;
    private readonly ILockManager? _lockManager;
    private readonly object _syncRoot = new();
    private readonly List<(byte[] Key, int Recno)> _entries;
    private readonly bool _writable;
    private List<int>? _recordOrderCache;

    public string Name { get; }
    public bool Unique { get; }
    public bool Descending { get; }
    public int KeyLength { get; }

    public CdxTagIndex(string compoundPath, CdxTagDescriptor descriptor, ILockManager? lockManager)
    {
        _compoundPath = compoundPath;
        _descriptor = descriptor;
        _lockManager = lockManager;
        _writable = lockManager != null;
        Name = descriptor.Name;
        Unique = descriptor.Unique;
        Descending = descriptor.Descending;
        KeyLength = descriptor.KeyLength;
        _entries = LoadEntries();
        if (_writable) EnablePageCache();
    }

    private List<(byte[] Key, int Recno)> LoadEntries()
    {
        if (_descriptor.RootOffset == 0)
        {
            return [];
        }

        using var fs = new FileStream(_compoundPath, FileMode.Open, FileAccess.Read, FileShare.Read);
        return WalkTagTree(fs, _descriptor.RootOffset);
    }

    private List<(byte[] Key, int Recno)> WalkTagTree(FileStream fs, uint offset)
    {
        var page = new byte[CdxFormat.PageSize];
        fs.Position = offset;
        fs.ReadExactly(page);
        var attr = BitConverter.ToUInt16(page);
        var keyCount = BitConverter.ToUInt16(page, 2);
        var isLeaf = (attr & 2) != 0;

        if (isLeaf)
        {
            return CdxCompactDecoder.DecodeLeafKeys(page, keyCount, KeyLength)
                .Select(k => (k.Key, (int)k.Recno))
                .ToList();
        }

        var result = new List<(byte[] Key, int Recno)>();
        for (var i = 0; i <= keyCount; i++)
        {
            var child = CdxCompactDecoder.GetInteriorChild(page, i, keyCount, KeyLength);
            if (child != 0 && child != uint.MaxValue)
            {
                result.AddRange(WalkTagTree(fs, child));
            }
        }

        return result;
    }

    public string GetKeyExpressionString() => _descriptor.Expression;

    public int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
    {
        // Bug DbfEngine-Seek-Chiave-Formato (09-Errors/GMBasicAI/DbfEngine-Seek-Chiave-Formato-Bug.md):
        // EnablePageCache() (chiamata dal costruttore quando _writable=true, es. DBF USE ...
        // EXCLUSIVE) svuota _entries per il lazy-load on-demand, ma EnsureEntriesLoaded() era
        // invocato SOLO da DeleteWithCache — mai raggiunto da Seek (né da Insert/Delete
        // "semplici" via Mutate, vedi sotto). Seek cercava quindi SEMPRE in una lista vuota,
        // found=False qualunque fosse il formato della chiave provata — NON era affatto un
        // problema di formato/collation dei byte (CdxCompactDecoder decodifica correttamente,
        // verificato leggendo i 29 tag PRIMARY del fixture: recno 1..29, chiave " N" + padding).
        EnsureEntriesLoaded();
        return InMemoryIndexOps.Seek(_entries, _syncRoot, _lockManager, Name, keyBytes, soft, context);
    }

    public (int Recno, byte[]? KeyBytes) SeekWithKeyBytes(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
    {
        EnsureEntriesLoaded();
        return InMemoryIndexOps.SeekWithKey(_entries, _syncRoot, _lockManager, Name, keyBytes, soft, context);
    }

    public void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
    {
        var keyCopy = keyBytes.ToArray();
        Mutate(context, () => InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, keyCopy, recno));
    }

    public void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context) =>
        Mutate(context, () =>
        {
            foreach (var (key, recno) in entries)
            {
                InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, key.ToArray(), recno);
            }
        });

    public void Delete(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
    {
        var keyCopy = keyBytes.ToArray();
        Mutate(context, () => InMemoryIndexOps.Delete(_entries, _syncRoot, keyCopy, recno));
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
                InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, key.ToArray(), recno);
            }

            _recordOrderCache = null;
            RewriteCompound();
        }
    }

    // Stesso bug di Seek sopra: tutti questi leggono _entries direttamente — senza
    // EnsureEntriesLoaded() restano vuoti su ogni tag CDX aperto in modalità writable
    // (page cache attiva). Vedi 09-Errors/GMBasicAI/DbfEngine-Seek-Chiave-Formato-Bug.md.
    public int GetFirstRecordNumber() { EnsureEntriesLoaded(); return InMemoryIndexOps.GetFirst(_entries, _syncRoot); }
    public int GetLastRecordNumber() { EnsureEntriesLoaded(); return InMemoryIndexOps.GetLast(_entries, _syncRoot); }
    public IEnumerable<int> GetAllRecordNumbers() { EnsureEntriesLoaded(); return InMemoryIndexOps.GetAll(_entries, _syncRoot, ref _recordOrderCache); }

    public (int recno, uint pageNum, int indexInLeaf) GetFirstRecordPosition(TransactionContext? context)
    {
        EnsureEntriesLoaded();
        return InMemoryIndexOps.FirstPosition(_entries, _syncRoot, _lockManager, Name, context);
    }

    public (int recno, uint pageNum, int indexInLeaf) GetLastRecordPosition(TransactionContext? context)
    {
        EnsureEntriesLoaded();
        return InMemoryIndexOps.LastPosition(_entries, _syncRoot, _lockManager, Name, context);
    }

    public (int recno, uint newPageNum, int newIndexInLeaf) GetNextRecord(
        uint currentPageNum, int currentIndexInLeaf, int direction, TransactionContext? context)
    {
        EnsureEntriesLoaded();
        return InMemoryIndexOps.NextRecord(_entries, _syncRoot, _lockManager, Name, currentPageNum, currentIndexInLeaf, direction, context);
    }

    public void InvalidateCache() => InMemoryIndexOps.Invalidate(ref _recordOrderCache, _syncRoot);

    public void Dispose()
    {
    }

    private void Mutate(TransactionContext? context, Action action)
    {
        EnsureWritable();
        _ = context;
        lock (_syncRoot)
        {
            // Stesso bug di Seek: Insert/InsertBatch/Delete (i chiamanti di Mutate) operano su
            // _entries direttamente — senza questa chiamata partirebbero da una lista vuota su
            // un tag con page cache attiva, perdendo silenziosamente tutte le chiavi esistenti.
            EnsureEntriesLoaded();
            action();
            _recordOrderCache = null;
            if (_pageCache != null)
                FlushCompoundIncremental();
            else
                RewriteCompound();
        }
    }

    private void RewriteCompound()
    {
        var allTags = CdxCompound.ReadTagTable(_compoundPath);
        var rebuilt = new List<(string, string, int, bool, bool, IReadOnlyList<(ReadOnlyMemory<byte>, int)>)>();
        foreach (var tag in allTags)
        {
            if (tag.Name.Equals(Name, StringComparison.OrdinalIgnoreCase))
            {
                rebuilt.Add((Name, _descriptor.Expression, KeyLength, Unique, Descending,
                    _entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
            }
            else
            {
                var other = new CdxTagIndex(_compoundPath, tag, null);
                rebuilt.Add((tag.Name, tag.Expression, tag.KeyLength, tag.Unique, tag.Descending,
                    other._entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
            }
        }

        CdxCompound.WriteCompound(_compoundPath, rebuilt);
    }

    private void EnsureWritable()
    {
        if (!_writable)
        {
            throw new NotSupportedException($"Il tag CDX '{Name}' è aperto in sola lettura.");
        }
    }
}

/// <summary>Shared in-memory sorted-index operations for ponytail writable compound tags.</summary>
internal static class InMemoryIndexOps
{
    public static int Seek(
        List<(byte[] Key, int Recno)> entries,
        object syncRoot,
        ILockManager? lockManager,
        string name,
        ReadOnlySpan<byte> keyBytes,
        bool soft,
        TransactionContext? context)
    {
        if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
        {
            var lockKey = LockManagerBase.GetKeyLockKey(name, keyBytes.ToArray());
            lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{name}'");
            context.ReadLockedKeys.Add(lockKey);
        }

        lock (syncRoot)
        {
            if (entries.Count == 0)
            {
                return -1;
            }

            var idx = FindKeyIndex(entries, keyBytes, out var exact);
            if (exact)
            {
                return entries[idx].Recno;
            }

            return soft && idx < entries.Count ? entries[idx].Recno : -1;
        }
    }

    public static (int Recno, byte[]? KeyBytes) SeekWithKey(
        List<(byte[] Key, int Recno)> entries,
        object syncRoot,
        ILockManager? lockManager,
        string name,
        ReadOnlySpan<byte> keyBytes,
        bool soft,
        TransactionContext? context)
    {
        if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
        {
            var lockKey = LockManagerBase.GetKeyLockKey(name, keyBytes.ToArray());
            lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{name}'");
            context.ReadLockedKeys.Add(lockKey);
        }

        lock (syncRoot)
        {
            if (entries.Count == 0)
            {
                return (-1, null);
            }

            var idx = FindKeyIndex(entries, keyBytes, out var exact);
            if (exact)
            {
                return (entries[idx].Recno, entries[idx].Key);
            }

            return soft && idx < entries.Count ? (entries[idx].Recno, entries[idx].Key) : (-1, null);
        }
    }

    public static void Insert(
        List<(byte[] Key, int Recno)> entries,
        object syncRoot,
        bool unique,
        string name,
        byte[] keyBytes,
        int recno)
    {
        lock (syncRoot)
        {
            var idx = FindKeyIndex(entries, keyBytes, out var exact);
            if (unique && exact)
            {
                throw new InvalidOperationException($"Chiave duplicata nell'indice '{name}'.");
            }

            entries.Insert(idx, (keyBytes, recno));
        }
    }

    public static void Delete(
        List<(byte[] Key, int Recno)> entries,
        object syncRoot,
        byte[] keyBytes,
        int recno)
    {
        lock (syncRoot)
        {
            var idx = FindKeyIndex(entries, keyBytes, out var exact);
            if (!exact)
            {
                return;
            }

            for (var i = idx; i < entries.Count; i++)
            {
                if (entries[i].Key.AsSpan().SequenceCompareTo(keyBytes) != 0)
                {
                    break;
                }

                if (entries[i].Recno == recno)
                {
                    entries.RemoveAt(i);
                    return;
                }
            }
        }
    }

    public static int GetFirst(List<(byte[] Key, int Recno)> entries, object syncRoot)
    {
        lock (syncRoot)
        {
            return entries.Count > 0 ? entries[0].Recno : 0;
        }
    }

    public static int GetLast(List<(byte[] Key, int Recno)> entries, object syncRoot)
    {
        lock (syncRoot)
        {
            return entries.Count > 0 ? entries[^1].Recno : 0;
        }
    }

    public static IEnumerable<int> GetAll(
        List<(byte[] Key, int Recno)> entries,
        object syncRoot,
        ref List<int>? cache)
    {
        lock (syncRoot)
        {
            cache ??= entries.Select(e => e.Recno).ToList();
            return cache.ToList();
        }
    }

    public static (int recno, uint pageNum, int indexInLeaf) FirstPosition(
        List<(byte[] Key, int Recno)> entries,
        object syncRoot,
        ILockManager? lockManager,
        string name,
        TransactionContext? context)
    {
        lock (syncRoot)
        {
            if (entries.Count == 0)
            {
                return (0, 0, 0);
            }

            if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
            {
                var lockKey = LockManagerBase.GetKeyLockKey(name, entries[0].Key);
                lockManager.AcquireReadLock(lockKey, 5000, $"prima chiave dell'indice '{name}'");
                context.ReadLockedKeys.Add(lockKey);
            }

            return (entries[0].Recno, 1, 0);
        }
    }

    public static (int recno, uint newPageNum, int newIndexInLeaf) LastPosition(
        List<(byte[] Key, int Recno)> entries,
        object syncRoot,
        ILockManager? lockManager,
        string name,
        TransactionContext? context)
    {
        lock (syncRoot)
        {
            if (entries.Count == 0)
            {
                return (0, 0, 0);
            }

            var last = entries.Count - 1;
            if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
            {
                var lockKey = LockManagerBase.GetKeyLockKey(name, entries[last].Key);
                lockManager.AcquireReadLock(lockKey, 5000, $"ultima chiave dell'indice '{name}'");
                context.ReadLockedKeys.Add(lockKey);
            }

            return (entries[last].Recno, 1, last);
        }
    }

    public static (int recno, uint newPageNum, int newIndexInLeaf) NextRecord(
        List<(byte[] Key, int Recno)> entries,
        object syncRoot,
        ILockManager? lockManager,
        string name,
        uint currentPageNum,
        int currentIndexInLeaf,
        int direction,
        TransactionContext? context)
    {
        _ = currentPageNum;
        lock (syncRoot)
        {
            var newIndex = currentIndexInLeaf + direction;
            if (newIndex < 0 || newIndex >= entries.Count)
            {
                return (0, 0, 0);
            }

            if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
            {
                var lockKey = LockManagerBase.GetKeyLockKey(name, entries[newIndex].Key);
                lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{name}' durante la scansione");
                context.ReadLockedKeys.Add(lockKey);
            }

            return (entries[newIndex].Recno, 1, newIndex);
        }
    }

    public static void Invalidate(ref List<int>? cache, object syncRoot)
    {
        lock (syncRoot)
        {
            cache = null;
        }
    }

    private static int FindKeyIndex(List<(byte[] Key, int Recno)> entries, ReadOnlySpan<byte> keyBytes, out bool exact)
    {
        exact = false;
        var lo = 0;
        var hi = entries.Count - 1;
        var insertAt = entries.Count;

        while (lo <= hi)
        {
            var mid = (lo + hi) >> 1;
            var cmp = keyBytes.SequenceCompareTo(entries[mid].Key);
            if (cmp == 0)
            {
                exact = true;
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
}

#endregion
