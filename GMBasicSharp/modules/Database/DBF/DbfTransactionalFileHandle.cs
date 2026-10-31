// FILE: Basic.DBF.DbfTransactionalFileHandle.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Buffers;
using System.Collections.Concurrent;
using Basic.DBF;
using Basic.Databases;

namespace Basic.DBF;

/// <summary>
/// Definisce lo stato di una transazione attiva all'interno del Transaction Manager.
/// </summary>
internal enum TransactionStatus
{
    /// <summary>
    /// La transazione è attualmente in corso.
    /// </summary>
    Active,

    /// <summary>
    /// La transazione è stata completata con successo.
    /// </summary>
    Committed,

    /// <summary>
    /// La transazione è stata annullata.
    /// </summary>
    RolledBack
}

/// <summary>
/// Gestore centralizzato per lo stato di tutte le transazioni operanti su un file DBF.
/// Assegna ID univoci e tiene traccia delle transazioni attive, commesse e annullate,
/// fornendo la logica di visibilità per l'implementazione MVCC.
/// </summary>
internal class TransactionManager
{
    private long _nextTransactionId = 1;
    private readonly ConcurrentDictionary<long, TransactionStatus> _transactionStates = new();

    /// <summary>
    /// Avvia una nuova transazione, assegnandole un ID univoco e registrandola come attiva.
    /// </summary>
    /// <returns>L'ID univoco della nuova transazione.</returns>
    public long BeginTransaction()
    {
        var txId = Interlocked.Increment(ref _nextTransactionId);
        _transactionStates[txId] = TransactionStatus.Active;
        return txId;
    }

    /// <summary>
    /// Marca una transazione come commessa.
    /// </summary>
    /// <param name="txId">L'ID della transazione da commettere.</param>
    public void CommitTransaction(long txId) => _transactionStates[txId] = TransactionStatus.Committed;

    /// <summary>
    /// Marca una transazione come annullata.
    /// </summary>
    /// <param name="txId">L'ID della transazione da annullare.</param>
    public void RollbackTransaction(long txId) => _transactionStates[txId] = TransactionStatus.RolledBack;

    /// <summary>
    /// Determina se la versione di un record, creata da una certa transazione,
    /// è visibile alla transazione corrente, secondo le regole di "Read Committed".
    /// </summary>
    /// <param name="recordTransactionId">L'ID della transazione che ha scritto la versione del record.</param>
    /// <param name="currentTransactionId">L'ID della transazione che sta tentando di leggere il record.</param>
    /// <returns>True se la versione del record è visibile, altrimenti False.</returns>
    public bool IsVersionVisible(long recordTransactionId, long currentTransactionId)
    {
        if (recordTransactionId == 0 || recordTransactionId == currentTransactionId)
        {
            return true;
        }

        if (_transactionStates.TryGetValue(recordTransactionId, out var status))
        {
            return status == TransactionStatus.Committed;
        }

        return true;
    }
}

/// <summary>
/// Wrapper transazionale per un file DBF. Aggiunge le capacità di journaling (WAL),
/// gestione delle transazioni e locking lettura/scrittura a un DbfFile a basso livello,
/// garantendo atomicità, durabilità e isolamento.
/// </summary>
internal class DbfTransactionalFileHandle : IDisposable
{
    private DbfFile _dbfFile;
    private readonly WriteAheadLog _wal;
    private readonly string _filePath;
    private readonly bool _exclusive;
    private readonly LockingStrategy _lockingStrategy;

    /// <summary>
    /// Il gestore di lock lettura/scrittura (shared/exclusive) per questo file,
    /// la cui strategia (intra-processo o multi-processo) è determinata alla creazione dell'handle.
    /// </summary>
    public ILockManager RecordLockManager { get; }

    private readonly TransactionManager _transactionManager = new();
    private readonly ConcurrentDictionary<int, long> _threadTransactionMap = new();
    private int _bulkLoadDepth;
    private int _groupCommitDepth;
    private int _fileLockDepth;
    private bool _directWalDirty;

    /// <summary>
    /// True durante <see cref="BeginBulkLoadSession"/> — flush WAL/disco differito.
    /// </summary>
    public bool IsBulkLoadActive => _bulkLoadDepth > 0;

    /// <summary>
    /// ponytail: intra-process auto-commit salta WAL — cross-process usa DirectWrite batch.
    /// </summary>
    private bool UseTransactionalWal() => IsExplicitTransactionActive();

