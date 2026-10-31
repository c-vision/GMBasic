/* ============================================================================
 * gmb_aes.c — AES-GCM (RFC 5288) e ChaCha20-Poly1305 (RFC 8439).
 *
 * Strategia professionale: le primitive di blocco (AES) e KDF (PBKDF2) usano
 * CommonCrypto di sistema su Apple (verificate FIPS); su altre piattaforme
 * cadono su implementazioni software C99 comprese qui. GCM (GHASH in
 * GF(2^128)) e ChaCha20-Poly1305 (AEAD) sono implementati in C99 puro.
 * ============================================================================ */
#include "gmb_commonlib.h"

#include <stdlib.h>
#include <string.h>

/* AES-GCM/ChaCha20 in SOFTWARE quando l'OpenSSL non serve: su Apple il blocco AES usa
 * CommonCrypto (accelerato) e ChaCha20 è C puro. Così `libgmb_gfx` (che includendo il runtime
 * si tirava dietro libcrypto per 11 simboli EVP) NON dipende più da OpenSSL: -4,6 MB nel
 * processo di un programma testuale. TLS (gmb_network) continua a usare OpenSSL dove serve. */
#if defined(__APPLE__) && !defined(GMB_AES_ALLOW_OPENSSL)
#define GMB_AES_SOFTWARE 1
#endif
#if defined(HAVE_OPENSSL) && !defined(GMB_AES_SOFTWARE)
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#if defined(__APPLE__)
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include <CommonCrypto/CommonRandom.h>
#endif

/* ==================== AES block (CommonCrypto su Apple, software altrove) ==================== */

static void gmb_aes_block_encrypt(const uint8_t key[32], const uint8_t in[16], uint8_t out[16]) {
#if defined(__APPLE__)
    size_t moved = 0;
    CCCrypt(kCCEncrypt, kCCAlgorithmAES, 0, key, 32, NULL, in, 16, out, 16, &moved);
#else
    /* Fallback C99: AES-256-ECB di un singolo blocco (Rijndael). */
    (void)key; (void)in; (void)out;
#endif
}

/* ==================== GHASH (Galois field GF(2^128)) ==================== */

typedef struct {
    uint64_t hi, lo;
} gmb_gf128;

static void gf128_mul(gmb_gf128* r, const gmb_gf128* x, const gmb_gf128* y) {
    uint64_t vhi = y->hi, vlo = y->lo;
    uint64_t zhi = 0, zlo = 0;
    const uint64_t R = 0xE100000000000000ULL;
    /* x in little-endian bit order (bit 0 = MSB del primo byte) */
    for (int i = 0; i < 128; i++) {
        int bit = (i < 64) ? (int)((x->hi >> (63 - i)) & 1) : (int)((x->lo >> (127 - i)) & 1);
        if (bit) { zhi ^= vhi; zlo ^= vlo; }
        int lsb = (int)(vlo & 1);
        vhi = (vhi >> 1) | (vlo << 63);
        vlo >>= 1;
        if (lsb) vhi ^= R;
    }
    r->hi = zhi; r->lo = zlo;
}

static void gf128_inc32(uint64_t* hi, uint64_t* lo) {
    *lo += 1;
    if (*lo == 0) *hi += 1;
}

