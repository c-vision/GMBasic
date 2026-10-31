static int rd_i32_le(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}

static int rd_i64_le(FILE* f, int64_t* v)
{
    uint8_t b[8];
    uint64_t u = 0;
    int i;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)b[i] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

typedef struct {
    uint8_t* key;
    int keyLen;
    int64_t offset;
} SstBlockIndexEntry;

struct SstReader {
    FILE* stream;
    char FilePath[1024];
    int64_t indexOffset;
    int64_t bloomOffset;
    BloomFilter* bloom;
    SstBlockIndexEntry* index;
    int indexN;
};

static int64_t SstReader_DataEndOffset(const SstReader* self)
{
    return self->bloomOffset > 0 ? self->bloomOffset : self->indexOffset;
}

static int SstReader_FindBlockIndexForKey(const SstReader* self, const uint8_t* key, int keyLen)
{
    int low = 0, high = self->indexN - 1, result = -1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (ByteArrayComparer_Compare(self->index[mid].key, self->index[mid].keyLen, key, keyLen) <= 0) {
            result = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return result;
}

static void SstReader_FreeIndex(SstReader* self)
{
    int i;
    if (!self->index) return;
    for (i = 0; i < self->indexN; i++) free(self->index[i].key);
    free(self->index);
    self->index = NULL;
    self->indexN = 0;
}

SstReader* SstReader_Open(const char* filePath)
{
    SstReader* self;
    int32_t indexEntryCount, i;
    int hasV2 = 0;
    if (!filePath) return NULL;
    self = (SstReader*)calloc(1, sizeof(*self));
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", filePath);
    self->stream = fopen(filePath, "rb");
    if (!self->stream) { free(self); return NULL; }
    if (SstFooter_Read(self->stream, &self->indexOffset, &self->bloomOffset, &hasV2) != GMB_DB_OK) {
        fclose(self->stream);
        free(self);
        return NULL;
    }
    if (self->bloomOffset > 0) {
        if (fseek(self->stream, (long)self->bloomOffset, SEEK_SET) != 0) {
            fclose(self->stream);
            free(self);
            return NULL;
        }
        self->bloom = BloomFilter_ReadFrom(self->stream);
        if (!self->bloom) {
            fclose(self->stream);
            free(self);
            return NULL;
        }
    }
    if (fseek(self->stream, (long)self->indexOffset, SEEK_SET) != 0
        || rd_i32_le(self->stream, &indexEntryCount) != 0 || indexEntryCount < 0) {
        BloomFilter_Dispose(self->bloom);
        fclose(self->stream);
        free(self);
        return NULL;
    }
    self->indexN = indexEntryCount;
    if (indexEntryCount > 0) {
        self->index = (SstBlockIndexEntry*)calloc((size_t)indexEntryCount, sizeof(*self->index));
        for (i = 0; i < indexEntryCount; i++) {
            int32_t keyLength;
            if (rd_i32_le(self->stream, &keyLength) != 0 || keyLength < 0) {
                SstReader_Dispose(self);
                return NULL;
            }
            self->index[i].keyLen = keyLength;
            self->index[i].key = (uint8_t*)malloc((size_t)keyLength);
            if (keyLength > 0 && fread(self->index[i].key, 1, (size_t)keyLength, self->stream) != (size_t)keyLength) {
                SstReader_Dispose(self);
                return NULL;
            }
            if (rd_i64_le(self->stream, &self->index[i].offset) != 0) {
                SstReader_Dispose(self);
                return NULL;
            }
        }
    }
    return self;
}

void SstReader_Dispose(SstReader* self)
{
    if (!self) return;
    if (self->stream) fclose(self->stream);
    BloomFilter_Dispose(self->bloom);
    SstReader_FreeIndex(self);
    free(self);
}

const char* SstReader_GetFilePath(const SstReader* self)
{
    return self ? self->FilePath : "";
}

int SstReader_HasBloomFilter(const SstReader* self)
{
    return self && self->bloom != NULL;
}

int SstReader_TryFind(SstReader* self, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen)
{
    int blockIndex;
    int64_t blockStart, blockEnd;
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!self || !self->stream || !key) return 0;
    if (self->bloom && !BloomFilter_MayContain(self->bloom, key, keyLen)) return 0;
    blockIndex = SstReader_FindBlockIndexForKey(self, key, keyLen);
    if (blockIndex < 0) return 0;
    blockStart = self->index[blockIndex].offset;
    blockEnd = (blockIndex + 1 < self->indexN)
        ? self->index[blockIndex + 1].offset
        : SstReader_DataEndOffset(self);
    if (fseek(self->stream, (long)blockStart, SEEK_SET) != 0) return 0;
    while (ftell(self->stream) < blockEnd) {
        int32_t currentKeyLength, cmp;
        uint8_t* currentKey;
        uint8_t exists;
        if (rd_i32_le(self->stream, &currentKeyLength) != 0 || currentKeyLength < 0) return 0;
        currentKey = (uint8_t*)malloc((size_t)currentKeyLength);
        if (currentKeyLength > 0 && fread(currentKey, 1, (size_t)currentKeyLength, self->stream) != (size_t)currentKeyLength) {
            free(currentKey);
            return 0;
        }
        cmp = ByteArrayComparer_Compare(currentKey, currentKeyLength, key, keyLen);
        free(currentKey);
        if (cmp == 0) {
            if (fread(&exists, 1, 1, self->stream) != 1) return 0;
            if (exists) {
                int32_t vlen;
                uint8_t* buf;
                if (rd_i32_le(self->stream, &vlen) != 0 || vlen < 0) return 0;
                buf = (uint8_t*)malloc((size_t)vlen);
                if (vlen > 0 && fread(buf, 1, (size_t)vlen, self->stream) != (size_t)vlen) {
                    free(buf);
                    return 0;
                }
                if (value) *value = buf;
                else free(buf);
                if (valueLen) *valueLen = vlen;
            }
            return 1;
        }
        if (cmp > 0) return 0;
        if (fread(&exists, 1, 1, self->stream) != 1) return 0;
        if (exists) {
            int32_t skipLen;
            if (rd_i32_le(self->stream, &skipLen) != 0 || skipLen < 0) return 0;
            if (fseek(self->stream, (long)skipLen, SEEK_CUR) != 0) return 0;
        }
    }
    return 0;
}

