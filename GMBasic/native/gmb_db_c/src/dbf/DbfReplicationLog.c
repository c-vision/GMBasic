/* FILE: native/gmb_db_c/src/DBF/DbfReplicationLog.c */
/* Port 1:1 di modules/Database/DBF/DbfReplicationLog.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfReplicationLog
//
//using System.Text;
//using Basic.Databases.Replication;
//
//namespace Basic.DBF;
//
//internal enum DbfReplicationOp : byte
//{
//    Append = 1,
//    Put = 2,
//    Delete = 3,
//    Recall = 4,
//}
//
///// <summary>
///// Append-only sidecar log for DBF record mutations (one line per committed op).
///// </summary>
///// <remarks>
///// ponytail: synchronous append per line, single local follower file, no network replay or compaction.
///// Upgrade path: async channel, binary framing, transactional batch markers.
///// </remarks>
//internal sealed class DbfReplicationLog : IDisposable
//{
//    private readonly StreamWriter _writer;
//    private readonly object _writeLock = new();
//
//    public string LogPath { get; }
//
//    public DbfReplicationLog(string logPath)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(logPath);
//        LogPath = logPath;
//        // ponytail: sync flush per line — upgrade: FileOptions.Asynchronous + background writer
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
//    public static string FormatLine(DbfReplicationOp op, int recNo, IReadOnlyDictionary<string, object?>? values = null) =>
//        ReplicationLogLine.Format((ReplicationOp)op, recNo, values);
//
//    public void Dispose() => _writer.Dispose();
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <stdio.h>

/* C#: DbfReplicationLog — append-only sidecar, flush per riga, UTF-8 no BOM. */

struct DbfReplicationLog {
    FILE* writer;
    char LogPath[1024];
};

DbfReplicationLog* DbfReplicationLog_New(const char* logPath)
{
    DbfReplicationLog* self;
    if (!logPath || !*logPath) return NULL;
    self = (DbfReplicationLog*)calloc(1, sizeof(*self));
    if (!self) return NULL;
    snprintf(self->LogPath, sizeof(self->LogPath), "%s", logPath);
    self->writer = fopen(logPath, "ab");
    if (!self->writer) {
        free(self);
        return NULL;
    }
    return self;
}

void DbfReplicationLog_Dispose(DbfReplicationLog* self)
{
    if (!self) return;
    if (self->writer) {
        fflush(self->writer);
        fclose(self->writer);
    }
    free(self);
}

const char* DbfReplicationLog_get_LogPath(const DbfReplicationLog* self)
{
    return self ? self->LogPath : NULL;
}

void DbfReplicationLog_WriteLine(DbfReplicationLog* self, const char* line)
{
    if (!self || !self->writer || !line) return;
    fputs(line, self->writer);
    fputc('\n', self->writer);
    fflush(self->writer);
}
