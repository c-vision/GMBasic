/* FILE: native/gmb_db_c/src/SPEED/Compactor.c */
/* Port 1:1 di modules/Database/SPEED/Compactor.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Compactor.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Collections.Concurrent;
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Helper class per la compattazione che wrappa un SstReader per facilitare l'iterazione nel merge.
///// </summary>
//internal class SstIterator
//{
//    private readonly IEnumerator<KeyValuePair<byte[], byte[]?>> _enumerator;
//    public bool HasValue { get; private set; }
//
//    public SstIterator(SstReader reader)
//    {
//        _enumerator = reader.GetEnumerator();
//        HasValue = _enumerator.MoveNext();
//    }
//
//    public KeyValuePair<byte[], byte[]?> Current => _enumerator.Current;
//
//    public void MoveNext()
//    {
//        HasValue = _enumerator.MoveNext();
//    }
//}
//
///// <summary>
///// Fornisce la logica per la compattazione dei file SSTable.
///// La compattazione è un processo in background essenziale per un LSM-Tree, che unisce
///// più file piccoli in file più grandi, rimuovendo dati duplicati e cancellati.
///// </summary>
//internal static class Compactor
//{
//    /// <summary>
//    /// La soglia (numero di file) per i livelli L1 e superiori che innesca una compattazione.
//    /// Tipicamente più alta di quella di L0.
//    /// </summary>
//    private const int CompactLevelThreshold = 10;
//
//    /// <summary>
//    /// Esegue la compattazione dei file di Livello 0 per una data tabella.
//    /// Unisce tutti i file L0 in un unico nuovo file a un livello superiore, rimuovendo
//    /// dati obsoleti e cancellati. Viene attivata quando il numero di file in L0 supera la soglia.
//    /// </summary>
//    /// <param name="databasePath">Il percorso radice del database.</param>
//    /// <param name="manifest">L'istanza del manifest da aggiornare atomicamente.</param>
//    /// <param name="tableId">L'ID della tabella da compattare.</param>
//    /// <param name="readerCache">La cache dei lettori SSTable da cui rimuovere i vecchi file.</param>
//    /// <returns>Un <see cref="Task"/> che rappresenta l'operazione di compattazione asincrona.</returns>
//    public static async Task CompactLevel0Async(Database db, int tableId,
//        ConcurrentDictionary<string, SstReader> readerCache)
//    {
//        // 1. Identifica i file da compattare
//        if (!db.Manifest.TryGetTable(tableId, out var table) || table is null) return;
//
//        var level0Files = table.GetFilesForLevel(0).ToList();
//        if (level0Files.Count < Database.CompactLevel0Threshold) return;
//
//        // Ordina i file dal più recente al più vecchio per gestire correttamente i duplicati
//        level0Files.Sort();
//        level0Files.Reverse();
//
//        // 2. Prepara i lettori e l'iteratore per il k-way merge
//        var readers = level0Files.Select(f =>
//            readerCache.GetOrAdd(Path.Combine(db.DatabasePath, Config.SstDirectoryName, f),
//                path => new SstReader(path))).ToList();
//        var iterators = readers.Select(r => new SstIterator(r)).ToList();
//
//        var mergedResults = KWayMerge(iterators);
//
//        // 3. Scrivi il risultato compattato su un nuovo file SSTable al Livello 1
//        var newFileNumber = db.Manifest.GetNextFileNumber();
//        var newSstPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, $"{newFileNumber:D6}_{tableId}.sst");
//
//        await Task.Run(() => SstWriter.WriteFromEnumerable(newSstPath, mergedResults));
//
//        // 4. Aggiorna atomicamente il Manifest
//        db.Manifest.UpdateAfterCompaction(tableId, 0, level0Files, 1, Path.GetFileName(newSstPath));
//        db.Manifest.Save();
//
//        // 5. Pulisci i vecchi file L0 e rimuovili dalla cache dei reader
//        foreach (var oldFile in level0Files)
//        {
//            var oldPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, oldFile);
//            if (readerCache.TryRemove(oldPath, out var readerToDispose))
//            {
//                readerToDispose.Dispose();
//            }
//
//            File.Delete(oldPath);
//        }
//    }
//
//    /// <summary>
//    /// Esegue la compattazione di tutti i file di un livello N (con N > 0) in un nuovo singolo file
//    /// nel livello N+1. Questa operazione viene innescata quando il numero di file in un livello
//    /// supera la soglia definita, prevenendo il degrado delle performance di lettura.
//    /// </summary>
//    /// <param name="databasePath">Il percorso radice del database.</param>
//    /// <param name="manifest">L'istanza del manifest da aggiornare atomicamente.</param>
//    /// <param name="tableId">L'ID della tabella a cui appartengono i file.</param>
//    /// <param name="levelToCompact">Il livello (maggiore di 0) da cui compattare i file.</param>
//    /// <param name="readerCache">La cache dei lettori SSTable da cui rimuovere i vecchi file.</param>
//    /// <returns>Un <see cref="Task"/> che rappresenta l'operazione di compattazione asincrona.</returns>
//    /// <summary>
//    /// Esegue la compattazione di tutti i file di un livello N (con N > 0) in un nuovo singolo file
//    /// nel livello N+1. Questa operazione viene innescata quando il numero di file in un livello
//    /// supera la soglia definita, prevenendo il degrado delle performance di lettura.
//    /// </summary>
//    /// <param name="db">L'istanza del database principale, usata per accedere al percorso e al manifest.</param>
//    /// <param name="tableId">L'ID della tabella a cui appartengono i file.</param>
//    /// <param name="levelToCompact">Il livello (maggiore di 0) da cui compattare i file.</param>
//    /// <param name="readerCache">La cache dei lettori SSTable da cui rimuovere i vecchi file.</param>
//    /// <returns>Un <see cref="Task"/> che rappresenta l'operazione di compattazione asincrona.</returns>
//    public static async Task CompactLevelAsync(Database db, int tableId, int levelToCompact,
//        ConcurrentDictionary<string, SstReader> readerCache)
//    {
//        if (levelToCompact <= 0) return;
//
//        if (!db.Manifest.TryGetTable(tableId, out var table) || table is null) return;
//
//        var filesToCompact = table.GetFilesForLevel(levelToCompact).ToList();
//        if (filesToCompact.Count < CompactLevelThreshold) return;
//
//        var readers = filesToCompact.Select(f =>
//            readerCache.GetOrAdd(Path.Combine(db.DatabasePath, Config.SstDirectoryName, f),
//                path => new SstReader(path))).ToList();
//        var iterators = readers.Select(r => new SstIterator(r)).ToList();
//
//        var mergedResults = KWayMerge(iterators);
//
//        var newFileNumber = db.Manifest.GetNextFileNumber();
//        var newSstPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, $"{newFileNumber:D6}_{tableId}.sst");
//
//        await Task.Run(() => SstWriter.WriteFromEnumerable(newSstPath, mergedResults));
//
//        db.Manifest.UpdateAfterCompaction(tableId, levelToCompact, filesToCompact, levelToCompact + 1,
//            Path.GetFileName(newSstPath));
//        db.Manifest.Save();
//
//        foreach (var oldFile in filesToCompact)
//        {
//            var oldPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, oldFile);
//            if (readerCache.TryRemove(oldPath, out var readerToDispose))
//            {
//                readerToDispose.Dispose();
//            }
//
//            File.Delete(oldPath);
//        }
//    }
//
//    /// <summary>
//    /// Offline/ops compaction: merges all L0 and L1 SST files into one L1 file.
//    /// ponytail: levels 2+ untouched — upgrade path: full LSM major compact across all levels.
//    /// </summary>
//    /// <returns>Net SST files removed (0 when fewer than 2 source files).</returns>
//    public static int CompactLevels01Forced(Database db, int tableId,
//        ConcurrentDictionary<string, SstReader> readerCache)
//    {
//        if (!db.Manifest.TryGetTable(tableId, out var table) || table is null) return 0;
//
//        var filesToMerge = new List<string>();
//        var removedByLevel = new Dictionary<int, List<string>>();
//        foreach (var level in new[] { 0, 1 })
//        {
//            var levelFiles = table.GetFilesForLevel(level).ToList();
//            if (levelFiles.Count == 0) continue;
//
//            levelFiles.Sort();
//            levelFiles.Reverse();
//            filesToMerge.AddRange(levelFiles);
//            removedByLevel[level] = levelFiles;
//        }
//
//        if (filesToMerge.Count < 2) return 0;
//
//        var readers = filesToMerge.Select(f =>
//            readerCache.GetOrAdd(Path.Combine(db.DatabasePath, Config.SstDirectoryName, f),
//                path => new SstReader(path))).ToList();
//        var iterators = readers.Select(r => new SstIterator(r)).ToList();
//        var mergedResults = KWayMerge(iterators);
//
//        var newFileNumber = db.Manifest.GetNextFileNumber();
//        var newSstPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, $"{newFileNumber:D6}_{tableId}.sst");
//        SstWriter.WriteFromEnumerable(newSstPath, mergedResults);
//
//        db.Manifest.ReplaceLevelsAfterCompaction(tableId, removedByLevel, 1, Path.GetFileName(newSstPath));
//        db.Manifest.Save();
//
//        foreach (var oldFile in filesToMerge)
//        {
//            var oldPath = Path.Combine(db.DatabasePath, Config.SstDirectoryName, oldFile);
//            if (readerCache.TryRemove(oldPath, out var readerToDispose))
//                readerToDispose.Dispose();
//
//            if (File.Exists(oldPath))
//                File.Delete(oldPath);
//        }
//
//        return filesToMerge.Count - 1;
//    }
//
//    /// <summary>
//    /// Esegue un merge k-vie (k-way merge) su più iteratori ordinati, ottimizzato con un Min-Heap.
//    /// Combina i dati da diverse SSTable, rimuovendo i duplicati (mantenendo solo la versione
//    /// più recente, determinata dall'ordine degli iteratori) e le "tombstone" (valori nulli).
//    /// Questa versione utilizza una PriorityQueue per ottenere performance O(log k) per ogni passo del merge,
//    /// migliorando significativamente l'efficienza rispetto a una scansione lineare.
//    /// </summary>
//    /// <param name="iterators">Una lista di SstIterator, che devono essere forniti in ordine dal più recente al più vecchio.</param>
//    /// <returns>Un enumerabile che produce il flusso di dati unito e pulito.</returns>
//    private static IEnumerable<KeyValuePair<byte[], byte[]?>> KWayMerge(List<SstIterator> iterators)
//    {
//        // Un HashSet per tracciare le chiavi già processate, garantendo che solo la versione
//        // più recente di ogni chiave venga inclusa nel risultato finale.
//        var seenKeys = new HashSet<byte[]>(new ByteArrayEqualityComparer());
//
//        // Un Min-Heap (PriorityQueue) per trovare in modo efficiente l'iteratore con la chiave
//        // lexicograficamente più piccola in tempo O(log k), dove k è il numero di iteratori.
//        // L'elemento è l'iteratore, la priorità è la sua chiave corrente.
//        var priorityQueue = new PriorityQueue<SstIterator, byte[]>(new ByteArrayComparer());
//
//        // Inizializza la coda con il primo elemento di ogni iteratore non vuoto.
//        foreach (var iterator in iterators)
//        {
//            if (iterator.HasValue)
//            {
//                priorityQueue.Enqueue(iterator, iterator.Current.Key);
//            }
//        }
//
//        // Continua il processo finché ci sono elementi nella coda di priorità.
//        while (priorityQueue.TryDequeue(out var minIterator, out _))
//        {
//            // Estrai l'elemento con la chiave minima corrente.
//            var currentEntry = minIterator.Current;
//
//            // Avanza l'iteratore appena processato al suo elemento successivo.
//            minIterator.MoveNext();
//
//            // Se l'iteratore ha ancora valori, reinseriscilo nella coda con la sua nuova chiave.
//            if (minIterator.HasValue)
//            {
//                priorityQueue.Enqueue(minIterator, minIterator.Current.Key);
//            }
//
//            // Se questa è la prima volta che incontriamo questa chiave, significa che è la versione
//            // più recente disponibile (grazie all'ordine in cui operiamo e al Min-Heap).
//            if (seenKeys.Add(currentEntry.Key))
//            {
//                // Se il valore non è una "tombstone" (nullo), lo includiamo nel risultato.
//                if (currentEntry.Value != null)
//                {
//                    yield return currentEntry;
//                }
//            }
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>

#define COMPACT_MAX_FILES 64

typedef struct {
    uint8_t* key;
    int keyLen;
    uint8_t* val;
    int valLen;
    int tomb;
    int source;
} CompactEnt;

static int compact_cmp(const void* a, const void* b)
{
    const CompactEnt* x = (const CompactEnt*)a;
    const CompactEnt* y = (const CompactEnt*)b;
    int c = ByteArrayComparer_Compare(x->key, x->keyLen, y->key, y->keyLen);
    if (c != 0) return c;
    return x->source - y->source;
}

static int compact_name_desc(const void* a, const void* b)
{
    return strcmp((const char*)b, (const char*)a);
}

static int compact_push(CompactEnt** ents, int* n, int* cap, const uint8_t* key, int keyLen,
                        const uint8_t* val, int valLen, int tomb, int source)
{
    CompactEnt* e;
    if (*n >= *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *ents = (CompactEnt*)realloc(*ents, (size_t)(*cap) * sizeof(CompactEnt));
    }
    e = &(*ents)[(*n)++];
    e->keyLen = keyLen;
    e->valLen = valLen;
    e->tomb = tomb;
    e->source = source;
    e->key = (uint8_t*)malloc((size_t)keyLen);
    if (keyLen > 0) memcpy(e->key, key, (size_t)keyLen);
    e->val = NULL;
    if (!tomb && val && valLen > 0) {
        e->val = (uint8_t*)malloc((size_t)valLen);
        memcpy(e->val, val, (size_t)valLen);
    }
    return 0;
}

static void compact_free_ents(CompactEnt* ents, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        free(ents[i].key);
        free(ents[i].val);
    }
    free(ents);
}

static void compact_delete_ssts(Database* db, char files[][256], int nfiles)
{
    const char* root = Database_GetPath(db);
    int i;
    for (i = 0; i < nfiles; i++) {
        char path[1100];
        snprintf(path, sizeof(path), "%s/%s/%s", root, Config_SstDirectoryName, files[i]);
        Database_SstCacheRemove(db, path);
        unlink(path);
    }
}

/* iterators newest-first: source 0 wins. Tomb first hides older live (C# KWayMerge). */
static int compact_merge_write(Database* db, int tableId, char files[][256], int nfiles, char newName[256])
{
    CompactEnt* ents = NULL;
    int n = 0, cap = 0, i, src = 0;
    Memtable* out;
    uint64_t fileNumber;
    char sstPath[1100];
    const char* root;
    Manifest* man;

    if (nfiles < 2) return 0;
    root = Database_GetPath(db);
    man = Database_GetManifest(db);
    for (i = 0; i < nfiles; i++, src++) {
        char path[1100];
        SstReader* r;
        snprintf(path, sizeof(path), "%s/%s/%s", root, Config_SstDirectoryName, files[i]);
        r = Database_SstGetOrAdd(db, path);
        if (!r) { compact_free_ents(ents, n); return GMB_DB_ERR; }
        SstReader_Rewind(r);
        for (;;) {
            uint8_t *k = NULL, *v = NULL;
            int kl = 0, vl = 0;
            if (!SstReader_Next(r, &k, &kl, &v, &vl)) break;
            compact_push(&ents, &n, &cap, k, kl, v, vl, v == NULL, src);
            free(k);
            free(v);
        }
    }
    if (n > 1) qsort(ents, (size_t)n, sizeof(CompactEnt), compact_cmp);
    out = Memtable_New();
    for (i = 0; i < n; ) {
        int j = i + 1;
        while (j < n && ByteArrayComparer_Compare(ents[i].key, ents[i].keyLen, ents[j].key, ents[j].keyLen) == 0)
            j++;
        if (!ents[i].tomb)
            Memtable_Set(out, ents[i].key, ents[i].keyLen, ents[i].val, ents[i].valLen);
        i = j;
    }
    compact_free_ents(ents, n);
    fileNumber = Manifest_GetNextFileNumber(man);
    snprintf(sstPath, sizeof(sstPath), "%s/%s/%06llu_%d.sst",
             root, Config_SstDirectoryName, (unsigned long long)fileNumber, tableId);
    if (SstWriter_WriteFromMemtable(sstPath, out) != GMB_DB_OK) {
        Memtable_Dispose(out);
        return GMB_DB_ERR;
    }
    Memtable_Dispose(out);
    {
        const char* base = strrchr(sstPath, '/');
        snprintf(newName, 256, "%s", base ? base + 1 : sstPath);
    }
    return 1;
}

