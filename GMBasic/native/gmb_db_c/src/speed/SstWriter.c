/* FILE: native/gmb_db_c/src/SPEED/SstWriter.c */
/* Port 1:1 di modules/Database/SPEED/SstWriter.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.SsrWriter.cs
//// // ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Scrive dati ordinati in un file SSTable.
///// </summary>
//internal static class SstWriter
//{
//    private const int BlockSizeThreshold = 4096;
//
//    /// <summary>
//    /// Scrive il contenuto di una Memtable in un nuovo file SSTable.
//    /// </summary>
//    public static void WriteFromMemtable(string filePath, Memtable memtable)
//    {
//        WriteFromEnumerable(filePath, memtable);
//    }
//
//    /// <summary>
//    /// Scrive una sequenza ordinata di coppie chiave-valore in un file SSTable.
//    /// Questo è il metodo principale usato dalla compattazione.
//    /// </summary>
//    public static void WriteFromEnumerable(string filePath, IEnumerable<KeyValuePair<byte[], byte[]?>> data)
//    {
//        using var stream = new FileStream(filePath, FileMode.Create, FileAccess.Write);
//        using var writer = new BinaryWriter(stream);
//
//        var bloom = new BloomFilter(EstimateKeyCount(data));
//        var indexEntries = new List<(byte[] key, long offset)>();
//        long currentBlockSize = 0;
//        byte[]? firstKeyInBlock = null;
//        var keyCount = 0;
//
//        foreach (var entry in data)
//        {
//            keyCount++;
//            bloom.Add(entry.Key);
//
//            if (firstKeyInBlock == null)
//            {
//                firstKeyInBlock = entry.Key;
//                indexEntries.Add((firstKeyInBlock, stream.Position));
//            }
//
//            writer.Write(entry.Key.Length);
//            writer.Write(entry.Key);
//
//            var valueExists = entry.Value != null;
//            writer.Write(valueExists);
//
//            if (valueExists)
//            {
//                writer.Write(entry.Value!.Length);
//                writer.Write(entry.Value);
//                currentBlockSize += entry.Key.Length + entry.Value.Length + 9;
//            }
//            else
//            {
//                currentBlockSize += entry.Key.Length + 5;
//            }
//
//            if (currentBlockSize >= BlockSizeThreshold)
//            {
//                currentBlockSize = 0;
//                firstKeyInBlock = null;
//            }
//        }
//
//        long bloomOffset = 0;
//        if (keyCount > 0)
//        {
//            bloomOffset = stream.Position;
//            bloom.WriteTo(writer);
//        }
//
//        var indexOffset = stream.Position;
//
//        writer.Write(indexEntries.Count);
//        foreach (var (key, offset) in indexEntries)
//        {
//            writer.Write(key.Length);
//            writer.Write(key);
//            writer.Write(offset);
//        }
//
//        SstFooter.WriteV2(writer, bloomOffset, indexOffset);
//    }
//
//    private static int EstimateKeyCount(IEnumerable<KeyValuePair<byte[], byte[]?>> data) =>
//        data switch
//        {
//            Memtable memtable => memtable.KeyCount,
//            ICollection<KeyValuePair<byte[], byte[]?>> collection => collection.Count,
//            IReadOnlyCollection<KeyValuePair<byte[], byte[]?>> readOnly => readOnly.Count,
//            _ => 4096,
//        };
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

#define SstWriter_BlockSizeThreshold 4096

int SstWriter_WriteFromMemtable(const char* filePath, Memtable* memtable)
{
    FILE* stream;
    BloomFilter* bloom;
    int keyCount = Memtable_get_KeyCount(memtable);
    int i, indexN = 0;
    int64_t currentBlockSize = 0, bloomOffset = 0, indexOffset;
    uint8_t** indexKeys = NULL;
    int* indexKeyLens = NULL;
    int64_t* indexOffs = NULL;
    int firstInBlock = 1;

    stream = fopen(filePath, "wb");
    if (!stream) return GMB_DB_ERR;
    bloom = BloomFilter_New(keyCount > 0 ? keyCount : 1);

    for (i = 0; i < keyCount; i++) {
        uint8_t *key, *value;
        int keyLen, valueLen, tomb;
        uint8_t exists;
        Memtable_Nth(memtable, i, &key, &keyLen, &value, &valueLen, &tomb);
        BloomFilter_Add(bloom, key, keyLen);
        if (firstInBlock) {
            indexKeys = (uint8_t**)realloc(indexKeys, (size_t)(indexN + 1) * sizeof(uint8_t*));
            indexKeyLens = (int*)realloc(indexKeyLens, (size_t)(indexN + 1) * sizeof(int));
            indexOffs = (int64_t*)realloc(indexOffs, (size_t)(indexN + 1) * sizeof(int64_t));
            indexKeys[indexN] = key;
            indexKeyLens[indexN] = keyLen;
            indexOffs[indexN] = (int64_t)ftell(stream);
            indexN++;
            firstInBlock = 0;
        }
        wr_i32_le(stream, keyLen);
        fwrite(key, 1, (size_t)keyLen, stream);
        exists = tomb ? 0 : 1;
        fwrite(&exists, 1, 1, stream);
        if (exists) {
            wr_i32_le(stream, valueLen);
            fwrite(value, 1, (size_t)valueLen, stream);
            currentBlockSize += keyLen + valueLen + 9;
        } else {
            currentBlockSize += keyLen + 5;
        }
        if (currentBlockSize >= SstWriter_BlockSizeThreshold) {
            currentBlockSize = 0;
            firstInBlock = 1;
        }
    }
    if (keyCount > 0) {
        bloomOffset = ftell(stream);
        BloomFilter_WriteTo(bloom, stream);
    }
    indexOffset = ftell(stream);
    wr_i32_le(stream, indexN);
    for (i = 0; i < indexN; i++) {
        wr_i32_le(stream, indexKeyLens[i]);
        fwrite(indexKeys[i], 1, (size_t)indexKeyLens[i], stream);
        wr_i64_le(stream, indexOffs[i]);
    }
    SstFooter_WriteV2(stream, bloomOffset, indexOffset);
    BloomFilter_Dispose(bloom);
    free(indexKeys); free(indexKeyLens); free(indexOffs);
    fclose(stream);
    return GMB_DB_OK;
}
