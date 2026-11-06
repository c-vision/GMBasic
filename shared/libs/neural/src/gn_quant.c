/*
 * gn_quant.c — quantizzazione affine INT8 (da google/XNNPACK, BSD-3).
 */
#include "gn_quant.h"
#include <stdlib.h>
#include <math.h>

static int clamp_i8(long v)
{
    if (v < -128) return -128;
    if (v > 127) return 127;
    return (int)v;
}

void gn_quant_i8(const gn_real *x, int n, gn_real scale, int zero_point, signed char *out)
{
    int i;
    if (!x || !out || n < 1) return;
    if (scale == 0.0) scale = 1.0;
    for (i = 0; i < n; i++) {
        long q = (long)lround(x[i] / scale) + zero_point;
        out[i] = (signed char)clamp_i8(q);
    }
}

void gn_dequant_i8(const signed char *q, int n, gn_real scale, int zero_point, gn_real *out)
{
    int i;
    if (!q || !out || n < 1) return;
    for (i = 0; i < n; i++) out[i] = ((gn_real)q[i] - (gn_real)zero_point) * scale;
}

void gn_requant_i8(const int *acc, int n, gn_real in_scale, int in_zp,
                   gn_real out_scale, int out_zp, signed char *out)
{
    int i;
    gn_real ratio;
    if (!acc || !out || n < 1) return;
    if (out_scale == 0.0) out_scale = 1.0;
    ratio = in_scale / out_scale;
    for (i = 0; i < n; i++) {
        gn_real real = ((gn_real)acc[i] - (gn_real)in_zp) * in_scale;
        long q = (long)lround(real / out_scale) + out_zp;
        out[i] = (signed char)clamp_i8(q);
        (void)ratio;
    }
}

void gn_quant_params(const gn_real *x, int n, gn_real *scale, int *zero_point)
{
    int i;
    gn_real mn, mx;
    if (!x || n < 1 || !scale || !zero_point) return;
    mn = mx = x[0];
    for (i = 1; i < n; i++) { if (x[i] < mn) mn = x[i]; if (x[i] > mx) mx = x[i]; }
    if (mx - mn < 1e-12) { *scale = 1.0; *zero_point = 0; return; }
    *scale = (mx - mn) / 255.0;
    { long zp = (long)lround(-128.0 - mn / (*scale)); *zero_point = clamp_i8(zp); }
}

/* ===================== GEMM quantizzato uint8 (da QNNPACK) ===================== */

static int clamp_u8(long v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return (int)v;
}

void gn_quant_gemm_u8(unsigned char *out, const unsigned char *a, const unsigned char *b,
                      int m, int k, int n, gn_real scale_a, int zp_a,
                      gn_real scale_b, int zp_b, const int *bias,
                      gn_real scale_out, int zp_out)
{
    int i, j, l;
    gn_real ratio;
    if (!out || !a || !b || m < 1 || k < 1 || n < 1) return;
    if (scale_out == 0.0) scale_out = 1.0;
    ratio = (scale_a * scale_b) / scale_out;
    for (i = 0; i < m; i++) {
        long rowsum_a = 0;
        for (l = 0; l < k; l++) rowsum_a += a[(size_t)i * k + l];
        for (j = 0; j < n; j++) {
            long acc = 0, colsum_b = 0;
            for (l = 0; l < k; l++) {
                acc += (long)a[(size_t)i * k + l] * (long)b[(size_t)l * n + j];
                colsum_b += b[(size_t)l * n + j];
            }
            /* correzione zero-point: acc - zp_a*colsum - zp_b*rowsum + k*zp_a*zp_b */
            acc -= (long)zp_a * colsum_b;
            acc -= (long)zp_b * rowsum_a;
            acc += (long)k * zp_a * zp_b;
            if (bias) acc += bias[j];
            { long q = lround((gn_real)acc * ratio) + zp_out;
              out[(size_t)i * n + j] = (unsigned char)clamp_u8(q, 0, 255); }
        }
    }
}

void gn_quant_clamp_u8(unsigned char *x, int n, int lo, int hi)
{
    int i;
    if (!x || n < 1) return;
    if (lo < 0) lo = 0;
    if (hi > 255) hi = 255;
    for (i = 0; i < n; i++) { if (x[i] < lo) x[i] = (unsigned char)lo; else if (x[i] > hi) x[i] = (unsigned char)hi; }
}

/* ===================== Requantizzazione a shift e conv quantizzata (da NNoM) ===================== */

void gn_quant_requant_shift_i8(const int *acc, int n, int rshift, signed char *out)
{
    int i;
    int round;
    if (!acc || !out || n < 1) return;
    if (rshift < 0) rshift = 0;
    round = (rshift > 0) ? (1 << (rshift - 1)) : 0;
    for (i = 0; i < n; i++) {
        long v = ((long)acc[i] + round) >> rshift;
        if (v < -128) v = -128;
        if (v > 127) v = 127;
        out[i] = (signed char)v;
    }
}

