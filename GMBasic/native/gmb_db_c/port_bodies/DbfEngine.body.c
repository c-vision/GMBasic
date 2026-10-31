#include <unistd.h>
#include <limits.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pthread.h>

/* C#: DbfEngine ctor + memo + IndexFactory + record path (REPLACE/WAL/tx/CopyTo). */

#define GMB_DBF_ENGINE_KEY_MAX 256

struct DbfEngine {
    DbfTxHandle* tx;
    DbfFile* dbfFile;
    MemoFile* memoFile;
    IndexHandle indexes[GMB_DBF_ENGINE_MAX_INDEXES];
    int indexCount;
    int activeOrder; /* -1 = ordine fisico */
    LockingStrategy lockingStrategy;
    int exclusive;
    int deletedVisible; /* C# default false = SET DELETED ON */
    int exact;
    int currentRecordNumber;
    int currentIndexInLeaf;
    int eof;
    int bof;
    int found;
    int txNest;
    char filterExpr[512];
    VersioningManager* ver;
    int64_t lastReadRecordVersion;
    DbfReplicationLog* repl;
    char pendingRepl[32][768];
    int pendingReplN;
};

static int eng_ext_is(const char* path, const char* want)
{
    const char* slash;
    const char* base;
    const char* dot;
    char buf[8];
    size_t i, n;
    if (!path || !want) return 0;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    if (!dot) return 0;
    n = strlen(dot);
    if (n == 0 || n >= sizeof(buf)) return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)dot[i];
        buf[i] = (char)((c >= 'A' && c <= 'Z') ? (c + 32) : c);
    }
    buf[n] = '\0';
    return strcmp(buf, want) == 0;
}

static void eng_stem(const char* path, char* out, int cap)
{
    const char* slash;
    const char* base;
    const char* dot;
    size_t n;
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!path) return;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    n = dot ? (size_t)(dot - base) : strlen(base);
    if ((int)n >= cap) n = (size_t)(cap - 1);
    memcpy(out, base, n);
    out[n] = 0;
}

static void DbfEngine_UpdateBofEofStatus(DbfEngine* self)
{
    uint32_t n;
    if (!self || !self->dbfFile) return;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if ((uint32_t)self->currentRecordNumber > n) {
        self->eof = 1;
        self->currentRecordNumber = (int)n + 1;
    } else if (self->currentRecordNumber < 1) {
        self->bof = 1;
        self->currentRecordNumber = 0;
    } else {
        self->eof = 0;
        self->bof = 0;
    }
}

void DbfEngine_GoTop(DbfEngine* self)
{
    int rec;
    if (!self) return;
    self->bof = 0;
    self->eof = 0;
    self->currentIndexInLeaf = 0;
    if (self->activeOrder >= 0 && self->activeOrder < self->indexCount) {
        rec = IndexHandle_GetFirstRecordNumber(&self->indexes[self->activeOrder]);
        self->currentRecordNumber = rec;
    } else {
        self->currentRecordNumber = 1;
    }
    DbfEngine_UpdateBofEofStatus(self);
}

void DbfEngine_GoBottom(DbfEngine* self)
{
    int rec, n;
    if (!self) return;
    self->bof = 0;
    self->eof = 0;
    if (self->activeOrder >= 0 && self->activeOrder < self->indexCount) {
        rec = IndexHandle_GetLastRecordNumber(&self->indexes[self->activeOrder]);
        n = IndexHandle_GetEntryCount(&self->indexes[self->activeOrder]);
        self->currentIndexInLeaf = n > 0 ? n - 1 : 0;
        self->currentRecordNumber = rec;
    } else {
        self->currentRecordNumber = (int)DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
        self->currentIndexInLeaf = 0;
    }
    DbfEngine_UpdateBofEofStatus(self);
}

static void DbfEngine_Step(DbfEngine* self, int direction)
{
    IndexHandle* h;
    int n, rec;
    uint32_t total;
    if (!self || direction == 0) return;
    total = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (self->activeOrder >= 0 && self->activeOrder < self->indexCount) {
        h = &self->indexes[self->activeOrder];
        n = IndexHandle_GetEntryCount(h);
        if (n > 0) {
            self->currentIndexInLeaf += direction;
            if (self->currentIndexInLeaf < 0) {
                self->currentRecordNumber = 0;
            } else if (self->currentIndexInLeaf >= n) {
                self->currentRecordNumber = (int)total + 1;
            } else {
                rec = IndexHandle_GetRecnoAt(h, self->currentIndexInLeaf);
                self->currentRecordNumber = rec;
            }
            DbfEngine_UpdateBofEofStatus(self);
            return;
        }
    }
    self->currentRecordNumber += direction;
    DbfEngine_UpdateBofEofStatus(self);
}

void DbfEngine_Skip(DbfEngine* self, int recordsToSkip)
{
    int i, dir, absn;
    if (!self || recordsToSkip == 0) return;
    if ((self->eof && recordsToSkip > 0) || (self->bof && recordsToSkip < 0)) return;
    if (self->eof && recordsToSkip < 0) self->eof = 0;
    if (self->bof && recordsToSkip > 0) self->bof = 0;
    dir = recordsToSkip > 0 ? 1 : -1;
    absn = recordsToSkip > 0 ? recordsToSkip : -recordsToSkip;
    for (i = 0; i < absn; i++) {
        DbfEngine_Step(self, dir);
        if (self->bof || self->eof) break;
    }
}

int DbfEngine_IsCurrentRecordValid(const DbfEngine* self)
{
    int isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;
    if (!self || self->bof || self->eof) return 0;
    if (self->deletedVisible) return 1;
    if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &raw, &rawLen) != GMB_DB_OK)
        return 0;
    free(raw);
    return isDeleted ? 0 : 1;
}

void DbfEngine_SkipInternal(DbfEngine* self, int recordsToSkip)
{
    int dir, absn, skipped;
    if (!self || recordsToSkip == 0) return;
    if ((self->eof && recordsToSkip > 0) || (self->bof && recordsToSkip < 0)) return;
    if (self->eof && recordsToSkip < 0) self->eof = 0;
    if (self->bof && recordsToSkip > 0) self->bof = 0;
    dir = recordsToSkip > 0 ? 1 : -1;
    absn = recordsToSkip > 0 ? recordsToSkip : -recordsToSkip;
    skipped = 0;
    while (skipped < absn) {
        DbfEngine_Step(self, dir);
        if (self->bof || self->eof) return;
        if (DbfEngine_IsCurrentRecordValid(self) && DbfEngine_EvaluateFilter(self)) skipped++;
    }
}

void DbfEngine_GoTopInternal(DbfEngine* self)
{
    DbfEngine_GoTop(self);
    if (!DbfEngine_IsCurrentRecordValid(self) || !DbfEngine_EvaluateFilter(self))
        DbfEngine_SkipInternal(self, 1);
}

void DbfEngine_GoBottomInternal(DbfEngine* self)
{
    DbfEngine_GoBottom(self);
    if (!self->eof && (!DbfEngine_IsCurrentRecordValid(self) || !DbfEngine_EvaluateFilter(self)))
        DbfEngine_SkipInternal(self, -1);
}

static int eng_add_handles(DbfEngine* self, IndexHandle* add, int n)
{
    int i;
    if (n <= 0) return GMB_DB_OK;
    if (self->indexCount + n > GMB_DBF_ENGINE_MAX_INDEXES) {
        for (i = 0; i < n; i++) IndexHandle_Dispose(&add[i]);
        return GMB_DB_ERR;
    }
    for (i = 0; i < n; i++) self->indexes[self->indexCount++] = add[i];
    return GMB_DB_OK;
}

