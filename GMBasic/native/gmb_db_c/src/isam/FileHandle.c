/* FILE: native/gmb_db_c/src/ISAM/FileHandle.c */
/* Port 1:1 di modules/Database/ISAM/FileHandle.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.FileHandle.cs
//
//using System.Collections.Concurrent;
//using System.Buffers;
//using System.IO.MemoryMappedFiles;
//using System.Runtime.InteropServices;
//using System.Text;
//using Basic.ISAM;
//using Basic.Databases;
//
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.ISAM;
//
///// <summary>
///// Definisce lo stato di una transazione attiva.
///// </summary>
//internal enum TransactionStatus
//{
//    Active,
//    Committed,
//    RolledBack
//}
//
///// <summary>
///// Gestore centralizzato per lo stato di tutte le transazioni operanti su un file.
///// Assegna ID univoci e tiene traccia delle transazioni attive, commesse e annullate,
///// fornendo la logica di visibilità per l'implementazione MVCC.
///// </summary>
//internal class TransactionManager
//{
//    private long _nextTransactionId = 1;
//    private readonly ConcurrentDictionary<long, TransactionStatus> _transactionStates = new();
//    
//    /// <summary>
//    /// Avvia una nuova transazione, assegnandole un ID univoco e registrandola come attiva.
//    /// </summary>
//    /// <returns>L'ID della nuova transazione.</returns>
//    public long BeginTransaction()
//    {
//        var txId = Interlocked.Increment(ref _nextTransactionId);
//        _transactionStates[txId] = TransactionStatus.Active;
//        return txId;
//    }
//
//    /// <summary>
//    /// Marca una transazione come commessa.
//    /// </summary>
//    public void CommitTransaction(long txId)
//    {
//        _transactionStates[txId] = TransactionStatus.Committed;
//    }
//
//    /// <summary>
//    /// Marca una transazione come annullata.
//    /// </summary>
//    public void RollbackTransaction(long txId)
//    {
//        _transactionStates[txId] = TransactionStatus.RolledBack;
//    }
//
//    /// <summary>
//    /// Determina se la versione di un record, creata da una certa transazione,
//    /// è visibile alla transazione corrente, secondo le regole di "Read Committed".
//    /// </summary>
//    /// <param name="recordTransactionId">L'ID della transazione che ha scritto la versione del record.</param>
//    /// <param name="currentTransactionId">L'ID della transazione che sta tentando di leggere il record.</param>
//    /// <returns>True se la versione del record è visibile.</returns>
//    public bool IsVersionVisible(long recordTransactionId, long currentTransactionId)
//    {
//        // Un record è sempre visibile alla transazione che lo ha creato.
//        if (recordTransactionId == currentTransactionId)
//        {
//            return true;
//        }
//
//        // Altrimenti, il record è visibile solo se la transazione che lo ha creato
//        // è stata commessa.
//        if (_transactionStates.TryGetValue(recordTransactionId, out var status))
//        {
//            return status == TransactionStatus.Committed;
//        }
//
//        // Se non troviamo la transazione nella mappa, significa che è una transazione
//        // "vecchia" (avvenuta prima dell'apertura di questo handle) e quindi è commessa.
//        return true;
//    }
//}
//
///// <summary>
///// Rappresenta un file ISAM aperto e la sua area di lavoro associata (Work Area).
///// Questa classe è stateful e gestisce il puntatore al record corrente, i flag di stato
///// (EOF, BOF, FOUND) e i meccanismi di locking per la concorrenza.
///// </summary>
//public class IsamFileHandle : IDisposable // Rimosso ITransactionalFileHandle
//{
//    #region Properties
//
//    /// <summary>
//    /// La strategia di locking (Intra-Processo o Cross-Process) in uso da questo handle.
//    /// Determina il comportamento della cache dei nodi B+Tree per garantire la coerenza.
//    /// </summary>
//    public readonly LockingStrategy LockingStrategy;
//    
//    /// <summary>
//    /// Il percorso completo e normalizzato del file di database ISAM (.isdb) gestito da questo handle.
//    /// </summary>
//    public readonly string FilePath;
//
//    /// <summary>
//    /// Backing store (disk <see cref="FileStream"/> or shared <see cref="IsamMemStream"/> for <c>MEM:</c> tables).
//    /// </summary>
//    private readonly Stream _backingStream;
//
//    /// <summary>Disk stream when not <c>MEM:</c>-backed; used for mmap read-only (ISAM-C3).</summary>
//    private readonly FileStream? _diskStream;
//
//    /// <summary>True when <see cref="FilePath"/> uses the <see cref="IsamMemPath.Prefix"/> scheme.</summary>
//    private readonly bool _isMemBacked;
//
//    /// <summary>
//    /// Il gestore di lock lettura/scrittura (shared/exclusive) per questo file,
//    /// la cui strategia (intra-processo o multi-processo) è determinata alla creazione dell'handle.
//    /// </summary>
//    public readonly ILockManager RecordLockManager;
//
//    /// <summary>
//    /// Un lock a livello di file (<see cref="ReaderWriterLockSlim"/>) utilizzato per proteggere le operazioni
//    /// che modificano la struttura globale del file (es. aggiornamento dell'header, allocazione di nuove pagine, PACK).
//    /// Permette letture multiple concorrenti ma richiede un accesso esclusivo per le scritture strutturali.
//    /// </summary>
//    private readonly ReaderWriterLockSlim _fileLevelLock = new(LockRecursionPolicy.SupportsRecursion);
//
//    private const string FileStructureLockKey = "__FILE_STRUCT__";
//
//    private int _fileLockDepth;
//    private int _fileStructureReadLockDepth;
//
//    /// <summary>
//    /// L'istanza del <see cref="WriteAheadLog"/> che gestisce la registrazione di tutte le operazioni di modifica
//    /// prima che vengano scritte sul file principale, garantendo atomicità e durabilità.
//    /// </summary>
//    private WriteAheadLog? _wal;
//
//    /// <summary>
//    /// Gestore centralizzato che traccia lo stato (attivo, commesso, annullato) di ogni transazione.
//    /// </summary>
//    private readonly TransactionManager _transactionManager = new();
//
//    /// <summary>
//    /// Mappa l'ID del thread corrente all'ID della sua transazione attiva.
//    /// Questo permette a ogni thread di avere la propria transazione isolata.
//    /// </summary>
//    private readonly ConcurrentDictionary<int, long> _threadTransactionMap = new();
//
//    /// <summary>
//    /// Mappa in memoria dei nomi degli indici secondari ai loro offset di radice del B+Tree.
//    /// </summary>
//    private Dictionary<string, long> _secondaryIndexMap;
//
//    private Config.FileHeader? _transactionSnapshotHeader;
//    private byte[]? _transactionSnapshotIndexMap;
//
//    /// <summary>
//    /// Offset del primo byte dell'area heap (LOB + pagine B+). -1 se non ancora allocata.
//    /// </summary>
//    private long _heapRegionStart = -1;
//
//    private bool _headerDirty;
//    private int _bulkLoadDepth;
//    private bool _directWalDirty;
//    private readonly List<(long Offset, byte[] Data)> _pendingDirectMainWrites = new();
//    private Mutex? _crossProcessRecoverMutex;
//
//    /// <summary>
//    /// Serializza lifecycle WAL e scritture su <see cref="_backingStream"/> tra thread (ISAM-B9).
//    /// Acquisito prima di <see cref="LockFile"/> per evitare deadlock structural vs Commit.
//    /// </summary>
//    private readonly object _physicalIoLock = new();
//    private int _physicalIoLockDepth;
//    private int _physicalIoLockOwnerThreadId = -1;
//    private bool _ownsCrossProcessRecoverMutex;
//
//    /// <summary>
//    /// Modalità di durabilità I/O. Default <see cref="IsamDurabilityMode.Full"/>.
//    /// </summary>
//    public IsamDurabilityMode DurabilityMode { get; set; } = IsamDurabilityMode.Full;
//
//    /// <summary>
//    /// Timeout (ms) per lock su record e chiavi indice. Default 5000.
//    /// </summary>
//    public int LockTimeoutMilliseconds { get; set; } = 5000;
//
//    internal bool IsBulkLoadActive => _bulkLoadDepth > 0;
//
//    /// <summary>Cache pagine 4 KiB (ISAM-C1); null in cross-process.</summary>
//    internal IsamPageCache? ReadPageCache { get; private set; }
//
//    private MemoryMappedFile? _mmapFile;
//    private MemoryMappedViewAccessor? _mmapView;
//
//    /// <summary>ISAM-C3: letture via mmap; scritture sempre su stream.</summary>
//    public bool UseMmapReadOnly { get; private set; }
//
//    /// <summary>
//    /// Invocato dopo lo spostamento fisico dell'area heap per aggiornare puntatori negli indici B+.
//    /// Parametri: heapStart, heapEnd, delta.
//    /// </summary>
//    internal Action<long, long, long>? AfterHeapRelocated;
//
//    /// <summary>
//    /// La definizione del tipo definito dall'utente (<see cref="UserDefinedType"/>) ottenuta dal parser.
//    /// È essenziale per il <see cref="RecordMarshaller"/> per deserializzare correttamente i dati grezzi
//    /// in un'istanza di <see cref="UdtInstance"/>.
//    /// </summary>
//    public UserDefinedType UdtDefinition { get; }
//
//    /// <summary>
//    /// La copia in memoria dell'header del file (<see cref="Config.FileHeader"/>). Contiene metadati critici come
//    /// puntatori alle radici degli indici, dimensione dei record e puntatori alle liste di spazio libero.
//    /// Viene mantenuta sincronizzata con la versione su disco.
//    /// </summary>
//    public Config.FileHeader Header { get; private set; }
//
//    /// <summary>
//    /// Offset fisico dove inizia l'area record (dopo header, schema e mappa indici secondari).
//    /// </summary>
//    public long DataAreaStartOffset =>
//        Header.SecondaryIndexMapOffset > 0
//            ? Header.SecondaryIndexMapOffset + SchemaManager.SerializeIndexMap(_secondaryIndexMap).Length
//            : Header.SchemaOffset + Header.SchemaLength;
//
//    /// <summary>
//    /// La copia in memoria dello schema del database (<see cref="Config.IsamSchema"/>), che descrive la struttura logica
//    /// dei record, inclusi i nomi dei campi, i tipi di dato e gli indici definiti.
//    /// </summary>
//    public Config.IsamSchema Schema { get; private set; }
//
//    /// <summary>
//    /// Deprecato (ISAM-B9): offset cursore per-thread su <see cref="Engine.CurrentRecordOffset"/>.
//    /// </summary>
//    [Obsolete("Usare Engine.CurrentRecordOffset — stato navigazione per thread.")]
//    public long CurrentRecordOffset { get; set; } = -1;
//
//    /// <summary>
//    /// Deprecato (ISAM-B9): EOF per-thread su <see cref="Engine"/>.
//    /// </summary>
//    [Obsolete("Stato EOF spostato sul cursore per-thread dell'engine.")]
//    public bool EofFlag { get; set; }
//
//    /// <summary>
//    /// Deprecato (ISAM-B9): BOF per-thread su <see cref="Engine"/>.
//    /// </summary>
//    [Obsolete("Stato BOF spostato sul cursore per-thread dell'engine.")]
//    public bool BofFlag { get; set; }
//
//    /// <summary>
//    /// Flag booleano che indica se l'ultima operazione di ricerca (es. `SEEK`) ha trovato una corrispondenza esatta.
//    /// </summary>
//    public bool FoundFlag { get; set; }
//
//    /// <summary>
//    /// L'encoding dei caratteri utilizzato per leggere e scrivere dati testuali (es. stringhe, JSON) nel file.
//    /// Questo può essere configurato all'apertura del file.
//    /// </summary>
//    public Encoding FileEncoding { get; }
//
//    #endregion
//
//    /// <summary>
//    /// Inizializza una nuova istanza della classe <see cref="IsamFileHandle"/>, che rappresenta una work area
//    /// per un file ISAM. L'operazione crea il file fisico se non esiste, esegue il recovery da un eventuale
//    /// file WAL residuo e carica l'header e lo schema del database.
//    /// </summary>
//    /// <param name="filePath">Il percorso completo del file di database (.isdb) da aprire o creare.</param>
//    /// <param name="schemaFromCode">L'oggetto <see cref="Config.IsamSchema"/> derivato dalla definizione del `TYPE` nel codice sorgente.
//    /// Verrà usato per validare la struttura del file esistente o per creare un nuovo file.</param>
//    /// <param name="udtDefinition">La definizione del tipo definito dall'utente (<see cref="UserDefinedType"/>), necessaria per
//    /// la deserializzazione dei record nei tipi corretti.</param>
//    /// <param name="lockingStrategy">La strategia di locking da utilizzare (Intra-Processo per performance o Cross-Process per file sharing).</param>
//    /// <param name="fileEncoding">L'encoding da utilizzare per il file. Se nullo, viene usato Encoding.UTF8.</param>
//    /// <remarks>
//    /// Il costruttore orchestra il processo di avvio completo:
//    /// 1. Se il file non esiste, lo crea da zero utilizzando il metodo statico <see cref="CreateNew"/>.
//    /// 2. Esegue un controllo di recovery chiamando <see cref="WriteAheadLog.Recover"/>, garantendo che il database
//    ///    sia in uno stato consistente prima di qualsiasi operazione.
//    /// 3. Apre un <see cref="FileStream"/> sul file, che ora è garantito esistere.
//    /// 4. Inizializza i gestori per il locking e per il Write-Ahead Log.
//    /// 5. Carica l'header e lo schema dal disco e li valida contro lo schema fornito dal codice.
//    /// 6. Imposta l'encoding per tutte le operazioni di I/O testuali.
//    /// </remarks>
//    public IsamFileHandle(string filePath, Config.IsamSchema schemaFromCode, UserDefinedType udtDefinition,
//        LockingStrategy lockingStrategy, Encoding? fileEncoding = null)
//    {
//        FilePath = filePath;
//        UdtDefinition = udtDefinition;
//        LockingStrategy = lockingStrategy;
//        FileEncoding = fileEncoding ?? Encoding.UTF8; // Imposta l'encoding
//        _secondaryIndexMap = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
//        _isMemBacked = IsamMemPath.IsMemPath(FilePath);
//
//        if (_isMemBacked && lockingStrategy == LockingStrategy.CrossProcess)
//        {
//            throw new ArgumentException(
//                "MEM: tables are single-process only; use IntraProcess locking.",
//                nameof(lockingStrategy));
//        }
//
//        if (_isMemBacked)
//        {
//            var memKey = IsamMemPath.NormalizeKey(FilePath);
//            if (!IsamMemStore.Exists(memKey))
//            {
//                CreateNew(FilePath, schemaFromCode, FileEncoding);
//            }
//
//            _backingStream = IsamMemStore.OpenStream(memKey);
//            _diskStream = null;
//        }
//        else
//        {
//            // Se il file non esiste, lo creiamo prima di procedere.
//            if (!File.Exists(FilePath))
//            {
//                CreateNew(FilePath, schemaFromCode, FileEncoding); // Passa l'encoding
//            }
//            else
//            {
//                IsamOfflineMigrator.MigrateIfNeeded(FilePath, schemaFromCode);
//            }
//
//            // A questo punto, il file è garantito esistere.
//            if (lockingStrategy == LockingStrategy.CrossProcess)
//            {
//                var fileId = GetStableFileId(FilePath);
//                _crossProcessRecoverMutex = new Mutex(false, $"GMBASIC_ISAM_RECOVER_{fileId}");
//                _ownsCrossProcessRecoverMutex = _crossProcessRecoverMutex.WaitOne(0);
//                if (_ownsCrossProcessRecoverMutex)
//                {
//                    WriteAheadLog.Recover(FilePath);
//                }
//            }
//            else
//            {
//                WriteAheadLog.Recover(FilePath);
//            }
//
//            _diskStream = OpenFileWithRetry(FilePath, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite);
//            _backingStream = _diskStream;
//        }
//
//        // Istanzia il gestore di lock corretto in base alla strategia scelta.
//        RecordLockManager = lockingStrategy switch
//        {
//            LockingStrategy.IntraProcess => new IntraProcessLockManager(),
//            LockingStrategy.CrossProcess => new CrossProcessLockManager(FilePath),
//            _ => throw new ArgumentOutOfRangeException(nameof(lockingStrategy), "Strategia di locking non supportata.")
//        };
//
//        // Carica e valida l'header e lo schema dal file, che ora esiste sicuramente.
//        (Header, Schema) = LoadAndValidateHeader(schemaFromCode);
//        InitializeHeapRegionTracking();
//        if (lockingStrategy == LockingStrategy.IntraProcess)
//        {
//            ReadPageCache = new IsamPageCache();
//        }
//    }
//
//    /// <summary>Abilita mmap read-only per scan/LoadNode (ISAM-C3). Fallback silenzioso su stream.</summary>
//    internal void EnableMmapReadOnly()
//    {
//        if (UseMmapReadOnly || LockingStrategy != LockingStrategy.IntraProcess || _diskStream == null)
//        {
//            return;
//        }
//
//        try
//        {
//            _mmapFile = MemoryMappedFile.CreateFromFile(
//                _diskStream!,
//                mapName: null,
//                capacity: 0,
//                MemoryMappedFileAccess.Read,
//                HandleInheritability.None,
//                leaveOpen: true);
//            _mmapView = _mmapFile.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);
//            UseMmapReadOnly = true;
//        }
//        catch
//        {
//            DisableMmapReadOnly();
//        }
//    }
//
//    private void DisableMmapReadOnly()
//    {
//        UseMmapReadOnly = false;
//        _mmapView?.Dispose();
//        _mmapView = null;
//        _mmapFile?.Dispose();
//        _mmapFile = null;
//    }
//
//    #region Public API: Stream & Header
//
//    /// <summary>
//    /// Ottiene lo stream sottostante per operazioni di I/O dirette (disco o <c>MEM:</c>).
//    /// </summary>
//    public Stream GetStream() => _backingStream;
//
//    private void FlushBackingStream(bool flushToDisk)
//    {
//        if (_diskStream != null)
//        {
//            _diskStream.Flush(flushToDisk);
//            return;
//        }
//
//        _backingStream.Flush();
//    }
//
//    /// <summary>Flushes the backing store (fsync on disk; no-op for <c>MEM:</c>).</summary>
//    internal void FlushPhysical(bool flushToDisk) => FlushBackingStream(flushToDisk);
//
//    /// <summary>
//    /// Aggiorna l'header del file in modo sicuro, passando dal WAL (versione sincrona).
//    /// </summary>
//    public void UpdateHeader(Config.FileHeader newHeader)
//    {
//        Header = newHeader;
//
//        if (ShouldWriteHeaderToDiskImmediately())
//        {
//            WriteHeaderToDisk(newHeader);
//            _headerDirty = false;
//        }
//        else
//        {
//            _headerDirty = true;
//        }
//    }
//
//    /// <summary>
//    /// Avvia una sessione bulk-load: header differito, nessun WAL direct-write per record/pagine fino a fine sessione.
//    /// </summary>
//    internal void BeginBulkLoadSession() => _bulkLoadDepth++;
//
//    /// <summary>
//    /// Termina una sessione bulk-load e sincronizza header + dati su disco.
//    /// </summary>
//    internal void EndBulkLoadSession()
//    {
//        if (_bulkLoadDepth == 0)
//        {
//            throw new InvalidOperationException("Nessuna sessione bulk-load attiva.");
//        }
//
//        _bulkLoadDepth--;
//        if (_bulkLoadDepth > 0)
//        {
//            return;
//        }
//
//        FlushDurability();
//    }
//
//    /// <summary>
//    /// Scrive header pendente ed esegue fsync su stream (e cleanup WAL se presente).
//    /// </summary>
//    public void FlushDurability()
//    {
//        WithPhysicalIo(() =>
//        {
//            FlushPendingHeader();
//            if (ShouldUseDirectWal())
//            {
//                ApplyPendingDirectMainWrites();
//            }
//
//            SyncDirectWalBeforeMainFile();
//            FlushBackingStream(true);
//            FinalizeDirectWalAfterSync();
//            if (_wal != null)
//            {
//                _wal.Delete();
//                _wal = null;
//            }
//
//            ReloadHeaderFromDisk();
//        });
//    }
//
//    private bool ShouldWriteHeaderToDiskImmediately() =>
//        IsTransactionalWalActive
//        || (DurabilityMode == IsamDurabilityMode.Full && !IsBulkLoadActive);
//
//    private bool _coalesceAppendCountersForPut;
//
//    /// <summary>ISAM-B10: abilita merge contatori append-only durante header write di un PUT cross-process.</summary>
//    internal void BeginCoalesceAppendCountersForPut() => _coalesceAppendCountersForPut = true;
//
//    internal void EndCoalesceAppendCountersForPut() => _coalesceAppendCountersForPut = false;
//
//    private void FlushPendingHeader()
//    {
//        if (!_headerDirty)
//        {
//            return;
//        }
//
//        WriteHeaderToDisk(Header);
//        _headerDirty = false;
//    }
//
//    private void WriteHeaderToDisk(Config.FileHeader newHeader)
//    {
//        newHeader = CoalesceAppendCountersFromDisk(newHeader);
//        Header = newHeader;
//
//        var headerSize = Marshal.SizeOf<Config.FileHeader>();
//        var buffer = new byte[headerSize];
//        var ptr = Marshal.AllocHGlobal(headerSize);
//        try
//        {
//            Marshal.StructureToPtr(newHeader, ptr, true);
//            Marshal.Copy(ptr, buffer, 0, headerSize);
//        }
//        finally
//        {
//            Marshal.FreeHGlobal(ptr);
//        }
//
//        WithPhysicalIo(() =>
//        {
//            EnsureWalIfTransactionalCore();
//            if (IsTransactionalWalActive)
//            {
//                _wal?.LogUpdateHeader(buffer);
//                _backingStream.Seek(0, SeekOrigin.Begin);
//                _backingStream.Write(buffer, 0, buffer.Length);
//            }
//            else if (ShouldUseDirectWal())
//            {
//                if (LockingStrategy == LockingStrategy.CrossProcess)
//                {
//                    LogDirectWriteBeforeMainFile(0, buffer);
//                    _backingStream.Seek(0, SeekOrigin.Begin);
//                    _backingStream.Write(buffer, 0, buffer.Length);
//                    FlushBackingStream(true);
//                }
//                else
//                {
//                    QueueDirectMainWrite(0, buffer);
//                }
//            }
//            else
//            {
//                _backingStream.Seek(0, SeekOrigin.Begin);
//                _backingStream.Write(buffer, 0, buffer.Length);
//                if (LockingStrategy == LockingStrategy.CrossProcess)
//                {
//                    FlushBackingStream(true);
//                }
//            }
//        });
//    }
//
//    private void QueueDirectMainWrite(long offset, byte[] data)
//    {
//        LogDirectWriteBeforeMainFile(offset, data);
//        _pendingDirectMainWrites.Add((offset, data));
//    }
//
//    private void ApplyPendingDirectMainWrites()
//    {
//        if (_pendingDirectMainWrites.Count == 0)
//        {
//            return;
//        }
//
//        foreach (var (offset, data) in _pendingDirectMainWrites)
//        {
//            WriteBytesAt(offset, data);
//            if (offset == 0)
//            {
//                continue;
//            }
//
//            EnsureFileLengthAtLeast(offset + data.Length);
//        }
//
//        _pendingDirectMainWrites.Clear();
//    }
//
//    /// <summary>
//    /// Calcola il prossimo offset disponibile nell'heap append-only condiviso da LOB e pagine B+.
//    /// Garantisce che l'heap inizi oltre la griglia record con allineamento a pagina.
//    /// </summary>
//    internal long GetHeapAllocationOffset()
//    {
//        SyncStructuralStateFromDisk();
//        var minHeap = HeapRelocation.MinimumHeapStart(this);
//        return Math.Max(_backingStream.Length, Math.Max(Header.NextAvailablePageOffset, minHeap));
//    }
//
//    /// <summary>
//    /// Riserva <paramref name="size"/> byte nell'heap condiviso e aggiorna l'header.
//    /// Il chiamante deve già avere acquisito <see cref="LockFile"/> quando richiesto dal contesto.
//    /// </summary>
//    internal long ReserveHeapSpace(long size)
//    {
//        var start = GetHeapAllocationOffset();
//        var requiredLength = start + size;
//        if (_backingStream.Length < requiredLength)
//        {
//            _backingStream.SetLength(requiredLength);
//        }
//
//        var header = Header;
//        header.NextAvailablePageOffset = requiredLength;
//        UpdateHeader(header);
//        RegisterHeapAllocation(start, requiredLength);
//        return start;
//    }
//
//    /// <summary>
//    /// Registra una pagina B+ nel WAL (transazione esplicita o direct-write auto-commit).
//    /// </summary>
//    internal void LogPageWriteIfDurabilityRequired(long offset, byte[] pageData) =>
//        WithPhysicalIo(() => LogPageWriteIfDurabilityRequiredCore(offset, pageData));
//
//    private void LogPageWriteIfDurabilityRequiredCore(long offset, byte[] pageData)
//    {
//        if (IsTransactionalWalActive)
//        {
//            EnsureWalCore();
//            _wal!.LogWriteNode(offset, pageData);
//            return;
//        }
//
//        if (!ShouldUseDirectWal())
//        {
//            return;
//        }
//
//        EnsureWalCore();
//        _wal!.LogDirectWrite(offset, pageData);
//        _directWalDirty = true;
//    }
//
//    private void LogDirectWriteBeforeMainFile(long offset, byte[] data)
//    {
//        EnsureWalCore();
//        _wal!.LogDirectWrite(offset, data);
//        _directWalDirty = true;
//    }
//
//    /// <summary>
//    /// Sincronizza il WAL direct-write su disco prima del fsync del file principale (log-before-write batched).
//    /// </summary>
//    private void SyncDirectWalBeforeMainFile()
//    {
//        if (!_directWalDirty || _wal == null || IsTransactionalWalActive || !ShouldUseDirectWal())
//        {
//            return;
//        }
//
//        _wal.Flush();
//        _directWalDirty = false;
//    }
//
//    private bool ShouldUseDirectWal() =>
//        !_isMemBacked && DurabilityMode == IsamDurabilityMode.Full && !IsBulkLoadActive && !IsTransactionalWalActive;
//
//    /// <summary>
//    /// Ricarica header e mappa indici dopo una sostituzione esterna del file (es. PACK).
//    /// </summary>
//    internal void ReloadAfterExternalReplace(Config.IsamSchema codeSchema)
//    {
//        LockFile();
//        try
//        {
//            FlushBackingStream(true);
//            (Header, Schema) = ReadAndValidateHeaderFromStream(codeSchema);
//            InitializeHeapRegionTracking();
//        }
//        finally
//        {
//            UnlockFile();
//        }
//    }
//
//    /// <summary>
//    /// Writes a replicated ISCH block in-place. ponytail: growth past index-map slot shifts tail on follower.
//    /// </summary>
//    internal void WriteSchemaBlockForReplication(byte[] ischBytes)
//    {
//        ArgumentNullException.ThrowIfNull(ischBytes);
//        if (ischBytes.Length == 0)
//        {
//            throw new ArgumentException("ISCH payload is empty.", nameof(ischBytes));
//        }
//
//        var slotEnd = Header.SecondaryIndexMapOffset > 0
//            ? Header.SecondaryIndexMapOffset
//            : Header.SchemaOffset + Header.SchemaLength;
//        if (Header.SchemaOffset + ischBytes.Length > slotEnd)
//        {
//            EnsureSchemaSlot(ischBytes.Length);
//        }
//
//        LockFile();
//        try
//        {
//            var header = Header;
//            header.SchemaLength = ischBytes.Length;
//            UpdateHeader(header);
//            var stream = GetStream();
//            stream.Seek(header.SchemaOffset, SeekOrigin.Begin);
//            stream.Write(ischBytes, 0, ischBytes.Length);
//            Schema = SchemaManager.DeserializeSchema(ischBytes);
//            Commit();
//        }
//        finally
//        {
//            UnlockFile();
//        }
//    }
//
//    /// <summary>
//    /// Assicura che lo schema possa crescere fino a <paramref name="requiredSchemaBytes"/> senza
//    /// sovrascrivere l'area dati che lo segue: se lo slot corrente (fino a SecondaryIndexMapOffset
//    /// o, in sua assenza, fino a SchemaOffset+SchemaLength) non basta, sposta fisicamente tutto
//    /// ciò che segue (record dati inclusi) in avanti della differenza necessaria. Chiamare SEMPRE
//    /// prima di scrivere uno schema più grande di quello attuale — non solo nel percorso di
//    /// replica: CreateIndexAsync/DropIndexAsync (Engine.cs) aggiungono/rimuovono chiavi secondarie
//    /// allo schema, e DataAreaStartOffset è calcolato come SchemaOffset+SchemaLength quando non
//    /// c'è una mappa indici secondari separata — crescere lo schema senza riservare spazio prima
//    /// sposta silenziosamente in avanti il punto in cui la VM crede inizino i record, mentre i
//    /// byte fisici restano dove sono sempre stati: il primo record (offset più basso) finisce
//    /// letto a metà, con uno status byte casuale (bug scoperto testando DbIsamFullTests.bas: CREATE
//    /// INDEX su una tabella già popolata corrompeva silenziosamente la raggiungibilità del primo
//    /// record dopo un REINDEX).
//    /// </summary>
//    internal void EnsureSchemaSlot(int requiredSchemaBytes)
//    {
//        var slotEnd = Header.SecondaryIndexMapOffset > 0
//            ? Header.SecondaryIndexMapOffset
//            : Header.SchemaOffset + Header.SchemaLength;
//        var slotCapacity = slotEnd - Header.SchemaOffset;
//        if (requiredSchemaBytes <= slotCapacity)
//        {
//            return;
//        }
//
//        var delta = requiredSchemaBytes - slotCapacity;
//        var tailStart = slotEnd;
//        var stream = GetStream();
//        var tailLength = Math.Max(0L, stream.Length - tailStart);
//        if (tailLength > 0)
//        {
//            var tail = new byte[tailLength];
//            stream.Seek(tailStart, SeekOrigin.Begin);
//            stream.ReadExactly(tail);
//            stream.SetLength(stream.Length + delta);
//            stream.Seek(tailStart + delta, SeekOrigin.Begin);
//            stream.Write(tail);
//        }
//        else if (delta > 0)
//        {
//            stream.SetLength(stream.Length + delta);
//        }
//
//        var tailEnd = stream.Length;
//
//        // Lo shift fisico sopra scrive direttamente sullo stream, bypassando WriteBytesAt: senza
//        // invalidare qui, ReadPageCache mantiene pagine "vecchie" (offset pre-shift) che restano
//        // valide finché il B+Tree le serve dalla propria _nodeCache — ma un successivo cache-miss
//        // (es. ResetRoot su ROLLBACK) rilegge da ReadPageCache pagine con contenuto ANTECEDENTE allo
//        // shift, risuscitando puntatori a record ormai spostati (bug scoperto testando
//        // DbIsamFullTests.bas: SEEK dopo ROLLBACK, preceduto da CREATE/DROP INDEX, risolveva offset
//        // dentro il blocco schema invece del record).
//        InvalidatePageCacheForRange(tailStart, (int)Math.Min(int.MaxValue, tailEnd - tailStart));
//
//        ShiftHeaderPagePointers(tailStart, tailEnd, delta);
//        ShiftPageFreeList(tailStart, tailEnd, delta);
//        RewriteLobPointersInRecords(tailStart, tailEnd, delta);
//
//        // Come RelocateHeapRegion: senza questo, i BPlusTreeManager (primario e secondari) non
//        // vengono avvisati dello spostamento e le pagine dell'albero (puntatori a figli/fratelli
//        // e agli offset dei record) restano non corrette — causa dei record fantasma osservati.
//        AfterHeapRelocated?.Invoke(tailStart, tailEnd, delta);
//
//        // Le SaveNode dentro AfterHeapRelocated passano da WriteIndexPage, che in modalità
//        // DurabilityMode.Full/IntraProcess ACCODA la scrittura (QueueDirectMainWrite) invece di
//        // applicarla subito allo stream — solo _nodeCache (in memoria) riflette subito i puntatori
//        // corretti. Se _nodeCache viene poi svuotata (es. ResetRoot su ROLLBACK) PRIMA che questa
//        // coda venga applicata da un flush successivo, una rilettura da disco trova ancora i
//        // puntatori pre-shift: applichiamo subito la coda per non lasciare questa finestra.
//        if (ShouldUseDirectWal())
//        {
//            ApplyPendingDirectMainWrites();
//        }
//
//        var header = Header;
//        if (header.SecondaryIndexMapOffset > 0)
//        {
//            header.SecondaryIndexMapOffset += delta;
//        }
//
//        if (header.NextAvailablePageOffset >= tailStart)
//        {
//            header.NextAvailablePageOffset += delta;
//        }
//
//        if (header.LobHeapStartOffset >= tailStart)
//        {
//            header.LobHeapStartOffset += delta;
//        }
//
//        if (header.FreeListHeadOffset >= tailStart)
//        {
//            header.FreeListHeadOffset += delta;
//        }
//
//        UpdateHeader(header);
//        _heapRegionStart = header.LobHeapStartOffset > 0 ? header.LobHeapStartOffset : _heapRegionStart;
//
//        foreach (var key in _secondaryIndexMap.Keys.ToList())
//        {
//            var off = _secondaryIndexMap[key];
//            if (off >= tailStart)
//            {
//                _secondaryIndexMap[key] = off + delta;
//            }
//        }
//    }
//
//    #endregion
//
//    #region Public API: Record I/O (Sync/Async)
//
//    /// <summary>
//    /// Legge solo i metadati (stato e versione) di un record a un dato offset.
//    /// </summary>
//    /// <param name="offset">L'offset fisico del record.</param>
//    /// <param name="fileEncoding">L'encoding del file, necessario per la deserializzazione dei metadati.</param>
//    /// <returns>La struttura <see cref="RecordMetadata"/> letta.</returns>
//    public RecordMetadata ReadRecordMetadata(long offset, Encoding fileEncoding)
//    {
//        var metadataSize = Marshal.SizeOf<RecordMetadata>();
//        var buffer = new byte[metadataSize];
//        ReadBytesAt(offset, buffer);
//        return RecordMarshaller.ReadMetadata(buffer, fileEncoding);
//    }
//
//    /// <summary>
//    /// Legge un blocco di byte a un offset fisico, bypassando la cache di lettura del <see cref="FileStream"/>
//    /// quando più handle condividono lo stesso file (cross-process).
//    /// </summary>
//    public void ReadBytesAt(long offset, Span<byte> buffer)
//    {
//        if (TryReadFromPageCache(offset, buffer))
//        {
//            return;
//        }
//
//        ReadBytesAtCore(offset, buffer);
//    }
//
//    /// <summary>Legge una pagina B+ da cache, mmap o stream.</summary>
//    internal byte[] ReadPageBytes(long offset, long fileLength)
//    {
//        if (fileLength - offset < IsamPageCache.PageSize)
//        {
//            throw new EndOfStreamException(
//                $"Pagina B+ incompleta a offset {offset}: file length {fileLength}.");
//        }
//
//        if (ReadPageCache?.TryGetPage(offset, fileLength, out var cached) == true)
//        {
//            return cached;
//        }
//
//        var page = new byte[IsamPageCache.PageSize];
//        ReadBytesAtCore(offset, page);
//        ReadPageCache?.AddPage(offset, page);
//        return page;
//    }
//
//    /// <summary>Lettura posizionale senza page cache (B+ LoadNode).</summary>
//    internal void ReadBytesAtDirect(long offset, Span<byte> buffer) => ReadBytesAtCore(offset, buffer);
//
//    private void ReadBytesAtCore(long offset, Span<byte> buffer)
//    {
//        if (_isMemBacked && _backingStream is IsamMemStream memStream)
//        {
//            memStream.TableReadAt(offset, buffer);
//            return;
//        }
//
//        if (_mmapView != null && offset >= 0 && offset + buffer.Length <= _mmapView.Capacity)
//        {
//            ReadFromMmap(offset, buffer);
//            return;
//        }
//
//        var read = RandomAccess.Read(_diskStream!.SafeFileHandle, buffer, offset);
//        if (read != buffer.Length)
//        {
//            throw new EndOfStreamException(
//                $"Lettura incompleta a offset {offset}: attesi {buffer.Length} byte, letti {read}.");
//        }
//    }
//
//    private bool TryReadFromPageCache(long offset, Span<byte> buffer)
//    {
//        if (ReadPageCache == null)
//        {
//            return false;
//        }
//
//        var pageOffset = offset - offset % IsamPageCache.PageSize;
//        if (!ReadPageCache.TryGetPage(pageOffset, _backingStream.Length, out var page))
//        {
//            return false;
//        }
//
//        var pageIndex = (int)(offset - pageOffset);
//        if (pageIndex + buffer.Length > page.Length)
//        {
//            return false;
//        }
//
//        page.AsSpan(pageIndex, buffer.Length).CopyTo(buffer);
//        return true;
//    }
//
//    private void ReadFromMmap(long offset, Span<byte> buffer)
//    {
//        var rented = ArrayPool<byte>.Shared.Rent(buffer.Length);
//        try
//        {
//            _mmapView!.ReadArray(offset, rented, 0, buffer.Length);
//            rented.AsSpan(0, buffer.Length).CopyTo(buffer);
//        }
//        finally
//        {
//            ArrayPool<byte>.Shared.Return(rented);
//        }
//    }
//
//    private void InvalidatePageCacheForRange(long offset, int length)
//    {
//        if (ReadPageCache == null || length <= 0)
//        {
//            return;
//        }
//
//        var startPage = offset - offset % IsamPageCache.PageSize;
//        var endOffset = offset + length - 1;
//        var endPage = endOffset - endOffset % IsamPageCache.PageSize;
//        for (var page = startPage; page <= endPage; page += IsamPageCache.PageSize)
//        {
//            ReadPageCache.InvalidatePage(page);
//        }
//    }
//
//    private void WriteBytesAt(long offset, ReadOnlySpan<byte> data)
//    {
//        InvalidatePageCacheForRange(offset, data.Length);
//        if (_isMemBacked && _backingStream is IsamMemStream memStream)
//        {
//            memStream.TableWriteAt(offset, data);
//            return;
//        }
//
//        RandomAccess.Write(_diskStream!.SafeFileHandle, data, offset);
//    }
//
//    private void EnsureFileLengthAtLeast(long requiredLength)
//    {
//        if (_backingStream.Length < requiredLength)
//        {
//            _backingStream.SetLength(requiredLength);
//        }
//    }
//
//    /// <summary>
//    /// Legge un record completo a un offset fisico con coerenza cross-process.
//    /// </summary>
//    public void ReadRecordBytes(long offset, byte[] buffer)
//    {
//        ReadBytesAt(offset, buffer);
//    }
//
//    /// <summary>
//    /// Scrive un buffer di record su disco (versione sincrona).
//    /// </summary>
//    public void WriteRecord(long offset, byte[] recordData) => WriteRecordBytes(offset, recordData);
//
//    /// <summary>
//    /// Scrive un record su disco da span (path caldo zero-alloc).
//    /// </summary>
//    internal void WriteRecordBytes(long offset, ReadOnlySpan<byte> recordData) =>
//        WriteRecordFromBuffer(offset, recordData.ToArray(), recordData.Length);
//
//    /// <summary>
//    /// Scrive un record da buffer owned/rented; evita copia extra su path Buffered diretto.
//    /// </summary>
//    internal void WriteRecordFromBuffer(long offset, byte[] buffer, int length)
//    {
//        WithPhysicalIo(() =>
//        {
//            if (IsTransactionalWalActive)
//            {
//                EnsureWalCore();
//                var copy = new byte[length];
//                Buffer.BlockCopy(buffer, 0, copy, 0, length);
//                _wal!.LogWriteRecord(offset, copy);
//                WriteBytesAt(offset, copy);
//            }
//            else if (ShouldUseDirectWal())
//            {
//                if (LockingStrategy == LockingStrategy.CrossProcess)
//                {
//                    WriteBytesAt(offset, buffer.AsSpan(0, length));
//                    FlushBackingStream(true);
//                }
//                else
//                {
//                    var copy = new byte[length];
//                    Buffer.BlockCopy(buffer, 0, copy, 0, length);
//                    QueueDirectMainWrite(offset, copy);
//                }
//            }
//            else
//            {
//                WriteBytesAt(offset, buffer.AsSpan(0, length));
//                if (LockingStrategy == LockingStrategy.CrossProcess
//                    && DurabilityMode == IsamDurabilityMode.Full)
//                {
//                    FlushBackingStream(true);
//                }
//            }
//        });
//    }
//
//    /// <summary>
//    /// Scrive una pagina indice B+ (4096 byte) con logging WAL coerente.
//    /// ponytail: CrossProcess fa fsync per pagina — tradeoff throughput vs visibilità inter-processo.
//    /// </summary>
//    internal void WriteIndexPage(long offset, ReadOnlySpan<byte> pageBuffer)
//    {
//        var copy = pageBuffer.ToArray();
//        WithPhysicalIo(() =>
//        {
//            InvalidatePageCacheForRange(offset, copy.Length);
//            // Le pagine B+ non sono allineate a 4096 byte (condividono l'heap con record/LOB, che
//            // possono iniziare a offset arbitrari) — ReadPageBytes (letta da LoadNodeFromDisk) le
//            // mette in cache con la chiave ESATTA (non allineata), mentre InvalidatePageCacheForRange
//            // sopra invalida solo chiavi allineate a pagina: senza questa riga la voce non allineata
//            // non viene mai rimossa, e resta visibile per sempre a qualunque lettura futura che
//            // ripassi da cache-miss di _nodeCache (es. ResetRoot su ROLLBACK) — bug scoperto testando
//            // DbIsamFullTests.bas: SEEK dopo ROLLBACK, preceduto da CREATE/DROP INDEX, risolveva un
//            // offset dentro il blocco schema perché rileggeva questa pagina B+ ESATTA rimasta in
//            // cache con il contenuto precedente allo shift dell'header.
//            ReadPageCache?.InvalidatePage(offset);
//            if (ShouldUseDirectWal())
//            {
//                if (LockingStrategy == LockingStrategy.CrossProcess)
//                {
//                    LogDirectWriteBeforeMainFile(offset, copy);
//                    WriteBytesAt(offset, copy);
//                    EnsureFileLengthAtLeast(offset + copy.Length);
//                    FlushBackingStream(true);
//                    return;
//                }
//
//                QueueDirectMainWrite(offset, copy);
//                return;
//            }
//
//            WriteBytesAt(offset, copy);
//            EnsureFileLengthAtLeast(offset + copy.Length);
//
//            LogPageWriteIfDurabilityRequiredCore(offset, copy);
//            if (LockingStrategy == LockingStrategy.CrossProcess)
//            {
//                FlushBackingStream(true);
//            }
//        });
//    }
//
//    /// <summary>Zero-fill pagina B+ riallocata; usa RandomAccess coerente con <see cref="WriteIndexPage"/>.</summary>
//    internal void ZeroIndexPage(long pageOffset)
//    {
//        var page = new byte[IsamPageChecksum.PageSize];
//        WithPhysicalIo(() =>
//        {
//            WriteBytesAt(pageOffset, page);
//            // Vedi commento in WriteIndexPage: invalida anche la chiave non allineata.
//            ReadPageCache?.InvalidatePage(pageOffset);
//        });
//    }
//
//    /// <summary>
//    /// Scrive un buffer di record su disco (versione asincrona).
//    /// </summary>
//    public Task WriteRecordAsync(long offset, byte[] recordData, CancellationToken cancellationToken)
//    {
//        WithPhysicalIo(() =>
//        {
//            if (IsTransactionalWalActive)
//            {
//                EnsureWalCore();
//                _wal!.LogWriteRecord(offset, recordData);
//            }
//            else if (ShouldUseDirectWal())
//            {
//                QueueDirectMainWrite(offset, recordData);
//                return;
//            }
//
//            WriteBytesAt(offset, recordData);
//        });
//
//        return Task.CompletedTask;
//    }
//
//    /// <summary>
//    /// Scrive solo i metadati di un record a un dato offset, senza modificare i dati utente (versione sincrona).
//    /// </summary>
//    public void WriteRecordMetadata(long offset, RecordMetadata metadata)
//    {
//        var buffer = RecordMarshaller.SerializeMetadata(metadata);
//
//        WithPhysicalIo(() =>
//        {
//            EnsureWalIfTransactionalCore();
//            _wal?.LogWriteBytes(offset, buffer);
//
//            WriteBytesAt(offset, buffer);
//        });
//    }
//
//    #endregion
//
//    #region Public API: Free List & Allocation
//
//    /// <summary>
//    /// Alloca spazio per un nuovo record, dando priorità al riutilizzo di record cancellati dalla free list.
//    /// Acquisisce automaticamente il lock strutturale (B3).
//    /// </summary>
//    public long AllocateNewRecordOffset()
//    {
//        using (EnterStructuralLock())
//        {
//            SyncStructuralStateFromDisk();
//            return AllocateNewRecordOffsetCore();
//        }
//    }
//
//    /// <summary>
//    /// Alloca un nuovo slot record. Richiede <see cref="EnterStructuralLock"/> già acquisito dal chiamante.
//    /// </summary>
//    internal long AllocateNewRecordOffsetCore()
//    {
//        var header = Header;
//        long allocatedOffset;
//        if (header.FreeListHeadOffset != -1)
//        {
//            allocatedOffset = header.FreeListHeadOffset;
//
//            Span<byte> nextPtr = stackalloc byte[sizeof(long)];
//            ReadBytesAt(allocatedOffset + 1, nextPtr);
//            header.FreeListHeadOffset = BitConverter.ToInt64(nextPtr);
//        }
//        else
//        {
//            allocatedOffset = DataAreaStartOffset + header.LastRecNumber * header.RecordSize;
//            var requiredHeap =
//                HeapRelocation.AlignUp(allocatedOffset + header.RecordSize, HeapRelocation.PageSize);
//            EnsureHeapStartsAtOrAbove(requiredHeap);
//            header = Header;
//            header.LastRecNumber += 1;
//
//            var requiredLength = allocatedOffset + header.RecordSize;
//            if (_backingStream.Length < requiredLength)
//            {
//                _backingStream.SetLength(requiredLength);
//            }
//        }
//
//        header.ActiveRecordCount += 1;
//        UpdateHeader(header);
//        return allocatedOffset;
//    }
//
//    /// <summary>
//    /// Alloca <paramref name="count"/> slot record consecutivi. Richiede lock strutturale già acquisito.
//    /// ponytail: free-list slot consumati uno-per-uno; path sequenziale batchea LastRecNumber.
//    /// </summary>
//    internal long[] AllocateNewRecordOffsetBatchCore(int count)
//    {
//        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(count);
//
//        var offsets = new long[count];
//        var header = Header;
//        var allocated = 0;
//        Span<byte> nextPtr = stackalloc byte[sizeof(long)];
//
//        while (allocated < count && header.FreeListHeadOffset != -1)
//        {
//            var allocatedOffset = header.FreeListHeadOffset;
//            ReadBytesAt(allocatedOffset + 1, nextPtr);
//            header.FreeListHeadOffset = BitConverter.ToInt64(nextPtr);
//            offsets[allocated++] = allocatedOffset;
//        }
//
//        if (allocated < count)
//        {
//            var sequentialCount = count - allocated;
//            var startRec = header.LastRecNumber;
//            var firstOffset = DataAreaStartOffset + startRec * header.RecordSize;
//            var lastOffset = DataAreaStartOffset + (startRec + sequentialCount - 1) * header.RecordSize;
//            var requiredHeap = HeapRelocation.AlignUp(lastOffset + header.RecordSize, HeapRelocation.PageSize);
//            EnsureHeapStartsAtOrAbove(requiredHeap);
//            header = Header;
//            header.LastRecNumber += sequentialCount;
//
//            for (var i = 0; i < sequentialCount; i++)
//            {
//                offsets[allocated + i] = DataAreaStartOffset + (startRec + i) * header.RecordSize;
//            }
//
//            allocated = count;
//            var requiredLength = lastOffset + header.RecordSize;
//            if (_backingStream.Length < requiredLength)
//            {
//                _backingStream.SetLength(requiredLength);
//            }
//        }
//
//        header.ActiveRecordCount += count;
//        UpdateHeader(header);
//        return offsets;
//    }
//
//    /// <summary>
//    /// Aggiunge un record alla testa della Free List, marcandolo come cancellato e
//    /// riutilizzando il suo spazio per mantenere la catena della lista.
//    /// </summary>
//    /// <param name="recordOffset">L'offset fisico del record da aggiungere alla lista.</param>
//    /// <remarks>
//    /// L'operazione esegue i seguenti passi in modo atomico (protetto da un lock a livello di file):
//    /// 1. Scrive il byte di stato 'cancellato' (0x2A) all'inizio del record.
//    /// 2. Legge l'offset della vecchia testa della free list dall'header.
//    /// 3. Scrive questo offset (il puntatore al "prossimo" record libero) subito dopo il byte di stato,
//    ///    sovrascrivendo i primi byte dei dati utente del record, che non sono più necessari.
//    /// 4. Aggiorna l'header del file per far puntare la testa della free list a questo record.
//    /// 5. Decrementa il contatore dei record attivi.
//    /// </remarks>
//    public void AddRecordToFreeList(long recordOffset)
//    {
//        LockFile();
//        try
//        {
//            var header = Header;
//            _backingStream.Seek(recordOffset, SeekOrigin.Begin);
//            using (var writer = new BinaryWriter(_backingStream, FileEncoding, leaveOpen: true)) // Usa FileEncoding
//            {
//                writer.Write(Config.RecordStatusDeleted);
//                writer.Write(header.FreeListHeadOffset);
//            }
//
//            header.FreeListHeadOffset = recordOffset;
//            header.ActiveRecordCount -= 1;
//            UpdateHeader(header);
//        }
//        finally
//        {
//            UnlockFile();
//        }
//    }
//
//    #endregion
//
//    #region Public API: Transaction (Sync/Async)
//
//    /// <summary>
//    /// Avvia una transazione per il thread corrente, registrandola e restituendone l'ID univoco.
//    /// </summary>
//    /// <returns>L'ID della transazione appena creata.</returns>
//    public long BeginTransaction()
//    {
//        var txId = _transactionManager.BeginTransaction();
//        _threadTransactionMap[Thread.CurrentThread.ManagedThreadId] = txId;
//
//        var headerBytes = SerializeHeaderBytes(Header);
//        _transactionSnapshotHeader = DeserializeHeaderBytes(headerBytes);
//        _transactionSnapshotIndexMap = SchemaManager.SerializeIndexMap(_secondaryIndexMap);
//        WithPhysicalIo(() =>
//        {
//            EnsureWalCore();
//            _wal!.LogTransactionSnapshot(headerBytes, _transactionSnapshotIndexMap);
//            _wal.LogBeginTransaction();
//        });
//
//        return txId;
//    }
//
//    /// <summary>
//    /// Tenta di finalizzare una transazione esplicita precedentemente avviata.
//    /// L'operazione scrive un marcatore <see cref="WalOperationType.CommitTransaction"/> nel log,
//    /// che segnala al processo di recupero che tutte le operazioni precedenti (dall'ultimo
//    /// <see cref="BeginTransaction"/>) sono valide e devono essere applicate.
//    /// Dopo aver registrato il commit, il metodo decrementa un contatore interno di transazioni attive.
//    /// Se questo contatore raggiunge lo zero, invoca il metodo <see cref="Commit"/> finale per
//    /// pulire il file WAL e risincronizzare l'header.
//    /// </summary>
//    /// <remarks>
//    /// Questo approccio supporta una forma base di transazioni nidificate, dove solo il commit
//    /// della transazione più esterna (quando il contatore torna a zero) innesca la pulizia
//    /// effettiva del log.
//    /// </remarks>
//    public void CommitTransaction()
//    {
//        if (_threadTransactionMap.TryGetValue(Thread.CurrentThread.ManagedThreadId, out var txId))
//        {
//            _transactionManager.CommitTransaction(txId);
//            _threadTransactionMap.TryRemove(Thread.CurrentThread.ManagedThreadId, out _);
//        }
//
//        WithPhysicalIo(() =>
//        {
//            EnsureWalCore();
//            _wal!.LogCommitTransaction();
//        });
//
//        _transactionSnapshotHeader = null;
//        _transactionSnapshotIndexMap = null;
//    }
//
//    /// <summary>
//    /// Annulla una transazione esplicita in corso. L'operazione non scrive nulla nel Write-Ahead Log;
//    /// semplicemente decrementa il contatore delle transazioni attive.
//    /// </summary>
//    /// <remarks>
//    /// L'annullamento effettivo delle modifiche è implicito e gestito dal processo di recupero.
//    /// Poiché non verrà mai scritto un marcatore <see cref="WalOperationType.CommitTransaction"/> per questo
//    /// blocco di operazioni, il metodo <see cref="WriteAheadLog.Recover"/> ignorerà tutte le
//    /// operazioni registrate dall'ultimo <see cref="BeginTransaction"/>, ottenendo di fatto un rollback.
//    /// </remarks>
//    public void RollbackTransaction()
//    {
//        if (_threadTransactionMap.TryGetValue(Thread.CurrentThread.ManagedThreadId, out var txId))
//        {
//            _transactionManager.RollbackTransaction(txId);
//            _threadTransactionMap.TryRemove(Thread.CurrentThread.ManagedThreadId, out _);
//        }
//
//        RestoreTransactionSnapshot();
//        ResetWriteAheadLog();
//    }
//
//    /// <summary>
//    /// Commette la transazione corrente, sincronizzando i dati e il WAL su disco (versione sincrona).
//    /// Questa operazione include la pulizia del file WAL.
//    /// </summary>
//    public void Commit()
//    {
//        WithPhysicalIo(() =>
//        {
//            if (!IsBulkLoadActive)
//            {
//                FlushPendingHeader();
//            }
//
//            if (ShouldUseDirectWal())
//            {
//                ApplyPendingDirectMainWrites();
//            }
//
//            var fsync = !IsBulkLoadActive
//                        && (DurabilityMode == IsamDurabilityMode.Full
//                            || IsTransactionalWalActive
//                            || LockingStrategy == LockingStrategy.CrossProcess);
//            if (fsync)
//            {
//                SyncDirectWalBeforeMainFile();
//            }
//
//            FlushBackingStream(fsync);
//            FinalizeDirectWalAfterSync();
//            if (_wal != null && IsTransactionalWalActive)
//            {
//                _wal.Delete();
//                _wal = null;
//            }
//
//            if (!IsBulkLoadActive && ShouldReloadHeaderAfterCommit())
//            {
//                ReloadHeaderFromDisk();
//            }
//        });
//    }
//
//    private bool ShouldReloadHeaderAfterCommit() =>
//        LockingStrategy == LockingStrategy.CrossProcess
//        || DurabilityMode == IsamDurabilityMode.Full;
//
//    private void FinalizeDirectWalAfterSync()
//    {
//        if (_wal == null || IsTransactionalWalActive || !ShouldUseDirectWal())
//        {
//            return;
//        }
//
//        _wal.LogDirectCheckpoint();
//        _wal.Delete();
//        _wal = null;
//    }
//
//    /// <summary>
//    /// Commette la transazione corrente (versione asincrona).
//    /// </summary>
//    public Task CommitAsync() => Task.Run(Commit);
//
//    #endregion
//
//    #region Public API: Locking (Sync/Async)
//
//    /// <summary>
//    /// Tenta di acquisire un lock in scrittura sul file (versione sincrona, non bloccante).
//    /// </summary>
//    public bool TryLockFile(int timeoutMilliseconds) => _fileLevelLock.TryEnterWriteLock(timeoutMilliseconds);
//
//    /// <summary>
//    /// Alloca spazio per una versione undo MVCC fuori dalla griglia logica dei record.
//    /// Non modifica <c>ActiveRecordCount</c> né <c>LastRecNumber</c>.
//    /// </summary>
//    internal long AllocateUndoRecordOffset()
//    {
//        LockFile();
//        try
//        {
//            return ReserveHeapSpace(Header.RecordSize);
//        }
//        finally
//        {
//            UnlockFile();
//        }
//    }
//
//    /// <summary>
//    /// Acquisisce un lock in scrittura sul file (versione sincrona, bloccante).
//    /// </summary>
//    public void LockFile()
//    {
//        if (_fileLockDepth == 0 && LockingStrategy == LockingStrategy.CrossProcess)
//        {
//            RecordLockManager.AcquireWriteLock(FileStructureLockKey, Timeout.Infinite, "struttura file ISAM");
//        }
//
//        _fileLevelLock.EnterWriteLock();
//        _fileLockDepth++;
//    }
//
//    /// <summary>
//    /// B3: scope RAII per lock strutturale stretto (allocazione record, heap, radici B+).
//    /// </summary>
//    public StructuralLockScope EnterStructuralLock() => new(this);
//
//    /// <summary>Cross-process: read lock struttura file per traversata B+ multi-nodo.</summary>
//    public FileStructureReadScope EnterFileStructureReadScope() => new(this);
//
//    internal void EnterFileStructureReadLock()
//    {
//        if (LockingStrategy != LockingStrategy.CrossProcess || _fileLockDepth > 0)
//        {
//            return;
//        }
//
//        if (_fileStructureReadLockDepth > 0)
//        {
//            _fileStructureReadLockDepth++;
//            return;
//        }
//
//        RecordLockManager.AcquireReadLock(
//            FileStructureLockKey,
//            Timeout.Infinite,
//            "lettura pagina B+ ISAM");
//        _fileStructureReadLockDepth = 1;
//    }
//
//    internal void ExitFileStructureReadLock()
//    {
//        if (_fileStructureReadLockDepth == 0)
//        {
//            return;
//        }
//
//        if (--_fileStructureReadLockDepth > 0)
//        {
//            return;
//        }
//
//        RecordLockManager.ReleaseReadLock(FileStructureLockKey);
//    }
//
//    internal void EnterStructuralLockCore()
//    {
//        EnterPhysicalIoLock();
//        if (LockingStrategy == LockingStrategy.CrossProcess)
//        {
//            LockFile();
//        }
//    }
//
//    internal void ExitStructuralLockCore()
//    {
//        if (LockingStrategy == LockingStrategy.CrossProcess)
//        {
//            if (ShouldUseDirectWal())
//            {
//                ApplyPendingDirectMainWrites();
//                SyncDirectWalBeforeMainFile();
//            }
//
//            FlushBackingStream(true);
//            UnlockFile();
//        }
//
//        ExitPhysicalIoLock();
//    }
//
//    private void EnterPhysicalIoLock()
//    {
//        var threadId = Thread.CurrentThread.ManagedThreadId;
//        if (_physicalIoLockOwnerThreadId == threadId)
//        {
//            _physicalIoLockDepth++;
//            return;
//        }
//
//        Monitor.Enter(_physicalIoLock);
//        _physicalIoLockOwnerThreadId = threadId;
//        _physicalIoLockDepth = 1;
//    }
//
//    private void ExitPhysicalIoLock()
//    {
//        if (_physicalIoLockDepth == 0)
//        {
//            return;
//        }
//
//        if (--_physicalIoLockDepth == 0)
//        {
//            _physicalIoLockOwnerThreadId = -1;
//            Monitor.Exit(_physicalIoLock);
//        }
//    }
//
//    private void WithPhysicalIo(Action action)
//    {
//        EnterPhysicalIoLock();
//        try
//        {
//            action();
//        }
//        finally
//        {
//            ExitPhysicalIoLock();
//        }
//    }
//
//    /// <summary>
//    /// Esegue un'azione sotto <see cref="_physicalIoLock"/> (es. read-ahead B+ sequenziale).
//    /// </summary>
//    internal void RunUnderPhysicalIoLock(Action action) => WithPhysicalIo(action);
//
//    /// <summary>
//    /// Rilascia il lock in scrittura sul file.
//    /// </summary>
//    public void UnlockFile()
//    {
//        if (!_fileLevelLock.IsWriteLockHeld)
//        {
//            return;
//        }
//
//        _fileLevelLock.ExitWriteLock();
//        _fileLockDepth--;
//        if (_fileLockDepth == 0 && LockingStrategy == LockingStrategy.CrossProcess)
//        {
//            RecordLockManager.ReleaseWriteLock(FileStructureLockKey);
//        }
//    }
//
//    /// <summary>
//    /// Rilascia il lock in scrittura sul file (versione asincrona).
//    /// </summary>
//    public Task UnlockFileAsync()
//    {
//        UnlockFile();
//        return Task.CompletedTask;
//    }
//
//    /// <summary>
//    /// Tenta di acquisire un lock esclusivo su un record, attendendo per un massimo di <paramref name="timeoutMilliseconds"/>.
//    /// </summary>
//    /// <param name="offset">L'offset fisico del record da bloccare.</param>
//    /// <param name="timeoutMilliseconds">Il tempo massimo di attesa in millisecondi.</param>
//    /// <returns>True se il lock è stato acquisito, False se il timeout è scaduto.</returns>
//    public bool TryLockRecord(long offset, int timeoutMilliseconds)
//    {
//        try
//        {
//            var lockKey = LockManagerBase.GetRecordLockKey(offset);
//            RecordLockManager.AcquireWriteLock(lockKey, timeoutMilliseconds, $"record all'offset {offset}");
//            return true;
//        }
//        catch (LockTimeoutException)
//        {
//            return false;
//        }
//    }
//
//    /// <summary>
//    /// Acquisisce un lock bloccante (attesa infinita) su un record specifico.
//    /// </summary>
//    /// <param name="offset">L'offset fisico del record da bloccare.</param>
//    public void LockRecord(long offset)
//    {
//        try
//        {
//            var lockKey = LockManagerBase.GetRecordLockKey(offset);
//            RecordLockManager.AcquireWriteLock(lockKey, -1, $"record all'offset {offset}");
//        }
//        catch (LockTimeoutException ex)
//        {
//            throw new IOException("Impossibile acquisire il lock sul record.", ex);
//        }
//    }
//
//    /// <summary>
//    /// Acquisisce un lock su un record (versione asincrona, bloccante).
//    /// </summary>
//    public Task LockRecordAsync(long offset) => Task.Run(() => LockRecord(offset));
//
//    /// <summary>
//    /// Rilascia il lock su un record.
//    /// </summary>
//    public void UnlockRecord(long offset)
//    {
//        var lockKey = LockManagerBase.GetRecordLockKey(offset);
//        RecordLockManager.ReleaseWriteLock(lockKey);
//    }
//
//    /// <summary>
//    /// Rilascia il lock su un record (versione asincrona).
//    /// </summary>
//    public Task UnlockRecordAsync(long offset)
//    {
//        UnlockRecord(offset);
//        return Task.CompletedTask;
//    }
//
//    #endregion
//
//    #region Public API: Structure & Copy
//
//    /// <summary>
//    /// Crea un nuovo file di database ISAM vuoto ma strutturalmente valido, scrivendo su disco l'header
//    /// e il blocco dello schema. Se il file esiste già, viene sovrascritto.
//    /// </summary>
//    /// <param name="filePath">Il percorso completo del nuovo file .isdb da creare.</param>
//    /// <param name="schema">Lo schema (<see cref="Config.IsamSchema"/>) che definisce la struttura del nuovo file.</param>
//    /// <param name="fileEncoding">L'encoding da utilizzare per il file.</param>
//    public static void CreateNew(string filePath, Config.IsamSchema schema, Encoding fileEncoding)
//    {
//        if (IsamMemPath.IsMemPath(filePath))
//        {
//            IsamMemStore.CreateNew(IsamMemPath.NormalizeKey(filePath), schema, fileEncoding);
//            return;
//        }
//
//        var dir = Path.GetDirectoryName(filePath);
//        if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
//            Directory.CreateDirectory(dir);
//
//        var bytes = BuildNewFileBytes(schema, fileEncoding);
//        using var stream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None);
//        stream.Write(bytes, 0, bytes.Length);
//        stream.Flush();
//    }
//
//    /// <summary>Serializes a new empty ISAM file image (header + schema + index map).</summary>
//    internal static byte[] BuildNewFileBytes(Config.IsamSchema schema, Encoding fileEncoding)
//    {
//        _ = fileEncoding;
//        using var stream = new MemoryStream();
//
//        var recordSize = SchemaManager.CalculateRecordSize(schema);
//        var schemaBuffer = SchemaManager.SerializeSchema(schema);
//
//        var secondaryIndexMap = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
//        foreach (var indexDef in schema.SecondaryKeys)
//        {
//            secondaryIndexMap[indexDef.Name] = -1;
//        }
//
//        var mapBuffer = SchemaManager.SerializeIndexMap(secondaryIndexMap);
//
//        var secondaryOffsets = new long[15];
//        Array.Fill(secondaryOffsets, -1L);
//
//        var header = new Config.FileHeader
//        {
//            Magic = Config.MagicBytes,
//            Version = Config.FormatVersion,
//            RecordSize = recordSize,
//            SchemaOffset = Config.HeaderSize,
//            SchemaLength = schemaBuffer.Length,
//            SecondaryIndexMapOffset = Config.HeaderSize + schemaBuffer.Length,
//            PrimaryKeyRootOffset = -1,
//            FreeListHeadOffset = -1,
//            NextAvailablePageOffset = Config.HeaderSize + schemaBuffer.Length + mapBuffer.Length,
//            SecondaryIndexRootOffsets = secondaryOffsets,
//            LobHeapStartOffset =
//                Config.HeaderSize + schemaBuffer.Length + mapBuffer.Length,
//            PageFreeListHeadOffset = -1,
//            Reserved = new byte[Config.HeaderSize - Config.FileHeaderFieldsPrefixSize]
//        };
//
//        stream.Seek(0, SeekOrigin.Begin);
//        MarshalHelper.WriteStructure(stream, header); // Usa MarshalHelper
//        stream.Write(schemaBuffer, 0, schemaBuffer.Length);
//        stream.Write(mapBuffer, 0, mapBuffer.Length);
//        stream.SetLength(Math.Max(stream.Length,
//            header.NextAvailablePageOffset)); // Assicura che il file sia grande abbastanza
//        return stream.ToArray();
//    }
//
//    #endregion
//
//    /// <summary>
//    /// Aggiunge una pagina B+Tree non più utilizzata alla testa della lista delle pagine libere (free list),
//    /// rendendola disponibile per un futuro riutilizzo da parte del metodo AllocateNewPage".
//    /// </summary>
//    /// <param name="pageOffset">L'offset fisico in byte della pagina (nodo B+Tree) da liberare e aggiungere alla lista.</param>
//    /// <remarks>
//    /// Questo metodo è una componente fondamentale della gestione dello spazio su disco, permettendo al motore ISAM
//    /// di riciclare lo spazio dei nodi B+Tree eliminati (ad esempio, a seguito di un'operazione di `merge`)
//    /// e minimizzare la crescita e la frammentazione del file.
//    ///
//    /// L'operazione implementa la logica di "aggiunta in testa" (prepending) a una lista concatenata (singly-linked list)
//    /// di pagine libere. L'intero processo è protetto da un lock a livello di file per garantirne l'atomicità e
//    /// prevenire race condition. I passi eseguiti sono:
//    /// 1. Legge l'offset della testa corrente della free list dall'header del file.
//    /// 2. Scrive questo offset (che diventa il puntatore al "prossimo" elemento libero) nei primi byte della pagina
//    ///    che si sta liberando. In questo modo, la pagina liberata ora "punta" al resto della catena.
//    /// 3. Aggiorna l'header del file affinché la sua <c>PageFreeListHeadOffset</c> punti a questa nuova pagina, che è diventata
//    ///    la nuova testa della lista.
//    /// </remarks>
//    public void AddPageToFreeList(long pageOffset)
//    {
//        LockFile();
//        try
//        {
//            var header = Header;
//            // ponytail: zero + link; byte 8 = 0xFE (non B+ leaf/internal) evita interpretazione come nodo
//            ZeroIndexPage(pageOffset);
//            var headerBytes = new byte[9];
//            BitConverter.TryWriteBytes(headerBytes.AsSpan(), header.PageFreeListHeadOffset);
//            headerBytes[8] = 0xFE;
//            WithPhysicalIo(() => WriteBytesAt(pageOffset, headerBytes));
//
//            header.PageFreeListHeadOffset = pageOffset;
//            UpdateHeader(header);
//        }
//        finally
//        {
//            UnlockFile();
//        }
//    }
//
//    /// <summary>
//    /// Recupera l'ID della transazione attualmente associata al thread chiamante.
//    /// </summary>
//    /// <returns>L'ID della transazione attiva, o 0 se non c'è nessuna transazione attiva per questo thread.</returns>
//    public long GetCurrentTransactionId()
//    {
//        _threadTransactionMap.TryGetValue(Thread.CurrentThread.ManagedThreadId, out var txId);
//        return txId;
//    }
//
//    /// <summary>
//    /// Determina se la versione di un record, creata da una certa transazione,
//    /// è visibile alla transazione corrente, secondo le regole di "Read Committed".
//    /// </summary>
//    /// <param name="recordTransactionId">L'ID della transazione che ha scritto la versione del record.</param>
//    /// <param name="currentTransactionId">L'ID della transazione che sta tentando di leggere il record.</param>
//    /// <returns>True se la versione del record è visibile.</returns>
//    public bool IsVersionVisible(long recordTransactionId, long currentTransactionId)
//    {
//        return _transactionManager.IsVersionVisible(recordTransactionId, currentTransactionId);
//    }
//    
//    #region Large Object (LOB) Heap Management
//
//    /// <summary>
//    /// Alloca spazio per un Large Object (LOB) nel LOB Heap e ci scrive i dati forniti.
//    /// </summary>
//    /// <param name="data">L'array di byte che rappresenta il Large Object (es. JSON serializzato).</param>
//    /// <returns>L'offset fisico del LOB appena scritto nel LOB Heap.</returns>
//    /// <remarks>
//    /// Il LOB Heap è un'area del file dedicata a dati a lunghezza variabile. Ogni blocco LOB
//    /// è prefissato da un campo di 4 byte che indica la sua lunghezza.
//    /// Questa operazione è protetta da un lock a livello di file poiché modifica l'offset
//    /// `LobHeapStartOffset` nell'header.
//    /// </remarks>
//    public long WriteLob(byte[] data)
//    {
//        LockFile();
//        try
//        {
//            var lobOffset = GetHeapAllocationOffset();
//            _backingStream.Seek(lobOffset, SeekOrigin.Begin);
//            using (var writer = new BinaryWriter(_backingStream, FileEncoding, leaveOpen: true))
//            {
//                writer.Write(data.Length);
//                writer.Write(data);
//            }
//
//            var requiredLength = _backingStream.Position;
//            if (_backingStream.Length < requiredLength)
//            {
//                _backingStream.SetLength(requiredLength);
//            }
//
//            var header = Header;
//            header.NextAvailablePageOffset = requiredLength;
//            UpdateHeader(header);
//            RegisterHeapAllocation(lobOffset, requiredLength);
//
//            return lobOffset;
//        }
//        finally
//        {
//            UnlockFile();
//        }
//    }
//
//    /// <summary>
//    /// Legge un Large Object (LOB) dal LOB Heap a un offset specifico.
//    /// </summary>
//    /// <param name="offset">L'offset fisico nel file dove inizia il LOB.</param>
//    /// <returns>L'array di byte che rappresenta il Large Object.</returns>
//    /// <exception cref="IOException">Se l'offset non è valido o i dati sono corrotti.</exception>
//    public byte[] ReadLob(long offset)
//    {
//        if (offset < Header.SchemaOffset)
//            throw new IOException($"Offset LOB non valido: {offset}.");
//
//        Span<byte> lengthBuffer = stackalloc byte[sizeof(int)];
//        ReadBytesAt(offset, lengthBuffer);
//        var length = BitConverter.ToInt32(lengthBuffer);
//
//        if (length < 0 || offset + sizeof(int) + length > _backingStream.Length)
//            throw new IOException($"Lunghezza LOB non valida o dati corrotti a offset {offset}.");
//
//        var data = new byte[length];
//        ReadBytesAt(offset + sizeof(int), data);
//        return data;
//    }
//
//    #endregion
//    
//    /// <summary>
//    /// Rilascia in modo pulito tutte le risorse gestite dall'istanza di <see cref="IsamFileHandle"/>, garantendo la chiusura
//    /// sicura del file di database e di tutti i meccanismi di supporto associati.
//    /// </summary>
//    /// <remarks>
//    /// Questa è un'operazione fondamentale per la corretta gestione del ciclo di vita di un file di database. L'invocazione
//    /// di questo metodo (tipicamente in modo automatico tramite un blocco `using`) è essenziale per prevenire
//    /// resource leak (come handle di file lasciati aperti) e per assicurare che tutti i dati in buffer vengano
//    /// scritti correttamente su disco.
//    ///
//    /// Nello specifico, il metodo esegue in sequenza le seguenti operazioni di pulizia:
//    /// 1. Chiude il flusso di file sottostante (<see cref="FileStream"/>), rilasciando il lock del sistema operativo sul file .isdb.
//    /// 2. Rilascia le risorse del gestore di lock dei record (<see cref="LockManagerBase"/>).
//    /// 3. Rilascia le risorse del lock a livello di file (<see cref="ReaderWriterLockSlim"/>).
//    /// 4. Esegue il flush e chiude il file del Write-Ahead Log (<see cref="WriteAheadLog"/>).
//    /// </remarks>
//    public void Dispose()
//    {
//        if ((_bulkLoadDepth > 0 || _headerDirty) && !IsTransactionalWalActive)
//        {
//            try
//            {
//                FlushDurability();
//            }
//            catch
//            {
//                // Best-effort prima della chiusura handle.
//            }
//        }
//
//        WithPhysicalIo(() =>
//        {
//            DisableMmapReadOnly();
//            ReadPageCache?.Clear();
//            if (_diskStream != null)
//            {
//                _diskStream.Dispose();
//            }
//
//            // Rilascia i lock record residui del thread corrente PRIMA dello smaltimento: un
//            // RLOCK seguito da UNLOCK con cursore spostato (IsamDriver.Unlock usa il record
//            // corrente, non il numero richiesto) lasciava il ReaderWriterLockSlim tenuto e il
//            // Dispose sotto lanciava "The lock is being disposed while still being used".
//            if (RecordLockManager is Basic.Databases.IntraProcessLockManager ipmClose) ipmClose.ReleaseAllHeld();
//            RecordLockManager.Dispose();
//            _fileLevelLock.Dispose();
//            _wal?.Dispose();        });
//
//        if (_ownsCrossProcessRecoverMutex)
//        {
//            _crossProcessRecoverMutex?.ReleaseMutex();
//            _ownsCrossProcessRecoverMutex = false;
//        }
//
//        _crossProcessRecoverMutex?.Dispose();
//    }
//
//    private static string GetStableFileId(string filePath) =>
//        Convert.ToBase64String(Encoding.UTF8.GetBytes(Path.GetFullPath(filePath).ToUpperInvariant()))
//            .Replace('+', '_').Replace('/', '-');
//
//    #region Private Methods
//
//    private void EnsureWal() => WithPhysicalIo(EnsureWalCore);
//
//    private void EnsureWalCore()
//    {
//        if (_isMemBacked)
//        {
//            return;
//        }
//
//        _wal ??= new WriteAheadLog(FilePath);
//    }
//
//    private void EnsureWalIfTransactionalCore()
//    {
//        if (IsTransactionalWalActive)
//        {
//            EnsureWalCore();
//        }
//    }
//
//    private void ReloadHeaderFromDisk()
//    {
//        // RandomAccess.Read è posizionale: niente _fileLevelLock (evita deadlock con _physicalIoLock in Commit).
//        Header = ReadHeaderStructureFromStream();
//    }
//
//    private Config.FileHeader ReadHeaderStructureFromStream()
//    {
//        var headerSize = Marshal.SizeOf<Config.FileHeader>();
//        var buffer = new byte[headerSize];
//        if (_isMemBacked && _backingStream is IsamMemStream memStream)
//        {
//            memStream.TableReadAt(0, buffer);
//        }
//        else
//        {
//            RandomAccess.Read(_diskStream!.SafeFileHandle, buffer, 0);
//        }
//
//        using var ms = new MemoryStream(buffer);
//        return MarshalHelper.ReadStructure<Config.FileHeader>(ms);
//    }
//
//    private bool IsTransactionalWalActive =>
//        _threadTransactionMap.ContainsKey(Thread.CurrentThread.ManagedThreadId);
//
//    internal bool IsWalHandleOpen => _wal is not null;
//
//    internal bool IsDirectWalDirty => _directWalDirty;
//
//    internal bool HasOpenWalTransaction => IsTransactionalWalActive;
//
//    private void EnsureWalIfTransactional() =>
//        WithPhysicalIo(EnsureWalIfTransactionalCore);
//
//    /// <summary>
//    /// Scrive una versione "undo" di un record in un nuovo spazio allocato e, se in una
//    /// transazione, registra l'offset per un potenziale recupero di spazio in caso di rollback.
//    /// </summary>
//    /// <param name="currentOffset">L'offset del record di cui creare una copia di undo.</param>
//    /// <param name="context">Il contesto della transazione corrente. Se fornito, l'offset
//    /// del nuovo record di undo viene registrato al suo interno.</param>
//    /// <returns>L'offset del nuovo record di undo creato.</returns>
//    internal long WriteUndoVersion(long currentOffset, TransactionContext? context)
//    {
//        long undoOffset = 0;
//        WithPhysicalIo(() =>
//        {
//            var buffer = new byte[Header.RecordSize];
//            ReadBytesAt(currentOffset, buffer);
//            undoOffset = ReserveHeapSpace(Header.RecordSize);
//            WriteBytesAt(undoOffset, buffer);
//        });
//
//        context?.UndoLogIdentifiers.Add(undoOffset);
//        return undoOffset;
//    }
//
//    /// <summary>
//    /// Carica e valida l'header e lo schema da un file ISAM esistente, assicurando che il file sia un database
//    /// valido e che la sua struttura su disco sia compatibile con quella attesa dal codice dell'applicazione.
//    /// </summary>
//    /// <param name="codeSchema">Lo schema (<see cref="Config.IsamSchema"/>) definito nel codice sorgente (tramite `TYPE`). 
//    /// Questo rappresenta la struttura che il programma si aspetta di trovare.</param>
//    /// <returns>
//    /// Una tupla contenente l'oggetto <see cref="Config.FileHeader"/> e l'oggetto <see cref="Config.IsamSchema"/> letti dal file, 
//    /// se tutte le validazioni hanno successo.
//    /// </returns>
//    /// <exception cref="IOException">
//    /// Lanciata se il file non è un database ISAM valido (es. magic number errato) o se la sua versione 
//    /// non è supportata dall'engine corrente.
//    /// </exception>
//    /// <exception cref="InvalidOperationException">
//    /// Lanciata se lo schema del file su disco non è compatibile con lo schema definito nel codice. 
//    /// Questa è una salvaguardia critica per prevenire la corruzione dei dati.
//    /// </exception>
//    /// <remarks>
//    /// L'intero processo è protetto da un lock in lettura per consentire aperture concorrenti sicure del file.
//    /// Esegue una serie di validazioni sequenziali:
//    /// 1. Controlla il "magic number" per confermare il tipo di file.
//    /// 2. Controlla la versione del formato per garantire la compatibilità.
//    /// 3. Deserializza lo schema memorizzato nel file.
//    /// 4. Carica la mappa degli indici secondari.
//    /// 5. Esegue il confronto di compatibilità tra lo schema del file e quello del codice, che è la validazione
//    ///    più importante per garantire l'integrità dei dati.
//    /// </remarks>
//    private (Config.FileHeader header, Config.IsamSchema schema) LoadAndValidateHeader(Config.IsamSchema codeSchema)
//    {
//        _fileLevelLock.EnterReadLock();
//        try
//        {
//            return ReadAndValidateHeaderFromStream(codeSchema);
//        }
//        finally
//        {
//            _fileLevelLock.ExitReadLock();
//        }
//    }
//
//    private (Config.FileHeader header, Config.IsamSchema schema) ReadAndValidateHeaderFromStream(
//        Config.IsamSchema codeSchema)
//    {
//        _backingStream.Seek(0, SeekOrigin.Begin);
//        var header = MarshalHelper.ReadStructure<Config.FileHeader>(_backingStream);
//
//        for (var i = 0; i < Config.MagicBytes.Length; i++)
//        {
//            if (header.Magic[i] != Config.MagicBytes[i])
//            {
//                throw new IOException("Il file non è un database ISAM valido.");
//            }
//        }
//
//        if (!IsamFormatPolicy.IsReadable(header.Version))
//        {
//            throw new IOException(
//                header.Version > IsamFormatPolicy.CurrentFormatVersion
//                    ? $"Versione del database ({header.Version}) più recente del motore ({IsamFormatPolicy.CurrentFormatVersion})."
//                    : $"Versione del database ({header.Version}) precedente al minimo leggibile ({IsamFormatPolicy.MinimumReadableVersion}).");
//        }
//
//        var schemaBuffer = new byte[header.SchemaLength];
//        _backingStream.Seek(header.SchemaOffset, SeekOrigin.Begin);
//        _backingStream.ReadExactly(schemaBuffer, 0, schemaBuffer.Length);
//        var fileSchema = SchemaManager.DeserializeSchema(schemaBuffer);
//
//        if (header.SecondaryIndexMapOffset > 0)
//        {
//            _backingStream.Seek(header.SecondaryIndexMapOffset, SeekOrigin.Begin);
//            _secondaryIndexMap = SchemaManager.DeserializeIndexMapFromStream(_backingStream);
//        }
//
//        if (!SchemaManager.AreSchemasCompatible(codeSchema, fileSchema))
//        {
//            throw new InvalidOperationException(
//                "Schema del TYPE nel codice non corrisponde a quello del file ISAM.");
//        }
//
//        return (header, fileSchema);
//    }
//
//    /// <summary>
//    /// Tenta di aprire un <see cref="FileStream"/> implementando una logica di tentativi multipli per gestire
//    /// errori di accesso temporanei e transitori.
//    /// </summary>
//    /// <param name="path">Il percorso del file da aprire.</param>
//    /// <param name="mode">La modalità con cui aprire il file (es. <see cref="FileMode.Open"/>, <see cref="FileMode.Create"/>).</param>
//    /// <param name="access">Il tipo di accesso richiesto al file (es. <see cref="FileAccess.ReadWrite"/>).</param>
//    /// <param name="share">Specifica come il file può essere condiviso con altri processi.</param>
//    /// <param name="retries">Il numero massimo di tentativi da effettuare prima di lanciare l'eccezione finale. Il valore predefinito è 5.</param>
//    /// <param name="delayMs">La durata della pausa, in millisecondi, tra un tentativo fallito e il successivo. Il valore predefinito è 100.</param>
//    /// <returns>Un'istanza di <see cref="FileStream"/> aperta con successo.</returns>
//    /// <exception cref="IOException">Lancia l'ultima <see cref="IOException"/> catturata se tutti i tentativi falliscono.</exception>
//    /// <remarks>
//    /// Questo metodo helper aumenta la robustezza dell'apertura dei file, in particolare in ambienti multi-processo
//    /// o su sistemi dove altre utility (es. antivirus, software di backup) possono acquisire brevi lock sui file.
//    /// Invece di fallire immediatamente al primo <see cref="IOException"/>, attende per un breve intervallo e riprova,
//    /// aumentando significativamente la probabilità di successo in caso di contesa temporanea del file.
//    /// </remarks>
//    private static FileStream OpenFileWithRetry(string path, FileMode mode, FileAccess access, FileShare share,
//        int retries = 5, int delayMs = 100)
//    {
//        for (var i = 0; i < retries; i++)
//        {
//            try
//            {
//                return new FileStream(path, mode, access, share, 256 * 1024, FileOptions.RandomAccess);
//            }
//            catch (IOException) when (i < retries - 1)
//            {
//                Thread.Sleep(delayMs);
//            }
//        }
//
//        return new FileStream(path, mode, access, share, 256 * 1024, FileOptions.RandomAccess);
//    }
//
//    private static byte[] SerializeHeaderBytes(Config.FileHeader header)
//    {
//        using var stream = new MemoryStream();
//        MarshalHelper.WriteStructure(stream, header);
//        return stream.ToArray();
//    }
//
//    private static Config.FileHeader DeserializeHeaderBytes(byte[] headerBytes)
//    {
//        using var stream = new MemoryStream(headerBytes);
//        return MarshalHelper.ReadStructure<Config.FileHeader>(stream);
//    }
//
//    private void RestoreTransactionSnapshot()
//    {
//        if (_transactionSnapshotHeader is not { } snapshotHeader ||
//            _transactionSnapshotIndexMap is not { } snapshotMap)
//        {
//            return;
//        }
//
//        LockFile();
//        try
//        {
//            Header = snapshotHeader;
//            _secondaryIndexMap = SchemaManager.DeserializeIndexMap(snapshotMap);
//
//            _backingStream.Seek(0, SeekOrigin.Begin);
//            MarshalHelper.WriteStructure(_backingStream, Header);
//            if (Header.SecondaryIndexMapOffset > 0)
//            {
//                _backingStream.Seek(Header.SecondaryIndexMapOffset, SeekOrigin.Begin);
//                _backingStream.Write(snapshotMap, 0, snapshotMap.Length);
//            }
//
//            FlushBackingStream(true);
//        }
//        finally
//        {
//            UnlockFile();
//            _transactionSnapshotHeader = null;
//            _transactionSnapshotIndexMap = null;
//        }
//    }
//
//    private void ResetWriteAheadLog()
//    {
//        _wal?.Dispose();
//        _wal = null;
//        _directWalDirty = false;
//        _pendingDirectMainWrites.Clear();
//        if (_isMemBacked)
//        {
//            return;
//        }
//
//        var walPath = Path.ChangeExtension(FilePath, ".wal");
//        if (File.Exists(walPath))
//        {
//            File.Delete(walPath);
//        }
//    }
//
//    private void InitializeHeapRegionTracking()
//    {
//        var minHeap = HeapRelocation.MinimumHeapStart(this);
//        var header = Header;
//
//        if (header.NextAvailablePageOffset <= minHeap && header.PrimaryKeyRootOffset < 0)
//        {
//            _heapRegionStart = -1;
//            return;
//        }
//
//        if (header.LobHeapStartOffset >= minHeap && header.LobHeapStartOffset < header.NextAvailablePageOffset)
//        {
//            _heapRegionStart = header.LobHeapStartOffset;
//            return;
//        }
//
//        _heapRegionStart = header.NextAvailablePageOffset > minHeap ? minHeap : -1;
//    }
//
//    internal void SyncStructuralStateFromDisk()
//    {
//        if (LockingStrategy != LockingStrategy.CrossProcess)
//        {
//            return;
//        }
//
//        ReloadHeaderFromDisk();
//        InitializeHeapRegionTracking();
//    }
//
//    /// <summary>
//    /// ISAM-B10: prima di ogni scrittura header cross-process, preserva il massimo tra contatori
//    /// in-memory e su disco così un PUT non può sovrascrivere append concurrent.
//    /// </summary>
//    private Config.FileHeader CoalesceAppendCountersFromDisk(Config.FileHeader header)
//    {
//        if (!_coalesceAppendCountersForPut || LockingStrategy != LockingStrategy.CrossProcess)
//        {
//            return header;
//        }
//
//        var diskHeader = ReadHeaderStructureFromStream();
//        if (diskHeader.ActiveRecordCount > header.ActiveRecordCount)
//        {
//            header.ActiveRecordCount = diskHeader.ActiveRecordCount;
//        }
//
//        if (diskHeader.LastRecNumber > header.LastRecNumber)
//        {
//            header.LastRecNumber = diskHeader.LastRecNumber;
//        }
//
//        return header;
//    }
//
//    private void RegisterHeapAllocation(long start, long end)
//    {
//        if (_heapRegionStart >= 0 && start >= _heapRegionStart)
//        {
//            return;
//        }
//
//        _heapRegionStart = _heapRegionStart < 0 ? start : Math.Min(_heapRegionStart, start);
//        var header = Header;
//        if (header.LobHeapStartOffset == _heapRegionStart)
//        {
//            return;
//        }
//
//        header.LobHeapStartOffset = _heapRegionStart;
//        UpdateHeader(header);
//    }
//
//    private void EnsureHeapStartsAtOrAbove(long requiredStart)
//    {
//        if (_heapRegionStart < 0)
//        {
//            var header = Header;
//            if (header.NextAvailablePageOffset < requiredStart)
//            {
//                header.NextAvailablePageOffset = requiredStart;
//                UpdateHeader(header);
//            }
//
//            return;
//        }
//
//        if (requiredStart <= _heapRegionStart)
//        {
//            return;
//        }
//
//        RelocateHeapRegion(requiredStart);
//    }
//
//    private void RelocateHeapRegion(long newStart)
//    {
//        using var _ = IsamSlowOpLog.Measure("heap-reloc", $"{FilePath} newStart={newStart}");
//        var oldStart = _heapRegionStart;
//        var oldEnd = Header.NextAvailablePageOffset;
//        var length = oldEnd - oldStart;
//        if (length <= 0)
//        {
//            _heapRegionStart = newStart;
//            return;
//        }
//
//        var delta = newStart - oldStart;
//        var buffer = new byte[length];
//        ReadBytesAt(oldStart, buffer);
//        _backingStream.Seek(newStart, SeekOrigin.Begin);
//        _backingStream.Write(buffer, 0, buffer.Length);
//
//        // Stesso bug di EnsureSchemaSlot: scrittura diretta sullo stream, ReadPageCache non
//        // invalidato senza questa chiamata esplicita — vedi commento lì per il sintomo osservato.
//        var invalidateStart = Math.Min(oldStart, newStart);
//        var invalidateEnd = Math.Max(oldEnd, newStart + length);
//        InvalidatePageCacheForRange(invalidateStart, (int)Math.Min(int.MaxValue, invalidateEnd - invalidateStart));
//
//        ShiftHeaderPagePointers(oldStart, oldEnd, delta);
//        ShiftPageFreeList(oldStart, oldEnd, delta);
//        RewriteLobPointersInRecords(oldStart, oldEnd, delta);
//
//        AfterHeapRelocated?.Invoke(oldStart, oldEnd, delta);
//
//        // Vedi commento in EnsureSchemaSlot: le SaveNode innescate da AfterHeapRelocated possono
//        // finire in coda (QueueDirectMainWrite) invece che sullo stream — applichiamola subito.
//        if (ShouldUseDirectWal())
//        {
//            ApplyPendingDirectMainWrites();
//        }
//
//        var header = Header;
//        header.NextAvailablePageOffset = newStart + length;
//        header.LobHeapStartOffset = newStart;
//        UpdateHeader(header);
//        _heapRegionStart = newStart;
//    }
//
//    private void ShiftHeaderPagePointers(long heapStart, long heapEnd, long delta)
//    {
//        var header = Header;
//        if (header.PrimaryKeyRootOffset >= heapStart && header.PrimaryKeyRootOffset < heapEnd)
//        {
//            header.PrimaryKeyRootOffset += delta;
//        }
//
//        for (var i = 0; i < header.SecondaryIndexRootOffsets.Length; i++)
//        {
//            if (header.SecondaryIndexRootOffsets[i] >= heapStart &&
//                header.SecondaryIndexRootOffsets[i] < heapEnd)
//            {
//                header.SecondaryIndexRootOffsets[i] += delta;
//            }
//        }
//
//        Header = header;
//    }
//
//    private void ShiftPageFreeList(long heapStart, long heapEnd, long delta)
//    {
//        var header = Header;
//        if (header.PageFreeListHeadOffset >= heapStart && header.PageFreeListHeadOffset < heapEnd)
//        {
//            header.PageFreeListHeadOffset += delta;
//        }
//
//        Header = header;
//    }
//
//    private void RewriteLobPointersInRecords(long heapStart, long heapEnd, long delta)
//    {
//        var jsonOffsets = HeapRelocation.GetJsonFieldOffsets(Schema);
//        if (jsonOffsets.Length == 0)
//        {
//            return;
//        }
//
//        Span<byte> ptrBuf = stackalloc byte[sizeof(long)];
//        for (long i = 0; i < Header.LastRecNumber; i++)
//        {
//            var recordOffset = DataAreaStartOffset + i * Header.RecordSize;
//            foreach (var fieldOffset in jsonOffsets)
//            {
//                ReadBytesAt(recordOffset + fieldOffset, ptrBuf);
//                var ptr = BitConverter.ToInt64(ptrBuf);
//                if (ptr < heapStart || ptr >= heapEnd)
//                {
//                    continue;
//                }
//
//                var newPtr = ptr + delta;
//                _backingStream.Seek(recordOffset + fieldOffset, SeekOrigin.Begin);
//                _backingStream.Write(BitConverter.GetBytes(newPtr));
//            }
//        }
//    }
//
//    #endregion
//    
//}
//
///// <summary>
///// Scope RAII per il lock strutturale ISAM (ISAM-B3).
///// </summary>
//public readonly struct StructuralLockScope : IDisposable
//{
//    private readonly IsamFileHandle? _handle;
//
//    internal StructuralLockScope(IsamFileHandle handle)
//    {
//        _handle = handle;
//        _handle.EnterStructuralLockCore();
//    }
//
//    public void Dispose() => _handle?.ExitStructuralLockCore();
//}
//
///// <summary>Scope RAII read lock strutturale cross-process (ISAM-B10).</summary>
//public readonly struct FileStructureReadScope : IDisposable
//{
//    private readonly IsamFileHandle? _handle;
//
//    internal FileStructureReadScope(IsamFileHandle handle)
//    {
//        _handle = handle;
//        _handle.EnterFileStructureReadLock();
//    }
//
//    public void Dispose() => _handle?.ExitFileStructureReadLock();
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */








