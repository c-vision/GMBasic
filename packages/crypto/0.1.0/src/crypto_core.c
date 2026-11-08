/* crypto_core.c — hashing, HMAC, KDF, AEAD e random del dominio CRYPTOGRAPHY.
 *
 * Tutto passa da OpenSSL 3 (libcrypto): e' la dipendenza esterna che rende il
 * dominio un PACCHETTO e non una lib self-contained di shared/libs.
 */
#include "gmbcrypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>

/* ── mappature algoritmo → EVP ────────────────────────────────────────────── */
/* confronto senza distinzione di maiuscole: "SHA256" e "sha256" sono lo stesso algoritmo */
static int algo_ieq(const char* a, const char* b)
{
    if (a == NULL || b == NULL) return 0;
    while (*a != '\0' && *b != '\0') {
        int ca = (unsigned char)*a;
        int cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static const EVP_MD* md_for(const char* algo)
{
    if (algo == NULL) return NULL;
    if (algo_ieq(algo, "sha1")) return EVP_sha1();
    if (algo_ieq(algo, "sha256")) return EVP_sha256();
    if (algo_ieq(algo, "sha384")) return EVP_sha384();
    if (algo_ieq(algo, "sha512")) return EVP_sha512();
    if (algo_ieq(algo, "sha3-256")) return EVP_sha3_256();
    if (algo_ieq(algo, "sha3-512")) return EVP_sha3_512();
    if (algo_ieq(algo, "md5")) return EVP_md5();
    if (algo_ieq(algo, "ripemd160")) return EVP_ripemd160();
    return NULL;
}

/* nome canonico usato da HMAC/HKDF (OSSL_PARAM) */
static const char* md_name_for(const char* algo)
{
    if (algo == NULL) return NULL;
    if (strcmp(algo, "sha1") == 0) return "SHA1";
    if (strcmp(algo, "sha256") == 0) return "SHA256";
    if (strcmp(algo, "sha384") == 0) return "SHA384";
    if (strcmp(algo, "sha512") == 0) return "SHA512";
    if (strcmp(algo, "sha3-256") == 0) return "SHA3-256";
    if (strcmp(algo, "sha3-512") == 0) return "SHA3-512";
    return NULL;
}

/* ── hashing ──────────────────────────────────────────────────────────────── */
int ofx_crypto_digest(const char* algo, const uint8_t* data, size_t len,
                      uint8_t* out, size_t cap)
{
    const EVP_MD* md = md_for(algo);
    EVP_MD_CTX* ctx;
    unsigned int md_len = 0;
    if (md == NULL || out == NULL || (data == NULL && len > 0)) return -1;
    if ((size_t)EVP_MD_get_size(md) > cap) return -2;
    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) return -1;
    if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
        (len > 0 && EVP_DigestUpdate(ctx, data, len) != 1) ||
        EVP_DigestFinal_ex(ctx, out, &md_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    EVP_MD_CTX_free(ctx);
    return (int)md_len;
}

char* ofx_crypto_digest_hex(const char* algo, const uint8_t* data, size_t len)
{
    uint8_t buf[EVP_MAX_MD_SIZE];
    int n = ofx_crypto_digest(algo, data, len, buf, sizeof buf);
    if (n < 0) return NULL;
    return ofx_crypto_hex_encode(buf, (size_t)n);
}

char* ofx_crypto_digest_hex_str(const char* algo, const char* text)
{
    if (text == NULL) text = "";
    return ofx_crypto_digest_hex(algo, (const uint8_t*)text, strlen(text));
}

int ofx_crypto_digest_str(const char* algo, const char* text, uint8_t* out, size_t cap)
{
    if (text == NULL) text = "";
    return ofx_crypto_digest(algo, (const uint8_t*)text, strlen(text), out, cap);
}

char* ofx_crypto_file_hex(const char* path, const char* algo)
{
    const EVP_MD* md = md_for(algo);
    EVP_MD_CTX* ctx;
    unsigned char chunk[65536];
    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;
    FILE* f;
    size_t n;
    char* hex;

    if (path == NULL || md == NULL) return NULL;
    f = fopen(path, "rb");
    if (f == NULL) return NULL;
    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) { fclose(f); return NULL; }
    if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return NULL;
    }
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) {
        if (EVP_DigestUpdate(ctx, chunk, n) != 1) {
            EVP_MD_CTX_free(ctx);
            fclose(f);
            return NULL;
        }
    }
    fclose(f);
    if (EVP_DigestFinal_ex(ctx, buf, &md_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    EVP_MD_CTX_free(ctx);
    hex = ofx_crypto_hex_encode(buf, (size_t)md_len);
    return hex;
}

/* ── HMAC (EVP_MAC: niente API deprecate) ─────────────────────────────────── */
int ofx_crypto_hmac(const char* algo, const uint8_t* key, size_t key_len,
                    const uint8_t* data, size_t len, uint8_t* out, size_t cap)
{
    const char* md_name = md_name_for(algo);
    EVP_MAC* mac;
    EVP_MAC_CTX* ctx;
    OSSL_PARAM params[2];
    size_t out_len = 0;
    int rc = -1;

    if (md_name == NULL || out == NULL || (data == NULL && len > 0)) return -1;
    if (cap < 64) {
        /* le tag HMAC del dominio sono <= 64 byte (SHA-512) */
        return -2;
    }
    mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    if (mac == NULL) return -1;
    ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);
    if (ctx == NULL) return -1;
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, (char*)md_name, 0);
    params[1] = OSSL_PARAM_construct_end();
    if (EVP_MAC_init(ctx, key, key_len, params) == 1 &&
        (len == 0 || EVP_MAC_update(ctx, data, len) == 1) &&
        EVP_MAC_final(ctx, out, &out_len, cap) == 1) {
        rc = (int)out_len;
    }
    EVP_MAC_CTX_free(ctx);
    return rc;
}

