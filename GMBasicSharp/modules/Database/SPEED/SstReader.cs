// FILE: Basic.Databases.Speed.SstReader.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Collections;
using System.Text;

namespace Basic.Databases.Speed;

/// <summary>
/// Legge e interroga un singolo file SSTable, implementando l'interfaccia IEnumerable.
/// </summary>
internal class SstReader : IDisposable, IEnumerable<KeyValuePair<byte[], byte[]?>>
{
    private readonly FileStream? _stream;
    private readonly SortedList<byte[], long> _blockIndex;
    private readonly long _indexOffset;
    private readonly long _bloomOffset;
    private readonly BloomFilter? _bloomFilter;

    /// <summary>
    /// Ottiene il percorso del file SSTable gestito da questo reader.
    /// </summary>
    public string FilePath { get; }

    /// <summary>
    /// Whether this SST carries a bloom filter section.
    /// </summary>
    internal bool HasBloomFilter => _bloomFilter is not null;

    private long DataEndOffset => _bloomOffset > 0 ? _bloomOffset : _indexOffset;

    /// <summary>
    /// Inizializza un nuovo SstReader da un file fisico.
    /// </summary>
    /// <param name="filePath">Il percorso del file SSTable.</param>
    public SstReader(string filePath)
    {
        FilePath = filePath;
        _stream = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read);
        _blockIndex = new SortedList<byte[], long>(new ByteArrayComparer());

        SstFooter.Read(_stream, out _indexOffset, out _bloomOffset, out _);

        if (_bloomOffset > 0)
        {
            _stream.Seek(_bloomOffset, SeekOrigin.Begin);
            using var bloomReader = new BinaryReader(_stream, Encoding.UTF8, leaveOpen: true);
            _bloomFilter = BloomFilter.ReadFrom(bloomReader);
        }