/* C#: prop IsamFileHandle.get_UseMmapReadOnly */



















































































































#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif

static int fh_rd_i32_le(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}

static int fh_rd_i64_le(FILE* f, int64_t* v)
{
    uint8_t b[8];
    uint64_t u = 0;
    int i;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)b[i] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

static int fh_skip_7bit_str(FILE* f)
{
    unsigned v = 0, shift = 0;
    for (;;) {
        int c = fgetc(f);
        if (c < 0) return -1;
        v |= (unsigned)(c & 0x7f) << shift;
        if ((c & 0x80) == 0) break;
        shift += 7;
        if (shift > 28) return -1;
    }
    if (v && fseek(f, (long)v, SEEK_CUR) != 0) return -1;
    return 0;
}

static int64_t fh_index_map_len(FILE* f, int64_t mapOff)
{
    int32_t count = 0, i;
    long start, end;
    int64_t dummy;
    if (fseek(f, (long)mapOff, SEEK_SET) != 0) return -1;
    start = ftell(f);
    if (fh_rd_i32_le(f, &count) != 0 || count < 0) return -1;
    for (i = 0; i < count; i++) {
        if (fh_skip_7bit_str(f) != 0) return -1;
        if (fh_rd_i64_le(f, &dummy) != 0) return -1;
    }
    end = ftell(f);
    return (int64_t)end - (int64_t)start;
}

