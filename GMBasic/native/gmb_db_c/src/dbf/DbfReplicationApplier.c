/* FILE: native/gmb_db_c/src/DBF/DbfReplicationApplier.c */
/* Port 1:1 di modules/Database/DBF/DbfReplicationApplier.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfReplicationApplier
//
//using Basic.Databases.Replication;
//
//namespace Basic.DBF;
//
//internal static class DbfReplicationApplier
//{
//    public static int Apply(string replLogPath, DbfEngine target)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
//        ArgumentNullException.ThrowIfNull(target);
//        if (!File.Exists(replLogPath))
//        {
//            throw new FileNotFoundException("Replication log not found.", replLogPath);
//        }
//
//        target.SetReplication(string.Empty);
//        var state = new ReplicationApplyState();
//        var applied = 0;
//        foreach (var line in File.ReadLines(replLogPath))
//        {
//            if (ApplyLine(line, state, target))
//            {
//                applied++;
//            }
//        }
//
//        return applied;
//    }
//
//    public static int Follow(string replLogPath, DbfEngine target, string? checkpointPath = null)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
//        ArgumentNullException.ThrowIfNull(target);
//        target.SetReplication(string.Empty);
//        checkpointPath ??= ReplicationLogFollower.DefaultCheckpointPath(replLogPath);
//        return ReplicationLogFollower.FollowOnce(replLogPath, (state, line) => ApplyLine(line, state, target), checkpointPath);
//    }
//
//    internal static bool ApplyLine(string line, ReplicationApplyState state, DbfEngine target)
//    {
//        if (string.IsNullOrWhiteSpace(line))
//        {
//            return false;
//        }
//
//        if (!ReplicationLogLine.TryParse(line, out var op, out var primaryRecNo, out var values))
//        {
//            throw new InvalidDataException($"Invalid replication line: {line}");
//        }
//
//        switch (op)
//        {
//            case ReplicationOp.TxnBegin:
//                state.InTxn = true;
//                state.PendingTxnLines.Clear();
//                return false;
//            case ReplicationOp.TxnCommit:
//                state.InTxn = false;
//                var applied = 0;
//                foreach (var pending in state.PendingTxnLines)
//                {
//                    if (!ReplicationLogLine.TryParse(pending, out var pendingOp, out var pendingRecNo, out var pendingValues))
//                    {
//                        throw new InvalidDataException($"Invalid replication line: {pending}");
//                    }
//
//                    if (ApplyDataLine(pending, state, target, pendingOp, pendingRecNo, pendingValues))
//                    {
//                        applied++;
//                    }
//                }
//
//                state.PendingTxnLines.Clear();
//                return applied > 0;
//            default:
//                if (state.InTxn)
//                {
//                    state.PendingTxnLines.Add(line);
//                    return false;
//                }
//
//                return op switch
//                {
//                    ReplicationOp.Pack => ApplyPack(state, target, primaryRecNo),
//                    ReplicationOp.Schema => ApplySchema(state, target, values),
//                    ReplicationOp.Reindex => ApplyReindex(state, target, primaryRecNo),
//                    _ => ApplyDataLine(line, state, target, op, primaryRecNo, values),
//                };
//        }
//    }
//
//    private static bool ApplyPack(ReplicationApplyState state, DbfEngine target, long generation)
//    {
//        if (generation <= state.LastPackGeneration)
//        {
//            return false;
//        }
//
//        target.Pack();
//        state.ResetAfterPack(generation);
//        return true;
//    }
//
//    private static bool ApplySchema(ReplicationApplyState state, DbfEngine target, Dictionary<string, object?>? values)
//    {
//        if (values is null)
//        {
//            throw new InvalidDataException("SCHEMA requires metadata payload.");
//        }
//
//        var generation = ReplicationSchemaMetadata.RequireGeneration(values);
//        if (generation <= state.LastSchemaGeneration)
//        {
//            return false;
//        }
//
//        var gmsBytes = ReplicationSchemaMetadata.RequireBase64(values, ReplicationSchemaMetadata.GmsKey);
//        DbfSchemaManager.WriteSchemaBytes(target.FilePath, gmsBytes);
//        state.MarkSchemaApplied(generation);
//        return true;
//    }
//
//    private static bool ApplyReindex(ReplicationApplyState state, DbfEngine target, long generation)
//    {
//        if (generation <= state.LastReindexGeneration)
//        {
//            return false;
//        }
//
//        target.Reindex();
//        state.MarkReindexApplied(generation);
//        return true;
//    }
//
//    private static bool ApplyDataLine(string line, ReplicationApplyState state, DbfEngine target, ReplicationOp op, long primaryRecNo, Dictionary<string, object?>? values)
//    {
//        switch (op)
//        {
//            case ReplicationOp.Append:
//                if (TrySkipExistingAppend(target, state.RecMap, primaryRecNo) || !state.AppliedAppends.Add(primaryRecNo))
//                {
//                    return false;
//                }
//
//                var appendValues = values is null ? BlankValuesFromSchema(target) : ReplicationLogLine.NormalizeValues(values);
//                target.AppendRecord(appendValues, target.CalculateKeys(appendValues, target));
//                state.RecMap[primaryRecNo] = target.GetCurrentRecordNumber();
//                return true;
//            case ReplicationOp.Put:
//                if (values is null)
//                {
//                    throw new InvalidDataException($"PUT requires values: {line}");
//                }
//
//                var putValues = ReplicationLogLine.NormalizeValues(values);
//                var putRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
//                target.PositionAtRecord((int)putRecNo);
//                var oldValues = target.GetCurrentRecordValues() ?? throw new InvalidOperationException($"Record {putRecNo} not found for PUT.");
//                target.PutCurrentRecord(putValues, target.CalculateKeys(oldValues, target), target.CalculateKeys(putValues, target));
//                return true;
//            case ReplicationOp.Delete:
//                var deleteRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
//                target.PositionAtRecord((int)deleteRecNo);
//                var deleteValues = target.GetCurrentRecordValues() ?? throw new InvalidOperationException($"Record {deleteRecNo} not found for DELETE.");
//                target.DeleteCurrent(target.CalculateKeys(deleteValues, target));
//                return true;
//            case ReplicationOp.Recall:
//                var recallRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
//                target.PositionAtRecord((int)recallRecNo);
//                var recallValues = target.GetCurrentRecordValues() ?? throw new InvalidOperationException($"Record {recallRecNo} not found for RECALL.");
//                target.RecallCurrent(target.CalculateKeys(recallValues, target));
//                return true;
//            default:
//                throw new InvalidDataException($"Unsupported replication op: {op}");
//        }
//    }
//
//    private static long ResolveRecNo(IReadOnlyDictionary<long, long> recMap, long primaryRecNo) =>
//        recMap.TryGetValue(primaryRecNo, out var mapped) ? mapped : primaryRecNo;
//
//    private static bool TrySkipExistingAppend(DbfEngine target, Dictionary<long, long> recMap, long primaryRecNo)
//    {
//        if (primaryRecNo > target.GetTotalRecordCount())
//        {
//            return false;
//        }
//
//        target.PositionAtRecord((int)primaryRecNo);
//        if (target.GetCurrentRecordValues() is null)
//        {
//            return false;
//        }
//
//        recMap[primaryRecNo] = primaryRecNo;
//        return true;
//    }
//
//    private static Dictionary<string, object?> BlankValuesFromSchema(DbfEngine engine)
//    {
//        var values = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//        foreach (var field in engine.GetFields())
//        {
//            values[field.Name] = DbfFieldWriter.GetBlankValue(field.Type);
//        }
//
//        return values;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <stdio.h>
#include <unistd.h>

/* C#: DbfReplicationApplier — Apply/Follow/ApplyLine + PACK/SCHEMA/REINDEX. */

