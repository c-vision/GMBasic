/* bc_sign.c — firma ECDSA secp256k1 (DER) su hash gia' calcolato, DETERMINISTICA.
 *
 * OpenSSL e' l'unica dipendenza esterna della lib (motivo per cui il dominio e'
 * distribuito anche come PACCHETTO: le lib di fondazione gmbsuite non hanno
 * dipendenze esterne).
 *
 * PERCHE' DETERMINISTICA: la parita' fra le quattro gambe confronta l'output byte
 * per byte; ECDSA_do_sign estrae una `k` casuale, quindi due processi producono
 * firme diverse per lo stesso (chiave, hash) e la parita' dei sample Crypto non
 * poteva essere byte-identica. Qui la `k` e' derivata da (chiave privata, hash)
 * con HMAC-SHA256 secondo RFC 6979 §3.2 e la S e' normalizzata low-S (convenzione
 * Bitcoin/Ethereum): stessa chiave + stesso hash ⇒ STESSA firma DER, su ogni
 * backend. La firma viene poi verificata con ECDSA_do_verify prima di essere
 * restituita: se la verifica fallisce la funzione ritorna 0 (errore esplicito,
 * mai una firma sospetta).
 */
#include "gmbblockchain.h"

#include <string.h>

#if defined(GMB_BC_HAVE_OPENSSL)
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/obj_mac.h>

/* HMAC-SHA256 su 32 byte di output (le chiavi/dati qui sono sempre 32 byte o poco piu'). */
static int bc_hmac_sha256(const unsigned char key[32], const unsigned char* data, size_t datalen,
                          unsigned char out[32])
{
    unsigned int outlen = 0;
    unsigned char* r = HMAC(EVP_sha256(), key, 32, data, datalen, out, &outlen);
    return r != NULL && outlen == 32;
}

/* RFC 6979 §3.2 con qlen = hlen = 256 bit: k deterministica in [1, n-1]. */
static int bc_rfc6979_k(const BIGNUM* order, const unsigned char x[32], const unsigned char h1[32],
                        BIGNUM** k_out)
{
    unsigned char V[32], K[32], buf[97];
    int ok = 0;
    int tries;

    *k_out = NULL;
    memset(V, 0x01, sizeof V);
    memset(K, 0x00, sizeof K);

    /* K = HMAC_K(V || 0x00 || x || h1) ; V = HMAC_K(V) */
    memcpy(buf, V, 32); buf[32] = 0x00; memcpy(buf + 33, x, 32); memcpy(buf + 65, h1, 32);
    if (!bc_hmac_sha256(K, buf, 97, K)) return 0;
    if (!bc_hmac_sha256(K, V, 32, V)) return 0;

    /* K = HMAC_K(V || 0x01 || x || h1) ; V = HMAC_K(V) */
    memcpy(buf, V, 32); buf[32] = 0x01; memcpy(buf + 33, x, 32); memcpy(buf + 65, h1, 32);
    if (!bc_hmac_sha256(K, buf, 97, K)) return 0;
    if (!bc_hmac_sha256(K, V, 32, V)) return 0;

    for (tries = 0; tries < 1000; tries++) {
        BIGNUM* k;
        /* V = HMAC_K(V) ; k = bits2int(V) */
        if (!bc_hmac_sha256(K, V, 32, V)) return 0;
        k = BN_bin2bn(V, 32, NULL);
        if (k == NULL) return 0;
        if (!BN_is_zero(k) && BN_cmp(k, order) < 0) { *k_out = k; ok = 1; break; }
        BN_free(k);
        /* K = HMAC_K(V || 0x00) ; V = HMAC_K(V) */
        memcpy(buf, V, 32); buf[32] = 0x00;
        if (!bc_hmac_sha256(K, buf, 33, K)) return 0;
        if (!bc_hmac_sha256(K, V, 32, V)) return 0;
    }
    return ok;
}