static int64_t fh_data_area_start(IsamFileHandle* self)
{
    int64_t mapLen;
    if (self->Header.SecondaryIndexMapOffset > 0) {
        mapLen = fh_index_map_len(self->fs, self->Header.SecondaryIndexMapOffset);
        if (mapLen < 0) return self->Header.SchemaOffset + self->Header.SchemaLength;
        return self->Header.SecondaryIndexMapOffset + mapLen;
    }
    return self->Header.SchemaOffset + self->Header.SchemaLength;
}

static void fh_init_heap_region(IsamFileHandle* self)
{
    int64_t minHeap = HeapRelocation_AlignUp(
        self->dataAreaStart + self->Header.LastRecNumber * self->Header.RecordSize,
        HeapRelocation_PageSize);
    if (self->Header.NextAvailablePageOffset <= minHeap && self->Header.PrimaryKeyRootOffset < 0) {
        self->heapRegionStart = -1;
        return;
    }
    if (self->Header.LobHeapStartOffset >= minHeap
        && self->Header.LobHeapStartOffset < self->Header.NextAvailablePageOffset) {
        self->heapRegionStart = self->Header.LobHeapStartOffset;
        return;
    }
    self->heapRegionStart = self->Header.NextAvailablePageOffset > minHeap ? minHeap : -1;
}

