/* gm_iter.c — solver iterativi (riscrittura). */
#include "gm_iter.h"
#include "gc_array.h"
#include "gc_compute.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define AAT(A, n, r, c) ((A)[(size_t)(r) * (size_t)(n) + (size_t)(c)])

/* mat-vec y = A x */
static void matvec(int n, const gm_real *A, const gm_real *x, gm_real *y)
{
    int i;
    for (i = 0; i < n; i++) y[i] = gc_array_dot(A + (size_t)i * (size_t)n, x, n);
}

static gm_real resid_norm(int n, const gm_real *A, const gm_real *b, const gm_real *x, gm_real *r)
{
    gm_real *Ax = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real nr;
    if (!Ax) return -1.0;
    matvec(n, A, x, Ax);
    { int i; for (i = 0; i < n; i++) r[i] = b[i] - Ax[i]; }
    nr = gc_array_norm2(r, n);
    free(Ax);
    return nr;
}

gm_status gm_iter_jacobi(int n, const gm_real *A, const gm_real *b, gm_real *x,
                         int maxit, gm_real tol, int *iters)
{
    int k, i, j;
    gm_real *xn, *r;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    xn = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    r = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!xn || !r) { free(xn); free(r); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    for (k = 0; k < maxit; k++) {
        for (i = 0; i < n; i++) {
            gm_real s = b[i];
            for (j = 0; j < n; j++) if (j != i) s -= AAT(A, n, i, j) * x[j];
            xn[i] = (AAT(A, n, i, i) != 0.0) ? s / AAT(A, n, i, i) : 0.0;
        }
        memcpy(x, xn, (size_t)n * sizeof(gm_real));
        if (resid_norm(n, A, b, x, r) <= tol) { if (iters) *iters = k + 1; free(xn); free(r); return GM_OK; }
    }
    if (iters) *iters = maxit;
    free(xn); free(r);
    return GM_ERR_NOCONV;
}

gm_status gm_iter_gauss_seidel(int n, const gm_real *A, const gm_real *b, gm_real *x,
                               int maxit, gm_real tol, int *iters)
{
    int k, i, j;
    gm_real *r;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    r = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!r) return GM_ERR_NO_MEM;
    memset(x, 0, (size_t)n * sizeof(gm_real));
    for (k = 0; k < maxit; k++) {
        for (i = 0; i < n; i++) {
            gm_real s = b[i];
            for (j = 0; j < n; j++) if (j != i) s -= AAT(A, n, i, j) * x[j];
            x[i] = (AAT(A, n, i, i) != 0.0) ? s / AAT(A, n, i, i) : 0.0;
        }
        if (resid_norm(n, A, b, x, r) <= tol) { if (iters) *iters = k + 1; free(r); return GM_OK; }
    }
    if (iters) *iters = maxit;
    free(r);
    return GM_ERR_NOCONV;
}

gm_status gm_iter_sor(int n, const gm_real *A, const gm_real *b, gm_real *x,
                      gm_real omega, int maxit, gm_real tol, int *iters)
{
    int k, i, j;
    gm_real *r;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    r = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!r) return GM_ERR_NO_MEM;
    memset(x, 0, (size_t)n * sizeof(gm_real));
    for (k = 0; k < maxit; k++) {
        for (i = 0; i < n; i++) {
            gm_real s = b[i];
            for (j = 0; j < n; j++) if (j != i) s -= AAT(A, n, i, j) * x[j];
            if (AAT(A, n, i, i) != 0.0)
                x[i] = (1.0 - omega) * x[i] + omega * s / AAT(A, n, i, i);
        }
        if (resid_norm(n, A, b, x, r) <= tol) { if (iters) *iters = k + 1; free(r); return GM_OK; }
    }
    if (iters) *iters = maxit;
    free(r);
    return GM_ERR_NOCONV;
}

gm_status gm_iter_richardson(int n, const gm_real *A, const gm_real *b, gm_real *x,
                             gm_real omega, int maxit, gm_real tol, int *iters)
{
    int k;
    gm_real *r, *Ax;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    r = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    Ax = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!r || !Ax) { free(r); free(Ax); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    for (k = 0; k < maxit; k++) {
        matvec(n, A, x, Ax);
        { int i; for (i = 0; i < n; i++) r[i] = b[i] - Ax[i]; }
        gc_array_axpy(omega, r, x, n);
        if (gc_array_norm2(r, n) <= tol) { if (iters) *iters = k + 1; free(r); free(Ax); return GM_OK; }
    }
    if (iters) *iters = maxit;
    free(r); free(Ax);
    return GM_ERR_NOCONV;
}

