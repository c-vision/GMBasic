/* FILE: native/gmb_db_c/src/ISAM/IsamOfflineMigrator.c */
/* Port 1:1 di modules/Database/ISAM/IsamOfflineMigrator.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"
#include <unistd.h>

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamOfflineMigrator
//using System.Runtime.InteropServices;
//using System.Text;
//
//// ReSharper disable CheckNamespace
//
//namespace Basic.ISAM;
//
///// <summary>
///// Migrazione offline file ISAM legacy: schema pre-<c>ISCH</c> e correzione <see cref="Config.FileHeader.LobHeapStartOffset"/>.
///// </summary>
//public static class IsamOfflineMigrator
//{
//    private static readonly byte[] SchemaMagic = "ISCH"u8.ToArray();
//
//    /// <summary>
//    /// Applica migrazione in-place se necessario. Restituisce <see langword="true"/> se il file è stato modificato.
//    /// </summary>
//    public static bool MigrateIfNeeded(string filePath, Config.IsamSchema codeSchema)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(filePath);
//        ArgumentNullException.ThrowIfNull(codeSchema);
//
//        if (!File.Exists(filePath))
//        {
//            return false;
//        }
//
//        using var stream = new FileStream(filePath, FileMode.Open, FileAccess.ReadWrite, FileShare.Read);
//        var header = ReadHeader(stream);
//        ValidateMagic(header);
//
//        stream.Seek(header.SchemaOffset, SeekOrigin.Begin);
//        var schemaBuffer = new byte[header.SchemaLength];
//        _ = stream.Read(schemaBuffer, 0, schemaBuffer.Length);
//
//        var needsSchema = !HasIschMagic(schemaBuffer);
//        var needsHeap = NeedsLobHeapStartFix(header, stream, codeSchema);
//
//        if (!needsSchema && !needsHeap)
//        {
//            return false;
//        }
//
//        if (needsSchema)
//        {
//            RewriteSchemaBlock(stream, ref header, codeSchema);
//        }
//
//        if (needsHeap)
//        {
//            header.LobHeapStartOffset = ComputeMinimumHeapStart(header, stream, codeSchema);
//            WriteHeader(stream, header);
//        }
//
//        stream.Flush(true);
//        return true;
//    }
//
//    internal static bool HasIschMagic(ReadOnlySpan<byte> schemaBuffer) =>
//        schemaBuffer.Length >= SchemaMagic.Length && schemaBuffer[..SchemaMagic.Length].SequenceEqual(SchemaMagic);
//
//    internal static bool NeedsLobHeapStartFix(
//        Config.FileHeader header,
//        Stream stream,
//        Config.IsamSchema schema)
//    {
//        if (header.PrimaryKeyRootOffset < 0 && header.LastRecNumber == 0)
//        {
//            return false;
//        }
//
//        var minHeap = ComputeMinimumHeapStart(header, stream, schema);
//        if (header.LobHeapStartOffset < minHeap)
//        {
//            return true;
//        }
//
//        // ponytail: pre-fix bug stored heap end (NextAvailablePageOffset) as start.
//        return header.LobHeapStartOffset >= header.NextAvailablePageOffset
//               && header.NextAvailablePageOffset > minHeap;
//    }
//
//    internal static long ComputeMinimumHeapStart(
//        Config.FileHeader header,
//        Stream stream,
//        Config.IsamSchema schema)
//    {
//        var recordSize = header.RecordSize > 0
//            ? header.RecordSize
//            : SchemaManager.CalculateRecordSize(schema);
//        var dataStart = ComputeDataAreaStart(header, stream);
//        var dataEnd = dataStart + header.LastRecNumber * recordSize;
//        return HeapRelocation.AlignUp(dataEnd, HeapRelocation.PageSize);
//    }
//
//    private static long ComputeDataAreaStart(Config.FileHeader header, Stream stream)
//    {
//        if (header.SecondaryIndexMapOffset <= 0)
//        {
//            return header.SchemaOffset + header.SchemaLength;
//        }
//
//        return header.SecondaryIndexMapOffset + ReadIndexMapLengthFromStream(stream, header);
//    }
//
//    private static int ReadIndexMapLengthFromStream(Stream stream, Config.FileHeader header)
//    {
//        stream.Seek(header.SecondaryIndexMapOffset, SeekOrigin.Begin);
//        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
//        return ReadIndexMapByteLength(reader);
//    }
//
//    private static int ReadIndexMapByteLength(BinaryReader reader)
//    {
//        var start = reader.BaseStream.Position;
//        var count = reader.ReadInt32();
//        for (var i = 0; i < count; i++)
//        {
//            _ = reader.ReadString();
//            _ = reader.ReadInt64();
//        }
//
//        return (int)(reader.BaseStream.Position - start);
//    }
//
//    private static void RewriteSchemaBlock(Stream stream, ref Config.FileHeader header, Config.IsamSchema codeSchema)
//    {
//        var newSchema = SchemaManager.SerializeSchema(codeSchema);
//        if (newSchema.Length != header.SchemaLength)
//        {
//            throw new InvalidOperationException(
//                "Migrazione schema legacy richiede stessa lunghezza blocco schema; ricreare il file con PACK/export.");
//        }
//
//        stream.Seek(header.SchemaOffset, SeekOrigin.Begin);
//        stream.Write(newSchema, 0, newSchema.Length);
//    }
//
//    private static Config.FileHeader ReadHeader(Stream stream)
//    {
//        stream.Seek(0, SeekOrigin.Begin);
//        return MarshalHelper.ReadStructure<Config.FileHeader>(stream);
//    }
//
//    private static void WriteHeader(Stream stream, Config.FileHeader header)
//    {
//        stream.Seek(0, SeekOrigin.Begin);
//        MarshalHelper.WriteStructure(stream, header);
//    }
//
//    private static void ValidateMagic(Config.FileHeader header)
//    {
//        if (header.Magic == null || !header.Magic.AsSpan().SequenceEqual(Config.MagicBytes))
//        {
//            throw new InvalidDataException("File ISAM non valido.");
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

static int om_rd_i32(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}

static int om_rd_i64(FILE* f, int64_t* v)
{
    uint8_t b[8];
    uint64_t u = 0;
    int i;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)b[i] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

static int om_skip_7bit_str(FILE* f)
{
    unsigned v = 0, shift = 0;
    for (;;) {
        int c = fgetc(f);
        if (c < 0) return -1;
        v |= (unsigned)(c & 0x7f) << shift;
        if ((c & 0x80) == 0) break;
        shift += 7;
        if (shift > 28) return -1;
    }
    if (v && fseek(f, (long)v, SEEK_CUR) != 0) return -1;
    return 0;
}

static int om_index_map_len(FILE* f, const IsamFileHeader* h)
{
    int32_t count = 0, i;
    long start, end;
    int64_t dummy;
    if (h->SecondaryIndexMapOffset <= 0) return 0;
    if (fseek(f, (long)h->SecondaryIndexMapOffset, SEEK_SET) != 0) return -1;
    start = ftell(f);
    if (om_rd_i32(f, &count) != 0 || count < 0) return -1;
    for (i = 0; i < count; i++) {
        if (om_skip_7bit_str(f) != 0) return -1;
        if (om_rd_i64(f, &dummy) != 0) return -1;
    }
    end = ftell(f);
    return (int)(end - start);
}

static int64_t om_data_area_start(FILE* f, const IsamFileHeader* h)
{
    int mapLen;
    if (h->SecondaryIndexMapOffset <= 0)
        return h->SchemaOffset + h->SchemaLength;
    mapLen = om_index_map_len(f, h);
    if (mapLen < 0) return h->SchemaOffset + h->SchemaLength;
    return h->SecondaryIndexMapOffset + mapLen;
}

static int64_t om_min_heap(FILE* f, const IsamFileHeader* h, const IsamSchema* schema)
{
    int recSize = h->RecordSize > 0 ? h->RecordSize : SchemaManager_CalculateRecordSize(schema);
    int64_t dataStart = om_data_area_start(f, h);
    int64_t dataEnd = dataStart + h->LastRecNumber * (int64_t)recSize;
    return HeapRelocation_AlignUp(dataEnd, HeapRelocation_PageSize);
}

static int om_has_isch(const uint8_t* buf, int len)
{
    return buf && len >= 4 && memcmp(buf, SchemaManager_SchemaMagic, 4) == 0;
}

static int om_needs_heap(FILE* f, const IsamFileHeader* h, const IsamSchema* schema)
{
    int64_t minHeap;
    if (h->PrimaryKeyRootOffset < 0 && h->LastRecNumber == 0) return 0;
    minHeap = om_min_heap(f, h, schema);
    if (h->LobHeapStartOffset < minHeap) return 1;
    return h->LobHeapStartOffset >= h->NextAvailablePageOffset && h->NextAvailablePageOffset > minHeap;
}

static int om_rewrite_schema(FILE* f, const IsamFileHeader* h, const IsamSchema* schema)
{
    uint8_t* isch = NULL;
    int len = 0;
    if (SchemaManager_SerializeSchema(schema, &isch, &len) != GMB_DB_OK) return GMB_DB_ERR;
    if (len != h->SchemaLength) {
        free(isch);
        return GMB_DB_ERR;
    }
    if (fseek(f, (long)h->SchemaOffset, SEEK_SET) != 0) { free(isch); return GMB_DB_ERR; }
    if ((int)fwrite(isch, 1, (size_t)len, f) != len) { free(isch); return GMB_DB_ERR; }
    free(isch);
    return GMB_DB_OK;
}

int IsamOfflineMigrator_MigrateIfNeeded(const char* filePath, const IsamSchema* codeSchema)
{
    FILE* f;
    IsamFileHeader h;
    uint8_t* schemaBuf = NULL;
    int needsSchema, needsHeap, changed = 0;
    if (!filePath || !filePath[0] || !codeSchema) return GMB_DB_ARG;
    f = fopen(filePath, "rb+");
    if (!f) return 0;
    if (IsamFileHeader_Read(f, &h) != GMB_DB_OK) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (h.SchemaLength < 0 || h.SchemaLength > 1 << 20) { fclose(f); return GMB_DB_ERR; }
    schemaBuf = (uint8_t*)calloc(1, h.SchemaLength > 0 ? (size_t)h.SchemaLength : 1);
    if (!schemaBuf) { fclose(f); return GMB_DB_ERR; }
    if (h.SchemaLength > 0) {
        if (fseek(f, (long)h.SchemaOffset, SEEK_SET) != 0 ||
            (int)fread(schemaBuf, 1, (size_t)h.SchemaLength, f) != h.SchemaLength) {
            free(schemaBuf); fclose(f); return GMB_DB_ERR;
        }
    }
    needsSchema = !om_has_isch(schemaBuf, (int)h.SchemaLength);
    needsHeap = om_needs_heap(f, &h, codeSchema);
    free(schemaBuf);
    if (!needsSchema && !needsHeap) { fclose(f); return 0; }
    if (needsSchema) {
        if (om_rewrite_schema(f, &h, codeSchema) != GMB_DB_OK) { fclose(f); return GMB_DB_ERR; }
        changed = 1;
    }
    if (needsHeap) {
        h.LobHeapStartOffset = om_min_heap(f, &h, codeSchema);
        if (IsamFileHeader_Write(f, &h) != GMB_DB_OK) { fclose(f); return GMB_DB_ERR; }
        changed = 1;
    }
    fflush(f);
    fclose(f);
    return changed;
}

int IsamOfflineMigrator_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_offmig_selfcheck.isdb";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    FILE* f;
    IsamFileHeader h;
    uint8_t magic[4];
    Engine* e;
    IsamFieldValue vals[2];
    int rc;

    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema.Fields = fields;
    schema.FieldCount = 2;
    snprintf(schema.PrimaryKey.Name, sizeof(schema.PrimaryKey.Name), "%s", "Id");
    snprintf(schema.PrimaryKey.KeyExpression, sizeof(schema.PrimaryKey.KeyExpression), "%s", "Id");
    schema.PrimaryKey.KeyLength = 10;
    schema.PrimaryKey.IsUnique = 1;
    unlink(path);
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -322;
    rc = IsamOfflineMigrator_MigrateIfNeeded(path, &schema);
    if (rc != 0) return -323;
    f = fopen(path, "rb+");
    if (!f) return -324;
    if (IsamFileHeader_Read(f, &h) != GMB_DB_OK) { fclose(f); return -325; }
    if (fseek(f, (long)h.SchemaOffset, SEEK_SET) != 0) { fclose(f); return -326; }
    if (fwrite("XXXX", 1, 4, f) != 4) { fclose(f); return -327; }
    fclose(f);
    rc = IsamOfflineMigrator_MigrateIfNeeded(path, &schema);
    if (rc != 1) return -328;
    f = fopen(path, "rb");
    if (!f) return -329;
    if (IsamFileHeader_Read(f, &h) != GMB_DB_OK) { fclose(f); return -330; }
    if (fseek(f, (long)h.SchemaOffset, SEEK_SET) != 0) { fclose(f); return -331; }
    if (fread(magic, 1, 4, f) != 4) { fclose(f); return -332; }
    fclose(f);
    if (memcmp(magic, SchemaManager_SchemaMagic, 4) != 0) return -333;

    e = Engine_Open(path, &schema);
    if (!e) return -334;
    memset(vals, 0, sizeof(vals));
    vals[0].i64 = 1;
    vals[1].text = "N1";
    if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -335; }
    Engine_Dispose(e);
    f = fopen(path, "rb+");
    if (!f) return -336;
    if (IsamFileHeader_Read(f, &h) != GMB_DB_OK) { fclose(f); return -337; }
    h.LobHeapStartOffset = h.NextAvailablePageOffset;
    if (IsamFileHeader_Write(f, &h) != GMB_DB_OK) { fclose(f); return -338; }
    fclose(f);
    rc = IsamOfflineMigrator_MigrateIfNeeded(path, &schema);
    if (rc != 1) return -339;
    f = fopen(path, "rb");
    if (!f) return -340;
    if (IsamFileHeader_Read(f, &h) != GMB_DB_OK) { fclose(f); return -341; }
    fclose(f);
    if (h.LobHeapStartOffset >= h.NextAvailablePageOffset) return -342;
    unlink(path);
    return GMB_DB_OK;
}
