/* gs_diag.c — diagnostiche di convergenza MCMC (riscrittura C99 da volesti
 * diagnostics/: Geyer ESS, Gelman-Rubin / Brooks-Gelman PSRF, Geweke, thinning).
 * Convenzione: X[i*n + j] = dimensione i, campione j. */
#include "gs_diag.h"
#include "gs_dist.h"

#include <math.h>
#include <stdlib.h>

/* ---- helper densi ---- */

static void diag_mean(const gc_real *X, int d, int n, const int *cols, int nc, gc_real *mean)
{
    int i, k;
    for (i = 0; i < d; i++) mean[i] = 0.0;
    for (k = 0; k < nc; k++)
        for (i = 0; i < d; i++) mean[i] += X[i * n + cols[k]];
    for (i = 0; i < d; i++) mean[i] /= (gc_real)nc;
}

/* C = sum_k (x_k - mean)(x_k - mean)^T  (d×d, non normalizzata). */
static void diag_scatter(const gc_real *X, int d, int n, const int *cols, int nc,
                         const gc_real *mean, gc_real *C)
{
    int i, j, k;
    for (i = 0; i < d * d; i++) C[i] = 0.0;
    for (k = 0; k < nc; k++) {
        for (i = 0; i < d; i++) {
            gc_real di = X[i * n + cols[k]] - mean[i];
            for (j = 0; j < d; j++)
                C[i * d + j] += di * (X[j * n + cols[k]] - mean[j]);
        }
    }
}

/* Cholesky inferiore A = L L^T (A simmetrica DP). 1 = ok. */
static int diag_chol(const gc_real *A, int d, gc_real *L)
{
    int i, j, k;
    for (i = 0; i < d * d; i++) L[i] = 0.0;
    for (i = 0; i < d; i++) {
        for (j = 0; j <= i; j++) {
            gc_real s = A[i * d + j];
            for (k = 0; k < j; k++) s -= L[i * d + k] * L[j * d + k];
            if (i == j) {
                if (s <= 0.0) return 0;
                L[i * d + i] = sqrt(s);
            } else {
                L[i * d + j] = s / L[j * d + j];
            }
        }
    }
    return 1;
}

/* Inversa di L triangolare inferiore (d×d) in Linv. */
static void diag_tri_inv(const gc_real *L, int d, gc_real *Linv)
{
    int i, j, k;
    for (i = 0; i < d * d; i++) Linv[i] = 0.0;
    for (j = 0; j < d; j++) {
        Linv[j * d + j] = 1.0 / L[j * d + j];
        for (i = j + 1; i < d; i++) {
            gc_real s = 0.0;
            for (k = j; k < i; k++) s += L[i * d + k] * Linv[k * d + j];
            Linv[i * d + j] = -s / L[i * d + i];
        }
    }
}

/* C = A * B (d×d). */
static void diag_mul(const gc_real *A, const gc_real *B, int d, gc_real *C)
{
    int i, j, k;
    for (i = 0; i < d; i++)
        for (j = 0; j < d; j++) {
            gc_real s = 0.0;
            for (k = 0; k < d; k++) s += A[i * d + k] * B[k * d + j];
            C[i * d + j] = s;
        }
}

/* Massimo autovalore di una matrice simmetrica (rotazioni di Jacobi). */
static gc_real diag_sym_maxeval(const gc_real *Ain, int d)
{
    gc_real *A = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    gc_real mx;
    int sweep, p, q, i;
    if (!A) return -1.0;
    for (i = 0; i < d * d; i++) A[i] = Ain[i];
    for (sweep = 0; sweep < 100; sweep++) {
        gc_real off = 0.0;
        for (p = 0; p < d; p++) for (q = p + 1; q < d; q++) off += A[p * d + q] * A[p * d + q];
        if (sqrt(2.0 * off) < 1e-13) break;
        for (p = 0; p < d; p++) {
            for (q = p + 1; q < d; q++) {
                gc_real apq = A[p * d + q];
                gc_real theta, t, c, s;
                if (fabs(apq) < 1e-300) continue;
                theta = (A[q * d + q] - A[p * d + p]) / (2.0 * apq);
                t = (theta >= 0.0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(theta * theta + 1.0));
                c = 1.0 / sqrt(t * t + 1.0);
                s = t * c;
                for (i = 0; i < d; i++) {
                    gc_real aip = A[i * d + p], aiq = A[i * d + q];
                    A[i * d + p] = c * aip - s * aiq;
                    A[i * d + q] = s * aip + c * aiq;
                }
                for (i = 0; i < d; i++) {
                    gc_real api = A[p * d + i], aqi = A[q * d + i];
                    A[p * d + i] = c * api - s * aqi;
                    A[q * d + i] = s * api + c * aqi;
                }
            }
        }
    }
    mx = A[0];
    for (i = 1; i < d; i++) if (A[i * d + i] > mx) mx = A[i * d + i];
    free(A);
    return mx;
}

