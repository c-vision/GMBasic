#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#define BPlusPage_Size 4096

typedef struct {
    uint8_t* p;
    int n, cap;
} Bw;
typedef struct {
    const uint8_t* p;
    int n, i;
} Br;

static void bw_grow(Bw* b, int add)
{
    if (b->n + add <= b->cap) return;
    b->cap = b->cap ? b->cap * 2 : 256;
    while (b->n + add > b->cap) b->cap *= 2;
    b->p = (uint8_t*)realloc(b->p, (size_t)b->cap);
}
static void bw_u8(Bw* b, uint8_t v) { bw_grow(b, 1); b->p[b->n++] = v; }
static void bw_i32(Bw* b, int32_t v)
{
    bw_grow(b, 4);
    b->p[b->n++] = (uint8_t)v; b->p[b->n++] = (uint8_t)(v >> 8);
    b->p[b->n++] = (uint8_t)(v >> 16); b->p[b->n++] = (uint8_t)(v >> 24);
}
static void bw_i64(Bw* b, int64_t v)
{
    int i;
    bw_grow(b, 8);
    for (i = 0; i < 8; i++) b->p[b->n++] = (uint8_t)((uint64_t)v >> (8 * i));
}
static void bw_bytes(Bw* b, const uint8_t* d, int n)
{
    if (n <= 0) return;
    bw_grow(b, n);
    memcpy(b->p + b->n, d, (size_t)n);
    b->n += n;
}

static int br_u8(Br* r, uint8_t* v)
{
    if (r->i >= r->n) return -1;
    *v = r->p[r->i++];
    return 0;
}
static int br_i32(Br* r, int32_t* v)
{
    if (r->i + 4 > r->n) return -1;
    *v = (int32_t)(r->p[r->i] | (r->p[r->i + 1] << 8) | (r->p[r->i + 2] << 16) | (r->p[r->i + 3] << 24));
    r->i += 4;
    return 0;
}
static int br_i64(Br* r, int64_t* v)
{
    int i;
    uint64_t u = 0;
    if (r->i + 8 > r->n) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)r->p[r->i++] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

typedef struct BPlusNode {
    int64_t NodeOffset;
    int IsLeaf;
    int KeyCount, KeyCap;
    uint8_t** Keys;
    int* KeyLens;
    int64_t NextLeafOffset, PrevLeafOffset;
    int64_t** Recs;
    int* RecCounts;
    int64_t* Children;
    int ChildCount, ChildCap;
} BPlusNode;

typedef struct {
    int NodeUpdated;
    BPlusNode* NewNode;
    int64_t NewNodeOffset;
    uint8_t* PromotedKey;
    int PromotedKeyLen;
} BPlusInsertResult;

struct BPlusTreeManager {
    FILE* fs;
    IsamFileHandle* handle; /* shared header when Engine owns the file */
    char FilePath[PATH_MAX];
    int KeyLength;
    int Unique;
    int ownsFile;
    int secondarySlot; /* -1 = PK root; 0..14 = SecondaryIndexRootOffsets[slot] */
    int64_t rootNodeOffset;
    IsamLruCache* nodeCache;
    pthread_mutex_t syncRoot;
};

static BPlusNode* BPlusNode_New(int isLeaf)
{
    BPlusNode* n = (BPlusNode*)calloc(1, sizeof(*n));
    n->IsLeaf = isLeaf;
    n->NodeOffset = 0;
    n->NextLeafOffset = -1;
    n->PrevLeafOffset = -1;
    return n;
}

static void BPlusNode_EnsureKeys(BPlusNode* n, int need)
{
    if (n->KeyCap >= need) return;
    n->KeyCap = n->KeyCap ? n->KeyCap * 2 : 8;
    while (n->KeyCap < need) n->KeyCap *= 2;
    n->Keys = (uint8_t**)realloc(n->Keys, (size_t)n->KeyCap * sizeof(uint8_t*));
    n->KeyLens = (int*)realloc(n->KeyLens, (size_t)n->KeyCap * sizeof(int));
    if (n->IsLeaf) {
        n->Recs = (int64_t**)realloc(n->Recs, (size_t)n->KeyCap * sizeof(int64_t*));
        n->RecCounts = (int*)realloc(n->RecCounts, (size_t)n->KeyCap * sizeof(int));
    }
}

static void BPlusNode_EnsureChildren(BPlusNode* n, int need)
{
    if (n->ChildCap >= need) return;
    n->ChildCap = n->ChildCap ? n->ChildCap * 2 : 8;
    while (n->ChildCap < need) n->ChildCap *= 2;
    n->Children = (int64_t*)realloc(n->Children, (size_t)n->ChildCap * sizeof(int64_t));
}

static uint8_t* bplus_dup_key(const uint8_t* k, int n)
{
    uint8_t* p = (uint8_t*)malloc((size_t)n);
    memcpy(p, k, (size_t)n);
    return p;
}

static int BPlusNode_FindKey(const BPlusNode* n, const uint8_t* key, int keyLen, int* found)
{
    int low = 0, high = n->KeyCount - 1;
    *found = 0;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = ByteArrayComparer_Compare(n->Keys[mid], n->KeyLens[mid], key, keyLen);
        if (cmp == 0) { *found = 1; return mid; }
        if (cmp < 0) low = mid + 1;
        else high = mid - 1;
    }
    return low;
}

int BPlusTreeManager_GetMaxKeysPerNode(const BPlusTreeManager* self)
{
    int leafHeaderSize = 1 + 8 + 8 + 4;
    int perLeafKey = 4 + self->KeyLength + 4 + 8;
    int n = (BPlusPage_Size - leafHeaderSize) / perLeafKey;
    return n < 2 ? 2 : n;
}

int BPlusTreeManager_GetMaxKeysForInternalNode(const BPlusTreeManager* self)
{
    int internalHeaderSize = 1 + 4, childListOverhead = 4, perChild = 8;
    int perKey = 4 + self->KeyLength;
    int bytesPerKeySlot = perKey + perChild;
    int fixedOverhead = internalHeaderSize + childListOverhead + perChild;
    int n = (BPlusPage_Size - fixedOverhead) / bytesPerKeySlot;
    return n < 2 ? 2 : n;
}

int64_t BPlusTreeManager_GetRootOffset(const BPlusTreeManager* self) { return self->rootNodeOffset; }

static int BPlusTreeManager_ReadHeader(BPlusTreeManager* self, IsamFileHeader* h)
{
    if (self->handle) {
        *h = self->handle->Header;
        return GMB_DB_OK;
    }
    return IsamFileHeader_Read(self->fs, h);
}

static int BPlusTreeManager_WriteHeader(BPlusTreeManager* self, const IsamFileHeader* h)
{
    if (self->handle) self->handle->Header = *h;
    return IsamFileHeader_Write(self->fs, h);
}

static int64_t BPlusTreeManager_AllocateNewPage(BPlusTreeManager* self)
{
    IsamFileHeader h;
    int64_t offset;
    if (BPlusTreeManager_ReadHeader(self, &h) != GMB_DB_OK) return -1;
    if (h.PageFreeListHeadOffset != -1) {
        int64_t next = -1;
        uint8_t buf[8];
        fseek(self->fs, (long)h.PageFreeListHeadOffset, SEEK_SET);
        if (fread(buf, 1, 8, self->fs) == 8) {
            int i;
            uint64_t u = 0;
            for (i = 0; i < 8; i++) u |= (uint64_t)buf[i] << (8 * i);
            next = (int64_t)u;
            if (next <= 0) next = -1;
        }
        offset = h.PageFreeListHeadOffset;
        h.PageFreeListHeadOffset = next;
        BPlusTreeManager_WriteHeader(self, &h);
        IsamLruCache_Remove(self->nodeCache, (uint64_t)offset);
        return offset;
    }
    offset = h.NextAvailablePageOffset;
    h.NextAvailablePageOffset = offset + BPlusPage_Size;
    BPlusTreeManager_WriteHeader(self, &h);
    if (ftruncate(fileno(self->fs), (off_t)h.NextAvailablePageOffset) != 0) return -1;
    return offset;
}

