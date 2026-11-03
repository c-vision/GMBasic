/* FILE: native/gmb_db_c/src/DBF/DbfEngine.c */
/* Port 1:1 di modules/Database/DBF/DbfEngine.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"
#include "../../../gmb_runtime_c/include/gmb_eval_scope.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfEngine.cs
///*
//------------------------------------------------------------------------------------------------------------------------
//                         ANALISI DEL MOTORE DBF (REVISIONE POST-TRANSAZIONALE)
//------------------------------------------------------------------------------------------------------------------------
//Questo documento analizza le caratteristiche, l'architettura e la qualità dell'implementazione del motore DBF
//integrato nell'interprete BASIC, aggiornata per includere un robusto sistema transazionale e di concorrenza.
//
//------------------------------------------------------------------------------------------------------------------------
//1. SICUREZZA E DURABILITÀ DEI DATI (DATA SAFETY & DURABILITY)
//------------------------------------------------------------------------------------------------------------------------
//QUALITÀ STIMATA: ECCELLENTE
//
//CARATTERISTICHE IMPLEMENTATE:
//- WRITE-AHEAD LOG (WAL): Il motore DBF è stato potenziato con un sistema WAL. Ogni operazione di modifica (dati
// nel .dbf, header, flag di cancellazione) viene prima registrata in un file .wal. Questo elimina la storica
// fragilità del sistema DBF/NTX, garantendo che nessuna operazione venga lasciata a metà in caso di crash.
//- RECOVERY AUTOMATICO: All'apertura del file, il motore esegue un recovery automatico dal WAL, riapplicando le
// transazioni commesse e garantendo che il file .dbf e i suoi indici siano sempre in uno stato consistente.
//- VALIDAZIONE DELLO SCHEMA (GMS): Attraverso l'uso di un file satellite .gms, il motore valida ad ogni apertura che
// la struttura del file DBF su disco corrisponda esattamente a quella definita nel `TYPE` del codice, prevenendo
// la corruzione silenziosa dei dati, un problema comune nei sistemi xBase tradizionali.
//- TRANSAZIONI ACID MULTI-OPERAZIONE: Il motore supporta pienamente `BEGIN TRANSACTION`, `COMMIT` e `ROLLBACK`,
// garantendo l'Atomicità per operazioni complesse. Questo permette di eseguire modifiche su più record con la
// certezza che o tutte le operazioni avranno successo o nessuna di esse verrà applicata.
//
//------------------------------------------------------------------------------------------------------------------------
//2. GESTIONE DELLA CONCORRENZA (CONCURRENCY)
//------------------------------------------------------------------------------------------------------------------------
//QUALITÀ STIMATA: ECCELLENTE
//
//CARATTERISTICHE IMPLEMENTATE:
//- LOCKING LETTURA/SCRITTURA MULTI-PROCESSO: Il sistema di locking è stato completamente riprogettato. Utilizza una
// combinazione di Mutex e Semaphore a livello di sistema operativo per simulare un vero lock Lettura/Scrittura
// (condiviso/esclusivo) multipiattaforma. Questo supera di gran lunga il locking puramente esclusivo di Clipper.
//- CONCORRENZA OTTIMISTICA CON VERSIONING (MVCC): Tramite un file satellite opzionale (.gmv), il motore implementa
// il Multi-Version Concurrency Control. Ogni modifica genera una nuova versione del record, permettendo letture
// consistenti senza bloccare gli scrittori e prevenendo il "lost update problem" tramite il controllo della versione.
//- LIVELLI DI ISOLAMENTO: Il motore supporta i livelli di isolamento per un controllo granulare:
//   - Read Committed: Garantisce che non vengano lette modifiche non ancora commesse (no Dirty Reads).
//   - Repeatable Read: Garantisce che le riletture dello stesso record restituiscano sempre gli stessi dati (no
//     Non-Repeatable Reads), acquisendo lock di lettura condivisi sui record letti.
//- RILEVAMENTO DEADLOCK: Wait-for graph condiviso (<see cref="DeadlockMonitor"/>, pattern ISAM) su lock
// intra-processo e intra-thread cross-process; tra processi OS distinti: grafo wait-for su file (.gmb_waitgraph_*).
//
//------------------------------------------------------------------------------------------------------------------------
//3. PERFORMANCE E SCALABILITÀ (THROUGHPUT & SCALABILITY)
//------------------------------------------------------------------------------------------------------------------------
//QUALITÀ STIMATA: MOLTO BUONA
//
//CARATTERISTICHE IMPLEMENTATE:
//- INDICIZZAZIONE NTX (B-TREE): Il motore utilizza il formato di indice .ntx standard di Clipper, basato su B-Tree,
// che garantisce performance di ricerca logaritmiche e un'ampia compatibilità con gli strumenti esistenti.
//- OVERHEAD TRANSAZIONALE: L'introduzione del WAL, del versioning e del locking avanzato introduce un leggero
// overhead sulle operazioni di scrittura rispetto al sistema Clipper originale. Questo è un trade-off consapevole e
// necessario per ottenere un livello di affidabilità e sicurezza dei dati infinitamente superiore.
//- COMPATIBILITÀ: Mantenendo i formati di file .dbf e .ntx, il motore garantisce una facile migrazione e
// interoperabilità per le applicazioni legacy.
//
//------------------------------------------------------------------------------------------------------------------------
//4. MANUTENZIONE E BACKUP (MAINTENANCE & BACKUP)
//------------------------------------------------------------------------------------------------------------------------
//QUALITÀ STIMATA: ECCELLENTE
//
//CARATTERISTICHE IMPLEMENTATE:
//- COMANDI SICURI: Le operazioni di manutenzione classiche come `PACK`, `REINDEX` e `ZAP` sono state re-implementate
// per essere transazionalmente sicure. `PACK` non rischia più la perdita di dati in caso di fallimento, e `REINDEX`
// diventa uno strumento di ottimizzazione o recupero da danni fisici, non una necessità operativa quotidiana per
// correggere la corruzione degli indici.
//- BACKUP A CALDO (SNAPSHOT): Il sistema WAL permette di creare copie di backup consistenti del database mentre è in
// uso, con un impatto minimo sulla disponibilità del sistema.
//
//------------------------------------------------------------------------------------------------------------------------
//                                                 VERDETTO FINALE
//------------------------------------------------------------------------------------------------------------------------
//Il nostro motore DBF rappresenta un'evoluzione monumentale del classico sistema xBase. Mantenendo la
//compatibilità con i formati di file .dbf/.ntx, abbiamo eliminato le sue debolezze storiche (fragilità degli indici,
//assenza di transazioni, locking primitivo) e lo abbiamo trasformato in un sistema di database moderno, affidabile e
//concorrente, pienamente conforme ai principi ACID.
//
//Il "Clipper che avrebbe dovuto essere": offre la familiarità e la semplicità del modello dati DBF con la robustezza
//e la sicurezza che gli sviluppatori si aspettano da un database moderno.
//
//Conclusione:
//Il motore è una soluzione di altissima qualità, pronta per applicazioni multi-utente mission-critical dove
//l'affidabilità dei dati è fondamentale. È un sostituto superiore in ogni aspetto al sistema di file originale di
//Clipper.
//
//------------------------------------------------------------------------------------------------------------------------
//                                       TABELLA RIASSUNTIVA VS. CLIPPER 5.3 (DBF/NTX)
//------------------------------------------------------------------------------------------------------------------------
//Caratteristica        Clipper 5.3 (DBF/NTX)               Motore DBF                          Vincitore
//--------------------  ----------------------------------  --------------------------------------------  -------------
//Affidabilità Dati     Bassa (Corruzione indici freq.)     Eccellente (WAL & Atomicità)                  Nostro Motore
//Validazione Schema    Inesistente                         Sì, Rigorosa (tramite file .gms)              Nostro Motore
//Concorrenza           Manuale, Lock Esclusivo             Ibrida (MVCC + Lock Lettura/Scrittura)        Nostro Motore
//Transazioni           Nessuna                             Pieno Supporto ACID (fino a Repeatable Read)  Nostro Motore
//Compatibilità         Sì (Standard)                       Sì (Estesa con .gms e .gmv)                   Nostro Motore
//*/
//
//using System.Buffers;
//using System.Collections.Concurrent;
//using System.Globalization;
//using System.Text;
//using Basic.Databases;
//using Basic.Databases.Replication;
//using Basic.DBF;
//
//// ReSharper disable InconsistentNaming
//namespace Basic.DBF;
//
///// <summary>
///// Motore di alto livello per la gestione di un file DBF, che implementa la
///// logica a "Work Area" (cursore, stato, etc.).
///// </summary>
//public class DbfEngine : IDisposable
//{
//    static DbfEngine()
//    {
//        // ponytail: once per process — CP1251/windows-1252 need CodePagesEncodingProvider on .NET Core+
//        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
//    }
//
//    /// <summary>
//    /// Gestore a basso livello per l'accesso fisico al file .dbf.
//    /// </summary>
//    private readonly DbfFile _dbfFile;
//
//    /// <summary>
//    /// Gestore per il file memo associato (es. .dbt), usato per leggere e scrivere i campi Memo.
//    /// </summary>
//    private readonly IMemoFile _memoFile;
//
//    /// <summary>
//    /// Lista di tutti i file di indice (.ntx) aperti per questa work area.
//    /// </summary>
//    private readonly List<IIndexHandle> _indexes = [];
//
//    /// <summary>
//    /// Utility per il parsing dei dati grezzi dei campi nei tipi di dato del linguaggio.
//    /// </summary>
//    private readonly IFieldParser _fieldParser;
//
//    /// <summary>
//    /// La codifica dei caratteri utilizzata per leggere e scrivere i dati nel file, determinata dall'header.
//    /// </summary>
//    private readonly Encoding _encoding;
//
//    /// <summary>
//    /// Stato navigazione (cursore, BOF/EOF, filtro). ponytail: per engine instance, non ThreadLocal —
//    /// syscalls GPM possono girare su thread pool; ThreadLocal perdeva il cursore tra tick e syscall.
//    /// </summary>
//    private readonly DbfThreadCursor _cursor = new();
//
//    private DbfThreadCursor Nav => _cursor;
//
//    private int _currentRecordNumber
//    {
//        get => Nav.CurrentRecordNumber;
//        set => Nav.CurrentRecordNumber = value;
//    }
//
//    /// <summary>
//    /// Riferimento all'handle dell'indice attualmente attivo (impostato da SET ORDER).
//    /// Se è null, la navigazione avviene in ordine fisico.
//    /// </summary>
//    private IIndexHandle? _activeOrder;
//
//    /// <summary>
//    /// Flag che determina se i record marcati come cancellati sono visibili (impostato da SET DELETED).
//    /// </summary>
//    private bool _deletedVisible;
//
//    /// <summary>
//    /// Flag che determina se i confronti tra stringhe nelle ricerche devono essere esatti (impostato da SET EXACT).
//    /// </summary>
//    private bool _exact;
//
//    private Expr? _activeFilterExpression
//    {
//        get => Nav.ActiveFilterExpression;
//        set => Nav.ActiveFilterExpression = value;
//    }
//
//    private Func<Dictionary<string, object?>, bool>? _activeFilterEvaluator
//    {
//        get => Nav.ActiveFilterEvaluator;
//        set => Nav.ActiveFilterEvaluator = value;
//    }
//
//    private uint _currentLeafPageNum
//    {
//        get => Nav.CurrentLeafPageNum;
//        set => Nav.CurrentLeafPageNum = value;
//    }
//
//    private int _currentIndexInLeaf
//    {
//        get => Nav.CurrentIndexInLeaf;
//        set => Nav.CurrentIndexInLeaf = value;
//    }
//
//    /// <summary>
//    /// Wrapper transazionale che gestisce tutte le operazioni di scrittura tramite WAL e locking.
//    /// </summary>
//    private readonly DbfTransactionalFileHandle _txHandle;
//
//    /// <summary>
//    /// Mantiene lo stato delle transazioni attive per ogni thread.
//    /// La chiave è l'ID del thread gestito. Il valore è uno stack di contesti di transazione,
//    /// che permette di gestire correttamente le transazioni nidificate e i livelli di isolamento.
//    /// </summary>
//    private readonly ConcurrentDictionary<int, Stack<TransactionContext>> _threadTransactions = new();
//
//    /// <summary>
//    /// Gestore opzionale per il file satellite .gmv che contiene le versioni dei record.
//    /// È nullo se il versioning non è attivo per questo file.
//    /// </summary>
//    private readonly VersioningManager? _versioningManager = null;
//
//    private long _lastReadRecordVersion
//    {
//        get => Nav.LastReadRecordVersion;
//        set => Nav.LastReadRecordVersion = value;
//    }
//
//    /// <summary>
//    /// La strategia di locking (Intra-Processo o Cross-Process) in uso da questo motore.
//    /// Viene propagata agli handle degli indici per gestire correttamente il caching.
//    /// </summary>
//    private readonly LockingStrategy _lockingStrategy;
//
//    /// <summary>
//    /// Coda FIFO degli slot cancellati riutilizzabili; popolata on-delete e da scan one-shot lazy.
//    /// ponytail: nessun bitmap persistente — upgrade: header/footer free-list on disk.
//    /// </summary>
//    private readonly Queue<int> _deletedRecordCache = new();
//    private readonly HashSet<int> _deletedSlotSet = new();
//
//    private bool _deletedSlotsIndexed;
//
//    /// <summary>
//    /// Modalità durabilità I/O (default <see cref="DbfDurabilityMode.Full"/>).
//    /// </summary>
//    public DbfDurabilityMode DurabilityMode
//    {
//        get => _txHandle.DurabilityMode;
//        set => _txHandle.DurabilityMode = value;
//    }
//
//    /// <summary>
//    /// Forza fsync su DBF e checkpoint WAL se attivo.
//    /// </summary>
//    public void FlushDurability() => _txHandle.FlushDurability();
//
//    /// <summary>
//    /// Abilita la replica append-only verso un file di log. Passare <see langword="null"/> o stringa vuota per disabilitare.
//    /// Usare <c>"."</c> o <c>DEFAULT</c> per il sidecar predefinito <c>{dbf}.repl</c>.
//    /// </summary>
//    /// <param name="logPath">Percorso del file di log o sentinel per sidecar predefinito.</param>
//    /// <returns>Il percorso effettivo del log attivo, o <see langword="null"/> se disabilitato.</returns>
//    public string? SetReplication(string? logPath)
//    {
//        _replicationLog?.Dispose();
//        _replicationLog = null;
//
//        if (string.IsNullOrWhiteSpace(logPath))
//        {
//            return null;
//        }
//
//        var path = logPath.Trim();
//        if (path.Equals(".", StringComparison.Ordinal) ||
//            path.Equals("DEFAULT", StringComparison.OrdinalIgnoreCase))
//        {
//            path = FilePath + ".repl";
//        }
//
//        _replicationLog = new DbfReplicationLog(path);
//        return path;
//    }
//
//    /// <summary>
//    /// Log append-only sidecar attivo per la replica (null se disabilitata).
//    /// </summary>
//    public string? ReplicationLogPath => _replicationLog?.LogPath;
//
//    /// <summary>
//    /// Indica se la replica su file è attiva per questa work area.
//    /// </summary>
//    public bool IsReplicationEnabled => _replicationLog != null;
//
//    public int ApplyReplication(string replLogPath) => DbfReplicationApplier.Apply(replLogPath, this);
//
//    public int FollowReplication(string? replLogPath = null, string? checkpointPath = null)
//    {
//        var path = replLogPath ?? ReplicationLogPath ?? throw new InvalidOperationException("Replication log path is required.");
//        return DbfReplicationApplier.Follow(path, this, checkpointPath);
//    }
//
//    /// <summary>
//    /// Ships pending replication log lines over TCP to a remote receiver.
//    /// </summary>
//    public int ReplicationShipTcp(string host, int port, string? replLogPath = null, string? shipCheckpointPath = null)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(host);
//        var path = replLogPath ?? ReplicationLogPath
//                   ?? throw new InvalidOperationException("Replication log path is required.");
//        return ReplicationTcpShipper.ShipOnce(path, host, port, shipCheckpointPath);
//    }
//
//    /// <summary>
//    /// Listens once on <paramref name="port"/> and applies replication lines received over TCP.
//    /// </summary>
//    public int ReplicationRecvTcp(int port, string? checkpointPath = null)
//    {
//        SetReplication(string.Empty);
//        return ReplicationTcpReceiver.ReceiveOnce(
//            port,
//            (state, line) => DbfReplicationApplier.ApplyLine(line, state, this),
//            checkpointPath);
//    }
//
//    private DbfReplicationLog? _replicationLog;
//    private long _packGeneration;
//    private long _schemaGeneration;
//    private long _reindexGeneration;
//
//    /// <summary>
//    /// Ospita il callback opzionale all'Executor per la valutazione di espressioni complesse,
//    /// abilitato quando la work area viene aperta in modalità UNSAFE. Se nullo, il motore
//    /// utilizzerà solo il suo parser/valutatore interno per le chiavi di indice.
//    /// </summary>
//    public KeyExpressionEvaluatorCallback? Evaluator { get; set; }
//
//    /// <summary>
//    /// Initializes a new instance of the <see cref="DbfEngine"/> class.
//    /// </summary>
//    /// <param name="filePath">The full path to the .dbf file.</param>
//    /// <param name="indexPaths">An optional collection of paths to the index files (.ntx).</param>
//    /// <param name="exclusive">If set to <c>true</c> the file stream will be opened in exclusive mode.</param>
//    /// <param name="lockingStrategy">La strategia di locking da utilizzare (Intra-Processo o Cross-Process).</param>
//    /// <param name="isSchemaStrict">
//    /// (Opzionale) Se `true`, la validazione dello schema contro file DBF legacy (senza .gms) sarà stretta. 
//    /// Il default è `false` (validazione flessibile).
//    /// </param>
//    /// <param name="codeSchema">
//    /// (Opzionale) La definizione dello schema dal `TYPE` del codice. Se fornito, verrà eseguita la validazione
//    /// dello schema. Se `null`, la validazione viene saltata.
//    /// </param>
//    /// <param name="fieldMappings">
//    /// (Opzionale) Mappa nomi campo TYPE a colonne DBF fisiche per <c>USE ... AS ... MAP</c>.
//    /// </param>
//    public DbfEngine(string filePath, IEnumerable<string>? indexPaths, bool exclusive,
//        LockingStrategy lockingStrategy, bool isSchemaStrict = false, DbfSchemaInfo? codeSchema = null,
//        IReadOnlyDictionary<string, string>? fieldMappings = null)
//    {
//        _lockingStrategy = lockingStrategy;
//        _txHandle = new DbfTransactionalFileHandle(filePath, exclusive, lockingStrategy);
//        _dbfFile = _txHandle.GetDbfFile();
//
//        if (codeSchema != null)
//        {
//            DbfSchemaManager.Validate(_dbfFile, codeSchema, fieldMappings, isSchemaStrict);
//        }
//
//        _memoFile = DbfMemoFactory.Create(filePath, _dbfFile.Header.Version, createIfMissing: exclusive);
//        _fieldParser = new FieldParser();
//        _encoding = _dbfFile.Header.Encoding;
//
//        if (indexPaths != null)
//        {
//            foreach (var indexPath in indexPaths)
//            {
//                foreach (var indexHandle in IndexFactory.CreateHandles(
//                             indexPath, _txHandle.RecordLockManager, _lockingStrategy))
//                {
//                    _indexes.Add(indexHandle);
//                }
//            }
//        }
//
//        _activeOrder = _indexes.FirstOrDefault();
//        GoTop();
//    }
//
//    /// <summary>
//    /// Ottiene l'header del file DBF, che contiene i metadati fondamentali del file
//    /// come la versione, il numero totale di record, la data dell'ultimo aggiornamento,
//    /// e le lunghezze dell'header e dei singoli record. Questa struttura viene letta
//    /// una sola volta dal disco al momento dell'apertura del file.
//    /// </summary>
//    /// <returns>L'oggetto `DbfHeader` che rappresenta l'header del file.</returns>
//    public DbfHeader GetHeader() => _dbfFile.Header;
//
//    /// <summary>
//    /// Ottiene la lista in sola lettura delle definizioni dei campi (colonne) per questo file DBF.
//    /// Ogni elemento della lista è un oggetto `DbfField` che contiene metadati come il nome,
//    /// il tipo di dato, la lunghezza e il numero di decimali del campo. Questa struttura
//    /// viene letta una sola volta dall'header del file al momento dell'apertura.
//    /// </summary>
//    /// <returns>Una lista in sola lettura (`IReadOnlyList{DbfField}`) delle definizioni dei campi.</returns>
//    public IReadOnlyList<DbfField> GetFields() => _dbfFile.Fields;
//
//    /// <summary>
//    /// Restituisce una collezione in sola lettura di tutti gli handle degli indici (.ntx)
//    /// attualmente aperti e associati a questa work area.
//    /// </summary>
//    /// <returns>Una collezione enumerabile di interfacce <see cref="IIndexHandle"/>.</returns>
//    public IEnumerable<IIndexHandle> GetIndexes() => _indexes;
//
//    /// <summary>
//    /// Restituisce la codifica dei caratteri (<see cref="Encoding"/>) utilizzata da questo file DBF,
//    /// determinata dal language driver byte presente nell'header del file.
//    /// </summary>
//    /// <returns>L'istanza di <see cref="Encoding"/> utilizzata per tutte le operazioni su stringhe.</returns>
//    public Encoding GetEncoding() => _encoding;
//
//    /// <summary>
//    /// Ottiene un valore che indica se il cursore del record si trova oltre l'ultimo record (End Of File).
//    /// Questa proprietà diventa true dopo aver eseguito uno SKIP sull'ultimo record o
//    /// un SEEK per una chiave non trovata.
//    /// </summary>
//    public bool Eof
//    {
//        get => Nav.Eof;
//        private set => Nav.Eof = value;
//    }
//
//    /// <summary>
//    /// Ottiene un valore che indica se il cursore del record si trova prima del primo record (Beginning Of File).
//    /// Questa proprietà diventa true dopo aver eseguito uno SKIP all'indietro dal primo record.
//    /// </summary>
//    public bool Bof
//    {
//        get => Nav.Bof;
//        private set => Nav.Bof = value;
//    }
//
//    /// <summary>
//    /// Ottiene un valore che indica se l'ultima operazione di ricerca (SEEK) ha avuto successo,
//    /// trovando una corrispondenza esatta per la chiave cercata.
//    /// </summary>
//    public bool Found
//    {
//        get => Nav.Found;
//        private set => Nav.Found = value;
//    }
//
//    /// <summary>
//    /// Ottiene un valore che indica se è presente un indice attivo (ordine logico)
//    /// per la navigazione.
//    /// </summary>
//    public bool HasActiveOrder => _activeOrder != null;
//
//    /// <summary>
//    /// Ottiene la rappresentazione testuale dell'espressione di filtro attualmente attiva.
//    /// Restituisce una stringa vuota se nessun filtro è impostato.
//    /// </summary>
//    public string ActiveFilterExpression => _activeFilterExpression?.ToString() ?? "";
//
//    /// <summary>
//    /// Percorso assoluto del file .dbf sottostante.
//    /// </summary>
//    public string FilePath => _dbfFile.FilePath;
//
//    /// <summary>
//    /// Writes or replaces the .gms schema sidecar and logs SCHEMA when replication is enabled.
//    /// </summary>
//    /// <param name="schemaInfo">Enriched TYPE schema to persist.</param>
//    public void WriteSchemaSidecar(DbfSchemaInfo schemaInfo)
//    {
//        ArgumentNullException.ThrowIfNull(schemaInfo);
//        DbfSchemaManager.WriteSchemaInfo(FilePath, schemaInfo);
//        RecordReplicationSchema();
//    }
//
//    /// <summary>
//    /// Ottiene la rappresentazione testuale dell'espressione di chiave dell'indice attualmente attivo.
//    /// Restituisce una stringa vuota se nessun ordine di indice è impostato.
//    /// </summary>
//    public string ActiveIndexKeyExpression => _activeOrder?.GetKeyExpressionString() ?? "";
//
//    /// <summary>
//    /// Imposta la modalità di confronto esatto per le stringhe nelle operazioni di ricerca (SEEK)
//    /// e nelle relazioni.
//    /// </summary>
//    /// <param name="on">
//    /// True per attivare il confronto esatto (SET EXACT ON), dove le stringhe devono corrispondere
//    /// per l'intera lunghezza, incluso il padding. False per disattivarlo (SET EXACT OFF),
//    /// dove il confronto ha successo se la chiave inizia con la stringa di ricerca.
//    /// </param>
//    public void SetExact(bool on)
//    {
//        _exact = on;
//    }
//
//    /// <summary>
//    /// Ottiene il numero ordinale (1-based) dell'indice attualmente attivo.
//    /// Restituisce 0 se la navigazione è in ordine fisico (naturale).
//    /// </summary>
//    public int ActiveIndexOrder
//    {
//        get
//        {
//            if (_activeOrder == null)
//                return 0;
//
//            // Cerca l'indice nella lista per trovarne la posizione (0-based) e la converte in 1-based.
//            var index = _indexes.IndexOf(_activeOrder);
//            return index + 1;
//        }
//    }
//
//    /// <summary>
//    /// Verifica se il record attualmente puntato dal cursore è marcato come cancellato,
//    /// indipendentemente dall'impostazione di SET DELETED.
//    /// </summary>
//    /// <returns>
//    /// True se il record corrente è marcato per la cancellazione, altrimenti False.
//    /// Restituisce False anche se il cursore si trova a BOF o EOF.
//    /// </returns>
//    public bool IsCurrentRecordDeleted()
//    {
//        if (Bof || Eof)
//            return false;
//
//        var (isDeleted, _) = _dbfFile.ReadRawRecord(_currentRecordNumber);
//        return isDeleted;
//    }
//
//    /// <summary>
//    /// Posiziona il cursore sul primo record logico che soddisfa il filtro attivo,
//    /// applicando il locking appropriato per il livello di isolamento della transazione.
//    /// </summary>
//    /// <remarks>
//    /// Se eseguito all'interno di una transazione `Serializable`, questo metodo acquisisce un lock di lettura
//    /// sulla prima chiave dell'indice attivo per prevenire le letture fantasma.
//    /// </remarks>
//    public void GoTop()
//    {
//        Bof = false;
//        Eof = false; // Resetta sempre Eof quando si va a TOP
//
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//
//        if (_activeOrder != null)
//        {
//            var (recno, pageNum, indexInLeaf) = _activeOrder.GetFirstRecordPosition(context);
//            _currentRecordNumber = recno;
//            _currentLeafPageNum = pageNum;
//            _currentIndexInLeaf = indexInLeaf;
//        }
//        else
//        {
//            _currentRecordNumber = 1;
//        }
//
//        UpdateBofEofStatus();
//    }
//
//    /// <summary>
//    /// Posiziona il cursore sull'ultimo record logico che soddisfa il filtro attivo,
//    /// applicando il locking appropriato per il livello di isolamento della transazione.
//    /// </summary>
//    /// <remarks>
//    /// Se eseguito all'interno di una transazione `Serializable`, questo metodo acquisisce un lock di lettura
//    /// sull'ultima chiave dell'indice attivo per prevenire le letture fantasma.
//    /// </remarks>
//    public void GoBottom()
//    {
//        Eof = false;
//        Bof = false; // Resetta sempre Bof quando si va a BOTTOM
//
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//
//        if (_activeOrder != null)
//        {
//            var (recno, pageNum, indexInLeaf) = _activeOrder.GetLastRecordPosition(context);
//            _currentRecordNumber = recno;
//            _currentLeafPageNum = pageNum;
//            _currentIndexInLeaf = indexInLeaf;
//        }
//        else
//        {
//            _currentRecordNumber = (int)_dbfFile.Header.NumberOfRecords;
//        }
//
//        UpdateBofEofStatus();
//    }
//
//    /// <summary>
//    /// Muove il cursore del record di un numero specificato di posizioni FISICHE o LOGICHE.
//    /// Questa versione è non bloccante e sposta il cursore senza validare filtri o record cancellati.
//    /// La logica di iterazione per trovare il prossimo record valido è responsabilità dell'Executor.
//    /// </summary>
//    /// <param name="recordsToSkip">Il numero di record da saltare.</param>
//    public void Skip(int recordsToSkip = 1)
//    {
//        if (recordsToSkip == 0) return;
//        if ((Eof && recordsToSkip > 0) || (Bof && recordsToSkip < 0)) return;
//
//        if (Eof && recordsToSkip < 0) Eof = false;
//        if (Bof && recordsToSkip > 0) Bof = false;
//
//        for (var i = 0; i < Math.Abs(recordsToSkip); i++)
//        {
//            Step(Math.Sign(recordsToSkip));
//            if (Bof || Eof) break;
//        }
//    }
//
//    /// <summary>
//    /// [NUOVO - GPM-SAFE] Verifica se il record attualmente puntato dal cursore è valido
//    /// e soddisfa tutte le condizioni di visibilità (flag DELETED e filtro attivo).
//    /// Questa operazione è non bloccante e centrale per la navigazione GPM.
//    /// </summary>
//    /// <returns>True se il record corrente è visibile e passa il filtro, altrimenti False.</returns>
//    public bool IsRecordValidForFilter()
//    {
//        // Un record è valido per il filtro se supera entrambi i controlli:
//        // 1. IsCurrentRecordValid(): Controlla BOF/EOF e lo stato di cancellazione (SET DELETED).
//        // 2. EvaluateFilter(): Controlla la condizione di SET FILTER.
//        return IsCurrentRecordValid() && EvaluateFilter();
//    }
//
//    /// <summary>
//    /// Legge e parsa i valori della versione *visibile* del record attualmente puntato dal cursore,
//    /// rispettando il livello di isolamento della transazione corrente.
//    /// </summary>
//    /// <returns>
//    /// Un dizionario con i valori del record, o `null` se nessuna versione del record è visibile
//    /// o se il cursore non è posizionato su un record valido.
//    /// </returns>
//    /// <exception cref="LockTimeoutException">
//    /// Lanciata se, in una transazione `RepeatableRead` o superiore, non è possibile acquisire un lock di lettura
//    /// sul record. L'intera transazione viene automaticamente annullata.
//    /// </exception>
//    public Dictionary<string, object?>? GetCurrentRecordValues()
//    {
//        if (_currentRecordNumber <= 0 || _currentRecordNumber > _dbfFile.Header.NumberOfRecords)
//        {
//            _lastReadRecordVersion = 0;
//            return null;
//        }
//
//        return GetAndLockRecordValues(_currentRecordNumber);
//    }
//
//    /// <summary>
//    /// Sovrascrive il record corrente con i nuovi dati, aggiorna tutti gli indici aperti
//    /// e implementa la concorrenza ottimistica tramite versioning. Questa operazione richiede
//    /// che le chiavi di indice, vecchie e nuove, siano pre-calcolate dall'Executor
//    /// per mantenere il motore disaccoppiato dalla logica di valutazione delle espressioni.
//    /// </summary>
//    /// <param name="values">Un dizionario contenente i nuovi valori per i campi del record.</param>
//    /// <param name="oldKeys">Un dizionario in sola lettura che mappa ogni handle di indice alla sua vecchia
//    /// rappresentazione in byte, calcolata sui dati del record *prima* della modifica. Serve per
//    /// rimuovere correttamente le vecchie entry dagli indici.</param>
//    /// <param name="newKeys">Un dizionario in sola lettura che mappa ogni handle di indice alla sua nuova
//    /// rappresentazione in byte, calcolata sui nuovi dati del record. Serve per inserire
//    /// le nuove entry negli indici.</param>
//    /// <exception cref="InvalidOperationException">
//    /// Se il cursore non è posizionato su un record valido o se si verifica un conflitto di concorrenza ottimistica.
//    /// </exception>
//    /// <exception cref="IOException">Se si verifica un errore di I/O durante la lettura o scrittura del record.</exception>
//    /// <exception cref="LockTimeoutException">
//    /// Se non è possibile acquisire un lock di scrittura esclusivo sul record. Se eseguita all'interno di una
//    /// transazione, questa eccezione causerà un rollback automatico dell'intera transazione.
//    /// </exception>
//    public void PutCurrentRecord(Dictionary<string, object?> values, IReadOnlyDictionary<IIndexHandle, byte[]> oldKeys,
//        IReadOnlyDictionary<IIndexHandle, byte[]> newKeys)
//    {
//        if (_currentRecordNumber <= 0 || _currentRecordNumber > _dbfFile.Header.NumberOfRecords)
//            throw new InvalidOperationException("Cursore non posizionato su un record valido.");
//
//        var recNo = _currentRecordNumber;
//
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//        var isAutoCommit = context == null;
//
//        if (_versioningManager != null)
//        {
//            var metadata = _versioningManager.GetMetadata(recNo);
//            if (_lastReadRecordVersion != 0 && metadata.Version != _lastReadRecordVersion)
//            {
//                throw new InvalidOperationException(
//                    $"Conflitto di concorrenza: il record {recNo} è stato modificato da un altro processo.");
//            }
//        }
//
//        var lockKey = LockManagerBase.GetRecordLockKey(recNo);
//        try
//        {
//            _txHandle.RecordLockManager.AcquireWriteLock(lockKey, 5000, $"record {recNo}");
//        }
//        catch (DeadlockException)
//        {
//            RollbackOnLockFailure(isAutoCommit);
//            throw;
//        }
//        catch (LockTimeoutException)
//        {
//            RollbackOnLockFailure(isAutoCommit);
//            throw;
//        }
//
//        if (!isAutoCommit)
//        {
//            context!.WriteLockedRecNos.Add(recNo);
//        }
//
//        try
//        {
//            var payloadLength = _dbfFile.Header.RecordLength - 1;
//            var buffer = ArrayPool<byte>.Shared.Rent(payloadLength);
//            try
//            {
//                WriteValuesToBuffer(values, buffer.AsSpan(0, payloadLength));
//
//                long undoRecNo = -1;
//                if (_versioningManager != null)
//                {
//                    undoRecNo = _txHandle.AppendUndoVersion(recNo, context);
//                }
//
//                _txHandle.WriteRawRecord(recNo, buffer.AsSpan(0, payloadLength));
//
//                if (_versioningManager != null)
//                {
//                    var newMetadata = new VersioningManager.VersionMetadata
//                    {
//                        Version = DateTime.UtcNow.Ticks,
//                        TransactionId = isAutoCommit ? 0 : context!.TransactionId,
//                        PreviousVersionRecNo = undoRecNo
//                    };
//                    _versioningManager.SetMetadata(recNo, newMetadata);
//                }
//
//                foreach (var index in _indexes)
//                {
//                    var oldKey = oldKeys[index];
//                    var newKey = newKeys[index];
//                    if (!oldKey.AsSpan().SequenceEqual(newKey))
//                    {
//                        index.Update(oldKey, newKey, recNo, context);
//                    }
//
//                    index.InvalidateCache();
//                }
//
//                RecordReplicationOp(DbfReplicationOp.Put, recNo, values, context);
//                if (isAutoCommit)
//                {
//                    FinalizeAutoCommitUnlessBulkLoad();
//                }
//            }
//            finally
//            {
//                ArrayPool<byte>.Shared.Return(buffer);
//            }
//        }
//        finally
//        {
//            if (isAutoCommit)
//            {
//                _txHandle.RecordLockManager.ReleaseWriteLock(lockKey);
//            }
//        }
//    }
//
//    /// <summary>
//    /// Aggiunge un nuovo record al file, aggiorna tutti gli indici aperti e posiziona il cursore sul nuovo record.
//    /// L'operazione è transazionalmente sicura e supporta il livello `Serializable`. L'engine tenta di
//    /// riutilizzare lo spazio dei record cancellati prima di estendere fisicamente il file.
//    /// Le chiavi per tutti gli indici devono essere pre-calcolate dall'Executor.
//    /// </summary>
//    /// <param name="values">Un dizionario contenente i valori per i campi del nuovo record.</param>
//    /// <param name="newKeys">Un dizionario in sola lettura che mappa ogni handle di indice alla sua
//    /// rappresentazione in byte, calcolata sui dati del nuovo record. Serve per inserire
//    /// correttamente le nuove entry in tutti gli indici.</param>
//    /// <exception cref="IOException">Se il file è bloccato da un altro processo e non è possibile acquisire un lock a livello di file.</exception>
//    /// <remarks>
//    /// L'aggiunta di un record è un'operazione che può modificare la struttura del file e richiede un lock esclusivo
//    /// a livello di file. Per il livello di isolamento `Serializable`, acquisisce anche lock di scrittura sulle chiavi di
//    /// indice da inserire per prevenire letture fantasma (phantom reads) da parte di altre transazioni.
//    /// </remarks>
//    public void AppendRecord(Dictionary<string, object?> values, IReadOnlyDictionary<IIndexHandle, byte[]> newKeys)
//    {
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//        var isAutoCommit = context == null;
//
//        _txHandle.LockFile();
//
//        try
//        {
//            var payloadLength = _dbfFile.Header.RecordLength - 1;
//            var buffer = ArrayPool<byte>.Shared.Rent(payloadLength);
//            try
//            {
//                WriteValuesToBuffer(values, buffer.AsSpan(0, payloadLength));
//                int recNoToUse;
//                bool isRecycled;
//
//                // ponytail: free list on-delete + one-shot lazy index — no per-append O(n) scan
//                if (!TryTakeDeletedSlot(out var foundRecNo))
//                {
//                    recNoToUse = _txHandle.AppendRawRecord(buffer.AsSpan(0, payloadLength));
//                    isRecycled = false;
//                }
//                else
//                {
//                    recNoToUse = foundRecNo;
//                    isRecycled = true;
//                }
//
//                if (isRecycled)
//                {
//                    _txHandle.WriteRawRecord(recNoToUse, buffer.AsSpan(0, payloadLength));
//                    _txHandle.SetDeletedFlag(recNoToUse, false);
//                }
//
//                _currentRecordNumber = recNoToUse;
//                _currentLeafPageNum = 0;
//                _currentIndexInLeaf = 0;
//                UpdateBofEofStatus();
//
//                if (_versioningManager != null)
//                {
//                    var newMetadata = new VersioningManager.VersionMetadata
//                    {
//                        Version = DateTime.UtcNow.Ticks,
//                        TransactionId = isAutoCommit ? 0 : context!.TransactionId,
//                        PreviousVersionRecNo = -1
//                    };
//
//                    if (isRecycled)
//                    {
//                        _versioningManager.SetMetadata(recNoToUse, newMetadata);
//                    }
//                    else
//                    {
//                        _versioningManager.AddNewVersionEntry(newMetadata);
//                    }
//
//                    _lastReadRecordVersion = newMetadata.Version;
//                }
//
//                if (_indexes.Count > 0)
//                {
//                    foreach (var index in _indexes)
//                    {
//                        if (newKeys.TryGetValue(index, out var newKey))
//                        {
//                            index.Insert(newKey, recNoToUse, context);
//                        }
//
//                        index.InvalidateCache();
//                    }
//                }
//
//                RecordReplicationOp(DbfReplicationOp.Append, recNoToUse, values, context);
//                if (isAutoCommit)
//                {
//                    FinalizeAutoCommitUnlessBulkLoad();
//                }
//            }
//            finally
//            {
//                ArrayPool<byte>.Shared.Return(buffer);
//            }
//        }
//        finally
//        {
//            _txHandle.UnlockFile();
//        }
//    }
//
//    /// <summary>
//    /// Avvia una sessione bulk-load: un solo flush WAL/disco alla fine (<see cref="EndBulkLoad"/>).
//    /// </summary>
//    public void BeginBulkLoad() => _txHandle.BeginBulkLoadSession();
//
//    /// <summary>
//    /// Termina la sessione bulk-load e sincronizza WAL + file DBF su disco.
//    /// </summary>
//    public void EndBulkLoad()
//    {
//        _txHandle.EndBulkLoadSession();
//        _txHandle.FinalizeCommit();
//    }
//
//    /// <summary>
//    /// Differisce fsync su auto-commit (come bulk-load) fino a <see cref="EndGroupCommit"/>.
//    /// ponytail: opt-in — Full mode resta per-record senza group commit.
//    /// </summary>
//    public void BeginGroupCommit() => _txHandle.BeginGroupCommitSession();
//
//    /// <summary>
//    /// Chiude group-commit e sincronizza su disco se <see cref="DbfDurabilityMode.Full"/>.
//    /// </summary>
//    public void EndGroupCommit() => _txHandle.EndGroupCommitSession();
//
//    /// <summary>
//    /// Inserisce un batch di record con path ottimizzato: un lock file, un aggiornamento header,
//    /// buffer pooled. ponytail: nessun riutilizzo slot cancellati nel batch — solo append sequenziale.
//    /// </summary>
//    /// <param name="records">Valori per ogni record da inserire.</param>
//    /// <param name="keysPerRecord">Chiavi indice per record; null o vuoto se nessun indice.</param>
//    public void AppendManyRecords(
//        IReadOnlyList<Dictionary<string, object?>> records,
//        IReadOnlyList<IReadOnlyDictionary<IIndexHandle, byte[]>>? keysPerRecord = null)
//    {
//        ArgumentNullException.ThrowIfNull(records);
//        if (records.Count == 0)
//        {
//            return;
//        }
//
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        var transactionStack = GetTransactionStackOrNull(threadId);
//        var context = transactionStack is { Count: > 0 } ? transactionStack.Peek() : null;
//        var isAutoCommit = context == null;
//
//        var recordPayloadLength = _dbfFile.Header.RecordLength - 1;
//        var totalPayload = checked(recordPayloadLength * records.Count);
//        var batchPayload = ArrayPool<byte>.Shared.Rent(totalPayload);
//        try
//        {
//            for (var i = 0; i < records.Count; i++)
//            {
//                WriteValuesToBuffer(
//                    records[i],
//                    batchPayload.AsSpan(i * recordPayloadLength, recordPayloadLength));
//            }
//
//            _txHandle.LockFile();
//            try
//            {
//                var firstRecNo = _txHandle.AppendRawRecordsBatch(
//                    batchPayload.AsSpan(0, totalPayload),
//                    recordPayloadLength,
//                    records.Count);
//
//                _currentRecordNumber = firstRecNo + records.Count - 1;
//                _currentLeafPageNum = 0;
//                _currentIndexInLeaf = 0;
//                UpdateBofEofStatus();
//
//                if (keysPerRecord is { Count: > 0 } && _indexes.Count > 0)
//                {
//                    var indexBatches = new List<(ReadOnlyMemory<byte> key, int recno)>[_indexes.Count];
//                    for (var i = 0; i < _indexes.Count; i++)
//                    {
//                        indexBatches[i] = [];
//                    }
//
//                    for (var i = 0; i < records.Count; i++)
//                    {
//                        if (i >= keysPerRecord.Count)
//                        {
//                            break;
//                        }
//
//                        var recNo = firstRecNo + i;
//                        for (var j = 0; j < _indexes.Count; j++)
//                        {
//                            var index = _indexes[j];
//                            if (keysPerRecord[i].TryGetValue(index, out var newKey))
//                            {
//                                indexBatches[j].Add((newKey, recNo));
//                            }
//                        }
//                    }
//
//                    for (var j = 0; j < _indexes.Count; j++)
//                    {
//                        var batch = indexBatches[j];
//                        if (batch.Count > 0)
//                        {
//                            _indexes[j].InsertBatch(batch, context);
//                        }
//                    }
//                }
//
//                foreach (var index in _indexes)
//                {
//                    index.InvalidateCache();
//                }
//
//                for (var i = 0; i < records.Count; i++)
//                {
//                    RecordReplicationOp(DbfReplicationOp.Append, firstRecNo + i, records[i], context);
//                }
//
//                if (isAutoCommit)
//                {
//                    FinalizeAutoCommitUnlessBulkLoad();
//                }
//            }
//            finally
//            {
//                _txHandle.UnlockFile();
//            }
//        }
//        finally
//        {
//            ArrayPool<byte>.Shared.Return(batchPayload);
//        }
//    }
//
//    /// <summary>
//    /// Aggiunge un nuovo record vuoto (blank) alla fine del file, aggiorna tutti gli indici aperti
//    /// e posiziona il cursore sul nuovo record.
//    /// </summary>
//    /// <remarks>
//    /// Questa è l'implementazione a basso livello per il comando APPEND BLANK di Clipper.
//    /// Tutti i campi del nuovo record avranno i loro valori predefiniti (vuoti).
//    /// </remarks>
//    public void AppendBlankRecord(IReadOnlyDictionary<IIndexHandle, byte[]> blankKeys)
//    {
//        // Crea un dizionario vuoto che rappresenta il record blank.
//        // Il metodo AppendRecord si occuperà di scrivere i valori di default per ogni tipo di campo.
//        var blankValues = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//        AppendRecord(blankValues, blankKeys);
//    }
//
//    /// <summary>
//    /// Aggiunge record da un altro file DBF a quello corrente.
//    /// È possibile specificare un valutatore di condizione opzionale.
//    /// </summary>
//    /// <param name="sourceFilePath">Il percorso del file DBF da cui importare i record.</param>
//    /// <param name="conditionEvaluator">
//    /// Il delegato opzionale per valutare la condizione. Se fornito, verranno aggiunti solo
//    /// i record per cui il valutatore restituisce true.
//    /// </param>
//    public void AppendFrom(string sourceFilePath, Func<Dictionary<string, object?>, bool>? conditionEvaluator)
//    {
//        using var sourceEngine = new DbfEngine(sourceFilePath, null, false, LockingStrategy.IntraProcess);
//        if (sourceEngine.GetFields().Count != GetFields().Count)
//        {
//            throw new InvalidOperationException("La struttura dei file DBF sorgente e destinazione non è compatibile.");
//        }
//
//        const int batchSize = 256;
//        var batch = new List<Dictionary<string, object?>>(batchSize);
//        List<IReadOnlyDictionary<IIndexHandle, byte[]>>? keysBatch = null;
//        if (GetIndexes().Any())
//        {
//            keysBatch = new List<IReadOnlyDictionary<IIndexHandle, byte[]>>(batchSize);
//        }
//
//        void FlushBatch()
//        {
//            if (batch.Count == 0)
//            {
//                return;
//            }
//
//            AppendManyRecords(batch, keysBatch);
//            batch.Clear();
//            keysBatch?.Clear();
//        }
//
//        BeginBulkLoad();
//        try
//        {
//            sourceEngine.GoTopInternal();
//
//            while (!sourceEngine.Eof)
//            {
//                var values = sourceEngine.GetCurrentRecordValues();
//                if (values != null)
//                {
//                    var shouldAppend = conditionEvaluator == null || conditionEvaluator(values);
//
//                    if (shouldAppend)
//                    {
//                        batch.Add(new Dictionary<string, object?>(values, StringComparer.OrdinalIgnoreCase));
//                        keysBatch?.Add(CalculateKeys(values, this));
//
//                        if (batch.Count >= batchSize)
//                        {
//                            FlushBatch();
//                        }
//                    }
//                }
//
//                sourceEngine.SkipInternal();
//            }
//
//            FlushBatch();
//        }
//        finally
//        {
//            EndBulkLoad();
//        }
//    }
//
//    /// <summary>
//    /// Imposta o rimuove una funzione di filtro per la navigazione dei record.
//    /// Quando un filtro è attivo, i comandi di navigazione considereranno solo i record
//    /// per cui il valutatore restituisce true.
//    /// </summary>
//    /// <param name="evaluator">
//    /// Il delegato del valutatore da usare. Se null, il filtro viene rimosso.
//    /// Il delegato riceve l'AST del filtro e i valori del record corrente.
//    /// </param>
//    /// <param name="filterExpression">
//    /// L'albero di espressione originale del filtro, conservato per riferimento (es. per la funzione DBFILTER()).
//    /// </param>
//    public void SetFilter(Func<Dictionary<string, object?>, bool>? evaluator, Expr? filterExpression)
//    {
//        _activeFilterEvaluator = evaluator;
//        _activeFilterExpression = filterExpression;
//
//        // Dopo aver impostato o rimosso un filtro, riposiziona il cursore
//        // all'inizio del file per garantire uno stato coerente.
//        GoTopInternal();
//    }
//
//    /// <summary>
//    /// Marca il record corrente come cancellato e lo rimuove da tutti gli indici aperti.
//    /// L'operazione è transazionalmente sicura e richiede un lock di scrittura esclusivo.
//    /// </summary>
//    /// <param name="keysToDelete">Un dizionario in sola lettura che mappa ogni handle di indice alla sua
//    /// rappresentazione in byte, calcolata sui dati del record da cancellare. Serve per
//    /// rimuovere correttamente le entry da tutti gli indici.</param>
//    /// <exception cref="LockTimeoutException">
//    /// Se non è possibile acquisire un lock di scrittura. In una transazione, causa un rollback automatico.
//    /// </exception>
//    /// <remarks>
//    /// Questa operazione è un "soft delete". Per il livello `Serializable`, acquisisce anche lock di scrittura
//    /// sulle chiavi di indice da rimuovere per prevenire conflitti con letture di intervalli.
//    /// </remarks>
//    public void DeleteCurrent(IReadOnlyDictionary<IIndexHandle, byte[]> keysToDelete)
//    {
//        if (_currentRecordNumber <= 0 || _currentRecordNumber > _dbfFile.Header.NumberOfRecords)
//            return;
//
//        var recNo = _currentRecordNumber;
//
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//        var isAutoCommit = context == null;
//
//        var lockKey = LockManagerBase.GetRecordLockKey(recNo);
//        try
//        {
//            _txHandle.RecordLockManager.AcquireWriteLock(lockKey, 5000, $"record {recNo}");
//        }
//        catch (DeadlockException)
//        {
//            RollbackOnLockFailure(isAutoCommit);
//            throw;
//        }
//        catch (LockTimeoutException)
//        {
//            RollbackOnLockFailure(isAutoCommit);
//            throw;
//        }
//
//        if (!isAutoCommit)
//        {
//            context!.WriteLockedRecNos.Add(recNo);
//        }
//
//        try
//        {
//            // Se il versioning non è attivo, esegui il soft delete classico.
//            if (_versioningManager == null)
//            {
//                _txHandle.SetDeletedFlag(recNo, true);
//                RegisterDeletedSlot(recNo);
//            }
//            else // Se il versioning è attivo, crea una nuova versione cancellata.
//            {
//                // La variabile 'currentMetadata' è stata rimossa perché non era utilizzata.
//                // var currentMetadata = _versioningManager.GetMetadata(recNo);
//
//                // Crea una copia di "undo" che diventerà la nostra nuova versione.
//                var newVersionRecNo = _txHandle.AppendUndoVersion(recNo, context);
//                // Marca questa nuova versione come cancellata.
//                _txHandle.SetDeletedFlag(newVersionRecNo, true);
//
//                // Aggiorna i metadati del record *originale* per puntare alla nuova versione cancellata.
//                var newMetadata = new VersioningManager.VersionMetadata
//                {
//                    Version = DateTime.UtcNow.Ticks,
//                    TransactionId = isAutoCommit ? 0 : context!.TransactionId,
//                    PreviousVersionRecNo = newVersionRecNo
//                };
//                _versioningManager.SetMetadata(recNo, newMetadata);
//            }
//
//            // Rimuovi le chiavi del record da tutti gli indici, usando le chiavi pre-calcolate.
//            foreach (var index in _indexes)
//            {
//                if (keysToDelete.TryGetValue(index, out var key))
//                {
//                    index.Delete(key, recNo, context);
//                }
//
//                index.InvalidateCache();
//            }
//
//            RecordReplicationOp(DbfReplicationOp.Delete, recNo, null, context);
//            if (isAutoCommit)
//            {
//                FinalizeAutoCommitUnlessBulkLoad();
//            }
//
//            _currentLeafPageNum = 0;
//            _currentIndexInLeaf = 0;
//        }
//        finally
//        {
//            if (isAutoCommit)
//            {
//                _txHandle.RecordLockManager.ReleaseWriteLock(lockKey);
//            }
//        }
//    }
//
//    /// <summary>
//    /// Rimuove il flag di cancellazione dal record corrente ("undelete") e lo reinserisce in tutti gli indici aperti.
//    /// L'operazione è transazionalmente sicura e richiede un lock di scrittura esclusivo.
//    /// </summary>
//    /// <param name="keysToInsert">Un dizionario in sola lettura che mappa ogni handle di indice alla sua
//    /// rappresentazione in byte, calcolata sui dati del record da ripristinare. Serve per reinserire
//    /// correttamente le entry in tutti gli indici.</param>
//    /// <exception cref="LockTimeoutException">
//    /// Se non è possibile acquisire un lock di scrittura. In una transazione, causa un rollback automatico.
//    /// </exception>
//    /// <remarks>
//    /// Questa operazione è l'inverso di `DeleteCurrent`. Per il livello `Serializable`, acquisisce anche lock
//    /// di scrittura sulle chiavi di indice da reinserire.
//    /// </remarks>
//    public void RecallCurrent(IReadOnlyDictionary<IIndexHandle, byte[]> keysToInsert)
//    {
//        if (_currentRecordNumber <= 0 || _currentRecordNumber > _dbfFile.Header.NumberOfRecords)
//            return;
//
//        var recNo = _currentRecordNumber;
//
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//        var isAutoCommit = context == null;
//
//        var lockKey = LockManagerBase.GetRecordLockKey(recNo);
//        try
//        {
//            _txHandle.RecordLockManager.AcquireWriteLock(lockKey, 5000, $"record {recNo}");
//        }
//        catch (DeadlockException)
//        {
//            RollbackOnLockFailure(isAutoCommit);
//            throw;
//        }
//        catch (LockTimeoutException)
//        {
//            RollbackOnLockFailure(isAutoCommit);
//            throw;
//        }
//
//        if (!isAutoCommit)
//        {
//            context!.WriteLockedRecNos.Add(recNo);
//        }
//
//        try
//        {
//            // Se il versioning non è attivo, esegui l'undelete classico.
//            if (_versioningManager == null)
//            {
//                _txHandle.SetDeletedFlag(recNo, false);
//            }
//            else // Se il versioning è attivo, crea una nuova versione "attiva".
//            {
//                // La variabile 'currentMetadata' è stata rimossa perché non era utilizzata.
//                // var currentMetadata = _versioningManager.GetMetadata(recNo);
//
//                // Crea una copia di "undo" che diventerà la nostra nuova versione attiva.
//                var newVersionRecNo = _txHandle.AppendUndoVersion(recNo, context);
//                // Assicurati che questa nuova versione non sia marcata come cancellata.
//                _txHandle.SetDeletedFlag(newVersionRecNo, false);
//
//                // Aggiorna i metadati del record *originale* per puntare alla nuova versione attiva.
//                var newMetadata = new VersioningManager.VersionMetadata
//                {
//                    Version = DateTime.UtcNow.Ticks,
//                    TransactionId = isAutoCommit ? 0 : context!.TransactionId,
//                    PreviousVersionRecNo = newVersionRecNo
//                };
//                _versioningManager.SetMetadata(recNo, newMetadata);
//            }
//
//            // Reinserisci le chiavi del record in tutti gli indici, usando le chiavi pre-calcolate.
//            foreach (var index in _indexes)
//            {
//                if (keysToInsert.TryGetValue(index, out var key))
//                {
//                    index.Insert(key, recNo, context);
//                }
//
//                index.InvalidateCache();
//            }
//
//            RecordReplicationOp(DbfReplicationOp.Recall, recNo, null, context);
//            if (isAutoCommit)
//            {
//                FinalizeAutoCommitUnlessBulkLoad();
//            }
//
//            _currentLeafPageNum = 0;
//            _currentIndexInLeaf = 0;
//        }
//        finally
//        {
//            if (isAutoCommit)
//            {
//                _txHandle.RecordLockManager.ReleaseWriteLock(lockKey);
//            }
//        }
//    }
//
//    /// <summary>
//    /// Esporta i record del file corrente in un nuovo file DBF.
//    /// È possibile specificare un valutatore di condizione opzionale.
//    /// </summary>
//    /// <param name="destinationFilePath">Il percorso del nuovo file DBF da creare.</param>
//    /// <param name="conditionEvaluator">
//    /// Il delegato opzionale per valutare la condizione. Se fornito, verranno copiati solo
//    /// i record per cui il valutatore restituisce true.
//    /// </param>
//    /// <param name="codeSchema">Lo schema del TYPE del codice, necessario per creare il file .gms.</param>
//    public void CopyTo(string destinationFilePath, Func<Dictionary<string, object?>, bool>? conditionEvaluator,
//        DbfSchemaInfo codeSchema)
//    {
//        DbfFile.CreateNew(destinationFilePath, _encoding, GetFields());
//        DbfSchemaManager.WriteSchemaInfo(destinationFilePath, codeSchema);
//
//        using var destEngine = new DbfEngine(destinationFilePath, null, true, LockingStrategy.IntraProcess,
//            codeSchema: codeSchema);
//        destEngine.Evaluator = Evaluator; // Propaga l'evaluator per coerenza
//
//        // Salva lo stato del cursore
//        var originalRecordNumber = GetCurrentRecordNumber();
//        var originalEof = Eof;
//        var originalBof = Bof;
//
//        GoTopInternal();
//
//        while (!Eof)
//        {
//            var values = GetCurrentRecordValues();
//            if (values != null)
//            {
//                var shouldCopy = conditionEvaluator == null || conditionEvaluator(values);
//
//                if (shouldCopy)
//                {
//                    // REFACTOR: Usa il metodo helper centralizzato per calcolare le chiavi.
//                    var newKeys = CalculateKeys(values, destEngine);
//
//                    destEngine.AppendRecord(values, newKeys);
//                }
//            }
//
//            SkipInternal();
//        }
//
//        // Ripristina lo stato del cursore
//        _currentRecordNumber = (int)originalRecordNumber;
//        Eof = originalEof;
//        Bof = originalBof;
//        UpdateBofEofStatus();
//    }
//
//    /// <summary>
//    /// Writes formatted LIST lines to <paramref name="writer"/> and restores the cursor when done.
//    /// ponytail: iteration mirrors COPY TO; predicates supplied by GPM for FOR/WHILE/NEXT.
//    /// </summary>
//    /// <param name="writer">Output sink (console buffer, file, etc.).</param>
//    /// <param name="rowValues">Builds column values for one record.</param>
//    /// <param name="includeRecord">When set (FOR), only matching rows are written.</param>
//    /// <param name="whileContinue">When set (WHILE), stops when this returns false.</param>
//    /// <param name="maxRecords">When set (NEXT), caps rows written from the current position.</param>
//    /// <param name="startAtTop">When true, positions at the first logical record before iterating.</param>
//    /// <returns>Number of lines written.</returns>
//    public int WriteList(
//        TextWriter writer,
//        Func<Dictionary<string, object?>, IReadOnlyList<object?>> rowValues,
//        Func<Dictionary<string, object?>, bool>? includeRecord,
//        Func<Dictionary<string, object?>, bool>? whileContinue,
//        int? maxRecords,
//        bool startAtTop)
//    {
//        ArgumentNullException.ThrowIfNull(writer);
//        ArgumentNullException.ThrowIfNull(rowValues);
//
//        var originalRecordNumber = GetCurrentRecordNumber();
//        var originalEof = Eof;
//        var originalBof = Bof;
//
//        if (startAtTop)
//        {
//            GoTopInternal();
//        }
//
//        var written = 0;
//        var matched = 0;
//
//        while (!Eof)
//        {
//            if (maxRecords.HasValue && matched >= maxRecords.Value)
//            {
//                break;
//            }
//
//            if (!_deletedVisible && IsCurrentRecordDeleted())
//            {
//                SkipInternal();
//                continue;
//            }
//
//            var values = GetCurrentRecordValues();
//            if (values is null)
//            {
//                break;
//            }
//
//            if (whileContinue is not null && !whileContinue(values))
//            {
//                break;
//            }
//
//            var emit = includeRecord is null || includeRecord(values);
//            if (emit)
//            {
//                var row = rowValues(values);
//                writer.WriteLine(DbfListFormatter.FormatLine(GetCurrentRecordNumber(), row));
//                written++;
//                if (maxRecords.HasValue)
//                {
//                    matched++;
//                }
//            }
//
//            SkipInternal();
//        }
//
//        _currentRecordNumber = (int)originalRecordNumber;
//        Eof = originalEof;
//        Bof = originalBof;
//        UpdateBofEofStatus();
//
//        return written;
//    }
//
//    /// <summary>
//    /// Rimuove fisicamente tutti i record marcati come cancellati, creando una copia compattata del file
//    /// e ricostruendo tutti gli indici associati. Questa operazione è ad alta intensità di I/O e richiede
//    /// un accesso esclusivo al file.
//    /// </summary>
//    /// <exception cref="InvalidOperationException">Se viene tentato un PACK all'interno di una transazione esplicita,
//    /// poiché l'operazione non può essere annullata (rollback) in modo sicuro.</exception>
//    /// <exception cref="IOException">Se non è possibile ottenere un lock esclusivo sul file.</exception>
//    /// <remarks>
//    /// L'operazione di PACK è ora transazionalmente sicura a livello di file system e non invalida più
//    /// l'istanza corrente del DbfEngine. La logica è la seguente:
//    /// 1. Verifica che non ci sia una transazione attiva.
//    /// 2. Acquisisce un lock di scrittura esclusivo sull'intero file, bloccando ogni altro accesso.
//    /// 3. Crea un nuovo file temporaneo e vi copia solo i record attivi dal file corrente.
//    /// 4. Sostituisce atomicamente il file originale con la nuova versione compattata.
//    /// 5. Ricostruisce da zero tutti gli indici associati al nuovo file.
//    /// 6. Al termine, l'istanza DbfEngine rimane valida e operativa sul nuovo file compattato.
//    /// </remarks>
//    public void Pack()
//    {
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//
//        if (transactionStack is { Count: > 0 })
//        {
//            throw new InvalidOperationException(
//                "L'operazione PACK non può essere eseguita all'interno di una transazione esplicita.");
//        }
//
//        // Acquisisce un lock esclusivo per tutta la durata dell'operazione.
//        _txHandle.LockFile();
//        var tempFilePath = Path.GetTempFileName();
//        try
//        {
//            var originalFilePath = _dbfFile.FilePath;
//
//            // Crea il file compattato temporaneo.
//            // Usiamo istanze DbfFile a basso livello per leggere dal file sorgente (ancora aperto)
//            // e scrivere sul file di destinazione.
//            _dbfFile.CloneStructure(tempFilePath);
//            using (var destDbf = new DbfFile(tempFilePath, true))
//            {
//                for (var i = 1; i <= _dbfFile.Header.NumberOfRecords; i++)
//                {
//                    var (isDeleted, rawData) = _dbfFile.ReadRawRecord(i);
//                    if (!isDeleted)
//                    {
//                        destDbf.AppendRawRecord(rawData);
//                    }
//                }
//            }
//
//            // Sostituisce atomicamente il file originale.
//            // Poiché l'handle del file originale è ancora aperto, questa operazione
//            // funzionerà correttamente su Windows e sistemi simili che permettono la sostituzione di file aperti.
//            File.Replace(tempFilePath, originalFilePath, null);
//
//            // Ora che il file sottostante è stato sostituito, dobbiamo invalidare lo stato interno
//            // dell'engine e degli indici e forzare una re-inizializzazione.
//
//            // 1. Invalidare le cache degli indici
//            foreach (var index in _indexes)
//            {
//                index.InvalidateCache();
//            }
//
//            _deletedRecordCache.Clear();
//            _deletedSlotSet.Clear();
//            _deletedSlotsIndexed = false;
//
//            // 2. Ricostruire tutti gli indici scrivibili sul nuovo file.
//            Reindex();
//
//            // 3. Riposizionare il cursore all'inizio.
//            GoTop();
//            RecordReplicationPack();
//        }
//        finally
//        {
//            if (File.Exists(tempFilePath))
//            {
//                File.Delete(tempFilePath);
//            }
//
//            // Rilascia il lock esclusivo.
//            _txHandle.UnlockFile();
//        }
//    }
//
//    /// <summary>
//    /// Rimuove permanentemente tutti i record dal file, mantenendo la struttura.
//    /// L'operazione è transazionale e svuota anche tutti gli indici associati. Richiede un lock di scrittura
//    /// esclusivo sull'intero file.
//    /// </summary>
//    /// <remarks>
//    /// 1. Acquisisce un lock di scrittura sull'intero file.
//    /// 2. Esegue lo ZAP transazionale sul file DBF, che registra l'aggiornamento dell'header nel WAL.
//    /// 3. Se il versioning MVCC è attivo, svuota anche il file delle versioni.
//    /// 4. Svuota tutti gli indici associati.
//    /// 5. Se in modalità auto-commit, finalizza la transazione.
//    /// 6. Rilascia il lock di file.
//    /// </remarks>
//    public void Zap()
//    {
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//        var isAutoCommit = context == null;
//
//        _txHandle.LockFile();
//
//        try
//        {
//            _txHandle.Zap();
//            _versioningManager?.Clear();
//
//            _currentRecordNumber = 0;
//            _currentLeafPageNum = 0;
//            _currentIndexInLeaf = 0;
//            Eof = true;
//            Bof = true;
//            Found = false;
//
//            foreach (var index in _indexes)
//            {
//                index.Rebuild(Enumerable.Empty<(ReadOnlyMemory<byte> key, int recno)>);
//                // Invalida la cache di tutti gli indici (inclusi quelli read-only)
//                index.InvalidateCache();
//            }
//
//            if (isAutoCommit)
//            {
//                FinalizeAutoCommitUnlessBulkLoad();
//            }
//        }
//        finally
//        {
//            _txHandle.UnlockFile();
//        }
//    }
//
//    /// <summary>
//    /// Esegue una serie di controlli di validazione non distruttivi sul file DBF e sui suoi indici.
//    /// </summary>
//    /// <returns>
//    /// Una tupla contenente un booleano `IsValid` e una `string` con un messaggio di riepilogo.
//    /// </returns>
//    public (bool IsValid, string Message) ValidateIntegrity()
//    {
//        var errorMessages = new List<string>();
//
//        // 1. Valida tutti gli indici aperti.
//        foreach (var index in _indexes)
//        {
//            if (index is NtxFile ntx) // Assicuriamoci che sia il nostro indice scrivibile
//            {
//                if (!ntx.ValidateIndex())
//                {
//                    errorMessages.Add($"Errore: L'indice '{ntx.Name}' è corrotto.");
//                }
//            }
//        }
//
//        // 2. Controlla la coerenza tra il numero di record nel file e l'header.
//        var physicalRecordCount =
//            (_dbfFile.GetFileLength() - _dbfFile.Header.HeaderLength) / _dbfFile.Header.RecordLength;
//        if (physicalRecordCount != _dbfFile.Header.NumberOfRecords)
//        {
//            errorMessages.Add(
//                $"Errore di coerenza: L'header riporta {_dbfFile.Header.NumberOfRecords} record, ma la dimensione fisica del file ne indica {physicalRecordCount}.");
//        }
//
//        if (errorMessages.Count > 0)
//        {
//            return (false, string.Join("\n", errorMessages));
//        }
//
//        return (true, "Validazione completata con successo. Nessun errore trovato.");
//    }
//
//    /// <summary>
//    /// Ricostruisce da zero tutti gli indici scrivibili (.ntx) associati a questa work area in modo ottimizzato.
//    /// L'operazione richiede un lock di scrittura esclusivo sull'intero file DBF per garantirne l'esecuzione sicura.
//    /// Il calcolo delle chiavi per ogni record viene ora delegato al metodo centralizzato `NormalizeSingleKeyBytes`.
//    /// </summary>
//    /// <exception cref="LockTimeoutException">
//    /// Lanciata se non è possibile ottenere un lock esclusivo sul file per eseguire l'operazione.
//    /// </exception>
//    /// <remarks>
//    /// Questa versione ottimizzata esegue una sola scansione del file DBF. Per ogni record attivo, calcola
//    /// le chiavi per tutti gli indici, le memorizza in memoria e, al termine, ricostruisce ogni indice
//    /// in modo efficiente (bulk load) a partire dai dati ordinati.
//    /// </remarks>
//    public void Reindex()
//    {
//        _txHandle.LockFile();
//
//        try
//        {
//            var writableIndexes = _indexes.OfType<IWritableIndexHandle>().ToList();
//            if (writableIndexes.Count == 0) return;
//
//            // Prepara una struttura dati per raccogliere le chiavi di tutti gli indici in un unico passaggio.
//            var indexEntries = writableIndexes.ToDictionary(
//                index => index,
//                _ => new List<(ReadOnlyMemory<byte> key, int recno)>()
//            );
//
//            // Esegue una SOLA scansione del file DBF.
//            var recordCount = (int)GetTotalRecordCount();
//            for (var i = 1; i <= recordCount; i++)
//            {
//                var (isDeleted, _) = _dbfFile.ReadRawRecord(i);
//                if (isDeleted) continue;
//
//                var values = GetRecordValuesByRecNo(i);
//                if (values == null) continue;
//
//                // Per ogni record, calcola la chiave per ogni indice.
//                foreach (var index in writableIndexes)
//                {
//                    var keyExpressionString = index.GetKeyExpressionString();
//                    var targetType = new StringType(index.KeyLength); // Assunzione DBF
//
//                    // Valuta l'espressione della chiave usando l'Evaluator iniettato (RECNO() needs cursor).
//                    var keyObject = EvaluateWithRecordCursor(i,
//                        () => Evaluator!(keyExpressionString, values, targetType));
//
//                    // DELEGATO: Chiama il metodo helper centralizzato per normalizzare la singola chiave.
//                    var finalKeyBytes = NormalizeSingleKeyBytes(keyObject, index.KeyLength, _encoding);
//
//                    if (finalKeyBytes != null)
//                    {
//                        indexEntries[index].Add((new ReadOnlyMemory<byte>(finalKeyBytes), i));
//                    }
//                }
//            }
//
//            // Ora, ricostruisce ogni indice in parallelo a partire dalla sua lista di chiavi.
//            System.Threading.Tasks.Parallel.ForEach(writableIndexes, index =>
//            {
//                var entries = indexEntries[index];
//                entries.Sort((a, b) => a.key.Span.SequenceCompareTo(b.key.Span));
//                index.Rebuild(() => entries);
//            });
//
//            RecordReplicationReindex();
//        }
//        finally
//        {
//            _txHandle.UnlockFile();
//        }
//    }
//
//    /// <summary>
//    /// Avvia una nuova transazione per il thread corrente o incrementa il livello di annidamento di una transazione esistente.
//    /// </summary>
//    /// <param name="level">Il livello di isolamento desiderato per la transazione. Si applica solo alla transazione più esterna.
//    /// Il valore predefinito è <see cref="IsolationLevel.ReadCommitted"/>.</param>
//    /// <remarks>
//    /// Questo motore supporta transazioni nidificate. La prima chiamata a `BeginTransaction` avvia una transazione fisica
//    /// (registrando un marcatore nel WAL) e crea un <see cref="TransactionContext"/>. Chiamate successive sullo stesso thread
//    /// incrementano semplicemente il livello di annidamento spingendo lo stesso contesto sullo stack.
//    /// Le modifiche verranno rese permanenti solo quando verrà eseguito il `Commit` corrispondente alla prima chiamata
//    /// `BeginTransaction`. Un `Rollback` a qualsiasi livello annullerà l'intera transazione.
//    /// </remarks>
//    public void BeginTransaction(IsolationLevel level = IsolationLevel.ReadCommitted)
//    {
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        var transactionStack = GetOrCreateTransactionStack(threadId);
//
//        if (transactionStack.Count == 0)
//        {
//            // È la transazione più esterna: avvia la transazione fisica.
//            var txId = _txHandle.BeginTransaction();
//            var context = new TransactionContext(txId, level); // Passa il livello di isolamento
//            DeadlockMonitor.BindTransaction(threadId, txId);
//            transactionStack.Push(context);
//            WriteReplicationTxnMarker(ReplicationOp.TxnBegin);
//        }
//        else
//        {
//            // Transazione nidificata: riutilizza il contesto di quella esterna.
//            var outerContext = transactionStack.Peek();
//            transactionStack.Push(outerContext);
//        }
//    }
//
//    /// <summary>
//    /// Esegue il commit del livello di transazione corrente. 
//    /// Se questo è il livello di transazione più esterno, tutte le modifiche vengono rese permanenti.
//    /// </summary>
//    /// <exception cref="InvalidOperationException">
//    /// Lanciata se non c'è nessuna transazione attiva da commettere per il thread corrente.
//    /// </exception>
//    /// <remarks>
//    /// In un contesto di transazioni nidificate, una chiamata a `Commit` decrementa semplicemente il livello di 
//    /// annidamento. La transazione fisica viene finalizzata (scrivendo il marcatore di commit nel WAL e pulendo il file di log) 
//    /// e tutti i lock acquisiti (sia di lettura che di scrittura) vengono rilasciati solo quando il commit
//    /// della transazione più esterna (la prima `BeginTransaction`) viene eseguito.
//    /// </remarks>
//    public void Commit()
//    {
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        if (!_threadTransactions.TryGetValue(threadId, out var transactionStack) || transactionStack.Count == 0)
//        {
//            throw new InvalidOperationException("Nessuna transazione attiva da commettere.");
//        }
//
//        var context = transactionStack.Pop();
//
//        // Se lo stack è vuoto, è il commit della transazione più esterna.
//        if (transactionStack.Count == 0)
//        {
//            try
//            {
//                _txHandle.CommitTransaction();
//                FlushReplicationOnCommit(context);
//                WriteReplicationTxnMarker(ReplicationOp.TxnCommit);
//                FinalizeCommitUnlessBulkLoad(); // Pulisce il file WAL
//            }
//            finally
//            {
//                // Rilascia sempre tutti i lock acquisiti da questa transazione.
//                ReleaseTransactionalLocks(context);
//                DeadlockMonitor.UnbindTransaction(threadId);
//                RemoveTransactionStack(threadId);
//            }
//        }
//    }
//
//    private Stack<TransactionContext>? GetTransactionStackOrNull(int threadId) =>
//        _threadTransactions.TryGetValue(threadId, out var stack) ? stack : null;
//
//    private Stack<TransactionContext> GetOrCreateTransactionStack(int threadId) =>
//        _threadTransactions.GetOrAdd(threadId, static _ => new Stack<TransactionContext>());
//
//    private void RemoveTransactionStack(int threadId) =>
//        _threadTransactions.TryRemove(threadId, out _);
//
//    /// <summary>
//    /// Annulla l'intera transazione in corso per il thread corrente, scartando tutte le modifiche,
//    /// indipendentemente dal livello di annidamento.
//    /// </summary>
//    /// <exception cref="InvalidOperationException">
//    /// Lanciata se non c'è nessuna transazione attiva da annullare per il thread corrente.
//    /// </exception>
//    /// <remarks>
//    /// A differenza di `Commit`, una singola chiamata a `Rollback` a qualsiasi livello di annidamento
//    /// termina immediatamente l'intera catena di transazioni. Tutte le operazioni registrate nel WAL
//    /// dall'inizio della transazione più esterna vengono scartate. L'operazione garantisce anche
//    /// il rilascio di tutti i lock (di lettura e scrittura) acquisiti durante la transazione.
//    /// </remarks>
//    public void Rollback()
//    {
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        if (!_threadTransactions.TryGetValue(threadId, out var transactionStack) || transactionStack.Count == 0)
//        {
//            throw new InvalidOperationException("Nessuna transazione attiva da annullare.");
//        }
//
//        // Un rollback annulla l'intera catena, quindi prendiamo il contesto più esterno.
//        var outerContext = transactionStack.Last();
//        outerContext.PendingDbfReplicationOps.Clear();
//
//        try
//        {
//            _txHandle.RollbackTransaction();
//        }
//        finally
//        {
//            // Marca i record di undo come cancellati per recuperare lo spazio.
//            // Non possiamo usare una FreeList come in ISAM, quindi questo è il modo corretto per
//            // invalidare lo spazio in un file DBF.
//            foreach (var recNo in outerContext.UndoLogIdentifiers)
//            {
//                // Questa è un'operazione diretta sul file, non transazionale,
//                // perché stiamo pulendo dopo un rollback.
//                _txHandle.SetDeletedFlag((int)recNo, true);
//            }
//
//            // Rilascia sempre tutti i lock e pulisce completamente lo stack.
//            ReleaseTransactionalLocks(outerContext);
//            DeadlockMonitor.UnbindTransaction(threadId);
//            _threadTransactions.TryRemove(threadId, out _);
//        }
//    }
//
//    /// <summary>
//    /// Rollback transazionale su conflitto lock (timeout o deadlock rilevato dal wait-for graph).
//    /// </summary>
//    private void RollbackOnLockFailure(bool isAutoCommit)
//    {
//        if (!isAutoCommit)
//        {
//            Rollback();
//        }
//    }
//
//    private void RegisterDeletedSlot(int recNo)
//    {
//        if (_deletedSlotSet.Add(recNo))
//        {
//            _deletedRecordCache.Enqueue(recNo);
//        }
//    }
//
//    private bool TryTakeDeletedSlot(out int recNo)
//    {
//        if (_deletedRecordCache.TryDequeue(out recNo))
//        {
//            _deletedSlotSet.Remove(recNo);
//            return true;
//        }
//
//        if (!_deletedSlotsIndexed && _dbfFile.Header.NumberOfRecords > 0)
//        {
//            _deletedSlotsIndexed = true;
//            var recordLength = _dbfFile.Header.RecordLength;
//            var buffer = ArrayPool<byte>.Shared.Rent(recordLength);
//            try
//            {
//                for (var i = 1; i <= _dbfFile.Header.NumberOfRecords; i++)
//                {
//                    _dbfFile.ReadRawRecordInto(i, buffer.AsSpan(0, recordLength), out var isDeleted);
//                    if (isDeleted)
//                    {
//                        RegisterDeletedSlot(i);
//                    }
//                }
//            }
//            finally
//            {
//                ArrayPool<byte>.Shared.Return(buffer);
//            }
//
//            if (_deletedRecordCache.TryDequeue(out recNo))
//            {
//                _deletedSlotSet.Remove(recNo);
//                return true;
//            }
//        }
//
//        recNo = 0;
//        return false;
//    }
//
//    private void FinalizeCommitUnlessBulkLoad()
//    {
//        if (!_txHandle.IsBulkLoadActive)
//        {
//            _txHandle.FinalizeCommit();
//        }
//    }
//
//    private void FinalizeAutoCommitUnlessBulkLoad()
//    {
//        if (!_txHandle.IsBulkLoadActive)
//        {
//            _txHandle.FinalizeAutoCommit();
//        }
//    }
//
//    private void RecordReplicationOp(
//        DbfReplicationOp op,
//        int recNo,
//        IReadOnlyDictionary<string, object?>? values,
//        TransactionContext? context)
//    {
//        if (_replicationLog is null)
//        {
//            return;
//        }
//
//        var line = DbfReplicationLog.FormatLine(op, recNo, values);
//        if (context is null)
//        {
//            _replicationLog.WriteLines([line]);
//            return;
//        }
//
//        context.PendingDbfReplicationOps.Add(line);
//    }
//
//    private void FlushReplicationOnCommit(TransactionContext context)
//    {
//        if (_replicationLog is null || context.PendingDbfReplicationOps.Count == 0)
//        {
//            return;
//        }
//
//        _replicationLog.WriteLines(context.PendingDbfReplicationOps);
//        context.PendingDbfReplicationOps.Clear();
//    }
//
//    private void WriteReplicationTxnMarker(ReplicationOp op)
//    {
//        if (_replicationLog is null)
//        {
//            return;
//        }
//
//        _replicationLog.WriteLines([ReplicationLogLine.FormatTxnMarker(op)]);
//    }
//
//    private void RecordReplicationPack()
//    {
//        if (_replicationLog is null)
//        {
//            return;
//        }
//
//        var generation = ++_packGeneration;
//        _replicationLog.WriteLines([ReplicationLogLine.FormatPack(generation)]);
//    }
//
//    private void RecordReplicationSchema()
//    {
//        if (_replicationLog is null)
//        {
//            return;
//        }
//
//        var gmsBytes = DbfSchemaManager.ReadSchemaBytes(FilePath);
//        if (gmsBytes is null)
//        {
//            return;
//        }
//
//        var generation = ++_schemaGeneration;
//        var metadata = ReplicationSchemaMetadata.ForDbfGms(generation, gmsBytes);
//        _replicationLog.WriteLines([ReplicationLogLine.FormatSchema(metadata)]);
//    }
//
//    private void RecordReplicationReindex()
//    {
//        if (_replicationLog is null)
//        {
//            return;
//        }
//
//        var generation = ++_reindexGeneration;
//        _replicationLog.WriteLines([ReplicationLogLine.FormatReindex(generation)]);
//    }
//
//    #region Import & Export
//
//    /// <summary>
//    /// Esporta i record del file DBF in un file in formato CSV (Comma-Separated Values).
//    /// L'esportazione rispetta il filtro (<see cref="SetFilter"/>) e l'impostazione di <see cref="SetDeleted"/> attualmente attivi.
//    /// </summary>
//    /// <param name="filePath">Il percorso completo del file .csv da creare. Se esiste, sarà sovrascritto.</param>
//    public void ExportToCsv(string filePath)
//    {
//        var originalRecordNumber = GetCurrentRecordNumber();
//
//        using (var writer = new StreamWriter(filePath, false, _encoding))
//        {
//            // 1. Scrive la riga di intestazione con i nomi dei campi.
//            var header = string.Join(",", GetFields().Select(f => f.Name));
//            writer.WriteLine(header);
//
//            // 2. Itera su tutti i record che soddisfano le condizioni attive (filtro, deleted).
//            GoTopInternal();
//            while (!Eof)
//            {
//                var currentValues = GetCurrentRecordValues();
//                if (currentValues != null)
//                {
//                    // 3. Formatta e scrive ogni campo del record.
//                    var values = GetFields()
//                        .Select(field => currentValues.GetValueOrDefault(field.Name))
//                        .Select(FormatCsvField);
//
//                    writer.WriteLine(string.Join(",", values));
//                }
//
//                SkipInternal();
//            }
//        }
//
//        // 4. Ripristina la posizione originale del cursore.
//        if (originalRecordNumber > 0 && originalRecordNumber <= GetTotalRecordCount())
//        {
//            _currentRecordNumber = (int)originalRecordNumber;
//            UpdateBofEofStatus();
//        }
//        else
//        {
//            GoTopInternal();
//        }
//    }
//
//    /// <summary>
//    /// Esporta i record del file DBF in un file in formato JSON (JavaScript Object Notation).
//    /// L'esportazione rispetta il filtro (<see cref="SetFilter"/>) e l'impostazione di <see cref="SetDeleted"/> attualmente attivi.
//    /// </summary>
//    /// <param name="filePath">Il percorso completo del file .json da creare. Se esiste, sarà sovrascritto.</param>
//    public void ExportToJson(string filePath)
//    {
//        var originalRecordNumber = GetCurrentRecordNumber();
//
//        var jsonArray = new System.Text.Json.Nodes.JsonArray();
//
//        GoTopInternal();
//        while (!Eof)
//        {
//            var currentValues = GetCurrentRecordValues();
//            if (currentValues != null)
//            {
//                var jsonObject = new System.Text.Json.Nodes.JsonObject();
//                foreach (var field in currentValues)
//                {
//                    jsonObject[field.Key] = System.Text.Json.Nodes.JsonValue.Create(field.Value);
//                }
//
//                jsonArray.Add(jsonObject);
//            }
//
//            SkipInternal();
//        }
//
//        var options = new System.Text.Json.JsonSerializerOptions { WriteIndented = true };
//        File.WriteAllText(filePath, jsonArray.ToJsonString(options), _encoding);
//
//        if (originalRecordNumber > 0 && originalRecordNumber <= GetTotalRecordCount())
//        {
//            _currentRecordNumber = (int)originalRecordNumber;
//            UpdateBofEofStatus();
//        }
//        else
//        {
//            GoTopInternal();
//        }
//    }
//
//    /// <summary>
//    /// Importa record da un file CSV in modo atomico.
//    /// L'intera operazione viene eseguita all'interno di un'unica transazione. Se si verifica un errore,
//    /// tutte le aggiunte vengono annullate.
//    /// </summary>
//    /// <param name="filePath">Il percorso del file .csv da importare.</param>
//    /// <exception cref="InvalidDataException">Se l'header del CSV è vuoto, se i nomi dei campi non corrispondono allo schema,
//    /// o se una riga ha un numero di colonne diverso dall'header.</exception>
//    public void ImportFromCsv(string filePath)
//    {
//        BeginTransaction();
//        try
//        {
//            using (var reader = new StreamReader(filePath, _encoding))
//            {
//                var headerLine = reader.ReadLine();
//                if (string.IsNullOrEmpty(headerLine))
//                    throw new InvalidDataException("Il file CSV è vuoto o non contiene una riga di intestazione.");
//
//                var headers = headerLine.Split(',');
//                var schemaFields = GetFields().ToDictionary(f => f.Name, StringComparer.OrdinalIgnoreCase);
//
//                foreach (var header in headers)
//                {
//                    if (!schemaFields.ContainsKey(header))
//                        throw new InvalidDataException($"Il campo '{header}' dall'header CSV non esiste nel database.");
//                }
//
//                while (reader.ReadLine() is { } dataLine)
//                {
//                    var values = dataLine.Split(',');
//                    if (values.Length != headers.Length)
//                        throw new InvalidDataException(
//                            "Una riga di dati ha un numero di colonne diverso dall'intestazione.");
//
//                    var recordValues = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//                    for (var i = 0; i < headers.Length; i++)
//                    {
//                        recordValues[headers[i]] = values[i];
//                    }
//
//                    // REFACTOR: Usa il metodo helper centralizzato per calcolare le chiavi.
//                    var keys = CalculateKeys(recordValues, this);
//
//                    AppendRecord(recordValues, keys);
//                }
//            }
//
//            Commit();
//        }
//        catch (Exception)
//        {
//            Rollback();
//            throw;
//        }
//    }
//
//    /// <summary>
//    /// Importa record da un file JSON in modo atomico. Il file deve contenere un array di oggetti JSON.
//    /// L'intera operazione viene eseguita all'interno di un'unica transazione. Se si verifica un errore,
//    /// tutte le aggiunte vengono annullate.
//    /// </summary>
//    /// <param name="filePath">Il percorso del file .json da importare.</param>
//    /// <exception cref="System.Text.Json.JsonException">Se il file non contiene un JSON valido o non è un array di oggetti.</exception>
//    /// <exception cref="InvalidDataException">Se una proprietà di un oggetto JSON non corrisponde a un campo dello schema.</exception>
//    public void ImportFromJson(string filePath)
//    {
//        BeginTransaction();
//        try
//        {
//            var jsonString = File.ReadAllText(filePath, _encoding);
//            var jsonArray = System.Text.Json.Nodes.JsonNode.Parse(jsonString) as System.Text.Json.Nodes.JsonArray;
//
//            if (jsonArray == null)
//                throw new System.Text.Json.JsonException("Il file JSON non contiene un array alla radice.");
//
//            var schemaFields = GetFields().ToDictionary(f => f.Name, StringComparer.OrdinalIgnoreCase);
//
//            foreach (var jsonNode in jsonArray)
//            {
//                var jsonObject = jsonNode as System.Text.Json.Nodes.JsonObject;
//                if (jsonObject == null)
//                    continue;
//
//                var recordValues = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//
//                foreach (var property in jsonObject)
//                {
//                    if (!schemaFields.ContainsKey(property.Key))
//                        throw new InvalidDataException(
//                            $"La proprietà JSON '{property.Key}' non corrisponde a nessun campo del database.");
//
//                    object? value = null;
//                    if (property.Value is System.Text.Json.Nodes.JsonValue jsonValue)
//                    {
//                        jsonValue.TryGetValue(out value);
//                    }
//                    else if (property.Value != null)
//                    {
//                        value = property.Value.ToJsonString();
//                    }
//
//                    recordValues[property.Key] = value;
//                }
//
//                // REFACTOR: Usa il metodo helper centralizzato per calcolare le chiavi.
//                var keys = CalculateKeys(recordValues, this);
//
//                AppendRecord(recordValues, keys);
//            }
//
//            Commit();
//        }
//        catch (Exception)
//        {
//            Rollback();
//            throw;
//        }
//    }
//
//    #endregion
//
//    #region Private Methods
//
//    /// <summary>
//    /// Metodo helper per parsare i dati grezzi di un record in un dizionario.
//    /// </summary>
//    private Dictionary<string, object?> ParseRawRecord(ReadOnlySpan<byte> rawData)
//    {
//        var values = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//        var offset = 0;
//        foreach (var field in _dbfFile.Fields)
//        {
//            var fieldData = rawData.Slice(offset, field.Length);
//            values[field.Name] = _fieldParser.Parse(field, fieldData, _memoFile, _encoding);
//            offset += field.Length;
//        }
//        return values;
//    }
//
//    /// <summary>Lazy parsing: restituisce un record che parsa i campi on-demand.</summary>
//    private DbfLazyRecord ParseRawRecordLazy(byte[] rawData)
//    {
//        return new DbfLazyRecord(rawData, _dbfFile.Fields, _fieldParser, _memoFile, _encoding);
//    }
//
//    private Dictionary<string, object?> ParseRawRecord(byte[] rawData) => ParseRawRecord(rawData.AsSpan());
//
//    /// <summary>
//    /// Legge e parsa i valori di un record specifico tramite il suo numero fisico,
//    /// ignorando lo stato di cancellazione e il flag SET DELETED.
//    /// </summary>
//    private Dictionary<string, object?>? GetRecordValuesByRecNo(int recNo)
//    {
//        if (recNo <= 0 || recNo > _dbfFile.Header.NumberOfRecords) return null;
//
//        // Legge i dati grezzi, ignorando il flag 'isDeleted' restituito.
//        var (_, rawData) = _dbfFile.ReadRawRecord(recNo);
//
//        var values = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//        var offset = 0;
//        foreach (var field in _dbfFile.Fields)
//        {
//            var fieldData = rawData.AsSpan().Slice(offset, field.Length);
//            values[field.Name] = _fieldParser.Parse(field, fieldData, _memoFile, _encoding);
//            offset += field.Length;
//        }
//
//        return values;
//    }
//
//    /// <summary>
//    /// Positions the navigation cursor while evaluating per-record key expressions (e.g. STR(RECNO())).
//    /// ponytail: IndexOn/Reindex/Seek call Evaluator without moving the cursor first.
//    /// </summary>
//    private T EvaluateWithRecordCursor<T>(int recNo, Func<T> evaluate)
//    {
//        var savedRecNo = _currentRecordNumber;
//        var savedPage = _currentLeafPageNum;
//        var savedIndex = _currentIndexInLeaf;
//        _currentRecordNumber = recNo;
//        try
//        {
//            return evaluate();
//        }
//        finally
//        {
//            _currentRecordNumber = savedRecNo;
//            _currentLeafPageNum = savedPage;
//            _currentIndexInLeaf = savedIndex;
//        }
//    }
//
//    /// <summary>
//    /// Metodo helper privato che calcola le chiavi di indice per un dato set di valori di record.
//    /// Questo centralizza la logica di calcolo delle chiavi, evitando la duplicazione del codice
//    /// in vari metodi come AppendFrom, CopyTo, ecc., delegando la normalizzazione della singola
//    /// chiave a <see cref="NormalizeSingleKeyBytes"/>.
//    /// </summary>
//    /// <param name="recordValues">I valori del record per cui calcolare le chiavi.</param>
//    /// <param name="targetEngine">L'istanza del motore (tipicamente quello di destinazione)
//    /// che possiede gli indici e l'evaluator da utilizzare.</param>
//    /// <returns>Un dizionario in sola lettura che mappa ogni handle di indice alla sua chiave calcolata in byte.</returns>
//    /// <exception cref="InvalidOperationException">Se l'evaluator del motore di destinazione non è stato inizializzato.</exception>
//    internal IReadOnlyDictionary<IIndexHandle, byte[]> CalculateKeys(
//        IReadOnlyDictionary<string, object?> recordValues, DbfEngine targetEngine)
//    {
//        var keys = new Dictionary<IIndexHandle, byte[]>();
//        if (targetEngine._indexes.Count == 0)
//        {
//            return keys;
//        }
//
//        if (targetEngine.Evaluator == null)
//        {
//            throw new InvalidOperationException("L'evaluator non è stato inizializzato per il motore di destinazione.");
//        }
//
//        foreach (var index in targetEngine._indexes.OfType<IWritableIndexHandle>())
//        {
//            var keyExpressionString = index.GetKeyExpressionString();
//            var targetType = new StringType(index.KeyLength);
//
//            var keyObject = targetEngine.Evaluator(keyExpressionString, recordValues, targetType);
//
//            // DELEGATO: Chiama il metodo helper centralizzato per normalizzare la singola chiave.
//            var finalKeyBytes = NormalizeSingleKeyBytes(keyObject, index.KeyLength, targetEngine._encoding);
//
//            if (finalKeyBytes != null)
//            {
//                keys[index] = finalKeyBytes;
//            }
//            else
//            {
//                // Se la chiave è null, restituisce un array di byte vuoto/riempito di spazi.
//                // Questo è il comportamento desiderato per le chiavi nulle in xBase.
//                keys[index] = new byte[index.KeyLength];
//            }
//        }
//
//        return keys;
//    }
//
//    /// <summary>
//    /// Normalizza un valore di chiave generico in un array di byte a lunghezza fissa
//    /// secondo le regole di collazione dell'indice.
//    /// Questo metodo incapsula la logica di conversione e padding.
//    /// </summary>
//    /// <param name="keyValue">L'oggetto che rappresenta il valore della chiave (es. stringa, numero).</param>
//    /// <param name="keyLength">La lunghezza fissa desiderata per la chiave in byte.</param>
//    /// <param name="encoding">La codifica da utilizzare per convertire le stringhe in byte.</param>
//    /// <returns>Un array di byte che rappresenta la chiave normalizzata, o null se il valore è null.</returns>
//    public byte[]? NormalizeSingleKeyBytes(object? keyValue, int keyLength, Encoding encoding)
//    {
//        if (keyValue is null) return null;
//
//        var rawKeyString = keyValue is string s ? s : FormatIndexKeyLiteral(keyValue);
//        var rawKeyBytes = encoding.GetBytes(rawKeyString);
//
//        var finalKeyBytes = new byte[keyLength];
//        // L'insensibilità al maiuscolo è una proprietà intrinseca della collation dell'indice.
//        // Assumiamo true come standard per la compatibilità xBase.
//        CollationHelper.ToIndexKey(rawKeyBytes, finalKeyBytes, true, keyLength);
//
//        return finalKeyBytes;
//    }
//
//    /// <summary>
//    /// Muove il cursore del record avanti o indietro di un numero specificato di posizioni LOGICHE.
//    /// La navigazione ignora automaticamente i record che non soddisfano le condizioni di visibilità
//    /// (flag DELETED e filtro attivo).
//    /// </summary>
//    /// <param name="recordsToSkip">Il numero di record LOGICI da saltare. Un valore positivo muove in avanti,
//    /// uno negativo muove all'indietro.</param>
//    private void SkipInternal(int recordsToSkip = 1)
//    {
//        if (recordsToSkip == 0) return;
//        if ((Eof && recordsToSkip > 0) || (Bof && recordsToSkip < 0)) return;
//
//        if (Eof && recordsToSkip < 0) Eof = false;
//        if (Bof && recordsToSkip > 0) Bof = false;
//
//        var direction = Math.Sign(recordsToSkip);
//        var recordsSkipped = 0;
//
//        // Esegue un ciclo finché non abbiamo saltato il numero richiesto di record VALIDI.
//        while (recordsSkipped < Math.Abs(recordsToSkip))
//        {
//            // Esegue un singolo passo FISICO nella direzione data.
//            Step(direction);
//
//            // Se il passo fisico ci ha portato a BOF o EOF, la navigazione termina immediatamente.
//            if (Bof || Eof)
//            {
//                return;
//            }
//
//            // Dopo essersi spostati, controlla se il nuovo record corrente è valido (non cancellato)
//            // E se soddisfa il filtro.
//            if (IsCurrentRecordValid() && EvaluateFilter())
//            {
//                // Solo se il record è valido, incrementiamo il contatore dei record logici saltati.
//                recordsSkipped++;
//            }
//        }
//    }
//
//    /// <summary>
//    /// Versione interna di GoTop che contiene la logica di ciclo per trovare il primo
//    /// record valido. Da usare solo all'interno di operazioni avvolte in una syscall.
//    /// </summary>
//    private void GoTopInternal()
//    {
//        Bof = false;
//        Eof = false;
//
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//
//        if (_activeOrder != null)
//        {
//            var (recno, pageNum, indexInLeaf) = _activeOrder.GetFirstRecordPosition(context);
//            _currentRecordNumber = recno;
//            _currentLeafPageNum = pageNum;
//            _currentIndexInLeaf = indexInLeaf;
//        }
//        else
//        {
//            _currentRecordNumber = 1;
//        }
//
//        UpdateBofEofStatus();
//
//        // Ciclo per trovare il primo record che soddisfa il filtro
//        if (!IsCurrentRecordValid() || !EvaluateFilter())
//        {
//            SkipInternal();
//        }
//    }
//
//    /// <summary>
//    /// Versione interna di GoBottom che contiene la logica di ciclo per trovare l'ultimo
//    /// record valido. Da usare solo all'interno di operazioni avvolte in una syscall.
//    /// </summary>
//    private void GoBottomInternal()
//    {
//        // 1. Esegui il posizionamento iniziale "stupido".
//        Eof = false;
//        Bof = false;
//
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//
//        if (_activeOrder != null)
//        {
//            var (recno, pageNum, indexInLeaf) = _activeOrder.GetLastRecordPosition(context);
//            _currentRecordNumber = recno;
//            _currentLeafPageNum = pageNum;
//            _currentIndexInLeaf = indexInLeaf;
//        }
//        else
//        {
//            _currentRecordNumber = (int)_dbfFile.Header.NumberOfRecords;
//        }
//
//        UpdateBofEofStatus();
//
//        // 2. Esegui il ciclo di ricerca all'indietro se il record corrente non è valido.
//        if (!Eof && (!IsCurrentRecordValid() || !EvaluateFilter()))
//        {
//            SkipInternal(-1);
//        }
//    }
//
//    /// <summary>
//    /// Rilascia tutti i lock (di lettura e scrittura, su record e su chiavi/intervalli)
//    /// acquisiti da un contesto di transazione specifico.
//    /// </summary>
//    /// <param name="context">Il contesto della transazione che contiene i set delle risorse bloccate.</param>
//    /// <remarks>
//    /// Questa è un'operazione di cleanup critica, invocata sistematicamente dai metodi `Commit` e `Rollback`
//    /// per garantire che, indipendentemente dall'esito della transazione, tutte le risorse bloccate vengano
//    /// restituite al sistema in modo affidabile, prevenendo lock orfani.
//    /// </remarks>
//    private void ReleaseTransactionalLocks(TransactionContext context)
//    {
//        // Rilascia lock su record (per Repeatable Read)
//        foreach (var recNo in context.ReadLockedRecNos)
//        {
//            _txHandle.RecordLockManager.ReleaseReadLock(LockManagerBase.GetRecordLockKey(recNo));
//        }
//
//        foreach (var recNo in context.WriteLockedRecNos)
//        {
//            _txHandle.RecordLockManager.ReleaseWriteLock(LockManagerBase.GetRecordLockKey(recNo));
//        }
//
//        // Rilascia lock su chiavi/intervalli (per Serializable)
//        foreach (var lockKey in context.ReadLockedKeys)
//        {
//            _txHandle.RecordLockManager.ReleaseReadLock(lockKey);
//        }
//
//        foreach (var lockKey in context.WriteLockedKeys)
//        {
//            _txHandle.RecordLockManager.ReleaseWriteLock(lockKey);
//        }
//
//        // Pulisce i set nel contesto
//        context.ReadLockedRecNos.Clear();
//        context.WriteLockedRecNos.Clear();
//        context.ReadLockedKeys.Clear();
//        context.WriteLockedKeys.Clear();
//    }
//
//    /// <summary>
//    /// Metodo helper interno per leggere e parsare un record specifico, applicando la logica di locking
//    /// transazionale e di visibilità MVCC. Acquisisce un lock di lettura sul record se la transazione
//    /// ha un livello di isolamento `Repeatable Read` o superiore.
//    /// </summary>
//    /// <param name="recNo">Il numero del record (1-based) da leggere.</param>
//    /// <returns>
//    /// Un dizionario con i valori del record, o `null` se nessuna versione è visibile.
//    /// </returns>
//    /// <exception cref="LockTimeoutException">
//    /// Lanciata se, in una transazione `RepeatableRead` o superiore, non è possibile acquisire un lock di lettura
//    /// sul record. L'intera transazione viene automaticamente annullata.
//    /// </exception>
//    private Dictionary<string, object?>? GetAndLockRecordValues(int recNo)
//    {
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//
//        if (context is { IsolationLevel: >= IsolationLevel.RepeatableRead })
//        {
//            if (!context.ReadLockedRecNos.Contains(recNo) && !context.WriteLockedRecNos.Contains(recNo))
//            {
//                try
//                {
//                    var lockKey = LockManagerBase.GetRecordLockKey(recNo);
//                    _txHandle.RecordLockManager.AcquireReadLock(lockKey, 5000, $"record {recNo}");
//                    context.ReadLockedRecNos.Add(recNo);
//                }
//                catch (DeadlockException)
//                {
//                    Rollback();
//                    throw;
//                }
//                catch (LockTimeoutException)
//                {
//                    Rollback();
//                    throw;
//                }
//            }
//        }
//
//        if (_versioningManager == null)
//        {
//            var recordLength = _dbfFile.Header.RecordLength;
//            var buffer = ArrayPool<byte>.Shared.Rent(recordLength);
//            try
//            {
//                _dbfFile.ReadRawRecordInto(recNo, buffer.AsSpan(0, recordLength), out var isDeleted);
//                if (isDeleted && !_deletedVisible)
//                {
//                    _lastReadRecordVersion = 0;
//                    return null;
//                }
//
//                _lastReadRecordVersion = 0;
//                // buffer[0] è il flag di cancellazione: DbfLazyRecord calcola gli offset dei campi
//                // assumendo che l'array passato inizi già al primo campo (offset 0 == primo byte
//                // dati), quindi va scartato. Passandolo per intero ogni campo leggeva 1 byte in
//                // anticipo rispetto al reale, con l'ultimo byte di ciascun campo che "sconfinava"
//                // nel campo successivo (es. Valore="12" letto come "1", "2" finiva in testa al
//                // campo Descrizione successivo) — scoperto testando CONVERT DBF TO ISAM su
//                // DbIsamFullTests.bas sezione 11, dove DBF GET restituiva campi vuoti/corrotti.
//                return ParseRawRecordLazy(buffer.AsSpan(1, recordLength - 1).ToArray()).Materialize();
//            }
//            finally
//            {
//                ArrayPool<byte>.Shared.Return(buffer);
//            }
//        }
//
//        long currentRecNoToRead = recNo;
//        var currentTxId = _txHandle.GetCurrentTransactionId();
//        var recordLengthV = _dbfFile.Header.RecordLength;
//        var readBuffer = ArrayPool<byte>.Shared.Rent(recordLengthV);
//        try
//        {
//            while (currentRecNoToRead > 0)
//            {
//                var metadata = _versioningManager.GetMetadata((int)currentRecNoToRead);
//                if (_txHandle.IsVersionVisible(metadata.TransactionId, currentTxId))
//                {
//                    _dbfFile.ReadRawRecordInto(
//                        (int)currentRecNoToRead,
//                        readBuffer.AsSpan(0, recordLengthV),
//                        out var isDeleted);
//                    if (isDeleted && !_deletedVisible)
//                    {
//                        _lastReadRecordVersion = 0;
//                        return null;
//                    }
//
//                    _lastReadRecordVersion = metadata.Version;
//                    // Stesso scarto del flag di cancellazione del ramo senza transazione sopra.
//                    return ParseRawRecordLazy(readBuffer.AsSpan(1, recordLengthV - 1).ToArray()).Materialize();
//                }
//
//                currentRecNoToRead = metadata.PreviousVersionRecNo;
//            }
//        }
//        finally
//        {
//            ArrayPool<byte>.Shared.Return(readBuffer);
//        }
//
//        _lastReadRecordVersion = 0;
//        return null;
//    }
//
//    /// <summary>
//    /// Formatta un singolo valore per l'inserimento in un campo CSV, gestendo i caratteri speciali.
//    /// </summary>
//    private static string FormatCsvField(object? value)
//    {
//        if (value == null)
//        {
//            return "";
//        }
//
//        var stringValue = value.ToString() ?? "";
//        if (!stringValue.Contains(',') && !stringValue.Contains('"') && !stringValue.Contains('\r') &&
//            !stringValue.Contains('\n'))
//        {
//            return stringValue;
//        }
//
//        return $"\"{stringValue.Replace("\"", "\"\"")}\"";
//    }
//
//    /// <summary>
//    /// Scrive i valori di un record, forniti come dizionario, in un buffer di byte,
//    /// formattandoli secondo la struttura del file DBF. Itera su ogni campo definito
//    /// nel file, recupera il valore corrispondente dal dizionario e lo serializza
//    /// nella posizione corretta del buffer.
//    /// </summary>
//    /// <param name="values">Il dizionario contenente i valori da scrivere, dove la chiave è il nome del campo.</param>
//    /// <param name="buffer">Il buffer di destinazione in cui scrivere i dati formattati. La sua dimensione deve
//    /// corrispondere alla lunghezza del record (escluso il flag di cancellazione).</param>
//    private void WriteValuesToBuffer(Dictionary<string, object?> values, Span<byte> buffer)
//    {
//        var offset = 0;
//        foreach (var field in _dbfFile.Fields)
//        {
//            var value = LookupFieldValue(values, field.Name);
//            DbfFieldWriter.Write(buffer.Slice(offset, field.Length), field, value, _encoding, _memoFile);
//            offset += field.Length;
//        }
//    }
//
//    /// <summary>
//    /// Cerca il valore per un campo DBF in <paramref name="values"/> (tipicamente popolato da
//    /// REPLACE con i nomi campo BASIC così come dichiarati nel TYPE, non troncati). I nomi campo
//    /// DBF su disco (<paramref name="dbfFieldName"/>) sono invece limitati a 10 caratteri: un
//    /// lookup esatto case-sensitive fallisce silenziosamente per qualunque campo con nome più
//    /// lungo (es. "Descrizione" dichiarato nel TYPE vs "Descrizion" su disco), lasciando il
//    /// valore a null e scrivendo un campo vuoto/corrotto senza alcun errore visibile (bug
//    /// scoperto testando CONVERT DBF TO ISAM su DbIsamFullTests.bas sezione 11 — "Verifica dati
//    /// convertiti" restituiva sempre vuoto). Prova prima il match esatto (percorso comune, veloce),
//    /// poi confronta troncando ogni chiave a 10 caratteri, case-insensitive — stessa regola già
//    /// usata da DbfSchemaManager.IsHeaderCompatibleWithSchema per la validazione schema/file.
//    /// </summary>
//    private static object? LookupFieldValue(Dictionary<string, object?> values, string dbfFieldName)
//    {
//        if (values.TryGetValue(dbfFieldName, out var exact))
//        {
//            return exact;
//        }
//
//        foreach (var (key, value) in values)
//        {
//            var truncatedKey = key.Length > 10 ? key[..10] : key;
//            if (string.Equals(truncatedKey, dbfFieldName, StringComparison.OrdinalIgnoreCase))
//            {
//                return value;
//            }
//        }
//
//        return null;
//    }
//
//    /// <summary>
//    /// Aggiorna lo stato dei flag `Bof` (Beginning of File) e `Eof` (End of File)
//    /// basandosi sul valore attuale del puntatore del record (`_currentRecordNumber`).
//    /// Se il puntatore è oltre il numero totale di record, imposta `Eof` a true.
//    /// Se è minore di 1, imposta `Bof` a true. Altrimenti, li imposta entrambi a false.
//    /// Normalizza anche il valore di `_currentRecordNumber` ai limiti appropriati (0 per Bof, TotalRecords+1 per Eof).
//    /// </summary>
//    private void UpdateBofEofStatus()
//    {
//        if (_currentRecordNumber > _dbfFile.Header.NumberOfRecords)
//        {
//            Eof = true;
//            _currentRecordNumber = (int)_dbfFile.Header.NumberOfRecords + 1;
//        }
//        else if (_currentRecordNumber < 1)
//        {
//            Bof = true;
//            _currentRecordNumber = 0;
//        }
//        else
//        {
//            Eof = false;
//            Bof = false;
//        }
//    }
//
//    /// <summary>
//    /// Imposta se i record marcati come cancellati devono essere visibili o ignorati
//    /// durante le operazioni di navigazione.
//    /// Dopo aver modificato questa impostazione, il cursore viene riposizionato all'inizio del file.
//    /// </summary>
//    /// <param name="on">True per ignorare i record cancellati (comportamento di SET DELETED ON),
//    /// False per renderli visibili (comportamento di SET DELETED OFF).</param>
//    public void SetDeleted(bool on)
//    {
//        // La nostra logica interna è basata sulla visibilità, che è l'inverso del comando.
//        // SET DELETED ON (on=true) significa che i record cancellati NON sono visibili (_deletedVisible=false).
//        _deletedVisible = !on;
//
//        // Dopo aver cambiato la regola di visibilità, la posizione corrente del cursore
//        // potrebbe puntare a un record che ora è nascosto.
//        // Lo standard xBase prevede di riposizionare il cursore all'inizio del file
//        // per garantire uno stato coerente per la prossima operazione di navigazione.
//        GoTopInternal();
//    }
//
//    /// <summary>
//    /// Imposta o reimposta l'indice attivo (di controllo) per la work area.
//    /// Tutte le operazioni di navigazione sequenziale (GO, SKIP) e di ricerca (SEEK) useranno questo indice.
//    /// Dopo aver cambiato l'ordine, il cursore viene riposizionato all'inizio del file.
//    /// </summary>
//    /// <param name="indexName">Il nome dell'indice da attivare (corrisponde al nome del file senza estensione).
//    /// Se la stringa è vuota o null, nessun indice viene attivato (ordine fisico).</param>
//    /// <exception cref="FileNotFoundException">Se il nome dell'indice specificato non corrisponde a nessun indice aperto.</exception>
//    public void SetActiveOrder(string indexName)
//    {
//        if (string.IsNullOrEmpty(indexName))
//        {
//            _activeOrder = null;
//        }
//        else
//        {
//            _activeOrder =
//                _indexes.FirstOrDefault(idx => idx.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase));
//            if (_activeOrder == null)
//            {
//                throw new FileNotFoundException($"Indice '{indexName}' non trovato o non aperto con USE.");
//            }
//        }
//
//        // Poiché l'ordine è cambiato, la posizione esatta del cursore non è più valida.
//        _currentLeafPageNum = 0;
//        _currentIndexInLeaf = 0;
//
//        // Riposiziona il cursore all'inizio del nuovo ordine (fisico o di indice).
//        GoTopInternal();
//    }
//
//    /// <summary>
//    /// Imposta l'indice attivo (di controllo) per la work area tramite il suo numero ordinale.
//    /// Tutte le operazioni di navigazione sequenziale e di ricerca useranno questo indice.
//    /// Dopo aver cambiato l'ordine, il cursore viene riposizionato all'inizio del file.
//    /// </summary>
//    /// <param name="indexNumber">
//    /// Il numero ordinale (1-based) dell'indice da attivare, basato sull'ordine in cui gli indici
//    /// sono stati aperti con il comando USE. Un valore di 0 disattiva l'ordine di indice,
//    /// ripristinando la navigazione in ordine fisico (naturale).
//    /// </param>
//    /// <exception cref="IndexOutOfRangeException">
//    /// Se il numero dell'indice specificato è maggiore del numero di indici aperti.
//    /// </exception>
//    public void SetActiveOrder(int indexNumber)
//    {
//        if (indexNumber == 0)
//        {
//            _activeOrder = null;
//        }
//        else
//        {
//            // Converte l'indice 1-based in 0-based per l'accesso alla lista.
//            var index = indexNumber - 1;
//            if (index < 0 || index >= _indexes.Count)
//            {
//                throw new IndexOutOfRangeException(
//                    $"Numero di indice non valido: {indexNumber}. Sono aperti {_indexes.Count} indici.");
//            }
//
//            _activeOrder = _indexes[index];
//        }
//
//        // Poiché l'ordine è cambiato, la posizione esatta del cursore non è più valida.
//        _currentLeafPageNum = 0;
//        _currentIndexInLeaf = 0;
//
//        // Riposiziona il cursore all'inizio del nuovo ordine (fisico o di indice).
//        GoTop();
//    }
//
//    /// <summary>
//    /// Cerca un record utilizzando l'indice attivo e posiziona il cursore di conseguenza,
//    /// rispettando l'impostazione di SET EXACT e applicando il locking per il livello `Serializable`.
//    /// Questo metodo incapsula completamente la logica di ricerca e validazione.
//    /// </summary>
//    /// <param name="keyValue">Il valore della chiave da cercare (es. stringa, numero). Verrà normalizzato e confrontato internamente.</param>
//    public void Seek(object? keyValue)
//    {
//        Found = false; // Inizia sempre assumendo che la ricerca fallirà.
//        if (_activeOrder is null)
//        {
//            GoBottomInternal();
//            SkipInternal();
//            return;
//        }
//
//        // 1. Normalizza la chiave di ricerca per la ricerca B-Tree.
//        var keyBytes = NormalizeKeyValue(keyValue, _activeOrder);
//        if (keyBytes == null)
//        {
//            GoBottomInternal();
//            SkipInternal();
//            return;
//        }
//
//        // 2. Esegui una "soft seek" per trovare il primo record candidato (chiave >= chiave cercata),
//        // recuperando anche i byte REALI della chiave letti dall'indice (non ricalcolati).
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//        var (recNo, actualKeyBytes) = _activeOrder.SeekWithKeyBytes(keyBytes, true, context);
//        if (recNo <= 0)
//        {
//            // Nessun candidato trovato, vai a EOF.
//            GoBottomInternal();
//            SkipInternal();
//            return;
//        }
//
//        // 3. Un candidato è stato trovato. Ora dobbiamo validarlo secondo SET EXACT.
//        byte[]? foundKeyBytes;
//        if (actualKeyBytes != null)
//        {
//            // Percorso corretto: confronta contro la chiave REALE letta dall'indice (nessuna
//            // rivalutazione dell'espressione — Bug DbfEngine-Seek-Chiave-Formato, Categoria 6:
//            // l'Evaluator wired dai backend VM/IL non sa valutare espressioni Clipper/xBase
//            // arbitrarie come STR(RECNO()), solo nomi di campo diretti o il sentinel "__recno__").
//            foundKeyBytes = actualKeyBytes;
//        }
//        else
//        {
//            // Fallback legacy per implementazioni di IIndexHandle che non espongono ancora
//            // SeekWithKeyBytes: ricalcola la chiave rivalutando l'espressione sul record trovato.
//            var valuesOfFoundRecord = GetRecordValuesByRecNo(recNo);
//            if (valuesOfFoundRecord == null) // Record illeggibile, ricerca fallita.
//            {
//                GoBottomInternal();
//                SkipInternal();
//                return;
//            }
//
//            var keyExpressionString = _activeOrder.GetKeyExpressionString();
//            var targetType = new StringType(_activeOrder.KeyLength);
//            var foundKeyObject = EvaluateWithRecordCursor(recNo,
//                () => Evaluator!(keyExpressionString, valuesOfFoundRecord, targetType));
//            foundKeyBytes = NormalizeSingleKeyBytes(foundKeyObject, _activeOrder.KeyLength, _encoding);
//        }
//
//        bool isMatch;
//        if (_exact)
//        {
//            isMatch = foundKeyBytes != null && foundKeyBytes.AsSpan().SequenceEqual(keyBytes);
//        }
//        else
//        {
//            isMatch = foundKeyBytes != null && KeyBytesHaveSearchPrefix(foundKeyBytes, keyBytes);
//        }
//
//        // 4. Imposta lo stato finale in base al risultato della validazione.
//        if (isMatch)
//        {
//            Found = true;
//            _currentRecordNumber = recNo;
//            UpdateBofEofStatus();
//        }
//        else
//        {
//            // La soft seek ha trovato un record, ma non soddisfa SET EXACT. Vai a EOF.
//            GoBottomInternal();
//            SkipInternal();
//        }
//    }
//
//    /// <summary>
//    /// Ottiene la lunghezza totale in byte del file DBF su disco,
//    /// delegando la chiamata al gestore di file di basso livello.
//    /// </summary>
//    /// <returns>La lunghezza fisica del file.</returns>
//    public long GetFileLength() => _dbfFile.GetFileLength();
//
//    /// <summary>
//    /// Restituisce il numero di record fisico (ordinale) del record corrente.
//    /// </summary>
//    /// <returns>Il numero del record corrente (1-based), o 0 se non posizionato.</returns>
//    public long GetCurrentRecordNumber()
//    {
//        return _currentRecordNumber;
//    }
//
//    /// <summary>
//    /// Positions the cursor on a physical record for offline replication replay.
//    /// </summary>
//    /// <param name="recNo">1-based record number.</param>
//    internal void PositionAtRecord(int recNo)
//    {
//        if (recNo <= 0 || recNo > _dbfFile.Header.NumberOfRecords)
//        {
//            throw new ArgumentOutOfRangeException(nameof(recNo), recNo, "Record number out of range.");
//        }
//
//        _currentRecordNumber = recNo;
//        if (_versioningManager != null)
//        {
//            _lastReadRecordVersion = _versioningManager.GetMetadata(recNo).Version;
//        }
//        else
//        {
//            _lastReadRecordVersion = 0;
//        }
//
//        UpdateBofEofStatus();
//    }
//
//    /// <summary>
//    /// Restituisce il numero totale di record fisici nel file.
//    /// </summary>
//    public long GetTotalRecordCount()
//    {
//        return _dbfFile.Header.NumberOfRecords;
//    }
//
//    /// <summary>
//    /// Crea una copia fisicamente ordinata del file DBF corrente, generando anche un file di schema .gms associato.
//    /// </summary>
//    /// <param name="destinationFilePath">Il percorso del nuovo file DBF ordinato da creare.</param>
//    /// <param name="sortFields">Una lista di campi su cui ordinare, con la direzione specificata.</param>
//    /// <param name="codeSchema">Lo schema del TYPE del codice, necessario per creare il file .gms.</param>
//    public void SortTo(string destinationFilePath, IReadOnlyList<(string FieldName, bool Descending)> sortFields,
//        DbfSchemaInfo codeSchema)
//    {
//        if (!sortFields.Any())
//            throw new InvalidOperationException("Almeno un campo di ordinamento deve essere specificato per SORT.");
//
//        var records = new List<IReadOnlyDictionary<string, object?>>();
//        GoTopInternal();
//        while (!Eof)
//        {
//            var values = GetCurrentRecordValues();
//            if (values != null)
//            {
//                records.Add(values);
//            }
//
//            SkipInternal();
//        }
//
//        records.Sort((a, b) =>
//        {
//            foreach (var (fieldName, descending) in sortFields)
//            {
//                var valA = a.GetValueOrDefault(fieldName) as IComparable;
//                var valB = b.GetValueOrDefault(fieldName) as IComparable;
//                if (valA == null && valB == null) continue;
//                if (valA == null) return -1;
//                if (valB == null) return 1;
//                var comparison = valA.CompareTo(valB);
//                if (comparison != 0)
//                {
//                    return descending ? -comparison : comparison;
//                }
//            }
//
//            return 0;
//        });
//
//        DbfFile.CreateNew(destinationFilePath, _encoding, GetFields());
//        DbfSchemaManager.WriteSchemaInfo(destinationFilePath, codeSchema);
//
//        using var destEngine = new DbfEngine(destinationFilePath, null, true, LockingStrategy.IntraProcess,
//            codeSchema: codeSchema);
//        // Propaga l'evaluator per coerenza
//        destEngine.Evaluator = Evaluator;
//
//        foreach (var record in records)
//        {
//            // REFACTOR: Usa il metodo helper centralizzato per calcolare le chiavi.
//            var keys = CalculateKeys(record, destEngine);
//
//            destEngine.AppendRecord((Dictionary<string, object?>)record, keys);
//        }
//    }
//
//    /// <summary>
//    /// Crea un nuovo file DBF di riepilogo, generando anche un file di schema .gms associato.
//    /// </summary>
//    /// <param name="destinationFilePath">Il percorso del nuovo file DBF di riepilogo da creare.</param>
//    /// <param name="keyExpression">L'espressione di chiave usata per raggruppare i record.</param>
//    /// <param name="aggregates">Campi o espressioni numeriche da sommare per gruppo.</param>
//    /// <param name="codeSchema">Lo schema del TYPE del codice, necessario per creare il file .gms.</param>
//    /// <param name="conditionEvaluator">Il delegato opzionale per filtrare i record da includere nella totalizzazione.</param>
//    public void TotalOn(string destinationFilePath, Expr keyExpression, IReadOnlyList<DbfTotalAggregate> aggregates,
//        DbfSchemaInfo? codeSchema, Func<Dictionary<string, object?>, bool>? conditionEvaluator)
//    {
//        // La comparazione degli AST deve avvenire nell'Executor. Il motore riceve solo la conferma.
//        // Assumiamo che il chiamante abbia già verificato che l'ordine sia corretto.
//        if (_activeOrder is null)
//            throw new InvalidOperationException(
//                $"Il database deve essere indicizzato sull'espressione di chiave '{keyExpression}' prima di eseguire TOTAL.");
//
//        DbfFile.CreateNew(destinationFilePath, _encoding, GetFields());
//        if (codeSchema is not null)
//        {
//            DbfSchemaManager.WriteSchemaInfo(destinationFilePath, codeSchema);
//        }
//
//        using var destEngine = new DbfEngine(destinationFilePath, null, true, LockingStrategy.IntraProcess,
//            codeSchema: codeSchema);
//        destEngine.Evaluator = Evaluator; // Propaga l'evaluator
//
//        GoTopInternal();
//        if (Eof) return;
//
//        Dictionary<string, object?>? summaryRecord = null;
//        object? lastKeyValue = null;
//
//        while (!Eof)
//        {
//            var currentValues = GetCurrentRecordValues();
//            if (currentValues == null)
//            {
//                SkipInternal();
//                continue;
//            }
//
//            if (conditionEvaluator != null && !conditionEvaluator(currentValues))
//            {
//                SkipInternal();
//                continue;
//            }
//
//            // Calcola la chiave usando l'evaluator iniettato.
//            var keyExpressionString = DbfTotalExpressions.ToPlainString(keyExpression);
//            var targetType = new StringType(_activeOrder.KeyLength);
//            var currentKeyValue = Evaluator!(keyExpressionString, currentValues, targetType);
//
//            if (summaryRecord == null || !Equals(currentKeyValue, lastKeyValue))
//            {
//                if (summaryRecord != null)
//                {
//                    // REFACTOR: Usa il metodo helper centralizzato per calcolare le chiavi.
//                    var keys = CalculateKeys(summaryRecord, destEngine);
//                    destEngine.AppendRecord(summaryRecord, keys);
//                }
//
//                summaryRecord = new Dictionary<string, object?>(currentValues, StringComparer.OrdinalIgnoreCase);
//                lastKeyValue = currentKeyValue;
//
//                var keyFieldNames = DbfTotalExpressions.CollectKeyFieldNames(keyExpression);
//                var storageFields = aggregates
//                    .Select(a => a.StorageFieldName)
//                    .ToHashSet(StringComparer.OrdinalIgnoreCase);
//
//                foreach (var field in GetFields())
//                {
//                    if (!keyFieldNames.Contains(field.Name) && !storageFields.Contains(field.Name))
//                    {
//                        summaryRecord[field.Name] = null;
//                    }
//                }
//
//                ApplyAggregates(summaryRecord, currentValues, aggregates, isNewGroup: true);
//            }
//            else
//            {
//                ApplyAggregates(summaryRecord, currentValues, aggregates, isNewGroup: false);
//            }
//
//            SkipInternal();
//        }
//
//        if (summaryRecord != null)
//        {
//            // REFACTOR: Usa il metodo helper centralizzato anche per l'ultimo record.
//            var keys = CalculateKeys(summaryRecord, destEngine);
//            destEngine.AppendRecord(summaryRecord, keys);
//        }
//    }
//
//    private void ApplyAggregates(
//        Dictionary<string, object?> summaryRecord,
//        Dictionary<string, object?> currentValues,
//        IReadOnlyList<DbfTotalAggregate> aggregates,
//        bool isNewGroup)
//    {
//        if (aggregates.Count == 0 || Evaluator is null)
//        {
//            return;
//        }
//
//        foreach (var aggregate in aggregates)
//        {
//            try
//            {
//                var currentAmount = DbfTotalExpressions.EvaluateNumeric(
//                    Evaluator, aggregate.Expression, currentValues);
//                if (isNewGroup)
//                {
//                    summaryRecord[aggregate.StorageFieldName] = currentAmount;
//                }
//                else
//                {
//                    var previous = summaryRecord.TryGetValue(aggregate.StorageFieldName, out var summaryValue)
//                        ? Convert.ToDecimal(summaryValue)
//                        : 0m;
//                    summaryRecord[aggregate.StorageFieldName] = previous + currentAmount;
//                }
//            }
//            catch
//            {
//                /* Ignora valori non numerici */
//            }
//        }
//    }
//
//    /// <summary>
//    /// Crea un nuovo file di indice (.ntx) basato su un'espressione di chiave, lo popola con i dati
//    /// del file DBF corrente e lo aggiunge alla lista degli indici aperti per questa work area.
//    /// L'operazione è atomica e thread-safe. La logica di valutazione dell'espressione di chiave
//    /// per ogni record viene fornita tramite un delegato per mantenere il motore disaccoppiato.
//    /// </summary>
//    /// <param name="newIndexPath">Il percorso del nuovo file di indice da creare. Se esiste, sarà sovrascritto.</param>
//    /// <param name="keyExpression">L'albero di sintassi astratta (AST) dell'espressione di chiave,
//    /// utilizzato per memorizzare la definizione dell'indice nel file .ntx.</param>
//    /// <param name="keyLength">La lunghezza fissa della chiave per il nuovo indice.</param>
//    /// <param name="isUnique">True se il nuovo indice non deve ammettere chiavi duplicate.</param>
//    /// <param name="keyProducer">
//    /// Un delegato (funzione callback) fornito dall'Executor. Questa funzione riceve i valori di un
//    /// record e ha il compito di calcolare e restituire la rappresentazione binaria della chiave
//    /// corrispondente per il nuovo indice. Questo pattern a callback permette al motore di
//    /// rimanere agnostico riguardo alla logica di valutazione (SAFE vs UNSAFE).
//    /// </param>
//    public void IndexOn(string newIndexPath, Expr keyExpression, int keyLength, bool isUnique,
//        Func<Dictionary<string, object?>, byte[]?> keyProducer, string? tagName = null)
//    {
//        var extension = Path.GetExtension(newIndexPath).ToLowerInvariant();
//        var keyExpressionString = DbfTotalExpressions.ToPlainString(keyExpression);
//        var dbfBaseName = Path.GetFileNameWithoutExtension(_dbfFile.FilePath);
//
//        IWritableIndexHandle newIndex = extension switch
//        {
//            ".ndx" => NdxFile.CreateNew(newIndexPath, keyExpression, keyLength, isUnique,
//                _txHandle.RecordLockManager, _lockingStrategy),
//            ".mdx" or ".cdx" => CompoundIndexFactory.CreateOrReplaceTag(
//                newIndexPath,
//                tagName ?? throw new ArgumentException(
//                    "INDEX ON verso file compound richiede TAG nome OF file (.mdx/.cdx).", nameof(tagName)),
//                keyExpressionString,
//                keyLength,
//                isUnique,
//                dbfBaseName,
//                _txHandle.RecordLockManager,
//                _lockingStrategy),
//            _ => ClipperNtxWritable.CreateNew(newIndexPath, keyExpression, keyLength, isUnique,
//                _txHandle.RecordLockManager, _lockingStrategy),
//        };
//
//        // Popola il nuovo indice in modo efficiente usando Rebuild.
//        newIndex.Rebuild(Producer);
//
//        // Sostituisce un eventuale handle con lo stesso nome tag / file monofile.
//        _indexes.RemoveAll(i => i.Name.Equals(newIndex.Name, StringComparison.OrdinalIgnoreCase));
//        _indexes.Add(newIndex);
//        _activeOrder = newIndex;
//        return;
//
//        // Funzione "producer" che legge tutti i record validi dal DBF e calcola le loro chiavi usando il callback.
//        IEnumerable<(ReadOnlyMemory<byte> key, int recno)> Producer()
//        {
//            var records = new List<(ReadOnlyMemory<byte> key, int recno)>();
//
//            // Scansiona tutti i record validi
//            var recordCount = (int)GetTotalRecordCount();
//            for (var i = 1; i <= recordCount; i++)
//            {
//                var (isDeleted, _) = _dbfFile.ReadRawRecord(i);
//                // Rispetta l'impostazione di SET DELETED corrente
//                if (!isDeleted || _deletedVisible)
//                {
//                    var values = GetRecordValuesByRecNo(i);
//                    if (values != null)
//                    {
//                        var key = EvaluateWithRecordCursor(i, () => keyProducer(values));
//                        if (key != null)
//                        {
//                            records.Add((new ReadOnlyMemory<byte>(key), i));
//                        }
//                    }
//                }
//            }
//
//            // Ordina i record in base alla chiave per una costruzione B+Tree più efficiente (bulk load).
//            records.Sort((a, b) => a.key.Span.SequenceCompareTo(b.key.Span));
//            return records;
//        }
//    }
//
//    /// <summary>
//    /// Crea un nuovo file DBF vuoto con la stessa struttura (header e campi) del file corrente.
//    /// Crea anche un file di schema .gms associato per garantire la validazione forte in futuro.
//    /// </summary>
//    /// <param name="destinationFilePath">Il percorso del nuovo file da creare.</param>
//    /// <param name="codeSchema">Lo schema del TYPE del codice, necessario per creare il file .gms.</param>
//    public void CloneStructure(string destinationFilePath, DbfSchemaInfo codeSchema)
//    {
//        _dbfFile.CloneStructure(destinationFilePath);
//        // Scrive il file di schema associato per il nuovo DBF.
//        DbfSchemaManager.WriteSchemaInfo(destinationFilePath, codeSchema);
//    }
//
//    /// <summary>
//    /// Tenta di acquisire un lock di scrittura (esclusivo) non bloccante sul record attualmente puntato dal cursore.
//    /// </summary>
//    /// <returns>
//    /// `true` se il lock viene acquisito immediatamente (cioè, il record non era bloccato da un altro processo).
//    /// `false` se il record è già bloccato o se il cursore non è posizionato su un record valido.
//    /// </returns>
//    /// <remarks>
//    /// Questo metodo fornisce un meccanismo per implementare il locking pessimistico esplicito.
//    /// È l'equivalente funzionale del comando `RLOCK()` in xBase. L'operazione utilizza un timeout di 0 millisecondi
//    /// per garantire che la chiamata non attenda e restituisca immediatamente un risultato.
//    /// </remarks>
//    public bool TryLockCurrentRecord()
//    {
//        if (_currentRecordNumber <= 0 || _currentRecordNumber > _dbfFile.Header.NumberOfRecords)
//        {
//            return false;
//        }
//
//        try
//        {
//            var recNo = _currentRecordNumber;
//            var lockKey = LockManagerBase.GetRecordLockKey(recNo);
//            _txHandle.RecordLockManager.AcquireWriteLock(lockKey, 0, $"record {recNo}");
//            return true;
//        }
//        catch (DeadlockException)
//        {
//            return false;
//        }
//        catch (LockTimeoutException)
//        {
//            return false;
//        }
//    }
//
//    /// <summary>
//    /// Tenta di acquisire un lock di scrittura (esclusivo) non bloccante sull'intero file di database.
//    /// </summary>
//    /// <returns>
//    /// `true` se il lock viene acquisito immediatamente (cioè, il file non era bloccato da un altro processo).
//    /// `false` se il file è già bloccato.
//    /// </returns>
//    /// <remarks>
//    /// Questo metodo fornisce un meccanismo per implementare il locking pessimistico a livello di file.
//    /// È l'equivalente funzionale del comando `FLOCK()` in xBase. L'operazione utilizza un timeout di 0 millisecondi
//    /// per assicurare che la chiamata non attenda e restituisca immediatamente un risultato,
//    /// permettendo al programma chiamante di gestire il fallimento senza bloccarsi.
//    /// </remarks>
//    public bool TryLockFile()
//    {
//        try
//        {
//            var lockKey = LockManagerBase.GetRecordLockKey(-1); // -1 indica il lock a livello di file.
//            _txHandle.RecordLockManager.AcquireWriteLock(lockKey, 0, "file DBF");
//            return true;
//        }
//        catch (DeadlockException)
//        {
//            return false;
//        }
//        catch (LockTimeoutException)
//        {
//            return false;
//        }
//    }
//
//    /// <summary>
//    /// Rilascia in modo pulito tutte le risorse gestite dall'istanza del motore DBF.
//    /// Questo include la chiusura degli handle di file per il file .dbf, il file memo,
//    /// tutti i file di indice aperti e il meccanismo di lock. L'invocazione di questo
//    /// metodo è essenziale per prevenire perdite di risorse e per garantire che i file
//    /// non rimangano bloccati dal processo.
//    /// </summary>
//    public void Dispose()
//    {
//        _replicationLog?.Dispose();
//        _txHandle.Dispose();
//        _memoFile.Dispose();
//        _versioningManager?.Dispose();
//        foreach (var index in _indexes) index.Dispose();
//    }
//
//    /// <summary>
//    /// Normalizza un valore di ricerca (es. una stringa o un numero) in un array di byte
//    /// della lunghezza fissa richiesta dalla chiave dell'indice attivo.
//    /// </summary>
//    /// <param name="keyValue">Il valore da cercare.</param>
//    /// <param name="index">L'handle dell'indice attivo che definisce la lunghezza e le proprietà della chiave.</param>
//    /// <returns>Un array di byte che rappresenta la chiave normalizzata, pronta per la ricerca.</returns>
//    private byte[]? NormalizeKeyValue(object? keyValue, IIndexHandle index)
//    {
//        if (keyValue is null) return null;
//
//        var rawKeyString = FormatIndexKeyLiteral(keyValue);
//        var rawKeyBytes = _encoding.GetBytes(rawKeyString);
//
//        // Normalizza i byte alla lunghezza fissa dell'indice (padding/truncating).
//        var finalKeyBytes = new byte[index.KeyLength];
//        // L'insensibilità al maiuscolo è una proprietà intrinseca della collation dell'indice.
//        // Qui assumiamo true come standard per la compatibilità xBase.
//        CollationHelper.ToIndexKey(rawKeyBytes, finalKeyBytes, true, index.KeyLength);
//
//        return finalKeyBytes;
//    }
//
//    /// <summary>
//    /// Formats a SEEK/INDEX key literal: numeric values use STR$ rules (leading space) like STR(RECNO()).
//    /// </summary>
//    private static string FormatIndexKeyLiteral(object keyValue) =>
//        keyValue switch
//        {
//            string s => s,
//            IConvertible convertible => FormatStrLiteral(convertible),
//            _ => keyValue.ToString() ?? string.Empty,
//        };
//
//    /// <summary>STR$ compatibility: positive numbers get a leading space.</summary>
//    private static string FormatStrLiteral(IConvertible value)
//    {
//        var decValue = value.ToDecimal(CultureInfo.InvariantCulture);
//        var strValue = value.ToString(CultureInfo.InvariantCulture) ?? string.Empty;
//        return decValue >= 0 ? " " + strValue : strValue;
//    }
//
//    /// <summary>SET EXACT OFF: found key matches when its significant bytes start with the search prefix.</summary>
//    private static bool KeyBytesHaveSearchPrefix(ReadOnlySpan<byte> foundKey, ReadOnlySpan<byte> searchKey)
//    {
//        var prefixLen = SignificantKeyByteLength(searchKey);
//        if (prefixLen == 0)
//        {
//            return true;
//        }
//
//        if (SignificantKeyByteLength(foundKey) < prefixLen)
//        {
//            return false;
//        }
//
//        return foundKey[..prefixLen].SequenceEqual(searchKey[..prefixLen]);
//    }
//
//    private static int SignificantKeyByteLength(ReadOnlySpan<byte> key)
//    {
//        var end = key.Length;
//        while (end > 0 && key[end - 1] == (byte)' ')
//        {
//            end--;
//        }
//
//        return end;
//    }
//
//    /// <summary>
//    /// Verifica se il record attualmente puntato dal cursore è "valido" per la visualizzazione,
//    /// ignorando per ora i filtri. Un record è considerato invalido se il cursore è a BOF/EOF
//    /// o se il record è marcato come cancellato e SET DELETED è ON.
//    /// </summary>
//    /// <returns>True se il record è valido per le operazioni, altrimenti False.</returns>
//    private bool IsCurrentRecordValid()
//    {
//        // Se siamo a BOF o EOF, non c'è un record valido.
//        if (Bof || Eof)
//        {
//            return false;
//        }
//
//        // Se i record cancellati non sono visibili, dobbiamo controllare il flag del record corrente.
//        if (!_deletedVisible)
//        {
//            // ReadRawRecord è efficiente perché legge solo l'header del record.
//            var (isDeleted, _) = _dbfFile.ReadRawRecord(_currentRecordNumber);
//            if (isDeleted)
//            {
//                return false; // Il record è cancellato e non deve essere visto.
//            }
//        }
//
//        // Se i controlli precedenti sono superati, il record è valido.
//        return true;
//    }
//
//    /// <summary>
//    /// Esegue la valutazione del filtro attivo sul record corrente.
//    /// </summary>
//    /// <returns>
//    /// True se non c'è nessun filtro attivo o se il record corrente soddisfa la condizione del filtro.
//    /// False se il filtro è attivo e la condizione non è soddisfatta.
//    /// </returns>
//    /// <summary>
//    /// Esegue la valutazione del filtro attivo sul record corrente.
//    /// </summary>
//    private bool EvaluateFilter()
//    {
//        if (_activeFilterEvaluator == null || Bof || Eof)
//        {
//            return true;
//        }
//
//        var values = GetCurrentRecordValues();
//        if (values is null) return true; // Non filtrare se non c'è un record
//
//        return _activeFilterEvaluator(values);
//    }
//
//    /// <summary>
//    /// Esegue un singolo passo di navigazione atomico, muovendo il cursore di una posizione
//    /// in avanti o all'indietro. Non contiene logica di ciclo o di filtro.
//    /// </summary>
//    /// <param name="direction">La direzione del passo (1 per avanti, -1 per indietro).</param>
//    private void Step(int direction)
//    {
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        _threadTransactions.TryGetValue(threadId, out var transactionStack);
//        var context = transactionStack?.Count > 0 ? transactionStack.Peek() : null;
//
//        if (_activeOrder == null)
//        {
//            _currentRecordNumber += direction;
//        }
//        else
//        {
//            var (recno, newPage, newIndex) =
//                _activeOrder.GetNextRecord(_currentLeafPageNum, _currentIndexInLeaf, direction, context);
//            if (recno == 0)
//            {
//                _currentRecordNumber = (direction > 0)
//                    ? (int)_dbfFile.Header.NumberOfRecords + 1
//                    : 0;
//            }
//            else
//            {
//                _currentRecordNumber = recno;
//                _currentLeafPageNum = newPage;
//                _currentIndexInLeaf = newIndex;
//            }
//        }
//
//        UpdateBofEofStatus();
//    }
//
//    #endregion
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <limits.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pthread.h>

