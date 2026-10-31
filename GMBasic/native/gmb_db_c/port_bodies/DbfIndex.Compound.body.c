#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#define GMB_MDX_MAX_ENTRIES 4096
#define GMB_MDX_MAX_PAGES 512
#define GMB_MDX_MAX_DEPTH 16

static uint16_t mdx_ru16(const uint8_t* p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t mdx_ru32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void mdx_le16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void mdx_le32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int mdx_keycmp(const uint8_t* a, int alen, const uint8_t* b, int blen)
{
    int n = alen < blen ? alen : blen;
    int c = n > 0 ? memcmp(a, b, (size_t)n) : 0;
    if (c != 0) return c;
    return alen - blen;
}

static int mdx_align_up(int value, int alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

static void mdx_stem(const char* path, char* out, size_t cap)
{
    const char* slash;
    const char* base;
    const char* dot;
    size_t n;
    if (!path || !out || cap == 0) return;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    n = dot ? (size_t)(dot - base) : strlen(base);
    if (n >= cap) n = cap - 1;
    memcpy(out, base, n);
    out[n] = 0;
}

static void mdx_normalize_tag(const char* tagName, char* out, size_t cap)
{
    size_t i, n;
    if (!tagName || !out || cap == 0) return;
    while (*tagName == ' ' || *tagName == '\t') tagName++;
    n = strlen(tagName);
    while (n > 0 && (tagName[n - 1] == ' ' || tagName[n - 1] == '\t')) n--;
    if (n > (size_t)MdxFormat_TagNameLength) n = (size_t)MdxFormat_TagNameLength;
    if (n >= cap) n = cap - 1;
    for (i = 0; i < n; i++)
        out[i] = (char)toupper((unsigned char)tagName[i]);
    out[n] = 0;
}

static void mdx_read_cstring(const uint8_t* span, int spanLen, char* out, int cap)
{
    int len = spanLen, i;
    for (i = 0; i < spanLen; i++) {
        if (span[i] == 0) {
            len = i;
            break;
        }
    }
    while (len > 0 && span[len - 1] == ' ') len--;
    if (len >= cap) len = cap - 1;
    if (len > 0) memcpy(out, span, (size_t)len);
    out[len] = 0;
}

static void mdx_write_tag_name(uint8_t* dest, int destLen, const char* name)
{
    size_t n;
    memset(dest, 0, (size_t)destLen);
    if (!name) return;
    n = strlen(name);
    if (n > (size_t)destLen) n = (size_t)destLen;
    memcpy(dest, name, n);
}

typedef struct {
    uint8_t* Key;
    int KeyLen;
    int Recno;
} MdxEntry;

static int mdx_find_key(MdxEntry* entries, int n, const uint8_t* keyBytes, int keyLen, int* exactMatch)
{
    int lo = 0, hi = n - 1, insertAt = n;
    *exactMatch = 0;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = mdx_keycmp(keyBytes, keyLen, entries[mid].Key, entries[mid].KeyLen);
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

static int mdx_read_page(FILE* fs, uint32_t blockNumber, uint8_t* buffer)
{
    if (fseek(fs, (long)blockNumber * NdxFormat_BlockSize, SEEK_SET) != 0) return -1;
    if (fread(buffer, 1, NdxFormat_BlockSize, fs) != NdxFormat_BlockSize) return -1;
    return 0;
}

static void mdx_enumerate(
    FILE* fs,
    uint32_t blockNumber,
    int keyLength,
    int groupLen,
    MdxEntry* result,
    int* nResult,
    int depth)
{
    uint8_t page[NdxFormat_BlockSize];
    uint16_t itemCount;
    uint32_t child0;
    int i;
    if (depth > GMB_MDX_MAX_DEPTH || *nResult >= GMB_MDX_MAX_ENTRIES) return;
    if (mdx_read_page(fs, blockNumber, page) != 0) return;
    itemCount = mdx_ru16(page);
    if (itemCount == 0) return;
    child0 = mdx_ru32(page + 4);
    if (child0 == 0) {
        for (i = 0; i < (int)itemCount && *nResult < GMB_MDX_MAX_ENTRIES; i++) {
            int offset = 4 + (i * groupLen);
            MdxEntry* e = &result[*nResult];
            e->KeyLen = keyLength;
            e->Key = (uint8_t*)malloc((size_t)keyLength);
            if (!e->Key) return;
            memcpy(e->Key, page + offset + 8, (size_t)keyLength);
            e->Recno = (int)mdx_ru32(page + offset + 4);
            (*nResult)++;
        }
        return;
    }
    for (i = 0; i <= (int)itemCount; i++) {
        uint32_t child = mdx_ru32(page + 4 + (i * groupLen));
        if (child != 0)
            mdx_enumerate(fs, child, keyLength, groupLen, result, nResult, depth + 1);
    }
}

int MdxCompound_ReadTagTable(const char* path, MdxTagDescriptor* out, int cap, int* nOut)
{
    FILE* fs;
    uint8_t header[MdxFormat_HeaderSize];
    uint8_t table[GMB_MDX_MAX_TAGS * MdxFormat_TagEntrySize];
    uint8_t tagHeader[NdxFormat_BlockSize];
    long flen;
    int tagCount, i, n = 0;
    if (nOut) *nOut = 0;
    if (!path || !out || cap <= 0) return GMB_DB_ARG;
    fs = fopen(path, "rb");
    if (!fs) return GMB_DB_ERR;
    if (fseek(fs, 0, SEEK_END) != 0) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    flen = ftell(fs);
    if (flen < MdxFormat_TagTableOffset + MdxFormat_TagEntrySize) {
        fclose(fs);
        return GMB_DB_OK;
    }
    if (fseek(fs, 0, SEEK_SET) != 0 || fread(header, 1, MdxFormat_HeaderSize, fs) != MdxFormat_HeaderSize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    tagCount = header[28];
    if (tagCount == 0) {
        fclose(fs);
        return GMB_DB_OK;
    }
    if (tagCount > GMB_MDX_MAX_TAGS) tagCount = GMB_MDX_MAX_TAGS;
    if (fseek(fs, MdxFormat_TagTableOffset, SEEK_SET) != 0
        || fread(table, 1, (size_t)tagCount * MdxFormat_TagEntrySize, fs) != (size_t)tagCount * MdxFormat_TagEntrySize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    for (i = 0; i < tagCount && n < cap; i++) {
        const uint8_t* entry = table + i * MdxFormat_TagEntrySize;
        uint32_t headerPage = mdx_ru32(entry);
        MdxTagDescriptor* d;
        if (headerPage == 0) continue;
        d = &out[n];
        memset(d, 0, sizeof(*d));
        d->HeaderPage = headerPage;
        mdx_read_cstring(entry + 4, MdxFormat_TagNameLength, d->Name, (int)sizeof(d->Name));
        if (mdx_read_page(fs, headerPage, tagHeader) != 0) continue;
        d->RootPage = mdx_ru32(tagHeader);
        d->KeyLength = (int)mdx_ru16(tagHeader + 12);
        d->Unique = tagHeader[23] != 0;
        mdx_read_cstring(tagHeader + NdxFormat_ExpressionOffset, NdxFormat_ExpressionLength,
                         d->Expression, (int)sizeof(d->Expression));
        n++;
    }
    fclose(fs);
    if (nOut) *nOut = n;
    return GMB_DB_OK;
}

static void mdx_build_header(uint8_t* header, const char* dbfBaseName, uint8_t tagCount)
{
    char stem[32];
    memset(header, 0, MdxFormat_HeaderSize);
    header[0] = 2;
    header[20] = 2;
    header[28] = tagCount;
    header[29] = (uint8_t)MdxFormat_TagNameLength;
    mdx_stem(dbfBaseName ? dbfBaseName : "", stem, sizeof(stem));
    {
        size_t n = strlen(stem);
        if (n > 16) n = 16;
        memcpy(header + 4, stem, n);
    }
}

static void mdx_build_tag_header_page(uint8_t* page, const char* expression, int keyLength, int unique, uint32_t rootPage)
{
    int i, n;
    memset(page, 0, NdxFormat_BlockSize);
    mdx_le32(page, rootPage);
    mdx_le16(page + 12, (uint16_t)keyLength);
    mdx_le16(page + 14, NdxFormat_MaxKeysPerPage(keyLength));
    mdx_le16(page + 18, (uint16_t)NdxFormat_GroupLength(keyLength));
    page[23] = unique ? (uint8_t)1 : (uint8_t)0;
    if (expression) {
        n = (int)strlen(expression);
        if (n > NdxFormat_ExpressionLength) n = NdxFormat_ExpressionLength;
        for (i = 0; i < n; i++)
            page[NdxFormat_ExpressionOffset + i] = (uint8_t)tolower((unsigned char)expression[i]);
    }
}

static int mdx_build_ndx_pages(
    int keyLength,
    const uint8_t* const* keys,
    const int* keyLens,
    const int* recnos,
    int n,
    uint8_t pages[][NdxFormat_BlockSize],
    int maxPages,
    int* nOut)
{
    char tmp[] = "native/gmb_db_c/build/mdxpg_XXXXXX";
    FILE* fs;
    int fd, got = 0;
    uint8_t buf[NdxFormat_BlockSize];
    *nOut = 0;
    fd = mkstemp(tmp);
    if (fd < 0) return GMB_DB_ERR;
    close(fd);
    if (NdxBuilder_Write(tmp, "", keyLength, 0, keys, keyLens, recnos, n) != GMB_DB_OK) {
        unlink(tmp);
        return GMB_DB_ERR;
    }
    fs = fopen(tmp, "rb");
    if (!fs) {
        unlink(tmp);
        return GMB_DB_ERR;
    }
    if (fseek(fs, NdxFormat_BlockSize, SEEK_SET) != 0) {
        fclose(fs);
        unlink(tmp);
        return GMB_DB_ERR;
    }
    while (got < maxPages && fread(buf, 1, NdxFormat_BlockSize, fs) == NdxFormat_BlockSize) {
        memcpy(pages[got], buf, NdxFormat_BlockSize);
        got++;
    }
    fclose(fs);
    unlink(tmp);
    *nOut = got;
    return GMB_DB_OK;
}

int MdxCompound_WriteCompound(const char* path, const char* dbfBaseName, const MdxTagWrite* tags, int nTags)
{
    uint8_t header[MdxFormat_HeaderSize];
    uint8_t gap[MdxFormat_TagTableOffset - MdxFormat_HeaderSize];
    uint8_t tagTable[GMB_MDX_MAX_TAGS * MdxFormat_TagEntrySize];
    uint8_t dataPages[GMB_MDX_MAX_PAGES][NdxFormat_BlockSize];
    int nData = 0;
    int tagTableLen, dataStartOffset, dataStartBlock, pad, i, p;
    FILE* fs;
    char dirbuf[PATH_MAX];
    char* slash;

    if (!path || nTags < 0 || nTags > GMB_MDX_MAX_TAGS) return GMB_DB_ARG;
    if (nTags > 0 && !tags) return GMB_DB_ARG;

    memset(tagTable, 0, sizeof(tagTable));
    tagTableLen = nTags * MdxFormat_TagEntrySize;
    dataStartOffset = mdx_align_up(MdxFormat_TagTableOffset + tagTableLen, NdxFormat_BlockSize);
    dataStartBlock = dataStartOffset / NdxFormat_BlockSize;

    for (i = 0; i < nTags; i++) {
        uint32_t headerPageNum = (uint32_t)dataStartBlock + (uint32_t)nData;
        uint32_t rootPage;
        int ndxN = 0;
        uint8_t ndxPages[GMB_MDX_MAX_PAGES][NdxFormat_BlockSize];
        int k;
        if (nData >= GMB_MDX_MAX_PAGES) return GMB_DB_ERR;
        mdx_build_tag_header_page(dataPages[nData], tags[i].Expression, tags[i].KeyLength, tags[i].Unique, 0);
        nData++;
        if (mdx_build_ndx_pages(tags[i].KeyLength, tags[i].Keys, tags[i].KeyLens, tags[i].Recnos, tags[i].NEntries,
                                ndxPages, GMB_MDX_MAX_PAGES, &ndxN) != GMB_DB_OK)
            return GMB_DB_ERR;
        rootPage = (uint32_t)dataStartBlock + (uint32_t)nData;
        for (k = 0; k < ndxN; k++) {
            if (nData >= GMB_MDX_MAX_PAGES) return GMB_DB_ERR;
            memcpy(dataPages[nData], ndxPages[k], NdxFormat_BlockSize);
            nData++;
        }
        mdx_le32(dataPages[(int)(headerPageNum - (uint32_t)dataStartBlock)], rootPage);
        mdx_le32(dataPages[(int)(headerPageNum - (uint32_t)dataStartBlock)] + 4,
                 (uint32_t)dataStartBlock + (uint32_t)nData);
        mdx_le32(tagTable + i * MdxFormat_TagEntrySize, headerPageNum);
        mdx_write_tag_name(tagTable + i * MdxFormat_TagEntrySize + 4, MdxFormat_TagNameLength, tags[i].TagName);
    }

    mdx_build_header(header, dbfBaseName, (uint8_t)nTags);
    memset(gap, 0, sizeof(gap));
    pad = dataStartOffset - (MdxFormat_TagTableOffset + tagTableLen);

    snprintf(dirbuf, sizeof(dirbuf), "%s", path);
    slash = strrchr(dirbuf, '/');
    if (slash && slash != dirbuf) {
        *slash = 0;
        if (mkdir(dirbuf, 0755) != 0 && errno != EEXIST) return GMB_DB_ERR;
    }

    fs = fopen(path, "wb");
    if (!fs) return GMB_DB_ERR;
    if (fwrite(header, 1, MdxFormat_HeaderSize, fs) != MdxFormat_HeaderSize
        || fwrite(gap, 1, sizeof(gap), fs) != sizeof(gap)
        || (tagTableLen > 0 && fwrite(tagTable, 1, (size_t)tagTableLen, fs) != (size_t)tagTableLen)) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    if (pad > 0) {
        uint8_t z[512];
        memset(z, 0, sizeof(z));
        while (pad > 0) {
            int chunk = pad > 512 ? 512 : pad;
            if (fwrite(z, 1, (size_t)chunk, fs) != (size_t)chunk) {
                fclose(fs);
                return GMB_DB_ERR;
            }
            pad -= chunk;
        }
    }
    for (p = 0; p < nData; p++) {
        if (fwrite(dataPages[p], 1, NdxFormat_BlockSize, fs) != NdxFormat_BlockSize) {
            fclose(fs);
            return GMB_DB_ERR;
        }
    }
    fclose(fs);
    return GMB_DB_OK;
}

struct MdxTagIndex {
    char CompoundPath[PATH_MAX];
    MdxTagDescriptor Desc;
    int Writable;
    int nEntries;
    MdxEntry Entries[GMB_MDX_MAX_ENTRIES];
    pthread_mutex_t Lock;
    int LockInited;
};

static void mdx_free_entries(MdxTagIndex* self)
{
    int i;
    for (i = 0; i < self->nEntries; i++) {
        free(self->Entries[i].Key);
        self->Entries[i].Key = NULL;
    }
    self->nEntries = 0;
}

static int mdx_load_entries(MdxTagIndex* self)
{
    FILE* fs;
    int groupLen;
    self->nEntries = 0;
    if (self->Desc.RootPage == 0) return GMB_DB_OK;
    fs = fopen(self->CompoundPath, "rb");
    if (!fs) return GMB_DB_ERR;
    groupLen = NdxFormat_GroupLength(self->Desc.KeyLength);
    mdx_enumerate(fs, self->Desc.RootPage, self->Desc.KeyLength, groupLen, self->Entries, &self->nEntries, 0);
    fclose(fs);
    return GMB_DB_OK;
}

static MdxTagIndex* mdx_open_desc(const char* path, const MdxTagDescriptor* d, int writable)
{
    MdxTagIndex* self = (MdxTagIndex*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->CompoundPath, sizeof(self->CompoundPath), "%s", path);
    self->Desc = *d;
    self->Writable = writable;
    if (mdx_load_entries(self) != GMB_DB_OK) {
        MdxTagIndex_Dispose(self);
        return NULL;
    }
    if (pthread_mutex_init(&self->Lock, NULL) == 0)
        self->LockInited = 1;
    return self;
}

static int mdx_fill_write_from_loaded(MdxTagIndex* idx, MdxTagWrite* w,
                                      const uint8_t** keys, int* lens, int* recs)
{
    int i;
    w->TagName = idx->Desc.Name;
    w->Expression = idx->Desc.Expression;
    w->KeyLength = idx->Desc.KeyLength;
    w->Unique = idx->Desc.Unique;
    w->NEntries = idx->nEntries;
    w->Keys = keys;
    w->KeyLens = lens;
    w->Recnos = recs;
    for (i = 0; i < idx->nEntries; i++) {
        keys[i] = idx->Entries[i].Key;
        lens[i] = idx->Entries[i].KeyLen;
        recs[i] = idx->Entries[i].Recno;
    }
    return GMB_DB_OK;
}

static int mdx_rewrite(MdxTagIndex* self)
{
    MdxTagDescriptor table[GMB_MDX_MAX_TAGS];
    MdxTagWrite writes[GMB_MDX_MAX_TAGS];
    MdxTagIndex* others[GMB_MDX_MAX_TAGS];
    const uint8_t* keys[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int lens[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int recs[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int nTags = 0, i, rc;
    char dbfName[256];
    memset(others, 0, sizeof(others));
    if (MdxCompound_ReadTagTable(self->CompoundPath, table, GMB_MDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return GMB_DB_ERR;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, self->Desc.Name) == 0) {
            writes[i].TagName = self->Desc.Name;
            writes[i].Expression = self->Desc.Expression;
            writes[i].KeyLength = self->Desc.KeyLength;
            writes[i].Unique = self->Desc.Unique;
            writes[i].NEntries = self->nEntries;
            writes[i].Keys = keys[i];
            writes[i].KeyLens = lens[i];
            writes[i].Recnos = recs[i];
            {
                int k;
                for (k = 0; k < self->nEntries; k++) {
                    keys[i][k] = self->Entries[k].Key;
                    lens[i][k] = self->Entries[k].KeyLen;
                    recs[i][k] = self->Entries[k].Recno;
                }
            }
        } else {
            others[i] = mdx_open_desc(self->CompoundPath, &table[i], 0);
            if (!others[i]) {
                int j;
                for (j = 0; j < i; j++) MdxTagIndex_Dispose(others[j]);
                return GMB_DB_ERR;
            }
            mdx_fill_write_from_loaded(others[i], &writes[i], keys[i], lens[i], recs[i]);
        }
    }
    mdx_stem(self->CompoundPath, dbfName, sizeof(dbfName));
    rc = MdxCompound_WriteCompound(self->CompoundPath, dbfName, writes, nTags);
    for (i = 0; i < nTags; i++)
        MdxTagIndex_Dispose(others[i]);
    return rc;
}

MdxTagIndex* MdxTagIndex_CreateOrReplace(const char* compoundPath, const char* dbfBaseName,
                                         const char* tagName, const char* keyExpression, int keyLength, int isUnique)
{
    char norm[16];
    MdxTagDescriptor table[GMB_MDX_MAX_TAGS];
    MdxTagWrite writes[GMB_MDX_MAX_TAGS + 1];
    MdxTagIndex* others[GMB_MDX_MAX_TAGS];
    const uint8_t* keys[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int lens[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int recs[GMB_MDX_MAX_TAGS][GMB_MDX_MAX_ENTRIES];
    int nTags = 0, nWrites = 0, i, exists = 0;
    MdxTagDescriptor created;

    if (!compoundPath || !tagName || !keyExpression || keyLength <= 0) return NULL;
    mdx_normalize_tag(tagName, norm, sizeof(norm));
    if (norm[0] == 0) return NULL;
    memset(others, 0, sizeof(others));

    exists = access(compoundPath, F_OK) == 0;
    if (exists && MdxCompound_ReadTagTable(compoundPath, table, GMB_MDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0)
            continue;
        others[nWrites] = mdx_open_desc(compoundPath, &table[i], 0);
        if (!others[nWrites]) {
            int j;
            for (j = 0; j < nWrites; j++) MdxTagIndex_Dispose(others[j]);
            return NULL;
        }
        mdx_fill_write_from_loaded(others[nWrites], &writes[nWrites], keys[nWrites], lens[nWrites], recs[nWrites]);
        nWrites++;
    }
    writes[nWrites].TagName = norm;
    writes[nWrites].Expression = keyExpression;
    writes[nWrites].KeyLength = keyLength;
    writes[nWrites].Unique = isUnique;
    writes[nWrites].Keys = NULL;
    writes[nWrites].KeyLens = NULL;
    writes[nWrites].Recnos = NULL;
    writes[nWrites].NEntries = 0;
    nWrites++;
    if (MdxCompound_WriteCompound(compoundPath, dbfBaseName ? dbfBaseName : compoundPath, writes, nWrites) != GMB_DB_OK) {
        for (i = 0; i < nWrites - 1; i++) MdxTagIndex_Dispose(others[i]);
        return NULL;
    }
    for (i = 0; i < nWrites - 1; i++)
        MdxTagIndex_Dispose(others[i]);
    if (MdxCompound_ReadTagTable(compoundPath, table, GMB_MDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    memset(&created, 0, sizeof(created));
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0) {
            created = table[i];
            break;
        }
    }
    if (created.Name[0] == 0) return NULL;
    return mdx_open_desc(compoundPath, &created, 1);
}

MdxTagIndex* MdxTagIndex_Open(const char* compoundPath, const char* tagName, int writable)
{
    MdxTagDescriptor table[GMB_MDX_MAX_TAGS];
    int nTags = 0, i;
    char norm[16];
    if (!compoundPath || !tagName) return NULL;
    mdx_normalize_tag(tagName, norm, sizeof(norm));
    if (MdxCompound_ReadTagTable(compoundPath, table, GMB_MDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0)
            return mdx_open_desc(compoundPath, &table[i], writable);
    }
    return NULL;
}

void MdxTagIndex_Dispose(MdxTagIndex* self)
{
    if (!self) return;
    mdx_free_entries(self);
    if (self->LockInited) pthread_mutex_destroy(&self->Lock);
    free(self);
}

const char* MdxTagIndex_get_Name(const MdxTagIndex* self)
{
    return self ? self->Desc.Name : NULL;
}

const char* MdxTagIndex_GetKeyExpressionString(const MdxTagIndex* self)
{
    return self ? self->Desc.Expression : NULL;
}

int MdxTagIndex_get_KeyLength(const MdxTagIndex* self)
{
    return self ? self->Desc.KeyLength : 0;
}

int MdxTagIndex_Seek(MdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    int exact, idx, rec;
    if (!self || !keyBytes) return -1;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->nEntries == 0) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return -1;
    }
    idx = mdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (exact)
        rec = self->Entries[idx].Recno;
    else
        rec = (soft && idx < self->nEntries) ? self->Entries[idx].Recno : -1;
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rec;
}

int MdxTagIndex_Insert(MdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, rc;
    uint8_t* copy;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes || keyLen < 0) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->nEntries >= GMB_MDX_MAX_ENTRIES) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    idx = mdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (self->Desc.Unique && exact) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    copy = (uint8_t*)malloc((size_t)keyLen);
    if (!copy) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    if (keyLen > 0) memcpy(copy, keyBytes, (size_t)keyLen);
    memmove(&self->Entries[idx + 1], &self->Entries[idx], (size_t)(self->nEntries - idx) * sizeof(MdxEntry));
    self->Entries[idx].Key = copy;
    self->Entries[idx].KeyLen = keyLen;
    self->Entries[idx].Recno = recno;
    self->nEntries++;
    rc = mdx_rewrite(self);
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rc;
}

int MdxTagIndex_Delete(MdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, i;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    idx = mdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (exact) {
        for (i = idx; i < self->nEntries; i++) {
            if (mdx_keycmp(self->Entries[i].Key, self->Entries[i].KeyLen, keyBytes, keyLen) != 0)
                break;
            if (self->Entries[i].Recno == recno) {
                free(self->Entries[i].Key);
                memmove(&self->Entries[i], &self->Entries[i + 1],
                        (size_t)(self->nEntries - i - 1) * sizeof(MdxEntry));
                self->nEntries--;
                mdx_rewrite(self);
                break;
            }
        }
    }
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return GMB_DB_OK;
}

static void mdx_sc_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int MdxCompound_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/mdxsc_XXXXXX";
    char path[PATH_MAX];
    MdxTagIndex *nameIdx, *idIdx, *reopen;
    MdxTagDescriptor table[4];
    int nTags = 0, i;
    uint8_t key[10];
    uint8_t head[64];
    FILE* f;
    long flen;

    if (!mkdtemp(dir)) return -2;
    snprintf(path, sizeof(path), "%s/t.mdx", dir);

    nameIdx = MdxTagIndex_CreateOrReplace(path, "CUSTOMER", "name", "NAME", 10, 0);
    if (!nameIdx) return -3;
    if (!MdxTagIndex_get_Name(nameIdx) || strcmp(MdxTagIndex_get_Name(nameIdx), "NAME") != 0) {
        MdxTagIndex_Dispose(nameIdx);
        return -4;
    }
    if (!MdxTagIndex_GetKeyExpressionString(nameIdx)
        || strcmp(MdxTagIndex_GetKeyExpressionString(nameIdx), "name") != 0) {
        MdxTagIndex_Dispose(nameIdx);
        return -5;
    }

    f = fopen(path, "rb");
    if (!f) {
        MdxTagIndex_Dispose(nameIdx);
        return -6;
    }
    if (fread(head, 1, 32, f) != 32) {
        fclose(f);
        MdxTagIndex_Dispose(nameIdx);
        return -7;
    }
    fseek(f, 0, SEEK_END);
    flen = ftell(f);
    fclose(f);
    if (head[0] != 2 || head[20] != 2 || head[28] != 1 || head[29] != 10
        || memcmp(head + 4, "CUSTOMER", 8) != 0 || flen != 2048) {
        MdxTagIndex_Dispose(nameIdx);
        return -8;
    }

    if (MdxCompound_ReadTagTable(path, table, 4, &nTags) != GMB_DB_OK || nTags != 1
        || table[0].HeaderPage != 2 || table[0].RootPage != 3 || table[0].KeyLength != 10) {
        MdxTagIndex_Dispose(nameIdx);
        return -9;
    }

    for (i = 0; i < 8; i++) {
        mdx_sc_key(key, i);
        if (MdxTagIndex_Insert(nameIdx, key, 10, 100 + i) != GMB_DB_OK) {
            MdxTagIndex_Dispose(nameIdx);
            return -10 - i;
        }
    }
    mdx_sc_key(key, 3);
    if (MdxTagIndex_Seek(nameIdx, key, 10, 0) != 103) {
        MdxTagIndex_Dispose(nameIdx);
        return -20;
    }
    MdxTagIndex_Dispose(nameIdx);

    idIdx = MdxTagIndex_CreateOrReplace(path, "CUSTOMER", "id", "ID", 10, 1);
    if (!idIdx) return -21;
    mdx_sc_key(key, 0);
    if (MdxTagIndex_Insert(idIdx, key, 10, 1) != GMB_DB_OK) {
        MdxTagIndex_Dispose(idIdx);
        return -22;
    }
    if (MdxTagIndex_Insert(idIdx, key, 10, 2) != GMB_DB_ERR) {
        MdxTagIndex_Dispose(idIdx);
        return -23;
    }
    MdxTagIndex_Dispose(idIdx);

    if (MdxCompound_ReadTagTable(path, table, 4, &nTags) != GMB_DB_OK || nTags != 2) return -24;

    reopen = MdxTagIndex_Open(path, "NAME", 0);
    if (!reopen) return -25;
    mdx_sc_key(key, 3);
    if (MdxTagIndex_Seek(reopen, key, 10, 0) != 103) {
        MdxTagIndex_Dispose(reopen);
        return -26;
    }
    mdx_sc_key(key, 7);
    if (MdxTagIndex_Seek(reopen, key, 10, 0) != 107) {
        MdxTagIndex_Dispose(reopen);
        return -27;
    }
    if (MdxTagIndex_Insert(reopen, key, 10, 1) != GMB_DB_ERR) {
        MdxTagIndex_Dispose(reopen);
        return -28;
    }
    MdxTagIndex_Dispose(reopen);

    reopen = MdxTagIndex_Open(path, "ID", 1);
    if (!reopen) return -29;
    mdx_sc_key(key, 0);
    if (MdxTagIndex_Seek(reopen, key, 10, 0) != 1) {
        MdxTagIndex_Dispose(reopen);
        return -30;
    }
    mdx_sc_key(key, 3);
    if (MdxTagIndex_Delete(reopen, key, 10, 1) != GMB_DB_OK) {
        MdxTagIndex_Dispose(reopen);
        return -31;
    }
    mdx_sc_key(key, 0);
    if (MdxTagIndex_Delete(reopen, key, 10, 1) != GMB_DB_OK
        || MdxTagIndex_Seek(reopen, key, 10, 0) != -1) {
        MdxTagIndex_Dispose(reopen);
        return -32;
    }
    MdxTagIndex_Dispose(reopen);

    unlink(path);
    rmdir(dir);
    return GMB_DB_OK;
}

#define GMB_CDX_MAX_ENTRIES 4096
#define GMB_CDX_MAX_PAGES 512
#define GMB_CDX_MAX_DEPTH 16
#define GMB_CDX_MAX_KEY 256
#define CDX_U32MAX 0xFFFFFFFFu

typedef struct {
    uint8_t* Key;
    int KeyLen;
    int Recno;
} CdxEntry;

typedef struct {
    uint8_t Key[GMB_CDX_MAX_KEY];
    int Recno;
} CdxSortItem;

static int g_cdx_sort_keylen;

static int cdx_sort_cmp(const void* a, const void* b)
{
    const CdxSortItem* x = (const CdxSortItem*)a;
    const CdxSortItem* y = (const CdxSortItem*)b;
    return memcmp(x->Key, y->Key, (size_t)g_cdx_sort_keylen);
}

static int cdx_find_key(CdxEntry* entries, int n, const uint8_t* keyBytes, int keyLen, int* exactMatch)
{
    int lo = 0, hi = n - 1, insertAt = n;
    *exactMatch = 0;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = mdx_keycmp(keyBytes, keyLen, entries[mid].Key, entries[mid].KeyLen);
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

static void cdx_pad_key(uint8_t* dest, int keyLength, const uint8_t* key, int keyLen)
{
    int copy = keyLen < keyLength ? keyLen : keyLength;
    memset(dest, ' ', (size_t)keyLength);
    if (copy > 0) memcpy(dest, key, (size_t)copy);
}

static int cdx_count_dup(const uint8_t* prev, const uint8_t* key, int keyLength)
{
    int i = 0;
    while (i < keyLength && prev[i] == key[i]) i++;
    return i;
}

static int cdx_count_trail(const uint8_t* key, int keyLength, int dup)
{
    int trail = 0, i;
    for (i = keyLength - 1; i >= dup; i--) {
        if (key[i] != (uint8_t)' ') break;
        trail++;
    }
    return trail;
}

static int cdx_can_fit_leaf(int count, int keyLength)
{
    int keyPos = CdxFormat_PageSize;
    int poolEnd = CdxFormat_ExtNodeHeaderSize + (count * 4);
    int i;
    for (i = 0; i < count; i++) {
        keyPos -= keyLength;
        if (keyPos < poolEnd) return 0;
    }
    return 1;
}

static int cdx_max_keys_leaf(int keyLength)
{
    int lo = 1, hi = 256;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (cdx_can_fit_leaf(mid, keyLength)) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

static int cdx_max_keys_interior(int keyLength)
{
    int slot = keyLength + 4;
    int n = (CdxFormat_PageSize - CdxFormat_IntNodeHeaderSize) / slot - 1;
    return n > 1 ? n : 1;
}

static void cdx_build_empty_leaf(uint8_t* page, int keyLength)
{
    memset(page, 0, CdxFormat_PageSize);
    mdx_le16(page, 2);
    mdx_le16(page + 2, 0);
    mdx_le32(page + 4, CDX_U32MAX);
    mdx_le32(page + 8, CDX_U32MAX);
    page[20] = 20;
    page[21] = 4;
    page[22] = 4;
    page[23] = 4;
    (void)keyLength;
}

static int cdx_build_leaf(uint8_t* page, const CdxSortItem* entries, int n, int keyLength)
{
    uint8_t prev[GMB_CDX_MAX_KEY];
    int i, keyPos, poolOffset;
    memset(page, 0, CdxFormat_PageSize);
    mdx_le16(page, 3);
    mdx_le16(page + 2, (uint16_t)n);
    mdx_le32(page + 4, CDX_U32MAX);
    mdx_le32(page + 8, CDX_U32MAX);
    page[20] = 20;
    page[21] = 4;
    page[22] = 4;
    page[23] = 4;
    memset(prev, 0, (size_t)keyLength);
    keyPos = CdxFormat_PageSize;
    poolOffset = CdxFormat_ExtNodeHeaderSize;
    for (i = 0; i < n; i++) {
        int dup = cdx_count_dup(prev, entries[i].Key, keyLength);
        int trail = cdx_count_trail(entries[i].Key, keyLength, dup);
        int varLen = keyLength - dup - trail;
        uint32_t packed;
        if (varLen < 0) return GMB_DB_ERR;
        keyPos -= varLen;
        if (keyPos < poolOffset + (n * 4)) return GMB_DB_ERR;
        if (varLen > 0) memcpy(page + keyPos, entries[i].Key + dup, (size_t)varLen);
        packed = ((uint32_t)entries[i].Recno << 8) | ((uint32_t)trail << 4) | (uint32_t)dup;
        mdx_le32(page + poolOffset + (i * 4), packed);
        memcpy(prev, entries[i].Key, (size_t)keyLength);
    }
    return GMB_DB_OK;
}

static void cdx_build_interior(uint8_t* page, const uint32_t* childOffsets, int nChildren,
                              const uint8_t separators[][GMB_CDX_MAX_KEY], int nSep, int keyLength)
{
    int keyCount = nSep;
    int keysStart = CdxFormat_PageSize - ((keyCount + 1) * (keyLength + 4));
    int i;
    memset(page, 0, CdxFormat_PageSize);
    mdx_le16(page, 1);
    mdx_le16(page + 2, (uint16_t)keyCount);
    mdx_le32(page + 4, CDX_U32MAX);
    mdx_le32(page + 8, CDX_U32MAX);
    for (i = 0; i < keyCount; i++) {
        int slot = keysStart + (i * (keyLength + 4));
        memcpy(page + slot, separators[i], (size_t)keyLength);
        mdx_le32(page + slot + keyLength, childOffsets[i]);
    }
    mdx_le32(page + keysStart + (keyCount * (keyLength + 4)) + keyLength, childOffsets[keyCount]);
    (void)nChildren;
}

static int cdx_decode_leaf(const uint8_t* page, int keyCount, int keyLength,
                           CdxEntry* out, int cap, int* nOut)
{
    int recBits = page[20], dupBits = page[21], trlBits = page[22], reqByte = page[23];
    uint16_t dcMask, tcMask;
    int keyPos = CdxFormat_PageSize, i, n = 0;
    uint8_t prev[GMB_CDX_MAX_KEY];
    if (reqByte == 0) reqByte = 4;
    if (keyLength <= 0 || keyLength > GMB_CDX_MAX_KEY) return GMB_DB_ERR;
    dcMask = (uint16_t)((1 << dupBits) - 1);
    tcMask = (uint16_t)((1 << trlBits) - 1);
    memset(prev, 0, (size_t)keyLength);
    for (i = 0; i < keyCount && n < cap; i++) {
        int pos = i * reqByte;
        uint32_t packed = mdx_ru32(page + CdxFormat_ExtNodeHeaderSize + pos);
        int dup = dupBits > 0 ? (int)(packed & dcMask) : 0;
        int trail = trlBits > 0 ? (int)((packed >> dupBits) & tcMask) : 0;
        uint32_t recno = recBits > 0 ? packed >> (dupBits + trlBits) : 0u;
        int varLen = keyLength - dup - trail;
        CdxEntry* e;
        keyPos -= varLen;
        e = &out[n];
        e->KeyLen = keyLength;
        e->Key = (uint8_t*)malloc((size_t)keyLength);
        if (!e->Key) return GMB_DB_ERR;
        memset(e->Key, 0, (size_t)keyLength);
        if (dup > 0) memcpy(e->Key, prev, (size_t)dup);
        if (varLen > 0 && keyPos >= 0) memcpy(e->Key + dup, page + keyPos, (size_t)varLen);
        if (trail > 0) memset(e->Key + keyLength - trail, ' ', (size_t)trail);
        memcpy(prev, e->Key, (size_t)keyLength);
        e->Recno = (int)recno;
        n++;
    }
    *nOut = n;
    return GMB_DB_OK;
}

static uint32_t cdx_interior_child(const uint8_t* page, int keyIndex, int keyCount, int keyLength)
{
    int keysStart = CdxFormat_PageSize - ((keyCount + 1) * (keyLength + 4));
    int offset = keysStart + (keyIndex * (keyLength + 4)) + keyLength;
    if (offset + 4 > CdxFormat_PageSize) return 0;
    return mdx_ru32(page + offset);
}

int CdxCompact_MaxKeysPerLeaf(int keyLength)
{
    return cdx_max_keys_leaf(keyLength);
}

int CdxCompact_MaxKeysPerInterior(int keyLength)
{
    return cdx_max_keys_interior(keyLength);
}

int CdxCompact_BuildLeafPage(uint8_t* page, const uint8_t* keysFlat, const int* recnos, int n, int keyLength)
{
    CdxSortItem* items;
    int i, rc;
    if (!page || keyLength <= 0 || keyLength > GMB_CDX_MAX_KEY) return GMB_DB_ARG;
    if (n < 0) return GMB_DB_ARG;
    if (n == 0) {
        cdx_build_empty_leaf(page, keyLength);
        return GMB_DB_OK;
    }
    if (!keysFlat || !recnos) return GMB_DB_ARG;
    items = (CdxSortItem*)calloc((size_t)n, sizeof(*items));
    if (!items) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        memcpy(items[i].Key, keysFlat + (i * keyLength), (size_t)keyLength);
        items[i].Recno = recnos[i];
    }
    rc = cdx_build_leaf(page, items, n, keyLength);
    free(items);
    return rc;
}

int CdxCompact_BuildInteriorPage(uint8_t* page, const uint32_t* children, int nChildren,
                                 const uint8_t* sepsFlat, int nSep, int keyLength)
{
    uint8_t seps[256][GMB_CDX_MAX_KEY];
    int i;
    if (!page || !children || nChildren != nSep + 1 || keyLength <= 0 || nSep < 0 || nSep > 256)
        return GMB_DB_ARG;
    memset(seps, 0, sizeof(seps));
    for (i = 0; i < nSep; i++)
        memcpy(seps[i], sepsFlat + (i * keyLength), (size_t)keyLength);
    cdx_build_interior(page, children, nChildren, seps, nSep, keyLength);
    return GMB_DB_OK;
}

int CdxCompact_DecodeLeafKeys(const uint8_t* page, int keyCount, int keyLength,
                              uint8_t* keysFlat, int* recnos, int cap, int* nOut)
{
    CdxEntry tmp[128];
    int nd = 0, i, use, rc;
    if (nOut) *nOut = 0;
    if (!page || !keysFlat || !recnos || cap <= 0) return GMB_DB_ARG;
    memset(tmp, 0, sizeof(tmp));
    use = cap < 128 ? cap : 128;
    rc = cdx_decode_leaf(page, keyCount, keyLength, tmp, use, &nd);
    if (rc != GMB_DB_OK) {
        for (i = 0; i < nd; i++) free(tmp[i].Key);
        return rc;
    }
    for (i = 0; i < nd; i++) {
        memcpy(keysFlat + (i * keyLength), tmp[i].Key, (size_t)keyLength);
        recnos[i] = tmp[i].Recno;
        free(tmp[i].Key);
    }
    if (nOut) *nOut = nd;
    return GMB_DB_OK;
}

uint32_t CdxCompact_GetInteriorChild(const uint8_t* page, int keyIndex, int keyCount, int keyLength)
{
    return cdx_interior_child(page, keyIndex, keyCount, keyLength);
}

void CdxCompact_GetInteriorKey(const uint8_t* page, int index, int keyLength, uint8_t* out)
{
    int keyCount = (int)mdx_ru16(page + 2);
    int slotStart = CdxFormat_PageSize - ((keyCount + 1) * (keyLength + 4));
    memcpy(out, page + slotStart + (index * (keyLength + 4)), (size_t)keyLength);
}

void CdxCompact_SetInteriorChild(uint8_t* page, int index, int keyCount, int keyLength, uint32_t newChild)
{
    int slotStart = CdxFormat_PageSize - ((keyCount + 1) * (keyLength + 4));
    int offset = slotStart + (index * (keyLength + 4)) + keyLength;
    mdx_le32(page + offset, newChild);
}

static int cdx_read_page(FILE* fs, uint32_t offset, uint8_t* buffer)
{
    if (fseek(fs, (long)offset, SEEK_SET) != 0) return -1;
    if (fread(buffer, 1, CdxFormat_PageSize, fs) != CdxFormat_PageSize) return -1;
    return 0;
}

static void cdx_read_expression(const uint8_t* tagHeader, char* out, int cap)
{
    uint16_t keyExpLen = mdx_ru16(tagHeader + 510);
    int n;
    if (keyExpLen == 0 || cap <= 0) {
        if (cap > 0) out[0] = 0;
        return;
    }
    n = (int)keyExpLen;
    if (n > 510 - 16) n = 510 - 16;
    if (n >= cap) n = cap - 1;
    memcpy(out, tagHeader + 16, (size_t)n);
    out[n] = 0;
}

static void cdx_trim_name(char* s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\0')) n--;
    s[n] = 0;
}

static void cdx_walk_catalog(FILE* fs, uint32_t offset, CdxTagDescriptor* out, int cap, int* n, int depth)
{
    uint8_t page[CdxFormat_PageSize];
    uint16_t attr, keyCount;
    int isLeaf, i;
    if (depth > GMB_CDX_MAX_DEPTH || *n >= cap) return;
    if (cdx_read_page(fs, offset, page) != 0) return;
    attr = mdx_ru16(page);
    keyCount = mdx_ru16(page + 2);
    isLeaf = (attr & 2) != 0;
    if (isLeaf) {
        CdxEntry decoded[GMB_CDX_MAX_TAGS];
        int nd = 0, k = 0;
        memset(decoded, 0, sizeof(decoded));
        if (cdx_decode_leaf(page, keyCount, CdxFormat_TagNameLength, decoded, GMB_CDX_MAX_TAGS, &nd) != GMB_DB_OK) {
            for (k = 0; k < nd; k++) free(decoded[k].Key);
            return;
        }
        for (k = 0; k < nd && *n < cap; k++) {
            uint8_t tagHeader[CdxFormat_PageSize];
            CdxTagDescriptor* d;
            char name[16];
            int nl = decoded[k].KeyLen < 15 ? decoded[k].KeyLen : 15;
            memcpy(name, decoded[k].Key, (size_t)nl);
            name[nl] = 0;
            cdx_trim_name(name);
            if (name[0] == 0) {
                free(decoded[k].Key);
                continue;
            }
            if (cdx_read_page(fs, (uint32_t)decoded[k].Recno, tagHeader) != 0) {
                free(decoded[k].Key);
                continue;
            }
            d = &out[*n];
            memset(d, 0, sizeof(*d));
            snprintf(d->Name, sizeof(d->Name), "%s", name);
            d->HeaderOffset = (uint32_t)decoded[k].Recno;
            d->RootOffset = mdx_ru32(tagHeader);
            d->KeyLength = (int)mdx_ru16(tagHeader + 12);
            d->Unique = (tagHeader[14] & 0x01) != 0;
            d->Descending = tagHeader[502] != 0;
            cdx_read_expression(tagHeader, d->Expression, (int)sizeof(d->Expression));
            (*n)++;
            free(decoded[k].Key);
        }
        for (; k < nd; k++) free(decoded[k].Key);
        return;
    }
    for (i = 0; i < (int)keyCount; i++) {
        uint32_t child = cdx_interior_child(page, i, keyCount, CdxFormat_TagNameLength);
        if (child != 0 && child != CDX_U32MAX)
            cdx_walk_catalog(fs, child, out, cap, n, depth + 1);
    }
    {
        uint32_t right = cdx_interior_child(page, keyCount, keyCount, CdxFormat_TagNameLength);
        if (right != 0 && right != CDX_U32MAX)
            cdx_walk_catalog(fs, right, out, cap, n, depth + 1);
    }
}

int CdxCompound_ReadTagTable(const char* path, CdxTagDescriptor* out, int cap, int* nOut)
{
    FILE* fs;
    uint8_t header[CdxFormat_HeaderSize];
    long flen;
    uint32_t rootOffset;
    int n = 0;
    if (nOut) *nOut = 0;
    if (!path || !out || cap <= 0) return GMB_DB_ARG;
    fs = fopen(path, "rb");
    if (!fs) return GMB_DB_ERR;
    if (fseek(fs, 0, SEEK_END) != 0) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    flen = ftell(fs);
    if (flen < CdxFormat_HeaderSize) {
        fclose(fs);
        return GMB_DB_OK;
    }
    if (fseek(fs, 0, SEEK_SET) != 0 || fread(header, 1, CdxFormat_HeaderSize, fs) != CdxFormat_HeaderSize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    if ((header[14] & CdxFormat_TypeCompound) == 0) {
        fclose(fs);
        return GMB_DB_OK;
    }
    rootOffset = mdx_ru32(header);
    if (rootOffset == 0 || (long)rootOffset >= flen) {
        fclose(fs);
        return GMB_DB_OK;
    }
    cdx_walk_catalog(fs, rootOffset, out, cap, &n, 0);
    fclose(fs);
    if (nOut) *nOut = n;
    return GMB_DB_OK;
}

static void cdx_build_compound_header(uint8_t* header, uint32_t catalogRoot, int keyLength)
{
    memset(header, 0, CdxFormat_HeaderSize);
    mdx_le32(header, catalogRoot);
    mdx_le16(header + 12, (uint16_t)keyLength);
    header[14] = (uint8_t)(CdxFormat_TypeCompact | CdxFormat_TypeCompound);
}

static void cdx_build_tag_header(uint8_t* header, const char* expression, int keyLength,
                                 int unique, int descending, uint32_t rootOffset)
{
    int n = 0;
    memset(header, 0, CdxFormat_PageSize);
    mdx_le32(header, rootOffset);
    mdx_le16(header + 12, (uint16_t)keyLength);
    header[14] = (uint8_t)((unique ? 0x01 : 0x00) | CdxFormat_TypeCompact);
    header[502] = descending ? (uint8_t)1 : (uint8_t)0;
    if (expression) {
        n = (int)strlen(expression);
        if (n > 510 - 16) n = 510 - 16;
        memcpy(header + 16, expression, (size_t)n);
    }
    mdx_le16(header + 510, (uint16_t)n);
}

static int cdx_build_catalog_leaf(uint8_t* page, const char names[][16], const uint32_t* offsets, int n)
{
    CdxSortItem items[GMB_CDX_MAX_TAGS];
    int i;
    memset(items, 0, sizeof(items));
    for (i = 0; i < n; i++) {
        size_t nl = strlen(names[i]);
        if (nl > (size_t)CdxFormat_TagNameLength) nl = (size_t)CdxFormat_TagNameLength;
        memcpy(items[i].Key, names[i], nl);
        items[i].Recno = (int)offsets[i];
    }
    g_cdx_sort_keylen = CdxFormat_TagNameLength;
    qsort(items, (size_t)n, sizeof(items[0]), cdx_sort_cmp);
    if (n == 0) {
        cdx_build_empty_leaf(page, CdxFormat_TagNameLength);
        return GMB_DB_OK;
    }
    return cdx_build_leaf(page, items, n, CdxFormat_TagNameLength);
}

typedef struct {
    int PageIndex;
    uint8_t PromoteKey[GMB_CDX_MAX_KEY];
} CdxLevelItem;

static int cdx_build_interior_level(
    CdxLevelItem* children, int nChildren,
    uint8_t pages[][CdxFormat_PageSize], int* nPages,
    int keyLength, int maxBranchKeys, uint32_t treeBaseOffset,
    CdxLevelItem* parents, int* nParents)
{
    int index = 0;
    *nParents = 0;
    while (index < nChildren) {
        int groupStart = index;
        int childIdx[256];
        uint8_t seps[256][GMB_CDX_MAX_KEY];
        uint32_t childOff[257];
        int nChild = 1, nSep = 0, p, pi;
        if (*nPages >= GMB_CDX_MAX_PAGES) return GMB_DB_ERR;
        childIdx[0] = children[index].PageIndex;
        index++;
        while (index < nChildren && nSep < maxBranchKeys) {
            memcpy(seps[nSep], children[index].PromoteKey, (size_t)keyLength);
            childIdx[nChild++] = children[index].PageIndex;
            nSep++;
            index++;
        }
        for (p = 0; p < nChild; p++)
            childOff[p] = treeBaseOffset + (uint32_t)(childIdx[p] * CdxFormat_PageSize);
        cdx_build_interior(pages[*nPages], childOff, nChild, seps, nSep, keyLength);
        pi = *nPages;
        (*nPages)++;
        parents[*nParents].PageIndex = pi;
        if (nSep > 0) memcpy(parents[*nParents].PromoteKey, seps[0], (size_t)keyLength);
        else memcpy(parents[*nParents].PromoteKey, children[groupStart].PromoteKey, (size_t)keyLength);
        (*nParents)++;
    }
    return GMB_DB_OK;
}

static int cdx_build_tag_pages(
    int keyLength,
    const uint8_t* const* keys, const int* keyLens, const int* recnos, int n,
    uint32_t treeBaseOffset,
    uint8_t pages[][CdxFormat_PageSize], int* nPagesOut, int* rootIndexOut)
{
    CdxSortItem* sorted = NULL;
    int maxLeaf, maxBranch, i, nPages = 0;
    CdxLevelItem leaves[GMB_CDX_MAX_PAGES];
    int nLeaves = 0;
    *nPagesOut = 0;
    *rootIndexOut = 0;
    if (keyLength <= 0 || keyLength > GMB_CDX_MAX_KEY) return GMB_DB_ARG;
    if (n <= 0) {
        cdx_build_empty_leaf(pages[0], keyLength);
        *nPagesOut = 1;
        *rootIndexOut = 0;
        return GMB_DB_OK;
    }
    sorted = (CdxSortItem*)calloc((size_t)n, sizeof(*sorted));
    if (!sorted) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        int kl = keyLens ? keyLens[i] : keyLength;
        cdx_pad_key(sorted[i].Key, keyLength, keys[i], kl);
        sorted[i].Recno = recnos[i];
    }
    g_cdx_sort_keylen = keyLength;
    qsort(sorted, (size_t)n, sizeof(*sorted), cdx_sort_cmp);
    maxLeaf = cdx_max_keys_leaf(keyLength);
    maxBranch = cdx_max_keys_interior(keyLength);
    for (i = 0; i < n; ) {
        int take = n - i;
        if (take > maxLeaf) take = maxLeaf;
        if (nPages >= GMB_CDX_MAX_PAGES) {
            free(sorted);
            return GMB_DB_ERR;
        }
        if (cdx_build_leaf(pages[nPages], sorted + i, take, keyLength) != GMB_DB_OK) {
            free(sorted);
            return GMB_DB_ERR;
        }
        leaves[nLeaves].PageIndex = nPages;
        memcpy(leaves[nLeaves].PromoteKey, sorted[i].Key, (size_t)keyLength);
        nPages++;
        nLeaves++;
        i += take;
    }
    free(sorted);
    if (nLeaves == 1) {
        *nPagesOut = nPages;
        *rootIndexOut = leaves[0].PageIndex;
        return GMB_DB_OK;
    }
    {
        CdxLevelItem levelA[GMB_CDX_MAX_PAGES], levelB[GMB_CDX_MAX_PAGES];
        CdxLevelItem *cur = leaves, *next = levelA;
        int nCur = nLeaves, nNext;
        memcpy(levelA, leaves, (size_t)nLeaves * sizeof(leaves[0]));
        cur = levelA;
        next = levelB;
        while (nCur > 1) {
            if (cdx_build_interior_level(cur, nCur, pages, &nPages, keyLength, maxBranch,
                                         treeBaseOffset, next, &nNext) != GMB_DB_OK)
                return GMB_DB_ERR;
            {
                CdxLevelItem* tmp = cur;
                cur = next;
                next = tmp;
            }
            nCur = nNext;
        }
        *nPagesOut = nPages;
        *rootIndexOut = cur[0].PageIndex;
        return GMB_DB_OK;
    }
}

int CdxCompound_WriteCompound(const char* path, const CdxTagWrite* tags, int nTags)
{
    uint8_t pages[GMB_CDX_MAX_PAGES][CdxFormat_PageSize];
    uint8_t header[CdxFormat_HeaderSize];
    char catNames[GMB_CDX_MAX_TAGS][16];
    uint32_t catOff[GMB_CDX_MAX_TAGS];
    int nPages = 0, i, k, firstKeyLen = 0;
    uint32_t catalogRoot;
    FILE* fs;
    char dirbuf[PATH_MAX];
    char* slash;

    if (!path || nTags < 0 || nTags > GMB_CDX_MAX_TAGS) return GMB_DB_ARG;
    if (nTags > 0 && !tags) return GMB_DB_ARG;
    if (nTags > 0) firstKeyLen = tags[0].KeyLength;

    for (i = 0; i < nTags; i++) {
        uint32_t tagHeaderOffset = (uint32_t)(nPages * CdxFormat_PageSize + CdxFormat_HeaderSize);
        uint32_t treeBase = tagHeaderOffset + CdxFormat_PageSize;
        uint8_t tagPages[GMB_CDX_MAX_PAGES][CdxFormat_PageSize];
        int tagN = 0, rootIdx = 0;
        uint32_t tagRoot;
        if (nPages >= GMB_CDX_MAX_PAGES) return GMB_DB_ERR;
        cdx_build_tag_header(pages[nPages], tags[i].Expression, tags[i].KeyLength,
                             tags[i].Unique, tags[i].Descending, 0);
        nPages++;
        if (cdx_build_tag_pages(tags[i].KeyLength, tags[i].Keys, tags[i].KeyLens, tags[i].Recnos,
                                tags[i].NEntries, treeBase, tagPages, &tagN, &rootIdx) != GMB_DB_OK)
            return GMB_DB_ERR;
        tagRoot = treeBase + (uint32_t)(rootIdx * CdxFormat_PageSize);
        for (k = 0; k < tagN; k++) {
            if (nPages >= GMB_CDX_MAX_PAGES) return GMB_DB_ERR;
            memcpy(pages[nPages], tagPages[k], CdxFormat_PageSize);
            nPages++;
        }
        mdx_le32(pages[(int)((tagHeaderOffset - CdxFormat_HeaderSize) / CdxFormat_PageSize)], tagRoot);
        snprintf(catNames[i], sizeof(catNames[i]), "%s", tags[i].TagName ? tags[i].TagName : "");
        catOff[i] = tagHeaderOffset;
    }

    if (nPages >= GMB_CDX_MAX_PAGES) return GMB_DB_ERR;
    if (cdx_build_catalog_leaf(pages[nPages], catNames, catOff, nTags) != GMB_DB_OK)
        return GMB_DB_ERR;
    nPages++;
    catalogRoot = (uint32_t)(CdxFormat_HeaderSize + (nPages - 1) * CdxFormat_PageSize);
    cdx_build_compound_header(header, catalogRoot, firstKeyLen);

    snprintf(dirbuf, sizeof(dirbuf), "%s", path);
    slash = strrchr(dirbuf, '/');
    if (slash && slash != dirbuf) {
        *slash = 0;
        if (mkdir(dirbuf, 0755) != 0 && errno != EEXIST) return GMB_DB_ERR;
    }
    fs = fopen(path, "wb");
    if (!fs) return GMB_DB_ERR;
    if (fwrite(header, 1, CdxFormat_HeaderSize, fs) != CdxFormat_HeaderSize) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    for (i = 0; i < nPages; i++) {
        if (fwrite(pages[i], 1, CdxFormat_PageSize, fs) != CdxFormat_PageSize) {
            fclose(fs);
            return GMB_DB_ERR;
        }
    }
    fclose(fs);
    return GMB_DB_OK;
}

struct CdxTagIndex {
    char CompoundPath[PATH_MAX];
    CdxTagDescriptor Desc;
    int Writable;
    int nEntries;
    CdxEntry Entries[GMB_CDX_MAX_ENTRIES];
    pthread_mutex_t Lock;
    int LockInited;
    CdxPageCache* PageCache;
    uint32_t CachedRootOffset;
};

static void cdx_free_entries(CdxTagIndex* self)
{
    int i;
    for (i = 0; i < self->nEntries; i++) {
        free(self->Entries[i].Key);
        self->Entries[i].Key = NULL;
    }
    self->nEntries = 0;
}

static int cdx_walk_tag(FILE* fs, uint32_t offset, int keyLength, CdxEntry* out, int cap, int* n, int depth)
{
    uint8_t page[CdxFormat_PageSize];
    uint16_t attr, keyCount;
    int isLeaf, i;
    if (depth > GMB_CDX_MAX_DEPTH || *n >= cap) return GMB_DB_OK;
    if (cdx_read_page(fs, offset, page) != 0) return GMB_DB_ERR;
    attr = mdx_ru16(page);
    keyCount = mdx_ru16(page + 2);
    isLeaf = (attr & 2) != 0;
    if (isLeaf) {
        int nd = 0;
        if (cdx_decode_leaf(page, keyCount, keyLength, out + *n, cap - *n, &nd) != GMB_DB_OK)
            return GMB_DB_ERR;
        *n += nd;
        return GMB_DB_OK;
    }
    for (i = 0; i <= (int)keyCount; i++) {
        uint32_t child = cdx_interior_child(page, i, keyCount, keyLength);
        if (child != 0 && child != CDX_U32MAX) {
            if (cdx_walk_tag(fs, child, keyLength, out, cap, n, depth + 1) != GMB_DB_OK)
                return GMB_DB_ERR;
        }
    }
    return GMB_DB_OK;
}

static int cdx_load_entries(CdxTagIndex* self)
{
    FILE* fs;
    self->nEntries = 0;
    if (self->Desc.RootOffset == 0) return GMB_DB_OK;
    fs = fopen(self->CompoundPath, "rb");
    if (!fs) return GMB_DB_ERR;
    if (cdx_walk_tag(fs, self->Desc.RootOffset, self->Desc.KeyLength,
                     self->Entries, GMB_CDX_MAX_ENTRIES, &self->nEntries, 0) != GMB_DB_OK) {
        fclose(fs);
        return GMB_DB_ERR;
    }
    fclose(fs);
    return GMB_DB_OK;
}

static CdxTagIndex* cdx_open_desc(const char* path, const CdxTagDescriptor* d, int writable)
{
    CdxTagIndex* self = (CdxTagIndex*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->CompoundPath, sizeof(self->CompoundPath), "%s", path);
    self->Desc = *d;
    self->Writable = writable;
    if (cdx_load_entries(self) != GMB_DB_OK) {
        CdxTagIndex_Dispose(self);
        return NULL;
    }
    if (pthread_mutex_init(&self->Lock, NULL) == 0)
        self->LockInited = 1;
    return self;
}

static void cdx_fill_write(CdxTagIndex* idx, CdxTagWrite* w, const uint8_t** keys, int* lens, int* recs)
{
    int i;
    w->TagName = idx->Desc.Name;
    w->Expression = idx->Desc.Expression;
    w->KeyLength = idx->Desc.KeyLength;
    w->Unique = idx->Desc.Unique;
    w->Descending = idx->Desc.Descending;
    w->NEntries = idx->nEntries;
    w->Keys = keys;
    w->KeyLens = lens;
    w->Recnos = recs;
    for (i = 0; i < idx->nEntries; i++) {
        keys[i] = idx->Entries[i].Key;
        lens[i] = idx->Entries[i].KeyLen;
        recs[i] = idx->Entries[i].Recno;
    }
}

static int cdx_rewrite(CdxTagIndex* self)
{
    CdxTagDescriptor table[GMB_CDX_MAX_TAGS];
    CdxTagWrite writes[GMB_CDX_MAX_TAGS];
    CdxTagIndex* others[GMB_CDX_MAX_TAGS];
    const uint8_t* keys[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int lens[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int recs[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int nTags = 0, i, rc;
    memset(others, 0, sizeof(others));
    if (CdxCompound_ReadTagTable(self->CompoundPath, table, GMB_CDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return GMB_DB_ERR;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, self->Desc.Name) == 0) {
            writes[i].TagName = self->Desc.Name;
            writes[i].Expression = self->Desc.Expression;
            writes[i].KeyLength = self->Desc.KeyLength;
            writes[i].Unique = self->Desc.Unique;
            writes[i].Descending = self->Desc.Descending;
            writes[i].NEntries = self->nEntries;
            writes[i].Keys = keys[i];
            writes[i].KeyLens = lens[i];
            writes[i].Recnos = recs[i];
            {
                int k;
                for (k = 0; k < self->nEntries; k++) {
                    keys[i][k] = self->Entries[k].Key;
                    lens[i][k] = self->Entries[k].KeyLen;
                    recs[i][k] = self->Entries[k].Recno;
                }
            }
        } else {
            others[i] = cdx_open_desc(self->CompoundPath, &table[i], 0);
            if (!others[i]) {
                int j;
                for (j = 0; j < i; j++) CdxTagIndex_Dispose(others[j]);
                return GMB_DB_ERR;
            }
            cdx_fill_write(others[i], &writes[i], keys[i], lens[i], recs[i]);
        }
    }
    rc = CdxCompound_WriteCompound(self->CompoundPath, writes, nTags);
    for (i = 0; i < nTags; i++)
        CdxTagIndex_Dispose(others[i]);
    return rc;
}

CdxTagIndex* CdxTagIndex_CreateOrReplace(const char* compoundPath, const char* tagName,
                                         const char* keyExpression, int keyLength, int isUnique, int descending)
{
    char norm[16];
    CdxTagDescriptor table[GMB_CDX_MAX_TAGS];
    CdxTagWrite writes[GMB_CDX_MAX_TAGS + 1];
    CdxTagIndex* others[GMB_CDX_MAX_TAGS];
    const uint8_t* keys[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int lens[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int recs[GMB_CDX_MAX_TAGS][GMB_CDX_MAX_ENTRIES];
    int nTags = 0, nWrites = 0, i, exists;
    CdxTagDescriptor created;

    if (!compoundPath || !tagName || !keyExpression || keyLength <= 0) return NULL;
    mdx_normalize_tag(tagName, norm, sizeof(norm));
    if (norm[0] == 0) return NULL;
    memset(others, 0, sizeof(others));
    exists = access(compoundPath, F_OK) == 0;
    if (exists && CdxCompound_ReadTagTable(compoundPath, table, GMB_CDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0) continue;
        others[nWrites] = cdx_open_desc(compoundPath, &table[i], 0);
        if (!others[nWrites]) {
            int j;
            for (j = 0; j < nWrites; j++) CdxTagIndex_Dispose(others[j]);
            return NULL;
        }
        cdx_fill_write(others[nWrites], &writes[nWrites], keys[nWrites], lens[nWrites], recs[nWrites]);
        nWrites++;
    }
    writes[nWrites].TagName = norm;
    writes[nWrites].Expression = keyExpression;
    writes[nWrites].KeyLength = keyLength;
    writes[nWrites].Unique = isUnique;
    writes[nWrites].Descending = descending;
    writes[nWrites].Keys = NULL;
    writes[nWrites].KeyLens = NULL;
    writes[nWrites].Recnos = NULL;
    writes[nWrites].NEntries = 0;
    nWrites++;
    if (CdxCompound_WriteCompound(compoundPath, writes, nWrites) != GMB_DB_OK) {
        for (i = 0; i < nWrites - 1; i++) CdxTagIndex_Dispose(others[i]);
        return NULL;
    }
    for (i = 0; i < nWrites - 1; i++)
        CdxTagIndex_Dispose(others[i]);
    if (CdxCompound_ReadTagTable(compoundPath, table, GMB_CDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    memset(&created, 0, sizeof(created));
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0) {
            created = table[i];
            break;
        }
    }
    if (created.Name[0] == 0) return NULL;
    return cdx_open_desc(compoundPath, &created, 1);
}

CdxTagIndex* CdxTagIndex_Open(const char* compoundPath, const char* tagName, int writable)
{
    CdxTagDescriptor table[GMB_CDX_MAX_TAGS];
    int nTags = 0, i;
    char norm[16];
    if (!compoundPath || !tagName) return NULL;
    mdx_normalize_tag(tagName, norm, sizeof(norm));
    if (CdxCompound_ReadTagTable(compoundPath, table, GMB_CDX_MAX_TAGS, &nTags) != GMB_DB_OK)
        return NULL;
    for (i = 0; i < nTags; i++) {
        if (gmb_strcasecmp(table[i].Name, norm) == 0)
            return cdx_open_desc(compoundPath, &table[i], writable);
    }
    return NULL;
}

void CdxTagIndex_Dispose(CdxTagIndex* self)
{
    if (!self) return;
    cdx_free_entries(self);
    CdxPageCache_Dispose(self->PageCache);
    if (self->LockInited) pthread_mutex_destroy(&self->Lock);
    free(self);
}

const char* CdxTagIndex_get_Name(const CdxTagIndex* self)
{
    return self ? self->Desc.Name : NULL;
}

const char* CdxTagIndex_GetKeyExpressionString(const CdxTagIndex* self)
{
    return self ? self->Desc.Expression : NULL;
}

int CdxTagIndex_get_KeyLength(const CdxTagIndex* self)
{
    return self ? self->Desc.KeyLength : 0;
}

int CdxTagIndex_Seek(CdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    int exact, idx, rec;
    if (!self || !keyBytes) return -1;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->nEntries == 0)
        cdx_load_entries(self);
    if (self->nEntries == 0) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return -1;
    }
    idx = cdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (exact)
        rec = self->Entries[idx].Recno;
    else
        rec = (soft && idx < self->nEntries) ? self->Entries[idx].Recno : -1;
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rec;
}

void CdxTagIndex_EnablePageCache(CdxTagIndex* self)
{
    if (!self || !self->Writable) return;
    if (self->PageCache) return;
    self->PageCache = CdxPageCache_New(self->CompoundPath, 256);
    self->CachedRootOffset = self->Desc.RootOffset;
    cdx_free_entries(self);
}

static int cdx_update_tag_root(CdxTagIndex* self, uint32_t newRoot)
{
    uint8_t th[CdxFormat_PageSize];
    if (!self->PageCache || self->Desc.HeaderOffset == 0) return GMB_DB_ERR;
    if (CdxPageCache_GetPage(self->PageCache, self->Desc.HeaderOffset, th) != GMB_DB_OK)
        return GMB_DB_ERR;
    mdx_le32(th, newRoot);
    CdxPageCache_PutPage(self->PageCache, self->Desc.HeaderOffset, th, 1);
    if (CdxPageCache_Flush(self->PageCache) != GMB_DB_OK) return GMB_DB_ERR;
    self->CachedRootOffset = newRoot;
    self->Desc.RootOffset = newRoot;
    return GMB_DB_OK;
}

int CdxTagIndex_Insert(CdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, rc;
    uint8_t* copy;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes || keyLen < 0) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    if (self->PageCache) {
        uint32_t newRoot = 0;
        uint8_t padded[GMB_CDX_MAX_KEY];
        int kl = self->Desc.KeyLength;
        if (kl <= 0 || kl > GMB_CDX_MAX_KEY) {
            if (self->LockInited) pthread_mutex_unlock(&self->Lock);
            return GMB_DB_ARG;
        }
        cdx_pad_key(padded, kl, keyBytes, keyLen);
        rc = CdxBTreeBuilder_InsertEntry(self->PageCache, self->CachedRootOffset,
                                         padded, kl, recno, kl, self->Desc.Unique, &newRoot);
        if (rc == GMB_DB_OK)
            rc = cdx_update_tag_root(self, newRoot);
        if (rc == GMB_DB_OK)
            cdx_free_entries(self);
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return rc;
    }
    if (self->nEntries >= GMB_CDX_MAX_ENTRIES) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    idx = cdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (self->Desc.Unique && exact) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    copy = (uint8_t*)malloc((size_t)keyLen);
    if (!copy) {
        if (self->LockInited) pthread_mutex_unlock(&self->Lock);
        return GMB_DB_ERR;
    }
    if (keyLen > 0) memcpy(copy, keyBytes, (size_t)keyLen);
    memmove(&self->Entries[idx + 1], &self->Entries[idx], (size_t)(self->nEntries - idx) * sizeof(CdxEntry));
    self->Entries[idx].Key = copy;
    self->Entries[idx].KeyLen = keyLen;
    self->Entries[idx].Recno = recno;
    self->nEntries++;
    rc = cdx_rewrite(self);
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return rc;
}

int CdxTagIndex_Delete(CdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    int exact, idx, i;
    if (!self || !self->Writable) return GMB_DB_ERR;
    if (!keyBytes) return GMB_DB_ARG;
    if (self->LockInited) pthread_mutex_lock(&self->Lock);
    idx = cdx_find_key(self->Entries, self->nEntries, keyBytes, keyLen, &exact);
    if (exact) {
        for (i = idx; i < self->nEntries; i++) {
            if (mdx_keycmp(self->Entries[i].Key, self->Entries[i].KeyLen, keyBytes, keyLen) != 0)
                break;
            if (self->Entries[i].Recno == recno) {
                free(self->Entries[i].Key);
                memmove(&self->Entries[i], &self->Entries[i + 1],
                        (size_t)(self->nEntries - i - 1) * sizeof(CdxEntry));
                self->nEntries--;
                cdx_rewrite(self);
                break;
            }
        }
    }
    if (self->LockInited) pthread_mutex_unlock(&self->Lock);
    return GMB_DB_OK;
}

static void cdx_sc_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int CdxCompound_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/cdxsc_XXXXXX";
    char path[PATH_MAX];
    char path2[PATH_MAX];
    CdxTagIndex *nameIdx, *idIdx, *reopen, *many;
    CdxTagDescriptor table[4];
    int nTags = 0, i;
    uint8_t key[10];
    uint8_t head[16];
    FILE* f;
    long flen;

    if (!mkdtemp(dir)) return -2;
    snprintf(path, sizeof(path), "%s/t.cdx", dir);

    nameIdx = CdxTagIndex_CreateOrReplace(path, "name", "NAME", 10, 0, 0);
    if (!nameIdx) return -3;
    if (!CdxTagIndex_get_Name(nameIdx) || strcmp(CdxTagIndex_get_Name(nameIdx), "NAME") != 0) {
        CdxTagIndex_Dispose(nameIdx);
        return -4;
    }
    if (!CdxTagIndex_GetKeyExpressionString(nameIdx)
        || strcmp(CdxTagIndex_GetKeyExpressionString(nameIdx), "NAME") != 0) {
        CdxTagIndex_Dispose(nameIdx);
        return -5;
    }

    f = fopen(path, "rb");
    if (!f) {
        CdxTagIndex_Dispose(nameIdx);
        return -6;
    }
    if (fread(head, 1, 16, f) != 16) {
        fclose(f);
        CdxTagIndex_Dispose(nameIdx);
        return -7;
    }
    fseek(f, 0, SEEK_END);
    flen = ftell(f);
    fclose(f);
    if (head[14] != (CdxFormat_TypeCompact | CdxFormat_TypeCompound)
        || mdx_ru16(head + 12) != 10 || mdx_ru32(head) != 1536 || flen != 2048) {
        CdxTagIndex_Dispose(nameIdx);
        return -8;
    }

    if (CdxCompound_ReadTagTable(path, table, 4, &nTags) != GMB_DB_OK || nTags != 1
        || table[0].RootOffset != 1024 || table[0].KeyLength != 10 || table[0].Unique != 0) {
        CdxTagIndex_Dispose(nameIdx);
        return -9;
    }

    for (i = 0; i < 8; i++) {
        cdx_sc_key(key, i);
        if (CdxTagIndex_Insert(nameIdx, key, 10, 100 + i) != GMB_DB_OK) {
            CdxTagIndex_Dispose(nameIdx);
            return -10 - i;
        }
    }
    cdx_sc_key(key, 3);
    if (CdxTagIndex_Seek(nameIdx, key, 10, 0) != 103) {
        CdxTagIndex_Dispose(nameIdx);
        return -20;
    }
    CdxTagIndex_Dispose(nameIdx);

    idIdx = CdxTagIndex_CreateOrReplace(path, "id", "ID", 10, 1, 0);
    if (!idIdx) return -21;
    cdx_sc_key(key, 0);
    if (CdxTagIndex_Insert(idIdx, key, 10, 1) != GMB_DB_OK) {
        CdxTagIndex_Dispose(idIdx);
        return -22;
    }
    if (CdxTagIndex_Insert(idIdx, key, 10, 2) != GMB_DB_ERR) {
        CdxTagIndex_Dispose(idIdx);
        return -23;
    }
    CdxTagIndex_Dispose(idIdx);

    if (CdxCompound_ReadTagTable(path, table, 4, &nTags) != GMB_DB_OK || nTags != 2) return -24;

    reopen = CdxTagIndex_Open(path, "NAME", 0);
    if (!reopen) return -25;
    cdx_sc_key(key, 3);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 103) {
        CdxTagIndex_Dispose(reopen);
        return -26;
    }
    cdx_sc_key(key, 7);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 107) {
        CdxTagIndex_Dispose(reopen);
        return -27;
    }
    if (CdxTagIndex_Insert(reopen, key, 10, 1) != GMB_DB_ERR) {
        CdxTagIndex_Dispose(reopen);
        return -28;
    }
    CdxTagIndex_Dispose(reopen);

    reopen = CdxTagIndex_Open(path, "ID", 1);
    if (!reopen) return -29;
    cdx_sc_key(key, 0);
    if (CdxTagIndex_Seek(reopen, key, 10, 0) != 1) {
        CdxTagIndex_Dispose(reopen);
        return -30;
    }
    if (CdxTagIndex_Delete(reopen, key, 10, 1) != GMB_DB_OK
        || CdxTagIndex_Seek(reopen, key, 10, 0) != -1) {
        CdxTagIndex_Dispose(reopen);
        return -31;
    }
    CdxTagIndex_Dispose(reopen);

    snprintf(path2, sizeof(path2), "%s/many.cdx", dir);
    many = CdxTagIndex_CreateOrReplace(path2, "k", "K", 10, 0, 0);
    if (!many) return -32;
    for (i = 0; i < 40; i++) {
        cdx_sc_key(key, i);
        if (CdxTagIndex_Insert(many, key, 10, i + 1) != GMB_DB_OK) {
            CdxTagIndex_Dispose(many);
            return -33;
        }
    }
    CdxTagIndex_Dispose(many);
    many = CdxTagIndex_Open(path2, "K", 0);
    if (!many) return -34;
    cdx_sc_key(key, 0);
    if (CdxTagIndex_Seek(many, key, 10, 0) != 1) {
        CdxTagIndex_Dispose(many);
        return -35;
    }
    cdx_sc_key(key, 39);
    if (CdxTagIndex_Seek(many, key, 10, 0) != 40) {
        CdxTagIndex_Dispose(many);
        return -36;
    }
    cdx_sc_key(key, 20);
    if (CdxTagIndex_Seek(many, key, 10, 0) != 21) {
        CdxTagIndex_Dispose(many);
        return -37;
    }
    CdxTagIndex_Dispose(many);

    unlink(path);
    unlink(path2);
    rmdir(dir);
    return GMB_DB_OK;
}
