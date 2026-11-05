/* gm_linalg.c — algebra lineare densa SIMD. */
#include "gm_linalg.h"
#include "gc_array.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define AT(A, ld, r, c) ((A)[(size_t)(r) * (size_t)(ld) + (size_t)(c)])
#define IDX(i, j, n) ((size_t)(i) * (size_t)(n) + (size_t)(j))

gm_status gm_linalg_lu(int n, gm_real *A, int *piv)
{
    gm_real anorm = 0.0;
    gm_real tol;
    int i, j, k;

    if (n <= 0 || A == NULL) return GM_ERR_ARG;
    for (i = 0; i < n * n; i++) {
        gm_real v = fabs(A[i]);
        if (v > anorm) anorm = v;
    }
    tol = (anorm > 0.0) ? (gm_real)n * DBL_EPSILON * anorm : 0.0;

    for (k = 0; k < n; k++) {
        int p = k;
        gm_real best = fabs(AT(A, n, k, k));
        for (i = k + 1; i < n; i++) {
            gm_real v = fabs(AT(A, n, i, k));
            if (v > best) { best = v; p = i; }
        }
        if (piv) piv[k] = p;
        if (best <= tol) return GM_ERR_SINGULAR;
        if (p != k) {
            for (j = 0; j < n; j++) {
                gm_real t = AT(A, n, k, j);
                AT(A, n, k, j) = AT(A, n, p, j);
                AT(A, n, p, j) = t;
            }
        }
        {
            gm_real pivval = AT(A, n, k, k);
            for (i = k + 1; i < n; i++) {
                gm_real m = AT(A, n, i, k) / pivval;
                AT(A, n, i, k) = m;
                /* riga i -= m * riga k (parte a destra) */
                gc_array_axpy(-m, A + (size_t)k * (size_t)n + (k + 1),
                              A + (size_t)i * (size_t)n + (k + 1), n - k - 1);
            }
        }
    }
    return GM_OK;
}

gm_status gm_linalg_lu_solve(int n, const gm_real *LU, const int *piv,
                             const gm_real *b, gm_real *x)
{
    int i, j;
    if (n <= 0 || LU == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    for (i = 0; i < n; i++) x[i] = b[i];
    if (piv) {
        for (i = 0; i < n; i++) {
            int p = piv[i];
            if (p != i) {
                gm_real t = x[i];
                x[i] = x[p];
                x[p] = t;
            }
        }
    }
    /* forward: L y = P b (L unitaria) */
    for (i = 0; i < n; i++) {
        gm_real s = x[i];
        for (j = 0; j < i; j++) s -= AT(LU, n, i, j) * x[j];
        x[i] = s;
    }
    /* back: U x = y */
    for (i = n - 1; i >= 0; i--) {
        gm_real s = x[i];
        for (j = i + 1; j < n; j++) s -= AT(LU, n, i, j) * x[j];
        x[i] = s / AT(LU, n, i, i);
    }
    return GM_OK;
}

gm_status gm_linalg_solve(int n, const gm_real *A, const gm_real *b, gm_real *x)
{
    gm_real *lu;
    int *piv;
    gm_status st;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    lu = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    piv = (int *)malloc((size_t)n * sizeof(int));
    if (lu == NULL || piv == NULL) { free(lu); free(piv); return GM_ERR_NO_MEM; }
    memcpy(lu, A, (size_t)n * (size_t)n * sizeof(gm_real));
    st = gm_linalg_lu(n, lu, piv);
    if (st == GM_OK) st = gm_linalg_lu_solve(n, lu, piv, b, x);
    free(lu);
    free(piv);
    return st;
}

gm_status gm_linalg_cholesky(int n, gm_real *A)
{
    int i, j;
    if (n <= 0 || A == NULL) return GM_ERR_ARG;
    for (j = 0; j < n; j++) {
        gm_real s = AT(A, n, j, j);
        gm_real ljj;
        for (i = 0; i < j; i++) s -= AT(A, n, j, i) * AT(A, n, j, i);
        if (s <= 0.0) return GM_ERR_ARG; /* non SPD */
        ljj = sqrt(s);
        AT(A, n, j, j) = ljj;
        for (i = j + 1; i < n; i++) {
            gm_real v = AT(A, n, i, j);
            gm_real d = gc_array_dot(A + (size_t)i * (size_t)n,
                                     A + (size_t)j * (size_t)n, j);
            AT(A, n, i, j) = (v - d) / ljj;
        }
    }
    /* azzera il triangolo superiore */
    for (j = 1; j < n; j++)
        for (i = 0; i < j; i++) AT(A, n, i, j) = 0.0;
    return GM_OK;
}

gm_status gm_linalg_chol_solve(int n, const gm_real *L, const gm_real *b, gm_real *x)
{
    int i, j;
    if (n <= 0 || L == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    /* forward: L y = b */
    for (i = 0; i < n; i++) {
        gm_real s = b[i];
        for (j = 0; j < i; j++) s -= AT(L, n, i, j) * x[j];
        x[i] = s / AT(L, n, i, i);
    }
    /* back: Lᵀ x = y */
    for (i = n - 1; i >= 0; i--) {
        gm_real s = x[i];
        for (j = i + 1; j < n; j++) s -= AT(L, n, j, i) * x[j];
        x[i] = s / AT(L, n, i, i);
    }
    return GM_OK;
}

gm_real gm_linalg_det(int n, const gm_real *A)
{
    gm_real *lu;
    int *piv;
    gm_real det = 1.0;
    int i, swaps = 0;
    if (n <= 0 || A == NULL) return 0.0;
    lu = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    piv = (int *)malloc((size_t)n * sizeof(int));
    if (lu == NULL || piv == NULL) { free(lu); free(piv); return 0.0; }
    memcpy(lu, A, (size_t)n * (size_t)n * sizeof(gm_real));
    if (gm_linalg_lu(n, lu, piv) != GM_OK) {
        free(lu); free(piv);
        return 0.0;
    }
    for (i = 0; i < n; i++) {
        det *= AT(lu, n, i, i);
        if (piv[i] != i) swaps++;
    }
    if (swaps & 1) det = -det;
    free(lu);
    free(piv);
    return det;
}

gm_status gm_linalg_inverse(int n, const gm_real *A, gm_real *inv)
{
    gm_real *lu;
    int *piv;
    gm_real *e;
    gm_real *col;
    int i, j;
    gm_status st = GM_OK;
    if (n <= 0 || A == NULL || inv == NULL) return GM_ERR_ARG;
    lu = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    piv = (int *)malloc((size_t)n * sizeof(int));
    e = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    col = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (lu == NULL || piv == NULL || e == NULL || col == NULL) {
        free(lu); free(piv); free(e); free(col);
        return GM_ERR_NO_MEM;
    }
    memcpy(lu, A, (size_t)n * (size_t)n * sizeof(gm_real));
    st = gm_linalg_lu(n, lu, piv);
    if (st == GM_OK) {
        for (j = 0; j < n; j++) {
            for (i = 0; i < n; i++) e[i] = (i == j) ? 1.0 : 0.0;
            gm_linalg_lu_solve(n, lu, piv, e, col);
            for (i = 0; i < n; i++) AT(inv, n, i, j) = col[i];
        }
    }
    free(lu);
    free(piv);
    free(e);
    free(col);
    return st;
}

void gm_linalg_transpose(int m, int n, const gm_real *A, int lda, gm_real *B, int ldb)
{
    int i, j;
    if (A == NULL || B == NULL || m <= 0 || n <= 0) return;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) AT(B, ldb, j, i) = AT(A, lda, i, j);
}

void gm_linalg_identity(int n, gm_real *A, int lda)
{
    int i, j;
    if (A == NULL || n <= 0) return;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) AT(A, lda, i, j) = (i == j) ? 1.0 : 0.0;
}