/* C#: DbfEngine ctor + memo + IndexFactory + record path (REPLACE/WAL/tx/CopyTo). */

#define GMB_DBF_ENGINE_KEY_MAX 256

struct DbfEngine {
    DbfTxHandle* tx;
    DbfFile* dbfFile;
    MemoFile* memoFile;
    IndexHandle indexes[GMB_DBF_ENGINE_MAX_INDEXES];
    int indexCount;
    int activeOrder; /* -1 = ordine fisico */
    LockingStrategy lockingStrategy;
    int exclusive;
    int deletedVisible; /* C# default false = SET DELETED ON */
    int exact;
    int currentRecordNumber;
    int currentIndexInLeaf;
    int eof;
    int bof;
    int found;
    int txNest;
    char filterExpr[512];
    VersioningManager* ver;
    int64_t lastReadRecordVersion;
    DbfReplicationLog* repl;
    char pendingRepl[32][768];
    int pendingReplN;
    /* Generazione PACK (parità C# DbfEngine._packGeneration): incrementata a ogni PACK con
     * replicazione attiva e scritta come riga `PACK\t<generation>` nel log. */
    int64_t packGeneration;
};

static int eng_ext_is(const char* path, const char* want)
{
    const char* slash;
    const char* base;
    const char* dot;
    char buf[8];
    size_t i, n;
    if (!path || !want) return 0;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    if (!dot) return 0;
    n = strlen(dot);
    if (n == 0 || n >= sizeof(buf)) return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)dot[i];
        buf[i] = (char)((c >= 'A' && c <= 'Z') ? (c + 32) : c);
    }
    buf[n] = '\0';
    return strcmp(buf, want) == 0;
}

