/* FILE: native/gmb_db_c/include/gmb_db_port.h
 * Umbrella C per il port 1:1. I tipi vivono nei file omonimi sotto src/
 * (Locking.c, DbfDurabilityMode.c, …). Questo header è solo runtime C:
 * stdint, stringhe, errori che in C# sono eccezioni, e i tipi condivisi
 * tra TU (WriteAheadLog, DbfHeader, …) — stessi layout del C#.
 */
#ifndef GMB_DB_PORT_H
#define GMB_DB_PORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "gmb_decimal.h"
#include "gmb_expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C# LockTimeoutException / DeadlockException / ArgumentException → codici. */
#define GMB_DB_OK            0
#define GMB_DB_ERR          -1
#define GMB_DB_TIMEOUT      -2
#define GMB_DB_DEADLOCK     -3
#define GMB_DB_ARG          -4

#ifdef _WIN32
#define gmb_strcasecmp _stricmp
#define gmb_strncasecmp _strnicmp
#else
#define gmb_strcasecmp strcasecmp
#define gmb_strncasecmp strncasecmp
#endif

/* C#: public enum LockingStrategy */
typedef enum LockingStrategy {
    LockingStrategy_IntraProcess = 0,
    LockingStrategy_CrossProcess = 1
} LockingStrategy;

/* C#: public enum DbfDurabilityMode */
typedef enum DbfDurabilityMode {
    DbfDurabilityMode_Full = 0,
    DbfDurabilityMode_Buffered = 1
} DbfDurabilityMode;

/* C#: internal enum WalOperationType : byte (common/Databases/WriteAheadLog.cs) */
typedef enum WalOperationType {
    WalOperationType_WriteRecord = 1,
    WalOperationType_WriteNode = 2,
    WalOperationType_UpdateHeader = 3,
    WalOperationType_WriteBytes = 6,
    WalOperationType_DirectWrite = 7,
    WalOperationType_DirectCheckpoint = 8,
    WalOperationType_BeginTransaction = 10,
    WalOperationType_CommitTransaction = 11,
    WalOperationType_TransactionSnapshot = 12
} WalOperationType;

/* C#: public sealed record WalLogSummary */
typedef struct WalLogSummary {
    char DatabasePath[1024];
    char WalPath[1024];
    int Exists;
    int64_t SizeBytes;
    int64_t LastPosition;
    int PendingRecords;
    int UncommittedTransactionOps;
    int CommittedTransactionBlocks;
    int DirectWritesPending;
    int EndsWithCheckpoint;
    int InOpenTransaction;
    char DurabilityMode[32];
    int WalHandleActive; /* -1 = null */
    int DirectWalDirty;  /* -1 = null */
} WalLogSummary;

void WalLogSummary_Format(const WalLogSummary* s, char* out, int cap);

typedef struct WriteAheadLog WriteAheadLog;

WriteAheadLog* WriteAheadLog_New(const char* dbFilePath);
void WriteAheadLog_Dispose(WriteAheadLog* self);
void WriteAheadLog_LogBeginTransaction(WriteAheadLog* self);
void WriteAheadLog_LogTransactionSnapshot(WriteAheadLog* self, const uint8_t* headerData, int headerLen,
                                          const uint8_t* indexMapData, int mapLen);
void WriteAheadLog_LogCommitTransaction(WriteAheadLog* self);
void WriteAheadLog_LogWriteRecord(WriteAheadLog* self, int64_t offset, const uint8_t* recordData, int len);
void WriteAheadLog_LogWriteRecordSpan(WriteAheadLog* self, int64_t offset, const uint8_t* recordData, int len);
void WriteAheadLog_LogWriteNode(WriteAheadLog* self, int64_t offset, const uint8_t* nodeData, int len);
void WriteAheadLog_LogUpdateHeader(WriteAheadLog* self, const uint8_t* headerData, int len);
void WriteAheadLog_LogWriteBytes(WriteAheadLog* self, int64_t offset, const uint8_t* data, int len);
void WriteAheadLog_LogDirectWrite(WriteAheadLog* self, int64_t offset, const uint8_t* data, int len);
void WriteAheadLog_LogDirectCheckpoint(WriteAheadLog* self);
void WriteAheadLog_Flush(WriteAheadLog* self);
void WriteAheadLog_Delete(WriteAheadLog* self, int skipFlush);
int WriteAheadLog_Inspect(const char* dbFilePath, WalLogSummary* out);
void WriteAheadLog_Recover(const char* dbFilePath);
int WriteAheadLog_SelfCheck(void);

/* C#: RuntimeExpressionEvaluator — SET FILTER. Body = gmb_expr_eval_filter (`=` = uguaglianza). */
typedef int (*RtFilterGetField)(void* ctx, const char* fieldName, char* out, int cap);
int RuntimeExpressionEvaluator_Eval(const char* expression, RtFilterGetField get, void* ctx);

/* Parità C# (DbExprPCodeEvaluator → PCodeVM): la lettura del campo restituisce gmb_val_t
 * TIPIZZATO (INT/DOUBLE/BOOL/STRING), non testo — così confronti e aritmetica usano il tipo
 * reale del campo (Id > 3 numerico, Name = 'x' stringa), come il Dictionary<string,object?>
 * di C#. call = callback funzioni utente (NULL finché il wiring host non c'è). */
typedef int (*RtFilterGetFieldVal)(void* ctx, const char* fieldName, gmb_val_t* out);
int RuntimeExpressionEvaluator_EvalVal(const char* expression, RtFilterGetFieldVal get, void* ctx,
                                       gmb_expr_call_fn call);

/* C#: Basic.ISAM.IsamFilterPushdown — uguaglianze field=literal (stringa, non AST). */
#define IsamFilterPushdown_MaxEq 8
#define IsamFilterPushdown_NameCap 64
#define IsamFilterPushdown_ValCap 256
typedef struct {
    char Name[IsamFilterPushdown_NameCap];
    char Value[IsamFilterPushdown_ValCap];
} IsamFilterEquality;
int IsamFilterPushdown_TryParseEquality(const char* expression, char* fieldName, int fieldCap,
                                        char* value, int valueCap);
int IsamFilterPushdown_TryCollectEqualities(const char* expression, IsamFilterEquality* eqs, int maxEq,
                                            int* count);
int IsamFilterPushdown_TryGetKeyFieldNames(const char* keyExpression,
                                           char names[][IsamFilterPushdown_NameCap], int maxNames, int* count);
int IsamFilterPushdown_CountMatchingPrefixFields(const char names[][IsamFilterPushdown_NameCap], int nNames,
                                                 const IsamFilterEquality* eqs, int nEq);
int IsamFilterPushdown_ComputeMatchLength(const char* keyValue, int isFullKeyMatch);
int IsamFilterPushdown_FindEquality(const IsamFilterEquality* eqs, int nEq, const char* fieldName);

/* C#: Basic.Databases.Replication.ReplicationOp */
typedef enum ReplicationOp {
    ReplicationOp_Append = 1,
    ReplicationOp_Put = 2,
    ReplicationOp_Delete = 3,
    ReplicationOp_Recall = 4,
    ReplicationOp_TxnBegin = 10,
    ReplicationOp_TxnCommit = 11,
    ReplicationOp_Pack = 20,
    ReplicationOp_Schema = 21,
    ReplicationOp_Reindex = 22
} ReplicationOp;

void ReplicationLogLine_FormatTxnMarker(ReplicationOp op, char* out, int cap);
void ReplicationLogLine_FormatPack(int64_t generation, char* out, int cap);
void ReplicationLogLine_FormatReindex(int64_t generation, char* out, int cap);
void ReplicationLogLine_Format(ReplicationOp op, int64_t recNo, const char* jsonValues, char* out, int cap);
int ReplicationLogLine_TryParse(const char* line, ReplicationOp* op, int64_t* recNo, char* jsonOut, int jsonCap);

int ReplicationLogEncoding_FileHasUtf8Bom(const char* path);
int64_t ReplicationLogEncoding_AdjustFileOffset(const char* path, int64_t logicalOffset);

#define GMB_REPL_MAX_MAP 256
#define GMB_REPL_MAX_PENDING 64
#define GMB_REPL_LINE_CAP 768
typedef struct ReplicationApplyState {
    int64_t appliedAppends[GMB_REPL_MAX_MAP];
    int appliedAppendN;
    int64_t recMapFrom[GMB_REPL_MAX_MAP];
    int64_t recMapTo[GMB_REPL_MAX_MAP];
    int recMapN;
    char pendingTxnLines[GMB_REPL_MAX_PENDING][GMB_REPL_LINE_CAP];
    int pendingTxnN;
    int InTxn;
    int64_t LastPackGeneration;
    int64_t LastSchemaGeneration;
    int64_t LastReindexGeneration;
} ReplicationApplyState;
void ReplicationApplyState_Init(ReplicationApplyState* self);
void ReplicationApplyState_ResetAfterPack(ReplicationApplyState* self, int64_t generation);
void ReplicationApplyState_MarkSchemaApplied(ReplicationApplyState* self, int64_t generation);
void ReplicationApplyState_MarkReindexApplied(ReplicationApplyState* self, int64_t generation);
int ReplicationApplyState_AddAppliedAppend(ReplicationApplyState* self, int64_t recNo);
void ReplicationApplyState_SetRecMap(ReplicationApplyState* self, int64_t from, int64_t to);
int64_t ReplicationApplyState_ResolveRecNo(const ReplicationApplyState* self, int64_t primary);
int ReplicationApplyState_PushPending(ReplicationApplyState* self, const char* line);
void ReplicationApplyState_ClearPending(ReplicationApplyState* self);
void ReplicationApplyState_SaveCheckpoint(const ReplicationApplyState* self, const char* checkpointPath, int64_t byteOffset);
int64_t ReplicationApplyState_LoadCheckpoint(ReplicationApplyState* self, const char* checkpointPath);

typedef int (*ReplicationApplyLineFn)(ReplicationApplyState* state, const char* line, void* ctx);
void ReplicationLogFollower_DefaultCheckpointPath(const char* replLogPath, char* out, int cap);
int ReplicationLogFollower_FollowOnce(const char* replLogPath, ReplicationApplyLineFn applyLine, void* ctx,
                                      const char* checkpointPath);

void ReplicationShipState_DefaultCheckpointPath(const char* replLogPath, char* out, int cap);
int64_t ReplicationShipState_LoadOffset(const char* checkpointPath);
void ReplicationShipState_SaveOffset(const char* checkpointPath, int64_t offset);

int ReplicationTcpProtocol_AllocateLoopbackPort(void);
int64_t ReplicationTcpProtocol_LineByteCount(const char* line);
int ReplicationTcpProtocol_WriteLine(int fd, const char* line);
int ReplicationTcpProtocol_ReadLine(int fd, char* out, int cap); /* 0 ok, 1 EOF */
int ReplicationTcpProtocol_TryParseHello(const char* line, int64_t* byteOffset);
int ReplicationTcpProtocol_TryParseAck(const char* line, int64_t* byteOffset);
int ReplicationTcpProtocol_IsPing(const char* line);
int ReplicationTcpProtocol_IsBye(const char* line);
int ReplicationTcpProtocol_IsControlLine(const char* line);

int ReplicationTcpShipper_ShipOnce(const char* replLogPath, const char* host, int port,
                                   const char* shipCheckpointPath);
int ReplicationTcpReceiver_ReceiveOnce(int port, ReplicationApplyLineFn applyLine, void* ctx,
                                       const char* checkpointPath, int acceptTimeoutSec);
int ReplicationTcpReceiver_ReceiveSession(int fd, ReplicationApplyLineFn applyLine, void* ctx,
                                          const char* checkpointPath);
int ReplicationTcp_SelfCheck(void);

void ReplicationSchemaMetadata_ForIsam(int64_t generation, const char* ischB64, char* out, int cap);
void ReplicationSchemaMetadata_ForDbfGms(int64_t generation, const char* gmsB64, char* out, int cap);
int64_t ReplicationSchemaMetadata_RequireGeneration(const char* json);
int ReplicationSchemaMetadata_RequireBase64(const char* json, const char* key, char* out, int cap);
int ReplicationSchemaMetadata_DecodeBase64(const char* text, uint8_t* out, int cap, int* nOut);
void ReplicationSchemaMetadata_EncodeBase64(const uint8_t* data, int len, char* out, int cap);

void DbfSchemaManager_GetSchemaFilePath(const char* dbfFilePath, char* out, int cap);
int DbfSchemaManager_WriteSchemaBytes(const char* dbfFilePath, const uint8_t* bytes, int len);
int DbfSchemaManager_ReadSchemaBytes(const char* dbfFilePath, uint8_t* out, int cap, int* nOut);
void DbfSchemaManager_TryDeleteSchemaFile(const char* schemaFilePath);

typedef struct DbfEngine DbfEngine;
int DbfReplicationApplier_Apply(const char* replLogPath, DbfEngine* target);
int DbfReplicationApplier_Follow(const char* replLogPath, DbfEngine* target, const char* checkpointPath);
int DbfReplicationApplier_ApplyLine(const char* line, ReplicationApplyState* state, DbfEngine* target);
int DbfReplicationApplier_ApplyPack(ReplicationApplyState* state, DbfEngine* target, int64_t generation);
int DbfReplicationApplier_ApplySchema(ReplicationApplyState* state, DbfEngine* target, const char* json);
int DbfReplicationApplier_ApplyReindex(ReplicationApplyState* state, DbfEngine* target, int64_t generation);

