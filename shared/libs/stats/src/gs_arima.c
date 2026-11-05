/*
 * gs_arima.c — ARIMA(p,d,q) + differenze/integrazione, Box-Cox, diagnostica
 * (Ljung-Box, Box-Pierce, ARCH-LM), ndiffs/nsdiffs, auto-ARIMA, bande.
 * Adattato da KoplaNum/ARIMA (lite-C, MIT) all'API cronologica di gmbsuite.
 */
#include "gs_arima.h"
#include "gs_armagarch.h"
#include "gs_dist.h"
#include "gs_infer.h"
#include "gs_rolling.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- differenze / integrazione ---------------- */

int gs_diff(const double *x, int n, int lag, double *out)
{
    int i;
    if (!x || !out || n < 1 || lag < 1 || lag >= n) return -1;
    for (i = 0; i + lag < n; i++) out[i] = x[i + lag] - x[i];
    return n - lag;
}

int gs_integrate(const double *dx, int n, int lag, const double *init, double *out)
{
    int i;
    if (!dx || !out || !init || n < 0 || lag < 1) return -1;
    for (i = 0; i < lag; i++) out[i] = init[i];
    for (i = 0; i < n; i++) out[lag + i] = out[i] + dx[i];
    return n + lag;
}

int gs_seasonal_diff(const double *x, int n, int period, int order, double *out)
{
    int len = n, i;
    if (!x || !out || n < 1 || period < 1 || order < 0) return -1;
    if (out != x) memcpy(out, x, (size_t)n * sizeof(double));
    for (i = 0; i < order; i++) {
        len = gs_diff(out, len, period, out);
        if (len < 0) return -1;
    }
    return len;
}

/* ---------------- Box-Cox ---------------- */

int gs_boxcox(const double *x, int n, double lambda, double *out)
{
    int i;
    if (!x || !out || n < 1) return -1;
    for (i = 0; i < n; i++) {
        if (x[i] <= 0.0) return -1;
        out[i] = (fabs(lambda) < 1e-9) ? log(x[i]) : (pow(x[i], lambda) - 1.0) / lambda;
    }
    return 0;
}

int gs_boxcox_inv(const double *y, int n, double lambda, double *out)
{
    int i;
    if (!y || !out || n < 1) return -1;
    for (i = 0; i < n; i++)
        out[i] = (fabs(lambda) < 1e-9) ? exp(y[i]) : pow(lambda * y[i] + 1.0, 1.0 / lambda);
    return 0;
}

double gs_boxcox_lambda(const double *x, int n)
{
    double best = -1e300, bl = 0.0, l, sumlog = 0.0, *y;
    int i;
    if (!x || n < 5) return 0.0;
    for (i = 0; i < n; i++) { if (x[i] <= 0.0) return 0.0; sumlog += log(x[i]); }
    y = (double *)malloc((size_t)n * sizeof(double));
    if (!y) return 0.0;
    for (l = -2.0; l <= 2.0001; l += 0.05) {
        double mu = 0.0, var = 0.0, ll;
        for (i = 0; i < n; i++)
            y[i] = (fabs(l) < 1e-9) ? log(x[i]) : (pow(x[i], l) - 1.0) / l;
        for (i = 0; i < n; i++) mu += y[i];
        mu /= (double)n;
        for (i = 0; i < n; i++) var += (y[i] - mu) * (y[i] - mu);
        var /= (double)n;
        if (var <= 0.0) continue;
        ll = -0.5 * (double)n * log(var) + (l - 1.0) * sumlog;
        if (ll > best) { best = ll; bl = l; }
    }
    free(y);
    return bl;
}

/* ---------------- ARIMA ---------------- */

