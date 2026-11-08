/* crypto_encoding.c — hex, base64 (RFC 4648) e base64url senza padding.
 * Tutte le encode ritornano `char*` malloc'd; le decode scrivono nel buffer del
 * chiamante e ritornano i byte scritti (-1 input invalido, -2 cap insufficiente).
 */
#include "gmbcrypto.h"

#include <stdlib.h>
#include <string.h>

static const char HEX_LOWER[] = "0123456789abcdef";
static const char B64_STD[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char B64_URL[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/* ── hex ──────────────────────────────────────────────────────────────────── */
char* ofx_crypto_hex_encode(const uint8_t* data, size_t len)
{
    char* out;
    size_t i;
    if (data == NULL && len > 0) return NULL;
    out = (char*)malloc(len * 2 + 1);
    if (out == NULL) return NULL;
    for (i = 0; i < len; i++) {
        out[i * 2] = HEX_LOWER[data[i] >> 4];
        out[i * 2 + 1] = HEX_LOWER[data[i] & 0x0F];
    }
    out[len * 2] = '\0';
    return out;
}

char* ofx_crypto_hex_encode_str(const char* text)
{
    if (text == NULL) text = "";
    return ofx_crypto_hex_encode((const uint8_t*)text, strlen(text));
}

char* ofx_crypto_base64_encode_str(const char* text)
{
    if (text == NULL) text = "";
    return ofx_crypto_base64_encode((const uint8_t*)text, strlen(text));
}

char* ofx_crypto_base64url_encode_str(const char* text)
{
    if (text == NULL) text = "";
    return ofx_crypto_base64url_encode((const uint8_t*)text, strlen(text));
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int ofx_crypto_hex_decode(const char* text, uint8_t* out, size_t cap)
{
    size_t n, i;
    if (text == NULL || out == NULL) return -1;
    n = strlen(text);
    if (n % 2 != 0) return -1;
    if (n / 2 > cap) return -2;
    for (i = 0; i < n; i += 2) {
        int hi = hex_nibble(text[i]);
        int lo = hex_nibble(text[i + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return (int)(n / 2);
}

/* ── base64 / base64url ───────────────────────────────────────────────────── */
static char* b64_encode(const uint8_t* data, size_t len, const char* alphabet, int pad)
{
    size_t out_len = ((len + 2) / 3) * 4;
    char* out;
    size_t i, o = 0;
    if (data == NULL && len > 0) return NULL;
    out = (char*)malloc(out_len + 1);
    if (out == NULL) return NULL;
    for (i = 0; i + 2 < len; i += 3) {
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
        out[o++] = alphabet[(v >> 18) & 0x3F];
        out[o++] = alphabet[(v >> 12) & 0x3F];
        out[o++] = alphabet[(v >> 6) & 0x3F];
        out[o++] = alphabet[v & 0x3F];
    }
    if (i < len) {
        uint32_t v = (uint32_t)data[i] << 16;
        int rem = (int)(len - i);
        if (rem == 2) v |= (uint32_t)data[i + 1] << 8;
        out[o++] = alphabet[(v >> 18) & 0x3F];
        out[o++] = alphabet[(v >> 12) & 0x3F];
        if (rem == 2) {
            out[o++] = alphabet[(v >> 6) & 0x3F];
            if (pad) out[o++] = '=';
        } else {
            if (pad) { out[o++] = '='; out[o++] = '='; }
        }
    }
    out[o] = '\0';
    return out;
}

char* ofx_crypto_base64_encode(const uint8_t* data, size_t len)
{
    return b64_encode(data, len, B64_STD, 1);
}

char* ofx_crypto_base64url_encode(const uint8_t* data, size_t len)
{
    return b64_encode(data, len, B64_URL, 0);
}

static int b64_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;   /* standard e url */
    if (c == '/' || c == '_') return 63;
    return -1;
}

/* Accetta sia l'alfabeto standard sia quello url, con o senza padding. */
static int b64_decode(const char* text, uint8_t* out, size_t cap)
{
    size_t n, i, o = 0;
    uint32_t acc = 0;
    int bits = 0;
    if (text == NULL || out == NULL) return -1;
    n = strlen(text);
    for (i = 0; i < n; i++) {
        char c = text[i];
        int v;
        if (c == '=' || c == '\n' || c == '\r') continue;
        v = b64_value(c);
        if (v < 0) return -1;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= cap) return -2;
            out[o++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    return (int)o;
}

int ofx_crypto_base64_decode(const char* text, uint8_t* out, size_t cap)
{
    return b64_decode(text, out, cap);
}

int ofx_crypto_base64url_decode(const char* text, uint8_t* out, size_t cap)
{
    return b64_decode(text, out, cap);
}
