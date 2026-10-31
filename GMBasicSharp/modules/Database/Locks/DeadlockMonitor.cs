// FILE: Basic.Databases.Locks.DeadlockMonitor

using System.Collections.Concurrent;

// ReSharper disable CheckNamespace
// ReSharper disable InconsistentNaming

namespace Basic.Databases;

/// <summary>
/// Grafo wait-for intra-processo per rilevamento deadlock (vittima = transazione più giovane).
/// Traccia i proprietari dei lock a livello processo (condiviso tra tutti gli handle sullo stesso file).
/// </summary>
internal static class DeadlockMonitor
{
    internal const int PollIntervalMs = 25;

    private static readonly ConcurrentDictionary<int, long> ThreadTransactionIds = new();
    private static readonly ConcurrentDictionary<int, string> WaitingOnResource = new();
    private static readonly ConcurrentDictionary<int, bool> WaitingWantsWrite = new();
    private static readonly ConcurrentDictionary<int, byte> AbortRequested = new();
    private static readonly ConcurrentDictionary<string, int> WriteOwners = new();
    private static readonly ConcurrentDictionary<string, ConcurrentDictionary<int, byte>> ReadOwners = new();

    internal static void BindTransaction(int threadId, long transactionId) =>
        ThreadTransactionIds[threadId] = transactionId;

    internal static void UnbindTransaction(int threadId)
    {
        ThreadTransactionIds.TryRemove(threadId, out _);
        AbortRequested.TryRemove(threadId, out _);
        WaitingOnResource.TryRemove(threadId, out _);
        WaitingWantsWrite.TryRemove(threadId, out _);
        ReleaseAllLocksForThread(threadId);
    }

    /// <summary>
    /// Rimuove ownership residue dopo rollback/commit (evita stale entry nel grafo).
    /// </summary>
    internal static void ReleaseAllLocksForThread(int threadId)
    {
        foreach (var entry in WriteOwners)
        {
            if (entry.Value == threadId)
            {
                WriteOwners.TryRemove(entry.Key, out _);
            }
        }

        foreach (var entry in ReadOwners)
        {
            entry.Value.TryRemove(threadId, out _);
        }
    }

    internal static void SetWaiting(int threadId, string resourceKey, bool wantsWrite)
    {
        WaitingOnResource[threadId] = resourceKey;
        WaitingWantsWrite[threadId] = wantsWrite;
    }

    internal static void ClearWaiting(int threadId)
    {
        WaitingOnResource.TryRemove(threadId, out _);
        WaitingWantsWrite.TryRemove(threadId, out _);
    }

    internal static bool IsAbortRequested(int threadId) => AbortRequested.ContainsKey(threadId);

    internal static void RequestAbort(int threadId) => AbortRequested[threadId] = 0;

    internal static void ClearAbort(int threadId) => AbortRequested.TryRemove(threadId, out _);

    internal static void RecordWriteAcquired(int threadId, string resourceKey) =>
        WriteOwners[resourceKey] = threadId;

    internal static void RecordReadAcquired(int threadId, string resourceKey)
    {
        var readers = ReadOwners.GetOrAdd(resourceKey, static _ => new ConcurrentDictionary<int, byte>());
        readers[threadId] = 0;
    }

    internal static void RecordWriteReleased(int threadId, string resourceKey)
    {
        if (WriteOwners.TryGetValue(resourceKey, out var owner) && owner == threadId)
        {
            WriteOwners.TryRemove(resourceKey, out _);
        }
    }

    internal static void RecordReadReleased(int threadId, string resourceKey)
    {
        if (ReadOwners.TryGetValue(resourceKey, out var readers))
        {
            readers.TryRemove(threadId, out _);
        }
    }

    internal static (int? WriteOwner, IReadOnlyList<int> ReadOwners) GetResourceOwners(string resourceKey)
    {
        int? writeOwner = WriteOwners.TryGetValue(resourceKey, out var wo) ? wo : null;
        if (!ReadOwners.TryGetValue(resourceKey, out var readers) || readers.IsEmpty)
        {
            return (writeOwner, []);
        }

        return (writeOwner, readers.Keys.ToArray());
    }

    internal static bool TryResolveDeadlock(
        int waiterThreadId,
        string resourceKey,
        bool wantsWrite,
        out int victimThreadId)
    {
        victimThreadId = 0;
        var blockers = GetBlockers(resourceKey, wantsWrite, waiterThreadId);
        var cycleFound = false;

        foreach (var blocker in blockers)
        {
            if (!IsReachable(blocker, waiterThreadId, new HashSet<int>()))
            {
                continue;
            }

            cycleFound = true;
            break;
        }

        if (!cycleFound)
        {
            return false;
        }

        var participants = CollectCycleParticipants(waiterThreadId, blockers);
        victimThreadId = SelectYoungestTransaction(participants);
        if (victimThreadId != waiterThreadId)
        {
            RequestAbort(victimThreadId);
        }

        return true;
    }

    private static HashSet<int> CollectCycleParticipants(int waiterThreadId, HashSet<int> blockers)
    {
        var participants = new HashSet<int> { waiterThreadId };
        foreach (var blocker in blockers)
        {
            if (IsReachable(blocker, waiterThreadId, new HashSet<int>()))
            {
                participants.Add(blocker);
            }
        }

        return participants;
    }

    private static HashSet<int> GetBlockers(string resourceKey, bool wantsWrite, int waiterThreadId)
    {
        var (writeOwner, readOwners) = GetResourceOwners(resourceKey);
        var blockers = new HashSet<int>();
        if (writeOwner is int wo && wo != waiterThreadId)
        {
            blockers.Add(wo);
        }

        if (wantsWrite)
        {
            foreach (var reader in readOwners)
            {
                if (reader != waiterThreadId)
                {
                    blockers.Add(reader);
                }
            }
        }

        return blockers;
    }

    private static bool IsReachable(int fromThreadId, int targetThreadId, HashSet<int> visited)
    {
        if (fromThreadId == targetThreadId)
        {
            return true;
        }

        if (!visited.Add(fromThreadId))
        {
            return false;
        }

        if (!WaitingOnResource.TryGetValue(fromThreadId, out var waitedResource))
        {
            return false;
        }

        var wantsWrite = WaitingWantsWrite.GetValueOrDefault(fromThreadId, true);
        foreach (var holder in GetBlockers(waitedResource, wantsWrite, fromThreadId))
        {
            if (IsReachable(holder, targetThreadId, visited))
            {
                return true;
            }
        }

        return false;
    }

    private static int SelectYoungestTransaction(IEnumerable<int> threadIds)
    {
        var victim = 0;
        var maxTx = long.MinValue;
        foreach (var threadId in threadIds)
        {
            var txId = ThreadTransactionIds.GetValueOrDefault(threadId, threadId);
            if (txId >= maxTx)
            {
                maxTx = txId;
                victim = threadId;
            }
        }

        return victim;
    }
}
