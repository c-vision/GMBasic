/* gm_transform.c — trasformate (riscrittura). */
#include "gm_transform.h"
#include "gm_fourier.h"
#include "gm_complex.h"

#include <math.h>
#include <stdlib.h>

static gm_real stehfest_v(int k, int n)
{
    int n2 = n / 2;
    int j;
    gm_real s = 0.0;
    double fact;
    int lo = (k + 1) / 2;
    int hi = (k < n2) ? k : n2;
    if (hi < lo) return 0.0;
    for (j = lo; j <= hi; j++) {
        /* j^(n2) * (2j)! / ((n2-j)! j! (j-1)! (k-j)! (2j-k)!) */
        double num = pow((double)j, n2);
        int i;
        for (i = 1; i <= 2 * j; i++) num *= (double)i;               /* (2j)! */
        fact = num;
        for (i = 1; i <= n2 - j; i++) fact /= (double)i;             /* (n2-j)! */
        for (i = 1; i <= j; i++) fact /= (double)i;                  /* j! */
        for (i = 1; i <= j - 1; i++) fact /= (double)i;              /* (j-1)! */
        for (i = 1; i <= k - j; i++) fact /= (double)i;              /* (k-j)! */
        for (i = 1; i <= 2 * j - k; i++) fact /= (double)i;          /* (2j-k)! */
        s += fact;
    }
    return ((k + n2) % 2 == 0) ? s : -s;
}

gm_real gm_laplace_inverse_stehfest(gm_fn1 F, void *ctx, gm_real t, int n)
{
    gm_real ln2t = 0.69314718055994530942 / t;
    gm_real s = 0.0;
    int k;
    if (F == NULL || t <= 0.0 || n < 2) return 0.0;
    if (n % 2) n++;
    for (k = 1; k <= n; k++)
        s += stehfest_v(k, n) * F(ln2t * (gm_real)k, ctx);
    return ln2t * s;
}

gm_status gm_ztransform(const gm_real *x, int n, gm_real re, gm_real im,
                        gm_real *ore, gm_real *oim)
{
    gm_real zr = 1.0, zi = 0.0, sr = 0.0, si = 0.0;
    int m;
    if (x == NULL || ore == NULL || oim == NULL || n <= 0) return GM_ERR_ARG;
    for (m = 0; m < n; m++) {
        sr += x[m] * zr;
        si += x[m] * zi;
        /* zr,zi *= 1/z = conj(z)/|z|^2 */
        {
            gm_real d = re * re + im * im;
            gm_real nr, ni;
            if (d == 0.0) return GM_ERR_SINGULAR;
            nr = (zr * re + zi * im) / d;
            ni = (zi * re - zr * im) / d;
            zr = nr; zi = ni;
        }
    }
    *ore = sr; *oim = si;
    return GM_OK;
}

gm_status gm_wavelet_haar(const gm_real *x, int n, gm_real *out)
{
    gm_real *a;
    int len, i;
    const gm_real s = 0.70710678118654752440;
    if (x == NULL || out == NULL || n <= 0 || (n & (n - 1)) != 0) return GM_ERR_ARG;
    a = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!a) return GM_ERR_NO_MEM;
    for (i = 0; i < n; i++) a[i] = x[i];
    len = n;
    while (len > 1) {
        int half = len / 2;
        gm_real *tmp = (gm_real *)malloc((size_t)len * sizeof(gm_real));
        if (!tmp) { free(a); return GM_ERR_NO_MEM; }
        for (i = 0; i < half; i++) {
            gm_real u = a[2 * i], v = a[2 * i + 1];
            tmp[i] = (u + v) * s;
            tmp[half + i] = (u - v) * s;
        }
        for (i = 0; i < len; i++) a[i] = tmp[i];
        free(tmp);
        len = half;
    }
    for (i = 0; i < n; i++) out[i] = a[i];
    free(a);
    return GM_OK;
}

