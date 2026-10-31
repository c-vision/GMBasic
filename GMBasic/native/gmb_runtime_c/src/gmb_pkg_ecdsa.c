/* gmb_pkg_ecdsa.c — firme ECDSA P-256 PEM (riferimento C del Package Manager, OpenSSL). */
#include "gmb_pkg_ecdsa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void b64_enc(const unsigned char* in, size_t n, char* out, size_t cap)
{
    size_t j = 0;
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)in[i] << 16 | (i + 1 < n ? (unsigned)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
        char o[4] = { B64[(v >> 18) & 63], B64[(v >> 12) & 63], i + 1 < n ? B64[(v >> 6) & 63] : '=', i + 2 < n ? B64[v & 63] : '=' };
        for (int k = 0; k < 4 && j + 1 < cap; k++) out[j++] = o[k];
    }
    if (cap) out[j] = 0;
}

int gmb_pkg_ecdsa_sign_pem(const char* priv_pem, const unsigned char* data, size_t len, char* sig_b64, size_t cap)
{
    if (sig_b64 && cap) sig_b64[0] = 0;
    if (!priv_pem) return 0;
    BIO* bio = BIO_new_mem_buf(priv_pem, -1);
    if (!bio) return 0;
    EVP_PKEY* pk = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (!pk) return 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    int ok = 0;
    if (ctx && EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, pk) == 1) {
        size_t siglen = 0;
        if (EVP_DigestSign(ctx, NULL, &siglen, data, len) == 1) {
            unsigned char* sig = (unsigned char*)malloc(siglen);
            if (sig && EVP_DigestSign(ctx, sig, &siglen, data, len) == 1) { b64_enc(sig, siglen, sig_b64, cap); ok = 1; }
            free(sig);
        }
    }
    if (ctx) EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pk);
    return ok;
}

int gmb_pkg_ecdsa_public_pem(const char* priv_pem, char* pub_pem, size_t cap)
{
    if (pub_pem && cap) pub_pem[0] = 0;
    if (!priv_pem) return 0;
    BIO* bio = BIO_new_mem_buf(priv_pem, -1);
    EVP_PKEY* pk = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (!pk) return 0;
    BIO* out = BIO_new(BIO_s_mem());
    int ok = PEM_write_bio_PUBKEY(out, pk) == 1;
    if (ok) {
        char* p = NULL;
        long n = BIO_get_mem_data(out, &p);
        if (n > 0 && (size_t)n < cap) { memcpy(pub_pem, p, n); pub_pem[n] = 0; } else ok = 0;
    }
    BIO_free(out);
    EVP_PKEY_free(pk);
    return ok;
}

int gmb_pkg_ecdsa_keygen_pem(char* priv_pem, size_t pcap, char* pub_pem, size_t ucap)
{
    if (priv_pem && pcap) priv_pem[0] = 0;
    if (pub_pem && ucap) pub_pem[0] = 0;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (!ctx) return 0;
    EVP_PKEY* pk = NULL;
    int ok = EVP_PKEY_keygen_init(ctx) == 1
             && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1) == 1
             && EVP_PKEY_keygen(ctx, &pk) == 1;
    EVP_PKEY_CTX_free(ctx);
    if (!ok || !pk) return 0;
    BIO* po = BIO_new(BIO_s_mem());
    ok = PEM_write_bio_PrivateKey(po, pk, NULL, NULL, 0, NULL, NULL) == 1;
    if (ok) {
        char* p = NULL;
        long n = BIO_get_mem_data(po, &p);
        if (n > 0 && (size_t)n < pcap) { memcpy(priv_pem, p, n); priv_pem[n] = 0; } else ok = 0;
    }
    BIO_free(po);
    if (ok) {
        BIO* uo = BIO_new(BIO_s_mem());
        ok = PEM_write_bio_PUBKEY(uo, pk) == 1;
        if (ok) {
            char* p = NULL;
            long n = BIO_get_mem_data(uo, &p);
            if (n > 0 && (size_t)n < ucap) { memcpy(pub_pem, p, n); pub_pem[n] = 0; } else ok = 0;
        }
        BIO_free(uo);
    }
    EVP_PKEY_free(pk);
    return ok;
}
