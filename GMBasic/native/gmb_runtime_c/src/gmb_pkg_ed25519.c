/* gmb_pkg_ed25519.c — firme Ed25519 (riferimento C del Package Manager, via OpenSSL).
 * Compatibile RFC 8032 (stessa firma di BouncyCastle/C#). */
#include "gmb_pkg_ed25519.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

static int b64_dec(const char* in, unsigned char* out, size_t cap, size_t* outlen)
{
    int val = 0, bits = -8;
    size_t ol = 0;
    if (outlen) *outlen = 0;
    for (const char* p = in; p && *p; p++) {
        char c = *p;
        if (c == '=') break;
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        const char* q = strchr(B64, c);
        if (!q) return 0;
        val = (val << 6) | (int)(q - B64);
        bits += 6;
        if (bits >= 0) { if (ol >= cap) return 0; out[ol++] = (unsigned char)((val >> bits) & 0xff); bits -= 8; }
    }
    if (outlen) *outlen = ol;
    return 1;
}

int gmb_pkg_ed25519_sign(const char* private_b64, const unsigned char* data, size_t len, char* sig_b64, size_t cap)
{
    if (sig_b64 && cap) sig_b64[0] = 0;
    unsigned char seed[64];
    size_t sl = 0;
    if (!private_b64 || !b64_dec(private_b64, seed, sizeof(seed), &sl) || sl != 32) return 0;
    EVP_PKEY* pk = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, seed, 32);
    if (!pk) return 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    int ok = 0;
    if (ctx && EVP_DigestSignInit(ctx, NULL, NULL, NULL, pk) == 1) {
        unsigned char sig[64];
        size_t siglen = sizeof(sig);
        if (EVP_DigestSign(ctx, sig, &siglen, data, len) == 1) { b64_enc(sig, siglen, sig_b64, cap); ok = 1; }
    }
    if (ctx) EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pk);
    return ok;
}

int gmb_pkg_ed25519_public(const char* private_b64, char* pub_b64, size_t cap)
{
    if (pub_b64 && cap) pub_b64[0] = 0;
    unsigned char seed[64];
    size_t sl = 0;
    if (!private_b64 || !b64_dec(private_b64, seed, sizeof(seed), &sl) || sl != 32) return 0;
    EVP_PKEY* pk = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, seed, 32);
    if (!pk) return 0;
    unsigned char pub[32];
    size_t publen = sizeof(pub);
    int ok = EVP_PKEY_get_raw_public_key(pk, pub, &publen) == 1;
    if (ok) b64_enc(pub, publen, pub_b64, cap);
    EVP_PKEY_free(pk);
    return ok;
}

int gmb_pkg_ed25519_keygen(char* priv_b64, size_t pcap, char* pub_b64, size_t ucap)
{
    if (priv_b64 && pcap) priv_b64[0] = 0;
    if (pub_b64 && ucap) pub_b64[0] = 0;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if (!ctx) return 0;
    EVP_PKEY* pk = NULL;
    int ok = EVP_PKEY_keygen_init(ctx) == 1 && EVP_PKEY_keygen(ctx, &pk) == 1;
    EVP_PKEY_CTX_free(ctx);
    if (!ok || !pk) return 0;
    unsigned char seed[32], pub[32];
    size_t sl = sizeof(seed), pl = sizeof(pub);
    ok = EVP_PKEY_get_raw_private_key(pk, seed, &sl) == 1 && EVP_PKEY_get_raw_public_key(pk, pub, &pl) == 1;
    if (ok) { b64_enc(seed, sl, priv_b64, pcap); b64_enc(pub, pl, pub_b64, ucap); }
    EVP_PKEY_free(pk);
    return ok;
}

int gmb_pkg_ed25519_verify(const char* public_key, const unsigned char* data, size_t len, const char* sig_b64)
{
    if (!public_key || !sig_b64) return 0;
    unsigned char sig[256];
    size_t sigl = 0;
    if (!b64_dec(sig_b64, sig, sizeof(sig), &sigl)) return 0;

    /* Ed25519: pub base64 32B */
    unsigned char pubb[64];
    size_t pl = 0;
    if (b64_dec(public_key, pubb, sizeof(pubb), &pl) && pl == 32) {
        EVP_PKEY* pk = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pubb, 32);
        if (!pk) return 0;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        int ok = ctx && EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pk) == 1
                 && EVP_DigestVerify(ctx, sig, sigl, data, len) == 1;
        if (ctx) EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pk);
        return ok ? 1 : 0;
    }

    /* fallback ECDSA PEM (sig DER) */
    if (strstr(public_key, "BEGIN")) {
        BIO* bio = BIO_new_mem_buf(public_key, -1);
        if (!bio) return 0;
        EVP_PKEY* pk = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
        BIO_free(bio);
        if (!pk) return 0;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        int ok = ctx && EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pk) == 1
                 && EVP_DigestVerify(ctx, sig, sigl, data, len) == 1;
        if (ctx) EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pk);
        return ok ? 1 : 0;
    }
    return 0;
}
