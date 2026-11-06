/*
 * gn_pinn.h — loss physics-informed (PINN) da tsotchke/PINN.
 *
 * Residui al quadrato di equazioni della fisica, da usare come termine di loss
 * in una rete che approssima la soluzione. Le derivate (u_t, u_xx, ...) sono
 * fornite dal chiamante (autodiff o differenze finite).
 */
#ifndef GMBNEURAL_GN_PINN_H
#define GMBNEURAL_GN_PINN_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Heat: (u_t - alpha*u_xx)^2. */
gn_real gn_pinn_heat(gn_real u_t, gn_real u_xx, gn_real alpha);
/* Wave: (u_tt - c^2*u_xx)^2. */
gn_real gn_pinn_wave(gn_real u_tt, gn_real u_xx, gn_real c);
/* Laplace/Poisson: (u_xx + u_yy - f)^2. */
gn_real gn_pinn_laplace(gn_real u_xx, gn_real u_yy, gn_real f);
/* Advection: (u_t + beta*u_x)^2. */
gn_real gn_pinn_advection(gn_real u_t, gn_real u_x, gn_real beta);
/* Burgers: (u_t + u*u_x - nu*u_xx)^2. */
gn_real gn_pinn_burgers(gn_real u_t, gn_real u_x, gn_real u_xx, gn_real u, gn_real nu);
/* Condizioni al bordo/iniziali: (u - target)^2. */
gn_real gn_pinn_boundary(gn_real u, gn_real u_bc);
gn_real gn_pinn_initial(gn_real u, gn_real u0);

/* Normalizzazione adattiva dei termini di loss: ritorna max(|loss_i|, eps). */
gn_real gn_pinn_adaptive_norm(const gn_real *losses, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_PINN_H */