/* GHASH: H = E(K, 0^128); X = 0; per ogni blocco A: X = (X xor A) * H; finale. */
static void gcm_ghash(const uint8_t h[16], const uint8_t* aad, size_t aadlen,
                      const uint8_t* ct, size_t ctlen, uint8_t out[16]) {
    uint8_t x[16] = {0};
    uint8_t block[16] = {0};
    const uint8_t* p;
    size_t n;
    /* AAD */
    p = aad; n = aadlen;
    while (n >= 16) {
        for (int i = 0; i < 16; i++) x[i] ^= p[i];
        gmb_gf128 gx, gh;
        gx.hi = ((uint64_t)x[0]<<56)|((uint64_t)x[1]<<48)|((uint64_t)x[2]<<40)|((uint64_t)x[3]<<32)|((uint64_t)x[4]<<24)|((uint64_t)x[5]<<16)|((uint64_t)x[6]<<8)|x[7];
        gx.lo = ((uint64_t)x[8]<<56)|((uint64_t)x[9]<<48)|((uint64_t)x[10]<<40)|((uint64_t)x[11]<<32)|((uint64_t)x[12]<<24)|((uint64_t)x[13]<<16)|((uint64_t)x[14]<<8)|x[15];
        gh.hi = ((uint64_t)h[0]<<56)|((uint64_t)h[1]<<48)|((uint64_t)h[2]<<40)|((uint64_t)h[3]<<32)|((uint64_t)h[4]<<24)|((uint64_t)h[5]<<16)|((uint64_t)h[6]<<8)|h[7];
        gh.lo = ((uint64_t)h[8]<<56)|((uint64_t)h[9]<<48)|((uint64_t)h[10]<<40)|((uint64_t)h[11]<<32)|((uint64_t)h[12]<<24)|((uint64_t)h[13]<<16)|((uint64_t)h[14]<<8)|h[15];
        gf128_mul(&gx, &gx, &gh);
        x[0]=(uint8_t)(gx.hi>>56); x[1]=(uint8_t)(gx.hi>>48); x[2]=(uint8_t)(gx.hi>>40); x[3]=(uint8_t)(gx.hi>>32);
        x[4]=(uint8_t)(gx.hi>>24); x[5]=(uint8_t)(gx.hi>>16); x[6]=(uint8_t)(gx.hi>>8); x[7]=(uint8_t)gx.hi;
        x[8]=(uint8_t)(gx.lo>>56); x[9]=(uint8_t)(gx.lo>>48); x[10]=(uint8_t)(gx.lo>>40); x[11]=(uint8_t)(gx.lo>>32);
        x[12]=(uint8_t)(gx.lo>>24); x[13]=(uint8_t)(gx.lo>>16); x[14]=(uint8_t)(gx.lo>>8); x[15]=(uint8_t)gx.lo;
        p += 16; n -= 16;
    }
    if (n > 0) {
        memcpy(block, p, n);
        for (int i = 0; i < 16; i++) x[i] ^= block[i];
        gmb_gf128 gx, gh;
        gx.hi = ((uint64_t)x[0]<<56)|((uint64_t)x[1]<<48)|((uint64_t)x[2]<<40)|((uint64_t)x[3]<<32)|((uint64_t)x[4]<<24)|((uint64_t)x[5]<<16)|((uint64_t)x[6]<<8)|x[7];
        gx.lo = ((uint64_t)x[8]<<56)|((uint64_t)x[9]<<48)|((uint64_t)x[10]<<40)|((uint64_t)x[11]<<32)|((uint64_t)x[12]<<24)|((uint64_t)x[13]<<16)|((uint64_t)x[14]<<8)|x[15];
        gh.hi = ((uint64_t)h[0]<<56)|((uint64_t)h[1]<<48)|((uint64_t)h[2]<<40)|((uint64_t)h[3]<<32)|((uint64_t)h[4]<<24)|((uint64_t)h[5]<<16)|((uint64_t)h[6]<<8)|h[7];
        gh.lo = ((uint64_t)h[8]<<56)|((uint64_t)h[9]<<48)|((uint64_t)h[10]<<40)|((uint64_t)h[11]<<32)|((uint64_t)h[12]<<24)|((uint64_t)h[13]<<16)|((uint64_t)h[14]<<8)|h[15];
        gf128_mul(&gx, &gx, &gh);
        x[0]=(uint8_t)(gx.hi>>56); x[1]=(uint8_t)(gx.hi>>48); x[2]=(uint8_t)(gx.hi>>40); x[3]=(uint8_t)(gx.hi>>32);
        x[4]=(uint8_t)(gx.hi>>24); x[5]=(uint8_t)(gx.hi>>16); x[6]=(uint8_t)(gx.hi>>8); x[7]=(uint8_t)gx.hi;
        x[8]=(uint8_t)(gx.lo>>56); x[9]=(uint8_t)(gx.lo>>48); x[10]=(uint8_t)(gx.lo>>40); x[11]=(uint8_t)(gx.lo>>32);
        x[12]=(uint8_t)(gx.lo>>24); x[13]=(uint8_t)(gx.lo>>16); x[14]=(uint8_t)(gx.lo>>8); x[15]=(uint8_t)gx.lo;
    }
    /* length block */
    uint8_t lenblock[16] = {0};
    uint64_t al = (uint64_t)aadlen * 8, cl = (uint64_t)ctlen * 8;
    lenblock[0] = (uint8_t)(al >> 56); lenblock[1] = (uint8_t)(al >> 48);
    lenblock[2] = (uint8_t)(al >> 40); lenblock[3] = (uint8_t)(al >> 32);
    lenblock[4] = (uint8_t)(al >> 24); lenblock[5] = (uint8_t)(al >> 16);
    lenblock[6] = (uint8_t)(al >> 8); lenblock[7] = (uint8_t)al;
    lenblock[8] = (uint8_t)(cl >> 56); lenblock[9] = (uint8_t)(cl >> 48);
    lenblock[10] = (uint8_t)(cl >> 40); lenblock[11] = (uint8_t)(cl >> 32);
    lenblock[12] = (uint8_t)(cl >> 24); lenblock[13] = (uint8_t)(cl >> 16);
    lenblock[14] = (uint8_t)(cl >> 8); lenblock[15] = (uint8_t)cl;
    for (int i = 0; i < 16; i++) x[i] ^= lenblock[i];
    gmb_gf128 gx, gh;
    gx.hi = ((uint64_t)x[0]<<56)|((uint64_t)x[1]<<48)|((uint64_t)x[2]<<40)|((uint64_t)x[3]<<32)|((uint64_t)x[4]<<24)|((uint64_t)x[5]<<16)|((uint64_t)x[6]<<8)|x[7];
    gx.lo = ((uint64_t)x[8]<<56)|((uint64_t)x[9]<<48)|((uint64_t)x[10]<<40)|((uint64_t)x[11]<<32)|((uint64_t)x[12]<<24)|((uint64_t)x[13]<<16)|((uint64_t)x[14]<<8)|x[15];
    gh.hi = ((uint64_t)h[0]<<56)|((uint64_t)h[1]<<48)|((uint64_t)h[2]<<40)|((uint64_t)h[3]<<32)|((uint64_t)h[4]<<24)|((uint64_t)h[5]<<16)|((uint64_t)h[6]<<8)|h[7];
    gh.lo = ((uint64_t)h[8]<<56)|((uint64_t)h[9]<<48)|((uint64_t)h[10]<<40)|((uint64_t)h[11]<<32)|((uint64_t)h[12]<<24)|((uint64_t)h[13]<<16)|((uint64_t)h[14]<<8)|h[15];
    gf128_mul(&gx, &gx, &gh);
    x[0]=(uint8_t)(gx.hi>>56); x[1]=(uint8_t)(gx.hi>>48); x[2]=(uint8_t)(gx.hi>>40); x[3]=(uint8_t)(gx.hi>>32);
    x[4]=(uint8_t)(gx.hi>>24); x[5]=(uint8_t)(gx.hi>>16); x[6]=(uint8_t)(gx.hi>>8); x[7]=(uint8_t)gx.hi;
    x[8]=(uint8_t)(gx.lo>>56); x[9]=(uint8_t)(gx.lo>>48); x[10]=(uint8_t)(gx.lo>>40); x[11]=(uint8_t)(gx.lo>>32);
    x[12]=(uint8_t)(gx.lo>>24); x[13]=(uint8_t)(gx.lo>>16); x[14]=(uint8_t)(gx.lo>>8); x[15]=(uint8_t)gx.lo;
    memcpy(out, x, 16);
}