typedef struct DbfReplicationLog DbfReplicationLog;
DbfReplicationLog* DbfReplicationLog_New(const char* logPath);
void DbfReplicationLog_Dispose(DbfReplicationLog* self);
const char* DbfReplicationLog_get_LogPath(const DbfReplicationLog* self);
void DbfReplicationLog_WriteLine(DbfReplicationLog* self, const char* line);

typedef struct IsamReplicationLog IsamReplicationLog;
IsamReplicationLog* IsamReplicationLog_New(const char* logPath);
void IsamReplicationLog_Dispose(IsamReplicationLog* self);
const char* IsamReplicationLog_get_LogPath(const IsamReplicationLog* self);
void IsamReplicationLog_WriteLine(IsamReplicationLog* self, const char* line);

/* C#: VersioningManager — file .gmv pack=1, 24 byte/record. */
#pragma pack(push, 1)
typedef struct VersionMetadata {
    int64_t Version;
    int64_t TransactionId;
    int64_t PreviousVersionRecNo;
} VersionMetadata;
#pragma pack(pop)
typedef struct VersioningManager VersioningManager;
VersioningManager* VersioningManager_New(const char* dbfFilePath);
void VersioningManager_Dispose(VersioningManager* self);
VersionMetadata VersioningManager_GetMetadata(VersioningManager* self, int recNo);
void VersioningManager_SetMetadata(VersioningManager* self, int recNo, VersionMetadata metadata);
void VersioningManager_AddNewVersionEntry(VersioningManager* self, VersionMetadata initialMetadata);
void VersioningManager_Clear(VersioningManager* self);
int VersioningManager_SelfCheck(void);

/* C#: public enum DbfVersion / DbfFieldType */
typedef enum DbfVersion {
    DbfVersion_Unknown = 0,
    DbfVersion_DBase2 = 0x02,
    DbfVersion_DBase3PlusWithMemo = 0x83,
    DbfVersion_DBase4WithMemo = 0x8B,
    DbfVersion_VisualFoxPro = 0x30,
    DbfVersion_VisualFoxProWithMemo = 0xF5
} DbfVersion;

typedef enum DbfFieldType {
    DbfFieldType_Character = 'C',
    DbfFieldType_Date = 'D',
    DbfFieldType_Float = 'F',
    DbfFieldType_Logical = 'L',
    DbfFieldType_Memo = 'M',
    DbfFieldType_Numeric = 'N',
    DbfFieldType_Integer = 'I',
    DbfFieldType_Currency = 'Y',
    DbfFieldType_DateTime = 'T',
    DbfFieldType_Double = 'B',
    DbfFieldType_General = 'G',
    DbfFieldType_Varchar = 'V',
    DbfFieldType_Varbinary = 'Q',
    DbfFieldType_Blob = 'W',
    DbfFieldType_AutoIncrement = '+'
} DbfFieldType;

int DbfVersionExtensions_SupportsMemoFields(DbfVersion version);

/* C#: Basic.DBF.IMemoFile / MemoData / DbfMemoFactory / Db3MemoFile / Db4MemoFile / FptFile */
typedef struct MemoData {
    uint8_t* Data;
    int Length;
    int IsText; /* 1 = TextMemo (FPT type 1); 0 = BinaryMemo */
} MemoData;

void MemoData_Dispose(MemoData* m);

typedef struct MemoFile MemoFile;

MemoFile* DbfMemoFactory_Create(const char* dbfFilePath, DbfVersion dbfVersion, int createIfMissing);
void MemoFile_Dispose(MemoFile* self);
int MemoFile_IsNull(const MemoFile* self);
int MemoFile_GetMemo(MemoFile* self, int index, MemoData* out);
int MemoFile_WriteMemo(MemoFile* self, const uint8_t* data, int len, int isText);
void FptFile_FreeBlock(MemoFile* self, int blockIndex);
int DbfMemo_SelfCheck(void);

#pragma pack(push, 1)
typedef struct DbfHeader {
    uint8_t VersionByte;
    uint8_t Year;
    uint8_t Month;
    uint8_t Day;
    uint32_t NumberOfRecords;
    uint16_t HeaderLength;
    uint16_t RecordLength;
    uint16_t _reserved1;
    uint8_t _incompleteTransaction;
    uint8_t _encryptionFlag;
    uint32_t _freeRecordThread;
    uint64_t _reserved2;
    uint8_t _mdxFlag;
    uint8_t LanguageDriver;
    uint16_t _reserved4;
} DbfHeader;
#pragma pack(pop)

#define DbfHeader_Size 32

typedef struct DbfField {
    char Name[12];
    uint8_t Type;
    uint8_t Length;
    uint8_t DecimalCount;
    uint32_t Address;
} DbfField;

#define DbfField_StructSize 32

int DbfHeader_Read(FILE* reader, DbfHeader* out);
void DbfHeader_ToBytes(const DbfHeader* header, uint8_t out[DbfHeader_Size]);
DbfField DbfField_Create(const char* name, uint8_t type, uint8_t length, uint8_t decimalCount);
int DbfField_ReadAll(FILE* reader, DbfField** outFields, int* outCount);
const char* DbfCodePage_GetEncodingName(uint8_t languageDriver);
int DbfCodePage_GetLanguageDriver(const char* encodingWebName); /* -1 = null */

typedef struct DbfFile DbfFile;

DbfFile* DbfFile_New(const char* filePath, int exclusive, LockingStrategy lockingStrategy);
void DbfFile_Dispose(DbfFile* self);
int64_t DbfFile_GetFileLength(const DbfFile* self);
int DbfFile_ReadRawRecord(DbfFile* self, int recNo, int* isDeleted, uint8_t** data, int* dataLen);
int DbfFile_ReadRawRecordInto(DbfFile* self, int recNo, uint8_t* buffer, int bufferLen, int* isDeleted);
int DbfFile_WriteRawRecord(DbfFile* self, int recNo, const uint8_t* rawData, int rawLen);
int DbfFile_AppendRawRecord(DbfFile* self, const uint8_t* rawData, int rawLen);
int DbfFile_AppendRawRecordsBatch(DbfFile* self, const uint8_t* rawPayload, int recordPayloadLength, int count);
int DbfFile_ReloadHeaderFromDisk(DbfFile* self);
int DbfFile_FlushPendingHeader(DbfFile* self);
int DbfFile_RestoreHeader(DbfFile* self, const DbfHeader* header);
int DbfFile_FlushStream(DbfFile* self, int durable, int flushHeaderFirst);
int DbfFile_SetDeletedFlag(DbfFile* self, int recNo, int isDeleted);
int DbfFile_CloneStructure(DbfFile* self, const char* newFilePath);
int DbfFile_Zap(DbfFile* self);
int DbfFile_CreateNew(const char* filePath, const char* encodingWebName,
                      const DbfField* fields, int fieldCount);
const DbfHeader* DbfFile_get_Header(const DbfFile* self);
const DbfField* DbfFile_get_Fields(const DbfFile* self, int* count);
const char* DbfFile_get_FilePath(const DbfFile* self);

/* C#: DbfFieldWriter / FieldParser — valore come stringa (REPLACE). */
void DbfFieldWriter_Write(uint8_t* dest, int destLen, const DbfField* field, const char* value, MemoFile* memoFile);
void DbfFieldWriter_WriteCharacter(uint8_t* dest, int destLen, const char* value);
void DbfFieldWriter_WriteNumeric(uint8_t* dest, int destLen, const char* value, int decimals);
void DbfFieldWriter_WriteDate(uint8_t* dest, int destLen, const char* value);
void DbfFieldWriter_WriteLogical(uint8_t* dest, int destLen, const char* value);
void DbfFieldWriter_WriteFloat(uint8_t* dest, int destLen, const char* value, int decimals);
void DbfFieldWriter_WriteInteger(uint8_t* dest, int destLen, const char* value);
void DbfFieldWriter_WriteDateTime(uint8_t* dest, int destLen, const char* value);
void DbfFieldWriter_WriteCurrency(uint8_t* dest, int destLen, const char* value);
void DbfFieldWriter_WriteDouble(uint8_t* dest, int destLen, const char* value);
void DbfFieldWriter_WriteMemo(uint8_t* dest, int destLen, const DbfField* field, const char* value, MemoFile* memoFile);
void DbfFieldWriter_WriteMemoIndex(uint8_t* dest, int destLen, int blockIndex);
void DbfFieldWriter_WriteBlank(uint8_t* dest, int destLen, const DbfField* field, MemoFile* memoFile);
int DbfFieldWriter_SelfCheck(void);
int DbfFieldParser_Parse(const DbfField* field, const uint8_t* data, int len, MemoFile* memo, char* out, int cap);
int DbfFieldParser_ParseMemoIndex(const uint8_t* data, int len);
int DbfFieldParser_SelfCheck(void);

/* C#: DbfTransactionalFileHandle — WAL + DbfFile. IntraProcess auto-commit salta WAL. */
typedef struct DbfTxHandle DbfTxHandle;
DbfTxHandle* DbfTxHandle_New(const char* filePath, int exclusive, LockingStrategy lockingStrategy);
void DbfTxHandle_Dispose(DbfTxHandle* self);
DbfFile* DbfTxHandle_GetDbfFile(DbfTxHandle* self);
int DbfTxHandle_UseTransactionalWal(const DbfTxHandle* self);
void DbfTxHandle_SetDurability(DbfTxHandle* self, DbfDurabilityMode mode);
DbfDurabilityMode DbfTxHandle_GetDurability(const DbfTxHandle* self);
void DbfTxHandle_BeginBulkLoad(DbfTxHandle* self);
void DbfTxHandle_EndBulkLoad(DbfTxHandle* self);
void DbfTxHandle_BeginGroupCommit(DbfTxHandle* self);
void DbfTxHandle_EndGroupCommit(DbfTxHandle* self);
int64_t DbfTxHandle_BeginTransaction(DbfTxHandle* self);
int DbfTxHandle_CommitTransaction(DbfTxHandle* self);
int DbfTxHandle_RollbackTransaction(DbfTxHandle* self);
void DbfTxHandle_FinalizeCommit(DbfTxHandle* self);
void DbfTxHandle_FinalizeAutoCommit(DbfTxHandle* self);
void DbfTxHandle_FlushDurability(DbfTxHandle* self);
int DbfTxHandle_WriteRawRecord(DbfTxHandle* self, int recNo, const uint8_t* rawData, int rawLen);
int DbfTxHandle_AppendRawRecord(DbfTxHandle* self, const uint8_t* rawData, int rawLen);
int DbfTxHandle_SetDeletedFlag(DbfTxHandle* self, int recNo, int isDeleted);
int DbfTxHandle_Zap(DbfTxHandle* self);
int DbfTxHandle_IsExplicitTransactionActive(const DbfTxHandle* self);

/* SPEED WAL opcodes (Config.cs) */
typedef enum SpeedWalOperationType {
    SpeedWalOperationType_Set = 1,
    SpeedWalOperationType_Delete = 2
} SpeedWalOperationType;

#define Config_ManifestFileName "manifest.json"
#define Config_WalDirectoryName "wal"
#define Config_SstDirectoryName "tables"

/* ISAM on-disk constants (Constants.cs) */
#define Config_MagicString "ISAM_V10"
#define Config_HeaderSize (8 * 1024)
#define Config_FileHeaderFieldsPrefixSize 216
#define Config_RecordStatusActive 0x20
#define Config_RecordStatusDeleted 0x2A
#define Config_MaxKeysInNode 128
extern const uint8_t Config_MagicBytes[8];

#define IsamPageChecksum_PageSize 4096
#define IsamPageChecksum_ChecksumOffset (IsamPageChecksum_PageSize - 4)
void IsamPageChecksum_WriteChecksum(uint8_t* page, int pageLen);
int IsamPageChecksum_TryValidate(const uint8_t* page, int pageLen, uint32_t* storedChecksum);

int64_t HeapRelocation_AlignUp(int64_t value, int alignment);

#define SstFooter_MagicV2 0x5350544600000002LL
#define SstFooter_V2FooterSize ((int)(sizeof(int64_t) * 3))
int SstFooter_Read(FILE* stream, int64_t* indexOffset, int64_t* bloomOffset, int* hasV2Footer);
void SstFooter_WriteV2(FILE* writer, int64_t bloomOffset, int64_t indexOffset);
int SstFooter_TryValidateOffsets(int64_t streamLength, int64_t indexOffset, int64_t bloomOffset,
                                 int hasV2Footer, char* error, int errorCap);

#define DeadlockMonitor_PollIntervalMs 25
void DeadlockMonitor_BindTransaction(int threadId, int64_t transactionId);
void DeadlockMonitor_UnbindTransaction(int threadId);
void DeadlockMonitor_ReleaseAllLocksForThread(int threadId);
void DeadlockMonitor_SetWaiting(int threadId, const char* resourceKey, int wantsWrite);
void DeadlockMonitor_ClearWaiting(int threadId);
int DeadlockMonitor_IsAbortRequested(int threadId);
void DeadlockMonitor_RequestAbort(int threadId);
void DeadlockMonitor_ClearAbort(int threadId);
void DeadlockMonitor_RecordWriteAcquired(int threadId, const char* resourceKey);
void DeadlockMonitor_RecordReadAcquired(int threadId, const char* resourceKey);
void DeadlockMonitor_RecordWriteReleased(int threadId, const char* resourceKey);
void DeadlockMonitor_RecordReadReleased(int threadId, const char* resourceKey);
int DeadlockMonitor_TryResolveDeadlock(int waiterThreadId, const char* resourceKey, int wantsWrite,
                                       int* victimThreadId);