int SstReader_TryValidateFooter(const char* filePath, char* error, int errorCap)
{
    FILE* stream;
    int64_t indexOffset = 0, bloomOffset = 0, streamLen;
    int hasV2 = 0;
    int32_t indexEntryCount, i;
    BloomFilter* bloom;
    if (error && errorCap) error[0] = 0;
    if (!filePath) {
        if (error) snprintf(error, (size_t)errorCap, "file too small");
        return 0;
    }
    stream = fopen(filePath, "rb");
    if (!stream) {
        if (error) snprintf(error, (size_t)errorCap, "open failed");
        return 0;
    }
    fseek(stream, 0, SEEK_END);
    streamLen = ftell(stream);
    if (streamLen < (int64_t)sizeof(int64_t)) {
        fclose(stream);
        if (error) snprintf(error, (size_t)errorCap, "file too small");
        return 0;
    }
    if (SstFooter_Read(stream, &indexOffset, &bloomOffset, &hasV2) != GMB_DB_OK
        || !SstFooter_TryValidateOffsets(streamLen, indexOffset, bloomOffset, hasV2, error, errorCap)) {
        fclose(stream);
        return 0;
    }
    if (bloomOffset > 0) {
        if (fseek(stream, (long)bloomOffset, SEEK_SET) != 0) {
            fclose(stream);
            if (error) snprintf(error, (size_t)errorCap, "bloom seek failed");
            return 0;
        }
        bloom = BloomFilter_ReadFrom(stream);
        if (!bloom) {
            fclose(stream);
            if (error) snprintf(error, (size_t)errorCap, "invalid bloom");
            return 0;
        }
        BloomFilter_Dispose(bloom);
    }
    if (fseek(stream, (long)indexOffset, SEEK_SET) != 0 || rd_i32_le(stream, &indexEntryCount) != 0) {
        fclose(stream);
        if (error) snprintf(error, (size_t)errorCap, "invalid index entry count");
        return 0;
    }
    if (indexEntryCount < 0) {
        fclose(stream);
        if (error) snprintf(error, (size_t)errorCap, "invalid index entry count");
        return 0;
    }
    for (i = 0; i < indexEntryCount; i++) {
        int32_t keyLength;
        long pos;
        if (rd_i32_le(stream, &keyLength) != 0) {
            fclose(stream);
            if (error) snprintf(error, (size_t)errorCap, "invalid index entry %d", i);
            return 0;
        }
        pos = ftell(stream);
        if (keyLength < 0 || (int64_t)pos + keyLength + (int64_t)sizeof(int64_t) > streamLen) {
            fclose(stream);
            if (error) snprintf(error, (size_t)errorCap, "invalid index entry %d", i);
            return 0;
        }
        if (fseek(stream, (long)keyLength + (long)sizeof(int64_t), SEEK_CUR) != 0) {
            fclose(stream);
            if (error) snprintf(error, (size_t)errorCap, "invalid index entry %d", i);
            return 0;
        }
    }
    fclose(stream);
    return 1;
}

int SstReader_Rewind(SstReader* self)
{
    if (!self || !self->stream) return GMB_DB_ERR;
    clearerr(self->stream);
    return fseek(self->stream, 0, SEEK_SET) == 0 ? GMB_DB_OK : GMB_DB_ERR;
}

