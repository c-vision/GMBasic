/* gm_eig.c — autovalori generali via QR (riscrittura). */
#include "gm_eig.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define IDX(i, j, n) ((size_t)(i) * (size_t)(n) + (size_t)(j))

gm_status gm_hessenberg(int n, gm_real *A)
{
    int k, i, j;
    if (A == NULL || n < 0) return GM_ERR_ARG;
    if (n < 3) return GM_OK;
    for (k = 0; k < n - 2; k++) {
        gm_real norm = 0.0;
        for (i = k + 1; i < n; i++) norm += A[IDX(i, k, n)] * A[IDX(i, k, n)];
        norm = sqrt(norm);
        if (norm == 0.0) continue;
        {
            gm_real alpha = (A[IDX(k + 1, k, n)] >= 0.0) ? -norm : norm;
            gm_real *v = (gm_real *)malloc((size_t)(n - k - 1) * sizeof(gm_real));
            gm_real vnorm2 = 0.0;
            if (!v) return GM_ERR_NO_MEM;
            for (i = k + 1; i < n; i++) v[i - k - 1] = A[IDX(i, k, n)];
            v[0] -= alpha;
            for (i = 0; i < n - k - 1; i++) vnorm2 += v[i] * v[i];
            if (vnorm2 != 0.0) {
                for (j = k; j < n; j++) {
                    gm_real dot = 0.0;
                    for (i = k + 1; i < n; i++) dot += v[i - k - 1] * A[IDX(i, j, n)];
                    dot = 2.0 * dot / vnorm2;
                    for (i = k + 1; i < n; i++) A[IDX(i, j, n)] -= dot * v[i - k - 1];
                }
                for (i = 0; i < n; i++) {
                    gm_real dot = 0.0;
                    for (j = k + 1; j < n; j++) dot += v[j - k - 1] * A[IDX(i, j, n)];
                    dot = 2.0 * dot / vnorm2;
                    for (j = k + 1; j < n; j++) A[IDX(i, j, n)] -= dot * v[j - k - 1];
                }
            }
            free(v);
        }
    }
    return GM_OK;
}

gm_real gm_gershgorin_radius(int n, const gm_real *A, int i)
{
    gm_real s = 0.0;
    int j;
    if (A == NULL || i < 0 || i >= n) return 0.0;
    for (j = 0; j < n; j++) if (j != i) s += fabs(A[IDX(i, j, n)]);
    return s;
}

static void givens(gm_real a, gm_real b, gm_real *c, gm_real *s)
{
    gm_real r = hypot(a, b);
    if (r == 0.0) { *c = 1.0; *s = 0.0; }
    else { *c = a / r; *s = b / r; }
}