static void BPlusLeaf_Serialize(const BPlusNode* n, Bw* b)
{
    int i, j;
    bw_u8(b, 1);
    bw_i64(b, n->NextLeafOffset);
    bw_i64(b, n->PrevLeafOffset);
    bw_i32(b, n->KeyCount);
    for (i = 0; i < n->KeyCount; i++) {
        bw_i32(b, n->KeyLens[i]);
        bw_bytes(b, n->Keys[i], n->KeyLens[i]);
        bw_i32(b, n->RecCounts[i]);
        for (j = 0; j < n->RecCounts[i]; j++) bw_i64(b, n->Recs[i][j]);
    }
}

static void BPlusInternal_Serialize(const BPlusNode* n, Bw* b)
{
    int i;
    bw_u8(b, 0);
    bw_i32(b, n->KeyCount);
    for (i = 0; i < n->KeyCount; i++) {
        bw_i32(b, n->KeyLens[i]);
        bw_bytes(b, n->Keys[i], n->KeyLens[i]);
    }
    bw_i32(b, n->ChildCount);
    for (i = 0; i < n->ChildCount; i++) bw_i64(b, n->Children[i]);
}

static int BPlusLeaf_Deserialize(BPlusNode* n, Br* r)
{
    uint8_t leaf;
    int32_t keyCount, i, j, klen, oc;
    if (br_u8(r, &leaf) || br_i64(r, &n->NextLeafOffset) || br_i64(r, &n->PrevLeafOffset) || br_i32(r, &keyCount))
        return -1;
    n->IsLeaf = 1;
    BPlusNode_EnsureKeys(n, keyCount);
    for (i = 0; i < keyCount; i++) {
        if (br_i32(r, &klen) || r->i + klen > r->n) return -1;
        n->Keys[i] = (uint8_t*)malloc((size_t)klen);
        memcpy(n->Keys[i], r->p + r->i, (size_t)klen);
        r->i += klen;
        n->KeyLens[i] = klen;
        if (br_i32(r, &oc)) return -1;
        n->RecCounts[i] = oc;
        n->Recs[i] = (int64_t*)malloc((size_t)oc * sizeof(int64_t));
        for (j = 0; j < oc; j++)
            if (br_i64(r, &n->Recs[i][j])) return -1;
    }
    n->KeyCount = keyCount;
    return 0;
}

static int BPlusInternal_Deserialize(BPlusNode* n, Br* r)
{
    uint8_t leaf;
    int32_t keyCount, childCount, i, klen;
    if (br_u8(r, &leaf) || br_i32(r, &keyCount)) return -1;
    n->IsLeaf = 0;
    BPlusNode_EnsureKeys(n, keyCount);
    for (i = 0; i < keyCount; i++) {
        if (br_i32(r, &klen) || r->i + klen > r->n) return -1;
        n->Keys[i] = (uint8_t*)malloc((size_t)klen);
        memcpy(n->Keys[i], r->p + r->i, (size_t)klen);
        r->i += klen;
        n->KeyLens[i] = klen;
    }
    n->KeyCount = keyCount;
    if (br_i32(r, &childCount)) return -1;
    BPlusNode_EnsureChildren(n, childCount);
    for (i = 0; i < childCount; i++)
        if (br_i64(r, &n->Children[i])) return -1;
    n->ChildCount = childCount;
    return 0;
}

static BPlusNode* BPlusTreeManager_LoadNodeFromDisk(BPlusTreeManager* self, int64_t offset)
{
    uint8_t page[BPlusPage_Size];
    uint32_t crc;
    BPlusNode* n;
    Br r;
    fseek(self->fs, (long)offset, SEEK_SET);
    if (fread(page, 1, BPlusPage_Size, self->fs) != BPlusPage_Size) return NULL;
    if (!IsamPageChecksum_TryValidate(page, BPlusPage_Size, &crc) && crc != 0) return NULL;
    if (page[0] != 0 && page[0] != 1) return NULL;
    n = BPlusNode_New(page[0] == 1);
    n->NodeOffset = offset;
    r.p = page; r.n = BPlusPage_Size; r.i = 0;
    if (n->IsLeaf) {
        if (BPlusLeaf_Deserialize(n, &r) != 0) return NULL;
    } else {
        if (BPlusInternal_Deserialize(n, &r) != 0) return NULL;
    }
    return n;
}

static BPlusNode* BPlusTreeManager_LoadNode(BPlusTreeManager* self, int64_t offset)
{
    void* cached = NULL;
    BPlusNode* n;
    if (offset < 0) return NULL;
    if (IsamLruCache_TryGetValue(self->nodeCache, (uint64_t)offset, &cached) && cached)
        return (BPlusNode*)cached;
    n = BPlusTreeManager_LoadNodeFromDisk(self, offset);
    if (n) IsamLruCache_AddOrUpdate(self->nodeCache, (uint64_t)offset, n);
    return n;
}

static int64_t BPlusTreeManager_SaveNode(BPlusTreeManager* self, BPlusNode* node)
{
    uint8_t page[BPlusPage_Size];
    Bw b = {0};
    if (node->NodeOffset <= 0) {
        node->NodeOffset = BPlusTreeManager_AllocateNewPage(self);
        if (node->NodeOffset < 0) return -1;
    }
    if (node->IsLeaf) BPlusLeaf_Serialize(node, &b);
    else BPlusInternal_Serialize(node, &b);
    if (b.n > BPlusPage_Size) { free(b.p); return -1; }
    memset(page, 0, sizeof(page));
    memcpy(page, b.p, (size_t)b.n);
    free(b.p);
    IsamPageChecksum_WriteChecksum(page, BPlusPage_Size);
    fseek(self->fs, (long)node->NodeOffset, SEEK_SET);
    if (fwrite(page, 1, BPlusPage_Size, self->fs) != BPlusPage_Size) return -1;
    fflush(self->fs);
    IsamLruCache_AddOrUpdate(self->nodeCache, (uint64_t)node->NodeOffset, node);
    return node->NodeOffset;
}

static void BPlusTreeManager_UpdateRootOffsetInHeader(BPlusTreeManager* self)
{
    IsamFileHeader h;
    if (BPlusTreeManager_ReadHeader(self, &h) != GMB_DB_OK) return;
    if (self->secondarySlot >= 0 && self->secondarySlot < 15)
        h.SecondaryIndexRootOffsets[self->secondarySlot] = self->rootNodeOffset;
    else
        h.PrimaryKeyRootOffset = self->rootNodeOffset;
    BPlusTreeManager_WriteHeader(self, &h);
    fflush(self->fs);
}

static BPlusInsertResult BPlusLeaf_SplitAndPromote(BPlusTreeManager* self, BPlusNode* leaf)
{
    BPlusNode* sibling = BPlusNode_New(1);
    int splitIndex = BPlusFillFactor_ComputeLeafSplitIndex(leaf->KeyCount);
    int right = leaf->KeyCount - splitIndex, i;
    BPlusInsertResult r;
    int64_t originalNext = leaf->NextLeafOffset;
    BPlusNode_EnsureKeys(sibling, right);
    for (i = 0; i < right; i++) {
        int src = splitIndex + i;
        sibling->Keys[i] = leaf->Keys[src];
        sibling->KeyLens[i] = leaf->KeyLens[src];
        sibling->Recs[i] = leaf->Recs[src];
        sibling->RecCounts[i] = leaf->RecCounts[src];
    }
    sibling->KeyCount = right;
    leaf->KeyCount = splitIndex;
    if (leaf->NodeOffset <= 0) BPlusTreeManager_SaveNode(self, leaf);
    sibling->NextLeafOffset = originalNext;
    sibling->PrevLeafOffset = leaf->NodeOffset;
    BPlusTreeManager_SaveNode(self, sibling);
    leaf->NextLeafOffset = sibling->NodeOffset;
    if (originalNext != -1) {
        BPlusNode* nxt = BPlusTreeManager_LoadNode(self, originalNext);
        if (nxt && nxt->IsLeaf) {
            nxt->PrevLeafOffset = sibling->NodeOffset;
            BPlusTreeManager_SaveNode(self, nxt);
        }
    }
    BPlusTreeManager_SaveNode(self, sibling);
    BPlusTreeManager_SaveNode(self, leaf);
    memset(&r, 0, sizeof(r));
    r.NodeUpdated = 1;
    r.NewNode = sibling;
    r.NewNodeOffset = sibling->NodeOffset;
    r.PromotedKey = sibling->Keys[0];
    r.PromotedKeyLen = sibling->KeyLens[0];
    return r;
}

