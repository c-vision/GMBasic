/* FILE: native/gmb_db_c/src/Locks/CrossProcessLockManager.c */
/* Port 1:1 di modules/Database/Locks/CrossProcessLockManager.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Locks.CrossProcessLockManager.cs
//
//using System.Collections.Concurrent;
//using System.Text;
//using Basic.ISAM;
//
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//namespace Basic.Databases;
//
///// <summary>
///// Implementa ILockManager utilizzando Mutex nominati a livello di sistema operativo.
///// Simula un lock Lettura/Scrittura (Shared/Exclusive) quando i semafori nominati sono disponibili;
///// altrimenti ricade su mutex esclusivi (macOS e altre piattaforme senza semafori nominati).
///// </summary>
//internal class CrossProcessLockManager : LockManagerBase
//{
//    private const int MaxConcurrentReaders = 1024;
//
//    private sealed class ResourceLockHandles(string writeMutexName, string readSemaphoreName) : IDisposable
//    {
//        public Mutex WriteMutex { get; } = new(false, writeMutexName);
//        public Semaphore? ReadSemaphore { get; } = TryCreateReadSemaphore(readSemaphoreName);
//
//        private static Semaphore? TryCreateReadSemaphore(string name)
//        {
//            try
//            {
//                return new Semaphore(MaxConcurrentReaders, MaxConcurrentReaders, name);
//            }
//            catch (PlatformNotSupportedException)
//            {
//                return null;
//            }
//        }
//
//        public void Dispose()
//        {
//            WriteMutex.Dispose();
//            ReadSemaphore?.Dispose();
//        }
//    }
//
//    private enum LockType
//    {
//        Read,
//        Write
//    }
//
//    private readonly ConcurrentDictionary<string, (ResourceLockHandles handles, LockType type)> _acquiredLocks = new();
//    private readonly ConcurrentDictionary<string, ResourceLockHandles> _lockHandlesCache = new();
//    private readonly string _fileId;
//
//    /// <summary>
//    /// Inizializza una nuova istanza della classe CrossProcessLockManager.
//    /// </summary>
//    /// <param name="filePath">
//    /// Il percorso completo del file di database da proteggere. Viene usato per generare nomi univoci
//    /// per gli oggetti di sincronizzazione del sistema operativo.
//    /// </param>
//    public CrossProcessLockManager(string filePath)
//    {
//        _fileId = Convert.ToBase64String(Encoding.UTF8.GetBytes(Path.GetFullPath(filePath).ToUpperInvariant()))
//            .Replace('/', '_').Replace('+', '-');
//    }
//
//    /// <inheritdoc />
//    public override void AcquireReadLock(string resourceKey, int timeoutMilliseconds, string resourceDescription)
//    {
//        var handles = GetOrCreateHandles(resourceKey);
//        var threadId = Environment.CurrentManagedThreadId;
//        var deadline = NormalizeDeadline(timeoutMilliseconds);
//
//        DeadlockMonitor.SetWaiting(threadId, resourceKey, wantsWrite: false);
//        try
//        {
//            if (handles.ReadSemaphore is { } readSemaphore)
//            {
//                WaitWithDeadlockDetection(
//                    threadId,
//                    resourceKey,
//                    wantsWrite: false,
//                    deadline,
//                    resourceDescription,
//                    () => readSemaphore.WaitOne(0));
//            }
//            else
//            {
//                WaitWithDeadlockDetection(
//                    threadId,
//                    resourceKey,
//                    wantsWrite: false,
//                    deadline,
//                    resourceDescription,
//                    () => handles.WriteMutex.WaitOne(0));
//            }
//        }
//        finally
//        {
//            DeadlockMonitor.ClearWaiting(threadId);
//        }
//
//        _acquiredLocks[resourceKey] = (handles, LockType.Read);
//        DeadlockMonitor.RecordReadAcquired(threadId, resourceKey);
//    }
//
//    /// <inheritdoc />
//    public override void AcquireWriteLock(string resourceKey, int timeoutMilliseconds, string resourceDescription)
//    {
//        var handles = GetOrCreateHandles(resourceKey);
//        var threadId = Environment.CurrentManagedThreadId;
//        var deadline = NormalizeDeadline(timeoutMilliseconds);
//
//        DeadlockMonitor.SetWaiting(threadId, resourceKey, wantsWrite: true);
//        try
//        {
//            WaitWithDeadlockDetection(
//                threadId,
//                resourceKey,
//                wantsWrite: true,
//                deadline,
//                $"{resourceDescription} (fase 1)",
//                () => handles.WriteMutex.WaitOne(0));
//
//            if (handles.ReadSemaphore is { } readSemaphore)
//            {
//                try
//                {
//                    for (var i = 0; i < MaxConcurrentReaders; i++)
//                    {
//                        WaitWithDeadlockDetection(
//                            threadId,
//                            resourceKey,
//                            wantsWrite: true,
//                            deadline,
//                            $"{resourceDescription} (fase 2)",
//                            () => readSemaphore.WaitOne(0));
//                    }
//                }
//                catch
//                {
//                    handles.WriteMutex.ReleaseMutex();
//                    throw;
//                }
//            }
//        }
//        finally
//        {
//            DeadlockMonitor.ClearWaiting(threadId);
//        }
//
//        _acquiredLocks[resourceKey] = (handles, LockType.Write);
//        DeadlockMonitor.RecordWriteAcquired(threadId, resourceKey);
//    }
//
//    private static DateTime NormalizeDeadline(int timeoutMilliseconds) =>
//        timeoutMilliseconds < 0 ? DateTime.MaxValue : DateTime.UtcNow.AddMilliseconds(timeoutMilliseconds);
//
//    private static void WaitWithDeadlockDetection(
//        int threadId,
//        string resourceKey,
//        bool wantsWrite,
//        DateTime deadline,
//        string resourceDescription,
//        Func<bool> tryWait)
//    {
//        while (!tryWait())
//        {
//            if (DeadlockMonitor.IsAbortRequested(threadId))
//            {
//                DeadlockMonitor.ClearAbort(threadId);
//                var abortMsg =
//                    $"Transazione interrotta dal rilevatore di deadlock su {resourceDescription}.";
//                IsamSlowOpLog.LogDeadlock(abortMsg);
//                throw new DeadlockException(abortMsg);
//            }
//
//            if (DeadlockMonitor.TryResolveDeadlock(threadId, resourceKey, wantsWrite, out var victim) &&
//                victim == threadId)
//            {
//                var victimMsg =
//                    $"Deadlock rilevato; transazione annullata (vittima designata) su {resourceDescription}.";
//                IsamSlowOpLog.LogDeadlock(victimMsg);
//                throw new DeadlockException(victimMsg);
//            }
//
//            if (DateTime.UtcNow >= deadline)
//            {
//                throw new LockTimeoutException(
//                    $"Timeout scaduto durante l'attesa del lock multi-processo su {resourceDescription}.");
//            }
//
//            Thread.Sleep(DeadlockMonitor.PollIntervalMs);
//        }
//    }
//
//    /// <inheritdoc />
//    public override void ReleaseReadLock(string resourceKey)
//    {
//        if (_acquiredLocks.TryRemove(resourceKey, out var lockInfo) && lockInfo.type == LockType.Read)
//        {
//            if (lockInfo.handles.ReadSemaphore is { } readSemaphore)
//            {
//                readSemaphore.Release();
//            }
//            else
//            {
//                lockInfo.handles.WriteMutex.ReleaseMutex();
//            }
//
//            DeadlockMonitor.RecordReadReleased(Environment.CurrentManagedThreadId, resourceKey);
//        }
//    }
//
//    /// <inheritdoc />
//    public override void ReleaseWriteLock(string resourceKey)
//    {
//        if (_acquiredLocks.TryRemove(resourceKey, out var lockInfo) && lockInfo.type == LockType.Write)
//        {
//            lockInfo.handles.ReadSemaphore?.Release(MaxConcurrentReaders);
//            lockInfo.handles.WriteMutex.ReleaseMutex();
//            DeadlockMonitor.RecordWriteReleased(Environment.CurrentManagedThreadId, resourceKey);
//        }
//    }
//
//    /// <inheritdoc />
//    public override void Dispose()
//    {
//        foreach (var key in _acquiredLocks.Keys.ToList())
//        {
//            if (!_acquiredLocks.TryGetValue(key, out var lockInfo))
//            {
//                continue;
//            }
//
//            if (lockInfo.type == LockType.Read)
//            {
//                ReleaseReadLock(key);
//            }
//            else
//            {
//                ReleaseWriteLock(key);
//            }
//        }
//
//        foreach (var handles in _lockHandlesCache.Values)
//        {
//            handles.Dispose();
//        }
//
//        _lockHandlesCache.Clear();
//    }
//
//    #region Metodi Privati
//
//    private string GetHandleName(string resourceKey, string handleType)
//    {
//        var baseName = $"GMBASIC_DB_{_fileId}_{resourceKey}_{handleType}";
//        return OperatingSystem.IsWindows() ? $"Global\\{baseName}" : baseName;
//    }
//
//    private ResourceLockHandles GetOrCreateHandles(string resourceKey)
//    {
//        return _lockHandlesCache.GetOrAdd(resourceKey, key =>
//        {
//            var mutexName = GetHandleName(key, "WMUTEX");
//            var semaphoreName = GetHandleName(key, "RSEMA");
//            return new ResourceLockHandles(mutexName, semaphoreName);
//        });
//    }
//
//    #endregion
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */














