/*
 * or_ffi_util.c — shim ABI per OR-UTIL del namespace OPTIMIZE
 * (code, inventario, MCDM, MDP, simulazione).
 *
 * Dove la firma C supera gli 8 argomenti FFI: parametri di servizio fissi
 * (tolleranze, seed) e output impacchettati in un solo array.
 */
#include "gor_util.h"

#include <stdlib.h>
#include <stdint.h>

typedef long long i64;
typedef double (*ffx_util_cb)(const double *, long long);

static ffx_util_cb g_cb;

static gor_real ffx_util_bridge(const gor_real *x, int d, void *ctx)
{
    (void)ctx;
    return (gor_real)g_cb((const double *)x, (long long)d);
}

/* ── code ─────────────────────────────────────────────────────────────── */

/* out() = L, Lq, W, Wq (4); ritorna rho. */
double ofx_queue_mm1(double lambda, double mu, double *out, int64_t out_n)
{
    gor_real L = 0, Lq = 0, W = 0, Wq = 0, rho;
    if (out == NULL) return -1.0;
    rho = gor_queue_mm1((gor_real)lambda, (gor_real)mu, &L, &Lq, &W, &Wq);
    out[0] = (double)L; out[1] = (double)Lq; out[2] = (double)W; out[3] = (double)Wq;
    return (double)rho;
}

/* out() = L, Lq, W, Wq (4); ritorna rho. */
double ofx_queue_mmc(double lambda, double mu, i64 c, double *out, int64_t out_n)
{
    gor_real L = 0, Lq = 0, W = 0, Wq = 0, rho;
    if (out == NULL || c <= 0) return -1.0;
    rho = gor_queue_mmc((gor_real)lambda, (gor_real)mu, (int)c, &L, &Lq, &W, &Wq);
    out[0] = (double)L; out[1] = (double)Lq; out[2] = (double)W; out[3] = (double)Wq;
    return (double)rho;
}

double ofx_littles_law(double lambda, double w) { return (double)gor_littles_law((gor_real)lambda, (gor_real)w); }

/* ── inventario ───────────────────────────────────────────────────────── */

double ofx_eoq(double demand, double setup, double holding)
{ return (double)gor_eoq((gor_real)demand, (gor_real)setup, (gor_real)holding); }

double ofx_epq(double demand, double setup, double holding, double rate)
{ return (double)gor_epq((gor_real)demand, (gor_real)setup, (gor_real)holding, (gor_real)rate); }

double ofx_newsvendor(double cu, double co, double mean, double sd)
{ return (double)gor_newsvendor((gor_real)cu, (gor_real)co, (gor_real)mean, (gor_real)sd); }

double ofx_safety_stock(double z, double sigma)
{ return (double)gor_safety_stock((gor_real)z, (gor_real)sigma); }

/* ── MCDM ─────────────────────────────────────────────────────────────── */

i64 ofx_ahp(const double *pairwise, int64_t pairwise_n, i64 n, double *weights, int64_t weights_n, double *consistency, int64_t consistency_n)
{
    gor_real cons = 0.0;
    gor_status r;
    if (n <= 0 || pairwise == NULL || weights == NULL) return 0;
    r = gor_ahp((int)n, (const gor_real *)pairwise, (gor_real *)weights, &cons);
    if (consistency != NULL) consistency[0] = (double)cons;
    return (r == GOR_OK) ? 1 : 0;
}

i64 ofx_topsis(const double *X, int64_t X_n, i64 m, i64 n, const double *w, int64_t w_n, const i64 *benefit, int64_t benefit_n, double *scores, int64_t scores_n)
{
    int *b;
    gor_status r;
    if (m <= 0 || n <= 0 || X == NULL || w == NULL || benefit == NULL || scores == NULL) return 0;
    b = (int *)malloc((size_t)n * sizeof(int));
    if (b == NULL) return 0;
    { i64 i; for (i = 0; i < n; i++) b[i] = benefit[i] ? 1 : 0; }
    r = gor_topsis((int)m, (int)n, (const gor_real *)X, (const gor_real *)w, b, (gor_real *)scores);
    free(b);
    return (r == GOR_OK) ? 1 : 0;
}

/* ── MDP ──────────────────────────────────────────────────────────────── */

/* out() = V[ns] | policy[ns] (policy come double). tol fissa 1e-8. */
i64 ofx_mdp_value(const double *P, int64_t P_n, const double *R, int64_t R_n, i64 ns, i64 na, double gamma, i64 maxit, double *out, int64_t out_n)
{
    gor_real *V;
    int *pol;
    int it;
    i64 i;
    if (ns <= 0 || na <= 0 || P == NULL || R == NULL || out == NULL) return -1;
    V = (gor_real *)malloc((size_t)ns * sizeof(gor_real));
    pol = (int *)malloc((size_t)ns * sizeof(int));
    if (V == NULL || pol == NULL) { free(V); free(pol); return -1; }
    it = gor_mdp_value_iteration((int)ns, (int)na, (const gor_real *)P, (const gor_real *)R,
                                 (gor_real)gamma, 1e-8, (int)maxit, V, pol);
    for (i = 0; i < ns; i++) { out[i] = (double)V[i]; out[ns + i] = (double)pol[i]; }
    free(V); free(pol);
    return (i64)it;
}

