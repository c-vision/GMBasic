/* gs_mvdist.c — distribuzioni multivariate (da et_al.). */
#include "gs_mvdist.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Cholesky di cov; restituisce half_logdet = sum log L_ii e q = ||L^{-1}(x-loc)||^2.
 * Ritorna 0 su errore (cov non SPD / memoria). */
static int mv_prepare(int d, const gc_real *x, const gc_real *loc, const gc_real *cov,
                      gc_real *half_logdet, gc_real *q)
{
    gc_real *L, *y;
    int i, j;
    if (d <= 0 || !x || !loc || !cov) return 0;
    L = (gc_real *)malloc((size_t)d * d * sizeof(gc_real));
    y = (gc_real *)malloc((size_t)d * sizeof(gc_real));
    if (!L || !y) { free(L); free(y); return 0; }
    memcpy(L, cov, (size_t)d * d * sizeof(gc_real));
    if (gm_linalg_cholesky(d, L) != GM_OK) { free(L); free(y); return 0; }
    *half_logdet = 0.0;
    for (i = 0; i < d; i++) *half_logdet += log(L[i * d + i]);
    for (i = 0; i < d; i++) {
        gc_real s = x[i] - loc[i];
        for (j = 0; j < i; j++) s -= L[i * d + j] * y[j];
        y[i] = s / L[i * d + i];
    }
    *q = 0.0;
    for (i = 0; i < d; i++) *q += y[i] * y[i];
    free(L); free(y);
    return 1;
}

gc_real gs_mvgauss_logpdf(int d, const gc_real *x, const gc_real *loc, const gc_real *cov)
{
    gc_real half_logdet, q;
    if (!mv_prepare(d, x, loc, cov, &half_logdet, &q)) return NAN;
    return -0.5 * q - half_logdet - 0.5 * (gc_real)d * log(2.0 * M_PI);
}

gc_real gs_mvstudent_logpdf(int d, const gc_real *x, const gc_real *loc,
                            const gc_real *cov, gc_real nu)
{
    gc_real half_logdet, q, lognorm;
    if (isinf(nu)) return gs_mvgauss_logpdf(d, x, loc, cov);
    if (!mv_prepare(d, x, loc, cov, &half_logdet, &q)) return NAN;
    lognorm = lgamma(0.5 * (nu + (gc_real)d)) - lgamma(0.5 * nu)
            - 0.5 * (gc_real)d * log(nu * M_PI);
    return lognorm - half_logdet - 0.5 * (nu + (gc_real)d) * log1p(q / nu);
}

gc_real gs_mvgauss_pdf(int d, const gc_real *x, const gc_real *loc, const gc_real *cov)
{
    return exp(gs_mvgauss_logpdf(d, x, loc, cov));
}

gc_real gs_mvstudent_pdf(int d, const gc_real *x, const gc_real *loc,
                         const gc_real *cov, gc_real nu)
{
    return exp(gs_mvstudent_logpdf(d, x, loc, cov, nu));
}

/* --- Copule archimedeane --- */

double gs_copula_clayton_cdf(double u, double v, double theta)
{
    double x;
    if (theta <= 0 || u <= 0 || u >= 1 || v <= 0 || v >= 1) return 0.0 / 0.0;
    x = pow(u, -theta) + pow(v, -theta) - 1.0;
    return x > 0 ? pow(x, -1.0 / theta) : 0.0;
}

double gs_copula_gumbel_cdf(double u, double v, double theta)
{
    double a, b;
    if (theta < 1 || u <= 0 || u >= 1 || v <= 0 || v >= 1) return 0.0 / 0.0;
    a = pow(-log(u), theta);
    b = pow(-log(v), theta);
    return exp(-pow(a + b, 1.0 / theta));
}

double gs_copula_frank_cdf(double u, double v, double theta)
{
    double a, b, e;
    if (theta <= 0 || u <= 0 || u >= 1 || v <= 0 || v >= 1) return 0.0 / 0.0;
    a = exp(-theta * u);
    b = exp(-theta * v);
    e = exp(-theta);
    return -log(1.0 + (a - 1.0) * (b - 1.0) / (e - 1.0)) / theta;
}

