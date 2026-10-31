// FILE: Basic.Databases.Locks.CrossProcessLockManager.cs

using System.Collections.Concurrent;
using System.Text;
using Basic.ISAM;

// ReSharper disable CheckNamespace
// ReSharper disable InconsistentNaming
namespace Basic.Databases;

/// <summary>
/// Implementa ILockManager utilizzando Mutex nominati a livello di sistema operativo.
/// Simula un lock Lettura/Scrittura (Shared/Exclusive) quando i semafori nominati sono disponibili;
/// altrimenti ricade su mutex esclusivi (macOS e altre piattaforme senza semafori nominati).
/// </summary>
internal class CrossProcessLockManager : LockManagerBase
{
    private const int MaxConcurrentReaders = 1024;

    private sealed class ResourceLockHandles(string writeMutexName, string readSemaphoreName) : IDisposable
    {
        public Mutex WriteMutex { get; } = new(false, writeMutexName);
        public Semaphore? ReadSemaphore { get; } = TryCreateReadSemaphore(readSemaphoreName);

        private static Semaphore? TryCreateReadSemaphore(string name)
        {
            try
            {
                return new Semaphore(MaxConcurrentReaders, MaxConcurrentReaders, name);
            }
            catch (PlatformNotSupportedException)
            {
                return null;
            }
        }

        public void Dispose()
        {
            WriteMutex.Dispose();
            ReadSemaphore?.Dispose();
        }
    }

    private enum LockType
    {
        Read,
        Write
    }

    private readonly ConcurrentDictionary<string, (ResourceLockHandles handles, LockType type)> _acquiredLocks = new();
    private readonly ConcurrentDictionary<string, ResourceLockHandles> _lockHandlesCache = new();
    private readonly string _fileId;

    /// <summary>
    /// Inizializza una nuova istanza della classe CrossProcessLockManager.
    /// </summary>
    /// <param name="filePath">
    /// Il percorso completo del file di database da proteggere. Viene usato per generare nomi univoci
    /// per gli oggetti di sincronizzazione del sistema operativo.
    /// </param>
    public CrossProcessLockManager(string filePath)
    {
        _fileId = Convert.ToBase64String(Encoding.UTF8.GetBytes(Path.GetFullPath(filePath).ToUpperInvariant()))
            .Replace('/', '_').Replace('+', '-');
    }

    /// <inheritdoc />
    public override void AcquireReadLock(string resourceKey, int timeoutMilliseconds, string resourceDescription)
    {
        var handles = GetOrCreateHandles(resourceKey);
        var threadId = Environment.CurrentManagedThreadId;
        var deadline = NormalizeDeadline(timeoutMilliseconds);

        DeadlockMonitor.SetWaiting(threadId, resourceKey, wantsWrite: false);
        try
        {
            if (handles.ReadSemaphore is { } readSemaphore)
            {
                WaitWithDeadlockDetection(
                    threadId,
                    resourceKey,
                    wantsWrite: false,
                    deadline,
                    resourceDescription,
                    () => readSemaphore.WaitOne(0));
            }
            else
            {
                WaitWithDeadlockDetection(
                    threadId,
                    resourceKey,
                    wantsWrite: false,
                    deadline,
                    resourceDescription,
                    () => handles.WriteMutex.WaitOne(0));
            }
        }
        finally
        {
            DeadlockMonitor.ClearWaiting(threadId);
        }

        _acquiredLocks[resourceKey] = (handles, LockType.Read);
        DeadlockMonitor.RecordReadAcquired(threadId, resourceKey);
    }

