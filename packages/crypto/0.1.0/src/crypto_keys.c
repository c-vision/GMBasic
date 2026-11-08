/* ============================================================================
 * gmb_keys.c — Registro di chiavi crittografiche (ECDSA secp256k1, RSA).
 *
 * Backend OpenSSL (EVP) per generazione, firma/verifica e cifratura asimmetrica.
 * Gli handle sono interi opachi assegnati dal registro; il C# CryptoKeyRegistry
 * usa lo stesso modello (Store/Get/Close).
 * ============================================================================ */
#include "gmbcrypto.h"
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#if defined(HAVE_OPENSSL)
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/objects.h>
#include <openssl/err.h>
#include <openssl/sha.h>

/* dichiarato in gmb_runtime.h (gmb_encoding.c) */
static uint8_t* keys_b64_decode(const char* input, size_t* len_out);
#include <openssl/rand.h>
#endif

#define GMB_MAX_KEYS 256

#if defined(HAVE_OPENSSL)
static EVP_PKEY* gmb_key_store[GMB_MAX_KEYS] = {0};
static unsigned char gmb_key_private[GMB_MAX_KEYS] = {0};
#else
static void* gmb_key_store[GMB_MAX_KEYS] = {0};
static unsigned char gmb_key_private[GMB_MAX_KEYS] = {0};
#endif

/* Genera una coppia di chiavi. algo: "ECDSA" → secp256k1, "RSA" → RSA (bits).
   Ritorna 0 su successo; pvt/pub sono handle (>=1) da usare con le altre funzioni. */
int ofx_crypto_keys_generate(const char* algo, int bits, int* pvt_out, int* pub_out) {
#if defined(HAVE_OPENSSL)
    EVP_PKEY_CTX* ctx = NULL;
    EVP_PKEY* pkey = NULL;
    *pvt_out = 0; *pub_out = 0;
    if (!algo) algo = "RSA";
    if (strcmp(algo, "ECDSA") == 0) {
        ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
        if (!ctx) return -1;
        if (EVP_PKEY_keygen_init(ctx) <= 0) { EVP_PKEY_CTX_free(ctx); return -1; }
        if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_secp256k1) <= 0) { EVP_PKEY_CTX_free(ctx); return -1; }
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) { EVP_PKEY_CTX_free(ctx); return -1; }
    } else {
        if (bits <= 0) bits = 2048;
        ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
        if (!ctx) return -1;
        if (EVP_PKEY_keygen_init(ctx) <= 0) { EVP_PKEY_CTX_free(ctx); return -1; }
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0) { EVP_PKEY_CTX_free(ctx); return -1; }
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) { EVP_PKEY_CTX_free(ctx); return -1; }
    }
    EVP_PKEY_CTX_free(ctx);
    /* registra chiave privata e pubblica */
    int priv_h = 0, pub_h = 0;
    for (int i = 0; i < GMB_MAX_KEYS; i++) if (!gmb_key_store[i]) { priv_h = i + 1; break; }
    for (int i = 0; i < GMB_MAX_KEYS; i++) if (!gmb_key_store[i] && (i + 1) != priv_h) { pub_h = i + 1; break; }
    if (!priv_h || !pub_h) { EVP_PKEY_free(pkey); return -1; }
    gmb_key_store[priv_h-1] = pkey;
    gmb_key_private[priv_h-1] = 1;
    /* chiave pubblica SEPARATA: deriva i parametri pubblici dalla privata
       (due handle devono puntare a EVP_PKEY distinti — niente double-free) */
    EVP_PKEY* pub = NULL;
    if (strcmp(algo, "ECDSA") == 0) {
        EC_KEY* ec = EVP_PKEY_get1_EC_KEY(pkey);
        if (ec) {
            const EC_POINT* Q = EC_KEY_get0_public_key(ec);
            const EC_GROUP* grp = EC_KEY_get0_group(ec);
            EC_KEY* pub_ec = EC_KEY_new();
            if (pub_ec && grp) {
                if (EC_KEY_set_group(pub_ec, grp) == 1 && EC_KEY_set_public_key(pub_ec, Q) == 1)
                    pub = EVP_PKEY_new();
                if (pub) EVP_PKEY_assign_EC_KEY(pub, pub_ec);
                else EC_KEY_free(pub_ec);
            } else if (pub_ec) EC_KEY_free(pub_ec);
            EC_KEY_free(ec);
        }
    } else {
        RSA* rsa = EVP_PKEY_get1_RSA(pkey);
        if (rsa) {
            RSA* pub_rsa = RSAPublicKey_dup(rsa);
            RSA_free(rsa);
            if (pub_rsa) {
                pub = EVP_PKEY_new();
                if (pub) EVP_PKEY_assign_RSA(pub, pub_rsa);
                else RSA_free(pub_rsa);
            }
        }
    }
    if (!pub) { EVP_PKEY_free(pkey); gmb_key_store[priv_h-1] = NULL; return -1; }
    gmb_key_store[pub_h-1] = pub;
    gmb_key_private[pub_h-1] = 0;
    *pvt_out = priv_h;
    *pub_out = pub_h;
    return 0;
