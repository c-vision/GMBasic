/* FILE: native/gmb_db_c/src/ISAM/IsamReplicationLog.c */
/* Port 1:1 di modules/Database/ISAM/IsamReplicationLog.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamReplicationLog
//
//using System.Text;
//using Basic.Databases.Replication;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Append-only sidecar log for ISAM record mutations (one line per committed op).
///// </summary>
///// <remarks>
///// ponytail: synchronous append per line, single local follower file, no network replay or compaction.
///// </remarks>
//internal sealed class IsamReplicationLog : IDisposable
//{
//    private readonly StreamWriter _writer;
//    private readonly object _writeLock = new();
//
//    public string LogPath { get; }
//
//    public IsamReplicationLog(string logPath)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(logPath);
//        LogPath = logPath;
//        _writer = new StreamWriter(
//            new FileStream(logPath, FileMode.Append, FileAccess.Write, FileShare.Read),
//            ReplicationLogEncoding.Utf8NoBom)
//        {
//            AutoFlush = true,
//        };
//    }
//
//    public void WriteLines(IEnumerable<string> lines)
//    {
//        lock (_writeLock)
//        {
//            foreach (var line in lines)
//            {
//                _writer.WriteLine(line);
//            }
//        }
//    }
//
//    public static string FormatLine(ReplicationOp op, long recNo, IReadOnlyDictionary<string, object?>? values = null) =>
//        ReplicationLogLine.Format(op, recNo, values);
//
//    public void Dispose() => _writer.Dispose();
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

struct IsamReplicationLog {
    FILE* writer;
    char LogPath[1024];
};

IsamReplicationLog* IsamReplicationLog_New(const char* logPath)
{
    IsamReplicationLog* self;
    if (!logPath || !*logPath) return NULL;
    self = (IsamReplicationLog*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->LogPath, sizeof(self->LogPath), "%s", logPath);
    self->writer = fopen(logPath, "ab");
    if (!self->writer) {
        free(self);
        return NULL;
    }
    return self;
}

void IsamReplicationLog_Dispose(IsamReplicationLog* self)
{
    if (!self) return;
    if (self->writer) {
        fflush(self->writer);
        fclose(self->writer);
    }
    free(self);
}

const char* IsamReplicationLog_get_LogPath(const IsamReplicationLog* self)
{
    return self ? self->LogPath : NULL;
}

void IsamReplicationLog_WriteLine(IsamReplicationLog* self, const char* line)
{
    if (!self || !self->writer || !line) return;
    fputs(line, self->writer);
    fputc('\n', self->writer);
    fflush(self->writer);
}
