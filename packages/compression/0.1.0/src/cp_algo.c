/* cp_algo.c — algoritmi di compressione del pacchetto `compression`.
 *
 * Copia self-contained dei kernel di gmb_compress.c (zlib/zstd/lz4/lzma) con
 * prefisso gmbcp_ per evitare collisioni con il runtime. NON include gli archivi
 * ZIP: ZIP/UNZIP restano builtin (gmb_zip_*, una sola implementazione).
 *
 * ABI esposta: gmbcp_compress_b64(algo, text) / gmbcp_decompress_b64(algo, b64).
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(__has_include)
#if __has_include(<zlib.h>)
#define GBMCP_HAVE_ZLIB 1
#include <zlib.h>
#endif
#if __has_include(<zstd.h>)
#define GBMCP_HAVE_ZSTD 1
#include <zstd.h>
#endif
#if __has_include(<lz4.h>)
#define GBMCP_HAVE_LZ4 1
#include <lz4.h>
#include <lz4frame.h>
#endif
#if __has_include(<lzma.h>)
#define GBMCP_HAVE_LZMA 1
#include <lzma.h>
#endif
#else
#define GBMCP_HAVE_ZLIB 1
#include <zlib.h>
#endif

unsigned char* gmbcp_bzip2_compress(const unsigned char* d, size_t n, size_t* out_len);
unsigned char* gmbcp_bzip2_decompress(const unsigned char* d, size_t n, size_t* out_len);
unsigned char* gmbcp_brotli_compress(const unsigned char* d, size_t n, size_t* out_len);
unsigned char* gmbcp_brotli_decompress(const unsigned char* d, size_t n, size_t* out_len);
char* gmbcp_b64_encode(const uint8_t* data, size_t len);
uint8_t* gmbcp_b64_decode(const char* input, size_t* len_out);

#if defined(GBMCP_HAVE_ZSTD)
static unsigned char* gmbcp_zstd_compress_buf(const unsigned char* d, size_t n, size_t* out_len)
{
    size_t cap = ZSTD_compressBound(n);
    unsigned char* b = (unsigned char*)malloc(cap);
    size_t got;
    if (!b) return NULL;
    got = ZSTD_compress(b, cap, d, n, 3);
    if (ZSTD_isError(got)) { free(b); return NULL; }
    *out_len = got;
    return b;
}
static unsigned char* gmbcp_zstd_decompress_buf(const unsigned char* d, size_t n, size_t* out_len)
{
    unsigned long long sz = ZSTD_getFrameContentSize(d, n);
    size_t cap = (sz == ZSTD_CONTENTSIZE_UNKNOWN || sz == ZSTD_CONTENTSIZE_ERROR || sz == 0)
                     ? (n * 8 + 65536) : (size_t)sz;
    for (int t = 0; t < 6; t++) {
        unsigned char* b = (unsigned char*)malloc(cap);
        size_t got;
        if (!b) return NULL;
        got = ZSTD_decompress(b, cap, d, n);
        if (!ZSTD_isError(got)) { *out_len = got; return b; }
        free(b);
        cap *= 2;
    }
    return NULL;
}
#endif

#if defined(GBMCP_HAVE_LZ4)
static unsigned char* gmbcp_lz4_compress_buf(const unsigned char* d, size_t n, size_t* out_len)
{
    size_t cap = LZ4F_compressFrameBound(n, NULL);
    unsigned char* b = (unsigned char*)malloc(cap);
    size_t got;
    if (!b) return NULL;
    got = LZ4F_compressFrame(b, cap, d, n, NULL);
    if (LZ4F_isError(got)) { free(b); return NULL; }
    *out_len = got;
    return b;
}
static unsigned char* gmbcp_lz4_decompress_buf(const unsigned char* d, size_t n, size_t* out_len)
{
    LZ4F_dctx* dctx = NULL;
    size_t cap = n * 4 + 65536, used = 0, src = 0, r;
    unsigned char* b;
    if (LZ4F_isError(LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION)) || !dctx) return NULL;
    b = (unsigned char*)malloc(cap);
    if (!b) { LZ4F_freeDecompressionContext(dctx); return NULL; }
    do {
        size_t dstSz = cap - used, srcSz = n - src;
        r = LZ4F_decompress(dctx, b + used, &dstSz, d + src, &srcSz, NULL);
        if (LZ4F_isError(r)) { free(b); LZ4F_freeDecompressionContext(dctx); return NULL; }
        used += dstSz;
        src += srcSz;
        if (r != 0 && used == cap) {
            unsigned char* nb;
            cap *= 2;
            nb = (unsigned char*)realloc(b, cap);
            if (!nb) { free(b); LZ4F_freeDecompressionContext(dctx); return NULL; }
            b = nb;
        }
    } while (r != 0);
    LZ4F_freeDecompressionContext(dctx);
    *out_len = used;
    return b;
}
#endif

#if defined(GBMCP_HAVE_LZMA)
static unsigned char* gmbcp_lzma_compress_buf(const unsigned char* d, size_t n, size_t* out_len)
{
    size_t cap = lzma_stream_buffer_bound(n);
    unsigned char* b = (unsigned char*)malloc(cap);
    size_t pos = 0;
    lzma_ret r;
    if (!b) return NULL;
    r = lzma_easy_buffer_encode(6, LZMA_CHECK_CRC64, NULL, d, n, b, &pos, cap);
    if (r != LZMA_OK) { free(b); return NULL; }
    *out_len = pos;
    return b;
}
static unsigned char* gmbcp_lzma_decompress_buf(const unsigned char* d, size_t n, size_t* out_len)
{
    size_t cap = n * 8 + 65536;
    unsigned char* b = (unsigned char*)malloc(cap);
    if (!b) return NULL;
    for (int t = 0; t < 8; t++) {
        uint64_t memlimit = UINT64_MAX;
        size_t in_pos = 0, out_pos = 0;
        lzma_ret r = lzma_stream_buffer_decode(&memlimit, 0, NULL, d, &in_pos, n, b, &out_pos, cap);
        if (r == LZMA_OK) { *out_len = out_pos; return b; }
        if (r != LZMA_BUF_ERROR) { free(b); return NULL; }
        cap *= 2;
        {
            unsigned char* nb = (unsigned char*)realloc(b, cap);
            if (!nb) { free(b); return NULL; }
            b = nb;
        }
    }
    free(b);
    return NULL;
}
#endif

/* algo: gzip | zlib | deflate (raw) — gli altri li gestisce by_name. */
static unsigned char* gmbcp_zlib_compress(const char* algo, const unsigned char* data, size_t len, size_t* out_len)
{
    *out_len = 0;
#if defined(GBMCP_HAVE_ZLIB)
    uLongf clen = compressBound((uLong)len);
    unsigned char* out = (unsigned char*)malloc(clen + 64);
    int windowBits = -15;
    z_stream zs;
    int rc;
    if (!out) return NULL;
    if (algo && strcmp(algo, "gzip") == 0) windowBits = 15 + 16;
    else if (algo && strcmp(algo, "zlib") == 0) windowBits = 15;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    { free(out); return NULL; }
    zs.next_in = (Bytef*)data;
    zs.avail_in = (uInt)len;
    zs.next_out = out;
    /* il bound non copre header/trailer gzip: diamo tutto il buffer allocato */
    zs.avail_out = (uInt)(clen + 64);
    rc = deflate(&zs, Z_FINISH);
    deflateEnd(&zs);
    if (rc != Z_STREAM_END) { free(out); return NULL; }
    *out_len = zs.total_out;
    return out;
#else
    (void)algo; (void)data; (void)len;
    return NULL;
#endif
}