static BPlusInsertResult BPlusLeaf_Insert(BPlusTreeManager* self, BPlusNode* leaf,
                                          const uint8_t* key, int keyLen, int64_t recordOffset)
{
    int found, index, i;
    BPlusInsertResult none;
    memset(&none, 0, sizeof(none));
    none.NewNodeOffset = -1;
    index = BPlusNode_FindKey(leaf, key, keyLen, &found);
    if (found) {
        if (self->Unique) {
            int j;
            for (j = 0; j < leaf->RecCounts[index]; j++)
                if (leaf->Recs[index][j] == recordOffset) {
                    none.NodeUpdated = 1;
                    BPlusTreeManager_SaveNode(self, leaf);
                    return none;
                }
            none.NodeUpdated = 0;
            return none; /* uniqueness violation → ERR via NodeUpdated=0 and NewNode=NULL; caller ignores */
        }
        leaf->Recs[index] = (int64_t*)realloc(leaf->Recs[index], (size_t)(leaf->RecCounts[index] + 1) * sizeof(int64_t));
        leaf->Recs[index][leaf->RecCounts[index]++] = recordOffset;
        BPlusTreeManager_SaveNode(self, leaf);
        none.NodeUpdated = 1;
        return none;
    }
    BPlusNode_EnsureKeys(leaf, leaf->KeyCount + 1);
    for (i = leaf->KeyCount; i > index; i--) {
        leaf->Keys[i] = leaf->Keys[i - 1];
        leaf->KeyLens[i] = leaf->KeyLens[i - 1];
        leaf->Recs[i] = leaf->Recs[i - 1];
        leaf->RecCounts[i] = leaf->RecCounts[i - 1];
    }
    leaf->Keys[index] = bplus_dup_key(key, keyLen);
    leaf->KeyLens[index] = keyLen;
    leaf->Recs[index] = (int64_t*)malloc(sizeof(int64_t));
    leaf->Recs[index][0] = recordOffset;
    leaf->RecCounts[index] = 1;
    leaf->KeyCount++;
    if (leaf->KeyCount <= BPlusTreeManager_GetMaxKeysPerNode(self)) {
        BPlusTreeManager_SaveNode(self, leaf);
        none.NodeUpdated = 1;
        return none;
    }
    return BPlusLeaf_SplitAndPromote(self, leaf);
}

static BPlusInsertResult BPlusInternal_Insert(BPlusTreeManager* self, BPlusNode* rootInternal,
                                              const uint8_t* key, int keyLen, int64_t recordOffset)
{
    typedef struct { BPlusNode* Node; int ChildIndex; } Spine;
    Spine spine[64];
    int depth = 0, visitedN = 0;
    int64_t visited[64];
    BPlusNode* node = rootInternal;
    BPlusInsertResult result, split, none;
    int i, found;
    memset(&none, 0, sizeof(none));
    none.NewNodeOffset = -1;
    while (!node->IsLeaf) {
        int index;
        int64_t childOffset;
        if (depth >= 64) return none;
        index = BPlusNode_FindKey(node, key, keyLen, &found);
        /* C#: se trovato, NON +1 in Insert (solo Search va a destra) */
        childOffset = node->Children[index];
        if (childOffset <= 0) return none;
        for (i = 0; i < visitedN; i++) if (visited[i] == childOffset) return none;
        visited[visitedN++] = childOffset;
        spine[depth].Node = node;
        spine[depth].ChildIndex = index;
        depth++;
        node = BPlusTreeManager_LoadNode(self, childOffset);
        if (!node) return none;
    }
    result = BPlusLeaf_Insert(self, node, key, keyLen, recordOffset);
    if (!result.NewNode) return none;
    if (!result.PromotedKey) return none;
    split = result;
    for (i = depth - 1; i >= 0; i--) {
        BPlusNode* parent = spine[i].Node;
        int childIndex = spine[i].ChildIndex, k, maxInternal;
        if (split.NewNodeOffset < 0)
            split.NewNodeOffset = BPlusTreeManager_SaveNode(self, split.NewNode);
        BPlusNode_EnsureKeys(parent, parent->KeyCount + 1);
        BPlusNode_EnsureChildren(parent, parent->ChildCount + 1);
        for (k = parent->KeyCount; k > childIndex; k--) {
            parent->Keys[k] = parent->Keys[k - 1];
            parent->KeyLens[k] = parent->KeyLens[k - 1];
        }
        parent->Keys[childIndex] = bplus_dup_key(split.PromotedKey, split.PromotedKeyLen);
        parent->KeyLens[childIndex] = split.PromotedKeyLen;
        parent->KeyCount++;
        for (k = parent->ChildCount; k > childIndex + 1; k--)
            parent->Children[k] = parent->Children[k - 1];
        parent->Children[childIndex + 1] = split.NewNodeOffset;
        parent->ChildCount++;
        maxInternal = BPlusTreeManager_GetMaxKeysForInternalNode(self);
        if (parent->KeyCount <= maxInternal) {
            BPlusTreeManager_SaveNode(self, parent);
            return none;
        }
        {
            BPlusNode* sibling = BPlusNode_New(0);
            int splitIndex = BPlusFillFactor_ComputeInternalSplitIndex(parent->KeyCount);
            uint8_t* promoted = parent->Keys[splitIndex];
            int promotedLen = parent->KeyLens[splitIndex];
            int rightKeys = parent->KeyCount - splitIndex - 1;
            int rightChildren = parent->ChildCount - (splitIndex + 1);
            int j;
            BPlusNode_EnsureKeys(sibling, rightKeys);
            BPlusNode_EnsureChildren(sibling, rightChildren);
            for (j = 0; j < rightKeys; j++) {
                sibling->Keys[j] = parent->Keys[splitIndex + 1 + j];
                sibling->KeyLens[j] = parent->KeyLens[splitIndex + 1 + j];
            }
            sibling->KeyCount = rightKeys;
            for (j = 0; j < rightChildren; j++)
                sibling->Children[j] = parent->Children[splitIndex + 1 + j];
            sibling->ChildCount = rightChildren;
            parent->KeyCount = splitIndex;
            parent->ChildCount = splitIndex + 1;
            BPlusTreeManager_SaveNode(self, parent);
            BPlusTreeManager_SaveNode(self, sibling);
            memset(&split, 0, sizeof(split));
            split.NodeUpdated = 1;
            split.NewNode = sibling;
            split.NewNodeOffset = sibling->NodeOffset;
            split.PromotedKey = promoted;
            split.PromotedKeyLen = promotedLen;
        }
    }
    return split;
}

