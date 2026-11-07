/*
 * spectral_ffi.c — shim ABI per il namespace BASIC SPECTRAL (trasformate di gmbmath-numerical).
 *
 * Tutti gli array sono DOUBLE piatti; le dimensioni arrivano dall'ABI (dimensione implicita
 * dopo ogni puntatore), quindi non servono parametri di lunghezza per il segnale.
 *
 * Lo spettro è rappresentato come coppie (re, im) interleavate: una STFT con nf frame e
 * N/2+1 bin occupa 2*nf*(N/2+1) double, frame per frame (prima i bin del frame 0, poi
 * quelli del frame 1, ...).
 *
 * Ritorno: >= 0 = valore utile (numero di frame per STFT, campioni per ISTFT),
 *          -1 = argomento non valido o buffer troppo corto.
 */
#include "gm_transform.h"
#include "gm_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;

/* ── trasformate su una sequenza ──────────────────────────────────────────── */

i64 ofx_sp_dct(const double *x, i64 x_n, double *out, i64 out_n)
{
    if (!x || !out || x_n < 1 || out_n < x_n) return -1;
    if (gm_dct(x, (int)x_n, out) != GM_OK) return -1;
    return (i64)x_n;
}

i64 ofx_sp_dst(const double *x, i64 x_n, double *out, i64 out_n)
{
    if (!x || !out || x_n < 1 || out_n < x_n) return -1;
    if (gm_dst(x, (int)x_n, out) != GM_OK) return -1;
    return (i64)x_n;
}

i64 ofx_sp_idct(const double *x, i64 x_n, double *out, i64 out_n)
{
    if (!x || !out || x_n < 1 || out_n < x_n) return -1;
    if (gm_idct(x, (int)x_n, out) != GM_OK) return -1;
    return (i64)x_n;
}

/* ── trasformate 2D (rows x cols, row-major) ──────────────────────────────── */

i64 ofx_sp_dct2(const double *x, i64 x_n, i64 rows, i64 cols, double *out, i64 out_n)
{
    if (!x || !out || rows < 1 || cols < 1) return -1;
    if (x_n < rows * cols || out_n < rows * cols) return -1;
    if (gm_dct2(x, (int)rows, (int)cols, out) != GM_OK) return -1;
    return rows * cols;
}

i64 ofx_sp_idct2(const double *x, i64 x_n, i64 rows, i64 cols, double *out, i64 out_n)
{
    if (!x || !out || rows < 1 || cols < 1) return -1;
    if (x_n < rows * cols || out_n < rows * cols) return -1;
    if (gm_idct2(x, (int)rows, (int)cols, out) != GM_OK) return -1;
    return rows * cols;
}

/* ── Haar, Hilbert, Z ─────────────────────────────────────────────────────── */

i64 ofx_sp_haar(const double *x, i64 x_n, double *out, i64 out_n)
{
    if (!x || !out || x_n < 2 || out_n < x_n) return -1;
    if (gm_wavelet_haar(x, (int)x_n, out) != GM_OK) return -1;
    return (i64)x_n;
}

i64 ofx_sp_hilbert(const double *x, i64 x_n, double *out, i64 out_n)
{
    if (!x || !out || x_n < 1 || out_n < x_n) return -1;
    if (gm_hilbert_transform(x, (int)x_n, out) != GM_OK) return -1;
    return (i64)x_n;
}

/* Z-trasformata in z = re + i·im: out(0) = parte reale, out(1) = immaginaria. */
i64 ofx_sp_ztransform(const double *x, i64 x_n, double re, double im,
                      double *out, i64 out_n)
{
    gm_real ore = 0, oim = 0;
    if (!x || !out || x_n < 1 || out_n < 2) return -1;
    if (gm_ztransform(x, (int)x_n, re, im, &ore, &oim) != GM_OK) return -1;
    out[0] = ore;
    out[1] = oim;
    return 2;
}

/* ── STFT / iSTFT ─────────────────────────────────────────────────────────── */

/* Finestra w() di N campioni, hop; out() = 2*nf*(N/2+1) double (re,im interleaved).
 * Ritorna il numero di frame nf = (n-N)/hop + 1, oppure -1. */
i64 ofx_sp_stft(const double *x, i64 x_n, i64 N, const double *w, i64 w_n,
                i64 hop, double *out, i64 out_n)
{
    gm_cpx *buf;
    i64 nf, half, k, f;
    if (!x || !w || !out || N < 2 || hop < 1 || w_n < N || x_n < N) return -1;
    half = N / 2 + 1;
    nf = (x_n - N) / hop + 1;
    if (nf < 1 || out_n < 2 * nf * half) return -1;
    buf = (gm_cpx *)malloc((size_t)(nf * half) * sizeof(gm_cpx));
    if (!buf) return -1;
    if (gm_dsp_stft((int)x_n, x, (int)N, w, (int)hop, buf, NULL) != 0) { free(buf); return -1; }
    for (k = 0; k < nf * half; k++) { out[2 * k] = buf[k].re; out[2 * k + 1] = buf[k].im; }
    free(buf);
    (void)f;
    return nf;
}

/* X() = nf*(N/2+1) coppie (re,im), finestra w(); out() riceve n campioni (out_n).
 * Ritorna n, oppure -1. */
i64 ofx_sp_istft(const double *X, i64 X_n, i64 nframes, i64 N, const double *w, i64 w_n,
                 i64 hop, double *out, i64 out_n)
{
    gm_cpx *buf;
    i64 half, k;
    if (!X || !w || !out || N < 2 || hop < 1 || nframes < 1 || w_n < N || out_n < 1) return -1;
    half = N / 2 + 1;
    if (X_n < 2 * nframes * half) return -1;
    buf = (gm_cpx *)malloc((size_t)(nframes * half) * sizeof(gm_cpx));
    if (!buf) return -1;
    for (k = 0; k < nframes * half; k++) { buf[k].re = X[2 * k]; buf[k].im = X[2 * k + 1]; }
    if (gm_dsp_istft((int)nframes, buf, (int)N, w, (int)hop, out, (int)out_n) != 0)
    {
        free(buf);
        return -1;
    }
    free(buf);
    return out_n;
}
