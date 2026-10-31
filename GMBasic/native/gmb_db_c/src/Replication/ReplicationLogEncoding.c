/* FILE: native/gmb_db_c/src/Replication/ReplicationLogEncoding.c */
/* Port 1:1 di modules/Database/Replication/ReplicationLogEncoding.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Replication.ReplicationLogEncoding
//
//using System.Text;
//
//namespace Basic.Databases.Replication;
//
///// <summary>UTF-8 helpers for replication sidecar logs (byte checkpoints must match on-disk bytes).</summary>
//internal static class ReplicationLogEncoding
//{
//    internal static readonly UTF8Encoding Utf8NoBom = new(encoderShouldEmitUTF8Identifier: false);
//
//    public static bool FileHasUtf8Bom(string path)
//    {
//        if (!File.Exists(path))
//        {
//            return false;
//        }
//
//        Span<byte> header = stackalloc byte[3];
//        using var stream = File.OpenRead(path);
//        return stream.Read(header) == 3 &&
//               header[0] == 0xEF &&
//               header[1] == 0xBB &&
//               header[2] == 0xBF;
//    }
//
//    /// <summary>Maps logical checkpoint offsets (line-byte sums) to file offsets when a legacy BOM is present.</summary>
//    public static long AdjustFileOffset(string path, long logicalOffset) =>
//        logicalOffset > 0 && FileHasUtf8Bom(path) ? logicalOffset + 3 : logicalOffset;
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: internal static class ReplicationLogEncoding */
/* UTF-8 helpers for replication sidecar logs (byte checkpoints must match on-disk bytes). */

int ReplicationLogEncoding_FileHasUtf8Bom(const char* path)
{
    unsigned char header[3];
    FILE* f;
    size_t n;
    if (!path) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(header, 1, 3, f);
    fclose(f);
    return n == 3 && header[0] == 0xEF && header[1] == 0xBB && header[2] == 0xBF;
}

/* Maps logical checkpoint offsets (line-byte sums) to file offsets when a legacy BOM is present. */
int64_t ReplicationLogEncoding_AdjustFileOffset(const char* path, int64_t logicalOffset)
{
    return (logicalOffset > 0 && ReplicationLogEncoding_FileHasUtf8Bom(path))
        ? logicalOffset + 3 : logicalOffset;
}

