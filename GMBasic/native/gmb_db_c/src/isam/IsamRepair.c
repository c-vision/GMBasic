/* FILE: native/gmb_db_c/src/ISAM/IsamRepair.c */
/* Port 1:1 di modules/Database/ISAM/IsamRepair.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamRepair
//
//using System.Runtime.InteropServices;
//using Basic.Databases;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Best-effort offline prep for corrupt ISAM files before engine open.
///// ponytail: WAL recover + header/index-root fixes; full rebuild via <see cref="Engine.Repair"/>.
///// </summary>
//public static class IsamRepair
//{
//    /// <summary>
//    /// Replays WAL and patches header/index roots so a damaged file can be opened for repair.
//    /// </summary>
//    /// <param name="filePath">Path to the .isdb file.</param>
//    public static void PrepareForOpen(string filePath)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(filePath);
//        if (!File.Exists(filePath))
//        {
//            throw new FileNotFoundException("ISAM file not found.", filePath);
//        }
//
//        WriteAheadLog.Recover(filePath);
//        FixHeaderBestEffort(filePath);
//    }
//
//    private static void FixHeaderBestEffort(string filePath)
//    {
//        using var stream = new FileStream(filePath, FileMode.Open, FileAccess.ReadWrite, FileShare.Read);
//        var fileLength = stream.Length;
//        if (fileLength < Config.FileHeaderFieldsPrefixSize)
//        {
//            throw new InvalidDataException($"ISAM file too small to repair ({fileLength} bytes).");
//        }
//
//        stream.Seek(0, SeekOrigin.Begin);
//        var header = MarshalHelper.ReadStructure<Config.FileHeader>(stream);
//
//        for (var i = 0; i < Config.MagicBytes.Length; i++)
//        {
//            header.Magic[i] = Config.MagicBytes[i];
//        }
//
//        if (header.PrimaryKeyRootOffset != -1 &&
//            (header.PrimaryKeyRootOffset < 0 || header.PrimaryKeyRootOffset >= fileLength))
//        {
//            header.PrimaryKeyRootOffset = -1;
//        }
//
//        for (var i = 0; i < header.SecondaryIndexRootOffsets.Length; i++)
//        {
//            var root = header.SecondaryIndexRootOffsets[i];
//            if (root != -1 && (root < 0 || root >= fileLength))
//            {
//                header.SecondaryIndexRootOffsets[i] = -1;
//            }
//        }
//
//        if (header.SchemaOffset <= 0 || header.SchemaOffset >= fileLength)
//        {
//            header.SchemaOffset = Config.HeaderSize;
//        }
//
//        if (header.SchemaLength <= 0 || header.SchemaOffset + header.SchemaLength > fileLength)
//        {
//            var maxLen = fileLength - header.SchemaOffset;
//            if (maxLen <= 0)
//            {
//                throw new InvalidDataException("Cannot locate schema block for repair.");
//            }
//
//            header.SchemaLength = maxLen;
//        }
//
//        if (header.RecordSize > 0)
//        {
//            var dataStart = EstimateDataAreaStart(header, stream, fileLength);
//            var maxRecords = Math.Max(0, (fileLength - dataStart) / header.RecordSize);
//            if (header.LastRecNumber > maxRecords)
//            {
//                header.LastRecNumber = maxRecords;
//            }
//        }
//
//        if (header.PageFreeListHeadOffset != -1 &&
//            (header.PageFreeListHeadOffset < 0 || header.PageFreeListHeadOffset >= fileLength))
//        {
//            header.PageFreeListHeadOffset = -1;
//        }
//
//        if (header.FreeListHeadOffset != -1 &&
//            (header.FreeListHeadOffset < 0 || header.FreeListHeadOffset >= fileLength))
//        {
//            header.FreeListHeadOffset = -1;
//        }
//
//        stream.Seek(0, SeekOrigin.Begin);
//        MarshalHelper.WriteStructure(stream, header);
//        stream.Flush(true);
//    }
//
//    private static long EstimateDataAreaStart(Config.FileHeader header, Stream stream, long fileLength)
//    {
//        if (header.SecondaryIndexMapOffset > 0 && header.SecondaryIndexMapOffset < fileLength)
//        {
//            stream.Seek(header.SecondaryIndexMapOffset, SeekOrigin.Begin);
//            try
//            {
//                var map = SchemaManager.DeserializeIndexMapFromStream(stream);
//                return header.SecondaryIndexMapOffset + SchemaManager.SerializeIndexMap(map).Length;
//            }
//            catch (IOException)
//            {
//                // fall through
//            }
//            catch (InvalidDataException)
//            {
//                // fall through
//            }
//        }
//
//        return header.SchemaOffset + header.SchemaLength;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>

static int64_t repair_file_len(FILE* f)
{
    int64_t n;
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    n = (int64_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    return n;
}

static int64_t repair_data_start(const IsamFileHeader* h)
{
    if (h->SchemaOffset > 0 && h->SchemaLength > 0)
        return h->SchemaOffset + h->SchemaLength;
    return Config_HeaderSize;
}

static int repair_fix_header(const char* filePath)
{
    FILE* f;
    IsamFileHeader h;
    int64_t fileLength, dataStart, maxRecords;
    int i;
    uint8_t prefix[Config_FileHeaderFieldsPrefixSize];
    f = fopen(filePath, "r+b");
    if (!f) return GMB_DB_ERR;
    fileLength = repair_file_len(f);
    if (fileLength < Config_FileHeaderFieldsPrefixSize) { fclose(f); return GMB_DB_ERR; }
    if (fread(prefix, 1, sizeof(prefix), f) != sizeof(prefix)) { fclose(f); return GMB_DB_ERR; }
    memset(&h, 0, sizeof(h));
    memcpy(&h, prefix, sizeof(prefix));
    memcpy(h.Magic, Config_MagicBytes, 8);
    if (h.PrimaryKeyRootOffset != -1 && (h.PrimaryKeyRootOffset < 0 || h.PrimaryKeyRootOffset >= fileLength))
        h.PrimaryKeyRootOffset = -1;
    for (i = 0; i < 15; i++) {
        if (h.SecondaryIndexRootOffsets[i] != -1 &&
            (h.SecondaryIndexRootOffsets[i] < 0 || h.SecondaryIndexRootOffsets[i] >= fileLength))
            h.SecondaryIndexRootOffsets[i] = -1;
    }
    if (h.SchemaOffset <= 0 || h.SchemaOffset >= fileLength)
        h.SchemaOffset = Config_HeaderSize;
    if (h.SchemaLength <= 0 || h.SchemaOffset + h.SchemaLength > fileLength) {
        int64_t maxLen = fileLength - h.SchemaOffset;
        if (maxLen <= 0) { fclose(f); return GMB_DB_ERR; }
        h.SchemaLength = maxLen;
    }
    if (h.RecordSize > 0) {
        dataStart = repair_data_start(&h);
        maxRecords = fileLength > dataStart ? (fileLength - dataStart) / h.RecordSize : 0;
        if (maxRecords < 0) maxRecords = 0;
        if (h.LastRecNumber > maxRecords) h.LastRecNumber = maxRecords;
    }
    if (h.PageFreeListHeadOffset != -1 &&
        (h.PageFreeListHeadOffset < 0 || h.PageFreeListHeadOffset >= fileLength))
        h.PageFreeListHeadOffset = -1;
    if (h.FreeListHeadOffset != -1 &&
        (h.FreeListHeadOffset < 0 || h.FreeListHeadOffset >= fileLength))
        h.FreeListHeadOffset = -1;
    if (fileLength >= Config_HeaderSize) {
        if (IsamFileHeader_Write(f, &h) != GMB_DB_OK) { fclose(f); return GMB_DB_ERR; }
    } else {
        fseek(f, 0, SEEK_SET);
        if (fwrite(&h, 1, sizeof(prefix), f) != sizeof(prefix)) { fclose(f); return GMB_DB_ERR; }
    }
    fflush(f);
    fclose(f);
    return GMB_DB_OK;
}

int IsamRepair_PrepareForOpen(const char* filePath)
{
    if (!filePath || !filePath[0]) return GMB_DB_ARG;
    if (access(filePath, F_OK) != 0) return GMB_DB_ERR;
    WriteAheadLog_Recover(filePath);
    return repair_fix_header(filePath);
}

int IsamRepair_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_repair.src.isdb";
    IsamFieldSchema fields[2];
    IsamIndexDefinition sec;
    IsamSchema schema;
    Engine* e;
    IsamFieldValue vals[2];
    FILE* f;
    uint8_t x = 'X';
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    memset(&sec, 0, sizeof(sec));
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
    schema.SecondaryKeys = &sec;
    schema.SecondaryKeyCount = 0;
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -240;
    e = Engine_Open(path, &schema);
    if (!e) return -241;
    memset(vals, 0, sizeof(vals));
    vals[0].i64 = 1;
    vals[1].text = "N1";
    if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -242; }
    Engine_Dispose(e);
    f = fopen(path, "r+b");
    if (!f) return -243;
    if (fwrite(&x, 1, 1, f) != 1) { fclose(f); return -243; }
    fclose(f);
    e = Engine_Open(path, &schema);
    if (e) { Engine_Dispose(e); return -244; }
    if (IsamRepair_PrepareForOpen(path) != GMB_DB_OK) return -245;
    e = Engine_Open(path, &schema);
    if (!e) return -246;
    if (Engine_GetTotalRecordCount(e) != 1) { Engine_Dispose(e); return -247; }
    Engine_Dispose(e);
    return GMB_DB_OK;
}