static int BPlusTreeManager_SearchNode(BPlusTreeManager* self, BPlusNode* node,
                                       const uint8_t* key, int keyLen, int64_t* offsets, int cap, int* count)
{
    int found, index;
    if (node->IsLeaf) {
        index = BPlusNode_FindKey(node, key, keyLen, &found);
        *count = 0;
        if (!found) return GMB_DB_OK;
        if (offsets && cap > 0) {
            int n = node->RecCounts[index] < cap ? node->RecCounts[index] : cap;
            int i;
            for (i = 0; i < n; i++) offsets[i] = node->Recs[index][i];
            *count = n;
        } else {
            *count = node->RecCounts[index];
        }
        return GMB_DB_OK;
    }
    index = BPlusNode_FindKey(node, key, keyLen, &found);
    if (found) index += 1;
    return BPlusTreeManager_SearchNode(self, BPlusTreeManager_LoadNode(self, node->Children[index]),
                                       key, keyLen, offsets, cap, count);
}

BPlusTreeManager* BPlusTreeManager_New(const char* filePath, int keyLength, int isUnique)
{
    BPlusTreeManager* self = (BPlusTreeManager*)calloc(1, sizeof(*self));
    IsamFileHeader h;
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", filePath);
    self->KeyLength = keyLength;
    self->Unique = isUnique;
    self->ownsFile = 1;
    self->secondarySlot = -1;
    self->nodeCache = IsamLruCache_New();
    pthread_mutex_init(&self->syncRoot, NULL);
    self->fs = fopen(filePath, "rb+");
    if (!self->fs || BPlusTreeManager_ReadHeader(self, &h) != GMB_DB_OK) {
        if (self->fs) fclose(self->fs);
        IsamLruCache_Dispose(self->nodeCache);
        free(self);
        return NULL;
    }
    self->rootNodeOffset = h.PrimaryKeyRootOffset;
    if (self->rootNodeOffset == 0) self->rootNodeOffset = -1;
    return self;
}

BPlusTreeManager* BPlusTreeManager_NewOnHandleAt(IsamFileHandle* handle, int keyLength, int isUnique,
                                                 int secondarySlot)
{
    BPlusTreeManager* self;
    int64_t root;
    if (!handle || !handle->fs) return NULL;
    self = (BPlusTreeManager*)calloc(1, sizeof(*self));
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", handle->FilePath);
    self->handle = handle;
    self->fs = handle->fs;
    self->KeyLength = keyLength;
    self->Unique = isUnique;
    self->ownsFile = 0;
    self->secondarySlot = secondarySlot;
    self->nodeCache = IsamLruCache_New();
    pthread_mutex_init(&self->syncRoot, NULL);
    if (secondarySlot >= 0 && secondarySlot < 15)
        root = handle->Header.SecondaryIndexRootOffsets[secondarySlot];
    else
        root = handle->Header.PrimaryKeyRootOffset;
    self->rootNodeOffset = (root == 0) ? -1 : root;
    return self;
}

BPlusTreeManager* BPlusTreeManager_NewOnHandle(IsamFileHandle* handle, int keyLength, int isUnique)
{
    return BPlusTreeManager_NewOnHandleAt(handle, keyLength, isUnique, -1);
}

void BPlusTreeManager_Dispose(BPlusTreeManager* self)
{
    if (!self) return;
    pthread_mutex_lock(&self->syncRoot);
    if (self->ownsFile && self->fs) fclose(self->fs);
    pthread_mutex_unlock(&self->syncRoot);
    IsamLruCache_Dispose(self->nodeCache);
    pthread_mutex_destroy(&self->syncRoot);
    free(self);
}

int BPlusTreeManager_Insert(BPlusTreeManager* self, const uint8_t* key, int keyLen, int64_t recordOffset)
{
    BPlusInsertResult result;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootNodeOffset == -1) {
        BPlusNode* root = BPlusNode_New(1);
        BPlusLeaf_Insert(self, root, key, keyLen, recordOffset);
        self->rootNodeOffset = BPlusTreeManager_SaveNode(self, root);
        BPlusTreeManager_UpdateRootOffsetInHeader(self);
        pthread_mutex_unlock(&self->syncRoot);
        return GMB_DB_OK;
    }
    {
        BPlusNode* rootNode = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
        if (!rootNode) { pthread_mutex_unlock(&self->syncRoot); return GMB_DB_ERR; }
        result = rootNode->IsLeaf
            ? BPlusLeaf_Insert(self, rootNode, key, keyLen, recordOffset)
            : BPlusInternal_Insert(self, rootNode, key, keyLen, recordOffset);
        if (result.NewNode) {
            int64_t siblingOff = result.NewNodeOffset;
            BPlusNode* newRoot;
            if (siblingOff < 0) siblingOff = BPlusTreeManager_SaveNode(self, result.NewNode);
            newRoot = BPlusNode_New(0);
            BPlusNode_EnsureKeys(newRoot, 1);
            BPlusNode_EnsureChildren(newRoot, 2);
            newRoot->Keys[0] = bplus_dup_key(result.PromotedKey, result.PromotedKeyLen);
            newRoot->KeyLens[0] = result.PromotedKeyLen;
            newRoot->KeyCount = 1;
            newRoot->Children[0] = self->rootNodeOffset;
            newRoot->Children[1] = siblingOff;
            newRoot->ChildCount = 2;
            self->rootNodeOffset = BPlusTreeManager_SaveNode(self, newRoot);
            BPlusTreeManager_UpdateRootOffsetInHeader(self);
        }
    }
    pthread_mutex_unlock(&self->syncRoot);
    return GMB_DB_OK;
}

int BPlusTreeManager_Search(BPlusTreeManager* self, const uint8_t* key, int keyLen,
                            int64_t* offsets, int cap, int* count)
{
    BPlusNode* root;
    int rc;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootNodeOffset == -1) { *count = 0; pthread_mutex_unlock(&self->syncRoot); return GMB_DB_OK; }
    root = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    if (!root) { pthread_mutex_unlock(&self->syncRoot); return GMB_DB_ERR; }
    rc = BPlusTreeManager_SearchNode(self, root, key, keyLen, offsets, cap, count);
    pthread_mutex_unlock(&self->syncRoot);
    return rc;
}

static int BPlusTreeManager_SearchLowerBoundNode(BPlusTreeManager* self, BPlusNode* node,
                                                 const uint8_t* key, int keyLen,
                                                 int64_t* offsets, int cap, int* count)
{
    int found, index, n, i;
    *count = 0;
    if (!node) return GMB_DB_ERR;
    if (node->IsLeaf) {
        index = BPlusNode_FindKey(node, key, keyLen, &found);
        if (index >= node->KeyCount) {
            if (node->NextLeafOffset < 0) return GMB_DB_OK;
            node = BPlusTreeManager_LoadNode(self, node->NextLeafOffset);
            if (!node || node->KeyCount <= 0) return GMB_DB_OK;
            index = 0;
        }
        n = node->RecCounts[index] < cap ? node->RecCounts[index] : cap;
        for (i = 0; i < n; i++) offsets[i] = node->Recs[index][i];
        *count = n;
        return GMB_DB_OK;
    }
    index = BPlusNode_FindKey(node, key, keyLen, &found);
    if (found) index += 1;
    return BPlusTreeManager_SearchLowerBoundNode(self,
        BPlusTreeManager_LoadNode(self, node->Children[index]), key, keyLen, offsets, cap, count);
}

int BPlusTreeManager_SearchLowerBound(BPlusTreeManager* self, const uint8_t* key, int keyLen,
                                      int64_t* offsets, int cap, int* count)
{
    BPlusNode* root;
    int rc;
    if (!self || !key || !count) return GMB_DB_ERR;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootNodeOffset == -1) {
        *count = 0;
        pthread_mutex_unlock(&self->syncRoot);
        return GMB_DB_OK;
    }
    root = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    if (!root) {
        pthread_mutex_unlock(&self->syncRoot);
        return GMB_DB_ERR;
    }
    rc = BPlusTreeManager_SearchLowerBoundNode(self, root, key, keyLen, offsets, cap, count);
    pthread_mutex_unlock(&self->syncRoot);
    return rc;
}

