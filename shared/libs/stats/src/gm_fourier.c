/* gm_fourier.c — Fourier (riscrittura). */
#include "gm_fourier.h"

#include <math.h>
#include <stdlib.h>

static int is_pow2(int n) { return n > 0 && (n & (n - 1)) == 0; }

gm_status gm_fft_cpx(gm_cpx *a, int n, int inverse)
{
    int i;
    if (a == NULL || n <= 0) return GM_ERR_ARG;
    if (!is_pow2(n)) {
        /* DFT O(n^2) */
        gm_cpx *out = (gm_cpx *)malloc((size_t)n * sizeof(gm_cpx));
        gm_real sign = inverse ? 1.0 : -1.0;
        int k, j;
        if (!out) return GM_ERR_NO_MEM;
        for (k = 0; k < n; k++) {
            gm_cpx s = gm_cpx_make(0.0, 0.0);
            for (j = 0; j < n; j++) {
                gm_real ang = sign * 6.28318530717958647692 * (gm_real)k * (gm_real)j / (gm_real)n;
                s = gm_cpx_add(s, gm_cpx_mul(a[j], gm_cpx_make(cos(ang), sin(ang))));
            }
            out[k] = s;
        }
        for (k = 0; k < n; k++) a[k] = out[k];
        free(out);
        return GM_OK;
    }
    /* bit-reversal */
    for (i = 1; i < n; i++) {
        int j = 0, bit, k = i;
        for (bit = n >> 1; bit > 0; bit >>= 1) { if (k & 1) j |= bit; k >>= 1; }
        if (j > i) { gm_cpx t = a[i]; a[i] = a[j]; a[j] = t; }
    }
    /* butterflies */
    {
        int len;
        for (len = 2; len <= n; len <<= 1) {
            gm_real ang = (inverse ? 1.0 : -1.0) * 6.28318530717958647692 / (gm_real)len;
            gm_cpx wl = gm_cpx_make(cos(ang), sin(ang));
            int start;
            for (start = 0; start < n; start += len) {
                gm_cpx w = gm_cpx_make(1.0, 0.0);
                int j;
                for (j = 0; j < len / 2; j++) {
                    gm_cpx u = a[start + j];
                    gm_cpx v = gm_cpx_mul(a[start + j + len / 2], w);
                    a[start + j] = gm_cpx_add(u, v);
                    a[start + j + len / 2] = gm_cpx_sub(u, v);
                    w = gm_cpx_mul(w, wl);
                }
            }
        }
    }
    return GM_OK;
}

gm_status gm_fft_power_spectrum(const gm_real *x, int n, gm_real *psd)
{
    gm_cpx *a;
    int i, half;
    if (x == NULL || psd == NULL || n <= 0) return GM_ERR_ARG;
    a = (gm_cpx *)malloc((size_t)n * sizeof(gm_cpx));
    if (!a) return GM_ERR_NO_MEM;
    for (i = 0; i < n; i++) a[i] = gm_cpx_make(x[i], 0.0);
    gm_fft_cpx(a, n, 0);
    half = n / 2;
    for (i = 0; i <= half; i++) psd[i] = gm_cpx_norm2(a[i]) / (gm_real)n;
    free(a);
    return GM_OK;
}

void gm_fft_smooth(gm_real *x, int n, int m)
{
    gm_real *tmp;
    int i, j;
    if (x == NULL || n <= 0 || m <= 0) return;
    tmp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!tmp) return;
    for (i = 0; i < n; i++) {
        gm_real s = 0.0;
        int cnt = 0;
        for (j = i - m; j <= i + m; j++) {
            if (j >= 0 && j < n) { s += x[j]; cnt++; }
        }
        tmp[i] = (cnt > 0) ? s / (gm_real)cnt : x[i];
    }
    for (i = 0; i < n; i++) x[i] = tmp[i];
    free(tmp);
}
