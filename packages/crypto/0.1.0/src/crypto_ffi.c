/* crypto_ffi.c — adattatori ABI per la superficie BASIC.
 *
 * Il bridge FFI di GMBasic passa gli ARRAY come elementi a 8 byte (gmb_int_t):
 * un `out() AS BYTE` arriva qui come `int64_t*` con un elemento per byte e i
 * valori 0..255. Questi adattatori convertono elementi↔byte e delegano alle
 * primitive di libgmbcrypto (nessuna logica duplicata).
 */
#include "gmbcrypto.h"

#include <stdlib.h>
#include <string.h>

static uint8_t* bytes_from_elems(const int64_t* v, size_t n)
{
    uint8_t* b;
    size_t i;
    b = (uint8_t*)malloc(n ? n : 1);
    if (b == NULL) return NULL;
    for (i = 0; i < n; i++) b[i] = (uint8_t)(v[i] & 0xFF);
    return b;
}

static int elems_from_bytes(const uint8_t* b, size_t n, int64_t* out, size_t cap)
{
    size_t i;
    if (n > cap) return -2;
    for (i = 0; i < n; i++) out[i] = (int64_t)b[i];
    return (int)n;
}

int ofx_ffi_digest_str(const char* algo, const char* text, int64_t* out, int64_t cap)
{
    uint8_t buf[128];
    int n = ofx_crypto_digest_str(algo, text, buf, sizeof buf);
    if (n < 0) return n;
    return elems_from_bytes(buf, (size_t)n, out, (size_t)cap);
}

int ofx_ffi_file_digest(const char* path, const char* algo, int64_t* out, int64_t cap)
{
    uint8_t buf[128];
    int n = ofx_crypto_file_digest(path, algo, buf, sizeof buf);
    if (n < 0) return n;
    return elems_from_bytes(buf, (size_t)n, out, (size_t)cap);
}

static int decode_into(const char* text, int64_t* out, int64_t cap,
                       int (*fn)(const char*, uint8_t*, size_t))
{
    uint8_t* buf;
    size_t cap_b;
    int n, rc;
    if (text == NULL || out == NULL) return -1;
    cap_b = strlen(text) + 4;
    buf = (uint8_t*)malloc(cap_b);
    if (buf == NULL) return -1;
    n = fn(text, buf, cap_b);
    if (n < 0) { free(buf); return n; }
    rc = elems_from_bytes(buf, (size_t)n, out, (size_t)cap);
    free(buf);
    return rc;
}

int ofx_ffi_hex_decode(const char* text, int64_t* out, int64_t cap)
{
    return decode_into(text, out, cap, ofx_crypto_hex_decode);
}

int ofx_ffi_b64_decode(const char* text, int64_t* out, int64_t cap)
{
    return decode_into(text, out, cap, ofx_crypto_base64_decode);
}

int ofx_ffi_b64url_decode(const char* text, int64_t* out, int64_t cap)
{
    return decode_into(text, out, cap, ofx_crypto_base64url_decode);
}

int ofx_ffi_random_bytes(int64_t* out, int64_t cap)
{
    uint8_t* buf;
    int rc;
    if (out == NULL || cap <= 0) return -1;
    buf = (uint8_t*)malloc((size_t)cap);
    if (buf == NULL) return -1;
    rc = ofx_crypto_random_bytes(buf, (size_t)cap);
    if (rc < 0) { free(buf); return rc; }
    rc = elems_from_bytes(buf, (size_t)cap, out, (size_t)cap);
    free(buf);
    return rc;
}

/* ── checksum (array in ingresso) ─────────────────────────────────────────── */
int64_t ofx_ffi_crc32(const int64_t* data, int64_t n)
{
    uint8_t* b = bytes_from_elems(data, (size_t)(n > 0 ? n : 0));
    int64_t r = b ? (int64_t)ofx_crypto_crc32(b, (size_t)(n > 0 ? n : 0)) : -1;
    free(b);
    return r;
}
int64_t ofx_ffi_crc32c(const int64_t* data, int64_t n)
{
    uint8_t* b = bytes_from_elems(data, (size_t)(n > 0 ? n : 0));
    int64_t r = b ? (int64_t)ofx_crypto_crc32c(b, (size_t)(n > 0 ? n : 0)) : -1;
    free(b);
    return r;
}
int64_t ofx_ffi_crc16(const int64_t* data, int64_t n, int64_t variant)
{
    uint8_t* b = bytes_from_elems(data, (size_t)(n > 0 ? n : 0));
    int64_t r = b ? (int64_t)ofx_crypto_crc16(b, (size_t)(n > 0 ? n : 0), (int)variant) : -1;
    free(b);
    return r;
}
int64_t ofx_ffi_crc64(const int64_t* data, int64_t n, int64_t variant)
{
    uint8_t* b = bytes_from_elems(data, (size_t)(n > 0 ? n : 0));
    int64_t r = b ? (int64_t)ofx_crypto_crc64(b, (size_t)(n > 0 ? n : 0), (int)variant) : -1;
    free(b);
    return r;
}
int64_t ofx_ffi_adler32(const int64_t* data, int64_t n)
{
    uint8_t* b = bytes_from_elems(data, (size_t)(n > 0 ? n : 0));
    int64_t r = b ? (int64_t)ofx_crypto_adler32(b, (size_t)(n > 0 ? n : 0)) : -1;
    free(b);
    return r;
}
int64_t ofx_ffi_fletcher32(const int64_t* data, int64_t n)
{
    uint8_t* b = bytes_from_elems(data, (size_t)(n > 0 ? n : 0));
    int64_t r = b ? (int64_t)ofx_crypto_fletcher32(b, (size_t)(n > 0 ? n : 0)) : -1;
    free(b);
    return r;
}

