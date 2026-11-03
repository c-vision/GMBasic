/* ============================================================================
 * gmb_ode.c — scipy.integrate.solve_ivp (RK45 adattivo, Dormand-Prince 5(4)).
 * Traiettoria: matrice flat [rows][yd+1] (col0 = t). rows passato via outn.
 * ============================================================================ */

#include "gmb_runtime.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { double* traj; int rows; int cap; int yd; } gmb_ode_traj_t;

static void gmb_ode_add_row(gmb_ode_traj_t* tr, double t, const double* yy)
{
    if (tr->rows >= tr->cap)
    {
        tr->cap *= 2;
        tr->traj = (double*)realloc(tr->traj, sizeof(double) * (size_t)tr->cap * (size_t)(tr->yd + 1));
    }
    tr->traj[tr->rows * (tr->yd + 1)] = t;
    for (int i = 0; i < tr->yd; i++) tr->traj[tr->rows * (tr->yd + 1) + 1 + i] = yy[i];
    tr->rows++;
}

/* Callback: void f(double t, const double* y, int yd, double* dydt, void* ctx) */
double* gmb_solve_ivp_rk45(void (*f)(double, const double*, int, double*, void*), void* ctx,
    const double* y0, int yd, double t0, double t1, double rtol, double atol, int* out_rows)
{
    double* y = (double*)malloc(sizeof(double) * (size_t)yd);
    if (!y) { *out_rows = 0; return NULL; }
    for (int i = 0; i < yd; i++) y[i] = y0[i];

    double* k1 = (double*)malloc(sizeof(double) * (size_t)yd);
    double* k2 = (double*)malloc(sizeof(double) * (size_t)yd);
    double* k3 = (double*)malloc(sizeof(double) * (size_t)yd);
    double* k4 = (double*)malloc(sizeof(double) * (size_t)yd);
    double* k5 = (double*)malloc(sizeof(double) * (size_t)yd);
    double* k6 = (double*)malloc(sizeof(double) * (size_t)yd);
    double* k7 = (double*)malloc(sizeof(double) * (size_t)yd);
    double* yk = (double*)malloc(sizeof(double) * (size_t)yd);
    double* y5 = (double*)malloc(sizeof(double) * (size_t)yd);
    double* y4 = (double*)malloc(sizeof(double) * (size_t)yd);
    double* err = (double*)malloc(sizeof(double) * (size_t)yd);
    if (!k1 || !k2 || !k3 || !k4 || !k5 || !k6 || !k7 || !yk || !y5 || !y4 || !err)
    {
        free(y); free(k1); free(k2); free(k3); free(k4); free(k5); free(k6); free(k7);
        free(yk); free(y5); free(y4); free(err);
        *out_rows = 0;
        return NULL;
    }

    /* accumulatore righe */
    gmb_ode_traj_t tr;
    tr.traj = (double*)malloc(sizeof(double) * (size_t)64 * (size_t)(yd + 1));
    tr.rows = 0;
    tr.cap = 64;
    tr.yd = yd;
    if (!tr.traj) { free(y); *out_rows = 0; return NULL; }

    double t = t0;
    double h = sqrt(fmax(1e-12, fabs(t1 - t0)));
    gmb_ode_add_row(&tr, t, y);
    int guard = 0;
    while (fabs(t - t1) > 1e-12 && guard++ < 20000)
    {
        if (fabs(h) > fabs(t1 - t)) h = t1 - t;
        f(t, y, yd, k1, ctx);
        for (int i = 0; i < yd; i++) yk[i] = y[i] + h * (1.0 / 5.0) * k1[i];
        f(t + h / 5.0, yk, yd, k2, ctx);
        for (int i = 0; i < yd; i++) yk[i] = y[i] + h * ((3.0 / 40.0) * k1[i] + (9.0 / 40.0) * k2[i]);
        f(t + 3.0 * h / 10.0, yk, yd, k3, ctx);
        for (int i = 0; i < yd; i++) yk[i] = y[i] + h * ((44.0 / 45.0) * k1[i] - (56.0 / 15.0) * k2[i] + (32.0 / 9.0) * k3[i]);
        f(t + 4.0 * h / 5.0, yk, yd, k4, ctx);
        for (int i = 0; i < yd; i++) yk[i] = y[i] + h * ((19372.0 / 6561.0) * k1[i] - (25360.0 / 2187.0) * k2[i] + (64448.0 / 6561.0) * k3[i] - (212.0 / 729.0) * k4[i]);
        f(t + h, yk, yd, k5, ctx);
        for (int i = 0; i < yd; i++) yk[i] = y[i] + h * ((9017.0 / 3168.0) * k1[i] - (355.0 / 33.0) * k2[i] + (46732.0 / 5247.0) * k3[i] + (49.0 / 176.0) * k4[i] - (5103.0 / 18656.0) * k5[i]);
        f(t + h, yk, yd, k6, ctx);
        for (int i = 0; i < yd; i++)
            y5[i] = y[i] + h * ((35.0 / 384.0) * k1[i] + (500.0 / 1113.0) * k3[i] + (125.0 / 192.0) * k4[i] - (2187.0 / 6784.0) * k5[i] + (11.0 / 84.0) * k6[i]);
        f(t + h, y5, yd, k7, ctx);
        for (int i = 0; i < yd; i++)
            y4[i] = y[i] + h * ((5179.0 / 57600.0) * k1[i] + (7571.0 / 16695.0) * k3[i] + (393.0 / 640.0) * k4[i] - (92097.0 / 339200.0) * k5[i] + (187.0 / 2100.0) * k6[i] + (1.0 / 40.0) * k7[i]);
        double maxErr = 0;
        for (int i = 0; i < yd; i++)
        {
            err[i] = y5[i] - y4[i];
            double sc = atol + rtol * fmax(fabs(y[i]), fabs(y5[i]));
            double e = err[i] / sc;
            maxErr += e * e;
        }
        maxErr = sqrt(maxErr / yd);
        if (maxErr <= 1.0)
        {
            t += h;
            for (int i = 0; i < yd; i++) y[i] = y5[i];
            gmb_ode_add_row(&tr, t, y);
        }
        double factor = maxErr > 0 ? fmin(5.0, fmax(0.1, 0.9 * pow(maxErr, -0.2))) : 5.0;
        h *= factor;
        if (fabs(h) < 1e-12) break;
    }

    *out_rows = tr.rows;
    free(y); free(k1); free(k2); free(k3); free(k4); free(k5); free(k6); free(k7);
    free(yk); free(y5); free(y4); free(err);
    return tr.traj;
}

