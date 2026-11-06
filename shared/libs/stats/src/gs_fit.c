/* gs_fit.c — curve fitting (riscrittura). */
#include "gs_fit.h"
#include "gm_linalg.h"

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

gc_status gs_fit_cheb_coef(gc_real (*f)(gc_real, void *), void *ctx, int m, gc_real *coef)
{
    int j, k;
    if (f == NULL || coef == NULL || m <= 0) return GC_ERR_ARG;
    for (j = 0; j < m; j++) {
        gc_real sum = 0.0;
        for (k = 0; k < m; k++) {
            gc_real x = cos(3.14159265358979323846 * ((gc_real)k + 0.5) / (gc_real)m);
            sum += f(x, ctx) * cos(3.14159265358979323846 * (gc_real)j * ((gc_real)k + 0.5) / (gc_real)m);
        }
        coef[j] = 2.0 * sum / (gc_real)m;
        if (j == 0) coef[j] *= 0.5;
    }
    return GC_OK;
}

gc_real gs_fit_cheb_eval(gc_real x, const gc_real *coef, int m)
{
    gc_real b1 = 0.0, b2 = 0.0;
    int j;
    for (j = m - 1; j >= 1; j--) {
        gc_real b0 = 2.0 * x * b1 - b2 + coef[j];
        b2 = b1; b1 = b0;
    }
    return x * b1 - b2 + coef[0];
}

gc_status gs_fit_pade(const gc_real *t, int m, int nb, gc_real *a, gc_real *b)
{
    /* a[0..m-nb-1], b[1..nb]; risolve il sistema di Padé con LU. */
    int na = m - nb; /* numero di coefficienti a = m-nb+1? usiamo na coefficienti */
    int i, j;
    gc_real *M, *rhs;
    int n = nb;
    if (t == NULL || a == NULL || b == NULL || nb <= 0 || m <= nb) return GC_ERR_ARG;
    M = (gc_real *)calloc((size_t)n * (size_t)n, sizeof(gc_real));
    rhs = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!M || !rhs) { free(M); free(rhs); return GC_ERR_NO_MEM; }
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            int idx = i + n - j;
            M[(size_t)i * (size_t)n + (size_t)j] = (idx >= 0 && idx <= m) ? t[idx] : 0.0;
        }
        rhs[i] = -t[n + i + 1];
    }
    if (gm_linalg_solve(n, M, rhs, b + 1) != GC_OK) { free(M); free(rhs); return GC_ERR_SINGULAR; }
    b[0] = 1.0;
    for (i = 0; i < na; i++) {
        gc_real s = t[i];
        for (j = 1; j <= nb && j <= i; j++) s += b[j] * t[i - j];
        a[i] = s;
    }
    free(M); free(rhs);
    return GC_OK;
}

gc_real gs_fit_pade_eval(gc_real x, const gc_real *a, int na, const gc_real *b, int nb)
{
    gc_real num = 0.0, den = 0.0, xp = 1.0;
    int i;
    for (i = 0; i < na; i++) { num += a[i] * xp; xp *= x; }
    xp = 1.0;
    for (i = 0; i <= nb; i++) { den += b[i] * xp; xp *= x; }
    return (den != 0.0) ? num / den : 0.0;
}

gc_real gs_fit_poly_eval(gc_real x, const gc_real *coef, int m)
{
    gc_real s = 0.0;
    int i;
    for (i = m; i >= 0; i--) s = s * x + coef[i];
    return s;
}