/* ===================== fattorizzazioni di Householder (da plasma/numruby) ===================== */

gm_status gm_linalg_qr(int m, int n, gm_real *A, int lda, gm_real *tau)
{
    int k, i, j;
    if (A == NULL || tau == NULL || m < 0 || n < 0 || m < n) return GM_ERR_ARG;
    for (k = 0; k < n; k++) {
        int len = m - k;
        gm_real norm = 0.0, alpha, beta, vnorm2 = 1.0, v0;
        for (i = 0; i < len; i++) norm += AT(A, lda, k + i, k) * AT(A, lda, k + i, k);
        norm = sqrt(norm);
        if (norm == 0.0) { tau[k] = 0.0; continue; }
        alpha = (AT(A, lda, k, k) >= 0.0) ? -norm : norm;
        v0 = AT(A, lda, k, k) - alpha;
        AT(A, lda, k, k) = 1.0;                 /* v[0] = 1 (normalizzato) */
        for (i = 1; i < len; i++) AT(A, lda, k + i, k) /= v0;
        for (i = 1; i < len; i++) vnorm2 += AT(A, lda, k + i, k) * AT(A, lda, k + i, k);
        beta = 2.0 / vnorm2;
        tau[k] = beta;
        for (j = k + 1; j < n; j++) {
            gm_real dot = AT(A, lda, k, j);
            for (i = 1; i < len; i++) dot += AT(A, lda, k + i, k) * AT(A, lda, k + i, j);
            dot *= beta;
            AT(A, lda, k, j) -= dot;
            for (i = 1; i < len; i++) AT(A, lda, k + i, j) -= dot * AT(A, lda, k + i, k);
        }
        AT(A, lda, k, k) = alpha;
    }
    return GM_OK;
}

gm_status gm_linalg_lstsq(int m, int n, const gm_real *A, const gm_real *b, gm_real *x)
{
    gm_real *W, *y;
    gm_real *tau;
    int k, i, j;
    if (A == NULL || b == NULL || x == NULL || m < n || n <= 0) return GM_ERR_ARG;
    W = (gm_real *)malloc((size_t)m * (size_t)n * sizeof(gm_real));
    y = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    tau = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!W || !y || !tau) { free(W); free(y); free(tau); return GM_ERR_NO_MEM; }
    memcpy(W, A, (size_t)m * (size_t)n * sizeof(gm_real));
    memcpy(y, b, (size_t)m * sizeof(gm_real));
    gm_linalg_qr(m, n, W, n, tau);
    for (k = 0; k < n; k++) {
        int len = m - k;
        gm_real dot;
        if (tau[k] == 0.0) continue;
        dot = y[k];
        for (i = 1; i < len; i++) dot += W[(size_t)(k + i) * (size_t)n + (size_t)k] * y[k + i];
        dot *= tau[k];
        y[k] -= dot;
        for (i = 1; i < len; i++) y[k + i] -= dot * W[(size_t)(k + i) * (size_t)n + (size_t)k];
    }
    for (i = n - 1; i >= 0; i--) {
        gm_real s = y[i];
        for (j = i + 1; j < n; j++) s -= W[(size_t)i * (size_t)n + (size_t)j] * x[j];
        x[i] = (W[(size_t)i * (size_t)n + (size_t)i] != 0.0) ? s / W[(size_t)i * (size_t)n + (size_t)i] : 0.0;
    }
    free(W); free(y); free(tau);
    return GM_OK;
}

gm_status gm_linalg_lq(int m, int n, gm_real *A, int lda, gm_real *tau)
{
    int k, i, j;
    if (A == NULL || tau == NULL || m < 0 || n < 0 || m > n) return GM_ERR_ARG;
    for (k = 0; k < m; k++) {
        int len = n - k;
        gm_real norm = 0.0, alpha, beta, vnorm2 = 1.0, v0;
        for (i = 0; i < len; i++) norm += AT(A, lda, k, k + i) * AT(A, lda, k, k + i);
        norm = sqrt(norm);
        if (norm == 0.0) { tau[k] = 0.0; continue; }
        alpha = (AT(A, lda, k, k) >= 0.0) ? -norm : norm;
        v0 = AT(A, lda, k, k) - alpha;
        AT(A, lda, k, k) = 1.0;
        for (i = 1; i < len; i++) AT(A, lda, k, k + i) /= v0;
        for (i = 1; i < len; i++) vnorm2 += AT(A, lda, k, k + i) * AT(A, lda, k, k + i);
        beta = 2.0 / vnorm2;
        tau[k] = beta;
        for (i = k + 1; i < m; i++) {
            gm_real dot = AT(A, lda, i, k);
            for (j = 1; j < len; j++) dot += AT(A, lda, i, k + j) * AT(A, lda, k, k + j);
            dot *= beta;
            AT(A, lda, i, k) -= dot;
            for (j = 1; j < len; j++) AT(A, lda, i, k + j) -= dot * AT(A, lda, k, k + j);
        }
        AT(A, lda, k, k) = alpha;
    }
    return GM_OK;
}

gm_status gm_linalg_lq_minnorm(int m, int n, const gm_real *A, const gm_real *b, gm_real *x)
{
    gm_real *W, *M, *tau, *y;
    int i, j, k;
    if (A == NULL || b == NULL || x == NULL || m > n || m <= 0) return GM_ERR_ARG;
    W = (gm_real *)malloc((size_t)m * (size_t)n * sizeof(gm_real));
    M = (gm_real *)calloc((size_t)m * (size_t)m, sizeof(gm_real));
    tau = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    y = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    if (!W || !M || !tau || !y) { free(W); free(M); free(tau); free(y); return GM_ERR_NO_MEM; }
    memcpy(W, A, (size_t)m * (size_t)n * sizeof(gm_real));
    gm_linalg_lq(m, n, W, n, tau);
    /* M = L Lᵀ (L = triangolo inferiore di W, m×m) */
    for (i = 0; i < m; i++)
        for (j = 0; j <= i; j++) {
            gm_real s = 0.0;
            for (k = 0; k <= (i < j ? i : j); k++) s += W[(size_t)i * (size_t)n + (size_t)k] * W[(size_t)j * (size_t)n + (size_t)k];
            M[(size_t)i * (size_t)m + (size_t)j] = s;
            M[(size_t)j * (size_t)m + (size_t)i] = s;
        }
    if (gm_linalg_solve(m, M, b, y) != GM_OK) { free(W); free(M); free(tau); free(y); return GM_ERR_SINGULAR; }
    for (j = 0; j < n; j++) {
        gm_real s = 0.0;
        for (i = 0; i < m; i++) s += A[(size_t)i * (size_t)n + (size_t)j] * y[i];
        x[j] = s;
    }
    free(W); free(M); free(tau); free(y);
    return GM_OK;
}

