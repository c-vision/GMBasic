// FILE: Basic.ISAM.IsamPageChecksum
namespace Basic.ISAM;

/// <summary>
/// CRC32 IEEE per pagine B+ da 4096 byte. I byte 4092–4095 contengono il checksum;
/// durante il calcolo quella regione è trattata come zero (stile SQLite page checksum).
/// </summary>
internal static class IsamPageChecksum
{
    internal const int PageSize = 4096;
    internal const int ChecksumOffset = PageSize - sizeof(uint);
    private static readonly uint[] Crc32Table = CreateCrc32Table();

    internal static void WriteChecksum(Span<byte> page)
    {
        if (page.Length < PageSize)
        {
            throw new ArgumentException($"Il buffer pagina deve essere almeno {PageSize} byte.", nameof(page));
        }

        page[ChecksumOffset] = 0;
        page[ChecksumOffset + 1] = 0;
        page[ChecksumOffset + 2] = 0;
        page[ChecksumOffset + 3] = 0;

        var crc = Compute(page[..PageSize]);
        page[ChecksumOffset] = (byte)crc;
        page[ChecksumOffset + 1] = (byte)(crc >> 8);
        page[ChecksumOffset + 2] = (byte)(crc >> 16);
        page[ChecksumOffset + 3] = (byte)(crc >> 24);
    }

    internal static bool TryValidate(ReadOnlySpan<byte> page, out uint storedChecksum)
    {
        storedChecksum = 0;
        if (page.Length < PageSize)
        {
            return false;
        }

        storedChecksum = BitConverter.ToUInt32(page[ChecksumOffset..PageSize]);
        if (storedChecksum == 0)
        {
            return true;
        }

        var copy = page[..PageSize].ToArray();
        copy[ChecksumOffset] = 0;
        copy[ChecksumOffset + 1] = 0;
        copy[ChecksumOffset + 2] = 0;
        copy[ChecksumOffset + 3] = 0;
        return Compute(copy) == storedChecksum;
    }

    private static uint Compute(ReadOnlySpan<byte> data)
    {
        var crc = 0xFFFFFFFFu;
        foreach (var b in data)
        {
            crc = Crc32Table[(crc ^ b) & 0xFF] ^ (crc >> 8);
        }

        return crc ^ 0xFFFFFFFFu;
    }

    private static uint[] CreateCrc32Table()
    {
        const uint polynomial = 0xEDB88320u;
        var table = new uint[256];
        for (uint i = 0; i < 256; i++)
        {
            var crc = i;
            for (var j = 0; j < 8; j++)
            {
                crc = (crc & 1) != 0 ? polynomial ^ (crc >> 1) : crc >> 1;
            }

            table[i] = crc;
        }

        return table;
    }
}
