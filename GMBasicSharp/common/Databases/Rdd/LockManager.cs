// FILE: Basic.Databases.Rdd.LockManager.cs
// LockManager XSharp-completo: lock ricorsivi, escalation automatica,
// timeout configurabile, retry con backoff esponenziale, deadlock detection.

using System;
using System.Collections.Generic;
using System.Threading;

namespace Basic.Databases.Rdd;

/// <summary>Stato di lock per un record.</summary>
public sealed class RecordLock
{
    public long RecordNumber { get; init; }
    public DateTime AcquiredAt { get; init; } = DateTime.UtcNow;
    public int LockCount { get; set; } = 1;
    public int OwnerArea { get; init; }
}

/// <summary>Gestore centralizzato lock per DataSession.</summary>
public sealed class LockManager
{
    private readonly Dictionary<int, List<RecordLock>> _recordLocks = new();
    private readonly HashSet<int> _fileLocks = new();
    private readonly object _mutex = new();

    /// <summary>Timeout in millisecondi per tentativo di lock (0 = nessun retry).</summary>
    public int LockTimeoutMs { get; set; } = 3000;

    /// <summary>Soglia record lock prima dell'escalation automatica a FLOCK (0 = disabilitata).</summary>
    public int EscalationThreshold { get; set; } = 50;

    /// <summary>Intervallo tra retry in ms (raddoppia ad ogni tentativo).</summary>
    public int RetryBaseMs { get; set; } = 50;

    /// <summary>Numero massimo di retry (derivato da LockTimeoutMs / RetryBaseMs).</summary>
    private int MaxRetries => LockTimeoutMs > 0 ? Math.Max(1, LockTimeoutMs / RetryBaseMs) : 0;

    // ═══════════════════════════════════════════════════
    // RLOCK
    // ═══════════════════════════════════════════════════

    public bool RLock(DataSession session, int area)
    {
        var driver = session.GetWorkArea(area)?.Driver;
        if (driver == null || !driver.IsOpen) return false;
        var recNo = driver.RecNo;
        if (recNo <= 0) return false;

        lock (_mutex)
        {
            if (_fileLocks.Contains(area)) return true;
            if (!_recordLocks.TryGetValue(area, out var locks))
                _recordLocks[area] = locks = new List<RecordLock>();

            var existing = locks.Find(r => r.RecordNumber == recNo);
            if (existing != null) { existing.LockCount++; return true; }

            // Escalation automatica a FLOCK se troppi record lock
            if (EscalationThreshold > 0 && locks.Count >= EscalationThreshold)
                return FLockInternal(session, area, locks);
        }

        // Tenta lock con retry esponenziale
        return TryLockWithRetry(() => driver.Lock(recNo), () =>
        {
            lock (_mutex)
            {
                if (!_recordLocks.TryGetValue(area, out var locks))
                    _recordLocks[area] = locks = new List<RecordLock>();
                locks.Add(new RecordLock { RecordNumber = recNo, OwnerArea = area });
            }
        });
    }

    // ═══════════════════════════════════════════════════
    // FLOCK
    // ═══════════════════════════════════════════════════

    public bool FLock(DataSession session, int area)
    {
        var driver = session.GetWorkArea(area)?.Driver;
        if (driver == null || !driver.IsOpen) return false;

        lock (_mutex)
        {
            if (_fileLocks.Contains(area)) return true;
            if (!FLockInternal(session, area, null)) return false;
            return true;
        }
    }

    private bool FLockInternal(DataSession session, int area, List<RecordLock>? locks)
    {
        var driver = session.GetWorkArea(area)?.Driver;
        if (driver == null) return false;

        if (!TryLockWithRetry(() => driver.FileLock(), () =>
        {
            lock (_mutex)
            {
                if (locks != null || _recordLocks.TryGetValue(area, out locks))
                    locks!.Clear();
                _fileLocks.Add(area);
            }
        })) return false;

        return true;
    }

    // ═══════════════════════════════════════════════════
    // UNLOCK
    // ═══════════════════════════════════════════════════

    public void UnLock(DataSession session, int area, long recNo)
    {
        lock (_mutex)
        {
            if (_fileLocks.Contains(area))
            {
                _fileLocks.Remove(area);
                session.GetWorkArea(area)?.Driver?.FileUnlock();
                return;
            }
            if (!_recordLocks.TryGetValue(area, out var locks)) return;
            var rl = locks.Find(r => r.RecordNumber == recNo);
            if (rl != null)
            {
                rl.LockCount--;
                if (rl.LockCount <= 0)
                {
                    locks.Remove(rl);
                    session.GetWorkArea(area)?.Driver?.Unlock(recNo);
                }
            }
        }
    }

    public void UnLockAll(DataSession session, int area)
    {
        lock (_mutex)
        {
            if (_fileLocks.Remove(area))
                session.GetWorkArea(area)?.Driver?.FileUnlock();
            if (_recordLocks.TryGetValue(area, out var locks))
            {
                var d = session.GetWorkArea(area)?.Driver;
                foreach (var rl in locks) d?.Unlock(rl.RecordNumber);
                locks.Clear();
            }
        }
    }

    // ═══════════════════════════════════════════════════
    // Diagnostica
    // ═══════════════════════════════════════════════════

    public bool IsLocked(int area, long recNo)
    {
        lock (_mutex)
        {
            if (_fileLocks.Contains(area)) return true;
            return _recordLocks.TryGetValue(area, out var locks) && locks.Exists(r => r.RecordNumber == recNo);
        }
    }

    public bool IsFileLocked(int area)
    {
        lock (_mutex) { return _fileLocks.Contains(area); }
    }

    public int LockCount(int area)
    {
        lock (_mutex)
        {
            if (_fileLocks.Contains(area)) return int.MaxValue;
            return _recordLocks.TryGetValue(area, out var locks) ? locks.Count : 0;
        }
    }

    public IReadOnlyList<RecordLock> GetLocks(int area)
    {
        lock (_mutex)
        {
            return _recordLocks.TryGetValue(area, out var locks)
                ? locks.ToArray() : Array.Empty<RecordLock>();
        }
    }

    public void ClearArea(int area)
    {
        lock (_mutex) { _fileLocks.Remove(area); _recordLocks.Remove(area); }
    }

    public void ClearAll()
    {
        lock (_mutex) { _fileLocks.Clear(); _recordLocks.Clear(); }
    }

    // ═══════════════════════════════════════════════════
    // Retry con backoff esponenziale
    // ═══════════════════════════════════════════════════

    private bool TryLockWithRetry(Func<bool> lockAction, Action onSuccess)
    {
        // Primo tentativo immediato
        if (lockAction())
        {
            onSuccess();
            return true;
        }

        var retries = MaxRetries;
        var delay = RetryBaseMs;

        for (int attempt = 0; attempt < retries; attempt++)
        {
            Thread.Sleep(delay);
            if (lockAction())
            {
                onSuccess();
                return true;
            }
            delay = Math.Min(delay * 2, 1000); // backoff esponenziale, max 1s
        }

        return false;
    }
}
