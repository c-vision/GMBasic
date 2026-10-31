/* ============================================================================
 * gmb_bzip2.c — BZIP2 e Brotli (per COMPRESSION.BZIP2/BROTLI e relative UN-).
 * ============================================================================ */
#include "gmb_compress.h"
#include <stdlib.h>
#include <string.h>

#ifdef __has_include
#if __has_include(<bzlib.h>)
#define GMB_HAVE_BZIP2 1
#include <bzlib.h>
#endif
#else
#include <bzlib.h>
#define GMB_HAVE_BZIP2 1
#endif

unsigned char* gmb_bzip2_compress(const unsigned char* data, size_t len, size_t* out_len)
{
    *out_len = 0;
#ifdef GMB_HAVE_BZIP2
    unsigned int cap = (unsigned int)(len * 1.1) + 600;
    unsigned char* out = (unsigned char*)malloc(cap);
    if (!out) return NULL;
    unsigned int olen = cap;
    if (BZ2_bzBuffToBuffCompress((char*)out, &olen, (char*)data, (unsigned int)len, 9, 0, 0) != BZ_OK)
    { free(out); return NULL; }
    *out_len = olen;
    return out;
#else
    return NULL;
#endif
}

unsigned char* gmb_bzip2_decompress(const unsigned char* data, size_t len, size_t* out_len)
{
    *out_len = 0;
#ifdef GMB_HAVE_BZIP2
    size_t cap = len * 4 + 1024;
    unsigned char* out = (unsigned char*)malloc(cap);
    if (!out) return NULL;
    for (;;)
    {
        unsigned int olen = (unsigned int)cap;
        int rc = BZ2_bzBuffToBuffDecompress((char*)out, &olen, (char*)data, (unsigned int)len, 0, 0);
        if (rc == BZ_OK || rc == BZ_STREAM_END)
        {
            *out_len = olen;
            return out;
        }
        if (rc != BZ_OUTBUFF_FULL) { free(out); return NULL; }
        cap *= 2;
        unsigned char* nb = (unsigned char*)realloc(out, cap);
        if (!nb) { free(out); return NULL; }
        out = nb;
    }
#else
    return NULL;
#endif
}

#ifdef __has_include
#if __has_include(<brotli/encode.h>)
#define GMB_HAVE_BROTLI 1
#include <brotli/encode.h>
#include <brotli/decode.h>
#endif
#endif

unsigned char* gmb_brotli_compress(const unsigned char* data, size_t len, size_t* out_len)
{
    *out_len = 0;
#ifdef GMB_HAVE_BROTLI
    size_t cap = BrotliEncoderMaxCompressedSize(len);
    unsigned char* out = (unsigned char*)malloc(cap ? cap : len + 16);
    if (!out) return NULL;
    size_t olen = cap;
    if (!BrotliEncoderCompress(BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW, BROTLI_DEFAULT_MODE,
                               len, data, &olen, out))
    { free(out); return NULL; }
    *out_len = olen;
    return out;
#else
    return NULL;
#endif
}

unsigned char* gmb_brotli_decompress(const unsigned char* data, size_t len, size_t* out_len)
{
    *out_len = 0;
#ifdef GMB_HAVE_BROTLI
    size_t cap = len * 4 + 1024;
    unsigned char* out = (unsigned char*)malloc(cap);
    if (!out) return NULL;
    for (;;)
    {
        size_t olen = cap;
        BrotliDecoderResult r = BrotliDecoderDecompress(len, data, &olen, out);
        if (r == BROTLI_DECODER_RESULT_SUCCESS)
        {
            *out_len = olen;
            return out;
        }
        cap *= 2;
        unsigned char* nb = (unsigned char*)realloc(out, cap);
        if (!nb) { free(out); return NULL; }
        out = nb;
    }
#else
    return NULL;
#endif
}
