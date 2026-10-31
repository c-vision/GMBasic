/* ============================================================================
 * gmb_timeseries.c — builtin TIMESERIES: array allocati (free() al chiamante).
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* Errore dell'ultima funzione TIMESERIES (parità dei throw ArgumentException C#). "" = ok.
 * Il chiamante azzera prima (gmb_ts_clear_error) e controlla dopo (gmb_ts_last_error). */
static char g_ts_err[160];
const char* gmb_ts_last_error(void) { return g_ts_err; }
void gmb_ts_clear_error(void) { g_ts_err[0] = '\0'; }
static void ts_err(const char* m) { snprintf(g_ts_err, sizeof(g_ts_err), "%s", m ? m : ""); }

gmb_double_t* gmb_ts_rollautocorr(gmb_double_t* series, gmb_int_t n, gmb_int_t period, gmb_int_t lag, gmb_int_t* out_len) {
    if (!series || n < period || period < lag + 1 || lag < 1) { ts_err("Dati, finestra o lag non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - period + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)(len > 0 ? len : 0), sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++) {
        double mean = 0.0;
        for (gmb_int_t k = 0; k < period; k++) mean += series[i + k];
        mean /= (double)period;
        double num = 0.0, den = 0.0;
        for (gmb_int_t j = lag; j < period; j++)
            num += (series[i + j] - mean) * (series[i + j - lag] - mean);
        for (gmb_int_t j = 0; j < period; j++)
            den += (series[i + j] - mean) * (series[i + j] - mean);
        out[i] = (den != 0.0) ? num / den : 0.0;
    }
    return out;
}

gmb_double_t* gmb_ts_rolldelta(gmb_double_t* series, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len) {
    if (!series || period < 1) { ts_err("Dati, finestra o ordine non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - period + 1;
    if (len < 0) len = 0;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)(len > 0 ? len : 0), sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++)
        out[i] = series[i + period - 1] - series[i];
    return out;
}

gmb_double_t* gmb_ts_moving_average(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || window < 1 || n < window) { ts_err("La finestra deve essere almeno 1."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    double sum = 0.0;
    for (gmb_int_t i = 0; i < window; i++) sum += data[i];
    out[0] = sum / window;
    for (gmb_int_t i = window; i < n; i++) {
        sum += data[i] - data[i - window];
        out[i - window + 1] = sum / window;
    }
    return out;
}

gmb_double_t* gmb_ts_ema(gmb_double_t* data, gmb_int_t n, double alpha, gmb_int_t* out_len) {
    if (!data || n <= 0 || alpha <= 0 || alpha > 1) { ts_err("Alpha deve essere in (0,1]."); *out_len = 0; return NULL; }
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)n, sizeof(gmb_double_t));
    *out_len = n;
    out[0] = data[0];
    for (gmb_int_t i = 1; i < n; i++)
        out[i] = alpha * data[i] + (1 - alpha) * out[i - 1];
    return out;
}

gmb_double_t* gmb_ts_returns(gmb_double_t* data, gmb_int_t n, int log_returns, gmb_int_t* out_len) {
    if (!data || n < 2) { *out_len = 0; return NULL; }
    gmb_int_t len = n - 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 1; i < n; i++) {
        if (log_returns) out[i - 1] = log(data[i] / data[i - 1]);
        else out[i - 1] = (data[i] - data[i - 1]) / data[i - 1];
    }
    return out;
}

double gmb_ts_volatility(gmb_double_t* data, gmb_int_t n, int log_returns) {
    gmb_int_t rlen = 0;
    gmb_double_t* rets = gmb_ts_returns(data, n, log_returns, &rlen);
    if (!rets || rlen == 0) { free(rets); return 0.0; }
    double mean = 0.0;
    for (gmb_int_t i = 0; i < rlen; i++) mean += rets[i];
    mean /= rlen;
    double var = 0.0;
    for (gmb_int_t i = 0; i < rlen; i++) var += (rets[i] - mean) * (rets[i] - mean);
    free(rets);
    return sqrt(var / rlen);
}