static void eng_stem(const char* path, char* out, int cap)
{
    const char* slash;
    const char* base;
    const char* dot;
    size_t n;
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!path) return;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    n = dot ? (size_t)(dot - base) : strlen(base);
    if ((int)n >= cap) n = (size_t)(cap - 1);
    memcpy(out, base, n);
    out[n] = 0;
}

static void DbfEngine_UpdateBofEofStatus(DbfEngine* self)
{
    uint32_t n;
    if (!self || !self->dbfFile) return;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if ((uint32_t)self->currentRecordNumber > n) {
        self->eof = 1;
        self->currentRecordNumber = (int)n + 1;
    } else if (self->currentRecordNumber < 1) {
        self->bof = 1;
        self->currentRecordNumber = 0;
    } else {
        self->eof = 0;
        self->bof = 0;
    }
}

void DbfEngine_GoTop(DbfEngine* self)
{
    int rec;
    if (!self) return;
    self->bof = 0;
    self->eof = 0;
    self->currentIndexInLeaf = 0;
    if (self->activeOrder >= 0 && self->activeOrder < self->indexCount) {
        rec = IndexHandle_GetFirstRecordNumber(&self->indexes[self->activeOrder]);
        self->currentRecordNumber = rec;
    } else {
        self->currentRecordNumber = 1;
    }
    DbfEngine_UpdateBofEofStatus(self);
}

