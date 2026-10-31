#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

/* C#: internal class WriteAheadLog */

static int wr_u8(FILE* f, uint8_t v) { return fwrite(&v, 1, 1, f) == 1 ? 0 : -1; }
static int wr_i32(FILE* f, int32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}
static int wr_i64(FILE* f, int64_t v)
{
    uint8_t b[8];
    int i;
    for (i = 0; i < 8; i++) b[i] = (uint8_t)((uint64_t)v >> (8 * i));
    return fwrite(b, 1, 8, f) == 8 ? 0 : -1;
}
static int wr_bytes(FILE* f, const uint8_t* p, int n)
{
    if (n <= 0) return 0;
    return fwrite(p, 1, (size_t)n, f) == (size_t)n ? 0 : -1;
}
static int rd_u8(FILE* f, uint8_t* v) { return fread(v, 1, 1, f) == 1 ? 0 : -1; }
static int rd_i32(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}
static int rd_i64(FILE* f, int64_t* v)
{
    uint8_t b[8];
    uint64_t u = 0;
    int i;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)b[i] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

typedef struct WalLockEntry {
    char path[PATH_MAX];
    pthread_mutex_t intra;
    int lockFd;
    struct WalLockEntry* next;
} WalLockEntry;

static pthread_mutex_t g_walTableMu = PTHREAD_MUTEX_INITIALIZER;
static WalLockEntry* g_walTable;

static void wal_change_ext(const char* db, char* out, size_t cap)
{
    size_t n = strlen(db);
    const char* dot = strrchr(db, '.');
    const char* slash = strrchr(db, '/');
    if (dot && (!slash || dot > slash))
        n = (size_t)(dot - db);
    snprintf(out, cap, "%.*s.wal", (int)n, db);
}

static WalLockEntry* wal_get_locks(const char* walPath)
{
    WalLockEntry* e;
    char lockPath[PATH_MAX];
    pthread_mutex_lock(&g_walTableMu);
    for (e = g_walTable; e; e = e->next) {
        if (gmb_strcasecmp(e->path, walPath) == 0) {
            pthread_mutex_unlock(&g_walTableMu);
            return e;
        }
    }
    e = (WalLockEntry*)calloc(1, sizeof(*e));
    snprintf(e->path, sizeof(e->path), "%s", walPath);
    pthread_mutex_init(&e->intra, NULL);
    snprintf(lockPath, sizeof(lockPath), "%s.xlock", walPath);
    e->lockFd = open(lockPath, O_RDWR | O_CREAT, 0666);
    e->next = g_walTable;
    g_walTable = e;
    pthread_mutex_unlock(&g_walTableMu);
    return e;
}

struct WriteAheadLog {
    char walFilePath[PATH_MAX];
    FILE* logStream;
    WalLockEntry* locks;
    int disposed;
};

static void WriteAheadLog_EnsureOpen(WriteAheadLog* self)
{
    if (!self->disposed) return;
    self->logStream = fopen(self->walFilePath, "ab+");
    self->disposed = 0;
}

static void WriteAheadLog_FlushInternal(WriteAheadLog* self)
{
    if (!self->logStream) return;
    fflush(self->logStream);
    fsync(fileno(self->logStream));
}

WriteAheadLog* WriteAheadLog_New(const char* dbFilePath)
{
    WriteAheadLog* self = (WriteAheadLog*)calloc(1, sizeof(*self));
    wal_change_ext(dbFilePath, self->walFilePath, sizeof(self->walFilePath));
    self->locks = wal_get_locks(self->walFilePath);
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    self->logStream = fopen(self->walFilePath, "ab+");
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
    return self;
}