gc_real gs_fit_poly_ls(const gc_real *x, const gc_real *y, int n, int m, gc_real *coef)
{
    /* equazioni normali (A^T A) c = A^T y, con A = Vandermonde */
    int i, j, k;
    gc_real *N, *r, ssq = 0.0;
    N = (gc_real *)calloc((size_t)(m + 1) * (size_t)(m + 1), sizeof(gc_real));
    r = (gc_real *)calloc((size_t)(m + 1), sizeof(gc_real));
    if (!N || !r) { free(N); free(r); return 0.0; }
    for (i = 0; i < n; i++) {
        for (j = 0; j <= m; j++) {
            gc_real xj = pow(x[i], (gc_real)j);
            r[j] += xj * y[i];
            for (k = 0; k <= m; k++) N[(size_t)j * (size_t)(m + 1) + (size_t)k] += xj * pow(x[i], (gc_real)k);
        }
    }
    if (gm_linalg_solve(m + 1, N, r, coef) != GC_OK) { free(N); free(r); return 0.0; }
    for (i = 0; i < n; i++) {
        gc_real e = y[i] - gs_fit_poly_eval(x[i], coef, m);
        ssq += e * e;
    }
    free(N); free(r);
    return ssq;
}

gc_real gs_fit_qr_ls(gc_real *A, gc_real *b, int m, int n)
{
    /* Householder QR su A (m×n row-major), poi risolve R x = Q^T b. */
    int k, i, j;
    gc_real ssq = 0.0;
    for (k = 0; k < n; k++) {
        gc_real norm = 0.0, alpha, beta;
        for (i = k; i < m; i++) norm += A[(size_t)i * (size_t)n + (size_t)k] * A[(size_t)i * (size_t)n + (size_t)k];
        norm = sqrt(norm);
        if (norm == 0.0) continue;
        alpha = A[(size_t)k * (size_t)n + (size_t)k];
        beta = (alpha >= 0.0) ? -norm : norm;
        /* v = x + beta e1 ... applica riflessione a A e b */
        {
            gc_real *v = (gc_real *)malloc((size_t)(m - k) * sizeof(gc_real));
            gc_real vnorm2 = 0.0, vtb = 0.0;
            if (!v) return 0.0;
            for (i = k; i < m; i++) {
                v[i - k] = A[(size_t)i * (size_t)n + (size_t)k];
                vnorm2 += v[i - k] * v[i - k];
            }
            v[0] -= beta;
            vnorm2 = 0.0;
            for (i = k; i < m; i++) vnorm2 += v[i - k] * v[i - k];
            if (vnorm2 == 0.0) { free(v); continue; }
            for (i = k; i < m; i++) vtb += v[i - k] * b[i];
            for (i = k; i < m; i++) b[i] -= 2.0 * vtb * v[i - k] / vnorm2;
            for (j = k; j < n; j++) {
                gc_real vta = 0.0;
                for (i = k; i < m; i++) vta += v[i - k] * A[(size_t)i * (size_t)n + (size_t)j];
                for (i = k; i < m; i++) A[(size_t)i * (size_t)n + (size_t)j] -= 2.0 * vta * v[i - k] / vnorm2;
            }
            free(v);
        }
    }
    /* back substitution R x = b (in b) */
    for (i = n - 1; i >= 0; i--) {
        gc_real s = b[i];
        for (j = i + 1; j < n; j++) s -= A[(size_t)i * (size_t)n + (size_t)j] * b[j];
        b[i] = (A[(size_t)i * (size_t)n + (size_t)i] != 0.0) ? s / A[(size_t)i * (size_t)n + (size_t)i] : 0.0;
    }
    for (i = n; i < m; i++) ssq += b[i] * b[i];
    return ssq;
}

/* ---- regressione lineare via discesa del gradiente (da nct) ---- */