void IsamSlowOpLog_LogDeadlock(const char* detail);

typedef struct IntraProcessLockManager IntraProcessLockManager;
IntraProcessLockManager* IntraProcessLockManager_New(void);
void IntraProcessLockManager_Dispose(IntraProcessLockManager* self);
int IntraProcessLockManager_AcquireReadLock(IntraProcessLockManager* self, const char* resourceKey,
                                            int timeoutMilliseconds, const char* resourceDescription);
int IntraProcessLockManager_AcquireWriteLock(IntraProcessLockManager* self, const char* resourceKey,
                                             int timeoutMilliseconds, const char* resourceDescription);
void IntraProcessLockManager_ReleaseReadLock(IntraProcessLockManager* self, const char* resourceKey);
void IntraProcessLockManager_ReleaseWriteLock(IntraProcessLockManager* self, const char* resourceKey);
void IntraProcessLockManager_ReleaseAllHeld(IntraProcessLockManager* self);

typedef struct CrossProcessLockManager CrossProcessLockManager;
CrossProcessLockManager* CrossProcessLockManager_New(const char* filePath);
void CrossProcessLockManager_Dispose(CrossProcessLockManager* self);
int CrossProcessLockManager_AcquireReadLock(CrossProcessLockManager* self, const char* resourceKey,
                                            int timeoutMilliseconds, const char* resourceDescription);
int CrossProcessLockManager_AcquireWriteLock(CrossProcessLockManager* self, const char* resourceKey,
                                             int timeoutMilliseconds, const char* resourceDescription);
void CrossProcessLockManager_ReleaseReadLock(CrossProcessLockManager* self, const char* resourceKey);
void CrossProcessLockManager_ReleaseWriteLock(CrossProcessLockManager* self, const char* resourceKey);
int CrossProcessLockManager_SelfCheck(void);

typedef struct LruCache LruCache;
LruCache* LruCache_New(int capacity);
void LruCache_Dispose(LruCache* self);
int LruCache_TryGetValue(LruCache* self, uint64_t key, void** value);
void LruCache_Pin(LruCache* self, uint64_t key);
void LruCache_Unpin(LruCache* self, uint64_t key);
void LruCache_AddOrUpdate(LruCache* self, uint64_t key, void* value);
void LruCache_Remove(LruCache* self, uint64_t key);
void LruCache_Clear(LruCache* self);

typedef struct IsamLruCache IsamLruCache;
#define IsamLruCache_DefaultCapacity 128
IsamLruCache* IsamLruCache_New(void);
void IsamLruCache_Dispose(IsamLruCache* self);
int IsamLruCache_TryGetValue(IsamLruCache* self, uint64_t key, void** value);
void IsamLruCache_AddOrUpdate(IsamLruCache* self, uint64_t key, void* value);
void IsamLruCache_Pin(IsamLruCache* self, uint64_t key);
void IsamLruCache_Unpin(IsamLruCache* self, uint64_t key);
void IsamLruCache_Remove(IsamLruCache* self, uint64_t key);
void IsamLruCache_Clear(IsamLruCache* self);

typedef struct BloomFilter BloomFilter;
#define BloomFilter_DefaultBitsPerKey 10
#define BloomFilter_HashCount 2
int BloomFilter_BitsPerKeyForCount(int expectedKeyCount);
BloomFilter* BloomFilter_New(int expectedKeyCount);
void BloomFilter_Dispose(BloomFilter* self);
void BloomFilter_Add(BloomFilter* self, const uint8_t* key, int keyLen);
int BloomFilter_MayContain(BloomFilter* self, const uint8_t* key, int keyLen);
void BloomFilter_WriteTo(BloomFilter* self, FILE* writer);
BloomFilter* BloomFilter_ReadFrom(FILE* reader);

int ByteArrayEqualityComparer_Equals(const uint8_t* x, int xLen, const uint8_t* y, int yLen);
int ByteArrayEqualityComparer_GetHashCode(const uint8_t* obj, int len);
int ByteArrayComparer_Compare(const uint8_t* x, int xLen, const uint8_t* y, int yLen);

typedef struct Memtable Memtable;
Memtable* Memtable_New(void);
void Memtable_Dispose(Memtable* self);
int Memtable_Set(Memtable* self, const uint8_t* key, int keyLen, const uint8_t* value, int valueLen);
int Memtable_Delete(Memtable* self, const uint8_t* key, int keyLen);
int Memtable_Get(Memtable* self, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen, int* isTombstone);
int64_t Memtable_get_EstimatedSize(const Memtable* self);
int Memtable_get_KeyCount(const Memtable* self);
int Memtable_Nth(const Memtable* self, int index, uint8_t** key, int* keyLen, uint8_t** value, int* valueLen, int* isTombstone);

typedef struct SpeedWriteAheadLog SpeedWriteAheadLog;
SpeedWriteAheadLog* SpeedWriteAheadLog_New(const char* databasePath, uint64_t logSequenceNumber);
void SpeedWriteAheadLog_Dispose(SpeedWriteAheadLog* self);
const char* SpeedWriteAheadLog_GetWalPath(const SpeedWriteAheadLog* self);
void SpeedWriteAheadLog_LogSet(SpeedWriteAheadLog* self, int tableId, const uint8_t* key, int keyLen,
                               const uint8_t* value, int valueLen);
void SpeedWriteAheadLog_LogDelete(SpeedWriteAheadLog* self, int tableId, const uint8_t* key, int keyLen);
void SpeedWriteAheadLog_Flush(SpeedWriteAheadLog* self);
int SpeedWriteAheadLog_Recover(const char* databasePath, Memtable*** memtables, int** tableIds, int* count);

int SstWriter_WriteFromMemtable(const char* filePath, Memtable* memtable);

typedef struct SstReader SstReader;
SstReader* SstReader_Open(const char* filePath);
void SstReader_Dispose(SstReader* self);
const char* SstReader_GetFilePath(const SstReader* self);
int SstReader_HasBloomFilter(const SstReader* self);
int SstReader_TryFind(SstReader* self, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen);
int SstReader_Rewind(SstReader* self);
int SstReader_Next(SstReader* self, uint8_t** key, int* keyLen, uint8_t** value, int* valueLen);
int SstReader_TryValidateFooter(const char* filePath, char* error, int errorCap);
int SstReader_SelfCheck(void);

typedef struct Manifest Manifest;
Manifest* Manifest_Load(const char* databasePath);
void Manifest_Dispose(Manifest* self);
int Manifest_Save(Manifest* self);
uint64_t Manifest_GetNextFileNumber(Manifest* self);
int Manifest_CreateTable(Manifest* self, const char* typeName);
int Manifest_TryGetTable(const Manifest* self, int tableId, char* nameOut, int nameCap);
int Manifest_AddSstFile(Manifest* self, int tableId, const char* sstFileName);
int Manifest_CollectSstNewestFirst(const Manifest* self, int tableId, char names[][256], int max);
int Manifest_CollectSstOldestFirst(const Manifest* self, int tableId, char names[][256], int max);
int Manifest_TryGetTableByName(const Manifest* self, const char* name, int* idOut);
int Manifest_GetFilesForLevel(const Manifest* self, int tableId, int level, char names[][256], int max);
int Manifest_UpdateAfterCompaction(Manifest* self, int tableId, int sourceLevel,
                                   char compacted[][256], int compactedN, int destLevel, const char* newFile);
int Manifest_ReplaceLevelsAfterCompaction(Manifest* self, int tableId,
                                          char l0[][256], int n0, char l1[][256], int n1,
                                          int destLevel, const char* newFile);
int Manifest_ReplaceAllSstAfterMajorCompact(Manifest* self, int tableId, int destLevel,
                                            const char* newFile);
int Manifest_AddSecondaryIndex(Manifest* self, const char* mainTableName, const char* indexName,
                               const char* keyExpression, int keyLength);
int Manifest_SecondaryKeyCount(const Manifest* self, int tableId);
int Manifest_GetSecondaryKey(const Manifest* self, int tableId, int i,
                             char* nameOut, int nameCap, char* exprOut, int exprCap, int* keyLength);
void Manifest_IndexTableName(const char* mainTableName, const char* indexName, char* out, int cap);
int Manifest_CountSstFiles(const Manifest* self, int tableId);
int64_t Manifest_GetRowCount(const Manifest* self, int tableId);
int Manifest_IsRowCountTrusted(const Manifest* self, int tableId);
int Manifest_AddField(Manifest* self, int tableId, const char* name, const char* typeName, int length);
int Manifest_FieldCount(const Manifest* self, int tableId);
int Manifest_GetField(const Manifest* self, int tableId, int i,
                      char* nameOut, int nameCap, char* typeOut, int typeCap, int* length);
int Manifest_FindField(const Manifest* self, int tableId, const char* name,
                       char* typeOut, int typeCap, int* length);
int Manifest_AddFilesToGarbageCollection(Manifest* self, const char* const* files, int n);
int Manifest_GetFilesForGarbageCollection(const Manifest* self, char names[][256], int max);
int Manifest_RemoveFileFromGarbageCollection(Manifest* self, const char* fileName);
int Manifest_PrepareTableForDeletion(Manifest* self, const char* tableName,
                                     int* tableIdOut, char files[][256], int max);
int Manifest_RemoveSecondaryIndex(Manifest* self, const char* mainTableName, const char* indexName);
void Manifest_SetRowCount(Manifest* self, int tableId, int64_t count);
void Manifest_AdjustRowCount(Manifest* self, int tableId, int64_t delta);
int Manifest_TableCount(const Manifest* self);
int Manifest_GetTableIdAt(const Manifest* self, int index, int* idOut);

#define Database_CompactLevel0Threshold 4
#define Compactor_CompactLevelThreshold 10
#define Database_MemtableThreshold (4 * 1024 * 1024)

typedef struct DatabaseKv {
    const uint8_t* key;
    int keyLen;
    const uint8_t* value;
    int valueLen;
} DatabaseKv;

typedef struct Database Database;
Database* Database_Open(const char* path);
void Database_Dispose(Database* self);
const char* Database_GetPath(const Database* self);
Manifest* Database_GetManifest(Database* self);
SstReader* Database_SstGetOrAdd(Database* self, const char* path);
void Database_SstCacheRemove(Database* self, const char* path);
int Database_CreateTable(Database* self, const char* typeName);
int Database_CreateTableWithFields(Database* self, const char* typeName,
                                   const char* const* names, const char* const* types,
                                   const int* lengths, int fieldCount);
int Database_DropTableInternal(Database* self, const char* tableName);
int Database_Set(Database* self, int tableId, const uint8_t* key, int keyLen, const uint8_t* value, int valueLen);
int Database_SetMany(Database* self, int tableId, const DatabaseKv* entries, int count);
int Database_Delete(Database* self, int tableId, const uint8_t* key, int keyLen);
int Database_Get(Database* self, int tableId, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen);
void Database_ForceFlushTableSync(Database* self, int tableId);
void Database_TriggerCompactionChain(Database* self, int tableId);
void Database_FlushDurability(Database* self);
void Database_SetDurability(Database* self, int full);   /* full=1 -> fsync per set; 0 -> differito */
int Database_GetDurabilityFull(const Database* self);
int Database_GetGroupCommitDepth(const Database* self);
void Database_BeginGroupCommit(Database* self);
int Database_EndGroupCommit(Database* self);
void Database_BeginTransaction(Database* self);
int Database_Commit(Database* self);
void Database_Rollback(Database* self);
typedef struct DatabaseScan DatabaseScan;
DatabaseScan* Database_Scan(Database* self, int tableId);
DatabaseScan* Database_ScanRange(Database* self, int tableId,
                                 const uint8_t* startKey, int startKeyLen,
                                 const uint8_t* endKey, int endKeyLen);
int Database_ScanNext(DatabaseScan* scan, uint8_t** key, int* keyLen, uint8_t** value, int* valueLen);
void Database_ScanDispose(DatabaseScan* scan);
int Database_SetOrder(Database* self, int tableId, const char* indexName);
int Database_GoTop(Database* self, int tableId);
int Database_GoBottom(Database* self, int tableId);
int Database_Skip(Database* self, int tableId, int recordCount);
int Database_Seek(Database* self, int tableId, const uint8_t* key, int keyLen, const char* indexName);
int Database_GetCurrent(Database* self, int tableId, uint8_t** value, int* valueLen);
int Database_GetCurrentKey(Database* self, int tableId, const uint8_t** key, int* keyLen);
int Database_Eof(const Database* self, int tableId);
int Database_Bof(const Database* self, int tableId);
int Database_DropView(Database* self, const char* viewName);
int Database_DropIndex(Database* self, int mainTableId, const char* indexName);
int Database_EvalIndexKey(Database* self, int tableId, const char* expression,
                          const uint8_t* udt, int udtLen, uint8_t** out, int* outLen);