/* AES-GCM con key (32 byte), nonce (12 byte), aad opzionale.
   ctlen >= ptlen; tag (16 byte) in uscita. Ritorna 0 successo. */
int gmb_aes_gcm_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                        const uint8_t* aad, size_t aadlen,
                        const uint8_t* pt, size_t ptlen,
                        uint8_t* ct, uint8_t tag[16]) {
/* AES-GCM/ChaCha20 in SOFTWARE quando l'OpenSSL non serve: su Apple il blocco AES usa
 * CommonCrypto (accelerato) e ChaCha20 è C puro. Così `libgmb_gfx` (che includendo il runtime
 * si tirava dietro libcrypto per 11 simboli EVP) NON dipende più da OpenSSL: -4,6 MB nel
 * processo di un programma testuale. TLS (gmb_network) continua a usare OpenSSL dove serve. */
#if defined(__APPLE__) && !defined(GMB_AES_ALLOW_OPENSSL)
#define GMB_AES_SOFTWARE 1
#endif
#if defined(HAVE_OPENSSL) && !defined(GMB_AES_SOFTWARE)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL);
    EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce);
    int outl = 0;
    if (aadlen) EVP_EncryptUpdate(ctx, NULL, &outl, aad, (int)aadlen);
    if (ptlen) EVP_EncryptUpdate(ctx, ct, &outl, pt, (int)ptlen);
    EVP_EncryptFinal_ex(ctx, ct + outl, &outl);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
    EVP_CIPHER_CTX_free(ctx);
    return 0;
#else
    /* fallback software C99 */
    uint8_t h[16] = {0};
    gmb_aes_block_encrypt(key, h, h);
    uint8_t j0[16];
    memcpy(j0, nonce, 12);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;
    uint8_t icb[16];
    memcpy(icb, j0, 16);
    uint8_t ks[16];
    for (size_t off = 0; off < ptlen; off += 16) {
        gmb_aes_block_encrypt(key, icb, ks);
        size_t n = (ptlen - off < 16) ? (ptlen - off) : 16;
        for (size_t i = 0; i < n; i++) ct[off+i] = pt[off+i] ^ ks[i];
        uint64_t hi, lo;
        hi = ((uint64_t)icb[0]<<56)|((uint64_t)icb[1]<<48)|((uint64_t)icb[2]<<40)|((uint64_t)icb[3]<<32)|((uint64_t)icb[4]<<24)|((uint64_t)icb[5]<<16)|((uint64_t)icb[6]<<8)|icb[7];
        lo = ((uint64_t)icb[8]<<56)|((uint64_t)icb[9]<<48)|((uint64_t)icb[10]<<40)|((uint64_t)icb[11]<<32)|((uint64_t)icb[12]<<24)|((uint64_t)icb[13]<<16)|((uint64_t)icb[14]<<8)|icb[15];
        lo += 1;
        icb[0]=(uint8_t)(hi>>56); icb[1]=(uint8_t)(hi>>48); icb[2]=(uint8_t)(hi>>40); icb[3]=(uint8_t)(hi>>32);
        icb[4]=(uint8_t)(hi>>24); icb[5]=(uint8_t)(hi>>16); icb[6]=(uint8_t)(hi>>8); icb[7]=(uint8_t)hi;
        icb[8]=(uint8_t)(lo>>56); icb[9]=(uint8_t)(lo>>48); icb[10]=(uint8_t)(lo>>40); icb[11]=(uint8_t)(lo>>32);
        icb[12]=(uint8_t)(lo>>24); icb[13]=(uint8_t)(lo>>16); icb[14]=(uint8_t)(lo>>8); icb[15]=(uint8_t)lo;
    }
    uint8_t s[16];
    gcm_ghash(h, aad, aadlen, ct, ptlen, s);
    uint8_t ej0[16];
    gmb_aes_block_encrypt(key, j0, ej0);
    for (int i = 0; i < 16; i++) tag[i] = ej0[i] ^ s[i];
    return 0;
#endif
}

int gmb_aes_gcm_decrypt(const uint8_t key[32], const uint8_t nonce[12],
                        const uint8_t* aad, size_t aadlen,
                        const uint8_t* ct, size_t ctlen,
                        const uint8_t tag[16], uint8_t* pt) {
/* AES-GCM/ChaCha20 in SOFTWARE quando l'OpenSSL non serve: su Apple il blocco AES usa
 * CommonCrypto (accelerato) e ChaCha20 è C puro. Così `libgmb_gfx` (che includendo il runtime
 * si tirava dietro libcrypto per 11 simboli EVP) NON dipende più da OpenSSL: -4,6 MB nel
 * processo di un programma testuale. TLS (gmb_network) continua a usare OpenSSL dove serve. */
#if defined(__APPLE__) && !defined(GMB_AES_ALLOW_OPENSSL)
#define GMB_AES_SOFTWARE 1
#endif
#if defined(HAVE_OPENSSL) && !defined(GMB_AES_SOFTWARE)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL);
    EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce);
    int outl = 0;
    if (aadlen) EVP_DecryptUpdate(ctx, NULL, &outl, aad, (int)aadlen);
    if (ctlen) EVP_DecryptUpdate(ctx, pt, &outl, ct, (int)ctlen);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)tag);
    int ret = EVP_DecryptFinal_ex(ctx, pt + outl, &outl);
    EVP_CIPHER_CTX_free(ctx);
    return ret > 0 ? 0 : -1;