static void bplus_selfcheck_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int BPlusTreeManager_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_bplus_selfcheck.isdb";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    BPlusTreeManager* mgr;
    uint8_t key[10];
    int i, n;
    int64_t off;
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema.Fields = fields;
    schema.FieldCount = 2;
    snprintf(schema.PrimaryKey.Name, sizeof(schema.PrimaryKey.Name), "%s", "Id");
    snprintf(schema.PrimaryKey.KeyExpression, sizeof(schema.PrimaryKey.KeyExpression), "%s", "Id");
    schema.PrimaryKey.KeyLength = 10;
    schema.PrimaryKey.IsUnique = 1;
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -2;
    mgr = BPlusTreeManager_New(path, 10, 1);
    if (!mgr) return -3;
    for (i = 0; i < 200; i++) {
        bplus_selfcheck_key(key, i);
        if (BPlusTreeManager_Insert(mgr, key, 10, 10000 + i) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -4;
        }
    }
    for (i = 0; i < 200; i++) {
        bplus_selfcheck_key(key, i);
        n = 0; off = -1;
        if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK || n != 1 || off != 10000 + i) {
            BPlusTreeManager_Dispose(mgr); return -100 - i;
        }
    }
    BPlusTreeManager_Dispose(mgr);
    mgr = BPlusTreeManager_New(path, 10, 1);
    if (!mgr || mgr->rootNodeOffset <= 0) { if (mgr) BPlusTreeManager_Dispose(mgr); return -5; }
    bplus_selfcheck_key(key, 150);
    n = 0; off = -1;
    if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK || off != 10150) {
        BPlusTreeManager_Dispose(mgr); return -6;
    }
    for (i = 0; i < 200; i += 2) {
        bplus_selfcheck_key(key, i);
        if (BPlusTreeManager_Delete(mgr, key, 10, 10000 + i) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -7;
        }
    }
    for (i = 0; i < 200; i++) {
        bplus_selfcheck_key(key, i);
        n = 0; off = -1;
        if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -200 - i;
        }
        if (i % 2 == 0) {
            if (n != 0) { BPlusTreeManager_Dispose(mgr); return -300 - i; }
        } else if (n != 1 || off != 10000 + i) {
            BPlusTreeManager_Dispose(mgr); return -400 - i;
        }
    }
    for (i = 1; i < 200; i += 2) {
        bplus_selfcheck_key(key, i);
        if (BPlusTreeManager_Delete(mgr, key, 10, 10000 + i) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -8;
        }
    }
    bplus_selfcheck_key(key, 99);
    n = 1; off = 0;
    if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK || n != 0) {
        BPlusTreeManager_Dispose(mgr); return -9;
    }
    for (i = 0; i < 20; i++) {
        bplus_selfcheck_key(key, i);
        if (BPlusTreeManager_Insert(mgr, key, 10, 20000 + i) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -10;
        }
    }
    BPlusTreeManager_Dispose(mgr);
    mgr = BPlusTreeManager_New(path, 10, 1);
    if (!mgr) return -11;
    bplus_selfcheck_key(key, 10);
    n = 0; off = -1;
    if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK || n != 1 || off != 20010) {
        BPlusTreeManager_Dispose(mgr); return -12;
    }
    BPlusTreeManager_Dispose(mgr);
    return GMB_DB_OK;
}

static int BPlusTreeManager_IsActiveRecord(BPlusTreeManager* self, int64_t recordOffset)
{
    uint8_t status = 0;
    if (recordOffset < 0 || !self->fs) return 0;
    if (self->handle) {
        RecordMetadata meta;
        if (IsamFileHandle_ReadRecordMetadata(self->handle, recordOffset, &meta) != GMB_DB_OK) return 0;
        return meta.Status == Config_RecordStatusActive;
    }
    fseek(self->fs, (long)recordOffset, SEEK_SET);
    if (fread(&status, 1, 1, self->fs) != 1) return 0;
    return status == Config_RecordStatusActive;
}

static BPlusNode* BPlusTreeManager_FindFirstLeaf(BPlusTreeManager* self)
{
    BPlusNode* node;
    if (self->rootNodeOffset < 0) return NULL;
    node = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    while (node && !node->IsLeaf) {
        if (node->ChildCount <= 0) return NULL;
        node = BPlusTreeManager_LoadNode(self, node->Children[0]);
    }
    return node;
}

static BPlusNode* BPlusTreeManager_FindLastLeaf(BPlusTreeManager* self)
{
    BPlusNode* node;
    if (self->rootNodeOffset < 0) return NULL;
    node = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    while (node && !node->IsLeaf) {
        if (node->ChildCount <= 0) return NULL;
        node = BPlusTreeManager_LoadNode(self, node->Children[node->ChildCount - 1]);
    }
    return node;
}

int64_t BPlusTreeManager_GetFirstRecordOffset(BPlusTreeManager* self)
{
    BPlusNode* leaf;
    int i, j;
    if (!self || self->rootNodeOffset < 0) return -1;
    pthread_mutex_lock(&self->syncRoot);
    leaf = BPlusTreeManager_FindFirstLeaf(self);
    while (leaf) {
        for (i = 0; i < leaf->KeyCount; i++) {
            for (j = 0; j < leaf->RecCounts[i]; j++) {
                int64_t off = leaf->Recs[i][j];
                if (BPlusTreeManager_IsActiveRecord(self, off)) {
                    pthread_mutex_unlock(&self->syncRoot);
                    return off;
                }
            }
        }
        if (leaf->NextLeafOffset < 0) break;
        leaf = BPlusTreeManager_LoadNode(self, leaf->NextLeafOffset);
    }
    pthread_mutex_unlock(&self->syncRoot);
    return -1;
}

int64_t BPlusTreeManager_GetLastRecordOffset(BPlusTreeManager* self)
{
    BPlusNode* leaf;
    int i, j;
    if (!self || self->rootNodeOffset < 0) return -1;
    pthread_mutex_lock(&self->syncRoot);
    leaf = BPlusTreeManager_FindLastLeaf(self);
    while (leaf) {
        for (i = leaf->KeyCount - 1; i >= 0; i--) {
            for (j = leaf->RecCounts[i] - 1; j >= 0; j--) {
                int64_t off = leaf->Recs[i][j];
                if (BPlusTreeManager_IsActiveRecord(self, off)) {
                    pthread_mutex_unlock(&self->syncRoot);
                    return off;
                }
            }
        }
        if (leaf->PrevLeafOffset < 0) break;
        leaf = BPlusTreeManager_LoadNode(self, leaf->PrevLeafOffset);
    }
    pthread_mutex_unlock(&self->syncRoot);
    return -1;
}

static int bplus_find_offset_in_leaf(const BPlusNode* leaf, int64_t recOff, int* keyIndex, int* offIndex)
{
    int i, j;
    for (i = 0; i < leaf->KeyCount; i++) {
        for (j = 0; j < leaf->RecCounts[i]; j++) {
            if (leaf->Recs[i][j] == recOff) {
                *keyIndex = i;
                *offIndex = j;
                return 1;
            }
        }
    }
    return 0;
}

static BPlusNode* bplus_locate_record(BPlusTreeManager* self, int64_t recOff, int* keyIndex, int* offIndex)
{
    /* ponytail: C# FindRecordPosition ricalcola la chiave via evaluator; scan foglie per offset. */
    BPlusNode* leaf = BPlusTreeManager_FindFirstLeaf(self);
    while (leaf) {
        if (bplus_find_offset_in_leaf(leaf, recOff, keyIndex, offIndex)) return leaf;
        if (leaf->NextLeafOffset < 0) return NULL;
        leaf = BPlusTreeManager_LoadNode(self, leaf->NextLeafOffset);
    }
    return NULL;
}

