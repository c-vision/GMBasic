#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#define NtxPage_HeaderSize 12
#define NtxPage_PageSize 1024
#define NtxPageType_Internal 0
#define NtxPageType_Leaf 1

static void ntx_u16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint16_t ntx_ru16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static void ntx_u32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t ntx_ru32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

typedef struct NtxPage {
    int PageSize;
    uint16_t KeyCount;
    int IsLeaf;
    uint32_t NextPageNum;
    uint32_t PrevPageNum;
    uint32_t NodeOffset;
    uint8_t Data[NtxPage_PageSize];
} NtxPage;

static NtxPage* NtxPage_New(void)
{
    NtxPage* p = (NtxPage*)calloc(1, sizeof(*p));
    p->PageSize = NtxPage_PageSize;
    return p;
}

static void NtxPage_Load(NtxPage* page)
{
    page->KeyCount = ntx_ru16(page->Data);
    page->IsLeaf = (ntx_ru16(page->Data + 2) & 1) == 1;
    if (page->IsLeaf) {
        page->NextPageNum = ntx_ru32(page->Data + 4);
        page->PrevPageNum = ntx_ru32(page->Data + 8);
    }
}

struct NtxFile {
    FILE* fs;
    int pageSize;
    char keyExpressionString[256];
    char FilePath[PATH_MAX];
    char Name[256];
    int Unique;
    int Descending;
    int KeyLength;
    uint32_t rootPageNum;
    uint32_t firstLeafPageNum;
    uint32_t freeListPageHead;
    LruCache* nodeCache;
    int useNodeCache;
    pthread_mutex_t syncRoot;
};

