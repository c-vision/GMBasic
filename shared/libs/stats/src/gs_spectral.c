/* gs_spectral.c — Random Matrix Theory: bound di Marchenko-Pastur, filtro,
 * diagnostica di perturbazione (Weyl/Davis-Kahan), coerenza. */
#include "gs_spectral.h"
#include "gm_sparse_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

gc_status gs_rmt_mp_bounds(int n, int T, gc_real sigma2,
                           gc_real *out_lambda_plus, gc_real *out_lambda_minus)
{
    gc_real q, r;
    if (n < 1 || T < 1 || sigma2 < 0.0) return GC_ERR_ARG;
    q = (gc_real)n / (gc_real)T;
    r = sqrt(q);
    if (out_lambda_plus) *out_lambda_plus = sigma2 * (1.0 + r) * (1.0 + r);
    if (out_lambda_minus) *out_lambda_minus = sigma2 * (1.0 - r) * (1.0 - r);
    return GC_OK;
}

gc_status gs_rmt_filter(const gc_real *cov, int n, int T, gc_real *out_cov)
{
    gc_real *A = NULL, *eval = NULL, *evec = NULL, *lam = NULL, *F = NULL;
    gc_real *dscale = NULL, tr = 0.0, sigma2, lplus, lminus, sum_noise = 0.0, avg_noise;
    int i, j, k, n_noise = 0;
    if (!cov || !out_cov || n < 1 || T < 1) return GC_ERR_ARG;
    A = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    eval = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    evec = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    lam = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    F = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    dscale = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!A || !eval || !evec || !lam || !F || !dscale) {
        free(A); free(eval); free(evec); free(lam); free(F); free(dscale);
        return GC_ERR_NO_MEM;
    }
    memcpy(A, cov, (size_t)n * n * sizeof(gc_real));
    memset(eval, 0, (size_t)n * sizeof(gc_real));
    memset(evec, 0, (size_t)n * n * sizeof(gc_real));
    dmat_eig_jacobi(n, A, eval, evec);

    for (i = 0; i < n; i++) tr += eval[i];
    sigma2 = tr / (gc_real)n;
    gs_rmt_mp_bounds(n, T, sigma2, &lplus, &lminus);
    (void)lminus;
    for (i = 0; i < n; i++) if (eval[i] < lplus) { sum_noise += eval[i]; n_noise++; }
    avg_noise = n_noise > 0 ? sum_noise / (gc_real)n_noise : 0.0;
    for (i = 0; i < n; i++) lam[i] = (eval[i] < lplus) ? avg_noise : eval[i];

    /* F = V diag(lam) V^T (evec: colonna k = autovettore k) */
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            gc_real s = 0.0;
            for (k = 0; k < n; k++) s += evec[i * n + k] * lam[k] * evec[j * n + k];
            F[i * n + j] = s;
        }
    /* riscalatura: ripristina la diagonale originale (correlazione -> diag 1) */
    for (i = 0; i < n; i++) {
        gc_real d = F[i * n + i];
        dscale[i] = d > 1e-300 ? sqrt(cov[i * n + i] / d) : 1.0;
    }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) out_cov[i * n + j] = F[i * n + j] * dscale[i] * dscale[j];

    free(A); free(eval); free(evec); free(lam); free(F); free(dscale);
    return GC_OK;
}

gc_real gs_rmt_coherence(const gc_real *v, int n)
{
    gc_real mx = 0.0;
    int i;
    if (!v || n < 1) return 0.0 / 0.0;
    for (i = 0; i < n; i++) {
        gc_real a = v[i] < 0.0 ? -v[i] : v[i];
        if (a > mx) mx = a;
    }
    return (gc_real)n * mx * mx;
}

gc_real gs_rmt_spectral_norm(const gc_real *A, int n)
{
    gc_real *W, *eval, *evec, mx = 0.0;
    int i;
    if (!A || n < 1) return 0.0 / 0.0;
    W = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    eval = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    evec = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    if (!W || !eval || !evec) { free(W); free(eval); free(evec); return 0.0 / 0.0; }
    memcpy(W, A, (size_t)n * n * sizeof(gc_real));
    memset(eval, 0, (size_t)n * sizeof(gc_real));
    memset(evec, 0, (size_t)n * n * sizeof(gc_real));
    dmat_eig_jacobi(n, W, eval, evec);
    for (i = 0; i < n; i++) {
        gc_real a = eval[i] < 0.0 ? -eval[i] : eval[i];
        if (a > mx) mx = a;
    }
    free(W); free(eval); free(evec);
    return mx;
}

gc_real gs_rmt_davis_kahan_bound(gc_real e_norm, gc_real gap)
{
    if (gap <= 0.0) return 0.0 / 0.0;
    if (e_norm < 0.0) e_norm = -e_norm;
    return e_norm / gap;
}

gc_real gs_rmt_principal_angle(const gc_real *u, const gc_real *v, int n)
{
    gc_real nu = 0.0, nv = 0.0, dot = 0.0, c;
    int i;
    if (!u || !v || n < 1) return 0.0 / 0.0;
    for (i = 0; i < n; i++) { nu += u[i] * u[i]; nv += v[i] * v[i]; dot += u[i] * v[i]; }
    if (nu <= 0.0 || nv <= 0.0) return 0.0 / 0.0;
    c = dot < 0.0 ? -dot : dot;
    c /= sqrt(nu * nv);
    if (c > 1.0) c = 1.0;
    return acos(c);
}

gc_real gs_rmt_tightness(gc_real observed, gc_real bound)
{
    if (bound <= 0.0) return 0.0;
    if (observed < 0.0) observed = -observed;
    return observed / bound;
}