static int64_t bplus_step_from(BPlusTreeManager* self, BPlusNode* leaf, int keyIndex, int offIndex, int direction)
{
    if (direction > 0) {
        offIndex++;
        if (offIndex >= leaf->RecCounts[keyIndex]) {
            keyIndex++;
            offIndex = 0;
            if (keyIndex >= leaf->KeyCount) {
                if (leaf->NextLeafOffset < 0) return -1;
                leaf = BPlusTreeManager_LoadNode(self, leaf->NextLeafOffset);
                if (!leaf || leaf->KeyCount <= 0) return -1;
                keyIndex = 0;
            }
        }
    } else {
        offIndex--;
        if (offIndex < 0) {
            keyIndex--;
            if (keyIndex < 0) {
                if (leaf->PrevLeafOffset < 0) return -1;
                leaf = BPlusTreeManager_LoadNode(self, leaf->PrevLeafOffset);
                if (!leaf || leaf->KeyCount <= 0) return -1;
                keyIndex = leaf->KeyCount - 1;
            }
            offIndex = leaf->RecCounts[keyIndex] - 1;
        }
    }
    if (keyIndex < 0 || keyIndex >= leaf->KeyCount) return -1;
    if (offIndex < 0 || offIndex >= leaf->RecCounts[keyIndex]) return -1;
    return leaf->Recs[keyIndex][offIndex];
}

int64_t BPlusTreeManager_FindNextRecordOffset(BPlusTreeManager* self, int64_t currentRecordOffset, int skipCount)
{
    BPlusNode* leaf;
    int keyIndex = 0, offIndex = 0, i, dir, guard;
    int64_t candidate;
    if (!self || skipCount == 0) return currentRecordOffset;
    pthread_mutex_lock(&self->syncRoot);
    leaf = bplus_locate_record(self, currentRecordOffset, &keyIndex, &offIndex);
    if (!leaf) { pthread_mutex_unlock(&self->syncRoot); return -1; }
    dir = skipCount > 0 ? 1 : -1;
    candidate = currentRecordOffset;
    for (i = 0; i < (skipCount < 0 ? -skipCount : skipCount); i++) {
        candidate = bplus_step_from(self, leaf, keyIndex, offIndex, dir);
        if (candidate < 0) { pthread_mutex_unlock(&self->syncRoot); return -1; }
        leaf = bplus_locate_record(self, candidate, &keyIndex, &offIndex);
        if (!leaf) { pthread_mutex_unlock(&self->syncRoot); return -1; }
    }
    guard = 0;
    while (candidate >= 0 && !BPlusTreeManager_IsActiveRecord(self, candidate) && guard++ < 1000000) {
        int64_t advanced;
        leaf = bplus_locate_record(self, candidate, &keyIndex, &offIndex);
        if (!leaf) { pthread_mutex_unlock(&self->syncRoot); return -1; }
        advanced = bplus_step_from(self, leaf, keyIndex, offIndex, dir);
        if (advanced < 0 || advanced == candidate) { pthread_mutex_unlock(&self->syncRoot); return advanced; }
        candidate = advanced;
    }
    pthread_mutex_unlock(&self->syncRoot);
    return candidate;
}

static int bplus_min_keys(const BPlusTreeManager* self)
{
    return BPlusFillFactor_ComputeMinKeys(BPlusTreeManager_GetMaxKeysPerNode(self));
}

static int BPlusInternal_FindChildIndex(const BPlusNode* n, const uint8_t* key, int keyLen)
{
    int found, index = BPlusNode_FindKey(n, key, keyLen, &found);
    if (found) index += 1;
    if (n->ChildCount <= 0) return 0;
    if (index >= n->ChildCount) index = n->ChildCount - 1;
    if (index < 0) index = 0;
    return index;
}

static int BPlusLeaf_Remove(BPlusNode* leaf, const uint8_t* key, int keyLen, int64_t recordOffset)
{
    int found, index, j, k;
    index = BPlusNode_FindKey(leaf, key, keyLen, &found);
    if (!found) return 0;
    for (j = 0; j < leaf->RecCounts[index]; j++) {
        if (leaf->Recs[index][j] != recordOffset) continue;
        for (k = j; k < leaf->RecCounts[index] - 1; k++)
            leaf->Recs[index][k] = leaf->Recs[index][k + 1];
        leaf->RecCounts[index]--;
        if (leaf->RecCounts[index] == 0) {
            free(leaf->Keys[index]);
            free(leaf->Recs[index]);
            for (k = index; k < leaf->KeyCount - 1; k++) {
                leaf->Keys[k] = leaf->Keys[k + 1];
                leaf->KeyLens[k] = leaf->KeyLens[k + 1];
                leaf->Recs[k] = leaf->Recs[k + 1];
                leaf->RecCounts[k] = leaf->RecCounts[k + 1];
            }
            leaf->KeyCount--;
        }
        return 1;
    }
    return 0;
}

static void BPlusTreeManager_AddPageToFreeList(BPlusTreeManager* self, int64_t pageOffset)
{
    IsamFileHeader h;
    uint8_t page[BPlusPage_Size];
    int i;
    if (pageOffset < 0) return;
    if (BPlusTreeManager_ReadHeader(self, &h) != GMB_DB_OK) return;
    memset(page, 0, sizeof(page));
    for (i = 0; i < 8; i++)
        page[i] = (uint8_t)((uint64_t)h.PageFreeListHeadOffset >> (8 * i));
    page[8] = 0xFE;
    fseek(self->fs, (long)pageOffset, SEEK_SET);
    fwrite(page, 1, BPlusPage_Size, self->fs);
    h.PageFreeListHeadOffset = pageOffset;
    BPlusTreeManager_WriteHeader(self, &h);
    fflush(self->fs);
    IsamLruCache_Remove(self->nodeCache, (uint64_t)pageOffset);
}

static void BPlusInternal_RemoveKeyAt(BPlusNode* n, int i)
{
    int k;
    if (i < 0 || i >= n->KeyCount) return;
    for (k = i; k < n->KeyCount - 1; k++) {
        n->Keys[k] = n->Keys[k + 1];
        n->KeyLens[k] = n->KeyLens[k + 1];
    }
    n->KeyCount--;
}

static void BPlusInternal_RemoveChildAt(BPlusNode* n, int i)
{
    int k;
    if (i < 0 || i >= n->ChildCount) return;
    for (k = i; k < n->ChildCount - 1; k++)
        n->Children[k] = n->Children[k + 1];
    n->ChildCount--;
}

static void BPlusLeaf_MoveFirstToLeft(BPlusNode* left, BPlusNode* right)
{
    int i = left->KeyCount;
    BPlusNode_EnsureKeys(left, i + 1);
    left->Keys[i] = right->Keys[0];
    left->KeyLens[i] = right->KeyLens[0];
    left->Recs[i] = right->Recs[0];
    left->RecCounts[i] = right->RecCounts[0];
    left->KeyCount++;
    for (i = 0; i < right->KeyCount - 1; i++) {
        right->Keys[i] = right->Keys[i + 1];
        right->KeyLens[i] = right->KeyLens[i + 1];
        right->Recs[i] = right->Recs[i + 1];
        right->RecCounts[i] = right->RecCounts[i + 1];
    }
    right->KeyCount--;
}

static void BPlusLeaf_MoveLastToRight(BPlusNode* left, BPlusNode* right)
{
    int last = left->KeyCount - 1, i;
    BPlusNode_EnsureKeys(right, right->KeyCount + 1);
    for (i = right->KeyCount; i > 0; i--) {
        right->Keys[i] = right->Keys[i - 1];
        right->KeyLens[i] = right->KeyLens[i - 1];
        right->Recs[i] = right->Recs[i - 1];
        right->RecCounts[i] = right->RecCounts[i - 1];
    }
    right->Keys[0] = left->Keys[last];
    right->KeyLens[0] = left->KeyLens[last];
    right->Recs[0] = left->Recs[last];
    right->RecCounts[0] = left->RecCounts[last];
    right->KeyCount++;
    left->KeyCount--;
}

