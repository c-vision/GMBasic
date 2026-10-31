// FILE: Basic.DBF.DbfReplicationLog

using System.Text;
using Basic.Databases.Replication;

namespace Basic.DBF;

internal enum DbfReplicationOp : byte
{
    Append = 1,
    Put = 2,
    Delete = 3,
    Recall = 4,
}

/// <summary>
/// Append-only sidecar log for DBF record mutations (one line per committed op).
/// </summary>
/// <remarks>
/// ponytail: synchronous append per line, single local follower file, no network replay or compaction.
/// Upgrade path: async channel, binary framing, transactional batch markers.
/// </remarks>
internal sealed class DbfReplicationLog : IDisposable
{
    private readonly StreamWriter _writer;
    private readonly object _writeLock = new();

    public string LogPath { get; }

    public DbfReplicationLog(string logPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(logPath);
        LogPath = logPath;
        // ponytail: sync flush per line — upgrade: FileOptions.Asynchronous + background writer
        _writer = new StreamWriter(
            new FileStream(logPath, FileMode.Append, FileAccess.Write, FileShare.Read),
            ReplicationLogEncoding.Utf8NoBom)
        {
            AutoFlush = true,
        };
    }

    public void WriteLines(IEnumerable<string> lines)
    {
        lock (_writeLock)
        {
            foreach (var line in lines)
            {
                _writer.WriteLine(line);
            }
        }
    }

    public static string FormatLine(DbfReplicationOp op, int recNo, IReadOnlyDictionary<string, object?>? values = null) =>
        ReplicationLogLine.Format((ReplicationOp)op, recNo, values);

    public void Dispose() => _writer.Dispose();
}
