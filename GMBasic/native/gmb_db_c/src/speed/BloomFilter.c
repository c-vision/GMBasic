/* FILE: native/gmb_db_c/src/SPEED/BloomFilter.c */
/* Port 1:1 di modules/Database/SPEED/BloomFilter.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.BloomFilter.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Minimal per-SST bloom filter for negative lookups before block I/O.
///// </summary>
//internal sealed class BloomFilter
//{
//    // ponytail: adaptive 8–14 bits/key by SST size — upgrade path: block-level sub-filters.
//    internal const int DefaultBitsPerKey = 10;
//    internal const int HashCount = 2;
//    private const int MinBitCount = 64;
//
//    /// <summary>Chooses bits/key from estimated key count (8 for tiny, 14 for large SSTs).</summary>
//    internal static int BitsPerKeyForCount(int expectedKeyCount) =>
//        expectedKeyCount switch
//        {
//            <= 64 => 8,
//            <= 512 => 10,
//            <= 4096 => 12,
//            _ => 14,
//        };
//
//    private readonly byte[] _bits;
//    private readonly int _bitCount;
//
//    /// <summary>
//    /// Creates a bloom filter sized for an estimated key count.
//    /// Undersizing only raises false-positive rate; it never causes false negatives.
//    /// </summary>
//    /// <param name="expectedKeyCount">Estimated number of keys that will be added.</param>
//    public BloomFilter(int expectedKeyCount = 1)
//    {
//        var bitsPerKey = BitsPerKeyForCount(Math.Max(1, expectedKeyCount));
//        _bitCount = Math.Max(MinBitCount, Math.Max(1, expectedKeyCount) * bitsPerKey);
//        _bits = new byte[(_bitCount + 7) / 8];
//    }
//
//    private BloomFilter(int bitCount, byte[] bits)
//    {
//        _bitCount = bitCount;
//        _bits = bits;
//    }
//
//    /// <summary>
//    /// Adds a key to the filter.
//    /// </summary>
//    /// <param name="key">The key bytes.</param>
//    public void Add(ReadOnlySpan<byte> key)
//    {
//        var (h1, h2) = HashPair(key);
//        for (var i = 0; i < HashCount; i++)
//        {
//            var bitIndex = (int)((h1 + (uint)i * h2) % (uint)_bitCount);
//            SetBit(bitIndex);
//        }
//    }
//
//    /// <summary>
//    /// Returns whether the key might be present. False means definitely absent.
//    /// </summary>
//    /// <param name="key">The key bytes.</param>
//    /// <returns><c>true</c> if the key might exist; <c>false</c> if it cannot exist.</returns>
//    public bool MayContain(ReadOnlySpan<byte> key)
//    {
//        var (h1, h2) = HashPair(key);
//        for (var i = 0; i < HashCount; i++)
//        {
//            var bitIndex = (int)((h1 + (uint)i * h2) % (uint)_bitCount);
//            if (!GetBit(bitIndex))
//                return false;
//        }
//
//        return true;
//    }
//
//    /// <summary>
//    /// Serializes the filter to a binary writer.
//    /// </summary>
//    /// <param name="writer">The destination writer.</param>
//    public void WriteTo(BinaryWriter writer)
//    {
//        writer.Write(_bitCount);
//        writer.Write(_bits);
//    }
//
//    /// <summary>
//    /// Deserializes a bloom filter from a binary reader.
//    /// </summary>
//    /// <param name="reader">The source reader.</param>
//    /// <returns>The deserialized filter.</returns>
//    public static BloomFilter ReadFrom(BinaryReader reader)
//    {
//        var bitCount = reader.ReadInt32();
//        if (bitCount < MinBitCount)
//            throw new InvalidDataException("invalid bloom bit count");
//
//        var byteCount = (bitCount + 7) / 8;
//        var bits = reader.ReadBytes(byteCount);
//        if (bits.Length != byteCount)
//            throw new EndOfStreamException("truncated bloom filter");
//
//        return new BloomFilter(bitCount, bits);
//    }
//
//    private void SetBit(int bitIndex)
//    {
//        var byteIndex = bitIndex >> 3;
//        _bits[byteIndex] |= (byte)(1 << (bitIndex & 7));
//    }
//
//    private bool GetBit(int bitIndex)
//    {
//        var byteIndex = bitIndex >> 3;
//        return (_bits[byteIndex] & (1 << (bitIndex & 7))) != 0;
//    }
//
//    private static (uint h1, uint h2) HashPair(ReadOnlySpan<byte> key)
//    {
//        var hash = Hash64(key);
//        return ((uint)hash, (uint)(hash >> 32));
//    }
//
//    private static ulong Hash64(ReadOnlySpan<byte> key)
//    {
//        const ulong offset = 14695981039346656037UL;
//        const ulong prime = 1099511628211UL;
//        var hash = offset;
//        foreach (var b in key)
//            hash = (hash ^ b) * prime;
//
//        return hash;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

struct BloomFilter {
    uint8_t* bits;
    int bitCount;
    int byteCount;
};

#define BloomFilter_MinBitCount 64

int BloomFilter_BitsPerKeyForCount(int expectedKeyCount)
{
    if (expectedKeyCount <= 64) return 8;
    if (expectedKeyCount <= 512) return 10;
    if (expectedKeyCount <= 4096) return 12;
    return 14;
}

static uint64_t BloomFilter_Hash64(const uint8_t* key, int keyLen)
{
    const uint64_t offset = 14695981039346656037ULL;
    const uint64_t prime = 1099511628211ULL;
    uint64_t hash = offset;
    int i;
    for (i = 0; i < keyLen; i++)
        hash = (hash ^ key[i]) * prime;
    return hash;
}

static void BloomFilter_SetBit(BloomFilter* self, int bitIndex)
{
    self->bits[bitIndex >> 3] |= (uint8_t)(1 << (bitIndex & 7));
}

static int BloomFilter_GetBit(const BloomFilter* self, int bitIndex)
{
    return (self->bits[bitIndex >> 3] & (1 << (bitIndex & 7))) != 0;
}

BloomFilter* BloomFilter_New(int expectedKeyCount)
{
    BloomFilter* self = (BloomFilter*)calloc(1, sizeof(*self));
    int bitsPerKey, n;
    if (expectedKeyCount < 1) expectedKeyCount = 1;
    bitsPerKey = BloomFilter_BitsPerKeyForCount(expectedKeyCount);
    n = expectedKeyCount * bitsPerKey;
    if (n < BloomFilter_MinBitCount) n = BloomFilter_MinBitCount;
    self->bitCount = n;
    self->byteCount = (n + 7) / 8;
    self->bits = (uint8_t*)calloc(1, (size_t)self->byteCount);
    return self;
}

void BloomFilter_Dispose(BloomFilter* self)
{
    if (!self) return;
    free(self->bits);
    free(self);
}

void BloomFilter_Add(BloomFilter* self, const uint8_t* key, int keyLen)
{
    uint64_t hash = BloomFilter_Hash64(key, keyLen);
    uint32_t h1 = (uint32_t)hash, h2 = (uint32_t)(hash >> 32);
    int i;
    for (i = 0; i < BloomFilter_HashCount; i++) {
        int bitIndex = (int)((h1 + (uint32_t)i * h2) % (uint32_t)self->bitCount);
        BloomFilter_SetBit(self, bitIndex);
    }
}

int BloomFilter_MayContain(BloomFilter* self, const uint8_t* key, int keyLen)
{
    uint64_t hash = BloomFilter_Hash64(key, keyLen);
    uint32_t h1 = (uint32_t)hash, h2 = (uint32_t)(hash >> 32);
    int i;
    for (i = 0; i < BloomFilter_HashCount; i++) {
        int bitIndex = (int)((h1 + (uint32_t)i * h2) % (uint32_t)self->bitCount);
        if (!BloomFilter_GetBit(self, bitIndex)) return 0;
    }
    return 1;
}

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

void BloomFilter_WriteTo(BloomFilter* self, FILE* writer)
{
    wr_i32_le(writer, self->bitCount);
    fwrite(self->bits, 1, (size_t)self->byteCount, writer);
}

BloomFilter* BloomFilter_ReadFrom(FILE* reader)
{
    int32_t bitCount;
    BloomFilter* self;
    if (rd_i32_le(reader, &bitCount) != 0 || bitCount < BloomFilter_MinBitCount) return NULL;
    self = (BloomFilter*)calloc(1, sizeof(*self));
    self->bitCount = bitCount;
    self->byteCount = (bitCount + 7) / 8;
    self->bits = (uint8_t*)malloc((size_t)self->byteCount);
    if (fread(self->bits, 1, (size_t)self->byteCount, reader) != (size_t)self->byteCount) {
        BloomFilter_Dispose(self);
        return NULL;
    }
    return self;
}
