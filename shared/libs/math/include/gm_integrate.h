/*
 * gm_integrate.h — integrazione numerica e ODE (riscrittura, da ccmath).
 */
#ifndef GMMATH_GM_INTEGRATE_H
#define GMMATH_GM_INTEGRATE_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Quadratura di Simpson con n sottointervalli (n pari). */
gm_real gm_integrate_simpson(gm_real (*f)(gm_real, void *), void *ctx,
                             gm_real a, gm_real b, int n);
/* Simpson adattiva (bisezione finché l'errore stimato < tol). */
gm_real gm_integrate_adaptive(gm_real (*f)(gm_real, void *), void *ctx,
                              gm_real a, gm_real b, gm_real tol);
/* Integrale di una serie di Chebyshev su [-1,1]. */
gm_real gm_integrate_chebyshev(const gm_real *coef, int m);

/* Integra un sistema y' = f(t,y) con RK4 classico.
 * f(t, y, dydt, n, ctx). y viene aggiornato. Ritorna 0. */
gm_status gm_ode_system(void (*f)(gm_real, const gm_real *, gm_real *, int, void *),
                        void *ctx, gm_real *y, int n, gm_real a, gm_real b, int steps);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_INTEGRATE_H */
