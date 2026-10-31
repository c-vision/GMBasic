/* FILE: native/gmb_db_c/src/SPEED/SstFooter.c */
/* Port 1:1 di modules/Database/SPEED/SstFooter.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.SstFooter.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Text;
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// SST footer layout helpers. V0 ends with indexOffset (8 bytes). V2 adds magic + bloomOffset.
///// </summary>
//internal static class SstFooter
//{
//    // "SPTF" + version 2
//    internal const long MagicV2 = 0x53505446_00000002L;
//    internal const int V2FooterSize = sizeof(long) * 3;
//
//    /// <summary>
//    /// Reads footer metadata from an SST file stream.
//    /// </summary>
//    /// <param name="stream">The SST file stream.</param>
//    /// <param name="indexOffset">The offset of the block index section.</param>
//    /// <param name="bloomOffset">The offset of the bloom section, or 0 when absent.</param>
//    /// <param name="hasV2Footer">Whether the file uses the V2 footer trailer.</param>
//    internal static void Read(Stream stream, out long indexOffset, out long bloomOffset, out bool hasV2Footer)
//    {
//        bloomOffset = 0;
//        hasV2Footer = false;
//        indexOffset = 0;
//
//        if (stream.Length < sizeof(long))
//            throw new InvalidDataException("SST file too small");
//
//        if (stream.Length >= V2FooterSize)
//        {
//            stream.Seek(-V2FooterSize, SeekOrigin.End);
//            using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
//            var magic = reader.ReadInt64();
//            if (magic == MagicV2)
//            {
//                bloomOffset = reader.ReadInt64();
//                indexOffset = reader.ReadInt64();
//                hasV2Footer = true;
//                return;
//            }
//        }
//
//        stream.Seek(-sizeof(long), SeekOrigin.End);
//        using (var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true))
//            indexOffset = reader.ReadInt64();
//    }
//
//    /// <summary>
//    /// Writes the V2 footer trailer.
//    /// </summary>
//    /// <param name="writer">The destination writer.</param>
//    /// <param name="bloomOffset">Bloom section offset, or 0 when absent.</param>
//    /// <param name="indexOffset">Block index section offset.</param>
//    internal static void WriteV2(BinaryWriter writer, long bloomOffset, long indexOffset)
//    {
//        writer.Write(MagicV2);
//        writer.Write(bloomOffset);
//        writer.Write(indexOffset);
//    }
//
//    /// <summary>
//    /// Validates footer offsets against file length.
//    /// </summary>
//    /// <param name="streamLength">Total SST file length.</param>
//    /// <param name="indexOffset">Block index offset.</param>
//    /// <param name="bloomOffset">Bloom section offset.</param>
//    /// <param name="hasV2Footer">Whether the V2 footer was detected.</param>
//    /// <param name="error">Validation error message.</param>
//    /// <returns><c>true</c> when offsets are consistent.</returns>
//    internal static bool TryValidateOffsets(long streamLength, long indexOffset, long bloomOffset, bool hasV2Footer,
//        out string error)
//    {
//        error = string.Empty;
//        var footerSize = hasV2Footer ? V2FooterSize : sizeof(long);
//
//        if (indexOffset < 0 || indexOffset >= streamLength - footerSize + sizeof(long))
//        {
//            error = "invalid index offset";
//            return false;
//        }
//
//        if (bloomOffset < 0 || bloomOffset >= indexOffset)
//        {
//            error = "invalid bloom offset";
//            return false;
//        }
//
//        return true;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

static int __attribute__((unused)) wr_i32_le(FILE* f, int32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}
static int __attribute__((unused)) rd_i32_le(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}
static int __attribute__((unused)) wr_i64_le(FILE* f, int64_t v)
{
    uint8_t b[8]; int i;
    for (i = 0; i < 8; i++) b[i] = (uint8_t)((uint64_t)v >> (8 * i));
    return fwrite(b, 1, 8, f) == 8 ? 0 : -1;
}
static int __attribute__((unused)) rd_i64_le(FILE* f, int64_t* v)
{
    uint8_t b[8]; uint64_t u = 0; int i;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)b[i] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

int SstFooter_Read(FILE* stream, int64_t* indexOffset, int64_t* bloomOffset, int* hasV2Footer)
{
    long len;
    *bloomOffset = 0;
    *hasV2Footer = 0;
    *indexOffset = 0;
    fseek(stream, 0, SEEK_END);
    len = ftell(stream);
    if (len < (long)sizeof(int64_t)) return GMB_DB_ERR;
    if (len >= SstFooter_V2FooterSize) {
        int64_t magic;
        fseek(stream, -SstFooter_V2FooterSize, SEEK_END);
        if (rd_i64_le(stream, &magic) == 0 && magic == SstFooter_MagicV2) {
            rd_i64_le(stream, bloomOffset);
            rd_i64_le(stream, indexOffset);
            *hasV2Footer = 1;
            return GMB_DB_OK;
        }
    }
    fseek(stream, -(long)sizeof(int64_t), SEEK_END);
    rd_i64_le(stream, indexOffset);
    return GMB_DB_OK;
}

void SstFooter_WriteV2(FILE* writer, int64_t bloomOffset, int64_t indexOffset)
{
    wr_i64_le(writer, SstFooter_MagicV2);
    wr_i64_le(writer, bloomOffset);
    wr_i64_le(writer, indexOffset);
}

int SstFooter_TryValidateOffsets(int64_t streamLength, int64_t indexOffset, int64_t bloomOffset,
                                 int hasV2Footer, char* error, int errorCap)
{
    int footerSize = hasV2Footer ? SstFooter_V2FooterSize : (int)sizeof(int64_t);
    if (error && errorCap) error[0] = 0;
    if (indexOffset < 0 || indexOffset >= streamLength - footerSize + (int64_t)sizeof(int64_t)) {
        if (error) snprintf(error, (size_t)errorCap, "invalid index offset");
        return 0;
    }
    if (bloomOffset < 0 || bloomOffset >= indexOffset) {
        if (error) snprintf(error, (size_t)errorCap, "invalid bloom offset");
        return 0;
    }
    return 1;
}
