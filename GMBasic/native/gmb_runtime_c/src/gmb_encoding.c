// FILE: gmb_encoding.c — Hex (TOHEX) e Base58
// Implementazioni in C99 puro. Convenzioni come gmb_commonlib.h.

#include "gmb_commonlib.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#if defined(HAVE_OPENSSL)
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <wincrypt.h>
static void gmb_random_bytes(void* out, size_t n) {
    uint8_t* p = (uint8_t*)out;
    size_t got = 0;
    HCRYPTPROV prov = 0;
    if (CryptAcquireContextW(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        while (got < n) {
            DWORD chunk = (DWORD)((n - got) > 1024 ? 1024 : (n - got));
            if (CryptGenRandom(prov, chunk, p + got)) got += chunk; else break;
        }
        CryptReleaseContext(prov, 0);
    }
    for (; got < n; got++) p[got] = (uint8_t)(rand() & 0xff);
}
#else
#include <unistd.h>
#include <fcntl.h>
static void gmb_random_bytes(void* out, size_t n) {
    uint8_t* p = (uint8_t*)out;
    size_t got = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        while (got < n) {
            ssize_t r = read(fd, p + got, n - got);
            if (r <= 0) break;
            got += (size_t)r;
        }
        close(fd);
        if (got == n) return;
    }
    for (; got < n; got++) p[got] = (uint8_t)(rand() & 0xff);
}
#endif

/* ------------------------- Hex ------------------------- */

void gmb_hex_encode(const uint8_t* data, size_t len, char* out) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i*2] = hex[data[i] >> 4];
        out[i*2+1] = hex[data[i] & 0x0f];
    }
}

/* Ponte BYTE ↔ ARRAY (C99Native). In questo backend i byte "grezzi" possono vivere in due
 * rappresentazioni: stringa binaria (gmb_string_t, esito di FROMHEX/digest) oppure array
 * numerico con elementi 0..255 (`DIM x(n) AS BYTE`). Per poterli usare in modo
 * intercambiabile (argomento di un parametro array) serve la conversione qui sotto.
 *
 * gmb_bytes_as_doubles: buffer a ROTAZIONE (nessuna allocazione da liberare dal chiamante,
 * vita valida per la durata dell'espressione che lo usa). Cade solo su OOM (catastrofico).
 * NB (2026-11-13): `gmb_hex_from_doubles` e `gmb_bytes_from_doubles` (ponte array→hex/binario
 * per le forme `TOHEX(array)`/`TOBASE64(array)`) sono stati RIMOSSI: quelle forme sono ritirate
 * con la migrazione CRYPTOGRAPHY a pacchetto FFI (per i byte c'è `BYTESHEX(a())`).
 * Vedi Bugs/(solved) - C99-TOHEX-TBASE64-Su-Array-Dopo-Migrazione-Crypto. */
#define GMB_AS_DOUBLES_SLOTS 8
const gmb_double_t* gmb_bytes_as_doubles(gmb_string_t s, gmb_int_t* len_out) {
    static gmb_double_t* buf[GMB_AS_DOUBLES_SLOTS];
    static size_t cap[GMB_AS_DOUBLES_SLOTS];
    static int slot = 0;
    int i = slot;
    slot = (slot + 1) % GMB_AS_DOUBLES_SLOTS;
    size_t n = (s.length > 0) ? (size_t)s.length : 0;
    if (n + 1 > cap[i]) {
        gmb_double_t* nb = (gmb_double_t*)realloc(buf[i], (n + 1) * sizeof(gmb_double_t));
        if (!nb) { if (len_out) *len_out = 0; return buf[i]; }
        buf[i] = nb;
        cap[i] = n + 1;
    }
    for (size_t k = 0; k < n; k++)
        buf[i][k] = (gmb_double_t)(unsigned char)(s.data ? s.data[k] : 0);
    if (len_out) *len_out = (gmb_int_t)n;
    return buf[i];
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
    // risultato little-endian → inverti, aggiungi gli zeri iniziali
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

char* gmb_base64_encode(const uint8_t* data, size_t len) {
    size_t out_len = 4 * ((len + 2) / 3);
    char* out = (char*)malloc(out_len + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i+1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i+2];
        out[o++] = B64_ALPHA[(n >> 18) & 63];
        out[o++] = B64_ALPHA[(n >> 12) & 63];
        out[o++] = (i + 1 < len) ? B64_ALPHA[(n >> 6) & 63] : '=';
        out[o++] = (i + 2 < len) ? B64_ALPHA[n & 63] : '=';
    }
    out[out_len] = '\0';
    return out;
}