    private bool UseDirectWalAutoCommit() =>
        !IsExplicitTransactionActive() && RecordLockManager is CrossProcessLockManager;

    private bool UseWalForDurability() =>
        UseTransactionalWal() || UseDirectWalAutoCommit();

    private bool ShouldFsyncNow() =>
        DurabilityMode == DbfDurabilityMode.Full && !IsBulkLoadActive && _groupCommitDepth == 0;

    /// <summary>
    /// ponytail: differisce fsync come bulk-load ma senza alterare il path WAL transazionale.
    /// Chiamare <see cref="EndGroupCommitSession"/> per un fsync unico in Full mode.
    /// </summary>
    public void BeginGroupCommitSession() => _groupCommitDepth++;

    /// <summary>
    /// Termina sessione group-commit e sincronizza su disco se Full mode.
    /// </summary>
    public void EndGroupCommitSession()
    {
        if (_groupCommitDepth == 0)
        {
            throw new InvalidOperationException("Nessuna sessione group-commit attiva.");
        }

        _groupCommitDepth--;
        if (_groupCommitDepth == 0)
        {
            FinalizeAutoCommit();
        }
    }

    /// <summary>
    /// True durante group-commit — fsync differito fino a <see cref="EndGroupCommitSession"/>.
    /// </summary>
    public bool IsGroupCommitActive => _groupCommitDepth > 0;

    /// <summary>
    /// Controlla fsync per auto-commit e commit espliciti.
    /// </summary>
    public DbfDurabilityMode DurabilityMode { get; set; } = DbfDurabilityMode.Full;

    private bool IsExplicitTransactionActive() =>
        _threadTransactionMap.ContainsKey(Thread.CurrentThread.ManagedThreadId);

    /// <summary>
    /// Avvia sessione bulk-load: nessun flush fino a <see cref="EndBulkLoadSession"/>.
    /// </summary>
    public void BeginBulkLoadSession() => _bulkLoadDepth++;

    /// <summary>
    /// Termina sessione bulk-load e sincronizza WAL + file DBF.
    /// </summary>
    public void EndBulkLoadSession()
    {
        if (_bulkLoadDepth == 0)
        {
            throw new InvalidOperationException("Nessuna sessione bulk-load attiva.");
        }

        _bulkLoadDepth--;
        if (_bulkLoadDepth > 0)
        {
            return;
        }

        _dbfFile.FlushPendingHeader();
        if (UseWalForDurability())
        {
            if (UseDirectWalAutoCommit())
            {
                SyncDirectWalBeforeMainFile();
            }
            else
            {
                _wal.Flush();
            }
        }

        _dbfFile.FlushStream(ShouldFsyncNow(), flushHeaderFirst: false);
    }

    private void SyncDirectWalBeforeMainFile()
    {
        if (!_directWalDirty || !UseDirectWalAutoCommit())
        {
            return;
        }

        _wal.Flush();
        _directWalDirty = false;
    }

    private void MarkDirectWalDirty() => _directWalDirty = true;

    private void FinalizeDirectWalAfterSync()
    {
        if (!UseDirectWalAutoCommit())
        {
            return;
        }

        _wal.LogDirectCheckpoint();
        _wal.Delete(skipFlush: true);
        _directWalDirty = false;
    }

    /// <summary>
    /// Scrive header pendente ed esegue fsync su DBF (e checkpoint WAL se attivo).
    /// </summary>
    public void FlushDurability()
    {
        _dbfFile.FlushPendingHeader();
        if (UseDirectWalAutoCommit())
        {
            SyncDirectWalBeforeMainFile();
            _dbfFile.FlushStream(true, flushHeaderFirst: false);
            FinalizeDirectWalAfterSync();
            return;
        }

        if (UseTransactionalWal())
        {
            _wal.Flush();
        }

        _dbfFile.FlushStream(true, flushHeaderFirst: false);
        if (UseTransactionalWal())
        {
            _wal.Delete(skipFlush: true);
        }
    }

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="DbfTransactionalFileHandle"/>.
    /// Esegue il recovery da un eventuale WAL residuo, poi apre il file DBF e i meccanismi di log e lock.
    /// </summary>
    /// <param name="filePath">Il percorso del file .dbf da gestire.</param>
    /// <param name="exclusive">True per aprire il file in modalità esclusiva (usato dal `FileStream`, non per la strategia di lock).</param>
    /// <param name="lockingStrategy">La strategia di locking da utilizzare (Intra-Processo o Cross-Process).</param>
    public DbfTransactionalFileHandle(string filePath, bool exclusive, LockingStrategy lockingStrategy)
    {
        WriteAheadLog.Recover(filePath);
        _filePath = filePath;
        _exclusive = exclusive;
        _lockingStrategy = lockingStrategy;
        _dbfFile = new DbfFile(filePath, exclusive, lockingStrategy);
        _wal = new WriteAheadLog(filePath);
        
        // Fabbrica di Lock: istanzia il gestore corretto in base alla strategia scelta.
        // DeadlockMonitor (wait-for graph) è condiviso tra IntraProcess e CrossProcess nello stesso processo OS.
        // ponytail: deadlock cross-process tra processi OS distinti resta timeout-only (nessun grafo distribuito).
        RecordLockManager = lockingStrategy switch
        {
            LockingStrategy.IntraProcess => new IntraProcessLockManager(),
            LockingStrategy.CrossProcess => new CrossProcessLockManager(filePath),
            _ => throw new ArgumentOutOfRangeException(nameof(lockingStrategy), "Strategia di locking non supportata.")
        };
    }