#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>

/* ponytail: C# named Mutex/Semaphore. POSIX fcntl is per-process, so a process-wide
 * table serializes CrossProcessLockManager instances in-process; fcntl blocks other
 * processes. Ceiling 64 lock files. Upgrade: grow table. */
#define GMB_CPLM_MAX 64
#define GMB_CPLM_PATH 512
#define GMB_CPLM_KEY 256

enum { GMB_CPLM_NONE = 0, GMB_CPLM_READ = 1, GMB_CPLM_WRITE = 2 };

typedef struct {
    char path[GMB_CPLM_PATH];
    int fd;
    int nRead;
    int nWrite;
    int used;
} CplmFile;

static struct {
    pthread_mutex_t mu;
    CplmFile files[GMB_CPLM_MAX];
} g_cplm = { PTHREAD_MUTEX_INITIALIZER, {{0}} };

typedef struct {
    char key[GMB_CPLM_KEY];
    int type;
} CplmHeld;

struct CrossProcessLockManager {
    char fileId[16];
    char lockDir[GMB_CPLM_PATH];
    CplmHeld held[GMB_CPLM_MAX];
    int nHeld;
    int wfFd;
    int wfSlot;
};

#define GMB_WF_SLOTS 128
#define GMB_WF_KEY 96
#define GMB_WF_HELD 8