void DbfEngine_GoBottom(DbfEngine* self)
{
    int rec, n;
    if (!self) return;
    self->bof = 0;
    self->eof = 0;
    if (self->activeOrder >= 0 && self->activeOrder < self->indexCount) {
        rec = IndexHandle_GetLastRecordNumber(&self->indexes[self->activeOrder]);
        n = IndexHandle_GetEntryCount(&self->indexes[self->activeOrder]);
        self->currentIndexInLeaf = n > 0 ? n - 1 : 0;
        self->currentRecordNumber = rec;
    } else {
        self->currentRecordNumber = (int)DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
        self->currentIndexInLeaf = 0;
    }
    DbfEngine_UpdateBofEofStatus(self);
}

static void DbfEngine_Step(DbfEngine* self, int direction)
{
    IndexHandle* h;
    int n, rec;
    uint32_t total;
    if (!self || direction == 0) return;
    total = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (self->activeOrder >= 0 && self->activeOrder < self->indexCount) {
        h = &self->indexes[self->activeOrder];
        n = IndexHandle_GetEntryCount(h);
        if (n > 0) {
            self->currentIndexInLeaf += direction;
            if (self->currentIndexInLeaf < 0) {
                self->currentRecordNumber = 0;
            } else if (self->currentIndexInLeaf >= n) {
                self->currentRecordNumber = (int)total + 1;
            } else {
                rec = IndexHandle_GetRecnoAt(h, self->currentIndexInLeaf);
                self->currentRecordNumber = rec;
            }
            DbfEngine_UpdateBofEofStatus(self);
            return;
        }
    }
    self->currentRecordNumber += direction;
    DbfEngine_UpdateBofEofStatus(self);
}