int Compactor_CompactLevel0(Database* db, int tableId)
{
    char files[COMPACT_MAX_FILES][256];
    char newName[256];
    int n, rc;
    Manifest* man;
    if (!db) return 0;
    man = Database_GetManifest(db);
    n = Manifest_GetFilesForLevel(man, tableId, 0, files, COMPACT_MAX_FILES);
    if (n < Database_CompactLevel0Threshold) return 0;
    qsort(files, (size_t)n, 256, compact_name_desc);
    rc = compact_merge_write(db, tableId, files, n, newName);
    if (rc <= 0) return rc;
    Manifest_UpdateAfterCompaction(man, tableId, 0, files, n, 1, newName);
    Manifest_Save(man);
    compact_delete_ssts(db, files, n);
    return n - 1;
}

int Compactor_CompactLevel(Database* db, int tableId, int level)
{
    char files[COMPACT_MAX_FILES][256];
    char newName[256];
    int n, rc;
    Manifest* man;
    if (!db || level <= 0) return 0;
    man = Database_GetManifest(db);
    n = Manifest_GetFilesForLevel(man, tableId, level, files, COMPACT_MAX_FILES);
    if (n < Compactor_CompactLevelThreshold) return 0;
    rc = compact_merge_write(db, tableId, files, n, newName);
    if (rc <= 0) return rc;
    Manifest_UpdateAfterCompaction(man, tableId, level, files, n, level + 1, newName);
    Manifest_Save(man);
    compact_delete_ssts(db, files, n);
    return n - 1;
}

