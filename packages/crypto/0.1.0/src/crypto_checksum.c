/* crypto_checksum.c — checksum non crittografici del dominio CRYPTOGRAPHY.
 *
 * Tutti i valori sono calcolati su byte; le varianti sono selezionate da un
 * intero, cosi' l'export BASIC resta una firma sola per famiglia.
 * Riferimenti dei vettori di test: "123456789" (CRC), "Wikipedia" (Adler-32).
 */
#include "gmbcrypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── CRC-32/ISO-HDLC (zlib): poly 0xEDB88320 riflesso, init 0xFFFFFFFF, xorout ── */
static uint32_t crc32_table[256];
static uint32_t crc32c_table[256];
static uint64_t crc64_table[256];
static int tables_ready = 0;

static void build_tables(void)
{
    int i, k;
    if (tables_ready) return;
    for (i = 0; i < 256; i++) {
        uint32_t c = (uint32_t)i;
        uint32_t d = (uint32_t)i;
        uint64_t e = (uint64_t)i;
        for (k = 0; k < 8; k++) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            d = (d & 1u) ? (0x82F63B78u ^ (d >> 1)) : (d >> 1);
            e = (e & 1ull) ? (0xC96C5795D7870F42ull ^ (e >> 1)) : (e >> 1);
        }
        crc32_table[i] = c;
        crc32c_table[i] = d;
        crc64_table[i] = e;
    }
    tables_ready = 1;
}

uint32_t ofx_crypto_crc32(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    build_tables();
    if (data == NULL) return 0;
    for (i = 0; i < len; i++)
        crc = crc32_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

uint32_t ofx_crypto_crc32c(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    build_tables();
    if (data == NULL) return 0;
    for (i = 0; i < len; i++)
        crc = crc32c_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

uint64_t ofx_crypto_crc64(const uint8_t* data, size_t len, int variant)
{
    uint64_t crc = (variant == 1) ? 0xFFFFFFFFFFFFFFFFull : 0ull;
    size_t i;
    build_tables();
    if (data == NULL) return 0;
    for (i = 0; i < len; i++)
        crc = crc64_table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return (variant == 1) ? (crc ^ 0xFFFFFFFFFFFFFFFFull) : crc;
}

/* ── CRC-16 nelle quattro varianti classiche ──────────────────────────────── */
uint32_t ofx_crypto_crc16(const uint8_t* data, size_t len, int variant)
{
    size_t i;
    int k;
    if (data == NULL) return 0;
    switch (variant) {
    case 0: { /* ARC / IBM: poly 0xA001 riflesso, init 0x0000 */
        uint16_t crc = 0x0000u;
        for (i = 0; i < len; i++) {
            crc ^= data[i];
            for (k = 0; k < 8; k++)
                crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
        }
        return crc;
    }
    case 1: { /* CCITT-FALSE: poly 0x1021, init 0xFFFF, non riflesso */
        uint16_t crc = 0xFFFFu;
        for (i = 0; i < len; i++) {
            crc ^= (uint16_t)((uint16_t)data[i] << 8);
            for (k = 0; k < 8; k++)
                crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
        return crc;
    }
    case 2: { /* XMODEM: poly 0x1021, init 0x0000 */
        uint16_t crc = 0x0000u;
        for (i = 0; i < len; i++) {
            crc ^= (uint16_t)((uint16_t)data[i] << 8);
            for (k = 0; k < 8; k++)
                crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
        return crc;
    }
    default: { /* KERMIT: poly 0x8408 riflesso, init 0x0000 */
        uint16_t crc = 0x0000u;
        for (i = 0; i < len; i++) {
            crc ^= data[i];
            for (k = 0; k < 8; k++)
                crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0x8408u) : (uint16_t)(crc >> 1);
        }
        return crc;
    }
    }
}

/* ── Adler-32 (RFC 1950) ──────────────────────────────────────────────────── */
uint32_t ofx_crypto_adler32(const uint8_t* data, size_t len)
{
    uint32_t a = 1, b = 0;
    size_t i;
    if (data == NULL) return 1;
    for (i = 0; i < len; i++) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

/* ── forme testuali ───────────────────────────────────────────────────────── */
char* ofx_crypto_crc32_hex_str(const char* text)
{
    char* out;
    uint32_t v;
    if (text == NULL) text = "";
    v = ofx_crypto_crc32((const uint8_t*)text, strlen(text));
    out = (char*)malloc(9);
    if (out == NULL) return NULL;
    snprintf(out, 9, "%08x", (unsigned)v);
    return out;
}

char* ofx_crypto_crc64_hex(const uint8_t* data, size_t len, int variant)
{
    char* out = (char*)malloc(17);
    if (out == NULL) return NULL;
    snprintf(out, 17, "%016llx", (unsigned long long)ofx_crypto_crc64(data, len, variant));
    return out;
}

/* ── Fletcher-32 (coppie di byte little-endian, modulo 65535) ─────────────── */
uint32_t ofx_crypto_fletcher32(const uint8_t* data, size_t len)
{
    uint32_t sum1 = 0xFFFFu, sum2 = 0xFFFFu;
    size_t i = 0;
    if (data == NULL) return 0;
    while (i < len) {
        /* parole little-endian: e' la convenzione del vettore canonico
           Fletcher-32("abcde") = 0xF04FC729 */
        uint32_t word = data[i];
        if (i + 1 < len) word |= (uint32_t)data[i + 1] << 8;
        sum1 = (sum1 + word) % 65535u;
        sum2 = (sum2 + sum1) % 65535u;
        i += 2;
    }
    return (sum2 << 16) | sum1;
}
