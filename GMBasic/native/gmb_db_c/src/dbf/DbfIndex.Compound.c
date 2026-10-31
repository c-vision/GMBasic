/* FILE: native/gmb_db_c/src/DBF/DbfIndex.Compound.c */
/* Port 1:1 di modules/Database/DBF/DbfIndex.Compound.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfIndex.Compound.cs
//
//using System.Text;
//using Basic.Databases;
//
//namespace Basic.DBF;
//
//#region MDX (dBASE IV compound)
//
//internal static class MdxFormat
//{
//    public const int HeaderSize = 512;
//    public const int TagTableOffset = 544;
//    public const int TagEntrySize = 48;
//    public const int TagNameLength = 10;
//}
//
//internal readonly record struct MdxTagDescriptor(string Name, uint HeaderPage, uint RootPage, int KeyLength, string Expression, bool Unique);
//
///// <summary>Parses dBASE IV MDX compound files and exposes per-tag indexes.</summary>
//internal static class MdxCompound
//{
//    public static IReadOnlyList<MdxTagDescriptor> ReadTagTable(string path)
//    {
//        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
//        if (fs.Length < MdxFormat.TagTableOffset + MdxFormat.TagEntrySize)
//        {
//            return [];
//        }
//
//        var header = new byte[MdxFormat.HeaderSize];
//        fs.ReadExactly(header);
//        var tagCount = header[28];
//        if (tagCount == 0)
//        {
//            return [];
//        }
//
//        fs.Position = MdxFormat.TagTableOffset;
//        var table = new byte[tagCount * MdxFormat.TagEntrySize];
//        fs.ReadExactly(table);
//
//        var tags = new List<MdxTagDescriptor>();
//        for (var i = 0; i < tagCount; i++)
//        {
//            var entry = table.AsSpan(i * MdxFormat.TagEntrySize, MdxFormat.TagEntrySize);
//            var headerPage = BitConverter.ToUInt32(entry);
//            if (headerPage == 0)
//            {
//                continue;
//            }
//
//            var name = ReadTagName(entry.Slice(4, MdxFormat.TagNameLength));
//            var tagHeader = ReadBlock(fs, headerPage);
//            var rootPage = BitConverter.ToUInt32(tagHeader);
//            var keyLength = BitConverter.ToUInt16(tagHeader, 12);
//            var unique = tagHeader[23] != 0;
//            var expr = ReadExpression(tagHeader);
//            tags.Add(new MdxTagDescriptor(name, headerPage, rootPage, keyLength, expr, unique));
//        }
//
//        return tags;
//    }
//
//    public static void WriteCompound(
//        string path,
//        string dbfBaseName,
//        IReadOnlyList<(string TagName, string Expression, int KeyLength, bool Unique,
//            IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> Entries)> tags)
//    {
//        var tagTable = new byte[tags.Count * MdxFormat.TagEntrySize];
//        var dataPages = new List<byte[]>();
//        var dataStartOffset = AlignUp(MdxFormat.TagTableOffset + tagTable.Length, NdxFormat.BlockSize);
//        var dataStartBlock = dataStartOffset / NdxFormat.BlockSize;
//
//        for (var i = 0; i < tags.Count; i++)
//        {
//            var tag = tags[i];
//            var headerPageNum = dataStartBlock + (uint)dataPages.Count;
//            dataPages.Add(BuildTagHeaderPage(tag.Expression, tag.KeyLength, tag.Unique, 0));
//
//            var ndxPages = BuildNdxPages(tag.KeyLength, tag.Entries);
//            var rootPage = dataStartBlock + (uint)dataPages.Count;
//            dataPages.AddRange(ndxPages);
//
//            var tagHeader = dataPages[(int)(headerPageNum - dataStartBlock)];
//            BitConverter.GetBytes(rootPage).CopyTo(tagHeader, 0);
//            BitConverter.GetBytes(dataStartBlock + (uint)dataPages.Count).CopyTo(tagHeader, 4);
//
//            var entryOffset = i * MdxFormat.TagEntrySize;
//            BitConverter.GetBytes(headerPageNum).CopyTo(tagTable, entryOffset);
//            WriteTagName(tagTable.AsSpan(entryOffset + 4, MdxFormat.TagNameLength), tag.TagName);
//        }
//
//        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
//        using var fs = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
//        fs.Write(BuildMdxHeader(dbfBaseName, (byte)tags.Count));
//        fs.Write(new byte[MdxFormat.TagTableOffset - MdxFormat.HeaderSize]);
//        fs.Write(tagTable);
//        var pad = dataStartOffset - (MdxFormat.TagTableOffset + tagTable.Length);
//        if (pad > 0)
//        {
//            fs.Write(new byte[pad]);
//        }
//
//        foreach (var page in dataPages)
//        {
//            fs.Write(page);
//        }
//    }
//
//    private static int AlignUp(int value, int alignment) =>
//        (value + alignment - 1) / alignment * alignment;
//
//    private static byte[] BuildMdxHeader(string dbfBaseName, byte tagCount)
//    {
//        var header = new byte[MdxFormat.HeaderSize];
//        header[0] = 2;
//        header[20] = 2;
//        header[28] = tagCount;
//        header[29] = MdxFormat.TagNameLength;
//        var nameBytes = Encoding.ASCII.GetBytes(Path.GetFileNameWithoutExtension(dbfBaseName));
//        nameBytes.AsSpan(0, Math.Min(nameBytes.Length, 16)).CopyTo(header.AsSpan(4));
//        return header;
//    }
//
//    private static byte[] BuildTagHeaderPage(string expression, int keyLength, bool unique, uint rootPage)
//    {
//        var page = new byte[NdxFormat.BlockSize];
//        BitConverter.GetBytes(rootPage).CopyTo(page, 0);
//        BitConverter.GetBytes((ushort)keyLength).CopyTo(page, 12);
//        BitConverter.GetBytes(NdxFormat.MaxKeysPerPage(keyLength)).CopyTo(page, 14);
//        BitConverter.GetBytes((ushort)NdxFormat.GroupLength(keyLength)).CopyTo(page, 18);
//        page[23] = unique ? (byte)1 : (byte)0;
//        var exprBytes = Encoding.ASCII.GetBytes(expression.ToLowerInvariant());
//        exprBytes.AsSpan(0, Math.Min(exprBytes.Length, NdxFormat.ExpressionLength))
//            .CopyTo(page.AsSpan(NdxFormat.ExpressionOffset));
//        return page;
//    }
//
//    private static List<byte[]> BuildNdxPages(int keyLength, IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries)
//    {
//        var tempPath = Path.Combine(Path.GetTempPath(), $"mdx_{Guid.NewGuid():N}.ndx");
//        try
//        {
//            NdxBuilder.Write(tempPath, string.Empty, keyLength, entries, unique: false);
//            using var fs = new FileStream(tempPath, FileMode.Open, FileAccess.Read, FileShare.Read);
//            fs.Position = NdxFormat.BlockSize;
//            var pages = new List<byte[]>();
//            var buf = new byte[NdxFormat.BlockSize];
//            while (fs.Read(buf) == NdxFormat.BlockSize)
//            {
//                pages.Add(buf.ToArray());
//                buf = new byte[NdxFormat.BlockSize];
//            }
//
//            return pages;
//        }
//        finally
//        {
//            if (File.Exists(tempPath))
//            {
//                File.Delete(tempPath);
//            }
//        }
//    }
//
//    private static byte[] ReadBlock(FileStream fs, uint pageNumber)
//    {
//        var buffer = new byte[NdxFormat.BlockSize];
//        fs.Position = NdxFormat.BlockOffset(pageNumber);
//        fs.ReadExactly(buffer);
//        return buffer;
//    }
//
//    private static string ReadTagName(ReadOnlySpan<byte> span)
//    {
//        var len = span.IndexOf((byte)0);
//        if (len < 0)
//        {
//            len = span.Length;
//        }
//
//        return len == 0 ? string.Empty : Encoding.ASCII.GetString(span[..len]).Trim();
//    }
//
//    private static void WriteTagName(Span<byte> dest, string name)
//    {
//        dest.Clear();
//        var bytes = Encoding.ASCII.GetBytes(name);
//        bytes.AsSpan(0, Math.Min(bytes.Length, dest.Length)).CopyTo(dest);
//    }
//
//    private static string ReadExpression(ReadOnlySpan<byte> tagHeader) =>
//        ReadTagName(tagHeader.Slice(NdxFormat.ExpressionOffset, NdxFormat.ExpressionLength));
//}
//
///// <summary>Single tag inside an MDX compound file.</summary>
//internal sealed partial class MdxTagIndex : IWritableIndexHandle
//{
//    private readonly string _compoundPath;
//    private readonly MdxTagDescriptor _descriptor;
//    private readonly ILockManager? _lockManager;
//    private readonly object _syncRoot = new();
//    private readonly List<(byte[] Key, int Recno)> _entries;
//    private readonly bool _writable;
//    private List<int>? _recordOrderCache;
//
//    public string Name { get; }
//    public bool Unique { get; }
//    public bool Descending { get; }
//    public int KeyLength { get; }
//
//    public MdxTagIndex(string compoundPath, MdxTagDescriptor descriptor, ILockManager? lockManager)
//    {
//        _compoundPath = compoundPath;
//        _descriptor = descriptor;
//        _lockManager = lockManager;
//        _writable = lockManager != null;
//        Name = descriptor.Name;
//        Unique = descriptor.Unique;
//        Descending = false;
//        KeyLength = descriptor.KeyLength;
//        _entries = LoadEntries();
//    }
//
//    private List<(byte[] Key, int Recno)> LoadEntries()
//    {
//        if (_descriptor.RootPage == 0)
//        {
//            return [];
//        }
//
//        using var fs = new FileStream(_compoundPath, FileMode.Open, FileAccess.Read, FileShare.Read);
//        var groupLen = NdxFormat.GroupLength(KeyLength);
//        var result = new List<(byte[] Key, int Recno)>();
//        EnumerateBlock(fs, _descriptor.RootPage, _descriptor.KeyLength, groupLen, result);
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
//        var buffer = new byte[NdxFormat.BlockSize];
//        fs.Position = NdxFormat.BlockOffset(blockNumber);
//        fs.ReadExactly(buffer);
//        var page = new NdxPage(buffer, groupLen);
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
//    public string GetKeyExpressionString() => _descriptor.Expression;
//
//    public int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context) =>
//        InMemoryIndexOps.Seek(_entries, _syncRoot, _lockManager, Name, keyBytes, soft, context);
//
//    public (int Recno, byte[]? KeyBytes) SeekWithKeyBytes(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context) =>
//        InMemoryIndexOps.SeekWithKey(_entries, _syncRoot, _lockManager, Name, keyBytes, soft, context);
//
//    public void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
//    {
//        var keyCopy = keyBytes.ToArray();
//        Mutate(context, () => InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, keyCopy, recno));
//    }
//
//    public void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context) =>
//        Mutate(context, () =>
//        {
//            foreach (var (key, recno) in entries)
//            {
//                InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, key.ToArray(), recno);
//            }
//        });
//
//    public void Delete(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
//    {
//        var keyCopy = keyBytes.ToArray();
//        Mutate(context, () => InMemoryIndexOps.Delete(_entries, _syncRoot, keyCopy, recno));
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
//                InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, key.ToArray(), recno);
//            }
//
//            _recordOrderCache = null;
//            RewriteCompound();
//        }
//    }
//
//    public int GetFirstRecordNumber() => InMemoryIndexOps.GetFirst(_entries, _syncRoot);
//    public int GetLastRecordNumber() => InMemoryIndexOps.GetLast(_entries, _syncRoot);
//    public IEnumerable<int> GetAllRecordNumbers() => InMemoryIndexOps.GetAll(_entries, _syncRoot, ref _recordOrderCache);
//
//    public (int recno, uint pageNum, int indexInLeaf) GetFirstRecordPosition(TransactionContext? context) =>
//        InMemoryIndexOps.FirstPosition(_entries, _syncRoot, _lockManager, Name, context);
//
//    public (int recno, uint pageNum, int indexInLeaf) GetLastRecordPosition(TransactionContext? context) =>
//        InMemoryIndexOps.LastPosition(_entries, _syncRoot, _lockManager, Name, context);
//
//    public (int recno, uint newPageNum, int newIndexInLeaf) GetNextRecord(
//        uint currentPageNum, int currentIndexInLeaf, int direction, TransactionContext? context) =>
//        InMemoryIndexOps.NextRecord(_entries, _syncRoot, _lockManager, Name, currentPageNum, currentIndexInLeaf, direction, context);
//
//    public void InvalidateCache() => InMemoryIndexOps.Invalidate(ref _recordOrderCache, _syncRoot);
//
//    public void Dispose()
//    {
//    }
//
//    private void Mutate(TransactionContext? context, Action action)
//    {
//        EnsureWritable();
//        lock (_syncRoot)
//        {
//            action();
//            _recordOrderCache = null;
//            RewriteCompound();
//        }
//    }
//
//    private void RewriteCompound()
//    {
//        // ponytail: rebuild entire MDX from all tags — reload others from disk first
//        var allTags = MdxCompound.ReadTagTable(_compoundPath);
//        var rebuilt = new List<(string, string, int, bool, IReadOnlyList<(ReadOnlyMemory<byte>, int)>)>();
//        foreach (var tag in allTags)
//        {
//            if (tag.Name.Equals(Name, StringComparison.OrdinalIgnoreCase))
//            {
//                rebuilt.Add((Name, _descriptor.Expression, KeyLength, Unique,
//                    _entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
//            }
//            else
//            {
//                var other = new MdxTagIndex(_compoundPath, tag, null);
//                rebuilt.Add((tag.Name, tag.Expression, tag.KeyLength, tag.Unique,
//                    other._entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
//            }
//        }
//
//        var dbfName = Path.GetFileNameWithoutExtension(_compoundPath);
//        MdxCompound.WriteCompound(_compoundPath, dbfName, rebuilt);
//    }
//
//    private void EnsureWritable()
//    {
//        if (!_writable)
//        {
//            throw new NotSupportedException($"Il tag MDX '{Name}' è aperto in sola lettura.");
//        }
//    }
//}
//
//#endregion
//
//#region CDX (FoxPro compound compact)
//
//internal static class CdxFormat
//{
//    public const int PageSize = 512;
//    public const int HeaderSize = 512;
//    public const int ExtNodeHeaderSize = 24;
//    public const int IntNodeHeaderSize = 12;
//    public const byte TypeCompound = 0x40;
//    public const byte TypeCompact = 0x20;
//    public const int TagNameLength = 10;
//}
//
//internal readonly record struct CdxTagDescriptor(
//    string Name, uint RootOffset, int KeyLength, string Expression, bool Unique, bool Descending);
//
///// <summary>Parses FoxPro CDX compound files.</summary>
//internal static class CdxCompound
//{
//    public static IReadOnlyList<CdxTagDescriptor> ReadTagTable(string path)
//    {
//        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
//        if (fs.Length < CdxFormat.HeaderSize)
//        {
//            return [];
//        }
//
//        var header = new byte[CdxFormat.HeaderSize];
//        fs.ReadExactly(header);
//        if ((header[14] & CdxFormat.TypeCompound) == 0)
//        {
//            return [];
//        }
//
//        var rootOffset = BitConverter.ToUInt32(header);
//        if (rootOffset == 0 || rootOffset >= fs.Length)
//        {
//            return [];
//        }
//
//        return EnumerateCatalog(fs, rootOffset);
//    }
//
//    private static List<CdxTagDescriptor> EnumerateCatalog(FileStream fs, uint rootOffset)
//    {
//        var tags = new List<CdxTagDescriptor>();
//        WalkCatalogNode(fs, rootOffset, tags);
//        return tags;
//    }
//
//    private static void WalkCatalogNode(FileStream fs, uint offset, List<CdxTagDescriptor> tags)
//    {
//        var page = ReadPage(fs, offset);
//        var attr = BitConverter.ToUInt16(page);
//        var keyCount = BitConverter.ToUInt16(page, 2);
//        var isLeaf = (attr & 2) != 0;
//
//        if (isLeaf)
//        {
//            var decoded = CdxCompactDecoder.DecodeLeafKeys(page, keyCount, CdxFormat.TagNameLength);
//            foreach (var entry in decoded)
//            {
//                var name = Encoding.ASCII.GetString(entry.Key).TrimEnd('\0', ' ');
//                if (string.IsNullOrEmpty(name))
//                {
//                    continue;
//                }
//
//                var tagHeader = ReadPage(fs, entry.Recno);
//                var tagRootOffset = BitConverter.ToUInt32(tagHeader);
//                var tagKeyLen = BitConverter.ToUInt16(tagHeader, 12);
//                var unique = (tagHeader[14] & 0x01) != 0;
//                var descending = tagHeader[502] != 0;
//                var expr = ReadCdxExpression(tagHeader);
//                tags.Add(new CdxTagDescriptor(name, tagRootOffset, tagKeyLen, expr, unique, descending));
//            }
//
//            return;
//        }
//
//        for (var i = 0; i < keyCount; i++)
//        {
//            var childOffset = CdxCompactDecoder.GetInteriorChild(page, i, keyCount, CdxFormat.TagNameLength);
//            if (childOffset != 0 && childOffset != uint.MaxValue)
//            {
//                WalkCatalogNode(fs, childOffset, tags);
//            }
//        }
//
//        var rightChild = CdxCompactDecoder.GetInteriorChild(page, keyCount, keyCount, CdxFormat.TagNameLength);
//        if (rightChild != 0 && rightChild != uint.MaxValue)
//        {
//            WalkCatalogNode(fs, rightChild, tags);
//        }
//    }
//
//    public static void WriteCompound(
//        string path,
//        IReadOnlyList<(string TagName, string Expression, int KeyLength, bool Unique, bool Descending,
//            IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> Entries)> tags)
//    {
//        var pages = new List<byte[]>();
//        var catalogLeaves = new List<(string Name, uint TagHeaderOffset)>();
//
//        foreach (var tag in tags)
//        {
//            var tagHeaderOffset = (uint)(pages.Count * CdxFormat.PageSize + CdxFormat.HeaderSize);
//            pages.Add(BuildCdxTagHeader(tag.Expression, tag.KeyLength, tag.Unique, tag.Descending, 0));
//
//            var treeBase = tagHeaderOffset + CdxFormat.PageSize;
//            var (tagPages, tagRootPageIndex) = BuildCdxTagPages(tag.KeyLength, tag.Entries, treeBase);
//            var tagRoot = treeBase + (uint)(tagRootPageIndex * CdxFormat.PageSize);
//            pages.AddRange(tagPages);
//            var tagHeader = pages[(int)((tagHeaderOffset - CdxFormat.HeaderSize) / CdxFormat.PageSize)];
//            BitConverter.GetBytes(tagRoot).CopyTo(tagHeader, 0);
//            catalogLeaves.Add((tag.TagName, tagHeaderOffset));
//        }
//
//        var catalogRoot = BuildCatalogTree(pages, catalogLeaves);
//        var header = BuildCdxCompoundHeader(catalogRoot, tags.FirstOrDefault().KeyLength);
//
//        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
//        using var fs = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
//        fs.Write(header);
//        foreach (var page in pages)
//        {
//            fs.Write(page);
//        }
//    }
//
//    private static byte[] BuildCdxCompoundHeader(uint catalogRoot, int keyLength)
//    {
//        var header = new byte[CdxFormat.HeaderSize];
//        BitConverter.GetBytes(catalogRoot).CopyTo(header, 0);
//        BitConverter.GetBytes((ushort)keyLength).CopyTo(header, 12);
//        header[14] = (byte)(CdxFormat.TypeCompact | CdxFormat.TypeCompound);
//        return header;
//    }
//
//    private static byte[] BuildCdxTagHeader(string expression, int keyLength, bool unique, bool descending, uint rootOffset)
//    {
//        const int exprOffset = 16;
//        var header = new byte[CdxFormat.PageSize];
//        BitConverter.GetBytes(rootOffset).CopyTo(header, 0);
//        BitConverter.GetBytes((ushort)keyLength).CopyTo(header, 12);
//        header[14] = (byte)((unique ? 0x01 : 0x00) | CdxFormat.TypeCompact);
//        header[502] = descending ? (byte)1 : (byte)0;
//        var exprBytes = Encoding.ASCII.GetBytes(expression);
//        var keyExpLen = Math.Min(exprBytes.Length, 510 - exprOffset);
//        BitConverter.GetBytes((ushort)keyExpLen).CopyTo(header, 510);
//        exprBytes.AsSpan(0, keyExpLen).CopyTo(header.AsSpan(exprOffset));
//        return header;
//    }
//
//    private static (List<byte[]> Pages, int RootPageIndex) BuildCdxTagPages(
//        int keyLength,
//        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries,
//        uint treeBaseOffset)
//    {
//        var sorted = entries
//            .Select(e => (Key: CdxCompactEncoder.PadIndexKey(e.Key.ToArray(), keyLength), e.Recno))
//            .OrderBy(e => e.Key, Comparer<byte[]>.Create(static (a, b) => a.AsSpan().SequenceCompareTo(b)))
//            .ToList();
//
//        if (sorted.Count == 0)
//        {
//            return ([CdxCompactEncoder.BuildEmptyLeaf(keyLength)], 0);
//        }
//
//        var maxLeaf = CdxCompactEncoder.MaxKeysPerLeaf(keyLength);
//        var maxBranch = CdxCompactEncoder.MaxKeysPerInterior(keyLength);
//        var pages = new List<byte[]>();
//        var leaves = new List<(int PageIndex, byte[] PromoteKey)>();
//
//        for (var i = 0; i < sorted.Count; i += maxLeaf)
//        {
//            var chunk = sorted.Skip(i).Take(maxLeaf).ToList();
//            pages.Add(CdxCompactEncoder.BuildLeaf(chunk, keyLength));
//            leaves.Add((pages.Count - 1, chunk[0].Key));
//        }
//
//        if (leaves.Count == 1)
//        {
//            return (pages, leaves[0].PageIndex);
//        }
//
//        var level = leaves;
//        while (level.Count > 1)
//        {
//            level = BuildCdxInteriorLevel(level, pages, keyLength, maxBranch, treeBaseOffset);
//        }
//
//        return (pages, level[0].PageIndex);
//    }
//
//    private static List<(int PageIndex, byte[] PromoteKey)> BuildCdxInteriorLevel(
//        List<(int PageIndex, byte[] PromoteKey)> children,
//        List<byte[]> pages,
//        int keyLength,
//        int maxBranchKeys,
//        uint treeBaseOffset)
//    {
//        var parents = new List<(int PageIndex, byte[] PromoteKey)>();
//        var index = 0;
//
//        while (index < children.Count)
//        {
//            var groupStart = index;
//            var childPageIndices = new List<int> { children[index].PageIndex };
//            var separators = new List<byte[]>();
//            index++;
//
//            while (index < children.Count && separators.Count < maxBranchKeys)
//            {
//                separators.Add(children[index].PromoteKey);
//                childPageIndices.Add(children[index].PageIndex);
//                index++;
//            }
//
//            var childOffsets = childPageIndices
//                .Select(i => treeBaseOffset + (uint)(i * CdxFormat.PageSize))
//                .ToList();
//            pages.Add(CdxCompactEncoder.BuildInteriorPage(childOffsets, separators, keyLength));
//            parents.Add((pages.Count - 1, separators.Count > 0 ? separators[0] : children[groupStart].PromoteKey));
//        }
//
//        return parents;
//    }
//
//    private static uint BuildCatalogTree(List<byte[]> pages, List<(string Name, uint TagHeaderOffset)> leaves)
//    {
//        var leafPage = CdxCompactEncoder.BuildCatalogLeaf(leaves);
//        pages.Add(leafPage);
//        return (uint)(CdxFormat.HeaderSize + (pages.Count - 1) * CdxFormat.PageSize);
//    }
//
//    private static IEnumerable<(string Name, uint TagHeaderOffset)> ReadCatalogLeaf(byte[] page, int keyCount)
//    {
//        var keyLen = CdxFormat.TagNameLength;
//        var keysStart = CdxFormat.PageSize - ((keyCount + 1) * (keyLen + 4));
//        for (var i = 0; i < keyCount; i++)
//        {
//            var offset = keysStart + (i * (keyLen + 4));
//            var name = Encoding.ASCII.GetString(page, offset, keyLen).TrimEnd('\0', ' ');
//            var tagHeaderOffset = BitConverter.ToUInt32(page, offset + keyLen);
//            yield return (name, tagHeaderOffset);
//        }
//    }
//
//    private static byte[] ReadPage(FileStream fs, uint offset)
//    {
//        var buffer = new byte[CdxFormat.PageSize];
//        fs.Position = offset;
//        fs.ReadExactly(buffer);
//        return buffer;
//    }
//
//    private static string ReadCdxExpression(ReadOnlySpan<byte> tagHeader)
//    {
//        const int exprOffset = 16;
//        var keyExpLen = BitConverter.ToUInt16(tagHeader.Slice(510, 2));
//        if (keyExpLen == 0)
//        {
//            return string.Empty;
//        }
//
//        return Encoding.ASCII.GetString(tagHeader.Slice(exprOffset, Math.Min((int)keyExpLen, 510 - exprOffset)));
//    }
//}
//
///// <summary>Compact CDX key decode/encode helpers.</summary>
//internal static class CdxCompactDecoder
//{
//    internal readonly record struct DecodedKey(byte[] Key, uint Recno);
//
//    public static List<DecodedKey> DecodeLeafKeys(byte[] page, int keyCount, int keyLength)
//    {
//        var recBits = page[20];
//        var dupBits = page[21];
//        var trlBits = page[22];
//        var reqByte = page[23];
//        if (reqByte == 0)
//        {
//            reqByte = 4;
//        }
//
//        var dcMask = (ushort)((1 << dupBits) - 1);
//        var tcMask = (ushort)((1 << trlBits) - 1);
//        var pool = page.AsSpan(CdxFormat.ExtNodeHeaderSize);
//        var keyPos = CdxFormat.PageSize;
//        var prevKey = new byte[keyLength];
//        var results = new List<DecodedKey>();
//
//        for (var i = 0; i < keyCount; i++)
//        {
//            var pos = i * reqByte;
//            var packed = BitConverter.ToUInt32(pool.Slice(pos, Math.Min((int)reqByte, 4)));
//            var dup = dupBits > 0 ? (int)(packed & dcMask) : 0;
//            var trail = trlBits > 0 ? (int)((packed >> dupBits) & tcMask) : 0;
//            var recno = recBits > 0 ? packed >> (dupBits + trlBits) : 0u;
//
//            var varLen = keyLength - dup - trail;
//            keyPos -= varLen;
//            var key = new byte[keyLength];
//            prevKey.AsSpan(0, dup).CopyTo(key);
//            if (varLen > 0)
//            {
//                page.AsSpan(keyPos, varLen).CopyTo(key.AsSpan(dup));
//            }
//
//            if (trail > 0)
//            {
//                key.AsSpan(keyLength - trail, trail).Fill((byte)' ');
//            }
//
//            prevKey = key;
//            results.Add(new DecodedKey(key, recno));
//        }
//
//        return results;
//    }
//
//    public static uint GetInteriorChild(byte[] page, int keyIndex, int keyCount, int keyLength)
//    {
//        var keysStart = CdxFormat.PageSize - ((keyCount + 1) * (keyLength + 4));
//        var offset = keysStart + (keyIndex * (keyLength + 4)) + keyLength;
//        if (offset + 4 > CdxFormat.PageSize)
//        {
//            return 0;
//        }
//
//        return BitConverter.ToUInt32(page, offset);
//    }
//}
//
//internal static class CdxCompactEncoder
//{
//    public static byte[] BuildEmptyLeaf(int keyLength)
//    {
//        var page = new byte[CdxFormat.PageSize];
//        BitConverter.GetBytes((ushort)2).CopyTo(page, 0);
//        BitConverter.GetBytes((ushort)0).CopyTo(page, 2);
//        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 4);
//        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 8);
//        page[20] = 20;
//        page[21] = 4;
//        page[22] = 4;
//        page[23] = 4;
//        _ = keyLength;
//        return page;
//    }
//
//    public static byte[] BuildLeaf(List<(byte[] Key, int Recno)> entries, int keyLength)
//    {
//        var page = new byte[CdxFormat.PageSize];
//        BitConverter.GetBytes((ushort)3).CopyTo(page, 0);
//        BitConverter.GetBytes((ushort)entries.Count).CopyTo(page, 2);
//        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 4);
//        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 8);
//        page[20] = 20;
//        page[21] = 4;
//        page[22] = 4;
//        page[23] = 4;
//
//        var prevKey = new byte[keyLength];
//        var keyPos = CdxFormat.PageSize;
//        var poolOffset = CdxFormat.ExtNodeHeaderSize;
//
//        for (var i = 0; i < entries.Count; i++)
//        {
//            var (keyRaw, recno) = entries[i];
//            var key = PadIndexKey(keyRaw, keyLength);
//            var dup = CountDuplicatePrefix(prevKey, key);
//            var trail = CountTrailingSpaces(key, keyLength, dup);
//            var varLen = keyLength - dup - trail;
//            if (varLen < 0 || dup + varLen > key.Length)
//            {
//                throw new InvalidOperationException(
//                    $"CDX compact leaf key encoding overflow (keyLength={keyLength}, dup={dup}, trail={trail}).");
//            }
//
//            keyPos -= varLen;
//            if (keyPos < poolOffset + (entries.Count * 4))
//            {
//                throw new InvalidOperationException(
//                    $"CDX compact leaf page overflow ({entries.Count} keys, keyLength={keyLength}).");
//            }
//
//            key.AsSpan(dup, varLen).CopyTo(page.AsSpan(keyPos));
//
//            var packed = (uint)recno << 8 | (uint)(trail << 4) | (uint)dup;
//            BitConverter.GetBytes(packed).CopyTo(page, poolOffset + (i * 4));
//            prevKey = key;
//        }
//
//        return page;
//    }
//
//    public static int MaxKeysPerLeaf(int keyLength)
//    {
//        var lo = 1;
//        var hi = 256;
//        while (lo < hi)
//        {
//            var mid = (lo + hi + 1) / 2;
//            if (CanFitLeaf(mid, keyLength))
//            {
//                lo = mid;
//            }
//            else
//            {
//                hi = mid - 1;
//            }
//        }
//
//        return lo;
//    }
//
//    public static int MaxKeysPerInterior(int keyLength)
//    {
//        var slot = keyLength + 4;
//        return Math.Max(1, (CdxFormat.PageSize - CdxFormat.IntNodeHeaderSize) / slot - 1);
//    }
//
//    public static byte[] BuildInteriorPage(
//        IReadOnlyList<uint> childOffsets,
//        IReadOnlyList<byte[]> separatorKeys,
//        int keyLength)
//    {
//        if (childOffsets.Count != separatorKeys.Count + 1)
//        {
//            throw new ArgumentException("CDX interior page requires one more child than separator keys.");
//        }
//
//        var keyCount = separatorKeys.Count;
//        var page = new byte[CdxFormat.PageSize];
//        BitConverter.GetBytes((ushort)1).CopyTo(page, 0);
//        BitConverter.GetBytes((ushort)keyCount).CopyTo(page, 2);
//        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 4);
//        BitConverter.GetBytes(uint.MaxValue).CopyTo(page, 8);
//
//        var keysStart = CdxFormat.PageSize - ((keyCount + 1) * (keyLength + 4));
//        for (var i = 0; i < keyCount; i++)
//        {
//            var slot = keysStart + (i * (keyLength + 4));
//            WritePaddedKey(page.AsSpan(slot, keyLength), separatorKeys[i], keyLength);
//            BitConverter.GetBytes(childOffsets[i]).CopyTo(page, slot + keyLength);
//        }
//
//        BitConverter.GetBytes(childOffsets[keyCount]).CopyTo(page, keysStart + (keyCount * (keyLength + 4)) + keyLength);
//        return page;
//    }
//
//    private static bool CanFitLeaf(int count, int keyLength)
//    {
//        var keyPos = CdxFormat.PageSize;
//        var poolEnd = CdxFormat.ExtNodeHeaderSize + (count * 4);
//        for (var i = 0; i < count; i++)
//        {
//            keyPos -= keyLength;
//            if (keyPos < poolEnd)
//            {
//                return false;
//            }
//        }
//
//        return true;
//    }
//
//    private static void WritePaddedKey(Span<byte> dest, byte[] key, int keyLength)
//    {
//        dest.Clear();
//        var copyLen = Math.Min(key.Length, keyLength);
//        key.AsSpan(0, copyLen).CopyTo(dest);
//    }
//
//    public static byte[] BuildCatalogLeaf(List<(string Name, uint TagHeaderOffset)> tags)
//    {
//        var entries = tags
//            .Select(t =>
//            {
//                var key = new byte[CdxFormat.TagNameLength];
//                Encoding.ASCII.GetBytes(t.Name).AsSpan(0, Math.Min(t.Name.Length, key.Length)).CopyTo(key);
//                return (Key: key, Recno: (int)t.TagHeaderOffset);
//            })
//            .OrderBy(e => e.Key, Comparer<byte[]>.Create(static (a, b) => a.AsSpan().SequenceCompareTo(b)))
//            .Select(e => (e.Key, e.Recno))
//            .ToList();
//
//        return BuildLeaf(entries, CdxFormat.TagNameLength);
//    }
//
//    private static int CountDuplicatePrefix(byte[] prev, byte[] key)
//    {
//        var max = Math.Min(prev.Length, key.Length);
//        var i = 0;
//        while (i < max && prev[i] == key[i])
//        {
//            i++;
//        }
//
//        return i;
//    }
//
//    private static int CountTrailingSpaces(byte[] key, int keyLength, int dup = 0)
//    {
//        var trail = 0;
//        var scanEnd = Math.Min(key.Length, keyLength);
//        for (var i = scanEnd - 1; i >= dup; i--)
//        {
//            if (key[i] != (byte)' ')
//            {
//                break;
//            }
//
//            trail++;
//        }
//
//        if (key.Length < keyLength)
//        {
//            trail += keyLength - Math.Max(key.Length, dup);
//        }
//
//        return trail;
//    }
//
//    internal static byte[] PadIndexKey(byte[] key, int keyLength)
//    {
//        if (key.Length == keyLength)
//        {
//            return key;
//        }
//
//        var padded = new byte[keyLength];
//        var copyLen = Math.Min(key.Length, keyLength);
//        key.AsSpan(0, copyLen).CopyTo(padded);
//        if (copyLen < keyLength)
//        {
//            padded.AsSpan(copyLen).Fill((byte)' ');
//        }
//
//        return padded;
//    }
//}
//
///// <summary>Single tag inside a FoxPro CDX compound file.</summary>
//internal sealed partial class CdxTagIndex : IWritableIndexHandle
//{
//    private readonly string _compoundPath;
//    private readonly CdxTagDescriptor _descriptor;
//    private readonly ILockManager? _lockManager;
//    private readonly object _syncRoot = new();
//    private readonly List<(byte[] Key, int Recno)> _entries;
//    private readonly bool _writable;
//    private List<int>? _recordOrderCache;
//
//    public string Name { get; }
//    public bool Unique { get; }
//    public bool Descending { get; }
//    public int KeyLength { get; }
//
//    public CdxTagIndex(string compoundPath, CdxTagDescriptor descriptor, ILockManager? lockManager)
//    {
//        _compoundPath = compoundPath;
//        _descriptor = descriptor;
//        _lockManager = lockManager;
//        _writable = lockManager != null;
//        Name = descriptor.Name;
//        Unique = descriptor.Unique;
//        Descending = descriptor.Descending;
//        KeyLength = descriptor.KeyLength;
//        _entries = LoadEntries();
//        if (_writable) EnablePageCache();
//    }
//
//    private List<(byte[] Key, int Recno)> LoadEntries()
//    {
//        if (_descriptor.RootOffset == 0)
//        {
//            return [];
//        }
//
//        using var fs = new FileStream(_compoundPath, FileMode.Open, FileAccess.Read, FileShare.Read);
//        return WalkTagTree(fs, _descriptor.RootOffset);
//    }
//
//    private List<(byte[] Key, int Recno)> WalkTagTree(FileStream fs, uint offset)
//    {
//        var page = new byte[CdxFormat.PageSize];
//        fs.Position = offset;
//        fs.ReadExactly(page);
//        var attr = BitConverter.ToUInt16(page);
//        var keyCount = BitConverter.ToUInt16(page, 2);
//        var isLeaf = (attr & 2) != 0;
//
//        if (isLeaf)
//        {
//            return CdxCompactDecoder.DecodeLeafKeys(page, keyCount, KeyLength)
//                .Select(k => (k.Key, (int)k.Recno))
//                .ToList();
//        }
//
//        var result = new List<(byte[] Key, int Recno)>();
//        for (var i = 0; i <= keyCount; i++)
//        {
//            var child = CdxCompactDecoder.GetInteriorChild(page, i, keyCount, KeyLength);
//            if (child != 0 && child != uint.MaxValue)
//            {
//                result.AddRange(WalkTagTree(fs, child));
//            }
//        }
//
//        return result;
//    }
//
//    public string GetKeyExpressionString() => _descriptor.Expression;
//
//    public int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
//    {
//        // Bug DbfEngine-Seek-Chiave-Formato (09-Errors/GMBasicAI/DbfEngine-Seek-Chiave-Formato-Bug.md):
//        // EnablePageCache() (chiamata dal costruttore quando _writable=true, es. DBF USE ...
//        // EXCLUSIVE) svuota _entries per il lazy-load on-demand, ma EnsureEntriesLoaded() era
//        // invocato SOLO da DeleteWithCache — mai raggiunto da Seek (né da Insert/Delete
//        // "semplici" via Mutate, vedi sotto). Seek cercava quindi SEMPRE in una lista vuota,
//        // found=False qualunque fosse il formato della chiave provata — NON era affatto un
//        // problema di formato/collation dei byte (CdxCompactDecoder decodifica correttamente,
//        // verificato leggendo i 29 tag PRIMARY del fixture: recno 1..29, chiave " N" + padding).
//        EnsureEntriesLoaded();
//        return InMemoryIndexOps.Seek(_entries, _syncRoot, _lockManager, Name, keyBytes, soft, context);
//    }
//
//    public (int Recno, byte[]? KeyBytes) SeekWithKeyBytes(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
//    {
//        EnsureEntriesLoaded();
//        return InMemoryIndexOps.SeekWithKey(_entries, _syncRoot, _lockManager, Name, keyBytes, soft, context);
//    }
//
//    public void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
//    {
//        var keyCopy = keyBytes.ToArray();
//        Mutate(context, () => InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, keyCopy, recno));
//    }
//
//    public void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context) =>
//        Mutate(context, () =>
//        {
//            foreach (var (key, recno) in entries)
//            {
//                InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, key.ToArray(), recno);
//            }
//        });
//
//    public void Delete(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
//    {
//        var keyCopy = keyBytes.ToArray();
//        Mutate(context, () => InMemoryIndexOps.Delete(_entries, _syncRoot, keyCopy, recno));
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
//                InMemoryIndexOps.Insert(_entries, _syncRoot, Unique, Name, key.ToArray(), recno);
//            }
//
//            _recordOrderCache = null;
//            RewriteCompound();
//        }
//    }
//
//    // Stesso bug di Seek sopra: tutti questi leggono _entries direttamente — senza
//    // EnsureEntriesLoaded() restano vuoti su ogni tag CDX aperto in modalità writable
//    // (page cache attiva). Vedi 09-Errors/GMBasicAI/DbfEngine-Seek-Chiave-Formato-Bug.md.
//    public int GetFirstRecordNumber() { EnsureEntriesLoaded(); return InMemoryIndexOps.GetFirst(_entries, _syncRoot); }
//    public int GetLastRecordNumber() { EnsureEntriesLoaded(); return InMemoryIndexOps.GetLast(_entries, _syncRoot); }
//    public IEnumerable<int> GetAllRecordNumbers() { EnsureEntriesLoaded(); return InMemoryIndexOps.GetAll(_entries, _syncRoot, ref _recordOrderCache); }
//
//    public (int recno, uint pageNum, int indexInLeaf) GetFirstRecordPosition(TransactionContext? context)
//    {
//        EnsureEntriesLoaded();
//        return InMemoryIndexOps.FirstPosition(_entries, _syncRoot, _lockManager, Name, context);
//    }
//
//    public (int recno, uint pageNum, int indexInLeaf) GetLastRecordPosition(TransactionContext? context)
//    {
//        EnsureEntriesLoaded();
//        return InMemoryIndexOps.LastPosition(_entries, _syncRoot, _lockManager, Name, context);
//    }
//
//    public (int recno, uint newPageNum, int newIndexInLeaf) GetNextRecord(
//        uint currentPageNum, int currentIndexInLeaf, int direction, TransactionContext? context)
//    {
//        EnsureEntriesLoaded();
//        return InMemoryIndexOps.NextRecord(_entries, _syncRoot, _lockManager, Name, currentPageNum, currentIndexInLeaf, direction, context);
//    }
//
//    public void InvalidateCache() => InMemoryIndexOps.Invalidate(ref _recordOrderCache, _syncRoot);
//
//    public void Dispose()
//    {
//    }
//
//    private void Mutate(TransactionContext? context, Action action)
//    {
//        EnsureWritable();
//        _ = context;
//        lock (_syncRoot)
//        {
//            // Stesso bug di Seek: Insert/InsertBatch/Delete (i chiamanti di Mutate) operano su
//            // _entries direttamente — senza questa chiamata partirebbero da una lista vuota su
//            // un tag con page cache attiva, perdendo silenziosamente tutte le chiavi esistenti.
//            EnsureEntriesLoaded();
//            action();
//            _recordOrderCache = null;
//            if (_pageCache != null)
//                FlushCompoundIncremental();
//            else
//                RewriteCompound();
//        }
//    }
//
//    private void RewriteCompound()
//    {
//        var allTags = CdxCompound.ReadTagTable(_compoundPath);
//        var rebuilt = new List<(string, string, int, bool, bool, IReadOnlyList<(ReadOnlyMemory<byte>, int)>)>();
//        foreach (var tag in allTags)
//        {
//            if (tag.Name.Equals(Name, StringComparison.OrdinalIgnoreCase))
//            {
//                rebuilt.Add((Name, _descriptor.Expression, KeyLength, Unique, Descending,
//                    _entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
//            }
//            else
//            {
//                var other = new CdxTagIndex(_compoundPath, tag, null);
//                rebuilt.Add((tag.Name, tag.Expression, tag.KeyLength, tag.Unique, tag.Descending,
//                    other._entries.Select(e => ((ReadOnlyMemory<byte>)e.Key, e.Recno)).ToList()));
//            }
//        }
//
//        CdxCompound.WriteCompound(_compoundPath, rebuilt);
//    }
//
//    private void EnsureWritable()
//    {
//        if (!_writable)
//        {
//            throw new NotSupportedException($"Il tag CDX '{Name}' è aperto in sola lettura.");
//        }
//    }
//}
//
///// <summary>Shared in-memory sorted-index operations for ponytail writable compound tags.</summary>
//internal static class InMemoryIndexOps
//{
//    public static int Seek(
//        List<(byte[] Key, int Recno)> entries,
//        object syncRoot,
//        ILockManager? lockManager,
//        string name,
//        ReadOnlySpan<byte> keyBytes,
//        bool soft,
//        TransactionContext? context)
//    {
//        if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
//        {
//            var lockKey = LockManagerBase.GetKeyLockKey(name, keyBytes.ToArray());
//            lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{name}'");
//            context.ReadLockedKeys.Add(lockKey);
//        }
//
//        lock (syncRoot)
//        {
//            if (entries.Count == 0)
//            {
//                return -1;
//            }
//
//            var idx = FindKeyIndex(entries, keyBytes, out var exact);
//            if (exact)
//            {
//                return entries[idx].Recno;
//            }
//
//            return soft && idx < entries.Count ? entries[idx].Recno : -1;
//        }
//    }
//
//    public static (int Recno, byte[]? KeyBytes) SeekWithKey(
//        List<(byte[] Key, int Recno)> entries,
//        object syncRoot,
//        ILockManager? lockManager,
//        string name,
//        ReadOnlySpan<byte> keyBytes,
//        bool soft,
//        TransactionContext? context)
//    {
//        if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
//        {
//            var lockKey = LockManagerBase.GetKeyLockKey(name, keyBytes.ToArray());
//            lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{name}'");
//            context.ReadLockedKeys.Add(lockKey);
//        }
//
//        lock (syncRoot)
//        {
//            if (entries.Count == 0)
//            {
//                return (-1, null);
//            }
//
//            var idx = FindKeyIndex(entries, keyBytes, out var exact);
//            if (exact)
//            {
//                return (entries[idx].Recno, entries[idx].Key);
//            }
//
//            return soft && idx < entries.Count ? (entries[idx].Recno, entries[idx].Key) : (-1, null);
//        }
//    }
//
//    public static void Insert(
//        List<(byte[] Key, int Recno)> entries,
//        object syncRoot,
//        bool unique,
//        string name,
//        byte[] keyBytes,
//        int recno)
//    {
//        lock (syncRoot)
//        {
//            var idx = FindKeyIndex(entries, keyBytes, out var exact);
//            if (unique && exact)
//            {
//                throw new InvalidOperationException($"Chiave duplicata nell'indice '{name}'.");
//            }
//
//            entries.Insert(idx, (keyBytes, recno));
//        }
//    }
//
//    public static void Delete(
//        List<(byte[] Key, int Recno)> entries,
//        object syncRoot,
//        byte[] keyBytes,
//        int recno)
//    {
//        lock (syncRoot)
//        {
//            var idx = FindKeyIndex(entries, keyBytes, out var exact);
//            if (!exact)
//            {
//                return;
//            }
//
//            for (var i = idx; i < entries.Count; i++)
//            {
//                if (entries[i].Key.AsSpan().SequenceCompareTo(keyBytes) != 0)
//                {
//                    break;
//                }
//
//                if (entries[i].Recno == recno)
//                {
//                    entries.RemoveAt(i);
//                    return;
//                }
//            }
//        }
//    }
//
//    public static int GetFirst(List<(byte[] Key, int Recno)> entries, object syncRoot)
//    {
//        lock (syncRoot)
//        {
//            return entries.Count > 0 ? entries[0].Recno : 0;
//        }
//    }
//
//    public static int GetLast(List<(byte[] Key, int Recno)> entries, object syncRoot)
//    {
//        lock (syncRoot)
//        {
//            return entries.Count > 0 ? entries[^1].Recno : 0;
//        }
//    }
//
//    public static IEnumerable<int> GetAll(
//        List<(byte[] Key, int Recno)> entries,
//        object syncRoot,
//        ref List<int>? cache)
//    {
//        lock (syncRoot)
//        {
//            cache ??= entries.Select(e => e.Recno).ToList();
//            return cache.ToList();
//        }
//    }
//
//    public static (int recno, uint pageNum, int indexInLeaf) FirstPosition(
//        List<(byte[] Key, int Recno)> entries,
//        object syncRoot,
//        ILockManager? lockManager,
//        string name,
//        TransactionContext? context)
//    {
//        lock (syncRoot)
//        {
//            if (entries.Count == 0)
//            {
//                return (0, 0, 0);
//            }
//
//            if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
//            {
//                var lockKey = LockManagerBase.GetKeyLockKey(name, entries[0].Key);
//                lockManager.AcquireReadLock(lockKey, 5000, $"prima chiave dell'indice '{name}'");
//                context.ReadLockedKeys.Add(lockKey);
//            }
//
//            return (entries[0].Recno, 1, 0);
//        }
//    }
//
//    public static (int recno, uint newPageNum, int newIndexInLeaf) LastPosition(
//        List<(byte[] Key, int Recno)> entries,
//        object syncRoot,
//        ILockManager? lockManager,
//        string name,
//        TransactionContext? context)
//    {
//        lock (syncRoot)
//        {
//            if (entries.Count == 0)
//            {
//                return (0, 0, 0);
//            }
//
//            var last = entries.Count - 1;
//            if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
//            {
//                var lockKey = LockManagerBase.GetKeyLockKey(name, entries[last].Key);
//                lockManager.AcquireReadLock(lockKey, 5000, $"ultima chiave dell'indice '{name}'");
//                context.ReadLockedKeys.Add(lockKey);
//            }
//
//            return (entries[last].Recno, 1, last);
//        }
//    }
//
//    public static (int recno, uint newPageNum, int newIndexInLeaf) NextRecord(
//        List<(byte[] Key, int Recno)> entries,
//        object syncRoot,
//        ILockManager? lockManager,
//        string name,
//        uint currentPageNum,
//        int currentIndexInLeaf,
//        int direction,
//        TransactionContext? context)
//    {
//        _ = currentPageNum;
//        lock (syncRoot)
//        {
//            var newIndex = currentIndexInLeaf + direction;
//            if (newIndex < 0 || newIndex >= entries.Count)
//            {
//                return (0, 0, 0);
//            }
//
//            if (context is { IsolationLevel: IsolationLevel.Serializable } && lockManager != null)
//            {
//                var lockKey = LockManagerBase.GetKeyLockKey(name, entries[newIndex].Key);
//                lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{name}' durante la scansione");
//                context.ReadLockedKeys.Add(lockKey);
//            }
//
//            return (entries[newIndex].Recno, 1, newIndex);
//        }
//    }
//
//    public static void Invalidate(ref List<int>? cache, object syncRoot)
//    {
//        lock (syncRoot)
//        {
//            cache = null;
//        }
//    }
//
//    private static int FindKeyIndex(List<(byte[] Key, int Recno)> entries, ReadOnlySpan<byte> keyBytes, out bool exact)
//    {
//        exact = false;
//        var lo = 0;
//        var hi = entries.Count - 1;
//        var insertAt = entries.Count;
//
//        while (lo <= hi)
//        {
//            var mid = (lo + hi) >> 1;
//            var cmp = keyBytes.SequenceCompareTo(entries[mid].Key);
//            if (cmp == 0)
//            {
//                exact = true;
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
//}
//
//#endregion

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#define GMB_MDX_MAX_ENTRIES 4096
#define GMB_MDX_MAX_PAGES 512
#define GMB_MDX_MAX_DEPTH 16

static uint16_t mdx_ru16(const uint8_t* p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t mdx_ru32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void mdx_le16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void mdx_le32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int mdx_keycmp(const uint8_t* a, int alen, const uint8_t* b, int blen)
{
    int n = alen < blen ? alen : blen;
    int c = n > 0 ? memcmp(a, b, (size_t)n) : 0;
    if (c != 0) return c;
    return alen - blen;
}

static int mdx_align_up(int value, int alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

static void mdx_stem(const char* path, char* out, size_t cap)
{
    const char* slash;
    const char* base;
    const char* dot;
    size_t n;
    if (!path || !out || cap == 0) return;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    n = dot ? (size_t)(dot - base) : strlen(base);
    if (n >= cap) n = cap - 1;
    memcpy(out, base, n);
    out[n] = 0;
}

static void mdx_normalize_tag(const char* tagName, char* out, size_t cap)
{
    size_t i, n;
    if (!tagName || !out || cap == 0) return;
    while (*tagName == ' ' || *tagName == '\t') tagName++;
    n = strlen(tagName);
    while (n > 0 && (tagName[n - 1] == ' ' || tagName[n - 1] == '\t')) n--;
    if (n > (size_t)MdxFormat_TagNameLength) n = (size_t)MdxFormat_TagNameLength;
    if (n >= cap) n = cap - 1;
    for (i = 0; i < n; i++)
        out[i] = (char)toupper((unsigned char)tagName[i]);
    out[n] = 0;
}

static void mdx_read_cstring(const uint8_t* span, int spanLen, char* out, int cap)
{
    int len = spanLen, i;
    for (i = 0; i < spanLen; i++) {
        if (span[i] == 0) {
            len = i;
            break;
        }
    }
    while (len > 0 && span[len - 1] == ' ') len--;
    if (len >= cap) len = cap - 1;
    if (len > 0) memcpy(out, span, (size_t)len);
    out[len] = 0;
}

static void mdx_write_tag_name(uint8_t* dest, int destLen, const char* name)
{
    size_t n;
    memset(dest, 0, (size_t)destLen);
    if (!name) return;
    n = strlen(name);
    if (n > (size_t)destLen) n = (size_t)destLen;
    memcpy(dest, name, n);
}

typedef struct {
    uint8_t* Key;
    int KeyLen;
    int Recno;
} MdxEntry;

static int mdx_find_key(MdxEntry* entries, int n, const uint8_t* keyBytes, int keyLen, int* exactMatch)
{
    int lo = 0, hi = n - 1, insertAt = n;
    *exactMatch = 0;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = mdx_keycmp(keyBytes, keyLen, entries[mid].Key, entries[mid].KeyLen);
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

static int mdx_read_page(FILE* fs, uint32_t blockNumber, uint8_t* buffer)
{
    if (fseek(fs, (long)blockNumber * NdxFormat_BlockSize, SEEK_SET) != 0) return -1;
    if (fread(buffer, 1, NdxFormat_BlockSize, fs) != NdxFormat_BlockSize) return -1;
    return 0;
}

static void mdx_enumerate(
    FILE* fs,
    uint32_t blockNumber,
    int keyLength,
    int groupLen,
    MdxEntry* result,
    int* nResult,
    int depth)
{
    uint8_t page[NdxFormat_BlockSize];
    uint16_t itemCount;
    uint32_t child0;
    int i;
    if (depth > GMB_MDX_MAX_DEPTH || *nResult >= GMB_MDX_MAX_ENTRIES) return;
    if (mdx_read_page(fs, blockNumber, page) != 0) return;
    itemCount = mdx_ru16(page);
    if (itemCount == 0) return;
    child0 = mdx_ru32(page + 4);
    if (child0 == 0) {
        for (i = 0; i < (int)itemCount && *nResult < GMB_MDX_MAX_ENTRIES; i++) {
            int offset = 4 + (i * groupLen);
            MdxEntry* e = &result[*nResult];
            e->KeyLen = keyLength;
            e->Key = (uint8_t*)malloc((size_t)keyLength);
            if (!e->Key) return;
            memcpy(e->Key, page + offset + 8, (size_t)keyLength);
            e->Recno = (int)mdx_ru32(page + offset + 4);
            (*nResult)++;
        }
        return;
    }
    for (i = 0; i <= (int)itemCount; i++) {
        uint32_t child = mdx_ru32(page + 4 + (i * groupLen));
        if (child != 0)
            mdx_enumerate(fs, child, keyLength, groupLen, result, nResult, depth + 1);
    }
}

int MdxCompound_ReadTagTable(const char* path, MdxTagDescriptor* out, int cap, int* nOut)
{
    FILE* fs;
    uint8_t header[MdxFormat_HeaderSize];
    uint8_t table[GMB_MDX_MAX_TAGS * MdxFormat_TagEntrySize];
    uint8_t tagHeader[NdxFormat_BlockSize];
    long flen;
    int tagCount, i, n = 0;
    if (nOut) *nOut = 0;
    if (!path || !out || cap <= 0) return GMB_DB_ARG;
    fs = fopen(path, "rb");
    if (!fs) return GMB_DB_ERR;
    if (fseek(fs, 0, SEEK_END) != 0) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    flen = ftell(fs);
    if (flen < MdxFormat_TagTableOffset + MdxFormat_TagEntrySize) {
        fclose(fs);
        return GMB_DB_OK;
    }
    if (fseek(fs, 0, SEEK_SET) != 0 || fread(header, 1, MdxFormat_HeaderSize, fs) != MdxFormat_HeaderSize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    tagCount = header[28];
    if (tagCount == 0) {
        fclose(fs);
        return GMB_DB_OK;
    }
    if (tagCount > GMB_MDX_MAX_TAGS) tagCount = GMB_MDX_MAX_TAGS;
    if (fseek(fs, MdxFormat_TagTableOffset, SEEK_SET) != 0
        || fread(table, 1, (size_t)tagCount * MdxFormat_TagEntrySize, fs) != (size_t)tagCount * MdxFormat_TagEntrySize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    for (i = 0; i < tagCount && n < cap; i++) {
        const uint8_t* entry = table + i * MdxFormat_TagEntrySize;
        uint32_t headerPage = mdx_ru32(entry);
        MdxTagDescriptor* d;
        if (headerPage == 0) continue;
        d = &out[n];
        memset(d, 0, sizeof(*d));
        d->HeaderPage = headerPage;
        mdx_read_cstring(entry + 4, MdxFormat_TagNameLength, d->Name, (int)sizeof(d->Name));
        if (mdx_read_page(fs, headerPage, tagHeader) != 0) continue;
        d->RootPage = mdx_ru32(tagHeader);
        d->KeyLength = (int)mdx_ru16(tagHeader + 12);
        d->Unique = tagHeader[23] != 0;
        mdx_read_cstring(tagHeader + NdxFormat_ExpressionOffset, NdxFormat_ExpressionLength,
                         d->Expression, (int)sizeof(d->Expression));
        n++;
    }
    fclose(fs);
    if (nOut) *nOut = n;
    return GMB_DB_OK;
}

static void mdx_build_header(uint8_t* header, const char* dbfBaseName, uint8_t tagCount)
{
    char stem[32];
    memset(header, 0, MdxFormat_HeaderSize);
    header[0] = 2;
    header[20] = 2;
    header[28] = tagCount;
    header[29] = (uint8_t)MdxFormat_TagNameLength;
    mdx_stem(dbfBaseName ? dbfBaseName : "", stem, sizeof(stem));
    {
        size_t n = strlen(stem);
        if (n > 16) n = 16;
        memcpy(header + 4, stem, n);
    }
}

static void mdx_build_tag_header_page(uint8_t* page, const char* expression, int keyLength, int unique, uint32_t rootPage)
{
    int i, n;
    memset(page, 0, NdxFormat_BlockSize);
    mdx_le32(page, rootPage);
    mdx_le16(page + 12, (uint16_t)keyLength);
    mdx_le16(page + 14, NdxFormat_MaxKeysPerPage(keyLength));
    mdx_le16(page + 18, (uint16_t)NdxFormat_GroupLength(keyLength));
    page[23] = unique ? (uint8_t)1 : (uint8_t)0;
    if (expression) {
        n = (int)strlen(expression);
        if (n > NdxFormat_ExpressionLength) n = NdxFormat_ExpressionLength;
        for (i = 0; i < n; i++)
            page[NdxFormat_ExpressionOffset + i] = (uint8_t)tolower((unsigned char)expression[i]);
    }
}

static int mdx_build_ndx_pages(
    int keyLength,
    const uint8_t* const* keys,
    const int* keyLens,
    const int* recnos,
    int n,
    uint8_t pages[][NdxFormat_BlockSize],
    int maxPages,
    int* nOut)
{
    char tmp[] = "native/gmb_db_c/build/mdxpg_XXXXXX";
    FILE* fs;
    int fd, got = 0;
    uint8_t buf[NdxFormat_BlockSize];
    *nOut = 0;
    fd = mkstemp(tmp);
    if (fd < 0) return GMB_DB_ERR;
    close(fd);
    if (NdxBuilder_Write(tmp, "", keyLength, 0, keys, keyLens, recnos, n) != GMB_DB_OK) {
        unlink(tmp);
        return GMB_DB_ERR;
    }
    fs = fopen(tmp, "rb");
    if (!fs) {
        unlink(tmp);
        return GMB_DB_ERR;
    }
    if (fseek(fs, NdxFormat_BlockSize, SEEK_SET) != 0) {
        fclose(fs);
        unlink(tmp);
        return GMB_DB_ERR;
    }
    while (got < maxPages && fread(buf, 1, NdxFormat_BlockSize, fs) == NdxFormat_BlockSize) {
        memcpy(pages[got], buf, NdxFormat_BlockSize);
        got++;
    }
    fclose(fs);
    unlink(tmp);
    *nOut = got;
    return GMB_DB_OK;
}

int MdxCompound_WriteCompound(const char* path, const char* dbfBaseName, const MdxTagWrite* tags, int nTags)
{
    uint8_t header[MdxFormat_HeaderSize];
    uint8_t gap[MdxFormat_TagTableOffset - MdxFormat_HeaderSize];
    uint8_t tagTable[GMB_MDX_MAX_TAGS * MdxFormat_TagEntrySize];
    uint8_t dataPages[GMB_MDX_MAX_PAGES][NdxFormat_BlockSize];
    int nData = 0;
    int tagTableLen, dataStartOffset, dataStartBlock, pad, i, p;
    FILE* fs;
    char dirbuf[PATH_MAX];
    char* slash;

    if (!path || nTags < 0 || nTags > GMB_MDX_MAX_TAGS) return GMB_DB_ARG;
    if (nTags > 0 && !tags) return GMB_DB_ARG;

    memset(tagTable, 0, sizeof(tagTable));
    tagTableLen = nTags * MdxFormat_TagEntrySize;
    dataStartOffset = mdx_align_up(MdxFormat_TagTableOffset + tagTableLen, NdxFormat_BlockSize);
    dataStartBlock = dataStartOffset / NdxFormat_BlockSize;

    for (i = 0; i < nTags; i++) {
        uint32_t headerPageNum = (uint32_t)dataStartBlock + (uint32_t)nData;
        uint32_t rootPage;
        int ndxN = 0;
        uint8_t ndxPages[GMB_MDX_MAX_PAGES][NdxFormat_BlockSize];
        int k;
        if (nData >= GMB_MDX_MAX_PAGES) return GMB_DB_ERR;
        mdx_build_tag_header_page(dataPages[nData], tags[i].Expression, tags[i].KeyLength, tags[i].Unique, 0);
        nData++;
        if (mdx_build_ndx_pages(tags[i].KeyLength, tags[i].Keys, tags[i].KeyLens, tags[i].Recnos, tags[i].NEntries,
                                ndxPages, GMB_MDX_MAX_PAGES, &ndxN) != GMB_DB_OK)
            return GMB_DB_ERR;
        rootPage = (uint32_t)dataStartBlock + (uint32_t)nData;
        for (k = 0; k < ndxN; k++) {
            if (nData >= GMB_MDX_MAX_PAGES) return GMB_DB_ERR;
            memcpy(dataPages[nData], ndxPages[k], NdxFormat_BlockSize);
            nData++;
        }
        mdx_le32(dataPages[(int)(headerPageNum - (uint32_t)dataStartBlock)], rootPage);
        mdx_le32(dataPages[(int)(headerPageNum - (uint32_t)dataStartBlock)] + 4,
                 (uint32_t)dataStartBlock + (uint32_t)nData);
        mdx_le32(tagTable + i * MdxFormat_TagEntrySize, headerPageNum);
        mdx_write_tag_name(tagTable + i * MdxFormat_TagEntrySize + 4, MdxFormat_TagNameLength, tags[i].TagName);
    }

    mdx_build_header(header, dbfBaseName, (uint8_t)nTags);
    memset(gap, 0, sizeof(gap));
    pad = dataStartOffset - (MdxFormat_TagTableOffset + tagTableLen);

    snprintf(dirbuf, sizeof(dirbuf), "%s", path);
    slash = strrchr(dirbuf, '/');
    if (slash && slash != dirbuf) {
        *slash = 0;
        if (mkdir(dirbuf, 0755) != 0 && errno != EEXIST) return GMB_DB_ERR;
    }

    fs = fopen(path, "wb");
    if (!fs) return GMB_DB_ERR;
    if (fwrite(header, 1, MdxFormat_HeaderSize, fs) != MdxFormat_HeaderSize
        || fwrite(gap, 1, sizeof(gap), fs) != sizeof(gap)
        || (tagTableLen > 0 && fwrite(tagTable, 1, (size_t)tagTableLen, fs) != (size_t)tagTableLen)) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    if (pad > 0) {
        uint8_t z[512];
        memset(z, 0, sizeof(z));
        while (pad > 0) {
            int chunk = pad > 512 ? 512 : pad;
            if (fwrite(z, 1, (size_t)chunk, fs) != (size_t)chunk) {
                fclose(fs);
                return GMB_DB_ERR;
            }
            pad -= chunk;
        }
    }
    for (p = 0; p < nData; p++) {
        if (fwrite(dataPages[p], 1, NdxFormat_BlockSize, fs) != NdxFormat_BlockSize) {
            fclose(fs);
            return GMB_DB_ERR;
        }
    }
    fclose(fs);
    return GMB_DB_OK;
}

struct MdxTagIndex {
    char CompoundPath[PATH_MAX];
    MdxTagDescriptor Desc;
    int Writable;
    int nEntries;
    MdxEntry Entries[GMB_MDX_MAX_ENTRIES];
    pthread_mutex_t Lock;
    int LockInited;
};

static void mdx_free_entries(MdxTagIndex* self)
{
    int i;
    for (i = 0; i < self->nEntries; i++) {
        free(self->Entries[i].Key);
        self->Entries[i].Key = NULL;
    }
    self->nEntries = 0;
}

static int mdx_load_entries(MdxTagIndex* self)
{
    FILE* fs;
    int groupLen;
    self->nEntries = 0;
    if (self->Desc.RootPage == 0) return GMB_DB_OK;
    fs = fopen(self->CompoundPath, "rb");
    if (!fs) return GMB_DB_ERR;
    groupLen = NdxFormat_GroupLength(self->Desc.KeyLength);
    mdx_enumerate(fs, self->Desc.RootPage, self->Desc.KeyLength, groupLen, self->Entries, &self->nEntries, 0);
    fclose(fs);
    return GMB_DB_OK;
}

static MdxTagIndex* mdx_open_desc(const char* path, const MdxTagDescriptor* d, int writable)
{
    MdxTagIndex* self = (MdxTagIndex*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->CompoundPath, sizeof(self->CompoundPath), "%s", path);
    self->Desc = *d;
    self->Writable = writable;
    if (mdx_load_entries(self) != GMB_DB_OK) {
        MdxTagIndex_Dispose(self);
        return NULL;
    }
    if (pthread_mutex_init(&self->Lock, NULL) == 0)
        self->LockInited = 1;
    return self;
}

static int mdx_fill_write_from_loaded(MdxTagIndex* idx, MdxTagWrite* w,
                                      const uint8_t** keys, int* lens, int* recs)
{
    int i;
    w->TagName = idx->Desc.Name;
    w->Expression = idx->Desc.Expression;
    w->KeyLength = idx->Desc.KeyLength;
    w->Unique = idx->Desc.Unique;
    w->NEntries = idx->nEntries;
    w->Keys = keys;
    w->KeyLens = lens;
    w->Recnos = recs;
    for (i = 0; i < idx->nEntries; i++) {
        keys[i] = idx->Entries[i].Key;
        lens[i] = idx->Entries[i].KeyLen;
        recs[i] = idx->Entries[i].Recno;
    }
    return GMB_DB_OK;
}

static int mdx_rewrite(MdxTagIndex* self)
{
    MdxTagDescriptor table[GMB_MDX_MAX_TAGS];
    MdxTagWrite writes[GMB_MDX_MAX_TAGS];
    MdxTagIndex* others[GMB_MDX_MAX_TAGS];
    const uint8_t* keys[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int lens[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int recs[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int nTags = 0, i, rc;
    char dbfName[256];
    memset(others, 0, sizeof(others));
    if (MdxCompound_ReadTagTable(self->CompoundPath, table, GMB_MDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return GMB_DB_ERR;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, self->Desc.Name) == 0) {
            writes[i].TagName = self->Desc.Name;
            writes[i].Expression = self->Desc.Expression;
            writes[i].KeyLength = self->Desc.KeyLength;
            writes[i].Unique = self->Desc.Unique;
            writes[i].NEntries = self->nEntries;
            writes[i].Keys = keys[i];
            writes[i].KeyLens = lens[i];
            writes[i].Recnos = recs[i];
            {
                int k;
                for (k = 0; k < self->nEntries; k++) {
                    keys[i][k] = self->Entries[k].Key;
                    lens[i][k] = self->Entries[k].KeyLen;
                    recs[i][k] = self->Entries[k].Recno;
                }
            }
        } else {
            others[i] = mdx_open_desc(self->CompoundPath, &table[i], 0);
            if (!others[i]) {
                int j;
                for (j = 0; j < i; j++) MdxTagIndex_Dispose(others[j]);
                return GMB_DB_ERR;
            }
            mdx_fill_write_from_loaded(others[i], &writes[i], keys[i], lens[i], recs[i]);
        }
    }
    mdx_stem(self->CompoundPath, dbfName, sizeof(dbfName));
    rc = MdxCompound_WriteCompound(self->CompoundPath, dbfName, writes, nTags);
    for (i = 0; i < nTags; i++)
        MdxTagIndex_Dispose(others[i]);
    return rc;
}

MdxTagIndex* MdxTagIndex_CreateOrReplace(const char* compoundPath, const char* dbfBaseName,
                                         const char* tagName, const char* keyExpression, int keyLength, int isUnique)
{
    char norm[16];
    MdxTagDescriptor table[GMB_MDX_MAX_TAGS];
    MdxTagWrite writes[GMB_MDX_MAX_TAGS + 1];
    MdxTagIndex* others[GMB_MDX_MAX_TAGS];
    const uint8_t* keys[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int lens[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int recs[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int nTags = 0, nWrites = 0, i, exists = 0;
    MdxTagDescriptor created;

    if (!compoundPath || !tagName || !keyExpression || keyLength <= 0) return NULL;
    mdx_normalize_tag(tagName, norm, sizeof(norm));
    if (norm[0] == 0) return NULL;
    memset(others, 0, sizeof(others));

    exists = access(compoundPath, F_OK) == 0;
    if (exists && MdxCompound_ReadTagTable(compoundPath, table, GMB_MDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0)
            continue;
        others[nWrites] = mdx_open_desc(compoundPath, &table[i], 0);
        if (!others[nWrites]) {
            int j;
            for (j = 0; j < nWrites; j++) MdxTagIndex_Dispose(others[j]);
            return NULL;
        }
        mdx_fill_write_from_loaded(others[nWrites], &writes[nWrites], keys[nWrites], lens[nWrites], recs[nWrites]);
        nWrites++;
    }
    writes[nWrites].TagName = norm;
    writes[nWrites].Expression = keyExpression;
    writes[nWrites].KeyLength = keyLength;
    writes[nWrites].Unique = isUnique;
    writes[nWrites].Keys = NULL;
    writes[nWrites].KeyLens = NULL;
    writes[nWrites].Recnos = NULL;
    writes[nWrites].NEntries = 0;
    nWrites++;
    if (MdxCompound_WriteCompound(compoundPath, dbfBaseName ? dbfBaseName : compoundPath, writes, nWrites) != GMB_DB_OK) {
        for (i = 0; i < nWrites - 1; i++) MdxTagIndex_Dispose(others[i]);
        return NULL;
    }
    for (i = 0; i < nWrites - 1; i++)
        MdxTagIndex_Dispose(others[i]);
    if (MdxCompound_ReadTagTable(compoundPath, table, GMB_MDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    memset(&created, 0, sizeof(created));
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0) {
            created = table[i];
            break;
        }
    }
    if (created.Name[0] == 0) return NULL;
    return mdx_open_desc(compoundPath, &created, 1);
}

MdxTagIndex* MdxTagIndex_Open(const char* compoundPath, const char* tagName, int writable)
{
    MdxTagDescriptor table[GMB_MDX_MAX_TAGS];
    int nTags = 0, i;
    char norm[16];
    if (!compoundPath || !tagName) return NULL;
    mdx_normalize_tag(tagName, norm, sizeof(norm));
    if (MdxCompound_ReadTagTable(compoundPath, table, GMB_MDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0)
            return mdx_open_desc(compoundPath, &table[i], writable);
    }
    return NULL;
}

void MdxTagIndex_Dispose(MdxTagIndex* self)
{
    if (!self) return;
    mdx_free_entries(self);
    if (self->LockInited) pthread_mutex_destroy(&self->Lock);
    free(self);
}

const char* MdxTagIndex_get_Name(const MdxTagIndex* self)
{
    return self ? self->Desc.Name : NULL;
}

const char* MdxTagIndex_GetKeyExpressionString(const MdxTagIndex* self)
{
    return self ? self->Desc.Expression : NULL;
}

int MdxTagIndex_get_KeyLength(const MdxTagIndex* self)
{
    return self ? self->Desc.KeyLength : 0;
}

int MdxTagIndex_Seek(MdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    int exact, idx, rec;
    if (!self || !keyBytes) return -1;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->nEntries == 0) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return -1;
    }
    idx = mdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (exact)
        rec = self->Entries[idx].Recno;
    else
        rec = (soft && idx < self->nEntries) ? self->Entries[idx].Recno : -1;
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rec;
}

int MdxTagIndex_Insert(MdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, rc;
    uint8_t* copy;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes || keyLen < 0) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->nEntries >= GMB_MDX_MAX_ENTRIES) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    idx = mdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (self->Desc.Unique && exact) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    copy = (uint8_t*)malloc((size_t)keyLen);
    if (!copy) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    if (keyLen > 0) memcpy(copy, keyBytes, (size_t)keyLen);
    memmove(&self->Entries[idx + 1], &self->Entries[idx], (size_t)(self->nEntries - idx) * sizeof(MdxEntry));
    self->Entries[idx].Key = copy;
    self->Entries[idx].KeyLen = keyLen;
    self->Entries[idx].Recno = recno;
    self->nEntries++;
    rc = mdx_rewrite(self);
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rc;
}

int MdxTagIndex_Delete(MdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, i;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    idx = mdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (exact) {
        for (i = idx; i < self->nEntries; i++) {
            if (mdx_keycmp(self->Entries[i].Key, self->Entries[i].KeyLen, keyBytes, keyLen) != 0)
                break;
            if (self->Entries[i].Recno == recno) {
                free(self->Entries[i].Key);
                memmove(&self->Entries[i], &self->Entries[i + 1],
                        (size_t)(self->nEntries - i - 1) * sizeof(MdxEntry));
                self->nEntries--;
                mdx_rewrite(self);
                break;
            }
        }
    }
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return GMB_DB_OK;
}

static void mdx_sc_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int MdxCompound_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/mdxsc_XXXXXX";
    char path[PATH_MAX];
    MdxTagIndex *nameIdx, *idIdx, *reopen;
    MdxTagDescriptor table[4];
    int nTags = 0, i;
    uint8_t key[10];
    uint8_t head[64];
    FILE* f;
    long flen;

    if (!mkdtemp(dir)) return -2;
    snprintf(path, sizeof(path), "%s/t.mdx", dir);

    nameIdx = MdxTagIndex_CreateOrReplace(path, "CUSTOMER", "name", "NAME", 10, 0);
    if (!nameIdx) return -3;
    if (!MdxTagIndex_get_Name(nameIdx) || strcmp(MdxTagIndex_get_Name(nameIdx), "NAME") != 0) {
        MdxTagIndex_Dispose(nameIdx);
        return -4;
    }
    if (!MdxTagIndex_GetKeyExpressionString(nameIdx)
        || strcmp(MdxTagIndex_GetKeyExpressionString(nameIdx), "name") != 0) {
        MdxTagIndex_Dispose(nameIdx);
        return -5;
    }

    f = fopen(path, "rb");
    if (!f) {
        MdxTagIndex_Dispose(nameIdx);
        return -6;
    }
    if (fread(head, 1, 32, f) != 32) {
        fclose(f);
        MdxTagIndex_Dispose(nameIdx);
        return -7;
    }
    fseek(f, 0, SEEK_END);
    flen = ftell(f);
    fclose(f);
    if (head[0] != 2 || head[20] != 2 || head[28] != 1 || head[29] != 10
        || memcmp(head + 4, "CUSTOMER", 8) != 0 || flen != 2048) {
        MdxTagIndex_Dispose(nameIdx);
        return -8;
    }

    if (MdxCompound_ReadTagTable(path, table, 4, &nTags) != GMB_DB_OK || nTags != 1
        || table[0].HeaderPage != 2 || table[0].RootPage != 3 || table[0].KeyLength != 10) {
        MdxTagIndex_Dispose(nameIdx);
        return -9;
    }

    for (i = 0; i < 8; i++) {
        mdx_sc_key(key, i);
        if (MdxTagIndex_Insert(nameIdx, key, 10, 100 + i) != GMB_DB_OK) {
            MdxTagIndex_Dispose(nameIdx);
            return -10 - i;
        }
    }
    mdx_sc_key(key, 3);
    if (MdxTagIndex_Seek(nameIdx, key, 10, 0) != 103) {
        MdxTagIndex_Dispose(nameIdx);
        return -20;
    }
    MdxTagIndex_Dispose(nameIdx);

    idIdx = MdxTagIndex_CreateOrReplace(path, "CUSTOMER", "id", "ID", 10, 1);
    if (!idIdx) return -21;
    mdx_sc_key(key, 0);
    if (MdxTagIndex_Insert(idIdx, key, 10, 1) != GMB_DB_OK) {
        MdxTagIndex_Dispose(idIdx);
        return -22;
    }
    if (MdxTagIndex_Insert(idIdx, key, 10, 2) != GMB_DB_ERR) {
        MdxTagIndex_Dispose(idIdx);
        return -23;
    }
    MdxTagIndex_Dispose(idIdx);

    if (MdxCompound_ReadTagTable(path, table, 4, &nTags) != GMB_DB_OK || nTags != 2) return -24;

    reopen = MdxTagIndex_Open(path, "NAME", 0);
    if (!reopen) return -25;
    mdx_sc_key(key, 3);
    if (MdxTagIndex_Seek(reopen, key, 10, 0) != 103) {
        MdxTagIndex_Dispose(reopen);
        return -26;
    }
    mdx_sc_key(key, 7);
    if (MdxTagIndex_Seek(reopen, key, 10, 0) != 107) {
        MdxTagIndex_Dispose(reopen);
        return -27;
    }
    if (MdxTagIndex_Insert(reopen, key, 10, 1) != GMB_DB_ERR) {
        MdxTagIndex_Dispose(reopen);
        return -28;
    }
    MdxTagIndex_Dispose(reopen);

    reopen = MdxTagIndex_Open(path, "ID", 1);
    if (!reopen) return -29;
    mdx_sc_key(key, 0);
    if (MdxTagIndex_Seek(reopen, key, 10, 0) != 1) {
        MdxTagIndex_Dispose(reopen);
        return -30;
    }
    mdx_sc_key(key, 3);
    if (MdxTagIndex_Delete(reopen, key, 10, 1) != GMB_DB_OK) {
        MdxTagIndex_Dispose(reopen);
        return -31;
    }
    mdx_sc_key(key, 0);
    if (MdxTagIndex_Delete(reopen, key, 10, 1) != GMB_DB_OK
        || MdxTagIndex_Seek(reopen, key, 10, 0) != -1) {
        MdxTagIndex_Dispose(reopen);
        return -32;
    }
    MdxTagIndex_Dispose(reopen);

    unlink(path);
    rmdir(dir);
    return GMB_DB_OK;
}

#define GMB_CDX_MAX_ENTRIES 4096
#define GMB_CDX_MAX_PAGES 512
#define GMB_CDX_MAX_DEPTH 16
#define GMB_CDX_MAX_KEY 256
#define CDX_U32MAX 0xFFFFFFFFu

typedef struct {
    uint8_t* Key;
    int KeyLen;
    int Recno;
} CdxEntry;

typedef struct {
    uint8_t Key[GMB_CDX_MAX_KEY];
    int Recno;
} CdxSortItem;

static int g_cdx_sort_keylen;

static int cdx_sort_cmp(const void* a, const void* b)
{
    const CdxSortItem* x = (const CdxSortItem*)a;
    const CdxSortItem* y = (const CdxSortItem*)b;
    return memcmp(x->Key, y->Key, (size_t)g_cdx_sort_keylen);
}

static int cdx_find_key(CdxEntry* entries, int n, const uint8_t* keyBytes, int keyLen, int* exactMatch)
{
    int lo = 0, hi = n - 1, insertAt = n;
    *exactMatch = 0;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = mdx_keycmp(keyBytes, keyLen, entries[mid].Key, entries[mid].KeyLen);
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

static void cdx_pad_key(uint8_t* dest, int keyLength, const uint8_t* key, int keyLen)
{
    int copy = keyLen < keyLength ? keyLen : keyLength;
    memset(dest, ' ', (size_t)keyLength);
    if (copy > 0) memcpy(dest, key, (size_t)copy);
}

static int cdx_count_dup(const uint8_t* prev, const uint8_t* key, int keyLength)
{
    int i = 0;
    while (i < keyLength && prev[i] == key[i]) i++;
    return i;
}

static int cdx_count_trail(const uint8_t* key, int keyLength, int dup)
{
    int trail = 0, i;
    for (i = keyLength - 1; i >= dup; i--) {
        if (key[i] != (uint8_t)' ') break;
        trail++;
    }
    return trail;
}

static int cdx_can_fit_leaf(int count, int keyLength)
{
    int keyPos = CdxFormat_PageSize;
    int poolEnd = CdxFormat_ExtNodeHeaderSize + (count * 4);
    int i;
    for (i = 0; i < count; i++) {
        keyPos -= keyLength;
        if (keyPos < poolEnd) return 0;
    }
    return 1;
}

static int cdx_max_keys_leaf(int keyLength)
{
    int lo = 1, hi = 256;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (cdx_can_fit_leaf(mid, keyLength)) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

static int cdx_max_keys_interior(int keyLength)
{
    int slot = keyLength + 4;
    int n = (CdxFormat_PageSize - CdxFormat_IntNodeHeaderSize) / slot - 1;
    return n > 1 ? n : 1;
}

static void cdx_build_empty_leaf(uint8_t* page, int keyLength)
{
    memset(page, 0, CdxFormat_PageSize);
    mdx_le16(page, 2);
    mdx_le16(page + 2, 0);
    mdx_le32(page + 4, CDX_U32MAX);
    mdx_le32(page + 8, CDX_U32MAX);
    page[20] = 20;
    page[21] = 4;
    page[22] = 4;
    page[23] = 4;
    (void)keyLength;
}

static int cdx_build_leaf(uint8_t* page, const CdxSortItem* entries, int n, int keyLength)
{
    uint8_t prev[GMB_CDX_MAX_KEY];
    int i, keyPos, poolOffset;
    memset(page, 0, CdxFormat_PageSize);
    mdx_le16(page, 3);
    mdx_le16(page + 2, (uint16_t)n);
    mdx_le32(page + 4, CDX_U32MAX);
    mdx_le32(page + 8, CDX_U32MAX);
    page[20] = 20;
    page[21] = 4;
    page[22] = 4;
    page[23] = 4;
    memset(prev, 0, (size_t)keyLength);
    keyPos = CdxFormat_PageSize;
    poolOffset = CdxFormat_ExtNodeHeaderSize;
    for (i = 0; i < n; i++) {
        int dup = cdx_count_dup(prev, entries[i].Key, keyLength);
        int trail = cdx_count_trail(entries[i].Key, keyLength, dup);
        int varLen = keyLength - dup - trail;
        uint32_t packed;
        if (varLen < 0) return GMB_DB_ERR;
        keyPos -= varLen;
        if (keyPos < poolOffset + (n * 4)) return GMB_DB_ERR;
        if (varLen > 0) memcpy(page + keyPos, entries[i].Key + dup, (size_t)varLen);
        packed = ((uint32_t)entries[i].Recno << 8) | ((uint32_t)trail << 4) | (uint32_t)dup;
        mdx_le32(page + poolOffset + (i * 4), packed);
        memcpy(prev, entries[i].Key, (size_t)keyLength);
    }
    return GMB_DB_OK;
}

static void cdx_build_interior(uint8_t* page, const uint32_t* childOffsets, int nChildren,
                              const uint8_t separators[][GMB_CDX_MAX_KEY], int nSep, int keyLength)
{
    int keyCount = nSep;
    int keysStart = CdxFormat_PageSize - ((keyCount + 1) * (keyLength + 4));
    int i;
    memset(page, 0, CdxFormat_PageSize);
    mdx_le16(page, 1);
    mdx_le16(page + 2, (uint16_t)keyCount);
    mdx_le32(page + 4, CDX_U32MAX);
    mdx_le32(page + 8, CDX_U32MAX);
    for (i = 0; i < keyCount; i++) {
        int slot = keysStart + (i * (keyLength + 4));
        memcpy(page + slot, separators[i], (size_t)keyLength);
        mdx_le32(page + slot + keyLength, childOffsets[i]);
    }
    mdx_le32(page + keysStart + (keyCount * (keyLength + 4)) + keyLength, childOffsets[keyCount]);
    (void)nChildren;
}

static int cdx_decode_leaf(const uint8_t* page, int keyCount, int keyLength,
                           CdxEntry* out, int cap, int* nOut)
{
    int recBits = page[20], dupBits = page[21], trlBits = page[22], reqByte = page[23];
    uint16_t dcMask, tcMask;
    int keyPos = CdxFormat_PageSize, i, n = 0;
    uint8_t prev[GMB_CDX_MAX_KEY];
    if (reqByte == 0) reqByte = 4;
    if (keyLength <= 0 || keyLength > GMB_CDX_MAX_KEY) return GMB_DB_ERR;
    dcMask = (uint16_t)((1 << dupBits) - 1);
    tcMask = (uint16_t)((1 << trlBits) - 1);
    memset(prev, 0, (size_t)keyLength);
    for (i = 0; i < keyCount && n < cap; i++) {
        int pos = i * reqByte;
        uint32_t packed = mdx_ru32(page + CdxFormat_ExtNodeHeaderSize + pos);
        int dup = dupBits > 0 ? (int)(packed & dcMask) : 0;
        int trail = trlBits > 0 ? (int)((packed >> dupBits) & tcMask) : 0;
        uint32_t recno = recBits > 0 ? packed >> (dupBits + trlBits) : 0u;
        int varLen = keyLength - dup - trail;
        CdxEntry* e;
        keyPos -= varLen;
        e = &out[n];
        e->KeyLen = keyLength;
        e->Key = (uint8_t*)malloc((size_t)keyLength);
        if (!e->Key) return GMB_DB_ERR;
        memset(e->Key, 0, (size_t)keyLength);
        if (dup > 0) memcpy(e->Key, prev, (size_t)dup);
        if (varLen > 0 && keyPos >= 0) memcpy(e->Key + dup, page + keyPos, (size_t)varLen);
        if (trail > 0) memset(e->Key + keyLength - trail, ' ', (size_t)trail);
        memcpy(prev, e->Key, (size_t)keyLength);
        e->Recno = (int)recno;
        n++;
    }
    *nOut = n;
    return GMB_DB_OK;
}

static uint32_t cdx_interior_child(const uint8_t* page, int keyIndex, int keyCount, int keyLength)
{
    int keysStart = CdxFormat_PageSize - ((keyCount + 1) * (keyLength + 4));
    int offset = keysStart + (keyIndex * (keyLength + 4)) + keyLength;
    if (offset + 4 > CdxFormat_PageSize) return 0;
    return mdx_ru32(page + offset);
}

int CdxCompact_MaxKeysPerLeaf(int keyLength)
{
    return cdx_max_keys_leaf(keyLength);
}

int CdxCompact_MaxKeysPerInterior(int keyLength)
{
    return cdx_max_keys_interior(keyLength);
}

int CdxCompact_BuildLeafPage(uint8_t* page, const uint8_t* keysFlat, const int* recnos, int n, int keyLength)
{
    CdxSortItem* items;
    int i, rc;
    if (!page || keyLength <= 0 || keyLength > GMB_CDX_MAX_KEY) return GMB_DB_ARG;
    if (n < 0) return GMB_DB_ARG;
    if (n == 0) {
        cdx_build_empty_leaf(page, keyLength);
        return GMB_DB_OK;
    }
    if (!keysFlat || !recnos) return GMB_DB_ARG;
    items = (CdxSortItem*)calloc((size_t)n, sizeof(*items));
    if (!items) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        memcpy(items[i].Key, keysFlat + (i * keyLength), (size_t)keyLength);
        items[i].Recno = recnos[i];
    }
    rc = cdx_build_leaf(page, items, n, keyLength);
    free(items);
    return rc;
}

int CdxCompact_BuildInteriorPage(uint8_t* page, const uint32_t* children, int nChildren,
                                 const uint8_t* sepsFlat, int nSep, int keyLength)
{
    uint8_t seps[256][GMB_CDX_MAX_KEY];
    int i;
    if (!page || !children || nChildren != nSep + 1 || keyLength <= 0 || nSep < 0 || nSep > 256)
        return GMB_DB_ARG;
    memset(seps, 0, sizeof(seps));
    for (i = 0; i < nSep; i++)
        memcpy(seps[i], sepsFlat + (i * keyLength), (size_t)keyLength);
    cdx_build_interior(page, children, nChildren, seps, nSep, keyLength);
    return GMB_DB_OK;
}

int CdxCompact_DecodeLeafKeys(const uint8_t* page, int keyCount, int keyLength,
                              uint8_t* keysFlat, int* recnos, int cap, int* nOut)
{
    CdxEntry tmp[128];
    int nd = 0, i, use, rc;
    if (nOut) *nOut = 0;
    if (!page || !keysFlat || !recnos || cap <= 0) return GMB_DB_ARG;
    memset(tmp, 0, sizeof(tmp));
    use = cap < 128 ? cap : 128;
    rc = cdx_decode_leaf(page, keyCount, keyLength, tmp, use, &nd);
    if (rc != GMB_DB_OK) {
        for (i = 0; i < nd; i++) free(tmp[i].Key);
        return rc;
    }
    for (i = 0; i < nd; i++) {
        memcpy(keysFlat + (i * keyLength), tmp[i].Key, (size_t)keyLength);
        recnos[i] = tmp[i].Recno;
        free(tmp[i].Key);
    }
    if (nOut) *nOut = nd;
    return GMB_DB_OK;
}

uint32_t CdxCompact_GetInteriorChild(const uint8_t* page, int keyIndex, int keyCount, int keyLength)
{
    return cdx_interior_child(page, keyIndex, keyCount, keyLength);
}

void CdxCompact_GetInteriorKey(const uint8_t* page, int index, int keyLength, uint8_t* out)
{
    int keyCount = (int)mdx_ru16(page + 2);
    int slotStart = CdxFormat_PageSize - ((keyCount + 1) * (keyLength + 4));
    memcpy(out, page + slotStart + (index * (keyLength + 4)), (size_t)keyLength);
}

void CdxCompact_SetInteriorChild(uint8_t* page, int index, int keyCount, int keyLength, uint32_t newChild)
{
    int slotStart = CdxFormat_PageSize - ((keyCount + 1) * (keyLength + 4));
    int offset = slotStart + (index * (keyLength + 4)) + keyLength;
    mdx_le32(page + offset, newChild);
}

static int cdx_read_page(FILE* fs, uint32_t offset, uint8_t* buffer)
{
    if (fseek(fs, (long)offset, SEEK_SET) != 0) return -1;
    if (fread(buffer, 1, CdxFormat_PageSize, fs) != CdxFormat_PageSize) return -1;
    return 0;
}

static void cdx_read_expression(const uint8_t* tagHeader, char* out, int cap)
{
    uint16_t keyExpLen = mdx_ru16(tagHeader + 510);
    int n;
    if (keyExpLen == 0 || cap <= 0) {
        if (cap > 0) out[0] = 0;
        return;
    }
    n = (int)keyExpLen;
    if (n > 510 - 16) n = 510 - 16;
    if (n >= cap) n = cap - 1;
    memcpy(out, tagHeader + 16, (size_t)n);
    out[n] = 0;
}

static void cdx_trim_name(char* s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\0')) n--;
    s[n] = 0;
}

static void cdx_walk_catalog(FILE* fs, uint32_t offset, CdxTagDescriptor* out, int cap, int* n, int depth)
{
    uint8_t page[CdxFormat_PageSize];
    uint16_t attr, keyCount;
    int isLeaf, i;
    if (depth > GMB_CDX_MAX_DEPTH || *n >= cap) return;
    if (cdx_read_page(fs, offset, page) != 0) return;
    attr = mdx_ru16(page);
    keyCount = mdx_ru16(page + 2);
    isLeaf = (attr & 2) != 0;
    if (isLeaf) {
        CdxEntry decoded[GMB_CDX_MAX_TAGS];
        int nd = 0, k = 0;
        memset(decoded, 0, sizeof(decoded));
        if (cdx_decode_leaf(page, keyCount, CdxFormat_TagNameLength, decoded, GMB_CDX_MAX_TAGS, &nd) != GMB_DB_OK) {
            for (k = 0; k < nd; k++) free(decoded[k].Key);
            return;
        }
        for (k = 0; k < nd && *n < cap; k++) {
            uint8_t tagHeader[CdxFormat_PageSize];
            CdxTagDescriptor* d;
            char name[16];
            int nl = decoded[k].KeyLen < 15 ? decoded[k].KeyLen : 15;
            memcpy(name, decoded[k].Key, (size_t)nl);
            name[nl] = 0;
            cdx_trim_name(name);
            if (name[0] == 0) {
                free(decoded[k].Key);
                continue;
            }
            if (cdx_read_page(fs, (uint32_t)decoded[k].Recno, tagHeader) != 0) {
                free(decoded[k].Key);
                continue;
            }
            d = &out[*n];
            memset(d, 0, sizeof(*d));
            snprintf(d->Name, sizeof(d->Name), "%s", name);
            d->HeaderOffset = (uint32_t)decoded[k].Recno;
            d->RootOffset = mdx_ru32(tagHeader);
            d->KeyLength = (int)mdx_ru16(tagHeader + 12);
            d->Unique = (tagHeader[14] & 0x01) != 0;
            d->Descending = tagHeader[502] != 0;
            cdx_read_expression(tagHeader, d->Expression, (int)sizeof(d->Expression));
            (*n)++;
            free(decoded[k].Key);
        }
        for (; k < nd; k++) free(decoded[k].Key);
        return;
    }
    for (i = 0; i < (int)keyCount; i++) {
        uint32_t child = cdx_interior_child(page, i, keyCount, CdxFormat_TagNameLength);
        if (child != 0 && child != CDX_U32MAX)
            cdx_walk_catalog(fs, child, out, cap, n, depth + 1);
    }
    {
        uint32_t right = cdx_interior_child(page, keyCount, keyCount, CdxFormat_TagNameLength);
        if (right != 0 && right != CDX_U32MAX)
            cdx_walk_catalog(fs, right, out, cap, n, depth + 1);
    }
}

int CdxCompound_ReadTagTable(const char* path, CdxTagDescriptor* out, int cap, int* nOut)
{
    FILE* fs;
    uint8_t header[CdxFormat_HeaderSize];
    long flen;
    uint32_t rootOffset;
    int n = 0;
    if (nOut) *nOut = 0;
    if (!path || !out || cap <= 0) return GMB_DB_ARG;
    fs = fopen(path, "rb");
    if (!fs) return GMB_DB_ERR;
    if (fseek(fs, 0, SEEK_END) != 0) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    flen = ftell(fs);
    if (flen < CdxFormat_HeaderSize) {
        fclose(fs);
        return GMB_DB_OK;
    }
    if (fseek(fs, 0, SEEK_SET) != 0 || fread(header, 1, CdxFormat_HeaderSize, fs) != CdxFormat_HeaderSize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    if ((header[14] & CdxFormat_TypeCompound) == 0) {
        fclose(fs);
        return GMB_DB_OK;
    }
    rootOffset = mdx_ru32(header);
    if (rootOffset == 0 || (long)rootOffset >= flen) {
        fclose(fs);
        return GMB_DB_OK;
    }
    cdx_walk_catalog(fs, rootOffset, out, cap, &n, 0);
    fclose(fs);
    if (nOut) *nOut = n;
    return GMB_DB_OK;
}

static void cdx_build_compound_header(uint8_t* header, uint32_t catalogRoot, int keyLength)
{
    memset(header, 0, CdxFormat_HeaderSize);
    mdx_le32(header, catalogRoot);
    mdx_le16(header + 12, (uint16_t)keyLength);
    header[14] = (uint8_t)(CdxFormat_TypeCompact | CdxFormat_TypeCompound);
}

static void cdx_build_tag_header(uint8_t* header, const char* expression, int keyLength,
                                 int unique, int descending, uint32_t rootOffset)
{
    int n = 0;
    memset(header, 0, CdxFormat_PageSize);
    mdx_le32(header, rootOffset);
    mdx_le16(header + 12, (uint16_t)keyLength);
    header[14] = (uint8_t)((unique ? 0x01 : 0x00) | CdxFormat_TypeCompact);
    header[502] = descending ? (uint8_t)1 : (uint8_t)0;
    if (expression) {
        n = (int)strlen(expression);
        if (n > 510 - 16) n = 510 - 16;
        memcpy(header + 16, expression, (size_t)n);
    }
    mdx_le16(header + 510, (uint16_t)n);
}

static int cdx_build_catalog_leaf(uint8_t* page, const char names[][16], const uint32_t* offsets, int n)
{
    CdxSortItem items[GMB_CDX_MAX_TAGS];
    int i;
    memset(items, 0, sizeof(items));
    for (i = 0; i < n; i++) {
        size_t nl = strlen(names[i]);
        if (nl > (size_t)CdxFormat_TagNameLength) nl = (size_t)CdxFormat_TagNameLength;
        memcpy(items[i].Key, names[i], nl);
        items[i].Recno = (int)offsets[i];
    }
    g_cdx_sort_keylen = CdxFormat_TagNameLength;
    qsort(items, (size_t)n, sizeof(items[0]), cdx_sort_cmp);
    if (n == 0) {
        cdx_build_empty_leaf(page, CdxFormat_TagNameLength);
        return GMB_DB_OK;
    }
    return cdx_build_leaf(page, items, n, CdxFormat_TagNameLength);
}

typedef struct {
    int PageIndex;
    uint8_t PromoteKey[GMB_CDX_MAX_KEY];
} CdxLevelItem;

static int cdx_build_interior_level(
    CdxLevelItem* children, int nChildren,
    uint8_t pages[][CdxFormat_PageSize], int* nPages,
    int keyLength, int maxBranchKeys, uint32_t treeBaseOffset,
    CdxLevelItem* parents, int* nParents)
{
    int index = 0;
    *nParents = 0;
    while (index < nChildren) {
        int groupStart = index;
        int childIdx[256];
        uint8_t seps[256][GMB_CDX_MAX_KEY];
        uint32_t childOff[257];
        int nChild = 1, nSep = 0, p, pi;
        if (*nPages >= GMB_CDX_MAX_PAGES) return GMB_DB_ERR;
        childIdx[0] = children[index].PageIndex;
        index++;
        while (index < nChildren && nSep < maxBranchKeys) {
            memcpy(seps[nSep], children[index].PromoteKey, (size_t)keyLength);
            childIdx[nChild++] = children[index].PageIndex;
            nSep++;
            index++;
        }
        for (p = 0; p < nChild; p++)
            childOff[p] = treeBaseOffset + (uint32_t)(childIdx[p] * CdxFormat_PageSize);
        cdx_build_interior(pages[*nPages], childOff, nChild, seps, nSep, keyLength);
        pi = *nPages;
        (*nPages)++;
        parents[*nParents].PageIndex = pi;
        if (nSep > 0) memcpy(parents[*nParents].PromoteKey, seps[0], (size_t)keyLength);
        else memcpy(parents[*nParents].PromoteKey, children[groupStart].PromoteKey, (size_t)keyLength);
        (*nParents)++;
    }
    return GMB_DB_OK;
}

static int cdx_build_tag_pages(
    int keyLength,
    const uint8_t* const* keys, const int* keyLens, const int* recnos, int n,
    uint32_t treeBaseOffset,
    uint8_t pages[][CdxFormat_PageSize], int* nPagesOut, int* rootIndexOut)
{
    CdxSortItem* sorted = NULL;
    int maxLeaf, maxBranch, i, nPages = 0;
    CdxLevelItem leaves[GMB_CDX_MAX_PAGES];
    int nLeaves = 0;
    *nPagesOut = 0;
    *rootIndexOut = 0;
    if (keyLength <= 0 || keyLength > GMB_CDX_MAX_KEY) return GMB_DB_ARG;
    if (n <= 0) {
        cdx_build_empty_leaf(pages[0], keyLength);
        *nPagesOut = 1;
        *rootIndexOut = 0;
        return GMB_DB_OK;
    }
    sorted = (CdxSortItem*)calloc((size_t)n, sizeof(*sorted));
    if (!sorted) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        int kl = keyLens ? keyLens[i] : keyLength;
        cdx_pad_key(sorted[i].Key, keyLength, keys[i], kl);
        sorted[i].Recno = recnos[i];
    }
    g_cdx_sort_keylen = keyLength;
    qsort(sorted, (size_t)n, sizeof(*sorted), cdx_sort_cmp);
    maxLeaf = cdx_max_keys_leaf(keyLength);
    maxBranch = cdx_max_keys_interior(keyLength);
    for (i = 0; i < n; ) {
        int take = n - i;
        if (take > maxLeaf) take = maxLeaf;
        if (nPages >= GMB_CDX_MAX_PAGES) {
            free(sorted);
            return GMB_DB_ERR;
        }
        if (cdx_build_leaf(pages[nPages], sorted + i, take, keyLength) != GMB_DB_OK) {
            free(sorted);
            return GMB_DB_ERR;
        }
        leaves[nLeaves].PageIndex = nPages;
        memcpy(leaves[nLeaves].PromoteKey, sorted[i].Key, (size_t)keyLength);
        nPages++;
        nLeaves++;
        i += take;
    }
    free(sorted);
    if (nLeaves == 1) {
        *nPagesOut = nPages;
        *rootIndexOut = leaves[0].PageIndex;
        return GMB_DB_OK;
    }
    {
        CdxLevelItem levelA[GMB_CDX_MAX_PAGES], levelB[GMB_CDX_MAX_PAGES];
        CdxLevelItem *cur = leaves, *next = levelA;
        int nCur = nLeaves, nNext;
        memcpy(levelA, leaves, (size_t)nLeaves * sizeof(leaves[0]));
        cur = levelA;
        next = levelB;
        while (nCur > 1) {
            if (cdx_build_interior_level(cur, nCur, pages, &nPages, keyLength, maxBranch,
                                         treeBaseOffset, next, &nNext) != GMB_DB_OK)
                return GMB_DB_ERR;
            {
                CdxLevelItem* tmp = cur;
                cur = next;
                next = tmp;
            }
            nCur = nNext;
        }
        *nPagesOut = nPages;
        *rootIndexOut = cur[0].PageIndex;
        return GMB_DB_OK;
    }
}

int CdxCompound_WriteCompound(const char* path, const CdxTagWrite* tags, int nTags)
{
    uint8_t pages[GMB_CDX_MAX_PAGES][CdxFormat_PageSize];
    uint8_t header[CdxFormat_HeaderSize];
    char catNames[GMB_CDX_MAX_TAGS][16];
    uint32_t catOff[GMB_CDX_MAX_TAGS];
    int nPages = 0, i, k, firstKeyLen = 0;
    uint32_t catalogRoot;
    FILE* fs;
    char dirbuf[PATH_MAX];
    char* slash;

    if (!path || nTags < 0 || nTags > GMB_CDX_MAX_TAGS) return GMB_DB_ARG;
    if (nTags > 0 && !tags) return GMB_DB_ARG;
    if (nTags > 0) firstKeyLen = tags[0].KeyLength;

    for (i = 0; i < nTags; i++) {
        uint32_t tagHeaderOffset = (uint32_t)(nPages * CdxFormat_PageSize + CdxFormat_HeaderSize);
        uint32_t treeBase = tagHeaderOffset + CdxFormat_PageSize;
        uint8_t tagPages[GMB_CDX_MAX_PAGES][CdxFormat_PageSize];
        int tagN = 0, rootIdx = 0;
        uint32_t tagRoot;
        if (nPages >= GMB_CDX_MAX_PAGES) return GMB_DB_ERR;
        cdx_build_tag_header(pages[nPages], tags[i].Expression, tags[i].KeyLength,
                             tags[i].Unique, tags[i].Descending, 0);
        nPages++;
        if (cdx_build_tag_pages(tags[i].KeyLength, tags[i].Keys, tags[i].KeyLens, tags[i].Recnos,
                                tags[i].NEntries, treeBase, tagPages, &tagN, &rootIdx) != GMB_DB_OK)
            return GMB_DB_ERR;
        tagRoot = treeBase + (uint32_t)(rootIdx * CdxFormat_PageSize);
        for (k = 0; k < tagN; k++) {
            if (nPages >= GMB_CDX_MAX_PAGES) return GMB_DB_ERR;
            memcpy(pages[nPages], tagPages[k], CdxFormat_PageSize);
            nPages++;
        }
        mdx_le32(pages[(int)((tagHeaderOffset - CdxFormat_HeaderSize) / CdxFormat_PageSize)], tagRoot);
        snprintf(catNames[i], sizeof(catNames[i]), "%s", tags[i].TagName ? tags[i].TagName : "");
        catOff[i] = tagHeaderOffset;
    }

    if (nPages >= GMB_CDX_MAX_PAGES) return GMB_DB_ERR;
    if (cdx_build_catalog_leaf(pages[nPages], catNames, catOff, nTags) != GMB_DB_OK)
        return GMB_DB_ERR;
    nPages++;
    catalogRoot = (uint32_t)(CdxFormat_HeaderSize + (nPages - 1) * CdxFormat_PageSize);
    cdx_build_compound_header(header, catalogRoot, firstKeyLen);

    snprintf(dirbuf, sizeof(dirbuf), "%s", path);
    slash = strrchr(dirbuf, '/');
    if (slash && slash != dirbuf) {
        *slash = 0;
        if (mkdir(dirbuf, 0755) != 0 && errno != EEXIST) return GMB_DB_ERR;
    }
    fs = fopen(path, "wb");
    if (!fs) return GMB_DB_ERR;
    if (fwrite(header, 1, CdxFormat_HeaderSize, fs) != CdxFormat_HeaderSize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    for (i = 0; i < nPages; i++) {
        if (fwrite(pages[i], 1, CdxFormat_PageSize, fs) != CdxFormat_PageSize) {
            fclose(fs);
            return GMB_DB_ERR;
        }
    }
    fclose(fs);
    return GMB_DB_OK;
}

struct CdxTagIndex {
    char CompoundPath[PATH_MAX];
    CdxTagDescriptor Desc;
    int Writable;
    int nEntries;
    CdxEntry Entries[GMB_CDX_MAX_ENTRIES];
    pthread_mutex_t Lock;
    int LockInited;
    CdxPageCache* PageCache;
    uint32_t CachedRootOffset;
};

static void cdx_free_entries(CdxTagIndex* self)
{
    int i;
    for (i = 0; i < self->nEntries; i++) {
        free(self->Entries[i].Key);
        self->Entries[i].Key = NULL;
    }
    self->nEntries = 0;
}

static int cdx_walk_tag(FILE* fs, uint32_t offset, int keyLength, CdxEntry* out, int cap, int* n, int depth)
{
    uint8_t page[CdxFormat_PageSize];
    uint16_t attr, keyCount;
    int isLeaf, i;
    if (depth > GMB_CDX_MAX_DEPTH || *n >= cap) return GMB_DB_OK;
    if (cdx_read_page(fs, offset, page) != 0) return GMB_DB_ERR;
    attr = mdx_ru16(page);
    keyCount = mdx_ru16(page + 2);
    isLeaf = (attr & 2) != 0;
    if (isLeaf) {
        int nd = 0;
        if (cdx_decode_leaf(page, keyCount, keyLength, out + *n, cap - *n, &nd) != GMB_DB_OK)
            return GMB_DB_ERR;
        *n += nd;
        return GMB_DB_OK;
    }
    for (i = 0; i <= (int)keyCount; i++) {
        uint32_t child = cdx_interior_child(page, i, keyCount, keyLength);
        if (child != 0 && child != CDX_U32MAX) {
            if (cdx_walk_tag(fs, child, keyLength, out, cap, n, depth + 1) != GMB_DB_OK)
                return GMB_DB_ERR;
        }
    }
    return GMB_DB_OK;
}

static int cdx_load_entries(CdxTagIndex* self)
{
    FILE* fs;
    self->nEntries = 0;
    if (self->Desc.RootOffset == 0) return GMB_DB_OK;
    fs = fopen(self->CompoundPath, "rb");
    if (!fs) return GMB_DB_ERR;
    if (cdx_walk_tag(fs, self->Desc.RootOffset, self->Desc.KeyLength,
                     self->Entries, GMB_CDX_MAX_ENTRIES, &self->nEntries, 0) != GMB_DB_OK) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    fclose(fs);
    return GMB_DB_OK;
}

static CdxTagIndex* cdx_open_desc(const char* path, const CdxTagDescriptor* d, int writable)
{
    CdxTagIndex* self = (CdxTagIndex*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->CompoundPath, sizeof(self->CompoundPath), "%s", path);
    self->Desc = *d;
    self->Writable = writable;
    if (cdx_load_entries(self) != GMB_DB_OK) {
        CdxTagIndex_Dispose(self);
        return NULL;
    }
    if (pthread_mutex_init(&self->Lock, NULL) == 0)
        self->LockInited = 1;
    return self;
}

static void cdx_fill_write(CdxTagIndex* idx, CdxTagWrite* w, const uint8_t** keys, int* lens, int* recs)
{
    int i;
    w->TagName = idx->Desc.Name;
    w->Expression = idx->Desc.Expression;
    w->KeyLength = idx->Desc.KeyLength;
    w->Unique = idx->Desc.Unique;
    w->Descending = idx->Desc.Descending;
    w->NEntries = idx->nEntries;
    w->Keys = keys;
    w->KeyLens = lens;
    w->Recnos = recs;
    for (i = 0; i < idx->nEntries; i++) {
        keys[i] = idx->Entries[i].Key;
        lens[i] = idx->Entries[i].KeyLen;
        recs[i] = idx->Entries[i].Recno;
    }
}

static int cdx_rewrite(CdxTagIndex* self)
{
    CdxTagDescriptor table[GMB_CDX_MAX_TAGS];
    CdxTagWrite writes[GMB_CDX_MAX_TAGS];
    CdxTagIndex* others[GMB_CDX_MAX_TAGS];
    const uint8_t* keys[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int lens[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int recs[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int nTags = 0, i, rc;
    memset(others, 0, sizeof(others));
    if (CdxCompound_ReadTagTable(self->CompoundPath, table, GMB_CDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return GMB_DB_ERR;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, self->Desc.Name) == 0) {
            writes[i].TagName = self->Desc.Name;
            writes[i].Expression = self->Desc.Expression;
            writes[i].KeyLength = self->Desc.KeyLength;
            writes[i].Unique = self->Desc.Unique;
            writes[i].Descending = self->Desc.Descending;
            writes[i].NEntries = self->nEntries;
            writes[i].Keys = keys[i];
            writes[i].KeyLens = lens[i];
            writes[i].Recnos = recs[i];
            {
                int k;
                for (k = 0; k < self->nEntries; k++) {
                    keys[i][k] = self->Entries[k].Key;
                    lens[i][k] = self->Entries[k].KeyLen;
                    recs[i][k] = self->Entries[k].Recno;
                }
            }
        } else {
            others[i] = cdx_open_desc(self->CompoundPath, &table[i], 0);
            if (!others[i]) {
                int j;
                for (j = 0; j < i; j++) CdxTagIndex_Dispose(others[j]);
                return GMB_DB_ERR;
            }
            cdx_fill_write(others[i], &writes[i], keys[i], lens[i], recs[i]);
        }
    }
    rc = CdxCompound_WriteCompound(self->CompoundPath, writes, nTags);
    for (i = 0; i < nTags; i++)
        CdxTagIndex_Dispose(others[i]);
    return rc;
}

CdxTagIndex* CdxTagIndex_CreateOrReplace(const char* compoundPath, const char* tagName,
                                         const char* keyExpression, int keyLength, int isUnique, int descending)
{
    char norm[16];
    CdxTagDescriptor table[GMB_CDX_MAX_TAGS];
    CdxTagWrite writes[GMB_CDX_MAX_TAGS + 1];
    CdxTagIndex* others[GMB_CDX_MAX_TAGS];
    const uint8_t* keys[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int lens[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int recs[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int nTags = 0, nWrites = 0, i, exists;
    CdxTagDescriptor created;

    if (!compoundPath || !tagName || !keyExpression || keyLength <= 0) return NULL;
    mdx_normalize_tag(tagName, norm, sizeof(norm));
    if (norm[0] == 0) return NULL;
    memset(others, 0, sizeof(others));
    exists = access(compoundPath, F_OK) == 0;
    if (exists && CdxCompound_ReadTagTable(compoundPath, table, GMB_CDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0) continue;
        others[nWrites] = cdx_open_desc(compoundPath, &table[i], 0);
        if (!others[nWrites]) {
            int j;
            for (j = 0; j < nWrites; j++) CdxTagIndex_Dispose(others[j]);
            return NULL;
        }
        cdx_fill_write(others[nWrites], &writes[nWrites], keys[nWrites], lens[nWrites], recs[nWrites]);
        nWrites++;
    }
    writes[nWrites].TagName = norm;
    writes[nWrites].Expression = keyExpression;
    writes[nWrites].KeyLength = keyLength;
    writes[nWrites].Unique = isUnique;
    writes[nWrites].Descending = descending;
    writes[nWrites].Keys = NULL;
    writes[nWrites].KeyLens = NULL;
    writes[nWrites].Recnos = NULL;
    writes[nWrites].NEntries = 0;
    nWrites++;
    if (CdxCompound_WriteCompound(compoundPath, writes, nWrites) != GMB_DB_OK) {
        for (i = 0; i < nWrites - 1; i++) CdxTagIndex_Dispose(others[i]);
        return NULL;
    }
    for (i = 0; i < nWrites - 1; i++)
        CdxTagIndex_Dispose(others[i]);
    if (CdxCompound_ReadTagTable(compoundPath, table, GMB_CDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    memset(&created, 0, sizeof(created));
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0) {
            created = table[i];
            break;
        }
    }
    if (created.Name[0] == 0) return NULL;
    return cdx_open_desc(compoundPath, &created, 1);
}

CdxTagIndex* CdxTagIndex_Open(const char* compoundPath, const char* tagName, int writable)
{
    CdxTagDescriptor table[GMB_CDX_MAX_TAGS];
    int nTags = 0, i;
    char norm[16];
    if (!compoundPath || !tagName) return NULL;
    mdx_normalize_tag(tagName, norm, sizeof(norm));
    if (CdxCompound_ReadTagTable(compoundPath, table, GMB_CDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0)
            return cdx_open_desc(compoundPath, &table[i], writable);
    }
    return NULL;
}

void CdxTagIndex_Dispose(CdxTagIndex* self)
{
    if (!self) return;
    cdx_free_entries(self);
    CdxPageCache_Dispose(self->PageCache);
    if (self->LockInited) pthread_mutex_destroy(&self->Lock);
    free(self);
}

const char* CdxTagIndex_get_Name(const CdxTagIndex* self)
{
    return self ? self->Desc.Name : NULL;
}

const char* CdxTagIndex_GetKeyExpressionString(const CdxTagIndex* self)
{
    return self ? self->Desc.Expression : NULL;
}

int CdxTagIndex_get_KeyLength(const CdxTagIndex* self)
{
    return self ? self->Desc.KeyLength : 0;
}

int CdxTagIndex_Seek(CdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    int exact, idx, rec;
    if (!self || !keyBytes) return -1;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->nEntries == 0)
        cdx_load_entries(self);
    if (self->nEntries == 0) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return -1;
    }
    idx = cdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (exact)
        rec = self->Entries[idx].Recno;
    else
        rec = (soft && idx < self->nEntries) ? self->Entries[idx].Recno : -1;
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rec;
}

void CdxTagIndex_EnablePageCache(CdxTagIndex* self)
{
    if (!self || !self->Writable) return;
    if (self->PageCache) return;
    self->PageCache = CdxPageCache_New(self->CompoundPath, 256);
    self->CachedRootOffset = self->Desc.RootOffset;
    cdx_free_entries(self);
}

static int cdx_update_tag_root(CdxTagIndex* self, uint32_t newRoot)
{
    uint8_t th[CdxFormat_PageSize];
    if (!self->PageCache || self->Desc.HeaderOffset == 0) return GMB_DB_ERR;
    if (CdxPageCache_GetPage(self->PageCache, self->Desc.HeaderOffset, th) != GMB_DB_OK)
        return GMB_DB_ERR;
    mdx_le32(th, newRoot);
    CdxPageCache_PutPage(self->PageCache, self->Desc.HeaderOffset, th, 1);
    if (CdxPageCache_Flush(self->PageCache) != GMB_DB_OK) return GMB_DB_ERR;
    self->CachedRootOffset = newRoot;
    self->Desc.RootOffset = newRoot;
    return GMB_DB_OK;
}

int CdxTagIndex_Insert(CdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, rc;
    uint8_t* copy;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes || keyLen < 0) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->PageCache) {
        uint32_t newRoot = 0;
        uint8_t padded[GMB_CDX_MAX_KEY];
        int kl = self->Desc.KeyLength;
        if (kl <= 0 || kl > GMB_CDX_MAX_KEY) {
            if (self->LockInited) pthread_mutex_unlock(&self->Lock);
            return GMB_DB_ARG;
        }
        cdx_pad_key(padded, kl, keyBytes, keyLen);
        rc = CdxBTreeBuilder_InsertEntry(self->PageCache, self->CachedRootOffset,
                                         padded, kl, recno, kl, self->Desc.Unique, &newRoot);
        if (rc == GMB_DB_OK)
            rc = cdx_update_tag_root(self, newRoot);
        if (rc == GMB_DB_OK)
            cdx_free_entries(self);
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return rc;
    }
    if (self->nEntries >= GMB_CDX_MAX_ENTRIES) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    idx = cdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (self->Desc.Unique && exact) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    copy = (uint8_t*)malloc((size_t)keyLen);
    if (!copy) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    if (keyLen > 0) memcpy(copy, keyBytes, (size_t)keyLen);
    memmove(&self->Entries[idx + 1], &self->Entries[idx], (size_t)(self->nEntries - idx) * sizeof(CdxEntry));
    self->Entries[idx].Key = copy;
    self->Entries[idx].KeyLen = keyLen;
    self->Entries[idx].Recno = recno;
    self->nEntries++;
    rc = cdx_rewrite(self);
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rc;
}

int CdxTagIndex_Delete(CdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, i;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    idx = cdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (exact) {
        for (i = idx; i < self->nEntries; i++) {
            if (mdx_keycmp(self->Entries[i].Key, self->Entries[i].KeyLen, keyBytes, keyLen) != 0)
                break;
            if (self->Entries[i].Recno == recno) {
                free(self->Entries[i].Key);
                memmove(&self->Entries[i], &self->Entries[i + 1],
                        (size_t)(self->nEntries - i - 1) * sizeof(CdxEntry));
                self->nEntries--;
                cdx_rewrite(self);
                break;
            }
        }
    }
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return GMB_DB_OK;
}

static void cdx_sc_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int CdxCompound_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/cdxsc_XXXXXX";
    char path[PATH_MAX];
    char path2[PATH_MAX];
    CdxTagIndex *nameIdx, *idIdx, *reopen, *many;
    CdxTagDescriptor table[4];
    int nTags = 0, i;
    uint8_t key[10];
    uint8_t head[16];
    FILE* f;
    long flen;

    if (!mkdtemp(dir)) return -2;
    snprintf(path, sizeof(path), "%s/t.cdx", dir);

    nameIdx = CdxTagIndex_CreateOrReplace(path, "name", "NAME", 10, 0, 0);
    if (!nameIdx) return -3;
    if (!CdxTagIndex_get_Name(nameIdx) || strcmp(CdxTagIndex_get_Name(nameIdx), "NAME") != 0) {
        CdxTagIndex_Dispose(nameIdx);
        return -4;
    }
    if (!CdxTagIndex_GetKeyExpressionString(nameIdx)
        || strcmp(CdxTagIndex_GetKeyExpressionString(nameIdx), "NAME") != 0) {
        CdxTagIndex_Dispose(nameIdx);
        return -5;
    }

    f = fopen(path, "rb");
    if (!f) {
        CdxTagIndex_Dispose(nameIdx);
        return -6;
    }
    if (fread(head, 1, 16, f) != 16) {
        fclose(f);
        CdxTagIndex_Dispose(nameIdx);
        return -7;
    }
    fseek(f, 0, SEEK_END);
    flen = ftell(f);
    fclose(f);
    if (head[14] != (CdxFormat_TypeCompact | CdxFormat_TypeCompound)
        || mdx_ru16(head + 12) != 10 || mdx_ru32(head) != 1536 || flen != 2048) {
        CdxTagIndex_Dispose(nameIdx);
        return -8;
    }

    if (CdxCompound_ReadTagTable(path, table, 4, &nTags) != GMB_DB_OK || nTags != 1
        || table[0].RootOffset != 1024 || table[0].KeyLength != 10 || table[0].Unique != 0) {
        CdxTagIndex_Dispose(nameIdx);
        return -9;
    }

    for (i = 0; i < 8; i++) {
        cdx_sc_key(key, i);
        if (CdxTagIndex_Insert(nameIdx, key, 10, 100 + i) != GMB_DB_OK) {
            CdxTagIndex_Dispose(nameIdx);
            return -10 - i;
        }
    }
    cdx_sc_key(key, 3);
    if (CdxTagIndex_Seek(nameIdx, key, 10, 0) != 103) {
        CdxTagIndex_Dispose(nameIdx);
        return -20;
    }
    CdxTagIndex_Dispose(nameIdx);

    idIdx = CdxTagIndex_CreateOrReplace(path, "id", "ID", 10, 1, 0);
    if (!idIdx) return -21;
    cdx_sc_key(key, 0);
    if (CdxTagIndex_Insert(idIdx, key, 10, 1) != GMB_DB_OK) {
        CdxTagIndex_Dispose(idIdx);
        return -22;
    }
    if (CdxTagIndex_Insert(idIdx, key, 10, 2) != GMB_DB_ERR) {
        CdxTagIndex_Dispose(idIdx);
        return -23;
    }
    CdxTagIndex_Dispose(idIdx);

    if (CdxCompound_ReadTagTable(path, table, 4, &nTags) != GMB_DB_OK || nTags != 2) return -24;

    reopen = CdxTagIndex_Open(path, "NAME", 0);
    if (!reopen) return -25;
    cdx_sc_key(key, 3);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 103) {
        CdxTagIndex_Dispose(reopen);
        return -26;
    }
    cdx_sc_key(key, 7);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 107) {
        CdxTagIndex_Dispose(reopen);
        return -27;
    }
    if (CdxTagIndex_Insert(reopen, key, 10, 1) != GMB_DB_ERR) {
        CdxTagIndex_Dispose(reopen);
        return -28;
    }
    CdxTagIndex_Dispose(reopen);

    reopen = CdxTagIndex_Open(path, "ID", 1);
    if (!reopen) return -29;
    cdx_sc_key(key, 0);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 1) {
        CdxTagIndex_Dispose(reopen);
        return -30;
    }
    if (CdxTagIndex_Delete(reopen, key, 10, 1) != GMB_DB_OK
        || CdxTagIndex_Seek(reopen, key, 10, 0) != -1) {
        CdxTagIndex_Dispose(reopen);
        return -31;
    }
    CdxTagIndex_Dispose(reopen);

    snprintf(path2, sizeof(path2), "%s/many.cdx", dir);
    many = CdxTagIndex_CreateOrReplace(path2, "k", "K", 10, 0, 0);
    if (!many) return -32;
    for (i = 0; i < 40; i++) {
        cdx_sc_key(key, i);
        if (CdxTagIndex_Insert(many, key, 10, i + 1) != GMB_DB_OK) {
            CdxTagIndex_Dispose(many);
            return -33;
        }
    }
    CdxTagIndex_Dispose(many);
    many = CdxTagIndex_Open(path2, "K", 0);
    if (!many) return -34;
    cdx_sc_key(key, 0);
    if (CdxTagIndex_Seek(many, key, 10, 0) != 1) {
        CdxTagIndex_Dispose(many);
        return -35;
    }
    cdx_sc_key(key, 39);
    if (CdxTagIndex_Seek(many, key, 10, 0) != 40) {
        CdxTagIndex_Dispose(many);
        return -36;
    }
    cdx_sc_key(key, 20);
    if (CdxTagIndex_Seek(many, key, 10, 0) != 21) {
        CdxTagIndex_Dispose(many);
        return -37;
    }
    CdxTagIndex_Dispose(many);

    unlink(path);
    unlink(path2);
    rmdir(dir);
    return GMB_DB_OK;
}
