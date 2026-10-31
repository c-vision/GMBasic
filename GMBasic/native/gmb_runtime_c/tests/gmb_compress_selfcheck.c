/* FILE: native/gmb_runtime_c/tests/gmb_compress_selfcheck.c
 * Round-trip compressione via COMPRESSION (deflate/gzip/zlib/bzip2/brotli + zstd/lz4/xz).
 * Verifica che ogni algoritmo disponibile comprima+decomprima correttamente una stringa. */
#include "gmb_compress.h"
#include "gmb_runtime.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0;
#define CHECK(cond, algo) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", algo); g_fail = 1; } } while (0)

static void roundtrip(const char* algo, int* n_ok, int* n_tot) {
    const char* text = "The quick brown fox jumps over the lazy dog. 1234567890. "
                       "The quick brown fox jumps over the lazy dog. 1234567890.";
    size_t clen = 0, dlen = 0;
    unsigned char* c = gmb_compress_data(algo, (const unsigned char*)text, strlen(text), &clen);
    size_t dl0 = 0;
    unsigned char* d = c ? gmb_decompress_data(algo, c, clen, &dl0) : NULL;
    (*n_tot)++;
    int ok = c && d && dl0 == strlen(text) && memcmp(d, text, strlen(text)) == 0;
    if (ok) (*n_ok)++;
    CHECK(ok, algo);
    if (c) free(c);
    if (d) free(d);
    (void)dlen;
}

int main(void) {
    int ok = 0, tot = 0;
    roundtrip("deflate", &ok, &tot);
    roundtrip("gzip", &ok, &tot);
    roundtrip("zlib", &ok, &tot);
#if defined(GMB_HAVE_BZ2)
    roundtrip("bzip2", &ok, &tot);
#endif
#if defined(GMB_HAVE_BROTLI)
    roundtrip("brotli", &ok, &tot);
#endif
#if defined(GMB_HAVE_ZSTD)
    roundtrip("zstd", &ok, &tot);
#endif
#if defined(GMB_HAVE_LZ4)
    roundtrip("lz4", &ok, &tot);
#endif
#if defined(GMB_HAVE_LZMA)
    roundtrip("xz", &ok, &tot);
#endif
    /* via la string-API (base64) per gli stessi nuovi algo */
#if defined(GMB_HAVE_ZSTD)
    {
        gmb_string_t s = gmb_string_alloc("hello zstd world hello zstd world");
        gmb_string_t c = gmb_compress_string("zstd", s);
        gmb_string_t d = gmb_decompress_string("zstd", c);
        tot++;
        if (d.data && strcmp(d.data, s.data) == 0) ok++;
        else CHECK(0, "zstd string-API");
    }
#endif
    printf("gmb_compress_selfcheck: %d/%d algoritmi OK\n", ok, tot);
    /* ZIP-SLIP: il nome dell'entry di un archivio non è fidato. Devono essere RIFIUTATI
     * path assoluti e componenti ".."; i nomi normali vanno accettati e normalizzati. */
    {
        char e[256];
        const char* bad[] = { "../evil.txt", "a/../../evil", "/abs/evil", "..\\evil" };
        const char* good[] = { "a/b.txt", "./a/c.txt", "a//b.txt" };
        int nbad = (int)(sizeof bad / sizeof bad[0]), ngood = (int)(sizeof good / sizeof good[0]);
        for (int i = 0; i < nbad; i++)
            if (gmb_zip_entry_safe(bad[i], e, sizeof e)) CHECK(0, "zip-slip accettato");
        for (int i = 0; i < ngood; i++)
            if (!gmb_zip_entry_safe(good[i], e, sizeof e)) CHECK(0, "zip entry valida rifiutata");
    }
    if (g_fail) return 1;
    puts("gmb_compress_selfcheck OK");
    return 0;
}
