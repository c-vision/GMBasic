/* gm_clinalg.c — algebra lineare complessa (riscrittura). */
#include "gm_clinalg.h"
#include "gm_sparse_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CAT(A, n, r, c) ((A)[(size_t)(r) * (size_t)(n) + (size_t)(c)])

gm_cpx gm_cpx_dot(const gm_cpx *a, const gm_cpx *b, int n)
{
    gm_cpx s = gm_cpx_make(0.0, 0.0);
    int i;
    for (i = 0; i < n; i++) s = gm_cpx_add(s, gm_cpx_mul(gm_cpx_conj(a[i]), b[i]));
    return s;
}

gm_real gm_cpx_norm(const gm_cpx *a, int n)
{
    gm_real s = 0.0;
    int i;
    for (i = 0; i < n; i++) s += gm_cpx_norm2(a[i]);
    return sqrt(s);
}

void gm_cpx_matmul(int n, const gm_cpx *A, const gm_cpx *B, gm_cpx *C)
{
    int i, j, k;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            gm_cpx s = gm_cpx_make(0.0, 0.0);
            for (k = 0; k < n; k++) s = gm_cpx_add(s, gm_cpx_mul(CAT(A, n, i, k), CAT(B, n, k, j)));
            CAT(C, n, i, j) = s;
        }
}

void gm_cpx_conjtranspose(int n, const gm_cpx *A, gm_cpx *C)
{
    int i, j;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) CAT(C, n, i, j) = gm_cpx_conj(CAT(A, n, j, i));
}

gm_status gm_cpx_solve(int n, const gm_cpx *A, const gm_cpx *b, gm_cpx *x)
{
    gm_cpx *M;
    int *piv, i, j, k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    M = (gm_cpx *)malloc((size_t)n * (size_t)n * sizeof(gm_cpx));
    piv = (int *)malloc((size_t)n * sizeof(int));
    if (!M || !piv) { free(M); free(piv); return GM_ERR_NO_MEM; }
    memcpy(M, A, (size_t)n * (size_t)n * sizeof(gm_cpx));
    for (i = 0; i < n; i++) x[i] = b[i];
    for (k = 0; k < n; k++) {
        int p = k;
        gm_real best = gm_cpx_abs(CAT(M, n, k, k));
        for (i = k + 1; i < n; i++) {
            gm_real v = gm_cpx_abs(CAT(M, n, i, k));
            if (v > best) { best = v; p = i; }
        }
        piv[k] = p;
        if (best == 0.0) { free(M); free(piv); return GM_ERR_SINGULAR; }
        if (p != k) {
            for (j = 0; j < n; j++) { gm_cpx t = CAT(M, n, k, j); CAT(M, n, k, j) = CAT(M, n, p, j); CAT(M, n, p, j) = t; }
        }
        for (i = k + 1; i < n; i++) {
            gm_cpx m = gm_cpx_div(CAT(M, n, i, k), CAT(M, n, k, k));
            CAT(M, n, i, k) = m;
            for (j = k + 1; j < n; j++)
                CAT(M, n, i, j) = gm_cpx_sub(CAT(M, n, i, j), gm_cpx_mul(m, CAT(M, n, k, j)));
        }
    }
    for (i = 0; i < n; i++) {
        int p = piv[i];
        if (p != i) { gm_cpx t = x[i]; x[i] = x[p]; x[p] = t; }
    }
    for (i = 0; i < n; i++) {
        gm_cpx s = x[i];
        for (j = 0; j < i; j++) s = gm_cpx_sub(s, gm_cpx_mul(CAT(M, n, i, j), x[j]));
        x[i] = s;
    }
    for (i = n - 1; i >= 0; i--) {
        gm_cpx s = x[i];
        for (j = i + 1; j < n; j++) s = gm_cpx_sub(s, gm_cpx_mul(CAT(M, n, i, j), x[j]));
        x[i] = gm_cpx_div(s, CAT(M, n, i, i));
    }
    free(M); free(piv);
    return GM_OK;
}

