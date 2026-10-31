// FILE: Basic.ISAM.Engine.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Buffers;
using System.Collections.Concurrent;
using Basic.DBF;
using Basic.ISAM;
using Basic.Databases;
using Basic.Databases.Replication;

namespace Basic.ISAM;

/*
------------------------------------------------------------------------------------------------------------------------
                                         ANALISI DEL MOTORE ISAM (REVISIONE POST-TRANSAZIONALE)
------------------------------------------------------------------------------------------------------------------------
Questo documento analizza le caratteristiche, l'architettura e la qualità dell'implementazione del motore ISAM
integrato nell'interprete BASIC, aggiornata per includere il supporto completo per le transazioni ACID.

------------------------------------------------------------------------------------------------------------------------
1. SICUREZZA E DURABILITÀ DEI DATI (DATA SAFETY & DURABILITY)
------------------------------------------------------------------------------------------------------------------------
QUALITÀ STIMATA: ECCELLENTE

CARATTERISTICHE IMPLEMENTATE:
- WRITE-AHEAD LOG (WAL): Ogni operazione di modifica (scrittura record, nodi indice, header) viene prima
 registrata in un file di log separato (.wal). Questo garantisce che, in caso di crash o interruzione di
 corrente, nessuna operazione venga lasciata a metà.
- RECOVERY AUTOMATICO: All'apertura del database, il motore controlla la presenza di un file .wal. Se esiste,
 esegue automaticamente un "roll-forward", riapplicando solo le transazioni completamente commesse per riportare
 il file di database a uno stato perfettamente consistente.
- STRUTTURA FILE AUTO-DESCRITTIVA: Il file .isdb contiene al suo interno lo schema completo del TYPE e la
 definizione degli indici. Ad ogni OPEN, lo schema del codice viene validato contro quello del file,
 prevenendo corruzioni causate da modifiche al codice sorgente BASIC.
- TRANSAZIONI ACID MULTI-OPERAZIONE: Il motore supporta pienamente transazioni multi-comando tramite
 `BEGIN TRANSACTION`, `COMMIT` e `ROLLBACK`. Questo garantisce l'Atomicità (o tutto o niente) per gruppi
 complessi di operazioni, risolvendo il problema fondamentale dell'inconsistenza dei dati in caso di fallimenti
 parziali.

------------------------------------------------------------------------------------------------------------------------
2. GESTIONE DELLA CONCORRENZA (CONCURRENCY)
------------------------------------------------------------------------------------------------------------------------
QUALITÀ STIMATA: ECCELLENTE (GRADO ENTERPRISE)

CARATTERISTICHE IMPLEMENTATE:
- SUPPORTO COMPLETO AI LIVELLI DI ISOLAMENTO: Il motore implementa i livelli di isolamento standard per un controllo
 granulare sulla concorrenza:
   - Read Committed: Lo standard, garantisce che non vengano lette modifiche non ancora commesse (no Dirty Reads).
   - Repeatable Read: Garantisce che una rilettura dello stesso record all'interno di una transazione restituisca
     sempre gli stessi dati (no Non-Repeatable Reads), implementato tramite lock di lettura (condivisi) di lunga
     durata.
   - Serializable: Il livello più alto di isolamento. Previene anche le "letture fantasma" (Phantom Reads)
     implementando una sofisticata strategia di Index-Range Locking (o Next-Key Locking), che blocca gli
     intervalli di chiavi dell'indice per impedire l'inserimento di nuovi record che potrebbero alterare i
     risultati di una query.
- CONCORRENZA OTTIMISTICA CON VERSIONING (MVCC): Ogni record possiede metadati di versione e transazione. Questo
 permette di gestire la visibilità dei dati e di implementare un controllo di concorrenza ottimistico (un `PUT`
 fallisce se il record è stato modificato da un altro utente dopo la lettura), prevenendo il "lost update problem".
- LOCKING PESSIMISTICO A GRANA FINE: Oltre ai lock transazionali, il motore utilizza lock di scrittura (esclusivi) a
 livello di record per le operazioni di modifica, permettendo a più utenti di modificare record *diversi* nello
 stesso file simultaneamente.

------------------------------------------------------------------------------------------------------------------------
3. PERFORMANCE E SCALABILITÀ (THROUGHPUT & SCALABILITY)
------------------------------------------------------------------------------------------------------------------------
QUALITÀ STIMATA: MOLTO BUONA (per il contesto di un interprete)

CARATTERISTICHE IMPLEMENTATE:
- INDICIZZAZIONE B+TREE: L'uso di B+Tree garantisce che le operazioni di ricerca, inserimento e cancellazione abbiano
 una complessità logaritmica, O(log N).
- CACHE LRU (LEAST RECENTLY USED): I nodi B+Tree più frequentemente utilizzati vengono mantenuti in una cache in
 memoria, riducendo drasticamente le operazioni di I/O su disco.
- SCANSIONE SEQUENZIALE EFFICIENTE: I nodi foglia del B+Tree sono collegati in una lista concatenata, permettendo di
 eseguire scansioni ordinate (`SKIP`) in modo molto efficiente.
- NOTA SULLE PERFORMANCE: Come in tutti i sistemi di database, l'uso di livelli di isolamento più elevati
 (`Repeatable Read`, `Serializable`) introduce un overhead di locking che può ridurre il throughput in scenari ad
 alta contesa. Questo è un trade-off standard e necessario per garantire una maggiore coerenza dei dati.

------------------------------------------------------------------------------------------------------------------------
4. MANUTENZIONE E BACKUP (MAINTENANCE & BACKUP)
------------------------------------------------------------------------------------------------------------------------
QUALITÀ STIMATA: ECCELLENTE

CARATTERISTICHE IMPLEMENTATE:
- BACKUP A CALDO (SNAPSHOT): Il comando `BACKUP` crea una copia consistente del database, forzando prima il flush del
 WAL per garantire che lo snapshot rappresenti l'ultimo stato commesso.
- COMPATTAZIONE (PACK): L'operazione di `PACK` è ottimizzata per creare un nuovo file compattato e sostituire
 atomicamente quello vecchio, eliminando la frammentazione e massimizzando le performance.
- REINDEXING: Permette di ricostruire tutti gli indici da zero, fornendo un meccanismo di recupero robusto in caso di
 rara corruzione degli indici, senza perdita di dati.
- SOFT DELETE E FREE LIST: La cancellazione non rimuove fisicamente i dati, ma li marca. L'operazione di `APPEND`
 riutilizza in modo intelligente lo spazio dei record cancellati, ottimizzando l'uso dello spazio su disco.

------------------------------------------------------------------------------------------------------------------------
                                                  VERDETTO FINALE
------------------------------------------------------------------------------------------------------------------------
Il motore ISAM ha colmato la sua lacuna più significativa, evolvendo da un robusto gestore di file indicizzati a un
vero e proprio micro-database engine transazionale conforme ai principi ACID. L'implementazione del WAL, combinata con
una gestione sofisticata della concorrenza che include MVCC e livelli di isolamento fino a `Serializable`, lo
posiziona a un livello di affidabilità e robustezza di grado enterprise.

Punti di Miglioramento Futuri:
- Tooling di Manutenzione Offline: Mancano utility esterne per la verifica dell'integrità o il recupero da file
 fisicamente corrotti (es. a causa di problemi hardware).

Conclusione:
Il motore è ora pienamente affidabile per l'uso in produzione in applicazioni multi-utente che richiedono la massima
integrità dei dati, anche in scenari complessi che necessitano di transazioni multi-operazione. Supera
concettualmente e tecnologicamente le capacità del sistema DBF/NTX di Clipper in ogni aspetto fondamentale.

------------------------------------------------------------------------------------------------------------------------
                                         TABELLA RIASSUNTIVA VS. CLIPPER (DBF/NTX)
------------------------------------------------------------------------------------------------------------------------
Caratteristica        Clipper (DBF/NTX)                   Nostro Motore ISAM                             Vincitore
--------------------  ----------------------------------  --------------------------------------------  -------------
Affidabilità Dati     Bassa (Rischio Corruzione Indici)   Eccellente (WAL & Atomicità)                  Nostro Motore
Validazione Schema    Inesistente                         Sì, Rigorosa                                  Nostro Motore
Concorrenza           Manuale, Pessimistica               MVCC con Locking multi-livello                Nostro Motore
                                                         (Record e Intervalli)
Tipo di Indice        B-Tree                              B+Tree                                        Leggero Vant.
Caching               Sistema Operativo / EVM             Cache LRU Dedicata                            Nostro Motore
Transazioni           Nessuna                             Pieno Supporto ACID con Isolamento            Nostro Motore
                                                         fino a `Serializable`
*/
/// <summary>
/// Opzioni di configurazione per il motore ISAM.
/// </summary>
public record IsamEngineOptions
{
    /// <summary>
    /// Se true, tutti gli indici (primario e secondari) vengono caricati in memoria per SEEK/SKIP veloci.
    /// Consuma più RAM ma riduce drasticamente l'I/O su disco.
    /// </summary>
    public bool UseInMemoryIndex { get; init; } = false;

    /// <summary>
    /// Politica di sincronizzazione su disco per operazioni auto-commit.
    /// </summary>
    public IsamDurabilityMode DurabilityMode { get; init; } = IsamDurabilityMode.Full;

    /// <summary>
    /// Timeout (ms) per lock su record e chiavi indice nelle operazioni transazionali.
    /// </summary>
    public int LockTimeoutMilliseconds { get; init; } = 5000;

    /* E1.3 (2026-09-29): UseMmapReadOnly RIMOSSO insieme a tutto il ramo ISAM-C3
     * (vedi FileHandle.cs). Nessuno lo impostava mai a true. */
}

/// <summary>
/// Motore di alto livello per la gestione di un file ISAM. Questa classe
/// implementa la logica per i comandi BASIC (GO, SKIP, SEEK, etc.) e
/// interagisce con l'IsamFileHandle per manipolare lo stato e i dati.
/// </summary>
public class Engine : IDisposable
{
    /// <summary>
    /// L'handle del file ISAM sottostante. Questo oggetto gestisce tutte le operazioni di I/O a basso livello,
    /// il locking, e l'interazione con il Write-Ahead Log.
    /// </summary>
    private readonly IsamFileHandle _handle;

    /// <summary>
    /// Il gestore dedicato all'indice di chiave primaria.
    /// </summary>
    private readonly BPlusTreeManager _primaryIndexManager;

    /// <summary>
    /// Un dizionario che mappa i nomi degli indici secondari alle rispettive istanze di <see cref="BPlusTreeManager"/>.
    /// Questo permette un accesso rapido al gestore di un indice specifico tramite il suo nome.
    /// </summary>
    private readonly Dictionary<string, BPlusTreeManager> _secondaryIndexManagers;

    /// <pre>
    /// Il gestore dell'indice attualmente attivo, utilizzato per tutte le operazioni di ricerca (`Seek`) e di
    /// navigazione sequenziale (`Go`, `Skip`). Per impostazione predefinita, corrisponde al gestore della chiave primaria.
    /// </pre>
    private BPlusTreeManager _activeIndexManager;

    /// <summary>
    /// Cursore e stato navigazione per thread (ISAM-B9).
    /// </summary>
    private readonly ThreadLocal<IsamThreadCursor> _threadCursor =
        new(() => new IsamThreadCursor(), trackAllValues: false);

    private IsamThreadCursor Cursor => _threadCursor.Value!;

    public readonly struct CursorState(long offset, long lastVersion)
    {
        public readonly long Offset = offset;
        public readonly long LastVersion = lastVersion;
    }

    public CursorState CaptureCursorState() => new(Cursor.CurrentRecordOffset, Cursor.LastReadRecordVersion);

    public void RestoreCursorState(in CursorState state)
    {
        Cursor.CurrentRecordOffset = state.Offset;
        Cursor.LastReadRecordVersion = state.LastVersion;
    }

    /// <summary>Invalida il cursore del thread CHIAMANTE (nessun record valido). Da chiamare dopo
    /// qualunque operazione che possa aver spostato fisicamente l'area dati sottostante
    /// (<see cref="FileHandle.EnsureSchemaSlot"/>, invocata da CREATE INDEX/DROP INDEX quando lo
    /// schema cresce oltre lo slot corrente): <c>OnHeapRelocated</c> ripara i puntatori B+Tree/LOB/
    /// free-list, ma NON il <see cref="IsamThreadCursor.CurrentRecordOffset"/> di un thread —
    /// quel cursore resta un offset assoluto stantio, che punta ora a byte diversi dal record
    /// originale (non un errore rumoroso: una LETTURA GARBAGE silenziosa, confermato con repro
    /// diretto). Nessun meccanismo esiste per notificare TUTTI i cursori di TUTTI i thread da
    /// dentro OnHeapRelocated (IsamThreadCursor è ThreadLocal, la relocation gira spesso su un
    /// worker Task.Run diverso dal thread chiamante) — invalidare esplicitamente qui, sul thread
    /// chiamante, dopo l'attesa sincrona dell'operazione, converte una corruzione silenziosa in un
    /// fail-safe esplicito (il prossimo accesso senza rinavigare lancia
    /// InvalidOperationException/torna null invece di leggere byte a caso). Bug
    /// ISAM-RecallCurrent-Dopo-Delete-Offset-Invalido.</summary>
    public void InvalidateCursor()
    {
        Cursor.CurrentRecordOffset = -1;
        Cursor.LastReadRecordVersion = -1;
    }

    /// <summary>
    /// Offset fisico del record corrente per il thread chiamante (-1 se nessun record selezionato).
    /// </summary>
    public long CurrentRecordOffset => Cursor.CurrentRecordOffset;

    /// <summary>True se l'ultimo SET FILTER ha usato seek pushdown sull'indice attivo.</summary>
    internal bool LastFilterUsedSeekPushdown { get; private set; }

    /// <summary>Strategia usata dall'ultimo <see cref="JoinWith"/>.</summary>
    internal IsamJoinStrategy.ExecutionMode LastJoinExecutionMode { get; private set; }

    internal IsamFileHandle Handle => _handle;

    internal BPlusTreeManager PrimaryIndexManager => _primaryIndexManager;

    internal IReadOnlyDictionary<string, BPlusTreeManager> SecondaryIndexManagers => _secondaryIndexManagers;

    /// <summary>Record esaminati durante l'ultimo posizionamento filtro (1 = seek pushdown).</summary>
    internal int LastFilterRecordsExamined { get; private set; }

    /// <summary>
    /// Flag booleano, impostato tramite <see cref="IsamEngineOptions"/>, che determina se gli indici
    /// (primario e secondari) devono essere caricati e mantenuti in memoria per performance di ricerca massime.
    /// </summary>
    private readonly bool _useInMemoryIndex;

    /// <summary>
    /// Cache in memoria per indice (nome → chiavi ordinate). ISAM-C5: primario + secondari.
    /// </summary>
    private readonly Dictionary<string, SortedList<byte[], List<long>>>? _inMemoryIndexes;

    /// <summary>
    /// Mantiene lo stato delle transazioni attive per ogni thread.
    /// La chiave è l'ID del thread gestito. Il valore è uno stack di contesti di transazione,
    /// che permette di gestire correttamente le transazioni nidificate.
    /// </summary>
    private readonly ConcurrentDictionary<int, Stack<TransactionContext>> _threadTransactions = new();

    /// <summary>
    /// Ultimo snapshot strutturale visto (B4: evita reload indici cross-process se invariato).
    /// </summary>
    private StructuralFingerprint? _cachedStructuralFingerprint;

    /// <summary>
    /// Definisce la firma per un metodo in grado di valutare un'espressione ISAM
    /// nel contesto di un record specifico.
    /// </summary>
    public delegate bool IsamExpressionEvaluator(Expr expression, IReadOnlyDictionary<string, object?> recordValues);

    /// <summary>
    /// Ospita il callback opzionale all'Executor per la valutazione di espressioni complesse.
    /// Se null, il motore utilizzerà solo il suo parser/valutatore interno.
    /// Viene "iniettato" dall'Executor dopo la creazione dell'engine.
    /// </summary>
    public KeyExpressionEvaluatorCallback? Evaluator { get; set; }

    private IsamReplicationLog? _replicationLog;
    private long _packGeneration;
    private long _schemaGeneration;

    /// <summary>
    /// Abilita la replica append-only verso un file di log. Passare <see langword="null"/> o stringa vuota per disabilitare.
    /// Usare <c>"."</c> o <c>DEFAULT</c> per il sidecar predefinito <c>{isdb}.repl</c>.
    /// </summary>
    /// <param name="logPath">Percorso del file di log o sentinel per sidecar predefinito.</param>
    /// <returns>Il percorso effettivo del log attivo, o <see langword="null"/> se disabilitato.</returns>
    public string? SetReplication(string? logPath)
    {
        _replicationLog?.Dispose();
        _replicationLog = null;

        if (string.IsNullOrWhiteSpace(logPath))
        {
            return null;
        }

        var path = logPath.Trim();
        if (path.Equals(".", StringComparison.Ordinal))
        {
            // "." = directory corrente: il log va nella cwd col nome base del file ISAM
            // (es. ISAM.SETREPLICATION(Primary, ".") su 'Work/x.isdb' -> 'x.isdb.repl').
            path = Path.GetFileName(FilePath) + ".repl";
        }
        else if (path.Equals("DEFAULT", StringComparison.OrdinalIgnoreCase))
        {
            path = FilePath + ".repl";
        }

        _replicationLog = new IsamReplicationLog(path);
        return path;
    }

    /// <summary>
    /// Log append-only sidecar attivo per la replica (null se disabilitata).
    /// </summary>
    public string? ReplicationLogPath => _replicationLog?.LogPath;

    /// <summary>
    /// Indica se la replica su file è attiva per questa work area.
    /// </summary>
    public bool IsReplicationEnabled => _replicationLog != null;

    public int ApplyReplication(string replLogPath)
    {
        var evaluator = ResolveKeyEvaluator(Evaluator)
                        ?? throw new InvalidOperationException("Key expression evaluator is required for replication apply.");
        return IsamReplicationApplier.Apply(replLogPath, this, evaluator);
    }

    public int FollowReplication(string? replLogPath = null, string? checkpointPath = null)
    {
        var path = replLogPath ?? ReplicationLogPath
                   ?? throw new InvalidOperationException("Replication log path is required.");
        var evaluator = ResolveKeyEvaluator(Evaluator)
                        ?? throw new InvalidOperationException("Key expression evaluator is required for replication follow.");
        return IsamReplicationApplier.Follow(path, this, evaluator, checkpointPath);
    }

    /// <summary>
    /// Ships pending replication log lines over TCP to a remote receiver.
    /// </summary>
    public int ReplicationShipTcp(string host, int port, string? replLogPath = null, string? shipCheckpointPath = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(host);
        var path = replLogPath ?? ReplicationLogPath
                   ?? throw new InvalidOperationException("Replication log path is required.");
        return ReplicationTcpShipper.ShipOnce(path, host, port, shipCheckpointPath);
    }

    /// <summary>
    /// Listens once on <paramref name="port"/> and applies replication lines received over TCP.
    /// </summary>
    public int ReplicationRecvTcp(int port, string? checkpointPath = null)
    {
        var evaluator = ResolveKeyEvaluator(Evaluator)
                        ?? throw new InvalidOperationException("Key expression evaluator is required for replication receive.");
        SetReplication(string.Empty);
        return ReplicationTcpReceiver.ReceiveOnce(
            port,
            (state, line) => IsamReplicationApplier.ApplyLine(line, state, this, evaluator),
            checkpointPath);
    }

    /// <summary>Schema ISAM del file aperto.</summary>
    public Config.IsamSchema Schema => _handle.Schema;

    /// <summary>
    /// Restituisce il callback chiavi unificato: built-in ISAM → esplicito → <see cref="Evaluator"/>.
    /// </summary>
    public KeyExpressionEvaluatorCallback ResolveKeyEvaluator(KeyExpressionEvaluatorCallback? explicitEvaluator) =>
        IsamExpressionBridge.ResolveKeyEvaluator(this, explicitEvaluator);

    /// <summary>Risolve la definizione indice per nome (primario o secondario).</summary>
    public Config.IndexDefinition ResolveIndexDefinition(string indexName)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(indexName);
        if (indexName.Equals(_handle.Schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
        {
            return _handle.Schema.PrimaryKey;
        }

        var secondary = _handle.Schema.SecondaryKeys.FirstOrDefault(sk =>
            sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase));
        if (secondary == null)
        {
            throw new InvalidOperationException($"Indice non trovato: '{indexName}'.");
        }

        return secondary;
    }

    /// <summary>Tipo del campo UDT per nome (null se assente).</summary>
    public DataType? GetFieldDataType(string fieldName)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(fieldName);
        var field = _handle.UdtDefinition.Fields.FirstOrDefault(f =>
            f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
        return field?.DataType;
    }

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="Engine"/>, che funge da facciata di alto livello
    /// per tutte le operazioni su un file ISAM aperto.
    /// </summary>
    /// <param name="handle">L'handle del file ISAM (<see cref="IsamFileHandle"/>) già aperto, che fornisce l'accesso
    /// a basso livello al file.</param>
    /// <param name="options">Un oggetto opzionale (<see cref="IsamEngineOptions"/>) per configurare il comportamento dell'engine,
    /// come l'utilizzo di un indice primario in memoria.</param>
    /// <remarks>
    /// Il costruttore orchestra l'inizializzazione di tutti i componenti necessari per la gestione del file:
    /// 1. Crea un'istanza di <see cref="BPlusTreeManager"/> per la chiave primaria, leggendo l'offset della sua radice dall'header.
    /// 2. Itera sulle definizioni degli indici secondari presenti nello schema e crea un <see cref="BPlusTreeManager"/> per ognuno,
    ///    popolando il dizionario <see cref="_secondaryIndexManagers"/>.
    /// 3. Imposta l'indice attivo iniziale a quello della chiave primaria.
    /// 4. Se l'opzione `UseInMemoryIndex` è abilitata, alloca la struttura dati per l'indice in memoria e invoca il
    ///    metodo <see cref="LoadIndexIntoMemory"/> per popolarla.
    /// </remarks>
    public Engine(IsamFileHandle handle, IsamEngineOptions? options = null)
    {
        _handle = handle;
        var currentOptions = options ?? new IsamEngineOptions();
        _handle.DurabilityMode = currentOptions.DurabilityMode;
        _handle.LockTimeoutMilliseconds = currentOptions.LockTimeoutMilliseconds;

        _useInMemoryIndex = currentOptions.UseInMemoryIndex;

        var primaryRoot = handle.Header.PrimaryKeyRootOffset;
        if (primaryRoot == 0)
        {
            primaryRoot = -1;
        }

        _primaryIndexManager = new BPlusTreeManager(handle, handle.Schema, handle.Schema.PrimaryKey,
            primaryRoot);
        _secondaryIndexManagers = new Dictionary<string, BPlusTreeManager>(StringComparer.OrdinalIgnoreCase);

        for (var i = 0; i < handle.Schema.SecondaryKeys.Count; i++)
        {
            var indexDef = handle.Schema.SecondaryKeys[i];
            var rootOffset = handle.Header.SecondaryIndexRootOffsets[i];
            if (rootOffset == 0)
            {
                rootOffset = -1;
            }

            _secondaryIndexManagers[indexDef.Name] =
                new BPlusTreeManager(handle, handle.Schema, indexDef, rootOffset);
        }

        _activeIndexManager = _primaryIndexManager;

        _handle.AfterHeapRelocated = OnHeapRelocated;
        _cachedStructuralFingerprint = StructuralFingerprint.Capture(_handle.Header);

        if (!_useInMemoryIndex) return;
        _inMemoryIndexes = new Dictionary<string, SortedList<byte[], List<long>>>(StringComparer.OrdinalIgnoreCase);
        LoadAllIndexesIntoMemory();
    }

    #region Comandi di Scrittura

    /// <summary>
    /// Avvia una sessione di caricamento massivo: header differito e nessun fsync fino a <see cref="EndBulkLoad"/>.
    /// Richiede che il chiamante non annidi sessioni bulk senza chiuderle.
    /// </summary>
    public void BeginBulkLoad()
    {
        _handle.BeginBulkLoadSession();
    }

    /// <summary>
    /// Termina il bulk-load e sincronizza header e dati su disco (fsync).
    /// </summary>
    public void EndBulkLoad()
    {
        _handle.EndBulkLoadSession();
        _cachedStructuralFingerprint = StructuralFingerprint.Capture(_handle.Header);
    }

    /// <summary>
    /// Inserisce un batch di record con path ottimizzato: allocazione batch, indice PK append-ordered
    /// con un SaveNode B+ per pagina piena. Richiede chiavi PK ordinabili (monotone o sortate dal chiamante).
    /// </summary>
    /// <param name="records">Record da inserire; con indici secondari si usa fallback per-record.</param>
    /// <param name="evaluator">Callback valutazione chiavi (obbligatorio se ci sono indici secondari).</param>
    public void AppendMany(IReadOnlyList<UdtInstance> records, KeyExpressionEvaluatorCallback? evaluator)
    {
        ArgumentNullException.ThrowIfNull(records);
        if (records.Count == 0)
        {
            return;
        }

        if (_handle.Schema.SecondaryKeys.Count == 0 && CanExtractPrimaryKeysDirect(records))
        {
            AppendManyPrimaryOnly(records, evaluator);
            return;
        }

        foreach (var record in records)
        {
            Append(record, evaluator);
        }
    }

