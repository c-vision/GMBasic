/*
 * ofx_gfx_codec.c — shim FFI BC1/DXT1 (gfxcodec.inc).
 * Blocchi come array di byte DOUBLE (0..255): 48 valori RGB in ingresso, 8 in uscita.
 */
#include "gm_bc1.h"
#include <math.h>
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

int ofx_gfx_bc1_encode_block(const double* rgb, double* out)
{
    unsigned char a[48], b[8];
    if (!rgb || !out) return -1;
    to_u8(rgb, a, 48);
    gm_bc1_encode_block(a, b);
    from_u8(b, out, 8);
    return 0;
}

int ofx_gfx_bc1_decode_block(const double* in, double* out)
{
    unsigned char a[8], b[48];
    if (!in || !out) return -1;
    to_u8(in, a, 8);
    gm_bc1_decode_block(a, b);
    from_u8(b, out, 48);
    return 0;
}

int ofx_gfx_bc1_encode(const double* rgb, int w, int h, double* out)
{
    if (!rgb || !out || w <= 0 || h <= 0) return -1;
    int n = w * h * 3;
    unsigned char* a = (unsigned char*)malloc((size_t)n);
    int nout = (w / 4) * (h / 4) * 8;
    unsigned char* b = (unsigned char*)malloc((size_t)(nout > 0 ? nout : 1));
    if (!a || !b) { free(a); free(b); return -1; }
    to_u8(rgb, a, n);
    int rc = gm_bc1_encode(w, h, a, b);
    if (rc >= 0) from_u8(b, out, nout);
    free(a); free(b);
    return rc;
}

int ofx_gfx_bc1_decode(const double* in, int w, int h, double* out)
{
    if (!in || !out || w <= 0 || h <= 0) return -1;
    int nin = (w / 4) * (h / 4) * 8;
    int nout = w * h * 3;
    unsigned char* a = (unsigned char*)malloc((size_t)(nin > 0 ? nin : 1));
    unsigned char* b = (unsigned char*)malloc((size_t)nout);
    if (!a || !b) { free(a); free(b); return -1; }
    to_u8(in, a, nin);
    int rc = gm_bc1_decode(w, h, a, b);
    if (rc >= 0) from_u8(b, out, nout);
    free(a); free(b);
    return rc;
}