i64 ofx_mdp_policy(const double *P, int64_t P_n, const double *R, int64_t R_n, i64 ns, i64 na, double gamma, i64 maxit, double *out, int64_t out_n)
{
    gor_real *V;
    int *pol;
    int it;
    i64 i;
    if (ns <= 0 || na <= 0 || P == NULL || R == NULL || out == NULL) return -1;
    V = (gor_real *)malloc((size_t)ns * sizeof(gor_real));
    pol = (int *)malloc((size_t)ns * sizeof(int));
    if (V == NULL || pol == NULL) { free(V); free(pol); return -1; }
    it = gor_mdp_policy_iteration((int)ns, (int)na, (const gor_real *)P, (const gor_real *)R,
                                  (gor_real)gamma, (int)maxit, V, pol);
    for (i = 0; i < ns; i++) { out[i] = (double)V[i]; out[ns + i] = (double)pol[i]; }
    free(V); free(pol);
    return (i64)it;
}

/* ── simulazione ──────────────────────────────────────────────────────── */

/* out(0) = deviazione standard. */
double ofx_montecarlo(ffx_util_cb f, i64 d, const double *lo, int64_t lo_n, const double *hi, int64_t hi_n, i64 n, double *out, int64_t out_n)
{
    gor_real sd = 0.0;
    double r;
    if (f == NULL || lo == NULL || hi == NULL || d <= 0 || n <= 0) return -1.0;
    g_cb = f;
    r = (double)gor_montecarlo(ffx_util_bridge, NULL, (int)d, (const gor_real *)lo,
                               (const gor_real *)hi, (int)n, 12345UL, &sd);
    if (out != NULL) out[0] = (double)sd;
    return r;
}

/* out(0) = VaR. */
double ofx_cvar(const double *returns, int64_t returns_n, i64 n, double alpha, double *out, int64_t out_n)
{
    gor_real var = 0.0;
    double r;
    if (n <= 0 || returns == NULL) return -1.0;
    r = (double)gor_cvar((int)n, (const gor_real *)returns, (gor_real)alpha, &var);
    if (out != NULL) out[0] = (double)var;
    return r;
}

/* out(0) = utilizzazione. */
double ofx_des_mm1(double lambda, double mu, i64 ncust, double *out, int64_t out_n)
{
    gor_real util = 0.0;
    double r;
    if (ncust <= 0) return -1.0;
    r = (double)gor_des_mm1((gor_real)lambda, (gor_real)mu, (int)ncust, 12345UL, &util);
    if (out != NULL) out[0] = (double)util;
    return r;
}

/* out(0) = z. */
double ofx_chance_constraint(const double *sigma, int64_t sigma_n, const double *x, int64_t x_n, i64 n, double alpha, double *out, int64_t out_n)
{
    gor_real shift = 0.0;
    double r;
    if (n <= 0 || sigma == NULL || x == NULL) return -1.0;
    r = (double)gor_chance_constraint((int)n, (const gor_real *)sigma, (const gor_real *)x,
                                      (gor_real)alpha, &shift);
    if (out != NULL) out[0] = (double)shift;
    return r;
}

#include <stdint.h>
/* Trampolini DECLARE NATIVE (ffi_ext_native.pl): firma originale, estensioni = INT64_MAX. */
i64 ofx_ahp__nat(const double *pairwise, i64 n, double *weights, double *consistency) { return ofx_ahp(pairwise, INT64_MAX, n, weights, INT64_MAX, consistency, INT64_MAX); }
double ofx_chance_constraint__nat(const double *sigma, const double *x, i64 n, double alpha, double *out) { return ofx_chance_constraint(sigma, INT64_MAX, x, INT64_MAX, n, alpha, out, INT64_MAX); }
double ofx_cvar__nat(const double *returns, i64 n, double alpha, double *out) { return ofx_cvar(returns, INT64_MAX, n, alpha, out, INT64_MAX); }
double ofx_des_mm1__nat(double lambda, double mu, i64 ncust, double *out) { return ofx_des_mm1(lambda, mu, ncust, out, INT64_MAX); }
i64 ofx_mdp_policy__nat(const double *P, const double *R, i64 ns, i64 na, double gamma, i64 maxit, double *out) { return ofx_mdp_policy(P, INT64_MAX, R, INT64_MAX, ns, na, gamma, maxit, out, INT64_MAX); }
i64 ofx_mdp_value__nat(const double *P, const double *R, i64 ns, i64 na, double gamma, i64 maxit, double *out) { return ofx_mdp_value(P, INT64_MAX, R, INT64_MAX, ns, na, gamma, maxit, out, INT64_MAX); }
double ofx_montecarlo__nat(ffx_util_cb f, i64 d, const double *lo, const double *hi, i64 n, double *out) { return ofx_montecarlo(f, d, lo, INT64_MAX, hi, INT64_MAX, n, out, INT64_MAX); }
double ofx_queue_mmc__nat(double lambda, double mu, i64 c, double *out) { return ofx_queue_mmc(lambda, mu, c, out, INT64_MAX); }
i64 ofx_topsis__nat(const double *X, i64 m, i64 n, const double *w, const i64 *benefit, double *scores) { return ofx_topsis(X, INT64_MAX, m, n, w, INT64_MAX, benefit, INT64_MAX, scores, INT64_MAX); }