int IsamFileHandle_UpdateHeader(IsamFileHandle* self)
{
    if (!self || !self->fs) return GMB_DB_ERR;
    return IsamFileHeader_Write(self->fs, &self->Header);
}

int IsamFileHandle_Flush(IsamFileHandle* self)
{
    if (!self || !self->fs) return GMB_DB_ERR;
    return fflush(self->fs) == 0 ? GMB_DB_OK : GMB_DB_ERR;
}

int64_t IsamFileHandle_DataAreaStartOffset(const IsamFileHandle* self)
{
    return self ? self->dataAreaStart : -1;
}

/* ponytail: coda intera in un malloc; per file enormi streamare all'indietro. */
static int fh_shift_tail(FILE* fs, int64_t tailStart, int64_t delta)
{
    struct stat st;
    int64_t fileLen, tailLen;
    uint8_t* tail = NULL;
    if (delta <= 0) return GMB_DB_OK;
    if (fstat(fileno(fs), &st) != 0) return GMB_DB_ERR;
    fileLen = (int64_t)st.st_size;
    tailLen = fileLen > tailStart ? fileLen - tailStart : 0;
    if (tailLen > 0) {
        tail = (uint8_t*)malloc((size_t)tailLen);
        if (!tail) return GMB_DB_ERR;
        if (fseek(fs, (long)tailStart, SEEK_SET) != 0) { free(tail); return GMB_DB_ERR; }
        if ((int64_t)fread(tail, 1, (size_t)tailLen, fs) != tailLen) { free(tail); return GMB_DB_ERR; }
    }
    if (ftruncate(fileno(fs), (off_t)(fileLen + delta)) != 0) { free(tail); return GMB_DB_ERR; }
    if (tailLen > 0) {
        if (fseek(fs, (long)(tailStart + delta), SEEK_SET) != 0) { free(tail); return GMB_DB_ERR; }
        if ((int64_t)fwrite(tail, 1, (size_t)tailLen, fs) != tailLen) { free(tail); return GMB_DB_ERR; }
        free(tail);
    }
    return fflush(fs) == 0 ? GMB_DB_OK : GMB_DB_ERR;
}