static unsigned char* gmbcp_zlib_decompress(const char* algo, const unsigned char* data, size_t len, size_t* out_len)
{
    *out_len = 0;
#if defined(GBMCP_HAVE_ZLIB)
    int windowBits = -15;
    size_t cap = len * 4 + 1024;
    unsigned char* out;
    z_stream zs;
    int rc;
    if (algo && strcmp(algo, "gzip") == 0) windowBits = 15 + 16;
    else if (algo && strcmp(algo, "zlib") == 0) windowBits = 15;
    out = (unsigned char*)malloc(cap);
    if (!out) return NULL;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, windowBits) != Z_OK) { free(out); return NULL; }
    zs.next_in = (Bytef*)data;
    zs.avail_in = (uInt)len;
    zs.next_out = out;
    zs.avail_out = (uInt)cap;
    rc = inflate(&zs, Z_FINISH);
    if (rc == Z_OK || rc == Z_BUF_ERROR) {
        do {
            size_t used = (size_t)(zs.next_out - out);
            unsigned char* nb;
            cap *= 2;
            nb = (unsigned char*)realloc(out, cap);
            if (!nb) { inflateEnd(&zs); free(out); return NULL; }
            out = nb;
            zs.next_out = out + used;
            zs.avail_out = (uInt)(cap - used);
            rc = inflate(&zs, Z_FINISH);
        } while (rc == Z_OK || rc == Z_BUF_ERROR);
    }
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) { free(out); return NULL; }
    *out_len = (size_t)(zs.total_out);
    return out;