    /// <inheritdoc />
    public override void AcquireWriteLock(string resourceKey, int timeoutMilliseconds, string resourceDescription)
    {
        var handles = GetOrCreateHandles(resourceKey);
        var threadId = Environment.CurrentManagedThreadId;
        var deadline = NormalizeDeadline(timeoutMilliseconds);

        DeadlockMonitor.SetWaiting(threadId, resourceKey, wantsWrite: true);
        try
        {
            WaitWithDeadlockDetection(
                threadId,
                resourceKey,
                wantsWrite: true,
                deadline,
                $"{resourceDescription} (fase 1)",
                () => handles.WriteMutex.WaitOne(0));

            if (handles.ReadSemaphore is { } readSemaphore)
            {
                try
                {
                    for (var i = 0; i < MaxConcurrentReaders; i++)
                    {
                        WaitWithDeadlockDetection(
                            threadId,
                            resourceKey,
                            wantsWrite: true,
                            deadline,
                            $"{resourceDescription} (fase 2)",
                            () => readSemaphore.WaitOne(0));
                    }
                }
                catch
                {
                    handles.WriteMutex.ReleaseMutex();
                    throw;
                }
            }
        }
        finally
        {
            DeadlockMonitor.ClearWaiting(threadId);
        }

        _acquiredLocks[resourceKey] = (handles, LockType.Write);
        DeadlockMonitor.RecordWriteAcquired(threadId, resourceKey);
    }

    private static DateTime NormalizeDeadline(int timeoutMilliseconds) =>
        timeoutMilliseconds < 0 ? DateTime.MaxValue : DateTime.UtcNow.AddMilliseconds(timeoutMilliseconds);

    private static void WaitWithDeadlockDetection(
        int threadId,
        string resourceKey,
        bool wantsWrite,
        DateTime deadline,
        string resourceDescription,
        Func<bool> tryWait)
    {
        while (!tryWait())
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
                    $"Timeout scaduto durante l'attesa del lock multi-processo su {resourceDescription}.");
            }

            Thread.Sleep(DeadlockMonitor.PollIntervalMs);
        }
    }

    /// <inheritdoc />
    public override void ReleaseReadLock(string resourceKey)
    {
        if (_acquiredLocks.TryRemove(resourceKey, out var lockInfo) && lockInfo.type == LockType.Read)
        {
            if (lockInfo.handles.ReadSemaphore is { } readSemaphore)
            {
                readSemaphore.Release();
            }
            else
            {
                lockInfo.handles.WriteMutex.ReleaseMutex();
            }

            DeadlockMonitor.RecordReadReleased(Environment.CurrentManagedThreadId, resourceKey);
        }
    }

    /// <inheritdoc />
    public override void ReleaseWriteLock(string resourceKey)
    {
        if (_acquiredLocks.TryRemove(resourceKey, out var lockInfo) && lockInfo.type == LockType.Write)
        {
            lockInfo.handles.ReadSemaphore?.Release(MaxConcurrentReaders);
            lockInfo.handles.WriteMutex.ReleaseMutex();
            DeadlockMonitor.RecordWriteReleased(Environment.CurrentManagedThreadId, resourceKey);
        }
    }

    /// <inheritdoc />
    public override void Dispose()
    {
        foreach (var key in _acquiredLocks.Keys.ToList())
        {
            if (!_acquiredLocks.TryGetValue(key, out var lockInfo))
            {
                continue;
            }

            if (lockInfo.type == LockType.Read)
            {
                ReleaseReadLock(key);
            }
            else
            {
                ReleaseWriteLock(key);
            }
        }

        foreach (var handles in _lockHandlesCache.Values)
        {
            handles.Dispose();
        }

        _lockHandlesCache.Clear();
    }

    #region Metodi Privati

    private string GetHandleName(string resourceKey, string handleType)
    {
        var baseName = $"GMBASIC_DB_{_fileId}_{resourceKey}_{handleType}";
        return OperatingSystem.IsWindows() ? $"Global\\{baseName}" : baseName;
    }

    private ResourceLockHandles GetOrCreateHandles(string resourceKey)
    {
        return _lockHandlesCache.GetOrAdd(resourceKey, key =>
        {
            var mutexName = GetHandleName(key, "WMUTEX");
            var semaphoreName = GetHandleName(key, "RSEMA");
            return new ResourceLockHandles(mutexName, semaphoreName);
        });
    }

    #endregion
}