DbfEngine* DbfEngine_New(const char* filePath, const char* const* indexPaths, int indexPathCount,
                         int exclusive, LockingStrategy lockingStrategy)
{
    DbfEngine* self;
    const DbfHeader* hdr;
    int i;

    if (!filePath) return NULL;
    self = (DbfEngine*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    self->lockingStrategy = lockingStrategy;
    self->exclusive = exclusive;
    self->activeOrder = -1;
    self->deletedVisible = 0;
    self->tx = DbfTxHandle_New(filePath, exclusive, lockingStrategy);
    if (!self->tx) {
        free(self);
        return NULL;
    }
    self->dbfFile = DbfTxHandle_GetDbfFile(self->tx);
    if (!self->dbfFile) {
        DbfTxHandle_Dispose(self->tx);
        free(self);
        return NULL;
    }
    hdr = DbfFile_get_Header(self->dbfFile);
    self->memoFile = DbfMemoFactory_Create(filePath, (DbfVersion)hdr->VersionByte, exclusive);
    if (!self->memoFile) {
        DbfTxHandle_Dispose(self->tx);
        free(self);
        return NULL;
    }

    /* C#: lockManager sul tx handle è sempre non-null → writable=1 */
    if (indexPaths) {
        for (i = 0; i < indexPathCount; i++) {
            IndexHandle tmp[GMB_DBF_ENGINE_MAX_INDEXES];
            int n = 0;
            if (!indexPaths[i]) continue;
            if (IndexFactory_CreateHandles(indexPaths[i], 1, tmp, GMB_DBF_ENGINE_MAX_INDEXES, &n) != GMB_DB_OK)
                continue;
            if (eng_add_handles(self, tmp, n) != GMB_DB_OK) {
                DbfEngine_Dispose(self);
                return NULL;
            }
        }
    }
    self->activeOrder = self->indexCount > 0 ? 0 : -1;
    DbfEngine_GoTop(self);
    return self;
}

void DbfEngine_Dispose(DbfEngine* self)
{
    int i;
    if (!self) return;
    if (self->memoFile) MemoFile_Dispose(self->memoFile);
    if (self->repl) DbfReplicationLog_Dispose(self->repl);
    if (self->ver) VersioningManager_Dispose(self->ver);
    if (self->tx) DbfTxHandle_Dispose(self->tx);
    self->dbfFile = NULL;
    for (i = 0; i < self->indexCount; i++) IndexHandle_Dispose(&self->indexes[i]);
    free(self);
}

DbfFile* DbfEngine_get_DbfFile(DbfEngine* self) { return self ? self->dbfFile : NULL; }
MemoFile* DbfEngine_get_MemoFile(DbfEngine* self) { return self ? self->memoFile : NULL; }

const DbfHeader* DbfEngine_GetHeader(const DbfEngine* self)
{
    return self && self->dbfFile ? DbfFile_get_Header(self->dbfFile) : NULL;
}

const DbfField* DbfEngine_GetFields(const DbfEngine* self, int* count)
{
    if (!self || !self->dbfFile) {
        if (count) *count = 0;
        return NULL;
    }
    return DbfFile_get_Fields(self->dbfFile, count);
}

int DbfEngine_GetIndexCount(const DbfEngine* self) { return self ? self->indexCount : 0; }

const IndexHandle* DbfEngine_GetIndex(const DbfEngine* self, int i)
{
    if (!self || i < 0 || i >= self->indexCount) return NULL;
    return &self->indexes[i];
}

int DbfEngine_AddIndexPath(DbfEngine* self, const char* indexPath)
{
    IndexHandle tmp[GMB_DBF_ENGINE_MAX_INDEXES];
    int n = 0;
    if (!self || !indexPath) return GMB_DB_ARG;
    if (IndexFactory_CreateHandles(indexPath, 1, tmp, GMB_DBF_ENGINE_MAX_INDEXES, &n) != GMB_DB_OK)
        return GMB_DB_ERR;
    if (n <= 0) return GMB_DB_OK;
    if (eng_add_handles(self, tmp, n) != GMB_DB_OK) return GMB_DB_ERR;
    if (self->activeOrder < 0 && self->indexCount > 0) self->activeOrder = 0;
    DbfEngine_GoTop(self);
    return GMB_DB_OK;
}

int DbfEngine_SetActiveOrderName(DbfEngine* self, const char* indexName)
{
    int i;
    if (!self) return GMB_DB_ARG;
    if (!indexName || indexName[0] == 0) {
        self->activeOrder = -1;
    } else {
        int found = -1;
        for (i = 0; i < self->indexCount; i++) {
            const char* n = IndexHandle_get_Name(&self->indexes[i]);
            if (n && gmb_strcasecmp(n, indexName) == 0) {
                found = i;
                break;
            }
        }
        if (found < 0) return GMB_DB_ERR;
        self->activeOrder = found;
    }
    DbfEngine_GoTop(self);
    return GMB_DB_OK;
}

int DbfEngine_SetActiveOrderNumber(DbfEngine* self, int indexNumber)
{
    if (!self) return GMB_DB_ARG;
    if (indexNumber == 0) {
        self->activeOrder = -1;
    } else {
        int idx = indexNumber - 1;
        if (idx < 0 || idx >= self->indexCount) return GMB_DB_ARG;
        self->activeOrder = idx;
    }
    DbfEngine_GoTop(self);
    return GMB_DB_OK;
}

void DbfEngine_SetDeleted(DbfEngine* self, int on)
{
    if (!self) return;
    self->deletedVisible = !on;
    DbfEngine_GoTopInternal(self);
}

void DbfEngine_SetExact(DbfEngine* self, int on)
{
    if (self) self->exact = on ? 1 : 0;
}

int DbfEngine_GetActiveKeyLength(const DbfEngine* self)
{
    if (!self || self->activeOrder < 0 || self->activeOrder >= self->indexCount) return 0;
    return IndexHandle_get_KeyLength(&self->indexes[self->activeOrder]);
}

int DbfEngine_Seek(DbfEngine* self, const uint8_t* keyBytes, int keyLen)
{
    int rec;
    if (!self) return GMB_DB_ARG;
    self->found = 0;
    if (self->activeOrder < 0 || self->activeOrder >= self->indexCount) {
        uint32_t n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
        self->currentRecordNumber = (int)n + 1;
        self->eof = 1;
        return GMB_DB_OK;
    }
    rec = IndexHandle_Seek(&self->indexes[self->activeOrder], keyBytes, keyLen, self->exact ? 0 : 1);
    if (rec > 0) {
        int n, i;
        self->currentRecordNumber = rec;
        self->found = 1;
        n = IndexHandle_GetEntryCount(&self->indexes[self->activeOrder]);
        self->currentIndexInLeaf = 0;
        for (i = 0; i < n; i++) {
            if (IndexHandle_GetRecnoAt(&self->indexes[self->activeOrder], i) == rec) {
                self->currentIndexInLeaf = i;
                break;
            }
        }
        DbfEngine_UpdateBofEofStatus(self);
        return GMB_DB_OK;
    }
    {
        uint32_t n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
        self->currentRecordNumber = (int)n + 1;
        self->eof = 1;
        self->found = 0;
    }
    return GMB_DB_OK;
}

int DbfEngine_get_Found(const DbfEngine* self) { return self ? self->found : 0; }
int DbfEngine_get_Eof(const DbfEngine* self) { return self ? self->eof : 0; }
int DbfEngine_get_Bof(const DbfEngine* self) { return self ? self->bof : 0; }
int DbfEngine_GetCurrentRecordNumber(const DbfEngine* self)
{
    return self ? self->currentRecordNumber : 0;
}

uint32_t DbfEngine_GetTotalRecordCount(const DbfEngine* self)
{
    if (!self || !self->dbfFile) return 0;
    return DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
}

static int eng_extract_field_key(DbfEngine* self, int recno, const char* fieldName,
                                 uint8_t* keyOut, int keyCap, int* keyLen)
{
    const DbfField* fields;
    int nFields = 0, i, recLen, isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;
    const DbfHeader* hdr;
    char expr[128];
    char* save = NULL;
    char* tok;
    int written = 0;

    if (keyLen) *keyLen = 0;
    if (!self || !fieldName || !keyOut || keyCap <= 0) return GMB_DB_ARG;
    hdr = DbfFile_get_Header(self->dbfFile);
    recLen = hdr->RecordLength;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    if (DbfFile_ReadRawRecord(self->dbfFile, recno, &isDeleted, &raw, &rawLen) != GMB_DB_OK)
        return GMB_DB_ERR;
    (void)isDeleted;
    (void)recLen;
    memset(keyOut, ' ', (size_t)keyCap);
    snprintf(expr, sizeof(expr), "%s", fieldName);
    tok = strtok_r(expr, "+", &save);
    while (tok) {
        while (*tok == ' ') tok++;
        {
            int off = 0;
            int found = 0;
            for (i = 0; i < nFields; i++) {
                int flen = (int)fields[i].Length;
                if (gmb_strcasecmp(fields[i].Name, tok) == 0) {
                    int copy = flen;
                    if (written + copy > keyCap) copy = keyCap - written;
                    if (copy < 0) copy = 0;
                    if (off + copy > rawLen) copy = rawLen - off;
                    if (copy < 0) copy = 0;
                    if (copy > 0) memcpy(keyOut + written, raw + off, (size_t)copy);
                    written += copy;
                    found = 1;
                    break;
                }
                off += flen;
            }
            if (!found && strchr(fieldName, '+') == NULL) {
                free(raw);
                return GMB_DB_ERR;
            }
        }
        tok = strtok_r(NULL, "+", &save);
    }
    if (keyLen) *keyLen = keyCap;
    free(raw);
    return written > 0 || strchr(fieldName, '+') ? GMB_DB_OK : GMB_DB_ERR;
}

static int eng_default_key_producer(DbfEngine* eng, int recno, uint8_t* keyOut, int keyCap, int* keyLen, void* ctx)
{
    const char* field = ctx ? (const char*)ctx : "";
    return eng_extract_field_key(eng, recno, field, keyOut, keyCap, keyLen);
}

static int eng_replace_index_by_name(DbfEngine* self, const char* name, IndexHandle neu)
{
    int i;
    for (i = 0; i < self->indexCount; i++) {
        const char* n = IndexHandle_get_Name(&self->indexes[i]);
        if (n && name && gmb_strcasecmp(n, name) == 0) {
            IndexHandle_Dispose(&self->indexes[i]);
            self->indexes[i] = neu;
            self->activeOrder = i;
            return GMB_DB_OK;
        }
    }
    if (self->indexCount >= GMB_DBF_ENGINE_MAX_INDEXES) {
        IndexHandle_Dispose(&neu);
        return GMB_DB_ERR;
    }
    self->indexes[self->indexCount] = neu;
    self->activeOrder = self->indexCount;
    self->indexCount++;
    return GMB_DB_OK;
}

static int eng_rebuild_into(DbfEngine* self, IndexHandle* h, int keyLength,
                            DbfEngineKeyProducer producer, void* ctx)
{
    uint32_t count, i;
    uint8_t key[GMB_DBF_ENGINE_KEY_MAX];
    int keyLen = 0;
    int isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;

    if (keyLength <= 0 || keyLength > GMB_DBF_ENGINE_KEY_MAX) return GMB_DB_ARG;
    count = DbfEngine_GetTotalRecordCount(self);
    for (i = 1; i <= count; i++) {
        if (DbfFile_ReadRawRecord(self->dbfFile, (int)i, &isDeleted, &raw, &rawLen) != GMB_DB_OK)
            return GMB_DB_ERR;
        free(raw);
        raw = NULL;
        if (isDeleted && !self->deletedVisible) continue;
        memset(key, ' ', (size_t)keyLength);
        keyLen = keyLength;
        if (producer(self, (int)i, key, keyLength, &keyLen, ctx) != GMB_DB_OK) continue;
        if (keyLen <= 0) continue;
        if (IndexHandle_Insert(h, key, keyLen, (int)i) != GMB_DB_OK) return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

int DbfEngine_IndexOn(DbfEngine* self, const char* newIndexPath, const char* keyExpression,
                      int keyLength, int isUnique, const char* tagName,
                      DbfEngineKeyProducer keyProducer, void* keyProducerCtx)
{
    IndexHandle h;
    char dbfBase[256];
    DbfEngineKeyProducer prod;
    void* ctx;
    const char* expr;

    if (!self || !newIndexPath || keyLength <= 0) return GMB_DB_ARG;
    expr = keyExpression ? keyExpression : "";
    prod = keyProducer ? keyProducer : eng_default_key_producer;
    ctx = keyProducer ? keyProducerCtx : (void*)expr;
    memset(&h, 0, sizeof(h));
    eng_stem(DbfFile_get_FilePath(self->dbfFile), dbfBase, sizeof(dbfBase));

    if (eng_ext_is(newIndexPath, ".ndx")) {
        NdxFile* ndx = NdxFile_CreateNew(newIndexPath, expr, keyLength, isUnique);
        if (!ndx) return GMB_DB_ERR;
        h.Kind = IndexHandleKind_Ndx;
        h.Ndx = ndx;
    } else if (eng_ext_is(newIndexPath, ".mdx")) {
        MdxTagIndex* mdx;
        if (!tagName || tagName[0] == 0) return GMB_DB_ARG;
        mdx = MdxTagIndex_CreateOrReplace(newIndexPath, dbfBase, tagName, expr, keyLength, isUnique);
        if (!mdx) return GMB_DB_ERR;
        h.Kind = IndexHandleKind_MdxTag;
        h.Mdx = mdx;
    } else if (eng_ext_is(newIndexPath, ".cdx")) {
        CdxTagIndex* cdx;
        if (!tagName || tagName[0] == 0) return GMB_DB_ARG;
        cdx = CdxTagIndex_CreateOrReplace(newIndexPath, tagName, expr, keyLength, isUnique, 0);
        if (!cdx) return GMB_DB_ERR;
        h.Kind = IndexHandleKind_CdxTag;
        h.Cdx = cdx;
    } else {
        /* C#: default ClipperNtxWritable, non NtxFile GM */
        ClipperNtxWritable* w = ClipperNtxWritable_CreateNew(newIndexPath, expr, keyLength, isUnique);
        if (!w) return GMB_DB_ERR;
        h.Kind = IndexHandleKind_ClipperNtxWritable;
        h.ClipperW = w;
    }
    snprintf(h.Path, sizeof(h.Path), "%s", newIndexPath);

    if (eng_rebuild_into(self, &h, keyLength, prod, ctx) != GMB_DB_OK) {
        IndexHandle_Dispose(&h);
        return GMB_DB_ERR;
    }
    return eng_replace_index_by_name(self, IndexHandle_get_Name(&h), h);
}

static int eng_key_for_index(DbfEngine* self, int recno, IndexHandle* h, uint8_t* key, int* keyLen)
{
    int cap = IndexHandle_get_KeyLength(h);
    const char* expr = IndexHandle_GetKeyExpressionString(h);
    if (cap <= 0 || cap > GMB_DBF_ENGINE_KEY_MAX) return GMB_DB_ERR;
    *keyLen = cap;
    memset(key, ' ', (size_t)cap);
    return eng_extract_field_key(self, recno, expr ? expr : "", key, cap, keyLen);
}

int DbfEngine_IsCurrentRecordDeleted(DbfEngine* self)
{
    int isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;
    if (!self || self->currentRecordNumber <= 0) return 0;
    if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &raw, &rawLen) != GMB_DB_OK)
        return 0;
    free(raw);
    return isDeleted;
}

int DbfEngine_PositionAtRecord(DbfEngine* self, int recNo)
{
    uint32_t n;
    if (!self) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (recNo <= 0 || (uint32_t)recNo > n) return GMB_DB_ARG;
    self->currentRecordNumber = recNo;
    self->currentIndexInLeaf = 0;
    DbfEngine_UpdateBofEofStatus(self);
    return GMB_DB_OK;
}

static int eng_filter_get(void* ctx, const char* name, char* out, int cap)
{
    return DbfEngine_GetFieldTextByName((DbfEngine*)ctx, name, out, cap);
}

static int64_t eng_ticks(void)
{
    static int64_t t = 1;
    return ++t;
}

static void eng_json_escape(char* dst, int cap, const char* s)
{
    int n = 0;
    if (!dst || cap <= 0) return;
    if (!s) s = "";
    while (*s && n < cap - 1) {
        if (*s == '"' || *s == '\\') {
            if (n + 2 >= cap) break;
            dst[n++] = '\\';
            dst[n++] = *s++;
        } else {
            dst[n++] = *s++;
        }
    }
    dst[n] = 0;
}

static void eng_json_current(DbfEngine* self, char* out, int cap)
{
    int nFields = 0, i, n;
    const DbfField* fields;
    char val[256], esc[512];
    if (!self || !out || cap <= 2) return;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    n = snprintf(out, (size_t)cap, "{");
    for (i = 0; i < nFields && n < cap - 2; i++) {
        if (DbfEngine_GetFieldText(self, i, val, (int)sizeof(val)) != GMB_DB_OK) val[0] = 0;
        eng_json_escape(esc, (int)sizeof(esc), val);
        n += snprintf(out + n, (size_t)(cap - n), "%s\"%s\":\"%s\"", i ? "," : "", fields[i].Name, esc);
    }
    if (n < cap - 1) snprintf(out + n, (size_t)(cap - n), "}");
}

static void eng_repl(DbfEngine* self, ReplicationOp op, int recNo, int withValues)
{
    char json[1024], line[1400];
    if (!self || !self->repl) return;
    json[0] = 0;
    if (withValues) eng_json_current(self, json, (int)sizeof(json));
    ReplicationLogLine_Format(op, recNo, withValues ? json : NULL, line, (int)sizeof(line));
    if (self->txNest > 0) {
        if (self->pendingReplN < 32)
            snprintf(self->pendingRepl[self->pendingReplN++], sizeof(self->pendingRepl[0]), "%s", line);
        return;
    }
    DbfReplicationLog_WriteLine(self->repl, line);
}

int DbfEngine_AppendRecord(DbfEngine* self, const uint8_t* payload, int payloadLen)
{
    int recNo, i;
    uint8_t key[GMB_DBF_ENGINE_KEY_MAX];
    int keyLen = 0;
    if (!self || !payload) return GMB_DB_ARG;
    recNo = DbfTxHandle_AppendRawRecord(self->tx, payload, payloadLen);
    if (recNo < 0) return GMB_DB_ERR;
    self->currentRecordNumber = recNo;
    self->currentIndexInLeaf = 0;
    DbfEngine_UpdateBofEofStatus(self);
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], key, &keyLen) != GMB_DB_OK) continue;
        IndexHandle_Insert(&self->indexes[i], key, keyLen, recNo);
    }
    if (self->ver) {
        VersionMetadata m;
        memset(&m, 0, sizeof(m));
        m.Version = eng_ticks();
        m.TransactionId = self->txNest > 0 ? 1 : 0;
        VersioningManager_AddNewVersionEntry(self->ver, m);
    }
    eng_repl(self, ReplicationOp_Append, recNo, 1);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_DeleteCurrent(DbfEngine* self)
{
    int recNo, i;
    uint8_t key[GMB_DBF_ENGINE_KEY_MAX];
    int keyLen = 0;
    uint32_t n;
    if (!self) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    recNo = self->currentRecordNumber;
    if (recNo <= 0 || (uint32_t)recNo > n) return GMB_DB_OK;
    if (DbfTxHandle_SetDeletedFlag(self->tx, recNo, 1) != GMB_DB_OK) return GMB_DB_ERR;
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], key, &keyLen) != GMB_DB_OK) continue;
        IndexHandle_Delete(&self->indexes[i], key, keyLen, recNo);
    }
    eng_repl(self, ReplicationOp_Delete, recNo, 0);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_RecallCurrent(DbfEngine* self)
{
    int recNo, i;
    uint8_t key[GMB_DBF_ENGINE_KEY_MAX];
    int keyLen = 0;
    uint32_t n;
    if (!self) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    recNo = self->currentRecordNumber;
    if (recNo <= 0 || (uint32_t)recNo > n) return GMB_DB_OK;
    if (DbfTxHandle_SetDeletedFlag(self->tx, recNo, 0) != GMB_DB_OK) return GMB_DB_ERR;
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], key, &keyLen) != GMB_DB_OK) continue;
        IndexHandle_Insert(&self->indexes[i], key, keyLen, recNo);
    }
    eng_repl(self, ReplicationOp_Recall, recNo, 0);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_Reindex(DbfEngine* self)
{
    int i, n;
    char paths[GMB_DBF_ENGINE_MAX_INDEXES][1024];
    char exprs[GMB_DBF_ENGINE_MAX_INDEXES][128];
    char tags[GMB_DBF_ENGINE_MAX_INDEXES][16];
    int keyLens[GMB_DBF_ENGINE_MAX_INDEXES];
    int uniques[GMB_DBF_ENGINE_MAX_INDEXES];
    if (!self) return GMB_DB_ARG;
    n = self->indexCount;
    for (i = 0; i < n; i++) {
        const char* p = IndexHandle_get_Path(&self->indexes[i]);
        const char* e = IndexHandle_GetKeyExpressionString(&self->indexes[i]);
        const char* t = IndexHandle_get_Name(&self->indexes[i]);
        snprintf(paths[i], sizeof(paths[i]), "%s", p ? p : "");
        snprintf(exprs[i], sizeof(exprs[i]), "%s", e ? e : "");
        snprintf(tags[i], sizeof(tags[i]), "%s", t ? t : "");
        keyLens[i] = IndexHandle_get_KeyLength(&self->indexes[i]);
        uniques[i] = IndexHandle_get_Unique(&self->indexes[i]);
    }
    for (i = 0; i < n; i++) {
        const char* tag = NULL;
        if (eng_ext_is(paths[i], ".mdx") || eng_ext_is(paths[i], ".cdx"))
            tag = tags[i];
        if (DbfEngine_IndexOn(self, paths[i], exprs[i], keyLens[i], uniques[i], tag, NULL, NULL) != GMB_DB_OK)
            return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

int DbfEngine_Pack(DbfEngine* self)
{
    char orig[1024], tmp[1100];
    DbfFile* dest;
    uint32_t count, i;
    int isDeleted = 0;
    uint8_t* raw = NULL;
    int rawLen = 0;
    if (!self) return GMB_DB_ARG;
    snprintf(orig, sizeof(orig), "%s", DbfFile_get_FilePath(self->dbfFile));
    snprintf(tmp, sizeof(tmp), "%s.pack", orig);
    if (DbfFile_CloneStructure(self->dbfFile, tmp) != GMB_DB_OK) return GMB_DB_ERR;
    dest = DbfFile_New(tmp, 1, self->lockingStrategy);
    if (!dest) {
        unlink(tmp);
        return GMB_DB_ERR;
    }
    count = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    for (i = 1; i <= count; i++) {
        if (DbfFile_ReadRawRecord(self->dbfFile, (int)i, &isDeleted, &raw, &rawLen) != GMB_DB_OK) {
            DbfFile_Dispose(dest);
            unlink(tmp);
            return GMB_DB_ERR;
        }
        if (!isDeleted) {
            if (DbfFile_AppendRawRecord(dest, raw, rawLen) < 0) {
                free(raw);
                DbfFile_Dispose(dest);
                unlink(tmp);
                return GMB_DB_ERR;
            }
        }
        free(raw);
        raw = NULL;
    }
    DbfFile_Dispose(dest);
    DbfTxHandle_Dispose(self->tx);
    self->tx = NULL;
    self->dbfFile = NULL;
    if (rename(tmp, orig) != 0) {
        unlink(tmp);
        self->tx = DbfTxHandle_New(orig, self->exclusive, self->lockingStrategy);
        self->dbfFile = self->tx ? DbfTxHandle_GetDbfFile(self->tx) : NULL;
        return GMB_DB_ERR;
    }
    self->tx = DbfTxHandle_New(orig, self->exclusive, self->lockingStrategy);
    self->dbfFile = self->tx ? DbfTxHandle_GetDbfFile(self->tx) : NULL;
    if (!self->dbfFile) return GMB_DB_ERR;
    if (DbfEngine_Reindex(self) != GMB_DB_OK) return GMB_DB_ERR;
    DbfEngine_GoTop(self);
    return GMB_DB_OK;
}

int DbfEngine_Zap(DbfEngine* self)
{
    if (!self) return GMB_DB_ARG;
    if (DbfTxHandle_Zap(self->tx) != GMB_DB_OK) return GMB_DB_ERR;
    if (DbfEngine_Reindex(self) != GMB_DB_OK) return GMB_DB_ERR;
    self->currentRecordNumber = 0;
    self->currentIndexInLeaf = 0;
    self->eof = 1;
    self->bof = 1;
    self->found = 0;
    if (self->ver) VersioningManager_Clear(self->ver);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

static int eng_field_offset(const DbfField* fields, int n, int idx)
{
    int i, off = 0;
    for (i = 0; i < idx && i < n; i++) off += fields[i].Length;
    return off;
}

static int eng_find_field(const DbfField* fields, int n, const char* name)
{
    char truncated[11];
    int i, len;
    if (!name) return -1;
    len = (int)strlen(name);
    if (len > 10) len = 10;
    memcpy(truncated, name, (size_t)len);
    truncated[len] = 0;
    for (i = 0; i < n; i++) {
        if (strcmp(fields[i].Name, name) == 0) return i;
    }
    for (i = 0; i < n; i++) {
        if (gmb_strcasecmp(fields[i].Name, truncated) == 0) return i;
    }
    return -1;
}

int DbfEngine_PutCurrentRecord(DbfEngine* self, const uint8_t* payload, int payloadLen)
{
    int recNo, i;
    uint8_t oldKeys[GMB_DBF_ENGINE_MAX_INDEXES][GMB_DBF_ENGINE_KEY_MAX];
    uint8_t newKey[GMB_DBF_ENGINE_KEY_MAX];
    int oldLens[GMB_DBF_ENGINE_MAX_INDEXES];
    int newLen = 0;
    uint32_t n;
    if (!self || !payload) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    recNo = self->currentRecordNumber;
    if (recNo <= 0 || (uint32_t)recNo > n) return GMB_DB_ERR;
    memset(oldLens, 0, sizeof(oldLens));
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], oldKeys[i], &oldLens[i]) != GMB_DB_OK)
            oldLens[i] = 0;
    }
    if (DbfTxHandle_WriteRawRecord(self->tx, recNo, payload, payloadLen) != GMB_DB_OK) return GMB_DB_ERR;
    for (i = 0; i < self->indexCount; i++) {
        if (eng_key_for_index(self, recNo, &self->indexes[i], newKey, &newLen) != GMB_DB_OK) continue;
        if (oldLens[i] != newLen || memcmp(oldKeys[i], newKey, (size_t)newLen) != 0) {
            if (oldLens[i] > 0) IndexHandle_Delete(&self->indexes[i], oldKeys[i], oldLens[i], recNo);
            IndexHandle_Insert(&self->indexes[i], newKey, newLen, recNo);
        }
    }
    if (self->ver) {
        VersionMetadata cur = VersioningManager_GetMetadata(self->ver, recNo);
        if (self->lastReadRecordVersion != 0 && cur.Version != self->lastReadRecordVersion)
            return GMB_DB_ERR;
        cur.Version = eng_ticks();
        cur.TransactionId = self->txNest > 0 ? 1 : 0;
        VersioningManager_SetMetadata(self->ver, recNo, cur);
        self->lastReadRecordVersion = cur.Version;
    }
    eng_repl(self, ReplicationOp_Put, recNo, 1);
    if (self->txNest == 0) DbfTxHandle_FinalizeAutoCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_ReplaceField(DbfEngine* self, int fieldIndex, const char* value)
{
    int nFields = 0, payloadLen, off, isDeleted = 0, rawLen = 0, rc;
    const DbfField* fields;
    uint8_t* payload = NULL;
    uint32_t n;
    if (!self) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (self->currentRecordNumber <= 0 || (uint32_t)self->currentRecordNumber > n) return GMB_DB_ERR;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    if (!fields || fieldIndex < 0 || fieldIndex >= nFields) return GMB_DB_ARG;
    if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &payload, &rawLen) != GMB_DB_OK)
        return GMB_DB_ERR;
    payloadLen = rawLen;
    off = eng_field_offset(fields, nFields, fieldIndex);
    if (off + fields[fieldIndex].Length > payloadLen) {
        free(payload);
        return GMB_DB_ERR;
    }
    DbfFieldWriter_Write(payload + off, fields[fieldIndex].Length, &fields[fieldIndex], value, self->memoFile);
    rc = DbfEngine_PutCurrentRecord(self, payload, payloadLen);
    free(payload);
    return rc;
}