void Database_RebuildManifestRowCounts(Database* self);
void Database_MaybeRebuildUntrustedRowCounts(Database* self);
void Database_AdjustRowCountOnSet(Database* self, int tableId, const uint8_t* key, int keyLen);
void Database_AdjustRowCountOnDelete(Database* self, int tableId, const uint8_t* key, int keyLen);
int Database_ForceCompactTable(Database* self, const char* tableName);
int Database_CompactAll(Database* self);
int Database_CollectGarbage(Database* self);
int Database_CreateSecondaryIndex(Database* self, int mainTableId, const char* indexName,
                                  const char* keyExpression, int keyLength);
int Database_ValidateIntegrity(Database* self, char* message, int messageCap);
int Database_ValidateIntegrityQuick(Database* self, char* message, int messageCap);
int Database_ValidateIntegrityFull(Database* self, char* message, int messageCap);
int Database_IsMainDataTable(Database* self, int tableId);
void Database_MaintainSecondaryIndexesOnPut(Database* self, int tableId,
                                            const uint8_t* pk, int pkLen,
                                            const uint8_t* oldVal, int oldLen,
                                            const uint8_t* newVal, int newLen);
void Database_MaintainSecondaryIndexesOnDelete(Database* self, int tableId,
                                               const uint8_t* pk, int pkLen,
                                               const uint8_t* oldVal, int oldLen);
int Database_Indexes_SelfCheck(void);

#define QueryOptimizer_MaxEqualities 8
typedef struct QueryOptimizerParsedEquality {
    char FieldName[64];
    char TableAlias[64];
    char RawValue[256];
} QueryOptimizerParsedEquality;
typedef struct QueryOptimizerIndexAccessPlan {
    char IndexName[64];
    int IsPrimaryKey;
    int EstimatedSstFiles;
    int64_t EstimatedRows;
} QueryOptimizerIndexAccessPlan;
int QueryOptimizer_TryParseSimpleEquality(const char* expression, const char* expectedTableAlias,
                                          QueryOptimizerParsedEquality* out);
int QueryOptimizer_TryParseCompoundAndEqualities(const char* expression, const char* expectedTableAlias,
                                                 QueryOptimizerParsedEquality* out, int cap, int* nOut);
int QueryOptimizer_PredicateMatchesIndex(const char* fieldName, const char* indexName, const char* keyExpression);
void QueryOptimizer_CoerceLiteral(const char* rawValue, char* out, int cap);
int QueryOptimizer_SelectBestIndex(const Manifest* manifest, int tableId,
                                   const QueryOptimizerParsedEquality* predicates, int predicateN,
                                   const char* hintFieldName, QueryOptimizerIndexAccessPlan* out);
int QueryOptimizer_ResolveJoinIndex(const Manifest* manifest, int tableId, const char* joinFieldName,
                                    char* indexNameOut, int cap);
int Database_TrySeekIndexedEquality(Database* self, int tableId, const char* whereClause,
                                    uint8_t** value, int* valueLen);
int QueryOptimizer_SelfCheck(void);

#define QUERYPLAN_MAX_PROJ 16
#define QUERYPLAN_MAX_ORDER 4
#define QUERYPLAN_MAX_GROUP 4
typedef struct QueryProjection {
    char ExpressionString[64];
    char Alias[64];
} QueryProjection;
typedef struct QuerySource {
    char TableName[128];
    char Alias[64];
} QuerySource;
typedef struct QueryJoin {
    int Present;
    QuerySource RightSource;
    char LeftKeyExpressionString[64];
    char RightIndexName[64];
} QueryJoin;
typedef struct QueryOrderBy {
    char ExpressionString[64];
    int Descending;
} QueryOrderBy;
typedef struct QueryPlan {
    QueryProjection Projections[QUERYPLAN_MAX_PROJ];
    int ProjectionN;
    char DestinationVariableName[64];
    char DestinationUdtName[64];
    QuerySource FromSource;
    QueryJoin JoinClause;
    char WhereClauseString[256];
    char GroupByClauseStrings[QUERYPLAN_MAX_GROUP][64];
    int GroupByN;
    char HavingClauseString[256];
    QueryOrderBy OrderByClauses[QUERYPLAN_MAX_ORDER];
    int OrderByN;
} QueryPlan;
typedef struct QueryResultCell {
    char Alias[64];
    uint8_t* Bytes;
    int Len;
} QueryResultCell;
typedef struct QueryResultRow {
    QueryResultCell Cells[QUERYPLAN_MAX_PROJ];
    int CellN;
    uint8_t* PrimaryKey;
    int PrimaryKeyLen;
} QueryResultRow;
typedef struct QueryResult {
    QueryResultRow* Rows;
    int N;
} QueryResult;

/* C#: Basic.Databases.Speed.ExpressionEvaluator
 * (string expressionString, Dictionary<string, object?> rowContext) → object?
 * Dummy = hook SPEED (default evaluator). Il body è gmb_expr_eval_filter. */
typedef struct SpeedEvalValue {
    int isNull;
    int isBool;
    int truth;
    int isFloat;
    int64_t i64;
    double f64;
    uint8_t* bytes;
    int len;
} SpeedEvalValue;
void SpeedEvalValue_Clear(SpeedEvalValue* v);
typedef int (*SpeedExpressionEvaluator)(const char* expressionString,
                                        const QueryResultRow* rowContext,
                                        void* user,
                                        SpeedEvalValue* out);
int SpeedExpressionEvaluator_Dummy(const char* expressionString,
                                   const QueryResultRow* rowContext,
                                   void* user,
                                   SpeedEvalValue* out);
void Database_SetIndexKeyEvaluator(Database* self, int mainTableId,
                                   SpeedExpressionEvaluator evaluator, void* evalUser);

void QueryPlan_Init(QueryPlan* plan);
int QueryPlan_AddProjection(QueryPlan* plan, const char* expression, const char* alias);
void QueryResult_Dispose(QueryResult* result);
int Database_ExecuteQuery(Database* self, const QueryPlan* plan, QueryResult* out);
int Database_ExecuteQueryEval(Database* self, const QueryPlan* plan,
                              SpeedExpressionEvaluator evaluator, void* evalUser,
                              QueryResult* out);
int Database_RefreshView(Database* self, const char* viewName, const QueryPlan* plan);
int Database_RefreshViewEval(Database* self, const char* viewName, const QueryPlan* plan,
                             SpeedExpressionEvaluator evaluator, void* evalUser);
int QueryPlan_SelfCheck(void);

typedef enum SpeedDataTypeKind {
    SpeedDataType_Unknown = 0,
    SpeedDataType_String = 1,
    SpeedDataType_Bit = 2,
    SpeedDataType_Byte = 3,
    SpeedDataType_SByte = 4,
    SpeedDataType_Integer = 5,
    SpeedDataType_UInteger = 6,
    SpeedDataType_Long = 7,
    SpeedDataType_ULong = 8,
    SpeedDataType_Quad = 9,
    SpeedDataType_UQuad = 10,
    SpeedDataType_Single = 11,
    SpeedDataType_Double = 12,
    SpeedDataType_Currency = 13,
    SpeedDataType_Date = 14,
    SpeedDataType_DateTime = 15,
    SpeedDataType_Json = 16,
    SpeedDataType_UserDefined = 17
} SpeedDataTypeKind;
SpeedDataTypeKind SpeedDataType_Parse(const char* typeName);
const char* SpeedDataType_Name(SpeedDataTypeKind kind);
int SpeedDataType_IsNumeric(SpeedDataTypeKind kind);
int SpeedMarshaller_ToBytes(SpeedDataTypeKind kind, const uint8_t* src, int srcLen,
                            uint8_t** out, int* outLen);
int SpeedMarshaller_FromBytes(SpeedDataTypeKind kind, const uint8_t* buf, int bufLen,
                              uint8_t** out, int* outLen);
int SpeedMarshaller_SerializeUdt(const Manifest* man, int tableId,
                                 const uint8_t* const* fieldValues, const int* fieldLens,
                                 uint8_t** out, int* outLen);
int SpeedMarshaller_ExtractField(const Manifest* man, int tableId,
                                 const uint8_t* udt, int udtLen, const char* fieldName,
                                 uint8_t** out, int* outLen);
int SpeedMarshaller_SelfCheck(void);

#define SpeedDatabaseLock_LockFileName ".speed.lock"
int SpeedDatabaseLock_Acquire(const char* databasePath);
void SpeedDatabaseLock_Release(const char* databasePath);
int SpeedDatabaseLock_SelfCheck(void);

int Compactor_CompactLevel0(Database* db, int tableId);
int Compactor_CompactLevel(Database* db, int tableId, int level);
int Compactor_CompactLevels01Forced(Database* db, int tableId);
int Compactor_SelfCheck(void);
int Database_SelfCheck(void);

typedef struct NtxFile NtxFile;
NtxFile* NtxFile_New(const char* path, LockingStrategy lockingStrategy);
NtxFile* NtxFile_CreateNew(const char* filePath, const char* keyExpression, int keyLength, int isUnique,
                           LockingStrategy lockingStrategy);
void NtxFile_Dispose(NtxFile* self);
int NtxFile_Seek(NtxFile* self, const uint8_t* keyBytes, int keyLen, int soft);
void NtxFile_Insert(NtxFile* self, const uint8_t* keyBytes, int keyLen, int recno);
const char* NtxFile_GetKeyExpressionString(const NtxFile* self);
const char* NtxFile_get_Name(const NtxFile* self);
int NtxFile_SelfCheck(void);

/* C#: Basic.DBF.NdxFormat / NdxFile — dBASE NDX, pagine 512, mutazioni = rewrite in-memory */
#define NdxFormat_BlockSize 512
#define NdxFormat_ExpressionOffset 24
#define NdxFormat_ExpressionLength 100
int NdxFormat_GroupLength(int keyLength);
uint16_t NdxFormat_MaxKeysPerPage(int keyLength);

typedef struct NdxFile NdxFile;
NdxFile* NdxFile_CreateNew(const char* filePath, const char* keyExpression, int keyLength, int isUnique);
NdxFile* NdxFile_OpenExisting(const char* path, int writable);
void NdxFile_Dispose(NdxFile* self);
int NdxFile_Seek(NdxFile* self, const uint8_t* keyBytes, int keyLen, int soft);
int NdxFile_Insert(NdxFile* self, const uint8_t* keyBytes, int keyLen, int recno);
int NdxFile_Delete(NdxFile* self, const uint8_t* keyBytes, int keyLen, int recno);
int NdxFile_GetFirstRecordNumber(NdxFile* self);
int NdxFile_GetLastRecordNumber(NdxFile* self);
const char* NdxFile_GetKeyExpressionString(const NdxFile* self);
const char* NdxFile_get_Name(const NdxFile* self);
int NdxFile_get_KeyLength(const NdxFile* self);
int NdxFile_get_Unique(const NdxFile* self);
int NdxFile_get_EntryCount(const NdxFile* self);
int NdxFile_get_RecnoAt(const NdxFile* self, int index);
int NdxFile_SelfCheck(void);
int NdxBuilder_Write(const char* path, const char* keyExpression, int keyLength, int unique,
                     const uint8_t* const* keys, const int* keyLens, const int* recnos, int n);

/* C#: Basic.DBF.MdxFormat / MdxCompound / MdxTagIndex */
#define MdxFormat_HeaderSize 512
#define MdxFormat_TagTableOffset 544
#define MdxFormat_TagEntrySize 48
#define MdxFormat_TagNameLength 10
#define GMB_MDX_MAX_TAGS 32

typedef struct {
    char Name[16];
    uint32_t HeaderPage;
    uint32_t RootPage;
    int KeyLength;
    char Expression[NdxFormat_ExpressionLength + 1];
    int Unique;
} MdxTagDescriptor;

typedef struct {
    const char* TagName;
    const char* Expression;
    int KeyLength;
    int Unique;
    const uint8_t* const* Keys;
    const int* KeyLens;
    const int* Recnos;
    int NEntries;
} MdxTagWrite;

int MdxCompound_ReadTagTable(const char* path, MdxTagDescriptor* out, int cap, int* nOut);
int MdxCompound_WriteCompound(const char* path, const char* dbfBaseName, const MdxTagWrite* tags, int nTags);

typedef struct MdxTagIndex MdxTagIndex;
MdxTagIndex* MdxTagIndex_CreateOrReplace(const char* compoundPath, const char* dbfBaseName,
                                         const char* tagName, const char* keyExpression, int keyLength, int isUnique);
MdxTagIndex* MdxTagIndex_Open(const char* compoundPath, const char* tagName, int writable);
void MdxTagIndex_Dispose(MdxTagIndex* self);
int MdxTagIndex_Seek(MdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int soft);
int MdxTagIndex_Insert(MdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno);
int MdxTagIndex_Delete(MdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno);
const char* MdxTagIndex_get_Name(const MdxTagIndex* self);
const char* MdxTagIndex_GetKeyExpressionString(const MdxTagIndex* self);
int MdxTagIndex_get_KeyLength(const MdxTagIndex* self);
int MdxCompound_SelfCheck(void);

/* C#: Basic.DBF.CdxFormat / CdxCompound / CdxTagIndex */
#define CdxFormat_PageSize 512
#define CdxFormat_HeaderSize 512
#define CdxFormat_ExtNodeHeaderSize 24
#define CdxFormat_IntNodeHeaderSize 12
#define CdxFormat_TypeCompound 0x40
#define CdxFormat_TypeCompact 0x20
#define CdxFormat_TagNameLength 10
#define GMB_CDX_MAX_TAGS 32
#define GMB_CDX_MAX_KEY 256