/* ── simmetrico ───────────────────────────────────────────────────────────── */
int ofx_ffi_aes_encrypt(const char* data, const char* password, int64_t* out, int64_t cap)
{
    size_t len = data ? strlen(data) : 0;
    uint8_t* buf = (uint8_t*)malloc(44 + len);
    int n, rc;
    if (buf == NULL) return -1;
    n = ofx_crypto_aes_password_encrypt((const uint8_t*)data, len, password, buf, 44 + len);
    if (n < 0) { free(buf); return n; }
    rc = elems_from_bytes(buf, (size_t)n, out, (size_t)cap);
    free(buf);
    return rc;
}

int ofx_ffi_aes_decrypt(const int64_t* enc, int64_t n, const char* password,
                        int64_t* out, int64_t cap)
{
    uint8_t* in;
    uint8_t* buf;
    int rc;
    if (n <= 0 || out == NULL) return -1;
    in = bytes_from_elems(enc, (size_t)n);
    if (in == NULL) return -1;
    buf = (uint8_t*)malloc((size_t)n);
    if (buf == NULL) { free(in); return -1; }
    rc = ofx_crypto_aes_password_decrypt(in, (size_t)n, password, buf, (size_t)n);
    if (rc >= 0) rc = elems_from_bytes(buf, (size_t)rc, out, (size_t)cap);
    free(buf);
    free(in);
    return rc;
}

int ofx_ffi_chacha_text(const char* text, int64_t n, const int64_t* key, int64_t klen,
                        const int64_t* nonce, int64_t nlen, int64_t* out, int64_t cap)
{
    uint8_t* k = bytes_from_elems(key, (size_t)klen);
    uint8_t* nn = bytes_from_elems(nonce, (size_t)nlen);
    uint8_t* buf;
    int rc;
    if (k == NULL || nn == NULL) { free(k); free(nn); return -1; }
    buf = (uint8_t*)malloc((size_t)(n > 0 ? n : 1));
    if (buf == NULL) { free(k); free(nn); return -1; }
    rc = ofx_crypto_chacha20_xor((const uint8_t*)text, (size_t)n, k, (size_t)klen, nn,
                                 (size_t)nlen, buf, (size_t)n);
    if (rc >= 0) rc = elems_from_bytes(buf, (size_t)rc, out, (size_t)cap);
    free(buf);
    free(k);
    free(nn);
    return rc;
}

int ofx_ffi_chacha_arr(const int64_t* data, int64_t n, const int64_t* key, int64_t klen,
                       const int64_t* nonce, int64_t nlen, int64_t* out, int64_t cap)
{
    uint8_t* d = bytes_from_elems(data, (size_t)(n > 0 ? n : 0));
    uint8_t* k = bytes_from_elems(key, (size_t)klen);
    uint8_t* nn = bytes_from_elems(nonce, (size_t)nlen);
    uint8_t* buf;
    int rc;
    if (d == NULL || k == NULL || nn == NULL) { free(d); free(k); free(nn); return -1; }
    buf = (uint8_t*)malloc((size_t)(n > 0 ? n : 1));
    if (buf == NULL) { free(d); free(k); free(nn); return -1; }
    rc = ofx_crypto_chacha20_xor(d, (size_t)n, k, (size_t)klen, nn, (size_t)nlen, buf, (size_t)n);
    if (rc >= 0) rc = elems_from_bytes(buf, (size_t)rc, out, (size_t)cap);
    free(buf);
    free(d);
    free(k);
    free(nn);
    return rc;
}