void DbfEngine_Skip(DbfEngine* self, int recordsToSkip)
{
    /* Navigazione LOGICA come il motore C# (DbfEngine.Skip → SkipInternal): salta i record
     * non visibili (SET DELETED ON) o esclusi dal filtro. Prima usava Step fisico e con un
     * filtro attivo `DO WHILE NOT EOF()` scorreva anche i record filtrati (FOR del TOTAL
     * ignorato, liste filtrate sbagliate su VM/C/C99Native). */
    DbfEngine_SkipInternal(self, recordsToSkip);
}

int DbfEngine_IsCurrentRecordValid(const DbfEngine* self)
{
    int isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;
    if (!self || self->bof || self->eof) return 0;
    if (self->deletedVisible) return 1;
    if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &raw, &rawLen) != GMB_DB_OK)
        return 0;
    free(raw);
    return isDeleted ? 0 : 1;
}

void DbfEngine_SkipInternal(DbfEngine* self, int recordsToSkip)
{
    int dir, absn, skipped;
    if (!self || recordsToSkip == 0) return;
    if ((self->eof && recordsToSkip > 0) || (self->bof && recordsToSkip < 0)) return;
    if (self->eof && recordsToSkip < 0) self->eof = 0;
    if (self->bof && recordsToSkip > 0) self->bof = 0;
    dir = recordsToSkip > 0 ? 1 : -1;
    absn = recordsToSkip > 0 ? recordsToSkip : -recordsToSkip;
    skipped = 0;
    while (skipped < absn) {
        DbfEngine_Step(self, dir);
        if (self->bof || self->eof) return;
        if (DbfEngine_IsCurrentRecordValid(self) && DbfEngine_EvaluateFilter(self)) skipped++;
    }
}

void DbfEngine_GoTopInternal(DbfEngine* self)
{
    DbfEngine_GoTop(self);
    if (!DbfEngine_IsCurrentRecordValid(self) || !DbfEngine_EvaluateFilter(self))
        DbfEngine_SkipInternal(self, 1);
}

void DbfEngine_GoBottomInternal(DbfEngine* self)
{
    DbfEngine_GoBottom(self);
    if (!self->eof && (!DbfEngine_IsCurrentRecordValid(self) || !DbfEngine_EvaluateFilter(self)))
        DbfEngine_SkipInternal(self, -1);
}

static int eng_add_handles(DbfEngine* self, IndexHandle* add, int n)
{
    int i;
    if (n <= 0) return GMB_DB_OK;
    if (self->indexCount + n > GMB_DBF_ENGINE_MAX_INDEXES) {
        for (i = 0; i < n; i++) IndexHandle_Dispose(&add[i]);
        return GMB_DB_ERR;
    }
    for (i = 0; i < n; i++) self->indexes[self->indexCount++] = add[i];
    return GMB_DB_OK;
}

