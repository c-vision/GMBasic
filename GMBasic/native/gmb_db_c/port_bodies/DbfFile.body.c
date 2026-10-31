#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

struct DbfFile {
    FILE* stream;
    int disposed;
    int headerDirty;
    DbfHeader Header;
    DbfField* Fields;
    int FieldCount;
    char FilePath[PATH_MAX];
};

static void DbfFile_now(uint8_t* y, uint8_t* m, uint8_t* d)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    *y = (uint8_t)((tm.tm_year + 1900) % 100);
    *m = (uint8_t)(tm.tm_mon + 1);
    *d = (uint8_t)tm.tm_mday;
}

static DbfHeader DbfFile_BumpHeaderRecordCount(DbfHeader header, uint32_t add)
{
    uint8_t y, m, d;
    header.NumberOfRecords += add;
    DbfFile_now(&y, &m, &d);
    header.Year = y;
    header.Month = m;
    header.Day = d;
    return header;
}

static int DbfFile_ValidateRecordNumber(const DbfFile* self, int recNo)
{
    if (recNo <= 0 || recNo > (int)self->Header.NumberOfRecords) return GMB_DB_ARG;
    return GMB_DB_OK;
}

static int DbfFile_WriteHeaderToDisk(DbfFile* self, DbfHeader header)
{
    uint8_t headerBytes[DbfHeader_Size];
    fseek(self->stream, 0, SEEK_SET);
    DbfHeader_ToBytes(&header, headerBytes);
    return fwrite(headerBytes, 1, DbfHeader_Size, self->stream) == DbfHeader_Size ? GMB_DB_OK : GMB_DB_ERR;
}

