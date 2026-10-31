/* FILE: native/gmb_db_c/src/Locks/DeadlockMonitor.c */
/* Port 1:1 di modules/Database/Locks/DeadlockMonitor.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Locks.DeadlockMonitor
//
//using System.Collections.Concurrent;
//
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//
//namespace Basic.Databases;
//
///// <summary>
///// Grafo wait-for intra-processo per rilevamento deadlock (vittima = transazione più giovane).
///// Traccia i proprietari dei lock a livello processo (condiviso tra tutti gli handle sullo stesso file).
///// </summary>
//internal static class DeadlockMonitor
//{
//    internal const int PollIntervalMs = 25;
//
//    private static readonly ConcurrentDictionary<int, long> ThreadTransactionIds = new();
//    private static readonly ConcurrentDictionary<int, string> WaitingOnResource = new();
//    private static readonly ConcurrentDictionary<int, bool> WaitingWantsWrite = new();
//    private static readonly ConcurrentDictionary<int, byte> AbortRequested = new();
//    private static readonly ConcurrentDictionary<string, int> WriteOwners = new();
//    private static readonly ConcurrentDictionary<string, ConcurrentDictionary<int, byte>> ReadOwners = new();
//
//    internal static void BindTransaction(int threadId, long transactionId) =>
//        ThreadTransactionIds[threadId] = transactionId;
//
//    internal static void UnbindTransaction(int threadId)
//    {
//        ThreadTransactionIds.TryRemove(threadId, out _);
//        AbortRequested.TryRemove(threadId, out _);
//        WaitingOnResource.TryRemove(threadId, out _);
//        WaitingWantsWrite.TryRemove(threadId, out _);
//        ReleaseAllLocksForThread(threadId);
//    }
//
//    /// <summary>
//    /// Rimuove ownership residue dopo rollback/commit (evita stale entry nel grafo).
//    /// </summary>
//    internal static void ReleaseAllLocksForThread(int threadId)
//    {
//        foreach (var entry in WriteOwners)
//        {
//            if (entry.Value == threadId)
//            {
//                WriteOwners.TryRemove(entry.Key, out _);
//            }
//        }
//
//        foreach (var entry in ReadOwners)
//        {
//            entry.Value.TryRemove(threadId, out _);
//        }
//    }
//
//    internal static void SetWaiting(int threadId, string resourceKey, bool wantsWrite)
//    {
//        WaitingOnResource[threadId] = resourceKey;
//        WaitingWantsWrite[threadId] = wantsWrite;
//    }
//
//    internal static void ClearWaiting(int threadId)
//    {
//        WaitingOnResource.TryRemove(threadId, out _);
//        WaitingWantsWrite.TryRemove(threadId, out _);
//    }
//
//    internal static bool IsAbortRequested(int threadId) => AbortRequested.ContainsKey(threadId);
//
//    internal static void RequestAbort(int threadId) => AbortRequested[threadId] = 0;
//
//    internal static void ClearAbort(int threadId) => AbortRequested.TryRemove(threadId, out _);
//
//    internal static void RecordWriteAcquired(int threadId, string resourceKey) =>
//        WriteOwners[resourceKey] = threadId;
//
//    internal static void RecordReadAcquired(int threadId, string resourceKey)
//    {
//        var readers = ReadOwners.GetOrAdd(resourceKey, static _ => new ConcurrentDictionary<int, byte>());
//        readers[threadId] = 0;
//    }
//
//    internal static void RecordWriteReleased(int threadId, string resourceKey)
//    {
//        if (WriteOwners.TryGetValue(resourceKey, out var owner) && owner == threadId)
//        {
//            WriteOwners.TryRemove(resourceKey, out _);
//        }
//    }
//
//    internal static void RecordReadReleased(int threadId, string resourceKey)
//    {
//        if (ReadOwners.TryGetValue(resourceKey, out var readers))
//        {
//            readers.TryRemove(threadId, out _);
//        }
//    }
//
//    internal static (int? WriteOwner, IReadOnlyList<int> ReadOwners) GetResourceOwners(string resourceKey)
//    {
//        int? writeOwner = WriteOwners.TryGetValue(resourceKey, out var wo) ? wo : null;
//        if (!ReadOwners.TryGetValue(resourceKey, out var readers) || readers.IsEmpty)
//        {
//            return (writeOwner, []);
//        }
//
//        return (writeOwner, readers.Keys.ToArray());
//    }
//
//    internal static bool TryResolveDeadlock(
//        int waiterThreadId,
//        string resourceKey,
//        bool wantsWrite,
//        out int victimThreadId)
//    {
//        victimThreadId = 0;
//        var blockers = GetBlockers(resourceKey, wantsWrite, waiterThreadId);
//        var cycleFound = false;
//
//        foreach (var blocker in blockers)
//        {
//            if (!IsReachable(blocker, waiterThreadId, new HashSet<int>()))
//            {
//                continue;
//            }
//
//            cycleFound = true;
//            break;
//        }
//
//        if (!cycleFound)
//        {
//            return false;
//        }
//
//        var participants = CollectCycleParticipants(waiterThreadId, blockers);
//        victimThreadId = SelectYoungestTransaction(participants);
//        if (victimThreadId != waiterThreadId)
//        {
//            RequestAbort(victimThreadId);
//        }
//
//        return true;
//    }
//
//    private static HashSet<int> CollectCycleParticipants(int waiterThreadId, HashSet<int> blockers)
//    {
//        var participants = new HashSet<int> { waiterThreadId };
//        foreach (var blocker in blockers)
//        {
//            if (IsReachable(blocker, waiterThreadId, new HashSet<int>()))
//            {
//                participants.Add(blocker);
//            }
//        }
//
//        return participants;
//    }
//
//    private static HashSet<int> GetBlockers(string resourceKey, bool wantsWrite, int waiterThreadId)
//    {
//        var (writeOwner, readOwners) = GetResourceOwners(resourceKey);
//        var blockers = new HashSet<int>();
//        if (writeOwner is int wo && wo != waiterThreadId)
//        {
//            blockers.Add(wo);
//        }
//
//        if (wantsWrite)
//        {
//            foreach (var reader in readOwners)
//            {
//                if (reader != waiterThreadId)
//                {
//                    blockers.Add(reader);
//                }
//            }
//        }
//
//        return blockers;
//    }
//
//    private static bool IsReachable(int fromThreadId, int targetThreadId, HashSet<int> visited)
//    {
//        if (fromThreadId == targetThreadId)
//        {
//            return true;
//        }
//
//        if (!visited.Add(fromThreadId))
//        {
//            return false;
//        }
//
//        if (!WaitingOnResource.TryGetValue(fromThreadId, out var waitedResource))
//        {
//            return false;
//        }
//
//        var wantsWrite = WaitingWantsWrite.GetValueOrDefault(fromThreadId, true);
//        foreach (var holder in GetBlockers(waitedResource, wantsWrite, fromThreadId))
//        {
//            if (IsReachable(holder, targetThreadId, visited))
//            {
//                return true;
//            }
//        }
//
//        return false;
//    }
//
//    private static int SelectYoungestTransaction(IEnumerable<int> threadIds)
//    {
//        var victim = 0;
//        var maxTx = long.MinValue;
//        foreach (var threadId in threadIds)
//        {
//            var txId = ThreadTransactionIds.GetValueOrDefault(threadId, threadId);
//            if (txId >= maxTx)
//            {
//                maxTx = txId;
//                victim = threadId;
//            }
//        }
//
//        return victim;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <pthread.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

typedef struct IntLong { int k; int64_t v; struct IntLong* n; } IntLong;
typedef struct IntStr { int k; char v[256]; struct IntStr* n; } IntStr;
typedef struct IntBool { int k; int v; struct IntBool* n; } IntBool;
typedef struct StrInt { char k[256]; int v; struct StrInt* n; } StrInt;
typedef struct StrIntSet { char k[256]; int* ids; int n, cap; struct StrIntSet* next; } StrIntSet;

static pthread_mutex_t g_dmMu = PTHREAD_MUTEX_INITIALIZER;
static IntLong* g_threadTx;
static IntStr* g_waitingOn;
static IntBool* g_waitingWrite;
static IntBool* g_abortReq;
static StrInt* g_writeOwners;
static StrIntSet* g_readOwners;

static IntLong* il_find(IntLong* h, int k) { while (h) { if (h->k == k) return h; h = h->n; } return NULL; }
static void il_set(IntLong** h, int k, int64_t v) {
    IntLong* e = il_find(*h, k);
    if (e) { e->v = v; return; }
    e = (IntLong*)calloc(1, sizeof(*e)); e->k = k; e->v = v; e->n = *h; *h = e;
}
static void il_remove(IntLong** h, int k) {
    IntLong **p = h, *e;
    while (*p) { if ((*p)->k == k) { e = *p; *p = e->n; free(e); return; } p = &(*p)->n; }
}
static int64_t il_get(IntLong* h, int k, int64_t def) { IntLong* e = il_find(h, k); return e ? e->v : def; }

static void is_set(IntStr** h, int k, const char* v) {
    IntStr* e = *h;
    while (e) { if (e->k == k) { snprintf(e->v, sizeof(e->v), "%s", v); return; } e = e->n; }
    e = (IntStr*)calloc(1, sizeof(*e)); e->k = k; snprintf(e->v, sizeof(e->v), "%s", v); e->n = *h; *h = e;
}
static int is_get(IntStr* h, int k, char* out, int cap) {
    while (h) { if (h->k == k) { snprintf(out, (size_t)cap, "%s", h->v); return 1; } h = h->n; } return 0;
}
static void is_remove(IntStr** h, int k) {
    IntStr **p = h, *e;
    while (*p) { if ((*p)->k == k) { e = *p; *p = e->n; free(e); return; } p = &(*p)->n; }
}

static void ib_set(IntBool** h, int k, int v) {
    IntBool* e = *h;
    while (e) { if (e->k == k) { e->v = v; return; } e = e->n; }
    e = (IntBool*)calloc(1, sizeof(*e)); e->k = k; e->v = v; e->n = *h; *h = e;
}
static int ib_has(IntBool* h, int k) { while (h) { if (h->k == k) return 1; h = h->n; } return 0; }
static int ib_get(IntBool* h, int k, int def) { while (h) { if (h->k == k) return h->v; h = h->n; } return def; }
static void ib_remove(IntBool** h, int k) {
    IntBool **p = h, *e;
    while (*p) { if ((*p)->k == k) { e = *p; *p = e->n; free(e); return; } p = &(*p)->n; }
}

static StrInt* si_find(StrInt* h, const char* k) { while (h) { if (strcmp(h->k, k) == 0) return h; h = h->n; } return NULL; }
static void si_set(StrInt** h, const char* k, int v) {
    StrInt* e = si_find(*h, k);
    if (e) { e->v = v; return; }
    e = (StrInt*)calloc(1, sizeof(*e)); snprintf(e->k, sizeof(e->k), "%s", k); e->v = v; e->n = *h; *h = e;
}
static int si_get(StrInt* h, const char* k, int* v) { StrInt* e = si_find(h, k); if (!e) return 0; *v = e->v; return 1; }
static void si_remove(StrInt** h, const char* k) {
    StrInt **p = h, *e;
    while (*p) { if (strcmp((*p)->k, k) == 0) { e = *p; *p = e->n; free(e); return; } p = &(*p)->n; }
}

static StrIntSet* ss_get(const char* k) {
    StrIntSet* e = g_readOwners;
    while (e) { if (strcmp(e->k, k) == 0) return e; e = e->next; }
    e = (StrIntSet*)calloc(1, sizeof(*e));
    snprintf(e->k, sizeof(e->k), "%s", k);
    e->next = g_readOwners;
    g_readOwners = e;
    return e;
}

void DeadlockMonitor_BindTransaction(int threadId, int64_t transactionId)
{
    pthread_mutex_lock(&g_dmMu);
    il_set(&g_threadTx, threadId, transactionId);
    pthread_mutex_unlock(&g_dmMu);
}

void DeadlockMonitor_ReleaseAllLocksForThread(int threadId)
{
    StrInt **p;
    StrIntSet* e;
    pthread_mutex_lock(&g_dmMu);
    p = &g_writeOwners;
    while (*p) {
        if ((*p)->v == threadId) { StrInt* x = *p; *p = x->n; free(x); }
        else p = &(*p)->n;
    }
    for (e = g_readOwners; e; e = e->next) {
        int i;
        for (i = 0; i < e->n; i++)
            if (e->ids[i] == threadId) {
                memmove(e->ids + i, e->ids + i + 1, (size_t)(e->n - i - 1) * sizeof(int));
                e->n--; i--;
            }
    }
    pthread_mutex_unlock(&g_dmMu);
}

void DeadlockMonitor_UnbindTransaction(int threadId)
{
    pthread_mutex_lock(&g_dmMu);
    il_remove(&g_threadTx, threadId);
    ib_remove(&g_abortReq, threadId);
    is_remove(&g_waitingOn, threadId);
    ib_remove(&g_waitingWrite, threadId);
    pthread_mutex_unlock(&g_dmMu);
    DeadlockMonitor_ReleaseAllLocksForThread(threadId);
}

void DeadlockMonitor_SetWaiting(int threadId, const char* resourceKey, int wantsWrite)
{
    pthread_mutex_lock(&g_dmMu);
    is_set(&g_waitingOn, threadId, resourceKey);
    ib_set(&g_waitingWrite, threadId, wantsWrite);
    pthread_mutex_unlock(&g_dmMu);
}

void DeadlockMonitor_ClearWaiting(int threadId)
{
    pthread_mutex_lock(&g_dmMu);
    is_remove(&g_waitingOn, threadId);
    ib_remove(&g_waitingWrite, threadId);
    pthread_mutex_unlock(&g_dmMu);
}

int DeadlockMonitor_IsAbortRequested(int threadId)
{
    int r;
    pthread_mutex_lock(&g_dmMu);
    r = ib_has(g_abortReq, threadId);
    pthread_mutex_unlock(&g_dmMu);
    return r;
}

void DeadlockMonitor_RequestAbort(int threadId)
{
    pthread_mutex_lock(&g_dmMu);
    ib_set(&g_abortReq, threadId, 0);
    pthread_mutex_unlock(&g_dmMu);
}

void DeadlockMonitor_ClearAbort(int threadId)
{
    pthread_mutex_lock(&g_dmMu);
    ib_remove(&g_abortReq, threadId);
    pthread_mutex_unlock(&g_dmMu);
}

void DeadlockMonitor_RecordWriteAcquired(int threadId, const char* resourceKey)
{
    pthread_mutex_lock(&g_dmMu);
    si_set(&g_writeOwners, resourceKey, threadId);
    pthread_mutex_unlock(&g_dmMu);
}

void DeadlockMonitor_RecordReadAcquired(int threadId, const char* resourceKey)
{
    StrIntSet* e;
    pthread_mutex_lock(&g_dmMu);
    e = ss_get(resourceKey);
    if (e->n >= e->cap) { e->cap = e->cap ? e->cap * 2 : 4; e->ids = (int*)realloc(e->ids, (size_t)e->cap * sizeof(int)); }
    e->ids[e->n++] = threadId;
    pthread_mutex_unlock(&g_dmMu);
}

void DeadlockMonitor_RecordWriteReleased(int threadId, const char* resourceKey)
{
    int owner;
    pthread_mutex_lock(&g_dmMu);
    if (si_get(g_writeOwners, resourceKey, &owner) && owner == threadId)
        si_remove(&g_writeOwners, resourceKey);
    pthread_mutex_unlock(&g_dmMu);
}

void DeadlockMonitor_RecordReadReleased(int threadId, const char* resourceKey)
{
    StrIntSet* e;
    int i;
    pthread_mutex_lock(&g_dmMu);
    e = ss_get(resourceKey);
    for (i = 0; i < e->n; i++)
        if (e->ids[i] == threadId) {
            memmove(e->ids + i, e->ids + i + 1, (size_t)(e->n - i - 1) * sizeof(int));
            e->n--;
            break;
        }
    pthread_mutex_unlock(&g_dmMu);
}

static void dm_get_blockers(const char* resourceKey, int wantsWrite, int waiterThreadId, int* out, int* nOut)
{
    int wo, i, n = 0;
    StrIntSet* e;
    if (si_get(g_writeOwners, resourceKey, &wo) && wo != waiterThreadId) out[n++] = wo;
    if (wantsWrite) {
        e = ss_get(resourceKey);
        for (i = 0; i < e->n; i++)
            if (e->ids[i] != waiterThreadId) out[n++] = e->ids[i];
    }
    *nOut = n;
}

static int dm_reachable(int fromThreadId, int targetThreadId, int* visited, int visitedN)
{
    char waited[256];
    int blockers[64], bn, i, vi;
    int wantsWrite;
    if (fromThreadId == targetThreadId) return 1;
    for (vi = 0; vi < visitedN; vi++) if (visited[vi] == fromThreadId) return 0;
    visited[visitedN++] = fromThreadId;
    if (!is_get(g_waitingOn, fromThreadId, waited, (int)sizeof(waited))) return 0;
    wantsWrite = ib_get(g_waitingWrite, fromThreadId, 1);
    dm_get_blockers(waited, wantsWrite, fromThreadId, blockers, &bn);
    for (i = 0; i < bn; i++)
        if (dm_reachable(blockers[i], targetThreadId, visited, visitedN)) return 1;
    return 0;
}

static int dm_youngest(const int* ids, int n)
{
    int victim = 0, i;
    int64_t maxTx = INT64_MIN;
    for (i = 0; i < n; i++) {
        int64_t txId = il_get(g_threadTx, ids[i], ids[i]);
        if (txId >= maxTx) { maxTx = txId; victim = ids[i]; }
    }
    return victim;
}

int DeadlockMonitor_TryResolveDeadlock(int waiterThreadId, const char* resourceKey, int wantsWrite, int* victimThreadId)
{
    int blockers[64], bn, i, cycle = 0;
    int visited[64];
    int participants[64], pn = 0;
    pthread_mutex_lock(&g_dmMu);
    *victimThreadId = 0;
    dm_get_blockers(resourceKey, wantsWrite, waiterThreadId, blockers, &bn);
    for (i = 0; i < bn; i++) {
        memset(visited, 0, sizeof(visited));
        if (dm_reachable(blockers[i], waiterThreadId, visited, 0)) { cycle = 1; break; }
    }
    if (!cycle) { pthread_mutex_unlock(&g_dmMu); return 0; }
    participants[pn++] = waiterThreadId;
    for (i = 0; i < bn; i++) {
        memset(visited, 0, sizeof(visited));
        if (dm_reachable(blockers[i], waiterThreadId, visited, 0))
            participants[pn++] = blockers[i];
    }
    *victimThreadId = dm_youngest(participants, pn);
    if (*victimThreadId != waiterThreadId)
        ib_set(&g_abortReq, *victimThreadId, 0);
    pthread_mutex_unlock(&g_dmMu);
    return 1;
}