#else
    (void)algo; (void)bits; *pvt_out = 0; *pub_out = 0; return -1;
#endif
}

void ofx_crypto_keys_close(int handle) {
    if (handle < 1 || handle > GMB_MAX_KEYS) return;
#if defined(HAVE_OPENSSL)
    EVP_PKEY_free((EVP_PKEY*)gmb_key_store[handle-1]);
#endif
    gmb_key_store[handle-1] = NULL;
    gmb_key_private[handle-1] = 0;
}

int ofx_crypto_keys_is_private(int handle) {
    if (handle < 1 || handle > GMB_MAX_KEYS) return 0;
    return gmb_key_private[handle-1];
}

/* Firma dati (SHA-256, ECDSA secp256k1): ritorna allocato, len_out. */
uint8_t* ofx_crypto_keys_sign(int handle, const uint8_t* data, size_t len, size_t* len_out) {
    *len_out = 0;
#if defined(HAVE_OPENSSL)
    if (handle < 1 || handle > GMB_MAX_KEYS || !gmb_key_store[handle-1]) return NULL;
    EVP_PKEY* key = (EVP_PKEY*)gmb_key_store[handle-1];
    EVP_MD_CTX* mctx = EVP_MD_CTX_new();
    if (!mctx) return NULL;
    uint8_t digest[32];
    unsigned int dlen = 0;
    if (EVP_DigestInit_ex(mctx, EVP_sha256(), NULL) <= 0) { EVP_MD_CTX_free(mctx); return NULL; }
    if (EVP_DigestUpdate(mctx, data, len) <= 0) { EVP_MD_CTX_free(mctx); return NULL; }
    if (EVP_DigestFinal_ex(mctx, digest, &dlen) <= 0) { EVP_MD_CTX_free(mctx); return NULL; }
    EVP_MD_CTX_free(mctx);

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new(key, NULL);
    if (!pctx) return NULL;
    size_t siglen = 0;
    if (EVP_PKEY_sign_init(pctx) <= 0) { EVP_PKEY_CTX_free(pctx); return NULL; }
    if (EVP_PKEY_CTX_set_signature_md(pctx, EVP_sha256()) <= 0) { EVP_PKEY_CTX_free(pctx); return NULL; }
    if (EVP_PKEY_sign(pctx, NULL, &siglen, digest, dlen) <= 0) { EVP_PKEY_CTX_free(pctx); return NULL; }
    uint8_t* sig = (uint8_t*)malloc(siglen);
    if (!sig) { EVP_PKEY_CTX_free(pctx); return NULL; }
    if (EVP_PKEY_sign(pctx, sig, &siglen, digest, dlen) <= 0) { EVP_PKEY_CTX_free(pctx); free(sig); return NULL; }
    EVP_PKEY_CTX_free(pctx);
    *len_out = siglen;
    return sig;
#else
    (void)handle; (void)data; (void)len; return NULL;
#endif
}