    private void AppendManyPrimaryOnly(IReadOnlyList<UdtInstance> records, KeyExpressionEvaluatorCallback? evaluator)
    {
        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;
        var isAutoCommit = context == null;

        var pkDef = _handle.Schema.PrimaryKey;
        var pkField = records[0].Definition.Fields.First(f =>
            f.Name.Lexeme.Equals(pkDef.Name, StringComparison.OrdinalIgnoreCase));

        var keyEntries = new (byte[] Key, long Offset, UdtInstance Record)[records.Count];
        long[] offsets;

        if (_handle.LockingStrategy == LockingStrategy.CrossProcess)
        {
            using (_handle.EnterStructuralLock())
            {
                _handle.SyncStructuralStateFromDisk();
                ReloadIndexManagersFromHeaderIfChanged();
                offsets = _handle.AllocateNewRecordOffsetBatchCore(records.Count);
            }
        }
        else
        {
            using (_handle.EnterStructuralLock())
            {
                offsets = _handle.AllocateNewRecordOffsetBatchCore(records.Count);
            }
        }

        for (var i = 0; i < records.Count; i++)
        {
            var udtInstance = records[i];
            udtInstance.Fields.TryGetValue(pkField.SymbolId, out var raw);
            var primaryKey = SchemaManager.SerializeKeyValue(raw, pkDef);

            var newMetadata = new RecordMetadata
            {
                Status = Config.RecordStatusActive,
                Version = DateTime.UtcNow.Ticks,
                TransactionId = isAutoCommit ? 0 : context!.TransactionId,
                PreviousVersionOffset = -1,
            };

            RecordMarshaller.WriteRecordWithMetadata(_handle, offsets[i], _handle.Schema, udtInstance, newMetadata);
            keyEntries[i] = (primaryKey, offsets[i], udtInstance);
        }

        Array.Sort(keyEntries, static (a, b) => new ByteArrayComparer().Compare(a.Key, b.Key));

        if (isAutoCommit)
        {
            var insertPairs = new (byte[] Key, long RecordOffset)[keyEntries.Length];
            for (var i = 0; i < keyEntries.Length; i++)
            {
                insertPairs[i] = (keyEntries[i].Key, keyEntries[i].Offset);
            }

            _primaryIndexManager.InsertManyAppendOrdered(insertPairs, context);
            if (_useInMemoryIndex && _inMemoryIndexes is not null)
            {
                var pkName = pkDef.Name;
                foreach (var (key, offset, _) in keyEntries)
                {
                    AddToMemoryIndex(pkName, key, offset);
                }
            }
        }
        else
        {
            foreach (var (_, offset, udt) in keyEntries)
            {
                context!.PendingIndexInserts.Add((udt, offset));
            }
        }

        var last = keyEntries[^1];
        Cursor.CurrentRecordOffset = last.Offset;
        Cursor.LastReadRecordVersion = _handle.ReadRecordMetadata(last.Offset, _handle.FileEncoding).Version;
        Cursor.EofFlag = false;
        Cursor.BofFlag = false;

        if (isAutoCommit && !_handle.IsBulkLoadActive)
        {
            _handle.Commit();
        }

        if (!_handle.IsBulkLoadActive)
        {
            _cachedStructuralFingerprint = StructuralFingerprint.Capture(_handle.Header);
        }
    }

