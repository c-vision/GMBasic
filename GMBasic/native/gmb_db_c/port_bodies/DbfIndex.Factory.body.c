#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <strings.h>

/* C#: Basic.DBF.IndexFactory — apre .ntx/.ndx/.mdx/.cdx in handle.
 * Clipper NTX (type 0x0003/0x0006): ClipperNtxWritable se writable, altrimenti ClipperNtxFile.
 * Catch → lista vuota. */

void IndexHandle_Dispose(IndexHandle* self)
{
    if (!self) return;
    switch (self->Kind) {
    case IndexHandleKind_Ntx:
        if (self->Ntx) NtxFile_Dispose(self->Ntx);
        break;
    case IndexHandleKind_Ndx:
        if (self->Ndx) NdxFile_Dispose(self->Ndx);
        break;
    case IndexHandleKind_MdxTag:
        if (self->Mdx) MdxTagIndex_Dispose(self->Mdx);
        break;
    case IndexHandleKind_CdxTag:
        if (self->Cdx) CdxTagIndex_Dispose(self->Cdx);
        break;
    case IndexHandleKind_ClipperNtx:
        if (self->Clipper) ClipperNtxFile_Dispose(self->Clipper);
        break;
    case IndexHandleKind_ClipperNtxWritable:
        if (self->ClipperW) ClipperNtxWritable_Dispose(self->ClipperW);
        break;
    default:
        break;
    }
    memset(self, 0, sizeof(*self));
}

const char* IndexHandle_get_Name(const IndexHandle* self)
{
    if (!self) return "";
    switch (self->Kind) {
    case IndexHandleKind_Ntx: return NtxFile_get_Name(self->Ntx);
    case IndexHandleKind_Ndx: return NdxFile_get_Name(self->Ndx);
    case IndexHandleKind_MdxTag: return MdxTagIndex_get_Name(self->Mdx);
    case IndexHandleKind_CdxTag: return CdxTagIndex_get_Name(self->Cdx);
    case IndexHandleKind_ClipperNtx: return ClipperNtxFile_get_Name(self->Clipper);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_get_Name(self->ClipperW);
    default: return "";
    }
}

int IndexHandle_get_KeyLength(const IndexHandle* self)
{
    if (!self) return 0;
    switch (self->Kind) {
    case IndexHandleKind_Ndx: return NdxFile_get_KeyLength(self->Ndx);
    case IndexHandleKind_MdxTag: return MdxTagIndex_get_KeyLength(self->Mdx);
    case IndexHandleKind_CdxTag: return CdxTagIndex_get_KeyLength(self->Cdx);
    case IndexHandleKind_ClipperNtx: return ClipperNtxFile_get_KeyLength(self->Clipper);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_get_KeyLength(self->ClipperW);
    default: return 0;
    }
}

int IndexHandle_Seek(IndexHandle* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    if (!self) return -1;
    switch (self->Kind) {
    case IndexHandleKind_Ntx: return NtxFile_Seek(self->Ntx, keyBytes, keyLen, soft);
    case IndexHandleKind_Ndx: return NdxFile_Seek(self->Ndx, keyBytes, keyLen, soft);
    case IndexHandleKind_MdxTag: return MdxTagIndex_Seek(self->Mdx, keyBytes, keyLen, soft);
    case IndexHandleKind_CdxTag: return CdxTagIndex_Seek(self->Cdx, keyBytes, keyLen, soft);
    case IndexHandleKind_ClipperNtx: return ClipperNtxFile_Seek(self->Clipper, keyBytes, keyLen, soft);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_Seek(self->ClipperW, keyBytes, keyLen, soft);
    default: return -1;
    }
}

int IndexHandle_Insert(IndexHandle* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    if (!self) return GMB_DB_ERR;
    switch (self->Kind) {
    case IndexHandleKind_Ntx:
        NtxFile_Insert(self->Ntx, keyBytes, keyLen, recno);
        return GMB_DB_OK;
    case IndexHandleKind_Ndx: return NdxFile_Insert(self->Ndx, keyBytes, keyLen, recno);
    case IndexHandleKind_MdxTag: return MdxTagIndex_Insert(self->Mdx, keyBytes, keyLen, recno);
    case IndexHandleKind_CdxTag: return CdxTagIndex_Insert(self->Cdx, keyBytes, keyLen, recno);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_Insert(self->ClipperW, keyBytes, keyLen, recno);
    default: return GMB_DB_ERR;
    }
}