int ofx_crypto_keys_verify(int handle, const uint8_t* data, size_t len, const uint8_t* sig, size_t siglen) {
#if defined(HAVE_OPENSSL)
    if (handle < 1 || handle > GMB_MAX_KEYS || !gmb_key_store[handle-1]) return 0;
    EVP_PKEY* key = (EVP_PKEY*)gmb_key_store[handle-1];
    EVP_MD_CTX* mctx = EVP_MD_CTX_new();
    if (!mctx) return 0;
    uint8_t digest[32];
    unsigned int dlen = 0;
    if (EVP_DigestInit_ex(mctx, EVP_sha256(), NULL) <= 0) { EVP_MD_CTX_free(mctx); return 0; }
    if (EVP_DigestUpdate(mctx, data, len) <= 0) { EVP_MD_CTX_free(mctx); return 0; }
    if (EVP_DigestFinal_ex(mctx, digest, &dlen) <= 0) { EVP_MD_CTX_free(mctx); return 0; }
    EVP_MD_CTX_free(mctx);
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new(key, NULL);
    if (!pctx) return 0;
    if (EVP_PKEY_verify_init(pctx) <= 0) { EVP_PKEY_CTX_free(pctx); return 0; }
    if (EVP_PKEY_CTX_set_signature_md(pctx, EVP_sha256()) <= 0) { EVP_PKEY_CTX_free(pctx); return 0; }
    int rc = EVP_PKEY_verify(pctx, sig, siglen, digest, dlen);
    EVP_PKEY_CTX_free(pctx);
    return rc == 1;
#else
    (void)handle; (void)data; (void)len; (void)sig; (void)siglen; return 0;
#endif
}

/* Cifratura asimmetrica (RSA-OAEP-SHA256 con chiave pubblica): ritorna allocato. */
uint8_t* ofx_crypto_keys_encrypt(int handle, const uint8_t* data, size_t len, size_t* len_out) {
    *len_out = 0;
#if defined(HAVE_OPENSSL)
    if (handle < 1 || handle > GMB_MAX_KEYS || !gmb_key_store[handle-1]) return NULL;
    EVP_PKEY* key = (EVP_PKEY*)gmb_key_store[handle-1];
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) return NULL;
    if (EVP_PKEY_encrypt_init(ctx) <= 0) { EVP_PKEY_CTX_free(ctx); return NULL; }
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) { EVP_PKEY_CTX_free(ctx); return NULL; }
    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) { EVP_PKEY_CTX_free(ctx); return NULL; }
    size_t outlen = 0;
    if (EVP_PKEY_encrypt(ctx, NULL, &outlen, data, len) <= 0) { EVP_PKEY_CTX_free(ctx); return NULL; }
    uint8_t* out = (uint8_t*)malloc(outlen);
    if (!out) { EVP_PKEY_CTX_free(ctx); return NULL; }
    if (EVP_PKEY_encrypt(ctx, out, &outlen, data, len) <= 0) { EVP_PKEY_CTX_free(ctx); free(out); return NULL; }
    EVP_PKEY_CTX_free(ctx);
    *len_out = outlen;
    return out;
#else
    (void)handle; (void)data; (void)len; return NULL;
#endif
}

uint8_t* ofx_crypto_keys_decrypt(int handle, const uint8_t* data, size_t len, size_t* len_out) {
    *len_out = 0;
#if defined(HAVE_OPENSSL)
    if (handle < 1 || handle > GMB_MAX_KEYS || !gmb_key_store[handle-1]) return NULL;
    EVP_PKEY* key = (EVP_PKEY*)gmb_key_store[handle-1];
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, NULL);
    if (!ctx) return NULL;
    if (EVP_PKEY_decrypt_init(ctx) <= 0) { EVP_PKEY_CTX_free(ctx); return NULL; }
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0) { EVP_PKEY_CTX_free(ctx); return NULL; }
    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0) { EVP_PKEY_CTX_free(ctx); return NULL; }
    size_t outlen = 0;
    if (EVP_PKEY_decrypt(ctx, NULL, &outlen, data, len) <= 0) { EVP_PKEY_CTX_free(ctx); return NULL; }
    uint8_t* out = (uint8_t*)malloc(outlen ? outlen : 1);
    if (!out) { EVP_PKEY_CTX_free(ctx); return NULL; }
    if (EVP_PKEY_decrypt(ctx, out, &outlen, data, len) <= 0) { EVP_PKEY_CTX_free(ctx); free(out); return NULL; }
    EVP_PKEY_CTX_free(ctx);
    *len_out = outlen;
    return out;
#else
    (void)handle; (void)data; (void)len; return NULL;
#endif
}

