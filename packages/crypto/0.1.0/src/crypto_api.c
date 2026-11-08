/* crypto_api.c — adattatori per la superficie BASIC (FFI).
 *
 * Il pacchetto usa handle `int` e buffer `uint8_t*`; l'ABI di GMBasic passa
 * handle a 64 bit e array di BYTE come puntatore. Qui stanno solo le
 * conversioni, senza duplicare la logica (che resta in crypto_keys.c).
 */
#include "gmbcrypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ofx_crypto_keys_generate_pair(const char* algo, int64_t bits, int64_t* out)
{
    int pvt = 0, pub = 0;
    if (out == NULL) return -1;
    out[0] = 0;
    out[1] = 0;
    if (ofx_crypto_keys_generate(algo, (int)bits, &pvt, &pub) != 0) return -1;
    out[0] = (int64_t)pvt;
    out[1] = (int64_t)pub;
    return 0;
}

int64_t ofx_crypto_keys_import64(const char* pem)
{
    return (int64_t)ofx_crypto_keys_import_pem(pem);
}

int ofx_crypto_keys_close64(int64_t handle)
{
    ofx_crypto_keys_close((int)handle);
    return 0;
}

int ofx_crypto_keys_sign_buf(int64_t handle, const char* data, size_t len,
                             uint8_t* out, size_t cap)
{
    size_t n = 0;
    uint8_t* sig = ofx_crypto_keys_sign((int)handle, (const uint8_t*)data, len, &n);
    if (sig == NULL) return -1;
    if (n > cap) { free(sig); return -2; }
    memcpy(out, sig, n);
    free(sig);
    return (int)n;
}

int ofx_crypto_keys_verify_buf(int64_t handle, const char* data, size_t len,
                               const uint8_t* sig, size_t siglen)
{
    return ofx_crypto_keys_verify((int)handle, (const uint8_t*)data, len, sig, siglen);
}

int ofx_crypto_keys_encrypt_buf(int64_t handle, const char* data, size_t len,
                                uint8_t* out, size_t cap)
{
    size_t n = 0;
    uint8_t* enc = ofx_crypto_keys_encrypt((int)handle, (const uint8_t*)data, len, &n);
    if (enc == NULL) return -1;
    if (n > cap) { free(enc); return -2; }
    memcpy(out, enc, n);
    free(enc);
    return (int)n;
}

int ofx_crypto_keys_decrypt_buf(int64_t handle, const uint8_t* enc, size_t len,
                                uint8_t* out, size_t cap)
{
    size_t n = 0;
    uint8_t* pt = ofx_crypto_keys_decrypt((int)handle, enc, len, &n);
    if (pt == NULL) return -1;
    if (n > cap) { free(pt); return -2; }
    memcpy(out, pt, n);
    free(pt);
    return (int)n;
}

/* ChaCha20 con il testo come STRING (la variante su array e' la funzione base). */
int ofx_crypto_chacha20_xor_text(const char* text, size_t len,
                                 const uint8_t* key, size_t klen,
                                 const uint8_t* nonce, size_t nlen,
                                 uint8_t* out, size_t cap)
{
    return ofx_crypto_chacha20_xor((const uint8_t*)text, len, key, klen, nonce, nlen, out, cap);
}

/* Digest di un file (streaming: il file non viene caricato in memoria). */
int ofx_crypto_file_digest(const char* path, const char* algo, uint8_t* out, size_t cap)
{
    FILE* f;
    uint8_t chunk[65536];
    uint8_t* data = NULL;
    size_t len = 0, used = 0, nread;
    int rc;

    if (path == NULL || out == NULL) return -1;
    f = fopen(path, "rb");
    if (f == NULL) return -1;
    for (;;) {
        nread = fread(chunk, 1, sizeof chunk, f);
        if (nread == 0) break;
        if (used + nread > len) {
            size_t nlen2 = len ? len * 2 : 65536;
            uint8_t* nd;
            while (nlen2 < used + nread) nlen2 *= 2;
            nd = (uint8_t*)realloc(data, nlen2);
            if (nd == NULL) { free(data); fclose(f); return -1; }
            data = nd;
            len = nlen2;
        }
        memcpy(data + used, chunk, nread);
        used += nread;
    }
    fclose(f);
    rc = ofx_crypto_digest(algo, data, used, out, cap);
    free(data);
    return rc;
}