DbfEngine* DbfEngine_New(const char* filePath, const char* const* indexPaths, int indexPathCount,
                         int exclusive, LockingStrategy lockingStrategy)
{
    DbfEngine* self;
    const DbfHeader* hdr;
    int i;

    if (!filePath) return NULL;
    self = (DbfEngine*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    self->lockingStrategy = lockingStrategy;
    self->exclusive = exclusive;
    self->activeOrder = -1;
    /* Clipper: SET DELETED OFF di default → i record cancellati SONO visibili a SKIP/EOF.
     * SET DELETED ON nasconde (DbfEngine_SetDeleted → deletedVisible=!on). */
    self->deletedVisible = 1;
    self->tx = DbfTxHandle_New(filePath, exclusive, lockingStrategy);
    if (!self->tx) {
        free(self);
        return NULL;
    }
    self->dbfFile = DbfTxHandle_GetDbfFile(self->tx);
    if (!self->dbfFile) {
        DbfTxHandle_Dispose(self->tx);
        free(self);
        return NULL;
    }
    hdr = DbfFile_get_Header(self->dbfFile);
    self->memoFile = DbfMemoFactory_Create(filePath, (DbfVersion)hdr->VersionByte, exclusive);
    if (!self->memoFile) {
        DbfTxHandle_Dispose(self->tx);
        free(self);
        return NULL;
    }

    /* C#: lockManager sul tx handle è sempre non-null → writable=1 */
    if (indexPaths) {
        for (i = 0; i < indexPathCount; i++) {
            IndexHandle tmp[GMB_DBF_ENGINE_MAX_INDEXES];
            int n = 0;
            if (!indexPaths[i]) continue;
            if (IndexFactory_CreateHandles(indexPaths[i], 1, tmp, GMB_DBF_ENGINE_MAX_INDEXES, &n) != GMB_DB_OK)
                continue;
            if (eng_add_handles(self, tmp, n) != GMB_DB_OK) {
                DbfEngine_Dispose(self);
                return NULL;
            }
        }
    }
    self->activeOrder = self->indexCount > 0 ? 0 : -1;
    DbfEngine_GoTop(self);
    return self;
}

void DbfEngine_Dispose(DbfEngine* self)
{
    int i;
    if (!self) return;
    if (self->memoFile) MemoFile_Dispose(self->memoFile);
    if (self->repl) DbfReplicationLog_Dispose(self->repl);
    if (self->ver) VersioningManager_Dispose(self->ver);
    if (self->tx) DbfTxHandle_Dispose(self->tx);
    self->dbfFile = NULL;
    for (i = 0; i < self->indexCount; i++) IndexHandle_Dispose(&self->indexes[i]);
    free(self);
}

DbfFile* DbfEngine_get_DbfFile(DbfEngine* self) { return self ? self->dbfFile : NULL; }
MemoFile* DbfEngine_get_MemoFile(DbfEngine* self) { return self ? self->memoFile : NULL; }

const DbfHeader* DbfEngine_GetHeader(const DbfEngine* self)
{
    return self && self->dbfFile ? DbfFile_get_Header(self->dbfFile) : NULL;
}

const DbfField* DbfEngine_GetFields(const DbfEngine* self, int* count)
{
    if (!self || !self->dbfFile) {
        if (count) *count = 0;
        return NULL;
    }
    return DbfFile_get_Fields(self->dbfFile, count);
}

int DbfEngine_GetIndexCount(const DbfEngine* self) { return self ? self->indexCount : 0; }

const IndexHandle* DbfEngine_GetIndex(const DbfEngine* self, int i)
{
    if (!self || i < 0 || i >= self->indexCount) return NULL;
    return &self->indexes[i];
}

int DbfEngine_AddIndexPath(DbfEngine* self, const char* indexPath)
{
    IndexHandle tmp[GMB_DBF_ENGINE_MAX_INDEXES];
    int n = 0;
    if (!self || !indexPath) return GMB_DB_ARG;
    if (IndexFactory_CreateHandles(indexPath, 1, tmp, GMB_DBF_ENGINE_MAX_INDEXES, &n) != GMB_DB_OK)
        return GMB_DB_ERR;
    if (n <= 0) return GMB_DB_OK;
    if (eng_add_handles(self, tmp, n) != GMB_DB_OK) return GMB_DB_ERR;
    if (self->activeOrder < 0 && self->indexCount > 0) self->activeOrder = 0;
    DbfEngine_GoTop(self);
    return GMB_DB_OK;
}

int DbfEngine_SetActiveOrderName(DbfEngine* self, const char* indexName)
{
    int i;
    if (!self) return GMB_DB_ARG;
    if (!indexName || indexName[0] == 0) {
        self->activeOrder = -1;
    } else {
        int found = -1;
        for (i = 0; i < self->indexCount; i++) {
            const char* n = IndexHandle_get_Name(&self->indexes[i]);
            if (n && gmb_strcasecmp(n, indexName) == 0) {
                found = i;
                break;
            }
        }
        if (found < 0) return GMB_DB_ERR;
        self->activeOrder = found;
    }
    DbfEngine_GoTop(self);
    return GMB_DB_OK;
}

int DbfEngine_SetActiveOrderNumber(DbfEngine* self, int indexNumber)
{
    if (!self) return GMB_DB_ARG;
    if (indexNumber == 0) {
        self->activeOrder = -1;
    } else {
        int idx = indexNumber - 1;
        if (idx < 0 || idx >= self->indexCount) return GMB_DB_ARG;
        self->activeOrder = idx;
    }
    DbfEngine_GoTop(self);
    return GMB_DB_OK;
}

void DbfEngine_SetDeleted(DbfEngine* self, int on)
{
    if (!self) return;
    self->deletedVisible = !on;
    DbfEngine_GoTopInternal(self);
}

void DbfEngine_SetExact(DbfEngine* self, int on)
{
    if (self) self->exact = on ? 1 : 0;
}

int DbfEngine_GetActiveKeyLength(const DbfEngine* self)
{
    if (!self || self->activeOrder < 0 || self->activeOrder >= self->indexCount) return 0;
    return IndexHandle_get_KeyLength(&self->indexes[self->activeOrder]);
}

int DbfEngine_Seek(DbfEngine* self, const uint8_t* keyBytes, int keyLen)
{
    int rec;
    if (!self) return GMB_DB_ARG;
    self->found = 0;
    if (self->activeOrder < 0 || self->activeOrder >= self->indexCount) {
        uint32_t n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
        self->currentRecordNumber = (int)n + 1;
        self->eof = 1;
        return GMB_DB_OK;
    }
    rec = IndexHandle_Seek(&self->indexes[self->activeOrder], keyBytes, keyLen, self->exact ? 0 : 1);
    if (rec > 0) {
        int n, i;
        self->currentRecordNumber = rec;
        self->found = 1;
        /* Soft seek: la chiave SUCCESSIVA soddisfa il seek ma FOUND() deve restare falso
         * se la chiave cercata non è un PREFISSO della chiave trovata (SET EXACT OFF) o
         * non è identica (ON). Vale per gli indici a campo singolo (espressione = un
         * identificatore); per gli indici a espressione non c'è una chiave testuale unica. */
        {
            const char* kexpr = IndexHandle_GetKeyExpressionString(&self->indexes[self->activeOrder]);
            if (kexpr && kexpr[0] && !strpbrk(kexpr, "+-*/() ")) {
                char cur[256], skey[256];
                if (DbfEngine_GetFieldTextByName(self, kexpr, cur, (int)sizeof(cur)) == GMB_DB_OK) {
                    char* t;
                    int kl = keyLen < (int)sizeof(skey) - 1 ? keyLen : (int)sizeof(skey) - 1;
                    memcpy(skey, keyBytes, (size_t)kl);
                    skey[kl] = 0;
                    t = skey + strlen(skey);
                    while (t > skey && t[-1] == ' ') *--t = 0;
                    t = cur + strlen(cur);
                    while (t > cur && t[-1] == ' ') *--t = 0;
                    if (self->exact) {
                        if (strcasecmp(cur, skey) != 0) self->found = 0;
                    } else if (strncasecmp(cur, skey, strlen(skey)) != 0) {
                        self->found = 0;
                    }
                }
            }
        }
        n = IndexHandle_GetEntryCount(&self->indexes[self->activeOrder]);
        self->currentIndexInLeaf = 0;
        for (i = 0; i < n; i++) {
            if (IndexHandle_GetRecnoAt(&self->indexes[self->activeOrder], i) == rec) {
                self->currentIndexInLeaf = i;
                break;
            }
        }
        DbfEngine_UpdateBofEofStatus(self);
        return GMB_DB_OK;
    }
    {
        uint32_t n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
        self->currentRecordNumber = (int)n + 1;
        self->eof = 1;
        self->found = 0;
    }
    return GMB_DB_OK;
}

int DbfEngine_get_Found(const DbfEngine* self) { return self ? self->found : 0; }
int DbfEngine_get_Eof(const DbfEngine* self) { return self ? self->eof : 0; }
int DbfEngine_get_Bof(const DbfEngine* self) { return self ? self->bof : 0; }
int DbfEngine_GetCurrentRecordNumber(const DbfEngine* self)
{
    return self ? self->currentRecordNumber : 0;
}

uint32_t DbfEngine_GetTotalRecordCount(const DbfEngine* self)
{
    if (!self || !self->dbfFile) return 0;
    return DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
}

static int eng_extract_field_key(DbfEngine* self, int recno, const char* fieldName,
                                 uint8_t* keyOut, int keyCap, int* keyLen)
{
    const DbfField* fields;
    int nFields = 0, i, recLen, isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;
    const DbfHeader* hdr;
    char expr[128];
    char* save = NULL;
    char* tok;
    int written = 0;

    if (keyLen) *keyLen = 0;
    if (!self || !fieldName || !keyOut || keyCap <= 0) return GMB_DB_ARG;
    hdr = DbfFile_get_Header(self->dbfFile);
    recLen = hdr->RecordLength;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    if (DbfFile_ReadRawRecord(self->dbfFile, recno, &isDeleted, &raw, &rawLen) != GMB_DB_OK)
        return GMB_DB_ERR;
    (void)isDeleted;
    (void)recLen;
    memset(keyOut, ' ', (size_t)keyCap);
    snprintf(expr, sizeof(expr), "%s", fieldName);
    tok = strtok_r(expr, "+", &save);
    while (tok) {
        while (*tok == ' ') tok++;
        /* TRIM finale: `Citta + Nome` produce i token "Citta " e " Nome"; senza il trim il
         * primo non matchava il campo (confronto case-insensitive ma non space-insensitive) →
         * l'indice composto veniva costruito con la SOLA ultima colonna (ordine sbagliato). */
        {
            size_t _tl = strlen(tok);
            while (_tl > 0 && (tok[_tl - 1] == ' ' || tok[_tl - 1] == '\t')) tok[--_tl] = 0;
        }
        {
            int off = 0;
            int found = 0;
            for (i = 0; i < nFields; i++) {
                int flen = (int)fields[i].Length;
                if (gmb_strcasecmp(fields[i].Name, tok) == 0) {
                    int copy = flen;
                    if (written + copy > keyCap) copy = keyCap - written;
                    if (copy < 0) copy = 0;
                    if (off + copy > rawLen) copy = rawLen - off;
                    if (copy < 0) copy = 0;
                    if (copy > 0) memcpy(keyOut + written, raw + off, (size_t)copy);
                    written += copy;
                    found = 1;
                    break;
                }
                off += flen;
            }
            if (!found && strchr(fieldName, '+') == NULL) {
                free(raw);
                return GMB_DB_ERR;
            }
        }
        tok = strtok_r(NULL, "+", &save);
    }
    if (keyLen) *keyLen = keyCap;
    free(raw);
    return written > 0 || strchr(fieldName, '+') ? GMB_DB_OK : GMB_DB_ERR;
}

static int eng_default_key_producer(DbfEngine* eng, int recno, uint8_t* keyOut, int keyCap, int* keyLen, void* ctx)
{
    const char* field = ctx ? (const char*)ctx : "";
    return eng_extract_field_key(eng, recno, field, keyOut, keyCap, keyLen);
}

static int eng_replace_index_by_name(DbfEngine* self, const char* name, IndexHandle neu)
{
    int i;
    for (i = 0; i < self->indexCount; i++) {
        const char* n = IndexHandle_get_Name(&self->indexes[i]);
        if (n && name && gmb_strcasecmp(n, name) == 0) {
            IndexHandle_Dispose(&self->indexes[i]);
            self->indexes[i] = neu;
            self->activeOrder = i;
            return GMB_DB_OK;
        }
    }
    if (self->indexCount >= GMB_DBF_ENGINE_MAX_INDEXES) {
        IndexHandle_Dispose(&neu);
        return GMB_DB_ERR;
    }
    self->indexes[self->indexCount] = neu;
    self->activeOrder = self->indexCount;
    self->indexCount++;
    return GMB_DB_OK;
}

static int eng_rebuild_into(DbfEngine* self, IndexHandle* h, int keyLength,
                            DbfEngineKeyProducer producer, void* ctx)
{
    uint32_t count, i;
    uint8_t key[GMB_DBF_ENGINE_KEY_MAX];
    int keyLen = 0;
    int isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;

    if (keyLength <= 0 || keyLength > GMB_DBF_ENGINE_KEY_MAX) return GMB_DB_ARG;
    count = DbfEngine_GetTotalRecordCount(self);
    for (i = 1; i <= count; i++) {
        if (DbfFile_ReadRawRecord(self->dbfFile, (int)i, &isDeleted, &raw, &rawLen) != GMB_DB_OK)
            return GMB_DB_ERR;
        free(raw);
        raw = NULL;
        if (isDeleted && !self->deletedVisible) continue;
        memset(key, ' ', (size_t)keyLength);
        keyLen = keyLength;
        if (producer(self, (int)i, key, keyLength, &keyLen, ctx) != GMB_DB_OK) continue;
        if (keyLen <= 0) continue;
        if (IndexHandle_Insert(h, key, keyLen, (int)i) != GMB_DB_OK) return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

int DbfEngine_IndexOn(DbfEngine* self, const char* newIndexPath, const char* keyExpression,
                      int keyLength, int isUnique, const char* tagName,
                      DbfEngineKeyProducer keyProducer, void* keyProducerCtx)
{
    IndexHandle h;
    char dbfBase[256];
    DbfEngineKeyProducer prod;
    void* ctx;
    const char* expr;

    if (!self || !newIndexPath || keyLength <= 0) return GMB_DB_ARG;
    expr = keyExpression ? keyExpression : "";
    prod = keyProducer ? keyProducer : eng_default_key_producer;
    ctx = keyProducer ? keyProducerCtx : (void*)expr;
    memset(&h, 0, sizeof(h));
    eng_stem(DbfFile_get_FilePath(self->dbfFile), dbfBase, sizeof(dbfBase));

    if (eng_ext_is(newIndexPath, ".ndx")) {
        NdxFile* ndx = NdxFile_CreateNew(newIndexPath, expr, keyLength, isUnique);
        if (!ndx) return GMB_DB_ERR;
        h.Kind = IndexHandleKind_Ndx;
        h.Ndx = ndx;
    } else if (eng_ext_is(newIndexPath, ".mdx")) {
        MdxTagIndex* mdx;
        if (!tagName || tagName[0] == 0) return GMB_DB_ARG;
        mdx = MdxTagIndex_CreateOrReplace(newIndexPath, dbfBase, tagName, expr, keyLength, isUnique);
        if (!mdx) return GMB_DB_ERR;
        h.Kind = IndexHandleKind_MdxTag;
        h.Mdx = mdx;
    } else if (eng_ext_is(newIndexPath, ".cdx")) {
        CdxTagIndex* cdx;
        if (!tagName || tagName[0] == 0) return GMB_DB_ARG;
        cdx = CdxTagIndex_CreateOrReplace(newIndexPath, tagName, expr, keyLength, isUnique, 0);
        if (!cdx) return GMB_DB_ERR;
        h.Kind = IndexHandleKind_CdxTag;
        h.Cdx = cdx;
    } else {
        /* C#: default ClipperNtxWritable, non NtxFile GM */
        ClipperNtxWritable* w = ClipperNtxWritable_CreateNew(newIndexPath, expr, keyLength, isUnique);
        if (!w) return GMB_DB_ERR;
        h.Kind = IndexHandleKind_ClipperNtxWritable;
        h.ClipperW = w;
    }
    snprintf(h.Path, sizeof(h.Path), "%s", newIndexPath);

    if (eng_rebuild_into(self, &h, keyLength, prod, ctx) != GMB_DB_OK) {
        IndexHandle_Dispose(&h);
        return GMB_DB_ERR;
    }
    return eng_replace_index_by_name(self, IndexHandle_get_Name(&h), h);
}

static int eng_key_for_index(DbfEngine* self, int recno, IndexHandle* h, uint8_t* key, int* keyLen)
{
    int cap = IndexHandle_get_KeyLength(h);
    const char* expr = IndexHandle_GetKeyExpressionString(h);
    if (cap <= 0 || cap > GMB_DBF_ENGINE_KEY_MAX) return GMB_DB_ERR;
    *keyLen = cap;
    memset(key, ' ', (size_t)cap);
    return eng_extract_field_key(self, recno, expr ? expr : "", key, cap, keyLen);
}

int DbfEngine_IsCurrentRecordDeleted(DbfEngine* self)
{
    int isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;
    if (!self || self->currentRecordNumber <= 0) return 0;
    if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &raw, &rawLen) != GMB_DB_OK)
        return 0;
    free(raw);
    return isDeleted;
}

int DbfEngine_PositionAtRecord(DbfEngine* self, int recNo)
{
    uint32_t n;
    if (!self) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (recNo <= 0 || (uint32_t)recNo > n) return GMB_DB_ARG;
    self->currentRecordNumber = recNo;
    self->currentIndexInLeaf = 0;
    DbfEngine_UpdateBofEofStatus(self);
    return GMB_DB_OK;
}

static int eng_filter_get(void* ctx, const char* name, char* out, int cap)
{
    return DbfEngine_GetFieldTextByName((DbfEngine*)ctx, name, out, cap);
}

static int64_t eng_ticks(void)
{
    static int64_t t = 1;
    return ++t;
}

static void eng_json_escape(char* dst, int cap, const char* s)
{
    int n = 0;
    if (!dst || cap <= 0) return;
    if (!s) s = "";
    while (*s && n < cap - 1) {
        if (*s == '"' || *s == '\\') {
            if (n + 2 >= cap) break;
            dst[n++] = '\\';
            dst[n++] = *s++;
        } else {
            dst[n++] = *s++;
        }
    }
    dst[n] = 0;
}

static void eng_json_current(DbfEngine* self, char* out, int cap)
{
    int nFields = 0, i, n;
    const DbfField* fields;
    char val[256], esc[512];
    if (!self || !out || cap <= 2) return;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    n = snprintf(out, (size_t)cap, "{");
    for (i = 0; i < nFields && n < cap - 2; i++) {
        if (DbfEngine_GetFieldText(self, i, val, (int)sizeof(val)) != GMB_DB_OK) val[0] = 0;
        eng_json_escape(esc, (int)sizeof(esc), val);
        n += snprintf(out + n, (size_t)(cap - n), "%s\"%s\":\"%s\"", i ? "," : "", fields[i].Name, esc);
    }
    if (n < cap - 1) snprintf(out + n, (size_t)(cap - n), "}");
}

static void eng_repl(DbfEngine* self, ReplicationOp op, int recNo, int withValues)
{
    char json[1024], line[1400];
    if (!self || !self->repl) return;
    json[0] = 0;
    if (withValues) eng_json_current(self, json, (int)sizeof(json));
    ReplicationLogLine_Format(op, recNo, withValues ? json : NULL, line, (int)sizeof(line));
    if (self->txNest > 0) {
        if (self->pendingReplN < 32)
            snprintf(self->pendingRepl[self->pendingReplN++], sizeof(self->pendingRepl[0]), "%s", line);
        return;
    }
    DbfReplicationLog_WriteLine(self->repl, line);
}

int DbfEngine_AppendRecord(DbfEngine* self, const uint8_t* payload, int payloadLen)
{
    int recNo, i;
    uint8_t key[GMB_DBF_ENGINE_KEY_MAX];
    int keyLen = 0;
    if (!self || !payload) return GMB_DB_ARG;
    recNo = DbfTxHandle_AppendRawRecord(self->tx, payload, payloadLen);
    if (recNo < 0) return GMB_DB_ERR;
    self->currentRecordNumber = recNo;
    self->currentIndexInLeaf = 0;
    DbfEngine_UpdateBofEofStatus(self);
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], key, &keyLen) != GMB_DB_OK) continue;
        IndexHandle_Insert(&self->indexes[i], key, keyLen, recNo);
    }
    if (self->ver) {
        VersionMetadata m;
        memset(&m, 0, sizeof(m));
        m.Version = eng_ticks();
        m.TransactionId = self->txNest > 0 ? 1 : 0;
        VersioningManager_AddNewVersionEntry(self->ver, m);
    }
    eng_repl(self, ReplicationOp_Append, recNo, 1);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_DeleteCurrent(DbfEngine* self)
{
    int recNo, i;
    uint8_t key[GMB_DBF_ENGINE_KEY_MAX];
    int keyLen = 0;
    uint32_t n;
    if (!self) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    recNo = self->currentRecordNumber;
    if (recNo <= 0 || (uint32_t)recNo > n) return GMB_DB_OK;
    if (DbfTxHandle_SetDeletedFlag(self->tx, recNo, 1) != GMB_DB_OK) return GMB_DB_ERR;
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], key, &keyLen) != GMB_DB_OK) continue;
        IndexHandle_Delete(&self->indexes[i], key, keyLen, recNo);
    }
    eng_repl(self, ReplicationOp_Delete, recNo, 0);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_RecallCurrent(DbfEngine* self)
{
    int recNo, i;
    uint8_t key[GMB_DBF_ENGINE_KEY_MAX];
    int keyLen = 0;
    uint32_t n;
    if (!self) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    recNo = self->currentRecordNumber;
    if (recNo <= 0 || (uint32_t)recNo > n) return GMB_DB_OK;
    if (DbfTxHandle_SetDeletedFlag(self->tx, recNo, 0) != GMB_DB_OK) return GMB_DB_ERR;
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], key, &keyLen) != GMB_DB_OK) continue;
        IndexHandle_Insert(&self->indexes[i], key, keyLen, recNo);
    }
    eng_repl(self, ReplicationOp_Recall, recNo, 0);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_Reindex(DbfEngine* self)
{
    int i, n;
    char paths[GMB_DBF_ENGINE_MAX_INDEXES][1024];
    char exprs[GMB_DBF_ENGINE_MAX_INDEXES][128];
    char tags[GMB_DBF_ENGINE_MAX_INDEXES][16];
    int keyLens[GMB_DBF_ENGINE_MAX_INDEXES];
    int uniques[GMB_DBF_ENGINE_MAX_INDEXES];
    if (!self) return GMB_DB_ARG;
    n = self->indexCount;
    for (i = 0; i < n; i++) {
        const char* p = IndexHandle_get_Path(&self->indexes[i]);
        const char* e = IndexHandle_GetKeyExpressionString(&self->indexes[i]);
        const char* t = IndexHandle_get_Name(&self->indexes[i]);
        snprintf(paths[i], sizeof(paths[i]), "%s", p ? p : "");
        snprintf(exprs[i], sizeof(exprs[i]), "%s", e ? e : "");
        snprintf(tags[i], sizeof(tags[i]), "%s", t ? t : "");
        keyLens[i] = IndexHandle_get_KeyLength(&self->indexes[i]);
        uniques[i] = IndexHandle_get_Unique(&self->indexes[i]);
    }
    for (i = 0; i < n; i++) {
        const char* tag = NULL;
        if (eng_ext_is(paths[i], ".mdx") || eng_ext_is(paths[i], ".cdx"))
            tag = tags[i];
        if (DbfEngine_IndexOn(self, paths[i], exprs[i], keyLens[i], uniques[i], tag, NULL, NULL) != GMB_DB_OK)
            return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

int DbfEngine_Pack(DbfEngine* self)
{
    char orig[1024], tmp[1100];
    DbfFile* dest;
    uint32_t count, i;
    int isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;
    if (!self) return GMB_DB_ARG;
    snprintf(orig, sizeof(orig), "%s", DbfFile_get_FilePath(self->dbfFile));
    snprintf(tmp, sizeof(tmp), "%s.pack", orig);
    if (DbfFile_CloneStructure(self->dbfFile, tmp) != GMB_DB_OK) return GMB_DB_ERR;
    dest = DbfFile_New(tmp, 1, self->lockingStrategy);
    if (!dest) {
        unlink(tmp);
        return GMB_DB_ERR;
    }
    count = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    for (i = 1; i <= count; i++) {
        if (DbfFile_ReadRawRecord(self->dbfFile, (int)i, &isDeleted, &raw, &rawLen) != GMB_DB_OK) {
            DbfFile_Dispose(dest);
            unlink(tmp);
            return GMB_DB_ERR;
        }
        if (!isDeleted) {
            if (DbfFile_AppendRawRecord(dest, raw, rawLen) < 0) {
                free(raw);
                DbfFile_Dispose(dest);
                unlink(tmp);
                return GMB_DB_ERR;
            }
        }
        free(raw);
        raw = NULL;
    }
    DbfFile_Dispose(dest);
    DbfTxHandle_Dispose(self->tx);
    self->tx = NULL;
    self->dbfFile = NULL;
    if (rename(tmp, orig) != 0) {
        unlink(tmp);
        self->tx = DbfTxHandle_New(orig, self->exclusive, self->lockingStrategy);
        self->dbfFile = self->tx ? DbfTxHandle_GetDbfFile(self->tx) : NULL;
        return GMB_DB_ERR;
    }
    self->tx = DbfTxHandle_New(orig, self->exclusive, self->lockingStrategy);
    self->dbfFile = self->tx ? DbfTxHandle_GetDbfFile(self->tx) : NULL;
    if (!self->dbfFile) return GMB_DB_ERR;
    if (DbfEngine_Reindex(self) != GMB_DB_OK) return GMB_DB_ERR;
    DbfEngine_GoTop(self);
    /* Parità C# DbfEngine.RecordReplicationPack (chiamato in coda a Pack): con la
     * replicazione attiva si scrive la riga `PACK\t<generation>` nel log. Senza, il follower
     * non applicava mai il PACK e i conteggi divergevano dal riferimento (Applied 4 vs 5 su
     * DbfReplication.bas). */
    if (self->repl) {
        char packLine[64];
        ReplicationLogLine_FormatPack(++self->packGeneration, packLine, (int)sizeof(packLine));
        DbfReplicationLog_WriteLine(self->repl, packLine);
    }
    return GMB_DB_OK;
}

int DbfEngine_Zap(DbfEngine* self)
{
    if (!self) return GMB_DB_ARG;
    if (DbfTxHandle_Zap(self->tx) != GMB_DB_OK) return GMB_DB_ERR;
    if (DbfEngine_Reindex(self) != GMB_DB_OK) return GMB_DB_ERR;
    self->currentRecordNumber = 0;
    self->currentIndexInLeaf = 0;
    self->eof = 1;
    self->bof = 1;
    self->found = 0;
    if (self->ver) VersioningManager_Clear(self->ver);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

static int eng_field_offset(const DbfField* fields, int n, int idx)
{
    int i, off = 0;
    for (i = 0; i < idx && i < n; i++) off += fields[i].Length;
    return off;
}

static int eng_find_field(const DbfField* fields, int n, const char* name)
{
    char truncated[11];
    int i, len;
    if (!name) return -1;
    len = (int)strlen(name);
    if (len > 10) len = 10;
    memcpy(truncated, name, (size_t)len);
    truncated[len] = 0;
    for (i = 0; i < n; i++) {
        if (strcmp(fields[i].Name, name) == 0) return i;
    }
    for (i = 0; i < n; i++) {
        if (gmb_strcasecmp(fields[i].Name, truncated) == 0) return i;
    }
    return -1;
}

int DbfEngine_PutCurrentRecord(DbfEngine* self, const uint8_t* payload, int payloadLen)
{
    int recNo, i;
    uint8_t oldKeys[GMB_DBF_ENGINE_MAX_INDEXES][GMB_DBF_ENGINE_KEY_MAX];
    uint8_t newKey[GMB_DBF_ENGINE_KEY_MAX];
    int oldLens[GMB_DBF_ENGINE_MAX_INDEXES];
    int newLen = 0;
    uint32_t n;
    if (!self || !payload) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    recNo = self->currentRecordNumber;
    if (recNo <= 0 || (uint32_t)recNo > n) return GMB_DB_ERR;
    memset(oldLens, 0, sizeof(oldLens));
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], oldKeys[i], &oldLens[i]) != GMB_DB_OK)
            oldLens[i] = 0;
    }
    if (DbfTxHandle_WriteRawRecord(self->tx, recNo, payload, payloadLen) != GMB_DB_OK) return GMB_DB_ERR;
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], newKey, &newLen) != GMB_DB_OK) continue;
        if (oldLens[i] != newLen || memcmp(oldKeys[i], newKey, (size_t)newLen) != 0) {
            if (oldLens[i] > 0) IndexHandle_Delete(&self->indexes[i], oldKeys[i], oldLens[i], recNo);
            IndexHandle_Insert(&self->indexes[i], newKey, newLen, recNo);
        }
    }
    if (self->ver) {
        VersionMetadata cur = VersioningManager_GetMetadata(self->ver, recNo);
        if (self->lastReadRecordVersion != 0 && cur.Version != self->lastReadRecordVersion)
            return GMB_DB_ERR;
        cur.Version = eng_ticks();
        cur.TransactionId = self->txNest > 0 ? 1 : 0;
        VersioningManager_SetMetadata(self->ver, recNo, cur);
        self->lastReadRecordVersion = cur.Version;
    }
    eng_repl(self, ReplicationOp_Put, recNo, 1);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_ReplaceField(DbfEngine* self, int fieldIndex, const char* value)
{
    int nFields = 0, payloadLen, off, isDeleted = 0, rawLen = 0, rc;
    const DbfField* fields;
    uint8_t* payload = NULL;
    uint32_t n;
    if (!self) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (self->currentRecordNumber <= 0 || (uint32_t)self->currentRecordNumber > n) return GMB_DB_ERR;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    if (!fields || fieldIndex < 0 || fieldIndex >= nFields) return GMB_DB_ARG;
    if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &payload, &rawLen) != GMB_DB_OK)
        return GMB_DB_ERR;
    payloadLen = rawLen;
    off = eng_field_offset(fields, nFields, fieldIndex);
    if (off + fields[fieldIndex].Length > payloadLen) {
        free(payload);
        return GMB_DB_ERR;
    }
    DbfFieldWriter_Write(payload + off, fields[fieldIndex].Length, &fields[fieldIndex], value, self->memoFile);
    rc = DbfEngine_PutCurrentRecord(self, payload, payloadLen);
    free(payload);
    return rc;
}

int DbfEngine_ReplaceFieldName(DbfEngine* self, const char* fieldName, const char* value)
{
    int nFields = 0, idx;
    const DbfField* fields;
    if (!self) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    idx = eng_find_field(fields, nFields, fieldName);
    if (idx < 0) return GMB_DB_ARG;
    return DbfEngine_ReplaceField(self, idx, value);
}

int DbfEngine_GetFieldText(DbfEngine* self, int fieldIndex, char* out, int cap)
{
    int nFields = 0, off, isDeleted = 0, rawLen = 0;
    const DbfField* fields;
    uint8_t* payload = NULL;
    int rc;
    uint32_t n;
    if (!self || !out || cap <= 0) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (self->currentRecordNumber <= 0 || (uint32_t)self->currentRecordNumber > n) return GMB_DB_ERR;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    if (!fields || fieldIndex < 0 || fieldIndex >= nFields) return GMB_DB_ARG;
    if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &payload, &rawLen) != GMB_DB_OK)
        return GMB_DB_ERR;
    off = eng_field_offset(fields, nFields, fieldIndex);
    if (off + fields[fieldIndex].Length > rawLen) {
        free(payload);
        return GMB_DB_ERR;
    }
    rc = DbfFieldParser_Parse(&fields[fieldIndex], payload + off, fields[fieldIndex].Length, self->memoFile, out, cap);
    free(payload);
    return rc;
}

int DbfEngine_GetFieldTextByName(DbfEngine* self, const char* fieldName, char* out, int cap)
{
    int nFields = 0, idx;
    const DbfField* fields;
    if (!self) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    idx = eng_find_field(fields, nFields, fieldName);
    if (idx < 0) return GMB_DB_ARG;
    return DbfEngine_GetFieldText(self, idx, out, cap);
}

int DbfEngine_AppendBlankRecord(DbfEngine* self)
{
    int nFields = 0, payloadLen, off = 0, i;
    const DbfField* fields;
    uint8_t* payload;
    int rc;
    if (!self) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    payloadLen = (int)DbfFile_get_Header(self->dbfFile)->RecordLength - 1;
    payload = (uint8_t*)malloc((size_t)payloadLen);
    if (!payload) return GMB_DB_ERR;
    memset(payload, ' ', (size_t)payloadLen);
    for (i = 0; i < nFields; i++) {
        DbfFieldWriter_WriteBlank(payload + off, fields[i].Length, &fields[i], self->memoFile);
        off += fields[i].Length;
    }
    rc = DbfEngine_AppendRecord(self, payload, payloadLen);
    free(payload);
    return rc;
}

int DbfEngine_AppendManyRecords(DbfEngine* self, const uint8_t* payloads, int payloadLen, int count)
{
    int i;
    if (!self || !payloads || count < 0) return GMB_DB_ARG;
    DbfTxHandle_BeginGroupCommit(self->tx);
    for (i = 0; i < count; i++) {
        if (DbfEngine_AppendRecord(self, payloads + (i * payloadLen), payloadLen) != GMB_DB_OK) {
            DbfTxHandle_EndGroupCommit(self->tx);
            return GMB_DB_ERR;
        }
    }
    DbfTxHandle_EndGroupCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_BeginTransaction(DbfEngine* self)
{
    if (!self) return GMB_DB_ARG;
    if (self->txNest == 0) {
        char marker[64];
        if (DbfTxHandle_BeginTransaction(self->tx) == 0) return GMB_DB_ERR;
        self->pendingReplN = 0;
        if (self->repl) {
            ReplicationLogLine_FormatTxnMarker(ReplicationOp_TxnBegin, marker, (int)sizeof(marker));
            DbfReplicationLog_WriteLine(self->repl, marker);
        }
    }
    self->txNest++;
    return GMB_DB_OK;
}

int DbfEngine_Commit(DbfEngine* self)
{
    if (!self || self->txNest <= 0) return GMB_DB_ERR;
    self->txNest--;
    if (self->txNest == 0) {
        int i;
        char marker[64];
        if (DbfTxHandle_CommitTransaction(self->tx) != GMB_DB_OK) return GMB_DB_ERR;
        for (i = 0; i < self->pendingReplN; i++)
            DbfReplicationLog_WriteLine(self->repl, self->pendingRepl[i]);
        self->pendingReplN = 0;
        if (self->repl) {
            ReplicationLogLine_FormatTxnMarker(ReplicationOp_TxnCommit, marker, (int)sizeof(marker));
            DbfReplicationLog_WriteLine(self->repl, marker);
        }
        DbfTxHandle_FinalizeCommit(self->tx);
    }
    return GMB_DB_OK;
}

int DbfEngine_Rollback(DbfEngine* self)
{
    if (!self || self->txNest <= 0) return GMB_DB_ERR;
    self->txNest = 0;
    self->pendingReplN = 0;
    if (DbfTxHandle_RollbackTransaction(self->tx) != GMB_DB_OK) return GMB_DB_ERR;
    return DbfEngine_Reindex(self);
}

void DbfEngine_BeginBulkLoad(DbfEngine* self)
{
    if (self) DbfTxHandle_BeginBulkLoad(self->tx);
}

void DbfEngine_EndBulkLoad(DbfEngine* self)
{
    if (self) DbfTxHandle_EndBulkLoad(self->tx);
}

void DbfEngine_BeginGroupCommit(DbfEngine* self)
{
    if (self) DbfTxHandle_BeginGroupCommit(self->tx);
}

void DbfEngine_EndGroupCommit(DbfEngine* self)
{
    if (self) DbfTxHandle_EndGroupCommit(self->tx);
}

void DbfEngine_FlushDurability(DbfEngine* self)
{
    if (self) DbfTxHandle_FlushDurability(self->tx);
}

int DbfEngine_CloneStructure(DbfEngine* self, const char* newFilePath)
{
    if (!self || !newFilePath) return GMB_DB_ARG;
    return DbfFile_CloneStructure(self->dbfFile, newFilePath);
}

static int eng_copy_visible_to(DbfEngine* self, const char* destPath, const int* order, int orderCount)
{
    DbfEngine* dest;
    uint32_t count, i;
    int isDeleted = 0, rawLen = 0;
    uint8_t* raw = NULL;
    int origRec, origEof, origBof;
    if (!self || !destPath) return GMB_DB_ARG;
    if (DbfFile_CloneStructure(self->dbfFile, destPath) != GMB_DB_OK) return GMB_DB_ERR;
    dest = DbfEngine_New(destPath, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!dest) {
        unlink(destPath);
        return GMB_DB_ERR;
    }
    origRec = self->currentRecordNumber;
    origEof = self->eof;
    origBof = self->bof;
    if (order && orderCount > 0) {
        for (i = 0; i < (uint32_t)orderCount; i++) {
            if (DbfFile_ReadRawRecord(self->dbfFile, order[i], &isDeleted, &raw, &rawLen) != GMB_DB_OK) {
                DbfEngine_Dispose(dest);
                unlink(destPath);
                return GMB_DB_ERR;
            }
            if ((!isDeleted || self->deletedVisible) && DbfEngine_AppendRecord(dest, raw, rawLen) != GMB_DB_OK) {
                free(raw);
                DbfEngine_Dispose(dest);
                unlink(destPath);
                return GMB_DB_ERR;
            }
            free(raw);
            raw = NULL;
        }
    } else {
        count = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
        DbfEngine_GoTopInternal(self);
        for (i = 1; i <= count; i++) {
            if (!DbfEngine_IsCurrentRecordValid(self)) break;
            if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &raw, &rawLen) != GMB_DB_OK) {
                DbfEngine_Dispose(dest);
                unlink(destPath);
                return GMB_DB_ERR;
            }
            if (DbfEngine_EvaluateFilter(self) && !isDeleted && DbfEngine_AppendRecord(dest, raw, rawLen) != GMB_DB_OK) {
                free(raw);
                DbfEngine_Dispose(dest);
                unlink(destPath);
                return GMB_DB_ERR;
            }
            free(raw);
            raw = NULL;
            DbfEngine_SkipInternal(self, 1);
        }
    }
    DbfEngine_Dispose(dest);
    self->currentRecordNumber = origRec;
    self->eof = origEof;
    self->bof = origBof;
    return GMB_DB_OK;
}

int DbfEngine_CopyTo(DbfEngine* self, const char* destinationFilePath)
{
    return eng_copy_visible_to(self, destinationFilePath, NULL, 0);
}

typedef struct EngSortRow {
    int recNo;
    char key[256];
} EngSortRow;

static int eng_sort_cmp(const void* a, const void* b)
{
    return strcmp(((const EngSortRow*)a)->key, ((const EngSortRow*)b)->key);
}

static int eng_sort_cmp_desc(const void* a, const void* b)
{
    return -strcmp(((const EngSortRow*)a)->key, ((const EngSortRow*)b)->key);
}

int DbfEngine_SortTo(DbfEngine* self, const char* destinationFilePath, const char* fieldName, int descending)
{
    uint32_t count, i;
    int nFields = 0, idx, nOut = 0;
    const DbfField* fields;
    EngSortRow* rows;
    int* order;
    int rc;
    int savedRec, savedEof, savedBof;
    if (!self || !destinationFilePath || !fieldName) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    idx = eng_find_field(fields, nFields, fieldName);
    if (idx < 0) return GMB_DB_ARG;
    count = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    rows = (EngSortRow*)calloc((size_t)count + 1, sizeof(EngSortRow));
    if (!rows) return GMB_DB_ERR;
    savedRec = self->currentRecordNumber;
    savedEof = self->eof;
    savedBof = self->bof;
    DbfEngine_GoTopInternal(self);
    while (DbfEngine_IsCurrentRecordValid(self) && nOut < (int)count) {
        rows[nOut].recNo = self->currentRecordNumber;
        if (DbfEngine_GetFieldText(self, idx, rows[nOut].key, (int)sizeof(rows[nOut].key)) != GMB_DB_OK)
            rows[nOut].key[0] = 0;
        nOut++;
        DbfEngine_SkipInternal(self, 1);
    }
    qsort(rows, (size_t)nOut, sizeof(EngSortRow), descending ? eng_sort_cmp_desc : eng_sort_cmp);
    order = (int*)malloc((size_t)nOut * sizeof(int));
    if (!order) {
        free(rows);
        return GMB_DB_ERR;
    }
    for (i = 0; i < (uint32_t)nOut; i++) order[i] = rows[i].recNo;
    free(rows);
    rc = eng_copy_visible_to(self, destinationFilePath, order, nOut);
    free(order);
    self->currentRecordNumber = savedRec;
    self->eof = savedEof;
    self->bof = savedBof;
    return rc;
}

/* ---- SORT multi-chiave (parità IL: fields[] + descending[] per campo) ----------------
 * Il sort a UNA chiave usava un'unica stringa; l'IL ordina per TUTTI i campi con
 * direzione per campo. Qui si costruisce una chiave PER CAMPO e si confronta campo per
 * campo con la sua direzione. Il contesto è in variabili di file (l'engine è usato da un
 * solo task GPM alla volta) perché qsort non accetta un ctx. Tiebreak su recNo ASC per
 * output deterministico (qsort senza tiebreak rende i pareggi arbitrari → parity flaky). */
#define ENG_SORT_MAXKEYS 8
typedef struct EngSortMultiRow {
    int recNo;
    char key[ENG_SORT_MAXKEYS][64];
} EngSortMultiRow;
static int g_eng_sort_nkeys;
static int g_eng_sort_desc[ENG_SORT_MAXKEYS];
static int g_eng_sort_numeric[ENG_SORT_MAXKEYS];

static int eng_sort_cmp_multi(const void* a, const void* b)
{
    const EngSortMultiRow* ra = (const EngSortMultiRow*)a;
    const EngSortMultiRow* rb = (const EngSortMultiRow*)b;
    int k;
    for (k = 0; k < g_eng_sort_nkeys; k++) {
        int c;
        /* Campi NUMERICI (N/F/I/Y/…) vanno confrontati come numeri: con strcmp
         * "50" > "250" (5 > 2) → SORT ON Saldo dava l'ordine sbagliato. */
        if (g_eng_sort_numeric[k]) {
            double da = atof(ra->key[k]);
            double db = atof(rb->key[k]);
            c = (da < db) ? -1 : (da > db ? 1 : 0);
        } else {
            c = strcmp(ra->key[k], rb->key[k]);
        }
        if (c != 0) return g_eng_sort_desc[k] ? -c : c;
    }
    return ra->recNo < rb->recNo ? -1 : (ra->recNo > rb->recNo ? 1 : 0);
}

static int eng_parse_csv_ints(const char* csv, int* out, int max)
{
    int n = 0;
    const char* p = csv;
    while (p && *p && n < max) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        out[n++] = (int)strtol(p, (char**)&p, 10);
        while (*p == ' ') p++;
    }
    return n;
}

int DbfEngine_SortToMulti(DbfEngine* self, const char* destinationFilePath,
                          const char* fieldsCsv, const char* descCsv)
{
    uint32_t count, i;
    int nFields = 0, nOut = 0, nkeys, k;
    const DbfField* fields;
    EngSortMultiRow* rows;
    int idx[ENG_SORT_MAXKEYS];
    int desc[ENG_SORT_MAXKEYS];
    int* order;
    int rc;
    int savedRec, savedEof, savedBof;
    char parts[ENG_SORT_MAXKEYS][64];
    int nparts = 0;
    const char* p;
    if (!self || !destinationFilePath || !fieldsCsv) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    if (!fields) return GMB_DB_ERR;
    /* split dei nomi campo (CSV) */
    p = fieldsCsv;
    while (p && *p && nparts < ENG_SORT_MAXKEYS) {
        const char* c = strchr(p, ',');
        size_t len = c ? (size_t)(c - p) : strlen(p);
        while (len > 0 && (p[0] == ' ' || p[0] == '\t')) { p++; len--; }
        while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
        if (len > 0 && len < sizeof(parts[0])) {
            memcpy(parts[nparts], p, len); parts[nparts][len] = 0; nparts++;
        }
        p = c ? c + 1 : NULL;
    }
    nkeys = nparts;
    for (k = 0; k < nkeys; k++) {
        idx[k] = eng_find_field(fields, nFields, parts[k]);
        desc[k] = 0;
        if (idx[k] < 0) return GMB_DB_ARG;
    }
    if (descCsv && *descCsv) (void)eng_parse_csv_ints(descCsv, desc, ENG_SORT_MAXKEYS);
    count = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    rows = (EngSortMultiRow*)calloc((size_t)count + 1, sizeof(EngSortMultiRow));
    if (!rows) return GMB_DB_ERR;
    savedRec = self->currentRecordNumber;
    savedEof = self->eof;
    savedBof = self->bof;
    DbfEngine_GoTopInternal(self);
    while (DbfEngine_IsCurrentRecordValid(self) && nOut < (int)count) {
        rows[nOut].recNo = self->currentRecordNumber;
        for (k = 0; k < nkeys; k++)
            if (DbfEngine_GetFieldText(self, idx[k], rows[nOut].key[k], (int)sizeof(rows[nOut].key[k])) != GMB_DB_OK)
                rows[nOut].key[k][0] = 0;
        nOut++;
        DbfEngine_SkipInternal(self, 1);
    }
    g_eng_sort_nkeys = nkeys;
    for (k = 0; k < nkeys; k++) {
        g_eng_sort_desc[k] = desc[k];
        /* Numerici (N/F/I/Y/…) → confronto numerico; C/M/D/L restano testuali. */
        {
            DbfFieldType ft = fields[idx[k]].Type;
            g_eng_sort_numeric[k] = (ft != DbfFieldType_Character && ft != DbfFieldType_Memo
                                     && ft != DbfFieldType_Date && ft != DbfFieldType_Logical);
        }
    }
    qsort(rows, (size_t)nOut, sizeof(EngSortMultiRow), eng_sort_cmp_multi);
    order = (int*)malloc((size_t)nOut * sizeof(int));
    if (!order) { free(rows); return GMB_DB_ERR; }
    for (i = 0; i < (uint32_t)nOut; i++) order[i] = rows[i].recNo;    free(rows);
    rc = eng_copy_visible_to(self, destinationFilePath, order, nOut);
    free(order);
    self->currentRecordNumber = savedRec;
    self->eof = savedEof;
    self->bof = savedBof;
    return rc;
}


