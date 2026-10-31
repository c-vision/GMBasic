// FILE: Basic.ISAM.IsamReplicationLog

using System.Text;
using Basic.Databases.Replication;

namespace Basic.ISAM;

/// <summary>
/// Append-only sidecar log for ISAM record mutations (one line per committed op).
/// </summary>
/// <remarks>
/// ponytail: synchronous append per line, single local follower file, no network replay or compaction.
/// </remarks>
internal sealed class IsamReplicationLog : IDisposable
{
    private readonly StreamWriter _writer;
    private readonly object _writeLock = new();

    public string LogPath { get; }

    public IsamReplicationLog(string logPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(logPath);
        LogPath = logPath;
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

    public static string FormatLine(ReplicationOp op, long recNo, IReadOnlyDictionary<string, object?>? values = null) =>
        ReplicationLogLine.Format(op, recNo, values);

    public void Dispose() => _writer.Dispose();
}
