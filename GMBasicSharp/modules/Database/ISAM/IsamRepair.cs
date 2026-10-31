// FILE: Basic.ISAM.IsamRepair

using System.Runtime.InteropServices;
using Basic.Databases;

namespace Basic.ISAM;

/// <summary>
/// Best-effort offline prep for corrupt ISAM files before engine open.
/// ponytail: WAL recover + header/index-root fixes; full rebuild via <see cref="Engine.Repair"/>.
/// </summary>
public static class IsamRepair
{
    /// <summary>
    /// Replays WAL and patches header/index roots so a damaged file can be opened for repair.
    /// </summary>
    /// <param name="filePath">Path to the .isdb file.</param>
    public static void PrepareForOpen(string filePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(filePath);
        if (!File.Exists(filePath))
        {
            throw new FileNotFoundException("ISAM file not found.", filePath);
        }

        WriteAheadLog.Recover(filePath);
        FixHeaderBestEffort(filePath);
    }

    private static void FixHeaderBestEffort(string filePath)
    {
        using var stream = new FileStream(filePath, FileMode.Open, FileAccess.ReadWrite, FileShare.Read);
        var fileLength = stream.Length;
        if (fileLength < Config.FileHeaderFieldsPrefixSize)
        {
            throw new InvalidDataException($"ISAM file too small to repair ({fileLength} bytes).");
        }

        stream.Seek(0, SeekOrigin.Begin);
        var header = MarshalHelper.ReadStructure<Config.FileHeader>(stream);

        for (var i = 0; i < Config.MagicBytes.Length; i++)
        {
            header.Magic[i] = Config.MagicBytes[i];
        }

        if (header.PrimaryKeyRootOffset != -1 &&
            (header.PrimaryKeyRootOffset < 0 || header.PrimaryKeyRootOffset >= fileLength))
        {
            header.PrimaryKeyRootOffset = -1;
        }

        for (var i = 0; i < header.SecondaryIndexRootOffsets.Length; i++)
        {
            var root = header.SecondaryIndexRootOffsets[i];
            if (root != -1 && (root < 0 || root >= fileLength))
            {
                header.SecondaryIndexRootOffsets[i] = -1;
            }
        }

        if (header.SchemaOffset <= 0 || header.SchemaOffset >= fileLength)
        {
            header.SchemaOffset = Config.HeaderSize;
        }

        if (header.SchemaLength <= 0 || header.SchemaOffset + header.SchemaLength > fileLength)
        {
            var maxLen = fileLength - header.SchemaOffset;
            if (maxLen <= 0)
            {
                throw new InvalidDataException("Cannot locate schema block for repair.");
            }

            header.SchemaLength = maxLen;
        }

        if (header.RecordSize > 0)
        {
            var dataStart = EstimateDataAreaStart(header, stream, fileLength);
            var maxRecords = Math.Max(0, (fileLength - dataStart) / header.RecordSize);
            if (header.LastRecNumber > maxRecords)
            {
                header.LastRecNumber = maxRecords;
            }
        }

        if (header.PageFreeListHeadOffset != -1 &&
            (header.PageFreeListHeadOffset < 0 || header.PageFreeListHeadOffset >= fileLength))
        {
            header.PageFreeListHeadOffset = -1;
        }

        if (header.FreeListHeadOffset != -1 &&
            (header.FreeListHeadOffset < 0 || header.FreeListHeadOffset >= fileLength))
        {
            header.FreeListHeadOffset = -1;
        }

        stream.Seek(0, SeekOrigin.Begin);
        MarshalHelper.WriteStructure(stream, header);
        stream.Flush(true);
    }

    private static long EstimateDataAreaStart(Config.FileHeader header, Stream stream, long fileLength)
    {
        if (header.SecondaryIndexMapOffset > 0 && header.SecondaryIndexMapOffset < fileLength)
        {
            stream.Seek(header.SecondaryIndexMapOffset, SeekOrigin.Begin);
            try
            {
                var map = SchemaManager.DeserializeIndexMapFromStream(stream);
                return header.SecondaryIndexMapOffset + SchemaManager.SerializeIndexMap(map).Length;
            }
            catch (IOException)
            {
                // fall through
            }
            catch (InvalidDataException)
            {
                // fall through
            }
        }

        return header.SchemaOffset + header.SchemaLength;
    }
}
