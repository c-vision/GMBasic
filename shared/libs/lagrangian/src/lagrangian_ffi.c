/*
 * lagrangian_ffi.c — shim ABI per il rilassamento lagrangiano (namespace OPTIMIZE).
 *
 * La callback C del motore è `gor_real (*)(const gor_real *lambda, int m, void *ctx,
 * gor_real *x)`: riceve i moltiplicatori e SCRIVE la soluzione del subproblema nel
 * buffer `x`. Il trampolino FFI non può restituire due valori (valore + vettore), quindi
 * la callback BASIC usa la forma "ritorno ARRAY" (descriptor 144):
 *
 *     Sub(lam() AS DOUBLE) AS ARRAY  ->  r(0) = valore del subproblema,
 *                                        r(1..nx) = x(0..nx-1)
 *
 * Il bridge spacchetta il vettore restituito. Vale su tutti i backend (la forma a due
 * array con scrittura sul secondo non è portabile: IL copia gli array in ingresso).
 */
#include "gor_lagr.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;
typedef double (*ffx_lagr_cb)(const double *lam, i64 m, double *out);

static ffx_lagr_cb g_lagr_cb;
static int g_lagr_nx;

static gor_real ffx_lagr_bridge(const gor_real *lambda, int m, void *ctx, gor_real *x)
{
    double *buf;
    double val = 0.0;
    i64 got;
    int i;
    (void)ctx;
    if (g_lagr_cb == NULL || lambda == NULL || x == NULL || g_lagr_nx <= 0) return 0.0;
    buf = (double *)calloc((size_t)g_lagr_nx + 1u, sizeof(double));
    if (buf == NULL) return 0.0;
    got = (i64)g_lagr_cb(lambda, (i64)m, buf);
    if (got > 0) {
        val = buf[0];
        for (i = 0; i < g_lagr_nx; i++) x[i] = (gor_real)buf[i + 1];
    }
    free(buf);
    return (gor_real)val;
}

/* out = [lower bound, lambda(m), best_x(nx)] (1 + m + nx valori). Ritorna il lower bound. */
double ofx_lagr_subgradient(ffx_lagr_cb sub, const double *A, const double *b,
                            i64 m, i64 nx, i64 maxit, double *out)
{
    double *lam, *best_x;
    gor_real lb;
    i64 i;
    if (sub == NULL || A == NULL || b == NULL || out == NULL || m <= 0 || nx <= 0) return 0.0;

    lam = (double *)malloc((size_t)m * sizeof(double));
    best_x = (double *)malloc((size_t)nx * sizeof(double));
    if (lam == NULL || best_x == NULL) { free(lam); free(best_x); return 0.0; }
    for (i = 0; i < nx; i++) best_x[i] = 0.0;
    for (i = 0; i < m; i++) lam[i] = 0.0;

    g_lagr_cb = sub;                     /* impostata PRIMA della solve */
    g_lagr_nx = (int)nx;
    lb = gor_lagr_subgradient((int)m, (int)nx, A, b, ffx_lagr_bridge, NULL,
                              (int)(maxit > 0 ? maxit : 100), 0.0, lam, best_x);
    g_lagr_cb = NULL;

    out[0] = (double)lb;
    for (i = 0; i < m; i++) out[1 + i] = lam[i];
    for (i = 0; i < nx; i++) out[1 + m + i] = best_x[i];

    free(lam);
    free(best_x);
    return (double)lb;
}
