/* crypto_sym.c — AES-GCM con password (formato storico del builtin) e ChaCha20 grezzo.
 *
 * Formato AESENCRYPT/AESDECRYPT (identico a gmb_aes_encrypt_password):
 *   salt 16B | nonce 12B | tag 16B | ciphertext
 *   chiave = PBKDF2-SHA256(password, salt, 100000 iter, 32 byte)
 * Cosi' i payload cifrati dal builtin restano decifrabili.
 */
#include "gmbcrypto.h"

#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#define SALT_LEN  16
#define NONCE_LEN 12
#define TAG_LEN   16
#define HDR_LEN   (SALT_LEN + NONCE_LEN + TAG_LEN) /* 44 */

int ofx_crypto_aes_password_encrypt(const uint8_t* data, size_t len, const char* password,
                                    uint8_t* out, size_t cap)
{
    uint8_t salt[SALT_LEN], nonce[NONCE_LEN], key[32];
    EVP_CIPHER_CTX* ctx;
    int l = 0, t = 0;

    if (password == NULL) password = "";
    if (out == NULL) return -1;
    if (cap < HDR_LEN + len) return -2;
    if (RAND_bytes(salt, SALT_LEN) != 1 || RAND_bytes(nonce, NONCE_LEN) != 1) return -1;
    if (PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, SALT_LEN, 100000,
                          EVP_sha256(), 32, key) != 1)
        return -1;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) return -1;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, NONCE_LEN, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    if (len > 0) {
        if (EVP_EncryptUpdate(ctx, out + HDR_LEN, &l, data, (int)len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        t = l;
    }
    if (EVP_EncryptFinal_ex(ctx, out + HDR_LEN + t, &l) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    t += l;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, TAG_LEN, out + SALT_LEN + NONCE_LEN) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    EVP_CIPHER_CTX_free(ctx);
    memcpy(out, salt, SALT_LEN);
    memcpy(out + SALT_LEN, nonce, NONCE_LEN);
    return (int)(HDR_LEN + (size_t)t);
}

int ofx_crypto_aes_password_decrypt(const uint8_t* enc, size_t len, const char* password,
                                    uint8_t* out, size_t cap)
{
    uint8_t key[32];
    EVP_CIPHER_CTX* ctx;
    size_t ctlen;
    int l = 0, t = 0;

    if (password == NULL) password = "";
    if (enc == NULL || out == NULL || len < HDR_LEN) return -1;
    ctlen = len - HDR_LEN;
    if (ctlen > cap) return -2;
    if (PKCS5_PBKDF2_HMAC(password, (int)strlen(password), enc, SALT_LEN, 100000,
                          EVP_sha256(), 32, key) != 1)
        return -1;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) return -1;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, NONCE_LEN, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, enc + SALT_LEN) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    if (ctlen > 0) {
        if (EVP_DecryptUpdate(ctx, out, &l, enc + HDR_LEN, (int)ctlen) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        t = l;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, TAG_LEN,
                            (void*)(enc + SALT_LEN + NONCE_LEN)) != 1 ||
        EVP_DecryptFinal_ex(ctx, out + t, &l) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1; /* password errata o payload manomesso */
    }
    t += l;
    EVP_CIPHER_CTX_free(ctx);
    return t;
}

/* ChaCha20 grezzo (RFC 8439, contatore iniziale 0): XOR del keystream sui dati. */
int ofx_crypto_chacha20_xor(const uint8_t* data, size_t len,
                            const uint8_t* key, size_t klen,
                            const uint8_t* nonce, size_t nlen,
                            uint8_t* out, size_t cap)
{
    uint8_t k[32];
    uint8_t iv[16]; /* counter (4, LE) || nonce (12) */
    EVP_CIPHER_CTX* ctx;
    size_t i;
    int l = 0, t = 0;

    if (out == NULL || key == NULL || nonce == NULL) return -1;
    if (len > cap) return -2;
    memset(k, 0, sizeof k);
    memset(iv, 0, sizeof iv);
    iv[0] = 1; /* RFC 8439: il primo blocco usa il contatore 1 */
    for (i = 0; i < klen && i < 32; i++) k[i] = key[i];
    for (i = 0; i < nlen && i < 12; i++) iv[4 + i] = nonce[i];

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) return -1;
    if (EVP_EncryptInit_ex(ctx, EVP_chacha20(), NULL, k, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    if (len > 0) {
        if (EVP_EncryptUpdate(ctx, out, &l, data, (int)len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
        t = l;
    }
    if (EVP_EncryptFinal_ex(ctx, out + t, &l) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    t += l;
    EVP_CIPHER_CTX_free(ctx);
    return t;
}