int DbfEngine_ReplaceFieldName(DbfEngine* self, const char* fieldName, const char* value)
{
    int nFields = 0, idx;
    const DbfField* fields;
    if (!self) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    idx = eng_find_field(fields, nFields, fieldName);
    if (idx < 0) return GMB_DB_ARG;
    return DbfEngine_ReplaceField(self, idx, value);
}

int DbfEngine_GetFieldText(DbfEngine* self, int fieldIndex, char* out, int cap)
{
    int nFields = 0, off, isDeleted = 0, rawLen = 0;
    const DbfField* fields;
    uint8_t* payload = NULL;
    int rc;
    uint32_t n;
    if (!self || !out || cap <= 0) return GMB_DB_ARG;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (self->currentRecordNumber <= 0 || (uint32_t)self->currentRecordNumber > n) return GMB_DB_ERR;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    if (!fields || fieldIndex < 0 || fieldIndex >= nFields) return GMB_DB_ARG;
    if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &payload, &rawLen) != GMB_DB_OK)
        return GMB_DB_ERR;
    off = eng_field_offset(fields, nFields, fieldIndex);
    if (off + fields[fieldIndex].Length > rawLen) {
        free(payload);
        return GMB_DB_ERR;
    }
    rc = DbfFieldParser_Parse(&fields[fieldIndex], payload + off, fields[fieldIndex].Length, self->memoFile, out, cap);
    free(payload);
    return rc;
}

