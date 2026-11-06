/* gc_checksum.c — CRC-32 IEEE + Adler-32 (vedi gc_checksum.h). */
#include "gc_checksum.h"

static uint32_t g_crc_table[256];
static int g_crc_ready = 0;

static void crc_init(void)
{
    if (g_crc_ready) return;
    g_crc_ready = 1;
    for (uint32_t i = 0; i < 256u; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_crc_table[i] = c;
    }
}

uint32_t gc_crc32_update(uint32_t crc, const uint8_t *data, size_t n)
{
    crc_init();
    for (size_t i = 0; i < n; i++)
        crc = g_crc_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc;
}

uint32_t gc_crc32(const uint8_t *data, size_t n)
{
    return gc_crc32_update(0xFFFFFFFFu, data, n) ^ 0xFFFFFFFFu;
}

uint32_t gc_adler32(const uint8_t *data, size_t n)
{
    uint32_t a = 1u, b = 0u;
    for (size_t i = 0; i < n; i++) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}