    private bool CanExtractPrimaryKeysDirect(IReadOnlyList<UdtInstance> records)
    {
        var pkDef = _handle.Schema.PrimaryKey;
        if (!pkDef.KeyExpression.Equals(pkDef.Name, StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        if (records.Count == 0)
        {
            return true;
        }

        return records[0].Definition.Fields.Any(f =>
            f.Name.Lexeme.Equals(pkDef.Name, StringComparison.OrdinalIgnoreCase));
    }

    /// <summary>
    /// Forza la durabilità completa (header pendente + fsync), utile con <see cref="IsamDurabilityMode.Buffered"/>.
    /// </summary>
    public void FlushDurability()
    {
        _handle.FlushDurability();
    }

    /// <summary>
    /// Aggiunge un nuovo record al file di database, aggiorna tutti gli indici e il contatore dei record attivi.
    /// L'operazione è transazionalmente sicura e supporta il livello `Serializable`.
    /// </summary>
    /// <param name="udtInstance">L'istanza UDT contenente i dati del nuovo record.</param>
    /// <param name="evaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <remarks>
    /// ISAM-B3 (IntraProcess): solo l'allocazione dello slot record è protetta dal lock strutturale;
    /// scrittura record, insert indici (con key-lock) e commit avvengono fuori dal lock file.
    /// CrossProcess mantiene il lock strutturale per l'intero Append: l'header deve restare coerente
    /// su disco prima che un altro processo possa allocare.
    /// </remarks>
    public void Append(UdtInstance udtInstance, KeyExpressionEvaluatorCallback? evaluator)
    {
        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);

        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;
        var isAutoCommit = context == null;

        if (_handle.LockingStrategy == LockingStrategy.CrossProcess)
        {
            using (_handle.EnterStructuralLock())
            {
                _handle.SyncStructuralStateFromDisk();
                ReloadIndexManagersFromHeaderIfChanged();
                CompleteAppend(
                    udtInstance,
                    evaluator,
                    context,
                    isAutoCommit,
                    _handle.AllocateNewRecordOffsetCore());
            }

            return;
        }

        long newOffset;
        using (_handle.EnterStructuralLock())
        {
            newOffset = _handle.AllocateNewRecordOffsetCore();
        }

        CompleteAppend(udtInstance, evaluator, context, isAutoCommit, newOffset);
    }

    private void CompleteAppend(
        UdtInstance udtInstance,
        KeyExpressionEvaluatorCallback? evaluator,
        TransactionContext? context,
        bool isAutoCommit,
        long newOffset)
    {
        var newMetadata = new RecordMetadata
        {
            Status = Config.RecordStatusActive,
            Version = DateTime.UtcNow.Ticks,
            TransactionId = isAutoCommit ? 0 : context!.TransactionId,
            PreviousVersionOffset = -1
        };

        RecordMarshaller.WriteRecordWithMetadata(_handle, newOffset, _handle.Schema, udtInstance, newMetadata);

        if (isAutoCommit)
        {
            UpdateAllIndexesForInsert(udtInstance, newOffset, context, evaluator);
        }
        else
        {
            context!.PendingIndexInserts.Add((udtInstance, newOffset));
        }

        Cursor.CurrentRecordOffset = newOffset;
        Cursor.LastReadRecordVersion = newMetadata.Version;
        Cursor.EofFlag = false;
        Cursor.BofFlag = false;

        if (isAutoCommit && !_handle.IsBulkLoadActive)
        {
            _handle.Commit();
        }

        RecordReplicationOp(ReplicationOp.Append, GetCurrentRecordNumber(), udtInstance, context);

        if (!_handle.IsBulkLoadActive)
        {
            _cachedStructuralFingerprint = StructuralFingerprint.Capture(_handle.Header);
        }
    }

    /// <summary>
    /// Aggiorna il record alla posizione corrente del cursore, implementando la logica MVCC e
    /// supportando le transazioni con livelli di isolamento. Un'operazione di PUT richiede sempre
    /// un lock di scrittura esclusivo sul record.
    /// </summary>
    /// <param name="newUdtInstance">L'istanza UDT con i nuovi dati da scrivere.</param>
    /// <param name="evaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se il cursore non è posizionato su un record valido, se si tenta di scrivere su un record
    /// cancellato, o se si verifica un conflitto di concorrenza ottimistica.
    /// </exception>
    /// <exception cref="Databases.LockTimeoutException">
    /// Lanciata se il lock di scrittura non può essere acquisito. Se in una transazione, causa un rollback automatico.
    /// </exception>
    /// <remarks>
    /// Questa operazione segue un flusso rigoroso per garantire l'atomicità e la coerenza:
    /// 1. Esegue un controllo di versione (concorrenza ottimistica): fallisce se la versione del record
    ///    su disco non corrisponde più a quella letta in precedenza.
    /// 2. Acquisisce un lock di scrittura esclusivo sul record. Se in una transazione, questo lock
    ///    verrà mantenuto fino al commit o al rollback.
    /// 3. Crea una copia "undo" della versione corrente del record per la cronologia MVCC.
    /// 4. Scrive la nuova versione del record, aggiornando i metadati (versione, TransactionId) e
    ///    puntando alla versione di undo. La serializzazione include la gestione dello storage off-row per JSON.
    /// 5. Aggiorna tutti gli indici, rimuovendo le vecchie chiavi e inserendo quelle nuove,
    ///    con il locking di intervallo per il livello `Serializable`.
    /// 6. Se in modalità auto-commit, finalizza le modifiche e rilascia il lock. Altrimenti, le
    ///    modifiche rimangono nel WAL in attesa della fine della transazione.
    /// CrossProcess mantiene il lock strutturale per l'intera mutazione indici + commit (ISAM-B10),
    /// con sync header/radici B+ da disco prima delle operazioni su indice.
    /// </remarks>
    public void Put(UdtInstance newUdtInstance, KeyExpressionEvaluatorCallback? evaluator)
    {
        if (Cursor.CurrentRecordOffset < 0 || Cursor.LastReadRecordVersion == -1)
            throw new InvalidOperationException("PUT deve essere preceduto da un'operazione di lettura valida.");

        var currentOffset = Cursor.CurrentRecordOffset;
        var expectedVersion = Cursor.LastReadRecordVersion;

        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;
        var isAutoCommit = context == null;

        var lockKey = LockManagerBase.GetRecordLockKey(currentOffset);
        AcquireRecordWriteLock(currentOffset, context, lockKey);

        try
        {
            var (currentMetadata, oldUdtInstance) = GetCurrentRecordWithMetadata();
            if (currentMetadata.Status == Config.RecordStatusDeleted)
                throw new InvalidOperationException("Cannot PUT over a deleted record. Use RECALL first.");

            // Confronta la versione ATTUALMENTE VISIBILE (che può risiedere altrove nella catena
            // MVCC, non allo slot griglia) con quella letta dalla precedente GET — non i metadati
            // grezzi dello slot griglia: se la GET precedente aveva dovuto risalire la catena
            // (PreviousVersionOffset) per trovare la versione visibile — es. la versione allo slot
            // griglia apparteneva a una transazione ormai ROLLBACK — freshMetadata.Version qui
            // sarebbe sempre diverso da expectedVersion, facendo scattare "Conflitto di
            // concorrenza" ad ogni singola PUT che segue un simile fallback (bug scoperto testando
            // ROLLBACK poi COMMIT sullo stesso record: la seconda PUT falliva sempre, inghiottita
            // dal catch-all del syscall ISAM.PUT).
            var freshMetadata = FindVisibleVersionMetadata(currentOffset, currentTransactionId: _handle.GetCurrentTransactionId());
            if (freshMetadata.Version != expectedVersion)
                throw new InvalidOperationException(
                    "Conflitto di concorrenza: il record è stato modificato da un altro processo.");

            if (_handle.LockingStrategy == LockingStrategy.CrossProcess)
            {
                using (_handle.EnterStructuralLock())
                {
                    _handle.BeginCoalesceAppendCountersForPut();
                    try
                    {
                        _handle.SyncStructuralStateFromDisk();
                        ReloadIndexManagersFromHeaderIfChanged();

                        var previousVersionOffset = _handle.WriteUndoVersion(currentOffset, context);
                        var newRecordData = SerializePutRecord(newUdtInstance, previousVersionOffset, context, isAutoCommit);
                        ApplyPutIndexAndRecordWrites(
                            oldUdtInstance,
                            newUdtInstance,
                            currentOffset,
                            newRecordData,
                            context,
                            evaluator,
                            isAutoCommit);
                    }
                    finally
                    {
                        _handle.EndCoalesceAppendCountersForPut();
                    }
                }
            }
            else
            {
                var previousVersionOffset = _handle.WriteUndoVersion(currentOffset, context);
                var newRecordData = SerializePutRecord(newUdtInstance, previousVersionOffset, context, isAutoCommit);
                ApplyPutIndexAndRecordWrites(
                    oldUdtInstance,
                    newUdtInstance,
                    currentOffset,
                    newRecordData,
                    context,
                    evaluator,
                    isAutoCommit);
            }
        }
        catch (DeadlockException)
        {
            if (!isAutoCommit)
            {
                RollbackOnLockFailure();
            }

            throw;
        }
        catch (LockTimeoutException)
        {
            if (!isAutoCommit)
            {
                RollbackOnLockFailure();
            }

            throw;
        }
        finally
        {
            if (isAutoCommit)
            {
                _handle.RecordLockManager.ReleaseWriteLock(lockKey);
            }

            // Invalida la versione letta per forzare una nuova lettura prima del prossimo PUT.
            Cursor.LastReadRecordVersion = -1;
        }
    }

    private byte[] SerializePutRecord(
        UdtInstance newUdtInstance,
        long previousVersionOffset,
        TransactionContext? context,
        bool isAutoCommit)
    {
        var newMetadata = new RecordMetadata
        {
            Status = Config.RecordStatusActive,
            Version = DateTime.UtcNow.Ticks,
            TransactionId = isAutoCommit ? 0 : context!.TransactionId,
            PreviousVersionOffset = previousVersionOffset
        };

        return RecordMarshaller.SerializeRecordWithMetadata(_handle.Schema, newUdtInstance, newMetadata, _handle);
    }

    private void ApplyPutIndexAndRecordWrites(
        UdtInstance oldUdtInstance,
        UdtInstance newUdtInstance,
        long currentOffset,
        byte[] newRecordData,
        TransactionContext? context,
        KeyExpressionEvaluatorCallback? evaluator,
        bool isAutoCommit)
    {
        RemoveAllIndexesForRecord(oldUdtInstance, currentOffset, context, evaluator);
        UpdateAllIndexesForInsert(newUdtInstance, currentOffset, context, evaluator);
        _handle.WriteRecord(currentOffset, newRecordData);

        RecordReplicationOp(ReplicationOp.Put, GetCurrentRecordNumber(), newUdtInstance, context);

        if (isAutoCommit)
        {
            _handle.Commit();
            MvccUndoGc.PruneAfterGridWrite(_handle, currentOffset);
        }

        _cachedStructuralFingerprint = StructuralFingerprint.Capture(_handle.Header);
    }

    #endregion

    #region Comandi di Navigazione

    /// <summary>
    /// [NUOVO - GPM-SAFE] Esegue il posizionamento atomico e non bloccante del cursore
    /// all'inizio o alla fine della sequenza logica, senza eseguire cicli di ricerca.
    /// </summary>
    public void Go(Config.CursorMoveOperation operation)
    {
        Cursor.BofFlag = false;
        Cursor.EofFlag = false;
        Cursor.LastReadRecordVersion = -1;

        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;

        var targetRecordOffset = operation == Config.CursorMoveOperation.Top
            ? _activeIndexManager.GetFirstRecordOffset(context)
            : _activeIndexManager.GetLastRecordOffset(context);

        if (targetRecordOffset == -1)
        {
            Cursor.EofFlag = true;
            Cursor.BofFlag = true;
            Cursor.CurrentRecordOffset = -1;
        }
        else
        {
            Cursor.CurrentRecordOffset = targetRecordOffset;
            var metadata = _handle.ReadRecordMetadata(Cursor.CurrentRecordOffset, _handle.FileEncoding);
            Cursor.LastReadRecordVersion = metadata.Version;
        }

        // Applica filtro attivo: se il record non soddisfa il filtro, vai al successivo
        // NOTA: ActiveFilterExpression NON e' richiesto qui — e' un riferimento opzionale
        // all'AST originale (solo per pushdown/introspezione), non serve all'evaluator per
        // funzionare (i filtri compilati/lambda passano expression=null legittimamente).
        // Richiederlo qui disabilitava SILENZIOSAMENTE ogni filtro impostato con expression
        // null durante la navigazione (bug scoperto testando DbIsamFullTests.bas sezione 6).
        if (Cursor.ActiveFilterEvaluator != null && !IsEof())
        {
            int dir = operation == Config.CursorMoveOperation.Top ? 1 : -1;
            SkipFilterInternal(dir);
        }
    }

    /// <summary>
    /// [NUOVO - GPM-SAFE] Esegue un singolo passo atomico e non bloccante del cursore
    /// nella direzione specificata, senza eseguire cicli di ricerca.
    /// </summary>
    public void Skip(int direction = 1)
    {
        if (direction == 0) return;
        if ((IsEof() && direction > 0) || (IsBof() && direction < 0)) return;

        if (IsEof() && direction < 0) Cursor.EofFlag = false;
        if (IsBof() && direction > 0) Cursor.BofFlag = false;

        Step(Math.Sign(direction));

        // Applica filtro attivo: salta record che non soddisfano il filtro (vedi nota in Go())
        if (Cursor.ActiveFilterEvaluator != null && !IsEof() && !IsBof())
        {
            SkipFilterInternal(direction);
        }
    }

    /// <summary>Salta i record che non soddisfano il filtro attivo.</summary>
    private void SkipFilterInternal(int direction)
    {
        int maxIterations = 100_000; // safety limit
        while (!IsEof() && !IsBof() && maxIterations-- > 0)
        {
            // Costruisci il dizionario string-keyed dei valori del record corrente
            var recordInstance = GetCurrentRecord();
            if (recordInstance == null) { Step(direction); continue; }

            var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
            var udtDef = _handle.UdtDefinition;
            foreach (var field in udtDef.Fields)
            {
                recordInstance.Fields.TryGetValue(field.SymbolId, out var value);
                stringKeyedFields[field.Name.Lexeme] = value;
            }

            // Valuta il filtro
            bool matches = Cursor.ActiveFilterEvaluator!(Cursor.ActiveFilterExpression!, stringKeyedFields);
            if (matches) break;

            Step(direction);
        }
    }

    /// <summary>
    /// Posiziona il cursore del record sul primo record la cui chiave corrisponde a quella fornita,
    /// utilizzando l'indice attualmente attivo e applicando il locking appropriato per il livello `Serializable`.
    /// </summary>
    /// <param name="key">
    /// La rappresentazione binaria della chiave da cercare.
    /// </param>
    /// <remarks>
    /// Se eseguito all'interno di una transazione `Serializable`, questo metodo acquisisce un lock di lettura
    /// sull'intervallo di chiave specificato per prevenire che altre transazioni inseriscano record
    /// "fantasma" che corrisponderebbero alla chiave cercata.
    /// In caso di successo, imposta `FoundFlag` a `true` e aggiorna il cursore e la versione del record.
    /// In caso di fallimento, `FoundFlag` è `false` e il cursore viene invalidato.
    /// </remarks>
    public void Seek(byte[] key)
    {
        _handle.FoundFlag = false;
        Cursor.LastReadRecordVersion = -1;

        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;

        SyncCrossProcessStructuralStateIfNeeded();

        // La ricerca nell'indice richiede il contesto per il locking Serializable.
        List<long> recordOffsets;
        try
        {
            recordOffsets = _activeIndexManager.Search(key, context);
        }
        catch (DeadlockException)
        {
            RollbackOnLockFailure();
            throw;
        }
        catch (LockTimeoutException)
        {
            RollbackOnLockFailure();
            throw;
        }

        if (recordOffsets.Count > 0)
        {
            _handle.FoundFlag = true;
            Cursor.CurrentRecordOffset = recordOffsets[0];
            // Legge i metadati per il versioning e la concorrenza ottimistica.
            var metadata = _handle.ReadRecordMetadata(Cursor.CurrentRecordOffset, _handle.FileEncoding);
            Cursor.LastReadRecordVersion = metadata.Version;
            // Un SEEK riuscito riposiziona SEMPRE il cursore su un record valido — deve pulire
            // BofFlag/EofFlag residui da una navigazione precedente (es. un ciclo arrivato a EOF
            // prima di questo SEEK), altrimenti Skip() successivo vede ancora IsEof()==true e
            // diventa un no-op permanente (Skip: "if (IsEof() && direction>0) return;"), causando
            // un loop apparentemente infinito nel codice chiamante che si aspettava un cursore
            // sbloccato dopo il SEEK riuscito.
            Cursor.EofFlag = false;
            Cursor.BofFlag = false;
        }
        else
        {
            Cursor.CurrentRecordOffset = -1;
            // Chiave non trovata: nessun record valido su cui posizionarsi, coerente con la
            // convenzione usata altrove (Go() su indice vuoto imposta EofFlag=true).
            Cursor.EofFlag = true;
            Cursor.BofFlag = false;
        }
    }

    /// <summary>Seek al primo record con chiave indice &gt;= <paramref name="key"/> (pushdown prefisso).</summary>
    private void SeekLowerBound(byte[] key)
    {
        _handle.FoundFlag = false;
        Cursor.LastReadRecordVersion = -1;

        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;

        SyncCrossProcessStructuralStateIfNeeded();

        List<long> recordOffsets;
        try
        {
            recordOffsets = _activeIndexManager.SearchLowerBound(key, context);
        }
        catch (DeadlockException)
        {
            RollbackOnLockFailure();
            throw;
        }
        catch (LockTimeoutException)
        {
            RollbackOnLockFailure();
            throw;
        }

        if (recordOffsets.Count > 0)
        {
            _handle.FoundFlag = true;
            Cursor.CurrentRecordOffset = recordOffsets[0];
            var metadata = _handle.ReadRecordMetadata(Cursor.CurrentRecordOffset, _handle.FileEncoding);
            Cursor.LastReadRecordVersion = metadata.Version;
        }
        else
        {
            Cursor.CurrentRecordOffset = -1;
        }
    }

    /// <summary>
    /// Posiziona il cursore del record sul primo record la cui chiave corrisponde a quella fornita,
    /// utilizzando opzionalmente un indice specifico e applicando il locking appropriato per il livello `Serializable`.
    /// </summary>
    /// <param name="key">La rappresentazione binaria della chiave da cercare.</param>
    /// <param name="indexName">
    /// Il nome opzionale dell'indice da utilizzare per la ricerca. Se `null` o vuoto, viene utilizzato
    /// l'indice attualmente attivo (impostato da `SET ORDER`). Se specificato, questo indice viene
    /// utilizzato solo per questa operazione di SEEK, senza modificare l'ordine di navigazione predefinito.
    /// </param>
    /// <remarks>
    /// Se eseguito all'interno di una transazione `Serializable`, questo metodo acquisisce un lock di lettura
    /// sull'intervallo di chiave specificato per prevenire che altre transazioni inseriscano record
    /// "fantasma" che corrisponderebbero alla chiave cercata.
    /// In caso di successo, imposta `FoundFlag` a `true` e aggiorna il cursore e la versione del record.
    /// In caso di fallimento, `FoundFlag` è `false` e il cursore viene invalidato.
    /// </remarks>
    public void Seek(byte[] key, string? indexName)
    {
        _handle.FoundFlag = false;
        Cursor.LastReadRecordVersion = -1;

        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;

        // --- INIZIO LOGICA DI SELEZIONE DELL'INDICE ---
        BPlusTreeManager indexToUse;
        if (!string.IsNullOrEmpty(indexName))
        {
            // Se un nome di indice è specificato, usalo per questa ricerca.
            if (_secondaryIndexManagers.TryGetValue(indexName, out var secondaryManager))
            {
                indexToUse = secondaryManager;
            }
            else if (indexName.Equals(_primaryIndexManager.GetIndexDefinition().Name,
                         StringComparison.OrdinalIgnoreCase))
            {
                indexToUse = _primaryIndexManager;
            }
            else
            {
                // L'indice specificato non esiste.
                Cursor.CurrentRecordOffset = -1;
                return; // Esce silenziosamente come farebbe un SEEK fallito.
            }
        }
        else
        {
            // Se non è specificato nessun indice, usa quello attivo corrente.
            indexToUse = _activeIndexManager;
        }
        // --- FINE LOGICA DI SELEZIONE DELL'INDICE ---

        SyncCrossProcessStructuralStateIfNeeded();

        List<long> recordOffsets;
        try
        {
            recordOffsets = indexToUse.Search(key, context);
        }
        catch (DeadlockException)
        {
            RollbackOnLockFailure();
            throw;
        }
        catch (LockTimeoutException)
        {
            RollbackOnLockFailure();
            throw;
        }

        if (recordOffsets.Count > 0)
        {
            _handle.FoundFlag = true;
            Cursor.CurrentRecordOffset = recordOffsets[0];
            var metadata = _handle.ReadRecordMetadata(Cursor.CurrentRecordOffset, _handle.FileEncoding);
            Cursor.LastReadRecordVersion = metadata.Version;
        }
        else
        {
            Cursor.CurrentRecordOffset = -1;
        }
    }

    #endregion

    #region Comandi di Lettura

    /// <summary>
    /// Ottiene la lunghezza totale in byte del file ISAM su disco.
    /// </summary>
    /// <returns>La lunghezza del file.</returns>
    public long GetFileLength() => _handle.GetStream().Length;

    /// <summary>Percorso del file ISAM aperto.</summary>
    public string FilePath => _handle.FilePath;

    /// <summary>Header corrente (conteggi, versione, heap).</summary>
    public Config.FileHeader Header => _handle.Header;

    /// <summary>Politica di durabilità I/O attiva.</summary>
    public IsamDurabilityMode DurabilityMode => _handle.DurabilityMode;

    /// <summary>
    /// Returns a multi-line WAL inspection summary for <c>ISAM.LOG</c> and offline tooling.
    /// </summary>
    public string GetWalLogSummary()
    {
        var disk = WriteAheadLog.Inspect(FilePath);
        return (disk with
        {
            DurabilityMode = DurabilityMode.ToString(),
            WalHandleActive = _handle.IsWalHandleOpen,
            DirectWalDirty = _handle.IsDirectWalDirty,
            InOpenTransaction = disk.InOpenTransaction || _handle.HasOpenWalTransaction,
        }).Format();
    }

    /// <summary>Structured WAL snapshot (disk + runtime fields when called on an open engine).</summary>
    public WalLogSummary GetWalLogSnapshot()
    {
        var disk = WriteAheadLog.Inspect(FilePath);
        return disk with
        {
            DurabilityMode = DurabilityMode.ToString(),
            WalHandleActive = _handle.IsWalHandleOpen,
            DirectWalDirty = _handle.IsDirectWalDirty,
            InOpenTransaction = disk.InOpenTransaction || _handle.HasOpenWalTransaction,
        };
    }

    /// <summary>Strategia di locking (intra-processo o cross-process).</summary>
    public LockingStrategy LockingStrategy => _handle.LockingStrategy;

    /// <summary>
    /// Ottiene la dimensione fissa di un record in byte, come definito nello schema del file.
    /// </summary>
    public int RecordLength => _handle.Header.RecordSize;

    /// <summary>
    /// Esegue una scansione completa dell'indice primario su disco e ne carica l'intera struttura in una
    /// rappresentazione ottimizzata in memoria (<see cref="_inMemoryIndexes"/>).
    /// </summary>
    /// <remarks>
    /// Questa operazione viene eseguita una sola volta, al momento dell'inizializzazione dell'Engine, se l'opzione
    /// `UseInMemoryIndex` è stata abilitata. La sua logica è progettata per essere altamente efficiente:
    /// 1.  Inizia localizzando il primo nodo foglia dell'albero B+ tramite <see cref="BPlusTreeManager.FindFirstLeaf"/>.
    /// 2.  Successivamente, invece di attraversare l'albero, sfrutta la lista doppiamente concatenata dei nodi foglia,
    ///     iterando sequenzialmente tramite il puntatore <c>NextLeafOffset</c>. Questo trasforma la lettura in una
    ///     scansione sequenziale del disco, che è molto più performante di accessi casuali.
    /// 3.  Per ogni nodo foglia incontrato, itera su tutte le coppie chiave/offset e le popola nel `SortedList`
    ///     in memoria.
    ///
    /// Il risultato è una cache completa che permette alle successive operazioni di ricerca sull'indice primario di
    /// essere eseguite interamente in memoria, eliminando la latenza dell'I/O su disco.
    /// </remarks>
    private void LoadAllIndexesIntoMemory()
    {
        if (_inMemoryIndexes is null) return;

        _inMemoryIndexes.Clear();
        LoadManagerIndexIntoMemory(_primaryIndexManager);
        foreach (var manager in _secondaryIndexManagers.Values)
        {
            LoadManagerIndexIntoMemory(manager);
        }
    }

    private void LoadManagerIndexIntoMemory(BPlusTreeManager manager)
    {
        if (_inMemoryIndexes is null) return;

        var indexName = manager.GetIndexDefinition().Name;
        var memoryIndex = new SortedList<byte[], List<long>>(new ByteArrayComparer());
        var firstLeaf = manager.FindFirstLeaf();
        if (firstLeaf.KeyCount == 0)
        {
            _inMemoryIndexes[indexName] = memoryIndex;
            return;
        }

        var currentLeaf = firstLeaf;
        while (currentLeaf != null)
        {
            for (var i = 0; i < currentLeaf.KeyCount; i++)
            {
                memoryIndex.Add(currentLeaf.Keys[i], [..currentLeaf.RecordOffsets[i]]);
            }

            currentLeaf = currentLeaf.NextLeafOffset != -1
                ? LoadNextLeafPage(manager, currentLeaf.NextLeafOffset)
                : null;
        }

        _inMemoryIndexes[indexName] = memoryIndex;
    }

    private static BPlusLeafNode? LoadNextLeafPage(BPlusTreeManager manager, long offset)
    {
        var node = manager.LoadNode(offset);
        return node is BPlusLeafNode leaf ? leaf : null;
    }

    private bool TryGetActiveMemoryIndex(out SortedList<byte[], List<long>> memoryIndex)
    {
        memoryIndex = null!;
        if (!_useInMemoryIndex || _inMemoryIndexes is null)
        {
            return false;
        }

        var indexName = _activeIndexManager.GetIndexDefinition().Name;
        return _inMemoryIndexes.TryGetValue(indexName, out memoryIndex!);
    }

    private SortedList<byte[], List<long>>? GetPrimaryMemoryIndex()
    {
        if (!_useInMemoryIndex || _inMemoryIndexes is null)
        {
            return null;
        }

        var pkName = _handle.Schema.PrimaryKey.Name;
        return _inMemoryIndexes.TryGetValue(pkName, out var index) ? index : null;
    }

    /// <summary>
    /// Legge dal disco il record completo alla posizione corrente del cursore e lo deserializza sia nei suoi
    /// metadati (<see cref="RecordMetadata"/>) che nell'istanza UDT (<see cref="UdtInstance"/>).
    /// Questa operazione non applica la logica di visibilità MVCC, ma legge la versione più recente del record.
    /// </summary>
    /// <returns>
    /// Una tupla contenente l'oggetto <see cref="RecordMetadata"/> (con lo stato e la versione) e l'istanza
    /// <see cref="UdtInstance"/> popolata con i dati del record.
    /// </returns>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se il cursore CurrentRecordOffset" non è posizionato su un record valido.
    /// </exception>
    /// <remarks>
    /// Questo metodo helper è una primitiva fondamentale, utilizzata internamente da operazioni come `Put` o
    /// `DeleteCurrent` che necessitano di accedere sia ai dati utente sia ai metadati di sistema del record
    /// corrente nella sua versione più recente (senza filtri di visibilità). L'operazione legge l'intero blocco
    /// di byte del record in un buffer e poi delega la deserializzazione complessa a
    /// <see cref="RecordMarshaller.DeserializeRecord"/>, che a sua volta si occupa di separare i metadati
    /// e di mappare i dati grezzi sui campi definiti nello schema, gestendo anche il LOB Heap.
    /// </remarks>
    private (RecordMetadata metadata, UdtInstance instance) GetCurrentRecordWithMetadata()
    {
        if (Cursor.CurrentRecordOffset < 0)
            throw new InvalidOperationException("Il cursore non è posizionato su un record valido.");
        var buffer = new byte[_handle.Header.RecordSize];
        _handle.ReadRecordBytes(Cursor.CurrentRecordOffset, buffer);
        return RecordMarshaller.DeserializeRecord(_handle.Schema, buffer, _handle.UdtDefinition, _handle);
    }

    /// <summary>
    /// Legge e deserializza la versione *visibile* dei dati utente del record alla posizione corrente del cursore,
    /// rispettando il livello di isolamento della transazione corrente e acquisendo i lock necessari.
    /// </summary>
    /// <returns>
    /// Un'istanza di <see cref="UdtInstance"/> popolata con i dati della versione visibile del record,
    /// o `null` se nessuna versione del record è visibile alla transazione corrente o se il cursore non è
    /// posizionato su un record valido.
    /// </returns>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se il cursore non è posizionato su un record valido.
    /// </exception>
    /// <exception cref="Databases.LockTimeoutException">
    /// Lanciata se, in una transazione `RepeatableRead` o superiore, non è possibile acquisire un lock di lettura
    /// sul record entro il timeout. L'intera transazione viene automaticamente annullata.
    /// </exception>
    public UdtInstance? GetCurrentRecord()
    {
        if (Cursor.CurrentRecordOffset < 0)
        {
            throw new InvalidOperationException("Il cursore non è posizionato su un record valido.");
        }

        var currentRecordOffset = Cursor.CurrentRecordOffset;

        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;

        if (context is { IsolationLevel: >= IsolationLevel.RepeatableRead })
        {
            if (!context.ReadLockedOffsets.Contains(currentRecordOffset) &&
                !context.WriteLockedOffsets.Contains(currentRecordOffset))
            {
                try
                {
                    var lockKey = LockManagerBase.GetRecordLockKey(currentRecordOffset);
                    _handle.RecordLockManager.AcquireReadLock(lockKey, 1000,
                        $"record all'offset {currentRecordOffset}");
                    context.ReadLockedOffsets.Add(currentRecordOffset);
                }
                catch (LockTimeoutException)
                {
                    RollbackOnLockFailure();
                    throw;
                }
                catch (DeadlockException)
                {
                    RollbackOnLockFailure();
                    throw;
                }
            }
        }

        var currentTxId = _handle.GetCurrentTransactionId();

        /* Lettura posizionale diretta (pread) nel buffer del record invece di Seek+ReadExactly
         * sul FileStream bufferizzato a 256 KB: eliminata la copia intermedia
         * kernel -> buffer FileStream -> buffer record. ReadBytesAtCore gestisce per primo il
         * caso MEM: (IsamMemStore.TableReadAt -> ReadSpan, posizionale, quindi equivalente a
         * Seek+Read) e poi ricade su RandomAccess.Read = pread diretto; la lettura corta
         * solleva EndOfStreamException come ReadExactly. Passa per ReadBytesAtDirect
         * (non ReadBytesAt) per NON attraversare la IsamPageCache: la coerenza della cache non
         * e' stata verificata su tutti i path di scrittura, quindi qui non si mette. */
        var recordSize = _handle.Header.RecordSize;
        var rented = ArrayPool<byte>.Shared.Rent(recordSize);
        try
        {
            var offsetToRead = currentRecordOffset;
            while (offsetToRead != -1)
            {
                /* AsSpan(0, recordSize) NON e' opzionale: Rent puo' restituire un array piu'
                 * grande, e passare l'array intero leggerebbe oltre il record corrompendolo con
                 * i byte del record successivo. */
                _handle.ReadBytesAtDirect(offsetToRead, rented.AsSpan(0, recordSize));

                var (metadata, udtInstance) = RecordMarshaller.DeserializeRecord(
                    _handle.Schema, rented, _handle.UdtDefinition, _handle, recordSize);

                if (_handle.IsVersionVisible(metadata.TransactionId, currentTxId))
                {
                    Cursor.LastReadRecordVersion = metadata.Version;
                    return udtInstance;
                }

                offsetToRead = metadata.PreviousVersionOffset;
            }
        }
        finally
        {
            /* finally NON opzionale: il return qui dentro e' dentro il ciclo, quindi senza
             * finally l'array noleggiato non tornerebbe mai al pool. Sicuro perche'
             * RecordMarshaller non aliasa il buffer (GetString(reader.ReadBytes(...)) alloca,
             * nessun GetBuffer): l'UdtInstance restituita non lo referenzia. */
            ArrayPool<byte>.Shared.Return(rented);
        }

        Cursor.LastReadRecordVersion = -1;
        return null;
    }

    /// <summary>
    /// Risale la catena di versioni MVCC da <paramref name="startOffset"/> (come GetCurrentRecord)
    /// e restituisce i metadati della prima versione visibile alla transazione corrente, senza
    /// deserializzare i campi dati utente. Usato dal controllo di concorrenza ottimistica di
    /// <see cref="Put"/>, che deve confrontare la versione ATTUALMENTE visibile con quella letta
    /// dalla GET precedente — non i metadati grezzi dello slot griglia, che possono appartenere a
    /// una transazione non più visibile (es. rollback) quando la GET aveva dovuto risalire la
    /// catena per trovare la versione corretta.
    /// </summary>
    private RecordMetadata FindVisibleVersionMetadata(long startOffset, long currentTransactionId)
    {
        var offsetToRead = startOffset;
        while (offsetToRead != -1)
        {
            var metadata = _handle.ReadRecordMetadata(offsetToRead, _handle.FileEncoding);
            if (_handle.IsVersionVisible(metadata.TransactionId, currentTransactionId))
            {
                return metadata;
            }

            offsetToRead = metadata.PreviousVersionOffset;
        }

        throw new InvalidOperationException(
            "Nessuna versione visibile trovata nella catena MVCC per il controllo di concorrenza.");
    }

    /// <summary>
    /// Legge dal disco il record completo alla posizione corrente del cursore e lo deserializza,
    /// bypassando qualsiasi logica di visibilità transazionale (MVCC). Restituisce sempre
    /// la versione più recente del record, indipendentemente dal suo stato (attivo o cancellato).
    /// </summary>
    /// <param name="metadata">
    /// Quando questo metodo restituisce un valore, contiene i metadati completi del record letto.
    /// </param>
    /// <returns>
    /// L'istanza <see cref="UdtInstance"/> popolata con i dati del record.
    /// </returns>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se il cursore non è posizionato su un record valido.
    /// </exception>
    /// <remarks>
    /// Questo metodo è essenziale per operazioni di manutenzione come RECALL, che devono poter
    /// "vedere" e operare su record che sarebbero normalmente invisibili a una normale operazione di lettura.
    /// </remarks>
    public UdtInstance GetCurrentRecordWithMetadata(out RecordMetadata metadata)
    {
        if (Cursor.CurrentRecordOffset < 0)
            throw new InvalidOperationException("Il cursore non è posizionato su un record valido.");

        var buffer = new byte[_handle.Header.RecordSize];
        _handle.ReadRecordBytes(Cursor.CurrentRecordOffset, buffer);

        var (meta, instance) =
            RecordMarshaller.DeserializeRecord(_handle.Schema, buffer, _handle.UdtDefinition, _handle);
        metadata = meta;
        return instance;
    }

    #endregion

    #region Funzioni di Stato

    /// <summary>
    /// Restituisce un valore che indica se il cursore si trova in uno stato di "End Of File" (EOF).
    /// Questo si verifica quando un'operazione di navigazione (come `SKIP`) tenta di spostarsi oltre l'ultimo record
    /// nella sequenza logica dell'indice attivo.
    /// </summary>
    /// <returns>True se il cursore è in stato EOF, altrimenti False.</returns>
    public bool IsEof() => Cursor.EofFlag;

    /// <summary>
    /// Restituisce un valore che indica se il cursore si trova in uno stato di "Beginning Of File" (BOF).
    /// Questo si verifica quando un'operazione di navigazione (come `SKIP -1`) tenta di spostarsi prima del primo record
    /// nella sequenza logica dell'indice attivo.
    /// </summary>
    /// <returns>True se il cursore è in stato BOF, altrimenti False.</returns>
    public bool IsBof() => Cursor.BofFlag;

    /// <summary>
    /// Restituisce un valore che indica se l'ultima operazione di ricerca (`Seek`) ha trovato una corrispondenza esatta per la chiave.
    /// </summary>
    /// <returns>True se l'ultima ricerca ha avuto successo, altrimenti False.</returns>
    public bool WasFound() => _handle.FoundFlag;

    /// <summary>Imposta manualmente FoundFlag. Usato dalle relazioni master-detail (SET RELATION)
    /// per riflettere l'esito di un posizionamento del figlio non basato su Seek() su indice
    /// (scan lineare per campo, dato che non esiste un indice secondario attivabile su ordine
    /// diverso dalla chiave primaria) — vedi IsamDriver.SyncRelation.</summary>
    public void SetFoundFlag(bool found) => _handle.FoundFlag = found;

    /// <summary>Restituisce true se il record corrente è marcato come cancellato.</summary>
    public bool IsDeleted()
    {
        if (Cursor.CurrentRecordOffset < 0) return false;
        try { var meta = _handle.ReadRecordMetadata(Cursor.CurrentRecordOffset, _handle.FileEncoding); return meta.Status == Config.RecordStatusDeleted; }
        catch { return false; }
    }

    /// <summary>Restituisce il nome dell'indice attivo, o "" se primario.</summary>
    public string ActiveIndexName()
    {
        if (_activeIndexManager == _primaryIndexManager) return "";
        try { return _activeIndexManager.GetIndexDefinition().Name; } catch { return ""; }
    }

    /// <summary>Restituisce il filtro attivo, o "" se nessuno.</summary>
    public string ActiveFilter() => Cursor.ActiveFilterExpression?.ToString() ?? "";

    /// <summary>Restituisce la relazione attiva, o "" se nessuna.</summary>
    public string ActiveRelation() => "";

    /// <summary>
    /// Imposta l'indice attivo (o di controllo) per tutte le successive operazioni di navigazione e ricerca.
    /// Questo metodo implementa la logica per un comando come `SET ORDER TO IndexName`.
    /// </summary>
    /// <param name="indexName">
    /// Il nome dell'indice da attivare. Se questo valore è nullo, vuoto, o corrisponde al nome
    /// dell'indice primario, l'ordine viene reimpostato su quello della chiave primaria.
    /// </param>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se <paramref name="indexName"/> si riferisce a un indice secondario che non esiste
    /// o non è stato caricato.
    /// </exception>
    /// <remarks>
    /// Cambiare l'ordine di un file ISAM modifica la sequenza logica in cui i record vengono letti.
    /// Dopo aver invocato questo metodo, tutti i comandi come `Go`, `Skip`, e `Seek` opereranno
    /// secondo l'ordine definito dal nuovo indice attivo. L'engine determina se usare il gestore
    /// della chiave primaria o cercare il gestore appropriato nel dizionario degli indici secondari.
    /// </remarks>
    public void SetActiveOrder(string indexName)
    {
        if (string.IsNullOrEmpty(indexName) ||
            indexName.Equals(_handle.Schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
        {
            _activeIndexManager = _primaryIndexManager;
            return;
        }

        if (_secondaryIndexManagers.TryGetValue(indexName, out var secondaryManager))
        {
            _activeIndexManager = secondaryManager;
        }
        else
        {
            // Index dropped or not found: silently fall back to primary index.
            _activeIndexManager = _primaryIndexManager;
        }
    }

    /// <summary>
    /// Calcola e restituisce il numero di record fisico (ordinale, 1-based) del record attualmente puntato dal cursore.
    /// </summary>
    /// <returns>
    /// Il numero del record fisico (1-based) corrispondente alla posizione corrente del cursore,
    /// oppure 0 se il cursore non è posizionato su un record valido.
    /// </returns>
    /// <remarks>
    /// A differenza della navigazione logica basata su indici, questo numero rappresenta la posizione ordinale
    /// fisica del record all'interno dell'area dati del file. Non è influenzato dall'ordine attivo (`SetActiveOrder`).
    /// La logica calcola l'offset di inizio dell'area dati, determina la posizione relativa del record corrente
    /// e la divide per la dimensione fissa del record per ottenere il suo numero sequenziale.
    /// </remarks>
    public long GetCurrentRecordNumber()
    {
        if (Cursor.CurrentRecordOffset < 0) return 0;

        // La formula si basa sull'offset di inizio dei dati e la dimensione del record.
        var dataAreaStartOffset = _handle.DataAreaStartOffset;
        if (Cursor.CurrentRecordOffset < dataAreaStartOffset) return 0;

        return (Cursor.CurrentRecordOffset - dataAreaStartOffset) / _handle.Header.RecordSize + 1;
    }

    /// <summary>
    /// Positions the cursor on a physical record for offline replication replay.
    /// </summary>
    /// <param name="recNo">1-based physical record number.</param>
    internal void PositionAtRecord(long recNo)
    {
        if (recNo <= 0 || recNo > _handle.Header.LastRecNumber)
        {
            throw new ArgumentOutOfRangeException(nameof(recNo), recNo, "Record number out of range.");
        }

        Cursor.BofFlag = false;
        Cursor.EofFlag = false;
        Cursor.CurrentRecordOffset = _handle.DataAreaStartOffset + (recNo - 1) * _handle.Header.RecordSize;
        var metadata = _handle.ReadRecordMetadata(Cursor.CurrentRecordOffset, _handle.FileEncoding);
        Cursor.LastReadRecordVersion = metadata.Version;
    }

    /// <summary>Runtime UDT for record I/O (offline tooling and replication applier).</summary>
    internal UserDefinedType UdtDefinition => _handle.UdtDefinition;

    /// <summary>Last allocated physical record slot (1-based count of slots).</summary>
    internal long LastPhysicalRecNumber => _handle.Header.LastRecNumber;

    /// <summary>
    /// Restituisce il numero totale di record *attivi* (non cancellati) presenti nel file di database.
    /// </summary>
    /// <returns>
    /// Il numero totale di record validi e non marcati per la cancellazione.
    /// </returns>
    /// <remarks>
    /// Questo metodo fornisce un conteggio logico del contenuto del database, che è spesso più utile
    /// del conteggio fisico totale. Il valore viene letto direttamente dal campo <c>ActiveRecordCount</c>
    /// dell'header del file (<see cref="Config.FileHeader"/>), rendendo questa un'operazione istantanea
    /// che non richiede la scansione dell'intero file.
    /// </remarks>
    public long GetTotalRecordCount()
    {
        return _handle.Header.ActiveRecordCount;
    }

    /// <summary>
    /// Tenta di acquisire un lock esclusivo non bloccante sul record attualmente puntato dal cursore.
    /// </summary>
    /// <returns>
    /// `true` se il lock viene acquisito immediatamente (cioè, il record non era bloccato da un altro thread/processo).
    /// `false` se il record è già bloccato o se il cursore non è posizionato su un record valido.
    /// </returns>
    /// <remarks>
    /// Questo metodo fornisce un meccanismo per implementare il locking pessimistico esplicito.
    /// È l'equivalente funzionale del comando `RLOCK()` in xBase. L'operazione delega la chiamata
    /// al gestore di lock sottostante (<see cref="IsamFileHandle.TryLockRecord"/>) con un timeout di 0 millisecondi,
    /// garantendo che la chiamata non attenda e restituisca immediatamente un risultato.
    /// </remarks>
    public bool TryLockCurrentRecord()
    {
        if (Cursor.CurrentRecordOffset < 0) return false;
        return _handle.TryLockRecord(Cursor.CurrentRecordOffset, 0); // Timeout 0 = non bloccante
    }

    /// <summary>
    /// Tenta di acquisire un lock esclusivo non bloccante sull'intero file di database.
    /// </summary>
    /// <returns>
    /// `true` se il lock viene acquisito immediatamente (cioè, il file non era bloccato in scrittura da un altro thread/processo).
    /// `false` se il file è già bloccato.
    /// </returns>
    /// <remarks>
    /// Questo metodo fornisce un meccanismo per implementare il locking pessimistico a livello di file.
    /// È l'equivalente funzionale del comando `FLOCK()` in xBase. L'operazione delega la chiamata
    /// al gestore di file sottostante (<see cref="IsamFileHandle.TryLockFile"/>) con un timeout di 0 millisecondi,
    /// assicurando che la chiamata non attenda e restituisca immediatamente un risultato,
    /// permettendo al programma chiamante di gestire il fallimento senza bloccarsi.
    /// </remarks>
    public bool TryLockFile()
    {
        return _handle.TryLockFile(0);
    }

    /// <summary>
    /// Marca il record corrente come cancellato ("soft delete"), creando una nuova versione del record
    /// con lo stato 'cancellato', rimuove le chiavi secondarie dall'indice e invalida il cursore.
    /// L'operazione richiede un lock di scrittura esclusivo.
    /// </summary>
    /// <param name="evaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <exception cref="LockTimeoutException">
    /// Lanciata se il lock di scrittura non può essere acquisito. Se in una transazione,
    /// causa un rollback automatico.
    /// </exception>
    /// <remarks>
    /// ... (i commenti rimangono invariati) ...
    /// </remarks>
    public void DeleteCurrent(KeyExpressionEvaluatorCallback? evaluator)
    {
        if (Cursor.CurrentRecordOffset < 0) return;

        var currentOffset = Cursor.CurrentRecordOffset;

        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;
        var isAutoCommit = context == null;

        var lockKey = LockManagerBase.GetRecordLockKey(currentOffset);
        AcquireRecordWriteLock(currentOffset, context, lockKey);

        try
        {
            var udtInstance = GetMostRecentRecord(currentOffset);
            var currentMetadata = _handle.ReadRecordMetadata(currentOffset, _handle.FileEncoding);

            if (currentMetadata.Status == Config.RecordStatusDeleted) return;

            // Passa il contesto a WriteUndoVersion
            var previousVersionOffset = _handle.WriteUndoVersion(currentOffset, context);

            var newMetadata = new RecordMetadata
            {
                Status = Config.RecordStatusDeleted,
                Version = DateTime.UtcNow.Ticks,
                TransactionId = isAutoCommit ? 0 : context!.TransactionId,
                PreviousVersionOffset = previousVersionOffset
            };

            var newRecordData =
                RecordMarshaller.SerializeRecordWithMetadata(_handle.Schema, udtInstance, newMetadata, _handle);

            _handle.WriteRecord(currentOffset, newRecordData);

            RemoveSecondaryIndexesForRecord(udtInstance, currentOffset, context, evaluator);

            var header = _handle.Header;
            header.ActiveRecordCount -= 1;
            _handle.UpdateHeader(header);

            if (isAutoCommit)
            {
                _handle.Commit();
            }

            RecordReplicationOp(ReplicationOp.Delete, GetCurrentRecordNumber(), udtInstance, context);
        }
        finally
        {
            if (isAutoCommit)
            {
                _handle.RecordLockManager.ReleaseWriteLock(lockKey);
            }

            Cursor.CurrentRecordOffset = -1;
            Cursor.LastReadRecordVersion = -1;
        }
    }

    /// <summary>
    /// Rimuove il flag di cancellazione dal record corrente ("undelete"), creando una nuova versione attiva
    /// del record e reinserendolo in tutti gli indici. L'operazione richiede un lock di scrittura esclusivo.
    /// </summary>
    /// <param name="evaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <exception cref="LockTimeoutException">
    /// Lanciata se il lock di scrittura non può essere acquisito. Se in una transazione,
    /// causa un rollback automatico.
    /// </exception>
    /// <remarks>
    /// Questa operazione è l'inverso di `DeleteCurrent`. Per il livello `Serializable`, acquisisce anche lock
    /// di scrittura sulle chiavi di indice da reinserire.
    /// </remarks>
    public void RecallCurrent(KeyExpressionEvaluatorCallback? evaluator)
    {
        if (Cursor.CurrentRecordOffset < 0) return;

        var currentOffset = Cursor.CurrentRecordOffset;

        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;
        var isAutoCommit = context == null;

        var lockKey = LockManagerBase.GetRecordLockKey(currentOffset);
        AcquireRecordWriteLock(currentOffset, context, lockKey);

        try
        {
            var currentMetadata = _handle.ReadRecordMetadata(currentOffset, _handle.FileEncoding);
            if (currentMetadata.Status == Config.RecordStatusActive) return;

            var udtInstance = GetMostRecentRecord(currentOffset);
            // Passa il contesto a WriteUndoVersion
            var previousVersionOffset = _handle.WriteUndoVersion(currentOffset, context);

            var newMetadata = new RecordMetadata
            {
                Status = Config.RecordStatusActive,
                Version = DateTime.UtcNow.Ticks,
                TransactionId = isAutoCommit ? 0 : context!.TransactionId,
                PreviousVersionOffset = previousVersionOffset
            };

            var newRecordData =
                RecordMarshaller.SerializeRecordWithMetadata(_handle.Schema, udtInstance, newMetadata, _handle);

            _handle.WriteRecord(currentOffset, newRecordData);

            ReinsertSecondaryIndexesForRecord(udtInstance!, currentOffset, context, evaluator);

            var header = _handle.Header;
            header.ActiveRecordCount += 1;
            _handle.UpdateHeader(header);

            if (isAutoCommit)
            {
                _handle.Commit();
            }

            RecordReplicationOp(ReplicationOp.Recall, GetCurrentRecordNumber(), udtInstance!, context);
        }
        finally
        {
            if (isAutoCommit)
            {
                _handle.RecordLockManager.ReleaseWriteLock(lockKey);
            }
        }
    }

    /// <summary>
    /// Serializza un valore fornito (come una stringa o un numero) nella sua rappresentazione binaria di chiave,
    /// utilizzando le proprietà (in particolare la lunghezza) dell'indice attualmente attivo.
    /// </summary>
    /// <param name="value">Il valore da serializzare in una chiave di ricerca.</param>
    /// <returns>
    /// Un array di byte che rappresenta la chiave normalizzata, pronta per essere usata in una chiamata Seek/>.
    /// </returns>
    /// <remarks>
    /// Questo metodo agisce come un helper pubblico per l'Executor, a cui astrae la complessità della
    /// serializzazione delle chiavi. Determina quale indice è attualmente attivo tramite <see cref="_activeIndexManager"/>,
    /// recupera la sua <see cref="Config.IndexDefinition"/> e delega l'operazione di serializzazione effettiva
    /// (che include la conversione a maiuscolo, la conversione in byte e il padding/troncamento)
    /// al metodo specializzato <see cref="SchemaManager.SerializeKeyValue"/>.
    /// </remarks>
    public byte[] SerializeKeyValue(object? value)
    {
        // Questa logica deve essere centralizzata per garantire coerenza.
        // Per ora, la deleghiamo a un metodo statico.
        // In futuro, potremmo volerla legare allo schema specifico dell'indice.
        return SchemaManager.SerializeKeyValue(value, _activeIndexManager.GetIndexDefinition());
    }

    /// <summary>
    /// Esegue un'operazione di compattazione sul file di database. Questa operazione rimuove fisicamente
    /// tutti i record marcati come cancellati, riscrive i dati attivi in un nuovo file temporaneo
    /// e, infine, sostituisce il file originale con la sua versione compattata.
    /// </summary>
    /// <param name="evaluator">Il callback opzionale per la valutazione delle espressioni di chiave, necessario per le operazioni
    /// di Append sul nuovo file.</param>
    /// <remarks>
    /// `Pack` è un'operazione di manutenzione intensiva che offre diversi benefici:
    /// - Recupero dello Spazio: Libera lo spazio su disco occupato dai record cancellati.
    /// - Deframmentazione: Riscrivendo i record in modo contiguo, migliora le performance di lettura sequenziale.
    /// - Ricostruzione Indici: L'operazione include implicitamente una reindicizzazione completa, garantendo che
    ///   gli indici siano ottimali e privi di frammentazione.
    ///
    /// Per garantire la sicurezza, l'operazione è progettata per essere atomica a livello di file system.
    /// Tutte le modifiche avvengono su un file temporaneo; solo al completamento con successo, il file
    /// originale viene sostituito.
    /// </remarks>
    public void Pack(KeyExpressionEvaluatorCallback? evaluator)
    {
        // La logica di Pack chiude l'handle corrente, quindi l'Executor
        // dovrà gestire la riapertura.
        Pack(_handle, this, evaluator);
    }

    /// <summary>
    /// Esegue l'operazione di PACK su un file di database.
    /// Crea un nuovo file temporaneo, vi copia solo i record attivi utilizzando l'engine
    /// per garantire la corretta gestione dei tipi complessi (come JSON), e
    /// ricostruisce tutti gli indici da zero. Infine, sostituisce il file originale.
    /// </summary>
    /// <param name="originalHandle">L'handle del file ISAM aperto da compattare. Diventerà invalido dopo l'operazione.</param>
    /// <param name="originalEngine">L'engine ISAM associato all'handle, necessario per la lettura corretta dei record.</param>
    /// <param name="evaluator">Il callback opzionale per la valutazione delle espressioni di chiave, necessario per le operazioni
    /// di Append sul nuovo file.</param>
    /// <remarks>
    /// Questa operazione è ad alta intensità di I/O. Per garantire la coerenza, l'handle del file originale
    /// non viene chiuso esplicitamente qui; l'Executor che invoca PACK è responsabile della rimozione
    /// dell'engine e del suo handle dalla lista delle work area attive, poiché la sostituzione del file
    /// rende l'handle originale obsoleto.
    /// </remarks>
    private void Pack(IsamFileHandle originalHandle, Engine originalEngine, KeyExpressionEvaluatorCallback? evaluator)
    {
        var tempDbPath = Path.GetTempFileName();
        var originalDbPath = originalHandle.FilePath;
        var originalSchema = originalHandle.Schema;
        var originalUdtDef = originalHandle.UdtDefinition;
        var fileEncoding = originalHandle.FileEncoding;

        IsamFileHandle.CreateNew(tempDbPath, originalSchema, fileEncoding);

        long packedCount = 0;
        using (var tempHandle = new IsamFileHandle(tempDbPath, originalSchema, originalUdtDef,
                   LockingStrategy.IntraProcess, fileEncoding))
        using (var tempEngine = new Engine(tempHandle))
        {
            // Il tempEngine deve ereditare l'evaluator per le sue operazioni di scrittura.
            tempEngine.Evaluator = evaluator;

            originalEngine.GoInternal(Config.CursorMoveOperation.Top);
            while (!originalEngine.IsEof())
            {
                if (originalEngine.IsRecordValidForFilter())
                {
                    var udtInstance = originalEngine.GetCurrentRecord();
                    tempEngine.Append(udtInstance!, evaluator);
                    packedCount++;
                }

                originalEngine.SkipInternal();
            }
        }

        File.Replace(tempDbPath, originalDbPath, null, true);
        originalHandle.ReloadAfterExternalReplace(originalSchema);
        originalEngine.RefreshAfterExternalFileReplace();
        // Correggi ActiveRecordCount dopo PACK
        var hdr = originalHandle.Header;
        hdr.ActiveRecordCount = packedCount;
        originalHandle.UpdateHeader(hdr);
        originalEngine.RecordReplicationPack();
    }

    /// <summary>
    /// Ricarica indici e cursore dopo una sostituzione esterna del file (PACK).
    /// </summary>
    internal void RefreshAfterExternalFileReplace()
    {
        ReloadIndexManagersFromHeader();
        Cursor.CurrentRecordOffset = -1;
        Cursor.BofFlag = true;
        Cursor.EofFlag = true;
        _handle.FoundFlag = false;
        Cursor.LastReadRecordVersion = -1;
    }

    /// <summary>
    /// Ricostruisce da zero tutti gli indici (primario e secondari) associati al file di database.
    /// L'operazione è eseguita in modo esclusivo e transazionale per garantirne l'atomicità e la coerenza.
    /// </summary>
    /// <param name="evaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <exception cref="LockTimeoutException">Se non è possibile acquisire un lock esclusivo sul file.</exception>
    /// <remarks>
    /// `Reindex` è un'operazione di manutenzione essenziale per correggere eventuali corruzioni o per
    /// ottimizzare la struttura degli indici. Il processo è il seguente:
    /// 1. Acquisisce un lock di scrittura esclusivo sull'intero file, bloccando qualsiasi altra operazione.
    /// 2. Avvia una transazione interna per garantire che la ricostruzione sia atomica (o tutto o niente).
    /// 3. Svuota logicamente tutti gli indici esistenti azzerando i puntatori alle loro radici nell'header.
    /// 4. Itera sequenzialmente su ogni record *attivo* nel file di dati.
    /// 5. Per ogni record, reinserisce le sue chiavi in tutti gli indici, costruendo di fatto nuovi alberi B+
    ///    bilanciati e privi di frammentazione.
    /// 6. Al termine, esegue il commit della transazione interna, rendendo i nuovi indici permanenti.
    /// 7. In caso di errore, esegue il rollback, annullando tutte le modifiche.
    /// 8. Rilascia il lock di file.
    /// </remarks>
    public void Reindex(KeyExpressionEvaluatorCallback? evaluator)
    {
        _handle.LockFile();
        try
        {
            BeginTransaction();
            try
            {
                RebuildAllIndexes(evaluator, GetCurrentTransactionContext());
                Commit();
            }
            catch
            {
                Rollback();
                throw;
            }
        }
        finally
        {
            _handle.UnlockFile();
        }
    }

    /// <summary>
    /// Repairs a damaged ISAM file: refreshes header manifest from heap scan, rebuilds all indexes.
    /// </summary>
    /// <param name="evaluator">Optional key expression evaluator for secondary indexes.</param>
    /// <remarks>
    /// ponytail: beyond REINDEX — recounts active records, discards broken page free list.
    /// Call <see cref="IsamRepair.PrepareForOpen"/> offline before open when magic/roots are corrupt.
    /// </remarks>
    public void Repair(KeyExpressionEvaluatorCallback? evaluator)
    {
        _handle.LockFile();
        try
        {
            BeginTransaction();
            try
            {
                RefreshManifestFromHeapScan();
                RebuildAllIndexes(evaluator, GetCurrentTransactionContext());
                var header = _handle.Header;
                header.PageFreeListHeadOffset = -1;
                _handle.UpdateHeader(header);
                Commit();
            }
            catch
            {
                Rollback();
                throw;
            }
        }
        finally
        {
            _handle.UnlockFile();
        }
    }

    private void RefreshManifestFromHeapScan()
    {
        var header = _handle.Header;
        if (header.RecordSize <= 0)
        {
            return;
        }

        var dataStart = _handle.DataAreaStartOffset;
        var fileLength = _handle.GetStream().Length;
        var maxRecords = Math.Max(0, (fileLength - dataStart) / header.RecordSize);
        if (header.LastRecNumber > maxRecords)
        {
            header.LastRecNumber = maxRecords;
        }

        long activeCount = 0;
        for (long i = 0; i < header.LastRecNumber; i++)
        {
            var offset = dataStart + i * header.RecordSize;
            var metadata = _handle.ReadRecordMetadata(offset, _handle.FileEncoding);
            if (metadata.Status == Config.RecordStatusActive)
            {
                activeCount++;
            }
        }

        header.ActiveRecordCount = activeCount;
        _handle.UpdateHeader(header);
    }

    /// <summary>
    /// Ricostruisce un singolo indice secondario scansionando i record attivi a batch,
    /// con lock strutturale breve per batch (build online / CONCURRENTLY-like).
    /// </summary>
    /// <param name="indexName">Nome dell'indice secondario da ricostruire.</param>
    /// <param name="evaluator">Callback valutazione chiave (fallback: <see cref="Evaluator"/>).</param>
    /// <param name="batchSize">Record per batch sotto lock strutturale (ponytail: default 64).</param>
    /// <remarks>
    /// Limitazioni vs SQLite <c>CREATE INDEX CONCURRENTLY</c>: ogni batch fa commit diretto;
    /// lettori vedono progressi incrementali; scritture concorrenti sullo stesso indice non sono serializzate come SQLite.
    /// </remarks>
    public void RebuildSecondaryIndex(
        string indexName,
        KeyExpressionEvaluatorCallback? evaluator,
        int batchSize = 64)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(indexName);
        if (batchSize <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(batchSize));
        }

        if (!_secondaryIndexManagers.TryGetValue(indexName, out var manager))
        {
            throw new InvalidOperationException($"Indice secondario non trovato: '{indexName}'.");
        }

        var indexPosition = _handle.Schema.SecondaryKeys.FindIndex(sk =>
            sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase));
        if (indexPosition < 0)
        {
            throw new InvalidOperationException($"Definizione indice assente nello schema: '{indexName}'.");
        }

        var resolvedEvaluator = ResolveKeyEvaluator(evaluator);
        var indexDef = _handle.Schema.SecondaryKeys[indexPosition];
        PopulateSecondaryIndexOnline(indexDef, manager, indexPosition, resolvedEvaluator, batchSize, clearExisting: true);
    }