gm_status gm_eig_qr(int n, const gm_real *A, gm_real *wr, gm_real *wi)
{
    gm_real *H;
    int hi, it = 0, maxit;
    if (A == NULL || wr == NULL || wi == NULL || n <= 0) return GM_ERR_ARG;
    H = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    if (!H) return GM_ERR_NO_MEM;
    memcpy(H, A, (size_t)n * (size_t)n * sizeof(gm_real));
    gm_hessenberg(n, H);
    hi = n - 1;
    maxit = 300 * n;
    while (hi >= 0) {
        int m = hi;
        while (m > 0) {
            gm_real thr = 1e-14 * (fabs(H[IDX(m, m, n)]) + fabs(H[IDX(m - 1, m - 1, n)]));
            if (fabs(H[IDX(m, m - 1, n)]) <= thr) { H[IDX(m, m - 1, n)] = 0.0; break; }
            m--;
        }
        if (m == hi) {
            wr[hi] = H[IDX(hi, hi, n)];
            wi[hi] = 0.0;
            hi--;
        } else if (m == hi - 1) {
            gm_real a = H[IDX(m, m, n)], b = H[IDX(m, hi, n)];
            gm_real c = H[IDX(hi, m, n)], d = H[IDX(hi, hi, n)];
            gm_real tr = a + d, det = a * d - b * c, disc = tr * tr - 4.0 * det;
            if (disc >= 0.0) {
                gm_real r = sqrt(disc);
                wr[m] = 0.5 * (tr + r); wi[m] = 0.0;
                wr[hi] = 0.5 * (tr - r); wi[hi] = 0.0;
            } else {
                gm_real r = sqrt(-disc);
                wr[m] = wr[hi] = 0.5 * tr;
                wi[m] = 0.5 * r; wi[hi] = -0.5 * r;
            }
            hi -= 2;
        } else {
            int k, i, j;
            gm_real a, b, c, d, tr, det, disc, mu, x, z;
            if (it++ >= maxit) {
                for (i = 0; i <= hi; i++) { wr[i] = H[IDX(i, i, n)]; wi[i] = 0.0; }
                break;
            }
            a = H[IDX(hi - 1, hi - 1, n)]; b = H[IDX(hi - 1, hi, n)];
            c = H[IDX(hi, hi - 1, n)]; d = H[IDX(hi, hi, n)];
            tr = a + d; det = a * d - b * c; disc = tr * tr - 4.0 * det;
            if (disc >= 0.0) {
                gm_real r = sqrt(disc), l1 = 0.5 * (tr + r), l2 = 0.5 * (tr - r);
                mu = (fabs(l1 - d) < fabs(l2 - d)) ? l1 : l2;
            } else {
                mu = 0.5 * tr;
            }
            x = H[IDX(m, m, n)] - mu;
            z = H[IDX(m + 1, m, n)];
            for (k = m; k < hi; k++) {
                gm_real cs, sn;
                givens(x, z, &cs, &sn);
                for (j = (k > m ? k - 1 : m); j <= hi; j++) {
                    gm_real t1 = H[IDX(k, j, n)], t2 = H[IDX(k + 1, j, n)];
                    H[IDX(k, j, n)] = cs * t1 + sn * t2;
                    H[IDX(k + 1, j, n)] = -sn * t1 + cs * t2;
                }
                for (i = m; i <= (k + 2 < hi ? k + 2 : hi); i++) {
                    gm_real t1 = H[IDX(i, k, n)], t2 = H[IDX(i, k + 1, n)];
                    H[IDX(i, k, n)] = cs * t1 + sn * t2;
                    H[IDX(i, k + 1, n)] = -sn * t1 + cs * t2;
                }
                if (k < hi - 1) { x = H[IDX(k + 1, k, n)]; z = H[IDX(k + 2, k, n)]; }
            }
        }
    }
    free(H);
    return GM_OK;
}

gm_status gm_eig_power(int n, const gm_real *A, gm_real *lambda, gm_real *v,
                       int iters, gm_real tol)
{
    gm_real *w;
    int i, j, k;
    if (A == NULL || lambda == NULL || v == NULL || n <= 0) return GM_ERR_ARG;
    w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!w) return GM_ERR_NO_MEM;
    for (i = 0; i < n; i++) v[i] = 1.0;
    { gm_real nrm = 0.0; for (i = 0; i < n; i++) nrm += v[i] * v[i]; nrm = sqrt(nrm); if (nrm > 0) for (i = 0; i < n; i++) v[i] /= nrm; }
    *lambda = 0.0;
    for (k = 0; k < iters; k++) {
        gm_real nrm = 0.0, newlam;
        for (i = 0; i < n; i++) { w[i] = 0.0; for (j = 0; j < n; j++) w[i] += A[IDX(i, j, n)] * v[j]; }
        for (i = 0; i < n; i++) nrm += w[i] * w[i];
        nrm = sqrt(nrm);
        if (nrm == 0.0) break;
        for (i = 0; i < n; i++) v[i] = w[i] / nrm;
        newlam = nrm;
        if (fabs(newlam - *lambda) <= tol * (1.0 + fabs(newlam))) { *lambda = newlam; break; }
        *lambda = newlam;
    }
    free(w);
    return GM_OK;
}

/* ===================== varianti del metodo delle potenze ===================== */

static gm_real eig_rayleigh(int n, const gm_real *A, const gm_real *v)
{
    gm_real num = 0.0, den = 0.0;
    int i, j;
    for (i = 0; i < n; i++) {
        gm_real av = 0.0;
        for (j = 0; j < n; j++) av += A[IDX(i, j, n)] * v[j];
        num += v[i] * av;
        den += v[i] * v[i];
    }
    return (den != 0.0) ? num / den : 0.0;
}

