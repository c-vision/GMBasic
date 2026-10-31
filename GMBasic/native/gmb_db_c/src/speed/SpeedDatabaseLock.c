/* FILE: native/gmb_db_c/src/SPEED/SpeedDatabaseLock.c */
/* Port 1:1 di modules/Database/SPEED/SpeedDatabaseLock.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.SpeedDatabaseLock
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//
//using System.Collections.Concurrent;
//using Basic.Databases;
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Cross-process exclusive lock for a SPEED database directory.
///// ponytail: exclusive <c>.speed.lock</c> file (FileShare.None) — not CrossProcessLockManager because
///// OS Mutex has thread-affinity and <see cref="Database.Dispose"/> may run on another thread after async tests.
///// Ref-count per normalized path allows multiple <see cref="Database"/> handles in one process.
///// </summary>
//internal static class SpeedDatabaseLock
//{
//    /// <summary>Lock file name inside the database directory.</summary>
//    internal const string LockFileName = ".speed.lock";
//
//    private sealed class Slot
//    {
//        public FileStream? LockStream;
//        public int RefCount;
//        public Lock Gate = new();
//    }
//
//    private static readonly ConcurrentDictionary<string, Slot> Slots = new(StringComparer.OrdinalIgnoreCase);
//
//    /// <summary>
//    /// Acquires the cross-process exclusive lock for <paramref name="databasePath"/>.
//    /// </summary>
//    /// <param name="databasePath">SPEED database root directory.</param>
//    /// <exception cref="LockTimeoutException">Another process already holds the lock.</exception>
//    public static void Acquire(string databasePath)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(databasePath);
//        var key = NormalizePath(databasePath);
//        var lockFile = Path.Combine(key, LockFileName);
//        var slot = Slots.GetOrAdd(key, _ => new Slot());
//        lock (slot.Gate)
//        {
//            if (slot.RefCount == 0)
//            {
//                try
//                {
//                    slot.LockStream = new FileStream(
//                        lockFile,
//                        FileMode.OpenOrCreate,
//                        FileAccess.ReadWrite,
//                        FileShare.None);
//                }
//                catch (IOException ex)
//                {
//                    throw new LockTimeoutException(
//                        $"Database SPEED già aperto da un altro processo: {key} ({ex.Message})");
//                }
//            }
//
//            slot.RefCount++;
//        }
//    }
//
//    /// <summary>
//    /// Releases one reference to the cross-process lock for <paramref name="databasePath"/>.
//    /// </summary>
//    /// <param name="databasePath">SPEED database root directory.</param>
//    public static void Release(string databasePath)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(databasePath);
//        var key = NormalizePath(databasePath);
//        if (!Slots.TryGetValue(key, out var slot))
//        {
//            return;
//        }
//
//        lock (slot.Gate)
//        {
//            if (slot.RefCount <= 0)
//            {
//                return;
//            }
//
//            slot.RefCount--;
//            if (slot.RefCount > 0)
//            {
//                return;
//            }
//
//            slot.LockStream?.Dispose();
//            slot.LockStream = null;
//            Slots.TryRemove(key, out _);
//
//            var lockFile = Path.Combine(key, LockFileName);
//            TryDeleteLockFile(lockFile);
//        }
//    }
//
//    internal static string NormalizePath(string databasePath) =>
//        Path.GetFullPath(databasePath).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
//
//    private static void TryDeleteLockFile(string lockFile)
//    {
//        try
//        {
//            if (File.Exists(lockFile))
//            {
//                File.Delete(lockFile);
//            }
//        }
//        catch
//        {
//            // ponytail: stale lock file harmless; OS releases flock on stream dispose
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#define SPEED_LOCK_MAX 16

typedef struct {
    char key[1024];
    int fd;
    int refCount;
} SpeedLockSlot;

static pthread_mutex_t g_speedLockMu = PTHREAD_MUTEX_INITIALIZER;
static SpeedLockSlot g_speedLocks[SPEED_LOCK_MAX];

static void speed_lock_normalize(const char* path, char* out, size_t cap)
{
    char tmp[PATH_MAX];
    size_t n;
    if (realpath(path, tmp))
        snprintf(out, cap, "%s", tmp);
    else
        snprintf(out, cap, "%s", path);
    n = strlen(out);
    while (n > 1 && (out[n - 1] == '/' || out[n - 1] == '\\'))
        out[--n] = 0;
}

static void speed_lock_try_delete(const char* lockFile)
{
    unlink(lockFile);
}

int SpeedDatabaseLock_Acquire(const char* databasePath)
{
    char key[1024];
    char lockFile[1100];
    int i, empty = -1, fd;
    if (!databasePath || !databasePath[0]) return GMB_DB_ARG;
    speed_lock_normalize(databasePath, key, sizeof(key));
    snprintf(lockFile, sizeof(lockFile), "%s/%s", key, SpeedDatabaseLock_LockFileName);
    pthread_mutex_lock(&g_speedLockMu);
    for (i = 0; i < SPEED_LOCK_MAX; i++) {
        if (g_speedLocks[i].refCount > 0 && strcmp(g_speedLocks[i].key, key) == 0) {
            g_speedLocks[i].refCount++;
            pthread_mutex_unlock(&g_speedLockMu);
            return GMB_DB_OK;
        }
        if (empty < 0 && g_speedLocks[i].refCount == 0)
            empty = i;
    }
    if (empty < 0) {
        pthread_mutex_unlock(&g_speedLockMu);
        return GMB_DB_ERR;
    }
    fd = open(lockFile, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        pthread_mutex_unlock(&g_speedLockMu);
        return GMB_DB_ERR;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        pthread_mutex_unlock(&g_speedLockMu);
        return GMB_DB_TIMEOUT;
    }
    snprintf(g_speedLocks[empty].key, sizeof(g_speedLocks[0].key), "%s", key);
    g_speedLocks[empty].fd = fd;
    g_speedLocks[empty].refCount = 1;
    pthread_mutex_unlock(&g_speedLockMu);
    return GMB_DB_OK;
}

void SpeedDatabaseLock_Release(const char* databasePath)
{
    char key[1024];
    int i;
    if (!databasePath || !databasePath[0]) return;
    speed_lock_normalize(databasePath, key, sizeof(key));
    pthread_mutex_lock(&g_speedLockMu);
    for (i = 0; i < SPEED_LOCK_MAX; i++) {
        if (g_speedLocks[i].refCount <= 0 || strcmp(g_speedLocks[i].key, key) != 0)
            continue;
        g_speedLocks[i].refCount--;
        if (g_speedLocks[i].refCount > 0)
            break;
        flock(g_speedLocks[i].fd, LOCK_UN);
        close(g_speedLocks[i].fd);
        g_speedLocks[i].fd = -1;
        {
            char lockFile[1100];
            snprintf(lockFile, sizeof(lockFile), "%s/%s", key, SpeedDatabaseLock_LockFileName);
            speed_lock_try_delete(lockFile);
        }
        g_speedLocks[i].key[0] = 0;
        break;
    }
    pthread_mutex_unlock(&g_speedLockMu);
}

/* Slot è una nested class C#; Acquire/Release reali sono SpeedDatabaseLock_Acquire/Release. */