gm_status gm_linalg_sym_tridiag(int n, const gm_real *Ain, gm_real *d, gm_real *e)
{
    gm_real *A, *v, *p, *w;
    int k, i, j;
    if (Ain == NULL || d == NULL || e == NULL || n <= 0) return GM_ERR_ARG;
    A = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    v = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    p = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!A || !v || !p || !w) { free(A); free(v); free(p); free(w); return GM_ERR_NO_MEM; }
    memcpy(A, Ain, (size_t)n * (size_t)n * sizeof(gm_real));
    for (k = 0; k < n - 2; k++) {
        int len = n - 1 - k;
        gm_real norm = 0.0, alpha, beta = 0.0, vnorm2 = 0.0, pv;
        for (i = 0; i < len; i++) norm += AT(A, n, k + 1 + i, k) * AT(A, n, k + 1 + i, k);
        norm = sqrt(norm);
        if (norm == 0.0) { AT(A, n, k + 1, k) = 0.0; AT(A, n, k, k + 1) = 0.0; continue; }
        alpha = (AT(A, n, k + 1, k) >= 0.0) ? -norm : norm;
        for (i = 0; i < len; i++) v[i] = AT(A, n, k + 1 + i, k);
        v[0] -= alpha;
        for (i = 0; i < len; i++) vnorm2 += v[i] * v[i];
        if (vnorm2 != 0.0) {
            beta = 2.0 / vnorm2;
            for (i = 0; i < len; i++) {
                gm_real s = 0.0;
                for (j = 0; j < len; j++) s += AT(A, n, k + 1 + i, k + 1 + j) * v[j];
                p[i] = beta * s;
            }
            pv = 0.0;
            for (i = 0; i < len; i++) pv += p[i] * v[i];
            for (i = 0; i < len; i++) w[i] = p[i] - 0.5 * beta * pv * v[i];
            for (i = 0; i < len; i++)
                for (j = 0; j < len; j++)
                    AT(A, n, k + 1 + i, k + 1 + j) -= v[i] * w[j] + w[i] * v[j];
        }
        AT(A, n, k + 1, k) = alpha;
        AT(A, n, k, k + 1) = alpha;
        for (i = 1; i < len; i++) { AT(A, n, k + 1 + i, k) = 0.0; AT(A, n, k, k + 1 + i) = 0.0; }
    }
    for (i = 0; i < n; i++) d[i] = AT(A, n, i, i);
    for (i = 0; i < n; i++) e[i] = (i < n - 1) ? AT(A, n, i + 1, i) : 0.0;
    free(A); free(v); free(p); free(w);
    return GM_OK;
}

gm_status gm_linalg_tridiag_eig(int n, gm_real *d, gm_real *e, gm_real *eval)
{
    int l, m, iter, i;
    if (d == NULL || e == NULL || eval == NULL || n <= 0) return GM_ERR_ARG;
    /* e[0..n-2] contiene già le subdiagonali; e[n-1]=0 non usato. */
    e[n - 1] = 0.0;
    for (l = 0; l < n; l++) {
        iter = 0;
        do {
            gm_real g, r = 0.0, s, c, p, f, b;
            for (m = l; m < n - 1; m++) {
                gm_real dd = fabs(d[m]) + fabs(d[m + 1]);
                if (fabs(e[m]) <= 1e-15 * dd) break;
            }
            if (m != l) {
                if (iter++ >= 100) return GM_ERR_NOCONV;
                g = (d[l + 1] - d[l]) / (2.0 * e[l]);
                r = hypot(g, 1.0);
                g = d[m] - d[l] + e[l] / (g + (g >= 0.0 ? fabs(r) : -fabs(r)));
                s = c = 1.0;
                p = 0.0;
                for (i = m - 1; i >= l; i--) {
                    f = s * e[i];
                    b = c * e[i];
                    r = hypot(f, g);
                    e[i + 1] = r;
                    if (r == 0.0) { d[i + 1] -= p; e[m] = 0.0; break; }
                    s = f / r;
                    c = g / r;
                    g = d[i + 1] - p;
                    r = (d[i] - g) * s + 2.0 * c * b;
                    d[i + 1] = g + (p = s * r);
                    g = c * r - b;
                }
                if (r == 0.0 && i >= l) continue;
                d[l] -= p;
                e[l] = g;
                e[m] = 0.0;
            }
        } while (m != l);
    }
    for (i = 0; i < n; i++) eval[i] = d[i];
    return GM_OK;
}