gn_status gn_quant_conv2d_i8(signed char *out, const unsigned char *in, int c, int h, int w,
                             const signed char *weight, const int *bias, int out_c,
                             int kh, int kw, int stride, int pad,
                             int bias_shift, int out_shift)
{
    int oh, ow, oc, oy, ox, npix, kk, k, p;
    int *acc;
    if (!out || !in || !weight || c < 1 || h < 1 || w < 1 || out_c < 1) return GN_ERR_ARG;
    oh = (h + 2 * pad - kh) / stride + 1;
    ow = (w + 2 * pad - kw) / stride + 1;
    if (oh < 1 || ow < 1) return GN_ERR_ARG;
    npix = oh * ow;
    kk = c * kh * kw;
    acc = (int *)malloc((size_t)npix * sizeof(int));
    if (!acc) return GN_ERR_NO_MEM;
    for (oc = 0; oc < out_c; oc++) {
        const signed char *wk = weight + (size_t)oc * kk;
        int b = bias ? bias[oc] : 0;
        for (p = 0; p < npix; p++) acc[p] = (bias_shift > 0) ? (b << bias_shift) : b;
        for (k = 0; k < kk; k++) {
            int ci2 = k / (kh * kw);
            int rem = k % (kh * kw);
            int kyy = rem / kw, kxx = rem % kw;
            int wv = wk[k];
            if (wv == 0) continue;
            for (oy = 0; oy < oh; oy++)
                for (ox = 0; ox < ow; ox++) {
                    int iy = oy * stride - pad + kyy, ix = ox * stride - pad + kxx;
                    int v = 0;
                    if (iy >= 0 && iy < h && ix >= 0 && ix < w)
                        v = in[((size_t)ci2 * h + iy) * w + ix];
                    acc[oy * ow + ox] += wv * v;
                }
        }
        gn_quant_requant_shift_i8(acc, npix, out_shift, out + (size_t)oc * npix);
    }
    free(acc);
    return GN_OK;
}

/* ===================== Quantizzazione per-canale (da yolo_quantization) ===================== */

void gn_quant_params_channel(const gn_real *w, int out_c, int per_ch,
                             gn_real *scale, int *zp)
{
    int oc, i;
    if (!w || !scale || !zp || out_c < 1 || per_ch < 1) return;
    for (oc = 0; oc < out_c; oc++) {
        const gn_real *row = w + (size_t)oc * per_ch;
        gn_real mn = row[0], mx = row[0];
        for (i = 1; i < per_ch; i++) { if (row[i] < mn) mn = row[i]; if (row[i] > mx) mx = row[i]; }
        if (mx - mn < 1e-12) { scale[oc] = 1.0; zp[oc] = 0; }
        else {
            long z;
            scale[oc] = (mx - mn) / 255.0;
            z = lround(-mn / scale[oc]);
            if (z < 0) z = 0; if (z > 255) z = 255;
            zp[oc] = (int)z;
        }
    }
}

gn_status gn_quant_conv2d_i8_channel(unsigned char *out, const unsigned char *in,
                                     int c, int h, int w,
                                     const signed char *weight, const int *bias,
                                     const gn_real *M, int out_c, int kh, int kw,
                                     int stride, int pad, int zp_in, int zp_out)
{
    int oh, ow, oc, ky, kx, ci, oy, ox, kk, k;
    if (!out || !in || !weight || !M || c < 1 || h < 1 || w < 1 || out_c < 1) return GN_ERR_ARG;
    oh = (h + 2 * pad - kh) / stride + 1;
    ow = (w + 2 * pad - kw) / stride + 1;
    if (oh < 1 || ow < 1) return GN_ERR_ARG;
    kk = c * kh * kw;
    for (oc = 0; oc < out_c; oc++) {
        const signed char *wk = weight + (size_t)oc * kk;
        int b = bias ? bias[oc] : 0;
        gn_real m = M[oc];
        for (oy = 0; oy < oh; oy++)
            for (ox = 0; ox < ow; ox++) {
                long acc = 0;
                for (k = 0; k < kk; k++) {
                    int cidx = k / (kh * kw);
                    int rem = k % (kh * kw);
                    ky = rem / kw; kx = rem % kw;
                    {
                        int iy = oy * stride - pad + ky;
                        int ix = ox * stride - pad + kx;
                        if (iy >= 0 && iy < h && ix >= 0 && ix < w)
                            acc += (long)wk[k] * (long)((int)in[((size_t)cidx * h + iy) * w + ix] - zp_in);
                    }
                }
                { long q = lround(m * (gn_real)acc) + b + zp_out;
                  if (q < 0) q = 0; if (q > 255) q = 255;
                  out[((size_t)oc * oh + oy) * ow + ox] = (unsigned char)q; }
            }
        (void)ci;
    }
    return GN_OK;
}