void WriteAheadLog_LogBeginTransaction(WriteAheadLog* self)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_BeginTransaction);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogTransactionSnapshot(WriteAheadLog* self, const uint8_t* headerData, int headerLen,
                                          const uint8_t* indexMapData, int mapLen)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_TransactionSnapshot);
    wr_i32(self->logStream, headerLen);
    wr_bytes(self->logStream, headerData, headerLen);
    wr_i32(self->logStream, mapLen);
    wr_bytes(self->logStream, indexMapData, mapLen);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogCommitTransaction(WriteAheadLog* self)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_CommitTransaction);
    WriteAheadLog_FlushInternal(self);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogWriteRecordSpan(WriteAheadLog* self, int64_t offset, const uint8_t* recordData, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_WriteRecord);
    wr_i64(self->logStream, offset);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, recordData, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogWriteRecord(WriteAheadLog* self, int64_t offset, const uint8_t* recordData, int len)
{
    WriteAheadLog_LogWriteRecordSpan(self, offset, recordData, len);
}

void WriteAheadLog_LogWriteNode(WriteAheadLog* self, int64_t offset, const uint8_t* nodeData, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_WriteNode);
    wr_i64(self->logStream, offset);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, nodeData, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogUpdateHeader(WriteAheadLog* self, const uint8_t* headerData, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_UpdateHeader);
    wr_i64(self->logStream, 0);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, headerData, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogWriteBytes(WriteAheadLog* self, int64_t offset, const uint8_t* data, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_WriteBytes);
    wr_i64(self->logStream, offset);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, data, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogDirectWrite(WriteAheadLog* self, int64_t offset, const uint8_t* data, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_DirectWrite);
    wr_i64(self->logStream, offset);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, data, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogDirectCheckpoint(WriteAheadLog* self)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_DirectCheckpoint);
    WriteAheadLog_FlushInternal(self);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_Flush(WriteAheadLog* self)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    WriteAheadLog_FlushInternal(self);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_Dispose(WriteAheadLog* self)
{
    if (!self || self->disposed) return;
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_FlushInternal(self);
    if (self->logStream) { fclose(self->logStream); self->logStream = NULL; }
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
    self->disposed = 1;
}

void WriteAheadLog_Delete(WriteAheadLog* self, int skipFlush)
{
    if (!self) return;
    if (self->disposed) {
        unlink(self->walFilePath);
        return;
    }
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    if (!self->disposed) {
        if (!skipFlush) WriteAheadLog_FlushInternal(self);
        if (self->logStream) { fclose(self->logStream); self->logStream = NULL; }
        self->disposed = 1;
        unlink(self->walFilePath);
    }
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

static void wal_empty_summary(WalLogSummary* s, const char* db, const char* wal, int exists, int64_t size)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->DatabasePath, sizeof(s->DatabasePath), "%s", db);
    snprintf(s->WalPath, sizeof(s->WalPath), "%s", wal);
    s->Exists = exists;
    s->SizeBytes = size;
    s->WalHandleActive = -1;
    s->DirectWalDirty = -1;
}