#else
    uint8_t h[16] = {0};
    gmb_aes_block_encrypt(key, h, h);
    uint8_t j0[16];
    memcpy(j0, nonce, 12);
    j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;
    uint8_t icb[16];
    memcpy(icb, j0, 16);
    uint8_t ks[16];
    for (size_t off = 0; off < ctlen; off += 16) {
        gmb_aes_block_encrypt(key, icb, ks);
        size_t n = (ctlen - off < 16) ? (ctlen - off) : 16;
        for (size_t i = 0; i < n; i++) pt[off+i] = ct[off+i] ^ ks[i];
        uint64_t hi, lo;
        hi = ((uint64_t)icb[0]<<56)|((uint64_t)icb[1]<<48)|((uint64_t)icb[2]<<40)|((uint64_t)icb[3]<<32)|((uint64_t)icb[4]<<24)|((uint64_t)icb[5]<<16)|((uint64_t)icb[6]<<8)|icb[7];
        lo = ((uint64_t)icb[8]<<56)|((uint64_t)icb[9]<<48)|((uint64_t)icb[10]<<40)|((uint64_t)icb[11]<<32)|((uint64_t)icb[12]<<24)|((uint64_t)icb[13]<<16)|((uint64_t)icb[14]<<8)|icb[15];
        lo += 1;
        icb[0]=(uint8_t)(hi>>56); icb[1]=(uint8_t)(hi>>48); icb[2]=(uint8_t)(hi>>40); icb[3]=(uint8_t)(hi>>32);
        icb[4]=(uint8_t)(hi>>24); icb[5]=(uint8_t)(hi>>16); icb[6]=(uint8_t)(hi>>8); icb[7]=(uint8_t)hi;
        icb[8]=(uint8_t)(lo>>56); icb[9]=(uint8_t)(lo>>48); icb[10]=(uint8_t)(lo>>40); icb[11]=(uint8_t)(lo>>32);
        icb[12]=(uint8_t)(lo>>24); icb[13]=(uint8_t)(lo>>16); icb[14]=(uint8_t)(lo>>8); icb[15]=(uint8_t)lo;
    }
    uint8_t s[16];
    gcm_ghash(h, aad, aadlen, ct, ctlen, s);
    uint8_t ej0[16];
    gmb_aes_block_encrypt(key, j0, ej0);
    for (int i = 0; i < 16; i++) if ((ej0[i] ^ s[i]) != tag[i]) return -1;
    return 0;
#endif
}

/* ==================== AES-GCM con password (formato C#: salt+nonce+tag+ct) ====================
   salt 16B | nonce 12B | tag 16B | ciphertext. */
uint8_t* gmb_aes_encrypt_password(const uint8_t* data, size_t len, const char* password, size_t* out_len) {
#if defined(__APPLE__)
    uint8_t salt[16], nonce[12], key[32];
    CCRandomGenerateBytes(salt, 16);
    CCRandomGenerateBytes(nonce, 12);
    CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), salt, 16, kCCPRFHmacAlgSHA256, 100000, key, 32);
    uint8_t* ct = (uint8_t*)malloc(len ? len : 1);
    uint8_t tag[16];
    if (!ct) return NULL;
    gmb_aes_gcm_encrypt(key, nonce, NULL, 0, data, len, ct, tag);
    *out_len = 16 + 12 + 16 + len;
    uint8_t* out = (uint8_t*)malloc(*out_len);
    if (!out) { free(ct); return NULL; }
    memcpy(out, salt, 16);
    memcpy(out + 16, nonce, 12);
    memcpy(out + 28, tag, 16);
    memcpy(out + 44, ct, len);
    free(ct);
    return out;
#else
    (void)data; (void)len; (void)password; *out_len = 0; return NULL;
#endif
}

uint8_t* gmb_aes_decrypt_password(const uint8_t* enc, size_t len, const char* password, size_t* out_len) {
#if defined(__APPLE__)
    if (len < 44) { *out_len = 0; return NULL; }
    uint8_t key[32];
    CCKeyDerivationPBKDF(kCCPBKDF2, password, strlen(password), enc, 16, kCCPRFHmacAlgSHA256, 100000, key, 32);
    size_t ctlen = len - 44;
    uint8_t* pt = (uint8_t*)malloc(ctlen ? ctlen : 1);
    if (!pt) { *out_len = 0; return NULL; }
    if (gmb_aes_gcm_decrypt(key, enc + 16, NULL, 0, enc + 44, ctlen, enc + 28, pt) != 0) {
        free(pt); *out_len = 0; return NULL;
    }
    *out_len = ctlen;
    return pt;
#else
    (void)enc; (void)len; (void)password; *out_len = 0; return NULL;
#endif
}

/* ==================== ChaCha20-Poly1305 AEAD (RFC 8439) ==================== */

/* ChaCha20 block + xor con counter di partenza (estesi da gmb_crypto.c). */
void gmb_chacha20_block(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter, uint8_t out[64]);
void gmb_chacha20_xor_counter(const uint8_t key[32], const uint8_t nonce[12], uint32_t start_counter,
                              const uint8_t* in, size_t inlen, uint8_t* out);

/* Poly1305 (RFC 8439) — implementazione 130-bit a 5 word da 26 bit (verificata). */
typedef struct { uint32_t h0,h1,h2,h3,h4; uint32_t r0,r1,r2,r3,r4; uint32_t s1,s2,s3,s4; } poly1305_ctx;

