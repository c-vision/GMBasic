/*
 * gm_interp.h — interpolazione 1D (SciPy interp1d), wrapper del legacy.
 */
#ifndef GMMATH_GM_INTERP_H
#define GMMATH_GM_INTERP_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Interpolazione lineare in tx sui punti (x[i], y[i]) ordinati. */
gm_real gm_interp_linear(const gm_real *x, const gm_real *y, int n, gm_real tx);
/* Spline cubica naturale. */
gm_real gm_interp_cubic_spline(const gm_real *x, const gm_real *y, int n, gm_real tx);
/* PCHIP (monotona a tratti). */
gm_real gm_interp_pchip(const gm_real *x, const gm_real *y, int n, gm_real tx);
/* Akima. */
gm_real gm_interp_akima(const gm_real *x, const gm_real *y, int n, gm_real tx);
/* Interpolazione polinomiale. */
gm_real gm_interp_polynomial(const gm_real *x, const gm_real *y, int n, gm_real tx);
/* A gradini (valore del nodo a sinistra). */
gm_real gm_interp_step(const gm_real *x, const gm_real *y, int n, gm_real tx);

/* Interpolazione bilineare su griglia regolare 2D: punti x[nx] e y[ny]
 * crescenti, valori f[nx*ny] row-major (f[i*ny + j], i = indice x, j = indice y).
 * Estrapolazione flat (clamp agli estremi). Richiede nx >= 2 e ny >= 2.
 * Da AI/Sources (IV surface, clean-room). */
gm_real gm_interp2_bilinear(const gm_real *x, int nx, const gm_real *y, int ny,
                            const gm_real *f, gm_real px, gm_real py);

/* Integrazione: trapezoidi su (x,y) e cumulativa. */
gm_real gm_integrate_trapezoid(const gm_real *y, const gm_real *x, int n);
gm_status gm_integrate_cumtrapezoid(const gm_real *y, const gm_real *x, int n, gm_real *out);

/* Hagan-West Monotone Convex (da IRCE #223): interpola il fattore di sconto
 * df(t) da zero rates z[] ai pillar t[] (t crescenti, CC). Left: zero rate
 * costante; right: flat forward. Se fwd_out != NULL ritorna il forward
 * istantaneo a tx. */
gm_real gm_interp_monotone_convex(const gm_real *t, const gm_real *z, int n,
                                  gm_real tx, gm_real *fwd_out);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_INTERP_H */