typedef struct {
    char Name[16];
    uint32_t RootOffset;
    uint32_t HeaderOffset;
    int KeyLength;
    char Expression[495];
    int Unique;
    int Descending;
} CdxTagDescriptor;

typedef struct {
    const char* TagName;
    const char* Expression;
    int KeyLength;
    int Unique;
    int Descending;
    const uint8_t* const* Keys;
    const int* KeyLens;
    const int* Recnos;
    int NEntries;
} CdxTagWrite;

int CdxCompound_ReadTagTable(const char* path, CdxTagDescriptor* out, int cap, int* nOut);
int CdxCompound_WriteCompound(const char* path, const CdxTagWrite* tags, int nTags);

typedef struct CdxTagIndex CdxTagIndex;
CdxTagIndex* CdxTagIndex_CreateOrReplace(const char* compoundPath, const char* tagName,
                                         const char* keyExpression, int keyLength, int isUnique, int descending);
CdxTagIndex* CdxTagIndex_Open(const char* compoundPath, const char* tagName, int writable);
void CdxTagIndex_Dispose(CdxTagIndex* self);
int CdxTagIndex_Seek(CdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int soft);
int CdxTagIndex_Insert(CdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno);
int CdxTagIndex_Delete(CdxTagIndex* self, const uint8_t* keyBytes, int keyLen, int recno);
const char* CdxTagIndex_get_Name(const CdxTagIndex* self);
const char* CdxTagIndex_GetKeyExpressionString(const CdxTagIndex* self);
int CdxTagIndex_get_KeyLength(const CdxTagIndex* self);
int CdxCompound_SelfCheck(void);

int CdxCompact_MaxKeysPerLeaf(int keyLength);
int CdxCompact_MaxKeysPerInterior(int keyLength);
int CdxCompact_BuildLeafPage(uint8_t* page, const uint8_t* keysFlat, const int* recnos, int n, int keyLength);
int CdxCompact_BuildInteriorPage(uint8_t* page, const uint32_t* children, int nChildren,
                                 const uint8_t* sepsFlat, int nSep, int keyLength);
int CdxCompact_DecodeLeafKeys(const uint8_t* page, int keyCount, int keyLength,
                              uint8_t* keysFlat, int* recnos, int cap, int* nOut);
uint32_t CdxCompact_GetInteriorChild(const uint8_t* page, int keyIndex, int keyCount, int keyLength);
void CdxCompact_GetInteriorKey(const uint8_t* page, int index, int keyLength, uint8_t* out);
void CdxCompact_SetInteriorChild(uint8_t* page, int index, int keyCount, int keyLength, uint32_t newChild);

typedef struct CdxPageCache CdxPageCache;
CdxPageCache* CdxPageCache_New(const char* filePath, int maxPages);
void CdxPageCache_Dispose(CdxPageCache* self);
int CdxPageCache_GetPage(CdxPageCache* self, uint32_t offset, uint8_t* out512);
void CdxPageCache_PutPage(CdxPageCache* self, uint32_t offset, const uint8_t* data, int dirty);
void CdxPageCache_MarkDirty(CdxPageCache* self, uint32_t offset);
uint32_t CdxPageCache_AllocatePage(CdxPageCache* self, const uint8_t* data);
int CdxPageCache_Flush(CdxPageCache* self);
void CdxPageCache_Clear(CdxPageCache* self);
int CdxPageCache_get_Count(const CdxPageCache* self);
int CdxBTreeBuilder_InsertEntry(CdxPageCache* cache, uint32_t rootOffset,
                                const uint8_t* key, int keyLen, int recno, int keyLength, int unique,
                                uint32_t* newRootOut);
void CdxTagIndex_EnablePageCache(CdxTagIndex* self);
int CdxPageCache_SelfCheck(void);

/* C#: Basic.DBF.IndexFactory + Clipper 5.x NTX (Harbour layout, pagine 1024) */
#define ClipperNtxHeader_PageSize 1024
#define ClipperNtxHeader_KeyExpressionOffset 22
#define ClipperNtxHeader_UniqueFlagOffset 278
#define ClipperNtxHeader_DescendingFlagOffset 280
#define GMB_INDEX_FACTORY_MAX_HANDLES 32
#define GMB_CLIPPER_MAX_ENTRIES 4096
#define GMB_CLIPPER_MAX_KEY 256
#define GMB_CLIPPER_MAX_PAGES 512

int ClipperNtxHeader_IsClipperType(uint16_t type);
uint32_t ClipperNtxHeader_ResolveBlockAddress(uint32_t block);
uint16_t ClipperNtxBuilder_CalcMaxKeys(int keyLength);

typedef struct ClipperNtxFile ClipperNtxFile;
ClipperNtxFile* ClipperNtxFile_New(const char* path);
void ClipperNtxFile_Dispose(ClipperNtxFile* self);
int ClipperNtxFile_Seek(ClipperNtxFile* self, const uint8_t* keyBytes, int keyLen, int soft);
int ClipperNtxFile_GetFirstRecordNumber(ClipperNtxFile* self);
int ClipperNtxFile_GetLastRecordNumber(ClipperNtxFile* self);
const char* ClipperNtxFile_GetKeyExpressionString(const ClipperNtxFile* self);
const char* ClipperNtxFile_get_Name(const ClipperNtxFile* self);
int ClipperNtxFile_get_KeyLength(const ClipperNtxFile* self);
int ClipperNtxFile_get_Unique(const ClipperNtxFile* self);
int ClipperNtxFile_get_Descending(const ClipperNtxFile* self);
int ClipperNtxFile_EnumerateEntries(ClipperNtxFile* self, uint8_t* keysFlat, int* recnos, int cap, int* nOut);

int ClipperNtxBuilder_Write(const char* path, const char* keyExpression, int keyLength,
                            const uint8_t* keysFlat, const int* recnos, int n, int unique, int descending);

typedef struct ClipperNtxWritable ClipperNtxWritable;
ClipperNtxWritable* ClipperNtxWritable_CreateNew(const char* filePath, const char* keyExpression,
                                                 int keyLength, int isUnique);
ClipperNtxWritable* ClipperNtxWritable_OpenExisting(const char* path);
void ClipperNtxWritable_Dispose(ClipperNtxWritable* self);
int ClipperNtxWritable_Seek(ClipperNtxWritable* self, const uint8_t* keyBytes, int keyLen, int soft);
int ClipperNtxWritable_Insert(ClipperNtxWritable* self, const uint8_t* keyBytes, int keyLen, int recno);
int ClipperNtxWritable_Delete(ClipperNtxWritable* self, const uint8_t* keyBytes, int keyLen, int recno);
int ClipperNtxWritable_GetFirstRecordNumber(ClipperNtxWritable* self);
int ClipperNtxWritable_GetLastRecordNumber(ClipperNtxWritable* self);
const char* ClipperNtxWritable_GetKeyExpressionString(const ClipperNtxWritable* self);
const char* ClipperNtxWritable_get_Name(const ClipperNtxWritable* self);
int ClipperNtxWritable_get_Unique(const ClipperNtxWritable* self);
int ClipperNtxWritable_get_EntryCount(const ClipperNtxWritable* self);
int ClipperNtxWritable_get_RecnoAt(const ClipperNtxWritable* self, int index);
int ClipperNtxWritable_get_KeyLength(const ClipperNtxWritable* self);
int ClipperNtxFile_SelfCheck(void);

typedef enum IndexHandleKind {
    IndexHandleKind_None = 0,
    IndexHandleKind_Ntx = 1,
    IndexHandleKind_Ndx = 2,
    IndexHandleKind_MdxTag = 3,
    IndexHandleKind_CdxTag = 4,
    IndexHandleKind_ClipperNtx = 5,
    IndexHandleKind_ClipperNtxWritable = 6
} IndexHandleKind;

typedef struct IndexHandle {
    IndexHandleKind Kind;
    char Path[1024];
    NtxFile* Ntx;
    NdxFile* Ndx;
    MdxTagIndex* Mdx;
    CdxTagIndex* Cdx;
    ClipperNtxFile* Clipper;
    ClipperNtxWritable* ClipperW;
} IndexHandle;

void IndexHandle_Dispose(IndexHandle* self);
const char* IndexHandle_get_Name(const IndexHandle* self);
const char* IndexHandle_get_Path(const IndexHandle* self);
const char* IndexHandle_GetKeyExpressionString(const IndexHandle* self);
int IndexHandle_get_KeyLength(const IndexHandle* self);
int IndexHandle_get_Unique(const IndexHandle* self);
int IndexHandle_Seek(IndexHandle* self, const uint8_t* keyBytes, int keyLen, int soft);
int IndexHandle_Insert(IndexHandle* self, const uint8_t* keyBytes, int keyLen, int recno);
int IndexHandle_Delete(IndexHandle* self, const uint8_t* keyBytes, int keyLen, int recno);
int IndexHandle_GetFirstRecordNumber(IndexHandle* self);
int IndexHandle_GetLastRecordNumber(IndexHandle* self);
int IndexHandle_GetEntryCount(IndexHandle* self);
int IndexHandle_GetRecnoAt(IndexHandle* self, int index);
int IndexFactory_CreateHandles(const char* indexPath, int writable, IndexHandle* out, int cap, int* nOut);
int IndexFactory_Create(const char* indexPath, int writable, IndexHandle* out);
int IndexFactory_SelfCheck(void);

/* C#: Basic.DBF.DbfEngine — work area: memo + lista indici (IndexOn / SET ORDER). */
#define GMB_DBF_ENGINE_MAX_INDEXES 32
typedef int (*DbfEngineKeyProducer)(DbfEngine* eng, int recno, uint8_t* keyOut, int keyCap, int* keyLen, void* ctx);
DbfEngine* DbfEngine_New(const char* filePath, const char* const* indexPaths, int indexPathCount,
                         int exclusive, LockingStrategy lockingStrategy);
void DbfEngine_Dispose(DbfEngine* self);
DbfFile* DbfEngine_get_DbfFile(DbfEngine* self);
MemoFile* DbfEngine_get_MemoFile(DbfEngine* self);
const DbfHeader* DbfEngine_GetHeader(const DbfEngine* self);
const DbfField* DbfEngine_GetFields(const DbfEngine* self, int* count);
int DbfEngine_GetIndexCount(const DbfEngine* self);
const IndexHandle* DbfEngine_GetIndex(const DbfEngine* self, int i); /* 0-based; NULL se fuori */
int DbfEngine_AddIndexPath(DbfEngine* self, const char* indexPath); /* SET INDEX: CreateHandles append */
int DbfEngine_SetActiveOrderName(DbfEngine* self, const char* indexName); /* NULL/"" = ordine fisico */
int DbfEngine_SetActiveOrderNumber(DbfEngine* self, int indexNumber); /* 1-based; 0 = fisico */
int DbfEngine_GetActiveKeyLength(const DbfEngine* self);
int DbfEngine_IndexOn(DbfEngine* self, const char* newIndexPath, const char* keyExpression,
                      int keyLength, int isUnique, const char* tagName,
                      DbfEngineKeyProducer keyProducer, void* keyProducerCtx);
void DbfEngine_GoTop(DbfEngine* self);
void DbfEngine_GoBottom(DbfEngine* self);
void DbfEngine_GoTopInternal(DbfEngine* self);
void DbfEngine_GoBottomInternal(DbfEngine* self);
void DbfEngine_Skip(DbfEngine* self, int recordsToSkip);
void DbfEngine_SkipInternal(DbfEngine* self, int recordsToSkip);
int DbfEngine_IsCurrentRecordValid(const DbfEngine* self);
int DbfEngine_IsCurrentRecordDeleted(DbfEngine* self);
int DbfEngine_PositionAtRecord(DbfEngine* self, int recNo);
int DbfEngine_AppendRecord(DbfEngine* self, const uint8_t* payload, int payloadLen);
int DbfEngine_AppendBlankRecord(DbfEngine* self);
int DbfEngine_AppendManyRecords(DbfEngine* self, const uint8_t* payloads, int payloadLen, int count);
int DbfEngine_PutCurrentRecord(DbfEngine* self, const uint8_t* payload, int payloadLen);
int DbfEngine_ReplaceField(DbfEngine* self, int fieldIndex, const char* value);
int DbfEngine_ReplaceFieldName(DbfEngine* self, const char* fieldName, const char* value);
int DbfEngine_GetFieldText(DbfEngine* self, int fieldIndex, char* out, int cap);
int DbfEngine_GetFieldTextByName(DbfEngine* self, const char* fieldName, char* out, int cap);
int DbfEngine_BeginTransaction(DbfEngine* self);
int DbfEngine_Commit(DbfEngine* self);
int DbfEngine_Rollback(DbfEngine* self);
void DbfEngine_BeginBulkLoad(DbfEngine* self);
void DbfEngine_EndBulkLoad(DbfEngine* self);
void DbfEngine_BeginGroupCommit(DbfEngine* self);
void DbfEngine_EndGroupCommit(DbfEngine* self);
void DbfEngine_FlushDurability(DbfEngine* self);
int DbfEngine_CopyTo(DbfEngine* self, const char* destinationFilePath);
int DbfEngine_SortTo(DbfEngine* self, const char* destinationFilePath, const char* fieldName, int descending);
int DbfEngine_SortToMulti(DbfEngine* self, const char* destinationFilePath, const char* fieldsCsv, const char* descCsv);
int DbfEngine_CloneStructure(DbfEngine* self, const char* newFilePath);
void DbfListFormatter_FormatValue(const char* valueUtf8, int isNull, int isBool, int boolVal,
                                  char* out, int cap);