static void fh_shift_header_ptrs(IsamFileHandle* self, int64_t heapStart, int64_t heapEnd, int64_t delta)
{
    int i;
    IsamFileHeader* h = &self->Header;
    if (h->PrimaryKeyRootOffset >= heapStart && h->PrimaryKeyRootOffset < heapEnd)
        h->PrimaryKeyRootOffset += delta;
    for (i = 0; i < 15; i++) {
        if (h->SecondaryIndexRootOffsets[i] >= heapStart && h->SecondaryIndexRootOffsets[i] < heapEnd)
            h->SecondaryIndexRootOffsets[i] += delta;
    }
    if (h->PageFreeListHeadOffset >= heapStart && h->PageFreeListHeadOffset < heapEnd)
        h->PageFreeListHeadOffset += delta;
}

/* Port di FileHandle.RewriteLobPointersInRecords: dopo uno shift fisico della coda,
 * aggiorna i puntatori LOB JSON dentro i record (valori assoluti + delta se in range). */
static void fh_rewrite_lob_pointers(IsamFileHandle* self, int64_t heapStart, int64_t heapEnd, int64_t delta)
{
    int64_t jsonOffsets[64];
    int nf, i;
    int64_t recordSize, rec;
    if (!self || !self->Schema) return;
    nf = HeapRelocation_GetJsonFieldOffsets(self->Schema, jsonOffsets, 64);
    if (nf == 0) return;
    recordSize = self->Header.RecordSize;
    for (rec = 0; rec < self->Header.LastRecNumber; rec++) {
        int64_t recordOffset = self->dataAreaStart + rec * recordSize;
        for (i = 0; i < nf; i++) {
            int64_t ptr = 0;
            int64_t pos = recordOffset + jsonOffsets[i];
            if (pos < 0) continue;
            if (fseeko(self->fs, (off_t)pos, SEEK_SET) != 0) continue;
            if (fread(&ptr, 1, 8, self->fs) != 8) continue;
            if (ptr < heapStart || ptr >= heapEnd) continue;
            ptr += delta;
            if (fseeko(self->fs, (off_t)pos, SEEK_SET) != 0) continue;
            if (fwrite(&ptr, 1, 8, self->fs) != 8) continue;
        }
    }
}

