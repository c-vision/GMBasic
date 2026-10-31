// FILE: Basic.Databases.Speed.Manifest.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Text.Json;

namespace Basic.Databases.Speed;

/// <summary>
/// Rappresenta i metadati di una singola tabella (o vista, o indice) all'interno del database.
/// Questa classe è immutabile e contiene tutte le informazioni necessarie per interpretare i dati della tabella.
/// </summary>
public sealed record TableMetadata
{
    /// <summary>
    /// L'identificatore numerico univoco della tabella.
    /// </summary>
    public int Id { get; init; }

    /// <summary>
    /// Il nome univoco della tabella (case-insensitive).
    /// </summary>
    public string Name { get; init; } = "";

    /// <summary>
    /// Lo schema logico della tabella, che definisce i campi e gli indici.
    /// </summary>
    public Config.SpeedSchema Schema { get; init; } = null!;

    /// <summary>
    /// Un dizionario che mappa un numero di livello di compattazione (es. 0, 1, 2)
    /// alla lista dei nomi dei file SSTable che risiedono a quel livello.
    /// </summary>
    public Dictionary<int, List<string>> SstFilesByLevel { get; init; } = [];

    /// <summary>
    /// Approximate live row count (excludes tombstones). Persisted on manifest flush.
    /// ponytail: legacy DBs have <see cref="RowCountTrusted"/> false — rebuild via full scan or rewrite on open.
    /// </summary>
    public long RowCount { get; set; }

    /// <summary>
    /// When false, <see cref="RowCount"/> is unknown (pre-counter manifest) and must not be used for O(1) checks.
    /// </summary>
    public bool RowCountTrusted { get; set; }

    /// <summary>
    /// Recupera la lista dei nomi dei file per un livello di compattazione specifico.
    /// </summary>
    /// <param name="level">Il livello di cui recuperare i file.</param>
    /// <returns>Un enumerabile di stringhe contenente i nomi dei file, o un enumerabile vuoto se il livello non esiste.</returns>
    public IEnumerable<string> GetFilesForLevel(int level)
    {
        return SstFilesByLevel.TryGetValue(level, out var files) ? files : Enumerable.Empty<string>();
    }
}

/// <summary>
/// Rappresenta la struttura dati completa del file manifest.json, serializzabile da/verso JSON.
/// </summary>
internal sealed record ManifestData
{
    /// <summary>
    /// Un contatore globale e monotonicamente crescente per assegnare ID univoci ai nuovi file (WAL e SSTable).
    /// </summary>
    public ulong NextFileNumber { get; set; } = 1;

    /// <summary>
    /// Un contatore globale e monotonicamente crescente per assegnare ID univoci alle nuove tabelle.
    /// </summary>
    public int NextTableId { get; set; } = 1;

    /// <summary>
    /// Il dizionario principale che mappa i nomi delle tabelle ai loro metadati.
    /// </summary>
    public Dictionary<string, TableMetadata> Tables { get; init; } = new(StringComparer.OrdinalIgnoreCase);
    
    /// <summary>
    /// Lista di nomi di file SSTable che sono stati rimossi logicamente (es. da un DROP INDEX
    /// o una compattazione) e sono in attesa di essere eliminati fisicamente dal garbage collector.
    /// </summary>
    public List<string> FilesToDelete { get; set; } = [];
}

/// <summary>
/// Gestisce il file MANIFEST del database. È il "cervello" del database, responsabile di
/// mantenere lo stato coerente dello schema, delle tabelle e della posizione dei file fisici.
/// Tutte le operazioni su questa classe sono thread-safe.
/// </summary>
internal class Manifest
{
    private readonly string _databasePath;
    private readonly string _manifestPath;
    private ManifestData _data;
    private readonly object _lock = new();

    /// <summary>
    /// Inizializza una nuova istanza della classe Manifest. Il costruttore è privato
    /// per forzare l'uso del metodo factory statico <see cref="Load"/>.
    /// </summary>
    private Manifest(string databasePath)
    {
        _databasePath = databasePath;
        _manifestPath = Path.Combine(databasePath, Config.ManifestFileName);
        _data = new ManifestData();
    }
    
    /// <summary>
    /// Carica il manifest dal disco. Se il file non esiste, ne crea uno nuovo in memoria.
    /// </summary>
    /// <param name="databasePath">Il percorso della directory del database.</param>
    /// <returns>Un'istanza della classe Manifest.</returns>
    public static Manifest Load(string databasePath)
    {
        var manifest = new Manifest(databasePath);
        if (File.Exists(manifest._manifestPath))
        {
            var json = File.ReadAllText(manifest._manifestPath);
            manifest._data = JsonSerializer.Deserialize<ManifestData>(json) ?? new ManifestData();
        }
        return manifest;
    }

