// FILE: Basic.Databases.Locks.IntraProcessLockManager.cs

using System.Collections.Concurrent;
using Basic.ISAM;

// ReSharper disable CheckNamespace
// ReSharper disable InconsistentNaming

namespace Basic.Databases;

/// <summary>
/// Locking <see cref="ReaderWriterLockSlim"/> intra-processo con rilevamento deadlock.
/// </summary>
internal sealed class IntraProcessLockManager : LockManagerBase
{
    private readonly ConcurrentDictionary<string, ReaderWriterLockSlim> _resourceLocks = new();

    /// <inheritdoc />
    public override void AcquireReadLock(string resourceKey, int timeoutMilliseconds, string resourceDescription)
    {
        var rwLock =
            _resourceLocks.GetOrAdd(resourceKey, _ => new ReaderWriterLockSlim(LockRecursionPolicy.NoRecursion));
        AcquireWithDeadlockDetection(
            resourceKey,
            timeoutMilliseconds,
            resourceDescription,
            wantsWrite: false,
            rwLock,
            static (lockSlim, timeout) => lockSlim.TryEnterReadLock(timeout));
    }

    /// <inheritdoc />
    public override void AcquireWriteLock(string resourceKey, int timeoutMilliseconds, string resourceDescription)
    {
        var rwLock =
            _resourceLocks.GetOrAdd(resourceKey, _ => new ReaderWriterLockSlim(LockRecursionPolicy.NoRecursion));
        AcquireWithDeadlockDetection(
            resourceKey,
            timeoutMilliseconds,
            resourceDescription,
            wantsWrite: true,
            rwLock,
            static (lockSlim, timeout) => lockSlim.TryEnterWriteLock(timeout));
    }

    /// <inheritdoc />
    public override void ReleaseReadLock(string resourceKey)
    {
        var threadId = Environment.CurrentManagedThreadId;
        if (_resourceLocks.TryGetValue(resourceKey, out var rwLock) && rwLock.IsReadLockHeld)
        {
            rwLock.ExitReadLock();
            DeadlockMonitor.RecordReadReleased(threadId, resourceKey);
        }
    }

    /// <inheritdoc />
    public override void ReleaseWriteLock(string resourceKey)
    {
        var threadId = Environment.CurrentManagedThreadId;
        if (_resourceLocks.TryGetValue(resourceKey, out var rwLock) && rwLock.IsWriteLockHeld)
        {
            rwLock.ExitWriteLock();
            DeadlockMonitor.RecordWriteReleased(threadId, resourceKey);
        }
    }

    /// <inheritdoc />
    public override void Dispose()
    {
        foreach (var rwLock in _resourceLocks.Values)
        {
            rwLock.Dispose();
        }

        _resourceLocks.Clear();
    }

    /// <summary>
    /// Rilascia tutti i lock ancora tenuti dal thread corrente (record lock rimasti attivi
    /// perche' un UNLOCK e' stato richiesto quando il cursore non era piu' sul record bloccato
    /// — IsamDriver.Unlock usava il record corrente, non il numero passato, fix in
    /// IsamDriver.Unlock/Engine.ReleaseAllRecordLocks). Senza questo, il Dispose smaltiva un
    /// ReaderWriterLockSlim ancora tenuto e lanciava "The lock is being disposed while still
    /// being used" (osservato chiudendo un'area ISAM dopo RLOCK+UNLOCK con cursore spostato,
    /// poi riaprendo un DBF). Rimane come rete di sicurezza a Dispose anche dopo il fix nel
    /// percorso normale (RLOCK annidati/percorsi d'errore non ancora coperti).
    /// </summary>
    public void ReleaseAllHeld()
    {
        foreach (var kv in _resourceLocks)
        {
            try
            {
                if (kv.Value.IsWriteLockHeld) kv.Value.ExitWriteLock();
                else if (kv.Value.IsReadLockHeld) kv.Value.ExitReadLock();
            }
            catch { /* best-effort: nessun lock tenuto in realtà */ }
        }
    }

    /// <summary>
    /// Come <see cref="ReleaseAllHeld"/> ma limitato ai lock RECORD (chiave "R_offset", vedi
    /// <see cref="LockManagerBase.GetRecordLockKey"/>) — usato da UNLOCK (record) per
    /// rilasciare il lock ORIGINALE indipendentemente da dove si trova ora il cursore, senza
    /// toccare il lock di struttura file o altri lock non-record che il thread potrebbe tenere
    /// contemporaneamente (es. durante REINDEX/PACK).
    /// </summary>
    public void ReleaseAllRecordLocksHeld()
    {
        foreach (var kv in _resourceLocks)
        {
            if (!kv.Key.StartsWith("R_", StringComparison.Ordinal)) continue;
            try
            {
                if (kv.Value.IsWriteLockHeld) kv.Value.ExitWriteLock();
                else if (kv.Value.IsReadLockHeld) kv.Value.ExitReadLock();
            }
            catch { /* best-effort: nessun lock tenuto in realtà */ }
        }
    }

    private static void AcquireWithDeadlockDetection(
        string resourceKey,
        int timeoutMilliseconds,
        string resourceDescription,
        bool wantsWrite,
        ReaderWriterLockSlim rwLock,
        Func<ReaderWriterLockSlim, int, bool> tryEnter)
    {
        var threadId = Environment.CurrentManagedThreadId;
        var deadline = timeoutMilliseconds < 0
            ? DateTime.MaxValue
            : DateTime.UtcNow.AddMilliseconds(timeoutMilliseconds);

        DeadlockMonitor.SetWaiting(threadId, resourceKey, wantsWrite);
        try
        {
            while (!tryEnter(rwLock, 0))
            {
                if (DeadlockMonitor.IsAbortRequested(threadId))
                {
                    DeadlockMonitor.ClearAbort(threadId);
                    var abortMsg =
                        $"Transazione interrotta dal rilevatore di deadlock su {resourceDescription}.";
                    IsamSlowOpLog.LogDeadlock(abortMsg);
                    throw new DeadlockException(abortMsg);
                }

                if (DeadlockMonitor.TryResolveDeadlock(threadId, resourceKey, wantsWrite, out var victim) &&
                    victim == threadId)
                {
                    var victimMsg =
                        $"Deadlock rilevato; transazione annullata (vittima designata) su {resourceDescription}.";
                    IsamSlowOpLog.LogDeadlock(victimMsg);
                    throw new DeadlockException(victimMsg);
                }

                if (DateTime.UtcNow >= deadline)
                {
                    throw new LockTimeoutException(
                        $"Timeout scaduto ({timeoutMilliseconds}ms) durante l'attesa del lock {(wantsWrite ? "di scrittura" : "di lettura")} intra-processo su {resourceDescription}.");
                }

                Thread.Sleep(DeadlockMonitor.PollIntervalMs);
            }

            if (wantsWrite)
            {
                DeadlockMonitor.RecordWriteAcquired(threadId, resourceKey);
            }
            else
            {
                DeadlockMonitor.RecordReadAcquired(threadId, resourceKey);
            }
        }
        finally
        {
            DeadlockMonitor.ClearWaiting(threadId);
        }
    }
}
