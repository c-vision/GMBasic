// FILE: Basic.Databases.Speed.BloomFilter.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.Databases.Speed;

/// <summary>
/// Minimal per-SST bloom filter for negative lookups before block I/O.
/// </summary>
internal sealed class BloomFilter
{
    // ponytail: adaptive 8–14 bits/key by SST size — upgrade path: block-level sub-filters.
    internal const int DefaultBitsPerKey = 10;
    internal const int HashCount = 2;
    private const int MinBitCount = 64;

    /// <summary>Chooses bits/key from estimated key count (8 for tiny, 14 for large SSTs).</summary>
    internal static int BitsPerKeyForCount(int expectedKeyCount) =>
        expectedKeyCount switch
        {
            <= 64 => 8,
            <= 512 => 10,
            <= 4096 => 12,
            _ => 14,
        };

    private readonly byte[] _bits;
    private readonly int _bitCount;

    /// <summary>
    /// Creates a bloom filter sized for an estimated key count.
    /// Undersizing only raises false-positive rate; it never causes false negatives.
    /// </summary>
    /// <param name="expectedKeyCount">Estimated number of keys that will be added.</param>
    public BloomFilter(int expectedKeyCount = 1)
    {
        var bitsPerKey = BitsPerKeyForCount(Math.Max(1, expectedKeyCount));
        _bitCount = Math.Max(MinBitCount, Math.Max(1, expectedKeyCount) * bitsPerKey);
        _bits = new byte[(_bitCount + 7) / 8];
    }

    private BloomFilter(int bitCount, byte[] bits)
    {
        _bitCount = bitCount;
        _bits = bits;
    }

    /// <summary>
    /// Adds a key to the filter.
    /// </summary>
    /// <param name="key">The key bytes.</param>
    public void Add(ReadOnlySpan<byte> key)
    {
        var (h1, h2) = HashPair(key);
        for (var i = 0; i < HashCount; i++)
        {
            var bitIndex = (int)((h1 + (uint)i * h2) % (uint)_bitCount);
            SetBit(bitIndex);
        }
    }

    /// <summary>
    /// Returns whether the key might be present. False means definitely absent.
    /// </summary>
    /// <param name="key">The key bytes.</param>
    /// <returns><c>true</c> if the key might exist; <c>false</c> if it cannot exist.</returns>
    public bool MayContain(ReadOnlySpan<byte> key)
    {
        var (h1, h2) = HashPair(key);
        for (var i = 0; i < HashCount; i++)
        {
            var bitIndex = (int)((h1 + (uint)i * h2) % (uint)_bitCount);
            if (!GetBit(bitIndex))
                return false;
        }

        return true;
    }

    /// <summary>
    /// Serializes the filter to a binary writer.
    /// </summary>
    /// <param name="writer">The destination writer.</param>
    public void WriteTo(BinaryWriter writer)
    {
        writer.Write(_bitCount);
        writer.Write(_bits);
    }

    /// <summary>
    /// Deserializes a bloom filter from a binary reader.
    /// </summary>
    /// <param name="reader">The source reader.</param>
    /// <returns>The deserialized filter.</returns>
    public static BloomFilter ReadFrom(BinaryReader reader)
    {
        var bitCount = reader.ReadInt32();
        if (bitCount < MinBitCount)
            throw new InvalidDataException("invalid bloom bit count");

        var byteCount = (bitCount + 7) / 8;
        var bits = reader.ReadBytes(byteCount);
        if (bits.Length != byteCount)
            throw new EndOfStreamException("truncated bloom filter");

        return new BloomFilter(bitCount, bits);
    }

    private void SetBit(int bitIndex)
    {
        var byteIndex = bitIndex >> 3;
        _bits[byteIndex] |= (byte)(1 << (bitIndex & 7));
    }

    private bool GetBit(int bitIndex)
    {
        var byteIndex = bitIndex >> 3;
        return (_bits[byteIndex] & (1 << (bitIndex & 7))) != 0;
    }

    private static (uint h1, uint h2) HashPair(ReadOnlySpan<byte> key)
    {
        var hash = Hash64(key);
        return ((uint)hash, (uint)(hash >> 32));
    }

    private static ulong Hash64(ReadOnlySpan<byte> key)
    {
        const ulong offset = 14695981039346656037UL;
        const ulong prime = 1099511628211UL;
        var hash = offset;
        foreach (var b in key)
            hash = (hash ^ b) * prime;

        return hash;
    }
}