typedef struct {
    uint32_t used;
    int32_t pid;
    uint32_t tid;
    int32_t wantsWrite;
    int32_t nHeld;
    int32_t abort;
    int64_t seq;
    char waitKey[GMB_WF_KEY];
    char held[GMB_WF_HELD][GMB_WF_KEY];
} GmbWfSlot;

typedef struct {
    uint32_t magic;
    uint32_t nslots;
    uint64_t nextSeq;
    GmbWfSlot slots[GMB_WF_SLOTS];
} GmbWfFile;

#define GMB_WF_MAGIC 0x57464731u

static int wf_flock(int fd, int lock)
{
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = lock ? F_WRLCK : F_UNLCK;
    fl.l_whence = SEEK_SET;
    return fcntl(fd, F_SETLKW, &fl);
}

static int wf_load(int fd, GmbWfFile* f)
{
    ssize_t n;
    if (lseek(fd, 0, SEEK_SET) < 0) return -1;
    n = read(fd, f, sizeof(*f));
    if (n == 0 || (n > 0 && n < (ssize_t)sizeof(*f)) || f->magic != GMB_WF_MAGIC) {
        memset(f, 0, sizeof(*f));
        f->magic = GMB_WF_MAGIC;
        f->nslots = GMB_WF_SLOTS;
        f->nextSeq = 1;
        if (lseek(fd, 0, SEEK_SET) < 0) return -1;
        if (write(fd, f, sizeof(*f)) != (ssize_t)sizeof(*f)) return -1;
    }
    return 0;
}

static int wf_save(int fd, const GmbWfFile* f)
{
    if (lseek(fd, 0, SEEK_SET) < 0) return -1;
    if (write(fd, f, sizeof(*f)) != (ssize_t)sizeof(*f)) return -1;
    return 0;
}

static int wf_alive(int32_t pid)
{
    if (pid <= 0) return 0;
    if (kill((pid_t)pid, 0) == 0) return 1;
    return errno != ESRCH;
}

static int wf_held_has(const GmbWfSlot* s, const char* key)
{
    int i;
    for (i = 0; i < s->nHeld; i++)
        if (strcmp(s->held[i], key) == 0) return 1;
    return 0;
}

static int wf_reachable(const GmbWfFile* f, int from, int target, int* vis, int vn)
{
    int h, v;
    if (from == target) return 1;
    for (v = 0; v < vn; v++) if (vis[v] == from) return 0;
    if (vn >= GMB_WF_SLOTS) return 0;
    vis[vn++] = from;
    if (!f->slots[from].used || !f->slots[from].waitKey[0]) return 0;
    for (h = 0; h < GMB_WF_SLOTS; h++) {
        if (!f->slots[h].used || h == from) continue;
        if (!wf_held_has(&f->slots[h], f->slots[from].waitKey)) continue;
        if (wf_reachable(f, h, target, vis, vn)) return 1;
    }
    return 0;
}

static int wf_open(CrossProcessLockManager* self)
{
    char path[GMB_CPLM_PATH];
    GmbWfFile f;
    int i, slot = -1;
    snprintf(path, sizeof(path), "%s/.gmb_waitgraph_%s", self->lockDir, self->fileId);
    self->wfFd = open(path, O_RDWR | O_CREAT, 0666);
    if (self->wfFd < 0) return -1;
    wf_flock(self->wfFd, 1);
    if (wf_load(self->wfFd, &f) != 0) {
        wf_flock(self->wfFd, 0);
        close(self->wfFd);
        self->wfFd = -1;
        return -1;
    }
    for (i = 0; i < GMB_WF_SLOTS; i++) {
        if (f.slots[i].used && !wf_alive(f.slots[i].pid)) {
            memset(&f.slots[i], 0, sizeof(f.slots[i]));
        }
        if (!f.slots[i].used && slot < 0) slot = i;
    }
    if (slot < 0) {
        wf_flock(self->wfFd, 0);
        close(self->wfFd);
        self->wfFd = -1;
        return -1;
    }
    memset(&f.slots[slot], 0, sizeof(f.slots[slot]));
    f.slots[slot].used = 1;
    f.slots[slot].pid = (int32_t)getpid();
    f.slots[slot].tid = (uint32_t)(uintptr_t)pthread_self();
    self->wfSlot = slot;
    wf_save(self->wfFd, &f);
    wf_flock(self->wfFd, 0);
    return 0;
}

