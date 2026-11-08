/* crypto_password.c — PASSWORDHASH / PASSWORDVERIFY.
 *
 * Formato identico a quello del builtin (gmb_password_hash in gmb_encoding.c):
 *   Base64(salt 16 byte) "." Base64(PBKDF2-SHA256(password, salt, 100000, 32 byte))
 * Cosi' gli hash gia' salvati restano verificabili dopo la migrazione.
 */
#include "gmbcrypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GMB_PWHASH_SALT_LEN 16u
#define GMB_PWHASH_ITER     100000
#define GMB_PWHASH_HASH_LEN 32u

char* ofx_crypto_password_hash(const char* password)
{
    uint8_t salt[GMB_PWHASH_SALT_LEN];
    uint8_t hash[GMB_PWHASH_HASH_LEN];
    char* salt_b64;
    char* hash_b64;
    char* out;
    size_t n;

    if (password == NULL) password = "";
    if (ofx_crypto_random_bytes(salt, sizeof salt) != (int)sizeof salt) return NULL;
    if (ofx_crypto_pbkdf2("sha256", (const uint8_t*)password, strlen(password),
                          salt, sizeof salt, GMB_PWHASH_ITER,
                          hash, sizeof hash) != (int)sizeof hash)
        return NULL;

    salt_b64 = ofx_crypto_base64_encode(salt, sizeof salt);
    hash_b64 = ofx_crypto_base64_encode(hash, sizeof hash);
    if (salt_b64 == NULL || hash_b64 == NULL) {
        free(salt_b64);
        free(hash_b64);
        return NULL;
    }
    n = strlen(salt_b64) + 1 + strlen(hash_b64) + 1;
    out = (char*)malloc(n);
    if (out == NULL) {
        free(salt_b64);
        free(hash_b64);
        return NULL;
    }
    snprintf(out, n, "%s.%s", salt_b64, hash_b64);
    free(salt_b64);
    free(hash_b64);
    return out;
}

int ofx_crypto_password_verify(const char* password, const char* stored)
{
    const char* dot;
    char* salt_b64;
    uint8_t salt[GMB_PWHASH_SALT_LEN];
    uint8_t want[GMB_PWHASH_HASH_LEN];
    uint8_t got[GMB_PWHASH_HASH_LEN];
    int salt_len, want_len, rc;

    if (password == NULL || stored == NULL) return 0;
    dot = strchr(stored, '.');
    if (dot == NULL || dot == stored) return 0;

    salt_b64 = (char*)malloc((size_t)(dot - stored) + 1);
    if (salt_b64 == NULL) return 0;
    memcpy(salt_b64, stored, (size_t)(dot - stored));
    salt_b64[dot - stored] = '\0';

    salt_len = ofx_crypto_base64_decode(salt_b64, salt, sizeof salt);
    free(salt_b64);
    want_len = ofx_crypto_base64_decode(dot + 1, want, sizeof want);
    if (salt_len <= 0 || want_len != (int)sizeof want) return 0;

    if (ofx_crypto_pbkdf2("sha256", (const uint8_t*)password, strlen(password),
                          salt, (size_t)salt_len, GMB_PWHASH_ITER,
                          got, sizeof got) != (int)sizeof got)
        return 0;

    rc = ofx_crypto_timing_safe_equal(got, want, sizeof got);
    memset(got, 0, sizeof got);
    memset(want, 0, sizeof want);
    return rc;
}
