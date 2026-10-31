// FILE: Basic.Databases.Replication.ReplicationLogEncoding

using System.Text;

namespace Basic.Databases.Replication;

/// <summary>UTF-8 helpers for replication sidecar logs (byte checkpoints must match on-disk bytes).</summary>
internal static class ReplicationLogEncoding
{
    internal static readonly UTF8Encoding Utf8NoBom = new(encoderShouldEmitUTF8Identifier: false);

    public static bool FileHasUtf8Bom(string path)
    {
        if (!File.Exists(path))
        {
            return false;
        }

        Span<byte> header = stackalloc byte[3];
        using var stream = File.OpenRead(path);
        return stream.Read(header) == 3 &&
               header[0] == 0xEF &&
               header[1] == 0xBB &&
               header[2] == 0xBF;
    }

    /// <summary>Maps logical checkpoint offsets (line-byte sums) to file offsets when a legacy BOM is present.</summary>
    public static long AdjustFileOffset(string path, long logicalOffset) =>
        logicalOffset > 0 && FileHasUtf8Bom(path) ? logicalOffset + 3 : logicalOffset;
}