size_t gmb_secp256k1_sign(const uint8_t hash[32], const uint8_t priv[32],
                          uint8_t* der, size_t cap)
{
    EC_KEY* key = NULL;
    const EC_GROUP* group = NULL;
    const BIGNUM* order = NULL;
    EC_POINT* R = NULL;
    EC_POINT* Q = NULL;
    BN_CTX* ctx = NULL;
    BIGNUM* d = NULL;
    BIGNUM* k = NULL;
    BIGNUM* e = NULL;
    BIGNUM* r = NULL;
    BIGNUM* s = NULL;
    BIGNUM* kinv = NULL;
    BIGNUM* tmp = NULL;
    BIGNUM* ns = NULL;
    ECDSA_SIG* sig = NULL;
    size_t out_len = 0;
    unsigned char* p = NULL;
    int n = 0;

    if (hash == NULL || priv == NULL || der == NULL || cap == 0) return 0;
    key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (key == NULL) return 0;
    group = EC_KEY_get0_group(key);
    if (group == NULL) goto done;
    order = EC_GROUP_get0_order(group);
    if (order == NULL) goto done;

    d = BN_bin2bn(priv, 32, NULL);
    if (d == NULL || BN_is_zero(d) || BN_cmp(d, order) >= 0) goto done;
    /* la chiave pubblica serve per la verifica finale (Q = d*G) */
    if (EC_KEY_set_private_key(key, d) <= 0) goto done;
    Q = EC_POINT_new(group);
    ctx = BN_CTX_new();
    if (Q == NULL || ctx == NULL) goto done;
    if (EC_POINT_mul(group, Q, d, NULL, NULL, ctx) != 1) goto done;
    if (EC_KEY_set_public_key(key, Q) <= 0) goto done;

    if (!bc_rfc6979_k(order, priv, hash, &k)) goto done;

    R = EC_POINT_new(group);
    e = BN_bin2bn(hash, 32, NULL);
    r = BN_new();
    s = BN_new();
    kinv = BN_new();
    tmp = BN_new();
    ns = BN_new();
    if (R == NULL || e == NULL || r == NULL || s == NULL || kinv == NULL || tmp == NULL || ns == NULL)
        goto done;

    /* r = (k*G).x mod n */
    if (EC_POINT_mul(group, R, k, NULL, NULL, ctx) != 1) goto done;
    if (EC_POINT_get_affine_coordinates(group, R, r, NULL, ctx) != 1) goto done;
    if (BN_nnmod(r, r, order, ctx) != 1) goto done;
    if (BN_is_zero(r)) goto done;

    /* s = k^-1 * (e + r*d) mod n */
    if (BN_mod_inverse(kinv, k, order, ctx) == NULL) goto done;
    if (BN_mod_mul(tmp, r, d, order, ctx) != 1) goto done;
    if (BN_mod_add(tmp, tmp, e, order, ctx) != 1) goto done;
    if (BN_mod_mul(s, kinv, tmp, order, ctx) != 1) goto done;
    if (BN_is_zero(s)) goto done;

    /* low-S: se s > n/2 allora s = n - s (firma canonica, stessa su tutte le gambe) */
    if (BN_cmp(s, order) > 0) goto done;
    if (BN_sub(ns, order, s) != 1) goto done;
    if (BN_cmp(s, ns) > 0) {
        BIGNUM* swap = s;
        s = ns;
        ns = swap;
    }

    sig = ECDSA_SIG_new();
    if (sig == NULL) goto done;
    if (ECDSA_SIG_set0(sig, r, s) != 1) goto done;
    r = NULL;   /* ownership trasferita alla firma */
    s = NULL;

    /* la firma calcolata a mano DEVE verificare: altrimenti errore esplicito */
    if (ECDSA_do_verify(hash, 32, sig, key) != 1) goto done;

    n = i2d_ECDSA_SIG(sig, NULL);
    if (n <= 0 || (size_t)n > cap) goto done;
    p = der;
    n = i2d_ECDSA_SIG(sig, &p);
    if (n > 0) out_len = (size_t)n;

done:
    if (sig != NULL) ECDSA_SIG_free(sig);
    if (r != NULL) BN_free(r);
    if (s != NULL) BN_free(s);
    if (ns != NULL) BN_free(ns);
    if (tmp != NULL) BN_free(tmp);
    if (kinv != NULL) BN_free(kinv);
    if (e != NULL) BN_free(e);
    if (k != NULL) BN_free(k);
    if (d != NULL) BN_free(d);
    if (R != NULL) EC_POINT_free(R);
    if (Q != NULL) EC_POINT_free(Q);
    if (ctx != NULL) BN_CTX_free(ctx);
    if (key != NULL) EC_KEY_free(key);
    return out_len;
}
#else
size_t gmb_secp256k1_sign(const uint8_t hash[32], const uint8_t priv[32],
                          uint8_t* der, size_t cap)
{
    (void)hash; (void)priv; (void)der; (void)cap;
    return 0;   /* build senza OpenSSL: la firma non e' disponibile (errore esplicito) */
}
#endif
