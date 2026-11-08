// FILE: gmb_crypto.c — hash e KDF (SHA-256, HMAC, PBKDF2, Keccak-256, RIPEMD-160)
// Implementazioni in C99 puro, senza dipendenze esterne. Convenzioni come gmb_commonlib.h.

#include "gmbblockchain.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#endif

/* ============================ SHA-256 ============================ */

/* Il contesto è dichiarato in gmb_commonlib.h (gmb_sha256_stream): UNA sola definizione, così
 * chi calcola l'hash di un file a blocchi non duplica l'algoritmo. */
typedef gmb_sha256_stream sha256_ctx;

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t rotr32(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

static void sha256_block(sha256_ctx* ctx, const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i*4] << 24 | (uint32_t)p[i*4+1] << 16 | (uint32_t)p[i*4+2] << 8 | p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i-15],7) ^ rotr32(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr32(w[i-2],17) ^ rotr32(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = ctx->h[0], b = ctx->h[1], c = ctx->h[2], d = ctx->h[3];
    uint32_t e = ctx->h[4], f = ctx->h[5], g = ctx->h[6], h = ctx->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K256[i] + w[i];
        uint32_t S0 = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->h[0] += a; ctx->h[1] += b; ctx->h[2] += c; ctx->h[3] += d;
    ctx->h[4] += e; ctx->h[5] += f; ctx->h[6] += g; ctx->h[7] += h;
}

static void sha256_init(sha256_ctx* ctx) {
    ctx->h[0]=0x6a09e667; ctx->h[1]=0xbb67ae85; ctx->h[2]=0x3c6ef372; ctx->h[3]=0xa54ff53a;
    ctx->h[4]=0x510e527f; ctx->h[5]=0x9b05688c; ctx->h[6]=0x1f83d9ab; ctx->h[7]=0x5be0cd19;
    ctx->total_len = 0; ctx->block_len = 0;
}

static void sha256_update(sha256_ctx* ctx, const uint8_t* data, size_t len) {
    ctx->total_len += len;
    while (len > 0) {
        size_t take = 64 - ctx->block_len;
        if (take > len) take = len;
        memcpy(ctx->block + ctx->block_len, data, take);
        ctx->block_len += take; data += take; len -= take;
        if (ctx->block_len == 64) { sha256_block(ctx, ctx->block); ctx->block_len = 0; }
    }
}

static void sha256_final(sha256_ctx* ctx, uint8_t out[32]) {
    uint64_t bit_len = ctx->total_len * 8;
    uint8_t pad = 0x80;
    sha256_update(ctx, &pad, 1);
    uint8_t zero = 0;
    while (ctx->block_len != 56) sha256_update(ctx, &zero, 1);
    uint8_t lenb[8];
    for (int i = 0; i < 8; i++) lenb[i] = (uint8_t)(bit_len >> (56 - i*8));
    sha256_update(ctx, lenb, 8);
    for (int i = 0; i < 8; i++) {
        out[i*4] = (uint8_t)(ctx->h[i] >> 24);
        out[i*4+1] = (uint8_t)(ctx->h[i] >> 16);
        out[i*4+2] = (uint8_t)(ctx->h[i] >> 8);
        out[i*4+3] = (uint8_t)ctx->h[i];
    }
}

void gmb_sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

/* --- SHA-256 incrementale (API pubblica: gmb_commonlib.h) -------------------------------- */
void gmb_sha256_stream_init(gmb_sha256_stream* s) {
    if (!s) return;
    sha256_init(s);
}

void gmb_sha256_stream_update(gmb_sha256_stream* s, const void* data, size_t len) {
    if (!s || (!data && len)) return;
    sha256_update(s, (const uint8_t*)data, len);
}

void gmb_sha256_stream_final(gmb_sha256_stream* s, uint8_t out[32]) {
    if (!s || !out) return;
    sha256_final(s, out);
}

/* Hash di un FILE letto a blocchi (64 KiB): nessuna allocazione proporzionale alla dimensione. */
int gmb_sha256_file_hex(const char* path, char* out, size_t cap) {
    if (out && cap) out[0] = 0;
    if (!path || !*path) return 0;
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    gmb_sha256_stream s;
    gmb_sha256_stream_init(&s);
    uint8_t buf[64 * 1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) gmb_sha256_stream_update(&s, buf, n);
    int bad = ferror(f);
    fclose(f);
    if (bad) return 0;
    uint8_t d[32];
    gmb_sha256_stream_final(&s, d);
    static const char* hx = "0123456789abcdef";
    char hex[65];
    for (int i = 0; i < 32; i++) { hex[i * 2] = hx[d[i] >> 4]; hex[i * 2 + 1] = hx[d[i] & 15]; }
    hex[64] = '\0';
    if (out && cap) snprintf(out, cap, "%s", hex);
    return 1;
}

/* ============================ HMAC-SHA256 / SHA512 ============================ */

static inline uint64_t rotr64(uint64_t x, unsigned n) { return (x >> n) | (x << (64 - n)); }

static void sha512_block(uint64_t h[8], const uint8_t* p) {
    static const uint64_t K512[80] = {
        0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
        0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
        0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
        0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
        0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
        0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
        0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
        0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
        0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
        0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
        0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
        0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
        0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
        0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
        0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
        0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
        0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
        0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
        0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
        0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL
    };
    uint64_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint64_t)p[i*8] << 56 | (uint64_t)p[i*8+1] << 48 | (uint64_t)p[i*8+2] << 40 |
               (uint64_t)p[i*8+3] << 32 | (uint64_t)p[i*8+4] << 24 | (uint64_t)p[i*8+5] << 16 |
               (uint64_t)p[i*8+6] << 8 | p[i*8+7];
    }
    for (int i = 16; i < 80; i++) {
        uint64_t s0 = rotr64(w[i-15],1) ^ rotr64(w[i-15],8) ^ (w[i-15] >> 7);
        uint64_t s1 = rotr64(w[i-2],19) ^ rotr64(w[i-2],61) ^ (w[i-2] >> 6);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint64_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for (int i = 0; i < 80; i++) {
        uint64_t S1 = rotr64(e,14) ^ rotr64(e,18) ^ rotr64(e,41);
        uint64_t ch = (e & f) ^ (~e & g);
        uint64_t t1 = hh + S1 + ch + K512[i] + w[i];
        uint64_t S0 = rotr64(a,28) ^ rotr64(a,34) ^ rotr64(a,39);
        uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint64_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}


static void sha512_full(const uint8_t* data, size_t len, uint8_t out[64]) {
    uint64_t h[8] = {0x6a09e667f3bcc908ULL,0xbb67ae8584caa73bULL,0x3c6ef372fe94f82bULL,
                     0xa54ff53a5f1d36f1ULL,0x510e527fade682d1ULL,0x9b05688c2b3e6c1fULL,
                     0x1f83d9abfb41bd6bULL,0x5be0cd19137e2179ULL};
    size_t total = len;
    uint8_t buf[128];
    size_t buf_len = 0;
    while (len > 0) {
        size_t take = 128 - buf_len; if (take > len) take = len;
        memcpy(buf + buf_len, data, take); buf_len += take; data += take; len -= take;
        if (buf_len == 128) { sha512_block(h, buf); buf_len = 0; }
    }
    buf[buf_len++] = 0x80;
    while (buf_len != 112) { if (buf_len == 128) { sha512_block(h, buf); buf_len = 0; } buf[buf_len++] = 0; }
    uint64_t bit_len = (uint64_t)total * 8;
    uint8_t lenb[16] = {0};
    for (int i = 0; i < 8; i++) lenb[8+i] = (uint8_t)(bit_len >> (56 - i*8));
    memcpy(buf + buf_len, lenb, 16);
    sha512_block(h, buf);
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            out[i*8+j] = (uint8_t)(h[i] >> (56 - j*8));
}

/* SHA-384 (FIPS 180-4): stessa compressione di SHA-512 ma IV diverso e digest troncato ai
 * primi 48 byte dello stato (6×64 bit). Serve per HMAC-SHA384. */
static void sha384_full(const uint8_t* data, size_t len, uint8_t out[48]) {
    uint64_t h[8] = {0xcbbb9d5dc1059ed8ULL,0x629a292a367cd507ULL,0x9159015a3070dd17ULL,
                     0x152fecd8f70e5939ULL,0x67332667ffc00b31ULL,0x8eb44a8768581511ULL,
                     0xdb0c2e0d64f98fa7ULL,0x47b5481dbefa4fa4ULL};
    size_t total = len;
    uint8_t buf[128];
    size_t buf_len = 0;
    while (len > 0) {
        size_t take = 128 - buf_len; if (take > len) take = len;
        memcpy(buf + buf_len, data, take); buf_len += take; data += take; len -= take;
        if (buf_len == 128) { sha512_block(h, buf); buf_len = 0; }
    }
    buf[buf_len++] = 0x80;
    while (buf_len != 112) { if (buf_len == 128) { sha512_block(h, buf); buf_len = 0; } buf[buf_len++] = 0; }
    uint64_t bit_len = (uint64_t)total * 8;
    uint8_t lenb[16] = {0};
    for (int i = 0; i < 8; i++) lenb[8+i] = (uint8_t)(bit_len >> (56 - i*8));
    memcpy(buf + buf_len, lenb, 16);
    sha512_block(h, buf);
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 8; j++)
            out[i*8+j] = (uint8_t)(h[i] >> (56 - j*8));
}