int WriteAheadLog_Inspect(const char* dbFilePath, WalLogSummary* out)
{
    char walPath[PATH_MAX];
    struct stat st;
    FILE* f;
    int committedBlocks = 0, activeTransactionOps = 0, directWrites = 0, pendingRecords = 0;
    int inTransaction = 0, endsWithCheckpoint = 0;
    int64_t lastPosition;
    if (!dbFilePath || !dbFilePath[0] || !out) return GMB_DB_ARG;
    wal_change_ext(dbFilePath, walPath, sizeof(walPath));
    if (stat(walPath, &st) != 0) {
        wal_empty_summary(out, dbFilePath, walPath, 0, 0);
        return GMB_DB_OK;
    }
    if (st.st_size == 0) {
        wal_empty_summary(out, dbFilePath, walPath, 1, 0);
        return GMB_DB_OK;
    }
    f = fopen(walPath, "rb");
    if (!f) return GMB_DB_ERR;
    lastPosition = st.st_size;
    for (;;) {
        uint8_t op;
        long pos = ftell(f);
        if (pos < 0 || pos >= st.st_size) break;
        if (rd_u8(f, &op) != 0) { lastPosition = pos; break; }
        switch ((WalOperationType)op) {
        case WalOperationType_TransactionSnapshot: {
            int32_t headerLen, mapLen;
            if (rd_i32(f, &headerLen) != 0) goto done;
            if (headerLen < 0 || headerLen > st.st_size - ftell(f)) goto done;
            fseek(f, headerLen, SEEK_CUR);
            if (rd_i32(f, &mapLen) != 0) goto done;
            if (mapLen < 0 || mapLen > st.st_size - ftell(f)) goto done;
            fseek(f, mapLen, SEEK_CUR);
            break;
        }
        case WalOperationType_BeginTransaction:
            activeTransactionOps = 0;
            inTransaction = 1;
            break;
        case WalOperationType_CommitTransaction:
            if (inTransaction) {
                committedBlocks++;
                activeTransactionOps = 0;
                inTransaction = 0;
            }
            break;
        case WalOperationType_DirectCheckpoint:
            directWrites = 0;
            endsWithCheckpoint = 1;
            break;
        case WalOperationType_WriteRecord:
        case WalOperationType_WriteNode:
        case WalOperationType_UpdateHeader:
        case WalOperationType_WriteBytes:
        case WalOperationType_DirectWrite: {
            int64_t offset;
            int32_t length;
            if (rd_i64(f, &offset) != 0) goto done;
            if (rd_i32(f, &length) != 0) goto done;
            if (length < 0 || length > st.st_size - ftell(f)) goto done;
            fseek(f, length, SEEK_CUR);
            pendingRecords++;
            if (inTransaction) activeTransactionOps++;
            else if (op == WalOperationType_DirectWrite) {
                directWrites++;
                endsWithCheckpoint = 0;
            }
            (void)offset;
            break;
        }
        default:
            break;
        }
    }
done:
    fclose(f);
    wal_empty_summary(out, dbFilePath, walPath, 1, st.st_size);
    out->LastPosition = lastPosition;
    out->PendingRecords = pendingRecords;
    out->UncommittedTransactionOps = activeTransactionOps;
    out->CommittedTransactionBlocks = committedBlocks;
    out->DirectWritesPending = directWrites;
    out->EndsWithCheckpoint = endsWithCheckpoint;
    out->InOpenTransaction = inTransaction;
    return GMB_DB_OK;
}

typedef struct {
    uint8_t op;
    int64_t offset;
    int32_t len;
    uint8_t* data;
} WalLoggedOp;

