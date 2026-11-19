/* gs_dist_extra.c — distribuzioni completate per la parità con il namespace BASIC `STATS`.
 *
 * Perché esiste (2026-11-19): la migrazione a FFI richiede che la libreria sia la sola fonte.
 * Queste 5 funzioni esistevano SOLO nel builtin (`CommonLib.Numerics`), quindi non potevano
 * essere deprecate. Le formule sono quelle del riferimento C#, ricalcate 1:1:
 *   isf(p) = ppf(1 - p)   (per uniform, lognormal, triangular)
 *   triangular_cdf: formula a due rami (sotto/sopra la moda)
 */
#include "gs_dist.h"
#include <math.h>

/* Lognormal: isf(p) = ppf(1-p) */
double gs_dist_lognormal_isf(double mu, double sigma, double p)
{
    return gs_dist_lognormal_ppf(mu, sigma, 1.0 - p);
}

/* Uniform: isf(p) = ppf(1-p) */
double gs_dist_uniform_isf(double a, double b, double p)
{
    return gs_dist_uniform_ppf(a, b, 1.0 - p);
}

/* Triangular CDF: (a = minimo, b = massimo, c = moda) */
double gs_dist_triangular_cdf(double a, double b, double c, double x)
{
    if (b <= a || c < a || c > b) return NAN;
    if (x <= a) return 0.0;
    if (x >= b) return 1.0;
    if (x <= c) return (x - a) * (x - a) / ((b - a) * (c - a));
    return 1.0 - (b - x) * (b - x) / ((b - a) * (b - c));
}

double gs_dist_triangular_sf(double a, double b, double c, double x)
{
    return 1.0 - gs_dist_triangular_cdf(a, b, c, x);
}

/* Triangular ISF: isf(p) = ppf(1-p) */
double gs_dist_triangular_isf(double a, double b, double c, double p)
{
    return gs_dist_triangular_ppf(a, b, c, 1.0 - p);
}
