/*
 * gm_ode2.h — ODE avanzati e problemi ai limiti (riscrittura).
 *
 * Multi-step (Adams-Bashforth/Moulton), BDF2 (stiff), midpoint implicito,
 * integrazione simplettica, shooting e differenze finite per BVP.
 */
#ifndef GMMATH_GM_ODE2_H
#define GMMATH_GM_ODE2_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* y' = f(t, y), y in R^n. */
typedef void (*gm_ode_fn)(gm_real t, const gm_real *y, gm_real *dy, int n, void *ctx);

gm_status gm_ode_adams_bashforth4(gm_ode_fn f, void *ctx, gm_real *y, int n,
                                  gm_real a, gm_real b, int steps);
gm_status gm_ode_adams_moulton4(gm_ode_fn f, void *ctx, gm_real *y, int n,
                                gm_real a, gm_real b, int steps);
gm_status gm_ode_bdf2(gm_ode_fn f, void *ctx, gm_real *y, int n,
                      gm_real a, gm_real b, int steps, int maxit, gm_real tol);
gm_status gm_ode_implicit_midpoint(gm_ode_fn f, void *ctx, gm_real *y, int n,
                                   gm_real a, gm_real b, int steps, int maxit, gm_real tol);
/* Simplettico (Euler simplettico) per sistemi Hamiltoniani y = (q, p):
 * dq/dt = dH/dp, dp/dt = -dH/dq passati come f(q,p). */
gm_status gm_ode_symplectic_euler(gm_ode_fn f, void *ctx, gm_real *y, int n,
                                  gm_real a, gm_real b, int steps);
/* Velocity Verlet per y'' = a(t, y): y = (posizioni, velocita). */
gm_status gm_ode_verlet(gm_ode_fn acc, void *ctx, gm_real *y, int n,
                        gm_real a, gm_real b, int steps);

/* BVP y'' = f(x, y, y'), y(a)=alpha, y(b)=beta su griglia n nodi. */
gm_status gm_bvp_shooting(void (*f)(gm_real, gm_real, gm_real, gm_real *, void *),
                          void *ctx, gm_real a, gm_real b, gm_real alpha, gm_real beta,
                          int n, gm_real *y);
gm_status gm_bvp_fd(void (*f)(gm_real, gm_real, gm_real, gm_real *, void *),
                    void *ctx, gm_real a, gm_real b, gm_real alpha, gm_real beta,
                    int n, gm_real *y);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_ODE2_H */