int DbfEngine_GetFieldTextByName(DbfEngine* self, const char* fieldName, char* out, int cap)
{
    int nFields = 0, idx;
    const DbfField* fields;
    if (!self) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    idx = eng_find_field(fields, nFields, fieldName);
    if (idx < 0) return GMB_DB_ARG;
    return DbfEngine_GetFieldText(self, idx, out, cap);
}

int DbfEngine_AppendBlankRecord(DbfEngine* self)
{
    int nFields = 0, payloadLen, off = 0, i;
    const DbfField* fields;
    uint8_t* payload;
    int rc;
    if (!self) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    payloadLen = (int)DbfFile_get_Header(self->dbfFile)->RecordLength - 1;
    payload = (uint8_t*)malloc((size_t)payloadLen);
    if (!payload) return GMB_DB_ERR;
    memset(payload, ' ', (size_t)payloadLen);
    for (i = 0; i < nFields; i++) {
        DbfFieldWriter_WriteBlank(payload + off, fields[i].Length, &fields[i], self->memoFile);
        off += fields[i].Length;
    }
    rc = DbfEngine_AppendRecord(self, payload, payloadLen);
    free(payload);
    return rc;
}

int DbfEngine_AppendManyRecords(DbfEngine* self, const uint8_t* payloads, int payloadLen, int count)
{
    int i;
    if (!self || !payloads || count < 0) return GMB_DB_ARG;
    DbfTxHandle_BeginGroupCommit(self->tx);
    for (i = 0; i < count; i++) {
        if (DbfEngine_AppendRecord(self, payloads + (i * payloadLen), payloadLen) != GMB_DB_OK) {
            DbfTxHandle_EndGroupCommit(self->tx);
            return GMB_DB_ERR;
        }
    }
    DbfTxHandle_EndGroupCommit(self->tx);
    return GMB_DB_OK;
}

int DbfEngine_BeginTransaction(DbfEngine* self)
{
    if (!self) return GMB_DB_ARG;
    if (self->txNest == 0) {
        char marker[64];
        if (DbfTxHandle_BeginTransaction(self->tx) == 0) return GMB_DB_ERR;
        self->pendingReplN = 0;
        if (self->repl) {
            ReplicationLogLine_FormatTxnMarker(ReplicationOp_TxnBegin, marker, (int)sizeof(marker));
            DbfReplicationLog_WriteLine(self->repl, marker);
        }
    }
    self->txNest++;
    return GMB_DB_OK;
}

int DbfEngine_Commit(DbfEngine* self)
{
    if (!self || self->txNest <= 0) return GMB_DB_ERR;
    self->txNest--;
    if (self->txNest == 0) {
        int i;
        char marker[64];
        if (DbfTxHandle_CommitTransaction(self->tx) != GMB_DB_OK) return GMB_DB_ERR;
        for (i = 0; i < self->pendingReplN; i++)
            DbfReplicationLog_WriteLine(self->repl, self->pendingRepl[i]);
        self->pendingReplN = 0;
        if (self->repl) {
            ReplicationLogLine_FormatTxnMarker(ReplicationOp_TxnCommit, marker, (int)sizeof(marker));
            DbfReplicationLog_WriteLine(self->repl, marker);
        }
        DbfTxHandle_FinalizeCommit(self->tx);
    }
    return GMB_DB_OK;
}

int DbfEngine_Rollback(DbfEngine* self)
{
    if (!self || self->txNest <= 0) return GMB_DB_ERR;
    self->txNest = 0;
    self->pendingReplN = 0;
    return DbfTxHandle_RollbackTransaction(self->tx);
}

void DbfEngine_BeginBulkLoad(DbfEngine* self)
{
    if (self) DbfTxHandle_BeginBulkLoad(self->tx);
}

void DbfEngine_EndBulkLoad(DbfEngine* self)
{
    if (self) DbfTxHandle_EndBulkLoad(self->tx);
}

void DbfEngine_BeginGroupCommit(DbfEngine* self)
{
    if (self) DbfTxHandle_BeginGroupCommit(self->tx);
}

void DbfEngine_EndGroupCommit(DbfEngine* self)
{
    if (self) DbfTxHandle_EndGroupCommit(self->tx);
}

void DbfEngine_FlushDurability(DbfEngine* self)
{
    if (self) DbfTxHandle_FlushDurability(self->tx);
}

int DbfEngine_CloneStructure(DbfEngine* self, const char* newFilePath)
{
    if (!self || !newFilePath) return GMB_DB_ARG;
    return DbfFile_CloneStructure(self->dbfFile, newFilePath);
}

static int eng_copy_visible_to(DbfEngine* self, const char* destPath, const int* order, int orderCount)
{
    DbfEngine* dest;
    uint32_t count, i;
    int isDeleted = 0, rawLen = 0;
    uint8_t* raw = NULL;
    int origRec, origEof, origBof;
    if (!self || !destPath) return GMB_DB_ARG;
    if (DbfFile_CloneStructure(self->dbfFile, destPath) != GMB_DB_OK) return GMB_DB_ERR;
    dest = DbfEngine_New(destPath, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!dest) {
        unlink(destPath);
        return GMB_DB_ERR;
    }
    origRec = self->currentRecordNumber;
    origEof = self->eof;
    origBof = self->bof;
    if (order && orderCount > 0) {
        for (i = 0; i < (uint32_t)orderCount; i++) {
            if (DbfFile_ReadRawRecord(self->dbfFile, order[i], &isDeleted, &raw, &rawLen) != GMB_DB_OK) {
                DbfEngine_Dispose(dest);
                unlink(destPath);
                return GMB_DB_ERR;
            }
            if (!isDeleted && DbfEngine_AppendRecord(dest, raw, rawLen) != GMB_DB_OK) {
                free(raw);
                DbfEngine_Dispose(dest);
                unlink(destPath);
                return GMB_DB_ERR;
            }
            free(raw);
            raw = NULL;
        }
    } else {
        count = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
        DbfEngine_GoTopInternal(self);
        for (i = 1; i <= count; i++) {
            if (!DbfEngine_IsCurrentRecordValid(self)) break;
            if (DbfFile_ReadRawRecord(self->dbfFile, self->currentRecordNumber, &isDeleted, &raw, &rawLen) != GMB_DB_OK) {
                DbfEngine_Dispose(dest);
                unlink(destPath);
                return GMB_DB_ERR;
            }
            if (!isDeleted && DbfEngine_AppendRecord(dest, raw, rawLen) != GMB_DB_OK) {
                free(raw);
                DbfEngine_Dispose(dest);
                unlink(destPath);
                return GMB_DB_ERR;
            }
            free(raw);
            raw = NULL;
            DbfEngine_SkipInternal(self, 1);
        }
    }
    DbfEngine_Dispose(dest);
    self->currentRecordNumber = origRec;
    self->eof = origEof;
    self->bof = origBof;
    return GMB_DB_OK;
}

int DbfEngine_CopyTo(DbfEngine* self, const char* destinationFilePath)
{
    return eng_copy_visible_to(self, destinationFilePath, NULL, 0);
}

typedef struct EngSortRow {
    int recNo;
    char key[256];
} EngSortRow;

static int eng_sort_cmp(const void* a, const void* b)
{
    return strcmp(((const EngSortRow*)a)->key, ((const EngSortRow*)b)->key);
}

static int eng_sort_cmp_desc(const void* a, const void* b)
{
    return -strcmp(((const EngSortRow*)a)->key, ((const EngSortRow*)b)->key);
}

int DbfEngine_SortTo(DbfEngine* self, const char* destinationFilePath, const char* fieldName, int descending)
{
    uint32_t count, i;
    int nFields = 0, idx, nOut = 0;
    const DbfField* fields;
    EngSortRow* rows;
    int* order;
    int rc;
    int savedRec, savedEof, savedBof;
    if (!self || !destinationFilePath || !fieldName) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    idx = eng_find_field(fields, nFields, fieldName);
    if (idx < 0) return GMB_DB_ARG;
    count = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    rows = (EngSortRow*)calloc((size_t)count + 1, sizeof(EngSortRow));
    if (!rows) return GMB_DB_ERR;
    savedRec = self->currentRecordNumber;
    savedEof = self->eof;
    savedBof = self->bof;
    DbfEngine_GoTopInternal(self);
    while (DbfEngine_IsCurrentRecordValid(self) && nOut < (int)count) {
        rows[nOut].recNo = self->currentRecordNumber;
        if (DbfEngine_GetFieldText(self, idx, rows[nOut].key, (int)sizeof(rows[nOut].key)) != GMB_DB_OK)
            rows[nOut].key[0] = 0;
        nOut++;
        DbfEngine_SkipInternal(self, 1);
    }
    qsort(rows, (size_t)nOut, sizeof(EngSortRow), descending ? eng_sort_cmp_desc : eng_sort_cmp);
    order = (int*)malloc((size_t)nOut * sizeof(int));
    if (!order) {
        free(rows);
        return GMB_DB_ERR;
    }
    for (i = 0; i < (uint32_t)nOut; i++) order[i] = rows[i].recNo;
    free(rows);
    rc = eng_copy_visible_to(self, destinationFilePath, order, nOut);
    free(order);
    self->currentRecordNumber = savedRec;
    self->eof = savedEof;
    self->bof = savedBof;
    return rc;
}

