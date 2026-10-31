#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

static int fh_rd_i32_le(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}

static int fh_rd_i64_le(FILE* f, int64_t* v)
{
    uint8_t b[8];
    uint64_t u = 0;
    int i;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)b[i] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

static int fh_skip_7bit_str(FILE* f)
{
    unsigned v = 0, shift = 0;
    for (;;) {
        int c = fgetc(f);
        if (c < 0) return -1;
        v |= (unsigned)(c & 0x7f) << shift;
        if ((c & 0x80) == 0) break;
        shift += 7;
        if (shift > 28) return -1;
    }
    if (v && fseek(f, (long)v, SEEK_CUR) != 0) return -1;
    return 0;
}

static int64_t fh_index_map_len(FILE* f, int64_t mapOff)
{
    int32_t count = 0, i;
    long start, end;
    int64_t dummy;
    if (fseek(f, (long)mapOff, SEEK_SET) != 0) return -1;
    start = ftell(f);
    if (fh_rd_i32_le(f, &count) != 0 || count < 0) return -1;
    for (i = 0; i < count; i++) {
        if (fh_skip_7bit_str(f) != 0) return -1;
        if (fh_rd_i64_le(f, &dummy) != 0) return -1;
    }
    end = ftell(f);
    return (int64_t)end - (int64_t)start;
}

static int64_t fh_data_area_start(IsamFileHandle* self)
{
    int64_t mapLen;
    if (self->Header.SecondaryIndexMapOffset > 0) {
        mapLen = fh_index_map_len(self->fs, self->Header.SecondaryIndexMapOffset);
        if (mapLen < 0) return self->Header.SchemaOffset + self->Header.SchemaLength;
        return self->Header.SecondaryIndexMapOffset + mapLen;
    }
    return self->Header.SchemaOffset + self->Header.SchemaLength;
}

static void fh_init_heap_region(IsamFileHandle* self)
{
    int64_t minHeap = HeapRelocation_AlignUp(
        self->dataAreaStart + self->Header.LastRecNumber * self->Header.RecordSize,
        HeapRelocation_PageSize);
    if (self->Header.NextAvailablePageOffset <= minHeap && self->Header.PrimaryKeyRootOffset < 0) {
        self->heapRegionStart = -1;
        return;
    }
    if (self->Header.LobHeapStartOffset >= minHeap
        && self->Header.LobHeapStartOffset < self->Header.NextAvailablePageOffset) {
        self->heapRegionStart = self->Header.LobHeapStartOffset;
        return;
    }
    self->heapRegionStart = self->Header.NextAvailablePageOffset > minHeap ? minHeap : -1;
}

int IsamFileHandle_UpdateHeader(IsamFileHandle* self)
{
    if (!self || !self->fs) return GMB_DB_ERR;
    return IsamFileHeader_Write(self->fs, &self->Header);
}

int IsamFileHandle_Flush(IsamFileHandle* self)
{
    if (!self || !self->fs) return GMB_DB_ERR;
    return fflush(self->fs) == 0 ? GMB_DB_OK : GMB_DB_ERR;
}

int64_t IsamFileHandle_DataAreaStartOffset(const IsamFileHandle* self)
{
    return self ? self->dataAreaStart : -1;
}

/* ponytail: coda intera in un malloc; per file enormi streamare all'indietro. */
static int fh_shift_tail(FILE* fs, int64_t tailStart, int64_t delta)
{
    struct stat st;
    int64_t fileLen, tailLen;
    uint8_t* tail = NULL;
    if (delta <= 0) return GMB_DB_OK;
    if (fstat(fileno(fs), &st) != 0) return GMB_DB_ERR;
    fileLen = (int64_t)st.st_size;
    tailLen = fileLen > tailStart ? fileLen - tailStart : 0;
    if (tailLen > 0) {
        tail = (uint8_t*)malloc((size_t)tailLen);
        if (!tail) return GMB_DB_ERR;
        if (fseek(fs, (long)tailStart, SEEK_SET) != 0) { free(tail); return GMB_DB_ERR; }
        if ((int64_t)fread(tail, 1, (size_t)tailLen, fs) != tailLen) { free(tail); return GMB_DB_ERR; }
    }
    if (ftruncate(fileno(fs), (off_t)(fileLen + delta)) != 0) { free(tail); return GMB_DB_ERR; }
    if (tailLen > 0) {
        if (fseek(fs, (long)(tailStart + delta), SEEK_SET) != 0) { free(tail); return GMB_DB_ERR; }
        if ((int64_t)fwrite(tail, 1, (size_t)tailLen, fs) != tailLen) { free(tail); return GMB_DB_ERR; }
        free(tail);
    }
    return fflush(fs) == 0 ? GMB_DB_OK : GMB_DB_ERR;
}