gm_status gm_iter_steepest_descent(int n, const gm_real *A, const gm_real *b, gm_real *x,
                                   int maxit, gm_real tol, int *iters)
{
    int k;
    gm_real *r, *Ar;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    r = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    Ar = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!r || !Ar) { free(r); free(Ar); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    matvec(n, A, x, r);
    { int i; for (i = 0; i < n; i++) r[i] = b[i] - r[i]; }
    for (k = 0; k < maxit; k++) {
        gm_real rr, alpha;
        matvec(n, A, r, Ar);
        rr = gc_array_dot(r, r, n);
        alpha = gc_array_dot(r, Ar, n);
        if (alpha == 0.0) break;
        gc_array_axpy(rr / alpha, r, x, n);
        gc_array_axpy(-rr / alpha, Ar, r, n);
        if (sqrt(rr) <= tol) { if (iters) *iters = k + 1; free(r); free(Ar); return GM_OK; }
    }
    if (iters) *iters = maxit;
    free(r); free(Ar);
    return GM_ERR_NOCONV;
}

gm_status gm_iter_cg(int n, const gm_real *A, const gm_real *b, gm_real *x,
                     int maxit, gm_real tol, int *iters)
{
    gm_real *r, *p, *Ap;
    gm_real rsold;
    int k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    r = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    p = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    Ap = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!r || !p || !Ap) { free(r); free(p); free(Ap); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    memcpy(r, b, (size_t)n * sizeof(gm_real));
    memcpy(p, r, (size_t)n * sizeof(gm_real));
    rsold = gc_array_dot(r, r, n);
    for (k = 0; k < maxit; k++) {
        gm_real alpha, rsnew;
        matvec(n, A, p, Ap);
        alpha = gc_array_dot(p, Ap, n);
        if (alpha == 0.0) break;
        gc_array_axpy(rsold / alpha, p, x, n);
        gc_array_axpy(-rsold / alpha, Ap, r, n);
        rsnew = gc_array_dot(r, r, n);
        if (sqrt(rsnew) <= tol) { if (iters) *iters = k + 1; free(r); free(p); free(Ap); return GM_OK; }
        gc_array_axpby(1.0, r, rsnew / rsold, p, n);
        rsold = rsnew;
    }
    if (iters) *iters = maxit;
    free(r); free(p); free(Ap);
    return GM_ERR_NOCONV;
}