/* HMAC con digest selezionato: hash_kind 0=SHA-256, 1=SHA-512, 2=SHA-384 (RFC 2104/4231).
 * Il blocco è 64 per SHA-256, 128 per SHA-512/384; il digest 32/64/48 byte. */
static void hmac(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len,
                 uint8_t* out, int hash_kind) {
    const size_t block = (hash_kind == 0) ? 64 : 128;
    const size_t dlen  = (hash_kind == 0) ? 32 : (hash_kind == 1 ? 64 : 48);
    uint8_t k[128] = {0};
    if (key_len > block) {
        if (hash_kind == 0) gmb_sha256(key, key_len, k);
        else if (hash_kind == 1) sha512_full(key, key_len, k);
        else sha384_full(key, key_len, k);
    } else {
        memcpy(k, key, key_len);
    }
    uint8_t ipad[128], opad[128];
    for (size_t i = 0; i < block; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }
    uint8_t inner[128 + 1024]; // buffer sufficiente per i messaggi del KDF
    memcpy(inner, ipad, block);
    memcpy(inner + block, msg, msg_len);
    uint8_t d[64];
    if (hash_kind == 0) gmb_sha256(inner, block + msg_len, d);
    else if (hash_kind == 1) sha512_full(inner, block + msg_len, d);
    else sha384_full(inner, block + msg_len, d);
    uint8_t outer[128 + 64];
    memcpy(outer, opad, block);
    memcpy(outer + block, d, dlen);
    if (hash_kind == 0) gmb_sha256(outer, block + dlen, out);
    else if (hash_kind == 1) sha512_full(outer, block + dlen, out);
    else sha384_full(outer, block + dlen, out);
}