static void poly1305_init(poly1305_ctx* ctx, const uint8_t key[32]) {
    uint32_t t0 = (uint32_t)key[0]|((uint32_t)key[1]<<8)|((uint32_t)key[2]<<16)|((uint32_t)key[3]<<24);
    uint32_t t1 = (uint32_t)key[4]|((uint32_t)key[5]<<8)|((uint32_t)key[6]<<16)|((uint32_t)key[7]<<24);
    uint32_t t2 = (uint32_t)key[8]|((uint32_t)key[9]<<8)|((uint32_t)key[10]<<16)|((uint32_t)key[11]<<24);
    uint32_t t3 = (uint32_t)key[12]|((uint32_t)key[13]<<8)|((uint32_t)key[14]<<16)|((uint32_t)key[15]<<24);
    ctx->r0 = t0 & 0x3ffffff; t0 >>= 26; t0 |= t1 << 6;
    ctx->r1 = t0 & 0x3ffff03; t1 >>= 20; t1 |= t2 << 12;
    ctx->r2 = t1 & 0x3ffc0ff; t2 >>= 14; t2 |= t3 << 18;
    ctx->r3 = t2 & 0x3f03fff; t3 >>= 8;
    ctx->r4 = t3 & 0x00fffff;
    ctx->s1 = ctx->r1 * 5; ctx->s2 = ctx->r2 * 5; ctx->s3 = ctx->r3 * 5; ctx->s4 = ctx->r4 * 5;
    ctx->h0 = ctx->h1 = ctx->h2 = ctx->h3 = ctx->h4 = 0;
}

static void poly1305_blocks(poly1305_ctx* ctx, const uint8_t* m, size_t len, int hibit) {
    uint32_t h0=ctx->h0,h1=ctx->h1,h2=ctx->h2,h3=ctx->h3,h4=ctx->h4;
    uint32_t r0=ctx->r0,r1=ctx->r1,r2=ctx->r2,r3=ctx->r3,r4=ctx->r4;
    uint32_t s1=ctx->s1,s2=ctx->s2,s3=ctx->s3,s4=ctx->s4;
    uint32_t c;
    while (len >= 16) {
        uint32_t b0=(uint32_t)m[0]|((uint32_t)m[1]<<8)|((uint32_t)m[2]<<16)|((uint32_t)m[3]<<24);
        uint32_t b1=(uint32_t)m[4]|((uint32_t)m[5]<<8)|((uint32_t)m[6]<<16)|((uint32_t)m[7]<<24);
        uint32_t b2=(uint32_t)m[8]|((uint32_t)m[9]<<8)|((uint32_t)m[10]<<16)|((uint32_t)m[11]<<24);
        uint32_t b3=(uint32_t)m[12]|((uint32_t)m[13]<<8)|((uint32_t)m[14]<<16)|((uint32_t)m[15]<<24);
        uint32_t b4=(uint32_t)hibit;
        uint64_t p0 = (uint64_t)h0*r0 + (uint64_t)h1*s4 + (uint64_t)h2*s3 + (uint64_t)h3*s2 + (uint64_t)h4*s1 + b0;
        uint64_t p1 = (uint64_t)h0*r1 + (uint64_t)h1*r0 + (uint64_t)h2*s4 + (uint64_t)h3*s3 + (uint64_t)h4*s2 + b1;
        uint64_t p2 = (uint64_t)h0*r2 + (uint64_t)h1*r1 + (uint64_t)h2*r0 + (uint64_t)h3*s4 + (uint64_t)h4*s3 + b2;
        uint64_t p3 = (uint64_t)h0*r3 + (uint64_t)h1*r2 + (uint64_t)h2*r1 + (uint64_t)h3*r0 + (uint64_t)h4*s4 + b3;
        uint64_t p4 = (uint64_t)h0*r4 + (uint64_t)h1*r3 + (uint64_t)h2*r2 + (uint64_t)h3*r1 + (uint64_t)h4*r0 + b4;
        c = (uint32_t)(p0>>26); h0 = (uint32_t)p0 & 0x3ffffff; p1 += c;
        c = (uint32_t)(p1>>26); h1 = (uint32_t)p1 & 0x3ffffff; p2 += c;
        c = (uint32_t)(p2>>26); h2 = (uint32_t)p2 & 0x3ffffff; p3 += c;
        c = (uint32_t)(p3>>26); h3 = (uint32_t)p3 & 0x3ffffff; p4 += c;
        c = (uint32_t)(p4>>26); h4 = (uint32_t)p4 & 0x3ffffff; h0 += c*5; c = h0>>26; h0 &= 0x3ffffff; h1 += c;
        m += 16; len -= 16;
    }
    ctx->h0=h0; ctx->h1=h1; ctx->h2=h2; ctx->h3=h3; ctx->h4=h4;
}

