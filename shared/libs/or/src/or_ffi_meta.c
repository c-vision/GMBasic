/*
 * or_ffi_meta.c — shim ABI per le META-EURISTICHE del namespace OPTIMIZE.
 *
 * Le funzioni C prendono `gor_meta_fn = double (*)(const double*, int, void*)`.
 * Il trampolino FFI di GMBasic per la callback vettoriale ha ABI
 * `double (*)(const double*, int64_t)`: qui si fa da ponte con un puntatore
 * globale valido solo durante la chiamata (stesso schema dei trampolini FFI).
 * Seed fissi dove la firma C supererebbe gli 8 argomenti FFI.
 */
#include "gor_meta.h"

#include <stdlib.h>
#include <stdint.h>

typedef long long i64;
typedef double (*ffx_meta_cb)(const double *, long long);

static ffx_meta_cb g_cb1;
static ffx_meta_cb g_cb2;

static gor_real ffx_bridge1(const gor_real *x, int d, void *ctx)
{
    (void)ctx;
    return (gor_real)g_cb1((const double *)x, (long long)d);
}

static gor_real ffx_bridge2(const gor_real *x, int d, void *ctx)
{
    (void)ctx;
    return (gor_real)g_cb2((const double *)x, (long long)d);
}

double ofx_meta_hill_climb(ffx_meta_cb f, double *x, int64_t x_n, i64 d, const double *lo, int64_t lo_n, const double *hi, int64_t hi_n,
                           i64 maxit)
{
    if (f == NULL || x == NULL || lo == NULL || hi == NULL || d <= 0) return -1.0;
    g_cb1 = f;
    return (double)gor_meta_hill_climb(ffx_bridge1, NULL, (gor_real *)x, (int)d,
                                       (const gor_real *)lo, (const gor_real *)hi,
                                       (int)maxit, 12345UL);
}

double ofx_meta_sa(ffx_meta_cb f, double *x, int64_t x_n, i64 d, const double *lo, int64_t lo_n, const double *hi, int64_t hi_n,
                   i64 maxit, double t0, double alpha)
{
    if (f == NULL || x == NULL || lo == NULL || hi == NULL || d <= 0) return -1.0;
    g_cb1 = f;
    return (double)gor_meta_sa(ffx_bridge1, NULL, (gor_real *)x, (int)d,
                               (const gor_real *)lo, (const gor_real *)hi, (int)maxit,
                               (gor_real)t0, (gor_real)alpha, 12345UL);
}

double ofx_meta_tabu(ffx_meta_cb f, double *x, int64_t x_n, i64 d, const double *lo, int64_t lo_n, const double *hi, int64_t hi_n,
                     i64 maxit, i64 tenure)
{
    if (f == NULL || x == NULL || lo == NULL || hi == NULL || d <= 0) return -1.0;
    g_cb1 = f;
    return (double)gor_meta_tabu(ffx_bridge1, NULL, (gor_real *)x, (int)d,
                                 (const gor_real *)lo, (const gor_real *)hi, (int)maxit,
                                 (int)tenure, 12345UL);
}

double ofx_meta_ga(ffx_meta_cb f, double *x, int64_t x_n, i64 d, const double *lo, int64_t lo_n, const double *hi, int64_t hi_n,
                   i64 pop, i64 maxgen)
{
    if (f == NULL || x == NULL || lo == NULL || hi == NULL || d <= 0) return -1.0;
    g_cb1 = f;
    return (double)gor_meta_ga(ffx_bridge1, NULL, (gor_real *)x, (int)d,
                               (const gor_real *)lo, (const gor_real *)hi,
                               (int)pop, (int)maxgen, 12345UL);
}

double ofx_meta_pso(ffx_meta_cb f, double *x, int64_t x_n, i64 d, const double *lo, int64_t lo_n, const double *hi, int64_t hi_n,
                    i64 swarm, i64 maxit)
{
    if (f == NULL || x == NULL || lo == NULL || hi == NULL || d <= 0) return -1.0;
    g_cb1 = f;
    return (double)gor_meta_pso(ffx_bridge1, NULL, (gor_real *)x, (int)d,
                                (const gor_real *)lo, (const gor_real *)hi,
                                (int)swarm, (int)maxit, 12345UL);
}