int Compactor_CompactLevels01Forced(Database* db, int tableId)
{
    char (*files)[256] = NULL;
    char tmp[COMPACT_MAX_FILES][256];
    char newName[256];
    int n = 0, cap = 0, lv, i, k, maxLv = 0, dest, rc;
    Manifest* man;
    if (!db) return 0;
    man = Database_GetManifest(db);
    for (lv = 0; lv < 16; lv++) {
        k = Manifest_GetFilesForLevel(man, tableId, lv, tmp, COMPACT_MAX_FILES);
        if (k > 0) maxLv = lv;
        if (k > 1) qsort(tmp, (size_t)k, 256, compact_name_desc);
        for (i = 0; i < k; i++) {
            if (n >= cap) {
                int ncap = cap ? cap * 2 : 16;
                char (*grown)[256] = (char (*)[256])realloc(files, (size_t)ncap * 256);
                if (!grown) {
                    free(files);
                    return GMB_DB_ERR;
                }
                files = grown;
                cap = ncap;
            }
            snprintf(files[n++], 256, "%s", tmp[i]);
        }
    }
    if (n < 2) {
        free(files);
        return 0;
    }
    rc = compact_merge_write(db, tableId, files, n, newName);
    if (rc <= 0) {
        free(files);
        return rc;
    }
    dest = maxLv < 1 ? 1 : maxLv;
    Manifest_ReplaceAllSstAfterMajorCompact(man, tableId, dest, newName);
    Manifest_Save(man);
    compact_delete_ssts(db, files, n);
    free(files);
    return n - 1;
}