/* Adapter C99 per l'ODE: lambda transpilata
   void fn(void* ctx, double t, double* y, double* out) — riempie out (dydt). */
static void gmb_ode_c99_fn(double t, const double* y, int yd, double* dydt, void* ctx)
{
    gmb_fn_t fn = *(const gmb_fn_t*)ctx;
    /* Bound dell'array y per la lambda (2026-11-03): il C99 passa `double*` senza
     * lunghezza; senza pubblicarlo `y(0)` nella lambda dava "Subscript out of range"
     * (AnalysisMiniTest lambda_param_array_infer). */
    gmb_int_t prev = gmb_lambda_bound(0);
    gmb_lambda_bound_set(0, (gmb_int_t)yd - 1);
    ((void(*)(void*, double, double*, double*))fn.fn)(fn.ctx, t, (double*)y, dydt);
    gmb_lambda_bound_set(0, prev);
}

gmb_int_t gmb_solve_ivp_c99(gmb_fn_t fn, double* y0, int n, double t0, double t1)
{
    if (n <= 0 || n > 64) return 0;
    int rows = 0;
    double* traj = gmb_solve_ivp_rk45(gmb_ode_c99_fn, &fn, y0, n, t0, t1, 1e-6, 1e-9, &rows);
    if (!traj || rows <= 0) { free(traj); return 0; }
    gmb_int_t h = gmb_matrix_from_flat(traj, rows, n + 1);
    free(traj);
    return h;
}