#else
    (void)algo; (void)data; (void)len;
    return NULL;
#endif
}

static unsigned char* gmbcp_compress_by_name(const char* algo, const unsigned char* d, size_t n, size_t* out_len)
{
    if (!algo || !*algo) algo = "gzip";
    if (strcmp(algo, "bzip2") == 0) return gmbcp_bzip2_compress(d, n, out_len);
    if (strcmp(algo, "brotli") == 0) return gmbcp_brotli_compress(d, n, out_len);
#if defined(GBMCP_HAVE_ZSTD)
    if (strcmp(algo, "zstd") == 0) return gmbcp_zstd_compress_buf(d, n, out_len);
#endif
#if defined(GBMCP_HAVE_LZ4)
    if (strcmp(algo, "lz4") == 0) return gmbcp_lz4_compress_buf(d, n, out_len);
#endif
#if defined(GBMCP_HAVE_LZMA)
    if (strcmp(algo, "xz") == 0 || strcmp(algo, "lzma") == 0) return gmbcp_lzma_compress_buf(d, n, out_len);
#endif
    return gmbcp_zlib_compress(algo, d, n, out_len);
}

static unsigned char* gmbcp_decompress_by_name(const char* algo, const unsigned char* d, size_t n, size_t* out_len)
{
    if (!algo || !*algo) algo = "gzip";
    if (strcmp(algo, "bzip2") == 0) return gmbcp_bzip2_decompress(d, n, out_len);
    if (strcmp(algo, "brotli") == 0) return gmbcp_brotli_decompress(d, n, out_len);
#if defined(GBMCP_HAVE_ZSTD)
    if (strcmp(algo, "zstd") == 0) return gmbcp_zstd_decompress_buf(d, n, out_len);
#endif
#if defined(GBMCP_HAVE_LZ4)
    if (strcmp(algo, "lz4") == 0) return gmbcp_lz4_decompress_buf(d, n, out_len);
#endif
#if defined(GBMCP_HAVE_LZMA)
    if (strcmp(algo, "xz") == 0 || strcmp(algo, "lzma") == 0) return gmbcp_lzma_decompress_buf(d, n, out_len);
#endif
    return gmbcp_zlib_decompress(algo, d, n, out_len);
}

/* testo → base64 del flusso compresso (malloc; liberare con free). */
char* gmbcp_compress_b64(const char* algo, const char* text)
{
    size_t out_len = 0;
    const char* t = text ? text : "";
    unsigned char* cb = gmbcp_compress_by_name(algo, (const unsigned char*)t, strlen(t), &out_len);
    char* b64;
    if (!cb) return NULL;
    b64 = gmbcp_b64_encode(cb, out_len);
    free(cb);
    return b64;
}

/* base64 del flusso compresso → testo NUL-terminato (malloc). */
char* gmbcp_decompress_b64(const char* algo, const char* b64)
{
    size_t raw_len = 0, out_len = 0;
    unsigned char* db = gmbcp_b64_decode(b64 ? b64 : "", &raw_len);
    unsigned char* cd = db ? gmbcp_decompress_by_name(algo, db, raw_len, &out_len) : NULL;
    char* out;
    if (db) free(db);
    if (!cd) return NULL;
    out = (char*)malloc(out_len + 1);
    if (out) { memcpy(out, cd, out_len); out[out_len] = 0; }
    free(cd);
    return out;
}
