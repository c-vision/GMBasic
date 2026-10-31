#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

/* SPLIT:BloomFilter.c */

struct BloomFilter {
    uint8_t* bits;
    int bitCount;
    int byteCount;
};

#define BloomFilter_MinBitCount 64

int BloomFilter_BitsPerKeyForCount(int expectedKeyCount)
{
    if (expectedKeyCount <= 64) return 8;
    if (expectedKeyCount <= 512) return 10;
    if (expectedKeyCount <= 4096) return 12;
    return 14;
}

static uint64_t BloomFilter_Hash64(const uint8_t* key, int keyLen)
{
    const uint64_t offset = 14695981039346656037ULL;
    const uint64_t prime = 1099511628211ULL;
    uint64_t hash = offset;
    int i;
    for (i = 0; i < keyLen; i++)
        hash = (hash ^ key[i]) * prime;
    return hash;
}

static void BloomFilter_SetBit(BloomFilter* self, int bitIndex)
{
    self->bits[bitIndex >> 3] |= (uint8_t)(1 << (bitIndex & 7));
}

static int BloomFilter_GetBit(const BloomFilter* self, int bitIndex)
{
    return (self->bits[bitIndex >> 3] & (1 << (bitIndex & 7))) != 0;
}

BloomFilter* BloomFilter_New(int expectedKeyCount)
{
    BloomFilter* self = (BloomFilter*)calloc(1, sizeof(*self));
    int bitsPerKey, n;
    if (expectedKeyCount < 1) expectedKeyCount = 1;
    bitsPerKey = BloomFilter_BitsPerKeyForCount(expectedKeyCount);
    n = expectedKeyCount * bitsPerKey;
    if (n < BloomFilter_MinBitCount) n = BloomFilter_MinBitCount;
    self->bitCount = n;
    self->byteCount = (n + 7) / 8;
    self->bits = (uint8_t*)calloc(1, (size_t)self->byteCount);
    return self;
}

void BloomFilter_Dispose(BloomFilter* self)
{
    if (!self) return;
    free(self->bits);
    free(self);
}

void BloomFilter_Add(BloomFilter* self, const uint8_t* key, int keyLen)
{
    uint64_t hash = BloomFilter_Hash64(key, keyLen);
    uint32_t h1 = (uint32_t)hash, h2 = (uint32_t)(hash >> 32);
    int i;
    for (i = 0; i < BloomFilter_HashCount; i++) {
        int bitIndex = (int)((h1 + (uint32_t)i * h2) % (uint32_t)self->bitCount);
        BloomFilter_SetBit(self, bitIndex);
    }
}

int BloomFilter_MayContain(BloomFilter* self, const uint8_t* key, int keyLen)
{
    uint64_t hash = BloomFilter_Hash64(key, keyLen);
    uint32_t h1 = (uint32_t)hash, h2 = (uint32_t)(hash >> 32);
    int i;
    for (i = 0; i < BloomFilter_HashCount; i++) {
        int bitIndex = (int)((h1 + (uint32_t)i * h2) % (uint32_t)self->bitCount);
        if (!BloomFilter_GetBit(self, bitIndex)) return 0;
    }
    return 1;
}

static int wr_i32_le(FILE* f, int32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}
static int rd_i32_le(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}
static int wr_i64_le(FILE* f, int64_t v)
{
    uint8_t b[8]; int i;
    for (i = 0; i < 8; i++) b[i] = (uint8_t)((uint64_t)v >> (8 * i));
    return fwrite(b, 1, 8, f) == 8 ? 0 : -1;
}
static int rd_i64_le(FILE* f, int64_t* v)
{
    uint8_t b[8]; uint64_t u = 0; int i;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)b[i] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

void BloomFilter_WriteTo(BloomFilter* self, FILE* writer)
{
    wr_i32_le(writer, self->bitCount);
    fwrite(self->bits, 1, (size_t)self->byteCount, writer);
}

BloomFilter* BloomFilter_ReadFrom(FILE* reader)
{
    int32_t bitCount;
    BloomFilter* self;
    if (rd_i32_le(reader, &bitCount) != 0 || bitCount < BloomFilter_MinBitCount) return NULL;
    self = (BloomFilter*)calloc(1, sizeof(*self));
    self->bitCount = bitCount;
    self->byteCount = (bitCount + 7) / 8;
    self->bits = (uint8_t*)malloc((size_t)self->byteCount);
    if (fread(self->bits, 1, (size_t)self->byteCount, reader) != (size_t)self->byteCount) {
        BloomFilter_Dispose(self);
        return NULL;
    }
    return self;
}