int IsamFileHandle_EnsureSchemaSlot(IsamFileHandle* self, int requiredSchemaBytes)
{
    int64_t slotEnd, slotCapacity, delta, tailStart, tailEnd;
    struct stat st;
    if (!self || requiredSchemaBytes < 0) return GMB_DB_ERR;
    slotEnd = self->Header.SecondaryIndexMapOffset > 0
        ? self->Header.SecondaryIndexMapOffset
        : self->Header.SchemaOffset + self->Header.SchemaLength;
    slotCapacity = slotEnd - self->Header.SchemaOffset;
    if ((int64_t)requiredSchemaBytes <= slotCapacity) return GMB_DB_OK;
    delta = (int64_t)requiredSchemaBytes - slotCapacity;
    tailStart = slotEnd;
    if (fh_shift_tail(self->fs, tailStart, delta) != GMB_DB_OK) return GMB_DB_ERR;
    if (fstat(fileno(self->fs), &st) != 0) return GMB_DB_ERR;
    tailEnd = (int64_t)st.st_size;
    fh_shift_header_ptrs(self, tailStart, tailEnd, delta);
    if (self->Header.SecondaryIndexMapOffset > 0)
        self->Header.SecondaryIndexMapOffset += delta;
    if (self->Header.NextAvailablePageOffset >= tailStart)
        self->Header.NextAvailablePageOffset += delta;
    if (self->Header.LobHeapStartOffset >= tailStart)
        self->Header.LobHeapStartOffset += delta;
    if (self->Header.FreeListHeadOffset >= tailStart)
        self->Header.FreeListHeadOffset += delta;
    /* aggiorna dataAreaStart/heapRegionStart PRIMA della rewrite dei puntatori LOB:
     * i record si sono fisicamente spostati di delta, vanno letti alla nuova posizione. */
    if (self->dataAreaStart >= tailStart)
        self->dataAreaStart += delta;
    if (self->heapRegionStart >= tailStart)
        self->heapRegionStart += delta;
    fh_rewrite_lob_pointers(self, tailStart, tailEnd, delta);
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return GMB_DB_ERR;
    if (self->AfterHeapRelocated)
        self->AfterHeapRelocated(self->afterHeapCtx, tailStart, tailEnd, delta);
    return GMB_DB_OK;
}