gm_status gm_cpx_inverse(int n, const gm_cpx *A, gm_cpx *inv)
{
    gm_cpx *e;
    int j, i;
    if (n <= 0 || A == NULL || inv == NULL) return GM_ERR_ARG;
    e = (gm_cpx *)malloc((size_t)n * sizeof(gm_cpx));
    if (!e) return GM_ERR_NO_MEM;
    for (j = 0; j < n; j++) {
        gm_cpx col[1];
        (void)col;
        for (i = 0; i < n; i++) e[i] = gm_cpx_make(i == j ? 1.0 : 0.0, 0.0);
        {
            gm_cpx *x = (gm_cpx *)malloc((size_t)n * sizeof(gm_cpx));
            if (!x) { free(e); return GM_ERR_NO_MEM; }
            if (gm_cpx_solve(n, A, e, x) != GM_OK) { free(x); free(e); return GM_ERR_SINGULAR; }
            for (i = 0; i < n; i++) CAT(inv, n, i, j) = x[i];
            free(x);
        }
    }
    free(e);
    return GM_OK;
}

gm_status gm_cpx_herm_eig(int n, const gm_cpx *A, gm_real *eval, gm_cpx *evec)
{
    int m = 2 * n, i, j, k;
    double *M, *ev, *evec2;
    int *ord;
    if (n <= 0 || A == NULL || eval == NULL || evec == NULL) return GM_ERR_ARG;
    M = (double *)malloc((size_t)m * (size_t)m * sizeof(double));
    ev = (double *)malloc((size_t)m * sizeof(double));
    evec2 = (double *)malloc((size_t)m * (size_t)m * sizeof(double));
    ord = (int *)malloc((size_t)m * sizeof(int));
    if (!M || !ev || !evec2 || !ord) { free(M); free(ev); free(evec2); free(ord); return GM_ERR_NO_MEM; }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            gm_cpx a = CAT(A, n, i, j);
            M[(size_t)i * (size_t)m + (size_t)j] = a.re;
            M[(size_t)i * (size_t)m + (size_t)(j + n)] = -a.im;
            M[(size_t)(i + n) * (size_t)m + (size_t)j] = a.im;
            M[(size_t)(i + n) * (size_t)m + (size_t)(j + n)] = a.re;
        }
    dmat_eig_jacobi(m, M, ev, evec2);
    for (i = 0; i < m; i++) ord[i] = i;
    /* insertion sort degli indici per ev crescente */
    for (i = 1; i < m; i++) {
        int oi = ord[i]; double key = ev[oi]; int jj = i - 1;
        while (jj >= 0 && ev[ord[jj]] > key) { ord[jj + 1] = ord[jj]; jj--; }
        ord[jj + 1] = oi;
    }
    for (k = 0; k < n; k++) {
        int col = ord[2 * k]; /* autovalori doppi: uno ogni coppia */
        eval[k] = ev[col];
        for (i = 0; i < n; i++) {
            gm_real u = evec2[(size_t)i * (size_t)m + (size_t)col];
            gm_real v = evec2[(size_t)(i + n) * (size_t)m + (size_t)col];
            CAT(evec, n, i, k) = gm_cpx_make(u, v);
        }
    }
    free(M); free(ev); free(evec2); free(ord);
    return GM_OK;
}

gm_real gm_cpx_herm_max(int n, const gm_cpx *A, gm_cpx *v, int iters)
{
    int i, j, it;
    gm_real lambda = 0.0;
    if (n <= 0 || A == NULL || v == NULL) return 0.0;
    for (i = 0; i < n; i++) v[i] = gm_cpx_make(i == 0 ? 1.0 : 0.0, 0.0);
    for (it = 0; it < iters; it++) {
        gm_cpx *w = (gm_cpx *)malloc((size_t)n * sizeof(gm_cpx));
        gm_real nrm;
        if (!w) return lambda;
        for (i = 0; i < n; i++) {
            gm_cpx s = gm_cpx_make(0.0, 0.0);
            for (j = 0; j < n; j++) s = gm_cpx_add(s, gm_cpx_mul(CAT(A, n, i, j), v[j]));
            w[i] = s;
        }
        nrm = gm_cpx_norm(w, n);
        if (nrm == 0.0) { free(w); return 0.0; }
        for (i = 0; i < n; i++) v[i] = gm_cpx_scale(w[i], 1.0 / nrm);
        lambda = nrm;
        free(w);
    }
    /* Rayleigh quotient (reale per hermitiana) */
    {
        gm_cpx *Av = (gm_cpx *)malloc((size_t)n * sizeof(gm_cpx));
        if (Av) {
            for (i = 0; i < n; i++) {
                gm_cpx s = gm_cpx_make(0.0, 0.0);
                for (j = 0; j < n; j++) s = gm_cpx_add(s, gm_cpx_mul(CAT(A, n, i, j), v[j]));
                Av[i] = s;
            }
            lambda = gm_cpx_dot(v, Av, n).re;
            free(Av);
        }
    }
    return lambda;
}
