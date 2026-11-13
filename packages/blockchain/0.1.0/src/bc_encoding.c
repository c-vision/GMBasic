/* bc_encoding.c — estratti di encoding (hex, base58, base32, blake2b, varint, RLP, ABI).
 * Derivati da GMBasic gmb_encoding.c; nessuna dipendenza dal runtime. */
#include "gmbblockchain.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char BASE32_ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

void gmb_hex_encode(const uint8_t* data, size_t len, char* out) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i*2] = hex[data[i] >> 4];
        out[i*2+1] = hex[data[i] & 0x0f];
    }
}

char* gmb_hex_encode_string(const uint8_t* data, size_t len) {
    char* out = (char*)malloc(len*2 + 1);
    if (!out) return NULL;
    gmb_hex_encode(data, len, out);
    out[len*2] = '\0';
    return out;
}

/* ------------------------- Base58 ------------------------- */

static const char BASE58_ALPHA[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
static int base58_index(char c) {
    for (int i = 0; i < 58; i++) if (BASE58_ALPHA[i] == c) return i;
    return -1;
}

char* gmb_base58_encode(const uint8_t* data, size_t len) {
    // algoritmo classico: converti il numero big-endian in base 58
    size_t zeros = 0;
    while (zeros < len && data[zeros] == 0) zeros++;
    // buffer base58 (2 caratteri per byte in media)
    size_t cap = len * 2 + 1;
    uint8_t* digits = (uint8_t*)calloc(cap, 1);
    if (!digits) return NULL;
    size_t digits_len = 0;
    for (size_t i = zeros; i < len; i++) {
        int carry = data[i];
        size_t j = 0;
        for (size_t k = 0; k < digits_len || carry; k++) {
            int sum = (int)digits[k] * 256 + carry;
            digits[k] = (uint8_t)(sum % 58);
            carry = sum / 58;
            j = k + 1;
        }
        digits_len = j;
    }
    size_t out_len = zeros + digits_len;
    char* out = (char*)malloc(out_len + 1);
    if (!out) { free(digits); return NULL; }
    size_t p = 0;
    for (size_t i = 0; i < zeros; i++) out[p++] = '1';
    for (size_t i = digits_len; i > 0; i--) out[p++] = BASE58_ALPHA[digits[i-1]];
    out[p] = '\0';
    free(digits);
    return out;
}

uint8_t* gmb_base58_decode(const char* input, size_t* len_out) {
    size_t in_len = strlen(input);
    // contatore '1' iniziali
    size_t zeros = 0;
    while (zeros < in_len && input[zeros] == '1') zeros++;
    uint8_t* out = (uint8_t*)calloc(in_len, 1);
    if (!out) return NULL;
    size_t out_len = 0;
    for (size_t i = zeros; i < in_len; i++) {
        int digit = base58_index(input[i]);
        if (digit < 0) { free(out); return NULL; }
        int carry = digit;
        size_t j = 0;
        for (size_t k = 0; k < out_len || carry; k++) {
            int sum = (k < out_len ? out[k] : 0) * 58 + carry;
            out[k] = (uint8_t)(sum & 0xff);
            carry = sum >> 8;
            j = k + 1;
        }
        out_len = j;
    }
    // risultato little-endian → inverti, aggiungi gli zeri iniziali.
    // NB: il ciclo bignum puo' lasciare byte a ZERO in coda (aggiunti solo per il carry):
    // vanno scartati, altrimenti il decodificato ha un byte in piu' (misurato: 21 byte per
    // un input di 20, con 0x00 finale). Gli zeri "veri" sono solo quelli dei '1' iniziali.
    while (out_len > 0 && out[out_len - 1] == 0) out_len--;
    uint8_t* result = (uint8_t*)malloc(out_len + zeros);
    if (!result) { free(out); return NULL; }
    memset(result, 0, zeros);
    for (size_t i = 0; i < out_len; i++) result[zeros + i] = out[out_len-1-i];
    free(out);
    *len_out = out_len + zeros;
    return result;
}

/* ------------------------- Base64 ------------------------- */
static const char B64_ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

uint8_t* gmb_hex_decode(const char* input, size_t* len_out) {
    if (!input) { *len_out = 0; return NULL; }
    size_t in_len = strlen(input);
    if (in_len % 2 != 0) { *len_out = 0; return NULL; }
    size_t out_len = in_len / 2;
    uint8_t* out = (uint8_t*)malloc(out_len ? out_len : 1);
    if (!out) { *len_out = 0; return NULL; }
    for (size_t i = 0; i < out_len; i++) {
        int hi = input[i*2], lo = input[i*2+1];
        int hv = (hi >= '0' && hi <= '9') ? hi - '0' : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10 : (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : -1;
        int lv = (lo >= '0' && lo <= '9') ? lo - '0' : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10 : (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : -1;
        if (hv < 0 || lv < 0) { free(out); *len_out = 0; return NULL; }
        out[i] = (uint8_t)((hv << 4) | lv);
    }
    *len_out = out_len;
    return out;
}

/* FROMHEX(hex) → byte binari come gmb_string_t (data+length, preserva i \0).
 * Il modello byte-array del mondo C: i byte sono una gmb_string binaria. */
char* gmb_base32_encode(const uint8_t* data, size_t len) {
    if (!data) return NULL;
    size_t olen = ((len + 4) / 5) * 8;
    char* out = (char*)malloc(olen + 1);
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < len; i += 5) {
        uint64_t buf = 0;   /* 5 byte = 40 bit: con uint32_t i primi 8 andavano persi */
        int bits = 0;
        for (int j = 0; j < 5 && i + j < len; j++) { buf = (buf << 8) | data[i + j]; bits += 8; }
        /* Gruppo FINALE non pieno: si allinea a un multiplo di 5 bit con zeri in CODA prima di
         * emettere i char. Senza, l'ultimo char veniva estratto con uno shift NEGATIVO
         * (`bits - 5*(j+1)`) e usciva 0 → l'ultimo byte del round-trip cambiava
         * (misurato: 0x6c invece di 0x6e su "gm-basic blockchain"). */
        int pad = (5 - (bits % 5)) % 5;
        buf <<= pad;
        bits += pad;
        int nchars = bits / 5;
        for (int j = 0; j < 8; j++) {
            if (j < nchars) out[oi++] = BASE32_ALPHA[(buf >> (bits - 5 * (j + 1))) & 0x1F];
            else out[oi++] = '=';
        }
    }
    out[oi] = '\0';
    return out;
}

uint8_t* gmb_base32_decode(const char* input, size_t* len_out) {
    if (!input || !len_out) return NULL;
    size_t inlen = strlen(input);
    /* stima: 5 byte ogni 8 char (escludi padding '=') */
    size_t nchars = 0;
    for (size_t i = 0; i < inlen; i++) if (input[i] != '=') nchars++;
    size_t olen = (nchars * 5) / 8;
    uint8_t* out = (uint8_t*)malloc(olen + 1);
    if (!out) return NULL;
    size_t oi = 0;
    uint64_t buf = 0;   /* 5 bit per char, senza maschera: serve > 32 bit */
    int bits = 0;
    for (size_t i = 0; i < inlen; i++) {
        if (input[i] == '=' || input[i] == ' ') continue;
        int val = -1;
        for (int c = 0; c < 32; c++) if (BASE32_ALPHA[c] == input[i]) { val = c; break; }
        if (val < 0) continue;
        buf = (buf << 5) | (uint32_t)val;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            if (oi < olen) out[oi++] = (uint8_t)((buf >> bits) & 0xFF);
        }
    }
    *len_out = oi;
    return out;
}

/* ==================== Blake2b-256 (via sha512-like core con IV blake2b) ==================== */
/* IV della variante Blake2b-256 (IV[0..7] con digest length 32). */
static const uint64_t BLAKE2B_IV[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static uint64_t rotr64(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }
static uint64_t B2B_LOAD64(const uint8_t* p) {
    return ((uint64_t)p[0]) | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

void gmb_blake2b256(const uint8_t* data, size_t len, uint8_t out[32]) {
    static const uint8_t sigma[12][16] = {
        {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        {14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3},
        {11,8,12,0,5,2,15,13,10,14,3,6,7,1,9,4},
        {7,9,3,1,13,12,11,14,2,6,5,10,4,0,15,8},
        {9,0,5,7,2,4,10,15,14,1,11,12,6,8,3,13},
        {2,12,6,10,0,11,8,3,4,13,7,5,15,14,1,9},
        {12,5,1,15,14,13,4,10,0,7,6,3,9,2,8,11},
        {13,11,7,14,12,1,3,9,5,0,15,4,8,6,2,10},
        {6,15,14,9,11,3,0,8,12,2,13,7,1,4,10,5},
        {10,2,8,4,7,6,1,5,15,11,9,14,3,12,13,0},
        {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        {14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3}
    };
    uint64_t h[8];
    /* Parameter block (RFC 7693): digest_length=32, key_length=0, fanout=1, depth=1 →
     * word little-endian 0x01010020, XOR SOLO in h[0] (prima veniva XORato in tutte le
     * parole: BLAKE2b sbagliato). */
    for (int i = 0; i < 8; i++) h[i] = BLAKE2B_IV[i];
    h[0] ^= 0x01010020ULL;
    uint8_t block[128];
    size_t full = len / 128;
    size_t rem = len % 128;
    uint64_t counter = 0;
    const uint8_t* p = data;
    for (size_t b = 0; b < full; b++) {
        memset(block, 0, sizeof(block));
        memcpy(block, p + b * 128, 128);
        counter += 128;   /* t include il blocco corrente (prima era off-by-one) */
        uint64_t m[16];
        for (int i = 0; i < 16; i++) m[i] = B2B_LOAD64(block + i * 8);
        uint64_t v[16];
        for (int i = 0; i < 8; i++) v[i] = h[i];
        for (int i = 0; i < 8; i++) v[8 + i] = BLAKE2B_IV[i];
        v[12] ^= counter;
        /* G usa i 16 indici del message schedule sigma per round (prima usava sempre
         * s[0]/s[1] per tutte le 8 G: BLAKE2b sbagliato). */
        for (int r = 0; r < 12; r++) {
            const uint8_t* s = sigma[r % 12];
            #define G(a,b,c,d,ix,iy) do { v[a]=v[a]+v[b]+m[ix]; v[d]=rotr64(v[d]^v[a],32); v[c]=v[c]+v[d]; v[b]=rotr64(v[b]^v[c],24); v[a]=v[a]+v[b]+m[iy]; v[d]=rotr64(v[d]^v[a],16); v[c]=v[c]+v[d]; v[b]=rotr64(v[b]^v[c],63); } while(0)
            G(0,4,8,12,s[0],s[1]); G(1,5,9,13,s[2],s[3]); G(2,6,10,14,s[4],s[5]); G(3,7,11,15,s[6],s[7]);
            G(0,5,10,15,s[8],s[9]); G(1,6,11,12,s[10],s[11]); G(2,7,8,13,s[12],s[13]); G(3,4,9,14,s[14],s[15]);
            #undef G
        }
        for (int i = 0; i < 8; i++) h[i] ^= v[i] ^ v[i + 8];
    }
    /* blocco finale: BLAKE2 non usa il padding 0x80 — solo zero-padding + flag last
     * (prima veniva inserito 0x80 come SHA: hash BLAKE2b sbagliato). */
    memset(block, 0, sizeof(block));
    memcpy(block, p + full * 128, rem);
    uint64_t m[16];
    for (int i = 0; i < 16; i++) m[i] = B2B_LOAD64(block + i * 8);
    uint64_t v[16];
    for (int i = 0; i < 8; i++) v[i] = h[i];
    for (int i = 0; i < 8; i++) v[8 + i] = BLAKE2B_IV[i];
    v[12] ^= counter + rem;
    v[14] ^= (uint64_t)-1; /* final block */
    for (int r = 0; r < 12; r++) {
        const uint8_t* s = sigma[r % 12];
        #define G(a,b,c,d,ix,iy) do { v[a]=v[a]+v[b]+m[ix]; v[d]=rotr64(v[d]^v[a],32); v[c]=v[c]+v[d]; v[b]=rotr64(v[b]^v[c],24); v[a]=v[a]+v[b]+m[iy]; v[d]=rotr64(v[d]^v[a],16); v[c]=v[c]+v[d]; v[b]=rotr64(v[b]^v[c],63); } while(0)
        G(0,4,8,12,s[0],s[1]); G(1,5,9,13,s[2],s[3]); G(2,6,10,14,s[4],s[5]); G(3,7,11,15,s[6],s[7]);
        G(0,5,10,15,s[8],s[9]); G(1,6,11,12,s[10],s[11]); G(2,7,8,13,s[12],s[13]); G(3,4,9,14,s[14],s[15]);
        #undef G
    }
    for (int i = 0; i < 8; i++) h[i] ^= v[i] ^ v[i + 8];
    for (int i = 0; i < 4; i++) {
        uint64_t hv = h[i];
        for (int b = 0; b < 8; b++) out[i * 8 + b] = (uint8_t)((hv >> (8 * b)) & 0xFF);
    }
}

/* Wrapper gmb_string_t BINARIO da buffer mallocato (ownership trasferita): il C99Native
 * lo usa per RLPENCODE/VARINTENCODE (uint8_t* + out-len → byte[]). Unica implementazione. */
uint8_t* gmb_varint_encode(int64_t value, size_t* len_out) {
    if (!len_out) return NULL;
    if (value < 0) { *len_out = 0; return NULL; }
    uint8_t buf[10];
    size_t n = 0;
    do {
        uint8_t b = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value != 0) b |= 0x80;
        buf[n++] = b;
    } while (value != 0 && n < sizeof(buf));
    uint8_t* o = (uint8_t*)malloc(n);
    if (!o) { *len_out = 0; return NULL; }
    memcpy(o, buf, n);
    *len_out = n;
    return o;
}

int64_t gmb_varint_decode(const uint8_t* data, size_t len, size_t* consumed) {
    int64_t result = 0;
    int shift = 0;
    size_t i = 0;
    if (!data || len == 0) { if (consumed) *consumed = 0; return 0; }
    for (; i < len; i++) {
        uint8_t b = data[i];
        result |= ((int64_t)(b & 0x7F)) << shift;
        if ((b & 0x80) == 0) { i++; break; }
        shift += 7;
        if (shift >= 64) { i++; break; }
    }
    if (consumed) *consumed = i;
    return result;
}

/* ==================== RLP encode (Ethereum) per input singolo ==================== */
uint8_t* gmb_rlp_encode(const uint8_t* data, size_t len, size_t* len_out) {
    if (!len_out) return NULL;
    uint8_t* out;
    size_t prefix = 1, ext = 0;
    if (len == 1 && data[0] < 0x80) {
        out = (uint8_t*)malloc(1); out[0] = data[0]; *len_out = 1; return out;
    }
    if (len < 56) {
        out = (uint8_t*)malloc(len + 1);
        out[0] = (uint8_t)(0x80 + len);
        memcpy(out + 1, data, len);
        *len_out = len + 1;
        return out;
    }
    size_t blen = 0, tmp = len;
    while (tmp > 0) { blen++; tmp >>= 8; }
    out = (uint8_t*)malloc(1 + blen + len);
    out[0] = (uint8_t)(0xB7 + blen);
    for (size_t i = 0; i < blen; i++) out[1 + i] = (uint8_t)((len >> (8 * (blen - 1 - i))) & 0xFF);
    memcpy(out + 1 + blen, data, len);
    *len_out = 1 + blen + len;
    (void)prefix; (void)ext;
    return out;
}

/* ==================== Password hash/verify ====================
 * Formato canonico (parità C# CorePasswordHash/CorePasswordVerify — IL/PCodeVM):
 *   PBKDF2-HMAC-SHA256, 16 byte salt CSPRNG, 100000 iterazioni, 32 byte hash.
 *   Stringa: Base64(salt) + "." + Base64(hash)   (Base64 standard con padding '=')
 * La stessa stringa prodotta/verificata da un backend è valida in tutti gli altri. */
#define GMB_PWHASH_SALT_LEN  16u
#define GMB_PWHASH_ITER      100000u
#define GMB_PWHASH_HASH_LEN  32u

char* gmb_abi_encode(const char* signature, const double* args, int argc, const char** strargs, const int* strargs_idx) {
    if (!signature) return NULL;
    uint8_t sel[32];
    gmb_keccak256((const uint8_t*)signature, strlen(signature), sel);
    /* "0x" (2) + selector (8 hex) + 32 byte per argomento (64 hex) + NUL.
     * L'allocazione precedente (8 + argc*64 + 1) non contava il prefisso "0x" e
     * andava in overflow di 2 byte. */
    char* out = (char*)malloc((size_t)(2 + 8 + argc * 64 + 1));
    if (!out) return NULL;
    char* o = out;
    *o++ = '0'; *o++ = 'x';
    for (int i = 0; i < 4; i++) {
        static const char HEX[] = "0123456789abcdef";
        *o++ = HEX[sel[i] >> 4]; *o++ = HEX[sel[i] & 0xF];
    }
    for (int a = 0; a < argc; a++) {
        uint64_t uv = 0;
        if (strargs_idx && strargs_idx[a]) uv = (uint64_t)strlen(strargs[strargs_idx[a]]);
        else uv = (uint64_t)(int64_t)args[a];
        /* Parola ABI da 32 byte (64 hex), big-endian right-aligned — parità
         * BlockchainUtils.EncodeAbiWord/IL (prima scriveva 16 byte, divergeva). */
        for (int i = 0; i < 32; i++) {
            static const char HEX[] = "0123456789abcdef";
            int sh = 31 - i;
            uint8_t byte = (sh < 8) ? (uint8_t)((uv >> (8 * sh)) & 0xFF) : 0;
            *o++ = HEX[byte >> 4]; *o++ = HEX[byte & 0xF];
        }
    }
    *o = '\0';
    return out;
}

