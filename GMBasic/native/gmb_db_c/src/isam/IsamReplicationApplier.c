/* FILE: native/gmb_db_c/src/ISAM/IsamReplicationApplier.c */
/* Port 1:1 di modules/Database/ISAM/IsamReplicationApplier.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamReplicationApplier
//
//using Basic.Databases;
//using Basic.Databases.Replication;
//
//namespace Basic.ISAM;
//
//internal static class IsamReplicationApplier
//{
//    public static int Apply(string replLogPath, Engine target, KeyExpressionEvaluatorCallback evaluator)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
//        ArgumentNullException.ThrowIfNull(target);
//        ArgumentNullException.ThrowIfNull(evaluator);
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
//            if (ApplyLine(line, state, target, evaluator))
//            {
//                applied++;
//            }
//        }
//
//        return applied;
//    }
//
//    public static int Follow(string replLogPath, Engine target, KeyExpressionEvaluatorCallback evaluator, string? checkpointPath = null)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
//        ArgumentNullException.ThrowIfNull(target);
//        ArgumentNullException.ThrowIfNull(evaluator);
//        target.SetReplication(string.Empty);
//        checkpointPath ??= ReplicationLogFollower.DefaultCheckpointPath(replLogPath);
//        return ReplicationLogFollower.FollowOnce(replLogPath, (state, line) => ApplyLine(line, state, target, evaluator), checkpointPath);
//    }
//
//    internal static bool ApplyLine(string line, ReplicationApplyState state, Engine target, KeyExpressionEvaluatorCallback evaluator)
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
//                    if (ApplyDataLine(pending, state, target, evaluator, pendingOp, pendingRecNo, pendingValues))
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
//                    ReplicationOp.Pack => ApplyPack(state, target, evaluator, primaryRecNo),
//                    ReplicationOp.Schema => ApplySchema(state, target, evaluator, values),
//                    ReplicationOp.Reindex => ApplyReindex(state, target, evaluator, 0),
//                    _ => ApplyDataLine(line, state, target, evaluator, op, primaryRecNo, values),
//                };
//        }
//    }
//
//    private static bool ApplyPack(ReplicationApplyState state, Engine target, KeyExpressionEvaluatorCallback evaluator, long generation)
//    {
//        if (generation <= state.LastPackGeneration)
//        {
//            return false;
//        }
//
//        target.Pack(evaluator);
//        state.ResetAfterPack(generation);
//        return true;
//    }
//
//    private static bool ApplySchema(
//        ReplicationApplyState state,
//        Engine target,
//        KeyExpressionEvaluatorCallback evaluator,
//        Dictionary<string, object?>? values)
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
//        var ischBytes = ReplicationSchemaMetadata.RequireBase64(values, ReplicationSchemaMetadata.IschKey);
//        target.ApplyReplicatedSchema(ischBytes, evaluator);
//        state.MarkSchemaApplied(generation);
//        return true;
//    }
//
//    private static bool ApplyReindex(ReplicationApplyState state, Engine target, KeyExpressionEvaluatorCallback evaluator, long generation)
//    {
//        if (generation <= state.LastReindexGeneration)
//        {
//            return false;
//        }
//
//        target.Reindex(evaluator);
//        state.MarkReindexApplied(generation);
//        return true;
//    }
//
//    private static bool ApplyDataLine(string line, ReplicationApplyState state, Engine target, KeyExpressionEvaluatorCallback evaluator, ReplicationOp op, long primaryRecNo, Dictionary<string, object?>? values)
//    {
//        switch (op)
//        {
//            case ReplicationOp.Append:
//                if (TrySkipExistingAppend(target, state.RecMap, primaryRecNo) || !state.AppliedAppends.Add(primaryRecNo))
//                {
//                    return false;
//                }
//
//                var appendRecord = values is null ? BlankRecordFromUdt(target.UdtDefinition) : BuildRecord(target.UdtDefinition, ReplicationLogLine.NormalizeValues(values));
//                target.Append(appendRecord, evaluator);
//                state.RecMap[primaryRecNo] = target.GetCurrentRecordNumber();
//                return true;
//            case ReplicationOp.Put:
//                if (values is null)
//                {
//                    throw new InvalidDataException($"PUT requires values: {line}");
//                }
//
//                var putRecord = BuildRecord(target.UdtDefinition, ReplicationLogLine.NormalizeValues(values));
//                var putRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
//                target.PositionAtRecord(putRecNo);
//                _ = target.GetCurrentRecordWithMetadata(out RecordMetadata putMetadata);
//                if (putMetadata.Status == Config.RecordStatusDeleted)
//                {
//                    throw new InvalidOperationException($"Record {putRecNo} not found for PUT.");
//                }
//
//                target.Put(putRecord, evaluator);
//                return true;
//            case ReplicationOp.Delete:
//                var deleteRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
//                target.PositionAtRecord(deleteRecNo);
//                _ = target.GetCurrentRecordWithMetadata(out RecordMetadata deleteMetadata);
//                if (deleteMetadata.Status == Config.RecordStatusDeleted)
//                {
//                    return false;
//                }
//
//                target.DeleteCurrent(evaluator);
//                return true;
//            case ReplicationOp.Recall:
//                var recallRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
//                target.PositionAtRecord(recallRecNo);
//                _ = target.GetCurrentRecordWithMetadata(out RecordMetadata recallMetadata);
//                if (recallMetadata.Status == Config.RecordStatusActive)
//                {
//                    return false;
//                }
//
//                target.RecallCurrent(evaluator);
//                return true;
//            default:
//                throw new InvalidDataException($"Unsupported replication op: {op}");
//        }
//    }
//
//    private static long ResolveRecNo(IReadOnlyDictionary<long, long> recMap, long primaryRecNo) =>
//        recMap.TryGetValue(primaryRecNo, out var mapped) ? mapped : primaryRecNo;
//
//    private static bool TrySkipExistingAppend(Engine target, Dictionary<long, long> recMap, long primaryRecNo)
//    {
//        if (primaryRecNo > target.LastPhysicalRecNumber)
//        {
//            return false;
//        }
//
//        target.PositionAtRecord(primaryRecNo);
//        _ = target.GetCurrentRecordWithMetadata(out RecordMetadata metadata);
//        if (metadata.Status == Config.RecordStatusDeleted)
//        {
//            return false;
//        }
//
//        recMap[primaryRecNo] = primaryRecNo;
//        return true;
//    }
//
//    private static UdtInstance BuildRecord(UserDefinedType udt, IReadOnlyDictionary<string, object?> values)
//    {
//        var fields = new Dictionary<int, object?>();
//        foreach (var field in udt.Fields)
//        {
//            fields[field.SymbolId] = values.TryGetValue(field.Name.Lexeme, out var value) ? value : DefaultFieldValue(field.DataType);
//        }
//
//        return new UdtInstance(udt, fields);
//    }
//
//    private static UdtInstance BlankRecordFromUdt(UserDefinedType udt)
//    {
//        var fields = new Dictionary<int, object?>();
//        foreach (var field in udt.Fields)
//        {
//            fields[field.SymbolId] = DefaultFieldValue(field.DataType);
//        }
//
//        return new UdtInstance(udt, fields);
//    }
//
//    private static object? DefaultFieldValue(DataType dataType) => dataType switch
//    {
//        StringType { FixedLength: { } len } => new string(' ', len),
//        StringType => string.Empty,
//        LongType => 0L,
//        JsonType => "{}",
//        BitType => false,
//        _ => null,
//    };
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>

static int isam_json_get(const char* json, const char* key, char* out, int cap)
{
    char pat[64];
    const char* p;
    int n = 0;
    if (!json || !key || !out || cap <= 0) return GMB_DB_ARG;
    out[0] = 0;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(json, pat);
    if (!p) return GMB_DB_ERR;
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

static int isam_is_text(const IsamFieldSchema* fs)
{
    return strcmp(fs->DataTypeCode, "StringType") == 0;
}

static IsamFieldValue* isam_fields_from_json(const IsamSchema* s, const char* json)
{
    IsamFieldValue* fields;
    int i;
    char val[512];
    if (!s || s->FieldCount <= 0) return NULL;
    fields = (IsamFieldValue*)calloc((size_t)s->FieldCount, sizeof(*fields));
    if (!fields) return NULL;
    for (i = 0; i < s->FieldCount; i++) {
        if (isam_is_text(&s->Fields[i])) {
            int cap = s->Fields[i].Length + 1;
            if (cap < 2) cap = 2;
            fields[i].textOut = (char*)malloc((size_t)cap);
            if (!fields[i].textOut) {
                int k;
                for (k = 0; k < i; k++) free(fields[k].textOut);
                free(fields);
                return NULL;
            }
            fields[i].textOutCap = cap;
            fields[i].textOut[0] = 0;
            if (json && isam_json_get(json, s->Fields[i].Name, val, (int)sizeof(val)) == GMB_DB_OK)
                snprintf(fields[i].textOut, (size_t)cap, "%s", val);
            fields[i].text = fields[i].textOut;
        } else if (json && isam_json_get(json, s->Fields[i].Name, val, (int)sizeof(val)) == GMB_DB_OK) {
            fields[i].i64 = (int64_t)strtoll(val, NULL, 10);
        }
    }
    return fields;
}

static void isam_free_fields(IsamFieldValue* fields, int n)
{
    int i;
    if (!fields) return;
    for (i = 0; i < n; i++) free(fields[i].textOut);
    free(fields);
}

static int isam_read_meta(Engine* target, RecordMetadata* meta)
{
    const IsamSchema* s = Engine_GetSchema(target);
    IsamFieldValue* f;
    int rc;
    if (!s) return GMB_DB_ERR;
    f = isam_fields_from_json(s, NULL);
    if (!f) return GMB_DB_ERR;
    rc = Engine_GetCurrent(target, f, s->FieldCount, meta);
    isam_free_fields(f, s->FieldCount);
    return rc;
}

static int isam_try_skip_append(Engine* target, ReplicationApplyState* state, int64_t primaryRecNo)
{
    RecordMetadata meta;
    if (primaryRecNo > Engine_GetLastPhysicalRecNumber(target)) return 0;
    if (Engine_PositionAtRecord(target, primaryRecNo) != GMB_DB_OK) return 0;
    if (isam_read_meta(target, &meta) != GMB_DB_OK) return 0;
    if (meta.Status == Config_RecordStatusDeleted) return 0;
    ReplicationApplyState_SetRecMap(state, primaryRecNo, primaryRecNo);
    return 1;
}

static int isam_apply_pack(ReplicationApplyState* state, Engine* target, int64_t generation)
{
    if (generation <= state->LastPackGeneration) return 0;
    if (Engine_Pack(target) != GMB_DB_OK) return -1;
    ReplicationApplyState_ResetAfterPack(state, generation);
    return 1;
}

static int isam_apply_data(ReplicationApplyState* state, Engine* target,
                           ReplicationOp op, int64_t recNo, const char* json)
{
    const IsamSchema* s = Engine_GetSchema(target);
    IsamFieldValue* fields;
    int64_t mapped;
    RecordMetadata meta;
    int n, rc;
    if (!s) return -1;
    n = s->FieldCount;
    mapped = ReplicationApplyState_ResolveRecNo(state, recNo);
    switch (op) {
    case ReplicationOp_Append:
        if (isam_try_skip_append(target, state, recNo) || !ReplicationApplyState_AddAppliedAppend(state, recNo))
            return 0;
        fields = isam_fields_from_json(s, json);
        if (!fields) return -1;
        rc = Engine_Append(target, fields, n);
        isam_free_fields(fields, n);
        if (rc != GMB_DB_OK) return -1;
        ReplicationApplyState_SetRecMap(state, recNo, Engine_GetCurrentRecordNumber(target));
        return 1;
    case ReplicationOp_Put:
        if (!json || !json[0]) return -1;
        if (Engine_PositionAtRecord(target, mapped) != GMB_DB_OK) return -1;
        if (isam_read_meta(target, &meta) != GMB_DB_OK) return -1;
        if (meta.Status == Config_RecordStatusDeleted) return -1;
        fields = isam_fields_from_json(s, json);
        if (!fields) return -1;
        rc = Engine_Put(target, fields, n);
        isam_free_fields(fields, n);
        return rc == GMB_DB_OK ? 1 : -1;
    case ReplicationOp_Delete:
        if (Engine_PositionAtRecord(target, mapped) != GMB_DB_OK) return -1;
        if (isam_read_meta(target, &meta) != GMB_DB_OK) return -1;
        if (meta.Status == Config_RecordStatusDeleted) return 0;
        return Engine_DeleteCurrent(target) == GMB_DB_OK ? 1 : -1;
    case ReplicationOp_Recall:
        if (Engine_PositionAtRecord(target, mapped) != GMB_DB_OK) return -1;
        if (isam_read_meta(target, &meta) != GMB_DB_OK) return -1;
        if (meta.Status == Config_RecordStatusActive) return 0;
        return Engine_RecallCurrent(target) == GMB_DB_OK ? 1 : -1;
    default:
        return -1;
    }
}

static int isam_apply_schema(ReplicationApplyState* state, Engine* target, const char* json)
{
    int64_t gen;
    char b64[4096];
    uint8_t raw[3072];
    int nRaw = 0;
    if (!json || !json[0]) return -1;
    gen = ReplicationSchemaMetadata_RequireGeneration(json);
    if (gen < 0) return -1;
    if (gen <= state->LastSchemaGeneration) return 0;
    if (ReplicationSchemaMetadata_RequireBase64(json, "isch", b64, (int)sizeof(b64)) != GMB_DB_OK)
        return -1;
    if (ReplicationSchemaMetadata_DecodeBase64(b64, raw, (int)sizeof(raw), &nRaw) != GMB_DB_OK)
        return -1;
    if (Engine_ApplyReplicatedSchema(target, raw, nRaw) != GMB_DB_OK) return -1;
    ReplicationApplyState_MarkSchemaApplied(state, gen);
    return 1;
}

static int isam_apply_reindex(ReplicationApplyState* state, Engine* target, int64_t generation)
{
    if (generation <= state->LastReindexGeneration) return 0;
    if (Engine_Reindex(target) != GMB_DB_OK) return -1;
    ReplicationApplyState_MarkReindexApplied(state, generation);
    return 1;
}

int IsamReplicationApplier_ApplyLine(const char* line, ReplicationApplyState* state, Engine* target)
{
    ReplicationOp op;
    int64_t recNo = 0;
    char json[2048];
    int i, applied = 0;
    if (!line || !state || !target) return -1;
    while (*line == ' ' || *line == '\t') line++;
    if (!*line) return 0;
    if (!ReplicationLogLine_TryParse(line, &op, &recNo, json, (int)sizeof(json)))
        return -1;
    if (op == ReplicationOp_TxnBegin) {
        state->InTxn = 1;
        ReplicationApplyState_ClearPending(state);
        return 0;
    }
    if (op == ReplicationOp_TxnCommit) {
        state->InTxn = 0;
        for (i = 0; i < state->pendingTxnN; i++) {
            ReplicationOp pop;
            int64_t pn = 0;
            char pj[GMB_REPL_LINE_CAP];
            int rc;
            if (!ReplicationLogLine_TryParse(state->pendingTxnLines[i], &pop, &pn, pj, (int)sizeof(pj)))
                return -1;
            rc = isam_apply_data(state, target, pop, pn, pj);
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
    if (op == ReplicationOp_Pack) return isam_apply_pack(state, target, recNo);
    if (op == ReplicationOp_Schema) return isam_apply_schema(state, target, json);
    if (op == ReplicationOp_Reindex) return isam_apply_reindex(state, target, recNo);
    return isam_apply_data(state, target, op, recNo, json);
}

static int isam_apply_line_cb(ReplicationApplyState* state, const char* line, void* ctx)
{
    return IsamReplicationApplier_ApplyLine(line, state, (Engine*)ctx);
}

int IsamReplicationApplier_Apply(const char* replLogPath, Engine* target)
{
    ReplicationApplyState state;
    FILE* f;
    char line[2048];
    int applied = 0;
    if (!replLogPath || !target) return GMB_DB_ARG;
    if (access(replLogPath, R_OK) != 0) return GMB_DB_ERR;
    Engine_SetReplication(target, "");
    ReplicationApplyState_Init(&state);
    f = fopen(replLogPath, "r");
    if (!f) return GMB_DB_ERR;
    while (fgets(line, (int)sizeof(line), f)) {
        int n = (int)strlen(line);
        int rc;
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!n) continue;
        rc = IsamReplicationApplier_ApplyLine(line, &state, target);
        if (rc < 0) { fclose(f); return GMB_DB_ERR; }
        if (rc > 0) applied++;
    }
    fclose(f);
    return applied;
}

int IsamReplicationApplier_Follow(const char* replLogPath, Engine* target, const char* checkpointPath)
{
    if (!replLogPath || !target) return GMB_DB_ARG;
    Engine_SetReplication(target, "");
    return ReplicationLogFollower_FollowOnce(replLogPath, isam_apply_line_cb, target, checkpointPath);
}

int IsamReplicationApplier_SelfCheck(void)
{
    char src[] = "native/gmb_db_c/build/isam_repl_src.isdb";
    char dst[] = "native/gmb_db_c/build/isam_repl_dst.isdb";
    char repl[] = "native/gmb_db_c/build/isam_repl_src.isdb.repl";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    Engine *s, *d;
    IsamFieldValue vals[2];
    char nameOut[32];
    RecordMetadata meta;
    int n;
    unlink(src);
    unlink(dst);
    unlink(repl);
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema.Fields = fields;
    schema.FieldCount = 2;
    snprintf(schema.PrimaryKey.Name, sizeof(schema.PrimaryKey.Name), "%s", "Id");
    snprintf(schema.PrimaryKey.KeyExpression, sizeof(schema.PrimaryKey.KeyExpression), "%s", "Id");
    schema.PrimaryKey.KeyLength = 10;
    schema.PrimaryKey.IsUnique = 1;
    if (IsamFileHandle_CreateNew(src, &schema) != GMB_DB_OK) return -290;
    if (IsamFileHandle_CreateNew(dst, &schema) != GMB_DB_OK) return -291;
    s = Engine_Open(src, &schema);
    d = Engine_Open(dst, &schema);
    if (!s || !d) { if (s) Engine_Dispose(s); if (d) Engine_Dispose(d); return -292; }
    if (Engine_SetReplication(s, "DEFAULT") != GMB_DB_OK) { Engine_Dispose(s); Engine_Dispose(d); return -293; }
    memset(vals, 0, sizeof(vals)); vals[0].i64 = 1; vals[1].text = "N1";
    if (Engine_Append(s, vals, 2) != GMB_DB_OK) { Engine_Dispose(s); Engine_Dispose(d); return -294; }
    memset(vals, 0, sizeof(vals)); vals[0].i64 = 2; vals[1].text = "N2";
    if (Engine_Append(s, vals, 2) != GMB_DB_OK) { Engine_Dispose(s); Engine_Dispose(d); return -294; }
    if (Engine_PositionAtRecord(s, 1) != GMB_DB_OK) { Engine_Dispose(s); Engine_Dispose(d); return -295; }
    memset(vals, 0, sizeof(vals)); vals[0].i64 = 1; vals[1].text = "NX";
    if (Engine_Put(s, vals, 2) != GMB_DB_OK) { Engine_Dispose(s); Engine_Dispose(d); return -296; }
    if (Engine_PositionAtRecord(s, 2) != GMB_DB_OK) { Engine_Dispose(s); Engine_Dispose(d); return -295; }
    if (Engine_DeleteCurrent(s) != GMB_DB_OK) { Engine_Dispose(s); Engine_Dispose(d); return -297; }
    n = IsamReplicationApplier_Apply(repl, d);
    Engine_Dispose(s);
    if (n < 4) { Engine_Dispose(d); return -298; }
    if (Engine_GetTotalRecordCount(d) != 1) { Engine_Dispose(d); return -299; }
    if (Engine_PositionAtRecord(d, 1) != GMB_DB_OK) { Engine_Dispose(d); return -300; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(d, vals, 2, &meta) != GMB_DB_OK || vals[0].i64 != 1) {
        Engine_Dispose(d); return -301;
    }
    if (strstr(nameOut, "NX") == NULL) {
        Engine_Dispose(d); return -302;
    }
    {
        uint8_t* isch = NULL;
        int ischLen = 0;
        char b64[1024], meta[1400], line[2048];
        ReplicationApplyState st;
        int rc;
        if (SchemaManager_SerializeSchema(&schema, &isch, &ischLen) != GMB_DB_OK || !isch) {
            Engine_Dispose(d); return -303;
        }
        ReplicationSchemaMetadata_EncodeBase64(isch, ischLen, b64, (int)sizeof(b64));
        ReplicationSchemaMetadata_ForIsam(1, b64, meta, (int)sizeof(meta));
        ReplicationLogLine_Format(ReplicationOp_Schema, 0, meta, line, (int)sizeof(line));
        ReplicationApplyState_Init(&st);
        rc = IsamReplicationApplier_ApplyLine(line, &st, d);
        free(isch);
        if (rc != 1) { Engine_Dispose(d); return -304; }
        rc = IsamReplicationApplier_ApplyLine(line, &st, d);
        if (rc != 0) { Engine_Dispose(d); return -305; }
        ReplicationLogLine_FormatReindex(1, line, (int)sizeof(line));
        rc = IsamReplicationApplier_ApplyLine(line, &st, d);
        if (rc != 1) { Engine_Dispose(d); return -306; }
        rc = IsamReplicationApplier_ApplyLine(line, &st, d);
        if (rc != 0) { Engine_Dispose(d); return -307; }
        if (Engine_SeekI64(d, 1) != GMB_DB_OK) { Engine_Dispose(d); return -308; }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_GetCurrent(d, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 1 ||
            strstr(nameOut, "NX") == NULL) {
            Engine_Dispose(d); return -309;
        }
    }
    Engine_Dispose(d);
    return GMB_DB_OK;
}
