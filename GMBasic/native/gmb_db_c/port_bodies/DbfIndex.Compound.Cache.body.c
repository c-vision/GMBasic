#include <unistd.h>
#include <limits.h>
#include <errno.h>

#define CDX_U32MAX 0xFFFFFFFFu

typedef struct {
    uint32_t Offset;
    uint8_t Data[CdxFormat_PageSize];
    int Dirty;
    int Occupied;
} CdxCacheSlot;

struct CdxPageCache {
    char FilePath[PATH_MAX];
    int MaxPages;
    int Count;
    CdxCacheSlot* Slots;
    uint32_t* Lru;
    int LruN;
};

static int cdxc_find(const CdxPageCache* self, uint32_t off)
{
    int i;
    for (i = 0; i < self->MaxPages; i++) {
        if (self->Slots[i].Occupied && self->Slots[i].Offset == off)
            return i;
    }
    return -1;
}

static int cdxc_free_slot(const CdxPageCache* self)
{
    int i;
    for (i = 0; i < self->MaxPages; i++) {
        if (!self->Slots[i].Occupied)
            return i;
    }
    return -1;
}

static void cdxc_touch(CdxPageCache* self, uint32_t off)
{
    int i, found = -1;
    for (i = 0; i < self->LruN; i++) {
        if (self->Lru[i] == off) {
            found = i;
            break;
        }
    }
    if (found > 0)
        memmove(&self->Lru[1], self->Lru, (size_t)found * sizeof(uint32_t));
    else if (found == 0)
        return;
    else {
        if (self->LruN < self->MaxPages)
            memmove(&self->Lru[1], self->Lru, (size_t)self->LruN * sizeof(uint32_t));
        self->LruN++;
        if (self->LruN > self->MaxPages) self->LruN = self->MaxPages;
    }
    self->Lru[0] = off;
}

static void cdxc_evict(CdxPageCache* self)
{
    uint32_t oldest;
    int i;
    if (self->LruN == 0) return;
    oldest = self->Lru[self->LruN - 1];
    self->LruN--;
    i = cdxc_find(self, oldest);
    if (i >= 0) {
        self->Slots[i].Occupied = 0;
        self->Count--;
    }
}

