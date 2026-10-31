// FILE: Basic.Databases.Replication.ReplicationLogFollower

using System.Text;

namespace Basic.Databases.Replication;

/// <summary>
/// Tail-follow helper: applies new replication log lines since the last checkpoint.
/// </summary>
/// <remarks>
/// ponytail: synchronous poll, local file only — upgrade: FileSystemWatcher or background thread.
/// </remarks>
internal static class ReplicationLogFollower
{
    public static string DefaultCheckpointPath(string replLogPath) => replLogPath + ".ckpt";

    /// <summary>
    /// Reads and applies log lines appended after the saved byte offset.
    /// </summary>
    /// <returns>Number of data lines applied (txn markers excluded).</returns>
    public static int FollowOnce(
        string replLogPath,
        Func<ReplicationApplyState, string, bool> applyLine,
        string? checkpointPath = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
        ArgumentNullException.ThrowIfNull(applyLine);

        if (!File.Exists(replLogPath))
        {
            throw new FileNotFoundException("Replication log not found.", replLogPath);
        }

        checkpointPath ??= DefaultCheckpointPath(replLogPath);
        var (offset, state) = ReplicationApplyState.LoadCheckpoint(checkpointPath);

        var applied = 0;
        using var fs = new FileStream(
            replLogPath,
            FileMode.Open,
            FileAccess.Read,
            FileShare.ReadWrite);
        fs.Seek(ReplicationLogEncoding.AdjustFileOffset(replLogPath, offset), SeekOrigin.Begin);

        using var reader = new StreamReader(fs, ReplicationLogEncoding.Utf8NoBom, detectEncodingFromByteOrderMarks: false, bufferSize: 4096, leaveOpen: true);
        string? line;
        while ((line = reader.ReadLine()) is not null)
        {
            if (string.IsNullOrWhiteSpace(line))
            {
                continue;
            }

            if (applyLine(state, line))
            {
                applied++;
            }
        }

        state.SaveCheckpoint(checkpointPath, fs.Position);
        return applied;
    }
}