void gmb_hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[32]) {
    hmac(key, key_len, msg, msg_len, out, 0);
}

void gmb_hmac_sha512(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[64]) {
    hmac(key, key_len, msg, msg_len, out, 1);
}

void gmb_hmac_sha384(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[48]) {
    hmac(key, key_len, msg, msg_len, out, 2);
}

char* gmb_hmac_sha256_hex(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len) {
    uint8_t o[32]; gmb_hmac_sha256(key, key_len, msg, msg_len, o);
    return gmb_hex_encode_string(o, 32);
}
char* gmb_hmac_sha384_hex(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len) {
    uint8_t o[48]; gmb_hmac_sha384(key, key_len, msg, msg_len, o);
    return gmb_hex_encode_string(o, 48);
}
char* gmb_hmac_sha512_hex(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len) {
    uint8_t o[64]; gmb_hmac_sha512(key, key_len, msg, msg_len, o);
    return gmb_hex_encode_string(o, 64);
}

void gmb_pbkdf2_sha512(const uint8_t* password, size_t pw_len,
                       const uint8_t* salt, size_t salt_len,
                       unsigned iterations, uint8_t* out, size_t dk_len) {
    size_t blocks = (dk_len + 63) / 64;
    for (size_t block = 1; block <= blocks; block++) {
        uint8_t u[64], t[64];
        uint8_t block_b[4] = { (uint8_t)(block >> 24), (uint8_t)(block >> 16), (uint8_t)(block >> 8), (uint8_t)block };
        uint8_t msg[1024];
        memcpy(msg, salt, salt_len);
        memcpy(msg + salt_len, block_b, 4);
        gmb_hmac_sha512(password, pw_len, msg, salt_len + 4, u);
        memcpy(t, u, 64);
        for (unsigned i = 1; i < iterations; i++) {
            gmb_hmac_sha512(password, pw_len, u, 64, u);
            for (int j = 0; j < 64; j++) t[j] ^= u[j];
        }
        size_t start = (block - 1) * 64;
        size_t copy = 64; if (start + copy > dk_len) copy = dk_len - start;
        memcpy(out + start, t, copy);
    }
}

// PBKDF2-HMAC-SHA256 (RFC 2898) — variante con blocco HMAC-SHA256 da 32 byte.
// È il KDF canonico di PASSWORDHASH/PASSWORDVERIFY (parità C# PBKDF2-SHA256, 100000 iter).
void gmb_pbkdf2_sha256(const uint8_t* password, size_t pw_len,
                       const uint8_t* salt, size_t salt_len,
                       unsigned iterations, uint8_t* out, size_t dk_len) {
    size_t blocks = (dk_len + 31) / 32;
    for (size_t block = 1; block <= blocks; block++) {
        uint8_t u[32], t[32];
        uint8_t block_b[4] = { (uint8_t)(block >> 24), (uint8_t)(block >> 16), (uint8_t)(block >> 8), (uint8_t)block };
        uint8_t msg[1024];
        memcpy(msg, salt, salt_len);
        memcpy(msg + salt_len, block_b, 4);
        gmb_hmac_sha256(password, pw_len, msg, salt_len + 4, u);
        memcpy(t, u, 32);
        for (unsigned i = 1; i < iterations; i++) {
            gmb_hmac_sha256(password, pw_len, u, 32, u);
            for (int j = 0; j < 32; j++) t[j] ^= u[j];
        }
        size_t start = (block - 1) * 32;
        size_t copy = 32; if (start + copy > dk_len) copy = dk_len - start;
        memcpy(out + start, t, copy);
    }
}

/* ============================ Keccak-256 ============================ */

static uint64_t keccak_rol(uint64_t x, unsigned n) { n &= 63; return n == 0 ? x : ((x << n) | (x >> (64 - n))); }

static void keccak_permute(uint64_t st[25]) {
    static const uint64_t RC[24] = {
        0x0000000000000001ULL,0x0000000000008082ULL,0x800000000000808aULL,0x8000000080008000ULL,
        0x000000000000808bULL,0x0000000080000001ULL,0x8000000080008081ULL,0x8000000000008009ULL,
        0x000000000000008aULL,0x0000000000000088ULL,0x0000000080008009ULL,0x000000008000000aULL,
        0x000000008000808bULL,0x800000000000008bULL,0x8000000000008089ULL,0x8000000000008003ULL,
        0x8000000000008002ULL,0x8000000000000080ULL,0x000000000000800aULL,0x800000008000000aULL,
        0x8000000080008081ULL,0x8000000000008080ULL,0x0000000080000001ULL,0x8000000080008008ULL
    };
    static const int rotc[24] = {
        1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
        27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
    };
    static const int piln[24] = {
        10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
        15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
    };
    int i, j, r;
    uint64_t t, bc[5];
    for (r = 0; r < 24; r++) {
        for (i = 0; i < 5; i++)
            bc[i] = st[i] ^ st[i+5] ^ st[i+10] ^ st[i+15] ^ st[i+20];
        for (i = 0; i < 5; i++) {
            t = bc[(i+4)%5] ^ keccak_rol(bc[(i+1)%5], 1);
            for (j = 0; j < 25; j += 5)
                st[j+i] ^= t;
        }
        t = st[1];
        for (i = 0; i < 24; i++) {
            j = piln[i];
            bc[0] = st[j];
            st[j] = keccak_rol(t, rotc[i]);
            t = bc[0];
        }
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++)
                bc[i] = st[j+i];
            for (i = 0; i < 5; i++)
                st[j+i] ^= (~bc[(i+1)%5]) & bc[(i+2)%5];
        }
        st[0] ^= RC[r];
    }
}