static void wf_close(CrossProcessLockManager* self)
{
    GmbWfFile f;
    if (!self || self->wfFd < 0) return;
    wf_flock(self->wfFd, 1);
    if (wf_load(self->wfFd, &f) == 0 && self->wfSlot >= 0 && self->wfSlot < GMB_WF_SLOTS)
        memset(&f.slots[self->wfSlot], 0, sizeof(f.slots[self->wfSlot]));
    wf_save(self->wfFd, &f);
    wf_flock(self->wfFd, 0);
    close(self->wfFd);
    self->wfFd = -1;
}

static void wf_publish_held(CrossProcessLockManager* self)
{
    GmbWfFile f;
    int i, n;
    if (!self || self->wfFd < 0 || self->wfSlot < 0) return;
    wf_flock(self->wfFd, 1);
    if (wf_load(self->wfFd, &f) != 0) {
        wf_flock(self->wfFd, 0);
        return;
    }
    n = self->nHeld < GMB_WF_HELD ? self->nHeld : GMB_WF_HELD;
    f.slots[self->wfSlot].nHeld = n;
    for (i = 0; i < n; i++)
        snprintf(f.slots[self->wfSlot].held[i], GMB_WF_KEY, "%s", self->held[i].key);
    wf_save(self->wfFd, &f);
    wf_flock(self->wfFd, 0);
}

static void wf_publish_held_unlocked(CrossProcessLockManager* self, GmbWfFile* f);

static int wf_begin_wait(CrossProcessLockManager* self, const char* key, int wantsWrite)
{
    GmbWfFile f;
    int vis[GMB_WF_SLOTS], h, cycle, victim, i;
    int64_t maxSeq;
    if (!self || self->wfFd < 0 || self->wfSlot < 0) return 0;
    wf_flock(self->wfFd, 1);
    if (wf_load(self->wfFd, &f) != 0) {
        wf_flock(self->wfFd, 0);
        return 0;
    }
    for (i = 0; i < GMB_WF_SLOTS; i++) {
        if (f.slots[i].used && !wf_alive(f.slots[i].pid))
            memset(&f.slots[i], 0, sizeof(f.slots[i]));
    }
    snprintf(f.slots[self->wfSlot].waitKey, GMB_WF_KEY, "%s", key);
    f.slots[self->wfSlot].wantsWrite = wantsWrite ? 1 : 0;
    f.slots[self->wfSlot].seq = (int64_t)f.nextSeq++;
    f.slots[self->wfSlot].abort = 0;
    wf_publish_held_unlocked(self, &f);

    cycle = 0;
    for (h = 0; h < GMB_WF_SLOTS; h++) {
        if (!f.slots[h].used || h == self->wfSlot) continue;
        if (!wf_held_has(&f.slots[h], key)) continue;
        memset(vis, 0, sizeof(vis));
        if (wf_reachable(&f, h, self->wfSlot, vis, 0)) {
            cycle = 1;
            break;
        }
    }
    if (cycle) {
        victim = self->wfSlot;
        maxSeq = f.slots[self->wfSlot].seq;
        for (i = 0; i < GMB_WF_SLOTS; i++) {
            if (!f.slots[i].used) continue;
            memset(vis, 0, sizeof(vis));
            if (i == self->wfSlot || wf_reachable(&f, i, self->wfSlot, vis, 0)) {
                if (f.slots[i].seq >= maxSeq) {
                    maxSeq = f.slots[i].seq;
                    victim = i;
                }
            }
        }
        if (victim != self->wfSlot)
            f.slots[victim].abort = 1;
        wf_save(self->wfFd, &f);
        wf_flock(self->wfFd, 0);
        return victim == self->wfSlot ? 1 : 0;
    }
    if (f.slots[self->wfSlot].abort) {
        wf_save(self->wfFd, &f);
        wf_flock(self->wfFd, 0);
        return 1;
    }
    wf_save(self->wfFd, &f);
    wf_flock(self->wfFd, 0);
    return 0;
}

static void wf_publish_held_unlocked(CrossProcessLockManager* self, GmbWfFile* f);

static void wf_end_wait(CrossProcessLockManager* self)
{
    GmbWfFile f;
    if (!self || self->wfFd < 0 || self->wfSlot < 0) return;
    wf_flock(self->wfFd, 1);
    if (wf_load(self->wfFd, &f) == 0) {
        f.slots[self->wfSlot].waitKey[0] = 0;
        f.slots[self->wfSlot].abort = 0;
        wf_save(self->wfFd, &f);
    }
    wf_flock(self->wfFd, 0);
}

