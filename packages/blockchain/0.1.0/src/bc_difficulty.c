/* gmb_difficulty.c — difficoltà di blocco (parità IL/PCodeVM BlockchainUtils).
 *
 * ESTRATTO da gmb_encoding.c: quel file è trascinato in OGNI dylib che usa base64/encoding,
 * e con lui entrava OpenSSL (BIGNUM) → `libgmb_gfx` dipendeva da libcrypto.3 (4,6 MB) per
 * una funzione che serve SOLO alla blockchain. Un file per dominio (§0quater AGENTS).
 */
#include "gmbblockchain.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#if defined(HAVE_OPENSSL)
#include <openssl/bn.h>
#include <openssl/crypto.h>
#endif

/* ==================== Block difficulty (parità IL/PCodeVM BlockchainUtils) ====================
 * IL: target = mantissa * 256^(exponent-3); hash = SHA256(SHA256(header)) big-endian;
 * risultato = target / hash (divisione intera BigInteger, resa come double). Prima usava
 * la formula standard maxTarget/target (gap noto vs IL/VM/C). */
double gmb_block_difficulty(const uint8_t* header, size_t len, int32_t bits) {
    int exponent = (bits >> 24) & 0xFF;
    uint32_t mantissa = (uint32_t)(bits & 0x00FFFFFF);
    uint8_t h1[32], h2[32];
    gmb_sha256(header, len, h1);
    gmb_sha256(h1, 32, h2);
#if defined(HAVE_OPENSSL)
    {
        BN_CTX* ctx = BN_CTX_new();
        BIGNUM* target = BN_new();
        BIGNUM* hash = BN_new();
        BIGNUM* res = BN_new();
        double d = 0.0;
        if (ctx && target && hash && res) {
            BN_set_word(target, mantissa);
            if (exponent >= 3) BN_lshift(target, target, 8 * (exponent - 3));
            else BN_rshift(target, target, 8 * (3 - exponent));
            BN_bin2bn(h2, 32, hash);
            if (!BN_is_zero(hash) && BN_div(res, NULL, target, hash, ctx) == 1) {
                char* s = BN_bn2dec(res);
                if (s) { d = strtod(s, NULL); free(s); }   /* CRYPTO_free usa l'allocatore di default (malloc) */
            }
        }
        BN_free(target); BN_free(hash); BN_free(res); BN_CTX_free(ctx);
        return d;
    }
#else
    /* Fallback senza OpenSSL: stima double (il build canonico ha OpenSSL). */
    double target = (exponent >= 3)
        ? (double)mantissa * pow(256.0, exponent - 3)
        : (double)mantissa / pow(256.0, 3 - exponent);
    double hash = 0.0;
    for (int i = 0; i < 32; i++) hash = hash * 256.0 + h2[i];
    return hash > 0.0 ? target / hash : 0.0;
#endif
}