char* ofx_crypto_hmac_hex(const char* algo, const char* key, const char* text)
{
    uint8_t buf[64];
    int n;
    if (key == NULL) key = "";
    if (text == NULL) text = "";
    n = ofx_crypto_hmac(algo, (const uint8_t*)key, strlen(key),
                        (const uint8_t*)text, strlen(text), buf, sizeof buf);
    if (n < 0) return NULL;
    return ofx_crypto_hex_encode(buf, (size_t)n);
}

/* ── PBKDF2 ───────────────────────────────────────────────────────────────── */
int ofx_crypto_pbkdf2(const char* algo, const uint8_t* pw, size_t pw_len,
                      const uint8_t* salt, size_t salt_len, int iters,
                      uint8_t* out, size_t out_len)
{
    const EVP_MD* md = md_for(algo);
    if (md == NULL || out == NULL || iters <= 0) return -1;
    if (PKCS5_PBKDF2_HMAC((const char*)(pw ? pw : (const uint8_t*)""), (int)pw_len,
                          salt, (int)salt_len, iters, md,
                          (int)out_len, out) != 1)
        return -1;
    return (int)out_len;
}

/* ── HKDF (RFC 5869) ──────────────────────────────────────────────────────── */
int ofx_crypto_hkdf(const char* algo, const uint8_t* ikm, size_t ikm_len,
                    const uint8_t* salt, size_t salt_len,
                    const uint8_t* info, size_t info_len,
                    uint8_t* out, size_t out_len)
{
    const char* md_name = md_name_for(algo);
    EVP_KDF* kdf;
    EVP_KDF_CTX* ctx;
    OSSL_PARAM params[6];
    int rc = -1, i = 0;

    if (md_name == NULL || out == NULL || ikm == NULL) return -1;
    kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
    if (kdf == NULL) return -1;
    ctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (ctx == NULL) return -1;

    params[i++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, (char*)md_name, 0);
    params[i++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void*)ikm, ikm_len);
    if (salt != NULL && salt_len > 0)
        params[i++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void*)salt, salt_len);
    if (info != NULL && info_len > 0)
        params[i++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void*)info, info_len);
    params[i] = OSSL_PARAM_construct_end();

    if (EVP_KDF_derive(ctx, out, out_len, params) == 1) rc = (int)out_len;
    EVP_KDF_CTX_free(ctx);
    return rc;
}

