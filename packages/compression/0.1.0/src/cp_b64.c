/* cp_b64.c — base64 standard (RFC 4648, padding '=') per il pacchetto `compression`.
 *
 * Copia self-contained di gmb_base64_encode/gmb_base64_decode del runtime, con il
 * prefisso gmbcp_ per non collidere con i simboli del runtime quando la dylib del
 * pacchetto e il processo GMBasic convivono.
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static const char GBMCP_B64_ALPHA[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* gmbcp_b64_encode(const uint8_t* data, size_t len)
{
    size_t out_len = 4 * ((len + 2) / 3);
    char* out = (char*)malloc(out_len + 1);
    size_t o = 0;
    if (!out) return NULL;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i + 2];
        out[o++] = GBMCP_B64_ALPHA[(n >> 18) & 63];
        out[o++] = GBMCP_B64_ALPHA[(n >> 12) & 63];
        out[o++] = (i + 1 < len) ? GBMCP_B64_ALPHA[(n >> 6) & 63] : '=';
        out[o++] = (i + 2 < len) ? GBMCP_B64_ALPHA[n & 63] : '=';
    }
    out[out_len] = '\0';
    return out;
}

static int gmbcp_b64_index(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

uint8_t* gmbcp_b64_decode(const char* input, size_t* len_out)
{
    size_t in_len = input ? strlen(input) : 0;
    size_t cap = in_len / 4 * 3 + 3;
    uint8_t* out = (uint8_t*)malloc(cap ? cap : 1);
    size_t o = 0;
    uint32_t buf = 0;
    int bits = 0;
    if (!out) return NULL;
    for (size_t i = 0; i < in_len; i++) {
        int d;
        if (input[i] == '=' || input[i] == '\n' || input[i] == '\r') continue;
        d = gmbcp_b64_index(input[i]);
        if (d < 0) continue;
        buf = (buf << 6) | (uint32_t)d;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (uint8_t)((buf >> bits) & 0xff);
        }
    }
    *len_out = o;
    return out;
}