void DbfListFormatter_FormatLine(int64_t recordNumber, const char* const* values, int nValues,
                                 char* out, int cap);
int DbfEngine_SetFilter(DbfEngine* self, const char* expression); /* NULL/"" rimuove; poi GoTopInternal */
const char* DbfEngine_GetFilterExpression(const DbfEngine* self);
int DbfEngine_EvaluateFilter(DbfEngine* self);
int DbfEngine_EnableVersioning(DbfEngine* self); /* C# default off; .gmv sidecar */
int DbfEngine_TotalOn(DbfEngine* self, const char* destinationFilePath, const char* keyField,
                      const char* const* sumFields, int sumFieldCount);
int DbfEngine_TotalOnCond(DbfEngine* self, const char* destinationFilePath, const char* keyField,
                          const char* const* sumFields, int sumFieldCount, const char* condition);
int DbfEngine_ExportToCsv(DbfEngine* self, const char* filePath);
int DbfEngine_ExportToJson(DbfEngine* self, const char* filePath);
int DbfEngine_ImportFromJson(DbfEngine* self, const char* filePath);
int DbfEngine_ImportFromCsv(DbfEngine* self, const char* filePath);
int DbfEngine_AppendFrom(DbfEngine* self, const char* sourceFilePath);
/* APPEND FROM ... FOR: il filtro (testo BASIC) si applica al file SORGENTE. */
int DbfEngine_AppendFromCond(DbfEngine* self, const char* sourceFilePath, const char* condition);
int DbfEngine_WriteList(DbfEngine* self, FILE* writer, int startAtTop);
int DbfEngine_SetReplication(DbfEngine* self, const char* logPath); /* NULL/"" off; "."/"DEFAULT" → {dbf}.repl */
const char* DbfEngine_GetReplicationLogPath(const DbfEngine* self);
int DbfEngine_ApplyReplication(DbfEngine* self, const char* replLogPath);
int DbfEngine_FollowReplication(DbfEngine* self, const char* replLogPath, const char* checkpointPath);
int DbfEngine_ReplicationShipTcp(DbfEngine* self, const char* host, int port, const char* replLogPath,
                                 const char* shipCheckpointPath);
int DbfEngine_ReplicationRecvTcp(DbfEngine* self, int port, const char* checkpointPath);
int DbfEngine_DeleteCurrent(DbfEngine* self);
int DbfEngine_RecallCurrent(DbfEngine* self);
int DbfEngine_Pack(DbfEngine* self);
int DbfEngine_Zap(DbfEngine* self);
int DbfEngine_Reindex(DbfEngine* self);
void DbfEngine_SetDeleted(DbfEngine* self, int on);
void DbfEngine_SetExact(DbfEngine* self, int on);
int DbfEngine_Seek(DbfEngine* self, const uint8_t* keyBytes, int keyLen);
int DbfEngine_get_Found(const DbfEngine* self);
int DbfEngine_get_Eof(const DbfEngine* self);
int DbfEngine_get_Bof(const DbfEngine* self);
int DbfEngine_GetCurrentRecordNumber(const DbfEngine* self);
uint32_t DbfEngine_GetTotalRecordCount(const DbfEngine* self);
int DbfEngine_SelfCheck(void);
int DbfEngine_ValidateIntegrity(DbfEngine* self);
int DbfEngine_Update(DbfEngine* self, const char* fieldsCsv, const char* exprsCsv);
int DbfEngine_UpdateOn(DbfEngine* dest, DbfEngine* src, const char* srcAlias, const char* keyField, const char* fieldsCsv, const char* exprsCsv);
int DbfEngine_Join(DbfEngine* self, DbfEngine* other, const char* destPath, const char* condition,
                   const char* aliasA, const char* aliasB);
int DbfEngine_SetDurability(DbfEngine* self, DbfDurabilityMode mode);

/* C#: modules/Database/Rdd/DbfDriver — adapter su DbfEngine.
 * SET RELATION = DbfDriver_SetRelation + SyncRelation; JOIN WITH = NestedLoopJoin.
 * DataSession / CoreDb.Relation restano community (common/Databases/Rdd), non in questa lib. */
#define GMB_RDD_MAX_FIELDS 128
typedef struct DbfDriver {
    DbfEngine* engine;
    char alias[32];
    char fileName[1024];
    int ownsEngine;
    int isOpen;
    int eof;
    int bof;
    int found;
    int64_t recNo;
    int64_t recCount;
    DbfField fields[GMB_RDD_MAX_FIELDS];
    int fieldCount;
    struct DbfDriver* parentRelation;
    char relationKey[64];
} DbfDriver;
DbfDriver* DbfDriver_New(void);
DbfDriver* DbfDriver_FromEngine(DbfEngine* engine, const char* alias, int ownsEngine);
void DbfDriver_Dispose(DbfDriver* self);
void DbfDriver_Open(DbfDriver* self, const char* filePath, const char* alias);
void DbfDriver_Close(DbfDriver* self);
void DbfDriver_SyncCursorState(DbfDriver* self);
void DbfDriver_GoTop(DbfDriver* self);
void DbfDriver_GoBottom(DbfDriver* self);
void DbfDriver_Skip(DbfDriver* self, int n);
int DbfDriver_Seek(DbfDriver* self, const char* keyText);
int DbfDriver_GetFieldIndex(const DbfDriver* self, const char* name);
int DbfDriver_GetFieldText(DbfDriver* self, int index, char* out, int cap);
int DbfDriver_get_Eof(const DbfDriver* self);
int DbfDriver_get_Found(const DbfDriver* self);
void DbfDriver_SetRelation(DbfDriver* self, DbfDriver* parent, const char* keyExpression);
int DbfDriver_SyncRelation(DbfDriver* self);
int DbfDriver_NestedLoopJoin(DbfDriver* src, DbfDriver* sec, const char* destPath, const char* condition);
int DbfDriver_CopyTo(DbfDriver* self, const char* destinationFilePath);
int DbfDriver_SortTo(DbfDriver* self, const char* destinationFilePath, const char* fieldName, int descending);
int DbfDriver_Zap(DbfDriver* self);
int DbfDriver_Pack(DbfDriver* self);
int DbfDriver_RddSelfCheck(void);

/* C#: Basic.Databases.Rdd.SpeedDriver — avvolge Database (cursore + indici). */
typedef struct SpeedDriver {
    Database* db;
    int tableId;
    int ownsDb;
    char alias[32];
    char fileName[1024];
    int isOpen;
    int eof;
    int bof;
    int found;
    int64_t recNo;
    int64_t recCount;
    uint8_t* currentKey;
    int currentKeyLen;
    uint8_t* currentValue;
    int currentValueLen;
} SpeedDriver;
SpeedDriver* SpeedDriver_New(void);
SpeedDriver* SpeedDriver_FromDatabase(Database* db, const char* alias, int ownsDb);
void SpeedDriver_Dispose(SpeedDriver* self);
void SpeedDriver_Open(SpeedDriver* self, const char* dir, const char* alias);
void SpeedDriver_Close(SpeedDriver* self);
void SpeedDriver_UseTable(SpeedDriver* self, int tableId);
void SpeedDriver_GoTop(SpeedDriver* self);
void SpeedDriver_GoBottom(SpeedDriver* self);
void SpeedDriver_Skip(SpeedDriver* self, int n);
int SpeedDriver_Seek(SpeedDriver* self, const uint8_t* key, int keyLen);
int SpeedDriver_GetCurrent(SpeedDriver* self, uint8_t** value, int* valueLen);
int SpeedDriver_get_Eof(const SpeedDriver* self);
int SpeedDriver_get_Found(const SpeedDriver* self);
int SpeedDriver_CreateIndex(SpeedDriver* self, const char* name, const char* keyExpression, int keyLength);
int SpeedDriver_DropIndex(SpeedDriver* self, const char* indexName);
int SpeedDriver_SetOrder(SpeedDriver* self, const char* indexName);
void SpeedDriver_Zap(SpeedDriver* self);
int SpeedDriver_Pack(SpeedDriver* self);
int SpeedDriver_Append(SpeedDriver* self, const uint8_t* key, int keyLen, const uint8_t* value, int valueLen);
int SpeedDriver_PutCurrent(SpeedDriver* self, const uint8_t* value, int valueLen);
int SpeedDriver_CreateTableWithFields(SpeedDriver* self, const char* typeName,
                                      const char* const* names, const char* const* types,
                                      const int* lengths, int fieldCount);
int SpeedDriver_SelfCheck(void);

typedef struct Engine Engine;
/* C#: Basic.Databases.Rdd.IsamDriver — avvolge Engine. */
typedef struct IsamDriver {
    Engine* engine;
    int ownsEngine;
    char alias[32];
    char fileName[1024];
    int isOpen;
    int eof;
    int bof;
    int found;
    int64_t recNo;
    int64_t recCount;
} IsamDriver;
IsamDriver* IsamDriver_New(void);
IsamDriver* IsamDriver_FromEngine(Engine* engine, const char* alias, int ownsEngine);
void IsamDriver_Dispose(IsamDriver* self);
void IsamDriver_Open(IsamDriver* self, const char* filePath, const char* alias);
void IsamDriver_Close(IsamDriver* self);
void IsamDriver_SyncCursorState(IsamDriver* self);
void IsamDriver_GoTop(IsamDriver* self);
void IsamDriver_GoBottom(IsamDriver* self);
void IsamDriver_Skip(IsamDriver* self, int n);
int IsamDriver_SeekText(IsamDriver* self, const char* keyText);
int IsamDriver_get_Eof(const IsamDriver* self);
int IsamDriver_get_Found(const IsamDriver* self);
int IsamDriver_SelfCheck(void);

#pragma pack(push, 1)
typedef struct IsamFileHeader {
    uint8_t Magic[8];
    int32_t Version;
    int64_t SchemaOffset;
    int64_t SchemaLength;
    int64_t PrimaryKeyRootOffset;
    int64_t SecondaryIndexMapOffset;
    int64_t FreeListHeadOffset;
    int32_t RecordSize;
    int64_t LastRecNumber;
    int64_t ActiveRecordCount;
    int64_t NextAvailablePageOffset;
    int64_t LobHeapStartOffset;
    int64_t PageFreeListHeadOffset;
    int64_t SecondaryIndexRootOffsets[15];
} IsamFileHeader;
#pragma pack(pop)

void IsamFileHeader_InitEmpty(IsamFileHeader* h, int recordSize);
int IsamFileHeader_Write(FILE* f, const IsamFileHeader* h);
int IsamFileHeader_Read(FILE* f, IsamFileHeader* h);
int IsamFileHeader_ValidateMagic(const IsamFileHeader* h);
int IsamFileHeader_SelfCheck(void);

#define IsamSchema_NameMax 256
#define RecordMetadata_Size 25 /* Pack=1: Status u8 + Version i64 + TxId i64 + PrevOffset i64 */
#define HeapRelocation_PageSize 4096
#define SchemaManager_SchemaMagic "ISCH"

/* C#: public struct RecordMetadata — layout fisso Pack=1 */
#pragma pack(push, 1)
typedef struct RecordMetadata {
    uint8_t Status;
    int64_t Version;
    int64_t TransactionId;
    int64_t PreviousVersionOffset;
} RecordMetadata;
#pragma pack(pop)

/* C#: public enum CursorMoveOperation */
typedef enum CursorMoveOperation {
    CursorMoveOperation_Top = 0,
    CursorMoveOperation_Bottom = 1,
    CursorMoveOperation_Next = 2,
    CursorMoveOperation_Previous = 3
} CursorMoveOperation;

/* C#: UdtInstance field bag — IntegerType in i64, StringType in text / textOut. */
typedef struct IsamFieldValue {
    int64_t i64;
    const char* text;
    char* textOut;
    int textOutCap;
} IsamFieldValue;
int IsamDriver_Append(IsamDriver* self, const IsamFieldValue* fields, int fieldCount);
int IsamDriver_GetCurrent(IsamDriver* self, IsamFieldValue* fields, int fieldCount, RecordMetadata* meta);
int IsamDriver_PutCurrent(IsamDriver* self, const IsamFieldValue* fields, int fieldCount);
void IsamDriver_Zap(IsamDriver* self);
int IsamDriver_Pack(IsamDriver* self);
int IsamDriver_CreateIndex(IsamDriver* self, const char* indexName, const char* keyExpression, int keyLength);
int IsamDriver_SetFilter(IsamDriver* self, const char* expression);
int IsamDriver_CopyTo(IsamDriver* self, const char* destinationFilePath, const char* forCondition);
int IsamDriver_SortTo(IsamDriver* self, const char* destinationFilePath, const char* fieldName, int descending,
                      const char* forCondition);
int IsamDriver_LockCurrentRecord(IsamDriver* self);
void IsamDriver_UnlockCurrentRecord(IsamDriver* self);
int IsamDriver_LockFile(IsamDriver* self);
void IsamDriver_UnlockFile(IsamDriver* self);
#define SchemaManager_SchemaFormatVersion 2

