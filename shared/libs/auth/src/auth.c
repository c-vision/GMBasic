/* ============================================================================
 * auth.c — JWT HS256 self-contained per la lib `auth` (shared/libs/auth).
 *
 * Header fisso {"alg":"HS256","typ":"JWT"}, firma HMAC-SHA256, base64url.
 * SHA-256/HMAC e base64url sono implementati QUI (C99 puro, nessuna dipendenza
 * esterna): la lib è self-contained e non linka OpenSSL.
 *
 * Simboli esportati con prefisso ofx_auth_ per non collidere col runtime.
 * ========================================================================== */
#include "gmbauth.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── SHA-256 ─────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t h[8];
    uint64_t total_len;
    uint8_t  block[64];
    size_t   block_len;
} auth_sha256_ctx;

static const uint32_t AUTH_K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t auth_rotr32(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

static void auth_sha256_block(auth_sha256_ctx* ctx, const uint8_t* p)
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i*4] << 24 | (uint32_t)p[i*4+1] << 16 | (uint32_t)p[i*4+2] << 8 | p[i*4+3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = auth_rotr32(w[i-15],7) ^ auth_rotr32(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = auth_rotr32(w[i-2],17) ^ auth_rotr32(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = ctx->h[0]; b = ctx->h[1]; c = ctx->h[2]; d = ctx->h[3];
    e = ctx->h[4]; f = ctx->h[5]; g = ctx->h[6]; h = ctx->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = auth_rotr32(e,6) ^ auth_rotr32(e,11) ^ auth_rotr32(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + AUTH_K256[i] + w[i];
        uint32_t S0 = auth_rotr32(a,2) ^ auth_rotr32(a,13) ^ auth_rotr32(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->h[0] += a; ctx->h[1] += b; ctx->h[2] += c; ctx->h[3] += d;
    ctx->h[4] += e; ctx->h[5] += f; ctx->h[6] += g; ctx->h[7] += h;
}

static void auth_sha256_init(auth_sha256_ctx* ctx)
{
    ctx->h[0]=0x6a09e667; ctx->h[1]=0xbb67ae85; ctx->h[2]=0x3c6ef372; ctx->h[3]=0xa54ff53a;
    ctx->h[4]=0x510e527f; ctx->h[5]=0x9b05688c; ctx->h[6]=0x1f83d9ab; ctx->h[7]=0x5be0cd19;
    ctx->total_len = 0; ctx->block_len = 0;
}

static void auth_sha256_update(auth_sha256_ctx* ctx, const uint8_t* data, size_t len)
{
    ctx->total_len += len;
    while (len > 0) {
        size_t take = 64 - ctx->block_len;
        if (take > len) take = len;
        memcpy(ctx->block + ctx->block_len, data, take);
        ctx->block_len += take; data += take; len -= take;
        if (ctx->block_len == 64) { auth_sha256_block(ctx, ctx->block); ctx->block_len = 0; }
    }
}

static void auth_sha256_final(auth_sha256_ctx* ctx, uint8_t out[32])
{
    uint64_t bit_len = ctx->total_len * 8;
    uint8_t pad = 0x80, zero = 0, lenb[8];
    int i;
    auth_sha256_update(ctx, &pad, 1);
    while (ctx->block_len != 56) auth_sha256_update(ctx, &zero, 1);
    for (i = 0; i < 8; i++) lenb[i] = (uint8_t)(bit_len >> (56 - i*8));
    auth_sha256_update(ctx, lenb, 8);
    for (i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(ctx->h[i] >> 24);
        out[i*4+1] = (uint8_t)(ctx->h[i] >> 16);
        out[i*4+2] = (uint8_t)(ctx->h[i] >> 8);
        out[i*4+3] = (uint8_t)ctx->h[i];
    }
}

static void auth_sha256(const uint8_t* data, size_t len, uint8_t out[32])
{
    auth_sha256_ctx ctx;
    auth_sha256_init(&ctx);
    auth_sha256_update(&ctx, data, len);
    auth_sha256_final(&ctx, out);
}

/* ── HMAC-SHA256 (RFC 2104) ──────────────────────────────────────────────── */
static void auth_hmac_sha256(const uint8_t* key, size_t key_len,
                             const uint8_t* msg, size_t msg_len, uint8_t out[32])
{
    uint8_t k[64];
    uint8_t* inner;
    uint8_t* outer;
    uint8_t d[32];
    size_t i;
    memset(k, 0, sizeof(k));
    if (key_len > sizeof(k)) auth_sha256(key, key_len, k);
    else memcpy(k, key, key_len);

    inner = (uint8_t*)malloc(64 + msg_len);
    outer = (uint8_t*)malloc(64 + 32);
    if (!inner || !outer) { free(inner); free(outer); return; }

    for (i = 0; i < 64; i++) { inner[i] = (uint8_t)(k[i] ^ 0x36); }
    memcpy(inner + 64, msg, msg_len);
    auth_sha256(inner, 64 + msg_len, d);

    for (i = 0; i < 64; i++) { outer[i] = (uint8_t)(k[i] ^ 0x5c); }
    memcpy(outer + 64, d, 32);
    auth_sha256(outer, 64 + 32, out);

    free(inner);
    free(outer);
}

/* ── base64url (RFC 4648 §5, senza padding) ──────────────────────────────── */
static const char AUTH_B64URL[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static char* auth_b64url_encode(const uint8_t* data, size_t len)
{
    size_t out_len = 4 * ((len + 2) / 3);
    size_t pad = (3 - (len % 3)) % 3;
    size_t olen = out_len - pad;
    char* out = (char*)malloc(olen + 1);
    size_t o = 0, i;
    if (!out) return NULL;
    for (i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i + 2];
        out[o++] = AUTH_B64URL[(n >> 18) & 63];
        out[o++] = AUTH_B64URL[(n >> 12) & 63];
        if (i + 1 < len) out[o++] = AUTH_B64URL[(n >> 6) & 63];
        if (i + 2 < len) out[o++] = AUTH_B64URL[n & 63];
    }
    out[olen] = '\0';
    return out;
}

static int auth_b64url_index(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

static uint8_t* auth_b64url_decode(const char* input, size_t* len_out)
{
    size_t in_len, cap, o = 0;
    uint32_t buf = 0;
    int bits = 0;
    uint8_t* out;
    size_t i;
    *len_out = 0;
    if (!input) return NULL;
    in_len = strlen(input);
    cap = in_len / 4 * 3 + 3;
    out = (uint8_t*)malloc(cap ? cap : 1);
    if (!out) return NULL;
    for (i = 0; i < in_len; i++) {
        char c = input[i];
        int d;
        if (c == '=' || c == '\n' || c == '\r') continue;
        d = auth_b64url_index(c);
        if (d < 0) { free(out); return NULL; }
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

/* ── JWT HS256 ───────────────────────────────────────────────────────────── */
#define AUTH_JWT_HEADER "{\"alg\":\"HS256\",\"typ\":\"JWT\"}"

char* ofx_auth_jwt_create(const char* claims, const char* secret)
{
    char* h;
    char* p;
    char* s;
    char* input;
    char* out;
    size_t hl, pl, sl;
    uint8_t sig[32];
    if (!claims) claims = "";
    if (!secret) secret = "";
    h = auth_b64url_encode((const uint8_t*)AUTH_JWT_HEADER, strlen(AUTH_JWT_HEADER));
    p = auth_b64url_encode((const uint8_t*)claims, strlen(claims));
    if (!h || !p) { free(h); free(p); return NULL; }
    hl = strlen(h); pl = strlen(p);
    input = (char*)malloc(hl + 1 + pl + 1);
    if (!input) { free(h); free(p); return NULL; }
    memcpy(input, h, hl);
    input[hl] = '.';
    memcpy(input + hl + 1, p, pl);
    input[hl + 1 + pl] = 0;
    auth_hmac_sha256((const uint8_t*)secret, strlen(secret), (const uint8_t*)input, hl + 1 + pl, sig);
    s = auth_b64url_encode(sig, 32);
    free(input);
    if (!s) { free(h); free(p); return NULL; }
    sl = strlen(s);
    out = (char*)malloc(hl + 1 + pl + 1 + sl + 1);
    if (!out) { free(h); free(p); free(s); return NULL; }
    memcpy(out, h, hl);
    out[hl] = '.';
    memcpy(out + hl + 1, p, pl);
    out[hl + 1 + pl] = '.';
    memcpy(out + hl + 1 + pl + 1, s, sl);
    out[hl + 1 + pl + 1 + sl] = 0;
    free(h); free(p); free(s);
    return out;
}

static int auth_jwt_split(const char* token, const char** h, size_t* hl,
                          const char** p, size_t* pl,
                          const char** s, size_t* sl)
{
    const char* d1;
    const char* d2;
    if (!token) return 0;
    d1 = strchr(token, '.');
    if (!d1 || d1 == token) return 0;
    d2 = strchr(d1 + 1, '.');
    if (!d2 || d2 == d1 + 1) return 0;
    if (strchr(d2 + 1, '.')) return 0;
    *h = token; *hl = (size_t)(d1 - token);
    *p = d1 + 1; *pl = (size_t)(d2 - d1 - 1);
    *s = d2 + 1; *sl = strlen(d2 + 1);
    return (*sl > 0) ? 1 : 0;
}

int ofx_auth_jwt_verify(const char* token, const char* secret)
{
    const char *h, *p, *s;
    size_t hl, pl, sl;
    char* input;
    char* expect;
    uint8_t sig[32];
    int ok;
    if (!auth_jwt_split(token, &h, &hl, &p, &pl, &s, &sl)) return 0;
    if (!secret) secret = "";
    input = (char*)malloc(hl + 1 + pl + 1);
    if (!input) return 0;
    memcpy(input, h, hl);
    input[hl] = '.';
    memcpy(input + hl + 1, p, pl);
    input[hl + 1 + pl] = 0;
    auth_hmac_sha256((const uint8_t*)secret, strlen(secret), (const uint8_t*)input, hl + 1 + pl, sig);
    free(input);
    expect = auth_b64url_encode(sig, 32);
    if (!expect) return 0;
    ok = (strlen(expect) == sl) && (memcmp(expect, s, sl) == 0);
    free(expect);
    return ok ? 1 : 0;
}

char* ofx_auth_jwt_payload(const char* token)
{
    const char *h, *p, *s;
    size_t hl, pl, sl;
    char* part;
    char* out;
    size_t olen = 0;
    uint8_t* raw;
    if (!auth_jwt_split(token, &h, &hl, &p, &pl, &s, &sl)) return strdup("");
    (void)h; (void)hl; (void)s; (void)sl;
    part = (char*)malloc(pl + 1);
    if (!part) return NULL;
    memcpy(part, p, pl);
    part[pl] = 0;
    raw = auth_b64url_decode(part, &olen);
    free(part);
    if (!raw) return strdup("");
    out = (char*)malloc(olen + 1);
    if (!out) { free(raw); return NULL; }
    memcpy(out, raw, olen);
    out[olen] = 0;
    free(raw);
    return out;
}