/* Inversa densa (Gauss-Jordan). 1 = ok. */
static int diag_inv(const gc_real *Ain, int d, gc_real *Ainv)
{
    gc_real *M = (gc_real *)malloc((size_t)d * (size_t)(2 * d) * sizeof(gc_real));
    int i, j, col, pr;
    if (!M) return 0;
    for (i = 0; i < d; i++) {
        for (j = 0; j < d; j++) M[i * 2 * d + j] = Ain[i * d + j];
        for (j = 0; j < d; j++) M[i * 2 * d + d + j] = (i == j) ? 1.0 : 0.0;
    }
    for (col = 0; col < d; col++) {
        gc_real piv = fabs(M[col * 2 * d + col]);
        pr = col;
        for (i = col + 1; i < d; i++)
            if (fabs(M[i * 2 * d + col]) > piv) { piv = fabs(M[i * 2 * d + col]); pr = i; }
        if (piv < 1e-14) { free(M); return 0; }
        if (pr != col)
            for (j = 0; j < 2 * d; j++) {
                gc_real t = M[col * 2 * d + j];
                M[col * 2 * d + j] = M[pr * 2 * d + j];
                M[pr * 2 * d + j] = t;
            }
        { gc_real p = M[col * 2 * d + col];
          for (j = 0; j < 2 * d; j++) M[col * 2 * d + j] /= p; }
        for (i = 0; i < d; i++) {
            gc_real f;
            if (i == col) continue;
            f = M[i * 2 * d + col];
            if (f == 0.0) continue;
            for (j = 0; j < 2 * d; j++) M[i * 2 * d + j] -= f * M[col * 2 * d + j];
        }
    }
    for (i = 0; i < d; i++) for (j = 0; j < d; j++) Ainv[i * d + j] = M[i * 2 * d + d + j];
    free(M);
    return 1;
}

/* ---- ESS (Geyer, initial positive sequence) su una catena 1-D ---- */

static gc_real diag_ess_1d(const gc_real *x, int n)
{
    gc_real mean = 0.0, var = 0.0, gap = 0.0, *z, *ac, *mip;
    int i, k, N_even, m;

    if (n < 2) return (gc_real)n;
    for (i = 0; i < n; i++) mean += x[i];
    mean /= (gc_real)n;
    for (i = 0; i < n; i++) var += (x[i] - mean) * (x[i] - mean);
    var /= (gc_real)n;

    z = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    ac = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    mip = (gc_real *)malloc((size_t)(n / 2 + 1) * sizeof(gc_real));
    if (!z || !ac || !mip) { free(z); free(ac); free(mip); return (gc_real)n; }

    { gc_real denom = sqrt(var + 1e-16);
      for (i = 0; i < n; i++) z[i] = (x[i] - mean) / denom; }

    /* autocorrelazione lineare (equivalente all'FFT con zero-padding) */
    for (k = 0; k < n; k++) {
        gc_real s = 0.0;
        for (i = 0; i + k < n; i++) s += z[i] * z[i + k];
        ac[k] = s / (gc_real)n;
    }

    N_even = n - (n % 2);
    m = N_even / 2;
    for (k = 0; k < m; k++) mip[k] = ac[2 * k] + ac[2 * k + 1];

    /* minimo cumulativo (sequenza monotona non crescente) */
    for (k = 1; k < m; k++) if (mip[k] > mip[k - 1]) mip[k] = mip[k - 1];

    for (k = 0; k < m; k++) if (mip[k] > 0.0) gap += mip[k];
    gap = 2.0 * gap - 1.0;
    if (gap < 1.0) gap = 1.0;

    free(z); free(ac); free(mip);
    return (gc_real)n / gap;
}

