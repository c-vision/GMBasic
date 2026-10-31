// FILE: Basic.Databases.Speed.SsrWriter.cs
// // ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.Databases.Speed;

/// <summary>
/// Scrive dati ordinati in un file SSTable.
/// </summary>
internal static class SstWriter
{
    private const int BlockSizeThreshold = 4096;

    /// <summary>
    /// Scrive il contenuto di una Memtable in un nuovo file SSTable.
    /// </summary>
    public static void WriteFromMemtable(string filePath, Memtable memtable)
    {
        WriteFromEnumerable(filePath, memtable);
    }

    /// <summary>
    /// Scrive una sequenza ordinata di coppie chiave-valore in un file SSTable.
    /// Questo è il metodo principale usato dalla compattazione.
    /// </summary>
    public static void WriteFromEnumerable(string filePath, IEnumerable<KeyValuePair<byte[], byte[]?>> data)
    {
        using var stream = new FileStream(filePath, FileMode.Create, FileAccess.Write);
        using var writer = new BinaryWriter(stream);

        var bloom = new BloomFilter(EstimateKeyCount(data));
        var indexEntries = new List<(byte[] key, long offset)>();
        long currentBlockSize = 0;
        byte[]? firstKeyInBlock = null;
        var keyCount = 0;

        foreach (var entry in data)
        {
            keyCount++;
            bloom.Add(entry.Key);

            if (firstKeyInBlock == null)
            {
                firstKeyInBlock = entry.Key;
                indexEntries.Add((firstKeyInBlock, stream.Position));
            }

            writer.Write(entry.Key.Length);
            writer.Write(entry.Key);

            var valueExists = entry.Value != null;
            writer.Write(valueExists);

            if (valueExists)
            {
                writer.Write(entry.Value!.Length);
                writer.Write(entry.Value);
                currentBlockSize += entry.Key.Length + entry.Value.Length + 9;
            }
            else
            {
                currentBlockSize += entry.Key.Length + 5;
            }

            if (currentBlockSize >= BlockSizeThreshold)
            {
                currentBlockSize = 0;
                firstKeyInBlock = null;
            }
        }

        long bloomOffset = 0;
        if (keyCount > 0)
        {
            bloomOffset = stream.Position;
            bloom.WriteTo(writer);
        }

        var indexOffset = stream.Position;

        writer.Write(indexEntries.Count);
        foreach (var (key, offset) in indexEntries)
        {
            writer.Write(key.Length);
            writer.Write(key);
            writer.Write(offset);
        }

        SstFooter.WriteV2(writer, bloomOffset, indexOffset);
    }

    private static int EstimateKeyCount(IEnumerable<KeyValuePair<byte[], byte[]?>> data) =>
        data switch
        {
            Memtable memtable => memtable.KeyCount,
            ICollection<KeyValuePair<byte[], byte[]?>> collection => collection.Count,
            IReadOnlyCollection<KeyValuePair<byte[], byte[]?>> readOnly => readOnly.Count,
            _ => 4096,
        };
}