static void BPlusInternal_MoveFirstChildToLeft(BPlusNode* left, BPlusNode* right, BPlusNode* parent, int sep)
{
    BPlusNode_EnsureKeys(left, left->KeyCount + 1);
    BPlusNode_EnsureChildren(left, left->ChildCount + 1);
    left->Keys[left->KeyCount] = parent->Keys[sep];
    left->KeyLens[left->KeyCount] = parent->KeyLens[sep];
    left->KeyCount++;
    left->Children[left->ChildCount++] = right->Children[0];
    BPlusInternal_RemoveChildAt(right, 0);
    parent->Keys[sep] = right->Keys[0];
    parent->KeyLens[sep] = right->KeyLens[0];
    BPlusInternal_RemoveKeyAt(right, 0);
}

static void BPlusInternal_MoveLastChildToRight(BPlusNode* left, BPlusNode* right, BPlusNode* parent, int sep)
{
    int lastKey = left->KeyCount - 1, lastChild = left->ChildCount - 1, i;
    BPlusNode_EnsureKeys(right, right->KeyCount + 1);
    BPlusNode_EnsureChildren(right, right->ChildCount + 1);
    for (i = right->KeyCount; i > 0; i--) {
        right->Keys[i] = right->Keys[i - 1];
        right->KeyLens[i] = right->KeyLens[i - 1];
    }
    for (i = right->ChildCount; i > 0; i--)
        right->Children[i] = right->Children[i - 1];
    right->Keys[0] = parent->Keys[sep];
    right->KeyLens[0] = parent->KeyLens[sep];
    right->KeyCount++;
    right->Children[0] = left->Children[lastChild];
    right->ChildCount++;
    parent->Keys[sep] = left->Keys[lastKey];
    parent->KeyLens[sep] = left->KeyLens[lastKey];
    left->KeyCount--;
    left->ChildCount--;
}

static void BPlusTreeManager_ReplaceSeparator(BPlusNode* parent, int sep, const BPlusNode* right)
{
    free(parent->Keys[sep]);
    parent->Keys[sep] = bplus_dup_key(right->Keys[0], right->KeyLens[0]);
    parent->KeyLens[sep] = right->KeyLens[0];
}

static void BPlusTreeManager_Redistribute(BPlusTreeManager* self, BPlusNode* parent, int leftIndex,
                                         BPlusNode* leftChild, BPlusNode* rightChild)
{
    if (leftChild->KeyCount < bplus_min_keys(self)) {
        if (leftChild->IsLeaf) {
            BPlusLeaf_MoveFirstToLeft(leftChild, rightChild);
            BPlusTreeManager_ReplaceSeparator(parent, leftIndex, rightChild);
        } else {
            BPlusInternal_MoveFirstChildToLeft(leftChild, rightChild, parent, leftIndex);
        }
    } else if (leftChild->IsLeaf) {
        BPlusLeaf_MoveLastToRight(leftChild, rightChild);
        BPlusTreeManager_ReplaceSeparator(parent, leftIndex, rightChild);
    } else {
        BPlusInternal_MoveLastChildToRight(leftChild, rightChild, parent, leftIndex);
    }
    BPlusTreeManager_SaveNode(self, parent);
    BPlusTreeManager_SaveNode(self, leftChild);
    BPlusTreeManager_SaveNode(self, rightChild);
}

static void BPlusTreeManager_Merge(BPlusTreeManager* self, BPlusNode* parent, int leftIndex)
{
    BPlusNode* leftChild = BPlusTreeManager_LoadNode(self, parent->Children[leftIndex]);
    BPlusNode* rightChild;
    int i;
    if (leftIndex < 0 || leftIndex + 1 >= parent->ChildCount) return;
    rightChild = BPlusTreeManager_LoadNode(self, parent->Children[leftIndex + 1]);
    if (!leftChild || !rightChild) return;
    if (leftChild->IsLeaf) {
        BPlusNode_EnsureKeys(leftChild, leftChild->KeyCount + rightChild->KeyCount);
        for (i = 0; i < rightChild->KeyCount; i++) {
            int d = leftChild->KeyCount + i;
            leftChild->Keys[d] = rightChild->Keys[i];
            leftChild->KeyLens[d] = rightChild->KeyLens[i];
            leftChild->Recs[d] = rightChild->Recs[i];
            leftChild->RecCounts[d] = rightChild->RecCounts[i];
        }
        leftChild->KeyCount += rightChild->KeyCount;
        rightChild->KeyCount = 0;
        leftChild->NextLeafOffset = rightChild->NextLeafOffset;
        if (rightChild->NextLeafOffset != -1) {
            BPlusNode* nxt = BPlusTreeManager_LoadNode(self, rightChild->NextLeafOffset);
            if (nxt && nxt->IsLeaf) {
                nxt->PrevLeafOffset = leftChild->NodeOffset;
                BPlusTreeManager_SaveNode(self, nxt);
            }
        }
    } else {
        BPlusNode_EnsureKeys(leftChild, leftChild->KeyCount + 1 + rightChild->KeyCount);
        BPlusNode_EnsureChildren(leftChild, leftChild->ChildCount + rightChild->ChildCount);
        leftChild->Keys[leftChild->KeyCount] = parent->Keys[leftIndex];
        leftChild->KeyLens[leftChild->KeyCount] = parent->KeyLens[leftIndex];
        leftChild->KeyCount++;
        for (i = 0; i < rightChild->KeyCount; i++) {
            leftChild->Keys[leftChild->KeyCount] = rightChild->Keys[i];
            leftChild->KeyLens[leftChild->KeyCount] = rightChild->KeyLens[i];
            leftChild->KeyCount++;
        }
        for (i = 0; i < rightChild->ChildCount; i++)
            leftChild->Children[leftChild->ChildCount++] = rightChild->Children[i];
        rightChild->KeyCount = 0;
        rightChild->ChildCount = 0;
    }
    if (leftChild->IsLeaf)
        free(parent->Keys[leftIndex]);
    BPlusInternal_RemoveKeyAt(parent, leftIndex);
    BPlusInternal_RemoveChildAt(parent, leftIndex + 1);
    BPlusTreeManager_SaveNode(self, leftChild);
    BPlusTreeManager_SaveNode(self, parent);
    BPlusTreeManager_AddPageToFreeList(self, rightChild->NodeOffset);
}

static void BPlusTreeManager_HandleUnderflow(BPlusTreeManager* self, BPlusNode* parent, int childIndex)
{
    int minKeys = bplus_min_keys(self);
    if (childIndex > 0) {
        BPlusNode* left = BPlusTreeManager_LoadNode(self, parent->Children[childIndex - 1]);
        if (left && left->KeyCount > minKeys) {
            BPlusTreeManager_Redistribute(self, parent, childIndex - 1, left,
                BPlusTreeManager_LoadNode(self, parent->Children[childIndex]));
            return;
        }
    }
    if (childIndex < parent->KeyCount) {
        BPlusNode* right = BPlusTreeManager_LoadNode(self, parent->Children[childIndex + 1]);
        if (right && right->KeyCount > minKeys) {
            BPlusTreeManager_Redistribute(self, parent, childIndex,
                BPlusTreeManager_LoadNode(self, parent->Children[childIndex]), right);
            return;
        }
    }
    if (childIndex > 0)
        BPlusTreeManager_Merge(self, parent, childIndex - 1);
    else
        BPlusTreeManager_Merge(self, parent, childIndex);
}