int DbfEngine_SetFilter(DbfEngine* self, const char* expression)
{
    if (!self) return GMB_DB_ARG;
    if (!expression || !*expression) self->filterExpr[0] = 0;
    else snprintf(self->filterExpr, sizeof(self->filterExpr), "%s", expression);
    DbfEngine_GoTopInternal(self);
    return GMB_DB_OK;
}

const char* DbfEngine_GetFilterExpression(const DbfEngine* self)
{
    if (!self || !self->filterExpr[0]) return NULL;
    return self->filterExpr;
}

int DbfEngine_EvaluateFilter(DbfEngine* self)
{
    uint32_t n;
    if (!self || !self->filterExpr[0] || self->bof || self->eof) return 1;
    n = DbfFile_get_Header(self->dbfFile)->NumberOfRecords;
    if (self->currentRecordNumber <= 0 || (uint32_t)self->currentRecordNumber > n) return 1;
    return RuntimeExpressionEvaluator_Eval(self->filterExpr, eng_filter_get, self);
}

int DbfEngine_EnableVersioning(DbfEngine* self)
{
    const char* path;
    if (!self) return GMB_DB_ARG;
    if (self->ver) return GMB_DB_OK;
    path = DbfFile_get_FilePath(self->dbfFile);
    self->ver = VersioningManager_New(path);
    return self->ver ? GMB_DB_OK : GMB_DB_ERR;
}

static void eng_csv_write_field(FILE* f, const char* s)
{
    const char* p;
    int quote = 0;
    if (!s) s = "";
    for (p = s; *p; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            quote = 1;
            break;
        }
    }
    if (!quote) {
        fputs(s, f);
        return;
    }
    fputc('"', f);
    for (p = s; *p; p++) {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

int DbfEngine_ExportToCsv(DbfEngine* self, const char* filePath)
{
    FILE* f;
    int nFields = 0, i, orig;
    const DbfField* fields;
    char val[512];
    if (!self || !filePath) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    f = fopen(filePath, "w");
    if (!f) return GMB_DB_ERR;
    orig = self->currentRecordNumber;
    for (i = 0; i < nFields; i++) {
        if (i) fputc(',', f);
        fputs(fields[i].Name, f);
    }
    fputc('\n', f);
    DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        for (i = 0; i < nFields; i++) {
            if (i) fputc(',', f);
            if (DbfEngine_GetFieldText(self, i, val, (int)sizeof(val)) != GMB_DB_OK) val[0] = 0;
            eng_csv_write_field(f, val);
        }
        fputc('\n', f);
        DbfEngine_SkipInternal(self, 1);
    }
    fclose(f);
    if (orig > 0) DbfEngine_PositionAtRecord(self, orig);
    else DbfEngine_GoTopInternal(self);
    return GMB_DB_OK;
}

int DbfEngine_ExportToJson(DbfEngine* self, const char* filePath)
{
    FILE* f;
    int nFields = 0, i, orig, first = 1;
    const DbfField* fields;
    char val[512], esc[1024];
    if (!self || !filePath) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    f = fopen(filePath, "w");
    if (!f) return GMB_DB_ERR;
    orig = self->currentRecordNumber;
    fputs("[\n", f);
    DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        if (!first) fputs(",\n", f);
        first = 0;
        fputs("  {\n", f);
        for (i = 0; i < nFields; i++) {
            if (DbfEngine_GetFieldText(self, i, val, (int)sizeof(val)) != GMB_DB_OK) val[0] = 0;
            eng_json_escape(esc, (int)sizeof(esc), val);
            fprintf(f, "    \"%s\": \"%s\"%s\n", fields[i].Name, esc, i + 1 < nFields ? "," : "");
        }
        fputs("  }", f);
        DbfEngine_SkipInternal(self, 1);
    }
    fputs("\n]\n", f);
    fclose(f);
    if (orig > 0) DbfEngine_PositionAtRecord(self, orig);
    else DbfEngine_GoTopInternal(self);
    return GMB_DB_OK;
}

static const char* eng_jskip(const char* p)
{
    while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static int eng_jstr(const char** pp, char* out, int cap)
{
    const char* p = eng_jskip(*pp);
    int n = 0;
    if (!p || *p != '"') return GMB_DB_ERR;
    p++;
    while (*p && *p != '"' && n < cap - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            out[n++] = *p++;
        } else {
            out[n++] = *p++;
        }
    }
    if (*p != '"') return GMB_DB_ERR;
    out[n] = 0;
    *pp = p + 1;
    return GMB_DB_OK;
}

static int eng_jval(const char** pp, char* out, int cap)
{
    const char* p = eng_jskip(*pp);
    int n = 0, depth;
    if (!p || !*p) return GMB_DB_ERR;
    if (*p == '"') return eng_jstr(pp, out, cap);
    if (*p == '{' || *p == '[') {
        depth = 0;
        do {
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']') depth--;
            if (n < cap - 1) out[n++] = *p;
            p++;
        } while (*p && depth > 0);
        out[n] = 0;
        *pp = p;
        return GMB_DB_OK;
    }
    if (strncmp(p, "true", 4) == 0) {
        snprintf(out, (size_t)cap, "T");
        *pp = p + 4;
        return GMB_DB_OK;
    }
    if (strncmp(p, "false", 5) == 0) {
        snprintf(out, (size_t)cap, "F");
        *pp = p + 5;
        return GMB_DB_OK;
    }
    if (strncmp(p, "null", 4) == 0) {
        out[0] = 0;
        *pp = p + 4;
        return GMB_DB_OK;
    }
    if (*p == '-') out[n++] = *p++;
    while ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+') {
        if (n < cap - 1) out[n++] = *p++;
        else p++;
    }
    out[n] = 0;
    *pp = p;
    return n > 0 ? GMB_DB_OK : GMB_DB_ERR;
}

int DbfEngine_ImportFromJson(DbfEngine* self, const char* filePath)
{
    FILE* f;
    char* buf = NULL;
    long sz;
    const char* p;
    int nFields = 0, payloadLen;
    const DbfField* fields;
    if (!self || !filePath) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    payloadLen = (int)DbfFile_get_Header(self->dbfFile)->RecordLength - 1;
    f = fopen(filePath, "rb");
    if (!f) return GMB_DB_ERR;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    rewind(f);
    buf = (char*)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return GMB_DB_ERR;
    }
    fclose(f);
    buf[sz] = 0;
    p = eng_jskip(buf);
    if (*p != '[') {
        free(buf);
        return GMB_DB_ERR;
    }
    p++;
    if (DbfEngine_BeginTransaction(self) != GMB_DB_OK) {
        free(buf);
        return GMB_DB_ERR;
    }
    p = eng_jskip(p);
    while (*p && *p != ']') {
        uint8_t* payload;
        int off = 0, fi;
        if (*p == ',') {
            p = eng_jskip(p + 1);
            continue;
        }
        if (*p != '{') {
            DbfEngine_Rollback(self);
            free(buf);
            return GMB_DB_ERR;
        }
        p++;
        payload = (uint8_t*)malloc((size_t)payloadLen);
        if (!payload) {
            DbfEngine_Rollback(self);
            free(buf);
            return GMB_DB_ERR;
        }
        memset(payload, ' ', (size_t)payloadLen);
        for (fi = 0; fi < nFields; fi++) {
            DbfFieldWriter_WriteBlank(payload + off, fields[fi].Length, &fields[fi], self->memoFile);
            off += fields[fi].Length;
        }
        p = eng_jskip(p);
        while (*p && *p != '}') {
            char key[64], val[1024];
            int idx;
            if (*p == ',') {
                p = eng_jskip(p + 1);
                continue;
            }
            if (eng_jstr(&p, key, (int)sizeof(key)) != GMB_DB_OK) {
                free(payload);
                DbfEngine_Rollback(self);
                free(buf);
                return GMB_DB_ERR;
            }
            p = eng_jskip(p);
            if (*p != ':') {
                free(payload);
                DbfEngine_Rollback(self);
                free(buf);
                return GMB_DB_ERR;
            }
            p++;
            if (eng_jval(&p, val, (int)sizeof(val)) != GMB_DB_OK) {
                free(payload);
                DbfEngine_Rollback(self);
                free(buf);
                return GMB_DB_ERR;
            }
            idx = eng_find_field(fields, nFields, key);
            if (idx < 0) {
                free(payload);
                DbfEngine_Rollback(self);
                free(buf);
                return GMB_DB_ERR;
            }
            off = eng_field_offset(fields, nFields, idx);
            DbfFieldWriter_Write(payload + off, fields[idx].Length, &fields[idx], val, self->memoFile);
            p = eng_jskip(p);
        }
        if (*p == '}') p++;
        if (DbfEngine_AppendRecord(self, payload, payloadLen) != GMB_DB_OK) {
            free(payload);
            DbfEngine_Rollback(self);
            free(buf);
            return GMB_DB_ERR;
        }
        free(payload);
        p = eng_jskip(p);
    }
    free(buf);
    return DbfEngine_Commit(self);
}

int DbfEngine_ImportFromCsv(DbfEngine* self, const char* filePath)
{
    FILE* f;
    char line[4096];
    char headers[GMB_DBF_ENGINE_MAX_INDEXES][32];
    int nHead = 0, i;
    int nFields = 0;
    const DbfField* fields;
    if (!self || !filePath) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    f = fopen(filePath, "r");
    if (!f) return GMB_DB_ERR;
    if (!fgets(line, (int)sizeof(line), f)) {
        fclose(f);
        return GMB_DB_ERR;
    }
    {
        char* save = NULL;
        char* tok = strtok_r(line, ",\r\n", &save);
        while (tok && nHead < GMB_DBF_ENGINE_MAX_INDEXES) {
            while (*tok == ' ') tok++;
            snprintf(headers[nHead++], sizeof(headers[0]), "%s", tok);
            tok = strtok_r(NULL, ",\r\n", &save);
        }
    }
    for (i = 0; i < nHead; i++) {
        if (eng_find_field(fields, nFields, headers[i]) < 0) {
            fclose(f);
            return GMB_DB_ERR;
        }
    }
    if (DbfEngine_BeginTransaction(self) != GMB_DB_OK) {
        fclose(f);
        return GMB_DB_ERR;
    }
    while (fgets(line, (int)sizeof(line), f)) {
        char* save = NULL;
        char* tok;
        int col = 0;
        int payloadLen = (int)DbfFile_get_Header(self->dbfFile)->RecordLength - 1;
        uint8_t* payload = (uint8_t*)malloc((size_t)payloadLen);
        int off = 0, fi;
        if (!payload) {
            DbfEngine_Rollback(self);
            fclose(f);
            return GMB_DB_ERR;
        }
        memset(payload, ' ', (size_t)payloadLen);
        for (fi = 0; fi < nFields; fi++) {
            DbfFieldWriter_WriteBlank(payload + off, fields[fi].Length, &fields[fi], self->memoFile);
            off += fields[fi].Length;
        }
        tok = strtok_r(line, ",\r\n", &save);
        while (tok && col < nHead) {
            int idx = eng_find_field(fields, nFields, headers[col]);
            if (idx >= 0) {
                int o = eng_field_offset(fields, nFields, idx);
                DbfFieldWriter_Write(payload + o, fields[idx].Length, &fields[idx], tok, self->memoFile);
            }
            col++;
            tok = strtok_r(NULL, ",\r\n", &save);
        }
        if (DbfEngine_AppendRecord(self, payload, payloadLen) != GMB_DB_OK) {
            free(payload);
            DbfEngine_Rollback(self);
            fclose(f);
            return GMB_DB_ERR;
        }
        free(payload);
    }
    fclose(f);
    return DbfEngine_Commit(self);
}