int IsamFileHandle_WriteSchemaBlockForReplication(IsamFileHandle* self, const uint8_t* isch, int len)
{
    IsamFieldSchema fields[64];
    IsamIndexDefinition secs[15];
    IsamSchema parsed;
    IsamSchema* live;
    int i;
    if (!self || !isch || len <= 0) return GMB_DB_ARG;
    memset(&parsed, 0, sizeof(parsed));
    memset(fields, 0, sizeof(fields));
    memset(secs, 0, sizeof(secs));
    if (SchemaManager_DeserializeSchema(isch, len, &parsed, fields, 64, secs, 15) != GMB_DB_OK)
        return GMB_DB_ERR;
    if (IsamFileHandle_EnsureSchemaSlot(self, len) != GMB_DB_OK) return GMB_DB_ERR;
    self->Header.SchemaLength = len;
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return GMB_DB_ERR;
    if (fseek(self->fs, (long)self->Header.SchemaOffset, SEEK_SET) != 0) return GMB_DB_ERR;
    if ((int)fwrite(isch, 1, (size_t)len, self->fs) != len) return GMB_DB_ERR;
    if (fflush(self->fs) != 0) return GMB_DB_ERR;
    live = (IsamSchema*)(void*)self->Schema;
    if (!live) return GMB_DB_OK;
    snprintf(live->TypeName, sizeof(live->TypeName), "%s", parsed.TypeName);
    live->PrimaryKey = parsed.PrimaryKey;
    /* ponytail: no in-place field migration — C# sostituisce l'oggetto Schema; qui si aggiorna
     * solo se il buffer Fields del caller ha già abbastanza slot (stesso numero o meno). */
    if (live->Fields && parsed.FieldCount > 0 && parsed.FieldCount <= live->FieldCount) {
        for (i = 0; i < parsed.FieldCount; i++) live->Fields[i] = fields[i];
        live->FieldCount = parsed.FieldCount;
    }
    if (live->SecondaryKeys) {
        int n = parsed.SecondaryKeyCount;
        if (n > 15) n = 15;
        for (i = 0; i < n; i++) live->SecondaryKeys[i] = secs[i];
        live->SecondaryKeyCount = n;
    }
    return GMB_DB_OK;
}

IsamFileHandle* IsamFileHandle_Open(const char* filePath, const IsamSchema* schema)
{
    IsamFileHandle* self;
    char key[256], backing[1024];
    const char* fsPath = filePath;
    if (!filePath || !schema) return NULL;
    self = (IsamFileHandle*)calloc(1, sizeof(*self));
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", filePath);
    self->Schema = schema;
    self->ownsFile = 1;
    if (IsamMemPath_IsMemPath(filePath)) {
        if (IsamMemPath_NormalizeKey(filePath, key, (int)sizeof(key)) != GMB_DB_OK) {
            free(self);
            return NULL;
        }
        if (!IsamMemStore_Exists(key)) {
            if (IsamFileHandle_CreateNew(filePath, schema) != GMB_DB_OK) {
                free(self);
                return NULL;
            }
        }
        if (IsamMemStore_BackingPath(key, backing, (int)sizeof(backing)) != GMB_DB_OK) {
            free(self);
            return NULL;
        }
        fsPath = backing;
    } else {
        FILE* probe = fopen(filePath, "rb");
        if (probe) {
            fclose(probe);
            if (IsamOfflineMigrator_MigrateIfNeeded(filePath, schema) < 0) {
                free(self);
                return NULL;
            }
        }
    }
    self->fs = fopen(fsPath, "rb+");
    if (!self->fs || IsamFileHeader_Read(self->fs, &self->Header) != GMB_DB_OK) {
        if (self->fs) fclose(self->fs);
        free(self);
        return NULL;
    }
    self->dataAreaStart = fh_data_area_start(self);
    fh_init_heap_region(self);
    return self;
}

void IsamFileHandle_Dispose(IsamFileHandle* self)
{
    if (!self) return;
    IsamFileHandle_DisableMmapReadOnly(self);
    free(self->undoGridOff);
    free(self->undoHeapOff);
    if (self->ownsFile && self->fs) fclose(self->fs);
    free(self);
}

static void fh_register_heap_allocation(IsamFileHandle* self, int64_t start);

/* Port 1:1 di FileHandle.RelocateHeapRegion: sposta la regione heap [oldStart,oldEnd)
 * (pagine B+ + LOB) in newStart, aggiornando i puntatori assoluti (header, free-list,
 * puntatori LOB nei record) e notificando la callback AfterHeapRelocated. */
int64_t IsamFileHandle_RelocateHeapRegion(IsamFileHandle* self, int64_t newStart)
{
    int64_t oldStart, oldEnd, length, delta;
    uint8_t* buf;
    if (!self) return GMB_DB_ERR;
    oldStart = self->heapRegionStart;
    oldEnd = self->Header.NextAvailablePageOffset;
    length = oldEnd - oldStart;
    if (length <= 0) { self->heapRegionStart = newStart; return GMB_DB_OK; }
    if (length > 0x7fffffff) return GMB_DB_ERR;   /* IsamFileHandle_ReadBytesAt usa int */
    delta = newStart - oldStart;
    buf = (uint8_t*)malloc((size_t)length);
    if (!buf) return GMB_DB_ERR;
    if (IsamFileHandle_ReadBytesAt(self, oldStart, buf, (int)length) != GMB_DB_OK) { free(buf); return GMB_DB_ERR; }
    if (fseeko(self->fs, (off_t)newStart, SEEK_SET) != 0 ||
        fwrite(buf, 1, (size_t)length, self->fs) != (size_t)length) { free(buf); return GMB_DB_ERR; }
    free(buf);
    fh_shift_header_ptrs(self, oldStart, oldEnd, delta);
    fh_rewrite_lob_pointers(self, oldStart, oldEnd, delta);
    if (self->AfterHeapRelocated)
        self->AfterHeapRelocated(self->afterHeapCtx, oldStart, oldEnd, delta);
    self->Header.NextAvailablePageOffset = newStart + length;
    self->Header.LobHeapStartOffset = newStart;
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return GMB_DB_ERR;
    self->heapRegionStart = newStart;
    return GMB_DB_OK;
}

/* Port 1:1 di FileHandle.EnsureHeapStartsAtOrAbove. */
int IsamFileHandle_EnsureHeapStartsAtOrAbove(IsamFileHandle* self, int64_t requiredStart)
{
    if (!self) return GMB_DB_ERR;
    if (self->heapRegionStart < 0) {
        if (self->Header.NextAvailablePageOffset < requiredStart) {
            self->Header.NextAvailablePageOffset = requiredStart;
            IsamFileHandle_UpdateHeader(self);
        }
        return GMB_DB_OK;
    }
    if (requiredStart <= self->heapRegionStart) return GMB_DB_OK;
    /* Amortizzazione: sposta la regione heap (pagine B+ + LOB) con un GAP oltre l'area
     * record, cosi' la relocation avviene ogni ~GAP byte di record e non ad ogni record
     * (la relocation muove l'intera regione: senza gap sarebbe O(n^2)). */
    return (int)IsamFileHandle_RelocateHeapRegion(self, requiredStart + (int64_t)(1 << 22));
}

static void fh_ensure_heap(IsamFileHandle* self, int64_t requiredStart)
{
    (void)IsamFileHandle_EnsureHeapStartsAtOrAbove(self, requiredStart);
}

/* Wrapper pubblico di fh_register_heap_allocation (chiamato dall'allocazione pagine B+). */
void IsamFileHandle_RegisterHeapAllocation(IsamFileHandle* self, int64_t start)
{
    fh_register_heap_allocation(self, start);
}

int64_t IsamFileHandle_AllocateNewRecordOffsetCore(IsamFileHandle* self)
{
    int64_t allocatedOffset, requiredHeap, requiredLength;
    if (!self) return -1;
    if (self->Header.FreeListHeadOffset != -1) {
        int64_t next = -1;
        allocatedOffset = self->Header.FreeListHeadOffset;
        fseek(self->fs, (long)(allocatedOffset + 1), SEEK_SET);
        if (fh_rd_i64_le(self->fs, &next) != 0) return -1;
        self->Header.FreeListHeadOffset = next;
    } else {
        allocatedOffset = self->dataAreaStart + self->Header.LastRecNumber * self->Header.RecordSize;
        requiredHeap = HeapRelocation_AlignUp(allocatedOffset + self->Header.RecordSize, HeapRelocation_PageSize);
        fh_ensure_heap(self, requiredHeap);
        self->Header.LastRecNumber += 1;
        requiredLength = allocatedOffset + self->Header.RecordSize;
        {
            struct stat st;
            if (fstat(fileno(self->fs), &st) == 0 && (int64_t)st.st_size < requiredLength) {
                if (ftruncate(fileno(self->fs), (off_t)requiredLength) != 0) return -1;
            }
        }
    }
    self->Header.ActiveRecordCount += 1;
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return -1;
    return allocatedOffset;
}

