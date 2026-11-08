/* bc_ffi.c — shim ABI per l'export BASIC `blockchain.inc`.
 *
 * Array di BYTE (2026-11-06): il bridge FFI non pinna un byte[] (elemento largo 1):
 * ogni elemento viaggia come intero a 8 byte nello scratch (valori 0..255) e qui
 * viene ridotto a uint8_t; le uscite riscrivono 0..255 nell'array del chiamante.
 * Le stringhe di ritorno sono `char*` malloc'd (il runtime FFI le copia e libera).
 */
#include "gmbblockchain.h"

#include <stdlib.h>
#include <string.h>

typedef long long i64;

/* elementi a 8 byte -> byte (troncamento a 8 bit: semantica BYTE). */
static uint8_t* bc_in(const int64_t* a, i64 n)
{
    uint8_t* b;
    i64 i;
    if (n < 0 || (n > 0 && a == NULL)) return NULL;
    b = (uint8_t*)malloc(n > 0 ? (size_t)n : 1);
    if (b == NULL) return NULL;
    for (i = 0; i < n; i++) b[i] = (uint8_t)(a[i] & 0xFF);
    return b;
}

/* byte -> elementi a 8 byte nel buffer del chiamante (cap elementi). */
static i64 bc_out(const uint8_t* b, i64 n, int64_t* out, i64 cap)
{
    i64 i;
    if (out == NULL || cap < n) return -1;
    for (i = 0; i < n; i++) out[i] = (int64_t)b[i];
    return n;
}

/* ── hashing ─────────────────────────────────────────────────────────────── */

i64 ofx_bc_keccak256(const int64_t* data, i64 len, int64_t* out, i64 cap)
{
    uint8_t* b = bc_in(data, len);
    uint8_t d[32];
    i64 r;
    if (b == NULL) return -1;
    gmb_keccak256(b, (size_t)len, d);
    free(b);
    r = bc_out(d, 32, out, cap);
    return r;
}

i64 ofx_bc_ripemd160(const int64_t* data, i64 len, int64_t* out, i64 cap)
{
    uint8_t* b = bc_in(data, len);
    uint8_t d[20];
    i64 r;
    if (b == NULL) return -1;
    gmb_ripemd160(b, (size_t)len, d);
    free(b);
    r = bc_out(d, 20, out, cap);
    return r;
}

i64 ofx_bc_blake2b256(const int64_t* data, i64 len, int64_t* out, i64 cap)
{
    uint8_t* b = bc_in(data, len);
    uint8_t d[32];
    i64 r;
    if (b == NULL) return -1;
    gmb_blake2b256(b, (size_t)len, d);
    free(b);
    r = bc_out(d, 32, out, cap);
    return r;
}

/* ── encoding ────────────────────────────────────────────────────────────── */

char* ofx_bc_base58encode(const int64_t* data, i64 len)
{
    uint8_t* b = bc_in(data, len);
    char* r;
    if (b == NULL) return NULL;
    r = gmb_base58_encode(b, (size_t)len);
    free(b);
    return r;
}

i64 ofx_bc_base58decode(const char* text, int64_t* out, i64 cap)
{
    size_t n = 0;
    uint8_t* b;
    i64 r;
    if (text == NULL) return -1;
    b = gmb_base58_decode(text, &n);
    if (b == NULL) return -1;
    r = bc_out(b, (i64)n, out, cap);
    free(b);
    return r;
}

char* ofx_bc_base32encode(const int64_t* data, i64 len)
{
    uint8_t* b = bc_in(data, len);
    char* r;
    if (b == NULL) return NULL;
    r = gmb_base32_encode(b, (size_t)len);
    free(b);
    return r;
}

i64 ofx_bc_base32decode(const char* text, int64_t* out, i64 cap)
{
    size_t n = 0;
    uint8_t* b;
    i64 r;
    if (text == NULL) return -1;
    b = gmb_base32_decode(text, &n);
    if (b == NULL) return -1;
    r = bc_out(b, (i64)n, out, cap);
    free(b);
    return r;
}

i64 ofx_bc_rlpencode(const int64_t* data, i64 len, int64_t* out, i64 cap)
{
    uint8_t* b = bc_in(data, len);
    uint8_t* enc;
    size_t outn = 0;
    i64 r;
    if (b == NULL) return -1;
    enc = gmb_rlp_encode(b, (size_t)len, &outn);
    free(b);
    if (enc == NULL) return -1;
    r = bc_out(enc, (i64)outn, out, cap);
    free(enc);
    return r;
}

i64 ofx_bc_varintencode(i64 value, int64_t* out, i64 cap)
{
    size_t n = 0;
    uint8_t* b = gmb_varint_encode((int64_t)value, &n);
    i64 r;
    if (b == NULL) return -1;
    r = bc_out(b, (i64)n, out, cap);
    free(b);
    return r;
}

i64 ofx_bc_varintdecode(const int64_t* data, i64 len)
{
    uint8_t* b = bc_in(data, len);
    size_t used = 0;
    int64_t v;
    if (b == NULL) return -1;
    v = gmb_varint_decode(b, (size_t)len, &used);
    free(b);
    return (i64)v;
}