/* SPLIT:ByteArrayEqualityComparer.c */

int ByteArrayEqualityComparer_Equals(const uint8_t* x, int xLen, const uint8_t* y, int yLen)
{
    if (x == y) return 1;
    if (!x || !y) return 0;
    if (xLen != yLen) return 0;
    return memcmp(x, y, (size_t)xLen) == 0;
}

int ByteArrayEqualityComparer_GetHashCode(const uint8_t* obj, int len)
{
    int hash = 17, i;
    for (i = 0; i < len; i++) hash = hash * 31 + obj[i];
    return hash;
}

int ByteArrayComparer_Compare(const uint8_t* x, int xLen, const uint8_t* y, int yLen)
{
    int min, i;
    if (x == y) return 0;
    if (!x) return -1;
    if (!y) return 1;
    min = xLen < yLen ? xLen : yLen;
    for (i = 0; i < min; i++) {
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    }
    if (xLen == yLen) return 0;
    return xLen < yLen ? -1 : 1;
}

/* SPLIT:Memtable.c */

typedef struct {
    uint8_t* key; int keyLen;
    uint8_t* value; int valueLen;
    int tombstone;
} MemtableEntry;

struct Memtable {
    MemtableEntry* table;
    int count, cap;
    int64_t EstimatedSize;
};

Memtable* Memtable_New(void)
{
    return (Memtable*)calloc(1, sizeof(Memtable));
}

void Memtable_Dispose(Memtable* self)
{
    int i;
    if (!self) return;
    for (i = 0; i < self->count; i++) {
        free(self->table[i].key);
        free(self->table[i].value);
    }
    free(self->table);
    free(self);
}

static int Memtable_Find(const Memtable* self, const uint8_t* key, int keyLen)
{
    int i;
    for (i = 0; i < self->count; i++)
        if (ByteArrayComparer_Compare(self->table[i].key, self->table[i].keyLen, key, keyLen) == 0)
            return i;
    return -1;
}

static int Memtable_InsertPos(const Memtable* self, const uint8_t* key, int keyLen)
{
    int i;
    for (i = 0; i < self->count; i++)
        if (ByteArrayComparer_Compare(self->table[i].key, self->table[i].keyLen, key, keyLen) > 0)
            return i;
    return self->count;
}

int Memtable_Set(Memtable* self, const uint8_t* key, int keyLen, const uint8_t* value, int valueLen)
{
    int idx;
    if (!key || !value) return GMB_DB_ARG;
    idx = Memtable_Find(self, key, keyLen);
    if (idx >= 0) {
        self->EstimatedSize -= self->table[idx].keyLen + (self->table[idx].tombstone ? 0 : self->table[idx].valueLen);
        free(self->table[idx].value);
        self->table[idx].value = (uint8_t*)malloc((size_t)valueLen);
        memcpy(self->table[idx].value, value, (size_t)valueLen);
        self->table[idx].valueLen = valueLen;
        self->table[idx].tombstone = 0;
        self->EstimatedSize += keyLen + valueLen;
        return GMB_DB_OK;
    }
    idx = Memtable_InsertPos(self, key, keyLen);
    if (self->count >= self->cap) {
        self->cap = self->cap ? self->cap * 2 : 8;
        self->table = (MemtableEntry*)realloc(self->table, (size_t)self->cap * sizeof(MemtableEntry));
    }
    memmove(self->table + idx + 1, self->table + idx, (size_t)(self->count - idx) * sizeof(MemtableEntry));
    self->table[idx].key = (uint8_t*)malloc((size_t)keyLen);
    memcpy(self->table[idx].key, key, (size_t)keyLen);
    self->table[idx].keyLen = keyLen;
    self->table[idx].value = (uint8_t*)malloc((size_t)valueLen);
    memcpy(self->table[idx].value, value, (size_t)valueLen);
    self->table[idx].valueLen = valueLen;
    self->table[idx].tombstone = 0;
    self->count++;
    self->EstimatedSize += keyLen + valueLen;
    return GMB_DB_OK;
}