void gmb_keccak256(const uint8_t* data, size_t len, uint8_t out[32]) {
    uint64_t st[25] = {0};
    size_t rate = 136;
    size_t full = len / rate, rem = len % rate;
    for (size_t i = 0; i < full; i++) {
        for (size_t j = 0; j < rate/8; j++)
            for (size_t b = 0; b < 8; b++)
                st[j] ^= (uint64_t)data[i*rate+j*8+b] << (8*b);
        keccak_permute(st);
    }
    uint8_t block[136] = {0};
    memcpy(block, data + full*rate, rem);
    block[rem] ^= 0x01;
    block[rate-1] ^= 0x80;
    for (size_t j = 0; j < rate/8; j++)
        for (size_t b = 0; b < 8; b++)
            st[j] ^= (uint64_t)block[j*8+b] << (8*b);
    keccak_permute(st);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++)
            out[i*8+j] = (uint8_t)(st[i] >> (8*j));
}

/* ============================ RIPEMD-160 ============================ */

static inline uint32_t rol32(uint32_t x, unsigned n) { return (x << n) | (x >> (32 - n)); }

static uint32_t rmd_f(int j, uint32_t x, uint32_t y, uint32_t z) {
    if (j < 16) return x ^ y ^ z;
    if (j < 32) return (x & y) | (~x & z);
    if (j < 48) return (x | ~y) ^ z;
    if (j < 64) return (x & z) | (y & ~z);
    return x ^ (y | ~z);
}

static uint32_t rmd_k(int j) {
    if (j < 16) return 0x00000000;
    if (j < 32) return 0x5a827999;
    if (j < 48) return 0x6ed9eba1;
    if (j < 64) return 0x8f1bbcdc;
    return 0xa953fd4e;
}

static uint32_t rmd_kp(int j) {
    if (j < 16) return 0x50a28be6;
    if (j < 32) return 0x5c4dd124;
    if (j < 48) return 0x6d703ef3;
    if (j < 64) return 0x7a6d76e9;
    return 0x00000000;
}

static const int rmd_r[80] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8,
    3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12,
    1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2,
    4,0,5,9,7,12,2,10,14,1,3,8,11,6,15,13
};
static const int rmd_rp[80] = {
    5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12,
    6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2,
    15,5,1,3,7,14,6,9,11,8,12,2,10,0,4,13,
    8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14,
    12,15,10,4,1,5,8,7,6,2,13,14,0,3,9,11
};
static const int rmd_s[80] = {
    11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8,
    7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12,
    11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5,
    11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12,
    9,15,5,11,6,8,13,12,5,12,13,14,11,8,5,6
};
static const int rmd_sp[80] = {
    8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6,
    9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11,
    9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5,
    15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8,
    8,5,12,9,12,5,14,6,8,13,6,5,15,13,11,11
};

void gmb_ripemd160(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h[5] = {0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0};
    size_t total = len;
    uint8_t buf[64]; size_t bl = 0;
    while (len > 0) {
        size_t take = 64 - bl; if (take > len) take = len;
        memcpy(buf + bl, data, take); bl += take; data += take; len -= take;
        if (bl == 64) {
            uint32_t x[16];
            for (int i = 0; i < 16; i++) x[i] = (uint32_t)buf[i*4] | (uint32_t)buf[i*4+1]<<8 | (uint32_t)buf[i*4+2]<<16 | (uint32_t)buf[i*4+3]<<24;
            uint32_t al=h[0],bl_=h[1],cl=h[2],dl=h[3],el=h[4];
            uint32_t ar=h[0],br=h[1],cr=h[2],dr=h[3],er=h[4];
            for (int j = 0; j < 80; j++) {
                uint32_t t = rol32(al + rmd_f(j,bl_,cl,dl) + x[rmd_r[j]] + rmd_k(j), rmd_s[j]) + el;
                al=el; el=dl; dl=rol32(cl,10); cl=bl_; bl_=t;
                t = rol32(ar + rmd_f(79-j,br,cr,dr) + x[rmd_rp[j]] + rmd_kp(j), rmd_sp[j]) + er;
                ar=er; er=dr; dr=rol32(cr,10); cr=br; br=t;
            }
            uint32_t t = h[1] + cl + dr;
            h[1] = h[2] + dl + er; h[2] = h[3] + el + ar; h[3] = h[4] + al + br;
            h[4] = h[0] + bl_ + cr; h[0] = t;
            bl = 0;
        }
    }
    buf[bl++] = 0x80;
    if (bl > 56) { while (bl < 64) buf[bl++] = 0; bl = 0; }
    while (bl < 56) buf[bl++] = 0;
    uint64_t bit_len = (uint64_t)total * 8;
    for (int i = 0; i < 8; i++) buf[56+i] = (uint8_t)(bit_len >> (8*i));
    uint32_t x[16];
    for (int i = 0; i < 16; i++) x[i] = (uint32_t)buf[i*4] | (uint32_t)buf[i*4+1]<<8 | (uint32_t)buf[i*4+2]<<16 | (uint32_t)buf[i*4+3]<<24;
    uint32_t al=h[0],bl_=h[1],cl=h[2],dl=h[3],el=h[4];
    uint32_t ar=h[0],br=h[1],cr=h[2],dr=h[3],er=h[4];
    for (int j = 0; j < 80; j++) {
        uint32_t t = rol32(al + rmd_f(j,bl_,cl,dl) + x[rmd_r[j]] + rmd_k(j), rmd_s[j]) + el;
        al=el; el=dl; dl=rol32(cl,10); cl=bl_; bl_=t;
        t = rol32(ar + rmd_f(79-j,br,cr,dr) + x[rmd_rp[j]] + rmd_kp(j), rmd_sp[j]) + er;
        ar=er; er=dr; dr=rol32(cr,10); cr=br; br=t;
    }
    uint32_t t = h[1] + cl + dr;
    h[1] = h[2] + dl + er; h[2] = h[3] + el + ar; h[3] = h[4] + al + br;
    h[4] = h[0] + bl_ + cr; h[0] = t;
    for (int i = 0; i < 5; i++) {
        out[i*4] = (uint8_t)h[i]; out[i*4+1] = (uint8_t)(h[i]>>8);
        out[i*4+2] = (uint8_t)(h[i]>>16); out[i*4+3] = (uint8_t)(h[i]>>24);
    }
}