int Compactor_SelfCheck(void)
{
    char dir[] = "/tmp/gmb_speed_compact_XXXXXX";
    Database* db;
    Manifest* man;
    int tableId, i, n0, n1, found, vlen;
    char names[16][256];
    uint8_t *val = NULL, *blob = NULL;
    char key[8], value[8];

    if (!mkdtemp(dir)) return -160;
    db = Database_Open(dir);
    if (!db) return -161;
    tableId = Database_CreateTable(db, "Person");
    if (tableId <= 0) { Database_Dispose(db); return -162; }
    for (i = 0; i < Database_CompactLevel0Threshold; i++) {
        snprintf(key, sizeof(key), "k%03d", i);
        snprintf(value, sizeof(value), "v%03d", i);
        if (Database_Set(db, tableId, (const uint8_t*)key, 4, (const uint8_t*)value, 4) != GMB_DB_OK) {
            Database_Dispose(db); return -163;
        }
        Database_ForceFlushTableSync(db, tableId);
    }
    man = Database_GetManifest(db);
    n0 = Manifest_GetFilesForLevel(man, tableId, 0, names, 16);
    if (n0 < Database_CompactLevel0Threshold) { Database_Dispose(db); return -164; }
    Database_TriggerCompactionChain(db, tableId);
    n0 = Manifest_GetFilesForLevel(man, tableId, 0, names, 16);
    n1 = Manifest_GetFilesForLevel(man, tableId, 1, names, 16);
    if (n0 != 0 || n1 < 1) { Database_Dispose(db); return -165; }
    found = Database_Get(db, tableId, (const uint8_t*)"k000", 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v000", 4) != 0) {
        free(val); Database_Dispose(db); return -166;
    }
    free(val);

    blob = (uint8_t*)malloc((size_t)Database_MemtableThreshold + 32);
    if (!blob) { Database_Dispose(db); return -167; }
    memset(blob, 0x5a, (size_t)Database_MemtableThreshold + 32);
    if (Database_Set(db, tableId, (const uint8_t*)"bigk", 4, blob, Database_MemtableThreshold + 32) != GMB_DB_OK) {
        free(blob); Database_Dispose(db); return -168;
    }
    free(blob);
    n0 = Manifest_GetFilesForLevel(man, tableId, 0, names, 16);
    if (n0 < 1) { Database_Dispose(db); return -169; }
    found = Database_Get(db, tableId, (const uint8_t*)"bigk", 4, &val, &vlen);
    if (!found || vlen != Database_MemtableThreshold + 32 || val[0] != 0x5a) {
        free(val); Database_Dispose(db); return -170;
    }
    free(val);
    {
        int round, n2, nTot, lv;
        char kbuf[16], vbuf[16];
        for (round = 0; round < Compactor_CompactLevelThreshold; round++) {
            for (i = 0; i < Database_CompactLevel0Threshold; i++) {
                snprintf(kbuf, sizeof(kbuf), "L%02d%02d", round, i);
                snprintf(vbuf, sizeof(vbuf), "V%02d%02d", round, i);
                if (Database_Set(db, tableId, (const uint8_t*)kbuf, 5, (const uint8_t*)vbuf, 5) != GMB_DB_OK) {
                    Database_Dispose(db); return -171;
                }
                Database_ForceFlushTableSync(db, tableId);
            }
            Database_TriggerCompactionChain(db, tableId);
        }
        n2 = Manifest_GetFilesForLevel(man, tableId, 2, names, 16);
        if (n2 < 1) { Database_Dispose(db); return -172; }
        if (Database_CompactAll(db) < 0) { Database_Dispose(db); return -173; }
        nTot = 0;
        for (lv = 0; lv < 16; lv++)
            nTot += Manifest_GetFilesForLevel(man, tableId, lv, names, 16);
        if (nTot < 1) { Database_Dispose(db); return -174; }
        found = Database_Get(db, tableId, (const uint8_t*)"k000", 4, &val, &vlen);
        if (!found || vlen != 4 || memcmp(val, "v000", 4) != 0) {
            free(val); Database_Dispose(db); return -175;
        }
        free(val);
        found = Database_Get(db, tableId, (const uint8_t*)"L0000", 5, &val, &vlen);
        if (!found || vlen != 5 || memcmp(val, "V0000", 5) != 0) {
            free(val); Database_Dispose(db); return -176;
        }
        free(val);
    }
    Database_Dispose(db);
    return GMB_DB_OK;
}