int gs_arima_fit(const double *x, int n, int p, int d, int q, gs_arima *out)
{
    double *w;
    gs_armagarch ag;
    int len = n, i;
    if (!x || !out || n < 10 || p < 0 || p > GS_ARIMA_MAXP || q < 0 || q > GS_ARIMA_MAXQ ||
        d < 0 || d > GS_ARIMA_MAXD) return GC_ERR_ARG;
    w = (double *)malloc((size_t)n * sizeof(double));
    if (!w) return GC_ERR_NO_MEM;
    memcpy(w, x, (size_t)n * sizeof(double));
    for (i = 0; i < d; i++) len = gs_diff(w, len, 1, w);
    memset(&ag, 0, sizeof(ag));
    if (gs_armagarch_fit(w, len, p, q, 0, &ag) != GC_OK) { free(w); return GC_ERR_ARG; }
    memset(out, 0, sizeof(*out));
    out->p = p; out->d = d; out->q = q; out->n = n;
    out->c = ag.c; out->sigma2 = ag.sigma2;
    for (i = 0; i < p; i++) out->phi[i] = ag.phi[i];
    for (i = 0; i < q; i++) out->theta[i] = ag.theta[i];
    out->loglik = ag.loglik; out->aic = ag.aic;
    free(w);
    return GC_OK;
}

/* Previsioni sul processo differenziato (d volte) e poi integrazione. */
static int arima_forecast_core(const gs_arima *m, const double *x, int n, int h, double *out)
{
    double *w, *e, *dvals;
    int len = n, i, k, start, off;
    if (!m || !x || !out || n < m->p + m->q + 5 || h < 1 || n < m->d + 3) return GC_ERR_ARG;
    w = (double *)malloc((size_t)(n + h) * sizeof(double));
    e = (double *)calloc((size_t)(n + h), sizeof(double));
    dvals = (double *)malloc((size_t)(m->d + 1) * sizeof(double));
    if (!w || !e || !dvals) { free(w); free(e); free(dvals); return GC_ERR_NO_MEM; }
    memcpy(w, x, (size_t)n * sizeof(double));
    for (i = 0; i < m->d; i++) len = gs_diff(w, len, 1, w);
    off = n - len;
    start = m->p > m->q ? m->p : m->q;
    if (start < 0) start = 0;
    for (k = start; k < len; k++) {
        double pred = m->c;
        int t = off + k;
        for (i = 1; i <= m->p; i++) pred += m->phi[i - 1] * w[t - i];
        for (i = 1; i <= m->q; i++) pred += m->theta[i - 1] * e[t - i];
        e[t] = w[t] - pred;
    }
    for (k = 0; k < h; k++) {
        double pred = m->c;
        int t = off + len + k;
        for (i = 1; i <= m->p; i++) pred += m->phi[i - 1] * w[t - i];
        for (i = 1; i <= m->q; i++) pred += m->theta[i - 1] * e[t - i];
        w[t] = pred; e[t] = 0.0;
    }
    /* ultime differenze (ordine 0..d) all'ultimo punto osservato */
    dvals[0] = x[n - 1];
    if (m->d >= 1) dvals[1] = x[n - 1] - x[n - 2];
    if (m->d >= 2) dvals[2] = x[n - 1] - 2.0 * x[n - 2] + x[n - 3];
    for (k = 0; k < h; k++) {
        double cur = w[off + len + k];
        for (i = m->d - 1; i >= 0; i--) { cur = dvals[i] + cur; dvals[i] = cur; }
        out[k] = cur;
    }
    free(w); free(e); free(dvals);
    return GC_OK;
}

int gs_arima_forecast(const gs_arima *m, const double *x, int n, int h, double *out)
{
    return arima_forecast_core(m, x, n, h, out);
}

/* Pesi psi (MA infinito) del modello ARMA, integrati d volte. */
static void arima_psi(const gs_arima *m, int h, double *psi)
{
    int k, i;
    psi[0] = 1.0;
    for (k = 1; k < h; k++) {
        double s = 0.0;
        for (i = 1; i <= m->p && i <= k; i++) s += m->phi[i - 1] * psi[k - i];
        if (k <= m->q) s += m->theta[k - 1];
        psi[k] = s;
    }
    for (i = 0; i < m->d; i++) {
        double s = 0.0;
        for (k = 0; k < h; k++) { s += psi[k]; psi[k] = s; }
    }
}

