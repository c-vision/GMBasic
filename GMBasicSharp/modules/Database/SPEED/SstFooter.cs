// FILE: Basic.Databases.Speed.SstFooter.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Text;

namespace Basic.Databases.Speed;

/// <summary>
/// SST footer layout helpers. V0 ends with indexOffset (8 bytes). V2 adds magic + bloomOffset.
/// </summary>
internal static class SstFooter
{
    // "SPTF" + version 2
    internal const long MagicV2 = 0x53505446_00000002L;
    internal const int V2FooterSize = sizeof(long) * 3;

    /// <summary>
    /// Reads footer metadata from an SST file stream.
    /// </summary>
    /// <param name="stream">The SST file stream.</param>
    /// <param name="indexOffset">The offset of the block index section.</param>
    /// <param name="bloomOffset">The offset of the bloom section, or 0 when absent.</param>
    /// <param name="hasV2Footer">Whether the file uses the V2 footer trailer.</param>
    internal static void Read(Stream stream, out long indexOffset, out long bloomOffset, out bool hasV2Footer)
    {
        bloomOffset = 0;
        hasV2Footer = false;
        indexOffset = 0;

        if (stream.Length < sizeof(long))
            throw new InvalidDataException("SST file too small");

        if (stream.Length >= V2FooterSize)
        {
            stream.Seek(-V2FooterSize, SeekOrigin.End);
            using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
            var magic = reader.ReadInt64();
            if (magic == MagicV2)
            {
                bloomOffset = reader.ReadInt64();
                indexOffset = reader.ReadInt64();
                hasV2Footer = true;
                return;
            }
        }

        stream.Seek(-sizeof(long), SeekOrigin.End);
        using (var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true))
            indexOffset = reader.ReadInt64();
    }

    /// <summary>
    /// Writes the V2 footer trailer.
    /// </summary>
    /// <param name="writer">The destination writer.</param>
    /// <param name="bloomOffset">Bloom section offset, or 0 when absent.</param>
    /// <param name="indexOffset">Block index section offset.</param>
    internal static void WriteV2(BinaryWriter writer, long bloomOffset, long indexOffset)
    {
        writer.Write(MagicV2);
        writer.Write(bloomOffset);
        writer.Write(indexOffset);
    }

    /// <summary>
    /// Validates footer offsets against file length.
    /// </summary>
    /// <param name="streamLength">Total SST file length.</param>
    /// <param name="indexOffset">Block index offset.</param>
    /// <param name="bloomOffset">Bloom section offset.</param>
    /// <param name="hasV2Footer">Whether the V2 footer was detected.</param>
    /// <param name="error">Validation error message.</param>
    /// <returns><c>true</c> when offsets are consistent.</returns>
    internal static bool TryValidateOffsets(long streamLength, long indexOffset, long bloomOffset, bool hasV2Footer,
        out string error)
    {
        error = string.Empty;
        var footerSize = hasV2Footer ? V2FooterSize : sizeof(long);

        if (indexOffset < 0 || indexOffset >= streamLength - footerSize + sizeof(long))
        {
            error = "invalid index offset";
            return false;
        }

        if (bloomOffset < 0 || bloomOffset >= indexOffset)
        {
            error = "invalid bloom offset";
            return false;
        }

        return true;
    }
}
