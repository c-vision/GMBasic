/* FILE: native/gmb_db_c/src/Replication/ReplicationLogFollower.c */
/* Port 1:1 di modules/Database/Replication/ReplicationLogFollower.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Replication.ReplicationLogFollower
//
//using System.Text;
//
//namespace Basic.Databases.Replication;
//
///// <summary>
///// Tail-follow helper: applies new replication log lines since the last checkpoint.
///// </summary>
///// <remarks>
///// ponytail: synchronous poll, local file only — upgrade: FileSystemWatcher or background thread.
///// </remarks>
//internal static class ReplicationLogFollower
//{
//    public static string DefaultCheckpointPath(string replLogPath) => replLogPath + ".ckpt";
//
//    /// <summary>
//    /// Reads and applies log lines appended after the saved byte offset.
//    /// </summary>
//    /// <returns>Number of data lines applied (txn markers excluded).</returns>
//    public static int FollowOnce(
//        string replLogPath,
//        Func<ReplicationApplyState, string, bool> applyLine,
//        string? checkpointPath = null)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
//        ArgumentNullException.ThrowIfNull(applyLine);
//
//        if (!File.Exists(replLogPath))
//        {
//            throw new FileNotFoundException("Replication log not found.", replLogPath);
//        }
//
//        checkpointPath ??= DefaultCheckpointPath(replLogPath);
//        var (offset, state) = ReplicationApplyState.LoadCheckpoint(checkpointPath);
//
//        var applied = 0;
//        using var fs = new FileStream(
//            replLogPath,
//            FileMode.Open,
//            FileAccess.Read,
//            FileShare.ReadWrite);
//        fs.Seek(ReplicationLogEncoding.AdjustFileOffset(replLogPath, offset), SeekOrigin.Begin);
//
//        using var reader = new StreamReader(fs, ReplicationLogEncoding.Utf8NoBom, detectEncodingFromByteOrderMarks: false, bufferSize: 4096, leaveOpen: true);
//        string? line;
//        while ((line = reader.ReadLine()) is not null)
//        {
//            if (string.IsNullOrWhiteSpace(line))
//            {
//                continue;
//            }
//
//            if (applyLine(state, line))
//            {
//                applied++;
//            }
//        }
//
//        state.SaveCheckpoint(checkpointPath, fs.Position);
//        return applied;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

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