gm_status gm_eig_shifted_power(int n, const gm_real *A, gm_real sigma, gm_real *lambda,
                               gm_real *v, int iters, gm_real tol)
{
    gm_real *w, prev = 1e300;
    int i, j, k;
    if (n <= 0 || A == NULL || lambda == NULL || v == NULL) return GM_ERR_ARG;
    w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!w) return GM_ERR_NO_MEM;
    for (i = 0; i < n; i++) v[i] = 1.0;
    for (k = 0; k < iters; k++) {
        gm_real nrm = 0.0;
        for (i = 0; i < n; i++) {
            gm_real s = 0.0;
            for (j = 0; j < n; j++) s += A[IDX(i, j, n)] * v[j];
            w[i] = s - sigma * v[i];
        }
        for (i = 0; i < n; i++) nrm += w[i] * w[i];
        nrm = sqrt(nrm);
        if (nrm == 0.0) break;
        for (i = 0; i < n; i++) v[i] = w[i] / nrm;
        *lambda = eig_rayleigh(n, A, v) + sigma;
        if (fabs(*lambda - prev) <= tol * (1.0 + fabs(*lambda))) { free(w); return GM_OK; }
        prev = *lambda;
    }
    free(w);
    return GM_OK;
}

gm_status gm_eig_inverse_power(int n, const gm_real *A, gm_real sigma, gm_real *lambda,
                               gm_real *v, int iters, gm_real tol)
{
    gm_real *M, *w, *b;
    int i, j, k;
    if (n <= 0 || A == NULL || lambda == NULL || v == NULL) return GM_ERR_ARG;
    M = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    b = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!M || !w || !b) { free(M); free(w); free(b); return GM_ERR_NO_MEM; }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) M[IDX(i, j, n)] = A[IDX(i, j, n)] - (i == j ? sigma : 0.0);
    for (i = 0; i < n; i++) v[i] = 1.0;
    for (k = 0; k < iters; k++) {
        gm_real nrm = 0.0;
        for (i = 0; i < n; i++) b[i] = v[i];
        if (gm_linalg_solve(n, M, b, w) != GM_OK) { free(M); free(w); free(b); return GM_ERR_SINGULAR; }
        for (i = 0; i < n; i++) nrm += w[i] * w[i];
        nrm = sqrt(nrm);
        if (nrm == 0.0) break;
        for (i = 0; i < n; i++) v[i] = w[i] / nrm;
    }
    *lambda = eig_rayleigh(n, A, v);
    free(M); free(w); free(b);
    (void)tol;
    return GM_OK;
}

gm_status gm_eig_rayleigh(int n, const gm_real *A, gm_real *lambda, gm_real *v,
                          int iters, gm_real tol)
{
    gm_real *M, *w, *b;
    int i, j, k;
    if (n <= 0 || A == NULL || lambda == NULL || v == NULL) return GM_ERR_ARG;
    M = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    b = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!M || !w || !b) { free(M); free(w); free(b); return GM_ERR_NO_MEM; }
    for (i = 0; i < n; i++) v[i] = 1.0;
    { gm_real nrm = sqrt((gm_real)n); for (i = 0; i < n; i++) v[i] /= nrm; }
    *lambda = eig_rayleigh(n, A, v);
    for (k = 0; k < iters; k++) {
        gm_real nrm = 0.0, lnew;
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++) M[IDX(i, j, n)] = A[IDX(i, j, n)] - (i == j ? *lambda : 0.0);
        for (i = 0; i < n; i++) b[i] = v[i];
        if (gm_linalg_solve(n, M, b, w) != GM_OK) { free(M); free(w); free(b); return GM_OK; } /* (A-lambda I) singolare: convergiuto */
        for (i = 0; i < n; i++) nrm += w[i] * w[i];
        nrm = sqrt(nrm);
        if (nrm == 0.0) break;
        for (i = 0; i < n; i++) v[i] = w[i] / nrm;
        lnew = eig_rayleigh(n, A, v);
        if (fabs(lnew - *lambda) <= tol * (1.0 + fabs(lnew))) { *lambda = lnew; free(M); free(w); free(b); return GM_OK; }
        *lambda = lnew;
    }
    free(M); free(w); free(b);
    return GM_OK;
}