    /// <summary>
    /// Restituisce l'istanza di <see cref="DbfFile"/> a basso livello associata a questo handle,
    /// che fornisce l'accesso fisico diretto al file.
    /// </summary>
    /// <returns>L'istanza di <see cref="DbfFile"/>.</returns>
    public DbfFile GetDbfFile() => _dbfFile;

    /// <summary>Dopo un PACK/ZAP che ha sostituito il file su disco (File.Replace), lo stream
    /// aperto dell'engine punta ancora all'inode VECCHIO (rimosso dal replace): ogni lettura
    /// successiva (header/record) restituirebbe i dati pre-compattazione. Riapre il DbfFile
    /// sullo STESSO path con le stesse impostazioni di apertura, mantenendo WAL e lock manager
    /// (nessuna nuova sessione di recovery).</summary>
    public void ReopenAfterFileReplace()
    {
        var old = _dbfFile;
        _dbfFile = new DbfFile(_filePath, _exclusive, _lockingStrategy);
        old.Dispose();
    }

    /// <summary>
    /// Recupera l'ID della transazione attualmente associata al thread chiamante.
    /// </summary>
    /// <returns>L'ID della transazione attiva, o 0 se non c'è nessuna transazione attiva per questo thread.</returns>
    public long GetCurrentTransactionId()
    {
        _threadTransactionMap.TryGetValue(Thread.CurrentThread.ManagedThreadId, out var txId);
        return txId;
    }

    /// <summary>
    /// Determina se la versione di un record è visibile alla transazione corrente,
    /// delegando la logica al gestore di transazioni.
    /// </summary>
    /// <param name="recordTransactionId">L'ID della transazione che ha scritto la versione del record.</param>
    /// <param name="currentTransactionId">L'ID della transazione che sta tentando di leggere il record.</param>
    /// <returns>True se la versione del record è visibile.</returns>
    public bool IsVersionVisible(long recordTransactionId, long currentTransactionId)
    {
        return _transactionManager.IsVersionVisible(recordTransactionId, currentTransactionId);
    }

    /// <summary>
    /// Acquisisce un lock di scrittura (esclusivo) e bloccante sull'intero file.
    /// ponytail: reentrante — Pack() chiama Reindex() sullo stesso thread.
    /// </summary>
    public void LockFile()
    {
        if (_fileLockDepth > 0)
        {
            _fileLockDepth++;
            return;
        }

        var lockKey = LockManagerBase.GetRecordLockKey(-1);
        RecordLockManager.AcquireWriteLock(lockKey, -1, "file DBF");
        _fileLockDepth = 1;
        if (RecordLockManager is CrossProcessLockManager)
        {
            _dbfFile.ReloadHeaderFromDisk();
        }
    }

    /// <summary>
    /// Rilascia un lock di scrittura (esclusivo) precedentemente acquisito sul file.
    /// </summary>
    public void UnlockFile()
    {
        if (_fileLockDepth == 0)
        {
            return;
        }

        _fileLockDepth--;
        if (_fileLockDepth > 0)
        {
            return;
        }

        var lockKey = LockManagerBase.GetRecordLockKey(-1);
        RecordLockManager.ReleaseWriteLock(lockKey);
    }