static long NtxFile_len(FILE* f)
{
    long cur = ftell(f), end;
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

static const uint8_t* NtxFile_GetLeafKey(const NtxFile* self, const NtxPage* page, int index)
{
    return page->Data + NtxPage_HeaderSize + index * (self->KeyLength + 4);
}
static uint32_t NtxFile_GetLeafRecno(const NtxFile* self, const NtxPage* page, int index)
{
    return ntx_ru32(NtxFile_GetLeafKey(self, page, index) + self->KeyLength);
}
static const uint8_t* NtxFile_GetInternalKey(const NtxFile* self, const NtxPage* page, int index)
{
    return page->Data + NtxPage_HeaderSize + (index * (4 + self->KeyLength)) + 4;
}
static uint32_t NtxFile_GetInternalPointer(const NtxFile* self, const NtxPage* page, int index)
{
    return ntx_ru32(page->Data + NtxPage_HeaderSize + index * (4 + self->KeyLength));
}
static int NtxFile_GetMaxKeysPerLeaf(const NtxFile* self)
{
    return (self->pageSize - NtxPage_HeaderSize) / (self->KeyLength + 4);
}
static int NtxFile_GetMaxKeysPerInternal(const NtxFile* self)
{
    return (self->pageSize - NtxPage_HeaderSize - 4) / (self->KeyLength + 4);
}

static NtxPage* NtxFile_ReadPage(NtxFile* self, uint32_t pageNum)
{
    void* cached = NULL;
    NtxPage* p;
    long offset;
    if (self->useNodeCache && LruCache_TryGetValue(self->nodeCache, pageNum, &cached) && cached)
        return (NtxPage*)cached;
    offset = (long)pageNum * self->pageSize;
    if (offset < 0 || offset + self->pageSize > NtxFile_len(self->fs)) return NULL;
    p = NtxPage_New();
    p->NodeOffset = pageNum;
    fseek(self->fs, offset, SEEK_SET);
    if (fread(p->Data, 1, (size_t)self->pageSize, self->fs) != (size_t)self->pageSize) {
        free(p);
        return NULL;
    }
    NtxPage_Load(p);
    if (self->useNodeCache) LruCache_AddOrUpdate(self->nodeCache, pageNum, p);
    return p;
}

static void NtxFile_WritePage(NtxFile* self, uint32_t pageNum, NtxPage* page)
{
    long offset = (long)pageNum * self->pageSize;
    fseek(self->fs, offset, SEEK_SET);
    fwrite(page->Data, 1, (size_t)self->pageSize, self->fs);
    if (self->useNodeCache) LruCache_AddOrUpdate(self->nodeCache, pageNum, page);
}

static void NtxFile_UpdateRootPageInHeader(NtxFile* self, uint32_t newRootPageNum)
{
    uint8_t b[4];
    self->rootPageNum = newRootPageNum;
    ntx_u32(b, newRootPageNum);
    fseek(self->fs, 0, SEEK_SET);
    fwrite(b, 1, 4, self->fs);
}

static void NtxFile_UpdateFirstLeafInHeader(NtxFile* self, uint32_t firstLeaf)
{
    uint8_t b[4];
    self->firstLeafPageNum = firstLeaf;
    ntx_u32(b, firstLeaf);
    fseek(self->fs, 4, SEEK_SET);
    fwrite(b, 1, 4, self->fs);
}

static void NtxFile_UpdateFreeListHeadInHeader(NtxFile* self, uint32_t newHead)
{
    uint8_t b[4];
    self->freeListPageHead = newHead;
    ntx_u32(b, newHead);
    fseek(self->fs, 12, SEEK_SET);
    fwrite(b, 1, 4, self->fs);
}

static uint32_t NtxFile_AllocateNewPage(NtxFile* self)
{
    if (self->freeListPageHead != 0) {
        uint32_t reclaimed = self->freeListPageHead;
        uint8_t nextBuf[4];
        long offset = (long)reclaimed * self->pageSize;
        fseek(self->fs, offset, SEEK_SET);
        fread(nextBuf, 1, 4, self->fs);
        NtxFile_UpdateFreeListHeadInHeader(self, ntx_ru32(nextBuf));
        if (self->useNodeCache) LruCache_Remove(self->nodeCache, reclaimed);
        return reclaimed;
    }
    {
        long newOff = NtxFile_len(self->fs);
        uint32_t newPageNum = (uint32_t)(newOff / self->pageSize);
        ftruncate(fileno(self->fs), (off_t)(newOff + self->pageSize));
        return newPageNum;
    }
}

static void NtxFile_RewriteLeafNode(NtxFile* self, NtxPage* page, const uint8_t** keys, const uint32_t* recnos, int n)
{
    int i, entrySize = self->KeyLength + 4, off = NtxPage_HeaderSize;
    memset(page->Data, 0, (size_t)self->pageSize);
    for (i = 0; i < n; i++) {
        memcpy(page->Data + off, keys[i], (size_t)self->KeyLength);
        ntx_u32(page->Data + off + self->KeyLength, recnos[i]);
        off += entrySize;
    }
    ntx_u16(page->Data, (uint16_t)n);
    ntx_u16(page->Data + 2, (uint16_t)NtxPageType_Leaf);
    NtxPage_Load(page);
}

static void NtxFile_UpdateLeafPointers(NtxPage* page, int hasNext, uint32_t next, int hasPrev, uint32_t prev)
{
    if (hasNext) ntx_u32(page->Data + 4, next);
    if (hasPrev) ntx_u32(page->Data + 8, prev);
    NtxPage_Load(page);
}

static void NtxFile_InsertIntoLeaf(NtxFile* self, NtxPage* page, const uint8_t* keyBytes, int recno)
{
    int i, entrySize = self->KeyLength + 4, insertOffset, move;
    uint8_t* span = page->Data;
    for (i = 0; i < page->KeyCount; i++)
        if (ByteArrayComparer_Compare(keyBytes, self->KeyLength, NtxFile_GetLeafKey(self, page, i), self->KeyLength) < 0)
            break;
    insertOffset = NtxPage_HeaderSize + i * entrySize;
    move = (page->KeyCount - i) * entrySize;
    if (move > 0) memmove(span + insertOffset + entrySize, span + insertOffset, (size_t)move);
    memcpy(span + insertOffset, keyBytes, (size_t)self->KeyLength);
    ntx_u32(span + insertOffset + self->KeyLength, (uint32_t)recno);
    ntx_u16(span, (uint16_t)(page->KeyCount + 1));
    NtxPage_Load(page);
}

static int NtxFile_FindInLeaf(NtxFile* self, const NtxPage* page, const uint8_t* keyBytes, int soft)
{
    int low = 0, high = page->KeyCount - 1, resultIndex = -1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = ByteArrayComparer_Compare(NtxFile_GetLeafKey(self, page, mid), self->KeyLength,
                                            keyBytes, self->KeyLength);
        if (cmp == 0) return (int)NtxFile_GetLeafRecno(self, page, mid);
        if (cmp < 0) low = mid + 1;
        else { resultIndex = mid; high = mid - 1; }
    }
    return (soft && resultIndex != -1) ? (int)NtxFile_GetLeafRecno(self, page, resultIndex) : -1;
}