static int b64_index(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

uint8_t* gmb_base64_decode(const char* input, size_t* len_out) {
    size_t in_len = strlen(input);
    size_t cap = in_len / 4 * 3 + 3;
    uint8_t* out = (uint8_t*)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    uint32_t buf = 0; int bits = 0;
    for (size_t i = 0; i < in_len; i++) {
        if (input[i] == '=' || input[i] == '\n' || input[i] == '\r') continue;
        int d = b64_index(input[i]);
        if (d < 0) continue;
        buf = (buf << 6) | (uint32_t)d;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (uint8_t)((buf >> bits) & 0xff);
        }
    }
    *len_out = o;
    return out;
}

/* ------------------------- Base64URL (RFC 4648 §5, no padding) -------------------------
 * Parità con C# CoreToBase64Url/CoreFromBase64Url (Standard base64 + replace + no '='). */
static const char B64URL_ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char* gmb_base64url_encode(const uint8_t* data, size_t len) {
    if (!data) return NULL;
    size_t out_len = 4 * ((len + 2) / 3);
    /* rimuovi il padding '=' (0/1/2 char) */
    size_t pad = (3 - (len % 3)) % 3;
    size_t olen = out_len - pad;
    char* out = (char*)malloc(olen + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i + 2];
        if (o < olen) out[o++] = B64URL_ALPHA[(n >> 18) & 63];
        if (o < olen) out[o++] = B64URL_ALPHA[(n >> 12) & 63];
        if (o < olen) out[o++] = (i + 1 < len) ? B64URL_ALPHA[(n >> 6) & 63] : '=';
        if (o < olen) out[o++] = (i + 2 < len) ? B64URL_ALPHA[n & 63] : '=';
    }
    out[olen] = '\0';
    return out;
}

static int b64url_index(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

uint8_t* gmb_base64url_decode(const char* input, size_t* len_out) {
    *len_out = 0;
    if (!input) return NULL;
    size_t in_len = strlen(input);
    if (in_len == 0) { uint8_t* e = (uint8_t*)malloc(1); if (e) { e[0] = 0; } return e; }
    size_t cap = in_len / 4 * 3 + 3;
    uint8_t* out = (uint8_t*)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    uint32_t buf = 0; int bits = 0;
    for (size_t i = 0; i < in_len; i++) {
        char c = input[i];
        if (c == '=' || c == '\n' || c == '\r') continue;
        int d = b64url_index(c);
        if (d < 0) { free(out); *len_out = 0; return NULL; }
        buf = (buf << 6) | (uint32_t)d;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (uint8_t)((buf >> bits) & 0xff);
        }
    }
    *len_out = o;
    return out;
}

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
gmb_string_t gmb_bytes_fromhex(gmb_string_t hex) {
    if (!hex.data) return gmb_string_alloc("");
    size_t n = 0;
    uint8_t* b = gmb_hex_decode(hex.data, &n);
    if (!b) return gmb_string_alloc("");
    gmb_string_t r;
    r.length = n;
    r.data = (char*)b;
    return r;
}

/* ------------------------- HTML encode (OWASP minimal set, parità .NET WebUtility.HtmlEncode)
 * & → &amp;   < → &lt;   > → &gt;   " → &quot;   ' → &#39;   (i byte non-ASCII passano invariati) */
