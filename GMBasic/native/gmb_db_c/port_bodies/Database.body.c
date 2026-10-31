#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>

#define SPEED_MAX_TABLES 32
#define SPEED_MAX_SST_CACHE 64
#define SPEED_SST_SEARCH_MAX 64

typedef struct {
    int tableId;
    Memtable* mem;
} SpeedMemSlot;

typedef struct {
    int tableId;
    SpeedWriteAheadLog* wal;
} SpeedWalSlot;

typedef struct {
    char path[1024];
    SstReader* reader;
} SpeedSstCacheSlot;

typedef struct SpeedTableCursor {
    int used;
    int tableId;
    uint8_t* indexKey;
    int indexKeyLen;
    uint8_t* pk;
    int pkLen;
    char activeIndex[64];
    int bof;
    int eof;
} SpeedTableCursor;

struct Database {
    char DatabasePath[1024];
    Manifest* manifest;
    SpeedMemSlot mems[SPEED_MAX_TABLES];
    int memN;
    SpeedWalSlot wals[SPEED_MAX_TABLES];
    int walN;
    SpeedSstCacheSlot sstCache[SPEED_MAX_SST_CACHE];
    int sstCacheN;
    int durabilityFull;
    int inTx;
    SpeedMemSlot txBuf[SPEED_MAX_TABLES];
    int txN;
    int groupCommitDepth;
    int batchWalSuppressDepth;
    int lockHeld;
    SpeedTableCursor cursors[SPEED_MAX_TABLES];
    struct {
        int tableId;
        SpeedExpressionEvaluator ev;
        void* user;
    } indexEvals[SPEED_MAX_TABLES];
    int indexEvalN;
};

static int db_dup_bytes(const uint8_t* src, int len, uint8_t** out, int* outLen)
{
    if (out) *out = NULL;
    if (outLen) *outLen = 0;
    if (len < 0) len = 0;
    if (len == 0) return 1;
    if (!src) return 0;
    *out = (uint8_t*)malloc((size_t)len);
    if (!*out) return 0;
    memcpy(*out, src, (size_t)len);
    if (outLen) *outLen = len;
    return 1;
}

static void db_cursor_clear_pos(SpeedTableCursor* c)
{
    if (!c) return;
    free(c->indexKey);
    free(c->pk);
    c->indexKey = NULL;
    c->pk = NULL;
    c->indexKeyLen = 0;
    c->pkLen = 0;
}

static SpeedTableCursor* db_cursor_find(const Database* self, int tableId)
{
    int i;
    if (!self) return NULL;
    for (i = 0; i < SPEED_MAX_TABLES; i++)
        if (self->cursors[i].used && self->cursors[i].tableId == tableId)
            return (SpeedTableCursor*)&self->cursors[i];
    return NULL;
}

static SpeedTableCursor* db_get_cursor(Database* self, int tableId)
{
    SpeedTableCursor* c;
    char name[128];
    int i;
    if (!self || tableId <= 0) return NULL;
    c = db_cursor_find(self, tableId);
    if (c) return c;
    if (!Manifest_TryGetTable(self->manifest, tableId, name, (int)sizeof(name))) return NULL;
    for (i = 0; i < SPEED_MAX_TABLES; i++) {
        if (self->cursors[i].used) continue;
        memset(&self->cursors[i], 0, sizeof(self->cursors[i]));
        self->cursors[i].used = 1;
        self->cursors[i].tableId = tableId;
        snprintf(self->cursors[i].activeIndex, sizeof(self->cursors[i].activeIndex), "Id");
        return &self->cursors[i];
    }
    return NULL;
}

static int db_move_empty(SpeedTableCursor* c)
{
    db_cursor_clear_pos(c);
    c->bof = 1;
    c->eof = 1;
    return 0;
}

static int db_move_boundary(SpeedTableCursor* c, int direction)
{
    db_cursor_clear_pos(c);
    if (direction > 0) { c->eof = 1; c->bof = 0; }
    else { c->bof = 1; c->eof = 0; }
    return 0;
}

static int db_cursor_set(SpeedTableCursor* c, const uint8_t* ik, int ikl, const uint8_t* pk, int pkl)
{
    uint8_t *nik = NULL, *npk = NULL;
    int nikl = 0, npkl = 0;
    if (!db_dup_bytes(ik, ikl, &nik, &nikl) || !db_dup_bytes(pk, pkl, &npk, &npkl)) {
        free(nik);
        free(npk);
        return 0;
    }
    db_cursor_clear_pos(c);
    c->indexKey = nik;
    c->indexKeyLen = nikl;
    c->pk = npk;
    c->pkLen = npkl;
    c->bof = 0;
    c->eof = 0;
    return 1;
}

static int db_get_scan_source(Database* self, int mainTableId, const char* indexName,
                              int* outId, int* isIndex)
{
    char mainName[128], idxTable[256];
    int idxId = 0;
    if (outId) *outId = mainTableId;
    if (isIndex) *isIndex = 0;
    if (!self || !indexName || !indexName[0] || gmb_strcasecmp(indexName, "Id") == 0)
        return 1;
    if (!Manifest_TryGetTable(self->manifest, mainTableId, mainName, (int)sizeof(mainName)))
        return 0;
    Manifest_IndexTableName(mainName, indexName, idxTable, (int)sizeof(idxTable));
    if (Manifest_TryGetTableByName(self->manifest, idxTable, &idxId) && idxId > 0) {
        if (outId) *outId = idxId;
        if (isIndex) *isIndex = 1;
        return 1;
    }
    return 0;
}

static Memtable* db_get_or_create_memtable(Database* self, int tableId)
{
    int i;
    for (i = 0; i < self->memN; i++)
        if (self->mems[i].tableId == tableId) return self->mems[i].mem;
    if (self->memN >= SPEED_MAX_TABLES) return NULL;
    self->mems[self->memN].tableId = tableId;
    self->mems[self->memN].mem = Memtable_New();
    return self->mems[self->memN++].mem;
}

static SpeedWriteAheadLog* db_get_or_create_wal(Database* self, int tableId)
{
    int i;
    uint64_t n;
    for (i = 0; i < self->walN; i++)
        if (self->wals[i].tableId == tableId) return self->wals[i].wal;
    if (self->walN >= SPEED_MAX_TABLES) return NULL;
    n = Manifest_GetNextFileNumber(self->manifest);
    self->wals[self->walN].tableId = tableId;
    self->wals[self->walN].wal = SpeedWriteAheadLog_New(self->DatabasePath, n);
    self->walN++;
    Manifest_Save(self->manifest);
    return self->wals[self->walN - 1].wal;
}

static SpeedWriteAheadLog* db_take_wal(Database* self, int tableId)
{
    int i;
    SpeedWriteAheadLog* wal;
    for (i = 0; i < self->walN; i++) {
        if (self->wals[i].tableId != tableId) continue;
        wal = self->wals[i].wal;
        memmove(&self->wals[i], &self->wals[i + 1], (size_t)(self->walN - i - 1) * sizeof(self->wals[0]));
        self->walN--;
        return wal;
    }
    return NULL;
}