static void RecoverCore(const char* dbFilePath, const char* walPath)
{
    struct stat st;
    FILE *wal, *db;
    WalLoggedOp* active = NULL;
    int activeN = 0, activeCap = 0;
    WalLoggedOp** committed = NULL;
    int* committedCounts = NULL;
    int committedN = 0, committedCap = 0;
    WalLoggedOp* directs = NULL;
    int directsN = 0, directsCap = 0;
    int inTransaction = 0, endsWithDirectCheckpoint = 0;
    uint8_t *pendingSnapH = NULL, *pendingSnapM = NULL, *openSnapH = NULL, *openSnapM = NULL;
    int pendingSnapHLen = 0, pendingSnapMLen = 0, openSnapHLen = 0, openSnapMLen = 0;
    int i, t;

    if (stat(walPath, &st) != 0) return;
    if (st.st_size == 0) { unlink(walPath); return; }

    wal = fopen(walPath, "rb");
    if (!wal) return;
    for (;;) {
        uint8_t op;
        long pos = ftell(wal);
        if (pos < 0 || pos >= st.st_size) break;
        if (rd_u8(wal, &op) != 0) break;
        switch ((WalOperationType)op) {
        case WalOperationType_TransactionSnapshot: {
            int32_t headerLen, mapLen;
            if (rd_i32(wal, &headerLen) != 0) goto parse_done;
            if (headerLen < 0 || headerLen > st.st_size - ftell(wal)) goto parse_done;
            free(pendingSnapH);
            pendingSnapH = (uint8_t*)malloc((size_t)headerLen);
            pendingSnapHLen = headerLen;
            if (headerLen > 0 && fread(pendingSnapH, 1, (size_t)headerLen, wal) != (size_t)headerLen) goto parse_done;
            if (rd_i32(wal, &mapLen) != 0) goto parse_done;
            if (mapLen < 0 || mapLen > st.st_size - ftell(wal)) goto parse_done;
            free(pendingSnapM);
            pendingSnapM = (uint8_t*)malloc((size_t)mapLen);
            pendingSnapMLen = mapLen;
            if (mapLen > 0 && fread(pendingSnapM, 1, (size_t)mapLen, wal) != (size_t)mapLen) goto parse_done;
            break;
        }
        case WalOperationType_BeginTransaction:
            for (i = 0; i < activeN; i++) free(active[i].data);
            activeN = 0;
            inTransaction = 1;
            free(openSnapH); free(openSnapM);
            openSnapH = pendingSnapH; openSnapHLen = pendingSnapHLen; pendingSnapH = NULL; pendingSnapHLen = 0;
            openSnapM = pendingSnapM; openSnapMLen = pendingSnapMLen; pendingSnapM = NULL; pendingSnapMLen = 0;
            break;
        case WalOperationType_CommitTransaction:
            if (inTransaction) {
                if (committedN >= committedCap) {
                    committedCap = committedCap ? committedCap * 2 : 4;
                    committed = (WalLoggedOp**)realloc(committed, (size_t)committedCap * sizeof(*committed));
                    committedCounts = (int*)realloc(committedCounts, (size_t)committedCap * sizeof(int));
                }
                committed[committedN] = (WalLoggedOp*)malloc((size_t)activeN * sizeof(WalLoggedOp));
                memcpy(committed[committedN], active, (size_t)activeN * sizeof(WalLoggedOp));
                committedCounts[committedN] = activeN;
                committedN++;
                activeN = 0;
                inTransaction = 0;
                free(openSnapH); free(openSnapM);
                openSnapH = openSnapM = NULL;
                openSnapHLen = openSnapMLen = 0;
            }
            break;
        case WalOperationType_DirectCheckpoint:
            for (i = 0; i < directsN; i++) free(directs[i].data);
            directsN = 0;
            endsWithDirectCheckpoint = 1;
            break;
        case WalOperationType_WriteRecord:
        case WalOperationType_WriteNode:
        case WalOperationType_UpdateHeader:
        case WalOperationType_WriteBytes:
        case WalOperationType_DirectWrite: {
            int64_t offset;
            int32_t length;
            WalLoggedOp rec;
            if (rd_i64(wal, &offset) != 0) goto parse_done;
            if (rd_i32(wal, &length) != 0) goto parse_done;
            if (length < 0 || length > st.st_size - ftell(wal)) goto parse_done;
            rec.op = op;
            rec.offset = offset;
            rec.len = length;
            rec.data = (uint8_t*)malloc((size_t)length);
            if (length > 0 && fread(rec.data, 1, (size_t)length, wal) != (size_t)length) {
                free(rec.data);
                goto parse_done;
            }
            if (inTransaction) {
                if (activeN >= activeCap) {
                    activeCap = activeCap ? activeCap * 2 : 8;
                    active = (WalLoggedOp*)realloc(active, (size_t)activeCap * sizeof(*active));
                }
                active[activeN++] = rec;
            } else if (op == WalOperationType_DirectWrite) {
                if (directsN >= directsCap) {
                    directsCap = directsCap ? directsCap * 2 : 8;
                    directs = (WalLoggedOp*)realloc(directs, (size_t)directsCap * sizeof(*directs));
                }
                directs[directsN++] = rec;
                endsWithDirectCheckpoint = 0;
            } else {
                free(rec.data);
            }
            break;
        }
        default:
            break;
        }
    }
parse_done:
    fclose(wal);

    db = fopen(dbFilePath, "rb+");
    if (db) {
        for (t = 0; t < committedN; t++) {
            for (i = 0; i < committedCounts[t]; i++) {
                fseek(db, committed[t][i].offset, SEEK_SET);
                fwrite(committed[t][i].data, 1, (size_t)committed[t][i].len, db);
            }
        }
        if (!endsWithDirectCheckpoint) {
            for (i = 0; i < directsN; i++) { free(directs[i].data); directs[i].data = NULL; }
            directsN = 0;
        } else {
            for (i = 0; i < directsN; i++) {
                fseek(db, directs[i].offset, SEEK_SET);
                fwrite(directs[i].data, 1, (size_t)directs[i].len, db);
            }
        }
        if (inTransaction && openSnapHLen > 0 && openSnapMLen > 0) {
            fseek(db, 0, SEEK_SET);
            fwrite(openSnapH, 1, (size_t)openSnapHLen, db);
            if (openSnapHLen >= 44) {
                int64_t mapOffset = 0;
                int k;
                for (k = 0; k < 8; k++) mapOffset |= (int64_t)openSnapH[36 + k] << (8 * k);
                if (mapOffset > 0) {
                    fseek(db, mapOffset, SEEK_SET);
                    fwrite(openSnapM, 1, (size_t)openSnapMLen, db);
                }
            }
        }
        fflush(db);
        fsync(fileno(db));
        fclose(db);
    }

    for (t = 0; t < committedN; t++) {
        for (i = 0; i < committedCounts[t]; i++) free(committed[t][i].data);
        free(committed[t]);
    }
    free(committed); free(committedCounts);
    for (i = 0; i < activeN; i++) free(active[i].data);
    free(active);
    for (i = 0; i < directsN; i++) free(directs[i].data);
    free(directs);
    free(pendingSnapH); free(pendingSnapM); free(openSnapH); free(openSnapM);
    unlink(walPath);
}

