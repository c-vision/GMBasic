#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <pthread.h>

/* C#: Basic.DBF.ClipperNtxHeader / ClipperPage / ClipperNtxFile
 * + ClipperNtxBuilder + ClipperNtxWritable (rewrite in-memory).
 * Layout Harbour/Clipper 5.x, pagine 1024, type 0x0006. Non è NtxFile GM. */

static void clip_u16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint16_t clip_ru16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static void clip_u32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t clip_ru32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int ClipperNtxHeader_IsClipperType(uint16_t type)
{
    uint16_t baseType = (uint16_t)(type & 0x87FF);
    return baseType == 0x0006 || baseType == 0x0003;
}

uint32_t ClipperNtxHeader_ResolveBlockAddress(uint32_t block)
{
    return (block > 0 && block < ClipperNtxHeader_PageSize) ? block * ClipperNtxHeader_PageSize : block;
}

uint16_t ClipperNtxBuilder_CalcMaxKeys(int keyLength)
{
    if (keyLength < 0) keyLength = 0;
    return (uint16_t)((ClipperNtxHeader_PageSize - 2) / (keyLength + 10) - 1);
}

static int clip_seq_cmp(const uint8_t* a, int na, const uint8_t* b, int nb)
{
    int n = na < nb ? na : nb;
    int c = n > 0 ? memcmp(a, b, (size_t)n) : 0;
    if (c != 0) return c;
    return na - nb;
}

static void clip_stem(const char* path, char* out, size_t cap)
{
    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;
    const char* dot = strrchr(base, '.');
    size_t n = dot ? (size_t)(dot - base) : strlen(base);
    if (n >= cap) n = cap - 1;
    memcpy(out, base, n);
    out[n] = '\0';
}

typedef struct {
    uint8_t Data[ClipperNtxHeader_PageSize];
    int KeyLength;
    int ItemCount;
} ClipperPage;

static int clip_entry_off(const ClipperPage* p, int index)
{
    return (int)clip_ru16(p->Data + 2 + (index << 1));
}

static void clip_get_key(const ClipperPage* p, int index, uint8_t* out)
{
    memcpy(out, p->Data + clip_entry_off(p, index) + 8, (size_t)p->KeyLength);
}

static uint32_t clip_child(const ClipperPage* p, int index)
{
    return clip_ru32(p->Data + clip_entry_off(p, index));
}

static uint32_t clip_recno(const ClipperPage* p, int index)
{
    return clip_ru32(p->Data + clip_entry_off(p, index) + 4);
}

static int clip_is_leaf(const ClipperPage* p)
{
    return p->ItemCount > 0 && clip_child(p, 0) == 0;
}

static void clip_page_load(ClipperPage* p, const uint8_t* data, int keyLength)
{
    memcpy(p->Data, data, ClipperNtxHeader_PageSize);
    p->KeyLength = keyLength;
    p->ItemCount = (int)clip_ru16(p->Data);
}

struct ClipperNtxFile {
    FILE* fs;
    uint8_t HeaderBuf[ClipperNtxHeader_PageSize];
    uint16_t Type;
    uint32_t RootBlock;
    int KeyLength;
    int Unique;
    int Descending;
    char Name[256];
    char Expr[257];
    char Path[PATH_MAX];
};

static int clip_read_page(ClipperNtxFile* self, uint32_t fileOffset, ClipperPage* page)
{
    uint8_t buf[ClipperNtxHeader_PageSize];
    if (!self->fs) return -1;
    if (fseek(self->fs, (long)fileOffset, SEEK_SET) != 0) return -1;
    if (fread(buf, 1, ClipperNtxHeader_PageSize, self->fs) != ClipperNtxHeader_PageSize) return -1;
    clip_page_load(page, buf, self->KeyLength);
    return 0;
}

static int clip_find_key_index(const ClipperPage* page, const uint8_t* keyBytes, int keyLen, int* exactMatch)
{
    int last = -1, begin = 0, end = page->ItemCount - 1;
    uint8_t midKey[GMB_CLIPPER_MAX_KEY];
    *exactMatch = 0;
    while (begin <= end) {
        int mid = (begin + end) >> 1;
        int cmp;
        clip_get_key(page, mid, midKey);
        cmp = clip_seq_cmp(keyBytes, keyLen, midKey, page->KeyLength);
        if (cmp == 0) {
            *exactMatch = 1;
            return mid;
        }
        if (cmp > 0) {
            begin = mid + 1;
        } else {
            last = mid;
            end = mid - 1;
        }
    }
    return last >= 0 ? last : page->ItemCount;
}

static int clip_find_child_index(const ClipperPage* page, const uint8_t* keyBytes, int keyLen)
{
    int i;
    uint8_t k[GMB_CLIPPER_MAX_KEY];
    for (i = 0; i < page->ItemCount; i++) {
        clip_get_key(page, i, k);
        if (clip_seq_cmp(keyBytes, keyLen, k, page->KeyLength) < 0) return i;
    }
    return page->ItemCount;
}