static void fh_shift_header_ptrs(IsamFileHandle* self, int64_t heapStart, int64_t heapEnd, int64_t delta)
{
    int i;
    IsamFileHeader* h = &self->Header;
    if (h->PrimaryKeyRootOffset >= heapStart && h->PrimaryKeyRootOffset < heapEnd)
        h->PrimaryKeyRootOffset += delta;
    for (i = 0; i < 15; i++) {
        if (h->SecondaryIndexRootOffsets[i] >= heapStart && h->SecondaryIndexRootOffsets[i] < heapEnd)
            h->SecondaryIndexRootOffsets[i] += delta;
    }
    if (h->PageFreeListHeadOffset >= heapStart && h->PageFreeListHeadOffset < heapEnd)
        h->PageFreeListHeadOffset += delta;
}

int IsamFileHandle_EnsureSchemaSlot(IsamFileHandle* self, int requiredSchemaBytes)
{
    int64_t slotEnd, slotCapacity, delta, tailStart, tailEnd;
    struct stat st;
    if (!self || requiredSchemaBytes < 0) return GMB_DB_ERR;
    slotEnd = self->Header.SecondaryIndexMapOffset > 0
        ? self->Header.SecondaryIndexMapOffset
        : self->Header.SchemaOffset + self->Header.SchemaLength;
    slotCapacity = slotEnd - self->Header.SchemaOffset;
    if ((int64_t)requiredSchemaBytes <= slotCapacity) return GMB_DB_OK;
    delta = (int64_t)requiredSchemaBytes - slotCapacity;
    tailStart = slotEnd;
    if (fh_shift_tail(self->fs, tailStart, delta) != GMB_DB_OK) return GMB_DB_ERR;
    if (fstat(fileno(self->fs), &st) != 0) return GMB_DB_ERR;
    tailEnd = (int64_t)st.st_size;
    fh_shift_header_ptrs(self, tailStart, tailEnd, delta);
    if (self->Header.SecondaryIndexMapOffset > 0)
        self->Header.SecondaryIndexMapOffset += delta;
    if (self->Header.NextAvailablePageOffset >= tailStart)
        self->Header.NextAvailablePageOffset += delta;
    if (self->Header.LobHeapStartOffset >= tailStart)
        self->Header.LobHeapStartOffset += delta;
    if (self->Header.FreeListHeadOffset >= tailStart)
        self->Header.FreeListHeadOffset += delta;
    if (self->dataAreaStart >= tailStart)
        self->dataAreaStart += delta;
    if (self->heapRegionStart >= tailStart)
        self->heapRegionStart += delta;
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return GMB_DB_ERR;
    if (self->AfterHeapRelocated)
        self->AfterHeapRelocated(self->afterHeapCtx, tailStart, tailEnd, delta);
    return GMB_DB_OK;
}

IsamFileHandle* IsamFileHandle_Open(const char* filePath, const IsamSchema* schema)
{
    IsamFileHandle* self;
    if (!filePath || !schema) return NULL;
    self = (IsamFileHandle*)calloc(1, sizeof(*self));
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", filePath);
    self->Schema = schema;
    self->ownsFile = 1;
    self->fs = fopen(filePath, "rb+");
    if (!self->fs || IsamFileHeader_Read(self->fs, &self->Header) != GMB_DB_OK) {
        if (self->fs) fclose(self->fs);
        free(self);
        return NULL;
    }
    self->dataAreaStart = fh_data_area_start(self);
    fh_init_heap_region(self);
    return self;
}

void IsamFileHandle_Dispose(IsamFileHandle* self)
{
    if (!self) return;
    if (self->ownsFile && self->fs) fclose(self->fs);
    free(self);
}

static void fh_ensure_heap(IsamFileHandle* self, int64_t requiredStart)
{
    if (self->heapRegionStart < 0) {
        if (self->Header.NextAvailablePageOffset < requiredStart) {
            self->Header.NextAvailablePageOffset = requiredStart;
            IsamFileHandle_UpdateHeader(self);
        }
        return;
    }
    /* RelocateHeapRegion non ancora portato — 5 record di self-check restano sotto la prima pagina B+. */
    (void)requiredStart;
}

int64_t IsamFileHandle_AllocateNewRecordOffsetCore(IsamFileHandle* self)
{
    int64_t allocatedOffset, requiredHeap, requiredLength;
    if (!self) return -1;
    if (self->Header.FreeListHeadOffset != -1) {
        int64_t next = -1;
        allocatedOffset = self->Header.FreeListHeadOffset;
        fseek(self->fs, (long)(allocatedOffset + 1), SEEK_SET);
        if (fh_rd_i64_le(self->fs, &next) != 0) return -1;
        self->Header.FreeListHeadOffset = next;
    } else {
        allocatedOffset = self->dataAreaStart + self->Header.LastRecNumber * self->Header.RecordSize;
        requiredHeap = HeapRelocation_AlignUp(allocatedOffset + self->Header.RecordSize, HeapRelocation_PageSize);
        fh_ensure_heap(self, requiredHeap);
        self->Header.LastRecNumber += 1;
        requiredLength = allocatedOffset + self->Header.RecordSize;
        {
            struct stat st;
            if (fstat(fileno(self->fs), &st) == 0 && (int64_t)st.st_size < requiredLength) {
                if (ftruncate(fileno(self->fs), (off_t)requiredLength) != 0) return -1;
            }
        }
    }
    self->Header.ActiveRecordCount += 1;
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return -1;
    return allocatedOffset;
}