int gs_diag_ess(const gc_real *X, int d, int n, gc_real *ess)
{
    int i;
    if (!X || !ess || d < 1 || n < 2) return -1;
    for (i = 0; i < d; i++) {
        gc_real *row = (gc_real *)malloc((size_t)n * sizeof(gc_real));
        int j;
        if (!row) return -1;
        for (j = 0; j < n; j++) row[j] = X[i * n + j];
        ess[i] = diag_ess_1d(row, n);
        free(row);
    }
    return 0;
}

/* ---- PSRF univariato (Gelman-Rubin, split-half) ---- */

int gs_diag_psrf_univariate(const gc_real *X, int d, int n, gc_real *rhat)
{
    int i;
    if (!X || !rhat || d < 1 || n < 4) return -1;
    for (i = 0; i < d; i++) {
        const gc_real *x = X + (size_t)i * n;
        int N1 = n / 2, N2 = n - N1, j;
        gc_real m1 = 0.0, m2 = 0.0, m00, W1 = 0.0, W2 = 0.0, W, B, sigma;
        for (j = 0; j < N1; j++) m1 += x[j];
        m1 /= (gc_real)N1;
        for (j = N1; j < n; j++) m2 += x[j];
        m2 /= (gc_real)N2;
        for (j = 0; j < N1; j++) W1 += (x[j] - m1) * (x[j] - m1);
        for (j = N1; j < n; j++) W2 += (x[j] - m2) * (x[j] - m2);
        W1 = (N1 > 1) ? W1 / (gc_real)(N1 - 1) : 0.0;
        W2 = (N2 > 1) ? W2 / (gc_real)(N2 - 1) : 0.0;
        W = 0.5 * (W1 + W2);
        m00 = (m1 + m2) * 0.5;
        B = (m1 - m00) * (m1 - m00) + (m2 - m00) * (m2 - m00);
        sigma = ((gc_real)(N1 - 1) / (gc_real)N1) * W + B;
        rhat[i] = (W > 0.0) ? sqrt(sigma / W) : 1.0;
    }
    return 0;
}

/* ---- PSRF multivariato (Brooks-Gelman) ---- */

gc_real gs_diag_psrf_multivariate(const gc_real *X, int d, int n)
{
    int N1, N2, i, j, k;
    int *c1, *c2;
    gc_real *m1, *m2, *m00, *C1, *C2, *W, *B, *L, *Linv, *T, *M, lmax;

    if (!X || d < 1 || n < 4) return -1.0;
    N1 = n / 2; N2 = n - N1;
    c1 = (int *)malloc((size_t)N1 * sizeof(int));
    c2 = (int *)malloc((size_t)N2 * sizeof(int));
    m1 = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    m2 = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    m00 = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    C1 = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    C2 = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    W = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    B = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    L = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    Linv = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    T = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    M = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    if (!c1 || !c2 || !m1 || !m2 || !m00 || !C1 || !C2 || !W || !B || !L || !Linv || !T || !M) {
        free(c1); free(c2); free(m1); free(m2); free(m00); free(C1); free(C2);
        free(W); free(B); free(L); free(Linv); free(T); free(M);
        return -1.0;
    }
    for (i = 0; i < N1; i++) c1[i] = i;
    for (i = 0; i < N2; i++) c2[i] = N1 + i;

    diag_mean(X, d, n, c1, N1, m1);
    diag_mean(X, d, n, c2, N2, m2);
    for (i = 0; i < d; i++) m00[i] = 0.5 * (m1[i] + m2[i]);
    diag_scatter(X, d, n, c1, N1, m1, C1);
    diag_scatter(X, d, n, c2, N2, m2, C2);
    for (i = 0; i < d * d; i++)
        W[i] = 0.5 * (C1[i] / (gc_real)(N1 - 1) + C2[i] / (gc_real)(N2 - 1));
    for (i = 0; i < d; i++)
        for (j = 0; j < d; j++)
            B[i * d + j] = (m1[i] - m00[i]) * (m1[j] - m00[j])
                         + (m2[i] - m00[i]) * (m2[j] - m00[j]);

    /* M = L^-1 B L^-T (simmetrica), stesso spettro di W^-1 B */
    if (!diag_chol(W, d, L)) { lmax = -1.0; goto done; }
    diag_tri_inv(L, d, Linv);
    diag_mul(Linv, B, d, T);
    /* M = T * Linv^T */
    for (i = 0; i < d; i++)
        for (j = 0; j < d; j++) {
            gc_real s = 0.0;
            for (k = 0; k < d; k++) s += T[i * d + k] * Linv[j * d + k];
            M[i * d + j] = s;
        }
    lmax = diag_sym_maxeval(M, d);
    if (lmax < 0.0) { lmax = -1.0; goto done; }
    { gc_real R = ((gc_real)(N1 - 1) / (gc_real)N1) + 1.5 * lmax;
      free(c1); free(c2); free(m1); free(m2); free(m00); free(C1); free(C2);
      free(W); free(B); free(L); free(Linv); free(T); free(M);
      return R; }

done:
    free(c1); free(c2); free(m1); free(m2); free(m00); free(C1); free(C2);
    free(W); free(B); free(L); free(Linv); free(T); free(M);
    return -1.0;
}