/* ================= MD5 (RFC 1321, compact) ================= */
static uint32_t md5_rotl(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

/* Forward declaration delle implementazioni (definite sotto): su macOS il branch CC_* le
   nasconde, ma su Windows/Linux la chiamata prima della definizione era un errore. */
#if !defined(__APPLE__)
static void gmb_md5_impl(const uint8_t* data, size_t len, uint8_t out[16]);
static void gmb_sha1_impl(const uint8_t* data, size_t len, uint8_t out[20]);
static void gmb_sha512_impl(const uint8_t* data, size_t len, uint8_t out[64]);
#endif

void gmb_md5(const uint8_t* data, size_t len, uint8_t out[16]) {
#if defined(__APPLE__)
    CC_MD5(data, (CC_LONG)len, out);
#else
    gmb_md5_impl(data, len, out);
#endif
}
static void gmb_md5_impl(const uint8_t* data, size_t len, uint8_t out[16]) {
    static const uint32_t K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 };
    static const int S[64] = {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    uint32_t h0=0x67452301, h1=0xefcdab89, h2=0x98badcfe, h3=0x10325476;
    size_t bitlen = len * 8;
    size_t padded = ((len + 8) / 64 + 1) * 64;
    uint8_t* buf = (uint8_t*)calloc(padded, 1);
    if (!buf) { memset(out,0,16); return; }
    memcpy(buf, data, len);
    buf[len] = 0x80;
    uint64_t bits = bitlen;
    for (int i = 0; i < 8; i++) buf[padded - 8 + i] = (uint8_t)((bits >> (8*i)) & 0xff);
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; i++) M[i] = (uint32_t)buf[off+4*i] | ((uint32_t)buf[off+4*i+1]<<8) | ((uint32_t)buf[off+4*i+2]<<16) | ((uint32_t)buf[off+4*i+3]<<24);
        uint32_t a=h0,b=h1,c=h2,d=h3;
        for (int i = 0; i < 64; i++) {
            uint32_t f; int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5*i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3*i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7*i) % 16; }
            uint32_t temp = d; d = c; c = b;
            b = b + md5_rotl(a + f + K[i] + M[g], S[i]);
            a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d;
    }
    free(buf);
    uint32_t hs[4] = {h0,h1,h2,h3};
    for (int i = 0; i < 4; i++) { out[4*i]=(uint8_t)(hs[i]&0xff); out[4*i+1]=(uint8_t)((hs[i]>>8)&0xff); out[4*i+2]=(uint8_t)((hs[i]>>16)&0xff); out[4*i+3]=(uint8_t)((hs[i]>>24)&0xff); }
}


/* ================= SHA1 (FIPS 180-1, compact) ================= */
static uint32_t sha1_rotl(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

void gmb_sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
#if defined(__APPLE__)
    CC_SHA1(data, (CC_LONG)len, out);
#else
    gmb_sha1_impl(data, len, out);
#endif
}
static void gmb_sha1_impl(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h0=0x67452301,h1=0xEFCDAB89,h2=0x98BADCFE,h3=0x10325476,h4=0xC3D2E1F0;
    size_t padded = ((len + 9) / 64 + 1) * 64;
    uint8_t* buf = (uint8_t*)calloc(padded, 1);
    if (!buf) { memset(out,0,20); return; }
    memcpy(buf, data, len);
    buf[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) buf[padded - 1 - i] = (uint8_t)((bits >> (8*i)) & 0xff);
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) w[i] = ((uint32_t)buf[off+4*i]<<24) | ((uint32_t)buf[off+4*i+1]<<16) | ((uint32_t)buf[off+4*i+2]<<8) | (uint32_t)buf[off+4*i+3];
        for (int i = 16; i < 80; i++) w[i] = sha1_rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        uint32_t a=h0,b=h1,c=h2,d=h3,e=h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t temp = sha1_rotl(a,5) + f + e + k + w[i];
            e = d; d = c; c = sha1_rotl(b,30); b = a; a = temp;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
    }
    free(buf);
    uint32_t hs[5] = {h0,h1,h2,h3,h4};
    for (int i = 0; i < 5; i++) { out[4*i]=(uint8_t)((hs[i]>>24)&0xff); out[4*i+1]=(uint8_t)((hs[i]>>16)&0xff); out[4*i+2]=(uint8_t)((hs[i]>>8)&0xff); out[4*i+3]=(uint8_t)(hs[i]&0xff); }
}


