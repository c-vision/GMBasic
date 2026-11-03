/*
 * gor_optimize.h — ottimizzazione globale e root finding (SciPy optimize).
 *
 * Wrapper moderno degli optimizer già presenti nel legacy gmi_optfit.c:
 * Powell, COBYLA, SLSQP, TNC, brute force, basin hopping, differential
 * evolution, curve_fit (LM), least_squares, brentq/brenth/ridder.
 */
#ifndef GMBORTOOLS_GOR_OPTIMIZE_H
#define GMBORTOOLS_GOR_OPTIMIZE_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* f(x, n, ctx) da minimizzare; out riceve la soluzione. */
typedef gor_real (*gor_opt_fn)(const gor_real *x, int n, void *ctx);

/* Modello per curve fitting: y = model(p, np, x, ctx). */
typedef gor_real (*gor_opt_model_fn)(const gor_real *p, int np, gor_real x, void *ctx);

void gor_opt_powell(gor_opt_fn f, void *ctx, const gor_real *x0, int n, gor_real *out);
void gor_opt_cobyla(gor_opt_fn f, void *ctx, const gor_real *x0, int n,
                    const gor_real *lo, const gor_real *hi, gor_real *out);
void gor_opt_slsqp(gor_opt_fn f, void *ctx, const gor_real *x0, int n,
                   const gor_real *lo, const gor_real *hi, gor_real *out);
void gor_opt_tnc(gor_opt_fn f, void *ctx, const gor_real *x0, int n,
                 const gor_real *lo, const gor_real *hi, gor_real *out);
void gor_opt_brute(gor_opt_fn f, void *ctx, const gor_real *lo, const gor_real *hi,
                   int n, int N, gor_real *out);
void gor_opt_basin_hopping(gor_opt_fn f, void *ctx, const gor_real *x0, int n,
                           int niter, gor_real *out);
void gor_opt_differential_evolution(gor_opt_fn f, void *ctx, const gor_real *lo,
                                    const gor_real *hi, int n, int maxit,
                                    gor_real tol, gor_real *out);

/* L-BFGS (limited-memory BFGS, Nocedal & Wright; da QFLib #217): quasi-Newton
 * con two-loop recursion (m coppie), gradiente numerico centrale, line search
 * Armijo. out = minimo. */
void gor_opt_lbfgs(gor_opt_fn f, void *ctx, const gor_real *x0, int n,
                   int m, int maxit, gor_real tol, gor_real *out);

/* Root finding 1D su [a,b]. */
gor_real gor_opt_brentq(gor_real (*f)(gor_real, void *), void *ctx, gor_real a, gor_real b);
gor_real gor_opt_brenth(gor_real (*f)(gor_real, void *), void *ctx, gor_real a, gor_real b);
gor_real gor_opt_ridder(gor_real (*f)(gor_real, void *), void *ctx, gor_real a, gor_real b);

/* Bayesian Optimization (GP + Expected Improvement): campiona n_init punti
 * random, fitta una GP (kernel RBF) sui punti valutati e per n_iter iterazioni
 * massimizza la acquisizione EI per scegliere il prossimo punto. out = minimo.
 * lo/hi = bounds per dimensione. */
void gor_opt_bayes(gor_opt_fn f, void *ctx, const gor_real *lo, const gor_real *hi,
                   int n, int n_init, int n_iter, gor_real *out);

/* ---- Adattatori scipy-like (parità con il namespace BASIC OPTIMIZE) ----
 * Aggiunti 2026-11-02: colmano i 5 nomi presenti nella superficie BASIC e
 * assenti in gmbortools (CurveFit, Minimize, MinimizeScalar, Simplex, CholeskySolve). */

/* curve_fit: modello f(p, np, x, ctx), dati (x, y, nx), guess p0[np] -> out[np]. */
void gor_opt_curve_fit(gor_opt_model_fn model, void *ctx,
                       const gor_real *x, const gor_real *y, int nx,
                       const gor_real *p0, int np, int maxiter, gor_real tol,
                       gor_real *out);

/* minimize: method = "nelder-mead" | "bfgs" | "l-bfgs" | "cg" | "powell" |
 * "tnc" | "cobyla" | "slsqp". lo/hi possono essere NULL (illimitato). */
int gor_opt_minimize(gor_opt_fn f, void *ctx, const gor_real *x0, int n,
                     const char *method, const gor_real *lo, const gor_real *hi,
                     gor_real *out);

/* minimize_scalar: method = "brent" | "golden". */
gor_real gor_opt_minimize_scalar(gor_real (*f)(gor_real, void *), void *ctx,
                                 gor_real a, gor_real b, const char *method);

/* simplex forma disuguaglianza: **max** c'x  s.t.  A x <= b, x >= 0
 * (parità con OPTIMIZE.SIMPLEX; OPTIMIZE.LINPROG è il minimizzatore).
 * A è m×n row-major. Ritorna GOR_OK (ottimo), GOR_ERR_INFEASIBLE,
 * GOR_ERR_NOCONV, GOR_ERR_ARG (b con elementi negativi non supportati). */
int gor_opt_simplex(const gor_real *c, const gor_real *A, int m, int n,
                    const gor_real *b, gor_real *x);

/* cholesky_solve: A (n×n simmetrica definita positiva, row-major), b -> x. */
int gor_opt_cholesky_solve(const gor_real *A, const gor_real *b, int n, gor_real *x);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_OPTIMIZE_H */
