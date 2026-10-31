#include <stdio.h>
#include <unistd.h>

/* C#: ReplicationApplyState — RecMap, txn buffer, checkpoint JSON PascalCase. */

void ReplicationApplyState_Init(ReplicationApplyState* self)
{
    if (!self) return;
    memset(self, 0, sizeof(*self));
}

void ReplicationApplyState_ResetAfterPack(ReplicationApplyState* self, int64_t generation)
{
    if (!self) return;
    self->recMapN = 0;
    self->appliedAppendN = 0;
    self->LastPackGeneration = generation;
}

void ReplicationApplyState_MarkSchemaApplied(ReplicationApplyState* self, int64_t generation)
{
    if (self) self->LastSchemaGeneration = generation;
}

void ReplicationApplyState_MarkReindexApplied(ReplicationApplyState* self, int64_t generation)
{
    if (self) self->LastReindexGeneration = generation;
}

int ReplicationApplyState_AddAppliedAppend(ReplicationApplyState* self, int64_t recNo)
{
    int i;
    if (!self) return 0;
    for (i = 0; i < self->appliedAppendN; i++)
        if (self->appliedAppends[i] == recNo) return 0;
    if (self->appliedAppendN >= GMB_REPL_MAX_MAP) return 0;
    self->appliedAppends[self->appliedAppendN++] = recNo;
    return 1;
}

void ReplicationApplyState_SetRecMap(ReplicationApplyState* self, int64_t from, int64_t to)
{
    int i;
    if (!self) return;
    for (i = 0; i < self->recMapN; i++) {
        if (self->recMapFrom[i] == from) {
            self->recMapTo[i] = to;
            return;
        }
    }
    if (self->recMapN >= GMB_REPL_MAX_MAP) return;
    self->recMapFrom[self->recMapN] = from;
    self->recMapTo[self->recMapN] = to;
    self->recMapN++;
}

int64_t ReplicationApplyState_ResolveRecNo(const ReplicationApplyState* self, int64_t primary)
{
    int i;
    if (!self) return primary;
    for (i = 0; i < self->recMapN; i++)
        if (self->recMapFrom[i] == primary) return self->recMapTo[i];
    return primary;
}

int ReplicationApplyState_PushPending(ReplicationApplyState* self, const char* line)
{
    if (!self || !line || self->pendingTxnN >= GMB_REPL_MAX_PENDING) return GMB_DB_ERR;
    snprintf(self->pendingTxnLines[self->pendingTxnN++], GMB_REPL_LINE_CAP, "%s", line);
    return GMB_DB_OK;
}

void ReplicationApplyState_ClearPending(ReplicationApplyState* self)
{
    if (self) self->pendingTxnN = 0;
}

void ReplicationApplyState_SaveCheckpoint(const ReplicationApplyState* self, const char* checkpointPath, int64_t byteOffset)
{
    FILE* f;
    int i;
    if (!self || !checkpointPath || !checkpointPath[0]) return;
    f = fopen(checkpointPath, "wb");
    if (!f) return;
    fprintf(f, "{\"Offset\":%lld,\"RecMap\":{", (long long)byteOffset);
    for (i = 0; i < self->recMapN; i++) {
        if (i) fputc(',', f);
        fprintf(f, "\"%lld\":%lld", (long long)self->recMapFrom[i], (long long)self->recMapTo[i]);
    }
    fprintf(f,
            "},\"LastPackGeneration\":%lld,\"LastSchemaGeneration\":%lld,\"LastReindexGeneration\":%lld}",
            (long long)self->LastPackGeneration,
            (long long)self->LastSchemaGeneration,
            (long long)self->LastReindexGeneration);
    fclose(f);
}

static int64_t ras_json_i64(const char* json, const char* key)
{
    char pat[64];
    const char* p;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    return (int64_t)atoll(p + 1);
}

int64_t ReplicationApplyState_LoadCheckpoint(ReplicationApplyState* self, const char* checkpointPath)
{
    FILE* f;
    char buf[8192];
    size_t n;
    const char* map;
    if (self) ReplicationApplyState_Init(self);
    if (!checkpointPath) return 0;
    f = fopen(checkpointPath, "rb");
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    if (!self) return ras_json_i64(buf, "Offset");
    self->LastPackGeneration = ras_json_i64(buf, "LastPackGeneration");
    self->LastSchemaGeneration = ras_json_i64(buf, "LastSchemaGeneration");
    self->LastReindexGeneration = ras_json_i64(buf, "LastReindexGeneration");
    map = strstr(buf, "\"RecMap\"");
    if (map) {
        map = strchr(map, '{');
        if (map) {
            map++;
            while (*map && *map != '}') {
                int64_t from, to;
                while (*map && *map != '"' && *map != '}') map++;
                if (*map != '"') break;
                from = (int64_t)atoll(map + 1);
                map = strchr(map, ':');
                if (!map) break;
                to = (int64_t)atoll(map + 1);
                ReplicationApplyState_SetRecMap(self, from, to);
                map = strchr(map, ',');
                if (!map) break;
                map++;
            }
        }
    }
    return ras_json_i64(buf, "Offset");
}