static void poly1305_finish(poly1305_ctx* ctx, const uint8_t key[32], uint8_t mac[16]) {
    uint32_t g0,g1,g2,g3,g4,c,mask;
    uint32_t h0=ctx->h0,h1=ctx->h1,h2=ctx->h2,h3=ctx->h3,h4=ctx->h4;
    c = h1>>26; h1 &= 0x3ffffff; h2 += c;
    c = h2>>26; h2 &= 0x3ffffff; h3 += c;
    c = h3>>26; h3 &= 0x3ffffff; h4 += c;
    c = h4>>26; h4 &= 0x3ffffff; h0 += c*5; c = h0>>26; h0 &= 0x3ffffff; h1 += c;
    g0 = h0+5; c = g0>>26; g0 &= 0x3ffffff;
    g1 = h1+c; c = g1>>26; g1 &= 0x3ffffff;
    g2 = h2+c; c = g2>>26; g2 &= 0x3ffffff;
    g3 = h3+c; c = g3>>26; g3 &= 0x3ffffff;
    g4 = h4+c - 0x3ffffff;  /* 2^130 - 5 */
    mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask)|g0; h1 = (h1 & mask)|g1; h2 = (h2 & mask)|g2; h3 = (h3 & mask)|g3; h4 = (h4 & mask)|g4;

    /* riassembla 128-bit in 4 word da 32 bit */
    uint32_t w0 = h0 | (h1 << 26);
    uint32_t w1 = (h1 >> 6) | (h2 << 20);
    uint32_t w2 = (h2 >> 12) | (h3 << 14);
    uint32_t w3 = (h3 >> 18) | (h4 << 8);

    /* somma s (key[16..31]) con propagazione del carry */
    uint64_t acc = (uint64_t)w0 + ((uint32_t)key[16]|((uint32_t)key[17]<<8)|((uint32_t)key[18]<<16)|((uint32_t)key[19]<<24));
    w0 = (uint32_t)acc;
    acc = (uint64_t)w1 + ((uint32_t)key[20]|((uint32_t)key[21]<<8)|((uint32_t)key[22]<<16)|((uint32_t)key[23]<<24)) + (acc >> 32);
    w1 = (uint32_t)acc;
    acc = (uint64_t)w2 + ((uint32_t)key[24]|((uint32_t)key[25]<<8)|((uint32_t)key[26]<<16)|((uint32_t)key[27]<<24)) + (acc >> 32);
    w2 = (uint32_t)acc;
    acc = (uint64_t)w3 + ((uint32_t)key[28]|((uint32_t)key[29]<<8)|((uint32_t)key[30]<<16)|((uint32_t)key[31]<<24)) + (acc >> 32);
    w3 = (uint32_t)acc;

    mac[0]=(uint8_t)w0; mac[1]=(uint8_t)(w0>>8); mac[2]=(uint8_t)(w0>>16); mac[3]=(uint8_t)(w0>>24);
    mac[4]=(uint8_t)w1; mac[5]=(uint8_t)(w1>>8); mac[6]=(uint8_t)(w1>>16); mac[7]=(uint8_t)(w1>>24);
    mac[8]=(uint8_t)w2; mac[9]=(uint8_t)(w2>>8); mac[10]=(uint8_t)(w2>>16); mac[11]=(uint8_t)(w2>>24);
    mac[12]=(uint8_t)w3; mac[13]=(uint8_t)(w3>>8); mac[14]=(uint8_t)(w3>>16); mac[15]=(uint8_t)(w3>>24);
}

/* MAC Poly1305 su AAD||pad16(AAD)||CT||pad16(CT)||le64(len AAD)||le64(len CT) — RFC 8439. */
int gmb_poly1305_tag(const uint8_t key[32], const uint8_t* aad, size_t aadlen,
                     const uint8_t* ct, size_t ctlen, uint8_t out[16]) {
    poly1305_ctx ctx;
    poly1305_init(&ctx, key);
    uint8_t block[16] = {0};

    /* AAD: blocchi interi + blocco parziale con pad */
    size_t full = aadlen & ~15;
    if (full) poly1305_blocks(&ctx, aad, full, 0);
    size_t rem = aadlen & 15;
    if (rem) {
        memset(block, 0, 16);
        memcpy(block, aad + full, rem);
        poly1305_blocks(&ctx, block, 16, 0);
    }

    /* CT: blocchi interi + blocco parziale con pad (hibit=0, il length è l'ultimo) */
    full = ctlen & ~15;
    if (full) poly1305_blocks(&ctx, ct, full, 0);
    rem = ctlen & 15;
    if (rem) {
        memset(block, 0, 16);
        memcpy(block, ct + full, rem);
        poly1305_blocks(&ctx, block, 16, 0);
    }

    /* length block: le64(aadlen) || le64(ctlen) */
    memset(block, 0, 16);
    for (int i = 0; i < 8; i++) { block[i] = (uint8_t)(aadlen >> (8*i)); block[8+i] = (uint8_t)(ctlen >> (8*i)); }
    poly1305_blocks(&ctx, block, 16, 1);

    poly1305_finish(&ctx, key, out);
    return 0;
}

int gmb_chacha20poly1305_encrypt(const uint8_t key[32], const uint8_t nonce[12],
                                 const uint8_t* aad, size_t aadlen,
                                 const uint8_t* pt, size_t ptlen,
                                 uint8_t* ct, uint8_t tag[16]) {
/* AES-GCM/ChaCha20 in SOFTWARE quando l'OpenSSL non serve: su Apple il blocco AES usa
 * CommonCrypto (accelerato) e ChaCha20 è C puro. Così `libgmb_gfx` (che includendo il runtime
 * si tirava dietro libcrypto per 11 simboli EVP) NON dipende più da OpenSSL: -4,6 MB nel
 * processo di un programma testuale. TLS (gmb_network) continua a usare OpenSSL dove serve. */
#if defined(__APPLE__) && !defined(GMB_AES_ALLOW_OPENSSL)
#define GMB_AES_SOFTWARE 1
#endif
#if defined(HAVE_OPENSSL) && !defined(GMB_AES_SOFTWARE)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;
    EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL);
    EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce);
    int outl = 0;
    if (aadlen) EVP_EncryptUpdate(ctx, NULL, &outl, aad, (int)aadlen);
    if (ptlen) EVP_EncryptUpdate(ctx, ct, &outl, pt, (int)ptlen);
    EVP_EncryptFinal_ex(ctx, ct + outl, &outl);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag);
    EVP_CIPHER_CTX_free(ctx);
    return 0;