static uint32_t NtxFile_DescendInternal(NtxFile* self, const NtxPage* page, const uint8_t* keyBytes)
{
    int i;
    for (i = 0; i < page->KeyCount; i++) {
        if (ByteArrayComparer_Compare(keyBytes, self->KeyLength,
                                      NtxFile_GetInternalKey(self, page, i), self->KeyLength) < 0)
            return NtxFile_GetInternalPointer(self, page, i);
    }
    return NtxFile_GetInternalPointer(self, page, page->KeyCount);
}

static void NtxFile_RewriteInternalNode(NtxFile* self, NtxPage* page, const uint8_t** keys, const uint32_t* ptrs, int keyN)
{
    int i, off = NtxPage_HeaderSize;
    memset(page->Data, 0, (size_t)self->pageSize);
    ntx_u16(page->Data, (uint16_t)keyN);
    ntx_u16(page->Data + 2, (uint16_t)NtxPageType_Internal);
    for (i = 0; i < keyN; i++) {
        ntx_u32(page->Data + off, ptrs[i]);
        off += 4;
        memcpy(page->Data + off, keys[i], (size_t)self->KeyLength);
        off += self->KeyLength;
    }
    ntx_u32(page->Data + off, ptrs[keyN]);
    NtxPage_Load(page);
}

static void NtxFile_InsertIntoInternal(NtxFile* self, NtxPage* page, const uint8_t* key, uint32_t newPageNum)
{
    int i, entrySize = self->KeyLength + 4, insertOffset, keyCount = page->KeyCount;
    uint8_t* span = page->Data;
    int rightPointerOffset, srcLen;
    for (i = 0; i < keyCount; i++) {
        if (ByteArrayComparer_Compare(key, self->KeyLength, NtxFile_GetInternalKey(self, page, i), self->KeyLength) < 0)
            break;
    }
    insertOffset = NtxPage_HeaderSize + i * entrySize;
    rightPointerOffset = insertOffset + 4;
    srcLen = (keyCount - i) * entrySize + 4;
    memmove(span + rightPointerOffset + entrySize, span + rightPointerOffset, (size_t)srcLen);
    memcpy(span + insertOffset + 4, key, (size_t)self->KeyLength);
    ntx_u32(span + insertOffset + 4 + self->KeyLength, newPageNum);
    ntx_u16(span, (uint16_t)(keyCount + 1));
    NtxPage_Load(page);
}

typedef struct { uint8_t* key; uint32_t newPageNum; } NtxPromoted;