static void wf_publish_held_unlocked(CrossProcessLockManager* self, GmbWfFile* f)
{
    int i, n;
    n = self->nHeld < GMB_WF_HELD ? self->nHeld : GMB_WF_HELD;
    f->slots[self->wfSlot].nHeld = n;
    for (i = 0; i < n; i++)
        snprintf(f->slots[self->wfSlot].held[i], GMB_WF_KEY, "%s", self->held[i].key);
}

static int wf_poll_abort(CrossProcessLockManager* self)
{
    GmbWfFile f;
    int abort = 0;
    if (!self || self->wfFd < 0 || self->wfSlot < 0) return 0;
    wf_flock(self->wfFd, 1);
    if (wf_load(self->wfFd, &f) == 0)
        abort = f.slots[self->wfSlot].abort != 0;
    wf_flock(self->wfFd, 0);
    return abort;
}

static int cplm_thread_id(void) { return (int)(intptr_t)pthread_self(); }

static void cplm_file_id(const char* filePath, char* out, int cap)
{
    unsigned h = 2166136261u;
    const char* p;
    for (p = filePath; p && *p; p++) {
        h ^= (unsigned)tolower((unsigned char)*p);
        h *= 16777619u;
    }
    snprintf(out, (size_t)cap, "%08x", h);
}

static void cplm_dirname(const char* path, char* out, int cap)
{
    const char* slash;
    size_t n;
    if (!path || !out || cap < 2) return;
    slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out, (size_t)cap, ".");
        return;
    }
    n = (size_t)(slash - path);
    if (n == 0) n = 1;
    if (n >= (size_t)cap) n = (size_t)cap - 1;
    memcpy(out, path, n);
    out[n] = 0;
}

static void cplm_lock_path(const CrossProcessLockManager* self, const char* resourceKey, char* out, int cap)
{
    char safe[128];
    int i;
    for (i = 0; resourceKey && resourceKey[i] && i < 120; i++) {
        unsigned char c = (unsigned char)resourceKey[i];
        safe[i] = (isalnum(c) || c == '.' || c == '_' || c == '-') ? (char)c : '_';
    }
    safe[i] = 0;
    if (!safe[0]) snprintf(safe, sizeof(safe), "k");
    snprintf(out, (size_t)cap, "%s/.gmb_xlock_%s_%s.lock", self->lockDir, self->fileId, safe);
}

static int cplm_fcntl(int fd, short type)
{
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = type;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLK, &fl) == 0) return 1;
    if (errno == EAGAIN || errno == EACCES) return 0;
    return -1;
}

static CplmFile* cplm_file_locked(const char* path)
{
    int i, freeSlot = -1;
    for (i = 0; i < GMB_CPLM_MAX; i++) {
        if (g_cplm.files[i].used && strcmp(g_cplm.files[i].path, path) == 0)
            return &g_cplm.files[i];
        if (!g_cplm.files[i].used && freeSlot < 0) freeSlot = i;
    }
    if (freeSlot < 0) return NULL;
    g_cplm.files[freeSlot].fd = open(path, O_RDWR | O_CREAT, 0666);
    if (g_cplm.files[freeSlot].fd < 0) return NULL;
    snprintf(g_cplm.files[freeSlot].path, GMB_CPLM_PATH, "%s", path);
    g_cplm.files[freeSlot].nRead = 0;
    g_cplm.files[freeSlot].nWrite = 0;
    g_cplm.files[freeSlot].used = 1;
    return &g_cplm.files[freeSlot];
}

/* g_cplm.mu held */
static int cplm_try_read_locked(const char* path)
{
    CplmFile* f = cplm_file_locked(path);
    int rc;
    if (!f) return -1;
    if (f->nWrite > 0) return 0;
    if (f->nRead > 0) {
        f->nRead++;
        return 1;
    }
    rc = cplm_fcntl(f->fd, F_RDLCK);
    if (rc == 1) f->nRead = 1;
    return rc;
}

static int cplm_try_write_locked(const char* path)
{
    CplmFile* f = cplm_file_locked(path);
    int rc;
    if (!f) return -1;
    if (f->nRead > 0 || f->nWrite > 0) return 0;
    rc = cplm_fcntl(f->fd, F_WRLCK);
    if (rc == 1) f->nWrite = 1;
    return rc;
}

static void cplm_release_read_locked(const char* path)
{
    CplmFile* f = cplm_file_locked(path);
    if (!f || f->nRead < 1) return;
    f->nRead--;
    if (f->nRead == 0 && f->nWrite == 0)
        (void)cplm_fcntl(f->fd, F_UNLCK);
}