gm_status gm_linalg_svd(int m, int n, const gm_real *A, gm_real *U, gm_real *S, gm_real *Vt)
{
    gm_real *B, *V;
    int i, j, p, q, sweep;
    if (A == NULL || U == NULL || S == NULL || Vt == NULL || m < n || n <= 0) return GM_ERR_ARG;
    B = (gm_real *)malloc((size_t)m * (size_t)n * sizeof(gm_real));
    V = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    if (!B || !V) { free(B); free(V); return GM_ERR_NO_MEM; }
    memcpy(B, A, (size_t)m * (size_t)n * sizeof(gm_real));
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) V[(size_t)i * (size_t)n + (size_t)j] = (i == j) ? 1.0 : 0.0;
    for (sweep = 0; sweep < 100; sweep++) {
        gm_real off = 0.0;
        for (p = 0; p < n - 1; p++)
            for (q = p + 1; q < n; q++) {
                gm_real a = 0.0, bb = 0.0, cc = 0.0, zeta, t, cs, sn;
                for (i = 0; i < m; i++) {
                    a += B[(size_t)i * (size_t)n + (size_t)p] * B[(size_t)i * (size_t)n + (size_t)p];
                    bb += B[(size_t)i * (size_t)n + (size_t)q] * B[(size_t)i * (size_t)n + (size_t)q];
                    cc += B[(size_t)i * (size_t)n + (size_t)p] * B[(size_t)i * (size_t)n + (size_t)q];
                }
                if (fabs(cc) <= 1e-15 * sqrt(a * bb)) continue;
                off += cc * cc;
                zeta = (bb - a) / (2.0 * cc);
                t = (zeta >= 0.0 ? 1.0 : -1.0) / (fabs(zeta) + sqrt(1.0 + zeta * zeta));
                cs = 1.0 / sqrt(1.0 + t * t);
                sn = cs * t;
                for (i = 0; i < m; i++) {
                    gm_real bp = B[(size_t)i * (size_t)n + (size_t)p], bq = B[(size_t)i * (size_t)n + (size_t)q];
                    B[(size_t)i * (size_t)n + (size_t)p] = cs * bp - sn * bq;
                    B[(size_t)i * (size_t)n + (size_t)q] = sn * bp + cs * bq;
                }
                for (i = 0; i < n; i++) {
                    gm_real vp = V[(size_t)i * (size_t)n + (size_t)p], vq = V[(size_t)i * (size_t)n + (size_t)q];
                    V[(size_t)i * (size_t)n + (size_t)p] = cs * vp - sn * vq;
                    V[(size_t)i * (size_t)n + (size_t)q] = sn * vp + cs * vq;
                }
            }
        if (off < 1e-28) break;
    }
    for (j = 0; j < n; j++) {
        gm_real nrm = 0.0;
        for (i = 0; i < m; i++) nrm += B[(size_t)i * (size_t)n + (size_t)j] * B[(size_t)i * (size_t)n + (size_t)j];
        S[j] = sqrt(nrm);
    }
    for (j = 0; j < n; j++)
        for (i = 0; i < m; i++)
            U[(size_t)i * (size_t)n + (size_t)j] = (S[j] > 0.0) ? B[(size_t)i * (size_t)n + (size_t)j] / S[j] : 0.0;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) Vt[(size_t)i * (size_t)n + (size_t)j] = V[(size_t)j * (size_t)n + (size_t)i];
    /* ordina per S decrescente */
    for (p = 0; p < n - 1; p++) {
        int bi = p;
        for (q = p + 1; q < n; q++) if (S[q] > S[bi]) bi = q;
        if (bi != p) {
            gm_real tmp = S[p]; S[p] = S[bi]; S[bi] = tmp;
            for (i = 0; i < m; i++) { gm_real t = U[(size_t)i * (size_t)n + (size_t)p]; U[(size_t)i * (size_t)n + (size_t)p] = U[(size_t)i * (size_t)n + (size_t)bi]; U[(size_t)i * (size_t)n + (size_t)bi] = t; }
            for (i = 0; i < n; i++) { gm_real t = Vt[(size_t)p * (size_t)n + (size_t)i]; Vt[(size_t)p * (size_t)n + (size_t)i] = Vt[(size_t)bi * (size_t)n + (size_t)i]; Vt[(size_t)bi * (size_t)n + (size_t)i] = t; }
        }
    }
    free(B); free(V);
    return GM_OK;
}

/* ===================== QR con Gram-Schmidt (da pola) ===================== */

gm_status gm_linalg_qr_cgs(int m, int n, const gm_real *A, gm_real *Q, gm_real *R)
{
    int i, j, k;
    if (A == NULL || Q == NULL || R == NULL || m < n || n <= 0) return GM_ERR_ARG;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) R[(size_t)i * (size_t)n + (size_t)j] = 0.0;
    /* colonne di Q = copia di A */
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) Q[(size_t)i * (size_t)n + (size_t)j] = A[(size_t)i * (size_t)n + (size_t)j];
    for (j = 0; j < n; j++) {
        for (i = 0; i < j; i++) {
            gm_real dot = 0.0;
            for (k = 0; k < m; k++) dot += Q[(size_t)k * (size_t)n + (size_t)i] * A[(size_t)k * (size_t)n + (size_t)j];
            R[(size_t)i * (size_t)n + (size_t)j] = dot;
            for (k = 0; k < m; k++) Q[(size_t)k * (size_t)n + (size_t)j] -= dot * Q[(size_t)k * (size_t)n + (size_t)i];
        }
        {
            gm_real nrm = 0.0;
            for (k = 0; k < m; k++) nrm += Q[(size_t)k * (size_t)n + (size_t)j] * Q[(size_t)k * (size_t)n + (size_t)j];
            nrm = sqrt(nrm);
            R[(size_t)j * (size_t)n + (size_t)j] = nrm;
            if (nrm > 0.0) for (k = 0; k < m; k++) Q[(size_t)k * (size_t)n + (size_t)j] /= nrm;
        }
    }
    return GM_OK;
}

gm_status gm_linalg_qr_mgs(int m, int n, const gm_real *A, gm_real *Q, gm_real *R)
{
    int i, j, k;
    if (A == NULL || Q == NULL || R == NULL || m < n || n <= 0) return GM_ERR_ARG;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) R[(size_t)i * (size_t)n + (size_t)j] = 0.0;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) Q[(size_t)i * (size_t)n + (size_t)j] = A[(size_t)i * (size_t)n + (size_t)j];
    for (j = 0; j < n; j++) {
        for (i = 0; i < j; i++) {
            gm_real dot = 0.0;
            for (k = 0; k < m; k++) dot += Q[(size_t)k * (size_t)n + (size_t)i] * Q[(size_t)k * (size_t)n + (size_t)j];
            R[(size_t)i * (size_t)n + (size_t)j] += dot;
            for (k = 0; k < m; k++) Q[(size_t)k * (size_t)n + (size_t)j] -= dot * Q[(size_t)k * (size_t)n + (size_t)i];
        }
        {
            gm_real nrm = 0.0;
            for (k = 0; k < m; k++) nrm += Q[(size_t)k * (size_t)n + (size_t)j] * Q[(size_t)k * (size_t)n + (size_t)j];
            nrm = sqrt(nrm);
            R[(size_t)j * (size_t)n + (size_t)j] = nrm;
            if (nrm > 0.0) for (k = 0; k < m; k++) Q[(size_t)k * (size_t)n + (size_t)j] /= nrm;
        }
    }
    return GM_OK;
}

/* ===================== bidiagonalizzazione (da pola gebd2) ===================== */