int IndexHandle_GetFirstRecordNumber(IndexHandle* self)
{
    if (!self) return 0;
    switch (self->Kind) {
    case IndexHandleKind_Ndx: return NdxFile_GetFirstRecordNumber(self->Ndx);
    case IndexHandleKind_ClipperNtx: return ClipperNtxFile_GetFirstRecordNumber(self->Clipper);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_GetFirstRecordNumber(self->ClipperW);
    default: return 0;
    }
}

const char* IndexHandle_get_Path(const IndexHandle* self)
{
    return self ? self->Path : "";
}

const char* IndexHandle_GetKeyExpressionString(const IndexHandle* self)
{
    if (!self) return "";
    switch (self->Kind) {
    case IndexHandleKind_Ntx: return NtxFile_GetKeyExpressionString(self->Ntx);
    case IndexHandleKind_Ndx: return NdxFile_GetKeyExpressionString(self->Ndx);
    case IndexHandleKind_MdxTag: return MdxTagIndex_GetKeyExpressionString(self->Mdx);
    case IndexHandleKind_CdxTag: return CdxTagIndex_GetKeyExpressionString(self->Cdx);
    case IndexHandleKind_ClipperNtx: return ClipperNtxFile_GetKeyExpressionString(self->Clipper);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_GetKeyExpressionString(self->ClipperW);
    default: return "";
    }
}

int IndexHandle_get_Unique(const IndexHandle* self)
{
    if (!self) return 0;
    switch (self->Kind) {
    case IndexHandleKind_Ndx: return NdxFile_get_Unique(self->Ndx);
    case IndexHandleKind_ClipperNtx: return ClipperNtxFile_get_Unique(self->Clipper);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_get_Unique(self->ClipperW);
    default: return 0;
    }
}

int IndexHandle_Delete(IndexHandle* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    if (!self) return GMB_DB_ERR;
    switch (self->Kind) {
    case IndexHandleKind_Ndx: return NdxFile_Delete(self->Ndx, keyBytes, keyLen, recno);
    case IndexHandleKind_MdxTag: return MdxTagIndex_Delete(self->Mdx, keyBytes, keyLen, recno);
    case IndexHandleKind_CdxTag: return CdxTagIndex_Delete(self->Cdx, keyBytes, keyLen, recno);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_Delete(self->ClipperW, keyBytes, keyLen, recno);
    default: return GMB_DB_ERR;
    }
}

int IndexHandle_GetLastRecordNumber(IndexHandle* self)
{
    if (!self) return 0;
    switch (self->Kind) {
    case IndexHandleKind_Ndx: return NdxFile_GetLastRecordNumber(self->Ndx);
    case IndexHandleKind_ClipperNtx: return ClipperNtxFile_GetLastRecordNumber(self->Clipper);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_GetLastRecordNumber(self->ClipperW);
    default: return 0;
    }
}

int IndexHandle_GetEntryCount(IndexHandle* self)
{
    if (!self) return 0;
    switch (self->Kind) {
    case IndexHandleKind_Ndx: return NdxFile_get_EntryCount(self->Ndx);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_get_EntryCount(self->ClipperW);
    default: return 0;
    }
}

int IndexHandle_GetRecnoAt(IndexHandle* self, int index)
{
    if (!self) return 0;
    switch (self->Kind) {
    case IndexHandleKind_Ndx: return NdxFile_get_RecnoAt(self->Ndx, index);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_get_RecnoAt(self->ClipperW, index);
    default: return 0;
    }
}

static void idx_stamp_path(IndexHandle* h, const char* path)
{
    if (!h || !path) return;
    snprintf(h->Path, sizeof(h->Path), "%s", path);
}

static int idx_ext_is(const char* path, const char* want)
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

