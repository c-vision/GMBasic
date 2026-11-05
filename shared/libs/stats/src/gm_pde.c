/* gm_pde.c — PDE alle differenze finite (riscrittura). */
#include "gm_pde.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

gm_status gm_pde_poisson1d(const gm_real *f, gm_real *u, int n, gm_real h)
{
    /* tridiagonale (-1, 2, -1)/h^2, bordo nullo -> Thomas */
    gm_real *c = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    gm_real *d = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    int i;
    if (f == NULL || u == NULL || n <= 0) { free(c); free(d); return GM_ERR_ARG; }
    if (!c || !d) { free(c); free(d); return GM_ERR_NO_MEM; }
    for (i = 0; i < n; i++) { c[i] = -1.0 / (h * h); d[i] = 2.0 / (h * h); u[i] = f[i]; }
    /* forward */
    for (i = 1; i < n; i++) {
        gm_real m = c[i - 1] / d[i - 1];
        d[i] -= m * c[i - 1];
        u[i] -= m * u[i - 1];
    }
    /* backward */
    u[n - 1] /= d[n - 1];
    for (i = n - 2; i >= 0; i--) u[i] = (u[i] - c[i] * u[i + 1]) / d[i];
    free(c); free(d);
    return GM_OK;
}

gm_status gm_pde_poisson2d(const gm_real *f, gm_real *u, int nx, int ny, gm_real h)
{
    /* -Δu = f con 5 punti, bordo nullo; Jacobi/SOR */
    int i, j, it;
    gm_real *un;
    if (f == NULL || u == NULL || nx <= 0 || ny <= 0) return GM_ERR_ARG;
    un = (gm_real *)malloc((size_t)nx * (size_t)ny * sizeof(gm_real));
    if (!un) return GM_ERR_NO_MEM;
    for (i = 0; i < nx * ny; i++) u[i] = 0.0;
    for (it = 0; it < 5000; it++) {
        for (i = 0; i < nx; i++)
            for (j = 0; j < ny; j++) {
                gm_real s = h * h * f[(size_t)i * (size_t)ny + (size_t)j];
                gm_real cnt = 0.0;
                if (i > 0) { s += u[(size_t)(i - 1) * (size_t)ny + (size_t)j]; cnt += 1.0; }
                if (i < nx - 1) { s += u[(size_t)(i + 1) * (size_t)ny + (size_t)j]; cnt += 1.0; }
                if (j > 0) { s += u[(size_t)i * (size_t)ny + (size_t)(j - 1)]; cnt += 1.0; }
                if (j < ny - 1) { s += u[(size_t)i * (size_t)ny + (size_t)(j + 1)]; cnt += 1.0; }
                un[(size_t)i * (size_t)ny + (size_t)j] = (cnt > 0.0) ? s / cnt : 0.0;
            }
        memcpy(u, un, (size_t)nx * (size_t)ny * sizeof(gm_real));
    }
    free(un);
    return GM_OK;
}

gm_status gm_pde_heat_explicit(const gm_real *u0, gm_real *u, int n,
                               gm_real dx, gm_real dt, gm_real alpha, int steps)
{
    gm_real r;
    gm_real *un, *uc;
    int s, i;
    if (u0 == NULL || u == NULL || n <= 2) return GM_ERR_ARG;
    r = alpha * dt / (dx * dx);
    un = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    uc = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!un || !uc) { free(un); free(uc); return GM_ERR_NO_MEM; }
    memcpy(uc, u0, (size_t)n * sizeof(gm_real));
    uc[0] = 0.0; uc[n - 1] = 0.0;
    for (s = 0; s < steps; s++) {
        un[0] = 0.0; un[n - 1] = 0.0;
        for (i = 1; i < n - 1; i++)
            un[i] = uc[i] + r * (uc[i - 1] - 2.0 * uc[i] + uc[i + 1]);
        memcpy(uc, un, (size_t)n * sizeof(gm_real));
    }
    memcpy(u, uc, (size_t)n * sizeof(gm_real));
    free(un); free(uc);
    return GM_OK;
}

gm_status gm_pde_wave(const gm_real *u0, const gm_real *v0, gm_real *u, int n,
                      gm_real dx, gm_real dt, gm_real c, int steps)
{
    gm_real r2;
    gm_real *up, *uc, *un;
    int s, i;
    if (u0 == NULL || u == NULL || n <= 2) return GM_ERR_ARG;
    r2 = (c * dt / dx) * (c * dt / dx);
    up = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    uc = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    un = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!up || !uc || !un) { free(up); free(uc); free(un); return GM_ERR_NO_MEM; }
    memcpy(uc, u0, (size_t)n * sizeof(gm_real));
    for (i = 0; i < n; i++) up[i] = (v0 ? u0[i] - dt * v0[i] : u0[i]);
    for (s = 0; s < steps; s++) {
        un[0] = 0.0; un[n - 1] = 0.0;
        for (i = 1; i < n - 1; i++)
            un[i] = 2.0 * uc[i] - up[i] + r2 * (uc[i - 1] - 2.0 * uc[i] + uc[i + 1]);
        memcpy(up, uc, (size_t)n * sizeof(gm_real));
        memcpy(uc, un, (size_t)n * sizeof(gm_real));
    }
    memcpy(u, uc, (size_t)n * sizeof(gm_real));
    free(up); free(uc); free(un);
    return GM_OK;
}