double gmb_ts_autocorrelation(gmb_double_t* data, gmb_int_t n, gmb_int_t lag) {
    if (!data || lag < 1 || lag >= n) { ts_err("Lag non valido."); return 0.0; }
    double mean = 0.0;
    for (gmb_int_t i = 0; i < n; i++) mean += data[i];
    mean /= n;
    double num = 0.0, den = 0.0;
    for (gmb_int_t i = lag; i < n; i++)
        num += (data[i] - mean) * (data[i - lag] - mean);
    for (gmb_int_t i = 0; i < n; i++)
        den += (data[i] - mean) * (data[i] - mean);
    return den != 0.0 ? num / den : 0.0;
}

gmb_double_t* gmb_ts_difference(gmb_double_t* data, gmb_int_t n, gmb_int_t order, gmb_int_t* out_len) {
    if (!data || n <= 1 || order < 1) { ts_err("L'ordine deve essere >= 1."); *out_len = 0; return NULL; }
    gmb_double_t* cur = (gmb_double_t*)malloc(sizeof(double) * (size_t)n);
    if (!cur) { *out_len = 0; return NULL; }
    memcpy(cur, data, sizeof(double) * (size_t)n);
    gmb_int_t clen = n;
    for (gmb_int_t o = 0; o < order && clen > 1; o++) {
        gmb_int_t newlen = clen - 1;
        for (gmb_int_t i = 0; i < newlen; i++) cur[i] = cur[i + 1] - cur[i];
        clen = newlen;
    }
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)clen, sizeof(gmb_double_t));
    if (out) memcpy(out, cur, sizeof(double) * (size_t)clen);
    free(cur);
    *out_len = clen;
    return out;
}

gmb_double_t* gmb_ts_deseasonalize(gmb_double_t* data, gmb_int_t n, gmb_int_t season, gmb_int_t* out_len) {
    if (!data || season < 2 || season > n / 2) { ts_err("Periodo non valido per la destagionalizzazione."); *out_len = 0; return NULL; }
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)n, sizeof(gmb_double_t));
    *out_len = n;
    /* Media mobile CENTRATA di ampiezza (season/2) che INCLUDE il punto i, con estremi
     * clampati — parity Numerics.TimeSeries.Deseasonalize (oracolo IL): left=i-half,
     * right=i+half, media su [left,right]. Prima si escludeva il centro e si divideva per i
     * soli vicini, dando 1 invece di 0.5 sull'ultimo punto della serie 1..10 con season=2. */
    gmb_int_t half = season / 2;
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_int_t left = (i - half > 0) ? i - half : 0;
        gmb_int_t right = (i + half < n - 1) ? i + half : n - 1;
        double sum = 0.0;
        for (gmb_int_t k = left; k <= right; k++) sum += data[k];
        out[i] = data[i] - sum / (double)(right - left + 1);
    }
    return out;
}

gmb_double_t* gmb_ts_rolling_mean(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || window < 1 || n < window) { ts_err("Finestra non valida."); *out_len = 0; return NULL; }
    return gmb_ts_moving_average(data, n, window, out_len);
}

gmb_double_t* gmb_ts_rolling_stddev(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || window < 1 || n < window) { ts_err("Finestra non valida."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++) {
        double mean = 0.0;
        for (gmb_int_t j = 0; j < window; j++) mean += data[i + j];
        mean /= window;
        double var = 0.0;
        for (gmb_int_t j = 0; j < window; j++) var += (data[i + j] - mean) * (data[i + j] - mean);
        out[i] = sqrt(var / window);
    }
    return out;
}