static long DbfFile_file_len(FILE* f)
{
    long cur = ftell(f), end;
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

DbfFile* DbfFile_New(const char* filePath, int exclusive, LockingStrategy lockingStrategy)
{
    DbfFile* self;
    (void)exclusive;
    (void)lockingStrategy;
    /* ponytail: multi work area / cross-process — record locks via ILockManager, not OS exclusive FileShare */
    self = (DbfFile*)calloc(1, sizeof(*self));
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", filePath);
    self->stream = fopen(filePath, "rb+");
    if (!self->stream) { free(self); return NULL; }
    if (DbfHeader_Read(self->stream, &self->Header) != GMB_DB_OK) {
        fclose(self->stream);
        free(self);
        return NULL;
    }
    if (DbfField_ReadAll(self->stream, &self->Fields, &self->FieldCount) != GMB_DB_OK) {
        fclose(self->stream);
        free(self);
        return NULL;
    }
    return self;
}

void DbfFile_Dispose(DbfFile* self)
{
    if (!self || self->disposed) return;
    DbfFile_FlushPendingHeader(self);
    if (self->stream) fclose(self->stream);
    free(self->Fields);
    self->disposed = 1;
}

int64_t DbfFile_GetFileLength(const DbfFile* self)
{
    return self ? (int64_t)DbfFile_file_len(self->stream) : 0;
}

int DbfFile_ReadRawRecordInto(DbfFile* self, int recNo, uint8_t* buffer, int bufferLen, int* isDeleted)
{
    int64_t offset;
    if (!self || !buffer || !isDeleted) return GMB_DB_ARG;
    if (bufferLen < self->Header.RecordLength) return GMB_DB_ARG;
    if (DbfFile_ValidateRecordNumber(self, recNo) != GMB_DB_OK) return GMB_DB_ARG;
    offset = (int64_t)self->Header.HeaderLength + ((int64_t)(recNo - 1) * self->Header.RecordLength);
    fseek(self->stream, (long)offset, SEEK_SET);
    if (fread(buffer, 1, self->Header.RecordLength, self->stream) != self->Header.RecordLength) return GMB_DB_ERR;
    *isDeleted = buffer[0] == '*';
    return GMB_DB_OK;
}

int DbfFile_ReadRawRecord(DbfFile* self, int recNo, int* isDeleted, uint8_t** data, int* dataLen)
{
    int recLen = self->Header.RecordLength;
    uint8_t* buffer = (uint8_t*)malloc((size_t)recLen);
    int rc = DbfFile_ReadRawRecordInto(self, recNo, buffer, recLen, isDeleted);
    if (rc != GMB_DB_OK) { free(buffer); return rc; }
    *dataLen = recLen - 1;
    *data = (uint8_t*)malloc((size_t)*dataLen);
    memcpy(*data, buffer + 1, (size_t)*dataLen);
    free(buffer);
    return GMB_DB_OK;
}

int DbfFile_WriteRawRecord(DbfFile* self, int recNo, const uint8_t* rawData, int rawLen)
{
    int64_t offset;
    (void)rawLen;
    if (DbfFile_ValidateRecordNumber(self, recNo) != GMB_DB_OK) return GMB_DB_ARG;
    offset = (int64_t)self->Header.HeaderLength + ((int64_t)(recNo - 1) * self->Header.RecordLength) + 1;
    fseek(self->stream, (long)offset, SEEK_SET);
    fwrite(rawData, 1, (size_t)(self->Header.RecordLength - 1), self->stream);
    return GMB_DB_OK;
}

int DbfFile_AppendRawRecord(DbfFile* self, const uint8_t* rawData, int rawLen)
{
    int64_t offset = (int64_t)self->Header.HeaderLength + (int64_t)self->Header.NumberOfRecords * self->Header.RecordLength;
    int64_t requiredLength = offset + self->Header.RecordLength;
    uint8_t space = (uint8_t)' ';
    (void)rawLen;
    if (DbfFile_file_len(self->stream) < requiredLength) {
        /* SetLength */
        int fd = fileno(self->stream);
        if (ftruncate(fd, (off_t)requiredLength) != 0) return GMB_DB_ERR;
    }
    fseek(self->stream, (long)offset, SEEK_SET);
    fwrite(&space, 1, 1, self->stream);
    fwrite(rawData, 1, (size_t)(self->Header.RecordLength - 1), self->stream);
    self->Header = DbfFile_BumpHeaderRecordCount(self->Header, 1);
    self->headerDirty = 1;
    return (int)self->Header.NumberOfRecords;
}

int DbfFile_AppendRawRecordsBatch(DbfFile* self, const uint8_t* rawPayload, int recordPayloadLength, int count)
{
    int startRecNo, i;
    int64_t offset, totalBytes, requiredLength;
    int recordLength;
    uint8_t space = (uint8_t)' ';
    if (count <= 0) return GMB_DB_ARG;
    if (rawPayload == NULL) return GMB_DB_ARG;
    startRecNo = (int)self->Header.NumberOfRecords + 1;
    offset = (int64_t)self->Header.HeaderLength + (int64_t)self->Header.NumberOfRecords * self->Header.RecordLength;
    recordLength = self->Header.RecordLength;
    totalBytes = (int64_t)recordLength * count;
    requiredLength = offset + totalBytes;
    if (DbfFile_file_len(self->stream) < requiredLength) {
        if (ftruncate(fileno(self->stream), (off_t)requiredLength) != 0) return GMB_DB_ERR;
    }
    fseek(self->stream, (long)offset, SEEK_SET);
    for (i = 0; i < count; i++) {
        fwrite(&space, 1, 1, self->stream);
        fwrite(rawPayload + (size_t)i * (size_t)recordPayloadLength, 1, (size_t)recordPayloadLength, self->stream);
    }
    self->Header = DbfFile_BumpHeaderRecordCount(self->Header, (uint32_t)count);
    self->headerDirty = 1;
    return startRecNo;
}

int DbfFile_ReloadHeaderFromDisk(DbfFile* self)
{
    FILE* probe = fopen(self->FilePath, "rb");
    int64_t expectedLength;
    if (!probe) return GMB_DB_ERR;
    if (DbfHeader_Read(probe, &self->Header) != GMB_DB_OK) { fclose(probe); return GMB_DB_ERR; }
    fclose(probe);
    self->headerDirty = 0;
    expectedLength = (int64_t)self->Header.HeaderLength + (int64_t)self->Header.NumberOfRecords * self->Header.RecordLength;
    if (DbfFile_file_len(self->stream) < expectedLength)
        ftruncate(fileno(self->stream), (off_t)expectedLength);
    return GMB_DB_OK;
}

int DbfFile_FlushPendingHeader(DbfFile* self)
{
    if (!self->headerDirty) return GMB_DB_OK;
    if (DbfFile_WriteHeaderToDisk(self, self->Header) != GMB_DB_OK) return GMB_DB_ERR;
    self->headerDirty = 0;
    return GMB_DB_OK;
}

int DbfFile_FlushStream(DbfFile* self, int durable, int flushHeaderFirst)
{
    if (flushHeaderFirst) DbfFile_FlushPendingHeader(self);
    fflush(self->stream);
    if (durable) fsync(fileno(self->stream));
    return GMB_DB_OK;
}

int DbfFile_SetDeletedFlag(DbfFile* self, int recNo, int isDeleted)
{
    int64_t offset;
    uint8_t flag;
    if (DbfFile_ValidateRecordNumber(self, recNo) != GMB_DB_OK) return GMB_DB_ARG;
    offset = (int64_t)self->Header.HeaderLength + ((int64_t)(recNo - 1) * self->Header.RecordLength);
    fseek(self->stream, (long)offset, SEEK_SET);
    flag = isDeleted ? (uint8_t)'*' : (uint8_t)' ';
    fwrite(&flag, 1, 1, self->stream);
    return GMB_DB_OK;
}

int DbfFile_CloneStructure(DbfFile* self, const char* newFilePath)
{
    FILE* newStream;
    int headerSize = self->Header.HeaderLength;
    uint8_t* headerBuffer;
    uint8_t y, m, d;
    time_t t = time(NULL);
    struct tm tm;
    newStream = fopen(newFilePath, "wb");
    if (!newStream) return GMB_DB_ERR;
    headerBuffer = (uint8_t*)calloc(1, (size_t)headerSize);
    fseek(self->stream, 0, SEEK_SET);
    if (fread(headerBuffer, 1, (size_t)headerSize, self->stream) != (size_t)headerSize) {
        free(headerBuffer); fclose(newStream); return GMB_DB_ERR;
    }
    localtime_r(&t, &tm);
    y = (uint8_t)(tm.tm_year); /* Year - 1900: tm_year is already years since 1900 */
    m = (uint8_t)(tm.tm_mon + 1);
    d = (uint8_t)tm.tm_mday;
    headerBuffer[1] = y;
    headerBuffer[2] = m;
    headerBuffer[3] = d;
    memset(headerBuffer + 4, 0, 4);
    fwrite(headerBuffer, 1, (size_t)headerSize, newStream);
    free(headerBuffer);
    fclose(newStream);
    return GMB_DB_OK;
}

int DbfFile_Zap(DbfFile* self)
{
    uint8_t y, m, d;
    uint8_t headerBytes[DbfHeader_Size];
    if (ftruncate(fileno(self->stream), (off_t)self->Header.HeaderLength) != 0) return GMB_DB_ERR;
    self->Header.NumberOfRecords = 0;
    DbfFile_now(&y, &m, &d);
    self->Header.Year = y;
    self->Header.Month = m;
    self->Header.Day = d;
    fseek(self->stream, 0, SEEK_SET);
    DbfHeader_ToBytes(&self->Header, headerBytes);
    fwrite(headerBytes, 1, DbfHeader_Size, self->stream);
    fflush(self->stream);
    return GMB_DB_OK;
}

int DbfFile_CreateNew(const char* filePath, const char* encodingWebName, const DbfField* fields, int fieldCount)
{
    FILE* fs;
    uint16_t headerLength, recordLength;
    int languageDriver, i;
    uint32_t fieldOffset = 1;
    time_t t = time(NULL);
    struct tm tm;
    uint8_t zeros17[17] = {0}, zeros2[2] = {0}, zeros14[14] = {0};
    uint8_t term0d = 0x0D, eof1a = 0x1A, ver = (uint8_t)DbfVersion_VisualFoxPro;
    uint8_t y, mo, d;
    uint32_t recCount = 0;
    localtime_r(&t, &tm);
    y = (uint8_t)tm.tm_year;
    mo = (uint8_t)(tm.tm_mon + 1);
    d = (uint8_t)tm.tm_mday;
    headerLength = (uint16_t)(32 + fieldCount * 32 + 1);
    recordLength = 1;
    for (i = 0; i < fieldCount; i++) recordLength = (uint16_t)(recordLength + fields[i].Length);
    languageDriver = DbfCodePage_GetLanguageDriver(encodingWebName);
    if (languageDriver < 0) languageDriver = 0x03;
    fs = fopen(filePath, "wb");
    if (!fs) return GMB_DB_ERR;
    fwrite(&ver, 1, 1, fs);
    fwrite(&y, 1, 1, fs);
    fwrite(&mo, 1, 1, fs);
    fwrite(&d, 1, 1, fs);
    fwrite(&recCount, 4, 1, fs);
    fwrite(&headerLength, 2, 1, fs);
    fwrite(&recordLength, 2, 1, fs);
    fwrite(zeros17, 1, 17, fs);
    { uint8_t ld = (uint8_t)languageDriver; fwrite(&ld, 1, 1, fs); }
    fwrite(zeros2, 1, 2, fs);
    for (i = 0; i < fieldCount; i++) {
        uint8_t nameBytes[11] = {0};
        size_t nl = strlen(fields[i].Name);
        if (nl > 10) nl = 10;
        memcpy(nameBytes, fields[i].Name, nl);
        fwrite(nameBytes, 1, 11, fs);
        fwrite(&fields[i].Type, 1, 1, fs);
        fwrite(&fieldOffset, 4, 1, fs);
        fwrite(&fields[i].Length, 1, 1, fs);
        fwrite(&fields[i].DecimalCount, 1, 1, fs);
        fwrite(zeros14, 1, 14, fs);
        fieldOffset += fields[i].Length;
    }
    fwrite(&term0d, 1, 1, fs);
    fwrite(&eof1a, 1, 1, fs);
    fclose(fs);
    return GMB_DB_OK;
}

const DbfHeader* DbfFile_get_Header(const DbfFile* self) { return &self->Header; }
const DbfField* DbfFile_get_Fields(const DbfFile* self, int* count)
{
    if (count) *count = self->FieldCount;
    return self->Fields;
}
const char* DbfFile_get_FilePath(const DbfFile* self) { return self->FilePath; }