static int dbf_json_get(const char* json, const char* key, char* out, int cap)
{
    char pat[64];
    const char* p;
    int n = 0;
    if (!json || !key || !out) return GMB_DB_ARG;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(json, pat);
    if (!p) {
        out[0] = 0;
        return GMB_DB_ERR;
    }
    p = strchr(p + strlen(pat), ':');
    if (!p) return GMB_DB_ERR;
    p++;
    while (*p == ' ') p++;
    if (*p == '"') {
        p++;
        while (*p && *p != '"' && n < cap - 1) {
            if (*p == '\\' && p[1]) p++;
            out[n++] = *p++;
        }
        out[n] = 0;
        return GMB_DB_OK;
    }
    while (*p && *p != ',' && *p != '}' && n < cap - 1) out[n++] = *p++;
    out[n] = 0;
    return GMB_DB_OK;
}

static int dbf_append_from_json(DbfEngine* target, const char* json)
{
    int nFields = 0, fi, off = 0, payloadLen;
    const DbfField* fields = DbfEngine_GetFields(target, &nFields);
    uint8_t* payload;
    char val[512];
    payloadLen = (int)DbfEngine_GetHeader(target)->RecordLength - 1;
    payload = (uint8_t*)malloc((size_t)payloadLen);
    if (!payload) return GMB_DB_ERR;
    memset(payload, ' ', (size_t)payloadLen);
    for (fi = 0; fi < nFields; fi++) {
        if (json && json[0] && dbf_json_get(json, fields[fi].Name, val, (int)sizeof(val)) == GMB_DB_OK)
            DbfFieldWriter_Write(payload + off, fields[fi].Length, &fields[fi], val, DbfEngine_get_MemoFile(target));
        else
            DbfFieldWriter_WriteBlank(payload + off, fields[fi].Length, &fields[fi], DbfEngine_get_MemoFile(target));
        off += fields[fi].Length;
    }
    {
        int rc = DbfEngine_AppendRecord(target, payload, payloadLen);
        free(payload);
        return rc;
    }
}