gm_status gm_linalg_bidiag(int m, int n, const gm_real *A, gm_real *d, gm_real *e)
{
    gm_real *W;
    int k, i, j;
    if (A == NULL || d == NULL || e == NULL || m < n || n <= 0) return GM_ERR_ARG;
    W = (gm_real *)malloc((size_t)m * (size_t)n * sizeof(gm_real));
    if (!W) return GM_ERR_NO_MEM;
    memcpy(W, A, (size_t)m * (size_t)n * sizeof(gm_real));
    for (k = 0; k < n; k++) {
        /* Householder a sinistra sulla colonna k */
        gm_real norm = 0.0, alpha, beta, vnorm2 = 1.0, v0;
        for (i = k; i < m; i++) norm += W[(size_t)i * (size_t)n + (size_t)k] * W[(size_t)i * (size_t)n + (size_t)k];
        norm = sqrt(norm);
        if (norm != 0.0) {
            alpha = (W[(size_t)k * (size_t)n + (size_t)k] >= 0.0) ? -norm : norm;
            v0 = W[(size_t)k * (size_t)n + (size_t)k] - alpha;
            W[(size_t)k * (size_t)n + (size_t)k] = 1.0;
            for (i = k + 1; i < m; i++) W[(size_t)i * (size_t)n + (size_t)k] /= v0;
            for (i = k + 1; i < m; i++) vnorm2 += W[(size_t)i * (size_t)n + (size_t)k] * W[(size_t)i * (size_t)n + (size_t)k];
            beta = 2.0 / vnorm2;
            for (j = k + 1; j < n; j++) {
                gm_real dot = W[(size_t)k * (size_t)n + (size_t)j];
                for (i = k + 1; i < m; i++) dot += W[(size_t)i * (size_t)n + (size_t)k] * W[(size_t)i * (size_t)n + (size_t)j];
                dot *= beta;
                W[(size_t)k * (size_t)n + (size_t)j] -= dot;
                for (i = k + 1; i < m; i++) W[(size_t)i * (size_t)n + (size_t)j] -= dot * W[(size_t)i * (size_t)n + (size_t)k];
            }
            W[(size_t)k * (size_t)n + (size_t)k] = alpha;
        }
        d[k] = W[(size_t)k * (size_t)n + (size_t)k];
        /* Householder a destra sulla riga k (colonne k+1..n-1) */
        if (k + 1 < n) {
            gm_real norm2 = 0.0, alpha2, beta2, vn2 = 1.0, v0b;
            for (j = k + 1; j < n; j++) norm2 += W[(size_t)k * (size_t)n + (size_t)j] * W[(size_t)k * (size_t)n + (size_t)j];
            norm2 = sqrt(norm2);
            if (norm2 != 0.0) {
                alpha2 = (W[(size_t)k * (size_t)n + (size_t)(k + 1)] >= 0.0) ? -norm2 : norm2;
                v0b = W[(size_t)k * (size_t)n + (size_t)(k + 1)] - alpha2;
                W[(size_t)k * (size_t)n + (size_t)(k + 1)] = 1.0;
                for (j = k + 2; j < n; j++) W[(size_t)k * (size_t)n + (size_t)j] /= v0b;
                for (j = k + 2; j < n; j++) vn2 += W[(size_t)k * (size_t)n + (size_t)j] * W[(size_t)k * (size_t)n + (size_t)j];
                beta2 = 2.0 / vn2;
                for (i = k + 1; i < m; i++) {
                    gm_real dot = W[(size_t)i * (size_t)n + (size_t)(k + 1)];
                    for (j = k + 2; j < n; j++) dot += W[(size_t)i * (size_t)n + (size_t)j] * W[(size_t)k * (size_t)n + (size_t)j];
                    dot *= beta2;
                    W[(size_t)i * (size_t)n + (size_t)(k + 1)] -= dot;
                    for (j = k + 2; j < n; j++) W[(size_t)i * (size_t)n + (size_t)j] -= dot * W[(size_t)k * (size_t)n + (size_t)j];
                }
                W[(size_t)k * (size_t)n + (size_t)(k + 1)] = alpha2;
            }
            e[k] = W[(size_t)k * (size_t)n + (size_t)(k + 1)];
        } else e[k] = 0.0;
    }
    free(W);
    return GM_OK;
}

/* ===================== metodi diretti SLE ===================== */

gm_status gm_linalg_thomas(int n, const gm_real *a, const gm_real *b, const gm_real *c,
                           const gm_real *d, gm_real *x)
{
    gm_real *cp, *dp;
    int i;
    if (n <= 0 || b == NULL || d == NULL || x == NULL) return GM_ERR_ARG;
    cp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    dp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!cp || !dp) { free(cp); free(dp); return GM_ERR_NO_MEM; }
    if (b[0] == 0.0) { free(cp); free(dp); return GM_ERR_SINGULAR; }
    cp[0] = (c ? c[0] : 0.0) / b[0];
    dp[0] = d[0] / b[0];
    for (i = 1; i < n; i++) {
        gm_real m = b[i] - (a ? a[i - 1] : 0.0) * cp[i - 1];
        if (m == 0.0) { free(cp); free(dp); return GM_ERR_SINGULAR; }
        cp[i] = ((c && i < n - 1) ? c[i] : 0.0) / m;
        dp[i] = (d[i] - (a ? a[i - 1] : 0.0) * dp[i - 1]) / m;
    }
    x[n - 1] = dp[n - 1];
    for (i = n - 2; i >= 0; i--) x[i] = dp[i] - cp[i] * x[i + 1];
    free(cp); free(dp);
    return GM_OK;
}

gm_status gm_linalg_ldlt(int n, gm_real *A, gm_real *d)
{
    int i, j, k;
    if (A == NULL || d == NULL || n <= 0) return GM_ERR_ARG;
    for (j = 0; j < n; j++) {
        gm_real s = A[IDX(j, j, n)];
        for (k = 0; k < j; k++) s -= A[IDX(j, k, n)] * A[IDX(j, k, n)] * d[k];
        d[j] = s;
        if (d[j] == 0.0) return GM_ERR_SINGULAR;
        for (i = j + 1; i < n; i++) {
            gm_real t = A[IDX(i, j, n)];
            for (k = 0; k < j; k++) t -= A[IDX(i, k, n)] * A[IDX(j, k, n)] * d[k];
            A[IDX(i, j, n)] = t / d[j];
        }
        A[IDX(j, j, n)] = 1.0;
    }
    return GM_OK;
}

gm_status gm_linalg_ldlt_solve(int n, const gm_real *L, const gm_real *d,
                               const gm_real *b, gm_real *x)
{
    int i, k;
    if (n <= 0 || L == NULL || d == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    /* L y = b */
    for (i = 0; i < n; i++) {
        gm_real s = b[i];
        for (k = 0; k < i; k++) s -= L[IDX(i, k, n)] * x[k];
        x[i] = s;
    }
    /* D z = y */
    for (i = 0; i < n; i++) x[i] /= d[i];
    /* L^T w = z */
    for (i = n - 1; i >= 0; i--) {
        gm_real s = x[i];
        for (k = i + 1; k < n; k++) s -= L[IDX(k, i, n)] * x[k];
        x[i] = s;
    }
    return GM_OK;
}

gm_status gm_linalg_gauss_jordan(int n, const gm_real *A, const gm_real *b, gm_real *x)
{
    gm_real *M;
    int i, j, k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    M = (gm_real *)malloc((size_t)n * (size_t)(n + 1) * sizeof(gm_real));
    if (!M) return GM_ERR_NO_MEM;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) M[IDX(i, j, n + 1)] = A[IDX(i, j, n)];
        M[IDX(i, n, n + 1)] = b[i];
    }
    for (i = 0; i < n; i++) {
        int piv = i;
        for (k = i + 1; k < n; k++) if (fabs(M[IDX(k, i, n + 1)]) > fabs(M[IDX(piv, i, n + 1)])) piv = k;
        if (M[IDX(piv, i, n + 1)] == 0.0) { free(M); return GM_ERR_SINGULAR; }
        if (piv != i) for (j = 0; j <= n; j++) { gm_real t = M[IDX(i, j, n + 1)]; M[IDX(i, j, n + 1)] = M[IDX(piv, j, n + 1)]; M[IDX(piv, j, n + 1)] = t; }
        {
            gm_real p = M[IDX(i, i, n + 1)];
            for (j = 0; j <= n; j++) M[IDX(i, j, n + 1)] /= p;
        }
        for (k = 0; k < n; k++) if (k != i) {
            gm_real f = M[IDX(k, i, n + 1)];
            for (j = 0; j <= n; j++) M[IDX(k, j, n + 1)] -= f * M[IDX(i, j, n + 1)];
        }
    }
    for (i = 0; i < n; i++) x[i] = M[IDX(i, n, n + 1)];
    free(M);
    return GM_OK;
}

