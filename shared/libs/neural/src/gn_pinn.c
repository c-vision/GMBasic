/*
 * gn_pinn.c — loss physics-informed (da tsotchke/PINN).
 */
#include "gn_pinn.h"
#include <math.h>

static gn_real sq(gn_real x) { return x * x; }

gn_real gn_pinn_heat(gn_real u_t, gn_real u_xx, gn_real alpha)
{
    return sq(u_t - alpha * u_xx);
}

gn_real gn_pinn_wave(gn_real u_tt, gn_real u_xx, gn_real c)
{
    return sq(u_tt - c * c * u_xx);
}

gn_real gn_pinn_laplace(gn_real u_xx, gn_real u_yy, gn_real f)
{
    return sq(u_xx + u_yy - f);
}

gn_real gn_pinn_advection(gn_real u_t, gn_real u_x, gn_real beta)
{
    return sq(u_t + beta * u_x);
}

gn_real gn_pinn_burgers(gn_real u_t, gn_real u_x, gn_real u_xx, gn_real u, gn_real nu)
{
    return sq(u_t + u * u_x - nu * u_xx);
}

gn_real gn_pinn_boundary(gn_real u, gn_real u_bc)
{
    return sq(u - u_bc);
}

gn_real gn_pinn_initial(gn_real u, gn_real u0)
{
    return sq(u - u0);
}

gn_real gn_pinn_adaptive_norm(const gn_real *losses, int n)
{
    gn_real mx = 1e-8;
    int i;
    if (!losses || n < 1) return 1.0;
    for (i = 0; i < n; i++) { gn_real a = fabs(losses[i]); if (a > mx) mx = a; }
    return mx;
}
