// FILE: Basic.Databases.Speed.Compactor.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Collections.Concurrent;

namespace Basic.Databases.Speed;

/// <summary>
/// Helper class per la compattazione che wrappa un SstReader per facilitare l'iterazione nel merge.
/// </summary>
internal class SstIterator
{
    private readonly IEnumerator<KeyValuePair<byte[], byte[]?>> _enumerator;
    public bool HasValue { get; private set; }

    public SstIterator(SstReader reader)
    {
        _enumerator = reader.GetEnumerator();
        HasValue = _enumerator.MoveNext();
    }

    public KeyValuePair<byte[], byte[]?> Current => _enumerator.Current;

    public void MoveNext()
    {
        HasValue = _enumerator.MoveNext();
    }
}

/// <summary>
/// Fornisce la logica per la compattazione dei file SSTable.
/// La compattazione è un processo in background essenziale per un LSM-Tree, che unisce
/// più file piccoli in file più grandi, rimuovendo dati duplicati e cancellati.
/// </summary>
internal static class Compactor
{
    /// <summary>
    /// La soglia (numero di file) per i livelli L1 e superiori che innesca una compattazione.
    /// Tipicamente più alta di quella di L0.
    /// </summary>
    private const int CompactLevelThreshold = 10;

    /// <summary>
    /// Esegue la compattazione dei file di Livello 0 per una data tabella.
    /// Unisce tutti i file L0 in un unico nuovo file a un livello superiore, rimuovendo
    /// dati obsoleti e cancellati. Viene attivata quando il numero di file in L0 supera la soglia.
    /// </summary>
    /// <param name="databasePath">Il percorso radice del database.</param>
    /// <param name="manifest">L'istanza del manifest da aggiornare atomicamente.</param>
    /// <param name="tableId">L'ID della tabella da compattare.</param>
    /// <param name="readerCache">La cache dei lettori SSTable da cui rimuovere i vecchi file.</param>
    /// <returns>Un <see cref="Task"/> che rappresenta l'operazione di compattazione asincrona.</returns>
    public static async Task CompactLevel0Async(Database db, int tableId,
        ConcurrentDictionary<string, SstReader> readerCache)
    {
        // 1. Identifica i file da compattare
        if (!db.Manifest.TryGetTable(tableId, out var table) || table is null) return;

        var level0Files = table.GetFilesForLevel(0).ToList();
        if (level0Files.Count < Database.CompactLevel0Threshold) return;

        // Ordina i file dal più recente al più vecchio per gestire correttamente i duplicati
        level0Files.Sort();
        level0Files.Reverse();

        // 2. Prepara i lettori e l'iteratore per il k-way merge
        var readers = level0Files.Select(f =>
            readerCache.GetOrAdd(Path.Combine(db.DatabasePath, Config.SstDirectoryName, f),
                path => new SstReader(path))).ToList();
        var iterators = readers.Select(r => new SstIterator(r)).ToList();

        var mergedResults = KWayMerge(iterators);

        // 3. Scrivi il risultato compattato su un nuovo file SSTable al Livello 1
        var newFileNumber = db.Manifest.GetNextFileNumber();
        var newSstPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, $"{newFileNumber:D6}_{tableId}.sst");

        await Task.Run(() => SstWriter.WriteFromEnumerable(newSstPath, mergedResults));

        // 4. Aggiorna atomicamente il Manifest
        db.Manifest.UpdateAfterCompaction(tableId, 0, level0Files, 1, Path.GetFileName(newSstPath));
        db.Manifest.Save();

        // 5. Pulisci i vecchi file L0 e rimuovili dalla cache dei reader
        foreach (var oldFile in level0Files)
        {
            var oldPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, oldFile);
            if (readerCache.TryRemove(oldPath, out var readerToDispose))
            {
                readerToDispose.Dispose();
            }