gm_status gm_hilbert_transform(const gm_real *x, int n, gm_real *out)
{
    gm_cpx *X;
    int k, m;
    if (x == NULL || out == NULL || n <= 0) return GM_ERR_ARG;
    X = (gm_cpx *)malloc((size_t)n * sizeof(gm_cpx));
    if (!X) return GM_ERR_NO_MEM;
    for (k = 0; k < n; k++) {
        gm_real sr = 0.0, si = 0.0;
        for (m = 0; m < n; m++) {
            gm_real ang = -2.0 * 3.14159265358979323846 * (gm_real)k * (gm_real)m / (gm_real)n;
            sr += x[m] * cos(ang);
            si += x[m] * sin(ang);
        }
        X[k].re = sr; X[k].im = si;
    }
    for (k = 0; k < n; k++) {
        gm_real h;
        if (k == 0 || (n % 2 == 0 && k == n / 2)) h = 0.0;
        else if (k < n / 2) h = -1.0;
        else h = 1.0;
        /* moltiplica per -i*h: (re,im) -> (im*h, -re*h) */
        {
            gm_real re = X[k].re, im = X[k].im;
            X[k].re = im * h;
            X[k].im = -re * h;
        }
    }
    for (m = 0; m < n; m++) {
        gm_real sr = 0.0;
        for (k = 0; k < n; k++) {
            gm_real ang = 2.0 * 3.14159265358979323846 * (gm_real)k * (gm_real)m / (gm_real)n;
            sr += X[k].re * cos(ang) - X[k].im * sin(ang);
        }
        out[m] = sr / (gm_real)n;
    }
    free(X);
    return GM_OK;
}

gm_status gm_dct(const gm_real *x, int n, gm_real *out)
{
    int k, m;
    if (x == NULL || out == NULL || n <= 0) return GM_ERR_ARG;
    for (k = 0; k < n; k++) {
        gm_real s = 0.0;
        gm_real ck = (k == 0) ? sqrt(1.0 / (gm_real)n) : sqrt(2.0 / (gm_real)n);
        for (m = 0; m < n; m++)
            s += x[m] * cos(3.14159265358979323846 * ((gm_real)m + 0.5) * (gm_real)k / (gm_real)n);
        out[k] = ck * s;
    }
    return GM_OK;
}

gm_status gm_dst(const gm_real *x, int n, gm_real *out)
{
    int k, m;
    if (x == NULL || out == NULL || n <= 0) return GM_ERR_ARG;
    for (k = 0; k < n; k++) {
        gm_real s = 0.0;
        gm_real ck = sqrt(2.0 / (gm_real)n);
        for (m = 0; m < n; m++)
            s += x[m] * sin(3.14159265358979323846 * ((gm_real)m + 0.5) * ((gm_real)k + 1.0) / (gm_real)n);
        out[k] = ck * s;
    }
    return GM_OK;
}

/* ---- IDCT e DCT 2D (da LA-Graphics) ---- */
gm_status gm_idct(const gm_real *X, int n, gm_real *out)
{
    int m, k;
    if (X == NULL || out == NULL || n <= 0) return GM_ERR_ARG;
    for (m = 0; m < n; m++) {
        gm_real s = 0.0;
        for (k = 0; k < n; k++) {
            gm_real ck = (k == 0) ? sqrt(1.0 / (gm_real)n) : sqrt(2.0 / (gm_real)n);
            s += ck * X[k] * cos(3.14159265358979323846 * ((gm_real)m + 0.5) * (gm_real)k / (gm_real)n);
        }
        out[m] = s;
    }
    return GM_OK;
}

gm_status gm_dct2(const gm_real *x, int rows, int cols, gm_real *out)
{
    gm_real *tmp;
    int r, c;
    if (x == NULL || out == NULL || rows <= 0 || cols <= 0) return GM_ERR_ARG;
    tmp = (gm_real *)malloc((size_t)rows * (size_t)cols * sizeof(gm_real));
    if (tmp == NULL) return GM_ERR_NO_MEM;
    for (r = 0; r < rows; r++) gm_dct(x + (size_t)r * cols, cols, tmp + (size_t)r * cols);
    for (c = 0; c < cols; c++) {
        gm_real *col = (gm_real *)malloc((size_t)rows * sizeof(gm_real));
        gm_real *ocol = (gm_real *)malloc((size_t)rows * sizeof(gm_real));
        if (!col || !ocol) { free(col); free(ocol); free(tmp); return GM_ERR_NO_MEM; }
        for (r = 0; r < rows; r++) col[r] = tmp[(size_t)r * cols + c];
        gm_dct(col, rows, ocol);
        for (r = 0; r < rows; r++) out[(size_t)r * cols + c] = ocol[r];
        free(col); free(ocol);
    }
    free(tmp);
    return GM_OK;
}