int gs_arima_forecast_bands(const gs_arima *m, const double *x, int n, int h, double alpha,
                            double *mean, double *lo, double *hi)
{
    double *psi, cum = 0.0, z;
    int k, r;
    if (!m || !mean || !lo || !hi || h < 1 || alpha <= 0.0 || alpha >= 1.0) return GC_ERR_ARG;
    r = gs_arima_forecast(m, x, n, h, mean);
    if (r != GC_OK) return r;
    psi = (double *)malloc((size_t)h * sizeof(double));
    if (!psi) return GC_ERR_NO_MEM;
    arima_psi(m, h, psi);
    z = gs_dist_normal_ppf(0.0, 1.0, 1.0 - alpha / 2.0);
    for (k = 0; k < h; k++) {
        double sd;
        cum += psi[k] * psi[k];
        sd = sqrt(m->sigma2 * cum);
        lo[k] = mean[k] - z * sd;
        hi[k] = mean[k] + z * sd;
    }
    free(psi);
    return GC_OK;
}

int gs_auto_arima(const double *x, int n, int pmax, int dmax, int qmax, gs_arima *out)
{
    gs_arima m, best;
    double best_aic = 1e300;
    int p, q, d, found = 0;
    if (!x || !out || n < 20 || pmax < 0 || qmax < 0 || dmax < 0) return GC_ERR_ARG;
    if (pmax > GS_ARIMA_MAXP) pmax = GS_ARIMA_MAXP;
    if (qmax > GS_ARIMA_MAXQ) qmax = GS_ARIMA_MAXQ;
    if (dmax > GS_ARIMA_MAXD) dmax = GS_ARIMA_MAXD;
    d = gs_ndiffs(x, n, dmax);
    if (d < 0) d = 0;
    for (p = 0; p <= pmax; p++)
        for (q = 0; q <= qmax; q++) {
            if (p == 0 && q == 0) continue;
            if (gs_arima_fit(x, n, p, d, q, &m) == GC_OK && m.aic < best_aic) {
                best_aic = m.aic; best = m; found = 1;
            }
        }
    if (!found) return GC_ERR_ARG;
    *out = best;
    return GC_OK;
}

/* ---------------- diagnostica ---------------- */

static double lb_core(const double *x, int n, int h, int corrected, double *out_p)
{
    double mu = 0.0, denom = 0.0, q = 0.0;
    int k, t;
    if (!x || n < 3 || h < 1) return -1.0;
    if (h > n - 1) h = n - 1;
    for (t = 0; t < n; t++) mu += x[t];
    mu /= (double)n;
    for (t = 0; t < n; t++) denom += (x[t] - mu) * (x[t] - mu);
    if (denom <= 0.0) return -1.0;
    for (k = 1; k <= h; k++) {
        double num = 0.0, rho;
        for (t = k; t < n; t++) num += (x[t] - mu) * (x[t - k] - mu);
        rho = num / denom;
        q += corrected ? rho * rho / (double)(n - k) : rho * rho;
    }
    q *= corrected ? (double)n * (double)(n + 2) : (double)n;
    if (out_p) *out_p = gs_dist_chi2_sf((double)h, q);
    return q;
}

double gs_ljung_box(const double *x, int n, int h, double *out_p) { return lb_core(x, n, h, 1, out_p); }
double gs_box_pierce(const double *x, int n, int h, double *out_p) { return lb_core(x, n, h, 0, out_p); }