/* ── AEAD ─────────────────────────────────────────────────────────────────── */
static const EVP_CIPHER* cipher_for(const char* algo)
{
    if (algo == NULL) return NULL;
    if (strcmp(algo, "aes256gcm") == 0) return EVP_aes_256_gcm();
    if (strcmp(algo, "aes128gcm") == 0) return EVP_aes_128_gcm();
    if (strcmp(algo, "chacha20poly1305") == 0) return EVP_chacha20_poly1305();
    return NULL;
}

#define GMB_TAG_LEN 16

int ofx_crypto_aead_encrypt(const char* algo,
                            const uint8_t* key, size_t key_len,
                            const uint8_t* iv, size_t iv_len,
                            const uint8_t* aad, size_t aad_len,
                            const uint8_t* pt, size_t pt_len,
                            uint8_t* out, size_t cap)
{
    const EVP_CIPHER* cipher = cipher_for(algo);
    EVP_CIPHER_CTX* ctx;
    int len = 0, total = 0;
    if (cipher == NULL || out == NULL || iv == NULL) return -1;
    if (key_len != (size_t)EVP_CIPHER_get_key_length(cipher)) return -1;
    if (pt_len + GMB_TAG_LEN > cap) return -2;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) return -1;
    if (EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)iv_len, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    if (aad_len > 0 && EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aad_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    if (pt_len > 0) {
        if (EVP_EncryptUpdate(ctx, out, &len, pt, (int)pt_len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        total = len;
    }
    if (EVP_EncryptFinal_ex(ctx, out + total, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    total += len;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, GMB_TAG_LEN, out + total) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    EVP_CIPHER_CTX_free(ctx);
    return total + GMB_TAG_LEN;
}

int ofx_crypto_aead_decrypt(const char* algo,
                            const uint8_t* key, size_t key_len,
                            const uint8_t* iv, size_t iv_len,
                            const uint8_t* aad, size_t aad_len,
                            const uint8_t* ct, size_t ct_len,
                            uint8_t* out, size_t cap)
{
    const EVP_CIPHER* cipher = cipher_for(algo);
    EVP_CIPHER_CTX* ctx;
    size_t body;
    int len = 0, total = 0;
    if (cipher == NULL || out == NULL || iv == NULL || ct == NULL) return -1;
    if (key_len != (size_t)EVP_CIPHER_get_key_length(cipher)) return -1;
    if (ct_len < GMB_TAG_LEN) return -1;
    body = ct_len - GMB_TAG_LEN;
    if (body > cap) return -2;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) return -1;
    if (EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)iv_len, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    if (aad_len > 0 && EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aad_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    if (body > 0) {
        if (EVP_DecryptUpdate(ctx, out, &len, ct, (int)body) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        total = len;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, GMB_TAG_LEN,
                            (void*)(ct + body)) != 1 ||
        EVP_DecryptFinal_ex(ctx, out + total, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1; /* autenticazione fallita */
    }
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return total;
}

/* ── random / utility ─────────────────────────────────────────────────────── */
int ofx_crypto_random_bytes(uint8_t* out, size_t len)
{
    if (out == NULL) return -1;
    if (len == 0) return 0;
    if (RAND_bytes(out, (int)len) != 1) return -1;
    return (int)len;
}

int ofx_crypto_timing_safe_equal(const uint8_t* a, const uint8_t* b, size_t len)
{
    if (a == NULL || b == NULL) return 0;
    return CRYPTO_memcmp(a, b, len) == 0 ? 1 : 0;
}