int DbfEngine_AppendFrom(DbfEngine* self, const char* sourceFilePath)
{
    DbfEngine* src;
    int nfDst = 0, nfSrc = 0, isDeleted = 0, rawLen = 0;
    uint8_t* raw = NULL;
    const DbfField* dstF;
    const DbfField* srcF;
    if (!self || !sourceFilePath) return GMB_DB_ARG;
    src = DbfEngine_New(sourceFilePath, NULL, 0, 0, LockingStrategy_IntraProcess);
    if (!src) return GMB_DB_ERR;
    dstF = DbfFile_get_Fields(self->dbfFile, &nfDst);
    srcF = DbfFile_get_Fields(src->dbfFile, &nfSrc);
    (void)dstF;
    (void)srcF;
    if (nfDst != nfSrc) {
        DbfEngine_Dispose(src);
        return GMB_DB_ERR;
    }
    DbfEngine_BeginBulkLoad(self);
    DbfEngine_GoTopInternal(src);
    while (!src->eof) {
        if (DbfFile_ReadRawRecord(src->dbfFile, src->currentRecordNumber, &isDeleted, &raw, &rawLen) != GMB_DB_OK) {
            DbfEngine_EndBulkLoad(self);
            DbfEngine_Dispose(src);
            return GMB_DB_ERR;
        }
        if (!isDeleted && DbfEngine_AppendRecord(self, raw, rawLen) != GMB_DB_OK) {
            free(raw);
            DbfEngine_EndBulkLoad(self);
            DbfEngine_Dispose(src);
            return GMB_DB_ERR;
        }
        free(raw);
        raw = NULL;
        DbfEngine_SkipInternal(src, 1);
    }
    DbfEngine_EndBulkLoad(self);
    DbfEngine_Dispose(src);
    return GMB_DB_OK;
}

int DbfEngine_WriteList(DbfEngine* self, FILE* writer, int startAtTop)
{
    int nFields = 0, i, orig, origEof, origBof, written = 0;
    const DbfField* fields;
    char val[256];
    const char* cols[64];
    char store[64][256];
    char line[2048];
    if (!self || !writer) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    if (nFields > 64) nFields = 64;
    orig = self->currentRecordNumber;
    origEof = self->eof;
    origBof = self->bof;
    if (startAtTop) DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        for (i = 0; i < nFields; i++) {
            if (DbfEngine_GetFieldText(self, i, val, (int)sizeof(val)) != GMB_DB_OK) val[0] = 0;
            snprintf(store[i], sizeof(store[i]), "%s", val);
            cols[i] = store[i];
            (void)fields;
        }
        DbfListFormatter_FormatLine(self->currentRecordNumber, cols, nFields, line, (int)sizeof(line));
        fprintf(writer, "%s\n", line);
        written++;
        DbfEngine_SkipInternal(self, 1);
    }
    self->currentRecordNumber = orig;
    self->eof = origEof;
    self->bof = origBof;
    DbfEngine_UpdateBofEofStatus(self);
    return written;
}

int DbfEngine_TotalOn(DbfEngine* self, const char* destinationFilePath, const char* keyField,
                      const char* const* sumFields, int sumFieldCount)
{
    DbfEngine* dest;
    char lastKey[256], curKey[256];
    double sums[32];
    int nFields = 0, i, keyIdx, haveGroup = 0;
    const DbfField* fields;
    uint8_t* payload;
    int payloadLen, off;
    if (!self || !destinationFilePath || !keyField) return GMB_DB_ARG;
    if (self->activeOrder < 0) return GMB_DB_ERR;
    if (sumFieldCount < 0 || sumFieldCount > 32) return GMB_DB_ARG;
    fields = DbfFile_get_Fields(self->dbfFile, &nFields);
    keyIdx = eng_find_field(fields, nFields, keyField);
    if (keyIdx < 0) return GMB_DB_ARG;
    if (DbfFile_CloneStructure(self->dbfFile, destinationFilePath) != GMB_DB_OK) return GMB_DB_ERR;
    dest = DbfEngine_New(destinationFilePath, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!dest) {
        unlink(destinationFilePath);
        return GMB_DB_ERR;
    }
    payloadLen = (int)DbfFile_get_Header(self->dbfFile)->RecordLength - 1;
    payload = (uint8_t*)malloc((size_t)payloadLen);
    if (!payload) {
        DbfEngine_Dispose(dest);
        unlink(destinationFilePath);
        return GMB_DB_ERR;
    }
    lastKey[0] = 0;
    memset(sums, 0, sizeof(sums));
    DbfEngine_GoTopInternal(self);
    while (!self->eof) {
        char num[64];
        if (DbfEngine_GetFieldText(self, keyIdx, curKey, (int)sizeof(curKey)) != GMB_DB_OK) curKey[0] = 0;
        if (haveGroup && strcmp(curKey, lastKey) != 0) {
            int fi;
            memset(payload, ' ', (size_t)payloadLen);
            off = 0;
            for (fi = 0; fi < nFields; fi++) {
                DbfFieldWriter_WriteBlank(payload + off, fields[fi].Length, &fields[fi], dest->memoFile);
                off += fields[fi].Length;
            }
            off = eng_field_offset(fields, nFields, keyIdx);
            DbfFieldWriter_Write(payload + off, fields[keyIdx].Length, &fields[keyIdx], lastKey, dest->memoFile);
            for (i = 0; i < sumFieldCount; i++) {
                int si = eng_find_field(fields, nFields, sumFields[i]);
                if (si < 0) continue;
                snprintf(num, sizeof(num), "%.4f", sums[i]);
                off = eng_field_offset(fields, nFields, si);
                DbfFieldWriter_Write(payload + off, fields[si].Length, &fields[si], num, dest->memoFile);
            }
            if (DbfEngine_AppendRecord(dest, payload, payloadLen) != GMB_DB_OK) {
                free(payload);
                DbfEngine_Dispose(dest);
                unlink(destinationFilePath);
                return GMB_DB_ERR;
            }
            memset(sums, 0, sizeof(sums));
        }
        snprintf(lastKey, sizeof(lastKey), "%s", curKey);
        haveGroup = 1;
        for (i = 0; i < sumFieldCount; i++) {
            int si = eng_find_field(fields, nFields, sumFields[i]);
            if (si < 0) continue;
            if (DbfEngine_GetFieldText(self, si, num, (int)sizeof(num)) != GMB_DB_OK) num[0] = 0;
            sums[i] += strtod(num, NULL);
        }
        DbfEngine_SkipInternal(self, 1);
    }
    if (haveGroup) {
        int fi;
        char num[64];
        memset(payload, ' ', (size_t)payloadLen);
        off = 0;
        for (fi = 0; fi < nFields; fi++) {
            DbfFieldWriter_WriteBlank(payload + off, fields[fi].Length, &fields[fi], dest->memoFile);
            off += fields[fi].Length;
        }
        off = eng_field_offset(fields, nFields, keyIdx);
        DbfFieldWriter_Write(payload + off, fields[keyIdx].Length, &fields[keyIdx], lastKey, dest->memoFile);
        for (i = 0; i < sumFieldCount; i++) {
            int si = eng_find_field(fields, nFields, sumFields[i]);
            if (si < 0) continue;
            snprintf(num, sizeof(num), "%.4f", sums[i]);
            off = eng_field_offset(fields, nFields, si);
            DbfFieldWriter_Write(payload + off, fields[si].Length, &fields[si], num, dest->memoFile);
        }
        if (DbfEngine_AppendRecord(dest, payload, payloadLen) != GMB_DB_OK) {
            free(payload);
            DbfEngine_Dispose(dest);
            unlink(destinationFilePath);
            return GMB_DB_ERR;
        }
    }
    free(payload);
    DbfEngine_Dispose(dest);
    return GMB_DB_OK;
}

int DbfEngine_SetReplication(DbfEngine* self, const char* logPath)
{
    char path[1024];
    if (!self) return GMB_DB_ARG;
    if (self->repl) {
        DbfReplicationLog_Dispose(self->repl);
        self->repl = NULL;
    }
    if (!logPath || !*logPath) return GMB_DB_OK;
    if (strcmp(logPath, ".") == 0 || gmb_strcasecmp(logPath, "DEFAULT") == 0) {
        snprintf(path, sizeof(path), "%s.repl", DbfFile_get_FilePath(self->dbfFile));
    } else {
        snprintf(path, sizeof(path), "%s", logPath);
    }
    self->repl = DbfReplicationLog_New(path);
    return self->repl ? GMB_DB_OK : GMB_DB_ERR;
}

const char* DbfEngine_GetReplicationLogPath(const DbfEngine* self)
{
    return self && self->repl ? DbfReplicationLog_get_LogPath(self->repl) : NULL;
}

int DbfEngine_ApplyReplication(DbfEngine* self, const char* replLogPath)
{
    return DbfReplicationApplier_Apply(replLogPath, self);
}

int DbfEngine_FollowReplication(DbfEngine* self, const char* replLogPath, const char* checkpointPath)
{
    const char* path = replLogPath;
    if (!self) return GMB_DB_ARG;
    if (!path || !path[0]) path = DbfEngine_GetReplicationLogPath(self);
    if (!path) return GMB_DB_ARG;
    return DbfReplicationApplier_Follow(path, self, checkpointPath);
}

int DbfEngine_ReplicationShipTcp(DbfEngine* self, const char* host, int port, const char* replLogPath,
                                 const char* shipCheckpointPath)
{
    const char* path = replLogPath;
    if (!self || !host) return GMB_DB_ARG;
    if (!path || !path[0]) path = DbfEngine_GetReplicationLogPath(self);
    if (!path) return GMB_DB_ARG;
    return ReplicationTcpShipper_ShipOnce(path, host, port, shipCheckpointPath);
}

static int eng_repl_apply_cb(ReplicationApplyState* state, const char* line, void* ctx)
{
    return DbfReplicationApplier_ApplyLine(line, state, (DbfEngine*)ctx);
}

int DbfEngine_ReplicationRecvTcp(DbfEngine* self, int port, const char* checkpointPath)
{
    if (!self) return GMB_DB_ARG;
    DbfEngine_SetReplication(self, "");
    return ReplicationTcpReceiver_ReceiveOnce(port, eng_repl_apply_cb, self, checkpointPath, 30);
}

static int eng_sc_rm(const char* path) { return path ? unlink(path) : 0; }

typedef struct EngRecvJob {
    DbfEngine* e;
    int port;
    int applied;
} EngRecvJob;

static void* eng_sc_recv(void* arg)
{
    EngRecvJob* job = (EngRecvJob*)arg;
    job->applied = DbfEngine_ReplicationRecvTcp(job->e, job->port, NULL);
    return NULL;
}