double ofx_meta_de(ffx_meta_cb f, double *x, int64_t x_n, i64 d, const double *lo, int64_t lo_n, const double *hi, int64_t hi_n,
                   i64 pop, i64 maxgen)
{
    if (f == NULL || x == NULL || lo == NULL || hi == NULL || d <= 0) return -1.0;
    g_cb1 = f;
    return (double)gor_meta_de(ffx_bridge1, NULL, (gor_real *)x, (int)d,
                               (const gor_real *)lo, (const gor_real *)hi,
                               (int)pop, (int)maxgen, 12345UL);
}

double ofx_meta_cmaes(ffx_meta_cb f, double *x, int64_t x_n, i64 d, const double *lo, int64_t lo_n, const double *hi, int64_t hi_n,
                      i64 maxit)
{
    if (f == NULL || x == NULL || lo == NULL || hi == NULL || d <= 0) return -1.0;
    g_cb1 = f;
    return (double)gor_meta_cmaes(ffx_bridge1, NULL, (gor_real *)x, (int)d,
                                  (const gor_real *)lo, (const gor_real *)hi,
                                  (int)maxit, 12345UL);
}

double ofx_meta_es(ffx_meta_cb f, double *x, int64_t x_n, i64 d, double sigma, double alpha,
                   i64 pop, i64 maxit)
{
    if (f == NULL || x == NULL || d <= 0) return -1.0;
    g_cb1 = f;
    return (double)gor_meta_es(ffx_bridge1, NULL, (gor_real *)x, (int)d,
                               (gor_real)sigma, (gor_real)alpha, (int)pop, (int)maxit, 12345ULL);
}

double ofx_meta_ars(ffx_meta_cb f, double *x, int64_t x_n, i64 d, double sigma, double alpha,
                    i64 top_b, i64 pop, i64 maxit)
{
    if (f == NULL || x == NULL || d <= 0) return -1.0;
    g_cb1 = f;
    return (double)gor_meta_ars(ffx_bridge1, NULL, (gor_real *)x, (int)d,
                                (gor_real)sigma, (gor_real)alpha, (int)top_b, (int)pop,
                                (int)maxit, 12345ULL);
}

double ofx_meta_aco_tsp(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, i64 ants, i64 maxit)
{
    int *t;
    double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)malloc((size_t)n * sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_meta_aco_tsp((int)n, (const gor_real *)dist, t, (int)ants, (int)maxit, 12345UL);
    { i64 i; for (i = 0; i < n; i++) tour[i] = (i64)t[i]; }
    free(t);
    return r;
}

i64 ofx_tabu_tenure(i64 kind, i64 it, i64 n)
{
    return (i64)gor_tabu_tenure((int)kind, (int)it, (int)n);
}

/* NSGA-II: out() = pareto[pop*d] | fv1[pop] | fv2[pop] (impacchettati per stare in 8 arg). */
i64 ofx_meta_nsga2(ffx_meta_cb f1, ffx_meta_cb f2, i64 d, const double *lo, int64_t lo_n, const double *hi, int64_t hi_n,
                   i64 pop, i64 maxgen, double *out, int64_t out_n)
{
    gor_real *pareto, *fv1, *fv2;
    int k;
    if (f1 == NULL || f2 == NULL || lo == NULL || hi == NULL || out == NULL ||
        d <= 0 || pop <= 0) return -1;
    pareto = (gor_real *)malloc((size_t)(pop * d) * sizeof(gor_real));
    fv1 = (gor_real *)malloc((size_t)pop * sizeof(gor_real));
    fv2 = (gor_real *)malloc((size_t)pop * sizeof(gor_real));
    if (pareto == NULL || fv1 == NULL || fv2 == NULL) {
        free(pareto); free(fv1); free(fv2); return -1;
    }
    g_cb1 = f1;
    g_cb2 = f2;
    k = gor_meta_nsga2(ffx_bridge1, NULL, ffx_bridge2, NULL, (int)d,
                       (const gor_real *)lo, (const gor_real *)hi, (int)pop, (int)maxgen,
                       12345UL, pareto, fv1, fv2);
    if (k > 0) {
        i64 i;
        for (i = 0; i < (i64)pop * d; i++) out[i] = (double)pareto[i];
        for (i = 0; i < pop; i++) out[(i64)pop * d + i] = (double)fv1[i];
        for (i = 0; i < pop; i++) out[(i64)pop * d + pop + i] = (double)fv2[i];
    }
    free(pareto); free(fv1); free(fv2);
    return (i64)k;
}
