// FILE: Basic.Databases.Speed.DatabaseIterator.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Collections;

namespace Basic.Databases.Speed;

/// <summary>
/// Implementa un enumeratore che esegue un merge "al volo" (on-the-fly) dei dati
/// provenienti dalla Memtable e da una collezione di lettori SSTable per una data tabella.
/// Restituisce le coppie chiave-valore in ordine, gestendo i duplicati e le "tombstone".
/// </summary>
internal class DatabaseIterator : IEnumerable<KeyValuePair<byte[], byte[]?>>
{
    private readonly Memtable _memtable;
    private readonly List<SstReader> _sstReaders;
    private readonly byte[]? _startKey;
    private readonly byte[]? _endKey;

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="DatabaseIterator"/>.
    /// </summary>
    /// <param name="memtable">La Memtable attiva al momento della creazione dell'iteratore.</param>
    /// <param name="sstReaders">Una lista di lettori SSTable per tutti i file della tabella.</param>
    /// <param name="startKey">La chiave di inizio (inclusiva) della scansione. Se null, parte dall'inizio.</param>
    /// <param name="endKey">La chiave di fine (esclusiva) della scansione. Se null, arriva fino alla fine.</param>
    public DatabaseIterator(Memtable memtable, List<SstReader> sstReaders, byte[]? startKey = null, byte[]? endKey = null)
    {
        _memtable = memtable;
        _sstReaders = sstReaders;
        _startKey = startKey;
        _endKey = endKey;
    }

    /// <summary>
    /// Restituisce l'enumeratore che esegue la logica di merge.
    /// </summary>
    public IEnumerator<KeyValuePair<byte[], byte[]?>> GetEnumerator()
    {
        var comparer = new ByteArrayComparer();

        var memtableIterator = new SstIterator(new MemtableAsSstReaderAdapter(_memtable));
        var sstIterators = _sstReaders.Select(r => new SstIterator(r)).ToList();

        var allIterators = new List<SstIterator> { memtableIterator };
        allIterators.AddRange(sstIterators);

        if (_startKey != null)
        {
            foreach (var it in allIterators)
            {
                while (it.HasValue && comparer.Compare(it.Current.Key, _startKey) < 0)
                {
                    it.MoveNext();
                }
            }
        }

        var seenKeys = new HashSet<byte[]>(new ByteArrayEqualityComparer());
        var heapComparer = Comparer<(byte[] Key, int SourceIndex)>.Create((a, b) =>
        {
            var keyCmp = comparer.Compare(a.Key, b.Key);
            return keyCmp != 0 ? keyCmp : a.SourceIndex.CompareTo(b.SourceIndex);
        });
        var priorityQueue = new PriorityQueue<TaggedSstIterator, (byte[] Key, int SourceIndex)>(heapComparer);

        for (var i = 0; i < allIterators.Count; i++)
        {
            var iterator = allIterators[i];
            if (iterator.HasValue)
            {
                priorityQueue.Enqueue(new TaggedSstIterator(iterator, i), (iterator.Current.Key, i));
            }
        }

        while (priorityQueue.TryDequeue(out var tagged, out _))
        {
            var currentEntry = tagged.Iterator.Current;

            if (_endKey != null && comparer.Compare(currentEntry.Key, _endKey) >= 0)
            {
                yield break;
            }

            tagged.Iterator.MoveNext();
            if (tagged.Iterator.HasValue)
            {
                priorityQueue.Enqueue(tagged, (tagged.Iterator.Current.Key, tagged.SourceIndex));
            }

            if (seenKeys.Add(currentEntry.Key) && currentEntry.Value != null)
            {
                yield return currentEntry;
            }
        }
    }

    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

    private readonly record struct TaggedSstIterator(SstIterator Iterator, int SourceIndex);

    /// <summary>
    /// Classe adapter per trattare una Memtable come se fosse un SstReader,
    /// permettendo di usarla nel k-way merge.
    /// </summary>
    private class MemtableAsSstReaderAdapter : SstReader
    {
        private readonly Memtable _memtable;
        public MemtableAsSstReaderAdapter(Memtable memtable) : base()
        {
            _memtable = memtable;
        }

        public override IEnumerator<KeyValuePair<byte[], byte[]?>> GetEnumerator()
        {
            return _memtable.GetEnumerator();
        }

        public override void Dispose() { }
    }
}