int Memtable_Delete(Memtable* self, const uint8_t* key, int keyLen)
{
    int idx;
    if (!key) return GMB_DB_ARG;
    idx = Memtable_Find(self, key, keyLen);
    if (idx >= 0) {
        self->EstimatedSize -= self->table[idx].keyLen + (self->table[idx].tombstone ? 0 : self->table[idx].valueLen);
        free(self->table[idx].value);
        self->table[idx].value = NULL;
        self->table[idx].valueLen = 0;
        self->table[idx].tombstone = 1;
        self->EstimatedSize += keyLen;
        return GMB_DB_OK;
    }
    idx = Memtable_InsertPos(self, key, keyLen);
    if (self->count >= self->cap) {
        self->cap = self->cap ? self->cap * 2 : 8;
        self->table = (MemtableEntry*)realloc(self->table, (size_t)self->cap * sizeof(MemtableEntry));
    }
    memmove(self->table + idx + 1, self->table + idx, (size_t)(self->count - idx) * sizeof(MemtableEntry));
    self->table[idx].key = (uint8_t*)malloc((size_t)keyLen);
    memcpy(self->table[idx].key, key, (size_t)keyLen);
    self->table[idx].keyLen = keyLen;
    self->table[idx].value = NULL;
    self->table[idx].valueLen = 0;
    self->table[idx].tombstone = 1;
    self->count++;
    self->EstimatedSize += keyLen;
    return GMB_DB_OK;
}

int Memtable_Get(Memtable* self, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen, int* isTombstone)
{
    int idx = Memtable_Find(self, key, keyLen);
    if (idx < 0) return 0;
    *isTombstone = self->table[idx].tombstone;
    *valueLen = self->table[idx].valueLen;
    *value = self->table[idx].value;
    return 1;
}

int64_t Memtable_get_EstimatedSize(const Memtable* self) { return self->EstimatedSize; }
int Memtable_get_KeyCount(const Memtable* self) { return self->count; }

int Memtable_Nth(const Memtable* self, int index, uint8_t** key, int* keyLen, uint8_t** value, int* valueLen, int* isTombstone)
{
    if (index < 0 || index >= self->count) return GMB_DB_ARG;
    *key = self->table[index].key;
    *keyLen = self->table[index].keyLen;
    *value = self->table[index].value;
    *valueLen = self->table[index].valueLen;
    *isTombstone = self->table[index].tombstone;
    return GMB_DB_OK;
}

/* SPLIT:Wal.c */

struct SpeedWriteAheadLog {
    FILE* logStream;
    char walFilePath[PATH_MAX];
    pthread_mutex_t writerLock;
};

SpeedWriteAheadLog* SpeedWriteAheadLog_New(const char* databasePath, uint64_t logSequenceNumber)
{
    SpeedWriteAheadLog* self = (SpeedWriteAheadLog*)calloc(1, sizeof(*self));
    char walDirectory[PATH_MAX];
    snprintf(walDirectory, sizeof(walDirectory), "%s/%s", databasePath, Config_WalDirectoryName);
    mkdir(walDirectory, 0777);
    snprintf(self->walFilePath, sizeof(self->walFilePath), "%s/%06llu.wal", walDirectory,
             (unsigned long long)logSequenceNumber);
    self->logStream = fopen(self->walFilePath, "ab");
    pthread_mutex_init(&self->writerLock, NULL);
    return self;
}

void SpeedWriteAheadLog_Dispose(SpeedWriteAheadLog* self)
{
    if (!self) return;
    if (self->logStream) fclose(self->logStream);
    pthread_mutex_destroy(&self->writerLock);
    free(self);
}

const char* SpeedWriteAheadLog_GetWalPath(const SpeedWriteAheadLog* self) { return self->walFilePath; }

void SpeedWriteAheadLog_LogSet(SpeedWriteAheadLog* self, int tableId, const uint8_t* key, int keyLen,
                               const uint8_t* value, int valueLen)
{
    uint8_t op = (uint8_t)SpeedWalOperationType_Set;
    pthread_mutex_lock(&self->writerLock);
    fwrite(&op, 1, 1, self->logStream);
    wr_i32_le(self->logStream, tableId);
    wr_i32_le(self->logStream, keyLen);
    fwrite(key, 1, (size_t)keyLen, self->logStream);
    wr_i32_le(self->logStream, valueLen);
    fwrite(value, 1, (size_t)valueLen, self->logStream);
    pthread_mutex_unlock(&self->writerLock);
}

void SpeedWriteAheadLog_LogDelete(SpeedWriteAheadLog* self, int tableId, const uint8_t* key, int keyLen)
{
    uint8_t op = (uint8_t)SpeedWalOperationType_Delete;
    pthread_mutex_lock(&self->writerLock);
    fwrite(&op, 1, 1, self->logStream);
    wr_i32_le(self->logStream, tableId);
    wr_i32_le(self->logStream, keyLen);
    fwrite(key, 1, (size_t)keyLen, self->logStream);
    pthread_mutex_unlock(&self->writerLock);
}

