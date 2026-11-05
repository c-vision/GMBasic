/*
 * gm_pde.h — equazioni alle derivate parziali (differenze finite).
 */
#ifndef GMMATH_GM_PDE_H
#define GMMATH_GM_PDE_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -u'' = f su (0,1) con u(0)=u(1)=0; u ha n punti interni, h passo. */
gm_status gm_pde_poisson1d(const gm_real *f, gm_real *u, int n, gm_real h);
/* -Δu = f su griglia nx×ny (row-major) con bordo nullo; u ha nx*ny nodi interni. */
gm_status gm_pde_poisson2d(const gm_real *f, gm_real *u, int nx, int ny, gm_real h);
/* Calore esplicito: u_t = alpha u_xx, n nodi, bordo nullo, steps passi. */
gm_status gm_pde_heat_explicit(const gm_real *u0, gm_real *u, int n,
                               gm_real dx, gm_real dt, gm_real alpha, int steps);
/* Onda: u_tt = c^2 u_xx, n nodi, bordo nullo. */
gm_status gm_pde_wave(const gm_real *u0, const gm_real *v0, gm_real *u, int n,
                      gm_real dx, gm_real dt, gm_real c, int steps);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_PDE_H */