/* Esporta la chiave in PEM (stringa allocata). */
char* ofx_crypto_keys_export_pem(int handle) {
#if defined(HAVE_OPENSSL)
    if (handle < 1 || handle > GMB_MAX_KEYS || !gmb_key_store[handle-1]) return NULL;
    EVP_PKEY* key = (EVP_PKEY*)gmb_key_store[handle-1];
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return NULL;
    if (gmb_key_private[handle-1]) PEM_write_bio_PrivateKey(bio, key, NULL, NULL, 0, NULL, NULL);
    else PEM_write_bio_PUBKEY(bio, key);
    char* pem = NULL;
    long plen = BIO_get_mem_data(bio, &pem);
    char* out = (char*)malloc((size_t)plen + 1);
    if (out) { memcpy(out, pem, (size_t)plen); out[plen] = 0; }
    BIO_free(bio);
    return out;
#else
    (void)handle; return NULL;
#endif
}

/* Importa una chiave da PEM; ritorna nuovo handle (0 su errore). */
int ofx_crypto_keys_import_pem(const char* pem) {
#if defined(HAVE_OPENSSL)
    if (!pem) return 0;
    BIO* bio = BIO_new_mem_buf(pem, -1);
    if (!bio) return 0;
    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    int is_priv = 1;
    if (!key) {
        BIO_free(bio);
        bio = BIO_new_mem_buf(pem, -1);
        key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
        is_priv = 0;
    }
    BIO_free(bio);
    if (!key) return 0;
    for (int i = 0; i < GMB_MAX_KEYS; i++) if (!gmb_key_store[i]) {
        gmb_key_store[i] = key;
        gmb_key_private[i] = (unsigned char)is_priv;
        return i + 1;
    }
    EVP_PKEY_free(key);
    return 0;
#else
    (void)pem; return 0;
#endif
}

/* VERIFYRAW(signature, data, publicKeyB64): verifica firma Ed25519 (chiave pubblica
   raw 32B base64) o ECDSA/RSA (PEM SubjectPublicKeyInfo) — port 1:1 di
   PackageBuilder.VerifyData (C#). Ritorna 1 se la firma è valida. */
int ofx_crypto_keys_verify_raw(const uint8_t* data, size_t len, const uint8_t* sig, size_t siglen, const char* pub_b64)
{
#if defined(HAVE_OPENSSL)
    if (!pub_b64 || !*pub_b64) return 0;
    size_t pub_len = 0;
    uint8_t* pub = keys_b64_decode(pub_b64, &pub_len);
    if (!pub) return 0;
    EVP_PKEY* key = NULL;
    int ok = 0;
    if (pub_len == 32)
    {
        key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub, pub_len);
        if (key)
        {
            EVP_MD_CTX* mctx = EVP_MD_CTX_new();
            if (mctx && EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, key) == 1 &&
                EVP_DigestVerify(mctx, sig, siglen, data, len) == 1) ok = 1;
            if (mctx) EVP_MD_CTX_free(mctx);
            EVP_PKEY_free(key);
        }
    }
    else
    {
        BIO* bio = BIO_new_mem_buf(pub_b64, -1);
        key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
        BIO_free(bio);
        if (key)
        {
            EVP_MD_CTX* mctx = EVP_MD_CTX_new();
            if (mctx && EVP_DigestVerifyInit(mctx, NULL, EVP_sha256(), NULL, key) == 1 &&
                EVP_DigestVerify(mctx, sig, siglen, data, len) == 1) ok = 1;
            if (mctx) EVP_MD_CTX_free(mctx);
            EVP_PKEY_free(key);
        }
    }
    free(pub);
    return ok;
#else
    (void)data; (void)len; (void)sig; (void)siglen; (void)pub_b64; return 0;
#endif
}


/* Base64 → byte: riusa il decoder del pacchetto (ofx_crypto_base64_decode).
   Ritorna un buffer malloc'd e la lunghezza; NULL se l'input non e' Base64 valido. */
static uint8_t* keys_b64_decode(const char* input, size_t* len_out)
{
    size_t cap;
    int n;
    uint8_t* buf;
    *len_out = 0;
    if (!input || !*input) return NULL;
    cap = strlen(input) / 4 * 3 + 3;
    buf = (uint8_t*)malloc(cap);
    if (!buf) return NULL;
    n = ofx_crypto_base64_decode(input, buf, cap);
    if (n <= 0) { free(buf); return NULL; }
    *len_out = (size_t)n;
    return buf;
}