/* ── indirizzi ───────────────────────────────────────────────────────────── */

char* ofx_bc_ethaddrfrompub(const int64_t* pub, i64 len)
{
    uint8_t* b = bc_in(pub, len);
    char* r;
    if (b == NULL) return NULL;
    r = gmb_eth_address_from_pub_hex(b, (size_t)len);
    free(b);
    return r;
}

char* ofx_bc_ethaddrchecksum(const char* addr)
{
    return gmb_eth_address_checksum(addr);
}

char* ofx_bc_btcaddrfrompub(const int64_t* pub, i64 len, i64 testnet)
{
    uint8_t* b = bc_in(pub, len);
    char* r;
    if (b == NULL) return NULL;
    r = gmb_btc_address_from_pub(b, (size_t)len, (int)testnet);
    free(b);
    return r;
}

/* ── difficolta' / firma ─────────────────────────────────────────────────── */

double ofx_bc_blockdifficulty(const int64_t* header, i64 len, i64 bits)
{
    uint8_t* b = bc_in(header, len);
    double d;
    if (b == NULL) return 0.0;
    d = gmb_block_difficulty(b, (size_t)len, (int32_t)bits);
    free(b);
    return d;
}

i64 ofx_bc_secp256k1sign(const int64_t* hash, i64 hlen, const int64_t* priv, i64 plen,
                         int64_t* out, i64 cap)
{
    uint8_t* h = bc_in(hash, hlen);
    uint8_t* p = bc_in(priv, plen);
    uint8_t der[128];
    size_t len;
    i64 r;
    if (h == NULL || p == NULL || hlen < 32 || plen < 32) { free(h); free(p); return -1; }
    len = gmb_secp256k1_sign(h, p, der, sizeof(der));
    free(h);
    free(p);
    if (len == 0) return -1;
    r = bc_out(der, (i64)len, out, cap);
    return r;
}

/* ── HD wallet (BIP39/BIP32) ─────────────────────────────────────────────── */

char* ofx_bc_generatemnemonic(i64 words)
{
    unsigned n = words <= 0 ? 12u : (unsigned)words;
    return gmb_bip39_generate_mnemonic(n);
}

i64 ofx_bc_mnemonicstoseed(const char* mnemonic, const char* pass, int64_t* out, i64 cap)
{
    uint8_t seed[64];
    if (out == NULL || cap < 64) return -1;
    gmb_bip39_mnemonic_to_seed(mnemonic, pass, seed);
    return bc_out(seed, 64, out, cap);
}

/* out = priv(32) || pub(33) || chainCode(32) = 97 elementi. */
i64 ofx_bc_derivekey(const int64_t* seed, i64 len, const char* path,
                     int64_t* out, i64 cap)
{
    uint8_t* s = bc_in(seed, len);
    uint8_t all[97], I[64];
    static const char k[] = "Bitcoin seed";
    i64 r;
    (void)path;   /* path non applicato: parita' con i backend (chiave master) */
    if (s == NULL) return -1;
    if (out == NULL || cap < 97) { free(s); return -1; }
    gmb_hmac_sha512((const uint8_t*)k, sizeof(k) - 1, s, (size_t)len, I);
    free(s);
    memcpy(all, I, 32);
    gmb_secp256k1_pubkey(I, all + 32);
    memcpy(all + 65, I + 32, 32);
    r = bc_out(all, 97, out, cap);
    return r;
}

i64 ofx_bc_getpublickey(const int64_t* priv, i64 len, int64_t* out, i64 cap)
{
    uint8_t* p = bc_in(priv, len);
    uint8_t pub[33];
    i64 r;
    if (p == NULL) return -1;
    gmb_secp256k1_pubkey(p, pub);
    free(p);
    r = bc_out(pub, 33, out, cap);
    return r;
}

/* Chiave pubblica NON compressa (65 byte): e' la forma che vuole ETHADDRFROMPUB. */
i64 ofx_bc_getpublickeyfull(const int64_t* priv, i64 len, int64_t* out, i64 cap)
{
    uint8_t* p = bc_in(priv, len);
    uint8_t pub[65];
    i64 r;
    if (p == NULL) return -1;
    if (len < 32) { free(p); return -1; }
    if (!gmb_secp256k1_pubkey_uncompressed(p, pub)) { free(p); return 0; }
    free(p);
    r = bc_out(pub, 65, out, cap);
    return r;
}

/* ── ABI EVM ─────────────────────────────────────────────────────────────── */

char* ofx_bc_abiencode(const char* sig, const double* nums, i64 n)
{
    if (sig == NULL || n < 0 || (n > 0 && nums == NULL)) return NULL;
    return gmb_abi_encode(sig, nums, (int)n, NULL, NULL);
}

i64 ofx_bc_abidecode(const char* hex_in, double* out, i64 cap)
{
    int count = 0;
    double* r;
    if (hex_in == NULL || out == NULL) return -1;
    r = gmb_abi_decode_doubles(hex_in, &count);
    if (r == NULL) return 0;
    if ((i64)count > cap) { free(r); return -1; }
    memcpy(out, r, (size_t)count * sizeof(double));
    free(r);
    return (i64)count;
}