int SstReader_Next(SstReader* self, uint8_t** key, int* keyLen, uint8_t** value, int* valueLen)
{
    int64_t end;
    int32_t klen;
    uint8_t exists;
    uint8_t* kbuf;
    if (key) *key = NULL;
    if (keyLen) *keyLen = 0;
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!self || !self->stream) return 0;
    end = SstReader_DataEndOffset(self);
    if (ftell(self->stream) >= end) return 0;
    if (rd_i32_le(self->stream, &klen) != 0 || klen < 0) return 0;
    kbuf = (uint8_t*)malloc((size_t)klen);
    if (klen > 0 && fread(kbuf, 1, (size_t)klen, self->stream) != (size_t)klen) {
        free(kbuf);
        return 0;
    }
    if (fread(&exists, 1, 1, self->stream) != 1) { free(kbuf); return 0; }
    if (key) *key = kbuf;
    else free(kbuf);
    if (keyLen) *keyLen = klen;
    if (exists) {
        int32_t vlen;
        uint8_t* vbuf;
        if (rd_i32_le(self->stream, &vlen) != 0 || vlen < 0) return 0;
        vbuf = (uint8_t*)malloc((size_t)vlen);
        if (vlen > 0 && fread(vbuf, 1, (size_t)vlen, self->stream) != (size_t)vlen) {
            free(vbuf);
            return 0;
        }
        if (value) *value = vbuf;
        else free(vbuf);
        if (valueLen) *valueLen = vlen;
    }
    return 1;
}

int SstReader_SelfCheck(void)
{
    const char* path = "native/gmb_db_c/build/sst_selfcheck.sst";
    Memtable* mem;
    SstReader* r;
    int i, vlen, found;
    uint8_t *val, key[8], value[8], miss[8];
    char err[128];

    mem = Memtable_New();
    if (!mem) return -1;
    for (i = 0; i < 500; i++) {
        int n = snprintf((char*)key, sizeof(key), "k%04d", i);
        int m = snprintf((char*)value, sizeof(value), "v%04d", i);
        if (Memtable_Set(mem, key, n, value, m) != GMB_DB_OK) { Memtable_Dispose(mem); return -2; }
    }
    if (Memtable_Delete(mem, (const uint8_t*)"k0100", 5) != GMB_DB_OK) { Memtable_Dispose(mem); return -3; }
    if (SstWriter_WriteFromMemtable(path, mem) != GMB_DB_OK) { Memtable_Dispose(mem); return -4; }
    Memtable_Dispose(mem);

    if (!SstReader_TryValidateFooter(path, err, (int)sizeof(err))) return -5;
    r = SstReader_Open(path);
    if (!r) return -6;
    if (!SstReader_HasBloomFilter(r)) { SstReader_Dispose(r); return -7; }

    for (i = 0; i < 500; i++) {
        int n = snprintf((char*)key, sizeof(key), "k%04d", i);
        uint8_t expect[8];
        int m = snprintf((char*)expect, sizeof(expect), "v%04d", i);
        found = SstReader_TryFind(r, key, n, &val, &vlen);
        if (i == 100) {
            if (!found || val != NULL || vlen != 0) { free(val); SstReader_Dispose(r); return -10; }
            continue;
        }
        if (!found || !val || vlen != m || memcmp(val, expect, (size_t)m) != 0) {
            free(val);
            SstReader_Dispose(r);
            return -20 - (i % 10);
        }
        free(val);
    }
    memcpy(miss, "zzmiss!", 7);
    if (SstReader_TryFind(r, miss, 7, &val, &vlen)) { free(val); SstReader_Dispose(r); return -8; }
    if (SstReader_Rewind(r) != GMB_DB_OK) { SstReader_Dispose(r); return -12; }
    {
        int nnext = 0, foundTomb = 0;
        for (;;) {
            uint8_t *nk = NULL, *nv = NULL;
            int nkl = 0, nvl = 0;
            if (!SstReader_Next(r, &nk, &nkl, &nv, &nvl)) break;
            nnext++;
            if (nkl == 5 && memcmp(nk, "k0100", 5) == 0) {
                if (nv != NULL) { free(nk); free(nv); SstReader_Dispose(r); return -13; }
                foundTomb = 1;
            }
            free(nk);
            free(nv);
        }
        if (nnext != 500 || !foundTomb) { SstReader_Dispose(r); return -14; }
    }
    SstReader_Dispose(r);
    r = SstReader_Open(path);
    if (!r) return -9;
    found = SstReader_TryFind(r, (const uint8_t*)"k0000", 5, &val, &vlen);
    if (!found || vlen != 5 || memcmp(val, "v0000", 5) != 0) { free(val); SstReader_Dispose(r); return -11; }
    free(val);
    SstReader_Dispose(r);
    return GMB_DB_OK;
}