static void cplm_release_write_locked(const char* path)
{
    CplmFile* f = cplm_file_locked(path);
    if (!f || f->nWrite < 1) return;
    f->nWrite = 0;
    (void)cplm_fcntl(f->fd, F_UNLCK);
}

static int cplm_held_find(const CrossProcessLockManager* self, const char* key)
{
    int i;
    for (i = 0; i < self->nHeld; i++) {
        if (strcmp(self->held[i].key, key) == 0) return i;
    }
    return -1;
}

CrossProcessLockManager* CrossProcessLockManager_New(const char* filePath)
{
    CrossProcessLockManager* self;
    if (!filePath || !filePath[0]) return NULL;
    self = (CrossProcessLockManager*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    cplm_dirname(filePath, self->lockDir, (int)sizeof(self->lockDir));
    cplm_file_id(filePath, self->fileId, (int)sizeof(self->fileId));
    self->wfFd = -1;
    self->wfSlot = -1;
    if (wf_open(self) != 0) {
        free(self);
        return NULL;
    }
    return self;
}

static int cplm_acquire(CrossProcessLockManager* self, const char* resourceKey, int timeoutMilliseconds,
                        const char* resourceDescription, int wantsWrite)
{
    char path[GMB_CPLM_PATH];
    int threadId;
    struct timespec start, now;
    int64_t deadlineMs;
    const char* desc = resourceDescription ? resourceDescription : "";
    if (!self || !resourceKey || !resourceKey[0]) return GMB_DB_ARG;
    if (cplm_held_find(self, resourceKey) >= 0) return GMB_DB_ERR;
    if (self->nHeld >= GMB_CPLM_MAX) return GMB_DB_ERR;
    cplm_lock_path(self, resourceKey, path, (int)sizeof(path));
    threadId = cplm_thread_id();
    clock_gettime(CLOCK_MONOTONIC, &start);
    deadlineMs = timeoutMilliseconds < 0 ? INT64_MAX : (int64_t)timeoutMilliseconds;
    DeadlockMonitor_SetWaiting(threadId, resourceKey, wantsWrite);
    if (wf_begin_wait(self, resourceKey, wantsWrite)) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Deadlock cross-process rilevato; transazione annullata (vittima) su %s.", desc);
        IsamSlowOpLog_LogDeadlock(msg);
        wf_end_wait(self);
        DeadlockMonitor_ClearWaiting(threadId);
        return GMB_DB_DEADLOCK;
    }
    for (;;) {
        int got;
        pthread_mutex_lock(&g_cplm.mu);
        got = wantsWrite ? cplm_try_write_locked(path) : cplm_try_read_locked(path);
        pthread_mutex_unlock(&g_cplm.mu);
        if (got == 1) {
            snprintf(self->held[self->nHeld].key, GMB_CPLM_KEY, "%s", resourceKey);
            self->held[self->nHeld].type = wantsWrite ? GMB_CPLM_WRITE : GMB_CPLM_READ;
            self->nHeld++;
            if (wantsWrite) DeadlockMonitor_RecordWriteAcquired(threadId, resourceKey);
            else DeadlockMonitor_RecordReadAcquired(threadId, resourceKey);
            wf_end_wait(self);
            wf_publish_held(self);
            DeadlockMonitor_ClearWaiting(threadId);
            return GMB_DB_OK;
        }
        if (got < 0) {
            wf_end_wait(self);
            DeadlockMonitor_ClearWaiting(threadId);
            return GMB_DB_ERR;
        }
        if (DeadlockMonitor_IsAbortRequested(threadId) || wf_poll_abort(self)) {
            char msg[256];
            DeadlockMonitor_ClearAbort(threadId);
            snprintf(msg, sizeof(msg), "Transazione interrotta dal rilevatore di deadlock su %s.", desc);
            IsamSlowOpLog_LogDeadlock(msg);
            wf_end_wait(self);
            DeadlockMonitor_ClearWaiting(threadId);
            return GMB_DB_DEADLOCK;
        }
        {
            int victim = 0;
            if (DeadlockMonitor_TryResolveDeadlock(threadId, resourceKey, wantsWrite, &victim) && victim == threadId) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Deadlock rilevato; transazione annullata (vittima designata) su %s.", desc);
                IsamSlowOpLog_LogDeadlock(msg);
                wf_end_wait(self);
                DeadlockMonitor_ClearWaiting(threadId);
                return GMB_DB_DEADLOCK;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (((now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000) >= deadlineMs) {
            wf_end_wait(self);
            DeadlockMonitor_ClearWaiting(threadId);
            return GMB_DB_TIMEOUT;
        }
        usleep((useconds_t)DeadlockMonitor_PollIntervalMs * 1000);
    }
}

int CrossProcessLockManager_AcquireReadLock(CrossProcessLockManager* self, const char* resourceKey,
                                            int timeoutMilliseconds, const char* resourceDescription)
{
    return cplm_acquire(self, resourceKey, timeoutMilliseconds, resourceDescription, 0);
}

int CrossProcessLockManager_AcquireWriteLock(CrossProcessLockManager* self, const char* resourceKey,
                                             int timeoutMilliseconds, const char* resourceDescription)
{
    return cplm_acquire(self, resourceKey, timeoutMilliseconds, resourceDescription, 1);
}

void CrossProcessLockManager_ReleaseReadLock(CrossProcessLockManager* self, const char* resourceKey)
{
    char path[GMB_CPLM_PATH];
    int i;
    if (!self || !resourceKey) return;
    i = cplm_held_find(self, resourceKey);
    if (i < 0 || self->held[i].type != GMB_CPLM_READ) return;
    cplm_lock_path(self, resourceKey, path, (int)sizeof(path));
    pthread_mutex_lock(&g_cplm.mu);
    cplm_release_read_locked(path);
    pthread_mutex_unlock(&g_cplm.mu);
    self->held[i] = self->held[self->nHeld - 1];
    self->nHeld--;
    wf_publish_held(self);
    DeadlockMonitor_RecordReadReleased(cplm_thread_id(), resourceKey);
}

void CrossProcessLockManager_ReleaseWriteLock(CrossProcessLockManager* self, const char* resourceKey)
{
    char path[GMB_CPLM_PATH];
    int i;
    if (!self || !resourceKey) return;
    i = cplm_held_find(self, resourceKey);
    if (i < 0 || self->held[i].type != GMB_CPLM_WRITE) return;
    cplm_lock_path(self, resourceKey, path, (int)sizeof(path));
    pthread_mutex_lock(&g_cplm.mu);
    cplm_release_write_locked(path);
    pthread_mutex_unlock(&g_cplm.mu);
    self->held[i] = self->held[self->nHeld - 1];
    self->nHeld--;
    wf_publish_held(self);
    DeadlockMonitor_RecordWriteReleased(cplm_thread_id(), resourceKey);
}

void CrossProcessLockManager_Dispose(CrossProcessLockManager* self)
{
    int i;
    if (!self) return;
    for (i = self->nHeld - 1; i >= 0; i--) {
        if (self->held[i].type == GMB_CPLM_READ)
            CrossProcessLockManager_ReleaseReadLock(self, self->held[i].key);
        else
            CrossProcessLockManager_ReleaseWriteLock(self, self->held[i].key);
    }
    wf_close(self);
    free(self);
}

static void cplm_selfcheck_rm_locks(const char* dir)
{
    DIR* d;
    struct dirent* ent;
    if (!dir) return;
    d = opendir(dir);
    if (!d) return;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, ".gmb_xlock_", 11) == 0
            || strncmp(ent->d_name, ".gmb_waitgraph_", 15) == 0) {
            char p[GMB_CPLM_PATH];
            snprintf(p, sizeof(p), "%s/%s", dir, ent->d_name);
            unlink(p);
        }
    }
    closedir(d);
}