static NtxPromoted NtxFile_SplitAndInsertLeaf(NtxFile* self, uint32_t pageNum, NtxPage* page,
                                              const uint8_t* keyBytes, int recno)
{
    uint32_t siblingPageNum = NtxFile_AllocateNewPage(self);
    NtxPage* sibling = NtxPage_New();
    int n = page->KeyCount + 1, i, mid, origNext;
    uint8_t** keys = (uint8_t**)malloc((size_t)n * sizeof(uint8_t*));
    uint32_t* recs = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
    uint8_t* newKeyCopy = (uint8_t*)malloc((size_t)self->KeyLength);
    NtxPromoted r;
    memcpy(newKeyCopy, keyBytes, (size_t)self->KeyLength);
    for (i = 0; i < page->KeyCount; i++) {
        keys[i] = (uint8_t*)malloc((size_t)self->KeyLength);
        memcpy(keys[i], NtxFile_GetLeafKey(self, page, i), (size_t)self->KeyLength);
        recs[i] = NtxFile_GetLeafRecno(self, page, i);
    }
    keys[n - 1] = newKeyCopy;
    recs[n - 1] = (uint32_t)recno;
    /* sort */
    for (i = 0; i < n; i++) {
        int j;
        for (j = i + 1; j < n; j++)
            if (ByteArrayComparer_Compare(keys[j], self->KeyLength, keys[i], self->KeyLength) < 0) {
                uint8_t* tk = keys[i]; uint32_t tr = recs[i];
                keys[i] = keys[j]; recs[i] = recs[j]; keys[j] = tk; recs[j] = tr;
            }
    }
    mid = n / 2;
    origNext = (int)page->NextPageNum;
    NtxFile_RewriteLeafNode(self, page, (const uint8_t**)keys, recs, mid);
    NtxFile_RewriteLeafNode(self, sibling, (const uint8_t**)(keys + mid), recs + mid, n - mid);
    NtxFile_UpdateLeafPointers(page, 1, siblingPageNum, 0, 0);
    NtxFile_UpdateLeafPointers(sibling, 1, (uint32_t)origNext, 1, pageNum);
    if (origNext != 0) {
        NtxPage* nextPage = NtxFile_ReadPage(self, (uint32_t)origNext);
        if (nextPage) {
            NtxFile_UpdateLeafPointers(nextPage, 0, 0, 1, siblingPageNum);
            NtxFile_WritePage(self, (uint32_t)origNext, nextPage);
        }
    }
    if (self->firstLeafPageNum == 0 || self->firstLeafPageNum == siblingPageNum)
        NtxFile_UpdateFirstLeafInHeader(self, pageNum);
    NtxFile_WritePage(self, pageNum, page);
    NtxFile_WritePage(self, siblingPageNum, sibling);
    r.key = (uint8_t*)malloc((size_t)self->KeyLength);
    memcpy(r.key, NtxFile_GetLeafKey(self, sibling, 0), (size_t)self->KeyLength);
    r.newPageNum = siblingPageNum;
    for (i = 0; i < n; i++) free(keys[i]);
    free(keys); free(recs);
    return r;
}

