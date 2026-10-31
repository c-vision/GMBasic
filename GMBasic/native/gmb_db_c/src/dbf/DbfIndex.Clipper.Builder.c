/* FILE: native/gmb_db_c/src/DBF/DbfIndex.Clipper.Builder.c */
/* Port 1:1 di modules/Database/DBF/DbfIndex.Clipper.Builder.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfIndex.Clipper.Builder
//
//using System.Text;
//
//namespace Basic.DBF;
//
///// <summary>
///// Writes Harbour/Clipper 5.x compatible NTX files (signature 0x0006, 1024-byte pages).
///// </summary>
//internal static class ClipperNtxBuilder
//{
//    public static ushort CalcMaxKeys(int keyLength) =>
//        (ushort)((ClipperNtxHeader.PageSize - 2) / (keyLength + 10) - 1);
//
//    public static void Write(
//        string path,
//        string keyExpression,
//        int keyLength,
//        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries,
//        bool unique,
//        bool descending = false)
//    {
//        var sorted = entries
//            .Select(e => (Key: e.Key.ToArray(), e.Recno))
//            .OrderBy(e => e.Key, Comparer<byte[]>.Create(static (a, b) => a.AsSpan().SequenceCompareTo(b)))
//            .ToList();
//
//        var maxKeys = CalcMaxKeys(keyLength);
//        var itemSize = (ushort)(keyLength + 8);
//        var dataPages = new List<byte[]>();
//
//        uint rootPage;
//        if (sorted.Count == 0)
//        {
//            dataPages.Add(BuildLeafPage([], keyLength, maxKeys));
//            rootPage = 1;
//        }
//        else
//        {
//            var leaves = new List<(uint PageNum, byte[] FirstKey)>();
//            for (var i = 0; i < sorted.Count; i += maxKeys)
//            {
//                var chunk = sorted.Skip(i).Take(maxKeys)
//                    .Select(e => (Key: (ReadOnlyMemory<byte>)e.Key, e.Recno))
//                    .ToList();
//                dataPages.Add(BuildLeafPage(chunk, keyLength, maxKeys));
//                leaves.Add(((uint)dataPages.Count, chunk[0].Key.ToArray()));
//            }
//
//            var level = leaves;
//            while (level.Count > 1)
//            {
//                level = BuildInternalLevel(level, dataPages, keyLength, maxKeys);
//            }
//
//            rootPage = level[0].PageNum;
//        }
//
//        var header = BuildHeader(
//            keyExpression,
//            keyLength,
//            itemSize,
//            maxKeys,
//            rootPage,
//            (uint)(dataPages.Count + 1),
//            unique,
//            descending);
//
//        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
//        using var fs = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
//        fs.Write(header);
//        foreach (var page in dataPages)
//        {
//            fs.Write(page);
//        }
//    }
//
//    private static List<(uint PageNum, byte[] FirstKey)> BuildInternalLevel(
//        List<(uint PageNum, byte[] FirstKey)> children,
//        List<byte[]> dataPages,
//        int keyLength,
//        ushort maxKeys)
//    {
//        var parents = new List<(uint PageNum, byte[] FirstKey)>();
//        var index = 0;
//
//        while (index < children.Count)
//        {
//            var groupStart = index;
//            var childPages = new List<uint> { children[index].PageNum };
//            var separators = new List<byte[]>();
//            index++;
//
//            while (index < children.Count && separators.Count < maxKeys)
//            {
//                separators.Add(children[index].FirstKey);
//                childPages.Add(children[index].PageNum);
//                index++;
//            }
//
//            dataPages.Add(BuildInternalPage(childPages, separators, keyLength, maxKeys));
//            var pageNum = (uint)dataPages.Count;
//            var promoteKey = separators.Count > 0
//                ? separators[0]
//                : children[groupStart].FirstKey;
//            parents.Add((pageNum, promoteKey));
//        }
//
//        return parents;
//    }
//
//    private static byte[] BuildHeader(
//        string keyExpression,
//        int keyLength,
//        ushort itemSize,
//        ushort maxKeys,
//        uint rootPage,
//        uint nextPage,
//        bool unique,
//        bool descending)
//    {
//        var header = new byte[ClipperNtxHeader.PageSize];
//        BitConverter.GetBytes((ushort)0x0006).CopyTo(header, 0);
//        BitConverter.GetBytes((ushort)0x0006).CopyTo(header, 2);
//        BitConverter.GetBytes(rootPage).CopyTo(header, 4);
//        BitConverter.GetBytes(nextPage).CopyTo(header, 8);
//        BitConverter.GetBytes(itemSize).CopyTo(header, 12);
//        BitConverter.GetBytes((ushort)keyLength).CopyTo(header, 14);
//        BitConverter.GetBytes(maxKeys).CopyTo(header, 18);
//        BitConverter.GetBytes((ushort)(maxKeys / 2)).CopyTo(header, 20);
//
//        var exprBytes = Encoding.ASCII.GetBytes(keyExpression);
//        exprBytes.AsSpan(0, Math.Min(exprBytes.Length, 255))
//            .CopyTo(header.AsSpan(ClipperNtxHeader.KeyExpressionOffset));
//
//        if (unique)
//        {
//            header[ClipperNtxHeader.UniqueFlagOffset] = 1;
//        }
//
//        if (descending)
//        {
//            header[ClipperNtxHeader.DescendingFlagOffset] = 1;
//        }
//
//        return header;
//    }
//
//    private static byte[] BuildLeafPage(
//        IReadOnlyList<(ReadOnlyMemory<byte> Key, int Recno)> entries,
//        int keyLength,
//        ushort maxKeys)
//    {
//        var page = new byte[ClipperNtxHeader.PageSize];
//        BitConverter.GetBytes((ushort)entries.Count).CopyTo(page, 0);
//
//        var dataOffset = (maxKeys + 2) << 1;
//        for (var slot = 0; slot <= maxKeys; slot++)
//        {
//            BitConverter.GetBytes((ushort)dataOffset).CopyTo(page, 2 + (slot << 1));
//            dataOffset += keyLength + 8;
//        }
//
//        for (var i = 0; i < entries.Count; i++)
//        {
//            var entryOffset = BitConverter.ToUInt16(page, 2 + (i << 1));
//            BitConverter.GetBytes(0u).CopyTo(page, entryOffset);
//            BitConverter.GetBytes((uint)entries[i].Recno).CopyTo(page, entryOffset + 4);
//            entries[i].Key.Span.CopyTo(page.AsSpan(entryOffset + 8, keyLength));
//        }
//
//        return page;
//    }
//
//    private static byte[] BuildInternalPage(
//        IReadOnlyList<uint> childPages,
//        IReadOnlyList<byte[]> separatorKeys,
//        int keyLength,
//        ushort maxKeys)
//    {
//        var page = new byte[ClipperNtxHeader.PageSize];
//        var itemCount = separatorKeys.Count;
//        BitConverter.GetBytes((ushort)itemCount).CopyTo(page, 0);
//
//        var dataOffset = (maxKeys + 2) << 1;
//        for (var slot = 0; slot <= maxKeys; slot++)
//        {
//            BitConverter.GetBytes((ushort)dataOffset).CopyTo(page, 2 + (slot << 1));
//            dataOffset += keyLength + 8;
//        }
//
//        for (var i = 0; i < childPages.Count; i++)
//        {
//            var entryOffset = BitConverter.ToUInt16(page, 2 + (i << 1));
//            BitConverter.GetBytes(childPages[i]).CopyTo(page, entryOffset);
//            BitConverter.GetBytes(0u).CopyTo(page, entryOffset + 4);
//            if (i < separatorKeys.Count)
//            {
//                separatorKeys[i].AsSpan(0, Math.Min(separatorKeys[i].Length, keyLength))
//                    .CopyTo(page.AsSpan(entryOffset + 8, keyLength));
//            }
//        }
//
//        return page;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */





