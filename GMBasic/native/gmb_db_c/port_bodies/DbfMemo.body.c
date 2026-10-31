#include <pthread.h>
#include <unistd.h>
#include <limits.h>

#define GMB_MEMO_DBT_BLOCK 512
#define GMB_MEMO_FPT_HEADER 512
#define GMB_MEMO_BLOCK_HDR 8
#define GMB_MEMO_TERM 0x1A
#define GMB_MEMO_FREE_CAP 64
#define GMB_MEMO_MAX (16 * 1024 * 1024) /* ponytail: C# MemoryStream unbounded; ceiling 16MB */

typedef enum {
    MemoKind_Null = 0,
    MemoKind_Db3 = 1,
    MemoKind_Db4 = 2,
    MemoKind_Fpt = 3
} MemoKind;

struct MemoFile {
    MemoKind Kind;
    FILE* Stream;
    pthread_mutex_t Lock;
    int LockInited;
    int Writable;
    uint16_t BlockSize; /* FPT; DB3/DB4 use GMB_MEMO_DBT_BLOCK */
    uint32_t FreeBlocks[GMB_MEMO_FREE_CAP];
    int FreeCount;
};

static uint16_t memo_be16(const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t memo_be32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t memo_le32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void memo_wr_be32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void memo_wr_le32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int memo_exists(const char* path)
{
    return path && access(path, F_OK) == 0;
}

static long memo_len(FILE* f)
{
    long cur, end;
    if (!f) return 0;
    cur = ftell(f);
    if (cur < 0) cur = 0;
    if (fseek(f, 0, SEEK_END) != 0) return 0;
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end < 0 ? 0 : end;
}

static int memo_set_len(FILE* f, long len)
{
    if (!f || len < 0) return -1;
    fflush(f);
    if (ftruncate(fileno(f), len) != 0) return -1;
    return 0;
}

static int memo_write_zeros(FILE* f, int n)
{
    uint8_t z[512];
    memset(z, 0, sizeof(z));
    while (n > 0) {
        int chunk = n > (int)sizeof(z) ? (int)sizeof(z) : n;
        if (fwrite(z, 1, (size_t)chunk, f) != (size_t)chunk) return -1;
        n -= chunk;
    }
    return 0;
}

/* Path.ChangeExtension(path, null): strip last '.' in the filename. */
static void memo_stem(const char* path, char* out, size_t cap)
{
    const char* slash;
    const char* base;
    const char* dot;
    size_t n;
    if (!path || !out || cap == 0) return;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    n = dot ? (size_t)(dot - path) : strlen(path);
    if (n >= cap) n = cap - 1;
    memcpy(out, path, n);
    out[n] = 0;
}

static int memo_ext_is(const char* path, const char* ext)
{
    const char* slash;
    const char* dot;
    if (!path || !ext) return 0;
    slash = strrchr(path, '/');
    dot = strrchr(slash ? slash + 1 : path, '.');
    return dot && gmb_strcasecmp(dot, ext) == 0;
}

static MemoFile* memo_null_new(void)
{
    MemoFile* m = (MemoFile*)calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->Kind = MemoKind_Null;
    return m;
}

static void memo_lock_init(MemoFile* m)
{
    if (pthread_mutex_init(&m->Lock, NULL) == 0)
        m->LockInited = 1;
}

static FILE* memo_fopen(const char* path, int writable)
{
    int exists = memo_exists(path);
    if (!exists && !writable) return NULL;
    if (exists)
        return fopen(path, writable ? "r+b" : "rb");
    return fopen(path, "w+b");
}

static void memo_stem_join(const char* stem, const char* ext, char* out, size_t cap)
{
    snprintf(out, cap, "%s%s", stem, ext);
}

static int memo_create_empty(const char* memoPath, DbfVersion dbfVersion)
{
    FILE* f;
    uint8_t header[GMB_MEMO_FPT_HEADER];
    if (!memoPath) return -1;
    if (dbfVersion == DbfVersion_VisualFoxPro || dbfVersion == DbfVersion_VisualFoxProWithMemo) {
        f = fopen(memoPath, "wb");
        if (!f) return -1;
        memset(header, 0, sizeof(header));
        header[6] = 0x40;
        header[7] = 0x00;
        if (fwrite(header, 1, sizeof(header), f) != sizeof(header)) {
            fclose(f);
            return -1;
        }
        fclose(f);
        return 0;
    }
    f = fopen(memoPath, "wb");
    if (!f) return -1;
    fclose(f);
    return 0;
}

static void fpt_load_free_blocks(MemoFile* m)
{
    uint8_t hdr[4];
    uint8_t entry[8];
    uint32_t nextFree;
    uint32_t visited[GMB_MEMO_FREE_CAP];
    int nVisited = 0;
    int i, seen;
    long flen;
    if (!m->Stream) return;
    if (fseek(m->Stream, 0, SEEK_SET) != 0) return;
    if (fread(hdr, 1, 4, m->Stream) != 4) return;
    nextFree = memo_be32(hdr);
    flen = memo_len(m->Stream);
    while (nextFree > 0
           && (int64_t)nextFree * (int64_t)m->BlockSize < flen
           && nVisited < GMB_MEMO_FREE_CAP) {
        seen = 0;
        for (i = 0; i < nVisited; i++) {
            if (visited[i] == nextFree) {
                seen = 1;
                break;
            }
        }
        if (seen) break;
        visited[nVisited++] = nextFree;
        if (m->FreeCount < GMB_MEMO_FREE_CAP)
            m->FreeBlocks[m->FreeCount++] = nextFree;
        if (fseek(m->Stream, (long)nextFree * (long)m->BlockSize, SEEK_SET) != 0) break;
        if (fread(entry, 1, 8, m->Stream) != 8) break;
        /* C#: BitConverter.ToUInt32(entry) — little-endian, even though FreeBlock writes BE */
        nextFree = memo_le32(entry);
    }
}

static MemoFile* memo_open_kind(MemoKind kind, const char* path, int writable)
{
    MemoFile* m;
    uint8_t header[GMB_MEMO_FPT_HEADER];
    long flen;
    m = (MemoFile*)calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->Kind = kind;
    m->Writable = writable;
    m->BlockSize = (kind == MemoKind_Fpt) ? 0 : (uint16_t)GMB_MEMO_DBT_BLOCK;
    memo_lock_init(m);

    if (kind != MemoKind_Fpt && !memo_exists(path) && !writable)
        return m;

    m->Stream = memo_fopen(path, writable);
    if (!m->Stream)
        return m;

    if (kind != MemoKind_Fpt)
        return m;

    flen = memo_len(m->Stream);
    if (flen < GMB_MEMO_FPT_HEADER) {
        if (writable) {
            memset(header, 0, sizeof(header));
            header[6] = 0x40;
            header[7] = 0x00;
            if (fseek(m->Stream, 0, SEEK_SET) != 0
                || fwrite(header, 1, sizeof(header), m->Stream) != sizeof(header))
                return m;
        } else {
            return m; /* _blockSize stays 0 — GetMemo null */
        }
    }

    if (fseek(m->Stream, 0, SEEK_SET) != 0) return m;
    if (fread(header, 1, GMB_MEMO_FPT_HEADER, m->Stream) != GMB_MEMO_FPT_HEADER) return m;
    m->BlockSize = memo_be16(header + 6);
    if (m->BlockSize == 0) m->BlockSize = 64;
    if (writable)
        fpt_load_free_blocks(m);
    return m;
}

MemoFile* DbfMemoFactory_Create(const char* dbfFilePath, DbfVersion dbfVersion, int createIfMissing)
{
    char stem[PATH_MAX];
    char fpt[PATH_MAX];
    char dbt[PATH_MAX];
    char chosen[PATH_MAX];
    int have = 0;

    if (!dbfFilePath) return memo_null_new();
    if (!DbfVersionExtensions_SupportsMemoFields(dbfVersion))
        return memo_null_new();

    memo_stem(dbfFilePath, stem, sizeof(stem));
    memo_stem_join(stem, ".fpt", fpt, sizeof(fpt));
    memo_stem_join(stem, ".dbt", dbt, sizeof(dbt));
    if (memo_exists(fpt)) {
        snprintf(chosen, sizeof(chosen), "%s", fpt);
        have = 1;
    } else if (memo_exists(dbt)) {
        snprintf(chosen, sizeof(chosen), "%s", dbt);
        have = 1;
    }

    if (!have && createIfMissing) {
        if (dbfVersion == DbfVersion_VisualFoxPro || dbfVersion == DbfVersion_VisualFoxProWithMemo)
            snprintf(chosen, sizeof(chosen), "%s", fpt);
        else
            snprintf(chosen, sizeof(chosen), "%s", dbt);
        if (memo_create_empty(chosen, dbfVersion) != 0)
            return memo_null_new();
        have = 1;
    }

    if (!have)
        return memo_null_new();

    if (memo_ext_is(chosen, ".fpt"))
        return memo_open_kind(MemoKind_Fpt, chosen, createIfMissing);
    if (memo_ext_is(chosen, ".dbt")) {
        if (dbfVersion == DbfVersion_DBase3PlusWithMemo)
            return memo_open_kind(MemoKind_Db3, chosen, createIfMissing);
        return memo_open_kind(MemoKind_Db4, chosen, createIfMissing);
    }
    return memo_null_new();
}

void MemoData_Dispose(MemoData* m)
{
    if (!m) return;
    free(m->Data);
    m->Data = NULL;
    m->Length = 0;
    m->IsText = 0;
}

void MemoFile_Dispose(MemoFile* self)
{
    if (!self) return;
    if (self->Stream) fclose(self->Stream);
    if (self->LockInited) pthread_mutex_destroy(&self->Lock);
    free(self);
}

int MemoFile_IsNull(const MemoFile* self)
{
    return !self || self->Kind == MemoKind_Null;
}

static int dbt_allocate(MemoFile* self)
{
    long len = memo_len(self->Stream);
    long rem;
    if (len == 0) {
        /* ponytail: blocco 0 riservato per compat dBase III */
        if (memo_set_len(self->Stream, GMB_MEMO_DBT_BLOCK) != 0) return 0;
        return 1;
    }
    rem = len % GMB_MEMO_DBT_BLOCK;
    if (rem != 0) {
        if (memo_set_len(self->Stream, len + (GMB_MEMO_DBT_BLOCK - rem)) != 0) return 0;
        len = memo_len(self->Stream);
    }
    return (int)(len / GMB_MEMO_DBT_BLOCK);
}

static int fpt_allocate(MemoFile* self)
{
    long len;
    long rem;
    long header = GMB_MEMO_FPT_HEADER;
    if (self->FreeCount > 0)
        return (int)self->FreeBlocks[--self->FreeCount];

    len = memo_len(self->Stream);
    if (len <= header) {
        if (memo_set_len(self->Stream, header + (long)self->BlockSize) != 0) return 0;
        return 1;
    }
    rem = (len - header) % (long)self->BlockSize;
    if (rem != 0) {
        if (memo_set_len(self->Stream, len + ((long)self->BlockSize - rem)) != 0) return 0;
        len = memo_len(self->Stream);
    }
    /* C#: (Length - HeaderSize) / blockSize — aligned files reuse the last index (not last+1). */
    return (int)((len - header) / (long)self->BlockSize);
}

static int db3_get(MemoFile* self, int index, MemoData* out)
{
    long position;
    long flen;
    uint8_t buf[GMB_MEMO_DBT_BLOCK];
    uint8_t* acc = NULL;
    int accLen = 0;
    size_t n;
    uint8_t* term;
    if (!self->Stream || index <= 0) return GMB_DB_ERR;
    position = (long)index * GMB_MEMO_DBT_BLOCK;
    flen = memo_len(self->Stream);
    if (position >= flen) return GMB_DB_ERR;
    if (fseek(self->Stream, position, SEEK_SET) != 0) return GMB_DB_ERR;
    for (;;) {
        n = fread(buf, 1, GMB_MEMO_DBT_BLOCK, self->Stream);
        if (n == 0) break;
        term = (uint8_t*)memchr(buf, GMB_MEMO_TERM, n);
        if (term) {
            int chunk = (int)(term - buf);
            if (accLen + chunk > GMB_MEMO_MAX) {
                free(acc);
                return GMB_DB_ERR;
            }
            if (chunk > 0) {
                uint8_t* nbuf = (uint8_t*)realloc(acc, (size_t)accLen + (size_t)chunk);
                if (!nbuf) {
                    free(acc);
                    return GMB_DB_ERR;
                }
                acc = nbuf;
                memcpy(acc + accLen, buf, (size_t)chunk);
                accLen += chunk;
            }
            break;
        }
        if (accLen + (int)n > GMB_MEMO_MAX) {
            free(acc);
            return GMB_DB_ERR;
        }
        {
            uint8_t* nbuf = (uint8_t*)realloc(acc, (size_t)accLen + n);
            if (!nbuf) {
                free(acc);
                return GMB_DB_ERR;
            }
            acc = nbuf;
            memcpy(acc + accLen, buf, n);
            accLen += (int)n;
        }
    }
    if (accLen <= 0) {
        free(acc);
        return GMB_DB_ERR;
    }
    out->Data = acc;
    out->Length = accLen;
    out->IsText = 1;
    return GMB_DB_OK;
}

static int db3_write(MemoFile* self, const uint8_t* data, int len)
{
    int blockIndex;
    uint8_t* bytes;
    int total;
    int offset;
    uint8_t block[GMB_MEMO_DBT_BLOCK];
    if (!self->Stream || !data || len <= 0) return 0;
    blockIndex = dbt_allocate(self);
    if (blockIndex <= 0) return 0;
    if (fseek(self->Stream, (long)blockIndex * GMB_MEMO_DBT_BLOCK, SEEK_SET) != 0) return 0;
    total = len + 1;
    bytes = (uint8_t*)malloc((size_t)total);
    if (!bytes) return 0;
    memcpy(bytes, data, (size_t)len);
    bytes[len] = GMB_MEMO_TERM;
    for (offset = 0; offset < total; offset += GMB_MEMO_DBT_BLOCK) {
        int chunk = total - offset;
        if (chunk > GMB_MEMO_DBT_BLOCK) chunk = GMB_MEMO_DBT_BLOCK;
        if (chunk < GMB_MEMO_DBT_BLOCK) {
            memset(block, 0, sizeof(block));
            memcpy(block, bytes + offset, (size_t)chunk);
            if (fwrite(block, 1, GMB_MEMO_DBT_BLOCK, self->Stream) != GMB_MEMO_DBT_BLOCK) {
                free(bytes);
                return 0;
            }
        } else if (fwrite(bytes + offset, 1, (size_t)chunk, self->Stream) != (size_t)chunk) {
            free(bytes);
            return 0;
        }
    }
    free(bytes);
    fflush(self->Stream);
    return blockIndex;
}

static int db4_get(MemoFile* self, int index, MemoData* out)
{
    long position;
    long flen;
    uint8_t header[8];
    uint32_t length;
    uint8_t* data;
    if (!self->Stream || index <= 0) return GMB_DB_ERR;
    position = (long)index * GMB_MEMO_DBT_BLOCK;
    flen = memo_len(self->Stream);
    if (position + 8 > flen) return GMB_DB_ERR;
    if (fseek(self->Stream, position, SEEK_SET) != 0) return GMB_DB_ERR;
    if (fread(header, 1, 8, self->Stream) != 8) return GMB_DB_ERR;
    length = memo_le32(header + 4);
    if (length == 0 || (long)length > flen - (position + 8) || length > (uint32_t)GMB_MEMO_MAX)
        return GMB_DB_ERR;
    data = (uint8_t*)malloc(length);
    if (!data) return GMB_DB_ERR;
    if (fread(data, 1, length, self->Stream) != length) {
        free(data);
        return GMB_DB_ERR;
    }
    out->Data = data;
    out->Length = (int)length;
    out->IsText = 1;
    return GMB_DB_OK;
}

static int db4_write(MemoFile* self, const uint8_t* data, int len)
{
    int blockIndex;
    uint8_t header[8];
    int used;
    int padding;
    uint8_t pad[GMB_MEMO_DBT_BLOCK];
    if (!self->Stream) return 0;
    if (len < 0) return 0;
    if (len > 0 && !data) return 0;
    blockIndex = dbt_allocate(self);
    if (blockIndex <= 0) return 0;
    if (fseek(self->Stream, (long)blockIndex * GMB_MEMO_DBT_BLOCK, SEEK_SET) != 0) return 0;
    memset(header, 0, sizeof(header));
    memo_wr_le32(header + 4, (uint32_t)len);
    if (fwrite(header, 1, 8, self->Stream) != 8) return 0;
    if (len > 0 && fwrite(data, 1, (size_t)len, self->Stream) != (size_t)len) return 0;
    used = 8 + len;
    padding = (GMB_MEMO_DBT_BLOCK - (used % GMB_MEMO_DBT_BLOCK)) % GMB_MEMO_DBT_BLOCK;
    if (padding > 0) {
        memset(pad, 0, (size_t)padding);
        if (fwrite(pad, 1, (size_t)padding, self->Stream) != (size_t)padding) return 0;
    }
    fflush(self->Stream);
    return blockIndex;
}

static int fpt_get(MemoFile* self, int index, MemoData* out)
{
    long position;
    long flen;
    uint8_t hdr[GMB_MEMO_BLOCK_HDR];
    uint32_t type;
    uint32_t length;
    uint8_t* data;
    if (!self->Stream || self->BlockSize == 0 || index <= 0) return GMB_DB_ERR;
    position = (long)index * (long)self->BlockSize;
    flen = memo_len(self->Stream);
    if (position + GMB_MEMO_BLOCK_HDR > flen) return GMB_DB_ERR;
    if (fseek(self->Stream, position, SEEK_SET) != 0) return GMB_DB_ERR;
    if (fread(hdr, 1, GMB_MEMO_BLOCK_HDR, self->Stream) != GMB_MEMO_BLOCK_HDR) return GMB_DB_ERR;
    type = memo_be32(hdr);
    length = memo_be32(hdr + 4);
    if (length == 0 || (long)length > flen - (position + GMB_MEMO_BLOCK_HDR)
        || length > (uint32_t)GMB_MEMO_MAX)
        return GMB_DB_ERR;
    data = (uint8_t*)malloc(length);
    if (!data) return GMB_DB_ERR;
    if (fread(data, 1, length, self->Stream) != length) {
        free(data);
        return GMB_DB_ERR;
    }
    out->Data = data;
    out->Length = (int)length;
    out->IsText = type == 1 ? 1 : 0;
    return GMB_DB_OK;
}

static int fpt_write(MemoFile* self, const uint8_t* data, int len, int isText)
{
    int blockIndex;
    long pos;
    uint8_t hdr[GMB_MEMO_BLOCK_HDR];
    int used;
    int padding;
    if (!self->Stream || !self->Writable) return 0;
    if (len < 0) return 0;
    if (len > 0 && !data) return 0;
    if (self->BlockSize == 0) return 0;
    blockIndex = fpt_allocate(self);
    if (blockIndex <= 0) return 0;
    pos = (long)blockIndex * (long)self->BlockSize;
    /* C#: Windows FileStream.Lock retry; skipped on POSIX (OperatingSystem.IsWindows). */
    if (fseek(self->Stream, pos, SEEK_SET) != 0) return 0;
    memset(hdr, 0, sizeof(hdr));
    memo_wr_be32(hdr, isText ? 1u : 0u);
    memo_wr_be32(hdr + 4, (uint32_t)len);
    if (fwrite(hdr, 1, GMB_MEMO_BLOCK_HDR, self->Stream) != GMB_MEMO_BLOCK_HDR) return 0;
    if (len > 0 && fwrite(data, 1, (size_t)len, self->Stream) != (size_t)len) return 0;
    used = GMB_MEMO_BLOCK_HDR + len;
    padding = ((int)self->BlockSize - (used % (int)self->BlockSize)) % (int)self->BlockSize;
    if (padding > 0 && memo_write_zeros(self->Stream, padding) != 0) return 0;
    fflush(self->Stream);
    return blockIndex;
}

int MemoFile_GetMemo(MemoFile* self, int index, MemoData* out)
{
    int rc;
    if (!self || !out) return GMB_DB_ARG;
    memset(out, 0, sizeof(*out));
    if (self->Kind == MemoKind_Null) return GMB_DB_ERR;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    switch (self->Kind) {
    case MemoKind_Db3:
        rc = db3_get(self, index, out);
        break;
    case MemoKind_Db4:
        rc = db4_get(self, index, out);
        break;
    case MemoKind_Fpt:
        rc = fpt_get(self, index, out);
        break;
    default:
        rc = GMB_DB_ERR;
        break;
    }
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rc;
}

int MemoFile_WriteMemo(MemoFile* self, const uint8_t* data, int len, int isText)
{
    int rc = 0;
    if (!self) return 0;
    if (self->Kind == MemoKind_Null) return 0;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    switch (self->Kind) {
    case MemoKind_Db3:
        rc = isText ? db3_write(self, data, len) : 0;
        break;
    case MemoKind_Db4:
        rc = isText ? db4_write(self, data, len) : 0;
        break;
    case MemoKind_Fpt:
        rc = fpt_write(self, data, len, isText);
        break;
    default:
        rc = 0;
        break;
    }
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rc;
}

void FptFile_FreeBlock(MemoFile* self, int blockIndex)
{
    uint8_t hdr[4];
    uint8_t entry[8];
    uint32_t oldNextFree;
    long pos;
    if (!self || self->Kind != MemoKind_Fpt || !self->Stream || !self->Writable || blockIndex <= 0)
        return;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (fseek(self->Stream, 0, SEEK_SET) != 0) goto done;
    if (fread(hdr, 1, 4, self->Stream) != 4) goto done;
    oldNextFree = memo_be32(hdr);
    pos = (long)blockIndex * (long)self->BlockSize;
    memset(entry, 0, sizeof(entry));
    memo_wr_be32(entry, oldNextFree);
    if (fseek(self->Stream, pos, SEEK_SET) != 0) goto done;
    if (fwrite(entry, 1, 8, self->Stream) != 8) goto done;
    memo_wr_be32(hdr, (uint32_t)blockIndex);
    if (fseek(self->Stream, 0, SEEK_SET) != 0) goto done;
    if (fwrite(hdr, 1, 4, self->Stream) != 4) goto done;
    fflush(self->Stream);
    if (self->FreeCount < GMB_MEMO_FREE_CAP)
        self->FreeBlocks[self->FreeCount++] = (uint32_t)blockIndex;
done:
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
}

static void memo_sc_rm(const char* path)
{
    if (path) unlink(path);
}

int DbfMemo_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/memosc_XXXXXX";
    char dbf[PATH_MAX];
    char fpt[PATH_MAX];
    char dbt[PATH_MAX];
    char dbf64[PATH_MAX];
    char fpt64[PATH_MAX];
    MemoFile *m, *reopen;
    MemoData md;
    FILE* f;
    uint8_t head[96];
    size_t nread;
    long flen;
    int idx, idx2;
    const uint8_t hello[] = { 'h', 'e', 'l', 'l', 'o' };
    const uint8_t bin[] = { 0x00, 0x01, 0xFF };

    if (!mkdtemp(dir)) return -2;

    snprintf(dbf, sizeof(dbf), "%s/t.dbf", dir);
    m = DbfMemoFactory_Create(dbf, DbfVersion_DBase2, 1);
    if (!m) {
        rmdir(dir);
        return -3;
    }
    if (MemoFile_WriteMemo(m, hello, 5, 1) != 0) {
        MemoFile_Dispose(m);
        rmdir(dir);
        return -4;
    }
    if (MemoFile_GetMemo(m, 1, &md) != GMB_DB_ERR) {
        MemoData_Dispose(&md);
        MemoFile_Dispose(m);
        rmdir(dir);
        return -5;
    }
    MemoFile_Dispose(m);

    /* FPT CreateEmpty: header[6]=0x40, header[7]=0x00 → block size BE 16384, not 64. */
    m = DbfMemoFactory_Create(dbf, DbfVersion_VisualFoxProWithMemo, 1);
    if (!m) {
        rmdir(dir);
        return -6;
    }
    snprintf(fpt, sizeof(fpt), "%s/t.fpt", dir);
    f = fopen(fpt, "rb");
    if (!f) {
        MemoFile_Dispose(m);
        rmdir(dir);
        return -7;
    }
    nread = fread(head, 1, 512, f);
    fseek(f, 0, SEEK_END);
    flen = ftell(f);
    fclose(f);
    if (nread != 512 || flen != 512 || head[6] != 0x40 || head[7] != 0x00) {
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -8;
    }

    idx = MemoFile_WriteMemo(m, hello, 5, 1);
    if (idx != 1) {
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -9;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(m, 1, &md) != GMB_DB_OK || md.Length != 5 || md.IsText != 1
        || memcmp(md.Data, hello, 5) != 0) {
        MemoData_Dispose(&md);
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -10;
    }
    MemoData_Dispose(&md);

    f = fopen(fpt, "rb");
    if (!f) {
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -11;
    }
    if (fseek(f, 16384, SEEK_SET) != 0 || fread(head, 1, 16, f) != 16) {
        fclose(f);
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -11;
    }
    fseek(f, 0, SEEK_END);
    flen = ftell(f);
    fclose(f);
    if (head[0] != 0 || head[1] != 0 || head[2] != 0 || head[3] != 1
        || head[4] != 0 || head[5] != 0 || head[6] != 0 || head[7] != 5
        || memcmp(head + 8, hello, 5) != 0 || flen != 32768) {
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -12;
    }

    idx2 = MemoFile_WriteMemo(m, bin, 3, 0);
    if (idx2 != 2) {
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -13;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(m, 2, &md) != GMB_DB_OK || md.Length != 3 || md.IsText != 0
        || memcmp(md.Data, bin, 3) != 0) {
        MemoData_Dispose(&md);
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -14;
    }
    MemoData_Dispose(&md);
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(m, 1, &md) != GMB_DB_OK || md.Length != 5 || !md.IsText) {
        MemoData_Dispose(&md);
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -14;
    }
    MemoData_Dispose(&md);
    MemoFile_Dispose(m);

    reopen = DbfMemoFactory_Create(dbf, DbfVersion_VisualFoxProWithMemo, 0);
    if (!reopen) {
        memo_sc_rm(fpt);
        rmdir(dir);
        return -15;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(reopen, 1, &md) != GMB_DB_OK || md.Length != 5 || md.IsText != 1) {
        MemoData_Dispose(&md);
        MemoFile_Dispose(reopen);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -16;
    }
    MemoData_Dispose(&md);
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(reopen, 2, &md) != GMB_DB_OK || md.Length != 3 || md.IsText != 0) {
        MemoData_Dispose(&md);
        MemoFile_Dispose(reopen);
        memo_sc_rm(fpt);
        rmdir(dir);
        return -16;
    }
    MemoData_Dispose(&md);
    MemoFile_Dispose(reopen);
    memo_sc_rm(fpt);

    /* Block size 64 BE (00 40): write at index*64=64; second aligned Allocate returns 1 (C# quirk). */
    snprintf(dbf64, sizeof(dbf64), "%s/u.dbf", dir);
    snprintf(fpt64, sizeof(fpt64), "%s/u.fpt", dir);
    {
        uint8_t hdr512[512];
        FILE* touch = fopen(fpt64, "wb");
        if (!touch) {
            rmdir(dir);
            return -36;
        }
        memset(hdr512, 0, sizeof(hdr512));
        hdr512[6] = 0x00;
        hdr512[7] = 0x40;
        fwrite(hdr512, 1, 512, touch);
        fclose(touch);
    }
    m = DbfMemoFactory_Create(dbf64, DbfVersion_VisualFoxPro, 1);
    if (!m) {
        memo_sc_rm(fpt64);
        rmdir(dir);
        return -37;
    }
    idx = MemoFile_WriteMemo(m, hello, 5, 1);
    if (idx != 1) {
        MemoFile_Dispose(m);
        memo_sc_rm(fpt64);
        rmdir(dir);
        return -38;
    }
    f = fopen(fpt64, "rb");
    if (!f) {
        MemoFile_Dispose(m);
        memo_sc_rm(fpt64);
        rmdir(dir);
        return -39;
    }
    nread = fread(head, 1, 80, f);
    fclose(f);
    if (nread < 77 || head[6] != 0x00 || head[7] != 0x40
        || head[64] != 0 || head[65] != 0 || head[66] != 0 || head[67] != 1
        || head[68] != 0 || head[69] != 0 || head[70] != 0 || head[71] != 5
        || memcmp(head + 72, hello, 5) != 0) {
        MemoFile_Dispose(m);
        memo_sc_rm(fpt64);
        rmdir(dir);
        return -40;
    }
    idx2 = MemoFile_WriteMemo(m, bin, 3, 0);
    if (idx2 != 1) {
        MemoFile_Dispose(m);
        memo_sc_rm(fpt64);
        rmdir(dir);
        return -41;
    }
    MemoFile_Dispose(m);
    memo_sc_rm(fpt64);

    /* DB3: empty .dbt, block 0 reserved, memo at 512, terminator 0x1A, pad to 512. */
    m = DbfMemoFactory_Create(dbf, DbfVersion_DBase3PlusWithMemo, 1);
    if (!m) {
        rmdir(dir);
        return -17;
    }
    snprintf(dbt, sizeof(dbt), "%s/t.dbt", dir);
    idx = MemoFile_WriteMemo(m, hello, 5, 1);
    if (idx != 1) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -18;
    }
    if (MemoFile_WriteMemo(m, hello, 5, 0) != 0) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -19;
    }
    if (MemoFile_WriteMemo(m, hello, 0, 1) != 0) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -20;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(m, 1, &md) != GMB_DB_OK || md.Length != 5 || !md.IsText
        || memcmp(md.Data, hello, 5) != 0) {
        MemoData_Dispose(&md);
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -21;
    }
    MemoData_Dispose(&md);

    f = fopen(dbt, "rb");
    if (!f) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -22;
    }
    fseek(f, 0, SEEK_END);
    flen = ftell(f);
    fseek(f, 512, SEEK_SET);
    nread = fread(head, 1, 16, f);
    fclose(f);
    if (flen != 1024 || nread < 6 || memcmp(head, hello, 5) != 0 || head[5] != GMB_MEMO_TERM) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -23;
    }

    idx2 = MemoFile_WriteMemo(m, hello, 5, 1);
    if (idx2 != 2) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -24;
    }
    MemoFile_Dispose(m);
    memo_sc_rm(dbt);

    /* DB4: 8-byte header, length uint32 LE @+4. */
    m = DbfMemoFactory_Create(dbf, DbfVersion_DBase4WithMemo, 1);
    if (!m) {
        rmdir(dir);
        return -25;
    }
    idx = MemoFile_WriteMemo(m, hello, 5, 1);
    if (idx != 1) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -26;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(m, 1, &md) != GMB_DB_OK || md.Length != 5
        || memcmp(md.Data, hello, 5) != 0) {
        MemoData_Dispose(&md);
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -27;
    }
    MemoData_Dispose(&md);

    f = fopen(dbt, "rb");
    if (!f) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -28;
    }
    fseek(f, 512, SEEK_SET);
    nread = fread(head, 1, 16, f);
    fclose(f);
    if (nread < 13 || head[0] != 0 || head[1] != 0 || head[2] != 0 || head[3] != 0
        || head[4] != 5 || head[5] != 0 || head[6] != 0 || head[7] != 0
        || memcmp(head + 8, hello, 5) != 0) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -29;
    }
    if (MemoFile_WriteMemo(m, hello, 5, 0) != 0) {
        MemoFile_Dispose(m);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -30;
    }
    MemoFile_Dispose(m);

    reopen = DbfMemoFactory_Create(dbf, DbfVersion_DBase4WithMemo, 0);
    if (!reopen) {
        memo_sc_rm(dbt);
        rmdir(dir);
        return -31;
    }
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(reopen, 1, &md) != GMB_DB_OK || md.Length != 5) {
        MemoData_Dispose(&md);
        MemoFile_Dispose(reopen);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -32;
    }
    MemoData_Dispose(&md);
    MemoFile_Dispose(reopen);

    /* FindMemoFile: .fpt wins over .dbt. */
    {
        FILE* touch = fopen(fpt, "wb");
        uint8_t hdr512[512];
        if (!touch) {
            memo_sc_rm(dbt);
            rmdir(dir);
            return -33;
        }
        memset(hdr512, 0, sizeof(hdr512));
        hdr512[6] = 0x40;
        hdr512[7] = 0x00;
        fwrite(hdr512, 1, 512, touch);
        fclose(touch);
    }
    m = DbfMemoFactory_Create(dbf, DbfVersion_VisualFoxPro, 0);
    if (!m) {
        memo_sc_rm(fpt);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -34;
    }
    idx = MemoFile_WriteMemo(m, hello, 5, 1);
    if (idx != 0) {
        /* read-only FPT: writable=createIfMissing=0 → WriteMemo 0 */
        MemoFile_Dispose(m);
        memo_sc_rm(fpt);
        memo_sc_rm(dbt);
        rmdir(dir);
        return -35;
    }
    MemoFile_Dispose(m);
    memo_sc_rm(fpt);
    memo_sc_rm(dbt);
    rmdir(dir);
    return GMB_DB_OK;
}