int gs_regression_gd(int n, const gc_real *x, const gc_real *y, gc_real lr,
                     int epochs, int kind, int batch, gc_real *out, gc_real *loss)
{
    gc_real w = 0.0, b = 0.0;
    int ep, i;
    if (n <= 0 || !x || !y || !out || lr <= 0.0 || epochs <= 0) return -1;
    if (kind == 1) {                       /* SGD: un esempio per passo */
        unsigned rs = 12345u;
        int *idx = (int *)malloc((size_t)n * sizeof(int));
        if (!idx) return -1;
        for (i = 0; i < n; i++) idx[i] = i;
        for (ep = 0; ep < epochs; ep++) {
            for (i = n - 1; i > 0; i--) {
                int j;
                rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
                j = (int)(rs % (unsigned)(i + 1));
                { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
            }
            for (i = 0; i < n; i++) {
                int k = idx[i];
                gc_real e = w * x[k] + b - y[k];
                w -= lr * 2.0 * e * x[k];
                b -= lr * 2.0 * e;
            }
        }
        free(idx);
    } else {
        int bs = (kind == 2) ? (batch > 0 ? batch : 1) : n;
        int start;
        for (ep = 0; ep < epochs; ep++) {
            for (start = 0; start < n; start += bs) {
                int end = (start + bs < n) ? start + bs : n;
                gc_real gw = 0.0, gb = 0.0;
                int cnt = end - start;
                for (i = start; i < end; i++) {
                    gc_real e = w * x[i] + b - y[i];
                    gw += e * x[i];
                    gb += e;
                }
                w -= lr * 2.0 * gw / (gc_real)cnt;
                b -= lr * 2.0 * gb / (gc_real)cnt;
            }
        }
    }
    out[0] = w;
    out[1] = b;
    if (loss) {
        gc_real mse = 0.0;
        for (i = 0; i < n; i++) { gc_real e = w * x[i] + b - y[i]; mse += e * e; }
        *loss = mse / (gc_real)n;
    }
    return 0;
}

/* R² (coefficiente di determinazione) della retta dei minimi quadrati sui punti
 * (x_i = i+1, y_i = data[i]); salta EMPTY_VALUE (DBL_MAX) e i valori non finiti.
 * Semantica fedele a RSquared.mqh del libro MQL5 (Parte 7).
 * Restituisce in *r2 il coefficiente e in *angle la pendenza (tangente). */
void gs_rsquared(const gc_real *data, int n, gc_real *r2, gc_real *angle)
{
    int i, k = 0;
    gc_real Sx = 0, Sy = 0, Sxy = 0, Sx2 = 0, Sy2 = 0;
    if (r2) *r2 = 0;
    if (angle) *angle = 0;
    if (data == NULL || n <= 2) return;
    for (i = 0; i < n; ++i) {
        gc_real v = data[i];
        if (v == (gc_real)DBL_MAX || !isfinite((double)v)) continue;
        { gc_real x = (gc_real)(i + 1);
          Sx += x; Sy += v; Sxy += x * v; Sx2 += x * x; Sy2 += v * v; ++k; }
    }
    if (k <= 2) return;
    { gc_real nn = (gc_real)k;
      gc_real Sx22 = Sx * Sx / nn, Sy22 = Sy * Sy / nn, SxSy = Sx * Sy / nn;
      gc_real div = (Sx2 - Sx22) * (Sy2 - Sy22);
      if (fabs((double)div) < 2.220446049250313e-16) return;
      if (r2) *r2 = (Sxy - SxSy) * (Sxy - SxSy) / div;
      if (angle) *angle = (Sxy - SxSy) / (Sx2 - Sx22);
    }
}

/* Criterio del libro: R² pesato (1 - 1/sqrt(n+1)), negativo se pendenza < 0.
 * n = dimensione ORIGINALE dell'array (per il peso); i buchi restano. */
gc_real gs_rsquared_test(const gc_real *data, int n)
{
    gc_real r2 = 0, angle = 0, weight;
    gs_rsquared(data, n, &r2, &angle);
    weight = 1.0 - 1.0 / sqrt((double)n + 1.0);
    if (angle < 0) return -fabs((double)r2) * weight;
    return r2 * weight;
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
gc_real gs_fit_cheb_eval__ffi(gc_real x, const gc_real *coef, int64_t coef_ext, int m) { return gs_fit_cheb_eval(x, coef, m); }
gc_real gs_fit_pade_eval__ffi(gc_real x, const gc_real *a, int64_t a_ext, int na, const gc_real *b, int64_t b_ext, int nb) { return gs_fit_pade_eval(x, a, na, b, nb); }
gc_real gs_fit_poly_eval__ffi(gc_real x, const gc_real *coef, int64_t coef_ext, int m) { return gs_fit_poly_eval(x, coef, m); }
gc_real gs_rsquared_test__ffi(const gc_real *data, int64_t data_ext, int n) { return gs_rsquared_test(data, n); }