int ofx_ffi_aead_encrypt(const char* algo, const int64_t* key, int64_t klen,
                         const int64_t* iv, int64_t ivlen, const char* aad, int64_t aadlen,
                         const char* pt, int64_t ptlen, int64_t* out, int64_t cap)
{
    uint8_t* k = bytes_from_elems(key, (size_t)klen);
    uint8_t* v = bytes_from_elems(iv, (size_t)ivlen);
    uint8_t* buf;
    int rc;
    if (k == NULL || v == NULL) { free(k); free(v); return -1; }
    buf = (uint8_t*)malloc((size_t)ptlen + 16);
    if (buf == NULL) { free(k); free(v); return -1; }
    rc = ofx_crypto_aead_encrypt(algo, k, (size_t)klen, v, (size_t)ivlen,
                                 (const uint8_t*)aad, (size_t)aadlen,
                                 (const uint8_t*)pt, (size_t)ptlen, buf, (size_t)ptlen + 16);
    if (rc >= 0) rc = elems_from_bytes(buf, (size_t)rc, out, (size_t)cap);
    free(buf);
    free(k);
    free(v);
    return rc;
}

int ofx_ffi_aead_decrypt(const char* algo, const int64_t* key, int64_t klen,
                         const int64_t* iv, int64_t ivlen, const char* aad, int64_t aadlen,
                         const int64_t* ct, int64_t ctlen, int64_t* out, int64_t cap)
{
    uint8_t* k = bytes_from_elems(key, (size_t)klen);
    uint8_t* v = bytes_from_elems(iv, (size_t)ivlen);
    uint8_t* c = bytes_from_elems(ct, (size_t)(ctlen > 0 ? ctlen : 0));
    uint8_t* buf;
    int rc;
    if (k == NULL || v == NULL || c == NULL) { free(k); free(v); free(c); return -1; }
    buf = (uint8_t*)malloc((size_t)(ctlen > 0 ? ctlen : 1));
    if (buf == NULL) { free(k); free(v); free(c); return -1; }
    rc = ofx_crypto_aead_decrypt(algo, k, (size_t)klen, v, (size_t)ivlen,
                                 (const uint8_t*)aad, (size_t)aadlen,
                                 c, (size_t)ctlen, buf, (size_t)ctlen);
    if (rc >= 0) rc = elems_from_bytes(buf, (size_t)rc, out, (size_t)cap);
    free(buf);
    free(k);
    free(v);
    free(c);
    return rc;
}

/* ── asimmetrico (handle LONG) ────────────────────────────────────────────── */
int ofx_ffi_keys_sign(int64_t handle, const char* data, int64_t* out, int64_t cap)
{
    size_t n = 0;
    uint8_t* sig = ofx_crypto_keys_sign((int)handle, (const uint8_t*)data,
                                        data ? strlen(data) : 0, &n);
    int rc;
    if (sig == NULL) return -1;
    rc = elems_from_bytes(sig, n, out, (size_t)cap);
    free(sig);
    return rc;
}

int ofx_ffi_keys_verify(int64_t handle, const char* data, const int64_t* sig, int64_t slen)
{
    uint8_t* s = bytes_from_elems(sig, (size_t)(slen > 0 ? slen : 0));
    int rc;
    if (s == NULL) return 0;
    rc = ofx_crypto_keys_verify((int)handle, (const uint8_t*)data,
                                data ? strlen(data) : 0, s, (size_t)slen);
    free(s);
    return rc;
}

int ofx_ffi_keys_encrypt(int64_t handle, const char* data, int64_t* out, int64_t cap)
{
    size_t n = 0;
    uint8_t* enc = ofx_crypto_keys_encrypt((int)handle, (const uint8_t*)data,
                                           data ? strlen(data) : 0, &n);
    int rc;
    if (enc == NULL) return -1;
    rc = elems_from_bytes(enc, n, out, (size_t)cap);
    free(enc);
    return rc;
}

int ofx_ffi_keys_decrypt(int64_t handle, const int64_t* enc, int64_t n,
                         int64_t* out, int64_t cap)
{
    uint8_t* in = bytes_from_elems(enc, (size_t)(n > 0 ? n : 0));
    uint8_t* buf;
    int rc;
    size_t outlen = 0;
    if (in == NULL) return -1;
    buf = ofx_crypto_keys_decrypt((int)handle, in, (size_t)n, &outlen);
    free(in);
    if (buf == NULL) return -1;
    rc = elems_from_bytes(buf, outlen, out, (size_t)cap);
    free(buf);
    return rc;
}

int ofx_ffi_keys_verify_raw(const int64_t* data, int64_t n, const int64_t* sig, int64_t slen,
                            const char* pub)
{
    uint8_t* d = bytes_from_elems(data, (size_t)(n > 0 ? n : 0));
    uint8_t* s = bytes_from_elems(sig, (size_t)(slen > 0 ? slen : 0));
    int rc;
    if (d == NULL || s == NULL) { free(d); free(s); return 0; }
    rc = ofx_crypto_keys_verify_raw(d, (size_t)n, s, (size_t)slen, pub);
    free(d);
    free(s);
    return rc;
}