char* gmb_html_encode(const char* input) {
    if (!input) input = "";
    size_t in_len = strlen(input);
    size_t cap = in_len * 6 + 1;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < in_len; i++) {
        const char* rep = NULL;
        switch (input[i]) {
            case '&': rep = "&amp;"; break;
            case '<': rep = "&lt;"; break;
            case '>': rep = "&gt;"; break;
            case '"': rep = "&quot;"; break;
            case '\'': rep = "&#39;"; break;
            default: break;
        }
        if (rep) {
            size_t rl = strlen(rep);
            memcpy(out + o, rep, rl); o += rl;
        } else {
            out[o++] = input[i];
        }
    }
    out[o] = '\0';
    return out;
}

/* ------------------------- URL percent-encode (RFC 3986, parità .NET Uri.EscapeDataString)
 * Conserva gli unreserved A-Za-z0-9-._~; il resto in %HH maiuscolo su byte UTF-8. */
static int gmb_url_unreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '-' || c == '_' || c == '.' || c == '~';
}

char* gmb_url_encode(const char* input) {
    if (!input) input = "";
    size_t in_len = strlen(input);
    size_t cap = in_len * 3 + 1;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    static const char HEXU[] = "0123456789ABCDEF";
    size_t o = 0;
    for (size_t i = 0; i < in_len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (gmb_url_unreserved(c)) {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = HEXU[c >> 4];
            out[o++] = HEXU[c & 0x0F];
        }
    }
    out[o] = '\0';
    return out;
}

/* ------------------------- JS string encode (per literal JS a doppi apici)
 * Parità con CommonLib.WebEncoding.CoreJsEncode: escape di \\, ", ' e controlli C0
 * (\b \t \n \f \r, gli altri C0 come \u00xx minuscolo) + U+2028/U+2029 come \u2028/\u2029.
 * I byte non-ASCII passano invariati (UTF-8). */
static void js_append_hex4(char* out, size_t* o, unsigned cp) {
    static const char HEXL[] = "0123456789abcdef";
    out[(*o)++] = '\\'; out[(*o)++] = 'u';
    out[(*o)++] = HEXL[(cp >> 12) & 0xF]; out[(*o)++] = HEXL[(cp >> 8) & 0xF];
    out[(*o)++] = HEXL[(cp >> 4) & 0xF]; out[(*o)++] = HEXL[cp & 0xF];
}

char* gmb_js_encode(const char* input) {
    if (!input) input = "";
    size_t in_len = strlen(input);
    size_t cap = in_len * 6 + 1;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < in_len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == '\\') { out[o++] = '\\'; out[o++] = '\\'; }
        else if (c == '"') { out[o++] = '\\'; out[o++] = '"'; }
        else if (c == '\'') { out[o++] = '\\'; out[o++] = '\''; }
        else if (c == '\b') { out[o++] = '\\'; out[o++] = 'b'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\f') { out[o++] = '\\'; out[o++] = 'f'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c < 0x20) { js_append_hex4(out, &o, (unsigned)c); }
        else if (c == 0xE2 && i + 2 < in_len && (unsigned char)input[i+1] == 0x80
                 && ((unsigned char)input[i+2] == 0xA8 || (unsigned char)input[i+2] == 0xA9))
        {
            /* U+2028 (LS) / U+2029 (PS): pericolosi in una stringa JS (line terminator) */
            unsigned cp = ((unsigned char)input[i+2] == 0xA8) ? 0x2028u : 0x2029u;
            js_append_hex4(out, &o, cp);
            i += 2;
        }
        else { out[o++] = (char)c; }
    }
    out[o] = '\0';
    return out;
}

/* ------------------------- Upload security (parità CommonLib.Http.FileUpload) -------------------------
 * Nome file sicuro: ultimo segmento di path, rimozione di caratteri pericolosi e sequenze
 * "..", trim, cap 120 char. I punti singoli (a.png) sono conservati. */