/* ponytail: Recover replays *.wal into memtables but does not close those files (C# same).
 * First SST flush must consume them or reopen resurrects flushed keys. Ceiling: all tables share the wal dir. */
static void db_unlink_all_wals(const char* databasePath)
{
    char walDirectory[1100];
    DIR* dir;
    struct dirent* ent;
    snprintf(walDirectory, sizeof(walDirectory), "%s/%s", databasePath, Config_WalDirectoryName);
    dir = opendir(walDirectory);
    if (!dir) return;
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, ".wal") == 0) {
            char path[1100];
            snprintf(path, sizeof(path), "%s/%s", walDirectory, ent->d_name);
            unlink(path);
        }
    }
    closedir(dir);
}

static SstReader* db_sst_get_or_add(Database* self, const char* path)
{
    int i;
    for (i = 0; i < self->sstCacheN; i++)
        if (strcmp(self->sstCache[i].path, path) == 0) return self->sstCache[i].reader;
    if (self->sstCacheN >= SPEED_MAX_SST_CACHE) return NULL;
    self->sstCache[self->sstCacheN].reader = SstReader_Open(path);
    if (!self->sstCache[self->sstCacheN].reader) return NULL;
    snprintf(self->sstCache[self->sstCacheN].path, sizeof(self->sstCache[self->sstCacheN].path), "%s", path);
    return self->sstCache[self->sstCacheN++].reader;
}

static int db_should_fsync(const Database* self)
{
    return self->durabilityFull && self->groupCommitDepth == 0 && self->batchWalSuppressDepth == 0;
}

static void db_maybe_flush_wal(Database* self, SpeedWriteAheadLog* wal)
{
    if (wal && db_should_fsync(self)) SpeedWriteAheadLog_Flush(wal);
}

static void db_maybe_flush_and_compact(Database* self, int tableId);

static void db_flush_all_wals(Database* self)
{
    int i;
    for (i = 0; i < self->walN; i++)
        SpeedWriteAheadLog_Flush(self->wals[i].wal);
}

const char* Database_GetPath(const Database* self)
{
    return self ? self->DatabasePath : "";
}

Manifest* Database_GetManifest(Database* self)
{
    return self ? self->manifest : NULL;
}

SstReader* Database_SstGetOrAdd(Database* self, const char* path)
{
    return self ? db_sst_get_or_add(self, path) : NULL;
}

void Database_SstCacheRemove(Database* self, const char* path)
{
    int i;
    if (!self || !path) return;
    for (i = 0; i < self->sstCacheN; i++) {
        if (strcmp(self->sstCache[i].path, path) != 0) continue;
        SstReader_Dispose(self->sstCache[i].reader);
        memmove(&self->sstCache[i], &self->sstCache[i + 1],
                (size_t)(self->sstCacheN - i - 1) * sizeof(self->sstCache[0]));
        self->sstCacheN--;
        return;
    }
}

static Memtable* db_tx_buffer(Database* self, int tableId)
{
    int i;
    for (i = 0; i < self->txN; i++)
        if (self->txBuf[i].tableId == tableId) return self->txBuf[i].mem;
    if (self->txN >= SPEED_MAX_TABLES) return NULL;
    self->txBuf[self->txN].tableId = tableId;
    self->txBuf[self->txN].mem = Memtable_New();
    return self->txBuf[self->txN++].mem;
}

static Memtable* db_tx_buffer_get(const Database* self, int tableId)
{
    int i;
    for (i = 0; i < self->txN; i++)
        if (self->txBuf[i].tableId == tableId) return self->txBuf[i].mem;
    return NULL;
}

static void db_tx_clear(Database* self)
{
    int i;
    for (i = 0; i < self->txN; i++)
        Memtable_Dispose(self->txBuf[i].mem);
    self->txN = 0;
    self->inTx = 0;
}

static int db_copy_from_memtable(Memtable* mem, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen)
{
    uint8_t* mv = NULL;
    int mvlen = 0, tomb = 0;
    if (!Memtable_Get(mem, key, keyLen, &mv, &mvlen, &tomb)) return -1;
    if (tomb || !mv) return 0;
    if (value) {
        *value = (uint8_t*)malloc((size_t)mvlen);
        if (mvlen > 0) memcpy(*value, mv, (size_t)mvlen);
    }
    if (valueLen) *valueLen = mvlen;
    return 1;
}

Database* Database_Open(const char* path)
{
    Database* self;
    Memtable** recovered = NULL;
    int* ids = NULL;
    int n = 0, i;
    if (!path) return NULL;
    self = (Database*)calloc(1, sizeof(*self));
    snprintf(self->DatabasePath, sizeof(self->DatabasePath), "%s", path);
    self->durabilityFull = 1;
    mkdir(path, 0777);
    {
        char sub[1100];
        snprintf(sub, sizeof(sub), "%s/%s", path, Config_WalDirectoryName);
        mkdir(sub, 0777);
        snprintf(sub, sizeof(sub), "%s/%s", path, Config_SstDirectoryName);
        mkdir(sub, 0777);
    }
    if (SpeedDatabaseLock_Acquire(path) != GMB_DB_OK) {
        free(self);
        return NULL;
    }
    self->lockHeld = 1;
    self->manifest = Manifest_Load(path);
    if (!self->manifest) {
        SpeedDatabaseLock_Release(path);
        free(self);
        return NULL;
    }
    SpeedWriteAheadLog_Recover(path, &recovered, &ids, &n);
    for (i = 0; i < n && self->memN < SPEED_MAX_TABLES; i++) {
        self->mems[self->memN].tableId = ids[i];
        self->mems[self->memN].mem = recovered[i];
        self->memN++;
    }
    free(recovered);
    free(ids);
    Database_MaybeRebuildUntrustedRowCounts(self);
    return self;
}

void Database_Dispose(Database* self)
{
    int i;
    if (!self) return;
    Database_CollectGarbage(self);
    for (i = 0; i < self->walN; i++) {
        SpeedWriteAheadLog_Flush(self->wals[i].wal);
        SpeedWriteAheadLog_Dispose(self->wals[i].wal);
    }
    for (i = 0; i < self->sstCacheN; i++)
        SstReader_Dispose(self->sstCache[i].reader);
    for (i = 0; i < self->memN; i++)
        Memtable_Dispose(self->mems[i].mem);
    for (i = 0; i < SPEED_MAX_TABLES; i++) {
        if (!self->cursors[i].used) continue;
        db_cursor_clear_pos(&self->cursors[i]);
        self->cursors[i].used = 0;
    }
    db_tx_clear(self);
    Manifest_Dispose(self->manifest);
    if (self->lockHeld)
        SpeedDatabaseLock_Release(self->DatabasePath);
    free(self);
}