gmb_double_t* gmb_ts_rolling_zscore(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || window < 2 || n < window) { ts_err("Dati o finestra non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++) {
        double mean = 0.0;
        for (gmb_int_t j = 0; j < window; j++) mean += data[i + j];
        mean /= window;
        double var = 0.0;
        for (gmb_int_t j = 0; j < window; j++) var += (data[i + j] - mean) * (data[i + j] - mean);
        double std = sqrt(var / window);
        out[i] = std < 1e-12 ? 0.0 : (data[i + window - 1] - mean) / std;
    }
    return out;
}

gmb_double_t* gmb_ts_rolling_cov(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!x || !y || window < 2 || n < window) { ts_err("Dati o finestra non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++) {
        double mx = 0.0, my = 0.0;
        for (gmb_int_t j = 0; j < window; j++) { mx += x[i + j]; my += y[i + j]; }
        mx /= window; my /= window;
        double cov = 0.0;
        for (gmb_int_t j = 0; j < window; j++) cov += (x[i + j] - mx) * (y[i + j] - my);
        out[i] = cov / (double)(window - 1);   /* covarianza CAMPIONARIA: parity RollingCovariance (oracolo IL) */
    }
    return out;
}

gmb_double_t* gmb_ts_rolling_corr(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!x || !y || window < 2 || n < window) { ts_err("Finestra non valida."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++) {
        double mx = 0.0, my = 0.0;
        for (gmb_int_t j = 0; j < window; j++) { mx += x[i + j]; my += y[i + j]; }
        mx /= window; my /= window;
        double cov = 0.0, vx = 0.0, vy = 0.0;
        for (gmb_int_t j = 0; j < window; j++) {
            double dx = x[i + j] - mx, dy = y[i + j] - my;
            cov += dx * dy; vx += dx * dx; vy += dy * dy;
        }
        out[i] = (vx > 0.0 && vy > 0.0) ? cov / (sqrt(vx) * sqrt(vy)) : 0.0;
    }
    return out;
}

gmb_double_t* gmb_ts_outliers(gmb_double_t* data, gmb_int_t n, double threshold, gmb_int_t* out_len) {
    *out_len = 0;
    if (!data || n < 3) return NULL;
    double mean = 0.0;
    for (gmb_int_t i = 0; i < n; i++) mean += data[i];
    mean /= n;
    double var = 0.0;
    for (gmb_int_t i = 0; i < n; i++) var += (data[i] - mean) * (data[i] - mean);
    double std = sqrt(var / n);
    if (std <= 0) return NULL;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)n, sizeof(gmb_double_t));
    if (!out) return NULL;
    gmb_int_t cnt = 0;
    for (gmb_int_t i = 0; i < n; i++) {
        double z = fabs((data[i] - mean) / std);
        if (z > threshold) out[cnt++] = (double)i;
    }
    *out_len = cnt;
    return out;
}

/* Mediana di v[0..n) (v viene ordinato in place); n pari → media dei due centrali (parità Numerics.RollingMad). */
static double ts_median_inplace(double* v, gmb_int_t n) {
    for (gmb_int_t a = 1; a < n; a++) {                     /* insertion sort: finestre piccole, nessuna dipendenza */
        double t = v[a]; gmb_int_t b = a - 1;
        while (b >= 0 && v[b] > t) { v[b + 1] = v[b]; b--; }
        v[b + 1] = t;
    }
    gmb_int_t mid = n / 2;
    return (n % 2 == 0) ? (v[mid - 1] + v[mid]) / 2.0 : v[mid];
}

/* ROLLINGMAD: MAD mobile = mediana delle deviazioni assolute dalla mediana della finestra
 * (stessa definizione di common/CommonLib/Numerics/TimeSeries.cs RollingMad; finestra > n = errore). */