static NtxPromoted NtxFile_SplitAndInsertInternal(NtxFile* self, uint32_t pageNum, NtxPage* page,
                                                  const uint8_t* keyToInsert, uint32_t newChildPageNum)
{
    int n = page->KeyCount + 1, i, mid;
    uint8_t** keys = (uint8_t**)malloc((size_t)n * sizeof(uint8_t*));
    uint32_t* ptrsRight = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
    uint32_t firstPointer = NtxFile_GetInternalPointer(self, page, 0);
    uint32_t siblingPageNum;
    NtxPage* sibling;
    uint32_t *origPtrs, *sibPtrs;
    const uint8_t **origKeys, **sibKeys;
    int origN, sibN;
    NtxPromoted r;
    for (i = 0; i < page->KeyCount; i++) {
        keys[i] = (uint8_t*)malloc((size_t)self->KeyLength);
        memcpy(keys[i], NtxFile_GetInternalKey(self, page, i), (size_t)self->KeyLength);
        ptrsRight[i] = NtxFile_GetInternalPointer(self, page, i + 1);
    }
    keys[n - 1] = (uint8_t*)malloc((size_t)self->KeyLength);
    memcpy(keys[n - 1], keyToInsert, (size_t)self->KeyLength);
    ptrsRight[n - 1] = newChildPageNum;
    for (i = 0; i < n; i++) {
        int j;
        for (j = i + 1; j < n; j++)
            if (ByteArrayComparer_Compare(keys[j], self->KeyLength, keys[i], self->KeyLength) < 0) {
                uint8_t* tk = keys[i]; uint32_t tp = ptrsRight[i];
                keys[i] = keys[j]; ptrsRight[i] = ptrsRight[j]; keys[j] = tk; ptrsRight[j] = tp;
            }
    }
    mid = n / 2;
    siblingPageNum = NtxFile_AllocateNewPage(self);
    sibling = NtxPage_New();
    origN = mid;
    sibN = n - mid - 1;
    origKeys = (const uint8_t**)keys;
    sibKeys = (const uint8_t**)(keys + mid + 1);
    origPtrs = (uint32_t*)malloc((size_t)(origN + 1) * sizeof(uint32_t));
    sibPtrs = (uint32_t*)malloc((size_t)(sibN + 1) * sizeof(uint32_t));
    origPtrs[0] = firstPointer;
    for (i = 0; i < origN; i++) origPtrs[i + 1] = ptrsRight[i];
    sibPtrs[0] = ptrsRight[mid];
    for (i = 0; i < sibN; i++) sibPtrs[i + 1] = ptrsRight[mid + 1 + i];
    NtxFile_RewriteInternalNode(self, page, origKeys, origPtrs, origN);
    NtxFile_RewriteInternalNode(self, sibling, sibKeys, sibPtrs, sibN);
    NtxFile_WritePage(self, pageNum, page);
    NtxFile_WritePage(self, siblingPageNum, sibling);
    r.key = (uint8_t*)malloc((size_t)self->KeyLength);
    memcpy(r.key, keys[mid], (size_t)self->KeyLength);
    r.newPageNum = siblingPageNum;
    for (i = 0; i < n; i++) free(keys[i]);
    free(keys); free(ptrsRight); free(origPtrs); free(sibPtrs);
    return r;
}

static NtxPromoted NtxFile_InsertRecursive(NtxFile* self, uint32_t pageNum, const uint8_t* keyBytes, int recno)
{
    NtxPage* page = NtxFile_ReadPage(self, pageNum);
    NtxPromoted none = { NULL, 0 };
    if (!page) return none;
    if (page->IsLeaf) {
        if (page->KeyCount < NtxFile_GetMaxKeysPerLeaf(self)) {
            NtxFile_InsertIntoLeaf(self, page, keyBytes, recno);
            NtxFile_WritePage(self, pageNum, page);
            return none;
        }
        return NtxFile_SplitAndInsertLeaf(self, pageNum, page, keyBytes, recno);
    }
    {
        uint32_t child = NtxFile_DescendInternal(self, page, keyBytes);
        NtxPromoted promoted = NtxFile_InsertRecursive(self, child, keyBytes, recno);
        if (!promoted.key) return none;
        if (page->KeyCount < NtxFile_GetMaxKeysPerInternal(self)) {
            NtxFile_InsertIntoInternal(self, page, promoted.key, promoted.newPageNum);
            NtxFile_WritePage(self, pageNum, page);
            free(promoted.key);
            return none;
        }
        {
            NtxPromoted up = NtxFile_SplitAndInsertInternal(self, pageNum, page, promoted.key, promoted.newPageNum);
            free(promoted.key);
            return up;
        }
    }
}

