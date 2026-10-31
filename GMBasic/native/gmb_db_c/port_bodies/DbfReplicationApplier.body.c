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
