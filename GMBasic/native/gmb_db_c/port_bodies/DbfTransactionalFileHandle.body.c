#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* C#: DbfTransactionalFileHandle — WAL su write/append/delete/zap; IntraProcess auto-commit salta WAL. */

struct DbfTxHandle {
    DbfFile* dbf;
    WriteAheadLog* wal;
    LockingStrategy strategy;
    DbfDurabilityMode durability;
    int explicitTx;
    int64_t txId;
    int64_t nextTxId;
    int bulkDepth;
    int groupCommitDepth;
    int fileLockDepth;
    int directWalDirty;
    int commitPendingFinalize;
};

static int tx_use_transactional_wal(const DbfTxHandle* self)
{
    return self && self->explicitTx;
}

static int tx_use_direct_wal(const DbfTxHandle* self)
{
    return self && !self->explicitTx && self->strategy == LockingStrategy_CrossProcess;
}

static int tx_use_wal(const DbfTxHandle* self)
{
    return tx_use_transactional_wal(self) || tx_use_direct_wal(self);
}

static int tx_should_fsync(const DbfTxHandle* self)
{
    return self && self->durability == DbfDurabilityMode_Full && self->bulkDepth == 0 && self->groupCommitDepth == 0;
}

static void tx_sync_direct_wal(DbfTxHandle* self)
{
    if (!self || !self->directWalDirty || !tx_use_direct_wal(self)) return;
    WriteAheadLog_Flush(self->wal);
    self->directWalDirty = 0;
}

static void tx_finalize_direct_after_sync(DbfTxHandle* self)
{
    if (!tx_use_direct_wal(self)) return;
    WriteAheadLog_LogDirectCheckpoint(self->wal);
    WriteAheadLog_Delete(self->wal, 1);
    self->directWalDirty = 0;
}

static void tx_stamp_header(DbfHeader* h)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    h->Year = (uint8_t)(tm.tm_year % 100);
    h->Month = (uint8_t)(tm.tm_mon + 1);
    h->Day = (uint8_t)tm.tm_mday;
}

