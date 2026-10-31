/* FILE: native/gmb_db_c/src/DBF/DbfTransactionalFileHandle.c */
/* Port 1:1 di modules/Database/DBF/DbfTransactionalFileHandle.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfTransactionalFileHandle.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Buffers;
//using System.Collections.Concurrent;
//using Basic.DBF;
//using Basic.Databases;
//
//namespace Basic.DBF;
//
///// <summary>
///// Definisce lo stato di una transazione attiva all'interno del Transaction Manager.
///// </summary>
//internal enum TransactionStatus
//{
//    /// <summary>
//    /// La transazione è attualmente in corso.
//    /// </summary>
//    Active,
//
//    /// <summary>
//    /// La transazione è stata completata con successo.
//    /// </summary>
//    Committed,
//
//    /// <summary>
//    /// La transazione è stata annullata.
//    /// </summary>
//    RolledBack
//}
//
///// <summary>
///// Gestore centralizzato per lo stato di tutte le transazioni operanti su un file DBF.
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
//    /// <returns>L'ID univoco della nuova transazione.</returns>
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
//    /// <param name="txId">L'ID della transazione da commettere.</param>
//    public void CommitTransaction(long txId) => _transactionStates[txId] = TransactionStatus.Committed;
//
//    /// <summary>
//    /// Marca una transazione come annullata.
//    /// </summary>
//    /// <param name="txId">L'ID della transazione da annullare.</param>
//    public void RollbackTransaction(long txId) => _transactionStates[txId] = TransactionStatus.RolledBack;
//
//    /// <summary>
//    /// Determina se la versione di un record, creata da una certa transazione,
//    /// è visibile alla transazione corrente, secondo le regole di "Read Committed".
//    /// </summary>
//    /// <param name="recordTransactionId">L'ID della transazione che ha scritto la versione del record.</param>
//    /// <param name="currentTransactionId">L'ID della transazione che sta tentando di leggere il record.</param>
//    /// <returns>True se la versione del record è visibile, altrimenti False.</returns>
//    public bool IsVersionVisible(long recordTransactionId, long currentTransactionId)
//    {
//        if (recordTransactionId == 0 || recordTransactionId == currentTransactionId)
//        {
//            return true;
//        }
//
//        if (_transactionStates.TryGetValue(recordTransactionId, out var status))
//        {
//            return status == TransactionStatus.Committed;
//        }
//
//        return true;
//    }
//}
//
///// <summary>
///// Wrapper transazionale per un file DBF. Aggiunge le capacità di journaling (WAL),
///// gestione delle transazioni e locking lettura/scrittura a un DbfFile a basso livello,
///// garantendo atomicità, durabilità e isolamento.
///// </summary>
//internal class DbfTransactionalFileHandle : IDisposable
//{
//    private readonly DbfFile _dbfFile;
//    private readonly WriteAheadLog _wal;
//
//    /// <summary>
//    /// Il gestore di lock lettura/scrittura (shared/exclusive) per questo file,
//    /// la cui strategia (intra-processo o multi-processo) è determinata alla creazione dell'handle.
//    /// </summary>
//    public ILockManager RecordLockManager { get; }
//
//    private readonly TransactionManager _transactionManager = new();
//    private readonly ConcurrentDictionary<int, long> _threadTransactionMap = new();
//    private int _bulkLoadDepth;
//    private int _groupCommitDepth;
//    private int _fileLockDepth;
//    private bool _directWalDirty;
//
//    /// <summary>
//    /// True durante <see cref="BeginBulkLoadSession"/> — flush WAL/disco differito.
//    /// </summary>
//    public bool IsBulkLoadActive => _bulkLoadDepth > 0;
//
//    /// <summary>
//    /// ponytail: intra-process auto-commit salta WAL — cross-process usa DirectWrite batch.
//    /// </summary>
//    private bool UseTransactionalWal() => IsExplicitTransactionActive();
//
//    private bool UseDirectWalAutoCommit() =>
//        !IsExplicitTransactionActive() && RecordLockManager is CrossProcessLockManager;
//
//    private bool UseWalForDurability() =>
//        UseTransactionalWal() || UseDirectWalAutoCommit();
//
//    private bool ShouldFsyncNow() =>
//        DurabilityMode == DbfDurabilityMode.Full && !IsBulkLoadActive && _groupCommitDepth == 0;
//
//    /// <summary>
//    /// ponytail: differisce fsync come bulk-load ma senza alterare il path WAL transazionale.
//    /// Chiamare <see cref="EndGroupCommitSession"/> per un fsync unico in Full mode.
//    /// </summary>
//    public void BeginGroupCommitSession() => _groupCommitDepth++;
//
//    /// <summary>
//    /// Termina sessione group-commit e sincronizza su disco se Full mode.
//    /// </summary>
//    public void EndGroupCommitSession()
//    {
//        if (_groupCommitDepth == 0)
//        {
//            throw new InvalidOperationException("Nessuna sessione group-commit attiva.");
//        }
//
//        _groupCommitDepth--;
//        if (_groupCommitDepth == 0)
//        {
//            FinalizeAutoCommit();
//        }
//    }
//
//    /// <summary>
//    /// True durante group-commit — fsync differito fino a <see cref="EndGroupCommitSession"/>.
//    /// </summary>
//    public bool IsGroupCommitActive => _groupCommitDepth > 0;
//
//    /// <summary>
//    /// Controlla fsync per auto-commit e commit espliciti.
//    /// </summary>
//    public DbfDurabilityMode DurabilityMode { get; set; } = DbfDurabilityMode.Full;
//
//    private bool IsExplicitTransactionActive() =>
//        _threadTransactionMap.ContainsKey(Thread.CurrentThread.ManagedThreadId);
//
//    /// <summary>
//    /// Avvia sessione bulk-load: nessun flush fino a <see cref="EndBulkLoadSession"/>.
//    /// </summary>
//    public void BeginBulkLoadSession() => _bulkLoadDepth++;
//
//    /// <summary>
//    /// Termina sessione bulk-load e sincronizza WAL + file DBF.
//    /// </summary>
//    public void EndBulkLoadSession()
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
//        _dbfFile.FlushPendingHeader();
//        if (UseWalForDurability())
//        {
//            if (UseDirectWalAutoCommit())
//            {
//                SyncDirectWalBeforeMainFile();
//            }
//            else
//            {
//                _wal.Flush();
//            }
//        }
//
//        _dbfFile.FlushStream(ShouldFsyncNow(), flushHeaderFirst: false);
//    }
//
//    private void SyncDirectWalBeforeMainFile()
//    {
//        if (!_directWalDirty || !UseDirectWalAutoCommit())
//        {
//            return;
//        }
//
//        _wal.Flush();
//        _directWalDirty = false;
//    }
//
//    private void MarkDirectWalDirty() => _directWalDirty = true;
//
//    private void FinalizeDirectWalAfterSync()
//    {
//        if (!UseDirectWalAutoCommit())
//        {
//            return;
//        }
//
//        _wal.LogDirectCheckpoint();
//        _wal.Delete(skipFlush: true);
//        _directWalDirty = false;
//    }
//
//    /// <summary>
//    /// Scrive header pendente ed esegue fsync su DBF (e checkpoint WAL se attivo).
//    /// </summary>
//    public void FlushDurability()
//    {
//        _dbfFile.FlushPendingHeader();
//        if (UseDirectWalAutoCommit())
//        {
//            SyncDirectWalBeforeMainFile();
//            _dbfFile.FlushStream(true, flushHeaderFirst: false);
//            FinalizeDirectWalAfterSync();
//            return;
//        }
//
//        if (UseTransactionalWal())
//        {
//            _wal.Flush();
//        }
//
//        _dbfFile.FlushStream(true, flushHeaderFirst: false);
//        if (UseTransactionalWal())
//        {
//            _wal.Delete(skipFlush: true);
//        }
//    }
//
//    /// <summary>
//    /// Inizializza una nuova istanza della classe <see cref="DbfTransactionalFileHandle"/>.
//    /// Esegue il recovery da un eventuale WAL residuo, poi apre il file DBF e i meccanismi di log e lock.
//    /// </summary>
//    /// <param name="filePath">Il percorso del file .dbf da gestire.</param>
//    /// <param name="exclusive">True per aprire il file in modalità esclusiva (usato dal `FileStream`, non per la strategia di lock).</param>
//    /// <param name="lockingStrategy">La strategia di locking da utilizzare (Intra-Processo o Cross-Process).</param>
//    public DbfTransactionalFileHandle(string filePath, bool exclusive, LockingStrategy lockingStrategy)
//    {
//        WriteAheadLog.Recover(filePath);
//        _dbfFile = new DbfFile(filePath, exclusive, lockingStrategy);
//        _wal = new WriteAheadLog(filePath);
//        
//        // Fabbrica di Lock: istanzia il gestore corretto in base alla strategia scelta.
//        // DeadlockMonitor (wait-for graph) è condiviso tra IntraProcess e CrossProcess nello stesso processo OS.
//        // CrossProcessLockManager pubblica held/wait su file .gmb_waitgraph_* per deadlock tra processi OS (vittima = seq più alto).
//        RecordLockManager = lockingStrategy switch
//        {
//            LockingStrategy.IntraProcess => new IntraProcessLockManager(),
//            LockingStrategy.CrossProcess => new CrossProcessLockManager(filePath),
//            _ => throw new ArgumentOutOfRangeException(nameof(lockingStrategy), "Strategia di locking non supportata.")
//        };
//    }
//
//    /// <summary>
//    /// Restituisce l'istanza di <see cref="DbfFile"/> a basso livello associata a questo handle,
//    /// che fornisce l'accesso fisico diretto al file.
//    /// </summary>
//    /// <returns>L'istanza di <see cref="DbfFile"/>.</returns>
//    public DbfFile GetDbfFile() => _dbfFile;
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
//    /// Determina se la versione di un record è visibile alla transazione corrente,
//    /// delegando la logica al gestore di transazioni.
//    /// </summary>
//    /// <param name="recordTransactionId">L'ID della transazione che ha scritto la versione del record.</param>
//    /// <param name="currentTransactionId">L'ID della transazione che sta tentando di leggere il record.</param>
//    /// <returns>True se la versione del record è visibile.</returns>
//    public bool IsVersionVisible(long recordTransactionId, long currentTransactionId)
//    {
//        return _transactionManager.IsVersionVisible(recordTransactionId, currentTransactionId);
//    }
//
//    /// <summary>
//    /// Acquisisce un lock di scrittura (esclusivo) e bloccante sull'intero file.
//    /// ponytail: reentrante — Pack() chiama Reindex() sullo stesso thread.
//    /// </summary>
//    public void LockFile()
//    {
//        if (_fileLockDepth > 0)
//        {
//            _fileLockDepth++;
//            return;
//        }
//
//        var lockKey = LockManagerBase.GetRecordLockKey(-1);
//        RecordLockManager.AcquireWriteLock(lockKey, -1, "file DBF");
//        _fileLockDepth = 1;
//        if (RecordLockManager is CrossProcessLockManager)
//        {
//            _dbfFile.ReloadHeaderFromDisk();
//        }
//    }
//
//    /// <summary>
//    /// Rilascia un lock di scrittura (esclusivo) precedentemente acquisito sul file.
//    /// </summary>
//    public void UnlockFile()
//    {
//        if (_fileLockDepth == 0)
//        {
//            return;
//        }
//
//        _fileLockDepth--;
//        if (_fileLockDepth > 0)
//        {
//            return;
//        }
//
//        var lockKey = LockManagerBase.GetRecordLockKey(-1);
//        RecordLockManager.ReleaseWriteLock(lockKey);
//    }
//
//    /// <summary>
//    /// Avvia una transazione fisica, registrandola nel WAL e nel <see cref="TransactionManager"/>.
//    /// Questo metodo viene invocato dall'engine all'inizio di una transazione di primo livello.
//    /// </summary>
//    /// <returns>L'ID univoco della transazione appena creata.</returns>
//    public long BeginTransaction()
//    {
//        var txId = _transactionManager.BeginTransaction();
//        _threadTransactionMap[Thread.CurrentThread.ManagedThreadId] = txId;
//        _wal.LogBeginTransaction();
//        return txId;
//    }
//
//    /// <summary>
//    /// Finalizza una transazione fisica, registrando il marcatore di commit nel WAL e aggiornando
//    /// lo stato della transazione nel <see cref="TransactionManager"/>.
//    /// </summary>
//    public void CommitTransaction()
//    {
//        if (_threadTransactionMap.TryRemove(Thread.CurrentThread.ManagedThreadId, out var txId))
//        {
//            _transactionManager.CommitTransaction(txId);
//        }
//
//        _wal.LogCommitTransaction();
//    }
//
//    /// <summary>
//    /// Annulla una transazione fisica. L'operazione aggiorna lo stato nel <see cref="TransactionManager"/>
//    /// ma non scrive nulla nel WAL. Il recovery ignorerà le operazioni della transazione non commessa,
//    /// ottenendo di fatto un rollback.
//    /// </summary>
//    public void RollbackTransaction()
//    {
//        if (_threadTransactionMap.TryRemove(Thread.CurrentThread.ManagedThreadId, out var txId))
//        {
//            _transactionManager.RollbackTransaction(txId);
//        }
//    }
//
//    /// <summary>
//    /// Esegue il commit finale su disco, forzando la scrittura del WAL e cancellando il file di log.
//    /// Questo metodo viene invocato dall'engine solo al termine della transazione più esterna
//    /// per garantire la durabilità.
//    /// </summary>
//    public void FinalizeCommit()
//    {
//        _dbfFile.FlushPendingHeader();
//        if (UseDirectWalAutoCommit())
//        {
//            SyncDirectWalBeforeMainFile();
//            _dbfFile.FlushStream(true, flushHeaderFirst: false);
//            FinalizeDirectWalAfterSync();
//            return;
//        }
//
//        if (UseTransactionalWal())
//        {
//            _wal.Flush();
//        }
//
//        _dbfFile.FlushStream(true, flushHeaderFirst: false);
//        if (UseTransactionalWal())
//        {
//            _wal.Delete(skipFlush: true);
//        }
//    }
//
//    /// <summary>
//    /// Durabilità auto-commit: fsync DBF (+ checkpoint WAL cross-process se Full).
//    /// </summary>
//    public void FinalizeAutoCommit()
//    {
//        _dbfFile.FlushPendingHeader();
//        if (UseDirectWalAutoCommit())
//        {
//            if (ShouldFsyncNow())
//            {
//                SyncDirectWalBeforeMainFile();
//                _dbfFile.FlushStream(true, flushHeaderFirst: false);
//                FinalizeDirectWalAfterSync();
//            }
//            else
//            {
//                _dbfFile.FlushStream(false, flushHeaderFirst: false);
//            }
//
//            return;
//        }
//
//        if (UseTransactionalWal())
//        {
//            _wal.LogCommitTransaction();
//            _dbfFile.FlushStream(true, flushHeaderFirst: false);
//            _wal.Delete(skipFlush: true);
//        }
//        else
//        {
//            _dbfFile.FlushStream(ShouldFsyncNow(), flushHeaderFirst: false);
//        }
//    }
//
//    /// <summary>
//    /// Scrive i dati grezzi di un record nel file, registrando l'operazione nel WAL per garantire l'atomicità.
//    /// </summary>
//    /// <param name="recNo">Il numero del record (1-based) da sovrascrivere.</param>
//    /// <param name="rawData">I dati grezzi del record (escluso il flag di cancellazione).</param>
//    public void WriteRawRecord(int recNo, ReadOnlySpan<byte> rawData)
//    {
//        var offset = _dbfFile.Header.HeaderLength + ((long)(recNo - 1) * _dbfFile.Header.RecordLength);
//        if (UseTransactionalWal())
//        {
//            _wal.LogWriteBytes(offset + 1, rawData.ToArray());
//        }
//        else if (UseDirectWalAutoCommit())
//        {
//            _wal.LogDirectWrite(offset + 1, rawData.ToArray());
//            MarkDirectWalDirty();
//        }
//
//        _dbfFile.WriteRawRecord(recNo, rawData);
//    }
//
//    /// <summary>
//    /// Aggiunge <paramref name="count"/> record consecutivi con un solo aggiornamento header (path caldo batch).
//    /// </summary>
//    /// <returns>Il numero del primo record inserito (1-based).</returns>
//    public int AppendRawRecordsBatch(ReadOnlySpan<byte> rawPayload, int recordPayloadLength, int count)
//    {
//        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(count);
//        if (rawPayload.Length < (long)recordPayloadLength * count)
//        {
//            throw new ArgumentException("Payload batch troppo corto.", nameof(rawPayload));
//        }
//
//        var currentHeader = _dbfFile.Header;
//        var recordLength = currentHeader.RecordLength;
//        var firstOffset = currentHeader.HeaderLength +
//                          (long)currentHeader.NumberOfRecords * recordLength;
//
//        var futureHeader = currentHeader;
//        futureHeader.NumberOfRecords += (uint)count;
//        futureHeader.Year = (byte)(DateTime.Now.Year % 100);
//        futureHeader.Month = (byte)DateTime.Now.Month;
//        futureHeader.Day = (byte)DateTime.Now.Day;
//
//        if (UseTransactionalWal())
//        {
//            _wal.LogUpdateHeader(DbfHeader.ToBytes(futureHeader));
//
//            var fullRecordBuffer = ArrayPool<byte>.Shared.Rent(recordLength);
//            try
//            {
//                for (var i = 0; i < count; i++)
//                {
//                    var recordOffset = firstOffset + (long)i * recordLength;
//                    fullRecordBuffer.AsSpan(0, recordLength).Clear();
//                    fullRecordBuffer[0] = (byte)' ';
//                    rawPayload.Slice(i * recordPayloadLength, recordPayloadLength)
//                        .CopyTo(fullRecordBuffer.AsSpan(1));
//                    _wal.LogWriteRecord(recordOffset, fullRecordBuffer.AsSpan(0, recordLength));
//                }
//            }
//            finally
//            {
//                ArrayPool<byte>.Shared.Return(fullRecordBuffer);
//            }
//        }
//        else if (UseDirectWalAutoCommit())
//        {
//            _wal.LogDirectWrite(0, DbfHeader.ToBytes(futureHeader));
//
//            var fullRecordBuffer = ArrayPool<byte>.Shared.Rent(recordLength);
//            try
//            {
//                for (var i = 0; i < count; i++)
//                {
//                    var recordOffset = firstOffset + (long)i * recordLength;
//                    fullRecordBuffer.AsSpan(0, recordLength).Clear();
//                    fullRecordBuffer[0] = (byte)' ';
//                    rawPayload.Slice(i * recordPayloadLength, recordPayloadLength)
//                        .CopyTo(fullRecordBuffer.AsSpan(1));
//                    _wal.LogDirectWrite(recordOffset, fullRecordBuffer.AsSpan(0, recordLength).ToArray());
//                }
//
//                MarkDirectWalDirty();
//            }
//            finally
//            {
//                ArrayPool<byte>.Shared.Return(fullRecordBuffer);
//            }
//        }
//
//        return _dbfFile.AppendRawRecordsBatch(rawPayload, recordPayloadLength, count);
//    }
//
//    /// <summary>
//    /// Aggiunge un nuovo record in modo atomico, registrando nel WAL sia la scrittura dei dati
//    /// che l'aggiornamento dell'header del file.
//    /// </summary>
//    /// <param name="rawData">I dati grezzi del record da aggiungere (escluso il flag di cancellazione).</param>
//    /// <returns>Il numero del nuovo record aggiunto.</returns>
//    public int AppendRawRecord(ReadOnlySpan<byte> rawData)
//    {
//        var currentHeader = _dbfFile.Header;
//        var newRecordOffset = currentHeader.HeaderLength +
//                              ((long)currentHeader.NumberOfRecords * currentHeader.RecordLength);
//
//        var futureHeader = currentHeader;
//        futureHeader.NumberOfRecords++;
//        futureHeader.Year = (byte)(DateTime.Now.Year % 100);
//        futureHeader.Month = (byte)DateTime.Now.Month;
//        futureHeader.Day = (byte)DateTime.Now.Day;
//
//        if (UseTransactionalWal())
//        {
//            _wal.LogUpdateHeader(DbfHeader.ToBytes(futureHeader));
//
//            var recordLength = currentHeader.RecordLength;
//            var fullRecordBuffer = ArrayPool<byte>.Shared.Rent(recordLength);
//            try
//            {
//                fullRecordBuffer.AsSpan(0, recordLength).Clear();
//                fullRecordBuffer[0] = (byte)' ';
//                rawData.CopyTo(fullRecordBuffer.AsSpan(1));
//                _wal.LogWriteRecord(newRecordOffset, fullRecordBuffer.AsSpan(0, recordLength));
//            }
//            finally
//            {
//                ArrayPool<byte>.Shared.Return(fullRecordBuffer);
//            }
//        }
//        else if (UseDirectWalAutoCommit())
//        {
//            var recordLength = currentHeader.RecordLength;
//            var fullRecordBuffer = ArrayPool<byte>.Shared.Rent(recordLength);
//            try
//            {
//                fullRecordBuffer.AsSpan(0, recordLength).Clear();
//                fullRecordBuffer[0] = (byte)' ';
//                rawData.CopyTo(fullRecordBuffer.AsSpan(1));
//                _wal.LogDirectWrite(0, DbfHeader.ToBytes(futureHeader));
//                _wal.LogDirectWrite(newRecordOffset, fullRecordBuffer.AsSpan(0, recordLength).ToArray());
//                MarkDirectWalDirty();
//            }
//            finally
//            {
//                ArrayPool<byte>.Shared.Return(fullRecordBuffer);
//            }
//        }
//
//        return _dbfFile.AppendRawRecord(rawData);
//    }
//
//    /// <summary>
//    /// Scrive il flag di cancellazione per un record specifico in modo transazionale,
//    /// registrando la modifica di un singolo byte nel WAL.
//    /// </summary>
//    /// <param name="recNo">Il numero del record (1-based) da modificare.</param>
//    /// <param name="isDeleted">True per marcare come cancellato ('*'), False per marcare come attivo (' ').</param>
//    public void SetDeletedFlag(int recNo, bool isDeleted)
//    {
//        var offset = _dbfFile.Header.HeaderLength + ((long)(recNo - 1) * _dbfFile.Header.RecordLength);
//        var flag = new[] { isDeleted ? (byte)'*' : (byte)' ' };
//        if (UseTransactionalWal())
//        {
//            _wal.LogWriteBytes(offset, flag);
//        }
//        else if (UseDirectWalAutoCommit())
//        {
//            _wal.LogDirectWrite(offset, flag);
//            MarkDirectWalDirty();
//        }
//
//        _dbfFile.SetDeletedFlag(recNo, isDeleted);
//    }
//
//    /// <summary>
//    /// Rimuove tutti i record dal file in modo transazionale, aggiornando l'header del file
//    /// e registrando la modifica nel WAL.
//    /// </summary>
//    public void Zap()
//    {
//        var futureHeader = _dbfFile.Header;
//        futureHeader.NumberOfRecords = 0;
//        futureHeader.Year = (byte)(DateTime.Now.Year % 100);
//        futureHeader.Month = (byte)(DateTime.Now.Month);
//        futureHeader.Day = (byte)DateTime.Now.Day;
//        _wal.LogUpdateHeader(DbfHeader.ToBytes(futureHeader));
//        _dbfFile.Zap();
//    }
//
//    /// <summary>
//    /// Legge i dati grezzi di un record esistente e li appende alla fine del file,
//    /// creando una copia di "undo". Se in una transazione, registra il numero del
//    /// nuovo record per un eventuale recupero di spazio in caso di rollback.
//    /// </summary>
//    /// <param name="recNo">Il numero del record (1-based) da cui creare la copia.</param>
//    /// <param name="context">Il contesto della transazione corrente. Se fornito, il nuovo
//    /// numero di record viene registrato al suo interno.</param>
//    /// <returns>Il numero del nuovo record di undo appena creato.</returns>
//    public int AppendUndoVersion(int recNo, TransactionContext? context)
//    {
//        var (_, rawData) = _dbfFile.ReadRawRecord(recNo);
//        var newRecNo = AppendRawRecord(rawData);
//        // Aggiornato per usare la proprietà unificata 'UndoLogIdentifiers'.
//        context?.UndoLogIdentifiers.Add(newRecNo);
//        return newRecNo;
//    }
//
//    /// <summary>
//    /// Rilascia tutte le risorse gestite, inclusi gli handle di file per DBF e WAL,
//    /// e il gestore di lock multi-processo.
//    /// </summary>
//    public void Dispose()
//    {
//        FlushDurability();
//        _wal.Dispose();
//        _dbfFile.Dispose();
//        RecordLockManager.Dispose();
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* C#: DbfTransactionalFileHandle — WAL su write/append/delete/zap; IntraProcess auto-commit salta WAL. */

