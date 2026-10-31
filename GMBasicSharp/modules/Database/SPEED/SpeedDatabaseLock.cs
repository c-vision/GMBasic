// FILE: Basic.Databases.Speed.SpeedDatabaseLock
// ReSharper disable CheckNamespace
// ReSharper disable InconsistentNaming

using System.Collections.Concurrent;
using Basic.Databases;

namespace Basic.Databases.Speed;

/// <summary>
/// Cross-process exclusive lock for a SPEED database directory.
/// ponytail: exclusive <c>.speed.lock</c> file (FileShare.None) — not CrossProcessLockManager because
/// OS Mutex has thread-affinity and <see cref="Database.Dispose"/> may run on another thread after async tests.
/// Ref-count per normalized path allows multiple <see cref="Database"/> handles in one process.
/// </summary>
internal static class SpeedDatabaseLock
{
    /// <summary>Lock file name inside the database directory.</summary>
    internal const string LockFileName = ".speed.lock";

    private sealed class Slot
    {
        public FileStream? LockStream;
        public int RefCount;
        public Lock Gate = new();
    }

    private static readonly ConcurrentDictionary<string, Slot> Slots = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Acquires the cross-process exclusive lock for <paramref name="databasePath"/>.
    /// </summary>
    /// <param name="databasePath">SPEED database root directory.</param>
    /// <exception cref="LockTimeoutException">Another process already holds the lock.</exception>
    public static void Acquire(string databasePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(databasePath);
        var key = NormalizePath(databasePath);
        var lockFile = Path.Combine(key, LockFileName);
        var slot = Slots.GetOrAdd(key, _ => new Slot());
        lock (slot.Gate)
        {
            if (slot.RefCount == 0)
            {
                try
                {
                    slot.LockStream = new FileStream(
                        lockFile,
                        FileMode.OpenOrCreate,
                        FileAccess.ReadWrite,
                        FileShare.None);
                }
                catch (IOException ex)
                {
                    throw new LockTimeoutException(
                        $"Database SPEED già aperto da un altro processo: {key} ({ex.Message})");
                }
            }

            slot.RefCount++;
        }
    }

    /// <summary>
    /// Releases one reference to the cross-process lock for <paramref name="databasePath"/>.
    /// </summary>
    /// <param name="databasePath">SPEED database root directory.</param>
    public static void Release(string databasePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(databasePath);
        var key = NormalizePath(databasePath);
        if (!Slots.TryGetValue(key, out var slot))
        {
            return;
        }

        lock (slot.Gate)
        {
            if (slot.RefCount <= 0)
            {
                return;
            }

            slot.RefCount--;
            if (slot.RefCount > 0)
            {
                return;
            }

            slot.LockStream?.Dispose();
            slot.LockStream = null;
            Slots.TryRemove(key, out _);

            var lockFile = Path.Combine(key, LockFileName);
            TryDeleteLockFile(lockFile);
        }
    }

    internal static string NormalizePath(string databasePath) =>
        Path.GetFullPath(databasePath).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);

    private static void TryDeleteLockFile(string lockFile)
    {
        try
        {
            if (File.Exists(lockFile))
            {
                File.Delete(lockFile);
            }
        }
        catch
        {
            // ponytail: stale lock file harmless; OS releases flock on stream dispose
        }
    }
}