gmb_double_t* gmb_ts_rolling_mad(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || n <= 0 || window < 1 || n < window) { ts_err("Dati o finestra non validi."); *out_len = 0; return NULL; }
    gmb_int_t olen = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)malloc(olen * sizeof(gmb_double_t));
    gmb_double_t* win = (gmb_double_t*)malloc(window * sizeof(gmb_double_t));
    if (!out || !win) { free(out); free(win); *out_len = 0; return NULL; }
    for (gmb_int_t i = 0; i < olen; i++) {
        for (gmb_int_t j = 0; j < window; j++) win[j] = data[i + j];
        double med = ts_median_inplace(win, window);
        for (gmb_int_t j = 0; j < window; j++) win[j] = fabs(win[j] - med);
        out[i] = ts_median_inplace(win, window);
    }
    free(win);
    *out_len = olen;
    return out;
}

/* ROLLINGAUTOCORRELATION: autocorrelazione al lag in finestra mobile. */
gmb_double_t* gmb_ts_rolling_autocorr(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t lag, gmb_int_t* out_len) {
    if (n <= 0 || window <= 0 || lag <= 0) { ts_err("Dati, finestra o lag non validi."); *out_len = 0; return NULL; }
    if (window > n) window = n;
    gmb_int_t olen = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)malloc(olen * sizeof(gmb_double_t));
    if (!out) { *out_len = 0; return NULL; }
    for (gmb_int_t i = 0; i < olen; i++) {
        if (window <= lag) { out[i] = 0; continue; }
        double* w = data + i;
        double m = 0;
        for (gmb_int_t j = 0; j < window; j++) m += w[j];
        m /= window;
        double num = 0, den = 0;
        for (gmb_int_t j = 0; j < window - lag; j++) num += (w[j]-m)*(w[j+lag]-m);
        for (gmb_int_t j = 0; j < window; j++) den += (w[j]-m)*(w[j]-m);
        out[i] = den != 0 ? num / den : 0;
    }
    *out_len = olen;
    return out;
}

/* ROLLINGREGRESSION: pendenza di regressione lineare in finestra mobile (una per finestra). */
/* Regressione mobile: per ogni finestra restituisce una COPPIA [beta, alpha] (slope,
   intercept) — stesso contratto di ILBackend/CommonLib (double[finestre][2]). Layout piatto:
   out[2i]=beta, out[2i+1]=alpha, out_len = 2*(n-window+1). */
gmb_double_t* gmb_ts_rolling_regression(gmb_double_t* x, gmb_double_t* y, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (n <= 0 || window <= 0) { *out_len = 0; return NULL; }
    if (window > n) window = n;
    gmb_int_t olen = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)(2 * olen) * sizeof(gmb_double_t));
    if (!out) { *out_len = 0; return NULL; }
    for (gmb_int_t i = 0; i < olen; i++) {
        double sx = 0, sy = 0, sxy = 0, sx2 = 0;
        for (gmb_int_t j = 0; j < window; j++) {
            double xv = x[i+j], yv = y[i+j];
            sx += xv; sy += yv; sxy += xv*yv; sx2 += xv*xv;
        }
        double den = window*sx2 - sx*sx;
        double beta = den != 0 ? (window*sxy - sx*sy) / den : 0;
        double alpha = window != 0 ? (sy - beta*sx) / window : 0;
        out[2*i] = beta;
        out[2*i+1] = alpha;
    }
    *out_len = 2 * olen;
    return out;
}

/* DECOMPOSE: trend (media mobile) e stagionalita' (differenza dal trend per periodo). */
gmb_double_t* gmb_ts_decompose_trend(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len) {
    if (n <= 0) { *out_len = 0; return NULL; }
    if (period <= 0) period = 1;
    if (period > n) period = n;
    gmb_double_t* out = (gmb_double_t*)malloc(n * sizeof(gmb_double_t));
    if (!out) { *out_len = 0; return NULL; }
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_int_t start = i - period/2;
        gmb_int_t cnt = 0;
        double sum = 0;
        for (gmb_int_t j = start; j < start + period; j++)
            if (j >= 0 && j < n) { sum += data[j]; cnt++; }
        out[i] = cnt > 0 ? sum / cnt : data[i];
    }
    *out_len = n;
    return out;
}