        _stream.Seek(_indexOffset, SeekOrigin.Begin);
        using var tempReader = new BinaryReader(_stream, Encoding.UTF8, true);
        var indexEntryCount = tempReader.ReadInt32();
        for (var i = 0; i < indexEntryCount; i++)
        {
            var keyLength = tempReader.ReadInt32();
            var key = tempReader.ReadBytes(keyLength);
            var offset = tempReader.ReadInt64();
            _blockIndex.Add(key, offset);
        }
    }

    /// <summary>
    /// Costruttore protetto per classi derivate che non necessitano di un file fisico, come l'adapter della Memtable.
    /// Inizializza il reader in uno stato vuoto senza eseguire I/O su disco.
    /// </summary>
    protected SstReader()
    {
        FilePath = string.Empty;
        _stream = null;
        _blockIndex = new SortedList<byte[], long>(new ByteArrayComparer());
        _indexOffset = 0;
        _bloomOffset = 0;
        _bloomFilter = null;
    }

    /// <summary>
    /// Validates SST footer/index structure without scanning data blocks — O(footer) only.
    /// </summary>
    /// <param name="filePath">Path to SST file.</param>
    /// <param name="error">Error message when validation fails.</param>
    /// <returns>True when footer and block index are readable.</returns>
    public static bool TryValidateFooter(string filePath, out string error)
    {
        error = string.Empty;
        try
        {
            using var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read);
            if (stream.Length < sizeof(long))
            {
                error = "file too small";
                return false;
            }

            SstFooter.Read(stream, out var indexOffset, out var bloomOffset, out var hasV2Footer);
            if (!SstFooter.TryValidateOffsets(stream.Length, indexOffset, bloomOffset, hasV2Footer, out error))
                return false;

            if (bloomOffset > 0)
            {
                stream.Seek(bloomOffset, SeekOrigin.Begin);
                using var bloomReader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
                _ = BloomFilter.ReadFrom(bloomReader);
            }

            stream.Seek(indexOffset, SeekOrigin.Begin);
            using var reader = new BinaryReader(stream, Encoding.UTF8, true);
            var indexEntryCount = reader.ReadInt32();
            if (indexEntryCount < 0)
            {
                error = "invalid index entry count";
                return false;
            }

            for (var i = 0; i < indexEntryCount; i++)
            {
                var keyLength = reader.ReadInt32();
                if (keyLength < 0 || stream.Position + keyLength + sizeof(long) > stream.Length)
                {
                    error = $"invalid index entry {i}";
                    return false;
                }

                stream.Seek(keyLength, SeekOrigin.Current);
                stream.Seek(sizeof(long), SeekOrigin.Current);
            }

            return true;
        }
        catch (Exception ex)
        {
            error = ex.Message;
            return false;
        }
    }

    /// <summary>
    /// Tenta di trovare una chiave all'interno del file SSTable utilizzando l'indice a blocchi.
    /// </summary>
    /// <param name="key">La chiave da cercare.</param>
    /// <param name="value">Il valore trovato, o null se non esiste o è una tombstone.</param>
    /// <returns>True se la chiave è stata trovata (anche se è una tombstone), altrimenti False.</returns>
    public bool TryFind(byte[] key, out byte[]? value)
    {
        value = null;
        if (_stream is null) return false;

        if (_bloomFilter is not null && !_bloomFilter.MayContain(key))
            return false;

        var blockIndex = FindBlockIndexForKey(key);
        if (blockIndex < 0) return false;

        var blockStartOffset = _blockIndex.Values[blockIndex];
        var blockEndOffset = (blockIndex + 1 < _blockIndex.Count)
            ? _blockIndex.Values[blockIndex + 1]
            : DataEndOffset;

        _stream.Seek(blockStartOffset, SeekOrigin.Begin);
        var reader = new BinaryReader(_stream, Encoding.UTF8, true);
        var comparer = new ByteArrayComparer();

        while (_stream.Position < blockEndOffset)
        {
            var currentKeyLength = reader.ReadInt32();
            var currentKey = reader.ReadBytes(currentKeyLength);
            var comparison = comparer.Compare(currentKey, key);

            if (comparison == 0)
            {
                var valueExists = reader.ReadBoolean();
                if (valueExists)
                {
                    var valueLength = reader.ReadInt32();
                    value = reader.ReadBytes(valueLength);
                }

                return true;
            }

            if (comparison > 0) return false;

            var valueExistsToSkip = reader.ReadBoolean();
            if (valueExistsToSkip)
            {
                var valueLengthToSkip = reader.ReadInt32();
                _stream.Seek(valueLengthToSkip, SeekOrigin.Current);
            }
        }

        return false;
    }

    /// <summary>
    /// Esegue una ricerca binaria sull'indice in memoria per trovare il blocco corretto che potrebbe contenere la chiave.
    /// </summary>
    /// <param name="key">La chiave da cercare.</param>
    /// <returns>L'indice del blocco corretto, o -1 se la chiave è più piccola di qualsiasi chiave indicizzata.</returns>
    private int FindBlockIndexForKey(byte[] key)
    {
        var keys = _blockIndex.Keys;
        var comparer = new ByteArrayComparer();
        int low = 0, high = keys.Count - 1;
        var result = -1;

        while (low <= high)
        {
            var mid = low + (high - low) / 2;
            if (comparer.Compare(keys[mid], key) <= 0)
            {
                result = mid;
                low = mid + 1;
            }
            else
            {
                high = mid - 1;
            }
        }

        return result;
    }

    /// <summary>
    /// Restituisce un enumeratore che itera sequenzialmente su tutte le coppie chiave-valore nel file SSTable.
    /// </summary>
    public virtual IEnumerator<KeyValuePair<byte[], byte[]?>> GetEnumerator()
    {
        if (_stream is null) yield break;

        _stream.Seek(0, SeekOrigin.Begin);
        var reader = new BinaryReader(_stream, Encoding.UTF8, true);
        var dataEnd = DataEndOffset;

        while (_stream.Position < dataEnd)
        {
            var keyLength = reader.ReadInt32();
            var key = reader.ReadBytes(keyLength);
            var valueExists = reader.ReadBoolean();
            byte[]? value = null;
            if (valueExists)
            {
                var valueLength = reader.ReadInt32();
                value = reader.ReadBytes(valueLength);
            }

            yield return new KeyValuePair<byte[], byte[]?>(key, value);
        }
    }

    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

    /// <summary>
    /// Rilascia le risorse gestite, in particolare il FileStream.
    /// </summary>
    public virtual void Dispose()
    {
        _stream?.Dispose();
    }
}