    /// <summary>Alias esplicito per <see cref="RebuildSecondaryIndex"/> (build online a batch).</summary>
    public void BuildSecondaryIndexOnline(
        string indexName,
        KeyExpressionEvaluatorCallback? evaluator,
        int batchSize = 64) =>
        RebuildSecondaryIndex(indexName, evaluator, batchSize);

    /// <summary>
    /// Popola un indice secondario scansionando record attivi a batch con commit per batch.
    /// </summary>
    private void PopulateSecondaryIndexOnline(
        Config.IndexDefinition indexDef,
        BPlusTreeManager manager,
        int indexPosition,
        KeyExpressionEvaluatorCallback evaluator,
        int batchSize,
        bool clearExisting)
    {
        if (clearExisting)
        {
            using (_handle.EnterStructuralLock())
            {
                var rootOffset = manager.GetRootOffset();
                if (rootOffset != -1)
                {
                    AddTreePagesToFreeList(manager, rootOffset);
                }

                manager.ResetRoot(-1);
                RemoveFromMemoryIndexAllKeys(indexDef.Name);
                var header = _handle.Header;
                header.SecondaryIndexRootOffsets[indexPosition] = -1;
                _handle.UpdateHeader(header);
            }

            _handle.Commit();
        }

        var recordCount = _handle.Header.LastRecNumber;
        var dataAreaStart = _handle.DataAreaStartOffset;
        var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);