static int IndexFactory_CreateNtxHandle(const char* indexPath, int writable, IndexHandle* out)
{
    FILE* fs;
    long len;
    uint8_t typeBytes[2];
    uint16_t type;

    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    fs = fopen(indexPath, "rb");
    if (!fs) return 0;
    if (fseek(fs, 0, SEEK_END) != 0) {
        fclose(fs);
        return 0;
    }
    len = ftell(fs);
    if (len < ClipperNtxHeader_PageSize) {
        fclose(fs);
        return 0;
    }
    if (fseek(fs, 0, SEEK_SET) != 0 || fread(typeBytes, 1, 2, fs) != 2) {
        fclose(fs);
        return 0;
    }
    type = (uint16_t)(typeBytes[0] | (typeBytes[1] << 8));
    fclose(fs);

    if (ClipperNtxHeader_IsClipperType(type)) {
        if (writable) {
            out->Kind = IndexHandleKind_ClipperNtxWritable;
            out->ClipperW = ClipperNtxWritable_OpenExisting(indexPath);
            return out->ClipperW != NULL;
        }
        out->Kind = IndexHandleKind_ClipperNtx;
        out->Clipper = ClipperNtxFile_New(indexPath);
        return out->Clipper != NULL;
    }

    out->Kind = IndexHandleKind_Ntx;
    out->Ntx = NtxFile_New(indexPath, LockingStrategy_IntraProcess);
    return out->Ntx != NULL;
}

static void idx_dispose_n(IndexHandle* out, int n)
{
    int i;
    for (i = 0; i < n; i++) IndexHandle_Dispose(&out[i]);
}

int IndexFactory_CreateHandles(const char* indexPath, int writable, IndexHandle* out, int cap, int* nOut)
{
    int nTags, i;

    if (nOut) *nOut = 0;
    if (!indexPath || !out || !nOut || cap <= 0) return GMB_DB_OK;
    if (access(indexPath, F_OK) != 0) return GMB_DB_OK;

    if (idx_ext_is(indexPath, ".ntx")) {
        if (!IndexFactory_CreateNtxHandle(indexPath, writable, &out[0])) return GMB_DB_OK;
        idx_stamp_path(&out[0], indexPath);
        *nOut = 1;
        return GMB_DB_OK;
    }

    if (idx_ext_is(indexPath, ".ndx")) {
        NdxFile* h = NdxFile_OpenExisting(indexPath, writable);
        if (!h) return GMB_DB_OK;
        memset(&out[0], 0, sizeof(out[0]));
        out[0].Kind = IndexHandleKind_Ndx;
        out[0].Ndx = h;
        idx_stamp_path(&out[0], indexPath);
        *nOut = 1;
        return GMB_DB_OK;
    }

    if (idx_ext_is(indexPath, ".mdx")) {
        MdxTagDescriptor tags[GMB_MDX_MAX_TAGS];
        nTags = 0;
        if (MdxCompound_ReadTagTable(indexPath, tags, GMB_MDX_MAX_TAGS, &nTags) != GMB_DB_OK || nTags <= 0) {
            return GMB_DB_OK;
        }
        if (nTags > cap) return GMB_DB_OK;
        for (i = 0; i < nTags; i++) {
            MdxTagIndex* h = MdxTagIndex_Open(indexPath, tags[i].Name, writable);
            if (!h) {
                idx_dispose_n(out, i);
                *nOut = 0;
                return GMB_DB_OK;
            }
            memset(&out[i], 0, sizeof(out[i]));
            out[i].Kind = IndexHandleKind_MdxTag;
            out[i].Mdx = h;
            idx_stamp_path(&out[i], indexPath);
        }
        *nOut = nTags;
        return GMB_DB_OK;
    }

    if (idx_ext_is(indexPath, ".cdx")) {
        CdxTagDescriptor tags[GMB_INDEX_FACTORY_MAX_HANDLES];
        nTags = 0;
        if (CdxCompound_ReadTagTable(indexPath, tags, GMB_INDEX_FACTORY_MAX_HANDLES, &nTags) != GMB_DB_OK
            || nTags <= 0) {
            return GMB_DB_OK;
        }
        if (nTags > cap) return GMB_DB_OK;
        for (i = 0; i < nTags; i++) {
            CdxTagIndex* h = CdxTagIndex_Open(indexPath, tags[i].Name, writable);
            if (!h) {
                idx_dispose_n(out, i);
                *nOut = 0;
                return GMB_DB_OK;
            }
            memset(&out[i], 0, sizeof(out[i]));
            out[i].Kind = IndexHandleKind_CdxTag;
            out[i].Cdx = h;
            idx_stamp_path(&out[i], indexPath);
        }
        *nOut = nTags;
        return GMB_DB_OK;
    }

    return GMB_DB_OK;
}