char* gmb_upload_safe_name(const char* input) {
    if (!input) input = "";
    size_t len = strlen(input);
    /* ultimo segmento dopo '/' o '\' */
    const char* start = input;
    for (size_t i = 0; i < len; i++)
        if (input[i] == '/' || input[i] == '\\') start = input + i + 1;

    size_t cap = strlen(start) + 1;
    char* name = (char*)malloc(cap ? cap : 2);
    if (!name) return NULL;
    size_t o = 0;
    for (const char* p = start; *p; p++)
    {
        char c = *p;
        if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            continue;
        name[o++] = c;
    }
    name[o] = '\0';
    /* rimuovi le sequenze ".." finché presenti */
    char* hit;
    while ((hit = strstr(name, "..")) != NULL)
    {
        size_t rest = strlen(hit + 2);
        memmove(hit, hit + 2, rest + 1);
    }
    /* trim spazi ai bordi */
    size_t b = 0, e = strlen(name);
    while (b < e && name[b] == ' ') b++;
    while (e > b && name[e - 1] == ' ') e--;
    if (e > 120) e = 120;
    size_t outLen = e - b;
    char* out = (char*)malloc(outLen + 1);
    if (!out) { free(name); return NULL; }
    memcpy(out, name + b, outLen);
    out[outLen] = '\0';
    free(name);
    if (outLen == 0) { free(out); out = (char*)malloc(5); if (!out) return NULL; memcpy(out, "file", 5); }
    return out;
}

/* Content-type da magic-number (parità CommonLib.Http.FileUpload): ritorna una stringa
 * statica o "" se sconosciuto. */
static int gmb_upload_starts(const uint8_t* d, size_t l, const uint8_t* m, size_t ml)
{ return l >= ml && memcmp(d, m, ml) == 0; }

const char* gmb_upload_detect_type(const uint8_t* data, size_t len) {
    if (!data || len == 0) return "";
    static const uint8_t png[] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
    static const uint8_t jpeg[] = {0xFF,0xD8,0xFF};
    static const uint8_t zip[] = {0x50,0x4B,0x03,0x04};
    static const uint8_t gz[] = {0x1F,0x8B};
    if (gmb_upload_starts(data, len, png, sizeof(png))) return "image/png";
    if (gmb_upload_starts(data, len, jpeg, sizeof(jpeg))) return "image/jpeg";
    if (len >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)) return "image/gif";
    if (len >= 5 && memcmp(data, "%PDF-", 5) == 0) return "application/pdf";
    if (gmb_upload_starts(data, len, zip, sizeof(zip))) return "application/zip";
    if (gmb_upload_starts(data, len, gz, sizeof(gz))) return "application/gzip";
    return "";
}

/* ==================== Base32 (RFC 4648) ==================== */
static const char BASE32_ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

