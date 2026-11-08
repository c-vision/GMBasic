/* cp_ffi.c — shim ABI del pacchetto `compression` per l'export BASIC.
 *
 * Unica porta verso il BASIC: testo/base64 in ingresso, base64/testo in uscita.
 * L'algoritmo arriva dal BASIC in maiuscolo (GZIP, DEFLATE, ...) e qui viene
 * normalizzato in minuscolo, la stessa forma attesa dai kernel.
 * Nessuna funzione ZIP: ZIP/UNZIP restano builtin del runtime.
 */
#include "gmbcompression.h"

#include <stdlib.h>
#include <string.h>

char* gmbcp_compress_b64(const char* algo, const char* text);
char* gmbcp_decompress_b64(const char* algo, const char* b64);

/* copia minuscola e trim della stringa algoritmo (ASCII). */
static char* cp_norm_algo(const char* algo)
{
    const char* s = algo ? algo : "";
    size_t n, i, o = 0;
    char* out;
    while (*s == ' ' || *s == '\t') s++;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    out = (char*)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[o++] = c;
    }
    out[o] = 0;
    return out;
}

char* ofx_cp_compress_b64(const char* text, const char* algo)
{
    char* a = cp_norm_algo(algo);
    char* r;
    if (!a) return NULL;
    r = gmbcp_compress_b64(a, text);
    free(a);
    return r;
}

char* ofx_cp_decompress_b64(const char* b64, const char* algo)
{
    char* a = cp_norm_algo(algo);
    char* r;
    if (!a) return NULL;
    r = gmbcp_decompress_b64(a, b64);
    free(a);
    return r;
}

const char* ofx_cp_algorithms(void)
{
    return "GZIP,DEFLATE,BROTLI,ZLIB,BZIP2,ZSTD,LZ4,XZ";
}