static int clip_seek_in_page(ClipperNtxFile* self, uint32_t blockAddress, const uint8_t* keyBytes, int keyLen, int soft)
{
    uint32_t pageAddress = ClipperNtxHeader_ResolveBlockAddress(blockAddress);
    int guard = 0;
    while (guard++ < 32) {
        ClipperPage page;
        int exact, keyIndex;
        if (clip_read_page(self, pageAddress, &page) != 0) return -1;
        if (page.ItemCount == 0) return -1;
        if (!clip_is_leaf(&page)) {
            int childIndex = clip_find_child_index(&page, keyBytes, keyLen);
            uint32_t childBlock = clip_child(&page, childIndex);
            if (childBlock == 0) return -1;
            pageAddress = ClipperNtxHeader_ResolveBlockAddress(childBlock);
            continue;
        }
        keyIndex = clip_find_key_index(&page, keyBytes, keyLen, &exact);
        if (exact) return (int)clip_recno(&page, keyIndex);
        if (soft && keyIndex < page.ItemCount) return (int)clip_recno(&page, keyIndex);
        return -1;
    }
    return -1;
}

static int clip_edge_recno(ClipperNtxFile* self, uint32_t blockAddress, int direction)
{
    uint32_t pageAddress = ClipperNtxHeader_ResolveBlockAddress(blockAddress);
    int guard = 0;
    while (guard++ < 32) {
        ClipperPage page;
        int childIndex;
        uint32_t childBlock;
        if (clip_read_page(self, pageAddress, &page) != 0) return 0;
        if (page.ItemCount == 0) return 0;
        childIndex = direction < 0 ? 0 : page.ItemCount;
        childBlock = clip_child(&page, childIndex);
        if (childBlock == 0) {
            int keyIndex = direction < 0 ? 0 : page.ItemCount - 1;
            return (int)clip_recno(&page, keyIndex);
        }
        pageAddress = ClipperNtxHeader_ResolveBlockAddress(childBlock);
    }
    return 0;
}

