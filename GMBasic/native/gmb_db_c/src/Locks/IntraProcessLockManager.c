/* FILE: native/gmb_db_c/src/Locks/IntraProcessLockManager.c */
/* Port 1:1 di modules/Database/Locks/IntraProcessLockManager.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Locks.IntraProcessLockManager.cs
//
//using System.Collections.Concurrent;
//using Basic.ISAM;
//
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//
//namespace Basic.Databases;
//
///// <summary>
///// Locking <see cref="ReaderWriterLockSlim"/> intra-processo con rilevamento deadlock.
///// </summary>
//internal sealed class IntraProcessLockManager : LockManagerBase
//{
//    private readonly ConcurrentDictionary<string, ReaderWriterLockSlim> _resourceLocks = new();
//
//    /// <inheritdoc />
//    public override void AcquireReadLock(string resourceKey, int timeoutMilliseconds, string resourceDescription)
//    {
//        var rwLock =
//            _resourceLocks.GetOrAdd(resourceKey, _ => new ReaderWriterLockSlim(LockRecursionPolicy.NoRecursion));
//        AcquireWithDeadlockDetection(
//            resourceKey,
//            timeoutMilliseconds,
//            resourceDescription,
//            wantsWrite: false,
//            rwLock,
//            static (lockSlim, timeout) => lockSlim.TryEnterReadLock(timeout));
//    }
//
//    /// <inheritdoc />
//    public override void AcquireWriteLock(string resourceKey, int timeoutMilliseconds, string resourceDescription)
//    {
//        var rwLock =
//            _resourceLocks.GetOrAdd(resourceKey, _ => new ReaderWriterLockSlim(LockRecursionPolicy.NoRecursion));
//        AcquireWithDeadlockDetection(
//            resourceKey,
//            timeoutMilliseconds,
//            resourceDescription,
//            wantsWrite: true,
//            rwLock,
//            static (lockSlim, timeout) => lockSlim.TryEnterWriteLock(timeout));
//    }
//
//    /// <inheritdoc />
//    public override void ReleaseReadLock(string resourceKey)
//    {
//        var threadId = Environment.CurrentManagedThreadId;
//        if (_resourceLocks.TryGetValue(resourceKey, out var rwLock) && rwLock.IsReadLockHeld)
//        {
//            rwLock.ExitReadLock();
//            DeadlockMonitor.RecordReadReleased(threadId, resourceKey);
//        }
//    }
//
//    /// <inheritdoc />
//    public override void ReleaseWriteLock(string resourceKey)
//    {
//        var threadId = Environment.CurrentManagedThreadId;
//        if (_resourceLocks.TryGetValue(resourceKey, out var rwLock) && rwLock.IsWriteLockHeld)
//        {
//            rwLock.ExitWriteLock();
//            DeadlockMonitor.RecordWriteReleased(threadId, resourceKey);
//        }
//    }
//
//    /// <inheritdoc />
//    public override void Dispose()
//    {
//        foreach (var rwLock in _resourceLocks.Values)
//        {
//            rwLock.Dispose();
//        }
//
//        _resourceLocks.Clear();
//    }
//
//    /// <summary>
//    /// Rilascia tutti i lock ancora tenuti dal thread corrente (record lock rimasti attivi
//    /// perche' un UNLOCK e' stato richiesto quando il cursore non era piu' sul record bloccato
//    /// — IsamDriver.Unlock usa il record corrente, non il numero passato). Senza questo, il
//    /// Dispose smaltiva un ReaderWriterLockSlim ancora tenuto e lanciava "The lock is being
//    /// disposed while still being used" (osservato chiudendo un'area ISAM dopo RLOCK+UNLOCK
//    /// con cursore spostato, poi riaprendo un DBF).
//    /// </summary>
//    public void ReleaseAllHeld()
//    {
//        foreach (var kv in _resourceLocks)
//        {
//            try
//            {
//                if (kv.Value.IsWriteLockHeld) kv.Value.ExitWriteLock();
//                else if (kv.Value.IsReadLockHeld) kv.Value.ExitReadLock();
//            }
//            catch { /* best-effort: nessun lock tenuto in realtà */ }
//        }
//    }
//
//    private static void AcquireWithDeadlockDetection(
//        string resourceKey,
//        int timeoutMilliseconds,
//        string resourceDescription,
//        bool wantsWrite,
//        ReaderWriterLockSlim rwLock,
//        Func<ReaderWriterLockSlim, int, bool> tryEnter)
//    {
//        var threadId = Environment.CurrentManagedThreadId;
//        var deadline = timeoutMilliseconds < 0
//            ? DateTime.MaxValue
//            : DateTime.UtcNow.AddMilliseconds(timeoutMilliseconds);
//
//        DeadlockMonitor.SetWaiting(threadId, resourceKey, wantsWrite);
//        try
//        {
//            while (!tryEnter(rwLock, 0))
//            {
//                if (DeadlockMonitor.IsAbortRequested(threadId))
//                {
//                    DeadlockMonitor.ClearAbort(threadId);
//                    var abortMsg =
//                        $"Transazione interrotta dal rilevatore di deadlock su {resourceDescription}.";
//                    IsamSlowOpLog.LogDeadlock(abortMsg);
//                    throw new DeadlockException(abortMsg);
//                }
//
//                if (DeadlockMonitor.TryResolveDeadlock(threadId, resourceKey, wantsWrite, out var victim) &&
//                    victim == threadId)
//                {
//                    var victimMsg =
//                        $"Deadlock rilevato; transazione annullata (vittima designata) su {resourceDescription}.";
//                    IsamSlowOpLog.LogDeadlock(victimMsg);
//                    throw new DeadlockException(victimMsg);
//                }
//
//                if (DateTime.UtcNow >= deadline)
//                {
//                    throw new LockTimeoutException(
//                        $"Timeout scaduto ({timeoutMilliseconds}ms) durante l'attesa del lock {(wantsWrite ? "di scrittura" : "di lettura")} intra-processo su {resourceDescription}.");
//                }
//
//                Thread.Sleep(DeadlockMonitor.PollIntervalMs);
//            }
//
//            if (wantsWrite)
//            {
//                DeadlockMonitor.RecordWriteAcquired(threadId, resourceKey);
//            }
//            else
//            {
//                DeadlockMonitor.RecordReadAcquired(threadId, resourceKey);
//            }
//        }
//        finally
//        {
//            DeadlockMonitor.ClearWaiting(threadId);
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