typedef struct IsamIndexDefinition {
    char Name[IsamSchema_NameMax];
    char KeyExpression[IsamSchema_NameMax];
    int KeyLength;
    int IsUnique;
    int IsUnsafe;
    char PartialPredicate[IsamSchema_NameMax];
} IsamIndexDefinition;

typedef struct IsamFieldSchema {
    char Name[IsamSchema_NameMax];
    char DataTypeCode[64];
    int Length;
} IsamFieldSchema;

/* DOUBLE/SINGLE nel bag IsamFieldValue: il campo `i64` porta i BIT IEEE (DoubleType: 8 byte; SingleType:
 * i 32 bit bassi del float), cosi' il marshaller scrive gli stessi byte del C# (BitConverter) senza un
 * secondo membro e il layout su disco resta identico. I consumatori testuali (filtro, chiavi, export)
 * convertono con questi helper invece di stampare l'i64 come intero. */
#include <math.h>
#include <string.h>
static inline int IsamField_IsDouble(const IsamFieldSchema* f)
{ return f && strcmp(f->DataTypeCode, "DoubleType") == 0; }
static inline int IsamField_IsSingle(const IsamFieldSchema* f)
{ return f && strcmp(f->DataTypeCode, "SingleType") == 0; }
static inline int IsamField_IsFloat(const IsamFieldSchema* f)
{ return IsamField_IsDouble(f) || IsamField_IsSingle(f); }
static inline double IsamField_BitsToDouble(const IsamFieldSchema* f, int64_t bits)
{
    if (IsamField_IsSingle(f)) { uint32_t u = (uint32_t)bits; float x; memcpy(&x, &u, 4); return (double)x; }
    { double d; memcpy(&d, &bits, 8); return d; }
}
static inline int64_t IsamField_DoubleToBits(const IsamFieldSchema* f, double d)
{
    if (IsamField_IsSingle(f)) { float x = (float)d; uint32_t u; memcpy(&u, &x, 4); return (int64_t)u; }
    { int64_t b; memcpy(&b, &d, 8); return b; }
}
/* Testo del numero con round-trip minimo (19.99 -> "19.99"; Single via precisione float). */
static inline void IsamField_FormatFloat(const IsamFieldSchema* f, int64_t bits, char* out, int cap)
{
    double d = IsamField_BitsToDouble(f, bits);
    int prec = IsamField_IsSingle(f) ? 6 : 15;
    snprintf(out, (size_t)cap, "%.*g", prec, d);
    if (IsamField_IsSingle(f)) { if ((float)strtod(out, NULL) != (float)d) snprintf(out, (size_t)cap, "%.9g", d); }
    else if (strtod(out, NULL) != d) snprintf(out, (size_t)cap, "%.17g", d);
}

typedef struct IsamSchema {
    char TypeName[IsamSchema_NameMax];
    IsamFieldSchema* Fields;
    int FieldCount;
    IsamIndexDefinition PrimaryKey;
    IsamIndexDefinition* SecondaryKeys;
    int SecondaryKeyCount;
} IsamSchema;
int HeapRelocation_GetJsonFieldOffsets(const IsamSchema* schema, int64_t* out, int cap);

int SchemaManager_GetFieldSize(const char* typeCode, int length);
int SchemaManager_CalculateRecordSize(const IsamSchema* schema);
int SchemaManager_SerializeIndexMap(const char** names, const int64_t* offsets, int count,
                                    uint8_t** out, int* outLen);
int SchemaManager_SerializeSchema(const IsamSchema* schema, uint8_t** out, int* outLen);
int SchemaManager_DeserializeSchema(const uint8_t* buf, int len, IsamSchema* schema,
                                    IsamFieldSchema* fields, int fieldCap,
                                    IsamIndexDefinition* secs, int secCap);
int SchemaManager_SerializeKeyValueI64(int64_t value, int keyLength, uint8_t* out);
int SchemaManager_SerializeKeyValueText(const char* text, int keyLength, uint8_t* out);
int IsamFileHandle_BuildNewFileBytes(const IsamSchema* schema, uint8_t** out, int* outLen);
int IsamFileHandle_CreateNew(const char* filePath, const IsamSchema* schema);
int IsamFileHandle_SelfCheck(void);

typedef struct IsamFileHandle {
    FILE* fs;
    char FilePath[1024];
    IsamFileHeader Header;
    const IsamSchema* Schema;
    int64_t heapRegionStart;
    int64_t dataAreaStart;
    int FoundFlag;
    int ownsFile;
    void (*AfterHeapRelocated)(void* ctx, int64_t heapStart, int64_t heapEnd, int64_t delta);
    void* afterHeapCtx;
    uint8_t* mmapPtr;
    size_t mmapLen;
    int useMmapReadOnly;
    IsamFileHeader txSnapshotHeader;
    int hasTxSnapshot;
    int64_t currentTxId;
    int64_t nextTxId;
    int64_t* undoGridOff;
    int64_t* undoHeapOff;
    int undoN;
    int undoCap;
} IsamFileHandle;

IsamFileHandle* IsamFileHandle_Open(const char* filePath, const IsamSchema* schema);
void IsamFileHandle_Dispose(IsamFileHandle* self);
int64_t IsamFileHandle_DataAreaStartOffset(const IsamFileHandle* self);
int64_t IsamFileHandle_AllocateNewRecordOffsetCore(IsamFileHandle* self);
/* Port di FileHandle.AllocateNewRecordOffsetBatchCore: alloca `count` slot consecutivi
 * (free-list + sequenziale) con un SOLO EnsureHeapStartsAtOrAbove/ftruncate/UpdateHeader. */
int IsamFileHandle_AllocateNewRecordOffsetBatchCore(IsamFileHandle* self, int64_t* offsets, int count);
int IsamFileHandle_WriteRecordFromBuffer(IsamFileHandle* self, int64_t offset, const uint8_t* buf, int len);
int IsamFileHandle_ReadRecordBytes(IsamFileHandle* self, int64_t offset, uint8_t* buf, int len);
int IsamFileHandle_ReadRecordMetadata(IsamFileHandle* self, int64_t offset, RecordMetadata* out);
int IsamFileHandle_WriteRecordMetadata(IsamFileHandle* self, int64_t offset, const RecordMetadata* meta);
int64_t IsamFileHandle_ReserveHeapSpace(IsamFileHandle* self, int64_t size);
int64_t IsamFileHandle_WriteLob(IsamFileHandle* self, const uint8_t* data, int len);
int IsamFileHandle_ReadLob(IsamFileHandle* self, int64_t offset, char* out, int cap);
int64_t IsamFileHandle_WriteUndoVersion(IsamFileHandle* self, int64_t currentOffset);
int64_t IsamFileHandle_BeginTransaction(IsamFileHandle* self);
int IsamFileHandle_CommitTransaction(IsamFileHandle* self);
int IsamFileHandle_RollbackTransaction(IsamFileHandle* self);
int IsamFileHandle_UpdateHeader(IsamFileHandle* self);
int IsamFileHandle_Flush(IsamFileHandle* self);
int IsamFileHandle_ReadBytesAt(IsamFileHandle* self, int64_t offset, uint8_t* buf, int len);
int IsamFileHandle_EnableMmapReadOnly(IsamFileHandle* self);
void IsamFileHandle_DisableMmapReadOnly(IsamFileHandle* self);
int IsamFileHandle_get_UseMmapReadOnly(const IsamFileHandle* self);
int IsamFileHandle_EnsureSchemaSlot(IsamFileHandle* self, int requiredSchemaBytes);
/* Port di FileHandle.RelocateHeapRegion / EnsureHeapStartsAtOrAbove: sposta la regione
 * heap (pagine B+ + LOB) per mantenerla oltre l'area record, aggiornando i puntatori. */
int64_t IsamFileHandle_RelocateHeapRegion(IsamFileHandle* self, int64_t newStart);
int IsamFileHandle_EnsureHeapStartsAtOrAbove(IsamFileHandle* self, int64_t requiredStart);
void IsamFileHandle_RegisterHeapAllocation(IsamFileHandle* self, int64_t start);
int IsamFileHandle_WriteSchemaBlockForReplication(IsamFileHandle* self, const uint8_t* isch, int len);

int RecordMarshaller_SerializeToBuffer(const IsamSchema* schema, const RecordMetadata* meta,
                                       const IsamFieldValue* fields, int fieldCount,
                                       uint8_t* buffer, int bufferLen);
int RecordMarshaller_DeserializeFromBuffer(const IsamSchema* schema, const uint8_t* buffer, int bufferLen,
                                           RecordMetadata* meta, IsamFieldValue* fields, int fieldCount);
int RecordMarshaller_WriteRecordWithMetadata(IsamFileHandle* fileHandle, int64_t offset,
                                             const IsamFieldValue* fields, int fieldCount,
                                             const RecordMetadata* meta);

int BPlusFillFactor_ComputeLeafSplitIndex(int keyCount);
int BPlusFillFactor_ComputeInternalSplitIndex(int keyCount);
int BPlusFillFactor_ComputeMinKeys(int maxKeys);

typedef struct BPlusTreeManager BPlusTreeManager;
int IsamIntegrityChecker_ValidatePageFreeList(IsamFileHandle* self, char* err, int errCap);
int IsamIntegrityChecker_ValidateActiveRecordPointers(IsamFileHandle* self, BPlusTreeManager* primary, char* err, int errCap);
int IsamIntegrityChecker_CountRecords(BPlusTreeManager* primary, int64_t* out);
BPlusTreeManager* BPlusTreeManager_New(const char* filePath, int keyLength, int isUnique);
BPlusTreeManager* BPlusTreeManager_NewOnHandle(IsamFileHandle* handle, int keyLength, int isUnique);
BPlusTreeManager* BPlusTreeManager_NewOnHandleAt(IsamFileHandle* handle, int keyLength, int isUnique,
                                                 int secondarySlot);
void BPlusTreeManager_Dispose(BPlusTreeManager* self);
int BPlusTreeManager_GetMaxKeysPerNode(const BPlusTreeManager* self);
int BPlusTreeManager_GetMaxKeysForInternalNode(const BPlusTreeManager* self);
int BPlusTreeManager_Insert(BPlusTreeManager* self, const uint8_t* key, int keyLen, int64_t recordOffset);
/* Bulk append di chiavi strettamente crescenti (una SaveNode per pagina piena). */
int BPlusTreeManager_InsertManyAppendOrdered(BPlusTreeManager* self, const uint8_t* const* keys,
                                             const int* keyLens, const int64_t* offsets, int n);
/* Copia l'ultima chiave dell'indice in out; ritorna la lunghezza, 0 se vuoto, <0 errore. */
int BPlusTreeManager_GetLastKey(BPlusTreeManager* self, uint8_t* out, int cap);
int BPlusTreeManager_Delete(BPlusTreeManager* self, const uint8_t* key, int keyLen, int64_t recordOffset);
int BPlusTreeManager_Search(BPlusTreeManager* self, const uint8_t* key, int keyLen,
                            int64_t* offsets, int cap, int* count);
int BPlusTreeManager_SearchLowerBound(BPlusTreeManager* self, const uint8_t* key, int keyLen,
                                      int64_t* offsets, int cap, int* count);
int64_t BPlusTreeManager_GetRootOffset(const BPlusTreeManager* self);
int64_t BPlusTreeManager_GetFirstRecordOffset(BPlusTreeManager* self);
int64_t BPlusTreeManager_GetLastRecordOffset(BPlusTreeManager* self);
int64_t BPlusTreeManager_FindNextRecordOffset(BPlusTreeManager* self, int64_t currentRecordOffset, int skipCount);
int BPlusTreeManager_SelfCheck(void);
void BPlusTreeManager_ReloadRootFromHeader(BPlusTreeManager* self);
int BPlusTreeManager_RepairPointersAfterHeapMove(BPlusTreeManager* self,
                                                 int64_t heapStart, int64_t heapEnd, int64_t delta);
int BPlusTreeManager_VisitRecordOffsets(BPlusTreeManager* self, int (*cb)(void* ctx, int64_t offset), void* ctx);

void BPlusTreeManager_AddTreeToFreeList(BPlusTreeManager* self);

Engine* Engine_Open(const char* filePath, const IsamSchema* schema);
IsamFileHandle* Engine_GetHandle(Engine* self);
BPlusTreeManager* Engine_GetPrimaryIndex(Engine* self);
int Engine_EnableMmapReadOnly(Engine* self);
int Engine_get_UseMmapReadOnly(const Engine* self);
void Engine_Dispose(Engine* self);
int Engine_Append(Engine* self, const IsamFieldValue* fields, int fieldCount);
int Engine_Go(Engine* self, CursorMoveOperation operation);
int Engine_Skip(Engine* self, int direction);
int Engine_Seek(Engine* self, const uint8_t* key, int keyLen);
int Engine_SeekI64(Engine* self, int64_t keyValue);
int Engine_SeekText(Engine* self, const char* text);
int Engine_JoinWith(Engine* left, Engine* right, const char* destPath,
                    const char* leftField, const char* rightField);