int SpeedDatabaseLock_SelfCheck(void)
{
    char dir[] = "/tmp/gmb_speed_lock_XXXXXX";
    char lockPath[1100], stalePath[1100];
    Database *a, *b;
    Manifest* man;
    const char* gcName = "dead.sst";
    FILE* f;
    int probe, n, deleted;
    char names[8][256];

    if (!mkdtemp(dir)) return -120;
    a = Database_Open(dir);
    if (!a) return -121;
    snprintf(lockPath, sizeof(lockPath), "%s/%s", dir, SpeedDatabaseLock_LockFileName);
    if (access(lockPath, F_OK) != 0) { Database_Dispose(a); return -122; }
    probe = open(lockPath, O_RDWR);
    if (probe < 0) { Database_Dispose(a); return -123; }
    if (flock(probe, LOCK_EX | LOCK_NB) == 0) {
        flock(probe, LOCK_UN);
        close(probe);
        Database_Dispose(a);
        return -124;
    }
    close(probe);
    b = Database_Open(dir);
    if (!b) { Database_Dispose(a); return -125; }
    Database_Dispose(a);
    if (access(lockPath, F_OK) != 0) { Database_Dispose(b); return -126; }
    Database_Dispose(b);
    if (access(lockPath, F_OK) == 0) return -127;

    a = Database_Open(dir);
    if (!a) return -128;
    man = Database_GetManifest(a);
    snprintf(stalePath, sizeof(stalePath), "%s/%s/%s", dir, Config_SstDirectoryName, gcName);
    f = fopen(stalePath, "wb");
    if (!f) { Database_Dispose(a); return -129; }
    fwrite("xx", 1, 2, f);
    fclose(f);
    if (Manifest_AddFilesToGarbageCollection(man, &gcName, 1) != GMB_DB_OK) {
        Database_Dispose(a); return -130;
    }
    if (Manifest_Save(man) != GMB_DB_OK) { Database_Dispose(a); return -131; }
    n = Manifest_GetFilesForGarbageCollection(man, names, 8);
    if (n != 1 || strcmp(names[0], gcName) != 0) { Database_Dispose(a); return -132; }
    {
        Manifest* reload = Manifest_Load(dir);
        if (!reload) { Database_Dispose(a); return -133; }
        n = Manifest_GetFilesForGarbageCollection(reload, names, 8);
        Manifest_Dispose(reload);
        if (n != 1 || strcmp(names[0], gcName) != 0) { Database_Dispose(a); return -134; }
    }
    if (access(stalePath, F_OK) != 0) { Database_Dispose(a); return -135; }
    deleted = Database_CollectGarbage(a);
    if (deleted != 1) { Database_Dispose(a); return -136; }
    if (access(stalePath, F_OK) == 0) { Database_Dispose(a); return -137; }
    n = Manifest_GetFilesForGarbageCollection(man, names, 8);
    if (n != 0) { Database_Dispose(a); return -138; }
    Database_Dispose(a);
    return GMB_DB_OK;
}