gm_status gm_idct2(const gm_real *X, int rows, int cols, gm_real *out)
{
    gm_real *tmp;
    int r, c;
    if (X == NULL || out == NULL || rows <= 0 || cols <= 0) return GM_ERR_ARG;
    tmp = (gm_real *)malloc((size_t)rows * (size_t)cols * sizeof(gm_real));
    if (tmp == NULL) return GM_ERR_NO_MEM;
    for (r = 0; r < rows; r++) gm_idct(X + (size_t)r * cols, cols, tmp + (size_t)r * cols);
    for (c = 0; c < cols; c++) {
        gm_real *col = (gm_real *)malloc((size_t)rows * sizeof(gm_real));
        gm_real *ocol = (gm_real *)malloc((size_t)rows * sizeof(gm_real));
        if (!col || !ocol) { free(col); free(ocol); free(tmp); return GM_ERR_NO_MEM; }
        for (r = 0; r < rows; r++) col[r] = tmp[(size_t)r * cols + c];
        gm_idct(col, rows, ocol);
        for (r = 0; r < rows; r++) out[(size_t)r * cols + c] = ocol[r];
        free(col); free(ocol);
    }
    free(tmp);
    return GM_OK;
}

/* ===================== Hann / STFT / iSTFT (da sanoTTS) ===================== */

void gm_dsp_hann(int n, gm_real *w, int periodic)
{
    int i;
    if (!w || n < 1) return;
    for (i = 0; i < n; i++)
        w[i] = 0.5 * (1.0 - cos(2.0 * 3.14159265358979323846 * i / (gm_real)(periodic ? n : (n - 1))));
}

int gm_dsp_stft(int n, const gm_real *x, int N, const gm_real *w, int hop,
                gm_cpx *out, int *nframes)
{
    int nf, f, k, half = N / 2 + 1;
    gm_cpx *buf;
    if (n < N || N < 2 || hop < 1 || !x || !w || !out) return -1;
    buf = (gm_cpx *)malloc((size_t)N * sizeof(gm_cpx));
    if (!buf) return -1;
    nf = (n - N) / hop + 1;
    for (f = 0; f < nf; f++) {
        for (k = 0; k < N; k++) { buf[k].re = x[f * hop + k] * w[k]; buf[k].im = 0.0; }
        if (gm_fft_cpx(buf, N, 0) != GM_OK) { free(buf); return -1; }
        for (k = 0; k < half; k++) out[(size_t)f * half + k] = buf[k];
    }
    if (nframes) *nframes = nf;
    free(buf);
    return 0;
}

int gm_dsp_istft(int nframes, const gm_cpx *X, int N, const gm_real *w, int hop,
                 gm_real *x, int n)
{
    int half = N / 2 + 1, f, k, i;
    gm_real *num, *den, *win2;
    gm_cpx *buf;
    if (nframes < 1 || N < 2 || hop < 1 || !X || !w || !x || n < 1) return -1;
    num = (gm_real *)calloc((size_t)n, sizeof(gm_real));
    den = (gm_real *)calloc((size_t)n, sizeof(gm_real));
    win2 = (gm_real *)malloc((size_t)N * sizeof(gm_real));
    buf = (gm_cpx *)malloc((size_t)N * sizeof(gm_cpx));
    if (!num || !den || !win2 || !buf) { free(num); free(den); free(win2); free(buf); return -1; }
    for (k = 0; k < N; k++) win2[k] = w[k] * w[k];
    for (f = 0; f < nframes; f++) {
        /* ricostruisci lo spettro hermitiano completo */
        for (k = 0; k < half; k++) buf[k] = X[(size_t)f * half + k];
        for (k = 1; k < N - half + 1; k++) { buf[N - k].re = buf[k].re; buf[N - k].im = -buf[k].im; }
        if (gm_fft_cpx(buf, N, 1) != GM_OK) { free(num); free(den); free(win2); free(buf); return -1; }
        for (k = 0; k < N; k++) {
            i = f * hop + k;
            if (i >= n) break;
            num[i] += buf[k].re / (gm_real)N * w[k];
            den[i] += win2[k];
        }
    }
    for (i = 0; i < n; i++) x[i] = (den[i] > 1e-8) ? num[i] / den[i] : 0.0;
    free(num); free(den); free(win2); free(buf);
    return 0;
}
