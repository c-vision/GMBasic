/*
 * gs_kelly.c — criterio di Kelly generale (fondazione).
 */
#include "gs_kelly.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

double gs_kelly_binary(double p, double b)
{
    double f;
    if (b <= 0.0 || p <= 0.0 || p >= 1.0) return 0.0;
    f = (p * b - (1.0 - p)) / b;
    return f > 0.0 ? f : 0.0;
}

double gs_kelly_from_odds(double p, double dec)
{
    double f;
    if (dec <= 1.0 || p <= 0.0 || p >= 1.0) return 0.0;
    f = (p * dec - 1.0) / (dec - 1.0);
    return f > 0.0 ? f : 0.0;
}

double gs_kelly_continuous(double mu, double sigma2, double rf)
{
    if (sigma2 <= 0.0) return 0.0;
    return (mu - rf) / sigma2;
}

gc_status gs_kelly_multi(const double *mu, const double *cov, int d, double rf, double *f)
{
    double *A, *b;
    int i;
    if (!mu || !cov || !f || d < 1) return GC_ERR_ARG;
    A = (double *)malloc((size_t)d * (size_t)d * sizeof(double));
    b = (double *)malloc((size_t)d * sizeof(double));
    if (!A || !b) { free(A); free(b); return GC_ERR_NO_MEM; }
    memcpy(A, cov, (size_t)d * (size_t)d * sizeof(double));
    for (i = 0; i < d; i++) b[i] = mu[i] - rf;
    if (gm_linalg_solve(d, A, b, f) != 0) { free(A); free(b); return GC_ERR_SINGULAR; }
    free(A); free(b);
    return GC_OK;
}

double gs_kelly_growth(double f, double p, double b)
{
    if (f < 0.0 || f >= 1.0 || b <= 0.0) return -1e300;
    return p * log(1.0 + f * b) + (1.0 - p) * log(1.0 - f);
}

double gs_kelly_fraction(double f, double frac) { return f * frac; }
double gs_kelly_binary_capped(double p, double b, double cap)
{
    double f = gs_kelly_binary(p, b);
    return (cap > 0.0 && f > cap) ? cap : f;
}