static int eng_sc_write_rec(DbfEngine* e, const char* name10)
{
    const DbfHeader* hdr = DbfFile_get_Header(e->dbfFile);
    int payload = (int)hdr->RecordLength - 1;
    uint8_t* rec;
    int rc;
    rec = (uint8_t*)malloc((size_t)payload);
    if (!rec) return GMB_DB_ERR;
    memset(rec, ' ', (size_t)payload);
    memcpy(rec, name10, 10);
    rc = DbfFile_AppendRawRecord(e->dbfFile, rec, payload);
    free(rec);
    return rc < 0 ? GMB_DB_ERR : GMB_DB_OK;
}

int DbfEngine_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/engsc_XXXXXX";
    char dbf[PATH_MAX], ntx[PATH_MAX], ndx[PATH_MAX], fpt[PATH_MAX];
    char copy[PATH_MAX], sorted[PATH_MAX], wal[PATH_MAX], text[32];
    DbfField fields[2];
    DbfEngine* e;
    DbfEngine* reopen;
    DbfEngine* dest;
    const char* idxPaths[1];
    MemoData md;
    const uint8_t hello[] = "hello";
    uint8_t key[10];
    int nFields = 0, memoIdx;
    const DbfHeader* hdr;
    WalLogSummary sum;

    if (DbfFieldWriter_SelfCheck() != GMB_DB_OK) return -50;
    if (DbfFieldParser_SelfCheck() != GMB_DB_OK) return -51;
    if (VersioningManager_SelfCheck() != GMB_DB_OK) return -70;

    if (!mkdtemp(dir)) return -2;

    snprintf(dbf, sizeof(dbf), "%s/t.dbf", dir);
    snprintf(ntx, sizeof(ntx), "%s/byname.ntx", dir);
    snprintf(ndx, sizeof(ndx), "%s/id.ndx", dir);
    snprintf(fpt, sizeof(fpt), "%s/t.fpt", dir);
    copy[0] = sorted[0] = wal[0] = 0;
    fields[0] = DbfField_Create("NAME", (uint8_t)DbfFieldType_Character, 10, 0);
    fields[1] = DbfField_Create("NOTES", (uint8_t)DbfFieldType_Memo, 10, 0);
    if (DbfFile_CreateNew(dbf, "windows-1252", fields, 2) != GMB_DB_OK) return -3;

    e = DbfEngine_New(dbf, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!e) return -4;
    hdr = DbfEngine_GetHeader(e);
    if (!hdr || hdr->NumberOfRecords != 0) {
        DbfEngine_Dispose(e);
        return -5;
    }
    if (!DbfEngine_GetFields(e, &nFields) || nFields != 2) {
        DbfEngine_Dispose(e);
        return -6;
    }
    if (MemoFile_IsNull(DbfEngine_get_MemoFile(e))) {
        DbfEngine_Dispose(e);
        return -7;
    }
    if (access(fpt, F_OK) != 0) {
        DbfEngine_Dispose(e);
        return -8;
    }
    memoIdx = MemoFile_WriteMemo(DbfEngine_get_MemoFile(e), hello, 5, 1);
    if (memoIdx != 1) {
        DbfEngine_Dispose(e);
        return -9;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(DbfEngine_get_MemoFile(e), 1, &md) != GMB_DB_OK || md.Length != 5
        || memcmp(md.Data, hello, 5) != 0) {
        MemoData_Dispose(&md);
        DbfEngine_Dispose(e);
        return -10;
    }
    MemoData_Dispose(&md);

    if (eng_sc_write_rec(e, "BOB       ") != GMB_DB_OK || eng_sc_write_rec(e, "ALICE     ") != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -11;
    }
    if (DbfEngine_GetTotalRecordCount(e) != 2) {
        DbfEngine_Dispose(e);
        return -12;
    }
    if (DbfEngine_IndexOn(e, ntx, "NAME", 10, 0, NULL, NULL, NULL) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -13;
    }
    if (DbfEngine_GetIndexCount(e) != 1
        || DbfEngine_GetIndex(e, 0)->Kind != IndexHandleKind_ClipperNtxWritable) {
        DbfEngine_Dispose(e);
        return -14;
    }
    memset(key, ' ', 10);
    memcpy(key, "ALICE", 5);
    if (DbfEngine_SetActiveOrderNumber(e, 1) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -15;
    }
    if (DbfEngine_Seek(e, key, 10) != GMB_DB_OK || !DbfEngine_get_Found(e)
        || DbfEngine_GetCurrentRecordNumber(e) != 2) {
        DbfEngine_Dispose(e);
        return -16;
    }
    if (DbfEngine_IndexOn(e, ndx, "NAME", 10, 0, NULL, NULL, NULL) != GMB_DB_OK
        || DbfEngine_GetIndexCount(e) != 2) {
        DbfEngine_Dispose(e);
        return -17;
    }
    if (DbfEngine_SetActiveOrderName(e, "byname") != GMB_DB_OK
        || DbfEngine_SetActiveOrderName(e, "id") != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -18;
    }
    if (DbfEngine_SetActiveOrderName(e, "byname") != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -19;
    }
    DbfEngine_GoTop(e);
    if (DbfEngine_GetCurrentRecordNumber(e) != 2) {
        DbfEngine_Dispose(e);
        return -20;
    }
    DbfEngine_Skip(e, 1);
    if (DbfEngine_GetCurrentRecordNumber(e) != 1 || DbfEngine_get_Eof(e)) {
        DbfEngine_Dispose(e);
        return -41;
    }
    DbfEngine_Dispose(e);

    idxPaths[0] = ntx;
    reopen = DbfEngine_New(dbf, idxPaths, 1, 0, LockingStrategy_IntraProcess);
    if (!reopen) return -21;
    if (MemoFile_IsNull(DbfEngine_get_MemoFile(reopen))) {
        DbfEngine_Dispose(reopen);
        return -22;
    }
    if (DbfEngine_GetIndexCount(reopen) != 1) {
        DbfEngine_Dispose(reopen);
        return -23;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(DbfEngine_get_MemoFile(reopen), 1, &md) != GMB_DB_OK || md.Length != 5) {
        MemoData_Dispose(&md);
        DbfEngine_Dispose(reopen);
        return -24;
    }
    MemoData_Dispose(&md);
    memset(key, ' ', 10);
    memcpy(key, "BOB", 3);
    if (DbfEngine_SetActiveOrderNumber(reopen, 1) != GMB_DB_OK
        || DbfEngine_Seek(reopen, key, 10) != GMB_DB_OK || !DbfEngine_get_Found(reopen)
        || DbfEngine_GetCurrentRecordNumber(reopen) != 1) {
        DbfEngine_Dispose(reopen);
        return -25;
    }
    DbfEngine_Dispose(reopen);

    if (DbfEngine_New("/no/such/file.dbf", NULL, 0, 0, LockingStrategy_IntraProcess) != NULL)
        return -26;

    /* GO/SKIP fisico, DELETE, SET DELETED, PACK, ZAP */
    e = DbfEngine_New(dbf, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!e) return -27;
    DbfEngine_GoTop(e);
    if (DbfEngine_GetCurrentRecordNumber(e) != 1) {
        DbfEngine_Dispose(e);
        return -28;
    }
    DbfEngine_Skip(e, 1);
    if (DbfEngine_GetCurrentRecordNumber(e) != 2 || DbfEngine_get_Eof(e)) {
        DbfEngine_Dispose(e);
        return -29;
    }
    DbfEngine_GoBottom(e);
    if (DbfEngine_GetCurrentRecordNumber(e) != 2) {
        DbfEngine_Dispose(e);
        return -30;
    }
    DbfEngine_GoTop(e);
    DbfEngine_Skip(e, 1);
    if (DbfEngine_DeleteCurrent(e) != GMB_DB_OK || !DbfEngine_IsCurrentRecordDeleted(e)) {
        DbfEngine_Dispose(e);
        return -31;
    }
    DbfEngine_SetDeleted(e, 1);
    if (DbfEngine_GetCurrentRecordNumber(e) != 1) {
        DbfEngine_Dispose(e);
        return -32;
    }
    DbfEngine_SkipInternal(e, 1);
    if (!DbfEngine_get_Eof(e)) {
        DbfEngine_Dispose(e);
        return -33;
    }
    DbfEngine_SetDeleted(e, 0);
    DbfEngine_GoTop(e);
    DbfEngine_Skip(e, 1);
    if (DbfEngine_RecallCurrent(e) != GMB_DB_OK || DbfEngine_IsCurrentRecordDeleted(e)) {
        DbfEngine_Dispose(e);
        return -34;
    }
    DbfEngine_GoTop(e);
    DbfEngine_Skip(e, 1);
    if (DbfEngine_DeleteCurrent(e) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -35;
    }
    if (DbfEngine_IndexOn(e, ntx, "NAME", 10, 0, NULL, NULL, NULL) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -36;
    }
    if (DbfEngine_Pack(e) != GMB_DB_OK || DbfEngine_GetTotalRecordCount(e) != 1) {
        DbfEngine_Dispose(e);
        return -37;
    }
    DbfEngine_GoTop(e);
    if (DbfEngine_GetCurrentRecordNumber(e) != 1) {
        DbfEngine_Dispose(e);
        return -38;
    }
    {
        uint8_t pay[20];
        memset(pay, ' ', sizeof(pay));
        memcpy(pay, "CAROL     ", 10);
        if (DbfEngine_AppendRecord(e, pay, (int)(DbfFile_get_Header(e->dbfFile)->RecordLength - 1)) != GMB_DB_OK
            || DbfEngine_GetTotalRecordCount(e) != 2) {
            DbfEngine_Dispose(e);
            return -39;
        }
    }
    DbfEngine_GoTop(e);
    DbfEngine_Skip(e, 1);
    if (DbfEngine_ReplaceFieldName(e, "NAME", "DAVE") != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -52;
    }
    if (DbfEngine_GetFieldTextByName(e, "NAME", text, sizeof(text)) != GMB_DB_OK || strcmp(text, "DAVE") != 0) {
        DbfEngine_Dispose(e);
        return -53;
    }
    memset(key, ' ', 10);
    memcpy(key, "DAVE", 4);
    if (DbfEngine_SetActiveOrderNumber(e, 1) != GMB_DB_OK
        || DbfEngine_Seek(e, key, 10) != GMB_DB_OK || !DbfEngine_get_Found(e)) {
        DbfEngine_Dispose(e);
        return -54;
    }
    snprintf(copy, sizeof(copy), "%s/copy.dbf", dir);
    snprintf(sorted, sizeof(sorted), "%s/sort.dbf", dir);
    if (DbfEngine_CopyTo(e, copy) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -55;
    }
    dest = DbfEngine_New(copy, NULL, 0, 0, LockingStrategy_IntraProcess);
    if (!dest || DbfEngine_GetTotalRecordCount(dest) != 2) {
        if (dest) DbfEngine_Dispose(dest);
        DbfEngine_Dispose(e);
        return -56;
    }
    DbfEngine_Dispose(dest);
    if (DbfEngine_SortTo(e, sorted, "NAME", 0) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -57;
    }
    dest = DbfEngine_New(sorted, NULL, 0, 0, LockingStrategy_IntraProcess);
    if (!dest) {
        DbfEngine_Dispose(e);
        return -58;
    }
    DbfEngine_GoTop(dest);
    if (DbfEngine_GetFieldText(dest, 0, text, sizeof(text)) != GMB_DB_OK || strcmp(text, "BOB") != 0) {
        DbfEngine_Dispose(dest);
        DbfEngine_Dispose(e);
        return -59;
    }
    DbfEngine_Dispose(dest);
    if (DbfEngine_BeginTransaction(e) != GMB_DB_OK || DbfEngine_AppendBlankRecord(e) != GMB_DB_OK) {
        DbfEngine_Dispose(e);
        return -60;
    }
    snprintf(wal, sizeof(wal), "%s/t.wal", dir);
    if (WriteAheadLog_Inspect(dbf, &sum) != GMB_DB_OK || !sum.Exists || !sum.InOpenTransaction) {
        DbfEngine_Dispose(e);
        return -61;
    }
    if (DbfEngine_Commit(e) != GMB_DB_OK || DbfEngine_GetTotalRecordCount(e) != 3) {
        DbfEngine_Dispose(e);
        return -62;
    }
    if (DbfEngine_Zap(e) != GMB_DB_OK || DbfEngine_GetTotalRecordCount(e) != 0 || !DbfEngine_get_Eof(e)) {
        DbfEngine_Dispose(e);
        return -40;
    }
    DbfEngine_Dispose(e);

    {
        char sales[PATH_MAX], tot[PATH_MAX], csvp[PATH_MAX], src2[PATH_MAX], replp[PATH_MAX], sntx[PATH_MAX];
        DbfField sf[2];
        DbfEngine *s, *t, *app;
        const char* sums[1];
        uint8_t pay[16];
        char line[256];
        FILE* lf;
        snprintf(sales, sizeof(sales), "%s/sales.dbf", dir);
        snprintf(tot, sizeof(tot), "%s/tot.dbf", dir);
        snprintf(csvp, sizeof(csvp), "%s/sales.csv", dir);
        snprintf(src2, sizeof(src2), "%s/src2.dbf", dir);
        snprintf(sntx, sizeof(sntx), "%s/sales.ntx", dir);
        sf[0] = DbfField_Create("NAME", (uint8_t)DbfFieldType_Character, 10, 0);
        sf[1] = DbfField_Create("QTY", (uint8_t)DbfFieldType_Numeric, 6, 0);
        if (DbfFile_CreateNew(sales, "windows-1252", sf, 2) != GMB_DB_OK) return -71;
        s = DbfEngine_New(sales, NULL, 0, 1, LockingStrategy_IntraProcess);
        if (!s) return -72;
        memset(pay, ' ', sizeof(pay));
        memcpy(pay, "AAA       ", 10);
        memcpy(pay + 10, "    10", 6);
        if (DbfEngine_AppendRecord(s, pay, (int)(DbfFile_get_Header(s->dbfFile)->RecordLength - 1)) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -73;
        }
        memcpy(pay, "BBB       ", 10);
        memcpy(pay + 10, "     5", 6);
        DbfEngine_AppendRecord(s, pay, (int)(DbfFile_get_Header(s->dbfFile)->RecordLength - 1));
        memcpy(pay, "AAA       ", 10);
        memcpy(pay + 10, "     3", 6);
        DbfEngine_AppendRecord(s, pay, (int)(DbfFile_get_Header(s->dbfFile)->RecordLength - 1));
        if (DbfEngine_IndexOn(s, sntx, "NAME", 10, 0, NULL, NULL, NULL) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -74;
        }
        if (DbfEngine_SetFilter(s, "NAME='BBB'") != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -75;
        }
        if (DbfEngine_GetFieldText(s, 0, text, sizeof(text)) != GMB_DB_OK || strcmp(text, "BBB") != 0) {
            DbfEngine_Dispose(s);
            return -76;
        }
        DbfEngine_SkipInternal(s, 1);
        if (!DbfEngine_get_Eof(s)) {
            DbfEngine_Dispose(s);
            return -77;
        }
        DbfEngine_SetFilter(s, NULL);
        sums[0] = "QTY";
        if (DbfEngine_TotalOn(s, tot, "NAME", sums, 1) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -78;
        }
        t = DbfEngine_New(tot, NULL, 0, 0, LockingStrategy_IntraProcess);
        if (!t) {
            DbfEngine_Dispose(s);
            return -79;
        }
        DbfEngine_GoTop(t);
        if (DbfEngine_GetFieldText(t, 0, text, sizeof(text)) != GMB_DB_OK || strcmp(text, "AAA") != 0) {
            DbfEngine_Dispose(t);
            DbfEngine_Dispose(s);
            return -80;
        }
        if (DbfEngine_GetFieldText(t, 1, text, sizeof(text)) != GMB_DB_OK || strtod(text, NULL) != 13.0) {
            DbfEngine_Dispose(t);
            DbfEngine_Dispose(s);
            return -81;
        }
        DbfEngine_Dispose(t);
        if (DbfEngine_ExportToCsv(s, csvp) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -82;
        }
        if (DbfFile_CloneStructure(s->dbfFile, src2) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -83;
        }
        app = DbfEngine_New(src2, NULL, 0, 1, LockingStrategy_IntraProcess);
        if (!app || DbfEngine_AppendFrom(app, sales) != GMB_DB_OK || DbfEngine_GetTotalRecordCount(app) != 3) {
            if (app) DbfEngine_Dispose(app);
            DbfEngine_Dispose(s);
            return -84;
        }
        DbfEngine_Dispose(app);
        if (DbfEngine_SetReplication(s, "DEFAULT") != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -85;
        }
        memcpy(pay, "CCC       ", 10);
        memcpy(pay + 10, "     1", 6);
        if (DbfEngine_AppendRecord(s, pay, (int)(DbfFile_get_Header(s->dbfFile)->RecordLength - 1)) != GMB_DB_OK) {
            DbfEngine_Dispose(s);
            return -86;
        }
        snprintf(replp, sizeof(replp), "%s.repl", sales);
        lf = fopen(replp, "r");
        if (!lf) {
            DbfEngine_Dispose(s);
            return -87;
        }
        if (!fgets(line, (int)sizeof(line), lf) || strncmp(line, "APPEND\t", 7) != 0) {
            fclose(lf);
            DbfEngine_Dispose(s);
            return -88;
        }
        fclose(lf);
        {
            char jsonp[PATH_MAX], destp[PATH_MAX], ckpt[PATH_MAX], shipck[PATH_MAX];
            DbfEngine *imp, *fol, *recv;
            pthread_t th;
            int port, shipped, recvd;
            snprintf(jsonp, sizeof(jsonp), "%s/sales.json", dir);
            snprintf(destp, sizeof(destp), "%s/imp.dbf", dir);
            if (DbfEngine_ExportToJson(s, jsonp) != GMB_DB_OK) {
                DbfEngine_Dispose(s);
                return -89;
            }
            if (DbfFile_CloneStructure(s->dbfFile, destp) != GMB_DB_OK) {
                DbfEngine_Dispose(s);
                return -90;
            }
            imp = DbfEngine_New(destp, NULL, 0, 1, LockingStrategy_IntraProcess);
            if (!imp || DbfEngine_ImportFromJson(imp, jsonp) != GMB_DB_OK ||
                DbfEngine_GetTotalRecordCount(imp) != DbfEngine_GetTotalRecordCount(s)) {
                if (imp) DbfEngine_Dispose(imp);
                DbfEngine_Dispose(s);
                return -91;
            }
            DbfEngine_Dispose(imp);
            snprintf(destp, sizeof(destp), "%s/fol.dbf", dir);
            if (DbfFile_CloneStructure(s->dbfFile, destp) != GMB_DB_OK) {
                DbfEngine_Dispose(s);
                return -92;
            }
            fol = DbfEngine_New(destp, NULL, 0, 1, LockingStrategy_IntraProcess);
            snprintf(ckpt, sizeof(ckpt), "%s.ckpt", replp);
            if (!fol || DbfEngine_FollowReplication(fol, replp, ckpt) < 1) {
                if (fol) DbfEngine_Dispose(fol);
                DbfEngine_Dispose(s);
                return -93;
            }
            if (DbfEngine_FollowReplication(fol, replp, ckpt) != 0) {
                DbfEngine_Dispose(fol);
                DbfEngine_Dispose(s);
                return -94;
            }
            DbfEngine_Dispose(fol);
            snprintf(destp, sizeof(destp), "%s/tcp.dbf", dir);
            if (DbfFile_CloneStructure(s->dbfFile, destp) != GMB_DB_OK) {
                DbfEngine_Dispose(s);
                return -95;
            }
            recv = DbfEngine_New(destp, NULL, 0, 1, LockingStrategy_IntraProcess);
            if (!recv) {
                DbfEngine_Dispose(s);
                return -96;
            }
            port = ReplicationTcpProtocol_AllocateLoopbackPort();
            if (port <= 0) {
                DbfEngine_Dispose(recv);
                DbfEngine_Dispose(s);
                return -97;
            }
            {
                EngRecvJob* job = (EngRecvJob*)malloc(sizeof(EngRecvJob));
                if (!job) {
                    DbfEngine_Dispose(recv);
                    DbfEngine_Dispose(s);
                    return -98;
                }
                job->e = recv;
                job->port = port;
                job->applied = -1;
                if (pthread_create(&th, NULL, eng_sc_recv, job) != 0) {
                    free(job);
                    DbfEngine_Dispose(recv);
                    DbfEngine_Dispose(s);
                    return -99;
                }
                usleep(80000);
                snprintf(shipck, sizeof(shipck), "%s.ship.ckpt", replp);
                shipped = DbfEngine_ReplicationShipTcp(s, "127.0.0.1", port, replp, shipck);
                pthread_join(th, NULL);
                recvd = job->applied;
                free(job);
                if (shipped < 1 || recvd < 1) {
                    DbfEngine_Dispose(recv);
                    DbfEngine_Dispose(s);
                    return -100;
                }
            }
            DbfEngine_Dispose(recv);
            eng_sc_rm(jsonp);
            eng_sc_rm(ckpt);
            eng_sc_rm(shipck);
            {
                char extra[PATH_MAX];
                snprintf(extra, sizeof(extra), "%s/imp.dbf", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/imp.fpt", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/fol.dbf", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/fol.fpt", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/tcp.dbf", dir);
                eng_sc_rm(extra);
                snprintf(extra, sizeof(extra), "%s/tcp.fpt", dir);
                eng_sc_rm(extra);
            }
        }
        DbfEngine_Dispose(s);
        eng_sc_rm(sales);
        eng_sc_rm(tot);
        eng_sc_rm(csvp);
        eng_sc_rm(src2);
        eng_sc_rm(replp);
        eng_sc_rm(sntx);
        {
            char salesFpt[PATH_MAX];
            snprintf(salesFpt, sizeof(salesFpt), "%s/sales.fpt", dir);
            eng_sc_rm(salesFpt);
            snprintf(salesFpt, sizeof(salesFpt), "%s/tot.fpt", dir);
            eng_sc_rm(salesFpt);
            snprintf(salesFpt, sizeof(salesFpt), "%s/src2.fpt", dir);
            eng_sc_rm(salesFpt);
        }
    }

    eng_sc_rm(ntx);
    eng_sc_rm(ndx);
    eng_sc_rm(fpt);
    eng_sc_rm(dbf);
    eng_sc_rm(copy);
    eng_sc_rm(sorted);
    eng_sc_rm(wal);
    {
        char copyFpt[PATH_MAX], sortFpt[PATH_MAX];
        snprintf(copyFpt, sizeof(copyFpt), "%s/copy.fpt", dir);
        snprintf(sortFpt, sizeof(sortFpt), "%s/sort.fpt", dir);
        eng_sc_rm(copyFpt);
        eng_sc_rm(sortFpt);
    }
    rmdir(dir);
    {
        int rdd = DbfDriver_RddSelfCheck();
        if (rdd != GMB_DB_OK) return rdd;
    }
    return GMB_DB_OK;
}

/* JOIN/RELATION: DbfDriver_SetRelation / SyncRelation / NestedLoopJoin (non DbfEngine, non DataSession). */