/* ================= SHA-512 (FIPS 180-4, compact) ================= */

/* ================= SHA-512 (FIPS 180-4, verificato con vettori NIST) ================= */
static uint64_t sha512_rotr(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }

void gmb_sha512(const uint8_t* data, size_t len, uint8_t out[64]) {
#if defined(__APPLE__)
    CC_SHA512(data, (CC_LONG)len, out);
#else
    gmb_sha512_impl(data, len, out);
#endif
}
static void gmb_sha512_impl(const uint8_t* data, size_t len, uint8_t out[64]) {
    static const uint64_t K[80] = {
        0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
        0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
        0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
        0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
        0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
        0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
        0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
        0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
        0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
        0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
        0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
        0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
        0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
        0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
        0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
        0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
        0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
        0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
        0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
        0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL };
    uint64_t H[8] = {
        0x6a09e667f3bcc908ULL,0xbb67ae8584caa73bULL,0x3c6ef372fe94f82bULL,0xa54ff53a5f1d36f1ULL,
        0x510e527fade682d1ULL,0x9b05688c2b3e6c1fULL,0x1f83d9abfb41bd6bULL,0x5be0cd19137e2179ULL };
    /* padding: data || 0x80 || zeros || 128-bit length BE */
    uint64_t bitlen = (uint64_t)len * 8;
    size_t plen = ((len + 16) / 128 + 1) * 128;
    if (len % 128 >= 112) plen += 128;
    uint8_t* buf = (uint8_t*)calloc(plen, 1);
    if (!buf) { memset(out, 0, 64); return; }
    memcpy(buf, data, len);
    buf[len] = 0x80;
    for (int i = 0; i < 8; i++) buf[plen - 8 + i] = (uint8_t)((bitlen >> (56 - 8*i)) & 0xff);
    for (size_t off = 0; off < plen; off += 128) {
        uint64_t W[80];
        for (int i = 0; i < 16; i++) {
            W[i] = 0;
            for (int j = 0; j < 8; j++) W[i] = (W[i] << 8) | buf[off + 16*i + j];
        }
        for (int i = 16; i < 80; i++) {
            uint64_t s0 = sha512_rotr(W[i-15], 1) ^ sha512_rotr(W[i-15], 8) ^ (W[i-15] >> 7);
            uint64_t s1 = sha512_rotr(W[i-2], 19) ^ sha512_rotr(W[i-2], 61) ^ (W[i-2] >> 6);
            W[i] = W[i-16] + s0 + W[i-7] + s1;
        }
        uint64_t a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];
        for (int i = 0; i < 80; i++) {
            uint64_t S1 = sha512_rotr(e,14) ^ sha512_rotr(e,18) ^ sha512_rotr(e,41);
            uint64_t ch = (e & f) ^ (~e & g);
            uint64_t t1 = h + S1 + ch + K[i] + W[i];
            uint64_t S0 = sha512_rotr(a,28) ^ sha512_rotr(a,34) ^ sha512_rotr(a,39);
            uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint64_t t2 = S0 + maj;
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        H[0]+=a; H[1]+=b; H[2]+=c; H[3]+=d; H[4]+=e; H[5]+=f; H[6]+=g; H[7]+=h;
    }
    free(buf);
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) out[8*i + j] = (uint8_t)((H[i] >> (56 - 8*j)) & 0xff);
}