gmb_double_t* gmb_ts_decompose_seasonal(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len) {
    if (n <= 0) { *out_len = 0; return NULL; }
    if (period <= 0) period = 1;
    gmb_double_t* out = (gmb_double_t*)malloc(n * sizeof(gmb_double_t));
    if (!out) { *out_len = 0; return NULL; }
    /* media del trend per ogni fase del periodo, poi sottrai dalla media globale */
    double* trend = gmb_ts_decompose_trend(data, n, period, out_len);
    double global = 0;
    for (gmb_int_t i = 0; i < n; i++) global += data[i];
    global /= n;
    if (trend) {
        for (gmb_int_t i = 0; i < n; i++) out[i] = (data[i] - trend[i]) - global;
        free(trend);
    } else {
        for (gmb_int_t i = 0; i < n; i++) out[i] = 0;
    }
    *out_len = n;
    return out;
}

/* DECOMPOSE completo → matrice [3][n] (Trend, Seasonal, Residual) — contratto del campione
   (Decomposed(0)=trend, (1)=seasonal, (2)=residual). Residual = data − trend − seasonal.
   out_len = 3*n; layout out[0*n+i]=trend[i], out[1*n+i]=seasonal[i], out[2*n+i]=residual[i]. */
gmb_double_t* gmb_ts_decompose_matrix(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len) {
    if (n <= 0) { *out_len = 0; return NULL; }
    if (period <= 0) period = 1;
    gmb_int_t tlen = 0, slen = 0;
    double* trend = gmb_ts_decompose_trend(data, n, period, &tlen);
    double* seasonal = gmb_ts_decompose_seasonal(data, n, period, &slen);
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)(3 * n) * sizeof(gmb_double_t));
    if (!out) { free(trend); free(seasonal); *out_len = 0; return NULL; }
    for (gmb_int_t i = 0; i < n; i++) {
        double t = trend ? trend[i] : data[i];
        double s = seasonal ? seasonal[i] : 0;
        out[0*n + i] = t;
        out[1*n + i] = s;
        out[2*n + i] = data[i] - t - s;
    }
    free(trend); free(seasonal);
    *out_len = 3 * n;
    return out;
}

gmb_double_t** gmb_ts_covariance_matrix(const double* const* series, int k, int n)
{
    if (k <= 0 || n <= 0 || !series) return NULL;
    gmb_double_t** out = (gmb_double_t**)calloc((size_t)k, sizeof(gmb_double_t*));
    if (!out) return NULL;
    double* means = (double*)calloc((size_t)k, sizeof(double));
    if (!means) { free(out); return NULL; }
    for (int i = 0; i < k; i++)
        for (int j = 0; j < n; j++) means[i] += series[i][j];
    for (int i = 0; i < k; i++) means[i] /= n;
    for (int i = 0; i < k; i++)
    {
        out[i] = (gmb_double_t*)calloc((size_t)k, sizeof(gmb_double_t));
        if (!out[i]) continue;
        for (int j = 0; j < k; j++)
        {
            double s = 0;
            for (int l = 0; l < n; l++) s += (series[i][l] - means[i]) * (series[j][l] - means[j]);
            out[i][j] = s / n;
        }
    }
    free(means);
    return out;
}

/* TSCOVARIANCEMATRIX(data) — parità CommonLib.Numerics.CovarianceMatrix: `data` è un
 * jagged di n_obs osservazioni × n_vars variabili; ritorna n_vars × n_vars con correzione
 * di Bessel (n_obs-1). Diverso da gmb_ts_covariance_matrix (covarianza fra k serie). */