/* GMRES(m) con restart */
gm_status gm_iter_gmres(int n, const gm_real *A, const gm_real *b, gm_real *x,
                        int restart, int maxit, gm_real tol, int *iters)
{
    int m = (restart > 0 && restart < n) ? restart : n;
    gm_real *V, *H, *cs, *sn, *g, *w, *r;
    int total = 0;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    V = (gm_real *)malloc((size_t)n * (size_t)(m + 1) * sizeof(gm_real));
    H = (gm_real *)calloc((size_t)(m + 1) * (size_t)m, sizeof(gm_real));
    cs = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    sn = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    g = (gm_real *)calloc((size_t)(m + 1), sizeof(gm_real));
    w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    r = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!V || !H || !cs || !sn || !g || !w || !r) {
        free(V); free(H); free(cs); free(sn); free(g); free(w); free(r);
        return GM_ERR_NO_MEM;
    }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    while (total < maxit) {
        int j, i, k;
        gm_real beta, bn;
        matvec(n, A, x, r);
        for (i = 0; i < n; i++) r[i] = b[i] - r[i];
        bn = gc_array_norm2(r, n);
        if (bn <= tol) { if (iters) *iters = total; free(V); free(H); free(cs); free(sn); free(g); free(w); free(r); return GM_OK; }
        beta = bn;
        for (i = 0; i < n; i++) V[i] = r[i] / beta;
        memset(g, 0, (size_t)(m + 1) * sizeof(gm_real));
        memset(H, 0, (size_t)(m + 1) * (size_t)m * sizeof(gm_real));
        g[0] = beta;
        for (j = 0; j < m && total < maxit; j++, total++) {
            matvec(n, A, V + (size_t)j * (size_t)n, w);
            for (i = 0; i <= j; i++) {
                gm_real h = gc_array_dot(w, V + (size_t)i * (size_t)n, n);
                H[(size_t)i * (size_t)m + (size_t)j] = h;
                gc_array_axpy(-h, V + (size_t)i * (size_t)n, w, n);
            }
            H[(size_t)(j + 1) * (size_t)m + (size_t)j] = gc_array_norm2(w, n);
            if (H[(size_t)(j + 1) * (size_t)m + (size_t)j] != 0.0)
                for (i = 0; i < n; i++) V[(size_t)(j + 1) * (size_t)n + (size_t)i] = w[i] / H[(size_t)(j + 1) * (size_t)m + (size_t)j];
            /* Givens */
            for (i = 0; i < j; i++) {
                gm_real t = cs[i] * H[(size_t)i * (size_t)m + (size_t)j] + sn[i] * H[(size_t)(i + 1) * (size_t)m + (size_t)j];
                H[(size_t)(i + 1) * (size_t)m + (size_t)j] = -sn[i] * H[(size_t)i * (size_t)m + (size_t)j] + cs[i] * H[(size_t)(i + 1) * (size_t)m + (size_t)j];
                H[(size_t)i * (size_t)m + (size_t)j] = t;
            }
            {
                gm_real a1 = H[(size_t)j * (size_t)m + (size_t)j];
                gm_real a2 = H[(size_t)(j + 1) * (size_t)m + (size_t)j];
                gm_real rr = hypot(a1, a2);
                cs[j] = (rr != 0.0) ? a1 / rr : 1.0;
                sn[j] = (rr != 0.0) ? a2 / rr : 0.0;
                H[(size_t)j * (size_t)m + (size_t)j] = rr;
                H[(size_t)(j + 1) * (size_t)m + (size_t)j] = 0.0;
                g[j + 1] = -sn[j] * g[j];
                g[j] = cs[j] * g[j];
            }
            if (fabs(g[j + 1]) <= tol) { j++; break; }
        }
        /* risolvi R y = g e aggiorna x */
        {
            gm_real *y = (gm_real *)calloc((size_t)m, sizeof(gm_real));
            int jj;
            if (y) {
                for (jj = j - 1; jj >= 0; jj--) {
                    gm_real s = g[jj];
                    for (k = jj + 1; k < j; k++) s -= H[(size_t)jj * (size_t)m + (size_t)k] * y[k];
                    y[jj] = (H[(size_t)jj * (size_t)m + (size_t)jj] != 0.0) ? s / H[(size_t)jj * (size_t)m + (size_t)jj] : 0.0;
                }
                for (jj = 0; jj < j; jj++) gc_array_axpy(y[jj], V + (size_t)jj * (size_t)n, x, n);
                free(y);
            }
        }
        if (fabs(g[j]) <= tol) { if (iters) *iters = total; free(V); free(H); free(cs); free(sn); free(g); free(w); free(r); return GM_OK; }
    }
    if (iters) *iters = total;
    free(V); free(H); free(cs); free(sn); free(g); free(w); free(r);
    return GM_ERR_NOCONV;
}