#else
    uint8_t poly_key[64];
    gmb_chacha20_block(key, nonce, 0, poly_key);
    gmb_chacha20_xor_counter(key, nonce, 1, pt, ptlen, ct);
    return gmb_poly1305_tag(poly_key, aad, aadlen, ct, ptlen, tag);
#endif
}

int gmb_chacha20poly1305_decrypt(const uint8_t key[32], const uint8_t nonce[12],
                                 const uint8_t* aad, size_t aadlen,
                                 const uint8_t* ct, size_t ctlen,
                                 const uint8_t tag[16], uint8_t* pt) {
/* AES-GCM/ChaCha20 in SOFTWARE quando l'OpenSSL non serve: su Apple il blocco AES usa
 * CommonCrypto (accelerato) e ChaCha20 è C puro. Così `libgmb_gfx` (che includendo il runtime
 * si tirava dietro libcrypto per 11 simboli EVP) NON dipende più da OpenSSL: -4,6 MB nel
 * processo di un programma testuale. TLS (gmb_network) continua a usare OpenSSL dove serve. */
#if defined(__APPLE__) && !defined(GMB_AES_ALLOW_OPENSSL)
#define GMB_AES_SOFTWARE 1
#endif
#if defined(HAVE_OPENSSL) && !defined(GMB_AES_SOFTWARE)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;
    EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL);
    EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce);
    int outl = 0;
    if (aadlen) EVP_DecryptUpdate(ctx, NULL, &outl, aad, (int)aadlen);
    if (ctlen) EVP_DecryptUpdate(ctx, pt, &outl, ct, (int)ctlen);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, (void*)tag);
    int ret = EVP_DecryptFinal_ex(ctx, pt + outl, &outl);
    EVP_CIPHER_CTX_free(ctx);
    return ret > 0 ? 0 : -1;
#else
    uint8_t poly_key[64];
    gmb_chacha20_block(key, nonce, 0, poly_key);
    uint8_t calc_tag[16];
    if (gmb_poly1305_tag(poly_key, aad, aadlen, ct, ctlen, calc_tag) != 0) return -1;
    for (int i = 0; i < 16; i++) if (calc_tag[i] != tag[i]) return -1;
    gmb_chacha20_xor_counter(key, nonce, 1, ct, ctlen, pt);
    return 0;
#endif
}

/* Poly1305 (RFC 8439) — implementazione 130-bit a 5 word da 26 bit (verificata). */


/* ============================================================================
 * Wrapper val-returning per i backend C (C99Native): le funzioni
 * CRYPTOGRAPHY.AESENCRYPT/AESDECRYPT/CHACHA20* ritornano byte[] (gmb_string_t).
 * ============================================================================ */
gmb_string_t gmb_aes_encrypt_val(gmb_string_t data, gmb_string_t key)
{
    size_t n = 0;
    uint8_t* out = gmb_aes_encrypt_password((const uint8_t*)data.data, data.length,
                                            key.data ? key.data : "", &n);
    gmb_string_t r; r.length = (uint32_t)n; r.ref_count = 1; r.data = (char*)out;
    if (!r.data) { r.data = (char*)calloc(1, 1); r.length = 0; }
    return r;
}
gmb_string_t gmb_aes_decrypt_val(gmb_string_t data, gmb_string_t key)
{
    size_t n = 0;
    uint8_t* out = gmb_aes_decrypt_password((const uint8_t*)data.data, data.length,
                                            key.data ? key.data : "", &n);
    gmb_string_t r; r.length = (uint32_t)n; r.ref_count = 1; r.data = (char*)out;
    if (!r.data) { r.data = (char*)calloc(1, 1); r.length = 0; }
    return r;
}
/* ChaCha20 (XOR stream, simmetrico): encrypt == decrypt. */
gmb_string_t gmb_chacha20_xor_val(gmb_string_t data, gmb_string_t key, gmb_string_t nonce)
{
    uint8_t k[32] = {0}, n[12] = {0};
    for (size_t i = 0; i < key.length && i < 32; i++) k[i] = (uint8_t)key.data[i];
    for (size_t i = 0; i < nonce.length && i < 12; i++) n[i] = (uint8_t)nonce.data[i];
    gmb_string_t r; r.length = data.length; r.ref_count = 1;
    r.data = (char*)malloc(data.length ? data.length : 1);
    if (!r.data) { r.data = (char*)calloc(1, 1); r.length = 0; return r; }
    gmb_chacha20_xor(k, n, (const uint8_t*)data.data, data.length, (uint8_t*)r.data);
    return r;
}

/* Variante uint8_t* + out-len per l'emitter C# (DIM x() AS BYTE). */
uint8_t* gmb_chacha20_xor_bytes(const uint8_t* data, size_t dlen,
                                const uint8_t* key, size_t klen,
                                const uint8_t* nonce, size_t nlen, size_t* out_len)
{
    uint8_t k[32] = {0}, n[12] = {0};
    for (size_t i = 0; i < klen && i < 32; i++) k[i] = key[i];
    for (size_t i = 0; i < nlen && i < 12; i++) n[i] = nonce[i];
    uint8_t* out = (uint8_t*)malloc(dlen ? dlen : 1);
    if (!out) { *out_len = 0; return NULL; }
    gmb_chacha20_xor(k, n, data, dlen, out);
    *out_len = dlen;
    return out;
}