gmb_double_t** gmb_ts_covariance_obs(const double* const* obs, int n_obs, int n_vars)
{
    if (n_obs < 2 || n_vars <= 0 || !obs) return NULL;
    gmb_double_t** out = (gmb_double_t**)calloc((size_t)n_vars, sizeof(gmb_double_t*));
    if (!out) return NULL;
    double* means = (double*)calloc((size_t)n_vars, sizeof(double));
    if (!means) { free(out); return NULL; }
    for (int j = 0; j < n_vars; j++)
        for (int k = 0; k < n_obs; k++) means[j] += obs[k][j];
    for (int j = 0; j < n_vars; j++) means[j] /= n_obs;
    for (int i = 0; i < n_vars; i++)
    {
        out[i] = (gmb_double_t*)calloc((size_t)n_vars, sizeof(gmb_double_t));
        if (!out[i]) continue;
        for (int j = 0; j < n_vars; j++)
        {
            double s = 0;
            for (int k = 0; k < n_obs; k++) s += (obs[k][i] - means[i]) * (obs[k][j] - means[j]);
            out[i][j] = s / (n_obs - 1);
        }
    }
    free(means);
    return out;
}

static int gmb_dcmp_dbl(const void* a, const void* b) { double x = *(const double*)a, y = *(const double*)b; return (x > y) - (x < y); }

gmb_double_t* gmb_ts_minmax(gmb_double_t* data, gmb_int_t n, gmb_int_t* out_len) {
    if (!data || n <= 0) { ts_err("Serie vuota."); *out_len = 0; return NULL; }
    gmb_double_t* out = (gmb_double_t*)calloc(2, sizeof(gmb_double_t));
    *out_len = 2;
    double mn = data[0], mx = data[0];
    for (gmb_int_t i = 1; i < n; i++) { if (data[i] < mn) mn = data[i]; if (data[i] > mx) mx = data[i]; }
    out[0] = mn; out[1] = mx;
    return out;
}

gmb_double_t* gmb_ts_rolling_skewness(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || window < 3 || n < window) { ts_err("Dati o finestra non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++) {
        double s = 0; for (gmb_int_t j = 0; j < window; j++) s += data[i + j];
        double m = s / window, m2 = 0, m3 = 0;
        for (gmb_int_t j = 0; j < window; j++) { double d = data[i + j] - m; m2 += d * d; m3 += d * d * d; }
        m2 /= window; m3 /= window;
        double sd = sqrt(m2);
        out[i] = (sd > 1e-12) ? (m3 / (sd * sd * sd)) : 0.0;
    }
    return out;
}

gmb_double_t* gmb_ts_rolling_kurtosis(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || window < 4 || n < window) { ts_err("Dati o finestra non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++) {
        double s = 0; for (gmb_int_t j = 0; j < window; j++) s += data[i + j];
        double m = s / window, m4 = 0, s2 = 0;
        for (gmb_int_t j = 0; j < window; j++) {
            double d = data[i + j] - m;
            m4 += d * d * d * d;
            s2 += d * d;
        }
        /* Kurtosi con CORREZIONE DI BIAS, parity Numerics.TimeSeries.RollingKurtosis
         * (oracolo IL): var campionaria s2/(n-1) e formula
         *   n(n+1)Σd⁴ / ((n-1)(n-2)(n-3)·s²²) − 3(n-1)²/((n-2)(n-3)).
         * Prima si usava la kurtosi grezza m4/m2² − 3 (−1.3 invece di −1.2). */
        double n = (double)window;
        double var = s2 / (n - 1.0);
        if (var < 1e-12) { out[i] = 0.0; continue; }
        double k = (n * (n + 1.0) * m4) / ((n - 1.0) * (n - 2.0) * (n - 3.0) * var * var)
                 - (3.0 * (n - 1.0) * (n - 1.0)) / ((n - 2.0) * (n - 3.0));
        out[i] = k;
    }
    return out;
}