static int dbf_try_skip_append(DbfEngine* target, ReplicationApplyState* state, int64_t primaryRecNo)
{
    char tmp[8];
    if (primaryRecNo > (int64_t)DbfEngine_GetTotalRecordCount(target)) return 0;
    if (DbfEngine_PositionAtRecord(target, (int)primaryRecNo) != GMB_DB_OK) return 0;
    if (DbfEngine_GetFieldText(target, 0, tmp, (int)sizeof(tmp)) != GMB_DB_OK) return 0;
    ReplicationApplyState_SetRecMap(state, primaryRecNo, primaryRecNo);
    return 1;
}

static int dbf_apply_pack(ReplicationApplyState* state, DbfEngine* target, int64_t generation)
{
    if (generation <= state->LastPackGeneration) return 0;
    if (DbfEngine_Pack(target) != GMB_DB_OK) return -1;
    ReplicationApplyState_ResetAfterPack(state, generation);
    return 1;
}

static int dbf_apply_schema(ReplicationApplyState* state, DbfEngine* target, const char* json)
{
    int64_t gen;
    char b64[4096];
    uint8_t raw[3072];
    int nRaw = 0;
    if (!json || !json[0]) return -1;
    gen = ReplicationSchemaMetadata_RequireGeneration(json);
    if (gen < 0) return -1;
    if (gen <= state->LastSchemaGeneration) return 0;
    if (ReplicationSchemaMetadata_RequireBase64(json, "gms", b64, (int)sizeof(b64)) != GMB_DB_OK)
        return -1;
    if (ReplicationSchemaMetadata_DecodeBase64(b64, raw, (int)sizeof(raw), &nRaw) != GMB_DB_OK)
        return -1;
    if (DbfSchemaManager_WriteSchemaBytes(DbfFile_get_FilePath(DbfEngine_get_DbfFile(target)), raw, nRaw) != GMB_DB_OK)
        return -1;
    ReplicationApplyState_MarkSchemaApplied(state, gen);
    return 1;
}

static int dbf_apply_reindex(ReplicationApplyState* state, DbfEngine* target, int64_t generation)
{
    if (generation <= state->LastReindexGeneration) return 0;
    if (DbfEngine_Reindex(target) != GMB_DB_OK) return -1;
    ReplicationApplyState_MarkReindexApplied(state, generation);
    return 1;
}

static int dbf_apply_data(const char* line, ReplicationApplyState* state, DbfEngine* target,
                          ReplicationOp op, int64_t recNo, const char* json)
{
    int nFields, fi;
    const DbfField* fields;
    char val[512];
    (void)line;
    switch (op) {
    case ReplicationOp_Append:
        if (dbf_try_skip_append(target, state, recNo) || !ReplicationApplyState_AddAppliedAppend(state, recNo))
            return 0;
        if (dbf_append_from_json(target, json) != GMB_DB_OK) return -1;
        ReplicationApplyState_SetRecMap(state, recNo, DbfEngine_GetCurrentRecordNumber(target));
        return 1;
    case ReplicationOp_Put:
        if (!json || !json[0]) return -1;
        recNo = ReplicationApplyState_ResolveRecNo(state, recNo);
        if (DbfEngine_PositionAtRecord(target, (int)recNo) != GMB_DB_OK) return -1;
        fields = DbfEngine_GetFields(target, &nFields);
        for (fi = 0; fi < nFields; fi++) {
            if (dbf_json_get(json, fields[fi].Name, val, (int)sizeof(val)) == GMB_DB_OK)
                DbfEngine_ReplaceField(target, fi, val);
        }
        return 1;
    case ReplicationOp_Delete:
        recNo = ReplicationApplyState_ResolveRecNo(state, recNo);
        if (DbfEngine_PositionAtRecord(target, (int)recNo) != GMB_DB_OK) return -1;
        if (DbfEngine_DeleteCurrent(target) != GMB_DB_OK) return -1;
        return 1;
    case ReplicationOp_Recall:
        recNo = ReplicationApplyState_ResolveRecNo(state, recNo);
        if (DbfEngine_PositionAtRecord(target, (int)recNo) != GMB_DB_OK) return -1;
        if (DbfEngine_RecallCurrent(target) != GMB_DB_OK) return -1;
        return 1;
    default:
        return -1;
    }
}