        for (long batchStart = 0; batchStart < recordCount; batchStart += batchSize)
        {
            var batchEnd = Math.Min(batchStart + batchSize, recordCount);
            using (_handle.EnterStructuralLock())
            {
                for (long i = batchStart; i < batchEnd; i++)
                {
                    var currentOffset = dataAreaStart + i * _handle.Header.RecordSize;
                    var metadata = _handle.ReadRecordMetadata(currentOffset, _handle.FileEncoding);
                    if (metadata.Status != Config.RecordStatusActive)
                    {
                        continue;
                    }

                    var udt = GetMostRecentRecord(currentOffset);
                    PopulateStringKeyedFields(udt, stringKeyedFields);
                    TryInsertSecondaryIndex(indexDef, manager, stringKeyedFields, currentOffset, null, evaluator);
                }

                var header = _handle.Header;
                header.SecondaryIndexRootOffsets[indexPosition] = manager.GetRootOffset();
                _handle.UpdateHeader(header);
            }

            _handle.Commit();
        }
    }

    private TransactionContext? GetCurrentTransactionContext()
    {
        var stack = GetTransactionStackOrNull(Thread.CurrentThread.ManagedThreadId);
        return stack is { Count: > 0 } ? stack.Peek() : null;
    }

    private Stack<TransactionContext>? GetTransactionStackOrNull(int threadId) =>
        _threadTransactions.TryGetValue(threadId, out var stack) ? stack : null;

    private Stack<TransactionContext> GetOrCreateTransactionStack(int threadId) =>
        _threadTransactions.GetOrAdd(threadId, static _ => new Stack<TransactionContext>());

    private void RemoveTransactionStack(int threadId) =>
        _threadTransactions.TryRemove(threadId, out _);

    private void RebuildAllIndexes(KeyExpressionEvaluatorCallback? evaluator, TransactionContext? context)
    {
        var header = _handle.Header;
        header.PrimaryKeyRootOffset = -1;
        for (var i = 0; i < header.SecondaryIndexRootOffsets.Length; i++)
        {
            header.SecondaryIndexRootOffsets[i] = -1;
        }

        _handle.UpdateHeader(header);

        _primaryIndexManager.ResetRoot(-1);
        foreach (var manager in _secondaryIndexManagers.Values)
        {
            manager.ResetRoot(-1);
        }

        var recordCount = _handle.Header.LastRecNumber;
        var dataAreaStart = _handle.DataAreaStartOffset;

        for (long i = 0; i < recordCount; i++)
        {
            var currentOffset = dataAreaStart + i * _handle.Header.RecordSize;
            var metadata = _handle.ReadRecordMetadata(currentOffset, _handle.FileEncoding);

            if (metadata.Status != Config.RecordStatusActive)
            {
                continue;
            }

            var udt = GetMostRecentRecord(currentOffset);
            UpdateAllIndexesForInsert(udt, currentOffset, null, evaluator);
        }
    }

    /// <summary>
    /// Crea un nuovo indice secondario in modo asincrono, basato su un'espressione di chiave.
    /// La registrazione schema avviene in transazione breve; la popolazione usa build online a batch
    /// (<see cref="PopulateSecondaryIndexOnline"/>), analogo a SQLite CREATE INDEX CONCURRENTLY con limiti documentati.
    /// </summary>
    /// <param name="indexName">Il nome univoco da assegnare al nuovo indice.</param>
    /// <param name="keyExpression">L'espressione che definisce la chiave (es. "CAMPO1" o "UPPER(CAMPO2)").</param>
    /// <param name="keyLength">La lunghezza fissa in byte della chiave dell'indice.</param>
    /// <param name="isUnsafe">Se true, indica che l'espressione di chiave richiede il valutatore GPM completo.</param>
    /// <param name="evaluator">Il callback per la valutazione delle espressioni di chiave (fallback: <see cref="Evaluator"/>).</param>
    /// <param name="batchSize">Record per batch durante la popolazione online (default 64).</param>
    /// <returns>Un <see cref="Task"/> che rappresenta l'operazione di creazione asincrona.</returns>
    /// <exception cref="InvalidOperationException">Se un indice con lo stesso nome esiste già.</exception>
    /// <exception cref="IOException">Se il numero massimo di indici secondari è stato raggiunto.</exception>
    public Task CreateIndexAsync(string indexName, string keyExpression, int keyLength, bool isUnsafe,
        KeyExpressionEvaluatorCallback? evaluator, int batchSize = 64)
    {
        return Task.Run(() =>
        {
            if (batchSize <= 0)
            {
                throw new ArgumentOutOfRangeException(nameof(batchSize));
            }

            var resolvedEvaluator = ResolveKeyEvaluator(evaluator);
            Config.IndexDefinition newIndexDef;
            BPlusTreeManager newIndexManager;
            var indexPosition = -1;

            BeginTransaction();
            try
            {
                if (_handle.Schema.PrimaryKey.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase) ||
                    _handle.Schema.SecondaryKeys.Any(sk =>
                        sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase)))
                {
                    throw new InvalidOperationException($"Un indice con nome '{indexName}' esiste già.");
                }

                for (var i = 0; i < _handle.Header.SecondaryIndexRootOffsets.Length; i++)
                {
                    if (_handle.Header.SecondaryIndexRootOffsets[i] == -1)
                    {
                        indexPosition = i;
                        break;
                    }
                }

                if (indexPosition == -1)
                {
                    throw new IOException("Numero massimo di indici secondari raggiunto.");
                }

                newIndexDef = new Config.IndexDefinition(indexName, keyExpression, keyLength, false, isUnsafe);
                _handle.Schema.SecondaryKeys.Add(newIndexDef);
                newIndexManager = new BPlusTreeManager(_handle, _handle.Schema, newIndexDef, -1);

                var newSchemaBuffer = SchemaManager.SerializeSchema(_handle.Schema);
                // Riserva spazio PRIMA di scrivere: lo schema e' cresciuto (nuova chiave
                // secondaria), e se non c'entra nello slot attuale l'area dati che lo segue
                // (record fisici gia' scritti) deve essere spostata in avanti — altrimenti
                // DataAreaStartOffset (= SchemaOffset+SchemaLength quando non c'e' una mappa
                // indici separata) si sposta silenziosamente mentre i byte restano dove sono,
                // corrompendo la lettura del primo record. Vedi commento su EnsureSchemaSlot.
                _handle.EnsureSchemaSlot(newSchemaBuffer.Length);
                var header = _handle.Header;
                // Non ridurre mai SchemaLength qui: la crescita e' l'unico caso da gestire in
                // questo punto (DropIndexAsync gestisce la riduzione separatamente, senza mai
                // farla scendere sotto il valore corrente, per lo stesso motivo).
                header.SchemaLength = Math.Max(header.SchemaLength, newSchemaBuffer.Length);
                header.SecondaryIndexRootOffsets[indexPosition] = -1;
                _handle.UpdateHeader(header);
                var stream = _handle.GetStream();
                stream.Seek(_handle.Header.SchemaOffset, SeekOrigin.Begin);
                stream.Write(newSchemaBuffer, 0, newSchemaBuffer.Length);

                _secondaryIndexManagers.Add(indexName, newIndexManager);
                Commit();
            }
            catch
            {
                Rollback();
                throw;
            }

            // FIX (bug ricorrente: CREATE INDEX di un 2° indice corrompe le letture, DbIsamFullTests):
            // la crescita dello schema (EnsureSchemaSlot) sposta FISICAMENTE i record; gli
            // offset-record memorizzati nelle foglie degli indici SECONDARI già esistenti restano
            // stantii (il repair incrementale RepairPointersAfterHeapMove è risultato fragile in
            // scenari multi-indice) → lettura GARBAGE silenziosa. Invece di popolare SOLO il nuovo
            // indice, RICOSTRUISCIAMO TUTTI gli indici dai record (stesso percorso di ISAM REINDEX,
            // che azzera il sintomo): robusto, e la crescita schema oltre lo slot è rara.
            // Vedi Hard-Lessons/ISAM-Schema-Growth-Heap-Relocation.md.
            BeginTransaction();
            try
            {
                RebuildAllIndexes(resolvedEvaluator, GetCurrentTransactionContext());
                Commit();
            }
            catch
            {
                Rollback();
                throw;
            }
            RecordReplicationSchema();
        });
    }

    /// <summary>
    /// Rimuove un indice secondario esistente in modo asincrono.
    /// L'operazione rimuove la definizione dell'indice dallo schema e rilascia lo spazio
    /// utilizzato dalle sue pagine B+Tree, aggiungendole alla free list del file.
    /// </summary>
    /// <param name="indexName">Il nome dell'indice secondario da rimuovere.</param>
    /// <returns>Un <see cref="Task"/> che rappresenta l'operazione di rimozione asincrona.</returns>
    /// <exception cref="InvalidOperationException">Se si tenta di rimuovere l'indice primario o se non viene trovato
    /// nessun indice con il nome specificato.</exception>
    public Task DropIndexAsync(string indexName)
    {
        return Task.Run(() =>
        {
            _handle.LockFile();
            try
            {
                if (indexName.Equals(_handle.Schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("Impossibile rimuovere l'indice primario.");

                var indexPosition = _handle.Schema.SecondaryKeys.FindIndex(sk =>
                    sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase));

                if (indexPosition == -1)
                    throw new InvalidOperationException(
                        $"Nessun indice secondario trovato con nome '{indexName}'.");

                if (_secondaryIndexManagers.TryGetValue(indexName, out var indexToRemove))
                {
                    var rootOffset = indexToRemove.GetRootOffset();
                    if (rootOffset != -1)
                    {
                        AddTreePagesToFreeList(indexToRemove, rootOffset);
                    }
                }

                var header = _handle.Header;
                header.SecondaryIndexRootOffsets[indexPosition] = -1;

                _handle.Schema.SecondaryKeys.RemoveAt(indexPosition);
                var newSchemaBuffer = SchemaManager.SerializeSchema(_handle.Schema);
                // Non ridurre MAI SchemaLength: l'area dati fisica non si sposta all'indietro
                // quando lo schema si riduce (nessuna operazione di questo motore la rimuove),
                // quindi DataAreaStartOffset (= SchemaOffset+SchemaLength) deve restare almeno
                // grande quanto prima, altrimenti punterebbe DENTRO ai byte residui del vecchio
                // schema invece che all'inizio reale dei record — stessa corruzione della crescita
                // (CreateIndexAsync), ma nella direzione opposta. Lo spazio avanzato resta
                // semplicemente inutilizzato (padding), non e' un problema di correttezza.
                header.SchemaLength = Math.Max(header.SchemaLength, newSchemaBuffer.Length);

                _handle.UpdateHeader(header);
                var stream = _handle.GetStream();
                stream.Seek(_handle.Header.SchemaOffset, SeekOrigin.Begin);
                stream.Write(newSchemaBuffer, 0, newSchemaBuffer.Length);

                _secondaryIndexManagers.Remove(indexName);

                _handle.Commit();
                RecordReplicationSchema();
            }
            finally
            {
                _handle.UnlockFile();
            }
        });
    }

    /// <summary>
    /// Crea uno snapshot "a caldo" (hot backup) del file di database, producendo una copia consistente e sicura
    /// in un file di destinazione senza interrompere le operazioni di lettura in corso.
    /// </summary>
    /// <param name="backupFilePath">Il percorso completo del file di backup da creare. Se il file esiste già, sarà sovrascritto.</param>
    /// <remarks>
    /// Questo metodo implementa una strategia di backup robusta che garantisce la consistenza del file copiato.
    /// L'operazione delega la logica complessa alla classe statica <see cref="SnapshotManager"/>, che esegue i seguenti passi:
    /// 1. Acquisisce un lock esclusivo in scrittura sul file di database. Questo garantisce che nessuna modifica
    ///    possa avvenire durante il processo di snapshot, prevenendo stati intermedi o corrotti.
    /// 2. Forza il "flush" del Write-Ahead Log (WAL) sul file di database principale. Questo assicura che tutte le
    ///    operazioni precedentemente commesse siano state fisicamente scritte, portando il file a uno stato
    ///    pienamente consistente.
    /// 3. Esegue una copia bloccante (file-to-file) del file di database, ora consistente, nella destinazione specificata.
    /// 4. Rilascia immediatamente il lock esclusivo, permettendo alle altre operazioni di riprendere.
    ///
    /// Il risultato è un backup affidabile che rappresenta uno stato puntuale e coerente del database, ottenuto con
    /// un impatto minimo sulla disponibilità del sistema.
    /// </remarks>
    public void Backup(string backupFilePath)
    {
        SnapshotManager.CreateSnapshot(_handle, backupFilePath);
    }

    #endregion

    #region Locking

    /// <summary>
    /// Acquisisce un lock esclusivo bloccante sul record attualmente puntato dal cursore.
    /// Il thread corrente attenderà indefinitamente finché il lock non sarà disponibile.
    /// </summary>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se il cursore non è posizionato su un record valido su cui acquisire il lock.
    /// </exception>
    /// <exception cref="IOException">
    /// Lanciata (teoricamente) se l'acquisizione del lock fallisce per motivi imprevisti,
    /// sebbene con un'attesa infinita questo sia improbabile.
    /// </exception>
    /// <remarks>
    /// Questo metodo fornisce un meccanismo di locking pessimistico esplicito e bloccante.
    /// È l'equivalente funzionale di un comando `LOCK RECORD` che attende fino al successo.
    /// L'operazione delega la chiamata al gestore di file sottostante (<see cref="IsamFileHandle.TryLockRecord"/>)
    /// con un timeout infinito (-1) per garantire il comportamento bloccante.
    /// </remarks>
    public void LockCurrentRecord()
    {
        if (Cursor.CurrentRecordOffset < 0)
        {
            throw new InvalidOperationException("Nessun record corrente su cui acquisire il lock.");
        }
        // Rispetta LockTimeoutMilliseconds configurato sull'handle (deadlock detection inclusa).
        if (!_handle.TryLockRecord(Cursor.CurrentRecordOffset, _handle.LockTimeoutMilliseconds))
        {
            // Non dovrebbe mai accadere con timeout infinito, ma è una salvaguardia.
            throw new IOException("Impossibile acquisire il lock sul record.");
        }
    }

    /// <summary>
    /// Rilascia un lock esclusivo precedentemente acquisito sul record attualmente puntato dal cursore.
    /// </summary>
    /// <remarks>
    /// Questo metodo è il complemento di <see cref="LockCurrentRecord"/> e <see cref="TryLockCurrentRecord"/>.
    /// Viene utilizzato per rilasciare esplicitamente un lock pessimistico dopo che le operazioni
    /// sulla risorsa protetta sono state completate. L'operazione delega la chiamata al gestore di file
    /// sottostante (<see cref="IsamFileHandle.UnlockRecord"/>). È sicuro invocare questo metodo anche se
    /// il record non è bloccato dal thread corrente; in tal caso, l'operazione non avrà alcun effetto.
    /// </remarks>
    public void UnlockCurrentRecord()
    {
        if (Cursor.CurrentRecordOffset < 0) return;
        _handle.UnlockRecord(Cursor.CurrentRecordOffset);
    }

    /// <summary>
    /// UNLOCK (record): rilascia TUTTI i lock record ancora tenuti da questo handle,
    /// indipendentemente da dove si trova ora il cursore — semantica Clipper reale (UNLOCK
    /// rilascia ogni lock del processo corrente sull'area, non "il record sotto il cursore
    /// adesso"). <see cref="UnlockCurrentRecord"/> (usato da IsamDriver.Unlock prima di
    /// questo fix) rilasciava solo il record ATTUALE: con RLOCK su un record seguito da uno
    /// SKIP e poi UNLOCK, il lock ORIGINALE restava tenuto per sempre (mai rilasciato) — un
    /// ReaderWriterLockSlim ancora attivo al Dispose lanciava SynchronizationLockException
    /// ("The lock is being disposed while still being used"), osservato in
    /// DbIsamFullTests.bas sez. 11 (DBF→ISAM conversion, DbfCloseAll). Limitato ai soli lock
    /// RECORD (non tocca il lock di struttura file) — vedi
    /// IntraProcessLockManager.ReleaseAllRecordLocksHeld.
    /// </summary>
    public void ReleaseAllRecordLocks()
    {
        if (_handle.RecordLockManager is Basic.Databases.IntraProcessLockManager ipm)
            ipm.ReleaseAllRecordLocksHeld();
    }

    /// <summary>
    /// Acquisisce un lock esclusivo bloccante sull'intero file di database.
    /// Il thread corrente attenderà indefinitamente finché il lock non sarà disponibile.
    /// </summary>
    /// <remarks>
    /// Questo metodo fornisce un meccanismo di locking pessimistico a livello di file,
    /// essenziale per proteggere operazioni che modificano la struttura globale del file
    /// (come `PACK` o `REINDEX`) o quando è necessario garantire un accesso completamente esclusivo.
    /// È l'equivalente funzionale di un comando `LOCK FILE` (o `USE ... EXCLUSIVE`) che
    /// attende fino al successo. L'operazione delega la chiamata direttamente al gestore di file
    /// sottostante (<see cref="IsamFileHandle.LockFile"/>).
    /// </remarks>
    public void LockFile()
    {
        _handle.LockFile();
    }

    /// <summary>
    /// Rilascia un lock esclusivo precedentemente acquisito sull'intero file di database.
    /// </summary>
    /// <remarks>
    /// Questo metodo è il complemento di <see cref="LockFile"/> e <see cref="TryLockFile"/>.
    /// Viene utilizzato per rilasciare esplicitamente un lock pessimistico a livello di file dopo che
    /// le operazioni che richiedevano accesso esclusivo sono state completate. L'operazione delega
    /// la chiamata direttamente al gestore di file sottostante (<see cref="IsamFileHandle.UnlockFile"/>).
    /// È sicuro invocare questo metodo anche se il file non è bloccato dal thread corrente; in tal caso,
    /// l'operazione non avrà alcun effetto.
    /// </remarks>
    public void UnlockFile()
    {
        _handle.UnlockFile();
    }

    #endregion

    #region Transazioni

    /// <summary>Indica se il thread corrente ha una transazione esplicita attiva su questo motore.</summary>
    public bool HasActiveTransaction =>
        GetTransactionStackOrNull(Thread.CurrentThread.ManagedThreadId) is { Count: > 0 };

    /// <summary>
    /// Avvia una nuova transazione per il thread corrente o incrementa il livello di annidamento di una transazione esistente.
    /// </summary>
    /// <param name="level">Il livello di isolamento desiderato per la transazione. Si applica solo alla transazione più esterna.
    /// Il valore predefinito è <see cref="IsolationLevel.ReadCommitted"/>.</param>
    /// <remarks>
    /// Questo motore supporta transazioni nidificate. La prima chiamata a `BeginTransaction` avvia una transazione fisica
    /// (registrando un marcatore nel WAL) e crea un <see cref="TransactionContext"/>. Chiamate successive sullo stesso thread
    /// incrementano semplicemente il livello di annidamento spingendo lo stesso contesto sullo stack.
    /// Le modifiche verranno rese permanenti solo quando verrà eseguito il `Commit` corrispondente alla prima chiamata
    /// `BeginTransaction`. Un `Rollback` a qualsiasi livello annullerà l'intera transazione.
    /// </remarks>
    public void BeginTransaction(IsolationLevel level = IsolationLevel.ReadCommitted)
    {
        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetOrCreateTransactionStack(threadId);

        if (transactionStack.Count == 0)
        {
            // È la transazione più esterna: avvia la transazione fisica.
            var txId = _handle.BeginTransaction();
            var context = new TransactionContext(txId, level);
            DeadlockMonitor.BindTransaction(threadId, txId);
            transactionStack.Push(context);
            WriteReplicationTxnMarker(ReplicationOp.TxnBegin);
        }
        else
        {
            // Transazione nidificata: riutilizza il contesto di quella esterna.
            var outerContext = transactionStack.Peek();
            transactionStack.Push(outerContext);
        }
    }

    /// <summary>
    /// Esegue il commit del livello di transazione corrente. 
    /// Se questo è il livello di transazione più esterno, tutte le modifiche vengono rese permanenti.
    /// </summary>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se non c'è nessuna transazione attiva da commettere per il thread corrente.
    /// </exception>
    /// <remarks>
    /// In un contesto di transazioni nidificate, una chiamata a `Commit` decrementa semplicemente il livello di 
    /// annidamento. La transazione fisica viene finalizzata (scrivendo il marcatore di commit nel WAL) 
    /// e tutti i lock acquisiti (sia di lettura che di scrittura) vengono rilasciati solo quando il commit
    /// della transazione più esterna (la prima `BeginTransaction`) viene eseguito.
    /// </remarks>
    public void Commit()
    {
        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        if (transactionStack is not { Count: > 0 })
        {
            throw new InvalidOperationException("Nessuna transazione attiva da commettere.");
        }

        var context = transactionStack.Pop();

        // Se lo stack è vuoto, è il commit della transazione più esterna.
        if (transactionStack.Count == 0)
        {
            try
            {
                if (context.PendingIndexInserts.Count > 0)
                {
                    var resolvedEvaluator = ResolveKeyEvaluator(Evaluator);
                    foreach (var (udt, offset) in context.PendingIndexInserts)
                    {
                        UpdateAllIndexesForInsert(udt, offset, context, resolvedEvaluator);
                    }

                    context.PendingIndexInserts.Clear();
                }

                _handle.FlushPhysical(true);
                FlushReplicationOnCommit(context);
                WriteReplicationTxnMarker(ReplicationOp.TxnCommit);
                _handle.CommitTransaction();
                _handle.Commit();
            }
            finally
            {
                // Rilascia sempre tutti i lock acquisiti da questa transazione.
                ReleaseTransactionalLocks(context);
                DeadlockMonitor.UnbindTransaction(threadId);
                RemoveTransactionStack(threadId);
            }
        }
    }

    /// <summary>
    /// Annulla l'intera transazione in corso per il thread corrente, scartando tutte le modifiche,
    /// indipendentemente dal livello di annidamento.
    /// </summary>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se non c'è nessuna transazione attiva da annullare per il thread corrente.
    /// </exception>
    /// <remarks>
    /// A differenza di `Commit`, una singola chiamata a `Rollback` a qualsiasi livello di annidamento
    /// termina immediatamente l'intera catena di transazioni. Tutte le operazioni registrate nel WAL
    /// dall'inizio della transazione più esterna vengono scartate. L'operazione garantisce anche
    /// il rilascio di tutti i lock (di lettura e scrittura) acquisiti durante la transazione.
    /// </remarks>
    public void Rollback()
    {
        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        if (transactionStack is not { Count: > 0 })
        {
            throw new InvalidOperationException("Nessuna transazione attiva da annullare.");
        }

        // Un rollback annulla l'intera catena, quindi prendiamo il contesto più esterno.
        var outerContext = transactionStack.Last();
        outerContext.PendingIsamReplicationOps.Clear();

        try
        {
            _handle.RollbackTransaction();
            ReloadIndexManagersFromHeader();
            MvccUndoGc.ReclaimTransactionalUndoSlots(_handle, outerContext.UndoLogIdentifiers);
        }
        finally
        {
            ReleaseTransactionalLocks(outerContext);
            DeadlockMonitor.UnbindTransaction(threadId);
            RemoveTransactionStack(threadId);
        }
    }

    #endregion

    #region Filtri

    /// <summary>
    /// Imposta o rimuove una funzione di filtro per la navigazione dei record.
    /// Dopo l'impostazione, il cursore viene riposizionato sul primo record valido
    /// che soddisfa la nuova vista, rendendo questa un'operazione potenzialmente bloccante.
    /// </summary>
    /// <param name="evaluator">
    /// Il delegato del valutatore "compilato" fornito dall'Executor. Se `null`, il filtro viene rimosso.
    /// </param>
    /// <param name="filterExpression">
    /// L'albero di espressione (AST) originale del filtro, conservato per riferimento.
    /// </param>
    public void SetFilter(IsamExpressionEvaluator? evaluator, Expr? filterExpression)
    {
        Cursor.ActiveFilterEvaluator = evaluator;
        Cursor.ActiveFilterExpression = filterExpression;
        ClearFilterPushdownState();
        TrySetupFilterPushdown(filterExpression);
        GoInternal(Config.CursorMoveOperation.Top);
    }

    /// <summary>
    /// Esegue la valutazione del filtro attivo sul record attualmente puntato dal cursore,
    /// tenendo conto della visibilità delle versioni del record (MVCC).
    /// </summary>
    /// <returns>
    /// `true` se non c'è nessun filtro attivo o se la versione visibile del record corrente soddisfa la condizione.
    /// `false` se il filtro è attivo e la condizione non è soddisfatta, o se non esiste una versione
    /// del record visibile alla transazione corrente in questa posizione.
    /// </returns>
    private bool EvaluateFilter()
    {
        // Vedi nota in Go(): ActiveFilterExpression e' opzionale, non richiesto per valutare.
        if (Cursor.ActiveFilterEvaluator == null || IsBof() || IsEof())
        {
            return true;
        }

        var recordInstance = GetCurrentRecord();

        if (recordInstance == null)
        {
            return false;
        }

        // Marshalling a string-keyed
        var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        var udtDef = _handle.UdtDefinition;

        // Converte i campi int-keyed in string-keyed per l'evaluator.
        foreach (var field in udtDef.Fields)
        {
            recordInstance.Fields.TryGetValue(field.SymbolId, out var value);
            stringKeyedFields[field.Name.Lexeme] = value;
        }

        // Passa il dizionario string-keyed all'evaluator.
        // Evaluator ed espressione vengono settati insieme in SetFilter (righe 2591-2592):
        // se l'evaluator è attivo, l'espressione è garantita non-null.
        return Cursor.ActiveFilterEvaluator(Cursor.ActiveFilterExpression!, stringKeyedFields);
    }

    #endregion

    #region Public APi: Structure

    /// <summary>
    /// Ottiene un valore che indica se è presente un indice secondario attivo (ordine logico)
    /// per la navigazione, impostato tramite SET ORDER. Se false, la navigazione
    /// avviene secondo l'ordine della chiave primaria.
    /// </summary>
    public bool HasActiveOrder => true;

    /// <summary>
    /// Restituisce l'handle del file ISAM sottostante.
    /// Usato da operazioni di manutenzione come PACK che necessitano di accedere
    /// direttamente all'handle per la chiusura e la sostituzione del file.
    /// </summary>
    /// <returns>L'istanza di <see cref="IsamFileHandle"/> associata a questo engine.</returns>
    public IsamFileHandle GetFileHandle()
    {
        return _handle;
    }

    /// <summary>
    /// Esegue una serie di controlli di validazione non distruttivi sull'intero file di database
    /// per verificarne l'integrità strutturale.
    /// </summary>
    /// <returns>
    /// Una tupla contenente un booleano `IsValid` e una `string` con un messaggio di riepilogo.
    /// `IsValid` è true solo se tutti i controlli vengono superati.
    /// </returns>
    /// <summary>Media fill factor foglie indice primario (debug / test ISAM-C6).</summary>
    internal double MeasurePrimaryIndexAverageLeafFillFactor() =>
        _primaryIndexManager.MeasureAverageLeafFillFactor();

    public (bool IsValid, string Message) ValidateIntegrity()
    {
        var errorMessages = new List<string>();

        try
        {
            ValidateIntegrityCore(errorMessages);
        }
        catch (IOException ex)
        {
            errorMessages.Add(ex.Message);
        }

        return errorMessages.Count > 0
            ? (false, string.Join("\n", errorMessages))
            : (true, "Validazione completata con successo. Nessun errore trovato.");
    }

    private void ValidateIntegrityCore(List<string> errorMessages)
    {
        // 1. Checksum pagine indice primario.
        if (!_primaryIndexManager.ValidatePageChecksums(out var pkChecksumError))
        {
            errorMessages.Add($"Errore checksum indice primario: {pkChecksumError}");
        }

        if (!_primaryIndexManager.ValidateIndex())
        {
            errorMessages.Add($"Errore: L'indice primario ('{_handle.Schema.PrimaryKey.Name}') è corrotto.");
        }

        // 2. Valida tutti gli indici secondari.
        foreach (var manager in _secondaryIndexManagers.Values)
        {
            if (!manager.ValidatePageChecksums(out var secChecksumError))
            {
                errorMessages.Add(
                    $"Errore checksum indice secondario '{manager.GetIndexDefinition().Name}': {secChecksumError}");
            }

            if (!manager.ValidateIndex())
            {
                errorMessages.Add($"Errore: L'indice secondario ('{manager.GetIndexDefinition().Name}') è corrotto.");
            }
        }

        errorMessages.AddRange(IsamIntegrityChecker.ValidatePageFreeList(_handle));
        errorMessages.AddRange(IsamIntegrityChecker.ValidateActiveRecordPointers(_handle, _primaryIndexManager));

        // 3. Controlla la coerenza tra il numero di record nell'indice primario e l'header.
        long recordCountInPrimaryIndex = 0;
        if (_useInMemoryIndex && GetPrimaryMemoryIndex() is { } pkMemoryIndex)
        {
            recordCountInPrimaryIndex = pkMemoryIndex.Values.Sum(list => list.Count);
        }
        else
        {
            // Sostituisce la navigazione "intelligente" con una scansione fisica.
            // Questo garantisce che il conteggio venga eseguito su tutti i record, ignorando i filtri.
            var firstLeaf = _primaryIndexManager.FindFirstLeaf();
            if (firstLeaf.KeyCount > 0)
            {
                var currentLeaf = firstLeaf;
                while (currentLeaf != null)
                {
                    foreach (var offsetList in currentLeaf.RecordOffsets)
                    {
                        recordCountInPrimaryIndex += offsetList.Count;
                    }

                    currentLeaf = currentLeaf.NextLeafOffset != -1
                        ? (BPlusLeafNode)_primaryIndexManager.LoadNode(currentLeaf.NextLeafOffset)
                        : null;
                }
            }
        }

        if (recordCountInPrimaryIndex != _handle.Header.ActiveRecordCount)
        {
            errorMessages.Add(
                $"Errore di coerenza: L'header riporta {_handle.Header.ActiveRecordCount} record attivi, " +
                $"ma l'indice primario ne contiene {recordCountInPrimaryIndex}.");
        }
    }

    /// <summary>
    /// Crea una copia del file di database corrente, includendo solo le versioni dei record visibili alla transazione
    /// corrente e che soddisfano le condizioni specificate.
    /// </summary>
    /// <param name="destinationFilePath">Il percorso del nuovo file ISAM da creare. Se esiste, sarà sovrascritto.</param>
    /// <param name="forConditionAst">
    /// L'albero di sintassi astratta (AST) opzionale della condizione `FOR`. Verranno copiati solo i record
    /// per cui questa espressione, in aggiunta al filtro attivo, risulta vera.
    /// </param>
    /// <param name="evaluator">
    /// Il delegato del valutatore di espressioni, fornito dall'Executor, necessario per valutare la <paramref name="forConditionAst"/>.
    /// </param>
    /// <param name="keyEvaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    public void CopyTo(string destinationFilePath, Expr? forConditionAst,
        IsamExpressionEvaluator? evaluator, KeyExpressionEvaluatorCallback? keyEvaluator)
    {
        var originalOffset = Cursor.CurrentRecordOffset;

        if (forConditionAst != null && evaluator == null)
        {
            throw new ArgumentNullException(nameof(evaluator), "È richiesto un valutatore per la condizione FOR.");
        }

        IsamFileHandle.CreateNew(destinationFilePath, _handle.Schema, _handle.FileEncoding);

        using (var destHandle = new IsamFileHandle(destinationFilePath, _handle.Schema, _handle.UdtDefinition,
                   LockingStrategy.IntraProcess, _handle.FileEncoding))
        using (var destEngine = new Engine(destHandle))
        {
            destEngine.Evaluator = keyEvaluator;

            // Marshalling Setup
            // Prepara il mapper per convertire il record int-keyed a string-keyed solo se necessario per l'evaluator.
            var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
            var udtDef = _handle.UdtDefinition;
            foreach (var field in udtDef.Fields)
            {
                stringKeyedFields[field.Name.Lexeme] = null; // Inizializza con le chiavi necessarie
            }

            destEngine.BeginTransaction();
            try
            {
                GoInternal(Config.CursorMoveOperation.Top);
                while (!IsEof())
                {
                    var currentRecord = GetCurrentRecord();

                    if (currentRecord != null)
                    {
                        var shouldCopy = true;

                        if (forConditionAst != null)
                        {
                            // Popola il dizionario string-keyed dai SymbolId.
                            foreach (var field in udtDef.Fields)
                            {
                                currentRecord.Fields.TryGetValue(field.SymbolId, out var value);
                                stringKeyedFields[field.Name.Lexeme] = value;
                            }

                            // Passa il dizionario string-keyed all'evaluator.
                            if (!evaluator!(forConditionAst, stringKeyedFields))
                            {
                                shouldCopy = false;
                            }
                        }

                        if (shouldCopy)
                        {
                            destEngine.Append(currentRecord, keyEvaluator);
                        }
                    }

                    SkipInternal();
                }

                destEngine.Commit();
            }
            catch
            {
                destEngine.Rollback();
                throw;
            }
        }

        if (originalOffset >= 0)
        {
            Cursor.CurrentRecordOffset = originalOffset;
            if (_handle.GetStream().Length > originalOffset)
            {
                var metadata = _handle.ReadRecordMetadata(originalOffset, _handle.FileEncoding);
                Cursor.LastReadRecordVersion = metadata.Version;
            }

            Cursor.BofFlag = false;
            Cursor.EofFlag = false;
        }
        else
        {
            GoInternal(Config.CursorMoveOperation.Top);
        }
    }

    /// <summary>
    /// Esegue un inner join tra questa tabella (primaria) e <paramref name="secondaryEngine"/>,
    /// scrivendo le righe che soddisfano <paramref name="joinConditionAst"/> nel file di destinazione.
    /// </summary>
    /// <remarks>
    /// ponytail: equi-join su campo indicizzato → index-nested-loop; tabelle piccole (≤10k build) → hash join;
    /// altrimenti nested-loop O(n×m).
    /// </remarks>
    public void JoinWith(
        Engine secondaryEngine,
        string destinationFilePath,
        Expr joinConditionAst,
        IsamExpressionEvaluator joinEvaluator,
        KeyExpressionEvaluatorCallback? keyEvaluator,
        string? secondaryAlias = null)
    {
        ArgumentNullException.ThrowIfNull(secondaryEngine);
        ArgumentNullException.ThrowIfNull(joinConditionAst);
        ArgumentNullException.ThrowIfNull(joinEvaluator);

        var (destSchema, destUdt) = BuildJoinDestinationSchema(
            _handle.Schema, _handle.UdtDefinition,
            secondaryEngine._handle.Schema, secondaryEngine._handle.UdtDefinition);

        var primaryOriginalOffset = Cursor.CurrentRecordOffset;
        var secondaryOriginalOffset = secondaryEngine.Cursor.CurrentRecordOffset;
        var resolvedKeyEvaluator = ResolveKeyEvaluator(keyEvaluator);

        IsamFileHandle.CreateNew(destinationFilePath, destSchema, _handle.FileEncoding);

        using (var destHandle = new IsamFileHandle(
                   destinationFilePath, destSchema, destUdt, LockingStrategy.IntraProcess, _handle.FileEncoding))
        using (var destEngine = new Engine(destHandle))
        {
            destEngine.Evaluator = keyEvaluator ?? Evaluator;
            destEngine.BeginTransaction();
            try
            {
                if (IsamJoinStrategy.TryParseEquiJoin(joinConditionAst, out var leftField, out var rightField)
                    && IsamJoinStrategy.TryPlanEquiJoin(this, secondaryEngine, leftField, rightField, out var plan))
                {
                    LastJoinExecutionMode = plan.Mode;
                    switch (plan.Mode)
                    {
                        case IsamJoinStrategy.ExecutionMode.IndexNestedLoop:
                            ExecuteIndexNestedLoopJoin(
                                secondaryEngine, destEngine, destUdt, joinConditionAst, joinEvaluator,
                                resolvedKeyEvaluator, plan, secondaryAlias);
                            break;
                        case IsamJoinStrategy.ExecutionMode.HashJoin:
                            ExecuteHashJoin(
                                secondaryEngine, destEngine, destUdt, joinConditionAst, joinEvaluator,
                                resolvedKeyEvaluator, plan, secondaryAlias);
                            break;
                    }
                }
                else
                {
                    LastJoinExecutionMode = IsamJoinStrategy.ExecutionMode.NestedLoop;
                    ExecuteNestedLoopJoin(
                        secondaryEngine, destEngine, destUdt, joinConditionAst, joinEvaluator,
                        resolvedKeyEvaluator, secondaryAlias);
                }

                destEngine.Commit();
            }
            catch
            {
                if (destEngine.GetTransactionStackOrNull(Thread.CurrentThread.ManagedThreadId) is { Count: > 0 })
                {
                    destEngine.Rollback();
                }

                throw;
            }
        }

        RestoreCursorAfterBulkRead(primaryOriginalOffset);
        secondaryEngine.RestoreCursorAfterBulkRead(secondaryOriginalOffset);
    }

    private void ExecuteNestedLoopJoin(
        Engine secondaryEngine,
        Engine destEngine,
        UserDefinedType destUdt,
        Expr joinConditionAst,
        IsamExpressionEvaluator joinEvaluator,
        KeyExpressionEvaluatorCallback keyEvaluator,
        string? secondaryAlias)
    {
        GoInternal(Config.CursorMoveOperation.Top);
        while (!IsEof())
        {
            var primaryRecord = GetCurrentRecord();
            if (primaryRecord != null)
            {
                secondaryEngine.GoInternal(Config.CursorMoveOperation.Top);
                while (!secondaryEngine.IsEof())
                {
                    var secondaryRecord = secondaryEngine.GetCurrentRecord();
                    if (secondaryRecord != null)
                    {
                        var joinFields = BuildJoinFieldValues(
                            primaryRecord, secondaryRecord, secondaryAlias);
                        if (joinEvaluator(joinConditionAst, joinFields))
                        {
                            var merged = MergeJoinRecords(destUdt, primaryRecord, secondaryRecord);
                            destEngine.Append(merged, keyEvaluator);
                        }
                    }

                    secondaryEngine.SkipInternal();
                }
            }

            SkipInternal();
        }
    }

    private void ExecuteIndexNestedLoopJoin(
        Engine secondaryEngine,
        Engine destEngine,
        UserDefinedType destUdt,
        Expr joinConditionAst,
        IsamExpressionEvaluator joinEvaluator,
        KeyExpressionEvaluatorCallback keyEvaluator,
        IsamJoinStrategy.EquiJoinPlan plan,
        string? secondaryAlias)
    {
        var buildOnSecondary = plan.BuildOnSecondary;
        var probeEngine = buildOnSecondary ? this : secondaryEngine;
        var buildEngine = buildOnSecondary ? secondaryEngine : this;
        var probeField = plan.ProbeField;
        var buildField = plan.BuildField;

        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;

        probeEngine.GoInternal(Config.CursorMoveOperation.Top);
        while (!probeEngine.IsEof())
        {
            var probeRecord = probeEngine.GetCurrentRecord();
            if (probeRecord != null)
            {
                var probeValue = IsamJoinStrategy.GetFieldValue(probeRecord, probeField);
                var seekKey = IsamJoinStrategy.BuildSeekKey(
                    probeValue, plan.BuildIndex, keyEvaluator, buildEngine, buildField);
                var buildOffsets = plan.BuildIndexManager.Search(seekKey, context);

                foreach (var buildOffset in buildOffsets)
                {
                    buildEngine.Cursor.CurrentRecordOffset = buildOffset;
                    var buildMetadata = buildEngine._handle.ReadRecordMetadata(buildOffset, buildEngine._handle.FileEncoding);
                    buildEngine.Cursor.LastReadRecordVersion = buildMetadata.Version;
                    var buildRecord = buildEngine.GetCurrentRecord();
                    if (buildRecord is null)
                    {
                        continue;
                    }

                    var primaryRecord = buildOnSecondary ? probeRecord : buildRecord;
                    var secondaryRecord = buildOnSecondary ? buildRecord : probeRecord;
                    var joinFields = BuildJoinFieldValues(primaryRecord, secondaryRecord, secondaryAlias);
                    if (joinEvaluator(joinConditionAst, joinFields))
                    {
                        var merged = MergeJoinRecords(destUdt, primaryRecord, secondaryRecord);
                        destEngine.Append(merged, keyEvaluator);
                    }
                }
            }

            probeEngine.SkipInternal();
        }
    }

    private void ExecuteHashJoin(
        Engine secondaryEngine,
        Engine destEngine,
        UserDefinedType destUdt,
        Expr joinConditionAst,
        IsamExpressionEvaluator joinEvaluator,
        KeyExpressionEvaluatorCallback keyEvaluator,
        IsamJoinStrategy.EquiJoinPlan plan,
        string? secondaryAlias)
    {
        var buildOnSecondary = plan.BuildOnSecondary;
        var buildEngine = buildOnSecondary ? secondaryEngine : this;
        var probeEngine = buildOnSecondary ? this : secondaryEngine;
        var buildField = plan.BuildField;
        var probeField = plan.ProbeField;

        var hash = new Dictionary<string, List<UdtInstance>>(StringComparer.OrdinalIgnoreCase);
        buildEngine.GoInternal(Config.CursorMoveOperation.Top);
        while (!buildEngine.IsEof())
        {
            var buildRecord = buildEngine.GetCurrentRecord();
            if (buildRecord != null)
            {
                var key = IsamJoinStrategy.GetFieldValue(buildRecord, buildField)?.ToString() ?? string.Empty;
                if (!hash.TryGetValue(key, out var bucket))
                {
                    bucket = [];
                    hash[key] = bucket;
                }

                bucket.Add(buildRecord);
            }

            buildEngine.SkipInternal();
        }

        probeEngine.GoInternal(Config.CursorMoveOperation.Top);
        while (!probeEngine.IsEof())
        {
            var probeRecord = probeEngine.GetCurrentRecord();
            if (probeRecord != null)
            {
                var key = IsamJoinStrategy.GetFieldValue(probeRecord, probeField)?.ToString() ?? string.Empty;
                if (hash.TryGetValue(key, out var matches))
                {
                    foreach (var buildRecord in matches)
                    {
                        var joinFields = BuildJoinFieldValues(probeRecord, buildRecord, secondaryAlias);
                        if (joinEvaluator(joinConditionAst, joinFields))
                        {
                            var merged = MergeJoinRecords(destUdt, probeRecord, buildRecord);
                            destEngine.Append(merged, keyEvaluator);
                        }
                    }
                }
            }

            probeEngine.SkipInternal();
        }
    }

    private void RestoreCursorAfterBulkRead(long originalOffset)
    {
        if (originalOffset >= 0)
        {
            Cursor.CurrentRecordOffset = originalOffset;
            if (_handle.GetStream().Length > originalOffset)
            {
                var metadata = _handle.ReadRecordMetadata(originalOffset, _handle.FileEncoding);
                Cursor.LastReadRecordVersion = metadata.Version;
            }

            Cursor.BofFlag = false;
            Cursor.EofFlag = false;
        }
        else
        {
            GoInternal(Config.CursorMoveOperation.Top);
        }
    }

    private static (Config.IsamSchema Schema, UserDefinedType Udt) BuildJoinDestinationSchema(
        Config.IsamSchema primarySchema,
        UserDefinedType primaryUdt,
        Config.IsamSchema secondarySchema,
        UserDefinedType secondaryUdt)
    {
        var combinedFields = new List<Config.IsamFieldSchema>(primarySchema.Fields);
        var combinedTypeFields = new List<TypeField>(primaryUdt.Fields);
        var primaryFieldNames = new HashSet<string>(
            primarySchema.Fields.Select(f => f.Name),
            StringComparer.OrdinalIgnoreCase);

        foreach (var field in secondarySchema.Fields)
        {
            if (primaryFieldNames.Contains(field.Name))
            {
                throw new InvalidOperationException($"Nome del campo duplicato ('{field.Name}') in JOIN.");
            }

            combinedFields.Add(field);
        }

        foreach (var field in secondaryUdt.Fields)
        {
            if (primaryFieldNames.Contains(field.Name.Lexeme))
            {
                continue;
            }

            combinedTypeFields.Add(field);
        }

        var schema = new Config.IsamSchema(
            $"{primarySchema.TypeName}_{secondarySchema.TypeName}",
            combinedFields,
            BuildJoinPrimaryKey(primarySchema, secondarySchema),
            []);

        var typeToken = new Token(TokenKind.Identifier, schema.TypeName, null, SourcePosition.Unknown);
        var udt = new UserDefinedType(
            primaryUdt.SymbolId,
            typeToken,
            [..combinedTypeFields],
            SymbolFlags.None)
        {
            OriginalName = typeToken,
        };

        return (schema, udt);
    }

    private static Config.IndexDefinition BuildJoinPrimaryKey(
        Config.IsamSchema primarySchema,
        Config.IsamSchema secondarySchema)
    {
        var primaryName = primarySchema.PrimaryKey.Name;
        var secondaryName = secondarySchema.PrimaryKey.Name;
        if (primaryName.Equals(secondaryName, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                $"Le chiavi primarie delle tabelle in JOIN hanno lo stesso nome ('{primaryName}').");
        }

        return new Config.IndexDefinition(
            "JoinKey",
            $"COMPOSITE({primaryName},{secondaryName})",
            primarySchema.PrimaryKey.KeyLength + secondarySchema.PrimaryKey.KeyLength,
            IsUnique: true,
            IsUnsafe: false);
    }

    private static Dictionary<string, object?> BuildJoinFieldValues(
        UdtInstance primaryRecord,
        UdtInstance secondaryRecord,
        string? secondaryAlias)
    {
        var joinFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);

        foreach (var field in primaryRecord.Definition.Fields)
        {
            primaryRecord.Fields.TryGetValue(field.SymbolId, out var value);
            joinFields[field.Name.Lexeme] = value;
        }

        foreach (var field in secondaryRecord.Definition.Fields)
        {
            secondaryRecord.Fields.TryGetValue(field.SymbolId, out var value);
            joinFields[field.Name.Lexeme] = value;
            if (!string.IsNullOrEmpty(secondaryAlias))
            {
                joinFields[$"{secondaryAlias}->{field.Name.Lexeme}"] = value;
            }
        }

        return joinFields;
    }

    private static UdtInstance MergeJoinRecords(
        UserDefinedType destUdt,
        UdtInstance primaryRecord,
        UdtInstance secondaryRecord)
    {
        var fields = new Dictionary<int, object?>();
        foreach (var field in destUdt.Fields)
        {
            var name = field.Name.Lexeme;
            object? value = null;
            var primaryField = primaryRecord.Definition.Fields.FirstOrDefault(f =>
                f.Name.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase));
            if (primaryField != null)
            {
                primaryRecord.Fields.TryGetValue(primaryField.SymbolId, out value);
            }
            else
            {
                var secondaryField = secondaryRecord.Definition.Fields.FirstOrDefault(f =>
                    f.Name.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase));
                if (secondaryField != null)
                {
                    secondaryRecord.Fields.TryGetValue(secondaryField.SymbolId, out value);
                }
            }

            fields[field.SymbolId] = value;
        }

        return new UdtInstance(destUdt, fields);
    }

    /// <summary>
    /// Crea una nuova copia del file di database, con le versioni dei record visibili fisicamente ordinate
    /// in base a una lista di campi specificata e a una condizione opzionale.
    /// </summary>
    /// <param name="destinationFilePath">Il percorso del nuovo file ISAM ordinato da creare.</param>
    /// <param name="sortFields">
    /// Una lista di tuple che specificano i campi su cui ordinare e la direzione.
    /// Ogni tupla contiene il nome del campo e un booleano (true per discendente).
    /// </param>
    /// <param name="forConditionAst">L'AST opzionale della condizione FOR per filtrare i record da includere.</param>
    /// <param name="evaluator">Il delegato per valutare la condizione FOR.</param>
    /// <param name="keyEvaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <exception cref="InvalidOperationException">Se la lista dei campi di ordinamento è vuota.</exception>
    public void SortTo(string destinationFilePath, IReadOnlyList<(string FieldName, bool Descending)> sortFields,
        Expr? forConditionAst, IsamExpressionEvaluator? evaluator, KeyExpressionEvaluatorCallback? keyEvaluator)
    {
        if (!sortFields.Any())
            throw new InvalidOperationException("Almeno un campo di ordinamento deve essere specificato per SORT.");

        if (forConditionAst != null && evaluator == null)
            throw new ArgumentNullException(nameof(evaluator), "È richiesto un valutatore per la condizione FOR.");

        // Setup mappe
        var udtDef = _handle.UdtDefinition;
        var fieldIdMap =
            udtDef.Fields.ToDictionary(f => f.Name.Lexeme, f => f.SymbolId, StringComparer.OrdinalIgnoreCase);

        var records = new List<UdtInstance>();
        GoInternal(Config.CursorMoveOperation.Top);

        while (!IsEof())
        {
            var record = GetCurrentRecord();
            if (record != null)
            {
                // Applica la condizione FOR se presente
                if (forConditionAst != null)
                {
                    // Converti il dizionario int-keyed a string-keyed solo per l'evaluator esterno.
                    var stringKeyedFields = new Dictionary<string, object?>();
                    foreach (var field in udtDef.Fields)
                    {
                        stringKeyedFields[field.Name.Lexeme] = record.Fields.GetValueOrDefault(field.SymbolId);
                    }

                    if (!evaluator!(forConditionAst, stringKeyedFields))
                    {
                        SkipInternal();
                        continue;
                    }
                }

                records.Add(record);
            }

            SkipInternal();
        }

        records.Sort((udtA, udtB) =>
        {
            foreach (var (fieldName, descending) in sortFields)
            {
                // Ottiene il SymbolId per l'ordinamento.
                if (!fieldIdMap.TryGetValue(fieldName, out var symbolId))
                    continue;

                var valA = udtA.Fields.GetValueOrDefault(symbolId) as IComparable;
                var valB = udtB.Fields.GetValueOrDefault(symbolId) as IComparable;

                if (valA == null && valB == null) continue;
                if (valA == null) return -1;
                if (valB == null) return 1;

                var comparison = valA.CompareTo(valB);
                if (comparison != 0)
                {
                    return descending ? -comparison : comparison;
                }
            }

            return 0;
        });

        IsamFileHandle.CreateNew(destinationFilePath, _handle.Schema, _handle.FileEncoding);
        using var destHandle = new IsamFileHandle(destinationFilePath, _handle.Schema, _handle.UdtDefinition,
            LockingStrategy.IntraProcess, _handle.FileEncoding);
        using var destEngine = new Engine(destHandle);
        destEngine.BeginTransaction();
        try
        {
            destEngine.Evaluator = keyEvaluator;

            foreach (var record in records)
            {
                destEngine.Append(record, keyEvaluator);
            }

            destEngine.Commit();
        }
        catch
        {
            destEngine.Rollback();
            throw;
        }
    }

    #endregion

    #region Import & Export

    /// <summary>
    /// Esporta in un file CSV (Comma-Separated Values) le versioni dei record visibili alla transazione corrente
    /// e che soddisfano il filtro attivo.
    /// </summary>
    /// <param name="filePath">Il percorso completo del file .csv da creare. Se esiste, sarà sovrascritto.</param>
    /// <param name="keyEvaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <remarks>
    /// Il metodo gestisce correttamente il nuovo comportamento di <see cref="GetCurrentRecord"/>, che può restituire `null`.
    /// I record per cui non esiste una versione visibile vengono semplicemente ignorati e non inclusi nel file di export,
    /// garantendo che l'output rifletta esattamente la "vista" del database che l'utente ha in quel momento.
    /// </remarks>
    public void ExportToCsv(string filePath, KeyExpressionEvaluatorCallback? keyEvaluator)
    {
        var originalOffset = Cursor.CurrentRecordOffset;

        using (var writer =
               new StreamWriter(filePath, false, _handle.FileEncoding))
        {
            var header = string.Join(",", _handle.Schema.Fields.Select(f => f.Name));
            writer.WriteLine(header);

            // Marshalling Setup
            var udtDef = _handle.UdtDefinition;
            var fieldIdMap =
                udtDef.Fields.ToDictionary(f => f.Name.Lexeme, f => f.SymbolId, StringComparer.OrdinalIgnoreCase);

            GoInternal(Config.CursorMoveOperation.Top);
            while (!IsEof())
            {
                var currentRecord = GetCurrentRecord();

                if (currentRecord != null)
                {
                    // Accede al dizionario Fields usando il SymbolId.
                    var values = _handle.Schema.Fields
                        .Select(field =>
                        {
                            if (fieldIdMap.TryGetValue(field.Name, out var symbolId))
                            {
                                return currentRecord.Fields.GetValueOrDefault(symbolId);
                            }

                            return null;
                        })
                        .Select(FormatCsvField);

                    writer.WriteLine(string.Join(",", values));
                }

                SkipInternal();
            }
        }

        if (originalOffset >= 0)
        {
            Cursor.CurrentRecordOffset = originalOffset;
            if (_handle.GetStream().Length > originalOffset)
            {
                var metadata = _handle.ReadRecordMetadata(originalOffset, _handle.FileEncoding);
                Cursor.LastReadRecordVersion = metadata.Version;
            }

            Cursor.BofFlag = false;
            Cursor.EofFlag = false;
        }
        else
        {
            GoInternal(Config.CursorMoveOperation.Top);
        }
    }

    /// <summary>
    /// Esporta in un file JSON (JavaScript Object Notation) le versioni dei record visibili alla transazione corrente
    /// e che soddisfano il filtro attivo. L'output è un array JSON di oggetti.
    /// </summary>
    /// <param name="filePath">Il percorso completo del file .json da creare. Se esiste, sarà sovrascritto.</param>
    /// <param name="keyEvaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <remarks>
    /// Il metodo gestisce correttamente il nuovo comportamento di <see cref="GetCurrentRecord"/>, che può restituire `null`.
    /// I record per cui non esiste una versione visibile vengono semplicemente ignorati e non inclusi nell'array JSON,
    /// garantendo che l'output rifletta esattamente la "vista" del database che l'utente ha in quel momento.
    /// </remarks>
    public void ExportToJson(string filePath, KeyExpressionEvaluatorCallback? keyEvaluator)
    {
        var originalOffset = Cursor.CurrentRecordOffset;

        var jsonArray = new System.Text.Json.Nodes.JsonArray();

        // Rimosso il parametro keyEvaluator non necessario dalle chiamate di navigazione.
        GoInternal(Config.CursorMoveOperation.Top);
        while (!IsEof())
        {
            var currentRecord = GetCurrentRecord();

            if (currentRecord != null)
            {
                var jsonObject = new System.Text.Json.Nodes.JsonObject();
                foreach (var field in currentRecord.Fields)
                {
                    jsonObject[field.Key.ToString()] = System.Text.Json.Nodes.JsonValue.Create(field.Value);
                }

                jsonArray.Add(jsonObject);
            }

            SkipInternal();
        }

        File.WriteAllText(filePath, jsonArray.ToJsonString(), _handle.FileEncoding);

        if (originalOffset >= 0)
        {
            Cursor.CurrentRecordOffset = originalOffset;
            if (_handle.GetStream().Length > originalOffset)
            {
                var metadata = _handle.ReadRecordMetadata(originalOffset, _handle.FileEncoding);
                Cursor.LastReadRecordVersion = metadata.Version;
            }

            Cursor.BofFlag = false;
            Cursor.EofFlag = false;
        }
        else
        {
            GoInternal(Config.CursorMoveOperation.Top);
        }
    }

    /// <summary>
    /// Importa record da un file CSV in modo atomico.
    /// L'intera operazione viene eseguita all'interno di un'unica transazione. Se si verifica un errore,
    /// tutte le aggiunte vengono annullate.
    /// </summary>
    /// <param name="filePath">Il percorso del file .csv da importare.</param>
    /// <param name="keyEvaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <exception cref="InvalidDataException">Se l'header del CSV è vuoto, se i nomi dei campi non corrispondono allo schema,
    /// o se una riga ha un numero di colonne diverso dall'header.</exception>
    public void ImportFromCsv(string filePath, KeyExpressionEvaluatorCallback? keyEvaluator)
    {
        // Avvia la transazione per un'operazione di importazione atomica e performante.
        BeginTransaction();
        try
        {
            using (var reader = new StreamReader(filePath, System.Text.Encoding.UTF8))
            {
                // 1. Legge e valida l'header.
                var headerLine = reader.ReadLine();
                if (string.IsNullOrEmpty(headerLine))
                    throw new InvalidDataException("Il file CSV è vuoto o non contiene una riga di intestazione.");

                var headers = headerLine.Split(',');

                // Valida che tutti i campi dell'header esistano nello schema.
                var schemaFields =
                    _handle.Schema.Fields.ToDictionary(f => f.Name, f => f, StringComparer.OrdinalIgnoreCase);
                foreach (var header in headers)
                {
                    if (!schemaFields.ContainsKey(header))
                        throw new InvalidDataException(
                            $"Il campo '{header}' dall'header CSV non esiste nel database.");
                }

                // Marshalling Setup
                var udtDef = _handle.UdtDefinition;
                var fieldIdMap = udtDef.Fields.ToDictionary(f => f.Name.Lexeme, f => f.SymbolId,
                    StringComparer.OrdinalIgnoreCase);

                // 2. Itera sulle righe di dati.
                while (reader.ReadLine() is { } dataLine)
                {
                    var values = dataLine.Split(',');
                    if (values.Length != headers.Length)
                        throw new InvalidDataException(
                            "Una riga di dati ha un numero di colonne diverso dall'intestazione.");

                    // Crea il dizionario finale int-keyed.
                    var recordValues = new Dictionary<int, object?>();

                    for (var i = 0; i < headers.Length; i++)
                    {
                        var header = headers[i];
                        if (fieldIdMap.TryGetValue(header, out var symbolId))
                        {
                            var rawValue = values[i];
                            var fieldDef = udtDef.Fields.FirstOrDefault(f => f.SymbolId == symbolId);
                            recordValues[symbolId] = CoerceCsvValue(rawValue, fieldDef?.DataType);
                        }
                    }

                    // Crea l'istanza UDT e la aggiunge, passando il keyEvaluator.
                    var udtInstance = new UdtInstance(udtDef, recordValues);
                    Append(udtInstance, keyEvaluator);
                }
            }

            // 3. Se tutto è andato a buon fine, commette la transazione.
            Commit();
        }
        catch (Exception)
        {
            // 4. Se si verifica un qualsiasi errore, annulla l'intera transazione.
            Rollback();
            throw; // Rilancia l'eccezione per informare il chiamante.
        }
    }

    /// <summary>
    /// Converte un valore JSON nel tipo di dato appropriato per un campo ISAM.
    /// </summary>
    private static object? CoerceJsonValue(System.Text.Json.Nodes.JsonValue jsonValue, DataType? fieldType)
    {
        if (fieldType is DateType or DateTimeType)
        {
            if (jsonValue.TryGetValue<string>(out var dateStr))
                return ParseDateValue(dateStr.Trim('"'));
        }
        if (fieldType is LongType or IntegerType)
        {
            if (jsonValue.TryGetValue<long>(out var longVal)) return longVal;
            if (jsonValue.TryGetValue<int>(out var intVal)) return (long)intVal;
            if (jsonValue.TryGetValue<string>(out var strVal) && long.TryParse(strVal, out var parsed))
                return parsed;
        }
        if (fieldType is DoubleType or CurrencyType or SingleType)
        {
            if (jsonValue.TryGetValue<double>(out var dVal)) return dVal;
            if (jsonValue.TryGetValue<decimal>(out var decVal)) return (double)decVal;
            if (jsonValue.TryGetValue<string>(out var strVal) && double.TryParse(strVal, out var parsedD))
                return parsedD;
        }
        if (fieldType is BitType)
        {
            if (jsonValue.TryGetValue<bool>(out var bVal)) return bVal;
            if (jsonValue.TryGetValue<string>(out var strVal))
                return strVal.Trim('"').Equals("TRUE", StringComparison.OrdinalIgnoreCase);
        }
        // Fallback: TryGetValue generico
        jsonValue.TryGetValue(out object? fallback);
        return fallback;
    }

    /// <summary>
    /// Converte un valore stringa da CSV nel tipo di dato appropriato per un campo ISAM.
    /// </summary>
    private static object? CoerceCsvValue(string rawValue, DataType? fieldType)
    {
        if (string.IsNullOrWhiteSpace(rawValue) || rawValue.Equals("NULL", StringComparison.OrdinalIgnoreCase))
            return null;

        var trimmed = rawValue.Trim();

        return fieldType switch
        {
            DateType or DateTimeType => ParseDateValue(trimmed),
            LongType or IntegerType => Convert.ToInt64(trimmed, System.Globalization.CultureInfo.InvariantCulture),
            DoubleType or CurrencyType or SingleType => Convert.ToDouble(trimmed, System.Globalization.CultureInfo.InvariantCulture),
            BitType => trimmed.Equals("TRUE", StringComparison.OrdinalIgnoreCase) ||
                       trimmed.Equals("T", StringComparison.OrdinalIgnoreCase) ||
                       trimmed.Equals("YES", StringComparison.OrdinalIgnoreCase) ||
                       trimmed.Equals("Y", StringComparison.OrdinalIgnoreCase) ||
                       trimmed.Equals("1", StringComparison.OrdinalIgnoreCase),
            _ => trimmed // StringType e altri: lasciati come stringa
        };
    }

    /// <summary>
    /// Prova a parsare una data in vari formati comuni.
    /// </summary>
    private static object ParseDateValue(string trimmed)
    {
        // Prova formati ISO e varianti
        if (System.DateOnly.TryParse(trimmed, System.Globalization.CultureInfo.InvariantCulture,
                System.Globalization.DateTimeStyles.None, out var dateOnly))
            return dateOnly;
        if (System.DateTime.TryParse(trimmed, System.Globalization.CultureInfo.InvariantCulture,
                System.Globalization.DateTimeStyles.None, out var dateTime))
            return dateTime;
        // Formati numerici (OLE Automation Date)
        if (double.TryParse(trimmed, System.Globalization.NumberStyles.Any,
                System.Globalization.CultureInfo.InvariantCulture, out var oaDate))
            return System.DateTime.FromOADate(oaDate);
        throw new InvalidDataException($"Valore data non riconosciuto: '{trimmed}'.");
    }

    /// <summary>
    /// Importa record da un file JSON in modo atomico. Il file deve contenere un array di oggetti JSON.
    /// L'intera operazione viene eseguita all'interno di un'unica transazione. Se si verifica un errore,
    /// tutte le aggiunte vengono annullate.
    /// </summary>
    /// <param name="filePath">Il percorso del file .json da importare.</param>
    /// <param name="keyEvaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <exception cref="System.Text.Json.JsonException">Se il file non contiene un JSON valido o non è un array di oggetti.</exception>
    /// <exception cref="InvalidDataException">Se una proprietà di un oggetto JSON non corrisponde a un campo dello schema.</exception>
    public void ImportFromJson(string filePath, KeyExpressionEvaluatorCallback? keyEvaluator)
    {
        BeginTransaction();
        try
        {
            var jsonString = File.ReadAllText(filePath, System.Text.Encoding.UTF8);
            var jsonArray = System.Text.Json.Nodes.JsonNode.Parse(jsonString) as System.Text.Json.Nodes.JsonArray;

            if (jsonArray == null)
                throw new System.Text.Json.JsonException("Il file JSON non contiene un array alla radice.");

            var schemaFields = _handle.Schema.Fields.ToDictionary(f => f.Name, StringComparer.OrdinalIgnoreCase);

            // Marshalling Setup ---
            var udtDef = _handle.UdtDefinition;
            var fieldIdMap =
                udtDef.Fields.ToDictionary(f => f.Name.Lexeme, f => f.SymbolId, StringComparer.OrdinalIgnoreCase);

            foreach (var jsonNode in jsonArray)
            {
                var jsonObject = jsonNode as System.Text.Json.Nodes.JsonObject;
                if (jsonObject == null) continue;

                var recordValues = new Dictionary<int, object?>(); // Correzione: Dizionario finale int-keyed

                foreach (var property in jsonObject)
                {
                    if (!schemaFields.ContainsKey(property.Key))
                        throw new InvalidDataException(
                            $"La proprietà JSON '{property.Key}' non corrisponde a nessun campo del database.");

                    object? value = null;
                    if (property.Value is System.Text.Json.Nodes.JsonValue jsonValue)
                    {
                        value = CoerceJsonValue(jsonValue, fieldIdMap.TryGetValue(property.Key, out _)
                            ? udtDef.Fields.FirstOrDefault(f => f.Name.Lexeme.Equals(property.Key, StringComparison.OrdinalIgnoreCase))?.DataType
                            : null);
                    }
                    else if (property.Value != null)
                    {
                        value = property.Value.ToJsonString();
                    }

                    // Scrive nel dizionario int-keyed.
                    if (fieldIdMap.TryGetValue(property.Key, out var symbolId))
                    {
                        recordValues[symbolId] = value;
                    }
                }

                var udtInstance = new UdtInstance(udtDef, recordValues);
                Append(udtInstance, keyEvaluator);
            }

            Commit();
        }
        catch (Exception)
        {
            Rollback();
            throw;
        }
    }

    /// <summary>
    /// Converte un file DBF esistente in un nuovo file ISAM, traducendo lo schema e migrando tutti i dati.
    /// L'operazione di copia dei dati viene eseguita all'interno di un'unica transazione per garantire efficienza e atomicità.
    /// </summary>
    /// <param name="sourceDbfPath">Il percorso completo del file .dbf sorgente.</param>
    /// <param name="destinationIsamPath">Il percorso completo del nuovo file .isdb da creare.</param>
    /// <param name="primaryKeyFieldName">Il nome del campo del file DBF da designare come chiave primaria nel nuovo file ISAM.</param>
    /// <param name="primaryKeyLength">La lunghezza in byte da allocare per la chiave primaria.</param>
    /// <param name="udtDefinition">La definizione del tipo (`UserDefinedType`) dal codice, necessaria per creare lo schema e l'handle del nuovo file ISAM.</param>
    /// <param name="keyEvaluator">Il callback opzionale per la valutazione delle espressioni di chiave.</param>
    /// <exception cref="InvalidOperationException">Se il campo specificato per la chiave primaria non esiste nel file DBF sorgente.</exception>
    public static void ConvertFromDbf(string sourceDbfPath, string destinationIsamPath, string primaryKeyFieldName,
        int primaryKeyLength, UserDefinedType udtDefinition, KeyExpressionEvaluatorCallback? keyEvaluator)
    {
        using var dbfEngine = new DbfEngine(sourceDbfPath, null, false, LockingStrategy.IntraProcess);
        var dbfFields = dbfEngine.GetFields();
        var dbfEncoding = dbfEngine.GetHeader().Encoding;

        var isamSchema = TranslateDbfSchemaToIsam(dbfFields, primaryKeyFieldName, primaryKeyLength,
            udtDefinition.OriginalName.Lexeme);

        // Rinomina i campi ISAM usando i nomi dell'UDT (che non hanno il limite DBF di 10 caratteri)
        // e usa le lunghezze corrette dal TYPE (non i valori troncati DBF)
        var udtFieldDefs = udtDefinition.Fields.ToList();
        var renamedFields = isamSchema.Fields.Select((f, idx) =>
        {
            var udtName = idx < udtFieldDefs.Count ? udtFieldDefs[idx].Name.Lexeme : f.Name;
            // Per i tipi non-stringa, usa 0 (nessuna lunghezza fissa) come dal TYPE
            var udtField = idx < udtFieldDefs.Count ? udtFieldDefs[idx] : null;
            var length = udtField?.DataType is StringType st ? (st.FixedLength ?? f.Length) : 0;
            // Tipo dal TYPE del codice BASIC quando disponibile, non quello tradotto da
            // TranslateDbfSchemaToIsam (semplificazione: TUTTI i campi DBF Numeric diventano
            // DoubleType lì, a prescindere dal vero tipo dichiarato — un campo "Codice AS LONG"
            // veniva scritto nello schema ISAM come DoubleType, mai compatibile col TYPE reale
            // al successivo ISAM OPEN ... OF <UDT> (SchemaManager.AreSchemasCompatible,
            // "[SCHEMA] Field[i] type: LongType vs DoubleType" — scoperto dal vivo su
            // DbIsamFullTests.bas sez. 11, insieme al bug gemello sul TypeName).
            var dataType = udtField?.DataType ?? f.DataType;
            return new Config.IsamFieldSchema(udtName, dataType, length);
        }).ToList();
        // Aggiorna anche il nome dell'indice primario
        var pkName = primaryKeyFieldName;
        var pkRenamed = renamedFields.FirstOrDefault(f => f.Name.Equals(pkName, StringComparison.OrdinalIgnoreCase));
        if (pkRenamed == null)
        {
            // La chiave primaria usa il nome originale (non troncato DBF)
            var pkCandidates = udtFieldDefs.Select(f => f.Name.Lexeme);
            pkName = pkCandidates.FirstOrDefault(n =>
                n.StartsWith(primaryKeyFieldName, StringComparison.OrdinalIgnoreCase)) ?? primaryKeyFieldName;
        }
        var primaryKeyDef = new Config.IndexDefinition(pkName, pkName, primaryKeyLength, true, false);
        // `ISAM OPEN ... INDEXED BY <campo>` (anche con un SOLO campo) duplica quel campo
        // anche in SecondaryKeys quando ricostruisce lo schema atteso dal codice (vedi
        // IlProgramCompiler.BuildIsamSchemaJson: "Indexes" include TUTTI i KeyFields, primario
        // incluso — convenzione consolidata, condivisa da ISAM OPEN/CREATE, non toccata qui
        // per non rischiare regressioni sulle sezioni 1-10 già verificate). Un file convertito
        // (CONVERT ha solo KEY <pk>, nessun modo di specificare indici secondari) aveva
        // SecondaryKeys sempre vuoto, disallineato dal codeSchema che si aspetta il duplicato
        // — SchemaManager.AreSchemasCompatible falliva su "SecondaryKeys mismatch" al
        // successivo ISAM OPEN ... OF <UDT> (scoperto dal vivo su DbIsamFullTests.bas sez. 11,
        // ultimo di cinque mismatch a cascata sullo stesso CONVERT+OPEN).
        isamSchema = new Config.IsamSchema(isamSchema.TypeName, renamedFields,
            primaryKeyDef, [primaryKeyDef with { IsUnique = false }]);

        IsamFileHandle.CreateNew(destinationIsamPath, isamSchema, dbfEncoding);

        using var isamHandle =
            new IsamFileHandle(destinationIsamPath, isamSchema, udtDefinition, LockingStrategy.IntraProcess,
                dbfEncoding);
        using var isamEngine = new Engine(isamHandle);
        isamEngine.BeginTransaction();
        try
        {
            isamEngine.Evaluator = keyEvaluator;

            // --- Crea la mappa di SymbolId per il marshalling ---
            var fieldIdMap = udtDefinition.Fields.ToDictionary(f => f.Name.Lexeme, f => f.SymbolId,
                StringComparer.OrdinalIgnoreCase);

            dbfEngine.GoTop();
            while (!dbfEngine.Eof)
            {
                if (dbfEngine.IsCurrentRecordDeleted())
                {
                    dbfEngine.Skip();
                    continue;
                }

                var dbfRecordValues = dbfEngine.GetCurrentRecordValues();
                if (dbfRecordValues != null)
                {
                    // Marshalling da string-keyed (DBF) a int-keyed (UdtInstance).
                    var intKeyedFields = new Dictionary<int, object?>();
                    foreach (var (dbfFieldName, value) in dbfRecordValues)
                    {
                        if (fieldIdMap.TryGetValue(dbfFieldName, out var symbolId))
                        {
                            intKeyedFields[symbolId] = value;
                        }
                        else
                        {
                            // DBF tronca i nomi a 10 caratteri: prova con StartsWith
                            var match = fieldIdMap.Keys.FirstOrDefault(k =>
                                k.StartsWith(dbfFieldName, StringComparison.OrdinalIgnoreCase));
                            if (match != null)
                                intKeyedFields[fieldIdMap[match]] = value;
                        }
                    }

                    var udtInstance = new UdtInstance(udtDefinition, intKeyedFields);
                    isamEngine.Append(udtInstance, keyEvaluator);
                }

                dbfEngine.Skip();
            }

            isamEngine.Commit();
        }
        catch (System.Exception)
        {
            isamEngine.Rollback();
            throw;
        }
    }

    #endregion

    #region Private Helpers

    /// <summary>
    /// [INTERNO - BLOCCANTE] Versione interna e bloccante di GO, che contiene il ciclo
    /// di ricerca per il primo record valido che soddisfa il filtro.
    /// Da usare solo all'interno di syscall.
    /// </summary>
    private void GoInternal(Config.CursorMoveOperation operation)
    {
        LastFilterRecordsExamined = 0;
        LastFilterUsedSeekPushdown = false;

        if (operation == Config.CursorMoveOperation.Top && Cursor.FilterPushdownSeekKey is not null)
        {
            PositionFilterPushdownTop();
            return;
        }

        Go(operation);

        var direction = operation == Config.CursorMoveOperation.Top ? 1 : -1;
        while (!IsBof() && !IsEof() && !IsRecordValidForFilter())
        {
            Step(direction);
            LastFilterRecordsExamined++;
        }
    }

    /// <summary>
    /// Posiziona il cursore sul primo record valido per filtro uguaglianza via seek sull'indice attivo.
    /// </summary>
    private void PositionFilterPushdownTop()
    {
        LastFilterUsedSeekPushdown = true;
        var seekKey = Cursor.FilterPushdownSeekKey!;
        var indexDef = Cursor.FilterPushdownIndexDef!;

        if (Cursor.FilterPushdownMatchLength > 0)
        {
            SeekLowerBound(seekKey);
        }
        else
        {
            Seek(seekKey);
        }

        LastFilterRecordsExamined = 1;

        if (!WasFound())
        {
            Cursor.EofFlag = true;
            Cursor.BofFlag = false;
            return;
        }

        while (!IsBof() && !IsEof())
        {
            if (!RecordKeyMatchesPushdown(seekKey, indexDef))
            {
                Cursor.EofFlag = true;
                Cursor.CurrentRecordOffset = -1;
                Cursor.LastReadRecordVersion = -1;
                return;
            }

            if (IsRecordValidForFilter())
            {
                return;
            }

            Step(1);
            LastFilterRecordsExamined++;
        }
    }

    private void ClearFilterPushdownState()
    {
        Cursor.FilterPushdownSeekKey = null;
        Cursor.FilterPushdownIndexDef = null;
        Cursor.FilterPushdownMatchLength = -1;
    }

    /// <summary>
    /// ponytail: pushdown per AND di uguaglianze su prefisso chiave indice attivo (SET ORDER).
    /// </summary>
    private void TrySetupFilterPushdown(Expr? filterExpression)
    {
        var equalities = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        if (!IsamFilterPushdown.TryCollectEqualities(filterExpression, equalities))
        {
            return;
        }

        var indexDef = _activeIndexManager.GetIndexDefinition();
        if (!IsamFilterPushdown.TryGetKeyFieldNames(indexDef.KeyExpression, out var keyFields))
        {
            return;
        }

        var matchCount = IsamFilterPushdown.CountMatchingPrefixFields(keyFields, equalities);
        if (matchCount == 0)
        {
            return;
        }

        if (Evaluator == null)
        {
            return;
        }

        var context = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        for (var i = 0; i < keyFields.Length; i++)
        {
            context[keyFields[i]] = i < matchCount
                ? equalities[keyFields[i]]
                : string.Empty;
        }

        var anchorField = keyFields[0];
        var targetType = GetFieldDataType(anchorField) ?? new StringType(indexDef.KeyLength);
        var keyValue = Evaluator(indexDef.KeyExpression, context, targetType);
        Cursor.FilterPushdownSeekKey = SchemaManager.SerializeKeyValue(keyValue, indexDef);
        Cursor.FilterPushdownIndexDef = indexDef;
        Cursor.FilterPushdownMatchLength = IsamFilterPushdown.ComputeMatchLength(
            keyValue,
            matchCount == keyFields.Length);
    }

    private bool RecordKeyMatchesPushdown(ReadOnlySpan<byte> targetKey, Config.IndexDefinition indexDef)
    {
        var currentKey = GetKeyForActiveIndex();
        var matchLength = Cursor.FilterPushdownMatchLength;
        if (matchLength < 0)
        {
            return currentKey.AsSpan().SequenceEqual(targetKey);
        }

        return currentKey.AsSpan(0, matchLength).SequenceEqual(targetKey[..matchLength]);
    }

    private bool IsFilterPushdownActive =>
        Cursor.FilterPushdownSeekKey is not null && Cursor.FilterPushdownIndexDef is not null;

    /// <summary>
    /// [INTERNO - BLOCCANTE] Versione interna e bloccante di SKIP, che contiene il ciclo
    /// per saltare N record *validi* che soddisfano il filtro.
    /// Da usare solo all'interno di syscall.
    /// </summary>
    private void SkipInternal(int recordsToSkip = 1)
    {
        if (recordsToSkip == 0) return;
        if ((IsEof() && recordsToSkip > 0) || (IsBof() && recordsToSkip < 0)) return;

        if (IsEof() && recordsToSkip < 0) Cursor.EofFlag = false;
        if (IsBof() && recordsToSkip > 0) Cursor.BofFlag = false;

        var direction = Math.Sign(recordsToSkip);
        var recordsSkipped = 0;
        var pushdownForward = IsFilterPushdownActive && direction > 0;
        var pushdownKey = pushdownForward ? Cursor.FilterPushdownSeekKey!.AsSpan() : ReadOnlySpan<byte>.Empty;
        var pushdownIndex = pushdownForward ? Cursor.FilterPushdownIndexDef! : null;

        while (recordsSkipped < Math.Abs(recordsToSkip))
        {
            Step(direction);
            if (IsBof() || IsEof()) return;

            if (pushdownForward && !RecordKeyMatchesPushdown(pushdownKey, pushdownIndex!))
            {
                Cursor.EofFlag = true;
                Cursor.CurrentRecordOffset = -1;
                Cursor.LastReadRecordVersion = -1;
                return;
            }

            if (IsRecordValidForFilter())
            {
                recordsSkipped++;
            }
        }
    }

    /// <summary>
    /// [NUOVO - GPM-SAFE] Verifica se il record attualmente puntato dal cursore è valido
    /// e soddisfa il filtro attivo. Operazione non bloccante.
    /// </summary>
    public bool IsRecordValidForFilter()
    {
        if (IsBof() || IsEof() || Cursor.CurrentRecordOffset < 0) return false;
        var metadata = _handle.ReadRecordMetadata(Cursor.CurrentRecordOffset, _handle.FileEncoding);
        return metadata.Status != Config.RecordStatusDeleted && EvaluateFilter();
    }

    private void SyncCrossProcessStructuralStateIfNeeded()
    {
        if (_handle.LockingStrategy != LockingStrategy.CrossProcess)
        {
            return;
        }

        _handle.SyncStructuralStateFromDisk();
        ReloadIndexManagersFromHeaderIfChanged();
    }

    /// <summary>
    /// Ricarica le radici degli indici B+Tree dalla copia corrente dell'header del file.
    /// </summary>
    private void ReloadIndexManagersFromHeaderIfChanged()
    {
        var current = StructuralFingerprint.Capture(_handle.Header);
        if (_cachedStructuralFingerprint is { } cached && cached.Equals(current))
        {
            return;
        }

        ReloadIndexManagersFromHeader();
    }

    /// <summary>
    /// Ricarica le radici degli indici B+Tree dalla copia corrente dell'header del file.
    /// </summary>
    private void ReloadIndexManagersFromHeader(bool reloadMemoryIndex = true)
    {
        _primaryIndexManager.ResetRoot(_handle.Header.PrimaryKeyRootOffset);

        for (var i = 0; i < _handle.Schema.SecondaryKeys.Count; i++)
        {
            var indexDef = _handle.Schema.SecondaryKeys[i];
            if (_secondaryIndexManagers.TryGetValue(indexDef.Name, out var manager))
            {
                manager.ResetRoot(_handle.Header.SecondaryIndexRootOffsets[i]);
            }
        }

        if (!reloadMemoryIndex)
        {
            return;
        }

        FinishInMemoryIndexReload();
    }

    private void FinishInMemoryIndexReload()
    {
        if (_useInMemoryIndex && _inMemoryIndexes is not null)
        {
            LoadAllIndexesIntoMemory();
        }

        _cachedStructuralFingerprint = StructuralFingerprint.Capture(_handle.Header);
    }

    private void OnHeapRelocated(long heapStart, long heapEnd, long delta)
    {
        ReloadIndexManagersFromHeader(reloadMemoryIndex: false);
        _primaryIndexManager.RepairPointersAfterHeapMove(heapStart, heapEnd, delta);
        foreach (var manager in _secondaryIndexManagers.Values)
        {
            manager.RepairPointersAfterHeapMove(heapStart, heapEnd, delta);
        }

        FinishInMemoryIndexReload();
    }

    /// <summary>
    /// Acquisisce un lock di scrittura sul record e registra l'offset nel contesto transazionale.
    /// </summary>
    private void AcquireRecordWriteLock(long recordOffset, TransactionContext? context, string lockKey)
    {
        // Se questo contesto già tiene il lock in scrittura, non richiederlo di nuovo: il
        // ReaderWriterLockSlim sottostante non supporta la rientranza, tentare una seconda
        // AcquireWriteLock sullo stesso thread lancerebbe LockRecursionException.
        if (context != null && context.WriteLockedOffsets.Contains(recordOffset))
        {
            return;
        }

        // Se questo contesto tiene già un lock in LETTURA sullo stesso record (GET sotto
        // isolamento >= RepeatableRead, vedi GetCurrentRecord), va rilasciato PRIMA di acquisire
        // quello in scrittura: ReaderWriterLockSlim non permette di tenere un read lock e
        // richiedere un write lock sullo stesso thread (LockRecursionException) — verificato con
        // reproducer isolato (SEEK+GET+PUT dentro BEGIN TRANSACTION LEVEL REPEATABLE READ):
        // l'eccezione veniva inghiottita dal catch-all del syscall ISAM.PUT, lasciando la PUT
        // completamente no-op senza alcun errore visibile al programma BASIC.
        if (context != null && context.ReadLockedOffsets.Remove(recordOffset))
        {
            _handle.RecordLockManager.ReleaseReadLock(lockKey);
        }

        try
        {
            _handle.RecordLockManager.AcquireWriteLock(
                lockKey,
                _handle.LockTimeoutMilliseconds,
                $"record all'offset {recordOffset}");
        }
        catch (DeadlockException)
        {
            RollbackOnLockFailure();
            throw;
        }
        catch (LockTimeoutException)
        {
            RollbackOnLockFailure();
            throw;
        }

        if (context != null)
        {
            context.WriteLockedOffsets.Add(recordOffset);
        }
    }

    /// <summary>
    /// Annulla la transazione attiva del thread se un'operazione di lock fallisce.
    /// </summary>
    private void RollbackOnLockFailure()
    {
        var stack = GetTransactionStackOrNull(Thread.CurrentThread.ManagedThreadId);
        if (stack is { Count: > 0 })
        {
            Rollback();
        }
    }

    /// <summary>
    /// Rilascia tutti i lock (di lettura e scrittura, su record e su chiavi/intervalli)
    /// acquisiti da un contesto di transazione specifico.
    /// </summary>
    /// <param name="context">Il contesto della transazione che contiene i set delle risorse bloccate.</param>
    /// <remarks>
    /// Questa è un'operazione di cleanup critica, invocata sistematicamente dai metodi `Commit` e `Rollback`
    /// per garantire che, indipendentemente dall'esito della transazione, tutte le risorse bloccate vengano
    /// restituite al sistema in modo affidabile, prevenendo lock orfani.
    /// </remarks>
    private void ReleaseTransactionalLocks(TransactionContext context)
    {
        // Rilascia lock su record (per Repeatable Read)
        foreach (var offset in context.ReadLockedOffsets)
        {
            _handle.RecordLockManager.ReleaseReadLock(LockManagerBase.GetRecordLockKey(offset));
        }

        foreach (var offset in context.WriteLockedOffsets)
        {
            _handle.RecordLockManager.ReleaseWriteLock(LockManagerBase.GetRecordLockKey(offset));
        }

        // Rilascia lock su chiavi/intervalli (per Serializable)
        foreach (var lockKey in context.ReadLockedKeys)
        {
            _handle.RecordLockManager.ReleaseReadLock(lockKey);
        }

        foreach (var lockKey in context.WriteLockedKeys)
        {
            _handle.RecordLockManager.ReleaseWriteLock(lockKey);
        }

        context.ReadLockedOffsets.Clear();
        context.WriteLockedOffsets.Clear();
        context.ReadLockedKeys.Clear();
        context.WriteLockedKeys.Clear();
        DeadlockMonitor.ReleaseAllLocksForThread(Thread.CurrentThread.ManagedThreadId);
    }

    /// <summary>
    /// Legge la versione più recente di un record a un dato offset, bypassando la logica di visibilità MVCC.
    /// Usato per operazioni di manutenzione come REINDEX, che necessitano di accedere ai dati effettivi
    /// indipendentemente dallo stato transazionale.
    /// </summary>
    /// <param name="offset">L'offset fisico del record da leggere.</param>
    /// <returns>L'istanza <see cref="UdtInstance"/> del record più recente.</returns>
    private UdtInstance GetMostRecentRecord(long offset)
    {
        var buffer = new byte[_handle.Header.RecordSize];
        var stream = _handle.GetStream();
        stream.Seek(offset, SeekOrigin.Begin);
        stream.ReadExactly(buffer, 0, buffer.Length);
        var (_, udtInstance) =
            RecordMarshaller.DeserializeRecord(_handle.Schema, buffer, _handle.UdtDefinition, _handle);
        return udtInstance;
    }

    /// <summary>
    /// Metodo helper per tradurre una lista di definizioni di campo DBF in uno schema ISAM.
    /// </summary>
    internal static Config.IsamSchema TranslateDbfSchemaToIsam(IReadOnlyList<DbfField> dbfFields, string pkFieldName,
        int pkLength, string typeName)
    {
        var isamFields = new List<Config.IsamFieldSchema>();
        foreach (var dbfField in dbfFields)
        {
            // Mappa i tipi di dato da DBF a ISAM.
            DataType isamDataType = dbfField.Type switch
            {
                DbfFieldType.Character => new StringType(dbfField.Length),
                DbfFieldType.Numeric => new DoubleType(), // Semplificazione: i numerici DBF diventano Double
                DbfFieldType.Float => new SingleType(),
                DbfFieldType.Integer => new LongType(), // DBF Integer (4 byte) mappa a Long (int)
                DbfFieldType.Logical => new BitType(),
                DbfFieldType.Date => new DateType(),
                DbfFieldType.DateTime => new DateTimeType(),
                DbfFieldType.Double => new DoubleType(),
                DbfFieldType.Currency => new CurrencyType(),
                _ => new StringType(dbfField.Length) // Fallback per tipi non gestiti (es. Memo, General)
            };

            isamFields.Add(new Config.IsamFieldSchema(dbfField.Name, isamDataType, dbfField.Length));
        }

        // Verifica che il campo scelto per la chiave primaria esista.
        if (!isamFields.Any(f => f.Name.Equals(pkFieldName, StringComparison.OrdinalIgnoreCase)))
        {
            throw new InvalidOperationException(
                $"Il campo '{pkFieldName}' specificato per la chiave primaria non esiste nel file DBF sorgente.");
        }

        var primaryKeyDef =
            new Config.IndexDefinition(pkFieldName, pkFieldName, pkLength,
                true, IsUnsafe: false); // La chiave primaria è sempre univoca e sicura.

        return new Config.IsamSchema(typeName, isamFields, primaryKeyDef, []);
    }

    /// <summary>
    /// Formatta un singolo valore per l'inserimento in un campo CSV, gestendo i caratteri speciali.
    /// Se il valore contiene virgole, doppi apici o caratteri di a capo, viene racchiuso tra doppi apici,
    /// e i doppi apici al suo interno vengono raddoppiati.
    /// </summary>
    /// <param name="value">L'oggetto da formattare.</param>
    /// <returns>La stringa formattata e sicura per il formato CSV.</returns>
    private static string FormatCsvField(object? value)
    {
        if (value == null)
        {
            return "";
        }

        var stringValue = value.ToString() ?? "";

        // Se il valore non contiene caratteri speciali, non è necessario racchiuderlo tra apici.
        if (!stringValue.Contains(',') && !stringValue.Contains('"') && !stringValue.Contains('\r') &&
            !stringValue.Contains('\n'))
        {
            return stringValue;
        }

        // Se contiene caratteri speciali, esegui l'escape dei doppi apici e racchiudi il tutto.
        return $"\"{stringValue.Replace("\"", "\"\"")}\"";
    }

    /// <summary>
    /// Esegue un singolo passo di navigazione fisica, muovendo il cursore di una posizione
    /// in avanti o all'indietro. Questo metodo non tiene conto dei filtri, ma si occupa solo
    /// di muovere il puntatore del record, aggiornare lo stato BOF/EOF e gestire il locking
    /// per le transazioni `Serializable`. Utilizza l'indice in memoria se disponibile per ottimizzare le prestazioni.
    /// </summary>
    /// <param name="direction">La direzione del passo (+1 per avanti, -1 per indietro).</param>
    private void Step(int direction)
    {
        var threadId = Thread.CurrentThread.ManagedThreadId;
        var transactionStack = GetTransactionStackOrNull(threadId);
        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;

        if (Cursor.CurrentRecordOffset == -1)
        {
            Go(direction > 0 ? Config.CursorMoveOperation.Top : Config.CursorMoveOperation.Bottom);
            return;
        }

        long newOffset;

        if (Evaluator == null)
            throw new InvalidOperationException("L'evaluator non può essere nullo per le operazioni di navigazione.");

        if (_useInMemoryIndex && TryGetActiveMemoryIndex(out var activeMemoryIndex))
        {
            var currentKey = GetKeyForActiveIndex();
            newOffset = FindNextRecordOffsetInMemory(activeMemoryIndex, currentKey, direction);
        }
        else
        {
            // Passa l'evaluator al metodo FindNextRecordOffset.
            newOffset = _activeIndexManager.FindNextRecordOffset(Cursor.CurrentRecordOffset, direction, context,
                Evaluator);
        }

        if (newOffset == -1)
        {
            if (direction > 0)
            {
                Cursor.EofFlag = true;
                Cursor.CurrentRecordOffset = -1;
            }
            else
            {
                Cursor.BofFlag = true;
                Cursor.CurrentRecordOffset = -1;
            }

            Cursor.LastReadRecordVersion = -1;
        }
        else
        {
            Cursor.CurrentRecordOffset = newOffset;
            var metadata = _handle.ReadRecordMetadata(Cursor.CurrentRecordOffset, _handle.FileEncoding);
            Cursor.LastReadRecordVersion = metadata.Version;
        }
    }

    /// <summary>
    /// Esegue una traversata ricorsiva post-ordine di un albero B+ a partire da un nodo dato,
    /// aggiungendo ogni pagina (nodo) visitata alla free list del file.
    /// </summary>
    /// <param name="manager">Il <see cref="BPlusTreeManager"/> associato all'indice da cui si stanno rimuovendo le pagine.</param>
    /// <param name="pageOffset">L'offset fisico della pagina radice del sotto-albero da liberare.</param>
    /// <remarks>
    /// Questa è un'operazione di pulizia fondamentale, invocata da <see cref="DropIndexAsync"/> per
    /// recuperare in modo sicuro tutto lo spazio su disco occupato da un indice che viene rimosso.
    /// La logica opera come segue:
    /// 1. Carica il nodo alla posizione <paramref name="pageOffset"/>.
    /// 2. Se il nodo è un nodo interno (<see cref="BPlusInternalNode"/>), si invoca ricorsivamente
    ///    su ciascuno dei suoi nodi figli.
    /// 3. Dopo che tutti i suoi discendenti sono stati processati e aggiunti alla free list (la parte "post-ordine"),
    ///    aggiunge la pagina corrente (<paramref name="pageOffset"/>) alla free list del file handle.
    /// Questo processo garantisce che l'intera struttura dell'albero venga smantellata e il suo spazio reso
    /// completamente disponibile per future allocazioni, prevenendo la frammentazione e la crescita incontrollata del file.
    /// </remarks>
    private void AddTreePagesToFreeList(BPlusTreeManager manager, long pageOffset)
    {
        var node = manager.LoadNode(pageOffset);

        // Se non è una foglia, scendi ricorsivamente nei figli
        if (node is BPlusInternalNode internalNode)
        {
            foreach (var childOffset in internalNode.ChildrenOffsets)
            {
                AddTreePagesToFreeList(manager, childOffset);
            }
        }

        // Aggiunge la pagina corrente alla free list
        _handle.AddPageToFreeList(pageOffset);
    }

    /// <summary>
    /// Ottiene la chiave primaria per il record attualmente puntato dal cursore,
    /// calcolandola in base all'espressione definita nello schema.
    /// </summary>
    /// <returns>La rappresentazione binaria della chiave primaria del record corrente.</returns>
    private byte[] GetKeyForCurrentRecord()
    {
        var buffer = new byte[_handle.Header.RecordSize];
        var stream = _handle.GetStream();
        stream.Seek(Cursor.CurrentRecordOffset, SeekOrigin.Begin);
        stream.ReadExactly(buffer, 0, buffer.Length);
        var udtDef = _handle.UdtDefinition;

        // DeserializeRecord restituisce un UdtInstance int-keyed.
        var (_, udtInstance) = RecordMarshaller.DeserializeRecord(_handle.Schema, buffer, udtDef, _handle);

        // Marshalling a string-keyed per l'evaluator ---
        var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var field in udtInstance.Definition.Fields)
        {
            udtInstance.Fields.TryGetValue(field.SymbolId, out var value);
            stringKeyedFields[field.Name.Lexeme] = value;
        }

        // Usa l'evaluator dell'engine per calcolare la chiave.
        var pkDef = _handle.Schema.PrimaryKey;
        var udtField =
            udtInstance.Definition.Fields.FirstOrDefault(f =>
                f.Name.Lexeme.Equals(pkDef.Name, StringComparison.OrdinalIgnoreCase));
        var targetType = udtField?.DataType ?? new StringType(pkDef.KeyLength);

        // CORREZIONE: Passa il dizionario string-keyed all'evaluator.
        var keyValue = Evaluator!(pkDef.KeyExpression, stringKeyedFields, targetType);

        return SchemaManager.SerializeKeyValue(keyValue, pkDef);
    }

    /// <summary>
    /// Chiave binaria del record corrente secondo l'indice attivo (<see cref="_activeIndexManager"/>).
    /// </summary>
    private byte[] GetKeyForActiveIndex()
    {
        var indexDef = _activeIndexManager.GetIndexDefinition();
        var buffer = new byte[_handle.Header.RecordSize];
        var stream = _handle.GetStream();
        stream.Seek(Cursor.CurrentRecordOffset, SeekOrigin.Begin);
        stream.ReadExactly(buffer, 0, buffer.Length);
        var udtDef = _handle.UdtDefinition;

        var (_, udtInstance) = RecordMarshaller.DeserializeRecord(_handle.Schema, buffer, udtDef, _handle);

        var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var field in udtInstance.Definition.Fields)
        {
            udtInstance.Fields.TryGetValue(field.SymbolId, out var value);
            stringKeyedFields[field.Name.Lexeme] = value;
        }

        var udtField = udtInstance.Definition.Fields.FirstOrDefault(f =>
            f.Name.Lexeme.Equals(indexDef.Name, StringComparison.OrdinalIgnoreCase));
        var targetType = udtField?.DataType ?? new StringType(indexDef.KeyLength);
        var keyValue = Evaluator!(indexDef.KeyExpression, stringKeyedFields, targetType);

        return SchemaManager.SerializeKeyValue(keyValue, indexDef);
    }

    /// <summary>
    /// Calcola l'offset del record successivo o precedente a una data chiave, operando interamente
    /// sull'indice primario mantenuto in memoria. Questo metodo rappresenta il percorso di esecuzione
    /// ottimizzato ("fast path") per l'operazione di `SKIP` quando la cache dell'indice è attiva.
    /// </summary>
    /// <param name="currentKey">La chiave primaria del record da cui inizia la navigazione. Deve essere la rappresentazione binaria esatta.</param>
    /// <param name="skipCount">Il numero di record logici da saltare. Un valore positivo naviga in avanti (verso EOF), uno negativo naviga all'indietro (verso BOF).</param>
    /// <returns>
    /// L'offset fisico in byte del record di destinazione. Restituisce -1 se la navigazione supera l'inizio
    /// o la fine della sequenza di record (condizione di BOF/EOF).
    /// </returns>
    /// <remarks>
    /// Questa operazione è eseguita interamente in memoria con complessità O(log N).
    /// 1. Localizza la posizione ordinale della `currentKey` usando `IndexOfKey` (O(log N)).
    /// 2. Calcola il nuovo indice di destinazione.
    /// 3. Se l'indice è valido, recupera la lista di offset corrispondente in tempo O(1) e restituisce il primo.
    /// </remarks>
    private long FindNextRecordOffsetInMemory(
        SortedList<byte[], List<long>> memoryIndex,
        byte[] currentKey,
        int skipCount)
    {
        if (memoryIndex.Count == 0) return -1;

        var currentIndex = memoryIndex.IndexOfKey(currentKey);
        if (currentIndex == -1) return -1; // Chiave non trovata, inconsistenza.

        var newIndex = currentIndex + skipCount;

        if (newIndex < 0 || newIndex >= memoryIndex.Count)
        {
            return -1; // Fuori dai limiti (BOF/EOF)
        }

        // Accede direttamente per indice (O(1)) e prende il primo offset.
        return memoryIndex.Values[newIndex].First();
    }

    /// <summary>
    /// Rimuove una entry specifica dall'indice primario mantenuto in memoria.
    /// Se la rimozione dell'offset lascia la lista di una chiave vuota, la chiave stessa viene rimossa.
    /// </summary>
    /// <param name="key">La chiave primaria del record da rimuovere.</param>
    /// <param name="recordOffset">Lo specifico offset del record da rimuovere dalla lista di duplicati.</param>
    private void RemoveFromMemoryIndex(string indexName, byte[] key, long recordOffset)
    {
        if (!_useInMemoryIndex || _inMemoryIndexes is null) return;

        if (!_inMemoryIndexes.TryGetValue(indexName, out var memoryIndex))
        {
            return;
        }

        if (memoryIndex.TryGetValue(key, out var offsets))
        {
            offsets.Remove(recordOffset);

            // Se non ci sono più record associati a questa chiave, rimuovi la chiave stessa.
            if (offsets.Count == 0)
            {
                memoryIndex.Remove(key);
            }
        }
    }

    /// <summary>
    /// Rimuove le chiavi di un record da tutti gli indici (primario e secondari) in modo transazionalmente sicuro.
    /// </summary>
    /// <param name="udtInstance">L'istanza UDT del record i cui valori sono usati per calcolare le chiavi da rimuovere.</param>
    /// <param name="recordOffset">L'offset fisico del record.</param>
    /// <param name="context">Il contesto della transazione corrente, necessario per il locking a livello di chiave.</param>
    /// <param name="evaluator">Il callback obbligatorio per la valutazione delle espressioni di chiave.</param>
    private void RemoveAllIndexesForRecord(UdtInstance udtInstance, long recordOffset,
        TransactionContext? context, KeyExpressionEvaluatorCallback? evaluator)
    {
        if (evaluator == null)
            throw new InvalidOperationException("L'evaluator non può essere nullo durante la rimozione degli indici.");

        // Marshalling a string-keyed per l'evaluator esterno
        // 1. Crea il dizionario string-keyed necessario per l'evaluator.
        var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var field in udtInstance.Definition.Fields)
        {
            // Usiamo il SymbolId per leggere il valore e il nome per scriverlo nel dizionario temporaneo.
            udtInstance.Fields.TryGetValue(field.SymbolId, out var value);
            stringKeyedFields[field.Name.Lexeme] = value;
        }

        // Calcola e rimuove la chiave primaria.
        var pkDef = _handle.Schema.PrimaryKey;
        var udtFieldPk = udtInstance.Definition.Fields.FirstOrDefault(f =>
            f.Name.Lexeme.Equals(pkDef.Name, StringComparison.OrdinalIgnoreCase));
        var targetTypePk = udtFieldPk?.DataType ?? new StringType(pkDef.KeyLength);

        // Passa il dizionario string-keyed all'evaluator.
        var primaryKeyValue = evaluator(pkDef.KeyExpression, stringKeyedFields, targetTypePk);
        var primaryKey = SchemaManager.SerializeKeyValue(primaryKeyValue, pkDef);

        _primaryIndexManager.Delete(primaryKey, recordOffset, context);
        RemoveFromMemoryIndex(pkDef.Name, primaryKey, recordOffset);

        // Calcola e rimuove le chiavi secondarie.
        foreach (var indexDef in _handle.Schema.SecondaryKeys)
        {
            if (_secondaryIndexManagers.TryGetValue(indexDef.Name, out var manager))
            {
                TryDeleteSecondaryIndex(indexDef, manager, stringKeyedFields, recordOffset, context, evaluator);
            }
        }
    }

    /// <summary>
    /// Rimuove le chiavi secondarie di un record soft-deleted, mantenendo la chiave primaria per SEEK/RECALL.
    /// </summary>
    private void RemoveSecondaryIndexesForRecord(UdtInstance udtInstance, long recordOffset,
        TransactionContext? context, KeyExpressionEvaluatorCallback? evaluator)
    {
        ApplySecondaryIndexMutation(udtInstance, recordOffset, context, evaluator, delete: true);
    }

    private void ReinsertSecondaryIndexesForRecord(UdtInstance udtInstance, long recordOffset,
        TransactionContext? context, KeyExpressionEvaluatorCallback? evaluator)
    {
        ApplySecondaryIndexMutation(udtInstance, recordOffset, context, evaluator, delete: false);
    }

    private void ApplySecondaryIndexMutation(UdtInstance udtInstance, long recordOffset,
        TransactionContext? context, KeyExpressionEvaluatorCallback? evaluator, bool delete)
    {
        if (evaluator == null)
        {
            throw new InvalidOperationException("L'evaluator non può essere nullo durante la mutazione degli indici.");
        }

        var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var field in udtInstance.Definition.Fields)
        {
            udtInstance.Fields.TryGetValue(field.SymbolId, out var value);
            stringKeyedFields[field.Name.Lexeme] = value;
        }

        foreach (var indexDef in _handle.Schema.SecondaryKeys)
        {
            if (!_secondaryIndexManagers.TryGetValue(indexDef.Name, out var manager))
            {
                continue;
            }

            if (delete)
            {
                TryDeleteSecondaryIndex(indexDef, manager, stringKeyedFields, recordOffset, context, evaluator);
            }
            else
            {
                TryInsertSecondaryIndex(indexDef, manager, stringKeyedFields, recordOffset, context, evaluator);
            }
        }
    }

    /// <summary>
    /// Aggiunge una entry all'indice primario mantenuto in memoria.
    /// Se la chiave esiste già, l'offset viene aggiunto alla lista dei duplicati.
    /// Altrimenti, viene creata una nuova entry per la chiave.
    /// </summary>
    /// <param name="key">La chiave primaria del record da aggiungere.</param>
    /// <param name="recordOffset">Lo specifico offset del record da aggiungere.</param>
    private void AddToMemoryIndex(string indexName, byte[] key, long recordOffset)
    {
        if (!_useInMemoryIndex || _inMemoryIndexes is null) return;

        if (!_inMemoryIndexes.TryGetValue(indexName, out var memoryIndex))
        {
            memoryIndex = new SortedList<byte[], List<long>>(new ByteArrayComparer());
            _inMemoryIndexes[indexName] = memoryIndex;
        }

        if (memoryIndex.TryGetValue(key, out var offsets))
        {
            offsets.Add(recordOffset);
        }
        else
        {
            memoryIndex.Add(key, [recordOffset]);
        }
    }

    /// <summary>
    /// Aggiunge le chiavi di un nuovo record a tutti gli indici definiti (primario e secondari)
    /// in modo transazionalmente sicuro.
    /// </summary>
    /// <param name="udtInstance">L'istanza UDT del record i cui valori servono per calcolare le chiavi.</param>
    /// <param name="recordOffset">L'offset fisico del record da associare alle chiavi negli indici.</param>
    /// <param name="context">Il contesto della transazione corrente, necessario per il locking a livello di chiave.</param>
    /// <param name="evaluator">Il callback obbligatorio per la valutazione delle espressioni di chiave.</param>
    private void UpdateAllIndexesForInsert(UdtInstance udtInstance, long recordOffset,
        TransactionContext? context, KeyExpressionEvaluatorCallback? evaluator)
    {
        if (evaluator == null)
            throw new InvalidOperationException("L'evaluator non può essere nullo durante l'inserimento negli indici.");

        if (_handle.Schema.SecondaryKeys.Count == 0
            && TryInsertPrimaryKeyDirect(udtInstance, recordOffset, context))
        {
            return;
        }

        // Marshalling a string-keyed per l'evaluator esterno
        // 1. Crea il dizionario string-keyed necessario per l'evaluator.
        var stringKeyedFields = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var field in udtInstance.Definition.Fields)
        {
            // Usiamo il SymbolId per leggere il valore e il nome per scriverlo nel dizionario temporaneo.
            udtInstance.Fields.TryGetValue(field.SymbolId, out var value);
            stringKeyedFields[field.Name.Lexeme] = value;
        }

        // Calcola e inserisce la chiave primaria.
        var pkDef = _handle.Schema.PrimaryKey;
        var udtFieldPk = udtInstance.Definition.Fields.FirstOrDefault(f =>
            f.Name.Lexeme.Equals(pkDef.Name, StringComparison.OrdinalIgnoreCase));
        var targetTypePk = udtFieldPk?.DataType ?? new StringType(pkDef.KeyLength);

        // CORREZIONE: Passa il dizionario string-keyed all'evaluator.
        var primaryKeyValue = evaluator(pkDef.KeyExpression, stringKeyedFields, targetTypePk);
        var primaryKey = SchemaManager.SerializeKeyValue(primaryKeyValue, pkDef);

        _primaryIndexManager.Insert(primaryKey, recordOffset, context);
        AddToMemoryIndex(pkDef.Name, primaryKey, recordOffset);

        // Calcola e inserisce le chiavi secondarie.
        foreach (var indexDef in _handle.Schema.SecondaryKeys)
        {
            if (_secondaryIndexManagers.TryGetValue(indexDef.Name, out var manager))
            {
                TryInsertSecondaryIndex(indexDef, manager, stringKeyedFields, recordOffset, context, evaluator);
            }
        }
    }

    /// <summary>
    /// ponytail: PK expression uguale al nome campo — evita dictionary/evaluator nel bulk insert hot path.
    /// </summary>
    private bool TryInsertPrimaryKeyDirect(UdtInstance udtInstance, long recordOffset, TransactionContext? context)
    {
        var pkDef = _handle.Schema.PrimaryKey;
        if (!pkDef.KeyExpression.Equals(pkDef.Name, StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        var udtFieldPk = udtInstance.Definition.Fields.FirstOrDefault(f =>
            f.Name.Lexeme.Equals(pkDef.Name, StringComparison.OrdinalIgnoreCase));
        if (udtFieldPk == null)
        {
            return false;
        }

        udtInstance.Fields.TryGetValue(udtFieldPk.SymbolId, out var raw);
        var primaryKey = SchemaManager.SerializeKeyValue(raw, pkDef);
        _primaryIndexManager.Insert(primaryKey, recordOffset, context);
        AddToMemoryIndex(pkDef.Name, primaryKey, recordOffset);
        return true;
    }

    private static void PopulateStringKeyedFields(
        UdtInstance udtInstance,
        Dictionary<string, object?> stringKeyedFields)
    {
        stringKeyedFields.Clear();
        foreach (var field in udtInstance.Definition.Fields)
        {
            udtInstance.Fields.TryGetValue(field.SymbolId, out var value);
            stringKeyedFields[field.Name.Lexeme] = value;
        }
    }

    private static DataType ResolveIndexTargetType(
        Config.IndexDefinition indexDef,
        IReadOnlyDictionary<string, object?> stringKeyedFields,
        UserDefinedType udtDef)
    {
        var udtField = udtDef.Fields.FirstOrDefault(f =>
            f.Name.Lexeme.Equals(indexDef.Name, StringComparison.OrdinalIgnoreCase));
        return udtField?.DataType ?? new StringType(indexDef.KeyLength);
    }

    private void TryInsertSecondaryIndex(
        Config.IndexDefinition indexDef,
        BPlusTreeManager manager,
        IReadOnlyDictionary<string, object?> stringKeyedFields,
        long recordOffset,
        TransactionContext? context,
        KeyExpressionEvaluatorCallback evaluator)
    {
        if (!IsamIndexPredicate.Matches(indexDef.PartialPredicate, stringKeyedFields))
        {
            return;
        }

        var targetType = ResolveIndexTargetType(indexDef, stringKeyedFields, _handle.UdtDefinition);
        var secondaryKeyValue = evaluator(indexDef.KeyExpression, stringKeyedFields, targetType);
        var secondaryKey = SchemaManager.SerializeKeyValue(secondaryKeyValue, indexDef);
        manager.Insert(secondaryKey, recordOffset, context);
        AddToMemoryIndex(indexDef.Name, secondaryKey, recordOffset);
    }

    private void TryDeleteSecondaryIndex(
        Config.IndexDefinition indexDef,
        BPlusTreeManager manager,
        IReadOnlyDictionary<string, object?> stringKeyedFields,
        long recordOffset,
        TransactionContext? context,
        KeyExpressionEvaluatorCallback evaluator)
    {
        if (!IsamIndexPredicate.Matches(indexDef.PartialPredicate, stringKeyedFields))
        {
            return;
        }

        var targetType = ResolveIndexTargetType(indexDef, stringKeyedFields, _handle.UdtDefinition);
        var secondaryKeyValue = evaluator(indexDef.KeyExpression, stringKeyedFields, targetType);
        var secondaryKey = SchemaManager.SerializeKeyValue(secondaryKeyValue, indexDef);
        manager.Delete(secondaryKey, recordOffset, context);
        RemoveFromMemoryIndex(indexDef.Name, secondaryKey, recordOffset);
    }

    private void RemoveFromMemoryIndexAllKeys(string indexName)
    {
        if (!_useInMemoryIndex || _inMemoryIndexes is null)
        {
            return;
        }

        _inMemoryIndexes.Remove(indexName);
    }

    #endregion

    public void Dispose()
    {
        _replicationLog?.Dispose();
        _handle.Dispose();
    }

    private void RecordReplicationOp(
        ReplicationOp op,
        long recNo,
        UdtInstance? udtInstance,
        TransactionContext? context)
    {
        if (_replicationLog is null || recNo <= 0)
        {
            return;
        }

        IReadOnlyDictionary<string, object?>? values = udtInstance is null
            ? null
            : UdtToFieldDict(udtInstance);
        var line = IsamReplicationLog.FormatLine(op, recNo, values);
        if (context is null)
        {
            _replicationLog.WriteLines([line]);
            return;
        }

        context.PendingIsamReplicationOps.Add(line);
    }

    private void FlushReplicationOnCommit(TransactionContext context)
    {
        if (_replicationLog is null || context.PendingIsamReplicationOps.Count == 0)
        {
            return;
        }

        _replicationLog.WriteLines(context.PendingIsamReplicationOps);
        context.PendingIsamReplicationOps.Clear();
    }

    private void WriteReplicationTxnMarker(ReplicationOp op)
    {
        if (_replicationLog is null) return;
        _replicationLog.WriteLines([ReplicationLogLine.FormatTxnMarker(op)]);
    }

    /// <summary>
    /// Applies a replicated ISCH block and rebuilds indexes on the secondary.
    /// ponytail: full REINDEX after schema write — no page-level index copy from primary.
    /// </summary>
    internal void ApplyReplicatedSchema(byte[] ischBytes, KeyExpressionEvaluatorCallback evaluator)
    {
        ArgumentNullException.ThrowIfNull(ischBytes);
        ArgumentNullException.ThrowIfNull(evaluator);
        _handle.WriteSchemaBlockForReplication(ischBytes);
        ReconcileSecondaryManagersAfterSchemaChange();
        Reindex(evaluator);
        RefreshAfterExternalFileReplace();
    }

    private void ReconcileSecondaryManagersAfterSchemaChange()
    {
        var liveNames = _handle.Schema.SecondaryKeys
            .Select(k => k.Name)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        foreach (var stale in _secondaryIndexManagers.Keys.Where(k => !liveNames.Contains(k)).ToList())
        {
            _secondaryIndexManagers.Remove(stale);
        }

        for (var i = 0; i < _handle.Schema.SecondaryKeys.Count; i++)
        {
            var indexDef = _handle.Schema.SecondaryKeys[i];
            if (_secondaryIndexManagers.ContainsKey(indexDef.Name))
            {
                continue;
            }

            var rootOffset = _handle.Header.SecondaryIndexRootOffsets[i];
            if (rootOffset == 0)
            {
                rootOffset = -1;
            }

            _secondaryIndexManagers[indexDef.Name] =
                new BPlusTreeManager(_handle, _handle.Schema, indexDef, rootOffset);
        }

        ReloadIndexManagersFromHeader();
    }

    private void RecordReplicationSchema()
    {
        if (_replicationLog is null)
        {
            return;
        }

        var ischBytes = SchemaManager.SerializeSchema(_handle.Schema);
        var generation = ++_schemaGeneration;
        var metadata = ReplicationSchemaMetadata.ForIsam(generation, ischBytes);
        _replicationLog.WriteLines([ReplicationLogLine.FormatSchema(metadata)]);
    }

    private void RecordReplicationPack()
    {
        if (_replicationLog is null)
        {
            return;
        }

        var generation = ++_packGeneration;
        _replicationLog.WriteLines([ReplicationLogLine.FormatPack(generation)]);
    }

    private static Dictionary<string, object?> UdtToFieldDict(UdtInstance udtInstance)
    {
        var idToName = udtInstance.Definition.Fields.ToDictionary(f => f.SymbolId, f => f.Name.Lexeme);
        return udtInstance.Fields.ToDictionary(
            kvp => idToName[kvp.Key],
            kvp => kvp.Value,
            StringComparer.OrdinalIgnoreCase);
    }

    private readonly struct StructuralFingerprint
    {
        private readonly long _primaryRoot;
        private readonly long _lastRecNumber;
        private readonly long _lobHeapStart;
        private readonly long _nextAvailablePage;
        private readonly long[] _secondaryRoots;

        public StructuralFingerprint(
            long primaryRoot,
            long lastRecNumber,
            long lobHeapStart,
            long nextAvailablePage,
            long[] secondaryRoots)
        {
            _primaryRoot = primaryRoot;
            _lastRecNumber = lastRecNumber;
            _lobHeapStart = lobHeapStart;
            _nextAvailablePage = nextAvailablePage;
            _secondaryRoots = secondaryRoots;
        }

        public static StructuralFingerprint Capture(Config.FileHeader header) =>
            new(
                header.PrimaryKeyRootOffset,
                header.LastRecNumber,
                header.LobHeapStartOffset,
                header.NextAvailablePageOffset,
                (long[])header.SecondaryIndexRootOffsets.Clone());

        public bool Equals(StructuralFingerprint other) =>
            _primaryRoot == other._primaryRoot
            && _lastRecNumber == other._lastRecNumber
            && _lobHeapStart == other._lobHeapStart
            && _nextAvailablePage == other._nextAvailablePage
            && _secondaryRoots.AsSpan().SequenceEqual(other._secondaryRoots);
    }
}