gmb_double_t* gmb_ts_rolling_median(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || window < 1 || n < window) { ts_err("Dati o finestra non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    double* buf = (double*)malloc((size_t)window * sizeof(double));
    for (gmb_int_t i = 0; i < len; i++) {
        for (gmb_int_t j = 0; j < window; j++) buf[j] = data[i + j];
        qsort(buf, (size_t)window, sizeof(double), gmb_dcmp_dbl);
        out[i] = (window % 2) ? buf[window / 2] : (buf[window / 2 - 1] + buf[window / 2]) / 2.0;
    }
    free(buf);
    return out;
}

gmb_double_t* gmb_ts_rolling_quantile(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double q, gmb_int_t* out_len) {
    if (!data || window < 1 || n < window) { ts_err("Dati o finestra non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    double* buf = (double*)malloc((size_t)window * sizeof(double));
    for (gmb_int_t i = 0; i < len; i++) {
        for (gmb_int_t j = 0; j < window; j++) buf[j] = data[i + j];
        qsort(buf, (size_t)window, sizeof(double), gmb_dcmp_dbl);
        double pos = (window - 1) * q;
        int lo = (int)pos;
        double frac = pos - lo;
        out[i] = (lo + 1 < window) ? buf[lo] * (1 - frac) + buf[lo + 1] * frac : buf[lo];
    }
    free(buf);
    return out;
}

gmb_double_t* gmb_ts_rolling_sharpe(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double rf, gmb_int_t* out_len) {
    if (!data || window < 2 || n < window) { ts_err("Dati o finestra non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++) {
        double s = 0; for (gmb_int_t j = 0; j < window; j++) s += data[i + j];
        double m = s / window, v = 0;
        for (gmb_int_t j = 0; j < window; j++) { double d = data[i + j] - m; v += d * d; }
        v /= window;   /* deviazione standard POPOLAZIONE: parity RollingSharpe (oracolo IL) */
        double sd = sqrt(v);
        out[i] = (sd > 1e-12) ? (m - rf) / sd : 0.0;
    }
    return out;
}

gmb_double_t* gmb_ts_rolling_sortino(gmb_double_t* data, gmb_int_t n, gmb_int_t window, double rf, double target, gmb_int_t* out_len) {
    if (!data || window < 2 || n < window) { ts_err("Dati o finestra non validi."); *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    *out_len = len;
    for (gmb_int_t i = 0; i < len; i++) {
        double s = 0; for (gmb_int_t j = 0; j < window; j++) s += data[i + j];
        double m = s / window, dd = 0;
        for (gmb_int_t j = 0; j < window; j++) { double d = data[i + j] - target; if (d < 0) dd += d * d; }
        dd /= window;
        double ds = sqrt(dd);
        out[i] = (ds > 1e-12) ? (m - rf) / ds : 0.0;
    }
    return out;
}

gmb_double_t* gmb_ts_rolling_minmax(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || window < 1 || n < window) { *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)(2 * len), sizeof(gmb_double_t));
    *out_len = 2 * len;
    for (gmb_int_t i = 0; i < len; i++) {
        double mn = data[i], mx = data[i];
        for (gmb_int_t j = 1; j < window; j++) { double v = data[i + j]; if (v < mn) mn = v; if (v > mx) mx = v; }
        out[2 * i] = mn; out[2 * i + 1] = mx;
    }
    return out;
}

gmb_double_t* gmb_ts_rolling_regression_single(gmb_double_t* data, gmb_int_t n, gmb_int_t window, gmb_int_t* out_len) {
    if (!data || window < 2 || n < window) { *out_len = 0; return NULL; }
    gmb_int_t len = n - window + 1;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)(2 * len), sizeof(gmb_double_t));
    *out_len = 2 * len;
    for (gmb_int_t i = 0; i < len; i++) {
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (gmb_int_t j = 0; j < window; j++) { double x = j, y = data[i + j]; sx += x; sy += y; sxx += x * x; sxy += x * y; }
        double denom = window * sxx - sx * sx;
        double beta = (fabs(denom) > 1e-12) ? (window * sxy - sx * sy) / denom : 0.0;
        double alpha = (sy - beta * sx) / window;
        out[2 * i] = beta; out[2 * i + 1] = alpha;
    }
    return out;
}