static int cplm_selfcheck_fork_deadlock(void)
{
    char dir[] = "native/gmb_db_c/build/cplmfork_XXXXXX";
    char db[PATH_MAX];
    int tochild[2], toparent[2];
    pid_t pid;
    int st, parent_rc, child_rc;
    char sig;
    CrossProcessLockManager* p;

    if (!mkdtemp(dir)) return -20;
    snprintf(db, sizeof(db), "%s/book.dbf", dir);
    if (pipe(tochild) != 0 || pipe(toparent) != 0) {
        rmdir(dir);
        return -21;
    }
    pid = fork();
    if (pid < 0) {
        close(tochild[0]); close(tochild[1]);
        close(toparent[0]); close(toparent[1]);
        rmdir(dir);
        return -22;
    }
    if (pid == 0) {
        CrossProcessLockManager* m;
        int rc;
        close(tochild[1]);
        close(toparent[0]);
        m = CrossProcessLockManager_New(db);
        if (!m) _exit(40);
        if (CrossProcessLockManager_AcquireWriteLock(m, "B", 2000, "B") != GMB_DB_OK)
            _exit(41);
        if (write(toparent[1], "b", 1) != 1) _exit(42);
        if (read(tochild[0], &sig, 1) != 1) _exit(43);
        rc = CrossProcessLockManager_AcquireWriteLock(m, "A", 5000, "A");
        CrossProcessLockManager_Dispose(m);
        close(tochild[0]);
        close(toparent[1]);
        if (rc == GMB_DB_DEADLOCK) _exit(0);
        if (rc == GMB_DB_OK) _exit(1);
        _exit(2);
    }
    close(tochild[0]);
    close(toparent[1]);
    p = CrossProcessLockManager_New(db);
    if (!p) {
        kill(pid, SIGTERM);
        waitpid(pid, &st, 0);
        close(tochild[1]); close(toparent[0]);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -23;
    }
    if (CrossProcessLockManager_AcquireWriteLock(p, "A", 2000, "A") != GMB_DB_OK) {
        CrossProcessLockManager_Dispose(p);
        kill(pid, SIGTERM);
        waitpid(pid, &st, 0);
        close(tochild[1]); close(toparent[0]);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -24;
    }
    if (write(tochild[1], "a", 1) != 1
        || read(toparent[0], &sig, 1) != 1) {
        CrossProcessLockManager_Dispose(p);
        kill(pid, SIGTERM);
        waitpid(pid, &st, 0);
        close(tochild[1]); close(toparent[0]);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -25;
    }
    parent_rc = CrossProcessLockManager_AcquireWriteLock(p, "B", 5000, "B");
    CrossProcessLockManager_Dispose(p);
    close(tochild[1]);
    close(toparent[0]);
    if (waitpid(pid, &st, 0) < 0) {
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -26;
    }
    child_rc = WIFEXITED(st) ? WEXITSTATUS(st) : 99;
    cplm_selfcheck_rm_locks(dir);
    rmdir(dir);
    if (parent_rc != GMB_DB_DEADLOCK && parent_rc != GMB_DB_OK) return -27;
    if (child_rc > 1) return -28;
    if (parent_rc != GMB_DB_DEADLOCK && child_rc != 0) return -29;
    return GMB_DB_OK;
}