int Database_CollectGarbage(Database* self)
{
    char names[64][256];
    int n, i, deleted = 0, changed = 0;
    if (!self || !self->manifest) return 0;
    n = Manifest_GetFilesForGarbageCollection(self->manifest, names, 64);
    for (i = 0; i < n; i++) {
        char path[1100];
        snprintf(path, sizeof(path), "%s/%s/%s", self->DatabasePath, Config_SstDirectoryName, names[i]);
        if (unlink(path) == 0 || access(path, F_OK) != 0) {
            Manifest_RemoveFileFromGarbageCollection(self->manifest, names[i]);
            deleted++;
            changed = 1;
        }
    }
    if (changed)
        Manifest_Save(self->manifest);
    return deleted;
}

int Database_CreateTable(Database* self, const char* typeName)
{
    if (!self) return -1;
    return Manifest_CreateTable(self->manifest, typeName);
}

static void db_evict_table(Database* self, int tableId)
{
    int i;
    SpeedWriteAheadLog* wal;
    wal = db_take_wal(self, tableId);
    if (wal) {
        SpeedWriteAheadLog_Flush(wal);
        SpeedWriteAheadLog_Dispose(wal);
    }
    for (i = 0; i < self->memN; i++) {
        if (self->mems[i].tableId != tableId) continue;
        Memtable_Dispose(self->mems[i].mem);
        memmove(&self->mems[i], &self->mems[i + 1],
                (size_t)(self->memN - i - 1) * sizeof(self->mems[0]));
        self->memN--;
        break;
    }
}

int Database_DropTableInternal(Database* self, const char* tableName)
{
    char files[64][256];
    const char* ptrs[64];
    int n, i, tableId = -1;
    if (!self || !tableName) return GMB_DB_ARG;
    n = Manifest_PrepareTableForDeletion(self->manifest, tableName, &tableId, files, 64);
    if (tableId < 0) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        char path[1100];
        ptrs[i] = files[i];
        snprintf(path, sizeof(path), "%s/%s/%s", self->DatabasePath, Config_SstDirectoryName, files[i]);
        Database_SstCacheRemove(self, path);
    }
    if (n > 0)
        Manifest_AddFilesToGarbageCollection(self->manifest, ptrs, n);
    Manifest_Save(self->manifest);
    db_evict_table(self, tableId);
    {
        SpeedTableCursor* c = db_cursor_find(self, tableId);
        if (c) {
            db_cursor_clear_pos(c);
            c->used = 0;
        }
    }
    return GMB_DB_OK;
}

int Database_Set(Database* self, int tableId, const uint8_t* key, int keyLen, const uint8_t* value, int valueLen)
{
    Memtable* mem;
    SpeedWriteAheadLog* wal;
    uint8_t* old = NULL;
    int oldLen = 0;
    int maintain = 0;
    if (!self || !key || !value) return GMB_DB_ARG;
    if (self->inTx) {
        mem = db_tx_buffer(self, tableId);
        return mem ? Memtable_Set(mem, key, keyLen, value, valueLen) : GMB_DB_ERR;
    }
    Database_AdjustRowCountOnSet(self, tableId, key, keyLen);
    if (Database_IsMainDataTable(self, tableId)
        && Manifest_SecondaryKeyCount(self->manifest, tableId) > 0) {
        Database_Get(self, tableId, key, keyLen, &old, &oldLen);
        maintain = 1;
    }
    wal = db_get_or_create_wal(self, tableId);
    mem = db_get_or_create_memtable(self, tableId);
    if (!wal || !mem) { free(old); return GMB_DB_ERR; }
    SpeedWriteAheadLog_LogSet(wal, tableId, key, keyLen, value, valueLen);
    if (Memtable_Set(mem, key, keyLen, value, valueLen) != GMB_DB_OK) { free(old); return GMB_DB_ERR; }
    db_maybe_flush_wal(self, wal);
    if (maintain)
        Database_MaintainSecondaryIndexesOnPut(self, tableId, key, keyLen, old, oldLen, value, valueLen);
    free(old);
    db_maybe_flush_and_compact(self, tableId);
    return GMB_DB_OK;
}

int Database_Delete(Database* self, int tableId, const uint8_t* key, int keyLen)
{
    Memtable* mem;
    SpeedWriteAheadLog* wal;
    uint8_t* old = NULL;
    int oldLen = 0;
    int maintain = 0;
    if (!self || !key) return GMB_DB_ARG;
    if (self->inTx) {
        mem = db_tx_buffer(self, tableId);
        return mem ? Memtable_Delete(mem, key, keyLen) : GMB_DB_ERR;
    }
    Database_AdjustRowCountOnDelete(self, tableId, key, keyLen);
    if (Database_IsMainDataTable(self, tableId)
        && Manifest_SecondaryKeyCount(self->manifest, tableId) > 0) {
        Database_Get(self, tableId, key, keyLen, &old, &oldLen);
        maintain = 1;
    }
    wal = db_get_or_create_wal(self, tableId);
    mem = db_get_or_create_memtable(self, tableId);
    if (!wal || !mem) { free(old); return GMB_DB_ERR; }
    SpeedWriteAheadLog_LogDelete(wal, tableId, key, keyLen);
    if (Memtable_Delete(mem, key, keyLen) != GMB_DB_OK) { free(old); return GMB_DB_ERR; }
    db_maybe_flush_wal(self, wal);
    if (maintain)
        Database_MaintainSecondaryIndexesOnDelete(self, tableId, key, keyLen, old, oldLen);
    free(old);
    db_maybe_flush_and_compact(self, tableId);
    return GMB_DB_OK;
}

int Database_Get(Database* self, int tableId, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen)
{
    Memtable* mem;
    char names[SPEED_SST_SEARCH_MAX][256];
    int n, i;
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!self || !key) return 0;
    if (self->inTx) {
        Memtable* buf = db_tx_buffer_get(self, tableId);
        if (buf) {
            int rc = db_copy_from_memtable(buf, key, keyLen, value, valueLen);
            if (rc >= 0) return rc;
        }
    }
    mem = db_get_or_create_memtable(self, tableId);
    {
        int rc = mem ? db_copy_from_memtable(mem, key, keyLen, value, valueLen) : -1;
        if (rc >= 0) return rc;
    }
    n = Manifest_CollectSstNewestFirst(self->manifest, tableId, names, SPEED_SST_SEARCH_MAX);
    for (i = 0; i < n; i++) {
        char path[1100];
        SstReader* r;
        uint8_t* sv = NULL;
        int slen = 0;
        snprintf(path, sizeof(path), "%s/%s/%s", self->DatabasePath, Config_SstDirectoryName, names[i]);
        r = db_sst_get_or_add(self, path);
        if (!r) continue;
        if (SstReader_TryFind(r, key, keyLen, &sv, &slen)) {
            if (!sv) return 0;
            if (value) *value = sv;
            else free(sv);
            if (valueLen) *valueLen = slen;
            return 1;
        }
    }
    return 0;
}