/* ---- Geweke (Hotelling T^2 + F) ---- */

int gs_diag_geweke(const gc_real *X, int d, int n,
                   gc_real frac_first, gc_real frac_last, gc_real alpha)
{
    int N1, N2, i, j, *c1, *c2, ret;
    gc_real *m1, *m2, *C1, *C2, *S, *Sinv, *diff, *tmp, T2, U, F1, F2, df2;

    if (!X || d < 1 || n < 4 || frac_first <= 0.0 || frac_last <= 0.0
        || frac_first > 1.0 || frac_last > 1.0 || alpha <= 0.0 || alpha >= 1.0)
        return -1;
    N1 = (int)((gc_real)n * frac_first);
    N2 = (int)((gc_real)n * frac_last);
    if (N1 < 2 || N2 < 2 || N1 + N2 > n) return -1;

    c1 = (int *)malloc((size_t)N1 * sizeof(int));
    c2 = (int *)malloc((size_t)N2 * sizeof(int));
    m1 = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    m2 = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    C1 = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    C2 = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    S = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    Sinv = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    diff = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    tmp = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    if (!c1 || !c2 || !m1 || !m2 || !C1 || !C2 || !S || !Sinv || !diff || !tmp) {
        free(c1); free(c2); free(m1); free(m2); free(C1); free(C2);
        free(S); free(Sinv); free(diff); free(tmp);
        return -1;
    }
    for (i = 0; i < N1; i++) c1[i] = i;
    for (i = 0; i < N2; i++) c2[i] = n - N2 + i;

    diag_mean(X, d, n, c1, N1, m1);
    diag_mean(X, d, n, c2, N2, m2);
    diag_scatter(X, d, n, c1, N1, m1, C1);
    diag_scatter(X, d, n, c2, N2, m2, C2);
    /* covarianza pooled */
    for (i = 0; i < d * d; i++)
        S[i] = ((gc_real)(N1 - 1) * (C1[i] / (gc_real)(N1 - 1))
              + (gc_real)(N2 - 1) * (C2[i] / (gc_real)(N2 - 1)))
              / (gc_real)(N1 + N2 - 2);
    if (!diag_inv(S, d, Sinv)) { ret = -1; goto done; }

    for (i = 0; i < d; i++) diff[i] = m1[i] - m2[i];
    for (i = 0; i < d; i++) { gc_real s = 0.0; for (j = 0; j < d; j++) s += Sinv[i * d + j] * diff[j]; tmp[i] = s; }
    T2 = 0.0;
    for (i = 0; i < d; i++) T2 += diff[i] * tmp[i];
    T2 *= ((gc_real)N1 * (gc_real)N2) / (gc_real)(N1 + N2);

    U = ((gc_real)(N1 + N2 - d - 1) / ((gc_real)(N1 + N2 - 2) * (gc_real)d)) * T2;

    df2 = (gc_real)(N1 + N2 - d - 1);
    F1 = gs_dist_f_ppf((gc_real)d, df2, alpha / 2.0);
    F2 = gs_dist_f_ppf((gc_real)d, df2, 1.0 - alpha / 2.0);
    ret = (U <= F1 || U > F2) ? 0 : 1;   /* 0 = rifiuta H0, 1 = non rifiuta */

done:
    free(c1); free(c2); free(m1); free(m2); free(C1); free(C2);
    free(S); free(Sinv); free(diff); free(tmp);
    return ret;
}

/* ---- thinning ---- */

int gs_diag_thin(const gc_real *x, int n, int thin, gc_real *out)
{
    int k = 0, idx = 0;
    if (!x || !out || n < 1 || thin < 1) return -1;
    while (idx < n) { out[k++] = x[idx]; idx += thin; }
    return k;
}
