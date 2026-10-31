#include <unistd.h>

#define COMPACT_MAX_FILES 64

typedef struct {
    uint8_t* key;
    int keyLen;
    uint8_t* val;
    int valLen;
    int tomb;
    int source;
} CompactEnt;

static int compact_cmp(const void* a, const void* b)
{
    const CompactEnt* x = (const CompactEnt*)a;
    const CompactEnt* y = (const CompactEnt*)b;
    int c = ByteArrayComparer_Compare(x->key, x->keyLen, y->key, y->keyLen);
    if (c != 0) return c;
    return x->source - y->source;
}

static int compact_name_desc(const void* a, const void* b)
{
    return strcmp((const char*)b, (const char*)a);
}

static int compact_push(CompactEnt** ents, int* n, int* cap, const uint8_t* key, int keyLen,
                        const uint8_t* val, int valLen, int tomb, int source)
{
    CompactEnt* e;
    if (*n >= *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *ents = (CompactEnt*)realloc(*ents, (size_t)(*cap) * sizeof(CompactEnt));
    }
    e = &(*ents)[(*n)++];
    e->keyLen = keyLen;
    e->valLen = valLen;
    e->tomb = tomb;
    e->source = source;
    e->key = (uint8_t*)malloc((size_t)keyLen);
    if (keyLen > 0) memcpy(e->key, key, (size_t)keyLen);
    e->val = NULL;
    if (!tomb && val && valLen > 0) {
        e->val = (uint8_t*)malloc((size_t)valLen);
        memcpy(e->val, val, (size_t)valLen);
    }
    return 0;
}

static void compact_free_ents(CompactEnt* ents, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        free(ents[i].key);
        free(ents[i].val);
    }
    free(ents);
}

static void compact_delete_ssts(Database* db, char files[][256], int nfiles)
{
    const char* root = Database_GetPath(db);
    int i;
    for (i = 0; i < nfiles; i++) {
        char path[1100];
        snprintf(path, sizeof(path), "%s/%s/%s", root, Config_SstDirectoryName, files[i]);
        Database_SstCacheRemove(db, path);
        unlink(path);
    }
}

/* iterators newest-first: source 0 wins. Tomb first hides older live (C# KWayMerge). */
static int compact_merge_write(Database* db, int tableId, char files[][256], int nfiles, char newName[256])
{
    CompactEnt* ents = NULL;
    int n = 0, cap = 0, i, src = 0;
    Memtable* out;
    uint64_t fileNumber;
    char sstPath[1100];
    const char* root;
    Manifest* man;

    if (nfiles < 2) return 0;
    root = Database_GetPath(db);
    man = Database_GetManifest(db);
    for (i = 0; i < nfiles; i++, src++) {
        char path[1100];
        SstReader* r;
        snprintf(path, sizeof(path), "%s/%s/%s", root, Config_SstDirectoryName, files[i]);
        r = Database_SstGetOrAdd(db, path);
        if (!r) { compact_free_ents(ents, n); return GMB_DB_ERR; }
        SstReader_Rewind(r);
        for (;;) {
            uint8_t *k = NULL, *v = NULL;
            int kl = 0, vl = 0;
            if (!SstReader_Next(r, &k, &kl, &v, &vl)) break;
            compact_push(&ents, &n, &cap, k, kl, v, vl, v == NULL, src);
            free(k);
            free(v);
        }
    }
    if (n > 1) qsort(ents, (size_t)n, sizeof(CompactEnt), compact_cmp);
    out = Memtable_New();
    for (i = 0; i < n; ) {
        int j = i + 1;
        while (j < n && ByteArrayComparer_Compare(ents[i].key, ents[i].keyLen, ents[j].key, ents[j].keyLen) == 0)
            j++;
        if (!ents[i].tomb)
            Memtable_Set(out, ents[i].key, ents[i].keyLen, ents[i].val, ents[i].valLen);
        i = j;
    }
    compact_free_ents(ents, n);
    fileNumber = Manifest_GetNextFileNumber(man);
    snprintf(sstPath, sizeof(sstPath), "%s/%s/%06llu_%d.sst",
             root, Config_SstDirectoryName, (unsigned long long)fileNumber, tableId);
    if (SstWriter_WriteFromMemtable(sstPath, out) != GMB_DB_OK) {
        Memtable_Dispose(out);
        return GMB_DB_ERR;
    }
    Memtable_Dispose(out);
    {
        const char* base = strrchr(sstPath, '/');
        snprintf(newName, 256, "%s", base ? base + 1 : sstPath);
    }
    return 1;
}

int Compactor_CompactLevel0(Database* db, int tableId)
{
    char files[COMPACT_MAX_FILES][256];
    char newName[256];
    int n, rc;
    Manifest* man;
    if (!db) return 0;
    man = Database_GetManifest(db);
    n = Manifest_GetFilesForLevel(man, tableId, 0, files, COMPACT_MAX_FILES);
    if (n < Database_CompactLevel0Threshold) return 0;
    qsort(files, (size_t)n, 256, compact_name_desc);
    rc = compact_merge_write(db, tableId, files, n, newName);
    if (rc <= 0) return rc;
    Manifest_UpdateAfterCompaction(man, tableId, 0, files, n, 1, newName);
    Manifest_Save(man);
    compact_delete_ssts(db, files, n);
    return n - 1;
}

int Compactor_CompactLevel(Database* db, int tableId, int level)
{
    char files[COMPACT_MAX_FILES][256];
    char newName[256];
    int n, rc;
    Manifest* man;
    if (!db || level <= 0) return 0;
    man = Database_GetManifest(db);
    n = Manifest_GetFilesForLevel(man, tableId, level, files, COMPACT_MAX_FILES);
    if (n < Compactor_CompactLevelThreshold) return 0;
    rc = compact_merge_write(db, tableId, files, n, newName);
    if (rc <= 0) return rc;
    Manifest_UpdateAfterCompaction(man, tableId, level, files, n, level + 1, newName);
    Manifest_Save(man);
    compact_delete_ssts(db, files, n);
    return n - 1;
}

int Compactor_CompactLevels01Forced(Database* db, int tableId)
{
    char (*files)[256] = NULL;
    char tmp[COMPACT_MAX_FILES][256];
    char newName[256];
    int n = 0, cap = 0, lv, i, k, maxLv = 0, dest, rc;
    Manifest* man;
    if (!db) return 0;
    man = Database_GetManifest(db);
    for (lv = 0; lv < 16; lv++) {
        k = Manifest_GetFilesForLevel(man, tableId, lv, tmp, COMPACT_MAX_FILES);
        if (k > 0) maxLv = lv;
        if (k > 1) qsort(tmp, (size_t)k, 256, compact_name_desc);
        for (i = 0; i < k; i++) {
            if (n >= cap) {
                int ncap = cap ? cap * 2 : 16;
                char (*grown)[256] = (char (*)[256])realloc(files, (size_t)ncap * 256);
                if (!grown) {
                    free(files);
                    return GMB_DB_ERR;
                }
                files = grown;
                cap = ncap;
            }
            snprintf(files[n++], 256, "%s", tmp[i]);
        }
    }
    if (n < 2) {
        free(files);
        return 0;
    }
    rc = compact_merge_write(db, tableId, files, n, newName);
    if (rc <= 0) {
        free(files);
        return rc;
    }
    dest = maxLv < 1 ? 1 : maxLv;
    Manifest_ReplaceAllSstAfterMajorCompact(man, tableId, dest, newName);
    Manifest_Save(man);
    compact_delete_ssts(db, files, n);
    free(files);
    return n - 1;
}