    /// <summary>
    /// Salva lo stato corrente del manifest su disco in modo atomico, utilizzando un file temporaneo
    /// e un'operazione di replace per prevenire la corruzione in caso di crash.
    /// </summary>
    public void Save()
    {
        lock (_lock)
        {
            var tempPath = _manifestPath + ".tmp";
            var json = JsonSerializer.Serialize(_data, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(tempPath, json);
            if (File.Exists(_manifestPath))
                File.Replace(tempPath, _manifestPath, null);
            else
                File.Move(tempPath, _manifestPath);
        }
    }

    /// <summary>
    /// Ottiene il prossimo numero sequenziale univoco per un file (WAL o SSTable).
    /// </summary>
    public ulong GetNextFileNumber()
    {
        lock (_lock)
        {
            return _data.NextFileNumber++;
        }
    }
    
    /// <summary>
    /// Ottiene il prossimo ID univoco per una nuova tabella, incrementando il contatore interno.
    /// L'operazione è protetta da un lock per garantire la thread-safety.
    /// </summary>
    /// <returns>Un nuovo ID intero per una tabella.</returns>
    private int GetNextTableId()
    {
        lock (_lock)
        {
            return _data.NextTableId++;
        }
    }

    /// <summary>
    /// Aggiunge un nuovo file SSTable al Livello 0 di una tabella specifica.
    /// </summary>
    /// <param name="tableId">L'ID della tabella a cui aggiungere il file.</param>
    /// <param name="sstFileName">Il nome del file SSTable (senza percorso).</param>
    public void AddSstFile(int tableId, string sstFileName)
    {
        lock (_lock)
        {
            var table = _data.Tables.Values.FirstOrDefault(t => t.Id == tableId);
            if (table == null) return;

            if (!table.SstFilesByLevel.TryGetValue(0, out var level0Files))
            {
                level0Files = [];
                table.SstFilesByLevel[0] = level0Files;
            }
            level0Files.Add(Path.GetFileName(sstFileName));
        }
    }
    
    /// <summary>
    /// Tenta di recuperare i metadati di una tabella dato il suo ID.
    /// </summary>
    /// <param name="tableId">L'ID della tabella da cercare.</param>
    /// <param name="metadata">I metadati della tabella trovata.</param>
    /// <returns>True se la tabella è stata trovata, altrimenti False.</returns>
    public bool TryGetTable(int tableId, out TableMetadata? metadata)
    {
        lock (_lock)
        {
            metadata = _data.Tables.Values.FirstOrDefault(t => t.Id == tableId);
            return metadata != null;
        }
    }
    
    /// <summary>
    /// Tenta di recuperare i metadati di una tabella dato il suo nome.
    /// </summary>
    /// <param name="name">Il nome della tabella da cercare.</param>
    /// <param name="metadata">I metadati della tabella trovata.</param>
    /// <returns>True se la tabella è stata trovata, altrimenti False.</returns>
    public bool TryGetTable(string name, out TableMetadata? metadata)
    {
        lock (_lock)
        {
            return _data.Tables.TryGetValue(name, out metadata);
        }
    }
    
    /// <summary>
    /// Crea una nuova tabella (o indice secondario) nel manifest, se non esiste già.
    /// Se la tabella esiste, valida che lo schema fornito sia compatibile con quello esistente.
    /// </summary>
    /// <param name="schema">Lo schema della tabella da creare o validare.</param>
    /// <returns>I metadati della tabella appena creata o di quella esistente e validata.</returns>
    /// <exception cref="InvalidOperationException">Lanciata se la tabella esiste ma lo schema fornito non è compatibile.</exception>
    public TableMetadata CreateTable(Config.SpeedSchema schema)
    {
        lock (_lock)
        {
            if (_data.Tables.TryGetValue(schema.TypeName, out var existingTable))
            {
                // SchemaEquals, non l'uguaglianza del record: i campi List non hanno
                // Equals per valore e i DataType sono nodi AST (Position/SourceToken),
                // quindi il confronto per record rigettava sempre la riapertura.
                if (!Config.SchemaEquals(existingTable.Schema, schema))
                {
                    throw new InvalidOperationException($"Lo schema del TYPE '{schema.TypeName}' non corrisponde a quello memorizzato.");
                }
                return existingTable;
            }

            var newTable = new TableMetadata
            {
                Id = GetNextTableId(),
                Name = schema.TypeName,
                Schema = schema,
                RowCount = 0,
                RowCountTrusted = true,
            };
            _data.Tables[newTable.Name] = newTable;
            Save();
            return newTable;
        }
    }

    /// <summary>
    /// Rimuove logicamente una tabella (o vista/indice) dal manifest e restituisce la lista dei suoi file fisici.
    /// </summary>
    /// <param name="tableName">Il nome della tabella da rimuovere.</param>
    /// <returns>Una tupla contenente l'ID della tabella rimossa e la lista dei suoi file da cancellare.</returns>
    public (int tableId, List<string> files) PrepareTableForDeletion(string tableName)
    {
        lock (_lock)
        {
            if (!_data.Tables.Remove(tableName, out var tableToRemove))
            {
                return (-1, []);
            }
            var filesToDelete = tableToRemove.SstFilesByLevel.Values.SelectMany(f => f).ToList();
            return (tableToRemove.Id, filesToDelete);
        }
    }
    
    /// <summary>
    /// Adds a secondary index definition to a main table schema if not already present.
    /// </summary>
    public void AddSecondaryIndex(string mainTableName, Config.SpeedIndexDefinition indexDef)
    {
        lock (_lock)
        {
            if (!TryGetTable(mainTableName, out var mainTable) || mainTable is null) return;
            if (mainTable.Schema.SecondaryKeys.Any(sk =>
                    sk.Name.Equals(indexDef.Name, StringComparison.OrdinalIgnoreCase)))
                return;
            mainTable.Schema.SecondaryKeys.Add(indexDef);
        }
    }

    /// <summary>
    /// Returns a snapshot of all table metadata entries.
    /// </summary>
    public IReadOnlyList<TableMetadata> GetAllTables()
    {
        lock (_lock)
        {
            return _data.Tables.Values.ToList();
        }
    }

    /// <summary>
    /// Rimuove la definizione di un indice secondario dallo schema di una tabella principale.
    /// </summary>
    /// <param name="mainTableName">Il nome della tabella principale da modificare.</param>
    /// <param name="indexName">Il nome dell'indice secondario da rimuovere.</param>
    public void RemoveSecondaryIndex(string mainTableName, string indexName)
    {
        lock (_lock)
        {
            if (TryGetTable(mainTableName, out var mainTable) && mainTable is not null)
            {
                mainTable.Schema.SecondaryKeys.RemoveAll(sk => sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase));
            }
        }
    }