int Engine_LastJoinExecutionMode(const Engine* self);
int Engine_GetCurrent(Engine* self, IsamFieldValue* fields, int fieldCount, RecordMetadata* meta);
int Engine_Put(Engine* self, const IsamFieldValue* fields, int fieldCount);
int Engine_DeleteCurrent(Engine* self);
int Engine_RecallCurrent(Engine* self);
int Engine_Pack(Engine* self);
int Engine_Reindex(Engine* self);
int Engine_ApplyReplicatedSchema(Engine* self, const uint8_t* isch, int len);
int Engine_SetOrder(Engine* self, const char* indexName);
int Engine_CreateIndex(Engine* self, const char* indexName, const char* keyExpression, int keyLength);
int Engine_DropIndex(Engine* self, const char* indexName);
int Engine_SetFilter(Engine* self, const char* expression);
int Engine_LastFilterUsedSeekPushdown(const Engine* self);
int Engine_IsEof(const Engine* self);
int Engine_IsBof(const Engine* self);
int Engine_WasFound(const Engine* self);
void Engine_SetFoundFlag(Engine* self, int found);
int64_t Engine_GetCurrentRecordNumber(const Engine* self);
int64_t Engine_GetTotalRecordCount(const Engine* self);
const IsamSchema* Engine_GetSchema(const Engine* self);
int Engine_CreateSnapshot(Engine* self, const char* backupFilePath);
int Engine_PositionAtRecord(Engine* self, int64_t recNo);
int64_t Engine_GetLastPhysicalRecNumber(const Engine* self);
int Engine_SetReplication(Engine* self, const char* logPath);
int Engine_SelfCheck(void);
int Engine_CopyTo(Engine* self, const char* destinationFilePath, const char* forCondition);
int Engine_SortTo(Engine* self, const char* destinationFilePath, const char* fieldName, int descending,
                  const char* forCondition);
int Engine_ExportToCsv(Engine* self, const char* filePath);
int Engine_ImportFromCsv(Engine* self, const char* filePath);
int Engine_AppendMany(Engine* self, const IsamFieldValue* records, int recordCount, int fieldCount);
int Engine_BeginTransaction(Engine* self);
int Engine_Commit(Engine* self);
void Engine_Rollback(Engine* self);
int Engine_HasActiveTransaction(const Engine* self);
int Engine_LockCurrentRecord(Engine* self);
void Engine_UnlockCurrentRecord(Engine* self);
int Engine_TryLockCurrentRecord(Engine* self);
int Engine_LockFile(Engine* self);
void Engine_UnlockFile(Engine* self);
int Engine_TryLockFile(Engine* self);
int Engine_LockFileInProcess(Engine* self);
int Engine_Zap(Engine* self);
int Engine_CopySort_SelfCheck(void);

int IsamFileHandle_TryLockRecord(IsamFileHandle* self, int64_t offset);
void IsamFileHandle_UnlockRecord(IsamFileHandle* self, int64_t offset);
int IsamFileHandle_LockFile(IsamFileHandle* self);
void IsamFileHandle_UnlockFile(IsamFileHandle* self);
int IsamFileHandle_TryLockFile(IsamFileHandle* self);

int IsamReplicationApplier_Apply(const char* replLogPath, Engine* target);
int IsamReplicationApplier_Follow(const char* replLogPath, Engine* target, const char* checkpointPath);
int IsamReplicationApplier_ApplyLine(const char* line, ReplicationApplyState* state, Engine* target);
int IsamReplicationApplier_SelfCheck(void);

typedef struct IsamQuery IsamQuery;
IsamQuery* IsamQuery_From(Engine* engine);
void IsamQuery_Dispose(IsamQuery* self);
int IsamQuery_Where(IsamQuery* self, const char* predicate);
int IsamQuery_OrderBy(IsamQuery* self, const char* indexName);
int IsamQuery_Take(IsamQuery* self, int count);
int IsamQuery_Count(IsamQuery* self);
int64_t IsamQuery_Sum(IsamQuery* self, const char* fieldName);
int IsamQuery_ToList(IsamQuery* self, int64_t* ids, int cap);
int IsamQuery_Execute(IsamQuery* self, int64_t* ids, int cap);
int IsamQuery_LastUsedSeekPushdown(const IsamQuery* self);
int IsamQuery_SelfCheck(void);

int IsamBackupRestore_BackupIncremental(const char* sourceIsdb, const char* destinationDirectory);
int IsamBackupRestore_Restore(const char* backupDirectory, const char* targetIsdb);
int IsamBackupRestore_SelfCheck(void);

int IsamRepair_PrepareForOpen(const char* filePath);
int IsamRepair_SelfCheck(void);

int IsamOfflineMigrator_MigrateIfNeeded(const char* filePath, const IsamSchema* codeSchema);
int IsamOfflineMigrator_SelfCheck(void);

int IsamOfflineToolkit_RecoverWal(const char* filePath);
int IsamOfflineToolkit_Repair(const char* filePath);
int IsamOfflineToolkit_ReadHeader(const char* filePath, IsamFileHeader* out);
Engine* IsamOfflineToolkit_OpenEngine(const char* filePath);
int IsamOfflineToolkit_ApplyReplicationLog(const char* replLogPath, const char* targetIsdbPath);
int IsamOfflineToolkit_FollowReplicationLog(const char* replLogPath, const char* targetIsdbPath,
                                            const char* checkpointPath);
int IsamOfflineToolkit_SelfCheck(void);

int IsamExportImport_ExportJson(const char* sourceIsdb, const char* outputJsonPath, const IsamSchema* schema);
int IsamExportImport_ImportJson(const char* destinationIsdb, const char* inputJsonPath);
int IsamExportImport_ExportDbf(const char* sourceIsdb, const char* outputDbfPath, const IsamSchema* schema);
int IsamExportImport_ImportDbf(const char* sourceDbf, const char* destinationIsdb,
                               const char* primaryKeyFieldName, int primaryKeyLength);
int IsamExportImport_SelfCheck(void);

#define IsamMemPath_Prefix "MEM:"
int IsamMemPath_IsMemPath(const char* path);
int IsamMemPath_NormalizeKey(const char* memPath, char* out, int cap);
int IsamMemPath_FromTableName(const char* tableName, char* out, int cap);
int IsamMemStore_Exists(const char* key);
int IsamMemStore_CreateNew(const char* key, const IsamSchema* schema);
int IsamMemStore_BackingPath(const char* key, char* out, int cap);
int IsamMemStore_Remove(const char* key);
int IsamMemStore_SelfCheck(void);

int SnapshotManager_CreateSnapshot(IsamFileHandle* handle, const char* backupFilePath);
int SnapshotManager_SelfCheck(void);

typedef struct IsamThreadCursor {
    int64_t CurrentRecordOffset;
    int EofFlag;
    int BofFlag;
    int64_t LastReadRecordVersion;
    char ActiveFilterExpression[512];
    uint8_t* FilterPushdownSeekKey;
    int FilterPushdownSeekKeyLen;
    int FilterPushdownMatchLength;
} IsamThreadCursor;
void IsamThreadCursor_Init(IsamThreadCursor* c);
void IsamThreadCursor_Dispose(IsamThreadCursor* c);
int IsamThreadCursor_SelfCheck(void);

int IsamKeyExpressionEvaluator_TryEvaluateBuiltIn(
    const char* expression, const IsamSchema* schema, const IsamFieldValue* fields,
    char* outText, int textCap, int64_t* outI64, int* isText);
int IsamExpressionBridge_TryEvaluateKey(
    Engine* engine, const char* expression, const IsamFieldValue* fields,
    char* outText, int textCap, int64_t* outI64, int* isText);
int IsamExpressionBridge_SelfCheck(void);

int IsamQueryPredicate_TryParseEquality(const char* predicate, char* fieldName, int fieldCap,
                                        char* value, int valueCap, int* isNum);
int IsamQueryPredicate_IsSimpleFieldKeyExpression(const char* keyExpression, const char* fieldName);

#define IsamJoinStrategy_HashJoinMaxBuildRecords 10000
typedef enum IsamJoinMode {
    IsamJoinMode_NestedLoop = 0,
    IsamJoinMode_IndexNestedLoop = 1,
    IsamJoinMode_HashJoin = 2
} IsamJoinMode;
int IsamJoinStrategy_IndexCoversField(const IsamIndexDefinition* def, const char* fieldName);
int IsamJoinStrategy_FieldValuesEqual(const IsamFieldValue* a, int aText, const IsamFieldValue* b, int bText);
int IsamJoinStrategy_SerializeFieldKey(const IsamFieldValue* v, int isText, int keyLength, uint8_t* out);
void IsamJoinStrategy_HashKey(const IsamFieldValue* v, int isText, char* out, int cap);

#define MvccUndoGc_MaxRetainedChainDepth 1
void MvccUndoGc_PruneAfterGridWrite(IsamFileHandle* handle, int64_t gridOffset);
void MvccUndoGc_ReclaimTransactionalUndoSlots(IsamFileHandle* handle, const int64_t* undoOffsets, int count);

/* C#: Basic.Ledger.LedgerIndexRecord / LedgerEntry / LedgerContext */
typedef struct {
    int64_t FileOffset;
    int32_t Date;
} LedgerIndexRecord;

typedef struct {
    int64_t TransactionId;
    int DateY, DateM, DateD;
    char Status;
    const char* Description;
    const char* Account;
    GmbDecimal Amount;
    const char* Comment;
    int AmountIsExplicit;
    const char* Currency;
    double ExchangeRate;
} LedgerEntry;

typedef struct LedgerContext LedgerContext;

LedgerContext* LedgerContext_New(const char* journalPath);
void LedgerContext_Dispose(LedgerContext* self);
const char* LedgerContext_JournalFilePath(const LedgerContext* self);
const char* LedgerContext_IndexFilePath(const LedgerContext* self);
const char* LedgerContext_BudgetFilePath(const LedgerContext* self);
int LedgerContext_IsDirty(const LedgerContext* self);
int LedgerContext_IsBudgetsDirty(const LedgerContext* self);

int LedgerManager_SaveIndexToFile(const char* indexPath, const LedgerContext* ctx);
int LedgerManager_ReadIndexFromFile(const char* indexPath, LedgerContext* ctx);
int LedgerManager_IsTransactionBalanced(LedgerEntry* entries, int n);
int LedgerManager_FormatAmountForJournal(GmbDecimal amount, const char* currency, double exchangeRate,
                                         char* out, int cap);
int64_t LedgerManager_AddTransaction(LedgerContext* ctx, int year, int month, int day,
                                     const char* description, LedgerEntry* entries, int n);
int64_t LedgerManager_UpdateTransaction(LedgerContext* ctx, int64_t txId, int year, int month, int day,
                                        const char* description, LedgerEntry* entries, int n);
int LedgerManager_IndexCount(const LedgerContext* ctx);
int LedgerManager_GetSortedIndexIds(const LedgerContext* ctx, int64_t* out, int max, int* nOut);
int LedgerManager_TryGetIndex(const LedgerContext* ctx, int64_t txId, LedgerIndexRecord* out);
int LedgerManager_GetTransaction(LedgerContext* ctx, int64_t txId, LedgerEntry* out, int cap, int* nOut);
int LedgerManager_DeleteTransaction(LedgerContext* ctx, int64_t txId);
int LedgerManager_RebuildIndexFromFile(const char* journalPath, LedgerContext* ctx);
int LedgerManager_PackJournal(LedgerContext* ctx);

#define GMB_LEDGER_NAME 128
typedef struct {
    int hasDate;
    char dateOp[3]; /* ">", ">=", "<", "<=", "=" */
    int32_t dateVal;
    int hasAccount;
    char accountPat[GMB_LEDGER_NAME];
} LedgerQueryFilter;

int LedgerManager_ParseLedgerQuery(const char* query, LedgerQueryFilter* out);
int LedgerManager_LoadChartOfAccounts(const char* coaPath, LedgerContext* ctx);
int LedgerManager_GetFilteredEntries(LedgerContext* ctx, const char* query, LedgerEntry* out, int cap, int* nOut);
int LedgerManager_GetAccounts(LedgerContext* ctx, const char* rootAccount,
                              char names[][GMB_LEDGER_NAME], int maxNames, int* nOut);
int LedgerManager_SelfCheck(void);
int LedgerManager_Query_SelfCheck(void);

typedef struct {
    char Account[GMB_LEDGER_NAME];
    char Period[16];
    GmbDecimal BudgetAmount;
    GmbDecimal ActualAmount;
    GmbDecimal Variance;
} LedgerBudgetVariance;

int LedgerManager_LoadBudgets(const char* budgetPath, LedgerContext* ctx);
int LedgerManager_SaveBudgets(const char* budgetPath, LedgerContext* ctx);
int LedgerManager_SetBudget(LedgerContext* ctx, const char* account, GmbDecimal amount, const char* period);
int LedgerManager_GetVariance(LedgerContext* ctx, const char* account, const char* period,
                              LedgerBudgetVariance* out);
int LedgerManager_Budget_SelfCheck(void);

int LedgerManager_ImportCsv(LedgerContext* ctx, const char* csvPath, const char* mappingRules, int* importedOut);
int LedgerManager_GetBalancePlotData(LedgerContext* ctx, const char* query,
                                     char labels[][GMB_LEDGER_NAME], double* amounts, int max, int* nOut);
int LedgerManager_Csv_SelfCheck(void);
int LedgerManager_AppendAuditLogEntry(LedgerContext* ctx, const char* description);
int LedgerManager_Audit_SelfCheck(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_DB_PORT_H */