DbfTxHandle* DbfTxHandle_New(const char* filePath, int exclusive, LockingStrategy lockingStrategy)
{
    DbfTxHandle* self;
    if (!filePath) return NULL;
    WriteAheadLog_Recover(filePath);
    self = (DbfTxHandle*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    self->strategy = lockingStrategy;
    self->durability = DbfDurabilityMode_Full;
    self->nextTxId = 1;
    self->dbf = DbfFile_New(filePath, exclusive, lockingStrategy);
    if (!self->dbf) {
        free(self);
        return NULL;
    }
    self->wal = WriteAheadLog_New(filePath);
    if (!self->wal) {
        DbfFile_Dispose(self->dbf);
        free(self);
        return NULL;
    }
    return self;
}

void DbfTxHandle_FlushDurability(DbfTxHandle* self)
{
    if (!self) return;
    DbfFile_FlushPendingHeader(self->dbf);
    if (tx_use_direct_wal(self)) {
        tx_sync_direct_wal(self);
        DbfFile_FlushStream(self->dbf, 1, 0);
        tx_finalize_direct_after_sync(self);
        return;
    }
    if (tx_use_transactional_wal(self)) WriteAheadLog_Flush(self->wal);
    DbfFile_FlushStream(self->dbf, 1, 0);
    if (tx_use_transactional_wal(self)) WriteAheadLog_Delete(self->wal, 1);
}

void DbfTxHandle_Dispose(DbfTxHandle* self)
{
    if (!self) return;
    DbfTxHandle_FlushDurability(self);
    if (self->wal) WriteAheadLog_Dispose(self->wal);
    if (self->dbf) DbfFile_Dispose(self->dbf);
    free(self);
}

DbfFile* DbfTxHandle_GetDbfFile(DbfTxHandle* self)
{
    return self ? self->dbf : NULL;
}

int DbfTxHandle_UseTransactionalWal(const DbfTxHandle* self)
{
    return tx_use_transactional_wal(self);
}

void DbfTxHandle_SetDurability(DbfTxHandle* self, DbfDurabilityMode mode)
{
    if (self) self->durability = mode;
}

DbfDurabilityMode DbfTxHandle_GetDurability(const DbfTxHandle* self)
{
    return self ? self->durability : DbfDurabilityMode_Full;
}

void DbfTxHandle_BeginBulkLoad(DbfTxHandle* self)
{
    if (self) self->bulkDepth++;
}

void DbfTxHandle_EndBulkLoad(DbfTxHandle* self)
{
    if (!self || self->bulkDepth == 0) return;
    self->bulkDepth--;
    if (self->bulkDepth > 0) return;
    DbfFile_FlushPendingHeader(self->dbf);
    if (tx_use_wal(self)) {
        if (tx_use_direct_wal(self)) tx_sync_direct_wal(self);
        else WriteAheadLog_Flush(self->wal);
    }
    DbfFile_FlushStream(self->dbf, tx_should_fsync(self), 0);
}

void DbfTxHandle_BeginGroupCommit(DbfTxHandle* self)
{
    if (self) self->groupCommitDepth++;
}

void DbfTxHandle_EndGroupCommit(DbfTxHandle* self)
{
    if (!self || self->groupCommitDepth == 0) return;
    self->groupCommitDepth--;
    if (self->groupCommitDepth == 0) DbfTxHandle_FinalizeAutoCommit(self);
}

int64_t DbfTxHandle_BeginTransaction(DbfTxHandle* self)
{
    if (!self) return 0;
    self->nextTxId++;
    self->txId = self->nextTxId;
    self->explicitTx = 1;
    WriteAheadLog_LogBeginTransaction(self->wal);
    WriteAheadLog_Flush(self->wal);
    return self->txId;
}

int DbfTxHandle_CommitTransaction(DbfTxHandle* self)
{
    if (!self || !self->explicitTx) return GMB_DB_ERR;
    self->explicitTx = 0;
    self->commitPendingFinalize = 1;
    WriteAheadLog_LogCommitTransaction(self->wal);
    return GMB_DB_OK;
}

int DbfTxHandle_RollbackTransaction(DbfTxHandle* self)
{
    if (!self || !self->explicitTx) return GMB_DB_ERR;
    self->explicitTx = 0;
    return GMB_DB_OK;
}

void DbfTxHandle_FinalizeCommit(DbfTxHandle* self)
{
    int dropWal;
    if (!self) return;
    dropWal = self->commitPendingFinalize || tx_use_transactional_wal(self);
    DbfFile_FlushPendingHeader(self->dbf);
    if (tx_use_direct_wal(self)) {
        tx_sync_direct_wal(self);
        DbfFile_FlushStream(self->dbf, 1, 0);
        tx_finalize_direct_after_sync(self);
        self->commitPendingFinalize = 0;
        return;
    }
    if (dropWal) WriteAheadLog_Flush(self->wal);
    DbfFile_FlushStream(self->dbf, 1, 0);
    if (dropWal) WriteAheadLog_Delete(self->wal, 1);
    self->commitPendingFinalize = 0;
}

void DbfTxHandle_FinalizeAutoCommit(DbfTxHandle* self)
{
    if (!self) return;
    DbfFile_FlushPendingHeader(self->dbf);
    if (tx_use_direct_wal(self)) {
        if (tx_should_fsync(self)) {
            tx_sync_direct_wal(self);
            DbfFile_FlushStream(self->dbf, 1, 0);
            tx_finalize_direct_after_sync(self);
        } else {
            DbfFile_FlushStream(self->dbf, 0, 0);
        }
        return;
    }
    if (tx_use_transactional_wal(self)) {
        WriteAheadLog_LogCommitTransaction(self->wal);
        DbfFile_FlushStream(self->dbf, 1, 0);
        WriteAheadLog_Delete(self->wal, 1);
    } else {
        DbfFile_FlushStream(self->dbf, tx_should_fsync(self), 0);
    }
}

int DbfTxHandle_WriteRawRecord(DbfTxHandle* self, int recNo, const uint8_t* rawData, int rawLen)
{
    const DbfHeader* h;
    int64_t offset;
    if (!self || !rawData) return GMB_DB_ARG;
    h = DbfFile_get_Header(self->dbf);
    offset = (int64_t)h->HeaderLength + ((int64_t)(recNo - 1) * h->RecordLength);
    if (tx_use_transactional_wal(self)) {
        WriteAheadLog_LogWriteBytes(self->wal, offset + 1, rawData, rawLen);
    } else if (tx_use_direct_wal(self)) {
        WriteAheadLog_LogDirectWrite(self->wal, offset + 1, rawData, rawLen);
        self->directWalDirty = 1;
    }
    return DbfFile_WriteRawRecord(self->dbf, recNo, rawData, rawLen);
}

int DbfTxHandle_AppendRawRecord(DbfTxHandle* self, const uint8_t* rawData, int rawLen)
{
    DbfHeader current, future;
    int64_t newOff;
    uint8_t hdrBytes[DbfHeader_Size];
    uint8_t* full;
    int recLen, rc;
    if (!self || !rawData) return GMB_DB_ARG;
    current = *DbfFile_get_Header(self->dbf);
    newOff = (int64_t)current.HeaderLength + ((int64_t)current.NumberOfRecords * current.RecordLength);
    future = current;
    future.NumberOfRecords++;
    tx_stamp_header(&future);
    recLen = (int)current.RecordLength;
    if (tx_use_transactional_wal(self)) {
        DbfHeader_ToBytes(&future, hdrBytes);
        WriteAheadLog_LogUpdateHeader(self->wal, hdrBytes, DbfHeader_Size);
        full = (uint8_t*)calloc(1, (size_t)recLen);
        if (!full) return GMB_DB_ERR;
        full[0] = (uint8_t)' ';
        if (rawLen > recLen - 1) rawLen = recLen - 1;
        if (rawLen > 0) memcpy(full + 1, rawData, (size_t)rawLen);
        WriteAheadLog_LogWriteRecord(self->wal, newOff, full, recLen);
        free(full);
    } else if (tx_use_direct_wal(self)) {
        DbfHeader_ToBytes(&future, hdrBytes);
        WriteAheadLog_LogDirectWrite(self->wal, 0, hdrBytes, DbfHeader_Size);
        full = (uint8_t*)calloc(1, (size_t)recLen);
        if (!full) return GMB_DB_ERR;
        full[0] = (uint8_t)' ';
        if (rawLen > recLen - 1) rawLen = recLen - 1;
        if (rawLen > 0) memcpy(full + 1, rawData, (size_t)rawLen);
        WriteAheadLog_LogDirectWrite(self->wal, newOff, full, recLen);
        free(full);
        self->directWalDirty = 1;
    }
    rc = DbfFile_AppendRawRecord(self->dbf, rawData, rawLen);
    if (tx_use_wal(self)) WriteAheadLog_Flush(self->wal);
    return rc;
}

int DbfTxHandle_SetDeletedFlag(DbfTxHandle* self, int recNo, int isDeleted)
{
    const DbfHeader* h;
    int64_t offset;
    uint8_t flag[1];
    if (!self) return GMB_DB_ARG;
    h = DbfFile_get_Header(self->dbf);
    offset = (int64_t)h->HeaderLength + ((int64_t)(recNo - 1) * h->RecordLength);
    flag[0] = isDeleted ? (uint8_t)'*' : (uint8_t)' ';
    if (tx_use_transactional_wal(self)) {
        WriteAheadLog_LogWriteBytes(self->wal, offset, flag, 1);
    } else if (tx_use_direct_wal(self)) {
        WriteAheadLog_LogDirectWrite(self->wal, offset, flag, 1);
        self->directWalDirty = 1;
    }
    return DbfFile_SetDeletedFlag(self->dbf, recNo, isDeleted);
}

int DbfTxHandle_Zap(DbfTxHandle* self)
{
    DbfHeader future;
    uint8_t hdrBytes[DbfHeader_Size];
    if (!self) return GMB_DB_ARG;
    future = *DbfFile_get_Header(self->dbf);
    future.NumberOfRecords = 0;
    tx_stamp_header(&future);
    DbfHeader_ToBytes(&future, hdrBytes);
    WriteAheadLog_LogUpdateHeader(self->wal, hdrBytes, DbfHeader_Size);
    return DbfFile_Zap(self->dbf);
}

int DbfTxHandle_IsExplicitTransactionActive(const DbfTxHandle* self)
{
    return self && self->explicitTx;
}