void Database_ForceFlushTableSync(Database* self, int tableId)
{
    Memtable* toFlush = NULL;
    SpeedWriteAheadLog* oldWal;
    uint64_t fileNumber;
    char sstPath[1100];
    int i;
    if (!self) return;
    for (i = 0; i < self->memN; i++) {
        if (self->mems[i].tableId != tableId) continue;
        if (Memtable_get_KeyCount(self->mems[i].mem) == 0) return;
        toFlush = self->mems[i].mem;
        self->mems[i].mem = Memtable_New();
        break;
    }
    if (!toFlush) return;
    oldWal = db_take_wal(self, tableId);
    fileNumber = Manifest_GetNextFileNumber(self->manifest);
    snprintf(sstPath, sizeof(sstPath), "%s/%s/%06llu_%d.sst",
             self->DatabasePath, Config_SstDirectoryName,
             (unsigned long long)fileNumber, tableId);
    SstWriter_WriteFromMemtable(sstPath, toFlush);
    Manifest_AddSstFile(self->manifest, tableId, sstPath);
    Manifest_Save(self->manifest);
    if (oldWal) {
        char walCopy[1024];
        const char* walPath = SpeedWriteAheadLog_GetWalPath(oldWal);
        snprintf(walCopy, sizeof(walCopy), "%s", walPath ? walPath : "");
        SpeedWriteAheadLog_Dispose(oldWal);
        if (walCopy[0]) unlink(walCopy);
    }
    db_unlink_all_wals(self->DatabasePath);
    Memtable_Dispose(toFlush);
}

void Database_TriggerCompactionChain(Database* self, int tableId)
{
    int level;
    if (!self) return;
    Compactor_CompactLevel0(self, tableId);
    for (level = 1; level < 8; level++) {
        char names[64][256];
        int n = Manifest_GetFilesForLevel(self->manifest, tableId, level, names, 64);
        if (n >= Compactor_CompactLevelThreshold)
            Compactor_CompactLevel(self, tableId, level);
    }
}

/* ponytail: C# fire-and-forget Task.Run; C runs inline so Dispose cannot race the SST rewrite. */
static void db_maybe_flush_and_compact(Database* self, int tableId)
{
    int i;
    if (!self || self->inTx) return;
    for (i = 0; i < self->memN; i++) {
        if (self->mems[i].tableId != tableId) continue;
        if (Memtable_get_EstimatedSize(self->mems[i].mem) > Database_MemtableThreshold) {
            Database_ForceFlushTableSync(self, tableId);
            Database_TriggerCompactionChain(self, tableId);
        }
        return;
    }
}

void Database_BeginTransaction(Database* self)
{
    if (!self || self->inTx) return;
    self->inTx = 1;
    self->txN = 0;
}

void Database_Rollback(Database* self)
{
    if (self) db_tx_clear(self);
}

int Database_Commit(Database* self)
{
    int t, i;
    if (!self || !self->inTx) return GMB_DB_OK;
    if (self->txN == 0) { self->inTx = 0; return GMB_DB_OK; }
    for (t = 0; t < self->txN; t++) {
        Memtable* buf = self->txBuf[t].mem;
        int tableId = self->txBuf[t].tableId;
        int n = Memtable_get_KeyCount(buf);
        SpeedWriteAheadLog* wal;
        Memtable* mem;
        if (n == 0) continue;
        wal = db_get_or_create_wal(self, tableId);
        mem = db_get_or_create_memtable(self, tableId);
        if (!wal || !mem) { db_tx_clear(self); return GMB_DB_ERR; }
        for (i = 0; i < n; i++) {
            uint8_t *key, *val, *old = NULL;
            int keyLen, valLen, tomb, oldLen = 0;
            Memtable_Nth(buf, i, &key, &keyLen, &val, &valLen, &tomb);
            if (Database_IsMainDataTable(self, tableId)
                && Manifest_SecondaryKeyCount(self->manifest, tableId) > 0)
                Database_Get(self, tableId, key, keyLen, &old, &oldLen);
            if (tomb) {
                SpeedWriteAheadLog_LogDelete(wal, tableId, key, keyLen);
                if (Memtable_Delete(mem, key, keyLen) != GMB_DB_OK) { free(old); db_tx_clear(self); return GMB_DB_ERR; }
                Database_MaintainSecondaryIndexesOnDelete(self, tableId, key, keyLen, old, oldLen);
            } else {
                SpeedWriteAheadLog_LogSet(wal, tableId, key, keyLen, val, valLen);
                if (Memtable_Set(mem, key, keyLen, val, valLen) != GMB_DB_OK) { free(old); db_tx_clear(self); return GMB_DB_ERR; }
                Database_MaintainSecondaryIndexesOnPut(self, tableId, key, keyLen, old, oldLen, val, valLen);
            }
            free(old);
            old = NULL;
            oldLen = 0;
        }
        db_maybe_flush_wal(self, wal);
    }
    db_tx_clear(self);
    return GMB_DB_OK;
}

typedef struct {
    uint8_t* key;
    int keyLen;
    uint8_t* val;
    int valLen;
    int tomb;
    int source;
} ScanEnt;

struct DatabaseScan {
    ScanEnt* live;
    int n;
    int i;
};

static int db_scan_cmp(const void* a, const void* b)
{
    const ScanEnt* x = (const ScanEnt*)a;
    const ScanEnt* y = (const ScanEnt*)b;
    int c = ByteArrayComparer_Compare(x->key, x->keyLen, y->key, y->keyLen);
    if (c != 0) return c;
    return x->source - y->source;
}

static int db_scan_push(ScanEnt** ents, int* n, int* cap, const uint8_t* key, int keyLen,
                        const uint8_t* val, int valLen, int tomb, int source)
{
    ScanEnt* e;
    if (*n >= *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *ents = (ScanEnt*)realloc(*ents, (size_t)(*cap) * sizeof(ScanEnt));
    }
    e = &(*ents)[(*n)++];
    e->keyLen = keyLen;
    e->valLen = valLen;
    e->tomb = tomb;
    e->source = source;
    e->key = (uint8_t*)malloc((size_t)keyLen);
    if (keyLen > 0) memcpy(e->key, key, (size_t)keyLen);
    e->val = NULL;
    if (!tomb && valLen > 0) {
        e->val = (uint8_t*)malloc((size_t)valLen);
        memcpy(e->val, val, (size_t)valLen);
    }
    return 0;
}