char* gmb_base32_encode(const uint8_t* data, size_t len) {
    if (!data) return NULL;
    size_t olen = ((len + 4) / 5) * 8;
    char* out = (char*)malloc(olen + 1);
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < len; i += 5) {
        uint32_t buf = 0;
        int bits = 0;
        for (int j = 0; j < 5 && i + j < len; j++) { buf = (buf << 8) | data[i + j]; bits += 8; }
        int nchars = (bits + 4) / 5;
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
    uint32_t buf = 0;
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
gmb_string_t gmb_string_take_bytes(uint8_t* data, size_t len) {
    gmb_string_t r;
    if (!data) { r.data = (char*)calloc(1, 1); r.length = 0; r.ref_count = 1; return r; }
    r.data = (char*)data;
    r.length = len;
    r.ref_count = 1;
    return r;
}

/* ==================== VarInt LEB128 (parità IL/PCodeVM BlockchainUtils) ====================
 * LEB128 a 7 bit per byte, little-endian, high-bit = continuation. Prima era il "compact
 * varint" di Bitcoin (0xFD/0xFE/0xFF + little-endian): divergeva da IL/PCodeVM. */
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

char* gmb_password_hash(const char* password) {
    if (!password) password = "";
    size_t pwlen = strlen(password);
    uint8_t salt[GMB_PWHASH_SALT_LEN];
    gmb_random_bytes(salt, GMB_PWHASH_SALT_LEN);
    uint8_t hash[GMB_PWHASH_HASH_LEN];
    gmb_pbkdf2_sha256((const uint8_t*)password, pwlen, salt, GMB_PWHASH_SALT_LEN,
                      GMB_PWHASH_ITER, hash, GMB_PWHASH_HASH_LEN);
    char* salt_b64 = gmb_base64_encode(salt, GMB_PWHASH_SALT_LEN);   /* 24 char */
    char* hash_b64 = gmb_base64_encode(hash, GMB_PWHASH_HASH_LEN);   /* 44 char */
    if (!salt_b64 || !hash_b64) { free(salt_b64); free(hash_b64); return NULL; }
    char* out = (char*)malloc(24 + 1 + 44 + 1);
    if (!out) { free(salt_b64); free(hash_b64); return NULL; }
    sprintf(out, "%s.%s", salt_b64, hash_b64);
    free(salt_b64);
    free(hash_b64);
    return out;
}

int gmb_password_verify(const char* password, const char* stored) {
    if (!password || !stored) return 0;
    /* formato canonico: Base64(salt).Base64(hash) — accetta il primo '.' come separatore */
    const char* dot = strchr(stored, '.');
    if (!dot || dot == stored) return 0;
    char* salt_b64 = (char*)malloc((size_t)(dot - stored) + 1);
    if (!salt_b64) return 0;
    memcpy(salt_b64, stored, (size_t)(dot - stored));
    salt_b64[dot - stored] = '\0';
    const char* hash_b64 = dot + 1;
    size_t salt_len = 0, hash_len = 0;
    uint8_t* salt = gmb_base64_decode(salt_b64, &salt_len);
    uint8_t* expected = gmb_base64_decode(hash_b64, &hash_len);
    free(salt_b64);
    if (!salt || !expected) { free(salt); free(expected); return 0; }
    if (salt_len == 0 || hash_len == 0 || salt_len > 64 || hash_len > 64) {
        free(salt); free(expected); return 0;
    }
    uint8_t actual[64];
    gmb_pbkdf2_sha256((const uint8_t*)password, strlen(password), salt, salt_len,
                      GMB_PWHASH_ITER, actual, hash_len);
    free(salt);
    if (hash_len != GMB_PWHASH_HASH_LEN) { free(expected); return 0; }
    int diff = 0;
    for (size_t i = 0; i < hash_len; i++) diff |= actual[i] ^ expected[i];
    free(expected);
    return diff == 0 ? 1 : 0;
}

/* ==================== ABI encode (Ethereum): metodo + argomenti uint/string → hex 0x ==================== */
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

char* gmb_abi_encode_vals(const char* signature, const gmb_val_t* args, int argc)
{
    if (!signature) return NULL;
    const double* nums = NULL; const char** strs = NULL; const int* stridx = NULL;
    double* nbuf = NULL; const char** sbuf = NULL; int* ibuf = NULL;
    if (argc > 0)
    {
        nbuf = (double*)calloc((size_t)argc, sizeof(double));
        sbuf = (const char**)calloc((size_t)argc, sizeof(const char*));
        ibuf = (int*)calloc((size_t)argc, sizeof(int));
        if (nbuf && sbuf && ibuf)
        {
            for (int i = 0; i < argc; i++)
            {
                if (args[i].kind == GMB_TYPE_STRING && args[i].v_string.data)
                { sbuf[i] = args[i].v_string.data; ibuf[i] = 1; }
                else nbuf[i] = gmb_val_to_double(args[i]);
            }
            nums = nbuf; strs = sbuf; stridx = ibuf;
        }
    }
    char* out = gmb_abi_encode(signature, nums, argc, strs, stridx);
    free(nbuf); free(sbuf); free(ibuf);
    return out;
}