int DbfEngine_SetFilter(DbfEngine* self, const char* expression)
{
    if (!self) return GMB_DB_ARG;
    if (!expression || !*expression) self->filterExpr[0] = 0;
    else snprintf(self->filterExpr, sizeof(self->filterExpr), "%s", expression);
    DbfEngine_GoTopInternal(self);
    return GMB_DB_OK;
}

const char* DbfEngine_GetFilterExpression(const DbfEngine* self)
{
    if (!self || !self->filterExpr[0]) return NULL;
    return self->filterExpr;
}

int DbfEngine_EvaluateFilter(DbfEngine* self)
{
    uint32_t n;
    if (!self || !self->filterExpr[0] || self->bof || self->eof) return 1;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (self->currentRecordNumber <= 0 || (uint32_t)self->currentRecordNumber > n) return 1;
    return RuntimeExpressionEvaluator_Eval(self->filterExpr, eng_filter_get, self);
}

int DbfEngine_EnableVersioning(DbfEngine* self)
{
    const char* path;
    if (!self) return GMB_DB_ARG;
    if (self->ver) return GMB_DB_OK;
    path = DbfFile_get_FilePath(self->dbfFile);
    self->ver = VersioningManager_New(path);
    return self->ver ? GMB_DB_OK : GMB_DB_ERR;
}

static void eng_csv_write_field(FILE* f, const char* s)
{
    const char* p;
    int quote = 0;
    if (!s) s = "";
    for (p = s; *p; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            quote = 1;
            break;
        }
    }
    if (!quote) {
        fputs(s, f);
        return;
    }
    fputc('"', f);
    for (p = s; *p; p++) {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

int DbfEngine_ExportToCsv(DbfEngine* self, const char* filePath)
{
    FILE* f;
    int nFields = 0, i, orig;
    const DbfField* fields;
    char val[512];
    if (!self || !filePath) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    f = fopen(filePath, "w");
    if (!f) return GMB_DB_ERR;
    orig = self->currentRecordNumber;
    for (i = 0; i < nFields; i++) {
        if (i) fputc(',', f);
        fputs(fields[i].Name, f);
    }
    fputc('\n', f);
    DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        for (i = 0; i < nFields; i++) {
            if (i) fputc(',', f);
            if (DbfEngine_GetFieldText(self, i, val, (int)sizeof(val)) != GMB_DB_OK) val[0] = 0;
            eng_csv_write_field(f, val);
        }
        fputc('\n', f);
        DbfEngine_SkipInternal(self, 1);
    }
    fclose(f);
    if (orig > 0) DbfEngine_PositionAtRecord(self, orig);
    else DbfEngine_GoTopInternal(self);
    return GMB_DB_OK;
}

int DbfEngine_ExportToJson(DbfEngine* self, const char* filePath)
{
    FILE* f;
    int nFields = 0, i, orig, first = 1;
    const DbfField* fields;
    char val[512], esc[1024];
    if (!self || !filePath) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    f = fopen(filePath, "w");
    if (!f) return GMB_DB_ERR;
    orig = self->currentRecordNumber;
    fputs("[\n", f);
    DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        if (!first) fputs(",\n", f);
        first = 0;
        fputs("  {\n", f);
        for (i = 0; i < nFields; i++) {
            if (DbfEngine_GetFieldText(self, i, val, (int)sizeof(val)) != GMB_DB_OK) val[0] = 0;
            eng_json_escape(esc, (int)sizeof(esc), val);
            fprintf(f, "    \"%s\": \"%s\"%s\n", fields[i].Name, esc, i + 1 < nFields ? "," : "");
        }
        fputs("  }", f);
        DbfEngine_SkipInternal(self, 1);
    }
    fputs("\n]\n", f);
    fclose(f);
    if (orig > 0) DbfEngine_PositionAtRecord(self, orig);
    else DbfEngine_GoTopInternal(self);
    return GMB_DB_OK;
}

static const char* eng_jskip(const char* p)
{
    while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static int eng_jstr(const char** pp, char* out, int cap)
{
    const char* p = eng_jskip(*pp);
    int n = 0;
    if (!p || *p != '"') return GMB_DB_ERR;
    p++;
    while (*p && *p != '"' && n < cap - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            out[n++] = *p++;
        } else {
            out[n++] = *p++;
        }
    }
    if (*p != '"') return GMB_DB_ERR;
    out[n] = 0;
    *pp = p + 1;
    return GMB_DB_OK;
}

static int eng_jval(const char** pp, char* out, int cap)
{
    const char* p = eng_jskip(*pp);
    int n = 0, depth;
    if (!p || !*p) return GMB_DB_ERR;
    if (*p == '"') return eng_jstr(pp, out, cap);
    if (*p == '{' || *p == '[') {
        depth = 0;
        do {
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']') depth--;
            if (n < cap - 1) out[n++] = *p;
            p++;
        } while (*p && depth > 0);
        out[n] = 0;
        *pp = p;
        return GMB_DB_OK;
    }
    if (strncmp(p, "true", 4) == 0) {
        snprintf(out, (size_t)cap, "T");
        *pp = p + 4;
        return GMB_DB_OK;
    }
    if (strncmp(p, "false", 5) == 0) {
        snprintf(out, (size_t)cap, "F");
        *pp = p + 5;
        return GMB_DB_OK;
    }
    if (strncmp(p, "null", 4) == 0) {
        out[0] = 0;
        *pp = p + 4;
        return GMB_DB_OK;
    }
    if (*p == '-') out[n++] = *p++;
    while ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+') {
        if (n < cap - 1) out[n++] = *p++;
        else p++;
    }
    out[n] = 0;
    *pp = p;
    return n > 0 ? GMB_DB_OK : GMB_DB_ERR;
}

int DbfEngine_ImportFromJson(DbfEngine* self, const char* filePath)
{
    FILE* f;
    char* buf = NULL;
    long sz;
    const char* p;
    int nFields = 0, payloadLen;
    const DbfField* fields;
    if (!self || !filePath) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    payloadLen = (int)DbfFile_get_Header(self->dbfFile)->RecordLength - 1;
    f = fopen(filePath, "rb");
    if (!f) return GMB_DB_ERR;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    rewind(f);
    buf = (char*)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return GMB_DB_ERR;
    }
    fclose(f);
    buf[sz] = 0;
    p = eng_jskip(buf);
    if (*p != '[') {
        free(buf);
        return GMB_DB_ERR;
    }
    p++;
    if (DbfEngine_BeginTransaction(self) != GMB_DB_OK) {
        free(buf);
        return GMB_DB_ERR;
    }
    p = eng_jskip(p);
    while (*p && *p != ']') {
        uint8_t* payload;
        int off = 0, fi;
        if (*p == ',') {
            p = eng_jskip(p + 1);
            continue;
        }
        if (*p != '{') {
            DbfEngine_Rollback(self);
            free(buf);
            return GMB_DB_ERR;
        }
        p++;
        payload = (uint8_t*)malloc((size_t)payloadLen);
        if (!payload) {
            DbfEngine_Rollback(self);
            free(buf);
            return GMB_DB_ERR;
        }
        memset(payload, ' ', (size_t)payloadLen);
        for (fi = 0; fi < nFields; fi++) {
            DbfFieldWriter_WriteBlank(payload + off, fields[fi].Length, &fields[fi], self->memoFile);
            off += fields[fi].Length;
        }
        p = eng_jskip(p);
        while (*p && *p != '}') {
            char key[64], val[1024];
            int idx;
            if (*p == ',') {
                p = eng_jskip(p + 1);
                continue;
            }
            if (eng_jstr(&p, key, (int)sizeof(key)) != GMB_DB_OK) {
                free(payload);
                DbfEngine_Rollback(self);
                free(buf);
                return GMB_DB_ERR;
            }
            p = eng_jskip(p);
            if (*p != ':') {
                free(payload);
                DbfEngine_Rollback(self);
                free(buf);
                return GMB_DB_ERR;
            }
            p++;
            if (eng_jval(&p, val, (int)sizeof(val)) != GMB_DB_OK) {
                free(payload);
                DbfEngine_Rollback(self);
                free(buf);
                return GMB_DB_ERR;
            }
            idx = eng_find_field(fields, nFields, key);
            if (idx < 0) {
                free(payload);
                DbfEngine_Rollback(self);
                free(buf);
                return GMB_DB_ERR;
            }
            off = eng_field_offset(fields, nFields, idx);
            DbfFieldWriter_Write(payload + off, fields[idx].Length, &fields[idx], val, self->memoFile);
            p = eng_jskip(p);
        }
        if (*p == '}') p++;
        if (DbfEngine_AppendRecord(self, payload, payloadLen) != GMB_DB_OK) {
            free(payload);
            DbfEngine_Rollback(self);
            free(buf);
            return GMB_DB_ERR;
        }
        free(payload);
        p = eng_jskip(p);
    }
    free(buf);
    return DbfEngine_Commit(self);
}

int DbfEngine_ImportFromCsv(DbfEngine* self, const char* filePath)
{
    FILE* f;
    char line[4096];
    char headers[GMB_DBF_ENGINE_MAX_INDEXES][32];
    int nHead = 0, i;
    int nFields = 0;
    const DbfField* fields;
    if (!self || !filePath) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    f = fopen(filePath, "r");
    if (!f) return GMB_DB_ERR;
    if (!fgets(line, (int)sizeof(line), f)) {
        fclose(f);
        return GMB_DB_ERR;
    }
    {
        char* save = NULL;
        char* tok = strtok_r(line, ",\r\n", &save);
        while (tok && nHead < GMB_DBF_ENGINE_MAX_INDEXES) {
            while (*tok == ' ') tok++;
            snprintf(headers[nHead++], sizeof(headers[0]), "%s", tok);
            tok = strtok_r(NULL, ",\r\n", &save);
        }
    }
    for (i = 0; i < nHead; i++) {
        if (eng_find_field(fields, nFields, headers[i]) < 0) {
            fclose(f);
            return GMB_DB_ERR;
        }
    }
    if (DbfEngine_BeginTransaction(self) != GMB_DB_OK) {
        fclose(f);
        return GMB_DB_ERR;
    }
    while (fgets(line, (int)sizeof(line), f)) {
        char* save = NULL;
        char* tok;
        int col = 0;
        int payloadLen = (int)DbfFile_get_Header(self->dbfFile)->RecordLength - 1;
        uint8_t* payload = (uint8_t*)malloc((size_t)payloadLen);
        int off = 0, fi;
        if (!payload) {
            DbfEngine_Rollback(self);
            fclose(f);
            return GMB_DB_ERR;
        }
        memset(payload, ' ', (size_t)payloadLen);
        for (fi = 0; fi < nFields; fi++) {
            DbfFieldWriter_WriteBlank(payload + off, fields[fi].Length, &fields[fi], self->memoFile);
            off += fields[fi].Length;
        }
        tok = strtok_r(line, ",\r\n", &save);
        while (tok && col < nHead) {
            int idx = eng_find_field(fields, nFields, headers[col]);
            if (idx >= 0) {
                int o = eng_field_offset(fields, nFields, idx);
                DbfFieldWriter_Write(payload + o, fields[idx].Length, &fields[idx], tok, self->memoFile);
            }
            col++;
            tok = strtok_r(NULL, ",\r\n", &save);
        }
        if (DbfEngine_AppendRecord(self, payload, payloadLen) != GMB_DB_OK) {
            free(payload);
            DbfEngine_Rollback(self);
            fclose(f);
            return GMB_DB_ERR;
        }
        free(payload);
    }
    fclose(f);
    return DbfEngine_Commit(self);
}

int DbfEngine_AppendFromCond(DbfEngine* self, const char* sourceFilePath, const char* condition)
{
    DbfEngine* src;
    int nfDst = 0, nfSrc = 0, isDeleted = 0, rawLen = 0;
    uint8_t* raw = NULL;
    const DbfField* dstF;
    const DbfField* srcF;
    if (!self || !sourceFilePath) return GMB_DB_ARG;
    src = DbfEngine_New(sourceFilePath, NULL, 0, 0, LockingStrategy_IntraProcess);
    if (!src) return GMB_DB_ERR;
    dstF = DbfFile_get_Fields(self->dbfFile, &nfDst);
    srcF = DbfFile_get_Fields(src->dbfFile, &nfSrc);
    (void)dstF;
    (void)srcF;
    if (nfDst != nfSrc) {
        DbfEngine_Dispose(src);
        return GMB_DB_ERR;
    }
    /* APPEND FROM ... FOR <cond>: il filtro va applicato al file SORGENTE
     * (l'engine apre una propria istanza di `src`), non alla destinazione. */
    if (condition && *condition) DbfEngine_SetFilter(src, condition);
    DbfEngine_BeginBulkLoad(self);
    DbfEngine_GoTopInternal(src);
    while (!src->eof) {
        if (DbfFile_ReadRawRecord(src->dbfFile, src->currentRecordNumber, &isDeleted, &raw, &rawLen) != GMB_DB_OK) {
            DbfEngine_EndBulkLoad(self);
            DbfEngine_Dispose(src);
            return GMB_DB_ERR;
        }
        if (DbfEngine_EvaluateFilter(src) && !isDeleted && DbfEngine_AppendRecord(self, raw, rawLen) != GMB_DB_OK) {
            free(raw);
            DbfEngine_EndBulkLoad(self);
            DbfEngine_Dispose(src);
            return GMB_DB_ERR;
        }
        free(raw);
        raw = NULL;
        DbfEngine_SkipInternal(src, 1);
    }
    DbfEngine_EndBulkLoad(self);
    DbfEngine_Dispose(src);
    return GMB_DB_OK;
}

int DbfEngine_AppendFrom(DbfEngine* self, const char* sourceFilePath)
{
    return DbfEngine_AppendFromCond(self, sourceFilePath, NULL);
}

int DbfEngine_WriteList(DbfEngine* self, FILE* writer, int startAtTop)
{
    int nFields = 0, i, orig, origEof, origBof, written = 0;
    const DbfField* fields;
    char val[256];
    const char* cols[64];
    char store[64][256];
    char line[2048];
    if (!self || !writer) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    if (nFields > 64) nFields = 64;
    orig = self->currentRecordNumber;
    origEof = self->eof;
    origBof = self->bof;
    if (startAtTop) DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        for (i = 0; i < nFields; i++) {
            if (DbfEngine_GetFieldText(self, i, val, (int)sizeof(val)) != GMB_DB_OK) val[0] = 0;
            snprintf(store[i], sizeof(store[i]), "%s", val);
            cols[i] = store[i];
            (void)fields;
        }
        DbfListFormatter_FormatLine(self->currentRecordNumber, cols, nFields, line, (int)sizeof(line));
        fprintf(writer, "%s\n", line);
        written++;
        DbfEngine_SkipInternal(self, 1);
    }
    self->currentRecordNumber = orig;
    self->eof = origEof;
    self->bof = origBof;
    DbfEngine_UpdateBofEofStatus(self);
    return written;
}

int DbfEngine_TotalOnCond(DbfEngine* self, const char* destinationFilePath, const char* keyField,
                          const char* const* sumFields, int sumFieldCount, const char* condition)
{
    DbfEngine* dest;
    char lastKey[256], curKey[256];
    double sums[32];
    int nFields = 0, i, keyIdx, haveGroup = 0;
    const DbfField* fields;
    uint8_t* payload;
    int payloadLen, off;
    if (!self || !destinationFilePath || !keyField) return GMB_DB_ARG;
    if (self->activeOrder < 0) return GMB_DB_ERR;
    if (sumFieldCount < 0 || sumFieldCount > 32) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    keyIdx = eng_find_field(fields, nFields, keyField);
    if (keyIdx < 0) return GMB_DB_ARG;
    if (DbfFile_CloneStructure(self->dbfFile, destinationFilePath) != GMB_DB_OK) return GMB_DB_ERR;
    dest = DbfEngine_New(destinationFilePath, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!dest) {
        unlink(destinationFilePath);
        return GMB_DB_ERR;
    }
    payloadLen = (int)DbfFile_get_Header(self->dbfFile)->RecordLength - 1;
    payload = (uint8_t*)malloc((size_t)payloadLen);
    if (!payload) {
        DbfEngine_Dispose(dest);
        unlink(destinationFilePath);
        return GMB_DB_ERR;
    }
    lastKey[0] = 0;
    memset(sums, 0, sizeof(sums));
    DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        char num[64];
        if (DbfEngine_GetFieldText(self, keyIdx, curKey, (int)sizeof(curKey)) != GMB_DB_OK) curKey[0] = 0;
        if (condition && *condition
            && !RuntimeExpressionEvaluator_Eval(condition, eng_filter_get, self)) {
            DbfEngine_SkipInternal(self, 1);
            continue;
        }
        if (haveGroup && strcmp(curKey, lastKey) != 0) {
            int fi;
            memset(payload, ' ', (size_t)payloadLen);
            off = 0;
            for (fi = 0; fi < nFields; fi++) {
                DbfFieldWriter_WriteBlank(payload + off, fields[fi].Length, &fields[fi], dest->memoFile);
                off += fields[fi].Length;
            }
            off = eng_field_offset(fields, nFields, keyIdx);
            DbfFieldWriter_Write(payload + off, fields[keyIdx].Length, &fields[keyIdx], lastKey, dest->memoFile);
            for (i = 0; i < sumFieldCount; i++) {
                int si = eng_find_field(fields, nFields, sumFields[i]);
                if (si < 0) continue;
                snprintf(num, sizeof(num), "%.4f", sums[i]);
                off = eng_field_offset(fields, nFields, si);
                DbfFieldWriter_Write(payload + off, fields[si].Length, &fields[si], num, dest->memoFile);
            }
            if (DbfEngine_AppendRecord(dest, payload, payloadLen) != GMB_DB_OK) {
                free(payload);
                DbfEngine_Dispose(dest);
                unlink(destinationFilePath);
                return GMB_DB_ERR;
            }
            memset(sums, 0, sizeof(sums));
        }
        snprintf(lastKey, sizeof(lastKey), "%s", curKey);
        haveGroup = 1;
        for (i = 0; i < sumFieldCount; i++) {
            int si = eng_find_field(fields, nFields, sumFields[i]);
            if (si < 0) continue;
            if (DbfEngine_GetFieldText(self, si, num, (int)sizeof(num)) != GMB_DB_OK) num[0] = 0;
            sums[i] += strtod(num, NULL);
        }
        DbfEngine_SkipInternal(self, 1);
    }
    if (haveGroup) {
        int fi;
        char num[64];
        memset(payload, ' ', (size_t)payloadLen);
        off = 0;
        for (fi = 0; fi < nFields; fi++) {
            DbfFieldWriter_WriteBlank(payload + off, fields[fi].Length, &fields[fi], dest->memoFile);
            off += fields[fi].Length;
        }
        off = eng_field_offset(fields, nFields, keyIdx);
        DbfFieldWriter_Write(payload + off, fields[keyIdx].Length, &fields[keyIdx], lastKey, dest->memoFile);
        for (i = 0; i < sumFieldCount; i++) {
            int si = eng_find_field(fields, nFields, sumFields[i]);
            if (si < 0) continue;
            snprintf(num, sizeof(num), "%.4f", sums[i]);
            off = eng_field_offset(fields, nFields, si);
            DbfFieldWriter_Write(payload + off, fields[si].Length, &fields[si], num, dest->memoFile);
        }
        if (DbfEngine_AppendRecord(dest, payload, payloadLen) != GMB_DB_OK) {
            free(payload);
            DbfEngine_Dispose(dest);
            unlink(destinationFilePath);
            return GMB_DB_ERR;
        }
    }
    free(payload);
    DbfEngine_Dispose(dest);
    return GMB_DB_OK;
}

int DbfEngine_TotalOn(DbfEngine* self, const char* destinationFilePath, const char* keyField,
                      const char* const* sumFields, int sumFieldCount)
{
    return DbfEngine_TotalOnCond(self, destinationFilePath, keyField, sumFields, sumFieldCount, NULL);
}

int DbfEngine_SetReplication(DbfEngine* self, const char* logPath)
{
    char path[1024];
    if (!self) return GMB_DB_ARG;
    if (self->repl) {
        DbfReplicationLog_Dispose(self->repl);
        self->repl = NULL;
    }
    if (!logPath || !*logPath) return GMB_DB_OK;
    if (strcmp(logPath, ".") == 0 || gmb_strcasecmp(logPath, "DEFAULT") == 0) {
        snprintf(path, sizeof(path), "%s.repl", DbfFile_get_FilePath(self->dbfFile));
    } else {
        snprintf(path, sizeof(path), "%s", logPath);
    }
    self->repl = DbfReplicationLog_New(path);
    return self->repl ? GMB_DB_OK : GMB_DB_ERR;
}

const char* DbfEngine_GetReplicationLogPath(const DbfEngine* self)
{
    return self && self->repl ? DbfReplicationLog_get_LogPath(self->repl) : NULL;
}

int DbfEngine_ApplyReplication(DbfEngine* self, const char* replLogPath)
{
    return DbfReplicationApplier_Apply(replLogPath, self);
}

int DbfEngine_FollowReplication(DbfEngine* self, const char* replLogPath, const char* checkpointPath)
{
    const char* path = replLogPath;
    if (!self) return GMB_DB_ARG;
    if (!path || !path[0]) path = DbfEngine_GetReplicationLogPath(self);
    if (!path) return GMB_DB_ARG;
    return DbfReplicationApplier_Follow(path, self, checkpointPath);
}

int DbfEngine_ReplicationShipTcp(DbfEngine* self, const char* host, int port, const char* replLogPath,
                                 const char* shipCheckpointPath)
{
    const char* path = replLogPath;
    if (!self || !host) return GMB_DB_ARG;
    if (!path || !path[0]) path = DbfEngine_GetReplicationLogPath(self);
    if (!path) return GMB_DB_ARG;
    return ReplicationTcpShipper_ShipOnce(path, host, port, shipCheckpointPath);
}

static int eng_repl_apply_cb(ReplicationApplyState* state, const char* line, void* ctx)
{
    return DbfReplicationApplier_ApplyLine(line, state, (DbfEngine*)ctx);
}

int DbfEngine_ReplicationRecvTcp(DbfEngine* self, int port, const char* checkpointPath)
{
    if (!self) return GMB_DB_ARG;
    DbfEngine_SetReplication(self, "");
    return ReplicationTcpReceiver_ReceiveOnce(port, eng_repl_apply_cb, self, checkpointPath, 30);
}

static int eng_sc_rm(const char* path) { return path ? unlink(path) : 0; }

typedef struct EngRecvJob {
    DbfEngine* e;
    int port;
    int applied;
} EngRecvJob;

static void* eng_sc_recv(void* arg)
{
    EngRecvJob* job = (EngRecvJob*)arg;
    job->applied = DbfEngine_ReplicationRecvTcp(job->e, job->port, NULL);
    return NULL;
}

static int eng_sc_write_rec(DbfEngine* e, const char* name10)
{
    const DbfHeader* hdr = DbfFile_get_Header(e->dbfFile);
    int payload = (int)hdr->RecordLength - 1;
    uint8_t* rec;
    int rc;
    rec = (uint8_t*)malloc((size_t)payload);
    if (!rec) return GMB_DB_ERR;
    memset(rec, ' ', (size_t)payload);
    memcpy(rec, name10, 10);
    rc = DbfFile_AppendRawRecord(e->dbfFile, rec, payload);
    free(rec);
    return rc < 0 ? GMB_DB_ERR : GMB_DB_OK;
}

int DbfEngine_SetDurability(DbfEngine* self, DbfDurabilityMode mode)
{
    if (!self || !self->tx) return GMB_DB_ERR;
    DbfTxHandle_SetDurability(self->tx, mode);
    return GMB_DB_OK;
}

/* DBF UPDATE: itera tutti i record e valuta per-record le espressioni di valore
 * (stringhe, valutate con gmb_expr_eval_filter_ex contro i campi del record
 * corrente) scrivendo i campi — parita' C# DbfUpdate (che ignora keyField e
 * itera tutti i record). fieldsCsv/exprsCsv sono CSV paralleli. */
typedef struct { DbfEngine* eng; } DbfUpdateCtx;

static int dbf_update_get(void* ctx, const char* fieldName, gmb_val_t* out)
{
    DbfUpdateCtx* c = (DbfUpdateCtx*)ctx;
    DbfEngine* e = c ? c->eng : NULL;
    int nFields = 0, idx;
    const DbfField* fields;
    char text[256];
    if (!out) return -1;
    *out = gmb_val_nil();
    if (!e || !fieldName) return 0;
    fields = DbfFile_get_Fields(e->dbfFile, &nFields);
    idx = eng_find_field(fields, nFields, fieldName);
    if (idx < 0) return 0;
    if (DbfEngine_GetFieldText(e, idx, text, (int)sizeof(text)) != GMB_DB_OK) return 0;
    { char* t1 = text + strlen(text); while (t1 > text && (t1[-1] == ' ' || t1[-1] == '\t')) *--t1 = 0; }
    if (fields[idx].Type == DbfFieldType_Character || fields[idx].Type == DbfFieldType_Memo)
        *out = gmb_make_val_string(text);
    else
        *out = gmb_make_val_double(atof(text));
    return 0;
}

static int dbf_split_csv(const char* csv, char out[][64], int maxn)
{
    int n = 0;
    const char* p = csv ? csv : "";
    while (*p && n < maxn) {
        int fi = 0;
        while (*p && *p != ',' && fi < 63) out[n][fi++] = *p++;
        out[n][fi] = 0;
        if (*p == ',') p++;
        n++;
    }
    return n;
}

static void dbf_val_to_str(const gmb_val_t* v, char* out, int cap)
{
    if (!v || !out || cap <= 0) return;
    out[0] = 0;
    switch (v->kind) {
        case GMB_TYPE_STRING: {
            const char* s = v->v_string.data ? v->v_string.data : "";
            size_t n = v->v_string.length;
            if (n >= (size_t)cap) n = (size_t)cap - 1;
            memcpy(out, s, n); out[n] = 0;
            break;
        }
        case GMB_TYPE_INT: snprintf(out, cap, "%lld", (long long)v->v_int); break;
        case GMB_TYPE_DOUBLE: snprintf(out, cap, "%g", v->v_double); break;
        case GMB_TYPE_BOOL: snprintf(out, cap, "%d", v->v_bool ? 1 : 0); break;
        default: out[0] = 0; break;
    }
}

int DbfEngine_Update(DbfEngine* self, const char* fieldsCsv, const char* exprsCsv)
{
    char flds[32][64], exprs[32][64];
    int nf, i, nFields = 0;
    const DbfField* fields;
    DbfUpdateCtx ctx;
    if (!self || !self->dbfFile) return GMB_DB_ARG;
    nf = dbf_split_csv(fieldsCsv, flds, 32);
    if (nf != dbf_split_csv(exprsCsv, exprs, 32)) return GMB_DB_ARG;
    if (nf == 0) return GMB_DB_OK;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    ctx.eng = self;
    DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        for (i = 0; i < nf; i++) {
            gmb_val_t v = gmb_val_nil();
            char valstr[128];
            int idx = eng_find_field(fields, nFields, flds[i]);
            if (idx < 0) continue;
            int erc = gmb_eval_filter_env(exprs[i], dbf_update_get, NULL, &ctx, NULL,
                                           NULL, NULL, NULL, NULL, &v);
            if (erc != 0)
                continue;
            dbf_val_to_str(&v, valstr, (int)sizeof(valstr));
            DbfEngine_ReplaceField(self, idx, valstr);
        }
        DbfEngine_SkipInternal(self, 1);
    }
    return GMB_DB_OK;
}

/* ── DBF UPDATE ON <key> FROM <alias> REPLACE … ────────────────────────────────────────
 * Per OGNI record della destinazione (self) cerca nel SORGENTE il record con lo STESSO
 * valore del campo <key> e applica le sostituzioni. Contesto di valutazione:
 *   - nomi NUDI (`Nome`, `Saldo`)            → campi della DESTINAZIONE (area corrente);
 *   - nomi QUALIFICATI `<alias>.<campo>`      → campi del SORGENTE.
 * È la semantica Clipper (e il commento del sample ClipperDbfLateBinding §7). */
typedef struct { DbfEngine* dest; DbfEngine* src; char alias[64]; } DbfUpdateOnCtx;

static int dbf_update_on_get(void* ctx, const char* fieldName, gmb_val_t* out)
{
    DbfUpdateOnCtx* u = (DbfUpdateOnCtx*)ctx;
    DbfEngine* e;
    const char* bare = fieldName;
    int side_src = 0;
    int nFields = 0, idx;
    const DbfField* fields;
    char text[256];
    if (!out) return -1;
    *out = gmb_val_nil();
    if (!u || !fieldName) return 0;
    {
        const char* dot = strchr(fieldName, '.');
        if (dot && dot != fieldName) {
            char head[64];
            size_t hl = (size_t)(dot - fieldName);
            if (hl < sizeof(head)) {
                memcpy(head, fieldName, hl); head[hl] = 0;
                if (u->alias[0] && gmb_strcasecmp(head, u->alias) == 0) { side_src = 1; bare = dot + 1; }
            }
        }
    }
    e = side_src ? u->src : u->dest;
    if (!e || !e->dbfFile) return 0;
    fields = DbfFile_get_Fields(e->dbfFile, &nFields);
    idx = eng_find_field(fields, nFields, bare);
    if (idx < 0) return 0;
    if (DbfEngine_GetFieldText(e, idx, text, (int)sizeof(text)) != GMB_DB_OK) return 0;
    { char* t1 = text + strlen(text); while (t1 > text && (t1[-1] == ' ' || t1[-1] == '\t')) *--t1 = 0; }
    if (fields[idx].Type == DbfFieldType_Character || fields[idx].Type == DbfFieldType_Memo)
        *out = gmb_make_val_string(text);
    else
        *out = gmb_make_val_double(atof(text));
    return 0;
}