DatabaseScan* Database_Scan(Database* self, int tableId)
{
    DatabaseScan* scan;
    ScanEnt* ents = NULL;
    int n = 0, cap = 0, i, src = 0;
    Memtable* mem;
    char names[SPEED_SST_SEARCH_MAX][256];
    int nfiles;
    if (!self) return NULL;
    mem = db_get_or_create_memtable(self, tableId);
    if (mem) {
        int kc = Memtable_get_KeyCount(mem);
        for (i = 0; i < kc; i++) {
            uint8_t *k, *v;
            int kl, vl, tomb;
            Memtable_Nth(mem, i, &k, &kl, &v, &vl, &tomb);
            db_scan_push(&ents, &n, &cap, k, kl, v, vl, tomb, src);
        }
    }
    src = 1;
    nfiles = Manifest_CollectSstOldestFirst(self->manifest, tableId, names, SPEED_SST_SEARCH_MAX);
    for (i = 0; i < nfiles; i++, src++) {
        char path[1100];
        SstReader* r;
        snprintf(path, sizeof(path), "%s/%s/%s", self->DatabasePath, Config_SstDirectoryName, names[i]);
        r = db_sst_get_or_add(self, path);
        if (!r) continue;
        SstReader_Rewind(r);
        for (;;) {
            uint8_t *k = NULL, *v = NULL;
            int kl = 0, vl = 0;
            if (!SstReader_Next(r, &k, &kl, &v, &vl)) break;
            db_scan_push(&ents, &n, &cap, k, kl, v, vl, v == NULL, src);
            free(k);
            free(v);
        }
    }
    if (n > 1) qsort(ents, (size_t)n, sizeof(ScanEnt), db_scan_cmp);
    scan = (DatabaseScan*)calloc(1, sizeof(*scan));
    for (i = 0; i < n; ) {
        int j = i + 1;
        while (j < n && ByteArrayComparer_Compare(ents[i].key, ents[i].keyLen, ents[j].key, ents[j].keyLen) == 0) {
            free(ents[j].key);
            free(ents[j].val);
            j++;
        }
        if (!ents[i].tomb) {
            scan->live = (ScanEnt*)realloc(scan->live, (size_t)(scan->n + 1) * sizeof(ScanEnt));
            scan->live[scan->n++] = ents[i];
        } else {
            free(ents[i].key);
            free(ents[i].val);
        }
        i = j;
    }
    free(ents);
    return scan;
}

int Database_ScanNext(DatabaseScan* scan, uint8_t** key, int* keyLen, uint8_t** value, int* valueLen)
{
    ScanEnt* e;
    if (key) *key = NULL;
    if (keyLen) *keyLen = 0;
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!scan || scan->i >= scan->n) return 0;
    e = &scan->live[scan->i++];
    if (key) { *key = e->key; e->key = NULL; }
    else free(e->key);
    if (keyLen) *keyLen = e->keyLen;
    if (value) { *value = e->val; e->val = NULL; }
    else free(e->val);
    if (valueLen) *valueLen = e->valLen;
    return 1;
}

void Database_ScanDispose(DatabaseScan* scan)
{
    int i;
    if (!scan) return;
    for (i = 0; i < scan->n; i++) {
        free(scan->live[i].key);
        free(scan->live[i].val);
    }
    free(scan->live);
    free(scan);
}

int Database_SetOrder(Database* self, int tableId, const char* indexName)
{
    SpeedTableCursor* c;
    char mainName[128];
    int i, n;
    if (!self || !Manifest_TryGetTable(self->manifest, tableId, mainName, (int)sizeof(mainName)))
        return 0;
    c = db_get_cursor(self, tableId);
    if (!c) return 0;
    if (!indexName || !indexName[0] || gmb_strcasecmp(indexName, "Id") == 0) {
        snprintf(c->activeIndex, sizeof(c->activeIndex), "Id");
        return 1;
    }
    n = Manifest_SecondaryKeyCount(self->manifest, tableId);
    for (i = 0; i < n; i++) {
        char iname[64];
        if (!Manifest_GetSecondaryKey(self->manifest, tableId, i, iname, (int)sizeof(iname), NULL, 0, NULL))
            continue;
        if (gmb_strcasecmp(iname, indexName) == 0) {
            snprintf(c->activeIndex, sizeof(c->activeIndex), "%s", iname);
            return 1;
        }
    }
    return 0;
}

int Database_GoTop(Database* self, int tableId)
{
    SpeedTableCursor* c = db_get_cursor(self, tableId);
    int srcId = tableId, isIndex = 0;
    DatabaseScan* scan;
    uint8_t *k = NULL, *v = NULL;
    int kl = 0, vl = 0, ok;
    if (!c) return 0;
    if (!db_get_scan_source(self, tableId, c->activeIndex, &srcId, &isIndex))
        return db_move_empty(c);
    scan = Database_Scan(self, srcId);
    if (!scan || !Database_ScanNext(scan, &k, &kl, &v, &vl) || !k) {
        free(k); free(v);
        Database_ScanDispose(scan);
        return db_move_empty(c);
    }
    ok = db_cursor_set(c, k, kl, isIndex ? v : k, isIndex ? vl : kl);
    free(k); free(v);
    Database_ScanDispose(scan);
    return ok ? 1 : db_move_empty(c);
}

int Database_GoBottom(Database* self, int tableId)
{
    SpeedTableCursor* c = db_get_cursor(self, tableId);
    int srcId = tableId, isIndex = 0;
    DatabaseScan* scan;
    uint8_t *lastK = NULL, *lastV = NULL;
    int lastKl = 0, lastVl = 0, ok;
    if (!c) return 0;
    if (!db_get_scan_source(self, tableId, c->activeIndex, &srcId, &isIndex))
        return db_move_empty(c);
    scan = Database_Scan(self, srcId);
    if (!scan) return db_move_empty(c);
    {
        uint8_t *k = NULL, *v = NULL;
        int kl = 0, vl = 0;
        while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
            free(lastK); free(lastV);
            lastK = k; lastKl = kl;
            lastV = v; lastVl = vl;
            k = NULL; v = NULL;
        }
    }
    Database_ScanDispose(scan);
    if (!lastK) return db_move_empty(c);
    ok = db_cursor_set(c, lastK, lastKl, isIndex ? lastV : lastK, isIndex ? lastVl : lastKl);
    free(lastK); free(lastV);
    return ok ? 1 : db_move_empty(c);
}