int DbfReplicationApplier_ApplyLine(const char* line, ReplicationApplyState* state, DbfEngine* target)
{
    ReplicationOp op;
    int64_t recNo = 0;
    char json[1536];
    int i, applied;
    if (!line || !state || !target) return GMB_DB_ARG;
    if (!*line) return 0;
    json[0] = 0;
    if (!ReplicationLogLine_TryParse(line, &op, &recNo, json, (int)sizeof(json)))
        return -1;
    if (op == ReplicationOp_TxnBegin) {
        state->InTxn = 1;
        ReplicationApplyState_ClearPending(state);
        return 0;
    }
    if (op == ReplicationOp_TxnCommit) {
        state->InTxn = 0;
        applied = 0;
        for (i = 0; i < state->pendingTxnN; i++) {
            ReplicationOp pop;
            int64_t pn = 0;
            char pj[1536];
            int rc;
            pj[0] = 0;
            if (!ReplicationLogLine_TryParse(state->pendingTxnLines[i], &pop, &pn, pj, (int)sizeof(pj)))
                return -1;
            rc = dbf_apply_data(state->pendingTxnLines[i], state, target, pop, pn, pj);
            if (rc < 0) return -1;
            if (rc > 0) applied++;
        }
        ReplicationApplyState_ClearPending(state);
        return applied > 0 ? 1 : 0;
    }
    if (state->InTxn) {
        if (ReplicationApplyState_PushPending(state, line) != GMB_DB_OK) return -1;
        return 0;
    }
    if (op == ReplicationOp_Pack) return dbf_apply_pack(state, target, recNo);
    if (op == ReplicationOp_Schema) return dbf_apply_schema(state, target, json);
    if (op == ReplicationOp_Reindex) return dbf_apply_reindex(state, target, recNo);
    return dbf_apply_data(line, state, target, op, recNo, json);
}

static int dbf_apply_line_cb(ReplicationApplyState* state, const char* line, void* ctx)
{
    return DbfReplicationApplier_ApplyLine(line, state, (DbfEngine*)ctx);
}

int DbfReplicationApplier_Apply(const char* replLogPath, DbfEngine* target)
{
    ReplicationApplyState state;
    FILE* f;
    char line[2048];
    int applied = 0;
    if (!replLogPath || !target) return GMB_DB_ARG;
    if (access(replLogPath, R_OK) != 0) return GMB_DB_ERR;
    DbfEngine_SetReplication(target, "");
    ReplicationApplyState_Init(&state);
    f = fopen(replLogPath, "r");
    if (!f) return GMB_DB_ERR;
    while (fgets(line, (int)sizeof(line), f)) {
        int n = (int)strlen(line);
        int rc;
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!n) continue;
        rc = DbfReplicationApplier_ApplyLine(line, &state, target);
        if (rc < 0) {
            fclose(f);
            return GMB_DB_ERR;
        }
        if (rc > 0) applied++;
    }
    fclose(f);
    return applied;
}

int DbfReplicationApplier_Follow(const char* replLogPath, DbfEngine* target, const char* checkpointPath)
{
    if (!replLogPath || !target) return GMB_DB_ARG;
    DbfEngine_SetReplication(target, "");
    return ReplicationLogFollower_FollowOnce(replLogPath, dbf_apply_line_cb, target, checkpointPath);
}

int DbfReplicationApplier_ApplyPack(ReplicationApplyState* state, DbfEngine* target, int64_t generation)
{
    return dbf_apply_pack(state, target, generation);
}

int DbfReplicationApplier_ApplySchema(ReplicationApplyState* state, DbfEngine* target, const char* json)
{
    return dbf_apply_schema(state, target, json);
}

int DbfReplicationApplier_ApplyReindex(ReplicationApplyState* state, DbfEngine* target, int64_t generation)
{
    return dbf_apply_reindex(state, target, generation);
}
