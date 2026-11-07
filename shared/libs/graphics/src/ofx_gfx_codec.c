/*
 * ofx_gfx_codec.c — shim FFI BC1/DXT1 (gfxcodec.inc).
 * Blocchi come array di byte DOUBLE (0..255): 48 valori RGB in ingresso, 8 in uscita.
 *
 * ABI FFI (2026-11-06): per OGNI parametro array, subito dopo il suo puntatore arriva un
 * `int64_t` con l'estensione (lunghezza) di quell'array — vedi
 * Decisions/FFI-Array-Multidimensionali-Implementazione.md. `w`/`h` restano parametri
 * distinti (geometria dell'immagine): non sono deducibili dalla sola lunghezza.
 */
#include "gm_bc1.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static void to_u8(const double* in, unsigned char* out, int n)
{
    for (int i = 0; i < n; i++)
    {
        double v = floor(in[i] + 0.5);
        out[i] = (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
    }
}
static void from_u8(const unsigned char* in, double* out, int n)
{
    for (int i = 0; i < n; i++) out[i] = (double)in[i];
}

int ofx_gfx_bc1_encode_block(const double* rgb, int64_t rgb_n, double* out, int64_t out_n)
{
    unsigned char a[48], b[8];
    if (!rgb || !out || rgb_n < 48 || out_n < 8) return -1;
    to_u8(rgb, a, 48);
    gm_bc1_encode_block(a, b);
    from_u8(b, out, 8);
    return 0;
}

int ofx_gfx_bc1_decode_block(const double* in, int64_t in_n, double* out, int64_t out_n)
{
    unsigned char a[8], b[48];
    if (!in || !out || in_n < 8 || out_n < 48) return -1;
    to_u8(in, a, 8);
    gm_bc1_decode_block(a, b);
    from_u8(b, out, 48);
    return 0;
}

int ofx_gfx_bc1_encode(const double* rgb, int64_t rgb_n, int64_t w, int64_t h, double* out, int64_t out_n)
{
    if (!rgb || !out || w <= 0 || h <= 0) return -1;
    int n = (int)(w * h * 3);
    if (rgb_n < n) return -1;
    unsigned char* a = (unsigned char*)malloc((size_t)n);
    int nout = (int)((w / 4) * (h / 4) * 8);
    if (out_n < nout) { free(a); return -1; }
    unsigned char* b = (unsigned char*)malloc((size_t)(nout > 0 ? nout : 1));
    if (!a || !b) { free(a); free(b); return -1; }
    to_u8(rgb, a, n);
    int rc = gm_bc1_encode((int)w, (int)h, a, b);
    if (rc >= 0) from_u8(b, out, nout);
    free(a); free(b);
    return rc;
}

int ofx_gfx_bc1_decode(const double* in, int64_t in_n, int64_t w, int64_t h, double* out, int64_t out_n)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    int nin = (int)((w / 4) * (h / 4) * 8);
    if (in_n < nin) return -1;
    int nout = (int)(w * h * 3);
    if (out_n < nout) return -1;
    unsigned char* a = (unsigned char*)malloc((size_t)(nin > 0 ? nin : 1));
    unsigned char* b = (unsigned char*)malloc((size_t)nout);
    if (!a || !b) { free(a); free(b); return -1; }
    to_u8(in, a, nin);
    int rc = gm_bc1_decode((int)w, (int)h, a, b);
    if (rc >= 0) from_u8(b, out, nout);
    free(a); free(b);
    return rc;
}

#include <stdint.h>
/* Trampolini DECLARE NATIVE (ffi_ext_native.pl): firma originale, estensioni = INT64_MAX. */
int ofx_gfx_bc1_decode__nat(const double* in, int64_t w, int64_t h, double* out) { return ofx_gfx_bc1_decode(in, INT64_MAX, w, h, out, INT64_MAX); }
int ofx_gfx_bc1_encode__nat(const double* rgb, int64_t w, int64_t h, double* out) { return ofx_gfx_bc1_encode(rgb, INT64_MAX, w, h, out, INT64_MAX); }