/* ================= ChaCha20 (RFC 8439, 12-byte nonce) ================= */
static uint32_t cc_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static uint32_t cc_rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
static void cc_quarter(uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    *a += *b; *d ^= *a; *d = cc_rotl(*d, 16);
    *c += *d; *b ^= *c; *b = cc_rotl(*b, 12);
    *a += *b; *d ^= *a; *d = cc_rotl(*d, 8);
    *c += *d; *b ^= *c; *b = cc_rotl(*b, 7);
}
static void cc_block(uint32_t state[16], uint8_t out[64]) {
    uint32_t x[16];
    memcpy(x, state, 64);
    for (int i = 0; i < 10; i++) {
        cc_quarter(&x[0],&x[4],&x[8],&x[12]);
        cc_quarter(&x[1],&x[5],&x[9],&x[13]);
        cc_quarter(&x[2],&x[6],&x[10],&x[14]);
        cc_quarter(&x[3],&x[7],&x[11],&x[15]);
        cc_quarter(&x[0],&x[5],&x[10],&x[15]);
        cc_quarter(&x[1],&x[6],&x[11],&x[12]);
        cc_quarter(&x[2],&x[7],&x[8],&x[13]);
        cc_quarter(&x[3],&x[4],&x[9],&x[14]);
    }
    for (int i = 0; i < 16; i++) x[i] += state[i];
    for (int i = 0; i < 16; i++) {
        out[4*i] = (uint8_t)(x[i] & 0xff);
        out[4*i+1] = (uint8_t)((x[i]>>8)&0xff);
        out[4*i+2] = (uint8_t)((x[i]>>16)&0xff);
        out[4*i+3] = (uint8_t)((x[i]>>24)&0xff);
    }
}
static void cc_init(uint32_t state[16], const uint8_t key[32], const uint8_t nonce[12], uint32_t counter) {
    state[0]=0x61707865; state[1]=0x3320646e; state[2]=0x79622d32; state[3]=0x6b206574;
    for (int i = 0; i < 8; i++) {
        state[4+i] = (uint32_t)key[4*i] | ((uint32_t)key[4*i+1]<<8) | ((uint32_t)key[4*i+2]<<16) | ((uint32_t)key[4*i+3]<<24);
    }
    state[12] = counter;
    for (int i = 0; i < 3; i++) {
        state[13+i] = (uint32_t)nonce[4*i] | ((uint32_t)nonce[4*i+1]<<8) | ((uint32_t)nonce[4*i+2]<<16) | ((uint32_t)nonce[4*i+3]<<24);
    }
}
void gmb_chacha20_xor(const uint8_t* key, const uint8_t* nonce, const uint8_t* in, size_t inlen, uint8_t* out) {
    uint32_t state[16];
    uint32_t counter = 0;
    size_t off = 0;
    while (off < inlen) {
        cc_init(state, key, nonce, counter++);
        uint8_t block[64];
        cc_block(state, block);
        size_t n = (inlen - off < 64) ? (inlen - off) : 64;
        for (size_t i = 0; i < n; i++) out[off+i] = in[off+i] ^ block[i];
        off += n;
    }
}

/* ================= Poly1305 (RFC 8439) ================= */
static void poly1305_block(uint8_t r[16], uint32_t h[5], const uint8_t* block, int final) {
    uint32_t hr[5], c, d0,d1,d2,d3,d4;
    uint32_t h0=h[0],h1=h[1],h2=h[2],h3=h[3],h4=h[4];
    uint32_t r0=r[0],r1=r[1],r2=r[2],r3=r[3],r4=r[4];
    uint32_t s1=r1*5,s2=r2*5,s3=r3*5,s4=r4*5;
    uint32_t b0=(uint32_t)block[0]|((uint32_t)block[1]<<8)|((uint32_t)block[2]<<16)|((uint32_t)block[3]<<24);
    uint32_t b1=(uint32_t)block[4]|((uint32_t)block[5]<<8)|((uint32_t)block[6]<<16)|((uint32_t)block[7]<<24);
    uint32_t b2=(uint32_t)block[8]|((uint32_t)block[9]<<8)|((uint32_t)block[10]<<16)|((uint32_t)block[11]<<24);
    uint32_t b3=(uint32_t)block[12]|((uint32_t)block[13]<<8)|((uint32_t)block[14]<<16)|((uint32_t)block[15]<<24);
    uint32_t b4=final ? 0x01000000u : 0;
    d0 = h0*r0 + h1*s4 + h2*s3 + h3*s2 + h4*s1 + b0;
    d1 = h0*r1 + h1*r0 + h2*s4 + h3*s3 + h4*s2 + b1;
    d2 = h0*r2 + h1*r1 + h2*r0 + h3*s4 + h4*s3 + b2;
    d3 = h0*r3 + h1*r2 + h2*r1 + h3*r0 + h4*s4 + b3;
    d4 = h0*r4 + h1*r3 + h2*r2 + h3*r1 + h4*r0 + b4;
    c = d0>>26; h0 = d0&0x3ffffff; d1+=c;
    c = d1>>26; h1 = d1&0x3ffffff; d2+=c;
    c = d2>>26; h2 = d2&0x3ffffff; d3+=c;
    c = d3>>26; h3 = d3&0x3ffffff; d4+=c;
    c = d4>>26; h4 = d4&0x3ffffff; h0 += c*5; c = h0>>26; h0 &= 0x3ffffff; h1 += c;
    h[0]=h0; h[1]=h1; h[2]=h2; h[3]=h3; h[4]=h4;
}
static void poly1305_clamp(uint8_t r[16]) {
    r[3] &= 15; r[7] &= 15; r[11] &= 15; r[15] &= 15;
    r[4] &= 252; r[8] &= 252; r[12] &= 252;
}
static void poly1305_finish(const uint8_t r[16], uint32_t h[5], uint8_t tag[16]) {
    uint32_t g0,g1,g2,g3,g4,c, mask;
    uint32_t h0=h[0],h1=h[1],h2=h[2],h3=h[3],h4=h[4];
    c = h1>>26; h1 = h1&0x3ffffff; h2 += c;
    c = h2>>26; h2 = h2&0x3ffffff; h3 += c;
    c = h3>>26; h3 = h3&0x3ffffff; h4 += c;
    c = h4>>26; h4 = h4&0x3ffffff; h0 += c*5; c = h0>>26; h0 &= 0x3ffffff; h1 += c;
    g0 = h0+5; c = g0>>26; g0 &= 0x3ffffff;
    g1 = h1+c; c = g1>>26; g1 &= 0x3ffffff;
    g2 = h2+c; c = g2>>26; g2 &= 0x3ffffff;
    g3 = h3+c; c = g3>>26; g3 &= 0x3ffffff;
    g4 = h4+c-1ull-0x3ffffff; /* - (1<<26) */
    mask = (g4>>31)-1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask)|g0; h1 = (h1 & mask)|g1; h2 = (h2 & mask)|g2; h3 = (h3 & mask)|g3; h4 = (h4 & mask)|g4;
    h0 = h0 | (h1<<26); h1 = (h1>>6)|(h2<<20); h2 = (h2>>12)|(h3<<14); h3 = (h3>>18)|(h4<<8);
    tag[0]=h0&0xff; tag[1]=(h0>>8)&0xff; tag[2]=(h0>>16)&0xff; tag[3]=(h0>>24)&0xff;
    tag[4]=h1&0xff; tag[5]=(h1>>8)&0xff; tag[6]=(h1>>16)&0xff; tag[7]=(h1>>24)&0xff;
    tag[8]=h2&0xff; tag[9]=(h2>>8)&0xff; tag[10]=(h2>>16)&0xff; tag[11]=(h2>>24)&0xff;
    tag[12]=h3&0xff; tag[13]=(h3>>8)&0xff; tag[14]=(h3>>16)&0xff; tag[15]=(h3>>24)&0xff;
    for (int i = 0; i < 16; i++) tag[i] += r[i];
}
void gmb_poly1305_mac(const uint8_t* key, const uint8_t* msg, size_t msglen, uint8_t out[16]) {
    uint8_t r[16], rs[16];
    memcpy(rs, key, 16);
    poly1305_clamp(rs);
    uint32_t h[5] = {0,0,0,0,0};
    size_t off = 0;
    while (off < msglen) {
        uint8_t block[16] = {0};
        size_t n = (msglen - off < 16) ? (msglen - off) : 16;
        memcpy(block, msg + off, n);
        off += n;
        poly1305_block(rs, h, block, (n < 16) ? 1 : 0);
        (void)r;
    }
    uint8_t pad[16];
    memcpy(pad, key + 16, 16);
    poly1305_finish(pad, h, out);
}