typedef struct RwEntry {
    char key[256];
    pthread_rwlock_t lock;
    int writeHeld;
    int readHeld;
    struct RwEntry* next;
} RwEntry;

struct IntraProcessLockManager {
    RwEntry* locks;
    pthread_mutex_t tableMu;
};

static int current_thread_id(void) { return (int)(intptr_t)pthread_self(); }

IntraProcessLockManager* IntraProcessLockManager_New(void)
{
    IntraProcessLockManager* self = (IntraProcessLockManager*)calloc(1, sizeof(*self));
    pthread_mutex_init(&self->tableMu, NULL);
    return self;
}

static RwEntry* iplm_get(IntraProcessLockManager* self, const char* key)
{
    RwEntry* e;
    pthread_mutex_lock(&self->tableMu);
    for (e = self->locks; e; e = e->next)
        if (strcmp(e->key, key) == 0) { pthread_mutex_unlock(&self->tableMu); return e; }
    e = (RwEntry*)calloc(1, sizeof(*e));
    snprintf(e->key, sizeof(e->key), "%s", key);
    pthread_rwlock_init(&e->lock, NULL);
    e->next = self->locks;
    self->locks = e;
    pthread_mutex_unlock(&self->tableMu);
    return e;
}

static int iplm_acquire(IntraProcessLockManager* self, const char* resourceKey, int timeoutMilliseconds,
                        const char* resourceDescription, int wantsWrite)
{
    RwEntry* e = iplm_get(self, resourceKey);
    int threadId = current_thread_id();
    struct timespec start, now;
    int64_t deadlineMs;
    clock_gettime(CLOCK_MONOTONIC, &start);
    deadlineMs = timeoutMilliseconds < 0 ? INT64_MAX : (int64_t)timeoutMilliseconds;
    DeadlockMonitor_SetWaiting(threadId, resourceKey, wantsWrite);
    for (;;) {
        int got = wantsWrite ? (pthread_rwlock_trywrlock(&e->lock) == 0) : (pthread_rwlock_tryrdlock(&e->lock) == 0);
        if (got) {
            if (wantsWrite) { e->writeHeld = 1; DeadlockMonitor_RecordWriteAcquired(threadId, resourceKey); }
            else { e->readHeld = 1; DeadlockMonitor_RecordReadAcquired(threadId, resourceKey); }
            DeadlockMonitor_ClearWaiting(threadId);
            return GMB_DB_OK;
        }
        if (DeadlockMonitor_IsAbortRequested(threadId)) {
            char msg[256];
            DeadlockMonitor_ClearAbort(threadId);
            snprintf(msg, sizeof(msg), "Transazione interrotta dal rilevatore di deadlock su %s.", resourceDescription);
            IsamSlowOpLog_LogDeadlock(msg);
            DeadlockMonitor_ClearWaiting(threadId);
            return GMB_DB_DEADLOCK;
        }
        {
            int victim = 0;
            if (DeadlockMonitor_TryResolveDeadlock(threadId, resourceKey, wantsWrite, &victim) && victim == threadId) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Deadlock rilevato; transazione annullata (vittima designata) su %s.", resourceDescription);
                IsamSlowOpLog_LogDeadlock(msg);
                DeadlockMonitor_ClearWaiting(threadId);
                return GMB_DB_DEADLOCK;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (((now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000) >= deadlineMs) {
            DeadlockMonitor_ClearWaiting(threadId);
            return GMB_DB_TIMEOUT;
        }
        usleep((useconds_t)DeadlockMonitor_PollIntervalMs * 1000);
    }
}

int IntraProcessLockManager_AcquireReadLock(IntraProcessLockManager* self, const char* resourceKey,
                                            int timeoutMilliseconds, const char* resourceDescription)
{
    return iplm_acquire(self, resourceKey, timeoutMilliseconds, resourceDescription, 0);
}

int IntraProcessLockManager_AcquireWriteLock(IntraProcessLockManager* self, const char* resourceKey,
                                             int timeoutMilliseconds, const char* resourceDescription)
{
    return iplm_acquire(self, resourceKey, timeoutMilliseconds, resourceDescription, 1);
}

void IntraProcessLockManager_ReleaseReadLock(IntraProcessLockManager* self, const char* resourceKey)
{
    RwEntry* e = iplm_get(self, resourceKey);
    int threadId = current_thread_id();
    if (e->readHeld) {
        pthread_rwlock_unlock(&e->lock);
        e->readHeld = 0;
        DeadlockMonitor_RecordReadReleased(threadId, resourceKey);
    }
}

void IntraProcessLockManager_ReleaseWriteLock(IntraProcessLockManager* self, const char* resourceKey)
{
    RwEntry* e = iplm_get(self, resourceKey);
    int threadId = current_thread_id();
    if (e->writeHeld) {
        pthread_rwlock_unlock(&e->lock);
        e->writeHeld = 0;
        DeadlockMonitor_RecordWriteReleased(threadId, resourceKey);
    }
}

void IntraProcessLockManager_ReleaseAllHeld(IntraProcessLockManager* self)
{
    RwEntry* e;
    for (e = self->locks; e; e = e->next) {
        if (e->writeHeld) { pthread_rwlock_unlock(&e->lock); e->writeHeld = 0; }
        else if (e->readHeld) { pthread_rwlock_unlock(&e->lock); e->readHeld = 0; }
    }
}

void IntraProcessLockManager_Dispose(IntraProcessLockManager* self)
{
    RwEntry* e;
    if (!self) return;
    e = self->locks;
    while (e) {
        RwEntry* n = e->next;
        pthread_rwlock_destroy(&e->lock);
        free(e);
        e = n;
    }
    pthread_mutex_destroy(&self->tableMu);
    free(self);
}