int IndexFactory_Create(const char* indexPath, int writable, IndexHandle* out)
{
    IndexHandle tmp[GMB_INDEX_FACTORY_MAX_HANDLES];
    int n = 0, i;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (IndexFactory_CreateHandles(indexPath, writable, tmp, GMB_INDEX_FACTORY_MAX_HANDLES, &n) != GMB_DB_OK
        || n <= 0) {
        return 0;
    }
    *out = tmp[0];
    for (i = 1; i < n; i++) IndexHandle_Dispose(&tmp[i]);
    return 1;
}

static void idxfact_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

static int idxfact_seek(IndexHandle* h, const uint8_t* key, int keyLen, int soft)
{
    switch (h->Kind) {
    case IndexHandleKind_Ntx: return NtxFile_Seek(h->Ntx, key, keyLen, soft);
    case IndexHandleKind_Ndx: return NdxFile_Seek(h->Ndx, key, keyLen, soft);
    case IndexHandleKind_MdxTag: return MdxTagIndex_Seek(h->Mdx, key, keyLen, soft);
    case IndexHandleKind_CdxTag: return CdxTagIndex_Seek(h->Cdx, key, keyLen, soft);
    case IndexHandleKind_ClipperNtx: return ClipperNtxFile_Seek(h->Clipper, key, keyLen, soft);
    case IndexHandleKind_ClipperNtxWritable: return ClipperNtxWritable_Seek(h->ClipperW, key, keyLen, soft);
    default: return -1;
    }
}