struct DbfTxUndo {
    int recNo;
    uint8_t* raw;
    int rawLen;
};

struct DbfTxHandle {
    DbfFile* dbf;
    WriteAheadLog* wal;
    LockingStrategy strategy;
    DbfDurabilityMode durability;
    int explicitTx;
    int64_t txId;
    int64_t nextTxId;
    int bulkDepth;
    int groupCommitDepth;
    int fileLockDepth;
    int directWalDirty;
    int commitPendingFinalize;
    DbfHeader txSnapshot;
    int hasTxSnapshot;
    struct DbfTxUndo* undo;
    int undoN;
    int undoCap;
};

static void dbf_tx_clear_undo(DbfTxHandle* self);
static int dbf_tx_capture(DbfTxHandle* self, int recNo);

static int tx_use_transactional_wal(const DbfTxHandle* self)
{
    return self && self->explicitTx;
}

static int tx_use_direct_wal(const DbfTxHandle* self)
{
    return self && !self->explicitTx && self->strategy == LockingStrategy_CrossProcess;
}

static int tx_use_wal(const DbfTxHandle* self)
{
    return tx_use_transactional_wal(self) || tx_use_direct_wal(self);
}

static int tx_should_fsync(const DbfTxHandle* self)
{
    return self && self->durability == DbfDurabilityMode_Full && self->bulkDepth == 0 && self->groupCommitDepth == 0;
}