    /// <summary>
    /// Avvia una transazione fisica, registrandola nel WAL e nel <see cref="TransactionManager"/>.
    /// Questo metodo viene invocato dall'engine all'inizio di una transazione di primo livello.
    /// </summary>
    /// <returns>L'ID univoco della transazione appena creata.</returns>
    public long BeginTransaction()
    {
        var txId = _transactionManager.BeginTransaction();
        _threadTransactionMap[Thread.CurrentThread.ManagedThreadId] = txId;
        _wal.LogBeginTransaction();
        return txId;
    }

    /// <summary>
    /// Finalizza una transazione fisica, registrando il marcatore di commit nel WAL e aggiornando
    /// lo stato della transazione nel <see cref="TransactionManager"/>.
    /// </summary>
    public void CommitTransaction()
    {
        if (_threadTransactionMap.TryRemove(Thread.CurrentThread.ManagedThreadId, out var txId))
        {
            _transactionManager.CommitTransaction(txId);
        }

        _wal.LogCommitTransaction();
    }

    /// <summary>
    /// Annulla una transazione fisica. L'operazione aggiorna lo stato nel <see cref="TransactionManager"/>
    /// ma non scrive nulla nel WAL. Il recovery ignorerà le operazioni della transazione non commessa,
    /// ottenendo di fatto un rollback.
    /// </summary>
    public void RollbackTransaction()
    {
        if (_threadTransactionMap.TryRemove(Thread.CurrentThread.ManagedThreadId, out var txId))
        {
            _transactionManager.RollbackTransaction(txId);
        }
    }

    /// <summary>
    /// Esegue il commit finale su disco, forzando la scrittura del WAL e cancellando il file di log.
    /// Questo metodo viene invocato dall'engine solo al termine della transazione più esterna
    /// per garantire la durabilità.
    /// </summary>
    public void FinalizeCommit()
    {
        _dbfFile.FlushPendingHeader();
        if (UseDirectWalAutoCommit())
        {
            SyncDirectWalBeforeMainFile();
            _dbfFile.FlushStream(true, flushHeaderFirst: false);
            FinalizeDirectWalAfterSync();
            return;
        }

        if (UseTransactionalWal())
        {
            _wal.Flush();
        }

        _dbfFile.FlushStream(true, flushHeaderFirst: false);
        if (UseTransactionalWal())
        {
            _wal.Delete(skipFlush: true);
        }
    }

    /// <summary>
    /// Durabilità auto-commit: fsync DBF (+ checkpoint WAL cross-process se Full).
    /// </summary>
    public void FinalizeAutoCommit()
    {
        _dbfFile.FlushPendingHeader();
        if (UseDirectWalAutoCommit())
        {
            if (ShouldFsyncNow())
            {
                SyncDirectWalBeforeMainFile();
                _dbfFile.FlushStream(true, flushHeaderFirst: false);
                FinalizeDirectWalAfterSync();
            }
            else
            {
                _dbfFile.FlushStream(false, flushHeaderFirst: false);
            }

            return;
        }

        if (UseTransactionalWal())
        {
            _wal.LogCommitTransaction();
            _dbfFile.FlushStream(true, flushHeaderFirst: false);
            _wal.Delete(skipFlush: true);
        }
        else
        {
            _dbfFile.FlushStream(ShouldFsyncNow(), flushHeaderFirst: false);
        }
    }

    /// <summary>
    /// Scrive i dati grezzi di un record nel file, registrando l'operazione nel WAL per garantire l'atomicità.
    /// </summary>
    /// <param name="recNo">Il numero del record (1-based) da sovrascrivere.</param>
    /// <param name="rawData">I dati grezzi del record (escluso il flag di cancellazione).</param>
    public void WriteRawRecord(int recNo, ReadOnlySpan<byte> rawData)
    {
        var offset = _dbfFile.Header.HeaderLength + ((long)(recNo - 1) * _dbfFile.Header.RecordLength);
        if (UseTransactionalWal())
        {
            _wal.LogWriteBytes(offset + 1, rawData.ToArray());
        }
        else if (UseDirectWalAutoCommit())
        {
            _wal.LogDirectWrite(offset + 1, rawData.ToArray());
            MarkDirectWalDirty();
        }

        _dbfFile.WriteRawRecord(recNo, rawData);
    }