/* Port 1:1 di FileHandle.AllocateNewRecordOffsetBatchCore. */
int IsamFileHandle_AllocateNewRecordOffsetBatchCore(IsamFileHandle* self, int64_t* offsets, int count)
{
    int allocated = 0;
    int64_t recordSize, seq, startRec, i, requiredLength, lastOffset;
    if (!self || !offsets || count <= 0) return GMB_DB_ARG;
    recordSize = self->Header.RecordSize;
    while (allocated < count && self->Header.FreeListHeadOffset != -1) {
        int64_t next = -1, off = self->Header.FreeListHeadOffset;
        if (fseeko(self->fs, (off_t)(off + 1), SEEK_SET) != 0) return GMB_DB_ERR;
        if (fh_rd_i64_le(self->fs, &next) != 0) return GMB_DB_ERR;
        self->Header.FreeListHeadOffset = next;
        offsets[allocated++] = off;
    }
    if (allocated < count) {
        seq = count - allocated;
        startRec = self->Header.LastRecNumber;
        lastOffset = self->dataAreaStart + (startRec + seq - 1) * recordSize;
        (void)IsamFileHandle_EnsureHeapStartsAtOrAbove(self,
            HeapRelocation_AlignUp(lastOffset + recordSize, HeapRelocation_PageSize));
        self->Header.LastRecNumber += seq;
        for (i = 0; i < seq; i++)
            offsets[allocated + i] = self->dataAreaStart + (startRec + i) * recordSize;
        allocated = count;
        requiredLength = lastOffset + recordSize;
        {
            struct stat st;
            if (fstat(fileno(self->fs), &st) == 0 && (int64_t)st.st_size < requiredLength)
                if (ftruncate(fileno(self->fs), (off_t)requiredLength) != 0) return GMB_DB_ERR;
        }
    }
    self->Header.ActiveRecordCount += count;
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return GMB_DB_ERR;
    return GMB_DB_OK;
}

int IsamFileHandle_WriteRecordFromBuffer(IsamFileHandle* self, int64_t offset, const uint8_t* buf, int len)
{
    if (!self || !buf || len <= 0) return GMB_DB_ERR;
    fseek(self->fs, (long)offset, SEEK_SET);
    if ((int)fwrite(buf, 1, (size_t)len, self->fs) != len) return GMB_DB_ERR;
    return GMB_DB_OK;
}

void IsamFileHandle_DisableMmapReadOnly(IsamFileHandle* self)
{
    if (!self) return;
#ifndef _WIN32
    if (self->mmapPtr && self->mmapPtr != MAP_FAILED && self->mmapLen > 0)
        munmap(self->mmapPtr, self->mmapLen);
#endif
    self->mmapPtr = NULL;
    self->mmapLen = 0;
    self->useMmapReadOnly = 0;
}

int IsamFileHandle_EnableMmapReadOnly(IsamFileHandle* self)
{
#ifndef _WIN32
    struct stat st;
    void* p;
#endif
    if (!self || !self->fs) return GMB_DB_ARG;
    if (self->useMmapReadOnly) return GMB_DB_OK;
    if (IsamMemPath_IsMemPath(self->FilePath)) return GMB_DB_OK;
    fflush(self->fs);
#ifndef _WIN32
    if (fstat(fileno(self->fs), &st) != 0 || st.st_size <= 0) return GMB_DB_OK;
    p = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_SHARED, fileno(self->fs), 0);
    if (p == MAP_FAILED) return GMB_DB_OK; /* C#: fallback silenzioso su stream */
    self->mmapPtr = (uint8_t*)p;
    self->mmapLen = (size_t)st.st_size;
    self->useMmapReadOnly = 1;
#endif
    return GMB_DB_OK;
}

int IsamFileHandle_get_UseMmapReadOnly(const IsamFileHandle* self)
{
    return self && self->useMmapReadOnly;
}

int IsamFileHandle_ReadBytesAt(IsamFileHandle* self, int64_t offset, uint8_t* buf, int len)
{
    if (!self || !buf || len <= 0 || offset < 0) return GMB_DB_ERR;
    if (self->useMmapReadOnly && self->mmapPtr) {
        fflush(self->fs);
        if ((size_t)offset + (size_t)len <= self->mmapLen) {
            memcpy(buf, self->mmapPtr + offset, (size_t)len);
            return GMB_DB_OK;
        }
    }
    fseek(self->fs, (long)offset, SEEK_SET);
    if ((int)fread(buf, 1, (size_t)len, self->fs) != len) return GMB_DB_ERR;
    return GMB_DB_OK;
}

int IsamFileHandle_ReadRecordBytes(IsamFileHandle* self, int64_t offset, uint8_t* buf, int len)
{
    return IsamFileHandle_ReadBytesAt(self, offset, buf, len);
}

int IsamFileHandle_ReadRecordMetadata(IsamFileHandle* self, int64_t offset, RecordMetadata* out)
{
    uint8_t buf[RecordMetadata_Size];
    if (!out) return GMB_DB_ERR;
    if (IsamFileHandle_ReadRecordBytes(self, offset, buf, RecordMetadata_Size) != GMB_DB_OK) return GMB_DB_ERR;
    memcpy(out, buf, RecordMetadata_Size);
    return GMB_DB_OK;
}

int IsamFileHandle_WriteRecordMetadata(IsamFileHandle* self, int64_t offset, const RecordMetadata* meta)
{
    if (!meta) return GMB_DB_ERR;
    return IsamFileHandle_WriteRecordFromBuffer(self, offset, (const uint8_t*)meta, RecordMetadata_Size);
}

static int64_t fh_file_length(IsamFileHandle* self)
{
    struct stat st;
    if (fstat(fileno(self->fs), &st) != 0) return -1;
    return (int64_t)st.st_size;
}

static int64_t fh_get_heap_allocation_offset(IsamFileHandle* self)
{
    int64_t minHeap, start, flen;
    if (!self) return -1;
    minHeap = HeapRelocation_AlignUp(
        self->dataAreaStart + self->Header.LastRecNumber * self->Header.RecordSize,
        HeapRelocation_PageSize);
    start = self->Header.NextAvailablePageOffset;
    flen = fh_file_length(self);
    if (flen > start) start = flen;
    if (minHeap > start) start = minHeap;
    return start;
}

static void fh_register_heap_allocation(IsamFileHandle* self, int64_t start)
{
    if (self->heapRegionStart >= 0 && start >= self->heapRegionStart) return;
    self->heapRegionStart = self->heapRegionStart < 0 ? start
        : (start < self->heapRegionStart ? start : self->heapRegionStart);
    if (self->Header.LobHeapStartOffset == self->heapRegionStart) return;
    self->Header.LobHeapStartOffset = self->heapRegionStart;
    IsamFileHandle_UpdateHeader(self);
}

int64_t IsamFileHandle_ReserveHeapSpace(IsamFileHandle* self, int64_t size)
{
    int64_t start, requiredLength, flen;
    if (!self || size <= 0) return -1;
    start = fh_get_heap_allocation_offset(self);
    requiredLength = start + size;
    flen = fh_file_length(self);
    if (flen >= 0 && flen < requiredLength) {
        if (ftruncate(fileno(self->fs), (off_t)requiredLength) != 0) return -1;
    }
    self->Header.NextAvailablePageOffset = requiredLength;
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return -1;
    fh_register_heap_allocation(self, start);
    return start;
}

/* Large Object (LOB) Heap — port 1:1 di FileHandle.WriteLob/ReadLob (C#).
 * Formato (identico al C# FileHandle.cs): [int32 LE lunghezza][dati],
 * allocato nell'heap append-only condiviso (LOB + pagine B+). */
int64_t IsamFileHandle_WriteLob(IsamFileHandle* self, const uint8_t* data, int len)
{
    int64_t start, requiredLength, flen;
    uint8_t hdr[4];
    if (!self || !data || len < 0) return -1;
    start = fh_get_heap_allocation_offset(self);
    requiredLength = start + 4 + len;
    flen = fh_file_length(self);
    if (flen < requiredLength) {
        if (ftruncate(fileno(self->fs), (off_t)requiredLength) != 0) return -1;
    }
    if (fseek(self->fs, (long)start, SEEK_SET) != 0) return -1;
    hdr[0] = (uint8_t)(len & 0xff);
    hdr[1] = (uint8_t)((len >> 8) & 0xff);
    hdr[2] = (uint8_t)((len >> 16) & 0xff);
    hdr[3] = (uint8_t)((len >> 24) & 0xff);
    if (fwrite(hdr, 1, 4, self->fs) != 4) return -1;
    if (len > 0 && (int)fwrite(data, 1, (size_t)len, self->fs) != len) return -1;
    self->Header.NextAvailablePageOffset = requiredLength;
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return -1;
    fh_register_heap_allocation(self, start);
    return start;
}

/* Legge un LOB dal heap. Ritorna la lunghezza del payload (>=0), GMB_DB_ERR se invalido. */
int IsamFileHandle_ReadLob(IsamFileHandle* self, int64_t offset, char* out, int cap)
{
    int32_t len;
    int n;
    if (!self || offset < 0) return GMB_DB_ERR;
    if (fseek(self->fs, (long)offset, SEEK_SET) != 0) return GMB_DB_ERR;
    if (fh_rd_i32_le(self->fs, &len) != 0 || len < 0) return GMB_DB_ERR;
    if (out && cap > 0) {
        n = len < cap - 1 ? len : cap - 1;
        if (n > 0 && (int)fread(out, 1, (size_t)n, self->fs) != n) return GMB_DB_ERR;
        out[n] = '\0';
    }
    return (int)len;
}

static void fh_tx_clear_undo(IsamFileHandle* self)
{
    free(self->undoGridOff);
    free(self->undoHeapOff);
    self->undoGridOff = NULL;
    self->undoHeapOff = NULL;
    self->undoN = 0;
    self->undoCap = 0;
}

static int fh_tx_push_undo(IsamFileHandle* self, int64_t gridOff, int64_t heapOff)
{
    int64_t *g, *h;
    int cap;
    if (self->undoN >= self->undoCap) {
        cap = self->undoCap ? self->undoCap * 2 : 8;
        g = (int64_t*)realloc(self->undoGridOff, (size_t)cap * sizeof(int64_t));
        h = (int64_t*)realloc(self->undoHeapOff, (size_t)cap * sizeof(int64_t));
        if (!g || !h) {
            if (g) self->undoGridOff = g;
            if (h) self->undoHeapOff = h;
            return GMB_DB_ERR;
        }
        self->undoGridOff = g;
        self->undoHeapOff = h;
        self->undoCap = cap;
    }
    self->undoGridOff[self->undoN] = gridOff;
    self->undoHeapOff[self->undoN] = heapOff;
    self->undoN++;
    return GMB_DB_OK;
}

static void fh_tx_delete_wal(IsamFileHandle* self)
{
    char wal[1080];
    char* dot;
    if (!self || !self->FilePath[0] || IsamMemPath_IsMemPath(self->FilePath)) return;
    snprintf(wal, sizeof(wal), "%s", self->FilePath);
    dot = strrchr(wal, '.');
    if (dot && !strchr(dot, '/'))
        snprintf(dot, sizeof(wal) - (size_t)(dot - wal), ".wal");
    else if (strlen(wal) + 4 < sizeof(wal))
        strcat(wal, ".wal");
    unlink(wal);
}

int64_t IsamFileHandle_WriteUndoVersion(IsamFileHandle* self, int64_t currentOffset)
{
    int recSize;
    uint8_t* buf;
    int64_t undoOffset;
    if (!self || currentOffset < 0) return -1;
    recSize = self->Header.RecordSize;
    buf = (uint8_t*)malloc((size_t)recSize);
    if (!buf) return -1;
    if (IsamFileHandle_ReadRecordBytes(self, currentOffset, buf, recSize) != GMB_DB_OK) {
        free(buf);
        return -1;
    }
    undoOffset = IsamFileHandle_ReserveHeapSpace(self, recSize);
    if (undoOffset < 0) { free(buf); return -1; }
    if (IsamFileHandle_WriteRecordFromBuffer(self, undoOffset, buf, recSize) != GMB_DB_OK) {
        free(buf);
        return -1;
    }
    free(buf);
    if (self->hasTxSnapshot && fh_tx_push_undo(self, currentOffset, undoOffset) != GMB_DB_OK)
        return -1;
    return undoOffset;
}

int64_t IsamFileHandle_BeginTransaction(IsamFileHandle* self)
{
    if (!self) return 0;
    if (self->hasTxSnapshot) return self->currentTxId;
    self->txSnapshotHeader = self->Header;
    self->hasTxSnapshot = 1;
    self->nextTxId++;
    if (self->nextTxId <= 0) self->nextTxId = 1;
    self->currentTxId = self->nextTxId;
    return self->currentTxId;
}

int IsamFileHandle_CommitTransaction(IsamFileHandle* self)
{
    if (!self || !self->hasTxSnapshot) return GMB_DB_ERR;
    fh_tx_clear_undo(self);
    self->hasTxSnapshot = 0;
    self->currentTxId = 0;
    fh_tx_delete_wal(self);
    return GMB_DB_OK;
}

int IsamFileHandle_RollbackTransaction(IsamFileHandle* self)
{
    int i, recSize;
    uint8_t* buf;
    if (!self || !self->hasTxSnapshot) return GMB_DB_ERR;
    recSize = self->Header.RecordSize;
    buf = recSize > 0 ? (uint8_t*)malloc((size_t)recSize) : NULL;
    if (recSize > 0 && !buf) return GMB_DB_ERR;
    for (i = self->undoN - 1; i >= 0; i--) {
        if (!buf) break;
        if (IsamFileHandle_ReadRecordBytes(self, self->undoHeapOff[i], buf, recSize) != GMB_DB_OK) {
            free(buf);
            return GMB_DB_ERR;
        }
        if (IsamFileHandle_WriteRecordFromBuffer(self, self->undoGridOff[i], buf, recSize) != GMB_DB_OK) {
            free(buf);
            return GMB_DB_ERR;
        }
    }
    free(buf);
    MvccUndoGc_ReclaimTransactionalUndoSlots(self, self->undoHeapOff, self->undoN);
    self->Header = self->txSnapshotHeader;
    fh_init_heap_region(self);
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return GMB_DB_ERR;
    if (IsamFileHandle_Flush(self) != GMB_DB_OK) return GMB_DB_ERR;
    fh_tx_clear_undo(self);
    self->hasTxSnapshot = 0;
    self->currentTxId = 0;
    fh_tx_delete_wal(self);
    return GMB_DB_OK;
}

#ifndef _WIN32
static int fh_fcntl_lock(IsamFileHandle* self, short type, int64_t start, int64_t len, int wait)
{
    struct flock fl;
    if (!self || !self->fs) return GMB_DB_ARG;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = type;
    fl.l_whence = SEEK_SET;
    fl.l_start = (off_t)start;
    fl.l_len = (off_t)len;
    if (fcntl(fileno(self->fs), wait ? F_SETLKW : F_SETLK, &fl) != 0) return GMB_DB_ERR;
    return GMB_DB_OK;
}
#endif

int IsamFileHandle_TryLockRecord(IsamFileHandle* self, int64_t offset)
{
#ifdef _WIN32
    (void)self;
    (void)offset;
    return GMB_DB_OK;
#else
    int64_t rec = self && self->Header.RecordSize > 0 ? self->Header.RecordSize : 1;
    if (!self || offset < 0) return GMB_DB_ARG;
    return fh_fcntl_lock(self, F_WRLCK, offset, rec, 0);
#endif
}

void IsamFileHandle_UnlockRecord(IsamFileHandle* self, int64_t offset)
{
#ifdef _WIN32
    (void)self;
    (void)offset;
#else
    int64_t rec = self && self->Header.RecordSize > 0 ? self->Header.RecordSize : 1;
    if (!self || offset < 0) return;
    (void)fh_fcntl_lock(self, F_UNLCK, offset, rec, 0);
#endif
}

int IsamFileHandle_LockFile(IsamFileHandle* self)
{
#ifdef _WIN32
    (void)self;
    return GMB_DB_OK;
#else
    if (!self) return GMB_DB_ARG;
    return fh_fcntl_lock(self, F_WRLCK, 0, 0, 1);
#endif
}

int IsamFileHandle_TryLockFile(IsamFileHandle* self)
{
#ifdef _WIN32
    (void)self;
    return GMB_DB_OK;
#else
    if (!self) return GMB_DB_ARG;
    return fh_fcntl_lock(self, F_WRLCK, 0, 0, 0);
#endif
}

void IsamFileHandle_UnlockFile(IsamFileHandle* self)
{
#ifdef _WIN32
    (void)self;
#else
    if (!self) return;
    (void)fh_fcntl_lock(self, F_UNLCK, 0, 0, 0);
#endif
}