int DbfEngine_UpdateOn(DbfEngine* dest, DbfEngine* src, const char* srcAlias,
                       const char* keyField, const char* fieldsCsv, const char* exprsCsv)
{
    char flds[32][64], exprs[32][64];
    int nf, i;
    int nDestFields = 0, nSrcFields = 0;
    const DbfField* destFields;
    const DbfField* srcFields;
    int keyDest, keySrc;
    DbfUpdateOnCtx ctx;
    if (!dest || !dest->dbfFile || !src || !src->dbfFile || !keyField) return GMB_DB_ARG;
    nf = dbf_split_csv(fieldsCsv, flds, 32);
    if (nf != dbf_split_csv(exprsCsv, exprs, 32)) return GMB_DB_ARG;
    if (nf == 0) return GMB_DB_OK;
    destFields = DbfFile_get_Fields(dest->dbfFile, &nDestFields);
    srcFields = DbfFile_get_Fields(src->dbfFile, &nSrcFields);
    keyDest = eng_find_field(destFields, nDestFields, keyField);
    keySrc = eng_find_field(srcFields, nSrcFields, keyField);
    if (keyDest < 0 || keySrc < 0) return GMB_DB_ARG;
    ctx.dest = dest;
    ctx.src = src;
    snprintf(ctx.alias, sizeof(ctx.alias), "%s", srcAlias ? srcAlias : "");
    DbfEngine_GoTopInternal(dest);
    while (!dest->eof) {
        char dkey[256];
        int found = 0, okKey = 0;
        if (DbfEngine_GetFieldText(dest, keyDest, dkey, (int)sizeof(dkey)) == GMB_DB_OK) {
            char* t1 = dkey + strlen(dkey);
            while (t1 > dkey && (t1[-1] == ' ' || t1[-1] == '\t')) *--t1 = 0;
            okKey = 1;
        }
        if (okKey) {
            DbfEngine_GoTopInternal(src);
            while (!src->eof) {
                char skey[256];
                if (DbfEngine_GetFieldText(src, keySrc, skey, (int)sizeof(skey)) == GMB_DB_OK) {
                    char* t1 = skey + strlen(skey);
                    while (t1 > skey && (t1[-1] == ' ' || t1[-1] == '\t')) *--t1 = 0;
                    if (gmb_strcasecmp(skey, dkey) == 0) { found = 1; break; }
                }
                DbfEngine_SkipInternal(src, 1);
            }
        }
        if (found) {
            for (i = 0; i < nf; i++) {
                gmb_val_t v = gmb_val_nil();
                char valstr[128];
                int idx = eng_find_field(destFields, nDestFields, flds[i]);
                if (idx < 0) continue;
                int erc = gmb_eval_filter_env(exprs[i], dbf_update_on_get, NULL, &ctx, NULL,
                                               NULL, NULL, NULL, NULL, &v);
                if (erc != 0)
                    continue;
                dbf_val_to_str(&v, valstr, (int)sizeof(valstr));
                DbfEngine_ReplaceField(dest, idx, valstr);
            }
        }
        DbfEngine_SkipInternal(dest, 1);
    }
    return GMB_DB_OK;
}

/* DBF JOIN WITH: nested-loop join (parita' C# DbfDriver.NestedLoopJoin).
 * Merge gli schemi (campi src poi sec, nomi unici), crea il file di destinazione,
 * itera src x sec, valuta la condizione contro il contesto combinato e appende
 * i record corrispondenti. */
typedef struct { DbfEngine* a; DbfEngine* b; char aliasA[64]; char aliasB[64]; } DbfJoinCtx;

static int dbf_join_get(void* ctx, const char* fieldName, gmb_val_t* out)
{
    DbfJoinCtx* j = (DbfJoinCtx*)ctx;
    DbfEngine* e;
    int nFields = 0, idx;
    const DbfField* fields;
    char text[256];
    /* Riferimento qualificato Alias.Campo (es. O.IDCLI = C.ID con ID in entrambe):
     * l'alias seleziona il lato, il resto è il campo bare. Senza punto, o con alias
     * sconosciuto: legacy A-poi-B (src-wins, parità C# MergeFieldContext bare). */
    const char* bare = fieldName;
    int forceSide = 0; /* 0 = legacy, 1 = lato A, 2 = lato B */
    if (j && fieldName) {
        const char* dot = strchr(fieldName, '.');
        if (dot && dot != fieldName) {
            char head[64]; size_t hl = (size_t)(dot - fieldName);
            if (hl < sizeof(head)) {
                memcpy(head, fieldName, hl); head[hl] = 0;
                if (j->aliasA[0] && gmb_strcasecmp(head, j->aliasA) == 0) { forceSide = 1; bare = dot + 1; }
                else if (j->aliasB[0] && gmb_strcasecmp(head, j->aliasB) == 0) { forceSide = 2; bare = dot + 1; }
            }
        }
    }
    if (!out) return -1;
    *out = gmb_val_nil();
    if (!j || !fieldName) return 0;
    e = (forceSide == 2) ? j->b : j->a;
    fields = e ? DbfFile_get_Fields(e->dbfFile, &nFields) : NULL;
    idx = fields ? eng_find_field(fields, nFields, bare) : -1;
    if (idx < 0 && forceSide == 0 && j->b) {
        e = j->b;
        fields = DbfFile_get_Fields(e->dbfFile, &nFields);
        idx = fields ? eng_find_field(fields, nFields, bare) : -1;
    }
    if (!e || idx < 0) return 0;
    if (DbfEngine_GetFieldText(e, idx, text, (int)sizeof(text)) != GMB_DB_OK) return 0;
    { char* t1 = text + strlen(text); while (t1 > text && (t1[-1] == ' ' || t1[-1] == '\t')) *--t1 = 0; }
    if (fields[idx].Type == DbfFieldType_Character || fields[idx].Type == DbfFieldType_Memo)
        *out = gmb_make_val_string(text);
    else
        *out = gmb_make_val_double(atof(text));
    return 0;
}

static void dbf_join_unique_name(const char* base, char* out, int cap,
                                 const char (*names)[12], int* used)
{
    int k = 0;
    for (;;) {
        if (k == 0) snprintf(out, cap, "%s", base);
        else snprintf(out, cap, "%s%d", base, k);
        int clash = 0;
        for (int q = 0; q < *used; q++)
            if (gmb_strcasecmp(names[q], out) == 0) { clash = 1; break; }
        if (!clash) { snprintf((char*)names[*used], 12, "%s", out); (*used)++; return; }
        k++;
    }
}

int DbfEngine_Join(DbfEngine* self, DbfEngine* other, const char* destPath, const char* condition,
                   const char* aliasA, const char* aliasB)
{
    int nA = 0, nB = 0, nM = 0, i, j, payloadLen;
    const DbfField* fa = self ? DbfFile_get_Fields(self->dbfFile, &nA) : NULL;
    const DbfField* fb = other ? DbfFile_get_Fields(other->dbfFile, &nB) : NULL;
    DbfField merged[64];
    char usedNames[64][12];
    int usedCount = 0;
    DbfEngine* dest;
    DbfJoinCtx ctx;
    uint8_t* payload;
    const DbfHeader* hdr;
    if (!self || !other || !destPath || nA + nB > 64) return GMB_DB_ARG;
    ctx.a = self; ctx.b = other;
    snprintf(ctx.aliasA, sizeof(ctx.aliasA), "%s", aliasA ? aliasA : "");
    snprintf(ctx.aliasB, sizeof(ctx.aliasB), "%s", aliasB ? aliasB : "");
    for (i = 0; i < nA; i++) {
        char nm[12];
        dbf_join_unique_name(fa[i].Name, nm, 12, usedNames, &usedCount);
        merged[nM] = fa[i];
        snprintf(merged[nM].Name, 12, "%s", nm);
        nM++;
    }
    for (j = 0; j < nB; j++) {
        char nm[12];
        dbf_join_unique_name(fb[j].Name, nm, 12, usedNames, &usedCount);
        merged[nM] = fb[j];
        snprintf(merged[nM].Name, 12, "%s", nm);
        nM++;
    }
    unlink(destPath);
    if (DbfFile_CreateNew(destPath, "windows-1252", merged, nM) != GMB_DB_OK) return GMB_DB_ERR;
    dest = DbfEngine_New(destPath, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!dest) { unlink(destPath); return GMB_DB_ERR; }
    hdr = DbfFile_get_Header(self->dbfFile);
    payloadLen = (int)hdr->RecordLength - 1 + (nB > 0 ? (int)DbfFile_get_Header(other->dbfFile)->RecordLength - 1 : 0);
    payload = (uint8_t*)malloc((size_t)(payloadLen > 0 ? payloadLen : 1));
    if (!payload) { DbfEngine_Dispose(dest); unlink(destPath); return GMB_DB_ERR; }
    ctx.a = self; ctx.b = other;
    DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        DbfEngine_GoTopInternal(other);
        while (!other->eof) {
            if (condition && *condition) {
                gmb_val_t v = gmb_val_nil();
                int erc = gmb_eval_filter_env(condition, dbf_join_get, NULL, &ctx, NULL,
                                               NULL, NULL, NULL, NULL, &v);
                if (erc != 0) {
                    DbfEngine_SkipInternal(other, 1);
                    continue;
                }
                if (!(v.kind == GMB_TYPE_BOOL ? v.v_bool : (v.kind == GMB_TYPE_INT ? v.v_int != 0 : v.v_double != 0.0))) {
                    DbfEngine_SkipInternal(other, 1);
                    continue;
                }
            }
            /* build payload: campi src poi sec */
            memset(payload, ' ', (size_t)payloadLen);
            {
                int off = 0;
                for (i = 0; i < nA; i++) {
                    char txt[256];
                    DbfEngine_GetFieldText(self, i, txt, (int)sizeof(txt));
                    DbfFieldWriter_Write(payload + off, merged[i].Length, &merged[i], txt, dest->memoFile);
                    off += merged[i].Length;
                }
                for (j = 0; j < nB; j++) {
                    char txt[256];
                    DbfEngine_GetFieldText(other, j, txt, (int)sizeof(txt));
                    DbfFieldWriter_Write(payload + off, merged[nA + j].Length, &merged[nA + j], txt, dest->memoFile);
                    off += merged[nA + j].Length;
                }
            }
            DbfEngine_AppendRecord(dest, payload, payloadLen);
            DbfEngine_SkipInternal(other, 1);
        }
        DbfEngine_SkipInternal(self, 1);
    }
    free(payload);
    DbfEngine_Dispose(dest);
    return GMB_DB_OK;
}

int DbfEngine_ValidateIntegrity(DbfEngine* self)
{
    const DbfHeader* hdr;
    int64_t fileLen, physicalCount;
    int i;
    if (!self || !self->dbfFile) return GMB_DB_ERR;
    hdr = DbfFile_get_Header(self->dbfFile);
    if (!hdr) return GMB_DB_ERR;
    /* 1. Coerenza record fisici vs header (parita' C# ValidateIntegrity). */
    fileLen = DbfFile_GetFileLength(self->dbfFile);
    if (hdr->RecordLength <= 0 || hdr->HeaderLength <= 0) return GMB_DB_ERR;
    physicalCount = (fileLen - hdr->HeaderLength) / hdr->RecordLength;
    if (physicalCount != (int64_t)hdr->NumberOfRecords) return GMB_DB_ERR;
    /* 2. Sanity indici: le entry non devono superare i record del file. */
    for (i = 0; i < self->indexCount; i++) {
        if (IndexHandle_GetEntryCount(&self->indexes[i]) > (int64_t)hdr->NumberOfRecords)
            return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

int DbfEngine_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/engsc_XXXXXX";
    char dbf[PATH_MAX], ntx[PATH_MAX], ndx[PATH_MAX], fpt[PATH_MAX];
    char copy[PATH_MAX], sorted[PATH_MAX], wal[PATH_MAX], text[32];
    DbfField fields[2];
    DbfEngine* e;
    DbfEngine* reopen;
    DbfEngine* dest;
    const char* idxPaths[1];
    MemoData md;
    const uint8_t hello[] = "hello";
    uint8_t key[10];
    int nFields = 0, memoIdx;
    const DbfHeader* hdr;
    WalLogSummary sum;

    if (DbfFieldWriter_SelfCheck() != GMB_DB_OK) return -50;
    if (DbfFieldParser_SelfCheck() != GMB_DB_OK) return -51;
    if (VersioningManager_SelfCheck() != GMB_DB_OK) return -70;

    if (!mkdtemp(dir)) return -2;

    snprintf(dbf, sizeof(dbf), "%s/t.dbf", dir);
    snprintf(ntx, sizeof(ntx), "%s/byname.ntx", dir);
    snprintf(ndx, sizeof(ndx), "%s/id.ndx", dir);
    snprintf(fpt, sizeof(fpt), "%s/t.fpt", dir);
    copy[0] = sorted[0] = wal[0] = 0;
    fields[0] = DbfField_Create("NAME", (uint8_t)DbfFieldType_Character, 10, 0);
    fields[1] = DbfField_Create("NOTES", (uint8_t)DbfFieldType_Memo, 10, 0);
    if (DbfFile_CreateNew(dbf, "windows-1252", fields, 2) != GMB_DB_OK) return -3;

    e = DbfEngine_New(dbf, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!e) return -4;
    hdr = DbfEngine_GetHeader(e);
    if (!hdr || hdr->NumberOfRecords != 0) {
        DbfEngine_Dispose(e);
        return -5;
    }
    if (!DbfEngine_GetFields(e, &nFields) || nFields != 2) {
        DbfEngine_Dispose(e);
        return -6;
    }
    if (MemoFile_IsNull(DbfEngine_get_MemoFile(e))) {
        DbfEngine_Dispose(e);
        return -7;
    }
    if (access(fpt, F_OK) != 0) {
        DbfEngine_Dispose(e);
        return -8;
    }
    memoIdx = MemoFile_WriteMemo(DbfEngine_get_MemoFile(e), hello, 5, 1);
    if (memoIdx != 1) {
        DbfEngine_Dispose(e);
        return -9;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(DbfEngine_get_MemoFile(e), 1, &md) != GMB_DB_OK || md.Length != 5
        || memcmp(md.Data, hello, 5) != 0) {
        MemoData_Dispose(&md);
        DbfEngine_Dispose(e);
        return -10;
    }
    MemoData_Dispose(&md);

    if (eng_sc_write_rec(e, "BOB       ") != GMB_DB_OK || eng_sc_write_rec(e, "ALICE     ") != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -11;
    }
    if (DbfEngine_GetTotalRecordCount(e) != 2) {
        DbfEngine_Dispose(e);
        return -12;
    }
    if (DbfEngine_IndexOn(e, ntx, "NAME", 10, 0, NULL, NULL, NULL) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -13;
    }
    if (DbfEngine_GetIndexCount(e) != 1
        || DbfEngine_GetIndex(e, 0)->Kind != IndexHandleKind_ClipperNtxWritable) {
        DbfEngine_Dispose(e);
        return -14;
    }
    memset(key, ' ', 10);
    memcpy(key, "ALICE", 5);
    if (DbfEngine_SetActiveOrderNumber(e, 1) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -15;
    }
    if (DbfEngine_Seek(e, key, 10) != GMB_DB_OK || !DbfEngine_get_Found(e)
        || DbfEngine_GetCurrentRecordNumber(e) != 2) {
        DbfEngine_Dispose(e);
        return -16;
    }
    if (DbfEngine_IndexOn(e, ndx, "NAME", 10, 0, NULL, NULL, NULL) != GMB_DB_OK
        || DbfEngine_GetIndexCount(e) != 2) {
        DbfEngine_Dispose(e);
        return -17;
    }
    if (DbfEngine_SetActiveOrderName(e, "byname") != GMB_DB_OK
        || DbfEngine_SetActiveOrderName(e, "id") != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -18;
    }
    if (DbfEngine_SetActiveOrderName(e, "byname") != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -19;
    }
    DbfEngine_GoTop(e);
    if (DbfEngine_GetCurrentRecordNumber(e) != 2) {
        DbfEngine_Dispose(e);
        return -20;
    }
    DbfEngine_Skip(e, 1);
    if (DbfEngine_GetCurrentRecordNumber(e) != 1 || DbfEngine_get_Eof(e)) {
        DbfEngine_Dispose(e);
        return -41;
    }
    DbfEngine_Dispose(e);

    idxPaths[0] = ntx;
    reopen = DbfEngine_New(dbf, idxPaths, 1, 0, LockingStrategy_IntraProcess);
    if (!reopen) return -21;
    if (MemoFile_IsNull(DbfEngine_get_MemoFile(reopen))) {
        DbfEngine_Dispose(reopen);
        return -22;
    }
    if (DbfEngine_GetIndexCount(reopen) != 1) {
        DbfEngine_Dispose(reopen);
        return -23;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(DbfEngine_get_MemoFile(reopen), 1, &md) != GMB_DB_OK || md.Length != 5) {
        MemoData_Dispose(&md);
        DbfEngine_Dispose(reopen);
        return -24;
    }
    MemoData_Dispose(&md);
    memset(key, ' ', 10);
    memcpy(key, "BOB", 3);
    if (DbfEngine_SetActiveOrderNumber(reopen, 1) != GMB_DB_OK
        || DbfEngine_Seek(reopen, key, 10) != GMB_DB_OK || !DbfEngine_get_Found(reopen)
        || DbfEngine_GetCurrentRecordNumber(reopen) != 1) {
        DbfEngine_Dispose(reopen);
        return -25;
    }
    DbfEngine_Dispose(reopen);

    if (DbfEngine_New("/no/such/file.dbf", NULL, 0, 0, LockingStrategy_IntraProcess) != NULL)
        return -26;

    /* GO/SKIP fisico, DELETE, SET DELETED, PACK, ZAP */
    e = DbfEngine_New(dbf, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!e) return -27;
    DbfEngine_GoTop(e);
    if (DbfEngine_GetCurrentRecordNumber(e) != 1) {
        DbfEngine_Dispose(e);
        return -28;
    }
    DbfEngine_Skip(e, 1);
    if (DbfEngine_GetCurrentRecordNumber(e) != 2 || DbfEngine_get_Eof(e)) {
        DbfEngine_Dispose(e);
        return -29;
    }
    DbfEngine_GoBottom(e);
    if (DbfEngine_GetCurrentRecordNumber(e) != 2) {
        DbfEngine_Dispose(e);
        return -30;
    }
    DbfEngine_GoTop(e);
    DbfEngine_Skip(e, 1);
    if (DbfEngine_DeleteCurrent(e) != GMB_DB_OK || !DbfEngine_IsCurrentRecordDeleted(e)) {
        DbfEngine_Dispose(e);
        return -31;
    }
    DbfEngine_SetDeleted(e, 1);
    if (DbfEngine_GetCurrentRecordNumber(e) != 1) {
        DbfEngine_Dispose(e);
        return -32;
    }
    DbfEngine_SkipInternal(e, 1);
    if (!DbfEngine_get_Eof(e)) {
        DbfEngine_Dispose(e);
        return -33;
    }
    DbfEngine_SetDeleted(e, 0);
    DbfEngine_GoTop(e);
    DbfEngine_Skip(e, 1);
    if (DbfEngine_RecallCurrent(e) != GMB_DB_OK || DbfEngine_IsCurrentRecordDeleted(e)) {
        DbfEngine_Dispose(e);
        return -34;
    }
    DbfEngine_GoTop(e);
    DbfEngine_Skip(e, 1);
    if (DbfEngine_DeleteCurrent(e) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -35;
    }
    if (DbfEngine_IndexOn(e, ntx, "NAME", 10, 0, NULL, NULL, NULL) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -36;
    }
    if (DbfEngine_Pack(e) != GMB_DB_OK || DbfEngine_GetTotalRecordCount(e) != 1) {
        DbfEngine_Dispose(e);
        return -37;
    }
    DbfEngine_GoTop(e);
    if (DbfEngine_GetCurrentRecordNumber(e) != 1) {
        DbfEngine_Dispose(e);
        return -38;
    }
    {
        uint8_t pay[20];
        memset(pay, ' ', sizeof(pay));
        memcpy(pay, "CAROL     ", 10);
        if (DbfEngine_AppendRecord(e, pay, (int)(DbfFile_get_Header(e->dbfFile)->RecordLength - 1)) != GMB_DB_OK
            || DbfEngine_GetTotalRecordCount(e) != 2) {
            DbfEngine_Dispose(e);
            return -39;
        }
    }
    DbfEngine_GoTop(e);
    DbfEngine_Skip(e, 1);
    if (DbfEngine_ReplaceFieldName(e, "NAME", "DAVE") != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -52;
    }
    if (DbfEngine_GetFieldTextByName(e, "NAME", text, sizeof(text)) != GMB_DB_OK || strcmp(text, "DAVE") != 0) {
        DbfEngine_Dispose(e);
        return -53;
    }
    memset(key, ' ', 10);
    memcpy(key, "DAVE", 4);
    if (DbfEngine_SetActiveOrderNumber(e, 1) != GMB_DB_OK
        || DbfEngine_Seek(e, key, 10) != GMB_DB_OK || !DbfEngine_get_Found(e)) {
        DbfEngine_Dispose(e);
        return -54;
    }
    snprintf(copy, sizeof(copy), "%s/copy.dbf", dir);
    snprintf(sorted, sizeof(sorted), "%s/sort.dbf", dir);
    if (DbfEngine_CopyTo(e, copy) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -55;
    }
    dest = DbfEngine_New(copy, NULL, 0, 0, LockingStrategy_IntraProcess);
    if (!dest || DbfEngine_GetTotalRecordCount(dest) != 2) {
        if (dest) DbfEngine_Dispose(dest);
        DbfEngine_Dispose(e);
        return -56;
    }
    DbfEngine_Dispose(dest);
    if (DbfEngine_SortTo(e, sorted, "NAME", 0) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -57;
    }
    dest = DbfEngine_New(sorted, NULL, 0, 0, LockingStrategy_IntraProcess);
    if (!dest) {
        DbfEngine_Dispose(e);
        return -58;
    }
    DbfEngine_GoTop(dest);
    if (DbfEngine_GetFieldText(dest, 0, text, sizeof(text)) != GMB_DB_OK || strcmp(text, "BOB") != 0) {
        DbfEngine_Dispose(dest);
        DbfEngine_Dispose(e);
        return -59;
    }
    DbfEngine_Dispose(dest);
    if (DbfEngine_BeginTransaction(e) != GMB_DB_OK || DbfEngine_AppendBlankRecord(e) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -60;
    }
    snprintf(wal, sizeof(wal), "%s/t.wal", dir);
    if (WriteAheadLog_Inspect(dbf, &sum) != GMB_DB_OK || !sum.Exists || !sum.InOpenTransaction) {
        DbfEngine_Dispose(e);
        return -61;
    }
    if (DbfEngine_Commit(e) != GMB_DB_OK || DbfEngine_GetTotalRecordCount(e) != 3) {
        DbfEngine_Dispose(e);
        return -62;
    }
    if (DbfEngine_Zap(e) != GMB_DB_OK || DbfEngine_GetTotalRecordCount(e) != 0 || !DbfEngine_get_Eof(e)) {
        DbfEngine_Dispose(e);
        return -40;
    }
    DbfEngine_Dispose(e);

    {
        char sales[PATH_MAX], tot[PATH_MAX], csvp[PATH_MAX], src2[PATH_MAX], replp[PATH_MAX], sntx[PATH_MAX];
        DbfField sf[2];
        DbfEngine *s, *t, *app;
        const char* sums[1];
        uint8_t pay[16];
        char line[256];
        FILE* lf;
        snprintf(sales, sizeof(sales), "%s/sales.dbf", dir);
        snprintf(tot, sizeof(tot), "%s/tot.dbf", dir);
        snprintf(csvp, sizeof(csvp), "%s/sales.csv", dir);
        snprintf(src2, sizeof(src2), "%s/src2.dbf", dir);
        snprintf(sntx, sizeof(sntx), "%s/sales.ntx", dir);
        sf[0] = DbfField_Create("NAME", (uint8_t)DbfFieldType_Character, 10, 0);
        sf[1] = DbfField_Create("QTY", (uint8_t)DbfFieldType_Numeric, 6, 0);
        if (DbfFile_CreateNew(sales, "windows-1252", sf, 2) != GMB_DB_OK) return -71;
        s = DbfEngine_New(sales, NULL, 0, 1, LockingStrategy_IntraProcess);
        if (!s) return -72;
        memset(pay, ' ', sizeof(pay));
        memcpy(pay, "AAA       ", 10);
        memcpy(pay + 10, "    10", 6);
        if (DbfEngine_AppendRecord(s, pay, (int)(DbfFile_get_Header(s->dbfFile)->RecordLength - 1)) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -73;
        }
        memcpy(pay, "BBB       ", 10);
        memcpy(pay + 10, "     5", 6);
        DbfEngine_AppendRecord(s, pay, (int)(DbfFile_get_Header(s->dbfFile)->RecordLength - 1));
        memcpy(pay, "AAA       ", 10);
        memcpy(pay + 10, "     3", 6);
        DbfEngine_AppendRecord(s, pay, (int)(DbfFile_get_Header(s->dbfFile)->RecordLength - 1));
        if (DbfEngine_IndexOn(s, sntx, "NAME", 10, 0, NULL, NULL, NULL) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -74;
        }
        if (DbfEngine_SetFilter(s, "NAME='BBB'") != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -75;
        }
        if (DbfEngine_GetFieldText(s, 0, text, sizeof(text)) != GMB_DB_OK || strcmp(text, "BBB") != 0) {
            DbfEngine_Dispose(s);
            return -76;
        }
        DbfEngine_SkipInternal(s, 1);
        if (!DbfEngine_get_Eof(s)) {
            DbfEngine_Dispose(s);
            return -77;
        }
        DbfEngine_SetFilter(s, NULL);
        sums[0] = "QTY";
        if (DbfEngine_TotalOn(s, tot, "NAME", sums, 1) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -78;
        }
        t = DbfEngine_New(tot, NULL, 0, 0, LockingStrategy_IntraProcess);
        if (!t) {
            DbfEngine_Dispose(s);
            return -79;
        }
        DbfEngine_GoTop(t);
        if (DbfEngine_GetFieldText(t, 0, text, sizeof(text)) != GMB_DB_OK || strcmp(text, "AAA") != 0) {
            DbfEngine_Dispose(t);
            DbfEngine_Dispose(s);
            return -80;
        }
        if (DbfEngine_GetFieldText(t, 1, text, sizeof(text)) != GMB_DB_OK || strtod(text, NULL) != 13.0) {
            DbfEngine_Dispose(t);
            DbfEngine_Dispose(s);
            return -81;
        }
        DbfEngine_Dispose(t);
        if (DbfEngine_ExportToCsv(s, csvp) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -82;
        }
        if (DbfFile_CloneStructure(s->dbfFile, src2) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -83;
        }
        app = DbfEngine_New(src2, NULL, 0, 1, LockingStrategy_IntraProcess);
        if (!app || DbfEngine_AppendFrom(app, sales) != GMB_DB_OK || DbfEngine_GetTotalRecordCount(app) != 3) {
            if (app) DbfEngine_Dispose(app);
            DbfEngine_Dispose(s);
            return -84;
        }
        DbfEngine_Dispose(app);
        if (DbfEngine_SetReplication(s, "DEFAULT") != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -85;
        }
        memcpy(pay, "CCC       ", 10);
        memcpy(pay + 10, "     1", 6);
        if (DbfEngine_AppendRecord(s, pay, (int)(DbfFile_get_Header(s->dbfFile)->RecordLength - 1)) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -86;
        }
        snprintf(replp, sizeof(replp), "%s.repl", sales);
        lf = fopen(replp, "r");
        if (!lf) {
            DbfEngine_Dispose(s);
            return -87;
        }
        if (!fgets(line, (int)sizeof(line), lf) || strncmp(line, "APPEND\t", 7) != 0) {
            fclose(lf);
            DbfEngine_Dispose(s);
            return -88;
        }
        fclose(lf);
        {
            char jsonp[PATH_MAX], destp[PATH_MAX], ckpt[PATH_MAX], shipck[PATH_MAX];
            DbfEngine *imp, *fol, *recv;
            pthread_t th;
            int port, shipped, recvd;
            snprintf(jsonp, sizeof(jsonp), "%s/sales.json", dir);
            snprintf(destp, sizeof(destp), "%s/imp.dbf", dir);
            if (DbfEngine_ExportToJson(s, jsonp) != GMB_DB_OK) {
                DbfEngine_Dispose(s);
                return -89;
            }
            if (DbfFile_CloneStructure(s->dbfFile, destp) != GMB_DB_OK) {
                DbfEngine_Dispose(s);
                return -90;
            }
            imp = DbfEngine_New(destp, NULL, 0, 1, LockingStrategy_IntraProcess);
            if (!imp || DbfEngine_ImportFromJson(imp, jsonp) != GMB_DB_OK ||
                DbfEngine_GetTotalRecordCount(imp) != DbfEngine_GetTotalRecordCount(s)) {
                if (imp) DbfEngine_Dispose(imp);
                DbfEngine_Dispose(s);
                return -91;
            }
            DbfEngine_Dispose(imp);
            snprintf(destp, sizeof(destp), "%s/fol.dbf", dir);
            if (DbfFile_CloneStructure(s->dbfFile, destp) != GMB_DB_OK) {
                DbfEngine_Dispose(s);
                return -92;
            }
            fol = DbfEngine_New(destp, NULL, 0, 1, LockingStrategy_IntraProcess);
            snprintf(ckpt, sizeof(ckpt), "%s.ckpt", replp);
            if (!fol || DbfEngine_FollowReplication(fol, replp, ckpt) < 1) {
                if (fol) DbfEngine_Dispose(fol);
                DbfEngine_Dispose(s);
                return -93;
            }
            if (DbfEngine_FollowReplication(fol, replp, ckpt) != 0) {
                DbfEngine_Dispose(fol);
                DbfEngine_Dispose(s);
                return -94;
            }
            DbfEngine_Dispose(fol);
            snprintf(destp, sizeof(destp), "%s/tcp.dbf", dir);
            if (DbfFile_CloneStructure(s->dbfFile, destp) != GMB_DB_OK) {
                DbfEngine_Dispose(s);
                return -95;
            }
            recv = DbfEngine_New(destp, NULL, 0, 1, LockingStrategy_IntraProcess);
            if (!recv) {
                DbfEngine_Dispose(s);
                return -96;
            }
            port = ReplicationTcpProtocol_AllocateLoopbackPort();
            if (port <= 0) {
                DbfEngine_Dispose(recv);
                DbfEngine_Dispose(s);
                return -97;
            }
            {
                EngRecvJob* job = (EngRecvJob*)malloc(sizeof(EngRecvJob));
                if (!job) {
                    DbfEngine_Dispose(recv);
                    DbfEngine_Dispose(s);
                    return -98;
                }
                job->e = recv;
                job->port = port;
                job->applied = -1;
                if (pthread_create(&th, NULL, eng_sc_recv, job) != 0) {
                    free(job);
                    DbfEngine_Dispose(recv);
                    DbfEngine_Dispose(s);
                    return -99;
                }
                usleep(80000);
                snprintf(shipck, sizeof(shipck), "%s.ship.ckpt", replp);
                shipped = DbfEngine_ReplicationShipTcp(s, "127.0.0.1", port, replp, shipck);
                pthread_join(th, NULL);
                recvd = job->applied;
                free(job);
                if (shipped < 1 || recvd < 1) {
                    DbfEngine_Dispose(recv);
                    DbfEngine_Dispose(s);
                    return -100;
                }
            }
            DbfEngine_Dispose(recv);
            eng_sc_rm(jsonp);
            eng_sc_rm(ckpt);
            eng_sc_rm(shipck);
            {
                char extra[PATH_MAX];
                snprintf(extra, sizeof(extra), "%s/imp.dbf", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/imp.fpt", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/fol.dbf", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/fol.fpt", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/tcp.dbf", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/tcp.fpt", dir);
                eng_sc_rm(extra);
            }
        }
        DbfEngine_Dispose(s);
        eng_sc_rm(sales);
        eng_sc_rm(tot);
        eng_sc_rm(csvp);
        eng_sc_rm(src2);
        eng_sc_rm(replp);
        eng_sc_rm(sntx);
        {
            char salesFpt[PATH_MAX];
            snprintf(salesFpt, sizeof(salesFpt), "%s/sales.fpt", dir);
            eng_sc_rm(salesFpt);
            snprintf(salesFpt, sizeof(salesFpt), "%s/tot.fpt", dir);
            eng_sc_rm(salesFpt);
            snprintf(salesFpt, sizeof(salesFpt), "%s/src2.fpt", dir);
            eng_sc_rm(salesFpt);
        }
    }

    eng_sc_rm(ntx);
    eng_sc_rm(ndx);
    eng_sc_rm(fpt);
    eng_sc_rm(dbf);
    eng_sc_rm(copy);
    eng_sc_rm(sorted);
    eng_sc_rm(wal);
    {
        char copyFpt[PATH_MAX], sortFpt[PATH_MAX];
        snprintf(copyFpt, sizeof(copyFpt), "%s/copy.fpt", dir);
        snprintf(sortFpt, sizeof(sortFpt), "%s/sort.fpt", dir);
        eng_sc_rm(copyFpt);
        eng_sc_rm(sortFpt);
    }
    rmdir(dir);
    {
        int rdd = DbfDriver_RddSelfCheck();
        if (rdd != GMB_DB_OK) return rdd;
    }
    return GMB_DB_OK;
}

/* JOIN/RELATION: DbfDriver_SetRelation / SyncRelation / NestedLoopJoin (non DbfEngine, non DataSession). */
