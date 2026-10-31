/* FILE: native/gmb_db_c/src/ISAM/IsamPageChecksum.c */
/* Port 1:1 di modules/Database/ISAM/IsamPageChecksum.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamPageChecksum
//namespace Basic.ISAM;
//
///// <summary>
///// CRC32 IEEE per pagine B+ da 4096 byte. I byte 4092–4095 contengono il checksum;
///// durante il calcolo quella regione è trattata come zero (stile SQLite page checksum).
///// </summary>
//internal static class IsamPageChecksum
//{
//    internal const int PageSize = 4096;
//    internal const int ChecksumOffset = PageSize - sizeof(uint);
//    private static readonly uint[] Crc32Table = CreateCrc32Table();
//
//    internal static void WriteChecksum(Span<byte> page)
//    {
//        if (page.Length < PageSize)
//        {
//            throw new ArgumentException($"Il buffer pagina deve essere almeno {PageSize} byte.", nameof(page));
//        }
//
//        page[ChecksumOffset] = 0;
//        page[ChecksumOffset + 1] = 0;
//        page[ChecksumOffset + 2] = 0;
//        page[ChecksumOffset + 3] = 0;
//
//        var crc = Compute(page[..PageSize]);
//        page[ChecksumOffset] = (byte)crc;
//        page[ChecksumOffset + 1] = (byte)(crc >> 8);
//        page[ChecksumOffset + 2] = (byte)(crc >> 16);
//        page[ChecksumOffset + 3] = (byte)(crc >> 24);
//    }
//
//    internal static bool TryValidate(ReadOnlySpan<byte> page, out uint storedChecksum)
//    {
//        storedChecksum = 0;
//        if (page.Length < PageSize)
//        {
//            return false;
//        }
//
//        storedChecksum = BitConverter.ToUInt32(page[ChecksumOffset..PageSize]);
//        if (storedChecksum == 0)
//        {
//            return true;
//        }
//
//        var copy = page[..PageSize].ToArray();
//        copy[ChecksumOffset] = 0;
//        copy[ChecksumOffset + 1] = 0;
//        copy[ChecksumOffset + 2] = 0;
//        copy[ChecksumOffset + 3] = 0;
//        return Compute(copy) == storedChecksum;
//    }
//
//    private static uint Compute(ReadOnlySpan<byte> data)
//    {
//        var crc = 0xFFFFFFFFu;
//        foreach (var b in data)
//        {
//            crc = Crc32Table[(crc ^ b) & 0xFF] ^ (crc >> 8);
//        }
//
//        return crc ^ 0xFFFFFFFFu;
//    }
//
//    private static uint[] CreateCrc32Table()
//    {
//        const uint polynomial = 0xEDB88320u;
//        var table = new uint[256];
//        for (uint i = 0; i < 256; i++)
//        {
//            var crc = i;
//            for (var j = 0; j < 8; j++)
//            {
//                crc = (crc & 1) != 0 ? polynomial ^ (crc >> 1) : crc >> 1;
//            }
//
//            table[i] = crc;
//        }
//
//        return table;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <pthread.h>
#include <time.h>
static uint32_t IsamPageChecksum_Table[256];
static int IsamPageChecksum_TableReady;

static void IsamPageChecksum_CreateCrc32Table(void)
{
    const uint32_t polynomial = 0xEDB88320u;
    uint32_t i, j, crc;
    if (IsamPageChecksum_TableReady) return;
    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 0; j < 8; j++)
            crc = (crc & 1) != 0 ? polynomial ^ (crc >> 1) : crc >> 1;
        IsamPageChecksum_Table[i] = crc;
    }
    IsamPageChecksum_TableReady = 1;
}

static uint32_t IsamPageChecksum_Compute(const uint8_t* data, int len)
{
    uint32_t crc = 0xFFFFFFFFu;
    int i;
    IsamPageChecksum_CreateCrc32Table();
    for (i = 0; i < len; i++)
        crc = IsamPageChecksum_Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

void IsamPageChecksum_WriteChecksum(uint8_t* page, int pageLen)
{
    uint32_t crc;
    if (pageLen < IsamPageChecksum_PageSize) return;
    page[IsamPageChecksum_ChecksumOffset] = 0;
    page[IsamPageChecksum_ChecksumOffset + 1] = 0;
    page[IsamPageChecksum_ChecksumOffset + 2] = 0;
    page[IsamPageChecksum_ChecksumOffset + 3] = 0;
    crc = IsamPageChecksum_Compute(page, IsamPageChecksum_PageSize);
    page[IsamPageChecksum_ChecksumOffset] = (uint8_t)crc;
    page[IsamPageChecksum_ChecksumOffset + 1] = (uint8_t)(crc >> 8);
    page[IsamPageChecksum_ChecksumOffset + 2] = (uint8_t)(crc >> 16);
    page[IsamPageChecksum_ChecksumOffset + 3] = (uint8_t)(crc >> 24);
}

int IsamPageChecksum_TryValidate(const uint8_t* page, int pageLen, uint32_t* storedChecksum)
{
    uint8_t copy[IsamPageChecksum_PageSize];
    *storedChecksum = 0;
    if (pageLen < IsamPageChecksum_PageSize) return 0;
    *storedChecksum = (uint32_t)page[IsamPageChecksum_ChecksumOffset]
        | ((uint32_t)page[IsamPageChecksum_ChecksumOffset + 1] << 8)
        | ((uint32_t)page[IsamPageChecksum_ChecksumOffset + 2] << 16)
        | ((uint32_t)page[IsamPageChecksum_ChecksumOffset + 3] << 24);
    if (*storedChecksum == 0) return 1;
    memcpy(copy, page, IsamPageChecksum_PageSize);
    copy[IsamPageChecksum_ChecksumOffset] = 0;
    copy[IsamPageChecksum_ChecksumOffset + 1] = 0;
    copy[IsamPageChecksum_ChecksumOffset + 2] = 0;
    copy[IsamPageChecksum_ChecksumOffset + 3] = 0;
    return IsamPageChecksum_Compute(copy, IsamPageChecksum_PageSize) == *storedChecksum;
}