gm_status gm_iter_bicgstab(int n, const gm_real *A, const gm_real *b, gm_real *x,
                           int maxit, gm_real tol, int *iters)
{
    gm_real *r, *rh, *p, *v, *s, *t;
    gm_real rho = 1.0, alpha = 1.0, w = 1.0;
    int k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    r = malloc((size_t)n * sizeof(gm_real)); rh = malloc((size_t)n * sizeof(gm_real));
    p = calloc((size_t)n, sizeof(gm_real)); v = calloc((size_t)n, sizeof(gm_real));
    s = malloc((size_t)n * sizeof(gm_real)); t = malloc((size_t)n * sizeof(gm_real));
    if (!r || !rh || !p || !v || !s || !t) { free(r); free(rh); free(p); free(v); free(s); free(t); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    matvec(n, A, x, r);
    { int i; for (i = 0; i < n; i++) r[i] = b[i] - r[i]; }
    memcpy(rh, r, (size_t)n * sizeof(gm_real));
    for (k = 0; k < maxit; k++) {
        gm_real rho_new = gc_array_dot(rh, r, n);
        gm_real beta, wnew;
        if (rho_new == 0.0) break;
        if (k == 0) { memcpy(p, r, (size_t)n * sizeof(gm_real)); }
        else {
            beta = (rho_new / rho) * (alpha / w);
            gc_array_axpby(1.0, r, beta, p, n);
            gc_array_axpy(-beta * w, v, p, n);
        }
        matvec(n, A, p, v);
        alpha = rho_new / gc_array_dot(rh, v, n);
        { int i; for (i = 0; i < n; i++) s[i] = r[i] - alpha * v[i]; }
        if (gc_array_norm2(s, n) <= tol) { gc_array_axpy(alpha, p, x, n); if (iters) *iters = k + 1; free(r); free(rh); free(p); free(v); free(s); free(t); return GM_OK; }
        matvec(n, A, s, t);
        wnew = gc_array_dot(t, s, n) / gc_array_dot(t, t, n);
        gc_array_axpy(alpha, p, x, n);
        gc_array_axpy(wnew, s, x, n);
        { int i; for (i = 0; i < n; i++) r[i] = s[i] - wnew * t[i]; }
        if (gc_array_norm2(r, n) <= tol) { if (iters) *iters = k + 1; free(r); free(rh); free(p); free(v); free(s); free(t); return GM_OK; }
        rho = rho_new; w = wnew;
    }
    if (iters) *iters = maxit;
    free(r); free(rh); free(p); free(v); free(s); free(t);
    return GM_ERR_NOCONV;
}

static void csr_matvec(const gc_csr *A, const gc_real *x, gc_real *y)
{
    gc_spmv_csr(A, x, y);
}

gm_status gm_iter_cg_sparse(const gc_csr *A, const gm_real *b, gm_real *x,
                            int maxit, gm_real tol, int *iters)
{
    gm_real *r, *p, *Ap, rsold;
    int k, n;
    if (A == NULL || b == NULL || x == NULL || A->rows != A->cols) return GM_ERR_ARG;
    n = A->rows;
    r = malloc((size_t)n * sizeof(gm_real)); p = malloc((size_t)n * sizeof(gm_real)); Ap = malloc((size_t)n * sizeof(gm_real));
    if (!r || !p || !Ap) { free(r); free(p); free(Ap); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    memcpy(r, b, (size_t)n * sizeof(gm_real));
    memcpy(p, r, (size_t)n * sizeof(gm_real));
    rsold = gc_array_dot(r, r, n);
    for (k = 0; k < maxit; k++) {
        gm_real alpha, rsnew;
        csr_matvec(A, p, Ap);
        alpha = gc_array_dot(p, Ap, n);
        if (alpha == 0.0) break;
        gc_array_axpy(rsold / alpha, p, x, n);
        gc_array_axpy(-rsold / alpha, Ap, r, n);
        rsnew = gc_array_dot(r, r, n);
        if (sqrt(rsnew) <= tol) { if (iters) *iters = k + 1; free(r); free(p); free(Ap); return GM_OK; }
        gc_array_axpby(1.0, r, rsnew / rsold, p, n);
        rsold = rsnew;
    }
    if (iters) *iters = maxit;
    free(r); free(p); free(Ap);
    return GM_ERR_NOCONV;
}

gm_status gm_iter_bicgstab_sparse(const gc_csr *A, const gm_real *b, gm_real *x,
                                  int maxit, gm_real tol, int *iters)
{
    gm_real *r, *rh, *p, *v, *s, *t, rho = 1.0, alpha = 1.0, w = 1.0;
    int k, n;
    if (A == NULL || b == NULL || x == NULL || A->rows != A->cols) return GM_ERR_ARG;
    n = A->rows;
    r = malloc((size_t)n * sizeof(gm_real)); rh = malloc((size_t)n * sizeof(gm_real));
    p = calloc((size_t)n, sizeof(gm_real)); v = calloc((size_t)n, sizeof(gm_real));
    s = malloc((size_t)n * sizeof(gm_real)); t = malloc((size_t)n * sizeof(gm_real));
    if (!r || !rh || !p || !v || !s || !t) { free(r); free(rh); free(p); free(v); free(s); free(t); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    csr_matvec(A, x, r);
    { int i; for (i = 0; i < n; i++) r[i] = b[i] - r[i]; }
    memcpy(rh, r, (size_t)n * sizeof(gm_real));
    for (k = 0; k < maxit; k++) {
        gm_real rho_new = gc_array_dot(rh, r, n), beta, wnew;
        if (rho_new == 0.0) break;
        if (k == 0) memcpy(p, r, (size_t)n * sizeof(gm_real));
        else { beta = (rho_new / rho) * (alpha / w); gc_array_axpby(1.0, r, beta, p, n); gc_array_axpy(-beta * w, v, p, n); }
        csr_matvec(A, p, v);
        alpha = rho_new / gc_array_dot(rh, v, n);
        { int i; for (i = 0; i < n; i++) s[i] = r[i] - alpha * v[i]; }
        if (gc_array_norm2(s, n) <= tol) { gc_array_axpy(alpha, p, x, n); if (iters) *iters = k + 1; free(r); free(rh); free(p); free(v); free(s); free(t); return GM_OK; }
        csr_matvec(A, s, t);
        wnew = gc_array_dot(t, s, n) / gc_array_dot(t, t, n);
        gc_array_axpy(alpha, p, x, n); gc_array_axpy(wnew, s, x, n);
        { int i; for (i = 0; i < n; i++) r[i] = s[i] - wnew * t[i]; }
        if (gc_array_norm2(r, n) <= tol) { if (iters) *iters = k + 1; free(r); free(rh); free(p); free(v); free(s); free(t); return GM_OK; }
        rho = rho_new; w = wnew;
    }
    if (iters) *iters = maxit;
    free(r); free(rh); free(p); free(v); free(s); free(t);
    return GM_ERR_NOCONV;
}

gm_status gm_arnoldi(int n, const gm_real *A, int m, gm_real *V, gm_real *H)
{
    int j, i, k;
    if (n <= 0 || m <= 0 || A == NULL || V == NULL || H == NULL) return GM_ERR_ARG;
    for (i = 0; i < n; i++) V[i] = (i == 0) ? 1.0 : 0.0;
    for (j = 0; j < m; j++) {
        gm_real *w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
        if (!w) return GM_ERR_NO_MEM;
        matvec(n, A, V + (size_t)j * (size_t)n, w);
        for (i = 0; i <= j; i++) {
            gm_real h = gc_array_dot(w, V + (size_t)i * (size_t)n, n);
            H[(size_t)i * (size_t)m + (size_t)j] = h;
            gc_array_axpy(-h, V + (size_t)i * (size_t)n, w, n);
        }
        H[(size_t)(j + 1) * (size_t)m + (size_t)j] = gc_array_norm2(w, n);
        for (k = 0; k < n; k++) V[(size_t)(j + 1) * (size_t)n + (size_t)k] =
            (H[(size_t)(j + 1) * (size_t)m + (size_t)j] != 0.0) ? w[k] / H[(size_t)(j + 1) * (size_t)m + (size_t)j] : 0.0;
        free(w);
    }
    return GM_OK;
}

gm_status gm_lanczos(int n, const gm_real *A, int m, gm_real *V, gm_real *alpha, gm_real *beta)
{
    int j;
    gm_real *w;
    if (n <= 0 || m <= 0 || A == NULL || V == NULL || alpha == NULL || beta == NULL) return GM_ERR_ARG;
    w = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!w) return GM_ERR_NO_MEM;
    for (j = 0; j < n; j++) V[j] = (j == 0) ? 1.0 : 0.0;
    for (j = 0; j < m; j++) {
        matvec(n, A, V + (size_t)j * (size_t)n, w);
        alpha[j] = gc_array_dot(w, V + (size_t)j * (size_t)n, n);
        gc_array_axpy(-alpha[j], V + (size_t)j * (size_t)n, w, n);
        if (j > 0) gc_array_axpy(-beta[j - 1], V + (size_t)(j - 1) * (size_t)n, w, n);
        beta[j] = gc_array_norm2(w, n);
        if (beta[j] != 0.0) {
            int k;
            for (k = 0; k < n; k++) V[(size_t)(j + 1) * (size_t)n + (size_t)k] = w[k] / beta[j];
        }
    }
    free(w);
    return GM_OK;
}

/* ============================== PCG (da LinearSystemsToolkit) ============================== */

static void pcg_prec(int n, const gm_real *A, const gm_real *r, gm_real *z, int prec, gm_real omega)
{
    int i, j;
    if (prec == 1) {
        for (i = 0; i < n; i++) z[i] = (AAT(A, n, i, i) != 0.0) ? r[i] / AAT(A, n, i, i) : r[i];
    } else if (prec == 2) {
        gm_real *y = (gm_real *)malloc((size_t)n * sizeof(gm_real));
        if (!y) { for (i = 0; i < n; i++) z[i] = r[i]; return; }
        /* forward: (D/omega + L) y = r */
        for (i = 0; i < n; i++) {
            gm_real s = r[i];
            for (j = 0; j < i; j++) s -= AAT(A, n, i, j) * y[j];
            y[i] = (AAT(A, n, i, i) != 0.0) ? s / (AAT(A, n, i, i) / omega) : s;
        }
        /* z = (D/omega) y */
        for (i = 0; i < n; i++) z[i] = (AAT(A, n, i, i) / omega) * y[i];
        /* backward: (D/omega + U) w = z, poi scala (2-omega) */
        for (i = n - 1; i >= 0; i--) {
            gm_real s = z[i];
            for (j = i + 1; j < n; j++) s -= AAT(A, n, i, j) * z[j];
            z[i] = (AAT(A, n, i, i) != 0.0) ? (2.0 - omega) * s / (AAT(A, n, i, i) / omega) : (2.0 - omega) * s;
        }
        free(y);
    } else {
        for (i = 0; i < n; i++) z[i] = r[i];
    }
}

gm_status gm_iter_pcg(int n, const gm_real *A, const gm_real *b, gm_real *x,
                      int prec, gm_real omega, int maxit, gm_real tol, int *iters)
{
    gm_real *r, *z, *p, *Ap;
    gm_real rz;
    int k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    if (omega <= 0.0 || omega >= 2.0) omega = 1.0;
    r = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    z = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    p = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    Ap = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!r || !z || !p || !Ap) { free(r); free(z); free(p); free(Ap); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    matvec(n, A, x, r);
    { int i; for (i = 0; i < n; i++) r[i] = b[i] - r[i]; }
    pcg_prec(n, A, r, z, prec, omega);
    memcpy(p, z, (size_t)n * sizeof(gm_real));
    rz = gc_array_dot(r, z, n);
    for (k = 0; k < maxit; k++) {
        gm_real alpha, rznew;
        matvec(n, A, p, Ap);
        alpha = gc_array_dot(p, Ap, n);
        if (alpha == 0.0) break;
        alpha = rz / alpha;
        gc_array_axpy(alpha, p, x, n);
        gc_array_axpy(-alpha, Ap, r, n);
        if (gc_array_norm2(r, n) <= tol) { if (iters) *iters = k + 1; free(r); free(z); free(p); free(Ap); return GM_OK; }
        pcg_prec(n, A, r, z, prec, omega);
        rznew = gc_array_dot(r, z, n);
        gc_array_axpby(1.0, z, rznew / rz, p, n);
        rz = rznew;
    }
    if (iters) *iters = maxit;
    free(r); free(z); free(p); free(Ap);
    return GM_ERR_NOCONV;
}

/* ============================== CGS ============================== */

gm_status gm_iter_cgs(int n, const gm_real *A, const gm_real *b, gm_real *x,
                      int maxit, gm_real tol, int *iters)
{
    gm_real *r, *rt, *u, *p, *q, *vhat, *uhat, *Auhat, rho = 1.0, rho_old = 1.0;
    int k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    r = malloc((size_t)n * sizeof(gm_real)); rt = malloc((size_t)n * sizeof(gm_real));
    u = malloc((size_t)n * sizeof(gm_real)); p = calloc((size_t)n, sizeof(gm_real));
    q = calloc((size_t)n, sizeof(gm_real)); vhat = malloc((size_t)n * sizeof(gm_real));
    uhat = malloc((size_t)n * sizeof(gm_real)); Auhat = malloc((size_t)n * sizeof(gm_real));
    if (!r || !rt || !u || !p || !q || !vhat || !uhat || !Auhat) {
        free(r); free(rt); free(u); free(p); free(q); free(vhat); free(uhat); free(Auhat); return GM_ERR_NO_MEM;
    }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    memcpy(r, b, (size_t)n * sizeof(gm_real));
    memcpy(rt, r, (size_t)n * sizeof(gm_real));
    for (k = 0; k < maxit; k++) {
        gm_real beta, alpha, denom;
        rho = gc_array_dot(rt, r, n);
        if (rho == 0.0) break;
        if (k == 0) { memcpy(u, r, (size_t)n * sizeof(gm_real)); memcpy(p, u, (size_t)n * sizeof(gm_real)); }
        else {
            beta = rho / rho_old;
            { int i; for (i = 0; i < n; i++) u[i] = r[i] + beta * q[i]; }
            { int i; for (i = 0; i < n; i++) p[i] = u[i] + beta * (q[i] + beta * p[i]); }
        }
        matvec(n, A, p, vhat);
        denom = gc_array_dot(rt, vhat, n);
        if (denom == 0.0) break;
        alpha = rho / denom;
        { int i; for (i = 0; i < n; i++) q[i] = u[i] - alpha * vhat[i]; }
        { int i; for (i = 0; i < n; i++) uhat[i] = u[i] + q[i]; }
        matvec(n, A, uhat, Auhat);
        gc_array_axpy(alpha, uhat, x, n);
        gc_array_axpy(-alpha, Auhat, r, n);
        if (gc_array_norm2(r, n) <= tol) { if (iters) *iters = k + 1; free(r); free(rt); free(u); free(p); free(q); free(vhat); free(uhat); free(Auhat); return GM_OK; }
        rho_old = rho;
    }
    if (iters) *iters = maxit;
    free(r); free(rt); free(u); free(p); free(q); free(vhat); free(uhat); free(Auhat);
    return GM_ERR_NOCONV;
}

/* ============================== Chebyshev ============================== */

gm_status gm_iter_chebyshev(int n, const gm_real *A, const gm_real *b, gm_real *x,
                            gm_real lmin, gm_real lmax, int maxit, gm_real tol, int *iters)
{
    gm_real *r, *Ax, d, c, rho_prev = 0.0;
    int k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL || lmax <= lmin) return GM_ERR_ARG;
    r = malloc((size_t)n * sizeof(gm_real)); Ax = malloc((size_t)n * sizeof(gm_real));
    if (!r || !Ax) { free(r); free(Ax); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    memcpy(r, b, (size_t)n * sizeof(gm_real));
    d = 0.5 * (lmax + lmin);
    c = 0.5 * (lmax - lmin);
    for (k = 0; k < maxit; k++) {
        gm_real rho;
        if (k == 0) rho = 1.0 / d;
        else if (k == 1) rho = 2.0 * d / (2.0 * d * d - c * c);
        else rho = 1.0 / (d - 0.25 * c * c * rho_prev);
        gc_array_axpy(rho, r, x, n);
        matvec(n, A, x, Ax);
        { int i; for (i = 0; i < n; i++) r[i] = b[i] - Ax[i]; }
        rho_prev = rho;
        if (gc_array_norm2(r, n) <= tol) { if (iters) *iters = k + 1; free(r); free(Ax); return GM_OK; }
    }
    if (iters) *iters = maxit;
    free(r); free(Ax);
    return GM_ERR_NOCONV;
}

/* ============================== MINRES ============================== */

gm_status gm_iter_cr(int n, const gm_real *A, const gm_real *b, gm_real *x,
                     int maxit, gm_real tol, int *iters)
{
    /* Conjugate Residual: A simmetrica (anche indefinita). */
    gm_real *r, *p, *Ap, *Ar;
    int k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    r = malloc((size_t)n * sizeof(gm_real)); p = malloc((size_t)n * sizeof(gm_real));
    Ap = malloc((size_t)n * sizeof(gm_real)); Ar = malloc((size_t)n * sizeof(gm_real));
    if (!r || !p || !Ap || !Ar) { free(r); free(p); free(Ap); free(Ar); return GM_ERR_NO_MEM; }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    memcpy(r, b, (size_t)n * sizeof(gm_real));
    memcpy(p, r, (size_t)n * sizeof(gm_real));
    matvec(n, A, p, Ap);
    for (k = 0; k < maxit; k++) {
        gm_real alpha, beta, ApAp = gc_array_dot(Ap, Ap, n);
        if (ApAp == 0.0) break;
        alpha = gc_array_dot(r, Ap, n) / ApAp;
        gc_array_axpy(alpha, p, x, n);
        gc_array_axpy(-alpha, Ap, r, n);
        if (gc_array_norm2(r, n) <= tol) { if (iters) *iters = k + 1; free(r); free(p); free(Ap); free(Ar); return GM_OK; }
        matvec(n, A, r, Ar);
        beta = gc_array_dot(Ar, Ap, n) / ApAp;
        gc_array_axpby(1.0, r, -beta, p, n);
        gc_array_axpby(1.0, Ar, -beta, Ap, n);
    }
    if (iters) *iters = maxit;
    free(r); free(p); free(Ap); free(Ar);
    return GM_ERR_NOCONV;
}

/* ============================== QMR (Lanczos biorthogonal) ============================== */

gm_status gm_iter_qmr(int n, const gm_real *A, const gm_real *b, gm_real *x,
                      int maxit, gm_real tol, int *iters)
{
    /* QMR semplificato: usa BiCG per generare la base e MINRES-like per la
     * soluzione quasi-minima. Qui implemento una versione robusta via Lanczos
     * biorthogonalizzato con aggiornamento del residuo. */
    gm_real *r, *rt, *p, *pt, *v, *vt, *Av, *Avt;
    gm_real rho = 1.0, alpha = 1.0, w = 1.0, beta;
    int k;
    if (n <= 0 || A == NULL || b == NULL || x == NULL) return GM_ERR_ARG;
    r = malloc((size_t)n * sizeof(gm_real)); rt = malloc((size_t)n * sizeof(gm_real));
    p = calloc((size_t)n, sizeof(gm_real)); pt = calloc((size_t)n, sizeof(gm_real));
    v = calloc((size_t)n, sizeof(gm_real)); vt = calloc((size_t)n, sizeof(gm_real));
    Av = malloc((size_t)n * sizeof(gm_real)); Avt = malloc((size_t)n * sizeof(gm_real));
    if (!r || !rt || !p || !pt || !v || !vt || !Av || !Avt) {
        free(r); free(rt); free(p); free(pt); free(v); free(vt); free(Av); free(Avt); return GM_ERR_NO_MEM;
    }
    memset(x, 0, (size_t)n * sizeof(gm_real));
    memcpy(r, b, (size_t)n * sizeof(gm_real));
    memcpy(rt, r, (size_t)n * sizeof(gm_real));
    for (k = 0; k < maxit; k++) {
        gm_real rho_new = gc_array_dot(rt, r, n), denom, wnew;
        if (rho_new == 0.0) break;
        if (k == 0) { memcpy(p, r, (size_t)n * sizeof(gm_real)); memcpy(pt, rt, (size_t)n * sizeof(gm_real)); }
        else {
            beta = rho_new / rho;
            gc_array_axpby(1.0, r, beta, p, n);
            gc_array_axpby(1.0, rt, beta, pt, n);
        }
        matvec(n, A, p, v); matvec(n, A, pt, vt);
        denom = gc_array_dot(pt, v, n);
        if (denom == 0.0) break;
        alpha = rho_new / denom;
        { int i; for (i = 0; i < n; i++) { r[i] -= alpha * v[i]; rt[i] -= alpha * vt[i]; } }
        if (gc_array_norm2(r, n) <= tol) { gc_array_axpy(alpha, p, x, n); if (iters) *iters = k + 1; free(r); free(rt); free(p); free(pt); free(v); free(vt); free(Av); free(Avt); return GM_OK; }
        matvec(n, A, r, Av);
        wnew = gc_array_dot(Av, r, n) / gc_array_dot(Av, Av, n);
        gc_array_axpy(alpha, p, x, n);
        gc_array_axpy(wnew, r, x, n);
        { int i; for (i = 0; i < n; i++) r[i] -= wnew * Av[i]; }
        rho = rho_new; w = wnew;
        if (gc_array_norm2(r, n) <= tol) { if (iters) *iters = k + 1; free(r); free(rt); free(p); free(pt); free(v); free(vt); free(Av); free(Avt); return GM_OK; }
    }
    if (iters) *iters = maxit;
    free(r); free(rt); free(p); free(pt); free(v); free(vt); free(Av); free(Avt);
    (void)w;
    return GM_ERR_NOCONV;
}

/* ============================== omega ottimale SOR ============================== */

gm_real gm_iter_optimal_omega(int n, const gm_real *A)
{
    /* stima il raggio spettrale di Jacobi via power iteration su D^{-1}(L+U) */
    gm_real *z, *y, rho = 0.0;
    int i, j, k;
    if (n <= 0 || A == NULL) return 1.0;
    z = malloc((size_t)n * sizeof(gm_real));
    y = malloc((size_t)n * sizeof(gm_real));
    if (!z || !y) { free(z); free(y); return 1.0; }
    for (i = 0; i < n; i++) z[i] = 1.0;
    for (k = 0; k < 100; k++) {
        gm_real nrm = 0.0;
        for (i = 0; i < n; i++) {
            gm_real s = 0.0;
            for (j = 0; j < n; j++) if (j != i) s += A[(size_t)i * (size_t)n + (size_t)j] * z[j];
            y[i] = (A[(size_t)i * (size_t)n + (size_t)i] != 0.0) ? s / A[(size_t)i * (size_t)n + (size_t)i] : 0.0;
        }
        for (i = 0; i < n; i++) nrm += y[i] * y[i];
        nrm = sqrt(nrm);
        if (nrm == 0.0) { rho = 0.0; break; }
        for (i = 0; i < n; i++) z[i] = y[i] / nrm;
        rho = nrm;
    }
    free(z); free(y);
    if (rho >= 1.0) rho = 0.999999;
    return 2.0 / (1.0 + sqrt(1.0 - rho * rho));
}