int CrossProcessLockManager_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/cplmsc_XXXXXX";
    char db[PATH_MAX];
    CrossProcessLockManager *a, *b, *c;

    if (!mkdtemp(dir)) return -2;
    snprintf(db, sizeof(db), "%s/book.dbf", dir);

    a = CrossProcessLockManager_New(db);
    b = CrossProcessLockManager_New(db);
    c = CrossProcessLockManager_New(db);
    if (!a || !b || !c) {
        CrossProcessLockManager_Dispose(a);
        CrossProcessLockManager_Dispose(b);
        CrossProcessLockManager_Dispose(c);
        rmdir(dir);
        return -3;
    }

    if (CrossProcessLockManager_AcquireWriteLock(a, "rec1", 200, "w-a") != GMB_DB_OK) {
        CrossProcessLockManager_Dispose(a);
        CrossProcessLockManager_Dispose(b);
        CrossProcessLockManager_Dispose(c);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -4;
    }
    if (CrossProcessLockManager_AcquireWriteLock(b, "rec1", 80, "w-b") != GMB_DB_TIMEOUT) {
        CrossProcessLockManager_Dispose(a);
        CrossProcessLockManager_Dispose(b);
        CrossProcessLockManager_Dispose(c);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -5;
    }
    CrossProcessLockManager_ReleaseWriteLock(a, "rec1");
    if (CrossProcessLockManager_AcquireWriteLock(b, "rec1", 200, "w-b") != GMB_DB_OK) {
        CrossProcessLockManager_Dispose(a);
        CrossProcessLockManager_Dispose(b);
        CrossProcessLockManager_Dispose(c);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -6;
    }
    CrossProcessLockManager_ReleaseWriteLock(b, "rec1");

    if (CrossProcessLockManager_AcquireReadLock(a, "rec1", 200, "r-a") != GMB_DB_OK
        || CrossProcessLockManager_AcquireReadLock(b, "rec1", 200, "r-b") != GMB_DB_OK) {
        CrossProcessLockManager_Dispose(a);
        CrossProcessLockManager_Dispose(b);
        CrossProcessLockManager_Dispose(c);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -7;
    }
    if (CrossProcessLockManager_AcquireWriteLock(c, "rec1", 80, "w-c") != GMB_DB_TIMEOUT) {
        CrossProcessLockManager_Dispose(a);
        CrossProcessLockManager_Dispose(b);
        CrossProcessLockManager_Dispose(c);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -8;
    }
    CrossProcessLockManager_ReleaseReadLock(a, "rec1");
    CrossProcessLockManager_ReleaseReadLock(b, "rec1");
    if (CrossProcessLockManager_AcquireWriteLock(c, "rec1", 200, "w-c") != GMB_DB_OK) {
        CrossProcessLockManager_Dispose(a);
        CrossProcessLockManager_Dispose(b);
        CrossProcessLockManager_Dispose(c);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -9;
    }
    CrossProcessLockManager_ReleaseWriteLock(c, "rec1");

    if (CrossProcessLockManager_AcquireReadLock(a, NULL, 10, "x") != GMB_DB_ARG) {
        CrossProcessLockManager_Dispose(a);
        CrossProcessLockManager_Dispose(b);
        CrossProcessLockManager_Dispose(c);
        cplm_selfcheck_rm_locks(dir);
        rmdir(dir);
        return -10;
    }

    CrossProcessLockManager_Dispose(a);
    CrossProcessLockManager_Dispose(b);
    CrossProcessLockManager_Dispose(c);
    cplm_selfcheck_rm_locks(dir);
    rmdir(dir);
    {
        int fr = cplm_selfcheck_fork_deadlock();
        if (fr != GMB_DB_OK) return fr;
    }
    return GMB_DB_OK;
}