static void tx_sync_direct_wal(DbfTxHandle* self)
{
    if (!self || !self->directWalDirty || !tx_use_direct_wal(self)) return;
    WriteAheadLog_Flush(self->wal);
    self->directWalDirty = 0;
}

static void tx_finalize_direct_after_sync(DbfTxHandle* self)
{
    if (!tx_use_direct_wal(self)) return;
    WriteAheadLog_LogDirectCheckpoint(self->wal);
    WriteAheadLog_Delete(self->wal, 1);
    self->directWalDirty = 0;
}

static void tx_stamp_header(DbfHeader* h)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    h->Year = (uint8_t)(tm.tm_year % 100);
    h->Month = (uint8_t)(tm.tm_mon + 1);
    h->Day = (uint8_t)tm.tm_mday;
}

DbfTxHandle* DbfTxHandle_New(const char* filePath, int exclusive, LockingStrategy lockingStrategy)
{
    DbfTxHandle* self;
    if (!filePath) return NULL;
    WriteAheadLog_Recover(filePath);
    self = (DbfTxHandle*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    self->strategy = lockingStrategy;
    self->durability = DbfDurabilityMode_Full;
    self->nextTxId = 1;
    self->dbf = DbfFile_New(filePath, exclusive, lockingStrategy);
    if (!self->dbf) {
        free(self);
        return NULL;
    }
    self->wal = WriteAheadLog_New(filePath);
    if (!self->wal) {
        DbfFile_Dispose(self->dbf);
        free(self);
        return NULL;
    }
    return self;
}

void DbfTxHandle_FlushDurability(DbfTxHandle* self)
{
    if (!self) return;
    DbfFile_FlushPendingHeader(self->dbf);
    if (tx_use_direct_wal(self)) {
        tx_sync_direct_wal(self);
        DbfFile_FlushStream(self->dbf, 1, 0);
        tx_finalize_direct_after_sync(self);
        return;
    }
    if (tx_use_transactional_wal(self)) WriteAheadLog_Flush(self->wal);
    DbfFile_FlushStream(self->dbf, 1, 0);
    if (tx_use_transactional_wal(self)) WriteAheadLog_Delete(self->wal, 1);
}

void DbfTxHandle_Dispose(DbfTxHandle* self)
{
    if (!self) return;
    DbfTxHandle_FlushDurability(self);
    dbf_tx_clear_undo(self);
    if (self->wal) WriteAheadLog_Dispose(self->wal);
    if (self->dbf) DbfFile_Dispose(self->dbf);
    free(self);
}

DbfFile* DbfTxHandle_GetDbfFile(DbfTxHandle* self)
{
    return self ? self->dbf : NULL;
}

int DbfTxHandle_UseTransactionalWal(const DbfTxHandle* self)
{
    return tx_use_transactional_wal(self);
}

void DbfTxHandle_SetDurability(DbfTxHandle* self, DbfDurabilityMode mode)
{
    if (self) self->durability = mode;
}

DbfDurabilityMode DbfTxHandle_GetDurability(const DbfTxHandle* self)
{
    return self ? self->durability : DbfDurabilityMode_Full;
}

void DbfTxHandle_BeginBulkLoad(DbfTxHandle* self)
{
    if (self) self->bulkDepth++;
}

void DbfTxHandle_EndBulkLoad(DbfTxHandle* self)
{
    if (!self || self->bulkDepth == 0) return;
    self->bulkDepth--;
    if (self->bulkDepth > 0) return;
    DbfFile_FlushPendingHeader(self->dbf);
    if (tx_use_wal(self)) {
        if (tx_use_direct_wal(self)) tx_sync_direct_wal(self);
        else WriteAheadLog_Flush(self->wal);
    }
    DbfFile_FlushStream(self->dbf, tx_should_fsync(self), 0);
}

void DbfTxHandle_BeginGroupCommit(DbfTxHandle* self)
{
    if (self) self->groupCommitDepth++;
}

void DbfTxHandle_EndGroupCommit(DbfTxHandle* self)
{
    if (!self || self->groupCommitDepth == 0) return;
    self->groupCommitDepth--;
    if (self->groupCommitDepth == 0) DbfTxHandle_FinalizeAutoCommit(self);
}

static void dbf_tx_clear_undo(DbfTxHandle* self)
{
    int i;
    if (!self) return;
    for (i = 0; i < self->undoN; i++) free(self->undo[i].raw);
    free(self->undo);
    self->undo = NULL;
    self->undoN = 0;
    self->undoCap = 0;
    self->hasTxSnapshot = 0;
}

static int dbf_tx_capture(DbfTxHandle* self, int recNo)
{
    uint8_t* payload = NULL;
    int isDeleted = 0, rawLen = 0, i;
    const DbfHeader* h;
    if (!self || !self->explicitTx || recNo <= 0) return GMB_DB_OK;
    h = DbfFile_get_Header(self->dbf);
    if ((uint32_t)recNo > h->NumberOfRecords) return GMB_DB_OK;
    for (i = 0; i < self->undoN; i++) {
        if (self->undo[i].recNo == recNo) return GMB_DB_OK;
    }
    if (DbfFile_ReadRawRecord(self->dbf, recNo, &isDeleted, &payload, &rawLen) != GMB_DB_OK)
        return GMB_DB_ERR;
    if (self->undoN >= self->undoCap) {
        int cap = self->undoCap ? self->undoCap * 2 : 8;
        struct DbfTxUndo* u = (struct DbfTxUndo*)realloc(self->undo, (size_t)cap * sizeof(*u));
        if (!u) { free(payload); return GMB_DB_ERR; }
        self->undo = u;
        self->undoCap = cap;
    }
    self->undo[self->undoN].recNo = recNo;
    self->undo[self->undoN].raw = payload;
    self->undo[self->undoN].rawLen = rawLen;
    self->undoN++;
    (void)isDeleted;
    return GMB_DB_OK;
}

int64_t DbfTxHandle_BeginTransaction(DbfTxHandle* self)
{
    const DbfHeader* h;
    if (!self) return 0;
    if (self->explicitTx) return self->txId;
    h = DbfFile_get_Header(self->dbf);
    if (h) {
        self->txSnapshot = *h;
        self->hasTxSnapshot = 1;
    }
    self->nextTxId++;
    self->txId = self->nextTxId;
    self->explicitTx = 1;
    WriteAheadLog_LogBeginTransaction(self->wal);
    WriteAheadLog_Flush(self->wal);
    return self->txId;
}

int DbfTxHandle_CommitTransaction(DbfTxHandle* self)
{
    if (!self || !self->explicitTx) return GMB_DB_ERR;
    self->explicitTx = 0;
    self->commitPendingFinalize = 1;
    WriteAheadLog_LogCommitTransaction(self->wal);
    dbf_tx_clear_undo(self);
    return GMB_DB_OK;
}

int DbfTxHandle_RollbackTransaction(DbfTxHandle* self)
{
    int i;
    if (!self || !self->explicitTx) return GMB_DB_ERR;
    for (i = 0; i < self->undoN; i++) {
        if (DbfFile_WriteRawRecord(self->dbf, self->undo[i].recNo, self->undo[i].raw, self->undo[i].rawLen) != GMB_DB_OK)
            return GMB_DB_ERR;
    }
    if (self->hasTxSnapshot && DbfFile_RestoreHeader(self->dbf, &self->txSnapshot) != GMB_DB_OK)
        return GMB_DB_ERR;
    if (self->wal) WriteAheadLog_Delete(self->wal, 1);
    dbf_tx_clear_undo(self);
    self->explicitTx = 0;
    return GMB_DB_OK;
}

void DbfTxHandle_FinalizeCommit(DbfTxHandle* self)
{
    int dropWal;
    if (!self) return;
    dropWal = self->commitPendingFinalize || tx_use_transactional_wal(self);
    DbfFile_FlushPendingHeader(self->dbf);
    if (tx_use_direct_wal(self)) {
        tx_sync_direct_wal(self);
        DbfFile_FlushStream(self->dbf, 1, 0);
        tx_finalize_direct_after_sync(self);
        self->commitPendingFinalize = 0;
        return;
    }
    if (dropWal) WriteAheadLog_Flush(self->wal);
    DbfFile_FlushStream(self->dbf, 1, 0);
    if (dropWal) WriteAheadLog_Delete(self->wal, 1);
    self->commitPendingFinalize = 0;
}

void DbfTxHandle_FinalizeAutoCommit(DbfTxHandle* self)
{
    if (!self) return;
    DbfFile_FlushPendingHeader(self->dbf);
    if (tx_use_direct_wal(self)) {
        if (tx_should_fsync(self)) {
            tx_sync_direct_wal(self);
            DbfFile_FlushStream(self->dbf, 1, 0);
            tx_finalize_direct_after_sync(self);
        } else {
            DbfFile_FlushStream(self->dbf, 0, 0);
        }
        return;
    }
    if (tx_use_transactional_wal(self)) {
        WriteAheadLog_LogCommitTransaction(self->wal);
        DbfFile_FlushStream(self->dbf, 1, 0);
        WriteAheadLog_Delete(self->wal, 1);
    } else {
        DbfFile_FlushStream(self->dbf, tx_should_fsync(self), 0);
    }
}

int DbfTxHandle_WriteRawRecord(DbfTxHandle* self, int recNo, const uint8_t* rawData, int rawLen)
{
    const DbfHeader* h;
    int64_t offset;
    if (!self || !rawData) return GMB_DB_ARG;
    if (dbf_tx_capture(self, recNo) != GMB_DB_OK) return GMB_DB_ERR;
    h = DbfFile_get_Header(self->dbf);
    offset = (int64_t)h->HeaderLength + ((int64_t)(recNo - 1) * h->RecordLength);
    if (tx_use_transactional_wal(self)) {
        WriteAheadLog_LogWriteBytes(self->wal, offset + 1, rawData, rawLen);
    } else if (tx_use_direct_wal(self)) {
        WriteAheadLog_LogDirectWrite(self->wal, offset + 1, rawData, rawLen);
        self->directWalDirty = 1;
    }
    return DbfFile_WriteRawRecord(self->dbf, recNo, rawData, rawLen);
}

int DbfTxHandle_AppendRawRecord(DbfTxHandle* self, const uint8_t* rawData, int rawLen)
{
    DbfHeader current, future;
    int64_t newOff;
    uint8_t hdrBytes[DbfHeader_Size];
    uint8_t* full;
    int recLen, rc;
    if (!self || !rawData) return GMB_DB_ARG;
    current = *DbfFile_get_Header(self->dbf);
    newOff = (int64_t)current.HeaderLength + ((int64_t)current.NumberOfRecords * current.RecordLength);
    future = current;
    future.NumberOfRecords++;
    tx_stamp_header(&future);
    recLen = (int)current.RecordLength;
    if (tx_use_transactional_wal(self)) {
        DbfHeader_ToBytes(&future, hdrBytes);
        WriteAheadLog_LogUpdateHeader(self->wal, hdrBytes, DbfHeader_Size);
        full = (uint8_t*)calloc(1, (size_t)recLen);
        if (!full) return GMB_DB_ERR;
        full[0] = (uint8_t)' ';
        if (rawLen > recLen - 1) rawLen = recLen - 1;
        if (rawLen > 0) memcpy(full + 1, rawData, (size_t)rawLen);
        WriteAheadLog_LogWriteRecord(self->wal, newOff, full, recLen);
        free(full);
    } else if (tx_use_direct_wal(self)) {
        DbfHeader_ToBytes(&future, hdrBytes);
        WriteAheadLog_LogDirectWrite(self->wal, 0, hdrBytes, DbfHeader_Size);
        full = (uint8_t*)calloc(1, (size_t)recLen);
        if (!full) return GMB_DB_ERR;
        full[0] = (uint8_t)' ';
        if (rawLen > recLen - 1) rawLen = recLen - 1;
        if (rawLen > 0) memcpy(full + 1, rawData, (size_t)rawLen);
        WriteAheadLog_LogDirectWrite(self->wal, newOff, full, recLen);
        free(full);
        self->directWalDirty = 1;
    }
    rc = DbfFile_AppendRawRecord(self->dbf, rawData, rawLen);
    if (tx_use_wal(self)) WriteAheadLog_Flush(self->wal);
    return rc;
}

int DbfTxHandle_SetDeletedFlag(DbfTxHandle* self, int recNo, int isDeleted)
{
    const DbfHeader* h;
    int64_t offset;
    uint8_t flag[1];
    if (!self) return GMB_DB_ARG;
    if (dbf_tx_capture(self, recNo) != GMB_DB_OK) return GMB_DB_ERR;
    h = DbfFile_get_Header(self->dbf);
    offset = (int64_t)h->HeaderLength + ((int64_t)(recNo - 1) * h->RecordLength);
    flag[0] = isDeleted ? (uint8_t)'*' : (uint8_t)' ';
    if (tx_use_transactional_wal(self)) {
        WriteAheadLog_LogWriteBytes(self->wal, offset, flag, 1);
    } else if (tx_use_direct_wal(self)) {
        WriteAheadLog_LogDirectWrite(self->wal, offset, flag, 1);
        self->directWalDirty = 1;
    }
    return DbfFile_SetDeletedFlag(self->dbf, recNo, isDeleted);
}

int DbfTxHandle_Zap(DbfTxHandle* self)
{
    DbfHeader future;
    uint8_t hdrBytes[DbfHeader_Size];
    if (!self) return GMB_DB_ARG;
    future = *DbfFile_get_Header(self->dbf);
    future.NumberOfRecords = 0;
    tx_stamp_header(&future);
    DbfHeader_ToBytes(&future, hdrBytes);
    WriteAheadLog_LogUpdateHeader(self->wal, hdrBytes, DbfHeader_Size);
    return DbfFile_Zap(self->dbf);
}

int DbfTxHandle_IsExplicitTransactionActive(const DbfTxHandle* self)
{
    return self && self->explicitTx;
}