void SpeedWriteAheadLog_Flush(SpeedWriteAheadLog* self)
{
    fflush(self->logStream);
    fsync(fileno(self->logStream));
}

int SpeedWriteAheadLog_Recover(const char* databasePath, Memtable*** memtables, int** tableIds, int* count)
{
    char walDirectory[PATH_MAX];
    DIR* dir;
    struct dirent* ent;
    char** files = NULL;
    int nfiles = 0, i, t;
    Memtable** tables = NULL;
    int* ids = NULL;
    int ntables = 0, cap = 0;

    snprintf(walDirectory, sizeof(walDirectory), "%s/%s", databasePath, Config_WalDirectoryName);
    dir = opendir(walDirectory);
    if (!dir) { *memtables = NULL; *tableIds = NULL; *count = 0; return GMB_DB_OK; }
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, ".wal") == 0) {
            files = (char**)realloc(files, (size_t)(nfiles + 1) * sizeof(char*));
            files[nfiles] = (char*)malloc(PATH_MAX);
            snprintf(files[nfiles], PATH_MAX, "%s/%s", walDirectory, ent->d_name);
            nfiles++;
        }
    }
    closedir(dir);
    /* OrderBy filename */
    for (i = 0; i < nfiles; i++)
        for (t = i + 1; t < nfiles; t++)
            if (strcmp(files[t], files[i]) < 0) {
                char* tmp = files[i]; files[i] = files[t]; files[t] = tmp;
            }
    for (i = 0; i < nfiles; i++) {
        FILE* stream = fopen(files[i], "rb");
        long end;
        if (!stream) continue;
        fseek(stream, 0, SEEK_END);
        end = ftell(stream);
        fseek(stream, 0, SEEK_SET);
        while (ftell(stream) < end) {
            uint8_t op;
            int32_t tableId, keyLength, valueLength;
            uint8_t *key, *value;
            Memtable* mem = NULL;
            int found = -1, k;
            if (fread(&op, 1, 1, stream) != 1) break;
            if (rd_i32_le(stream, &tableId) != 0) break;
            for (k = 0; k < ntables; k++) if (ids[k] == tableId) { found = k; break; }
            if (found < 0) {
                if (ntables >= cap) { cap = cap ? cap * 2 : 4; tables = (Memtable**)realloc(tables, (size_t)cap * sizeof(*tables)); ids = (int*)realloc(ids, (size_t)cap * sizeof(int)); }
                tables[ntables] = Memtable_New();
                ids[ntables] = tableId;
                found = ntables++;
            }
            mem = tables[found];
            if (rd_i32_le(stream, &keyLength) != 0) break;
            key = (uint8_t*)malloc((size_t)keyLength);
            if (keyLength > 0 && fread(key, 1, (size_t)keyLength, stream) != (size_t)keyLength) { free(key); break; }
            if (op == SpeedWalOperationType_Set) {
                if (rd_i32_le(stream, &valueLength) != 0) { free(key); break; }
                value = (uint8_t*)malloc((size_t)valueLength);
                if (valueLength > 0 && fread(value, 1, (size_t)valueLength, stream) != (size_t)valueLength) { free(key); free(value); break; }
                Memtable_Set(mem, key, keyLength, value, valueLength);
                free(value);
            } else if (op == SpeedWalOperationType_Delete) {
                Memtable_Delete(mem, key, keyLength);
            }
            free(key);
        }
        fclose(stream);
        free(files[i]);
    }
    free(files);
    *memtables = tables;
    *tableIds = ids;
    *count = ntables;
    return GMB_DB_OK;
}

/* SPLIT:SstFooter.c */

int SstFooter_Read(FILE* stream, int64_t* indexOffset, int64_t* bloomOffset, int* hasV2Footer)
{
    long len;
    *bloomOffset = 0;
    *hasV2Footer = 0;
    *indexOffset = 0;
    fseek(stream, 0, SEEK_END);
    len = ftell(stream);
    if (len < (long)sizeof(int64_t)) return GMB_DB_ERR;
    if (len >= SstFooter_V2FooterSize) {
        int64_t magic;
        fseek(stream, -SstFooter_V2FooterSize, SEEK_END);
        if (rd_i64_le(stream, &magic) == 0 && magic == SstFooter_MagicV2) {
            rd_i64_le(stream, bloomOffset);
            rd_i64_le(stream, indexOffset);
            *hasV2Footer = 1;
            return GMB_DB_OK;
        }
    }
    fseek(stream, -(long)sizeof(int64_t), SEEK_END);
    rd_i64_le(stream, indexOffset);
    return GMB_DB_OK;
}