int Database_Skip(Database* self, int tableId, int recordCount)
{
    SpeedTableCursor* c = db_get_cursor(self, tableId);
    int srcId = tableId, isIndex = 0, direction, toSkip, ok = 0;
    DatabaseScan* scan;
    uint8_t *destK = NULL, *destV = NULL;
    int destKl = 0, destVl = 0;
    if (!c || !c->indexKey || recordCount == 0) return 0;
    direction = recordCount > 0 ? 1 : -1;
    toSkip = recordCount > 0 ? recordCount : -recordCount;
    if (!db_get_scan_source(self, tableId, c->activeIndex, &srcId, &isIndex))
        return db_move_boundary(c, direction);
    scan = Database_Scan(self, srcId);
    if (!scan) return db_move_boundary(c, direction);
    if (direction > 0) {
        int seen = 0, remain = toSkip;
        uint8_t *k = NULL, *v = NULL;
        int kl = 0, vl = 0;
        while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
            if (!seen) {
                if (ByteArrayComparer_Compare(k, kl, c->indexKey, c->indexKeyLen) == 0)
                    seen = 1;
                free(k); free(v);
                continue;
            }
            remain--;
            free(destK); free(destV);
            destK = k; destKl = kl;
            destV = v; destVl = vl;
            k = NULL; v = NULL;
            if (remain == 0) break;
        }
        free(k); free(v);
        if (remain > 0) {
            free(destK); free(destV);
            destK = NULL;
        }
    } else {
        typedef struct { uint8_t *k, *v; int kl, vl; } SkipBuf;
        SkipBuf* buf = (SkipBuf*)calloc((size_t)toSkip, sizeof(SkipBuf));
        int n = 0, i;
        uint8_t *k = NULL, *v = NULL;
        int kl = 0, vl = 0;
        if (!buf) { Database_ScanDispose(scan); return 0; }
        while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
            if (ByteArrayComparer_Compare(k, kl, c->indexKey, c->indexKeyLen) == 0) {
                free(k); free(v);
                break;
            }
            if (n == toSkip) {
                free(buf[0].k); free(buf[0].v);
                memmove(&buf[0], &buf[1], (size_t)(toSkip - 1) * sizeof(buf[0]));
                n--;
            }
            buf[n].k = k; buf[n].kl = kl; buf[n].v = v; buf[n].vl = vl;
            n++;
            k = NULL; v = NULL;
        }
        if (n == toSkip) {
            destK = buf[0].k; destKl = buf[0].kl;
            destV = buf[0].v; destVl = buf[0].vl;
            buf[0].k = NULL; buf[0].v = NULL;
        }
        for (i = 0; i < n; i++) { free(buf[i].k); free(buf[i].v); }
        free(buf);
    }
    Database_ScanDispose(scan);
    if (!destK) return db_move_boundary(c, direction);
    ok = db_cursor_set(c, destK, destKl, isIndex ? destV : destK, isIndex ? destVl : destKl);
    free(destK); free(destV);
    return ok ? 1 : db_move_boundary(c, direction);
}

int Database_Seek(Database* self, int tableId, const uint8_t* key, int keyLen, const char* indexName)
{
    SpeedTableCursor* c = db_get_cursor(self, tableId);
    int srcId = tableId, isIndex = 0;
    uint8_t* found = NULL;
    int foundLen = 0;
    const char* order;
    if (!c || !key) return 0;
    order = (indexName && indexName[0]) ? indexName : c->activeIndex;
    if (!db_get_scan_source(self, tableId, order, &srcId, &isIndex)) {
        db_cursor_clear_pos(c);
        return 0;
    }
    if (!Database_Get(self, srcId, key, keyLen, &found, &foundLen) || !found) {
        db_cursor_clear_pos(c);
        return 0;
    }
    if (isIndex) {
        uint8_t* main = NULL;
        int mainLen = 0;
        if (!Database_Get(self, tableId, found, foundLen, &main, &mainLen)) {
            free(found);
            db_cursor_clear_pos(c);
            return 0;
        }
        free(main);
        if (!db_cursor_set(c, key, keyLen, found, foundLen)) {
            free(found);
            db_cursor_clear_pos(c);
            return 0;
        }
        free(found);
        return 1;
    }
    if (!db_cursor_set(c, key, keyLen, key, keyLen)) {
        free(found);
        db_cursor_clear_pos(c);
        return 0;
    }
    free(found);
    return 1;
}

int Database_GetCurrent(Database* self, int tableId, uint8_t** value, int* valueLen)
{
    SpeedTableCursor* c = db_cursor_find(self, tableId);
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!c || !c->pk) return 0;
    return Database_Get(self, tableId, c->pk, c->pkLen, value, valueLen);
}

int Database_Eof(const Database* self, int tableId)
{
    SpeedTableCursor* c = db_cursor_find(self, tableId);
    return c && c->eof;
}

int Database_Bof(const Database* self, int tableId)
{
    SpeedTableCursor* c = db_cursor_find(self, tableId);
    return c && c->bof;
}

int Database_DropView(Database* self, const char* viewName)
{
    return Database_DropTableInternal(self, viewName);
}

int Database_DropIndex(Database* self, int mainTableId, const char* indexName)
{
    char mainName[128], idxName[256];
    int idxId = 0;
    if (!self || !indexName || !indexName[0]) return GMB_DB_ARG;
    if (!Manifest_TryGetTable(self->manifest, mainTableId, mainName, (int)sizeof(mainName)))
        return GMB_DB_ERR;
    Manifest_IndexTableName(mainName, indexName, idxName, (int)sizeof(idxName));
    if (!Manifest_TryGetTableByName(self->manifest, idxName, &idxId) || idxId <= 0)
        return GMB_DB_ERR;
    if (Database_DropTableInternal(self, idxName) != GMB_DB_OK) return GMB_DB_ERR;
    Manifest_RemoveSecondaryIndex(self->manifest, mainName, indexName);
    Manifest_Save(self->manifest);
    return GMB_DB_OK;
}

void Database_SetIndexKeyEvaluator(Database* self, int mainTableId,
                                   SpeedExpressionEvaluator evaluator, void* evalUser)
{
    int i;
    if (!self || !evaluator) return;
    for (i = 0; i < self->indexEvalN; i++) {
        if (self->indexEvals[i].tableId != mainTableId) continue;
        self->indexEvals[i].ev = evaluator;
        self->indexEvals[i].user = evalUser;
        return;
    }
    if (self->indexEvalN >= SPEED_MAX_TABLES) return;
    self->indexEvals[self->indexEvalN].tableId = mainTableId;
    self->indexEvals[self->indexEvalN].ev = evaluator;
    self->indexEvals[self->indexEvalN].user = evalUser;
    self->indexEvalN++;
}

int Database_EvalIndexKey(Database* self, int tableId, const char* expression,
                          const uint8_t* udt, int udtLen, uint8_t** out, int* outLen)
{
    int i, n, c, slot = -1;
    QueryResultRow row;
    SpeedEvalValue val;
    if (out) *out = NULL;
    if (outLen) *outLen = 0;
    if (!self || !expression) return 0;
    for (i = 0; i < self->indexEvalN; i++) {
        if (self->indexEvals[i].tableId == tableId) { slot = i; break; }
    }
    if (slot < 0 || !self->indexEvals[slot].ev) return 0;
    memset(&row, 0, sizeof(row));
    memset(&val, 0, sizeof(val));
    n = Manifest_FieldCount(self->manifest, tableId);
    for (i = 0; i < n && row.CellN < QUERYPLAN_MAX_PROJ; i++) {
        char fname[64], tname[32];
        uint8_t* extracted = NULL;
        int elen = 0, flen = 0;
        if (!Manifest_GetField(self->manifest, tableId, i, fname, (int)sizeof(fname),
                               tname, (int)sizeof(tname), &flen))
            continue;
        (void)tname;
        if (udt && udtLen > 0)
            SpeedMarshaller_ExtractField(self->manifest, tableId, udt, udtLen, fname, &extracted, &elen);
        snprintf(row.Cells[row.CellN].Alias, sizeof(row.Cells[0].Alias), "%s", fname);
        row.Cells[row.CellN].Bytes = extracted;
        row.Cells[row.CellN].Len = elen;
        row.CellN++;
    }
    self->indexEvals[slot].ev(expression, &row, self->indexEvals[slot].user, &val);
    for (c = 0; c < row.CellN; c++) free(row.Cells[c].Bytes);
    if (val.isNull || !val.bytes || val.len <= 0) {
        SpeedEvalValue_Clear(&val);
        return 0;
    }
    if (!db_dup_bytes(val.bytes, val.len, out, outLen)) {
        SpeedEvalValue_Clear(&val);
        return 0;
    }
    SpeedEvalValue_Clear(&val);
    return 1;
}

