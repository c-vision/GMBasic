#include <stdio.h>
#include <unistd.h>

/* C#: ReplicationLogFollower — coda dal checkpoint byte offset. */

void ReplicationLogFollower_DefaultCheckpointPath(const char* replLogPath, char* out, int cap)
{
    snprintf(out, (size_t)cap, "%s.ckpt", replLogPath ? replLogPath : "");
}

int ReplicationLogFollower_FollowOnce(const char* replLogPath, ReplicationApplyLineFn applyLine, void* ctx,
                                      const char* checkpointPath)
{
    ReplicationApplyState state;
    char ckpt[1024];
    FILE* f;
    char line[2048];
    int64_t offset;
    int applied = 0;
    long pos;
    if (!replLogPath || !applyLine) return GMB_DB_ARG;
    if (access(replLogPath, R_OK) != 0) return GMB_DB_ERR;
    if (!checkpointPath || !checkpointPath[0]) {
        ReplicationLogFollower_DefaultCheckpointPath(replLogPath, ckpt, (int)sizeof(ckpt));
        checkpointPath = ckpt;
    }
    offset = ReplicationApplyState_LoadCheckpoint(&state, checkpointPath);
    f = fopen(replLogPath, "rb");
    if (!f) return GMB_DB_ERR;
    if (fseek(f, (long)ReplicationLogEncoding_AdjustFileOffset(replLogPath, offset), SEEK_SET) != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    while (fgets(line, (int)sizeof(line), f)) {
        int n = (int)strlen(line);
        int rc;
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!n) continue;
        rc = applyLine(&state, line, ctx);
        if (rc < 0) {
            fclose(f);
            return GMB_DB_ERR;
        }
        if (rc > 0) applied++;
    }
    pos = ftell(f);
    fclose(f);
    ReplicationApplyState_SaveCheckpoint(&state, checkpointPath, pos >= 0 ? (int64_t)pos : 0);
    return applied;
}