void SstFooter_WriteV2(FILE* writer, int64_t bloomOffset, int64_t indexOffset)
{
    wr_i64_le(writer, SstFooter_MagicV2);
    wr_i64_le(writer, bloomOffset);
    wr_i64_le(writer, indexOffset);
}

int SstFooter_TryValidateOffsets(int64_t streamLength, int64_t indexOffset, int64_t bloomOffset,
                                 int hasV2Footer, char* error, int errorCap)
{
    int footerSize = hasV2Footer ? SstFooter_V2FooterSize : (int)sizeof(int64_t);
    if (error && errorCap) error[0] = 0;
    if (indexOffset < 0 || indexOffset >= streamLength - footerSize + (int64_t)sizeof(int64_t)) {
        if (error) snprintf(error, (size_t)errorCap, "invalid index offset");
        return 0;
    }
    if (bloomOffset < 0 || bloomOffset >= indexOffset) {
        if (error) snprintf(error, (size_t)errorCap, "invalid bloom offset");
        return 0;
    }
    return 1;
}

/* SPLIT:SstWriter.c */

#define SstWriter_BlockSizeThreshold 4096

int SstWriter_WriteFromMemtable(const char* filePath, Memtable* memtable)
{
    FILE* stream;
    BloomFilter* bloom;
    int keyCount = Memtable_get_KeyCount(memtable);
    int i, indexN = 0;
    int64_t currentBlockSize = 0, bloomOffset = 0, indexOffset;
    uint8_t** indexKeys = NULL;
    int* indexKeyLens = NULL;
    int64_t* indexOffs = NULL;
    int firstInBlock = 1;

    stream = fopen(filePath, "wb");
    if (!stream) return GMB_DB_ERR;
    bloom = BloomFilter_New(keyCount > 0 ? keyCount : 1);

    for (i = 0; i < keyCount; i++) {
        uint8_t *key, *value;
        int keyLen, valueLen, tomb;
        uint8_t exists;
        Memtable_Nth(memtable, i, &key, &keyLen, &value, &valueLen, &tomb);
        BloomFilter_Add(bloom, key, keyLen);
        if (firstInBlock) {
            indexKeys = (uint8_t**)realloc(indexKeys, (size_t)(indexN + 1) * sizeof(uint8_t*));
            indexKeyLens = (int*)realloc(indexKeyLens, (size_t)(indexN + 1) * sizeof(int));
            indexOffs = (int64_t*)realloc(indexOffs, (size_t)(indexN + 1) * sizeof(int64_t));
            indexKeys[indexN] = key;
            indexKeyLens[indexN] = keyLen;
            indexOffs[indexN] = (int64_t)ftell(stream);
            indexN++;
            firstInBlock = 0;
        }
        wr_i32_le(stream, keyLen);
        fwrite(key, 1, (size_t)keyLen, stream);
        exists = tomb ? 0 : 1;
        fwrite(&exists, 1, 1, stream);
        if (exists) {
            wr_i32_le(stream, valueLen);
            fwrite(value, 1, (size_t)valueLen, stream);
            currentBlockSize += keyLen + valueLen + 9;
        } else {
            currentBlockSize += keyLen + 5;
        }
        if (currentBlockSize >= SstWriter_BlockSizeThreshold) {
            currentBlockSize = 0;
            firstInBlock = 1;
        }
    }
    if (keyCount > 0) {
        bloomOffset = ftell(stream);
        BloomFilter_WriteTo(bloom, stream);
    }
    indexOffset = ftell(stream);
    wr_i32_le(stream, indexN);
    for (i = 0; i < indexN; i++) {
        wr_i32_le(stream, indexKeyLens[i]);
        fwrite(indexKeys[i], 1, (size_t)indexKeyLens[i], stream);
        wr_i64_le(stream, indexOffs[i]);
    }
    SstFooter_WriteV2(stream, bloomOffset, indexOffset);
    BloomFilter_Dispose(bloom);
    free(indexKeys); free(indexKeyLens); free(indexOffs);
    fclose(stream);
    return GMB_DB_OK;
}