void Database_MaybeRebuildUntrustedRowCounts(Database* self)
{
    int i, id = 0, n;
    if (!self || !self->manifest) return;
    n = Manifest_TableCount(self->manifest);
    for (i = 0; i < n; i++) {
        if (!Manifest_GetTableIdAt(self->manifest, i, &id)) continue;
        if (!Manifest_IsRowCountTrusted(self->manifest, id)) {
            Database_RebuildManifestRowCounts(self);
            return;
        }
    }
}

int Database_ForceCompactTable(Database* self, const char* tableName)
{
    int tableId = 0;
    if (!self || !tableName || !tableName[0]) return 0;
    if (!Manifest_TryGetTableByName(self->manifest, tableName, &tableId)) return -1;
    /* ponytail: C# also flushes secondary index tables; no index tables in this port. */
    Database_ForceFlushTableSync(self, tableId);
    return Compactor_CompactLevels01Forced(self, tableId);
}

int Database_CompactAll(Database* self)
{
    int i, n, id, reduced = 0, t, seen;
    if (!self || !self->manifest) return 0;
    n = Manifest_TableCount(self->manifest);
    for (i = 0; i < n; i++) {
        if (!Manifest_GetTableIdAt(self->manifest, i, &id) || id <= 0) continue;
        Database_ForceFlushTableSync(self, id);
        reduced += Compactor_CompactLevels01Forced(self, id);
    }
    for (i = 0; i < self->memN; i++) {
        id = self->mems[i].tableId;
        seen = 0;
        for (t = 0; t < n; t++) {
            int mid = 0;
            if (Manifest_GetTableIdAt(self->manifest, t, &mid) && mid == id) {
                seen = 1;
                break;
            }
        }
        if (seen) continue;
        Database_ForceFlushTableSync(self, id);
        reduced += Compactor_CompactLevels01Forced(self, id);
    }
    return reduced;
}

void Database_FlushDurability(Database* self)
{
    if (self) db_flush_all_wals(self);
}

void Database_BeginGroupCommit(Database* self)
{
    if (self) self->groupCommitDepth++;
}

int Database_EndGroupCommit(Database* self)
{
    if (!self || self->groupCommitDepth == 0) return GMB_DB_ERR;
    self->groupCommitDepth--;
    if (self->groupCommitDepth == 0 && self->durabilityFull)
        db_flush_all_wals(self);
    return GMB_DB_OK;
}

int Database_SetMany(Database* self, int tableId, const DatabaseKv* entries, int count)
{
    SpeedWriteAheadLog* wal;
    Memtable* mem;
    int i;
    if (!self) return GMB_DB_ARG;
    if (count <= 0) return GMB_DB_OK;
    if (!entries) return GMB_DB_ARG;
    if (self->inTx) {
        mem = db_tx_buffer(self, tableId);
        if (!mem) return GMB_DB_ERR;
        for (i = 0; i < count; i++) {
            if (Memtable_Set(mem, entries[i].key, entries[i].keyLen, entries[i].value, entries[i].valueLen) != GMB_DB_OK)
                return GMB_DB_ERR;
        }
        return GMB_DB_OK;
    }
    wal = db_get_or_create_wal(self, tableId);
    mem = db_get_or_create_memtable(self, tableId);
    if (!wal || !mem) return GMB_DB_ERR;
    self->batchWalSuppressDepth++;
    for (i = 0; i < count; i++) {
        uint8_t* old = NULL;
        int oldLen = 0;
        if (Database_IsMainDataTable(self, tableId)
            && Manifest_SecondaryKeyCount(self->manifest, tableId) > 0)
            Database_Get(self, tableId, entries[i].key, entries[i].keyLen, &old, &oldLen);
        SpeedWriteAheadLog_LogSet(wal, tableId, entries[i].key, entries[i].keyLen, entries[i].value, entries[i].valueLen);
        if (Memtable_Set(mem, entries[i].key, entries[i].keyLen, entries[i].value, entries[i].valueLen) != GMB_DB_OK) {
            self->batchWalSuppressDepth--;
            free(old);
            return GMB_DB_ERR;
        }
        Database_MaintainSecondaryIndexesOnPut(self, tableId, entries[i].key, entries[i].keyLen,
                                               old, oldLen, entries[i].value, entries[i].valueLen);
        free(old);
    }
    self->batchWalSuppressDepth--;
    if (self->batchWalSuppressDepth == 0 && db_should_fsync(self))
        db_flush_all_wals(self);
    db_maybe_flush_and_compact(self, tableId);
    return GMB_DB_OK;
}

