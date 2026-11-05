/* gs_mvdist_sample.c — copule archimedeane: sampler (RNG).
 * Parte di gmbstats (simulation); le pdf/logpdf e le CDF stanno in gmbstats-stats. */
#include "gs_mvdist.h"
#include "gs_sim.h"

#include <math.h>

/* Positive stable variate (Kanter) per il sampler Gumbel. */
static double stable_pos(double alpha, unsigned *s)
{
    /* Stable positiva S(alpha,1,cos(pi alpha/2)^{1/alpha},0): Laplace exp(-s^alpha). */
    double U, W, X;
    if (alpha == 1.0) return 1.0;
    U = 3.14159265358979 * ((*s = *s * 1103515245u + 12345u) % 100000u) / 100000.0;
    if (U <= 0) U = 1e-9;
    if (U >= 3.14159265358979) U = 3.14159265358979 - 1e-9;
    W = -log(1.0 - ((*s = *s * 1103515245u + 12345u) % 100000u) / 100000.0);
    {
        double sA = sin(alpha * U), sU = sin(U), c = cos(3.14159265358979 * alpha / 2.0);
        double term1 = pow(sA / sU, alpha / (1.0 - alpha));
        double term2 = pow(sin((1.0 - alpha) * U) / W, (1.0 - alpha) / alpha);
        X = term1 * term2 * pow(c > 0 ? c : 1e-9, 1.0 / alpha);
        if (!(X > 0) || !isfinite(X)) X = 1.0;
    }
    return X;
}

int gs_copula_clayton_sample(double theta, unsigned seed, double *u, double *v)
{
    double V, a, b;
    unsigned s = seed ? seed : 12345u;
    gs_rng r = { s };
    if (theta <= 0 || !u || !v) return GC_ERR_ARG;
    V = gs_rng_gamma(&r, 1.0 / theta, 1.0);
    a = (double)((r.s = r.s * 1103515245u + 12345u) % 100000u) / 100000.0;
    b = (double)((r.s = r.s * 1103515245u + 12345u) % 100000u) / 100000.0;
    *u = pow(1.0 - log(a > 0 ? a : 1e-9) / V, -1.0 / theta);
    *v = pow(1.0 - log(b > 0 ? b : 1e-9) / V, -1.0 / theta);
    return GC_OK;
}

int gs_copula_gumbel_sample(double theta, unsigned seed, double *u, double *v)
{
    double V, a, b;
    unsigned s = seed ? seed : 12345u;
    if (theta < 1 || !u || !v) return GC_ERR_ARG;
    V = stable_pos(1.0 / theta, &s);
    a = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
    b = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
    *u = exp(-pow(-log(a > 0 ? a : 1e-9) / V, 1.0 / theta));
    *v = exp(-pow(-log(b > 0 ? b : 1e-9) / V, 1.0 / theta));
    return GC_OK;
}

int gs_copula_frank_sample(double theta, unsigned seed, double *u, double *v)
{
    /* V ~ Log-series(1 - e^-theta) via Devroye. */
    double p = 1.0 - exp(-theta);
    double C = theta;   /* -ln(1-p) */
    double U0, V = 1.0, F;
    unsigned s = seed ? seed : 12345u;
    double a, b;
    if (theta <= 0 || !u || !v) return GC_ERR_ARG;
    U0 = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
    F = p / C;          /* CDF(V=1) */
    {
        double term = F;
        while (U0 > F) {
            V += 1.0;
            term *= p * (V - 1.0) / V;   /* prossimo PMF term */
            F += term;                   /* accumula la CDF */
            if (V > 1e6) break;
        }
    }
    a = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
    b = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
    *u = -log(1.0 + (exp(-theta) - 1.0) * pow(a > 0 ? a : 1e-9, 1.0 / V)) / theta;
    *v = -log(1.0 + (exp(-theta) - 1.0) * pow(b > 0 ? b : 1e-9, 1.0 / V)) / theta;
    return GC_OK;
}
