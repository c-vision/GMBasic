/*
 * ofx_ts2.c — shim FFI per la famiglia avanzata di TIMESERIES (gmbstats):
 * Hurst, frazionale, Granger, GARCH(1,1), Kalman 1D, stationary bootstrap,
 * entropie (SampEn/ApEn/PermEn/LZ), Lyapunov, OU fit, pesi BMA.
 * Uscite scalari o in out() piatto allocato dal wrapper BASIC.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gs_stats.h"
#include "gs_stats2.h"
#include "gs_kalman.h"

#define TS2_NAN (0.0 / 0.0)

double ofx_ts_hurst(const double *x, int n)
{
    if (!x || n < 8) return TS2_NAN;
    return gs_hurst_dfa(x, n, 4);
}

int ofx_ts_fracdiff(const double *x, int n, double d, double *out)
{
    if (!x || !out || n < 1) return 0;
    return gs_frac_diff(x, n, d, out) == 0 ? 1 : 0;
}

/* Granger: out(3) = statistic, pvalue, degrees_of_freedom. */
int ofx_ts_granger(const double *x, const double *y, int n, int p, double *out)
{
    double f = 0.0, pv = 0.0;
    if (!x || !y || !out || p < 1) return 0;
    if (gs_granger(x, y, n, p, &f, &pv) != 0) return 0;
    out[0] = f;
    out[1] = pv;
    out[2] = (double)p;
    return 1;
}

/* GARCH(1,1): out(3) = omega, alpha, beta — port 1:1 di Numerics.Garch11
 * (discesa del gradiente a passo adattivo). Deterministico fra compilatori:
 * l'ottimizzatore Nelder-Mead di gs_armagarch dava risultati diversi fra
 * C99/VM-C per rumore in virgola mobile. */
static double garch_nll(const double *r, int n, const double *p)
{
    double w = p[0], al = p[1], be = p[2];
    double s2 = r[0] * r[0], ll = 0.0;
    int t;
    if (s2 <= 0.0) s2 = 1e-6;
    for (t = 1; t < n; t++) {
        s2 = w + al * r[t - 1] * r[t - 1] + be * s2;
        if (s2 < 1e-12) s2 = 1e-12;
        ll += log(s2) + r[t] * r[t] / s2;
    }
    return 0.5 * ll;
}

int ofx_ts_garch11(const double *x, int n, double *out)
{
    double p[3], v = 0.0;
    const double h = 1e-5;
    double step = 0.05;
    int it, i;
    if (!x || !out || n < 5) return 0;
    for (i = 0; i < n; i++) v += x[i] * x[i];
    v /= (double)n;
    p[0] = 0.1 * v;
    p[1] = 0.1;
    p[2] = 0.8;
    for (it = 0; it < 2000; it++) {
        double f0 = garch_nll(x, n, p);
        double grad[3], gn = 0.0, np[3];
        for (i = 0; i < 3; i++) {
            double save = p[i];
            p[i] = save + h;
            grad[i] = (garch_nll(x, n, p) - f0) / h;
            p[i] = save;
            gn += grad[i] * grad[i];
        }
        gn = sqrt(gn);
        if (gn < 1e-10) break;
        for (i = 0; i < 3; i++) np[i] = p[i] - step * grad[i] / gn;
        if (np[0] < 1e-8) np[0] = 1e-8;
        if (np[1] < 0.0) np[1] = 0.0;
        if (np[2] < 0.0) np[2] = 0.0;
        if (np[1] + np[2] > 0.999) {
            double ss = 0.999 / (np[1] + np[2]);
            np[1] *= ss;
            np[2] *= ss;
        }
        if (garch_nll(x, n, np) < f0) {
            p[0] = np[0]; p[1] = np[1]; p[2] = np[2];
            step *= 1.05;
        } else {
            step *= 0.5;
        }
        if (step < 1e-12) break;
    }
    out[0] = p[0];
    out[1] = p[1];
    out[2] = p[2];
    return 1;
}

/* Kalman 1D: out(n) = stato filtrato. F=a, H=h, Q=q, R=r, x0=z[0], P0=1. */
int ofx_ts_kalman1d(const double *z, int n, double a, double h, double q, double r,
                    double *out)
{
    double F, H, Q, R, x0, P0;
    double *ps;
    int ok;
    if (!z || !out || n < 1) return 0;
    ps = (double *)calloc((size_t)n, sizeof(double));
    if (!ps) return 0;
    F = a; H = h; Q = q; R = r;
    x0 = z[0];
    P0 = 1.0;
    ok = gs_kalman_batch(z, n, 1, 1, &F, &H, &Q, &R, &x0, &P0, out, ps) == GC_OK;
    free(ps);
    return ok;
}

int ofx_ts_bootstrap(const double *x, int n, int window, int seed, double *out)
{
    if (!x || !out || n < 1 || window < 1) return 0;
    return gs_stationary_bootstrap(x, n, window, (unsigned)seed, out) == 0 ? 1 : 0;
}

double ofx_ts_sampen(const double *x, int n, int m, double r)
{
    if (!x || n < m + 1) return TS2_NAN;
    return gs_entropy_sample(x, n, m, r);
}

double ofx_ts_approxent(const double *x, int n, int m, double r)
{
    if (!x || n < m + 1) return TS2_NAN;
    return gs_entropy_approx(x, n, m, r);
}

double ofx_ts_perm(const double *x, int n, int m, int lag)
{
    if (!x || n < m + 1) return TS2_NAN;
    return gs_entropy_perm(x, n, m, lag);
}

double ofx_ts_lz(const double *x, int n)
{
    if (!x || n < 2) return 0.0;
    return gs_entropy_lz(x, n);
}

double ofx_ts_lyap(const double *x, int n, int m, int tau)
{
    if (!x || n < 4) return TS2_NAN;
    return gs_lyapunov(x, n, m, tau);
}

/* OU fit: out(5) = theta, mu, sigma, phi, half_life. */
int ofx_ts_oufit(const double *x, int n, double dt, double *out)
{
    if (!x || !out || n < 4) return 0;
    return gs_ou_fit(x, n, dt, &out[0], &out[1], &out[2], &out[3], &out[4]) == 0 ? 1 : 0;
}

/* BMA: out(m) = pesi posteriori da IC. */
int ofx_ts_bma(const double *ic, int m, double *out)
{
    if (!ic || !out || m < 1) return 0;
    return gs_bma_weights(ic, m, out) == 0 ? 1 : 0;
}