    /// <summary>
    /// Aggiunge <paramref name="count"/> record consecutivi con un solo aggiornamento header (path caldo batch).
    /// </summary>
    /// <returns>Il numero del primo record inserito (1-based).</returns>
    public int AppendRawRecordsBatch(ReadOnlySpan<byte> rawPayload, int recordPayloadLength, int count)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(count);
        if (rawPayload.Length < (long)recordPayloadLength * count)
        {
            throw new ArgumentException("Payload batch troppo corto.", nameof(rawPayload));
        }

        var currentHeader = _dbfFile.Header;
        var recordLength = currentHeader.RecordLength;
        var firstOffset = currentHeader.HeaderLength +
                          (long)currentHeader.NumberOfRecords * recordLength;

        var futureHeader = currentHeader;
        futureHeader.NumberOfRecords += (uint)count;
        futureHeader.Year = (byte)(DateTime.Now.Year % 100);
        futureHeader.Month = (byte)DateTime.Now.Month;
        futureHeader.Day = (byte)DateTime.Now.Day;

        if (UseTransactionalWal())
        {
            _wal.LogUpdateHeader(DbfHeader.ToBytes(futureHeader));

            var fullRecordBuffer = ArrayPool<byte>.Shared.Rent(recordLength);
            try
            {
                for (var i = 0; i < count; i++)
                {
                    var recordOffset = firstOffset + (long)i * recordLength;
                    fullRecordBuffer.AsSpan(0, recordLength).Clear();
                    fullRecordBuffer[0] = (byte)' ';
                    rawPayload.Slice(i * recordPayloadLength, recordPayloadLength)
                        .CopyTo(fullRecordBuffer.AsSpan(1));
                    _wal.LogWriteRecord(recordOffset, fullRecordBuffer.AsSpan(0, recordLength));
                }
            }
            finally
            {
                ArrayPool<byte>.Shared.Return(fullRecordBuffer);
            }
        }
        else if (UseDirectWalAutoCommit())
        {
            _wal.LogDirectWrite(0, DbfHeader.ToBytes(futureHeader));

            var fullRecordBuffer = ArrayPool<byte>.Shared.Rent(recordLength);
            try
            {
                for (var i = 0; i < count; i++)
                {
                    var recordOffset = firstOffset + (long)i * recordLength;
                    fullRecordBuffer.AsSpan(0, recordLength).Clear();
                    fullRecordBuffer[0] = (byte)' ';
                    rawPayload.Slice(i * recordPayloadLength, recordPayloadLength)
                        .CopyTo(fullRecordBuffer.AsSpan(1));
                    _wal.LogDirectWrite(recordOffset, fullRecordBuffer.AsSpan(0, recordLength).ToArray());
                }

                MarkDirectWalDirty();
            }
            finally
            {
                ArrayPool<byte>.Shared.Return(fullRecordBuffer);
            }
        }