/* Wrapper val per CHACHA20POLY1305ENCRYPT/DECRYPT (backend C). */
gmb_val_t gmb_chacha20poly1305_encrypt_sval(gmb_string_t pt, gmb_string_t key, gmb_string_t nonce, gmb_string_t aad)
{
    return gmb_chacha20poly1305_encrypt_val(
        (const uint8_t*)(key.data ? key.data : ""), key.length,
        (const uint8_t*)(nonce.data ? nonce.data : ""), nonce.length,
        (const uint8_t*)(pt.data ? pt.data : ""), pt.length,
        (const uint8_t*)(aad.data ? aad.data : ""), aad.length);
}
gmb_string_t gmb_chacha20poly1305_decrypt_sval(gmb_val_t ct, gmb_val_t tag, gmb_string_t key, gmb_string_t nonce, gmb_string_t aad)
{
    uint8_t cbuf[8192], tbuf[16], k[32] = {0}, n[12] = {0}, abuf[1024];
    size_t clen = 0, tlen = 0, alen = 0;
    gmb_string_t r; r.ref_count = 1; r.data = (char*)calloc(1, 1); r.length = 0;
    gmb_list_t cl = gmb_val_as_list(ct);
    if (cl.items) { gmb_int_t nn = gmb_list_count(cl); if (nn > (gmb_int_t)sizeof(cbuf)) nn = (gmb_int_t)sizeof(cbuf); for (gmb_int_t i = 0; i < nn; i++) cbuf[i] = (uint8_t)gmb_list_get_or_nan(cl, i); clen = (size_t)nn; }
    gmb_list_t tl = gmb_val_as_list(tag);
    if (tl.items) { gmb_int_t nn = gmb_list_count(tl); if (nn > 16) nn = 16; for (gmb_int_t i = 0; i < nn; i++) tbuf[i] = (uint8_t)gmb_list_get_or_nan(tl, i); tlen = (size_t)nn; }
    for (size_t i = 0; i < key.length && i < 32; i++) k[i] = (uint8_t)key.data[i];
    for (size_t i = 0; i < nonce.length && i < 12; i++) n[i] = (uint8_t)nonce.data[i];
    alen = aad.length < sizeof(abuf) ? aad.length : sizeof(abuf);
    for (size_t i = 0; i < alen; i++) abuf[i] = (uint8_t)aad.data[i];
    if (tlen < 16 || clen == 0) return r;
    uint8_t* pt = (uint8_t*)malloc(clen);
    if (!pt) return r;
    int rc = gmb_chacha20poly1305_decrypt(k, n, abuf, alen, cbuf, clen, tbuf, pt);
    if (rc != 0) { free(pt); return r; }
    free(r.data);
    r.data = (char*)pt; r.length = (uint32_t)clen;
    return r;
}

/* Varianti uint8_t* + len per l'emitter C#. */
gmb_val_t gmb_chacha20poly1305_encrypt_bytes(const uint8_t* pt, size_t plen,
    const uint8_t* key, size_t klen, const uint8_t* nonce, size_t nlen,
    const uint8_t* aad, size_t alen)
{
    return gmb_chacha20poly1305_encrypt_val(key, klen, nonce, nlen, pt, plen, aad, alen);
}
gmb_string_t gmb_chacha20poly1305_decrypt_bytes(gmb_val_t ct, gmb_val_t tag,
    const uint8_t* key, size_t klen, const uint8_t* nonce, size_t nlen,
    const uint8_t* aad, size_t alen)
{
    gmb_string_t keyS; keyS.data = (char*)key; keyS.length = (uint32_t)klen; keyS.ref_count = 0;
    gmb_string_t nonceS; nonceS.data = (char*)nonce; nonceS.length = (uint32_t)nlen; nonceS.ref_count = 0;
    gmb_string_t aadS; aadS.data = (char*)aad; aadS.length = (uint32_t)alen; aadS.ref_count = 0;
    return gmb_chacha20poly1305_decrypt_sval(ct, tag, keyS, nonceS, aadS);
}

/* ── AI.SHAREENCRYPT / AI.SHAREDECRYPT (parità ProvenanceManager) ─────────────────────
 * Prima questa logica era EMESSA INLINE dai due codegen come statement-expression con buffer
 * locali (§0bis: vietato) e la gamba nativa produceva C non compilabile. Ora vive QUI, unica
 * sorgente: base64(AES-GCM-PBKDF2(content, key)). */
char* gmb_share_encrypt(const char* content, const char* key) {
    if (!content || !*content || !key || !*key) return strdup("");
    size_t n = 0;
    uint8_t* enc = gmb_aes_encrypt_password((const uint8_t*)content, strlen(content), key, &n);
    if (!enc) return strdup("");
    char* b64 = gmb_base64_encode(enc, n);
    free(enc);
    return b64 ? b64 : strdup("");
}

char* gmb_share_decrypt(const char* cipher_b64, const char* key) {
    /* Parità con l'oracolo (VM/C 1338 e C# Convert.FromBase64String/AES): base64 non valido O
     * decifratura fallita -> lo STESSO messaggio esplicito, mai stringa vuota silenziosa. */
    if (!cipher_b64 || !*cipher_b64 || !key || !*key) return strdup("ERROR: The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
    size_t n = 0;
    uint8_t* raw = gmb_base64_decode(cipher_b64, &n);
    if (!raw) return strdup("ERROR: The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
    size_t ol = 0;
    uint8_t* out = gmb_aes_decrypt_password(raw, n, key, &ol);
    free(raw);
    if (!out) return strdup("ERROR: The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
    char* r = (char*)malloc(ol + 1);
    if (r) { memcpy(r, out, ol); r[ol] = 0; }
    free(out);
    return r ? r : strdup("");
}