void WriteAheadLog_Recover(const char* dbFilePath)
{
    char walPath[PATH_MAX];
    WalLockEntry* locks;
    wal_change_ext(dbFilePath, walPath, sizeof(walPath));
    locks = wal_get_locks(walPath);
    flock(locks->lockFd, LOCK_EX);
    RecoverCore(dbFilePath, walPath);
    flock(locks->lockFd, LOCK_UN);
}

int WriteAheadLog_SelfCheck(void)
{
    char dir[] = "/tmp/gmb_wal_selfcheck_XXXXXX";
    char db[PATH_MAX], wal[PATH_MAX];
    uint8_t payload[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t verify[8];
    FILE* f;
    WriteAheadLog* w;
    WalLogSummary s;
    if (!mkdtemp(dir)) return GMB_DB_ERR;
    snprintf(db, sizeof(db), "%s/t.dbf", dir);
    f = fopen(db, "wb");
    if (!f) return GMB_DB_ERR;
    fwrite("XXXXXXXX", 1, 8, f);
    fclose(f);
    w = WriteAheadLog_New(db);
    WriteAheadLog_LogBeginTransaction(w);
    WriteAheadLog_LogWriteRecord(w, 0, payload, 8);
    WriteAheadLog_LogCommitTransaction(w);
    WriteAheadLog_Dispose(w);
    free(w);
    wal_change_ext(db, wal, sizeof(wal));
    WriteAheadLog_Inspect(db, &s);
    if (!s.Exists || s.CommittedTransactionBlocks != 1) return GMB_DB_ERR;
    WriteAheadLog_Recover(db);
    f = fopen(db, "rb");
    if (!f || fread(verify, 1, 8, f) != 8) return GMB_DB_ERR;
    fclose(f);
    if (memcmp(verify, payload, 8) != 0) return GMB_DB_ERR;
    if (access(wal, F_OK) == 0) return GMB_DB_ERR;
    return GMB_DB_OK;
}