        return _dbfFile.AppendRawRecordsBatch(rawPayload, recordPayloadLength, count);
    }

    /// <summary>
    /// Aggiunge un nuovo record in modo atomico, registrando nel WAL sia la scrittura dei dati
    /// che l'aggiornamento dell'header del file.
    /// </summary>
    /// <param name="rawData">I dati grezzi del record da aggiungere (escluso il flag di cancellazione).</param>
    /// <returns>Il numero del nuovo record aggiunto.</returns>
    public int AppendRawRecord(ReadOnlySpan<byte> rawData)
    {
        var currentHeader = _dbfFile.Header;
        var newRecordOffset = currentHeader.HeaderLength +
                              ((long)currentHeader.NumberOfRecords * currentHeader.RecordLength);

        var futureHeader = currentHeader;
        futureHeader.NumberOfRecords++;
        futureHeader.Year = (byte)(DateTime.Now.Year % 100);
        futureHeader.Month = (byte)DateTime.Now.Month;
        futureHeader.Day = (byte)DateTime.Now.Day;

        if (UseTransactionalWal())
        {
            _wal.LogUpdateHeader(DbfHeader.ToBytes(futureHeader));

            var recordLength = currentHeader.RecordLength;
            var fullRecordBuffer = ArrayPool<byte>.Shared.Rent(recordLength);
            try
            {
                fullRecordBuffer.AsSpan(0, recordLength).Clear();
                fullRecordBuffer[0] = (byte)' ';
                rawData.CopyTo(fullRecordBuffer.AsSpan(1));
                _wal.LogWriteRecord(newRecordOffset, fullRecordBuffer.AsSpan(0, recordLength));
            }
            finally
            {
                ArrayPool<byte>.Shared.Return(fullRecordBuffer);
            }
        }
        else if (UseDirectWalAutoCommit())
        {
            var recordLength = currentHeader.RecordLength;
            var fullRecordBuffer = ArrayPool<byte>.Shared.Rent(recordLength);
            try
            {
                fullRecordBuffer.AsSpan(0, recordLength).Clear();
                fullRecordBuffer[0] = (byte)' ';
                rawData.CopyTo(fullRecordBuffer.AsSpan(1));
                _wal.LogDirectWrite(0, DbfHeader.ToBytes(futureHeader));
                _wal.LogDirectWrite(newRecordOffset, fullRecordBuffer.AsSpan(0, recordLength).ToArray());
                MarkDirectWalDirty();
            }
            finally
            {
                ArrayPool<byte>.Shared.Return(fullRecordBuffer);
            }
        }

        return _dbfFile.AppendRawRecord(rawData);
    }

    /// <summary>
    /// Scrive il flag di cancellazione per un record specifico in modo transazionale,
    /// registrando la modifica di un singolo byte nel WAL.
    /// </summary>
    /// <param name="recNo">Il numero del record (1-based) da modificare.</param>
    /// <param name="isDeleted">True per marcare come cancellato ('*'), False per marcare come attivo (' ').</param>
    public void SetDeletedFlag(int recNo, bool isDeleted)
    {
        var offset = _dbfFile.Header.HeaderLength + ((long)(recNo - 1) * _dbfFile.Header.RecordLength);
        var flag = new[] { isDeleted ? (byte)'*' : (byte)' ' };
        if (UseTransactionalWal())
        {
            _wal.LogWriteBytes(offset, flag);
        }
        else if (UseDirectWalAutoCommit())
        {
            _wal.LogDirectWrite(offset, flag);
            MarkDirectWalDirty();
        }

        _dbfFile.SetDeletedFlag(recNo, isDeleted);
    }

    /// <summary>
    /// Rimuove tutti i record dal file in modo transazionale, aggiornando l'header del file
    /// e registrando la modifica nel WAL.
    /// </summary>
    public void Zap()
    {
        var futureHeader = _dbfFile.Header;
        futureHeader.NumberOfRecords = 0;
        futureHeader.Year = (byte)(DateTime.Now.Year % 100);
        futureHeader.Month = (byte)(DateTime.Now.Month);
        futureHeader.Day = (byte)DateTime.Now.Day;
        _wal.LogUpdateHeader(DbfHeader.ToBytes(futureHeader));
        _dbfFile.Zap();
    }

    /// <summary>
    /// Legge i dati grezzi di un record esistente e li appende alla fine del file,
    /// creando una copia di "undo". Se in una transazione, registra il numero del
    /// nuovo record per un eventuale recupero di spazio in caso di rollback.
    /// </summary>
    /// <param name="recNo">Il numero del record (1-based) da cui creare la copia.</param>
    /// <param name="context">Il contesto della transazione corrente. Se fornito, il nuovo
    /// numero di record viene registrato al suo interno.</param>
    /// <returns>Il numero del nuovo record di undo appena creato.</returns>
    public int AppendUndoVersion(int recNo, TransactionContext? context)
    {
        var (_, rawData) = _dbfFile.ReadRawRecord(recNo);
        var newRecNo = AppendRawRecord(rawData);
        // Aggiornato per usare la proprietà unificata 'UndoLogIdentifiers'.
        context?.UndoLogIdentifiers.Add(newRecNo);
        return newRecNo;
    }

    /// <summary>
    /// Rilascia tutte le risorse gestite, inclusi gli handle di file per DBF e WAL,
    /// e il gestore di lock multi-processo.
    /// </summary>
    public void Dispose()
    {
        FlushDurability();
        _wal.Dispose();
        _dbfFile.Dispose();
        /* Rilascia i lock ANCORA TENUTI prima di smaltire il lock manager: un ciclo
         * DBF.RLOCK()/FLOCK() seguito da DBF CLOSE senza UNLOCK lasciava un
         * ReaderWriterLockSlim tenuto e Dispose lanciava SynchronizationLockException
         * ("The lock is being disposed while still being used") — trovato da
         * MiniTests/dbf/dbf_lock_filter.bas. Stessa rete di sicurezza già presente nel
         * percorso ISAM (modules/Database/ISAM/FileHandle.cs). */
        if (RecordLockManager is Basic.Databases.IntraProcessLockManager ipmClose) ipmClose.ReleaseAllHeld();
        RecordLockManager.Dispose();
    }
}