static int clip_enum_block(ClipperNtxFile* self, uint32_t blockAddress, uint8_t* keysFlat, int* recnos, int cap, int* n, int depth)
{
    ClipperPage page;
    int i;
    if (depth > 32) return GMB_DB_ERR;
    if (clip_read_page(self, ClipperNtxHeader_ResolveBlockAddress(blockAddress), &page) != 0) return GMB_DB_ERR;
    if (page.ItemCount == 0) return GMB_DB_OK;
    if (clip_is_leaf(&page)) {
        for (i = 0; i < page.ItemCount; i++) {
            if (*n >= cap) return GMB_DB_ERR;
            if (keysFlat) clip_get_key(&page, i, keysFlat + (*n) * self->KeyLength);
            if (recnos) recnos[*n] = (int)clip_recno(&page, i);
            (*n)++;
        }
        return GMB_DB_OK;
    }
    for (i = 0; i <= page.ItemCount; i++) {
        uint32_t child = clip_child(&page, i);
        if (child == 0) continue;
        if (clip_enum_block(self, child, keysFlat, recnos, cap, n, depth + 1) != GMB_DB_OK) return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

ClipperNtxFile* ClipperNtxFile_New(const char* path)
{
    ClipperNtxFile* self;
    long len;
    int i;
    if (!path) return NULL;
    self = (ClipperNtxFile*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->Path, sizeof(self->Path), "%s", path);
    clip_stem(path, self->Name, sizeof(self->Name));
    self->fs = fopen(path, "rb");
    if (!self->fs) {
        free(self);
        return NULL;
    }
    if (fseek(self->fs, 0, SEEK_END) != 0) {
        fclose(self->fs);
        free(self);
        return NULL;
    }
    len = ftell(self->fs);
    if (len < ClipperNtxHeader_PageSize) {
        fclose(self->fs);
        free(self);
        return NULL;
    }
    if (fseek(self->fs, 0, SEEK_SET) != 0
        || fread(self->HeaderBuf, 1, ClipperNtxHeader_PageSize, self->fs) != ClipperNtxHeader_PageSize) {
        fclose(self->fs);
        free(self);
        return NULL;
    }
    self->Type = clip_ru16(self->HeaderBuf);
    if (!ClipperNtxHeader_IsClipperType(self->Type)) {
        fclose(self->fs);
        free(self);
        return NULL;
    }
    self->RootBlock = clip_ru32(self->HeaderBuf + 4);
    self->KeyLength = (int)clip_ru16(self->HeaderBuf + 14);
    if (self->KeyLength < 0 || self->KeyLength > GMB_CLIPPER_MAX_KEY) {
        fclose(self->fs);
        free(self);
        return NULL;
    }
    self->Unique = self->HeaderBuf[ClipperNtxHeader_UniqueFlagOffset] != 0;
    self->Descending = self->HeaderBuf[ClipperNtxHeader_DescendingFlagOffset] != 0;
    for (i = 0; i < 256; i++) {
        uint8_t c = self->HeaderBuf[ClipperNtxHeader_KeyExpressionOffset + i];
        if (c == 0) break;
        self->Expr[i] = (char)c;
    }
    self->Expr[i] = '\0';
    return self;
}

void ClipperNtxFile_Dispose(ClipperNtxFile* self)
{
    if (!self) return;
    if (self->fs) fclose(self->fs);
    free(self);
}

int ClipperNtxFile_Seek(ClipperNtxFile* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    if (!self || !keyBytes) return -1;
    if (self->RootBlock == 0) return -1;
    return clip_seek_in_page(self, self->RootBlock, keyBytes, keyLen, soft);
}

int ClipperNtxFile_GetFirstRecordNumber(ClipperNtxFile* self)
{
    if (!self || self->RootBlock == 0) return 0;
    return clip_edge_recno(self, self->RootBlock, -1);
}

int ClipperNtxFile_GetLastRecordNumber(ClipperNtxFile* self)
{
    if (!self || self->RootBlock == 0) return 0;
    return clip_edge_recno(self, self->RootBlock, 1);
}

const char* ClipperNtxFile_GetKeyExpressionString(const ClipperNtxFile* self)
{
    return self ? self->Expr : "";
}

const char* ClipperNtxFile_get_Name(const ClipperNtxFile* self)
{
    return self ? self->Name : "";
}

int ClipperNtxFile_get_KeyLength(const ClipperNtxFile* self)
{
    return self ? self->KeyLength : 0;
}

int ClipperNtxFile_get_Unique(const ClipperNtxFile* self)
{
    return self ? self->Unique : 0;
}

int ClipperNtxFile_get_Descending(const ClipperNtxFile* self)
{
    return self ? self->Descending : 0;
}

int ClipperNtxFile_EnumerateEntries(ClipperNtxFile* self, uint8_t* keysFlat, int* recnos, int cap, int* nOut)
{
    int n = 0;
    if (nOut) *nOut = 0;
    if (!self || cap < 0) return GMB_DB_ERR;
    if (self->RootBlock == 0) return GMB_DB_OK;
    if (clip_enum_block(self, self->RootBlock, keysFlat, recnos, cap, &n, 0) != GMB_DB_OK) return GMB_DB_ERR;
    if (nOut) *nOut = n;
    return GMB_DB_OK;
}

static void clip_fill_offset_table(uint8_t* page, int keyLength, uint16_t maxKeys)
{
    int slot;
    int dataOffset = (maxKeys + 2) << 1;
    for (slot = 0; slot <= maxKeys; slot++) {
        clip_u16(page + 2 + (slot << 1), (uint16_t)dataOffset);
        dataOffset += keyLength + 8;
    }
}

static void clip_build_leaf(uint8_t* page, const uint8_t* keysFlat, const int* recnos, int n, int keyLength, uint16_t maxKeys)
{
    int i;
    memset(page, 0, ClipperNtxHeader_PageSize);
    clip_u16(page, (uint16_t)n);
    clip_fill_offset_table(page, keyLength, maxKeys);
    for (i = 0; i < n; i++) {
        int off = (int)clip_ru16(page + 2 + (i << 1));
        clip_u32(page + off, 0);
        clip_u32(page + off + 4, (uint32_t)recnos[i]);
        if (keyLength > 0 && keysFlat) memcpy(page + off + 8, keysFlat + i * keyLength, (size_t)keyLength);
    }
}

static void clip_build_internal(uint8_t* page, const uint32_t* childPages, int nChildren,
                                const uint8_t* sepsFlat, int nSep, int keyLength, uint16_t maxKeys)
{
    int i;
    memset(page, 0, ClipperNtxHeader_PageSize);
    clip_u16(page, (uint16_t)nSep);
    clip_fill_offset_table(page, keyLength, maxKeys);
    for (i = 0; i < nChildren; i++) {
        int off = (int)clip_ru16(page + 2 + (i << 1));
        clip_u32(page + off, childPages[i]);
        clip_u32(page + off + 4, 0);
        if (i < nSep && sepsFlat) memcpy(page + off + 8, sepsFlat + i * keyLength, (size_t)keyLength);
    }
}

typedef struct {
    uint32_t PageNum;
    uint8_t FirstKey[GMB_CLIPPER_MAX_KEY];
} ClipLevel;

static int clip_build_internal_level(ClipLevel* children, int nChildren, uint8_t pages[][ClipperNtxHeader_PageSize],
                                     int* nPages, int keyLength, uint16_t maxKeys, ClipLevel* parents, int* nParents)
{
    int index = 0;
    *nParents = 0;
    while (index < nChildren) {
        uint32_t childPages[GMB_CLIPPER_MAX_KEY];
        uint8_t seps[GMB_CLIPPER_MAX_KEY * GMB_CLIPPER_MAX_KEY];
        int nChild = 0, nSep = 0, groupStart = index;
        if (*nPages >= GMB_CLIPPER_MAX_PAGES) return GMB_DB_ERR;
        childPages[nChild++] = children[index].PageNum;
        index++;
        while (index < nChildren && nSep < (int)maxKeys) {
            memcpy(seps + nSep * keyLength, children[index].FirstKey, (size_t)keyLength);
            nSep++;
            childPages[nChild++] = children[index].PageNum;
            index++;
        }
        clip_build_internal(pages[*nPages], childPages, nChild, seps, nSep, keyLength, maxKeys);
        (*nPages)++;
        parents[*nParents].PageNum = (uint32_t)(*nPages);
        if (nSep > 0) memcpy(parents[*nParents].FirstKey, seps, (size_t)keyLength);
        else memcpy(parents[*nParents].FirstKey, children[groupStart].FirstKey, (size_t)keyLength);
        (*nParents)++;
    }
    return GMB_DB_OK;
}

typedef struct {
    uint8_t Key[GMB_CLIPPER_MAX_KEY];
    int Recno;
    int Orig;
    int KeyLen;
} ClipSortEnt;

static int clip_sort_cmp(const void* a, const void* b)
{
    const ClipSortEnt* x = a;
    const ClipSortEnt* y = b;
    int c = clip_seq_cmp(x->Key, x->KeyLen, y->Key, y->KeyLen);
    if (c != 0) return c;
    return x->Orig - y->Orig;
}

int ClipperNtxBuilder_Write(const char* path, const char* keyExpression, int keyLength,
                            const uint8_t* keysFlat, const int* recnos, int n, int unique, int descending)
{
    uint16_t maxKeys, itemSize;
    uint8_t (*pages)[ClipperNtxHeader_PageSize];
    ClipLevel* level;
    ClipSortEnt* sorted;
    int nPages = 0, i, exprLen;
    uint32_t rootPage;
    uint8_t header[ClipperNtxHeader_PageSize];
    FILE* fs;
    char parent[PATH_MAX];
    char* slash;

    if (!path || keyLength < 0 || keyLength > GMB_CLIPPER_MAX_KEY || n < 0 || n > GMB_CLIPPER_MAX_ENTRIES) {
        return GMB_DB_ERR;
    }
    maxKeys = ClipperNtxBuilder_CalcMaxKeys(keyLength);
    if (maxKeys < 1) return GMB_DB_ERR;
    itemSize = (uint16_t)(keyLength + 8);

    pages = (uint8_t (*)[ClipperNtxHeader_PageSize])calloc((size_t)GMB_CLIPPER_MAX_PAGES, ClipperNtxHeader_PageSize);
    level = (ClipLevel*)calloc((size_t)GMB_CLIPPER_MAX_PAGES, sizeof(ClipLevel));
    sorted = (ClipSortEnt*)calloc((size_t)(n > 0 ? n : 1), sizeof(ClipSortEnt));
    if (!pages || !level || !sorted) {
        free(pages);
        free(level);
        free(sorted);
        return GMB_DB_ERR;
    }

    for (i = 0; i < n; i++) {
        sorted[i].Recno = recnos[i];
        sorted[i].Orig = i;
        sorted[i].KeyLen = keyLength;
        if (keysFlat && keyLength > 0) memcpy(sorted[i].Key, keysFlat + i * keyLength, (size_t)keyLength);
    }
    if (n > 1) qsort(sorted, (size_t)n, sizeof(ClipSortEnt), clip_sort_cmp);

    if (n == 0) {
        clip_build_leaf(pages[0], NULL, NULL, 0, keyLength, maxKeys);
        nPages = 1;
        rootPage = 1;
    } else {
        ClipLevel leaves[GMB_CLIPPER_MAX_PAGES];
        int nLeaves = 0, pos = 0;
        while (pos < n) {
            int chunk = n - pos;
            int recs[GMB_CLIPPER_MAX_KEY];
            uint8_t keys[GMB_CLIPPER_MAX_KEY * GMB_CLIPPER_MAX_KEY];
            int k;
            if (chunk > (int)maxKeys) chunk = (int)maxKeys;
            if (nPages >= GMB_CLIPPER_MAX_PAGES) {
                free(pages);
                free(level);
                free(sorted);
                return GMB_DB_ERR;
            }
            for (k = 0; k < chunk; k++) {
                recs[k] = sorted[pos + k].Recno;
                memcpy(keys + k * keyLength, sorted[pos + k].Key, (size_t)keyLength);
            }
            clip_build_leaf(pages[nPages], keys, recs, chunk, keyLength, maxKeys);
            nPages++;
            leaves[nLeaves].PageNum = (uint32_t)nPages;
            memcpy(leaves[nLeaves].FirstKey, sorted[pos].Key, (size_t)keyLength);
            nLeaves++;
            pos += chunk;
        }
        memcpy(level, leaves, (size_t)nLeaves * sizeof(ClipLevel));
        while (nLeaves > 1) {
            int nParents = 0;
            if (clip_build_internal_level(level, nLeaves, pages, &nPages, keyLength, maxKeys, leaves, &nParents)
                != GMB_DB_OK) {
                free(pages);
                free(level);
                free(sorted);
                return GMB_DB_ERR;
            }
            memcpy(level, leaves, (size_t)nParents * sizeof(ClipLevel));
            nLeaves = nParents;
        }
        rootPage = level[0].PageNum;
    }

    memset(header, 0, sizeof(header));
    clip_u16(header, 0x0006);
    clip_u16(header + 2, 0x0006);
    clip_u32(header + 4, rootPage);
    clip_u32(header + 8, (uint32_t)(nPages + 1));
    clip_u16(header + 12, itemSize);
    clip_u16(header + 14, (uint16_t)keyLength);
    clip_u16(header + 18, maxKeys);
    clip_u16(header + 20, (uint16_t)(maxKeys / 2));
    exprLen = keyExpression ? (int)strlen(keyExpression) : 0;
    if (exprLen > 255) exprLen = 255;
    if (exprLen > 0) memcpy(header + ClipperNtxHeader_KeyExpressionOffset, keyExpression, (size_t)exprLen);
    if (unique) header[ClipperNtxHeader_UniqueFlagOffset] = 1;
    if (descending) header[ClipperNtxHeader_DescendingFlagOffset] = 1;

    snprintf(parent, sizeof(parent), "%s", path);
    slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        mkdir(parent, 0755);
    }

    fs = fopen(path, "wb");
    if (!fs) {
        free(pages);
        free(level);
        free(sorted);
        return GMB_DB_ERR;
    }
    if (fwrite(header, 1, ClipperNtxHeader_PageSize, fs) != ClipperNtxHeader_PageSize) {
        fclose(fs);
        free(pages);
        free(level);
        free(sorted);
        return GMB_DB_ERR;
    }
    for (i = 0; i < nPages; i++) {
        if (fwrite(pages[i], 1, ClipperNtxHeader_PageSize, fs) != ClipperNtxHeader_PageSize) {
            fclose(fs);
            free(pages);
            free(level);
            free(sorted);
            return GMB_DB_ERR;
        }
    }
    fclose(fs);
    free(pages);
    free(level);
    free(sorted);
    return GMB_DB_OK;
}

typedef struct {
    uint8_t Key[GMB_CLIPPER_MAX_KEY];
    int KeyLen;
    int Recno;
} ClipEnt;

struct ClipperNtxWritable {
    char Path[PATH_MAX];
    char Name[256];
    char Expr[257];
    int KeyLength;
    int Unique;
    int Descending;
    ClipEnt* Entries;
    int N;
    int Cap;
    pthread_mutex_t Lock;
};

static int clipw_flush(ClipperNtxWritable* self)
{
    uint8_t* flat = NULL;
    int* recs = NULL;
    int i, rc;
    if (self->N > 0) {
        flat = (uint8_t*)calloc((size_t)self->N, (size_t)self->KeyLength);
        recs = (int*)calloc((size_t)self->N, sizeof(int));
        if (!flat || !recs) {
            free(flat);
            free(recs);
            return GMB_DB_ERR;
        }
        for (i = 0; i < self->N; i++) {
            int ncopy = self->Entries[i].KeyLen < self->KeyLength ? self->Entries[i].KeyLen : self->KeyLength;
            if (ncopy > 0) memcpy(flat + i * self->KeyLength, self->Entries[i].Key, (size_t)ncopy);
            recs[i] = self->Entries[i].Recno;
        }
    }
    rc = ClipperNtxBuilder_Write(self->Path, self->Expr, self->KeyLength, flat, recs, self->N, self->Unique,
                                 self->Descending);
    free(flat);
    free(recs);
    return rc;
}

static int clipw_find(const ClipperNtxWritable* self, const uint8_t* keyBytes, int keyLen, int* exactMatch)
{
    int lo = 0, hi = self->N - 1, insertAt = self->N;
    *exactMatch = 0;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = clip_seq_cmp(keyBytes, keyLen, self->Entries[mid].Key, self->Entries[mid].KeyLen);
        if (cmp == 0) {
            *exactMatch = 1;
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

ClipperNtxWritable* ClipperNtxWritable_OpenExisting(const char* path)
{
    ClipperNtxFile* reader;
    ClipperNtxWritable* self;
    uint8_t* keys = NULL;
    int* recs = NULL;
    int n = 0, i;

    reader = ClipperNtxFile_New(path);
    if (!reader) return NULL;
    self = (ClipperNtxWritable*)calloc(1, sizeof(*self));
    if (!self) {
        ClipperNtxFile_Dispose(reader);
        return NULL;
    }
    snprintf(self->Path, sizeof(self->Path), "%s", path);
    snprintf(self->Name, sizeof(self->Name), "%s", ClipperNtxFile_get_Name(reader));
    snprintf(self->Expr, sizeof(self->Expr), "%s", ClipperNtxFile_GetKeyExpressionString(reader));
    self->KeyLength = ClipperNtxFile_get_KeyLength(reader);
    self->Unique = ClipperNtxFile_get_Unique(reader);
    self->Descending = ClipperNtxFile_get_Descending(reader);
    pthread_mutex_init(&self->Lock, NULL);
    self->Cap = GMB_CLIPPER_MAX_ENTRIES;
    self->Entries = (ClipEnt*)calloc((size_t)self->Cap, sizeof(ClipEnt));
    if (!self->Entries) {
        pthread_mutex_destroy(&self->Lock);
        free(self);
        ClipperNtxFile_Dispose(reader);
        return NULL;
    }
    if (self->KeyLength > 0) {
        keys = (uint8_t*)calloc((size_t)GMB_CLIPPER_MAX_ENTRIES, (size_t)self->KeyLength);
        recs = (int*)calloc((size_t)GMB_CLIPPER_MAX_ENTRIES, sizeof(int));
        if (!keys || !recs) {
            free(keys);
            free(recs);
            pthread_mutex_destroy(&self->Lock);
            free(self->Entries);
            free(self);
            ClipperNtxFile_Dispose(reader);
            return NULL;
        }
    }
    if (ClipperNtxFile_EnumerateEntries(reader, keys, recs, GMB_CLIPPER_MAX_ENTRIES, &n) != GMB_DB_OK) {
        free(keys);
        free(recs);
        pthread_mutex_destroy(&self->Lock);
        free(self->Entries);
        free(self);
        ClipperNtxFile_Dispose(reader);
        return NULL;
    }
    for (i = 0; i < n; i++) {
        self->Entries[i].KeyLen = self->KeyLength;
        self->Entries[i].Recno = recs ? recs[i] : 0;
        if (keys && self->KeyLength > 0) memcpy(self->Entries[i].Key, keys + i * self->KeyLength, (size_t)self->KeyLength);
    }
    self->N = n;
    free(keys);
    free(recs);
    ClipperNtxFile_Dispose(reader);
    return self;
}

ClipperNtxWritable* ClipperNtxWritable_CreateNew(const char* filePath, const char* keyExpression, int keyLength,
                                                 int isUnique)
{
    if (ClipperNtxBuilder_Write(filePath, keyExpression ? keyExpression : "", keyLength, NULL, NULL, 0, isUnique, 0)
        != GMB_DB_OK) {
        return NULL;
    }
    return ClipperNtxWritable_OpenExisting(filePath);
}

void ClipperNtxWritable_Dispose(ClipperNtxWritable* self)
{
    if (!self) return;
    pthread_mutex_destroy(&self->Lock);
    free(self->Entries);
    free(self);
}

int ClipperNtxWritable_Seek(ClipperNtxWritable* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    int exact, idx, rec = -1;
    if (!self || !keyBytes) return -1;
    pthread_mutex_lock(&self->Lock);
    if (self->N == 0) {
        pthread_mutex_unlock(&self->Lock);
        return -1;
    }
    idx = clipw_find(self, keyBytes, keyLen, &exact);
    if (exact) rec = self->Entries[idx].Recno;
    else if (soft && idx < self->N) rec = self->Entries[idx].Recno;
    pthread_mutex_unlock(&self->Lock);
    return rec;
}

int ClipperNtxWritable_Insert(ClipperNtxWritable* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, copyLen;
    if (!self || !keyBytes) return GMB_DB_ERR;
    pthread_mutex_lock(&self->Lock);
    if (self->N >= self->Cap) {
        pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    idx = clipw_find(self, keyBytes, keyLen, &exact);
    if (self->Unique && exact) {
        pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    if (idx < self->N) {
        memmove(&self->Entries[idx + 1], &self->Entries[idx], (size_t)(self->N - idx) * sizeof(ClipEnt));
    }
    memset(&self->Entries[idx], 0, sizeof(ClipEnt));
    copyLen = keyLen > GMB_CLIPPER_MAX_KEY ? GMB_CLIPPER_MAX_KEY : keyLen;
    if (copyLen < 0) copyLen = 0;
    self->Entries[idx].KeyLen = copyLen;
    self->Entries[idx].Recno = recno;
    if (copyLen > 0) memcpy(self->Entries[idx].Key, keyBytes, (size_t)copyLen);
    self->N++;
    if (clipw_flush(self) != GMB_DB_OK) {
        pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    pthread_mutex_unlock(&self->Lock);
    return GMB_DB_OK;
}

int ClipperNtxWritable_Delete(ClipperNtxWritable* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, i;
    if (!self || !keyBytes) return GMB_DB_ERR;
    pthread_mutex_lock(&self->Lock);
    idx = clipw_find(self, keyBytes, keyLen, &exact);
    if (!exact) {
        pthread_mutex_unlock(&self->Lock);
        return GMB_DB_OK;
    }
    for (i = idx; i < self->N; i++) {
        if (clip_seq_cmp(self->Entries[i].Key, self->Entries[i].KeyLen, keyBytes, keyLen) != 0) break;
        if (self->Entries[i].Recno == recno) {
            if (i + 1 < self->N) {
                memmove(&self->Entries[i], &self->Entries[i + 1], (size_t)(self->N - i - 1) * sizeof(ClipEnt));
            }
            self->N--;
            if (clipw_flush(self) != GMB_DB_OK) {
                pthread_mutex_unlock(&self->Lock);
                return GMB_DB_ERR;
            }
            pthread_mutex_unlock(&self->Lock);
            return GMB_DB_OK;
        }
    }
    pthread_mutex_unlock(&self->Lock);
    return GMB_DB_OK;
}

int ClipperNtxWritable_GetFirstRecordNumber(ClipperNtxWritable* self)
{
    int rec;
    if (!self) return 0;
    pthread_mutex_lock(&self->Lock);
    rec = self->N > 0 ? self->Entries[0].Recno : 0;
    pthread_mutex_unlock(&self->Lock);
    return rec;
}

int ClipperNtxWritable_GetLastRecordNumber(ClipperNtxWritable* self)
{
    int rec;
    if (!self) return 0;
    pthread_mutex_lock(&self->Lock);
    rec = self->N > 0 ? self->Entries[self->N - 1].Recno : 0;
    pthread_mutex_unlock(&self->Lock);
    return rec;
}

const char* ClipperNtxWritable_GetKeyExpressionString(const ClipperNtxWritable* self)
{
    return self ? self->Expr : "";
}

int ClipperNtxWritable_get_KeyLength(const ClipperNtxWritable* self)
{
    return self ? self->KeyLength : 0;
}

const char* ClipperNtxWritable_get_Name(const ClipperNtxWritable* self)
{
    return self ? self->Name : "";
}

int ClipperNtxWritable_get_Unique(const ClipperNtxWritable* self)
{
    return self ? self->Unique : 0;
}

int ClipperNtxWritable_get_EntryCount(const ClipperNtxWritable* self)
{
    return self ? self->N : 0;
}

int ClipperNtxWritable_get_RecnoAt(const ClipperNtxWritable* self, int index)
{
    if (!self || index < 0 || index >= self->N) return 0;
    return self->Entries[index].Recno;
}

static void clip_sc_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int ClipperNtxFile_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/clips_XXXXXX";
    char path[PATH_MAX];
    ClipperNtxWritable* w;
    ClipperNtxFile* ro;
    uint8_t key[10], head[32];
    FILE* f;
    long flen;
    int i, maxKeys;
    IndexHandle handles[4];
    int n = 0;

    if (!mkdtemp(dir)) return -2;
    snprintf(path, sizeof(path), "%s/t.ntx", dir);

    maxKeys = (int)ClipperNtxBuilder_CalcMaxKeys(10);
    if (maxKeys != 50) return -3;

    w = ClipperNtxWritable_CreateNew(path, "NAME", 10, 0);
    if (!w) return -4;
    if (strcmp(ClipperNtxWritable_GetKeyExpressionString(w), "NAME") != 0) {
        ClipperNtxWritable_Dispose(w);
        return -5;
    }

    f = fopen(path, "rb");
    if (!f) {
        ClipperNtxWritable_Dispose(w);
        return -6;
    }
    if (fread(head, 1, 32, f) != 32) {
        fclose(f);
        ClipperNtxWritable_Dispose(w);
        return -7;
    }
    fseek(f, 0, SEEK_END);
    flen = ftell(f);
    fclose(f);
    if (head[0] != 0x06 || head[1] != 0 || head[2] != 0x06 || clip_ru32(head + 4) != 1
        || clip_ru32(head + 8) != 2 || clip_ru16(head + 12) != 18 || clip_ru16(head + 14) != 10
        || clip_ru16(head + 18) != 50 || flen != 2048) {
        ClipperNtxWritable_Dispose(w);
        return -8;
    }

    for (i = 0; i < 8; i++) {
        clip_sc_key(key, i);
        if (ClipperNtxWritable_Insert(w, key, 10, 100 + i) != GMB_DB_OK) {
            ClipperNtxWritable_Dispose(w);
            return -10 - i;
        }
    }
    clip_sc_key(key, 3);
    if (ClipperNtxWritable_Seek(w, key, 10, 0) != 103) {
        ClipperNtxWritable_Dispose(w);
        return -20;
    }
    if (ClipperNtxWritable_GetFirstRecordNumber(w) != 100 || ClipperNtxWritable_GetLastRecordNumber(w) != 107) {
        ClipperNtxWritable_Dispose(w);
        return -21;
    }
    clip_sc_key(key, 3);
    if (ClipperNtxWritable_Delete(w, key, 10, 103) != GMB_DB_OK
        || ClipperNtxWritable_Seek(w, key, 10, 0) != -1) {
        ClipperNtxWritable_Dispose(w);
        return -22;
    }
    clip_sc_key(key, 3);
    if (ClipperNtxWritable_Seek(w, key, 10, 1) != 104) {
        ClipperNtxWritable_Dispose(w);
        return -23;
    }
    ClipperNtxWritable_Dispose(w);

    ro = ClipperNtxFile_New(path);
    if (!ro) return -24;
    clip_sc_key(key, 2);
    if (ClipperNtxFile_Seek(ro, key, 10, 0) != 102) {
        ClipperNtxFile_Dispose(ro);
        return -25;
    }
    if (ClipperNtxFile_GetFirstRecordNumber(ro) != 100 || ClipperNtxFile_GetLastRecordNumber(ro) != 107) {
        ClipperNtxFile_Dispose(ro);
        return -26;
    }
    ClipperNtxFile_Dispose(ro);

    /* unique */
    snprintf(path, sizeof(path), "%s/u.ntx", dir);
    w = ClipperNtxWritable_CreateNew(path, "ID", 10, 1);
    if (!w) return -27;
    clip_sc_key(key, 0);
    if (ClipperNtxWritable_Insert(w, key, 10, 1) != GMB_DB_OK) {
        ClipperNtxWritable_Dispose(w);
        return -28;
    }
    if (ClipperNtxWritable_Insert(w, key, 10, 2) != GMB_DB_ERR) {
        ClipperNtxWritable_Dispose(w);
        return -29;
    }
    ClipperNtxWritable_Dispose(w);

    /* più foglie: 80 chiavi, maxKeys=50 */
    snprintf(path, sizeof(path), "%s/big.ntx", dir);
    w = ClipperNtxWritable_CreateNew(path, "NAME", 10, 0);
    if (!w) return -30;
    for (i = 0; i < 80; i++) {
        clip_sc_key(key, i);
        if (ClipperNtxWritable_Insert(w, key, 10, i + 1) != GMB_DB_OK) {
            ClipperNtxWritable_Dispose(w);
            return -31;
        }
    }
    clip_sc_key(key, 70);
    if (ClipperNtxWritable_Seek(w, key, 10, 0) != 71) {
        ClipperNtxWritable_Dispose(w);
        return -32;
    }
    ClipperNtxWritable_Dispose(w);
    ro = ClipperNtxFile_New(path);
    if (!ro) return -33;
    for (i = 0; i < 80; i++) {
        clip_sc_key(key, i);
        if (ClipperNtxFile_Seek(ro, key, 10, 0) != i + 1) {
            ClipperNtxFile_Dispose(ro);
            return -34;
        }
    }
    if (ClipperNtxFile_GetFirstRecordNumber(ro) != 1 || ClipperNtxFile_GetLastRecordNumber(ro) != 80) {
        ClipperNtxFile_Dispose(ro);
        return -35;
    }
    ClipperNtxFile_Dispose(ro);

    if (IndexFactory_CreateHandles(path, 0, handles, 4, &n) != GMB_DB_OK || n != 1
        || handles[0].Kind != IndexHandleKind_ClipperNtx) {
        return -36;
    }
    clip_sc_key(key, 70);
    if (ClipperNtxFile_Seek(handles[0].Clipper, key, 10, 0) != 71) {
        IndexHandle_Dispose(&handles[0]);
        return -37;
    }
    IndexHandle_Dispose(&handles[0]);

    if (IndexFactory_CreateHandles(path, 1, handles, 4, &n) != GMB_DB_OK || n != 1
        || handles[0].Kind != IndexHandleKind_ClipperNtxWritable) {
        return -38;
    }
    clip_sc_key(key, 70);
    if (ClipperNtxWritable_Seek(handles[0].ClipperW, key, 10, 0) != 71) {
        IndexHandle_Dispose(&handles[0]);
        return -39;
    }
    IndexHandle_Dispose(&handles[0]);

    return GMB_DB_OK;
}