CdxPageCache* CdxPageCache_New(const char* filePath, int maxPages)
{
    CdxPageCache* self;
    if (!filePath) return NULL;
    if (maxPages <= 0) maxPages = 256;
    self = (CdxPageCache*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", filePath);
    self->MaxPages = maxPages;
    self->Slots = (CdxCacheSlot*)calloc((size_t)maxPages, sizeof(CdxCacheSlot));
    self->Lru = (uint32_t*)calloc((size_t)maxPages, sizeof(uint32_t));
    if (!self->Slots || !self->Lru) {
        CdxPageCache_Dispose(self);
        return NULL;
    }
    return self;
}

void CdxPageCache_Dispose(CdxPageCache* self)
{
    if (!self) return;
    free(self->Slots);
    free(self->Lru);
    free(self);
}

int CdxPageCache_GetPage(CdxPageCache* self, uint32_t offset, uint8_t* out512)
{
    int i;
    FILE* fs;
    if (!self || !out512) return GMB_DB_ARG;
    i = cdxc_find(self, offset);
    if (i >= 0) {
        memcpy(out512, self->Slots[i].Data, CdxFormat_PageSize);
        cdxc_touch(self, offset);
        return GMB_DB_OK;
    }
    fs = fopen(self->FilePath, "rb");
    if (!fs) return GMB_DB_ERR;
    if (fseek(fs, (long)offset, SEEK_SET) != 0
        || fread(out512, 1, CdxFormat_PageSize, fs) != CdxFormat_PageSize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    fclose(fs);
    if (self->Count >= self->MaxPages)
        cdxc_evict(self);
    i = cdxc_free_slot(self);
    if (i < 0) return GMB_DB_ERR;
    self->Slots[i].Offset = offset;
    memcpy(self->Slots[i].Data, out512, CdxFormat_PageSize);
    self->Slots[i].Dirty = 0;
    self->Slots[i].Occupied = 1;
    self->Count++;
    cdxc_touch(self, offset);
    return GMB_DB_OK;
}

void CdxPageCache_PutPage(CdxPageCache* self, uint32_t offset, const uint8_t* data, int dirty)
{
    int i;
    if (!self || !data) return;
    i = cdxc_find(self, offset);
    if (i < 0) {
        if (self->Count >= self->MaxPages)
            cdxc_evict(self);
        i = cdxc_free_slot(self);
        if (i < 0) return;
        self->Count++;
        self->Slots[i].Occupied = 1;
    }
    self->Slots[i].Offset = offset;
    memcpy(self->Slots[i].Data, data, CdxFormat_PageSize);
    self->Slots[i].Dirty = dirty;
    cdxc_touch(self, offset);
}

void CdxPageCache_MarkDirty(CdxPageCache* self, uint32_t offset)
{
    int i;
    if (!self) return;
    i = cdxc_find(self, offset);
    if (i >= 0) {
        self->Slots[i].Dirty = 1;
        cdxc_touch(self, offset);
    }
}

uint32_t CdxPageCache_AllocatePage(CdxPageCache* self, const uint8_t* data)
{
    FILE* fs;
    long off;
    if (!self || !data) return 0;
    fs = fopen(self->FilePath, "r+b");
    if (!fs) return 0;
    if (fseek(fs, 0, SEEK_END) != 0) {
        fclose(fs);
        return 0;
    }
    off = ftell(fs);
    if (off < 0 || fwrite(data, 1, CdxFormat_PageSize, fs) != CdxFormat_PageSize) {
        fclose(fs);
        return 0;
    }
    fclose(fs);
    CdxPageCache_PutPage(self, (uint32_t)off, data, 0);
    return (uint32_t)off;
}

int CdxPageCache_Flush(CdxPageCache* self)
{
    FILE* fs;
    int i;
    if (!self) return GMB_DB_ARG;
    fs = fopen(self->FilePath, "r+b");
    if (!fs) return GMB_DB_ERR;
    for (i = 0; i < self->MaxPages; i++) {
        if (!self->Slots[i].Occupied || !self->Slots[i].Dirty) continue;
        if (fseek(fs, (long)self->Slots[i].Offset, SEEK_SET) != 0
            || fwrite(self->Slots[i].Data, 1, CdxFormat_PageSize, fs) != CdxFormat_PageSize) {
            fclose(fs);
            return GMB_DB_ERR;
        }
        self->Slots[i].Dirty = 0;
    }
    fclose(fs);
    return GMB_DB_OK;
}

void CdxPageCache_Clear(CdxPageCache* self)
{
    int i;
    if (!self) return;
    for (i = 0; i < self->MaxPages; i++)
        self->Slots[i].Occupied = 0;
    self->Count = 0;
    self->LruN = 0;
}

int CdxPageCache_get_Count(const CdxPageCache* self)
{
    return self ? self->Count : 0;
}

typedef struct {
    int HasPage;
    uint8_t Page[CdxFormat_PageSize];
    uint32_t PageOffset;
    int HasSplit;
    uint8_t SplitKey[GMB_CDX_MAX_KEY];
    uint32_t SplitChild;
} CdxIns;

static int cdxc_keycmp(const uint8_t* a, const uint8_t* b, int n)
{
    return memcmp(a, b, (size_t)n);
}

static int cdxc_find_key(const uint8_t* keysFlat, int n, int keyLength, const uint8_t* key, int* exact)
{
    int lo = 0, hi = n - 1, insertAt = n;
    *exact = 0;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = cdxc_keycmp(key, keysFlat + mid * keyLength, keyLength);
        if (cmp == 0) {
            *exact = 1;
            return mid;
        }
        if (cmp < 0) {
            insertAt = mid;
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return insertAt;
}

static int cdxc_insert_in_leaf(CdxPageCache* cache, uint32_t offset, const uint8_t* page,
                               const uint8_t* key, int recno, int keyLength, int unique, CdxIns* out)
{
    int keyCount = (int)(page[2] | (page[3] << 8));
    int cap = CdxCompact_MaxKeysPerLeaf(keyLength) + 1;
    uint8_t* keys;
    int* recs;
    int n = 0, exact = 0, idx, maxLeaf;
    uint8_t left[CdxFormat_PageSize], right[CdxFormat_PageSize];
    uint32_t rightOff;
    memset(out, 0, sizeof(*out));
    keys = (uint8_t*)malloc((size_t)cap * (size_t)keyLength);
    recs = (int*)malloc((size_t)cap * sizeof(int));
    if (!keys || !recs) {
        free(keys);
        free(recs);
        return GMB_DB_ERR;
    }
    if (CdxCompact_DecodeLeafKeys(page, keyCount, keyLength, keys, recs, cap, &n) != GMB_DB_OK) {
        free(keys);
        free(recs);
        return GMB_DB_ERR;
    }
    idx = cdxc_find_key(keys, n, keyLength, key, &exact);
    if (unique && exact) {
        free(keys);
        free(recs);
        return GMB_DB_ERR;
    }
    if (n >= cap) {
        free(keys);
        free(recs);
        return GMB_DB_ERR;
    }
    memmove(keys + (idx + 1) * keyLength, keys + idx * keyLength, (size_t)(n - idx) * (size_t)keyLength);
    memmove(&recs[idx + 1], &recs[idx], (size_t)(n - idx) * sizeof(int));
    memcpy(keys + idx * keyLength, key, (size_t)keyLength);
    recs[idx] = recno;
    n++;
    maxLeaf = CdxCompact_MaxKeysPerLeaf(keyLength);
    if (n <= maxLeaf) {
        if (CdxCompact_BuildLeafPage(out->Page, keys, recs, n, keyLength) != GMB_DB_OK) {
            free(keys);
            free(recs);
            return GMB_DB_ERR;
        }
        out->HasPage = 1;
        out->PageOffset = offset;
        free(keys);
        free(recs);
        return GMB_DB_OK;
    }
    {
        int splitAt = n / 2;
        if (CdxCompact_BuildLeafPage(left, keys, recs, splitAt, keyLength) != GMB_DB_OK
            || CdxCompact_BuildLeafPage(right, keys + splitAt * keyLength, recs + splitAt, n - splitAt, keyLength) != GMB_DB_OK) {
            free(keys);
            free(recs);
            return GMB_DB_ERR;
        }
        rightOff = CdxPageCache_AllocatePage(cache, right);
        if (rightOff == 0) {
            free(keys);
            free(recs);
            return GMB_DB_ERR;
        }
        CdxPageCache_PutPage(cache, offset, left, 1);
        CdxPageCache_MarkDirty(cache, offset);
        memcpy(out->Page, left, CdxFormat_PageSize);
        out->HasPage = 1;
        out->PageOffset = offset;
        out->HasSplit = 1;
        memcpy(out->SplitKey, keys + splitAt * keyLength, (size_t)keyLength);
        out->SplitChild = rightOff;
    }
    free(keys);
    free(recs);
    return GMB_DB_OK;
}

static int cdxc_insert_split_interior(
    CdxPageCache* cache, uint32_t offset, uint8_t* page, int insertAfter, int keyCount,
    const uint8_t* splitKey, uint32_t splitChildOffset,
    uint32_t childOffset, const uint8_t* childData, int hasChildData, int keyLength, CdxIns* out)
{
    uint32_t children[256];
    uint8_t seps[256 * GMB_CDX_MAX_KEY];
    int nChild = 0, nSep = 0, i, maxBranch;
    memset(out, 0, sizeof(*out));
    if (hasChildData)
        CdxPageCache_PutPage(cache, childOffset, childData, 1);
    for (i = 0; i <= keyCount; i++) {
        children[nChild++] = CdxCompact_GetInteriorChild(page, i, keyCount, keyLength);
        if (i < keyCount) {
            CdxCompact_GetInteriorKey(page, i, keyLength, seps + nSep * keyLength);
            nSep++;
        }
        if (i == insertAfter) {
            memcpy(seps + nSep * keyLength, splitKey, (size_t)keyLength);
            nSep++;
            children[nChild++] = splitChildOffset;
        }
    }
    maxBranch = CdxCompact_MaxKeysPerInterior(keyLength);
    if (nSep <= maxBranch) {
        if (CdxCompact_BuildInteriorPage(out->Page, children, nChild, seps, nSep, keyLength) != GMB_DB_OK)
            return GMB_DB_ERR;
        out->HasPage = 1;
        out->PageOffset = offset;
        return GMB_DB_OK;
    }
    {
        int splitAt = nSep / 2;
        int leftNChild = splitAt + 1;
        int rightNChild = nChild - (splitAt + 1);
        int leftNSep = splitAt;
        int rightNSep = nSep - (splitAt + 1);
        uint8_t left[CdxFormat_PageSize], right[CdxFormat_PageSize];
        uint32_t rightOff;
        if (CdxCompact_BuildInteriorPage(left, children, leftNChild, seps, leftNSep, keyLength) != GMB_DB_OK
            || CdxCompact_BuildInteriorPage(right, children + leftNChild, rightNChild,
                                            seps + (splitAt + 1) * keyLength, rightNSep, keyLength) != GMB_DB_OK)
            return GMB_DB_ERR;
        rightOff = CdxPageCache_AllocatePage(cache, right);
        if (rightOff == 0) return GMB_DB_ERR;
        CdxPageCache_PutPage(cache, offset, left, 1);
        CdxPageCache_MarkDirty(cache, offset);
        memcpy(out->Page, left, CdxFormat_PageSize);
        out->HasPage = 1;
        out->PageOffset = offset;
        out->HasSplit = 1;
        memcpy(out->SplitKey, seps + splitAt * keyLength, (size_t)keyLength);
        out->SplitChild = rightOff;
        return GMB_DB_OK;
    }
}

static int cdxc_insert_rec(CdxPageCache* cache, uint32_t offset, const uint8_t* key, int recno,
                           int keyLength, int unique, int depth, CdxIns* out)
{
    uint8_t page[CdxFormat_PageSize];
    int isLeaf, keyCount, childIdx, i, rc;
    uint32_t childOffset;
    CdxIns child;
    memset(out, 0, sizeof(*out));
    if (depth > 16) return GMB_DB_ERR;
    if (CdxPageCache_GetPage(cache, offset, page) != GMB_DB_OK) return GMB_DB_ERR;
    isLeaf = (page[0] | (page[1] << 8)) & 2;
    if (isLeaf)
        return cdxc_insert_in_leaf(cache, offset, page, key, recno, keyLength, unique, out);
    keyCount = (int)(page[2] | (page[3] << 8));
    childIdx = 0;
    for (i = 0; i < keyCount; i++) {
        uint8_t sep[GMB_CDX_MAX_KEY];
        CdxCompact_GetInteriorKey(page, i, keyLength, sep);
        if (cdxc_keycmp(key, sep, keyLength) < 0) break;
        childIdx = i + 1;
    }
    childOffset = CdxCompact_GetInteriorChild(page, childIdx, keyCount, keyLength);
    if (childOffset == 0 || childOffset == CDX_U32MAX) {
        out->PageOffset = offset;
        return GMB_DB_OK;
    }
    rc = cdxc_insert_rec(cache, childOffset, key, recno, keyLength, unique, depth + 1, &child);
    if (rc != GMB_DB_OK) return rc;
    if (!child.HasSplit) {
        if (child.HasPage && child.PageOffset != childOffset) {
            CdxPageCache_PutPage(cache, child.PageOffset, child.Page, 1);
            CdxCompact_SetInteriorChild(page, childIdx, keyCount, keyLength, child.PageOffset);
            CdxPageCache_PutPage(cache, offset, page, 1);
        } else if (child.HasPage) {
            CdxPageCache_PutPage(cache, child.PageOffset, child.Page, 1);
        }
        out->PageOffset = offset;
        return GMB_DB_OK;
    }
    return cdxc_insert_split_interior(cache, offset, page, childIdx, keyCount,
                                      child.SplitKey, child.SplitChild,
                                      child.PageOffset, child.HasPage ? child.Page : NULL, child.HasPage,
                                      keyLength, out);
}

int CdxBTreeBuilder_InsertEntry(CdxPageCache* cache, uint32_t rootOffset,
                                const uint8_t* key, int keyLen, int recno, int keyLength, int unique,
                                uint32_t* newRootOut)
{
    CdxIns r;
    uint8_t padded[GMB_CDX_MAX_KEY];
    int copy, rc;
    if (!cache || !key || !newRootOut || keyLength <= 0 || keyLength > GMB_CDX_MAX_KEY)
        return GMB_DB_ARG;
    memset(padded, ' ', (size_t)keyLength);
    copy = keyLen < keyLength ? keyLen : keyLength;
    if (copy > 0) memcpy(padded, key, (size_t)copy);
    *newRootOut = rootOffset;
    rc = cdxc_insert_rec(cache, rootOffset, padded, recno, keyLength, unique, 0, &r);
    if (rc != GMB_DB_OK) return rc;
    if (r.HasSplit) {
        uint32_t children[2];
        uint8_t newRoot[CdxFormat_PageSize];
        uint32_t nr;
        children[0] = r.PageOffset;
        children[1] = r.SplitChild;
        if (CdxCompact_BuildInteriorPage(newRoot, children, 2, r.SplitKey, 1, keyLength) != GMB_DB_OK)
            return GMB_DB_ERR;
        nr = CdxPageCache_AllocatePage(cache, newRoot);
        if (nr == 0) return GMB_DB_ERR;
        CdxPageCache_MarkDirty(cache, nr);
        *newRootOut = nr;
    } else if (r.HasPage) {
        CdxPageCache_PutPage(cache, rootOffset, r.Page, 1);
        CdxPageCache_MarkDirty(cache, rootOffset);
        *newRootOut = rootOffset;
    }
    if (CdxPageCache_Flush(cache) != GMB_DB_OK) return GMB_DB_ERR;
    return GMB_DB_OK;
}

static void cdxc_sc_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int CdxPageCache_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/cdxpc_XXXXXX";
    char path[PATH_MAX];
    CdxTagIndex *idx, *reopen;
    CdxPageCache* cache;
    CdxTagDescriptor table[4];
    int nTags = 0, i;
    uint8_t key[10];
    uint8_t pg[CdxFormat_PageSize];
    FILE* f;
    long flen0, flen1;

    if (!mkdtemp(dir)) return -2;
    snprintf(path, sizeof(path), "%s/t.cdx", dir);

    idx = CdxTagIndex_CreateOrReplace(path, "name", "NAME", 10, 0, 0);
    if (!idx) return -3;
    CdxTagIndex_EnablePageCache(idx);
    for (i = 0; i < 8; i++) {
        cdxc_sc_key(key, i);
        if (CdxTagIndex_Insert(idx, key, 10, 100 + i) != GMB_DB_OK) {
            CdxTagIndex_Dispose(idx);
            return -10 - i;
        }
    }
    cdxc_sc_key(key, 3);
    if (CdxTagIndex_Seek(idx, key, 10, 0) != 103) {
        CdxTagIndex_Dispose(idx);
        return -20;
    }
    CdxTagIndex_Dispose(idx);

    if (CdxCompound_ReadTagTable(path, table, 4, &nTags) != GMB_DB_OK || nTags != 1
        || table[0].HeaderOffset != 512)
        return -21;
    reopen = CdxTagIndex_Open(path, "NAME", 0);
    if (!reopen) return -22;
    cdxc_sc_key(key, 0);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 100) {
        CdxTagIndex_Dispose(reopen);
        return -23;
    }
    cdxc_sc_key(key, 7);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 107) {
        CdxTagIndex_Dispose(reopen);
        return -24;
    }
    CdxTagIndex_Dispose(reopen);

    snprintf(path, sizeof(path), "%s/many.cdx", dir);
    idx = CdxTagIndex_CreateOrReplace(path, "k", "K", 10, 0, 0);
    if (!idx) return -25;
    f = fopen(path, "rb");
    if (!f) {
        CdxTagIndex_Dispose(idx);
        return -26;
    }
    fseek(f, 0, SEEK_END);
    flen0 = ftell(f);
    fclose(f);
    CdxTagIndex_EnablePageCache(idx);
    for (i = 0; i < 40; i++) {
        cdxc_sc_key(key, i);
        if (CdxTagIndex_Insert(idx, key, 10, i + 1) != GMB_DB_OK) {
            CdxTagIndex_Dispose(idx);
            return -30;
        }
    }
    CdxTagIndex_Dispose(idx);
    f = fopen(path, "rb");
    if (!f) return -31;
    fseek(f, 0, SEEK_END);
    flen1 = ftell(f);
    fclose(f);
    if (flen1 <= flen0) return -32;

    reopen = CdxTagIndex_Open(path, "K", 0);
    if (!reopen) return -33;
    cdxc_sc_key(key, 0);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 1) {
        CdxTagIndex_Dispose(reopen);
        return -34;
    }
    cdxc_sc_key(key, 39);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 40) {
        CdxTagIndex_Dispose(reopen);
        return -35;
    }
    cdxc_sc_key(key, 20);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 21) {
        CdxTagIndex_Dispose(reopen);
        return -36;
    }
    CdxTagIndex_Dispose(reopen);

    cache = CdxPageCache_New(path, 2);
    if (!cache) return -37;
    if (CdxPageCache_GetPage(cache, 512, pg) != GMB_DB_OK
        || CdxPageCache_GetPage(cache, 1024, pg) != GMB_DB_OK
        || CdxPageCache_GetPage(cache, 1536, pg) != GMB_DB_OK) {
        CdxPageCache_Dispose(cache);
        return -38;
    }
    if (CdxPageCache_get_Count(cache) != 2) {
        CdxPageCache_Dispose(cache);
        return -39;
    }
    CdxPageCache_Dispose(cache);

    unlink(path);
    snprintf(path, sizeof(path), "%s/t.cdx", dir);
    unlink(path);
    rmdir(dir);
    return GMB_DB_OK;
}
