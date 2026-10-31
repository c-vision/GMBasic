// FILE: Basic.ISAM.IsamOfflineMigrator
using System.Runtime.InteropServices;
using System.Text;

// ReSharper disable CheckNamespace

namespace Basic.ISAM;

/// <summary>
/// Migrazione offline file ISAM legacy: schema pre-<c>ISCH</c> e correzione <see cref="Config.FileHeader.LobHeapStartOffset"/>.
/// </summary>
public static class IsamOfflineMigrator
{
    private static readonly byte[] SchemaMagic = "ISCH"u8.ToArray();

    /// <summary>
    /// Applica migrazione in-place se necessario. Restituisce <see langword="true"/> se il file è stato modificato.
    /// </summary>
    public static bool MigrateIfNeeded(string filePath, Config.IsamSchema codeSchema)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(filePath);
        ArgumentNullException.ThrowIfNull(codeSchema);

        if (!File.Exists(filePath))
        {
            return false;
        }

        using var stream = new FileStream(filePath, FileMode.Open, FileAccess.ReadWrite, FileShare.Read);
        var header = ReadHeader(stream);
        ValidateMagic(header);

        stream.Seek(header.SchemaOffset, SeekOrigin.Begin);
        var schemaBuffer = new byte[header.SchemaLength];
        _ = stream.Read(schemaBuffer, 0, schemaBuffer.Length);

        var needsSchema = !HasIschMagic(schemaBuffer);
        var needsHeap = NeedsLobHeapStartFix(header, stream, codeSchema);

        if (!needsSchema && !needsHeap)
        {
            return false;
        }

        if (needsSchema)
        {
            RewriteSchemaBlock(stream, ref header, codeSchema);
        }

        if (needsHeap)
        {
            header.LobHeapStartOffset = ComputeMinimumHeapStart(header, stream, codeSchema);
            WriteHeader(stream, header);
        }

        stream.Flush(true);
        return true;
    }

    internal static bool HasIschMagic(ReadOnlySpan<byte> schemaBuffer) =>
        schemaBuffer.Length >= SchemaMagic.Length && schemaBuffer[..SchemaMagic.Length].SequenceEqual(SchemaMagic);

    internal static bool NeedsLobHeapStartFix(
        Config.FileHeader header,
        Stream stream,
        Config.IsamSchema schema)
    {
        if (header.PrimaryKeyRootOffset < 0 && header.LastRecNumber == 0)
        {
            return false;
        }

        var minHeap = ComputeMinimumHeapStart(header, stream, schema);
        if (header.LobHeapStartOffset < minHeap)
        {
            return true;
        }

        // ponytail: pre-fix bug stored heap end (NextAvailablePageOffset) as start.
        return header.LobHeapStartOffset >= header.NextAvailablePageOffset
               && header.NextAvailablePageOffset > minHeap;
    }

    internal static long ComputeMinimumHeapStart(
        Config.FileHeader header,
        Stream stream,
        Config.IsamSchema schema)
    {
        var recordSize = header.RecordSize > 0
            ? header.RecordSize
            : SchemaManager.CalculateRecordSize(schema);
        var dataStart = ComputeDataAreaStart(header, stream);
        var dataEnd = dataStart + header.LastRecNumber * recordSize;
        return HeapRelocation.AlignUp(dataEnd, HeapRelocation.PageSize);
    }

    private static long ComputeDataAreaStart(Config.FileHeader header, Stream stream)
    {
        if (header.SecondaryIndexMapOffset <= 0)
        {
            return header.SchemaOffset + header.SchemaLength;
        }

        return header.SecondaryIndexMapOffset + ReadIndexMapLengthFromStream(stream, header);
    }

    private static int ReadIndexMapLengthFromStream(Stream stream, Config.FileHeader header)
    {
        stream.Seek(header.SecondaryIndexMapOffset, SeekOrigin.Begin);
        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
        return ReadIndexMapByteLength(reader);
    }

    private static int ReadIndexMapByteLength(BinaryReader reader)
    {
        var start = reader.BaseStream.Position;
        var count = reader.ReadInt32();
        for (var i = 0; i < count; i++)
        {
            _ = reader.ReadString();
            _ = reader.ReadInt64();
        }

        return (int)(reader.BaseStream.Position - start);
    }

    private static void RewriteSchemaBlock(Stream stream, ref Config.FileHeader header, Config.IsamSchema codeSchema)
    {
        var newSchema = SchemaManager.SerializeSchema(codeSchema);
        if (newSchema.Length != header.SchemaLength)
        {
            throw new InvalidOperationException(
                "Migrazione schema legacy richiede stessa lunghezza blocco schema; ricreare il file con PACK/export.");
        }

        stream.Seek(header.SchemaOffset, SeekOrigin.Begin);
        stream.Write(newSchema, 0, newSchema.Length);
    }

    private static Config.FileHeader ReadHeader(Stream stream)
    {
        stream.Seek(0, SeekOrigin.Begin);
        return MarshalHelper.ReadStructure<Config.FileHeader>(stream);
    }

    private static void WriteHeader(Stream stream, Config.FileHeader header)
    {
        stream.Seek(0, SeekOrigin.Begin);
        MarshalHelper.WriteStructure(stream, header);
    }

    private static void ValidateMagic(Config.FileHeader header)
    {
        if (header.Magic == null || !header.Magic.AsSpan().SequenceEqual(Config.MagicBytes))
        {
            throw new InvalidDataException("File ISAM non valido.");
        }
    }
}
