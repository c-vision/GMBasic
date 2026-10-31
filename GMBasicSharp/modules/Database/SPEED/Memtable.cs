// FILE: Basic.Databases.Speed.Memtable.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Collections;

namespace Basic.Databases.Speed;

/// <summary>
/// Comparer per array di byte, essenziale per le strutture dati ordinate come SortedList.
/// </summary>
internal sealed class ByteArrayComparer : IComparer<byte[]>
{
    public int Compare(byte[]? x, byte[]? y)
    {
        if (ReferenceEquals(x, y)) return 0;
        if (x is null) return -1;
        if (y is null) return 1;

        var minLength = Math.Min(x.Length, y.Length);
        for (var i = 0; i < minLength; i++)
        {
            var cmp = x[i].CompareTo(y[i]);
            if (cmp != 0) return cmp;
        }

        return x.Length.CompareTo(y.Length);
    }
}

/// <summary>
/// Rappresenta la tabella in memoria (Memtable) in un'architettura LSM-Tree.
/// Accetta scritture e le mantiene ordinate in memoria prima che vengano scritte su disco (flush).
/// </summary>
internal class Memtable : IEnumerable<KeyValuePair<byte[], byte[]?>>
{
    /// <summary>
    /// Struttura dati sottostante che memorizza le coppie chiave-valore in ordine.
    /// Un valore nullo (tombstone) indica una cancellazione.
    /// </summary>
    /// <remarks>SortedDictionary (albero rosso-nero, insert O(log n)), NON SortedList: quest'ultima
    /// è un array ordinato con insert O(n) → caricamento bulk O(n^2) (bug SPEED-Memtable-O(n)).</remarks>
    private readonly SortedDictionary<byte[], byte[]?> _table;

    /// <summary>
    /// Dimensione stimata in byte dei dati contenuti nella Memtable.
    /// </summary>
    public long EstimatedSize { get; private set; }

    /// <summary>
    /// Numero di chiavi presenti nella Memtable (incluse tombstone).
    /// </summary>
    internal int KeyCount => _table.Count;

    public Memtable()
    {
        _table = new SortedDictionary<byte[], byte[]?>(new ByteArrayComparer());
        EstimatedSize = 0;
    }

    /// <summary>
    /// Inserisce o aggiorna una coppia chiave-valore.
    /// </summary>
    /// <param name="key">La chiave del dato.</param>
    /// <param name="value">Il valore del dato.</param>
    public void Set(byte[] key, byte[] value)
    {
        ArgumentNullException.ThrowIfNull(key);
        ArgumentNullException.ThrowIfNull(value);

        if (_table.TryGetValue(key, out var oldValue))
        {
            EstimatedSize -= key.Length + (oldValue?.Length ?? 0);
        }

        _table[key] = value;
        EstimatedSize += key.Length + value.Length;
    }

    /// <summary>
    /// Marca una chiave come cancellata inserendo una "tombstone" (valore nullo).
    /// </summary>
    /// <param name="key">La chiave da cancellare.</param>
    public void Delete(byte[] key)
    {
        ArgumentNullException.ThrowIfNull(key);
        
        if (_table.TryGetValue(key, out var oldValue))
        {
            EstimatedSize -= key.Length + (oldValue?.Length ?? 0);
        }

        _table[key] = null; // Tombstone
        EstimatedSize += key.Length;
    }

    /// <summary>
    /// Recupera un valore associato a una chiave.
    /// </summary>
    /// <param name="key">La chiave da cercare.</param>
    /// <param name="value">Il valore trovato, o null se la chiave non esiste o è stata cancellata.</param>
    /// <returns>True se la chiave esiste nella Memtable (anche se cancellata), altrimenti False.</returns>
    public bool Get(byte[] key, out byte[]? value)
    {
        return _table.TryGetValue(key, out value);
    }

    /// <summary>
    /// Restituisce un enumeratore per iterare sulla collezione ordinata.
    /// </summary>
    public IEnumerator<KeyValuePair<byte[], byte[]?>> GetEnumerator()
    {
        return _table.GetEnumerator();
    }

    IEnumerator IEnumerable.GetEnumerator()
    {
        return GetEnumerator();
    }
}