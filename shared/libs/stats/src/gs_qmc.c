/*
 * gs_qmc.c — strumenti quasi-Monte Carlo: sequenza di Sobol e Brownian bridge.
 * Riscrittura C99 clean-room da DAL (Derivatives-Algorithms-Lib, #174, MIT).
 * Solo libc/libm.
 */
#include "gs_qmc.h"

#include <math.h>
#include <stdlib.h>

/* ---- Sobol ---- */

/* Polinomi primitivi (bit pattern con bit di grado s) e gradi (s). */
static const int sobol_poly[GS_SOBOL_MAXDIM] = { 1, 3, 7, 11, 13, 19 };
static const int sobol_deg[GS_SOBOL_MAXDIM] = { 0, 1, 2, 3, 3, 4 };

int gs_sobol_init(gs_sobol *s, int maxdim)
{
    int k, j;
    if (!s || maxdim < 1 || maxdim > GS_SOBOL_MAXDIM) return -1;
    s->maxdim = maxdim;
    s->in = 0;
    for (k = 0; k < maxdim; k++) {
        unsigned long m[GS_SOBOL_MAXBIT];
        int deg = sobol_deg[k];
        s->ix[k] = 0;
        if (deg == 0) {
            /* dimensione 1: v_i = 2^-i (tutti m_i = 1) */
            for (j = 0; j < GS_SOBOL_MAXBIT; j++) m[j] = 1;
        } else {
            for (j = 0; j < deg; j++) m[j] = 1;
            for (j = deg; j < GS_SOBOL_MAXBIT; j++) {
                unsigned long x = (m[j - deg] << deg) ^ m[j - deg];
                int l;
                for (l = 1; l <= deg - 1; l++)
                    if ((sobol_poly[k] >> (deg - l)) & 1)
                        x ^= (m[j - l] << l);
                m[j] = x;
            }
        }
        for (j = 0; j < GS_SOBOL_MAXBIT; j++)
            s->dir[j][k] = m[j] << (GS_SOBOL_MAXBIT - 1 - j);
    }
    return 0;
}

void gs_sobol_next(gs_sobol *s, double *out)
{
    unsigned long im;
    int c = 0, k;
    if (!s || !out) return;
    im = (unsigned long)s->in;
    /* posizione del bit zero meno significativo di s->in */
    while ((im & 1) == 1) { im >>= 1; c++; }
    if (c >= GS_SOBOL_MAXBIT) c = GS_SOBOL_MAXBIT - 1;
    for (k = 0; k < s->maxdim; k++) {
        s->ix[k] ^= s->dir[c][k];
        out[k] = (double)s->ix[k] / 1073741824.0;   /* 2^30 */
    }
    s->in++;
}

/* ---- Brownian bridge ---- */

static void bridge_fill(const gc_real *t, int n, const gc_real *z, gc_real *w,
                        int L, int R)
{
    int k, best = -1;
    double target, a, b, tt, var;
    if (R - L < 2) return;
    target = 0.5 * (t[L] + t[R]);
    for (k = L + 1; k < R; k++)
        if (best < 0 || fabs(t[k] - target) < fabs(t[best] - target)) best = k;
    a = t[L];
    b = t[R];
    tt = t[best];
    var = (b - tt) * (tt - a) / (b - a);
    w[best] = w[L] + (w[R] - w[L]) * (tt - a) / (b - a) + sqrt(var) * z[best];
    bridge_fill(t, n, z, w, L, best);
    bridge_fill(t, n, z, w, best, R);
}

void gs_mc_brownian_bridge(const gc_real *t, int n, const gc_real *z, gc_real *w)
{
    if (!t || !z || !w || n <= 0) return;
    w[0] = 0.0;
    w[n] = sqrt(t[n]) * z[0];
    bridge_fill(t, n, z, w, 0, n);
}
/* ---- Tauchen (AR(1) -> catena di Markov), clean-room #176 ---- */

static double gs_norm_cdf(double x) { return 0.5 * erfc(-x * M_SQRT1_2); }

int gs_tauchen(double mu, double rho, double sigma, int n, double m,
               double *A, double *P)
{
    double sd, d, half;
    int i, j;
    if (!A || !P || n < 2 || sigma <= 0.0 || rho >= 1.0 || rho <= -1.0) return -1;
    if (m <= 0.0) m = 3.0;
    sd = sigma / sqrt(1.0 - rho * rho);
    d = 2.0 * m * sd / (n - 1);
    half = 0.5 * d;
    for (i = 0; i < n; i++) A[i] = mu + ((double)i - 0.5 * (n - 1)) * d;
    for (i = 0; i < n; i++) {
        double cmean = mu * (1.0 - rho) + rho * A[i];
        for (j = 0; j < n; j++) {
            double lo = (A[j] - half - cmean) / sigma;
            double hi = (A[j] + half - cmean) / sigma;
            if (j == 0) P[i * n + j] = gs_norm_cdf(hi);
            else if (j == n - 1) P[i * n + j] = 1.0 - gs_norm_cdf(lo);
            else P[i * n + j] = gs_norm_cdf(hi) - gs_norm_cdf(lo);
        }
    }
    return 0;
}