static void BPlusTreeManager_DeleteRecursive(BPlusTreeManager* self, int64_t pageOffset,
                                            const uint8_t* key, int keyLen, int64_t recordOffset)
{
    BPlusNode* page = BPlusTreeManager_LoadNode(self, pageOffset);
    int childIndex;
    int64_t childOffset;
    BPlusNode* child;
    if (!page) return;
    if (page->IsLeaf) {
        if (BPlusLeaf_Remove(page, key, keyLen, recordOffset))
            BPlusTreeManager_SaveNode(self, page);
        return;
    }
    childIndex = BPlusInternal_FindChildIndex(page, key, keyLen);
    childOffset = page->Children[childIndex];
    BPlusTreeManager_DeleteRecursive(self, childOffset, key, keyLen, recordOffset);
    child = BPlusTreeManager_LoadNode(self, childOffset);
    if (child && child->KeyCount < bplus_min_keys(self))
        BPlusTreeManager_HandleUnderflow(self, page, childIndex);
}

int BPlusTreeManager_Delete(BPlusTreeManager* self, const uint8_t* key, int keyLen, int64_t recordOffset)
{
    BPlusNode* rootPage;
    if (!self || !key) return GMB_DB_ERR;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootNodeOffset == -1) {
        pthread_mutex_unlock(&self->syncRoot);
        return GMB_DB_OK;
    }
    BPlusTreeManager_DeleteRecursive(self, self->rootNodeOffset, key, keyLen, recordOffset);
    rootPage = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    if (rootPage && !rootPage->IsLeaf && rootPage->KeyCount == 0 && rootPage->ChildCount > 0) {
        int64_t oldRoot = self->rootNodeOffset;
        self->rootNodeOffset = rootPage->Children[0];
        BPlusTreeManager_UpdateRootOffsetInHeader(self);
        BPlusTreeManager_AddPageToFreeList(self, oldRoot);
    }
    pthread_mutex_unlock(&self->syncRoot);
    return GMB_DB_OK;
}

/* leftover inventory stubs (nomi void, non collidono con l'API sopra) */
int BPlusTreeManager_BPlusTreeManager_New(void) { return GMB_DB_ERR; }
int BPlusTreeManager_InsertUnderTreeLock(void) { return GMB_DB_ERR; }
int BPlusTreeManager_InsertManyAppendOrdered(void) { return GMB_DB_ERR; }
int BPlusTreeManager_PromoteLeafSplitAsRoot(void) { return GMB_DB_ERR; }
int BPlusTreeManager_PromoteLeafSplit(void) { return GMB_DB_ERR; }
int BPlusTreeManager_InsertPromotedKeyIntoInternal(void) { return GMB_DB_ERR; }
int BPlusTreeManager_FindParentInternalNode(void) { return GMB_DB_ERR; }
int BPlusTreeManager_GetIndexDefinition(void) { return GMB_DB_ERR; }
int BPlusTreeManager_ResetRoot(void) { return GMB_DB_ERR; }
int BPlusTreeManager_LoadLeafNode(void) { return GMB_DB_ERR; }
int BPlusTreeManager_QueueLeafReadAhead(void) { return GMB_DB_ERR; }
int BPlusTreeManager_GetReadAheadLeaf(void) { return GMB_DB_ERR; }
int BPlusTreeManager_ClearReadAheadLeaves(void) { return GMB_DB_ERR; }
int BPlusTreeManager_ConsumeReadAheadSlot(void) { return GMB_DB_ERR; }
int BPlusTreeManager_TryConsumeReadAheadLeaf(void) { return GMB_DB_ERR; }
int BPlusTreeManager_IsReadAheadQueued(void) { return GMB_DB_ERR; }
int BPlusTreeManager_ZeroPage(void) { return GMB_DB_ERR; }
int BPlusTreeManager_ValidatePageChecksums(void) { return GMB_DB_ERR; }
int BPlusTreeManager_ValidatePageChecksumsRecursive(void) { return GMB_DB_ERR; }
int BPlusTreeManager_ReadFullPageBytes(void) { return GMB_DB_ERR; }
int BPlusTreeManager_MeasureAverageLeafFillFactor(void) { return GMB_DB_ERR; }

static void bplus_add_tree_pages(BPlusTreeManager* self, int64_t pageOffset)
{
    BPlusNode* node;
    int i;
    if (pageOffset < 0) return;
    node = BPlusTreeManager_LoadNode(self, pageOffset);
    if (!node) return;
    if (!node->IsLeaf) {
        for (i = 0; i < node->ChildCount; i++)
            bplus_add_tree_pages(self, node->Children[i]);
    }
    BPlusTreeManager_AddPageToFreeList(self, pageOffset);
}

void BPlusTreeManager_AddTreeToFreeList(BPlusTreeManager* self)
{
    if (!self || self->rootNodeOffset < 0) return;
    bplus_add_tree_pages(self, self->rootNodeOffset);
    self->rootNodeOffset = -1;
    IsamLruCache_Clear(self->nodeCache);
}

void BPlusTreeManager_ReloadRootFromHeader(BPlusTreeManager* self)
{
    int64_t r;
    if (!self || !self->handle) return;
    if (self->secondarySlot >= 0 && self->secondarySlot < 15)
        r = self->handle->Header.SecondaryIndexRootOffsets[self->secondarySlot];
    else
        r = self->handle->Header.PrimaryKeyRootOffset;
    self->rootNodeOffset = (r == 0) ? -1 : r;
}

int BPlusTreeManager_RepairPointersAfterHeapMove(BPlusTreeManager* self,
                                                 int64_t heapStart, int64_t heapEnd, int64_t delta)
{
    int64_t queue[1024];
    int64_t seen[1024];
    int qh = 0, qt = 0, nseen = 0;
    if (!self) return GMB_DB_ERR;
    IsamLruCache_Clear(self->nodeCache);
    if (self->rootNodeOffset < 0) return GMB_DB_OK;
    queue[qt++] = self->rootNodeOffset;
    while (qh < qt) {
        int64_t off = queue[qh++];
        BPlusNode* node;
        int s, dup = 0, i;
        for (s = 0; s < nseen; s++) {
            if (seen[s] == off) { dup = 1; break; }
        }
        if (dup) continue;
        if (nseen < 1024) seen[nseen++] = off;
        node = BPlusTreeManager_LoadNode(self, off);
        if (!node) continue;
        node->NodeOffset = off;
        if (!node->IsLeaf) {
            for (i = 0; i < node->ChildCount; i++) {
                if (node->Children[i] >= heapStart && node->Children[i] < heapEnd)
                    node->Children[i] += delta;
                if (node->Children[i] >= heapStart && qt < 1024)
                    queue[qt++] = node->Children[i];
            }
        } else {
            if (node->NextLeafOffset >= heapStart && node->NextLeafOffset < heapEnd)
                node->NextLeafOffset += delta;
            if (node->PrevLeafOffset >= heapStart && node->PrevLeafOffset < heapEnd)
                node->PrevLeafOffset += delta;
            for (i = 0; i < node->KeyCount; i++) {
                int r;
                for (r = 0; r < node->RecCounts[i]; r++) {
                    if (node->Recs[i][r] >= heapStart && node->Recs[i][r] < heapEnd)
                        node->Recs[i][r] += delta;
                }
            }
            if (node->NextLeafOffset >= heapStart && qt < 1024)
                queue[qt++] = node->NextLeafOffset;
        }
        BPlusTreeManager_SaveNode(self, node);
    }
    return GMB_DB_OK;
}

int BPlusTreeManager_ValidateIndex(void) { return GMB_DB_ERR; }
int BPlusTreeManager_ValidatePageRecursive(void) { return GMB_DB_ERR; }
int BPlusTreeManager_CheckLeafListConsistency(void) { return GMB_DB_ERR; }
int BPlusTreeManager_GetMinKeysPerNode(void) { return GMB_DB_ERR; }
int BPlusTreeManager_GetMinKeysForInternalNode(void) { return GMB_DB_ERR; }
int BPlusTreeManager_FindPreviousLeaf(void) { return GMB_DB_ERR; }
int BPlusTreeManager_IsValidRecordOffset(void) { return GMB_DB_ERR; }
int BPlusTreeManager_ReadNodeBytes(void) { return GMB_DB_ERR; }
