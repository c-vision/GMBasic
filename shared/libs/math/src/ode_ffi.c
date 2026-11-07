/*
 * ode_ffi.c — shim ABI per il namespace BASIC ODE (metodi avanzati di gmbmath-numerical).
 *
 * gm_ode2 lavora con una callback `void (*)(t, y, dy, n, ctx)` che SCRIVE dy. L'FFI
 * passa una callback BASIC nella forma "ritorno ARRAY" (la sola portabile: su IL gli
 * array in ingresso vengono copiati, quindi scrivere sul secondo array non arriverebbe
 * indietro):
 *
 *     f(t AS DOUBLE, y() AS DOUBLE) AS ARRAY   ->   r(0..n-1) = dy/dt
 *
 * Il ponte alloca il buffer di ritorno a ogni chiamata, come lo shim di lagrangian.
 *
 * Lo stato y NON è in ingresso/uscita: si passa y0() e si riceve yout(), perché la
 * modifica in place di un array in ingresso non è portabile (IL copia).
 *
 * Per i problemi ai limiti y'' = f(x, y, y') la callback è scalare:
 *     fn(x AS DOUBLE, y AS DOUBLE, dy AS DOUBLE) AS DOUBLE
 *
 * Il ponte usa stato globale (sincrono, thread singolo), come gli altri shim con callback.
 */
#include "gm_ode2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;

typedef double (*ode_cb)(const double *y, i64 n, double *out);
typedef double (*bvp_cb)(double x, double y, double dy);

static ode_cb g_ode;
static bvp_cb g_bvp;

static void ode_bridge(gm_real t, const gm_real *y, gm_real *dy, int n, void *ctx)
{
    double *buf;
    int i;
    (void)ctx;
    (void)t;   /* l'RHS è autonomo: il tempo entra nello stato se serve */
    if (!g_ode || !dy || n < 1) return;
    buf = (double *)calloc((size_t)n, sizeof(double));
    if (!buf) return;
    (void)g_ode((const double *)y, (i64)n, buf);
    for (i = 0; i < n; i++) dy[i] = (gm_real)buf[i];
    free(buf);
}

static void bvp_bridge(gm_real x, gm_real y, gm_real dy, gm_real *out, void *ctx)
{
    (void)ctx;
    if (out) *out = g_bvp ? (gm_real)g_bvp((double)x, (double)y, (double)dy) : (gm_real)0.0;
}

/* Copia y0 -> buffer di lavoro, esegue il metodo, copia il risultato in out. */
static i64 run_ode(ode_cb f, const double *y0, i64 y_n, double *out, i64 out_n,
                   int method, double a, double b, i64 steps, i64 maxit, double tol)
{
    double *y;
    gm_status st = GM_OK;
    if (!f || !y0 || !out || y_n < 1 || out_n < y_n || steps < 1) return -1;
    y = (double *)malloc((size_t)y_n * sizeof(double));
    if (!y) return -1;
    memcpy(y, y0, (size_t)y_n * sizeof(double));
    g_ode = f;
    switch (method)
    {
        case 0: st = gm_ode_adams_bashforth4(ode_bridge, NULL, y, (int)y_n, a, b, (int)steps); break;
        case 1: st = gm_ode_adams_moulton4(ode_bridge, NULL, y, (int)y_n, a, b, (int)steps); break;
        case 2: st = gm_ode_bdf2(ode_bridge, NULL, y, (int)y_n, a, b, (int)steps, (int)maxit, tol); break;
        case 3: st = gm_ode_implicit_midpoint(ode_bridge, NULL, y, (int)y_n, a, b, (int)steps, (int)maxit, tol); break;
        case 4: st = gm_ode_verlet(ode_bridge, NULL, y, (int)y_n, a, b, (int)steps); break;
        default: st = GM_ERR_ARG; break;
    }
    g_ode = NULL;
    if (st != GM_OK) { free(y); return -1; }
    memcpy(out, y, (size_t)y_n * sizeof(double));
    free(y);
    return 0;
}

i64 ofx_ode_ab4(ode_cb f, const double *y0, i64 y_n, double a, double b, i64 steps,
                double *out, i64 out_n)
{
    return run_ode(f, y0, y_n, out, out_n, 0, a, b, steps, 0, 0.0);
}

i64 ofx_ode_am4(ode_cb f, const double *y0, i64 y_n, double a, double b, i64 steps,
                double *out, i64 out_n)
{
    return run_ode(f, y0, y_n, out, out_n, 1, a, b, steps, 0, 0.0);
}

i64 ofx_ode_bdf2(ode_cb f, const double *y0, i64 y_n, double a, double b, i64 steps,
                 i64 maxit, double tol, double *out, i64 out_n)
{
    return run_ode(f, y0, y_n, out, out_n, 2, a, b, steps, maxit, tol);
}

i64 ofx_ode_implicit_midpoint(ode_cb f, const double *y0, i64 y_n, double a, double b,
                              i64 steps, i64 maxit, double tol, double *out, i64 out_n)
{
    return run_ode(f, y0, y_n, out, out_n, 3, a, b, steps, maxit, tol);
}

i64 ofx_ode_verlet(ode_cb f, const double *y0, i64 y_n, double a, double b, i64 steps,
                   double *out, i64 out_n)
{
    return run_ode(f, y0, y_n, out, out_n, 4, a, b, steps, 0, 0.0);
}

/* BVP y'' = f(x, y, y'), y(a) = alpha, y(b) = beta: out() ha out_n nodi. */
i64 ofx_ode_bvp_shooting(bvp_cb f, double a, double b, double alpha, double beta,
                         double *out, i64 out_n)
{
    gm_status st;
    if (!f || !out || out_n < 2) return -1;
    g_bvp = f;
    st = gm_bvp_shooting(bvp_bridge, NULL, a, b, alpha, beta, (int)out_n, out);
    g_bvp = NULL;
    return st == GM_OK ? 0 : -1;
}

i64 ofx_ode_bvp_fd(bvp_cb f, double a, double b, double alpha, double beta,
                   double *out, i64 out_n)
{
    gm_status st;
    if (!f || !out || out_n < 2) return -1;
    g_bvp = f;
    st = gm_bvp_fd(bvp_bridge, NULL, a, b, alpha, beta, (int)out_n, out);
    g_bvp = NULL;
    return st == GM_OK ? 0 : -1;
}