gm_status gm_linalg_cramer2(const gm_real *A, const gm_real *b, gm_real *x)
{
    gm_real det, d1, d2;
    if (A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    det = A[0] * A[3] - A[1] * A[2];
    if (det == 0.0) return GM_ERR_SINGULAR;
    d1 = b[0] * A[3] - A[1] * b[1];
    d2 = A[0] * b[1] - b[0] * A[2];
    x[0] = d1 / det; x[1] = d2 / det;
    return GM_OK;
}

static gm_real det3(const gm_real *M)
{
    return M[0] * (M[4] * M[8] - M[5] * M[7])
         - M[1] * (M[3] * M[8] - M[5] * M[6])
         + M[2] * (M[3] * M[7] - M[4] * M[6]);
}

gm_status gm_linalg_cramer3(const gm_real *A, const gm_real *b, gm_real *x)
{
    gm_real det, d[3];
    gm_real M[9];
    int c;
    if (A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    det = det3(A);
    if (det == 0.0) return GM_ERR_SINGULAR;
    for (c = 0; c < 3; c++) {
        int i;
        for (i = 0; i < 9; i++) M[i] = A[i];
        M[0 * 3 + c] = b[0]; M[1 * 3 + c] = b[1]; M[2 * 3 + c] = b[2];
        d[c] = det3(M);
    }
    x[0] = d[0] / det; x[1] = d[1] / det; x[2] = d[2] / det;
    return GM_OK;
}

gm_status gm_linalg_band_solve(int n, int kl, int ku, const gm_real *A, int lda,
                               const gm_real *b, gm_real *x)
{
    /* storage a banda (LAPACK): A[kl+ku+i-j][j] = a_ij. Gauss con pivoting (band). */
    gm_real *M, *rhs;
    int *piv;
    int i, j, k;
    if (n <= 0 || kl < 0 || ku < 0 || A == NULL || b == NULL || x == NULL || lda < 2 * kl + ku + 1) return GM_ERR_ARG;
    M = (gm_real *)calloc((size_t)lda * (size_t)n, sizeof(gm_real));
    rhs = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    piv = (int *)malloc((size_t)n * sizeof(int));
    if (!M || !rhs || !piv) { free(M); free(rhs); free(piv); return GM_ERR_NO_MEM; }
    for (j = 0; j < n; j++) {
        for (i = (j - ku > 0 ? j - ku : 0); i <= (j + kl < n ? j + kl : n - 1); i++)
            M[(size_t)(kl + ku + i - j) * (size_t)n + (size_t)j] = A[(size_t)(kl + ku + i - j) * (size_t)n + (size_t)j];
    }
    for (i = 0; i < n; i++) rhs[i] = b[i];
    /* eliminazione gaussiana band (senza pivoting, semplice) */
    for (k = 0; k < n; k++) {
        gm_real p = M[(size_t)(kl + ku) * (size_t)n + (size_t)k];
        if (p == 0.0) { free(M); free(rhs); free(piv); return GM_ERR_SINGULAR; }
        for (i = k + 1; i <= (k + kl < n ? k + kl : n - 1); i++) {
            gm_real f = M[(size_t)(kl + ku + i - k) * (size_t)n + (size_t)k] / p;
            for (j = k; j <= (k + ku < n ? k + ku : n - 1); j++)
                M[(size_t)(kl + ku + i - j) * (size_t)n + (size_t)j] -= f * M[(size_t)(kl + ku + k - j) * (size_t)n + (size_t)j];
            rhs[i] -= f * rhs[k];
        }
    }
    /* back substitution */
    for (i = n - 1; i >= 0; i--) {
        gm_real s = rhs[i];
        for (j = i + 1; j <= (i + ku < n ? i + ku : n - 1); j++)
            s -= M[(size_t)(kl + ku + i - j) * (size_t)n + (size_t)j] * x[j];
        x[i] = s / M[(size_t)(kl + ku) * (size_t)n + (size_t)i];
    }
    free(M); free(rhs); free(piv);
    return GM_OK;
}

/* ===================== minori / cofattori / aggiunta (da matrix_library) ===================== */

gm_real gm_linalg_minor(int n, const gm_real *A, int i, int j)
{
    gm_real *M, d;
    int r, c, rr, cc;
    if (n <= 1 || A == NULL || i < 0 || i >= n || j < 0 || j >= n) return 0.0;
    M = (gm_real *)malloc((size_t)(n - 1) * (size_t)(n - 1) * sizeof(gm_real));
    if (M == NULL) return 0.0;
    rr = 0;
    for (r = 0; r < n; r++) {
        if (r == i) continue;
        cc = 0;
        for (c = 0; c < n; c++) {
            if (c == j) continue;
            M[(size_t)rr * (size_t)(n - 1) + cc] = A[(size_t)r * (size_t)n + c];
            cc++;
        }
        rr++;
    }
    d = gm_linalg_det(n - 1, M);
    free(M);
    return d;
}

gm_status gm_linalg_cofactor(int n, const gm_real *A, gm_real *C)
{
    int i, j;
    if (n <= 0 || A == NULL || C == NULL) return GM_ERR_ARG;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            gm_real m = gm_linalg_minor(n, A, i, j);
            C[(size_t)i * (size_t)n + j] = ((i + j) & 1) ? -m : m;
        }
    return GM_OK;
}

gm_status gm_linalg_adjugate(int n, const gm_real *A, gm_real *Adj)
{
    gm_real *C;
    int i, j;
    if (n <= 0 || A == NULL || Adj == NULL) return GM_ERR_ARG;
    C = (gm_real *)malloc((size_t)n * (size_t)n * sizeof(gm_real));
    if (C == NULL) return GM_ERR_NO_MEM;
    if (gm_linalg_cofactor(n, A, C) != GM_OK) { free(C); return GM_ERR_ARG; }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) Adj[(size_t)i * (size_t)n + j] = C[(size_t)j * (size_t)n + i];
    free(C);
    return GM_OK;
}

/* ===================== Doolittle LU senza pivoting (da LU-DecompEngine) ===================== */