int IndexFactory_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/idxfsc_XXXXXX";
    char path[PATH_MAX];
    IndexHandle handles[GMB_INDEX_FACTORY_MAX_HANDLES];
    IndexHandle first;
    int n = 0;
    uint8_t key[10];
    FILE* f;
    NdxFile* ndx;
    NtxFile* ntx;
    MdxTagIndex* mdx;
    CdxTagIndex* cdx;
    uint8_t clipper[ClipperNtxHeader_PageSize];

    if (!mkdtemp(dir)) return -2;

    /* missing → [] */
    snprintf(path, sizeof(path), "%s/missing.ndx", dir);
    if (IndexFactory_CreateHandles(path, 0, handles, GMB_INDEX_FACTORY_MAX_HANDLES, &n) != GMB_DB_OK || n != 0) {
        return -3;
    }

    /* unknown extension → [] */
    snprintf(path, sizeof(path), "%s/t.xyz", dir);
    f = fopen(path, "wb");
    if (!f) return -4;
    fwrite("xx", 1, 2, f);
    fclose(f);
    if (IndexFactory_CreateHandles(path, 0, handles, GMB_INDEX_FACTORY_MAX_HANDLES, &n) != GMB_DB_OK || n != 0) {
        return -5;
    }

    /* Clipper NTX type 0x0006 → ClipperNtxFile (RootBlock=0, Seek -1) */
    snprintf(path, sizeof(path), "%s/clip.ntx", dir);
    memset(clipper, 0, sizeof(clipper));
    clipper[0] = 0x06;
    clipper[1] = 0x00;
    f = fopen(path, "wb");
    if (!f) return -6;
    if (fwrite(clipper, 1, sizeof(clipper), f) != sizeof(clipper)) {
        fclose(f);
        return -7;
    }
    fclose(f);
    if (!ClipperNtxHeader_IsClipperType(0x0006) || !ClipperNtxHeader_IsClipperType(0x0003)
        || ClipperNtxHeader_IsClipperType(0x0001)) {
        return -8;
    }
    if (IndexFactory_CreateHandles(path, 0, handles, GMB_INDEX_FACTORY_MAX_HANDLES, &n) != GMB_DB_OK
        || n != 1 || handles[0].Kind != IndexHandleKind_ClipperNtx) {
        return -9;
    }
    IndexHandle_Dispose(&handles[0]);

    /* NTX troppo corto → [] */
    snprintf(path, sizeof(path), "%s/short.ntx", dir);
    f = fopen(path, "wb");
    if (!f) return -10;
    fwrite("ab", 1, 2, f);
    fclose(f);
    if (IndexFactory_CreateHandles(path, 0, handles, GMB_INDEX_FACTORY_MAX_HANDLES, &n) != GMB_DB_OK || n != 0) {
        return -11;
    }

    /* NDX */
    snprintf(path, sizeof(path), "%s/t.ndx", dir);
    ndx = NdxFile_CreateNew(path, "NAME", 10, 0);
    if (!ndx) return -12;
    idxfact_key(key, 0);
    if (NdxFile_Insert(ndx, key, 10, 7) != GMB_DB_OK) {
        NdxFile_Dispose(ndx);
        return -13;
    }
    NdxFile_Dispose(ndx);
    if (IndexFactory_CreateHandles(path, 0, handles, GMB_INDEX_FACTORY_MAX_HANDLES, &n) != GMB_DB_OK
        || n != 1 || handles[0].Kind != IndexHandleKind_Ndx) {
        return -14;
    }
    if (idxfact_seek(&handles[0], key, 10, 0) != 7) {
        IndexHandle_Dispose(&handles[0]);
        return -15;
    }
    IndexHandle_Dispose(&handles[0]);
    if (!IndexFactory_Create(path, 0, &first) || first.Kind != IndexHandleKind_Ndx) return -16;
    IndexHandle_Dispose(&first);

    /* GM NTX */
    snprintf(path, sizeof(path), "%s/t.ntx", dir);
    ntx = NtxFile_CreateNew(path, "NAME", 10, 0, LockingStrategy_IntraProcess);
    if (!ntx) return -17;
    idxfact_key(key, 1);
    NtxFile_Insert(ntx, key, 10, 11);
    NtxFile_Dispose(ntx);
    if (IndexFactory_CreateHandles(path, 0, handles, GMB_INDEX_FACTORY_MAX_HANDLES, &n) != GMB_DB_OK
        || n != 1 || handles[0].Kind != IndexHandleKind_Ntx) {
        return -18;
    }
    if (idxfact_seek(&handles[0], key, 10, 0) != 11) {
        IndexHandle_Dispose(&handles[0]);
        return -19;
    }
    IndexHandle_Dispose(&handles[0]);

    /* MDX two tags */
    snprintf(path, sizeof(path), "%s/t.mdx", dir);
    mdx = MdxTagIndex_CreateOrReplace(path, "CUSTOMER", "name", "NAME", 10, 0);
    if (!mdx) return -20;
    idxfact_key(key, 2);
    if (MdxTagIndex_Insert(mdx, key, 10, 22) != GMB_DB_OK) {
        MdxTagIndex_Dispose(mdx);
        return -21;
    }
    MdxTagIndex_Dispose(mdx);
    mdx = MdxTagIndex_CreateOrReplace(path, "CUSTOMER", "id", "ID", 10, 0);
    if (!mdx) return -22;
    MdxTagIndex_Dispose(mdx);
    if (IndexFactory_CreateHandles(path, 0, handles, GMB_INDEX_FACTORY_MAX_HANDLES, &n) != GMB_DB_OK || n != 2) {
        return -23;
    }
    if (handles[0].Kind != IndexHandleKind_MdxTag || handles[1].Kind != IndexHandleKind_MdxTag) {
        idx_dispose_n(handles, n);
        return -24;
    }
    if (!MdxTagIndex_get_Name(handles[0].Mdx) || !MdxTagIndex_get_Name(handles[1].Mdx)) {
        idx_dispose_n(handles, n);
        return -25;
    }
    idxfact_key(key, 2);
    if (idxfact_seek(&handles[0], key, 10, 0) != 22 && idxfact_seek(&handles[1], key, 10, 0) != 22) {
        idx_dispose_n(handles, n);
        return -26;
    }
    if (!IndexFactory_Create(path, 0, &first) || first.Kind != IndexHandleKind_MdxTag) {
        idx_dispose_n(handles, n);
        return -27;
    }
    IndexHandle_Dispose(&first);
    idx_dispose_n(handles, n);

    /* CDX */
    snprintf(path, sizeof(path), "%s/t.cdx", dir);
    cdx = CdxTagIndex_CreateOrReplace(path, "NAME", "NAME", 10, 0, 0);
    if (!cdx) return -28;
    idxfact_key(key, 3);
    if (CdxTagIndex_Insert(cdx, key, 10, 33) != GMB_DB_OK) {
        CdxTagIndex_Dispose(cdx);
        return -29;
    }
    CdxTagIndex_Dispose(cdx);
    if (IndexFactory_CreateHandles(path, 0, handles, GMB_INDEX_FACTORY_MAX_HANDLES, &n) != GMB_DB_OK
        || n != 1 || handles[0].Kind != IndexHandleKind_CdxTag) {
        return -30;
    }
    if (idxfact_seek(&handles[0], key, 10, 0) != 33) {
        IndexHandle_Dispose(&handles[0]);
        return -31;
    }
    IndexHandle_Dispose(&handles[0]);

    return GMB_DB_OK;
}