double gs_arch_lm(const double *x, int n, int lags, double *out_p)
{
    double *e2, *A, *b, *sol, mu = 0.0, tss = 0.0, rss = 0.0, ybar = 0.0, stat;
    int t, i, j, m;
    if (!x || n < lags + 5 || lags < 1) return -1.0;
    m = n - lags;
    for (t = 0; t < n; t++) mu += x[t];
    mu /= (double)n;
    e2 = (double *)malloc((size_t)n * sizeof(double));
    A = (double *)calloc((size_t)(lags + 1) * (size_t)(lags + 1), sizeof(double));
    b = (double *)calloc((size_t)(lags + 1), sizeof(double));
    sol = (double *)malloc((size_t)(lags + 1) * sizeof(double));
    if (!e2 || !A || !b || !sol) { free(e2); free(A); free(b); free(sol); return -1.0; }
    for (t = 0; t < n; t++) { double e = x[t] - mu; e2[t] = e * e; }
    for (t = lags; t < n; t++) {
        double y = e2[t];
        A[0] += 1.0; b[0] += y;
        for (i = 1; i <= lags; i++) { A[i] += e2[t - i]; A[i * (lags + 1)] += e2[t - i]; b[i] += e2[t - i] * y; }
        for (i = 1; i <= lags; i++)
            for (j = 1; j <= lags; j++) A[i * (lags + 1) + j] += e2[t - i] * e2[t - j];
    }
    for (t = lags; t < n; t++) ybar += e2[t];
    ybar /= (double)m;
    for (t = lags; t < n; t++) { double d = e2[t] - ybar; tss += d * d; }
    if (gm_linalg_solve(lags + 1, A, b, sol) != 0 || tss <= 0.0) {
        free(e2); free(A); free(b); free(sol); return -1.0;
    }
    for (t = lags; t < n; t++) {
        double pred = sol[0], r;
        for (i = 1; i <= lags; i++) pred += sol[i] * e2[t - i];
        r = e2[t] - pred;
        rss += r * r;
    }
    stat = (double)m * (1.0 - rss / tss);
    if (out_p) *out_p = gs_dist_chi2_sf((double)lags, stat);
    free(e2); free(A); free(b); free(sol);
    return stat;
}

/* ---------------- ordini di differenziazione ---------------- */

int gs_ndiffs(const double *x, int n, int max_d)
{
    double *w;
    int d, len;
    if (!x || n < 20 || max_d < 0) return -1;
    w = (double *)malloc((size_t)n * sizeof(double));
    if (!w) return -1;
    memcpy(w, x, (size_t)n * sizeof(double));
    len = n;
    for (d = 0; d <= max_d; d++) {
        gs_adf_result r;
        int lags = gs_adf_max_lags(len);
        if (lags < 0) lags = 0;
        if (gs_adf(len, w, lags, GS_ADF_CONSTANT, &r) == 1 && r.statistic < r.critical[1]) break;
        if (d < max_d) len = gs_diff(w, len, 1, w);
    }
    free(w);
    return d > max_d ? max_d : d;
}

int gs_nsdiffs(const double *x, int n, int period, int max_d)
{
    double *tr, *se, *re, s;
    int r;
    if (!x || n < 2 * period || period < 2 || max_d < 0) return -1;
    tr = (double *)malloc((size_t)n * sizeof(double));
    se = (double *)malloc((size_t)n * sizeof(double));
    re = (double *)malloc((size_t)n * sizeof(double));
    if (!tr || !se || !re) { free(tr); free(se); free(re); return -1; }
    r = gs_seasonal_decompose(x, n, period, 0, tr, se, re);
    s = (r == 0) ? gs_seasonal_strength(se, re, n) : 0.0;
    free(tr); free(se); free(re);
    return (s > 0.64 && max_d >= 1) ? 1 : 0;
}

/* ---------------- winsorize ---------------- */

static int cmp_dbl(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int gs_winsorize(const double *x, int n, double pct, double *out)
{
    double *s, lo, hi, pos;
    int i, i0, i1;
    if (!x || !out || n < 3 || pct <= 0.0 || pct >= 0.5) return GC_ERR_ARG;
    s = (double *)malloc((size_t)n * sizeof(double));
    if (!s) return GC_ERR_NO_MEM;
    memcpy(s, x, (size_t)n * sizeof(double));
    qsort(s, (size_t)n, sizeof(double), cmp_dbl);
    pos = pct * (double)(n - 1);
    i0 = (int)floor(pos); i1 = i0 + 1 < n ? i0 + 1 : i0;
    lo = s[i0] + (pos - (double)i0) * (s[i1] - s[i0]);
    pos = (1.0 - pct) * (double)(n - 1);
    i0 = (int)floor(pos); i1 = i0 + 1 < n ? i0 + 1 : i0;
    hi = s[i0] + (pos - (double)i0) * (s[i1] - s[i0]);
    for (i = 0; i < n; i++) out[i] = x[i] < lo ? lo : (x[i] > hi ? hi : x[i]);
    free(s);
    return GC_OK;
}