    /// <summary>
    /// Prepara un indice secondario per la cancellazione. Rimuove la tabella-indice
    /// e la definizione dell'indice secondario dal manifest e restituisce la lista
    /// dei suoi file SSTable da eliminare.
    /// </summary>
    /// <param name="mainTableId">L'ID della tabella principale a cui appartiene l'indice.</param>
    /// <param name="indexName">Il nome dell'indice da rimuovere.</param>
    /// <returns>
    /// Una tupla contenente l'ID della tabella-indice rimossa e la lista dei suoi file
    /// da cancellare. Se l'indice non viene trovato, restituisce (-1, vuoto).
    /// </returns>
    public (int tableId, List<string> files) PrepareIndexForDeletion(int mainTableId, string indexName)
    {
        lock (_lock)
        {
            if (!TryGetTable(mainTableId, out var mainTable) || mainTable is null)
                return (-1, []);

            var indexTableName = $"_idx_{mainTable.Name}_{indexName}";
            if (!_data.Tables.TryGetValue(indexTableName, out var indexTable))
                return (-1, []);

            // Rimuovi la definizione dell'indice secondario dalla tabella principale.
            mainTable.Schema.SecondaryKeys.RemoveAll(sk => sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase));
            
            // Rimuovi la tabella-indice.
            _data.Tables.Remove(indexTableName);

            var filesToDelete = indexTable.SstFilesByLevel.Values.SelectMany(f => f).ToList();
            return (indexTable.Id, filesToDelete);
        }
    }
    
    /// <summary>
    /// Aggiorna atomicamente il manifest dopo un'operazione di compattazione.
    /// </summary>
    /// <param name="tableId">L'ID della tabella compattata.</param>
    /// <param name="sourceLevel">Il livello di origine dei file compattati.</param>
    /// <param name="compactedFiles">La lista dei nomi dei file che sono stati uniti e rimossi.</param>
    /// <param name="destLevel">Il livello di destinazione del nuovo file.</param>
    /// <param name="newFile">Il nome del nuovo file SSTable risultante dalla compattazione.</param>
    public void UpdateAfterCompaction(int tableId, int sourceLevel, IEnumerable<string> compactedFiles, int destLevel, string newFile)
    {
        lock (_lock)
        {
            if (!TryGetTable(tableId, out var table) || table is null) return;
            if (table.SstFilesByLevel.TryGetValue(sourceLevel, out var sourceFiles))
            {
                var filesToRemove = new HashSet<string>(compactedFiles);
                sourceFiles.RemoveAll(filesToRemove.Contains);
            }
            if (!table.SstFilesByLevel.TryGetValue(destLevel, out var destFiles))
            {
                destFiles = [];
                table.SstFilesByLevel[destLevel] = destFiles;
            }
            destFiles.Add(newFile);
        }
    }

    /// <summary>
    /// Replaces files across multiple source levels with one destination file (offline L0+L1 compact).
    /// </summary>
    public void ReplaceLevelsAfterCompaction(
        int tableId,
        IReadOnlyDictionary<int, List<string>> removedByLevel,
        int destLevel,
        string newFile)
    {
        lock (_lock)
        {
            if (!TryGetTable(tableId, out var table) || table is null) return;

            foreach (var (level, files) in removedByLevel)
            {
                if (!table.SstFilesByLevel.TryGetValue(level, out var levelFiles)) continue;
                var remove = new HashSet<string>(files);
                levelFiles.RemoveAll(remove.Contains);
            }

            if (!table.SstFilesByLevel.TryGetValue(destLevel, out var destFiles))
            {
                destFiles = [];
                table.SstFilesByLevel[destLevel] = destFiles;
            }

            destFiles.Add(newFile);
        }
    }
    
    /// <summary>
    /// Aggiunge una lista di nomi di file alla coda di garbage collection del manifest.
    /// </summary>
    /// <param name="files">La lista di nomi di file da schedulare per la cancellazione.</param>
    public void AddFilesToGarbageCollection(IEnumerable<string> files)
    {
        lock (_lock)
        {
            _data.FilesToDelete.AddRange(files);
        }
    }
    
    /// <summary>
    /// Recupera la lista corrente di file in attesa di garbage collection.
    /// </summary>
    /// <returns>Una copia della lista di nomi di file da eliminare.</returns>
    public List<string> GetFilesForGarbageCollection()
    {
        lock (_lock)
        {
            return [.._data.FilesToDelete];
        }
    }

    /// <summary>
    /// Rimuove un nome di file dalla lista di garbage collection, tipicamente dopo che
    /// il file è stato eliminato con successo.
    /// </summary>
    /// <param name="fileName">Il nome del file da rimuovere dalla lista.</param>
    public void RemoveFileFromGarbageCollection(string fileName)
    {
        lock (_lock)
        {
            _data.FilesToDelete.Remove(fileName);
        }
    }

    /// <summary>Live row count when trusted; otherwise 0.</summary>
    public long GetRowCount(int tableId)
    {
        lock (_lock)
        {
            var table = _data.Tables.Values.FirstOrDefault(t => t.Id == tableId);
            return table is { RowCountTrusted: true } ? table.RowCount : 0L;
        }
    }

    /// <summary>Whether manifest row count is maintained for this table.</summary>
    public bool IsRowCountTrusted(int tableId)
    {
        lock (_lock)
        {
            var table = _data.Tables.Values.FirstOrDefault(t => t.Id == tableId);
            return table?.RowCountTrusted ?? false;
        }
    }

    /// <summary>Sets absolute row count (e.g. after index backfill).</summary>
    public void SetRowCount(int tableId, long count)
    {
        lock (_lock)
        {
            var table = _data.Tables.Values.FirstOrDefault(t => t.Id == tableId);
            if (table is null) return;
            table.RowCount = Math.Max(0, count);
            table.RowCountTrusted = true;
        }
    }

    /// <summary>Increments/decrements live row count. Crash-safe: persisted on next <see cref="Save"/> (flush).</summary>
    public void AdjustRowCount(int tableId, long delta)
    {
        if (delta == 0) return;
        lock (_lock)
        {
            var table = _data.Tables.Values.FirstOrDefault(t => t.Id == tableId);
            if (table is null) return;
            table.RowCount = Math.Max(0, table.RowCount + delta);
            table.RowCountTrusted = true;
        }
    }
}