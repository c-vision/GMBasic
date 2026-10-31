#include <unistd.h>
#include <pthread.h>
#include <time.h>

/* SPLIT:Constants.c */
const uint8_t Config_MagicBytes[8] = { 'I', 'S', 'A', 'M', '_', 'V', '1', '0' };

void IsamFileHeader_InitEmpty(IsamFileHeader* h, int recordSize)
{
    int i;
    memset(h, 0, sizeof(*h));
    memcpy(h->Magic, Config_MagicBytes, 8);
    h->Version = 1; /* IsamFormatPolicy_CurrentFormatVersion */
    h->RecordSize = recordSize;
    h->SchemaOffset = Config_HeaderSize;
    h->SchemaLength = 0;
    h->PrimaryKeyRootOffset = -1;
    h->SecondaryIndexMapOffset = Config_HeaderSize;
    h->FreeListHeadOffset = -1;
    h->NextAvailablePageOffset = Config_HeaderSize;
    h->PageFreeListHeadOffset = -1;
    for (i = 0; i < 15; i++) h->SecondaryIndexRootOffsets[i] = -1;
}

int IsamFileHeader_ValidateMagic(const IsamFileHeader* h)
{
    return memcmp(h->Magic, Config_MagicBytes, 8) == 0;
}

int IsamFileHeader_Write(FILE* f, const IsamFileHeader* h)
{
    uint8_t buf[Config_HeaderSize];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, h, sizeof(*h));
    fseek(f, 0, SEEK_SET);
    return fwrite(buf, 1, Config_HeaderSize, f) == Config_HeaderSize ? GMB_DB_OK : GMB_DB_ERR;
}

int IsamFileHeader_Read(FILE* f, IsamFileHeader* h)
{
    uint8_t buf[Config_HeaderSize];
    fseek(f, 0, SEEK_SET);
    if (fread(buf, 1, Config_HeaderSize, f) != Config_HeaderSize) return GMB_DB_ERR;
    memcpy(h, buf, sizeof(*h));
    return IsamFileHeader_ValidateMagic(h) ? GMB_DB_OK : GMB_DB_ERR;
}

int IsamFileHeader_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_hdr_selfcheck.isdb";
    FILE* f;
    IsamFileHeader w, r;
    if (sizeof(IsamFileHeader) != 216) return GMB_DB_ERR;
    IsamFileHeader_InitEmpty(&w, 64);
    f = fopen(path, "wb+");
    if (!f) return GMB_DB_ERR;
    if (IsamFileHeader_Write(f, &w) != GMB_DB_OK) { fclose(f); return GMB_DB_ERR; }
    fflush(f);
    if (IsamFileHeader_Read(f, &r) != GMB_DB_OK) { fclose(f); return GMB_DB_ERR; }
    fclose(f);
    if (!IsamFileHeader_ValidateMagic(&r)) return GMB_DB_ERR;
    if (r.Version != 1 || r.RecordSize != 64) return GMB_DB_ERR;
    if (r.SchemaOffset != Config_HeaderSize) return GMB_DB_ERR;
    if (r.PrimaryKeyRootOffset != -1) return GMB_DB_ERR;
    return GMB_DB_OK;
}

/* SPLIT:IsamPageChecksum.c */
static uint32_t IsamPageChecksum_Table[256];
static int IsamPageChecksum_TableReady;

static void IsamPageChecksum_CreateCrc32Table(void)
{
    const uint32_t polynomial = 0xEDB88320u;
    uint32_t i, j, crc;
    if (IsamPageChecksum_TableReady) return;
    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 0; j < 8; j++)
            crc = (crc & 1) != 0 ? polynomial ^ (crc >> 1) : crc >> 1;
        IsamPageChecksum_Table[i] = crc;
    }
    IsamPageChecksum_TableReady = 1;
}