gm_status gm_linalg_lu_doolittle(int n, const gm_real *A, gm_real *L, gm_real *U)
{
    int i, j, k;
    if (n <= 0 || A == NULL || L == NULL || U == NULL) return GM_ERR_ARG;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) { L[(size_t)i * n + j] = 0.0; U[(size_t)i * n + j] = 0.0; }
        L[(size_t)i * n + i] = 1.0;
    }
    for (i = 0; i < n; i++) {
        /* U[i][k] per k >= i */
        for (k = i; k < n; k++) {
            gm_real s = A[(size_t)i * n + k];
            for (j = 0; j < i; j++) s -= L[(size_t)i * n + j] * U[(size_t)j * n + k];
            U[(size_t)i * n + k] = s;
        }
        if (fabs(U[(size_t)i * n + i]) < DBL_MIN) return GM_ERR_SINGULAR;
        /* L[k][i] per k > i */
        for (k = i + 1; k < n; k++) {
            gm_real s = A[(size_t)k * n + i];
            for (j = 0; j < i; j++) s -= L[(size_t)k * n + j] * U[(size_t)j * n + i];
            L[(size_t)k * n + i] = s / U[(size_t)i * n + i];
        }
    }
    return GM_OK;
}

/* ===================== norme/traccia/dot (da LINAL) ===================== */

gm_real gm_linalg_norm_frobenius(int m, int n, const gm_real *A)
{
    int i, k;
    gm_real s = 0.0;
    if (m <= 0 || n <= 0 || A == NULL) return 0.0;
    for (i = 0; i < m * n; i++) s += A[i] * A[i];
    (void)k;
    return sqrt(s);
}

gm_real gm_linalg_trace(int n, const gm_real *A)
{
    int i;
    gm_real s = 0.0;
    if (n <= 0 || A == NULL) return 0.0;
    for (i = 0; i < n; i++) s += A[(size_t)i * n + i];
    return s;
}

gm_real gm_linalg_dot(int m, int n, const gm_real *A, const gm_real *B)
{
    int i, k = m * n;
    gm_real s = 0.0;
    if (m <= 0 || n <= 0 || A == NULL || B == NULL) return 0.0;
    for (i = 0; i < k; i++) s += A[i] * B[i];
    return s;
}

/* ===================== riduzione a banda simmetrica (da dplasma) ===================== */

gm_status gm_linalg_sym_band_reduce(int n, int b, const gm_real *A, gm_real *T, gm_real *Q)
{
    int k, i, j;
    gm_real *v, *w;
    if (n <= 0 || b < 0 || b >= n || A == NULL || T == NULL || Q == NULL) return GM_ERR_ARG;
    memcpy(T, A, (size_t)n * (size_t)n * sizeof(gm_real));
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) Q[(size_t)i * n + j] = (i == j) ? 1.0 : 0.0;
    v = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!v || !w) { free(v); free(w); return GM_ERR_NO_MEM; }
    for (k = 0; k + b + 1 < n; k++) {
        int i0 = k + b;                  /* ultimo indice da tenere (si azzera i > i0) */
        int len = n - i0;
        gm_real nrm = 0.0, alpha, vtv = 0.0, beta;
        for (i = i0; i < n; i++) nrm += T[(size_t)i * n + k] * T[(size_t)i * n + k];
        nrm = sqrt(nrm);
        if (nrm == 0.0) continue;
        alpha = (T[(size_t)i0 * n + k] >= 0.0) ? -nrm : nrm;
        for (i = 0; i < n; i++) v[i] = 0.0;
        v[i0] = T[(size_t)i0 * n + k] - alpha;
        for (i = i0 + 1; i < n; i++) v[i] = T[(size_t)i * n + k];
        for (i = i0; i < n; i++) vtv += v[i] * v[i];
        if (vtv == 0.0) continue;
        beta = 2.0 / vtv;
        /* T <- P T P, P = I - beta v v^T (applicazione simmetrica) */
        /* w = T v */
        for (i = 0; i < n; i++) {
            gm_real s = 0.0;
            for (j = 0; j < n; j++) s += T[(size_t)i * n + j] * v[j];
            w[i] = s;
        }
        /* T <- T - beta (v w^T + w v^T) + beta^2 (v^T w) v v^T */
        {
            gm_real vw = 0.0;
            for (i = 0; i < n; i++) vw += v[i] * w[i];
            for (i = 0; i < n; i++)
                for (j = 0; j < n; j++)
                    T[(size_t)i * n + j] -= beta * (v[i] * w[j] + w[i] * v[j])
                                          - beta * beta * vw * v[i] * v[j];
        }
        /* Q <- Q P */
        for (i = 0; i < n; i++) {
            gm_real s = 0.0;
            for (j = 0; j < n; j++) s += Q[(size_t)i * n + j] * v[j];
            s *= beta;
            for (j = 0; j < n; j++) Q[(size_t)i * n + j] -= s * v[j];
        }
        (void)len;
    }
    free(v); free(w);
    return GM_OK;
}

/* ===================== RREF / rango / spazi (da Tyler-Ricks) ===================== */

int gm_linalg_rref(int m, int n, gm_real *A, int *pivots)
{
    int r = 0, c, i, k, p;
    gm_real tol, mx0 = 0.0;
    if (m <= 0 || n <= 0 || A == NULL) return 0;
    for (i = 0; i < m * n; i++) { gm_real a = fabs(A[i]); if (a > mx0) mx0 = a; }
    tol = (mx0 > 0.0) ? mx0 * (gm_real)(m > n ? m : n) * DBL_EPSILON : 0.0;
    for (c = 0; c < n && r < m; c++) {
        gm_real mx;
        p = r; mx = fabs(A[(size_t)r * n + c]);
        for (i = r + 1; i < m; i++) if (fabs(A[(size_t)i * n + c]) > mx) { mx = fabs(A[(size_t)i * n + c]); p = i; }
        if (mx <= tol) continue;
        if (p != r) for (k = 0; k < n; k++) { gm_real t = A[(size_t)r * n + k]; A[(size_t)r * n + k] = A[(size_t)p * n + k]; A[(size_t)p * n + k] = t; }
        { gm_real pv = A[(size_t)r * n + c];
          for (k = 0; k < n; k++) A[(size_t)r * n + k] /= pv; }
        for (i = 0; i < m; i++) {
            gm_real f;
            if (i == r) continue;
            f = A[(size_t)i * n + c];
            if (f != 0.0) for (k = 0; k < n; k++) A[(size_t)i * n + k] -= f * A[(size_t)r * n + k];
        }
        if (pivots) pivots[r] = c;
        r++;
    }
    return r;
}

int gm_linalg_rank(int m, int n, const gm_real *A)
{
    gm_real *W;
    int r;
    if (m <= 0 || n <= 0 || A == NULL) return 0;
    W = (gm_real *)malloc((size_t)m * (size_t)n * sizeof(gm_real));
    if (!W) return 0;
    memcpy(W, A, (size_t)m * (size_t)n * sizeof(gm_real));
    r = gm_linalg_rref(m, n, W, NULL);
    free(W);
    return r;
}