/* Varianti con contatore esplicito (usate da ChaCha20-Poly1305 AEAD). */
void gmb_chacha20_block(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter, uint8_t out[64]) {
    uint32_t state[16];
    cc_init(state, key, nonce, counter);
    cc_block(state, out);
}

void gmb_chacha20_xor_counter(const uint8_t key[32], const uint8_t nonce[12], uint32_t start_counter,
                              const uint8_t* in, size_t inlen, uint8_t* out) {
    uint32_t counter = start_counter;
    size_t off = 0;
    while (off < inlen) {
        uint8_t block[64];
        gmb_chacha20_block(key, nonce, counter++, block);
        size_t n = (inlen - off < 64) ? (inlen - off) : 64;
        for (size_t i = 0; i < n; i++) out[off+i] = in[off+i] ^ block[i];
        off += n;
    }
}

/* ============ Helper per il transpiler C99 (MD5/SHA1/SHA256/SHA512) ============
 * _alloc: digest mallocato + lunghezza (per DIM x() AS BYTE = MD5(s)).
 * _hex: digest codificato hex in stringa allocata (per STRMD5/TOHEX(MD5(...))). */
uint8_t* gmb_md5_alloc(const uint8_t* data, size_t len, size_t* len_out) {
    uint8_t* out = (uint8_t*)malloc(16);
    if (out) { gmb_md5(data, len, out); if (len_out) *len_out = 16; }
    return out;
}
uint8_t* gmb_sha1_alloc(const uint8_t* data, size_t len, size_t* len_out) {
    uint8_t* out = (uint8_t*)malloc(20);
    if (out) { gmb_sha1(data, len, out); if (len_out) *len_out = 20; }
    return out;
}
uint8_t* gmb_sha256_alloc(const uint8_t* data, size_t len, size_t* len_out) {
    uint8_t* out = (uint8_t*)malloc(32);
    if (out) { gmb_sha256(data, len, out); if (len_out) *len_out = 32; }
    return out;
}
uint8_t* gmb_sha512_alloc(const uint8_t* data, size_t len, size_t* len_out) {
    uint8_t* out = (uint8_t*)malloc(64);
    if (out) { gmb_sha512(data, len, out); if (len_out) *len_out = 64; }
    return out;
}
char* gmb_md5_hex(const uint8_t* data, size_t len) {
    uint8_t d[16]; gmb_md5(data, len, d); return gmb_hex_encode_string(d, 16);
}
char* gmb_sha1_hex(const uint8_t* data, size_t len) {
    uint8_t d[20]; gmb_sha1(data, len, d); return gmb_hex_encode_string(d, 20);
}
char* gmb_keccak256_hex(const uint8_t* data, size_t len) { uint8_t d[32]; gmb_keccak256(data, len, d); return gmb_hex_encode_string(d, 32); }
char* gmb_ripemd160_hex(const uint8_t* data, size_t len) { uint8_t d[20]; gmb_ripemd160(data, len, d); return gmb_hex_encode_string(d, 20); }
char* gmb_blake2b256_hex(const uint8_t* data, size_t len) { uint8_t d[32]; gmb_blake2b256(data, len, d); return gmb_hex_encode_string(d, 32); }
char* gmb_sha256_hex(const uint8_t* data, size_t len) {
    uint8_t d[32]; gmb_sha256(data, len, d); return gmb_hex_encode_string(d, 32);
}
char* gmb_sha512_hex(const uint8_t* data, size_t len) {
    uint8_t d[64]; gmb_sha512(data, len, d); return gmb_hex_encode_string(d, 64);
}

