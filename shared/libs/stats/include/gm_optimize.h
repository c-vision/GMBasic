/*
 * gm_optimize.h — ottimizzazione numerica (SciPy optimize), non nel legacy.
 *
 * Nelder-Mead, BFGS (gradiente numerico), gradiente coniugato, Powell,
 * sezione aurea, Brent 1D, Levenberg-Marquardt e curve_fit.
 */
#ifndef GMMATH_GM_OPTIMIZE_H
#define GMMATH_GM_OPTIMIZE_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* f(x, d, ctx) da minimizzare. */
typedef gm_real (*gm_opt_fn)(const gm_real *x, int d, void *ctx);

typedef struct gm_opt_result {
    gm_real fval;
    int iters;
    int success;
} gm_opt_result;

/* Minimizzazione multi-dimensionale: x[d] iniziale e soluzione. */
gm_opt_result gm_opt_nelder_mead(gm_opt_fn f, void *ctx, gm_real *x, int d,
                                 gm_real tol, int maxit);
gm_opt_result gm_opt_bfgs(gm_opt_fn f, void *ctx, gm_real *x, int d,
                          gm_real tol, int maxit);
gm_opt_result gm_opt_conjugate_gradient(gm_opt_fn f, void *ctx, gm_real *x, int d,
                                        gm_real tol, int maxit);

/* Minimizzazione 1D su [a,b]. */
gm_real gm_opt_golden_section(gm_real (*f)(gm_real, void *), void *ctx,
                              gm_real a, gm_real b, gm_real tol);
gm_real gm_opt_brent(gm_real (*f)(gm_real, void *), void *ctx,
                     gm_real a, gm_real b, gm_real tol);

/* Levenberg-Marquardt per minimi quadrati: residui r(x, m, ctx) -> m valori. */
typedef void (*gm_opt_res_fn)(const gm_real *x, int d, void *ctx, gm_real *res, int m);
gm_opt_result gm_opt_levenberg_marquardt(gm_opt_res_fn r, void *ctx, gm_real *x, int d,
                                         int m, gm_real tol, int maxit);

/* curve_fit: modello model(x, p, d, ctx) con parametri p[d] (fit di y).
 * x sono gli m punti indipendenti, y gli m valori osservati. */
typedef gm_real (*gm_opt_model_fn)(gm_real x, const gm_real *p, int d, void *ctx);
gm_opt_result gm_opt_curve_fit(gm_opt_model_fn model, void *ctx, const gm_real *x,
                               const gm_real *y, int m, gm_real *p, int d,
                               gm_real tol, int maxit);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_OPTIMIZE_H */