            File.Delete(oldPath);
        }
    }

    /// <summary>
    /// Esegue la compattazione di tutti i file di un livello N (con N > 0) in un nuovo singolo file
    /// nel livello N+1. Questa operazione viene innescata quando il numero di file in un livello
    /// supera la soglia definita, prevenendo il degrado delle performance di lettura.
    /// </summary>
    /// <param name="databasePath">Il percorso radice del database.</param>
    /// <param name="manifest">L'istanza del manifest da aggiornare atomicamente.</param>
    /// <param name="tableId">L'ID della tabella a cui appartengono i file.</param>
    /// <param name="levelToCompact">Il livello (maggiore di 0) da cui compattare i file.</param>
    /// <param name="readerCache">La cache dei lettori SSTable da cui rimuovere i vecchi file.</param>
    /// <returns>Un <see cref="Task"/> che rappresenta l'operazione di compattazione asincrona.</returns>
    /// <summary>
    /// Esegue la compattazione di tutti i file di un livello N (con N > 0) in un nuovo singolo file
    /// nel livello N+1. Questa operazione viene innescata quando il numero di file in un livello
    /// supera la soglia definita, prevenendo il degrado delle performance di lettura.
    /// </summary>
    /// <param name="db">L'istanza del database principale, usata per accedere al percorso e al manifest.</param>
    /// <param name="tableId">L'ID della tabella a cui appartengono i file.</param>
    /// <param name="levelToCompact">Il livello (maggiore di 0) da cui compattare i file.</param>
    /// <param name="readerCache">La cache dei lettori SSTable da cui rimuovere i vecchi file.</param>
    /// <returns>Un <see cref="Task"/> che rappresenta l'operazione di compattazione asincrona.</returns>
    public static async Task CompactLevelAsync(Database db, int tableId, int levelToCompact,
        ConcurrentDictionary<string, SstReader> readerCache)
    {
        if (levelToCompact <= 0) return;

        if (!db.Manifest.TryGetTable(tableId, out var table) || table is null) return;

        var filesToCompact = table.GetFilesForLevel(levelToCompact).ToList();
        if (filesToCompact.Count < CompactLevelThreshold) return;

        var readers = filesToCompact.Select(f =>
            readerCache.GetOrAdd(Path.Combine(db.DatabasePath, Config.SstDirectoryName, f),
                path => new SstReader(path))).ToList();
        var iterators = readers.Select(r => new SstIterator(r)).ToList();

        var mergedResults = KWayMerge(iterators);

        var newFileNumber = db.Manifest.GetNextFileNumber();
        var newSstPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, $"{newFileNumber:D6}_{tableId}.sst");

        await Task.Run(() => SstWriter.WriteFromEnumerable(newSstPath, mergedResults));

        db.Manifest.UpdateAfterCompaction(tableId, levelToCompact, filesToCompact, levelToCompact + 1,
            Path.GetFileName(newSstPath));
        db.Manifest.Save();

        foreach (var oldFile in filesToCompact)
        {
            var oldPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, oldFile);
            if (readerCache.TryRemove(oldPath, out var readerToDispose))
            {
                readerToDispose.Dispose();
            }

            File.Delete(oldPath);
        }
    }

    /// <summary>
    /// Offline/ops compaction: merges all L0 and L1 SST files into one L1 file.
    /// ponytail: levels 2+ untouched — upgrade path: full LSM major compact across all levels.
    /// </summary>
    /// <returns>Net SST files removed (0 when fewer than 2 source files).</returns>
    public static int CompactLevels01Forced(Database db, int tableId,
        ConcurrentDictionary<string, SstReader> readerCache)
    {
        if (!db.Manifest.TryGetTable(tableId, out var table) || table is null) return 0;

        var filesToMerge = new List<string>();
        var removedByLevel = new Dictionary<int, List<string>>();
        foreach (var level in new[] { 0, 1 })
        {
            var levelFiles = table.GetFilesForLevel(level).ToList();
            if (levelFiles.Count == 0) continue;

            levelFiles.Sort();
            levelFiles.Reverse();
            filesToMerge.AddRange(levelFiles);
            removedByLevel[level] = levelFiles;
        }

        if (filesToMerge.Count < 2) return 0;

        var readers = filesToMerge.Select(f =>
            readerCache.GetOrAdd(Path.Combine(db.DatabasePath, Config.SstDirectoryName, f),
                path => new SstReader(path))).ToList();
        var iterators = readers.Select(r => new SstIterator(r)).ToList();
        var mergedResults = KWayMerge(iterators);

        var newFileNumber = db.Manifest.GetNextFileNumber();
        var newSstPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, $"{newFileNumber:D6}_{tableId}.sst");
        SstWriter.WriteFromEnumerable(newSstPath, mergedResults);

        db.Manifest.ReplaceLevelsAfterCompaction(tableId, removedByLevel, 1, Path.GetFileName(newSstPath));
        db.Manifest.Save();

        foreach (var oldFile in filesToMerge)
        {
            var oldPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, oldFile);
            if (readerCache.TryRemove(oldPath, out var readerToDispose))
                readerToDispose.Dispose();

            if (File.Exists(oldPath))
                File.Delete(oldPath);
        }

        return filesToMerge.Count - 1;
    }

    /// <summary>
    /// Esegue un merge k-vie (k-way merge) su più iteratori ordinati, ottimizzato con un Min-Heap.
    /// Combina i dati da diverse SSTable, rimuovendo i duplicati (mantenendo solo la versione
    /// più recente, determinata dall'ordine degli iteratori) e le "tombstone" (valori nulli).
    /// Questa versione utilizza una PriorityQueue per ottenere performance O(log k) per ogni passo del merge,
    /// migliorando significativamente l'efficienza rispetto a una scansione lineare.
    /// </summary>
    /// <param name="iterators">Una lista di SstIterator, che devono essere forniti in ordine dal più recente al più vecchio.</param>
    /// <returns>Un enumerabile che produce il flusso di dati unito e pulito.</returns>
    private static IEnumerable<KeyValuePair<byte[], byte[]?>> KWayMerge(List<SstIterator> iterators)
    {
        // Un HashSet per tracciare le chiavi già processate, garantendo che solo la versione
        // più recente di ogni chiave venga inclusa nel risultato finale.
        var seenKeys = new HashSet<byte[]>(new ByteArrayEqualityComparer());

        // Un Min-Heap (PriorityQueue) per trovare in modo efficiente l'iteratore con la chiave
        // lexicograficamente più piccola in tempo O(log k), dove k è il numero di iteratori.
        // L'elemento è l'iteratore, la priorità è la sua chiave corrente.
        var priorityQueue = new PriorityQueue<SstIterator, byte[]>(new ByteArrayComparer());

        // Inizializza la coda con il primo elemento di ogni iteratore non vuoto.
        foreach (var iterator in iterators)
        {
            if (iterator.HasValue)
            {
                priorityQueue.Enqueue(iterator, iterator.Current.Key);
            }
        }

        // Continua il processo finché ci sono elementi nella coda di priorità.
        while (priorityQueue.TryDequeue(out var minIterator, out _))
        {
            // Estrai l'elemento con la chiave minima corrente.
            var currentEntry = minIterator.Current;

            // Avanza l'iteratore appena processato al suo elemento successivo.
            minIterator.MoveNext();

            // Se l'iteratore ha ancora valori, reinseriscilo nella coda con la sua nuova chiave.
            if (minIterator.HasValue)
            {
                priorityQueue.Enqueue(minIterator, minIterator.Current.Key);
            }

            // Se questa è la prima volta che incontriamo questa chiave, significa che è la versione
            // più recente disponibile (grazie all'ordine in cui operiamo e al Min-Heap).
            if (seenKeys.Add(currentEntry.Key))
            {
                // Se il valore non è una "tombstone" (nullo), lo includiamo nel risultato.
                if (currentEntry.Value != null)
                {
                    yield return currentEntry;
                }
            }
        }
    }
}