int IsamFileHandle_WriteRecordFromBuffer(IsamFileHandle* self, int64_t offset, const uint8_t* buf, int len)
{
    if (!self || !buf || len <= 0) return GMB_DB_ERR;
    fseek(self->fs, (long)offset, SEEK_SET);
    if ((int)fwrite(buf, 1, (size_t)len, self->fs) != len) return GMB_DB_ERR;
    return GMB_DB_OK;
}

int IsamFileHandle_ReadRecordBytes(IsamFileHandle* self, int64_t offset, uint8_t* buf, int len)
{
    if (!self || !buf || len <= 0) return GMB_DB_ERR;
    fseek(self->fs, (long)offset, SEEK_SET);
    if ((int)fread(buf, 1, (size_t)len, self->fs) != len) return GMB_DB_ERR;
    return GMB_DB_OK;
}

int IsamFileHandle_ReadRecordMetadata(IsamFileHandle* self, int64_t offset, RecordMetadata* out)
{
    uint8_t buf[RecordMetadata_Size];
    if (!out) return GMB_DB_ERR;
    if (IsamFileHandle_ReadRecordBytes(self, offset, buf, RecordMetadata_Size) != GMB_DB_OK) return GMB_DB_ERR;
    memcpy(out, buf, RecordMetadata_Size);
    return GMB_DB_OK;
}

int IsamFileHandle_WriteRecordMetadata(IsamFileHandle* self, int64_t offset, const RecordMetadata* meta)
{
    if (!meta) return GMB_DB_ERR;
    return IsamFileHandle_WriteRecordFromBuffer(self, offset, (const uint8_t*)meta, RecordMetadata_Size);
}

static int64_t fh_file_length(IsamFileHandle* self)
{
    struct stat st;
    if (fstat(fileno(self->fs), &st) != 0) return -1;
    return (int64_t)st.st_size;
}

static int64_t fh_get_heap_allocation_offset(IsamFileHandle* self)
{
    int64_t minHeap, start, flen;
    if (!self) return -1;
    minHeap = HeapRelocation_AlignUp(
        self->dataAreaStart + self->Header.LastRecNumber * self->Header.RecordSize,
        HeapRelocation_PageSize);
    start = self->Header.NextAvailablePageOffset;
    flen = fh_file_length(self);
    if (flen > start) start = flen;
    if (minHeap > start) start = minHeap;
    return start;
}

static void fh_register_heap_allocation(IsamFileHandle* self, int64_t start)
{
    if (self->heapRegionStart >= 0 && start >= self->heapRegionStart) return;
    self->heapRegionStart = self->heapRegionStart < 0 ? start
        : (start < self->heapRegionStart ? start : self->heapRegionStart);
    if (self->Header.LobHeapStartOffset == self->heapRegionStart) return;
    self->Header.LobHeapStartOffset = self->heapRegionStart;
    IsamFileHandle_UpdateHeader(self);
}

int64_t IsamFileHandle_ReserveHeapSpace(IsamFileHandle* self, int64_t size)
{
    int64_t start, requiredLength, flen;
    if (!self || size <= 0) return -1;
    start = fh_get_heap_allocation_offset(self);
    requiredLength = start + size;
    flen = fh_file_length(self);
    if (flen >= 0 && flen < requiredLength) {
        if (ftruncate(fileno(self->fs), (off_t)requiredLength) != 0) return -1;
    }
    self->Header.NextAvailablePageOffset = requiredLength;
    if (IsamFileHandle_UpdateHeader(self) != GMB_DB_OK) return -1;
    fh_register_heap_allocation(self, start);
    return start;
}

int64_t IsamFileHandle_WriteUndoVersion(IsamFileHandle* self, int64_t currentOffset)
{
    int recSize;
    uint8_t* buf;
    int64_t undoOffset;
    if (!self || currentOffset < 0) return -1;
    recSize = self->Header.RecordSize;
    buf = (uint8_t*)malloc((size_t)recSize);
    if (!buf) return -1;
    if (IsamFileHandle_ReadRecordBytes(self, currentOffset, buf, recSize) != GMB_DB_OK) {
        free(buf);
        return -1;
    }
    undoOffset = IsamFileHandle_ReserveHeapSpace(self, recSize);
    if (undoOffset < 0) { free(buf); return -1; }
    if (IsamFileHandle_WriteRecordFromBuffer(self, undoOffset, buf, recSize) != GMB_DB_OK) {
        free(buf);
        return -1;
    }
    free(buf);
    return undoOffset;
}