int Database_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/speedsc_XXXXXX";
    Database* db;
    int tableId, vlen, found;
    uint8_t *val, key[8], value[8];

    if (!mkdtemp(dir)) return -1;
    db = Database_Open(dir);
    if (!db) return -2;
    tableId = Database_CreateTable(db, "Person");
    if (tableId <= 0) { Database_Dispose(db); return -3; }

    snprintf((char*)key, sizeof(key), "%s", "k001");
    snprintf((char*)value, sizeof(value), "%s", "v001");
    if (Database_Set(db, tableId, key, 4, value, 4) != GMB_DB_OK) { Database_Dispose(db); return -4; }
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v001", 4) != 0) { free(val); Database_Dispose(db); return -5; }
    free(val);
    Database_Dispose(db);

    db = Database_Open(dir);
    if (!db) return -6;
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v001", 4) != 0) { free(val); Database_Dispose(db); return -7; }
    free(val);

    Database_ForceFlushTableSync(db, tableId);
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v001", 4) != 0) { free(val); Database_Dispose(db); return -8; }
    free(val);
    Database_Dispose(db);

    db = Database_Open(dir);
    if (!db) return -9;
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v001", 4) != 0) { free(val); Database_Dispose(db); return -10; }
    free(val);

    if (Database_Delete(db, tableId, key, 4) != GMB_DB_OK) { Database_Dispose(db); return -11; }
    if (Database_Get(db, tableId, key, 4, &val, &vlen)) { free(val); Database_Dispose(db); return -12; }

    snprintf((char*)key, sizeof(key), "%s", "k002");
    snprintf((char*)value, sizeof(value), "%s", "v002");
    if (Database_Set(db, tableId, key, 4, value, 4) != GMB_DB_OK) { Database_Dispose(db); return -13; }
    Database_ForceFlushTableSync(db, tableId);
    Database_Dispose(db);

    db = Database_Open(dir);
    if (!db) return -14;
    found = Database_Get(db, tableId, (const uint8_t*)"k001", 4, &val, &vlen);
    if (found) { free(val); Database_Dispose(db); return -15; }
    found = Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v002", 4) != 0) { free(val); Database_Dispose(db); return -16; }
    free(val);

    Database_BeginTransaction(db);
    snprintf((char*)key, sizeof(key), "%s", "k003");
    snprintf((char*)value, sizeof(value), "%s", "v003");
    if (Database_Set(db, tableId, key, 4, value, 4) != GMB_DB_OK) { Database_Dispose(db); return -17; }
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -18; }
    free(val);
    Database_Rollback(db);
    if (Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -19; }

    Database_BeginTransaction(db);
    if (Database_Set(db, tableId, (const uint8_t*)"k003", 4, (const uint8_t*)"v003", 4) != GMB_DB_OK) {
        Database_Dispose(db); return -20;
    }
    if (Database_Delete(db, tableId, (const uint8_t*)"k002", 4) != GMB_DB_OK) { Database_Dispose(db); return -21; }
    if (Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -22; }
    if (Database_Commit(db) != GMB_DB_OK) { Database_Dispose(db); return -23; }
    if (Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -24; }
    found = Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen);
    if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -25; }
    free(val);

    {
        DatabaseScan* scan = Database_Scan(db, tableId);
        uint8_t *sk = NULL, *sv = NULL;
        int skl = 0, svl = 0, seen = 0;
        char got[8][8];
        int gn = 0;
        if (!scan) { Database_Dispose(db); return -26; }
        while (Database_ScanNext(scan, &sk, &skl, &sv, &svl)) {
            if (gn < 8 && skl < 8) {
                memcpy(got[gn], sk, (size_t)skl);
                got[gn][skl] = 0;
                gn++;
            }
            seen++;
            free(sk);
            free(sv);
        }
        Database_ScanDispose(scan);
        if (seen != 2 || gn != 2 || strcmp(got[0], "k001") != 0 || strcmp(got[1], "k003") != 0) {
            Database_Dispose(db); return -27;
        }
    }

    Database_Dispose(db);
    db = Database_Open(dir);
    if (!db) return -28;
    if (Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -29; }
    found = Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen);
    if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -30; }
    free(val);

    {
        int reduced, n0, n1;
        char names[8][256];
        DatabaseScan* scan;
        uint8_t *sk = NULL, *sv = NULL;
        int skl = 0, svl = 0, seen = 0;
        char got[8];
        reduced = Database_ForceCompactTable(db, "Person");
        if (reduced < 1) { Database_Dispose(db); return -31; }
        if (Database_Get(db, tableId, (const uint8_t*)"k001", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -32; }
        if (Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -33; }
        found = Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen);
        if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -34; }
        free(val);
        n0 = Manifest_GetFilesForLevel(db->manifest, tableId, 0, names, 8);
        n1 = Manifest_GetFilesForLevel(db->manifest, tableId, 1, names, 8);
        if (n0 != 0 || n1 != 1) { Database_Dispose(db); return -35; }
        scan = Database_Scan(db, tableId);
        if (!scan) { Database_Dispose(db); return -36; }
        got[0] = 0;
        while (Database_ScanNext(scan, &sk, &skl, &sv, &svl)) {
            seen++;
            if (skl < 8) { memcpy(got, sk, (size_t)skl); got[skl] = 0; }
            free(sk);
            free(sv);
        }
        Database_ScanDispose(scan);
        if (seen != 1 || strcmp(got, "k003") != 0) { Database_Dispose(db); return -37; }
    }

    Database_Dispose(db);
    db = Database_Open(dir);
    if (!db) return -38;
    if (Database_Get(db, tableId, (const uint8_t*)"k001", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -39; }
    found = Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen);
    if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -40; }
    free(val);

    {
        DatabaseKv batch[3];
        uint8_t k0[4] = { 'k', '0', '1', '0' }, v0[4] = { 'v', '0', '1', '0' };
        uint8_t k1[4] = { 'k', '0', '1', '1' }, v1[4] = { 'v', '0', '1', '1' };
        uint8_t k2[4] = { 'k', '0', '1', '2' }, v2[4] = { 'v', '0', '1', '2' };
        batch[0].key = k0; batch[0].keyLen = 4; batch[0].value = v0; batch[0].valueLen = 4;
        batch[1].key = k1; batch[1].keyLen = 4; batch[1].value = v1; batch[1].valueLen = 4;
        batch[2].key = k2; batch[2].keyLen = 4; batch[2].value = v2; batch[2].valueLen = 4;
        if (Database_SetMany(db, tableId, batch, 3) != GMB_DB_OK) { Database_Dispose(db); return -41; }
        found = Database_Get(db, tableId, k0, 4, &val, &vlen);
        if (!found || memcmp(val, v0, 4) != 0) { free(val); Database_Dispose(db); return -42; }
        free(val);
        found = Database_Get(db, tableId, k2, 4, &val, &vlen);
        if (!found || memcmp(val, v2, 4) != 0) { free(val); Database_Dispose(db); return -43; }
        free(val);
        if (Database_SetMany(db, tableId, batch, 0) != GMB_DB_OK) { Database_Dispose(db); return -44; }
        if (Database_EndGroupCommit(db) != GMB_DB_ERR) { Database_Dispose(db); return -45; }
        Database_BeginGroupCommit(db);
        Database_BeginGroupCommit(db);
        if (Database_Set(db, tableId, (const uint8_t*)"k020", 4, (const uint8_t*)"v020", 4) != GMB_DB_OK) {
            Database_Dispose(db); return -46;
        }
        if (Database_EndGroupCommit(db) != GMB_DB_OK) { Database_Dispose(db); return -47; }
        if (Database_EndGroupCommit(db) != GMB_DB_OK) { Database_Dispose(db); return -48; }
        found = Database_Get(db, tableId, (const uint8_t*)"k020", 4, &val, &vlen);
        if (!found || memcmp(val, "v020", 4) != 0) { free(val); Database_Dispose(db); return -49; }
        free(val);
    }

    Database_Dispose(db);
    db = Database_Open(dir);
    if (!db) return -50;
    found = Database_Get(db, tableId, (const uint8_t*)"k010", 4, &val, &vlen);
    if (!found || memcmp(val, "v010", 4) != 0) { free(val); Database_Dispose(db); return -51; }
    free(val);
    found = Database_Get(db, tableId, (const uint8_t*)"k020", 4, &val, &vlen);
    if (!found || memcmp(val, "v020", 4) != 0) { free(val); Database_Dispose(db); return -52; }
    free(val);
    Database_Dispose(db);
    return GMB_DB_OK;
}