static void NtxFile_InsertUnderLock(NtxFile* self, const uint8_t* keyBytes, int recno)
{
    NtxPage* oldRoot;
    NtxPromoted promoted;
    if (self->rootPageNum == 0) return;
    oldRoot = NtxFile_ReadPage(self, self->rootPageNum);
    promoted = NtxFile_InsertRecursive(self, self->rootPageNum, keyBytes, recno);
    if (promoted.key && promoted.newPageNum != 0) {
        NtxPage* newRoot = NtxPage_New();
        uint8_t* span = newRoot->Data;
        uint32_t newRootPageNum;
        ntx_u16(span, 1);
        ntx_u16(span + 2, (uint16_t)NtxPageType_Internal);
        ntx_u32(span + NtxPage_HeaderSize, self->rootPageNum);
        memcpy(span + NtxPage_HeaderSize + 4, promoted.key, (size_t)self->KeyLength);
        ntx_u32(span + NtxPage_HeaderSize + 4 + self->KeyLength, promoted.newPageNum);
        NtxPage_Load(newRoot);
        newRootPageNum = NtxFile_AllocateNewPage(self);
        NtxFile_WritePage(self, newRootPageNum, newRoot);
        if (oldRoot && oldRoot->IsLeaf)
            NtxFile_UpdateFirstLeafInHeader(self, self->rootPageNum);
        NtxFile_UpdateRootPageInHeader(self, newRootPageNum);
        free(promoted.key);
    }
}

NtxFile* NtxFile_New(const char* path, LockingStrategy lockingStrategy)
{
    NtxFile* self = (NtxFile*)calloc(1, sizeof(*self));
    uint8_t header[NtxPage_PageSize];
    int exprEnd, i;
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", path);
    self->pageSize = NtxPage_PageSize;
    self->useNodeCache = lockingStrategy == LockingStrategy_IntraProcess;
    self->nodeCache = LruCache_New(256);
    pthread_mutex_init(&self->syncRoot, NULL);
    self->fs = fopen(path, "rb+");
    if (!self->fs || NtxFile_len(self->fs) < self->pageSize) {
        if (self->fs) fclose(self->fs);
        LruCache_Dispose(self->nodeCache);
        free(self);
        return NULL;
    }
    fseek(self->fs, 0, SEEK_SET);
    fread(header, 1, (size_t)self->pageSize, self->fs);
    self->rootPageNum = ntx_ru32(header);
    self->firstLeafPageNum = ntx_ru32(header + 4);
    self->freeListPageHead = ntx_ru32(header + 12);
    self->KeyLength = ntx_ru16(header + 8);
    self->Unique = (header[10] & 1) == 1;
    exprEnd = -1;
    for (i = 16; i < 16 + 256; i++) if (header[i] == 0) { exprEnd = i; break; }
    if (exprEnd < 0) exprEnd = 16 + 256;
    memcpy(self->keyExpressionString, header + 16, (size_t)(exprEnd - 16));
    {
        const char* slash = strrchr(path, '/');
        const char* base = slash ? slash + 1 : path;
        const char* dot = strrchr(base, '.');
        size_t n = dot ? (size_t)(dot - base) : strlen(base);
        if (n >= sizeof(self->Name)) n = sizeof(self->Name) - 1;
        memcpy(self->Name, base, n);
        self->Name[n] = 0;
    }
    self->Descending = 0;
    return self;
}

NtxFile* NtxFile_CreateNew(const char* filePath, const char* keyExpression, int keyLength, int isUnique,
                           LockingStrategy lockingStrategy)
{
    FILE* fs;
    uint8_t header[NtxPage_PageSize], page[NtxPage_PageSize];
    uint32_t root = 1, first = 1;
    memset(header, 0, sizeof(header));
    memset(page, 0, sizeof(page));
    ntx_u32(header, root);
    ntx_u32(header + 4, first);
    ntx_u16(header + 8, (uint16_t)keyLength);
    header[10] = isUnique ? 1 : 0;
    ntx_u32(header + 12, 0);
    if (keyExpression) {
        size_t n = strlen(keyExpression);
        if (n > 255) n = 255;
        memcpy(header + 16, keyExpression, n);
    }
    ntx_u16(page, 0);
    ntx_u16(page + 2, 1);
    ntx_u32(page + 4, 0);
    ntx_u32(page + 8, 0);
    fs = fopen(filePath, "wb");
    if (!fs) return NULL;
    fwrite(header, 1, sizeof(header), fs);
    fwrite(page, 1, sizeof(page), fs);
    fflush(fs);
    fclose(fs);
    return NtxFile_New(filePath, lockingStrategy);
}

