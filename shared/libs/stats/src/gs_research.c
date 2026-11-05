/* gs_research.c — p-variation (Labadie-Lehalle) e HAR-RV (Corsi). */
#include "gs_research.h"

#include <math.h>
#include <stdlib.h>

gc_real gs_p_variation(const gc_real *x, int n, gc_real p, int scale)
{
    gc_real s = 0.0;
    int i;
    if (!x || n < 2 || p <= 0.0) return 0.0 / 0.0;
    if (scale < 1) scale = 1;
    for (i = 0; i + scale < n; i++) s += pow(fabs(x[i + scale] - x[i]), p);
    return s;
}

gc_real gs_hurst_to_p(gc_real hurst)
{
    return hurst > 0.0 ? 1.0 / hurst : 0.0 / 0.0;
}

/* Gauss 4x4 con pivoting. */
static int solve4(gc_real A[4][4], gc_real b[4], gc_real x[4])
{
    int i, j, k;
    for (i = 0; i < 4; i++) {
        int piv = i; gc_real mx = fabs(A[i][i]);
        for (j = i + 1; j < 4; j++) if (fabs(A[j][i]) > mx) { mx = fabs(A[j][i]); piv = j; }
        if (mx < 1e-300) return -1;
        if (piv != i) {
            for (k = 0; k < 4; k++) { gc_real t = A[i][k]; A[i][k] = A[piv][k]; A[piv][k] = t; }
            { gc_real t = b[i]; b[i] = b[piv]; b[piv] = t; }
        }
        for (j = i + 1; j < 4; j++) {
            gc_real f = A[j][i] / A[i][i];
            for (k = i; k < 4; k++) A[j][k] -= f * A[i][k];
            b[j] -= f * b[i];
        }
    }
    for (i = 3; i >= 0; i--) {
        gc_real s = b[i];
        for (j = i + 1; j < 4; j++) s -= A[i][j] * x[j];
        x[i] = s / A[i][i];
    }
    return 0;
}

gc_status gs_har_fit(const gc_real *rv, int n, gs_har *out)
{
    gc_real A[4][4], b[4], coef[4], mw, mm;
    gc_real sy = 0.0, sy2 = 0.0, my;
    int t, i, j, cnt = 0;
    if (!rv || !out || n < 30) return GC_ERR_ARG;
    for (i = 0; i < 4; i++) { b[i] = 0.0; for (j = 0; j < 4; j++) A[i][j] = 0.0; }
    for (t = 22; t < n; t++) {
        gc_real x[4];
        mw = 0.0; for (i = t - 5; i < t; i++) mw += rv[i]; mw /= 5.0;
        mm = 0.0; for (i = t - 22; i < t; i++) mm += rv[i]; mm /= 22.0;
        x[0] = 1.0; x[1] = rv[t - 1]; x[2] = mw; x[3] = mm;
        for (i = 0; i < 4; i++) { for (j = 0; j < 4; j++) A[i][j] += x[i] * x[j]; b[i] += x[i] * rv[t]; }
        sy += rv[t]; sy2 += rv[t] * rv[t];
        cnt++;
    }
    if (cnt < 8) return GC_ERR_ARG;
    if (solve4(A, b, coef) != 0) return GC_ERR_ARG;
    out->c = coef[0]; out->bd = coef[1]; out->bw = coef[2]; out->bm = coef[3];
    out->n = cnt;
    my = sy / cnt;
    {
        gc_real ssr = 0.0, sst = sy2 - cnt * my * my;
        for (t = 22; t < n; t++) {
            gc_real pred, x[4];
            mw = 0.0; for (i = t - 5; i < t; i++) mw += rv[i]; mw /= 5.0;
            mm = 0.0; for (i = t - 22; i < t; i++) mm += rv[i]; mm /= 22.0;
            x[0] = 1.0; x[1] = rv[t - 1]; x[2] = mw; x[3] = mm;
            pred = coef[0] + coef[1] * x[1] + coef[2] * x[2] + coef[3] * x[3];
            ssr += (rv[t] - pred) * (rv[t] - pred);
        }
        out->r2 = sst > 0.0 ? 1.0 - ssr / sst : 0.0;
    }
    return GC_OK;
}

gc_real gs_har_forecast(const gs_har *m, const gc_real *rv, int n)
{
    gc_real mw = 0.0, mm = 0.0;
    int i;
    if (!m || !rv || n < 23) return 0.0 / 0.0;
    for (i = n - 5; i < n; i++) mw += rv[i];
    mw /= 5.0;
    for (i = n - 22; i < n; i++) mm += rv[i];
    mm /= 22.0;
    return m->c + m->bd * rv[n - 1] + m->bw * mw + m->bm * mm;
}