static uint32_t IsamPageChecksum_Compute(const uint8_t* data, int len)
{
    uint32_t crc = 0xFFFFFFFFu;
    int i;
    IsamPageChecksum_CreateCrc32Table();
    for (i = 0; i < len; i++)
        crc = IsamPageChecksum_Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

void IsamPageChecksum_WriteChecksum(uint8_t* page, int pageLen)
{
    uint32_t crc;
    if (pageLen < IsamPageChecksum_PageSize) return;
    page[IsamPageChecksum_ChecksumOffset] = 0;
    page[IsamPageChecksum_ChecksumOffset + 1] = 0;
    page[IsamPageChecksum_ChecksumOffset + 2] = 0;
    page[IsamPageChecksum_ChecksumOffset + 3] = 0;
    crc = IsamPageChecksum_Compute(page, IsamPageChecksum_PageSize);
    page[IsamPageChecksum_ChecksumOffset] = (uint8_t)crc;
    page[IsamPageChecksum_ChecksumOffset + 1] = (uint8_t)(crc >> 8);
    page[IsamPageChecksum_ChecksumOffset + 2] = (uint8_t)(crc >> 16);
    page[IsamPageChecksum_ChecksumOffset + 3] = (uint8_t)(crc >> 24);
}

int IsamPageChecksum_TryValidate(const uint8_t* page, int pageLen, uint32_t* storedChecksum)
{
    uint8_t copy[IsamPageChecksum_PageSize];
    *storedChecksum = 0;
    if (pageLen < IsamPageChecksum_PageSize) return 0;
    *storedChecksum = (uint32_t)page[IsamPageChecksum_ChecksumOffset]
        | ((uint32_t)page[IsamPageChecksum_ChecksumOffset + 1] << 8)
        | ((uint32_t)page[IsamPageChecksum_ChecksumOffset + 2] << 16)
        | ((uint32_t)page[IsamPageChecksum_ChecksumOffset + 3] << 24);
    if (*storedChecksum == 0) return 1;
    memcpy(copy, page, IsamPageChecksum_PageSize);
    copy[IsamPageChecksum_ChecksumOffset] = 0;
    copy[IsamPageChecksum_ChecksumOffset + 1] = 0;
    copy[IsamPageChecksum_ChecksumOffset + 2] = 0;
    copy[IsamPageChecksum_ChecksumOffset + 3] = 0;
    return IsamPageChecksum_Compute(copy, IsamPageChecksum_PageSize) == *storedChecksum;
}

/* SPLIT:HeapRelocation.c */
int64_t HeapRelocation_AlignUp(int64_t value, int alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

/* SPLIT:IsamSlowOpLog.c */
void IsamSlowOpLog_LogDeadlock(const char* detail)
{
    char esc[1024];
    int i, j = 0;
    if (!detail) detail = "";
    for (i = 0; detail[i] && j < 1020; i++)
        esc[j++] = detail[i] == '"' ? '\'' : detail[i];
    esc[j] = 0;
    fprintf(stderr, "isam_slow_op op=deadlock ms=0 detail=\"%s\"\n", esc);
}

/* SPLIT:LruCache.c */

typedef struct LruNode {
    uint64_t key;
    void* value;
    struct LruNode *prev, *next;
} LruNode;

struct LruCache {
    int capacity;
    int count;
    LruNode* head;
    LruNode* tail;
    LruNode** buckets;
    int bucketN;
    uint64_t* pinned;
    int pinnedN, pinnedCap;
    pthread_mutex_t mu;
};

/* hash buckets reserved for O(1) upgrade; list scan is the current ceiling. */
LruCache* LruCache_New(int capacity)
{
    LruCache* self;
    if (capacity <= 0) return NULL;
    self = (LruCache*)calloc(1, sizeof(*self));
    self->capacity = capacity;
    self->bucketN = capacity * 2 + 1;
    self->buckets = (LruNode**)calloc((size_t)self->bucketN, sizeof(LruNode*));
    pthread_mutex_init(&self->mu, NULL);
    return self;
}

void LruCache_Dispose(LruCache* self)
{
    LruNode* n;
    if (!self) return;
    n = self->head;
    while (n) { LruNode* nx = n->next; free(n); n = nx; }
    free(self->buckets);
    free(self->pinned);
    pthread_mutex_destroy(&self->mu);
    free(self);
}

static LruNode* LruCache_find(LruCache* self, uint64_t key)
{
    LruNode* n = self->head;
    while (n) { if (n->key == key) return n; n = n->next; }
    return NULL;
}

static int LruCache_is_pinned(LruCache* self, uint64_t key)
{
    int i;
    for (i = 0; i < self->pinnedN; i++) if (self->pinned[i] == key) return 1;
    return 0;
}

int LruCache_TryGetValue(LruCache* self, uint64_t key, void** value)
{
    LruNode* n;
    pthread_mutex_lock(&self->mu);
    n = LruCache_find(self, key);
    if (!n) { pthread_mutex_unlock(&self->mu); return 0; }
    if (n != self->head) {
        if (n->prev) n->prev->next = n->next;
        if (n->next) n->next->prev = n->prev;
        if (self->tail == n) self->tail = n->prev;
        n->prev = NULL;
        n->next = self->head;
        if (self->head) self->head->prev = n;
        self->head = n;
    }
    *value = n->value;
    pthread_mutex_unlock(&self->mu);
    return 1;
}

void LruCache_Pin(LruCache* self, uint64_t key)
{
    pthread_mutex_lock(&self->mu);
    if (!LruCache_is_pinned(self, key)) {
        if (self->pinnedN >= self->pinnedCap) {
            self->pinnedCap = self->pinnedCap ? self->pinnedCap * 2 : 4;
            self->pinned = (uint64_t*)realloc(self->pinned, (size_t)self->pinnedCap * sizeof(uint64_t));
        }
        self->pinned[self->pinnedN++] = key;
    }
    pthread_mutex_unlock(&self->mu);
}

void LruCache_Unpin(LruCache* self, uint64_t key)
{
    int i;
    pthread_mutex_lock(&self->mu);
    for (i = 0; i < self->pinnedN; i++)
        if (self->pinned[i] == key) {
            memmove(self->pinned + i, self->pinned + i + 1, (size_t)(self->pinnedN - i - 1) * sizeof(uint64_t));
            self->pinnedN--;
            break;
        }
    pthread_mutex_unlock(&self->mu);
}

void LruCache_AddOrUpdate(LruCache* self, uint64_t key, void* value)
{
    LruNode* existing;
    pthread_mutex_lock(&self->mu);
    existing = LruCache_find(self, key);
    if (existing) {
        existing->value = value;
        if (existing != self->head) {
            if (existing->prev) existing->prev->next = existing->next;
            if (existing->next) existing->next->prev = existing->prev;
            if (self->tail == existing) self->tail = existing->prev;
            existing->prev = NULL;
            existing->next = self->head;
            if (self->head) self->head->prev = existing;
            self->head = existing;
        }
    } else {
        LruNode* node;
        if (self->count >= self->capacity) {
            LruNode* victim = self->tail;
            while (victim && LruCache_is_pinned(self, victim->key)) victim = victim->prev;
            if (victim) {
                if (victim->prev) victim->prev->next = victim->next;
                if (victim->next) victim->next->prev = victim->prev;
                if (self->head == victim) self->head = victim->next;
                if (self->tail == victim) self->tail = victim->prev;
                free(victim);
                self->count--;
            }
        }
        node = (LruNode*)calloc(1, sizeof(*node));
        node->key = key;
        node->value = value;
        node->next = self->head;
        if (self->head) self->head->prev = node;
        self->head = node;
        if (!self->tail) self->tail = node;
        self->count++;
    }
    pthread_mutex_unlock(&self->mu);
}

void LruCache_Remove(LruCache* self, uint64_t key)
{
    LruNode* n;
    pthread_mutex_lock(&self->mu);
    n = LruCache_find(self, key);
    if (n) {
        if (n->prev) n->prev->next = n->next;
        if (n->next) n->next->prev = n->prev;
        if (self->head == n) self->head = n->next;
        if (self->tail == n) self->tail = n->prev;
        free(n);
        self->count--;
    }
    pthread_mutex_unlock(&self->mu);
}

void LruCache_Clear(LruCache* self)
{
    LruNode* n;
    pthread_mutex_lock(&self->mu);
    n = self->head;
    while (n) { LruNode* nx = n->next; free(n); n = nx; }
    self->head = self->tail = NULL;
    self->count = 0;
    pthread_mutex_unlock(&self->mu);
}

/* SPLIT:IsamLruCache.c */
struct IsamLruCache { LruCache* inner; };

IsamLruCache* IsamLruCache_New(void)
{
    IsamLruCache* self = (IsamLruCache*)calloc(1, sizeof(*self));
    self->inner = LruCache_New(IsamLruCache_DefaultCapacity);
    return self;
}
void IsamLruCache_Dispose(IsamLruCache* self) { if (!self) return; LruCache_Dispose(self->inner); free(self); }
int IsamLruCache_TryGetValue(IsamLruCache* self, uint64_t key, void** value) { return LruCache_TryGetValue(self->inner, key, value); }
void IsamLruCache_AddOrUpdate(IsamLruCache* self, uint64_t key, void* value) { LruCache_AddOrUpdate(self->inner, key, value); }
void IsamLruCache_Pin(IsamLruCache* self, uint64_t key) { LruCache_Pin(self->inner, key); }
void IsamLruCache_Unpin(IsamLruCache* self, uint64_t key) { LruCache_Unpin(self->inner, key); }
void IsamLruCache_Remove(IsamLruCache* self, uint64_t key) { LruCache_Remove(self->inner, key); }
void IsamLruCache_Clear(IsamLruCache* self) { LruCache_Clear(self->inner); }

/* SPLIT:DeadlockMonitor.c */

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

/* SPLIT:IntraProcessLockManager.c */

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