void NtxFile_Dispose(NtxFile* self)
{
    if (!self) return;
    pthread_mutex_lock(&self->syncRoot);
    if (self->fs) fclose(self->fs);
    pthread_mutex_unlock(&self->syncRoot);
    LruCache_Dispose(self->nodeCache);
    pthread_mutex_destroy(&self->syncRoot);
    free(self);
}

const char* NtxFile_GetKeyExpressionString(const NtxFile* self) { return self->keyExpressionString; }
const char* NtxFile_get_FilePath(const NtxFile* self) { return self->FilePath; }
const char* NtxFile_get_Name(const NtxFile* self) { return self->Name; }
int NtxFile_get_Unique(const NtxFile* self) { return self->Unique; }
int NtxFile_get_KeyLength(const NtxFile* self) { return self->KeyLength; }

int NtxFile_Seek(NtxFile* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    uint32_t pageNum;
    (void)keyLen;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootPageNum == 0) { pthread_mutex_unlock(&self->syncRoot); return -1; }
    pageNum = self->rootPageNum;
    for (;;) {
        NtxPage* page = NtxFile_ReadPage(self, pageNum);
        if (!page) { pthread_mutex_unlock(&self->syncRoot); return -1; }
        if (page->IsLeaf) {
            int rec = NtxFile_FindInLeaf(self, page, keyBytes, soft);
            pthread_mutex_unlock(&self->syncRoot);
            return rec;
        }
        pageNum = NtxFile_DescendInternal(self, page, keyBytes);
    }
}

void NtxFile_Insert(NtxFile* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    (void)keyLen;
    pthread_mutex_lock(&self->syncRoot);
    NtxFile_InsertUnderLock(self, keyBytes, recno);
    pthread_mutex_unlock(&self->syncRoot);
}

void NtxFile_InvalidateCache(NtxFile* self)
{
    pthread_mutex_lock(&self->syncRoot);
    (void)self;
    pthread_mutex_unlock(&self->syncRoot);
}

static void ntx_selfcheck_key(uint8_t key[10], int i)
{
    /* 10-byte Clipper-style key: "K000" padded with spaces (no embedded NUL). */
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int NtxFile_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ntxsc_XXXXXX";
    char path[PATH_MAX];
    NtxFile* idx;
    int i;
    uint8_t key[10];
    if (!mkdtemp(dir)) return -2;
    snprintf(path, sizeof(path), "%s/t.ntx", dir);
    idx = NtxFile_CreateNew(path, "NAME", 10, 0, LockingStrategy_IntraProcess);
    if (!idx) return -3;
    for (i = 0; i < 80; i++) {
        ntx_selfcheck_key(key, i);
        NtxFile_Insert(idx, key, 10, i + 1);
    }
    for (i = 0; i < 80; i++) {
        int rec;
        ntx_selfcheck_key(key, i);
        rec = NtxFile_Seek(idx, key, 10, 0);
        if (rec != i + 1) { NtxFile_Dispose(idx); return -100 - i; }
    }
    NtxFile_Dispose(idx);
    idx = NtxFile_New(path, LockingStrategy_IntraProcess);
    if (!idx) return -4;
    ntx_selfcheck_key(key, 40);
    if (NtxFile_Seek(idx, key, 10, 0) != 41) { NtxFile_Dispose(idx); return -5; }
    NtxFile_Dispose(idx);
    return GMB_DB_OK;
}