int gm_linalg_col_space(int m, int n, const gm_real *A, gm_real *basis)
{
    gm_real *W;
    int *piv;
    int r, i, j;
    if (m <= 0 || n <= 0 || A == NULL || basis == NULL) return 0;
    W = (gm_real *)malloc((size_t)m * (size_t)n * sizeof(gm_real));
    piv = (int *)malloc((size_t)(m < n ? m : n) * sizeof(int));
    if (!W || !piv) { free(W); free(piv); return 0; }
    memcpy(W, A, (size_t)m * (size_t)n * sizeof(gm_real));
    r = gm_linalg_rref(m, n, W, piv);
    for (i = 0; i < r; i++)
        for (j = 0; j < m; j++) basis[(size_t)j * r + i] = A[(size_t)j * n + piv[i]];
    free(W); free(piv);
    return r;
}

int gm_linalg_row_space(int m, int n, const gm_real *A, gm_real *basis)
{
    gm_real *W;
    int r, i, j;
    if (m <= 0 || n <= 0 || A == NULL || basis == NULL) return 0;
    W = (gm_real *)malloc((size_t)m * (size_t)n * sizeof(gm_real));
    if (!W) return 0;
    memcpy(W, A, (size_t)m * (size_t)n * sizeof(gm_real));
    r = gm_linalg_rref(m, n, W, NULL);
    for (i = 0; i < r; i++)
        for (j = 0; j < n; j++) basis[(size_t)i * n + j] = W[(size_t)i * n + j];
    free(W);
    return r;
}

/* ===================== Soluzioni di base di A x = b (da OR-Lab) ===================== */

static void bs_comb(int n, int m, int start, int depth, int *idx,
                    const gm_real *A, const gm_real *b, gm_real *sols, int *feas,
                    int max_sols, int *nsol)
{
    int i, j, k;
    if (*nsol >= max_sols) return;
    if (depth == m) {
        gm_real *S = (gm_real *)malloc((size_t)m * m * sizeof(gm_real));
        gm_real *bb = (gm_real *)malloc((size_t)m * sizeof(gm_real));
        gm_real *xx = (gm_real *)malloc((size_t)m * sizeof(gm_real));
        if (!S || !bb || !xx) { free(S); free(bb); free(xx); return; }
        for (i = 0; i < m; i++) {
            for (j = 0; j < m; j++) S[(size_t)i * m + j] = A[(size_t)i * n + idx[j]];
            bb[i] = b[i];
        }
        if (gm_linalg_gauss_jordan(m, S, bb, xx) == GM_OK) {
            gm_real *row = sols + (size_t)(*nsol) * n;
            int ok = 1;
            for (i = 0; i < n; i++) row[i] = 0.0;
            for (j = 0; j < m; j++) {
                if (!gm_is_finite(xx[j])) { ok = 0; break; }
                row[idx[j]] = xx[j];
            }
            if (ok) {
                int f = 1;
                for (i = 0; i < n; i++) if (row[i] < -1e-9) f = 0;
                if (feas) feas[*nsol] = f;
                (*nsol)++;
            }
        }
        free(S); free(bb); free(xx);
        return;
    }
    for (k = start; k <= n - (m - depth); k++) {
        idx[depth] = k;
        bs_comb(n, m, k + 1, depth + 1, idx, A, b, sols, feas, max_sols, nsol);
    }
}

int gm_linalg_basic_solutions(int m, int n, const gm_real *A, const gm_real *b,
                              gm_real *sols, int *feas, int max_sols)
{
    int *idx;
    int nsol = 0;
    if (m < 1 || n < 1 || m > n || !A || !b || !sols || max_sols < 1) return -1;
    idx = (int *)malloc((size_t)m * sizeof(int));
    if (!idx) return -1;
    bs_comb(n, m, 0, 0, idx, A, b, sols, feas, max_sols, &nsol);
    free(idx);
    return nsol;
}

/* ===================== Levinson-Durbin (da xiph/LPCNet) ===================== */

int gm_linalg_levinson_durbin(int p, const gm_real *r, gm_real *a, gm_real *err)
{
    gm_real *tmp;
    gm_real E;
    int i, m;
    if (p < 1 || !r || !a) return -1;
    tmp = (gm_real *)malloc((size_t)(p + 1) * sizeof(gm_real));
    if (!tmp) return -1;
    for (i = 0; i <= p; i++) a[i] = 0.0;
    a[0] = 1.0;
    E = r[0];
    if (fabs(E) < 1e-30) { free(tmp); if (err) *err = 0.0; return 0; }
    for (m = 1; m <= p; m++) {
        gm_real acc = r[m], k;
        for (i = 1; i < m; i++) acc += a[i] * r[m - i];
        k = -acc / E;
        for (i = 0; i <= p; i++) tmp[i] = a[i];
        for (i = 1; i < m; i++) a[i] = tmp[i] + k * tmp[m - i];
        a[m] = k;
        E *= (1.0 - k * k);
        if (E < 1e-30) E = 1e-30;
    }
    free(tmp);
    if (err) *err = E;
    return 0;
}

gm_status gm_linalg_levinson(int n, const gm_real *c, const gm_real *b, gm_real *x)
{
    double *a, *y, e;
    int i, j, k;
    if (n < 1 || !c || !b || !x) return GM_ERR_ARG;
    if (fabs(c[0]) < 1e-300) return GM_ERR_SINGULAR;
    a = (double *)calloc((size_t)n, sizeof(double));
    y = (double *)calloc((size_t)n, sizeof(double));
    if (!a || !y) { free(a); free(y); return GM_ERR_NO_MEM; }
    a[0] = 1.0;
    e = c[0];
    y[0] = b[0] / e;
    for (k = 1; k < n; k++) {
        double s, lam, s2, beta;
        double *anew, *ynew;
        anew = (double *)calloc((size_t)(k + 1), sizeof(double));
        ynew = (double *)calloc((size_t)(k + 1), sizeof(double));
        if (!anew || !ynew) { free(anew); free(ynew); free(a); free(y); return GM_ERR_NO_MEM; }
        s = 0.0;
        for (j = 0; j < k; j++) s += a[j] * c[k - j];
        lam = s / e;
        anew[0] = 1.0;
        for (j = 1; j < k; j++) anew[j] = a[j] - lam * a[k - j];
        anew[k] = -lam;
        e *= 1.0 - lam * lam;
        if (e < 1e-300) { free(anew); free(ynew); free(a); free(y); return GM_ERR_SINGULAR; }
        s2 = 0.0;
        for (j = 0; j < k; j++) s2 += c[k - j] * y[j];
        beta = (b[k] - s2) / e;
        for (j = 0; j < k; j++) ynew[j] = y[j] + beta * anew[k - j];
        ynew[k] = beta;
        free(a); free(y);
        a = anew; y = ynew;
    }
    for (i = 0; i < n; i++) x[i] = y[i];
    free(a); free(y);
    return GM_OK;
}
