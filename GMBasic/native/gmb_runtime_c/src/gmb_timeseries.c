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

/* DECOMPOSE: trend = media mobile CENTRATA di ±period/2 (estremi clampati, include il punto i),
 * stagionalità = media per fase di (data - trend). È la definizione di
 * CommonLib.Numerics.Decompose (oracolo IL): prima il trend usava una finestra di periodo
 * intero e la stagionalità era "deviazione dalla media globale", quindi le gambe divergevano. */
gmb_double_t* gmb_ts_decompose_trend(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len) {
    if (n <= 0) { *out_len = 0; return NULL; }
    if (period <= 0) period = 1;
    gmb_double_t* out = (gmb_double_t*)malloc(n * sizeof(gmb_double_t));
    if (!out) { *out_len = 0; return NULL; }
    gmb_int_t half = period / 2;
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_int_t left = (i - half > 0) ? i - half : 0;
        gmb_int_t right = (i + half < n - 1) ? i + half : n - 1;
        double sum = 0;
        for (gmb_int_t j = left; j <= right; j++) sum += data[j];
        out[i] = sum / (double)(right - left + 1);
    }
    *out_len = n;
    return out;
}

gmb_double_t* gmb_ts_decompose_seasonal(gmb_double_t* data, gmb_int_t n, gmb_int_t period, gmb_int_t* out_len) {
    if (n <= 0) { *out_len = 0; return NULL; }
    if (period <= 0) period = 1;
    gmb_double_t* out = (gmb_double_t*)malloc(n * sizeof(gmb_double_t));
    if (!out) { *out_len = 0; return NULL; }
    gmb_int_t tlen = 0;
    double* trend = gmb_ts_decompose_trend(data, n, period, &tlen);
    double* means = (double*)calloc((size_t)period, sizeof(double));
    int* counts = (int*)calloc((size_t)period, sizeof(int));
    if (!means || !counts) { free(trend); free(means); free(counts); *out_len = 0; return NULL; }
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_int_t idx = i % period;
        means[idx] += data[i] - (trend ? trend[i] : data[i]);
        counts[idx]++;
    }
    for (gmb_int_t i = 0; i < period; i++) if (counts[i] > 0) means[i] /= counts[i];
    for (gmb_int_t i = 0; i < n; i++) out[i] = means[i % period];
    free(trend); free(means); free(counts);
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
        double m = s / window, s2 = 0, s3 = 0;
        for (gmb_int_t j = 0; j < window; j++) { double d = data[i + j] - m; s2 += d * d; s3 += d * d * d; }
        /* Skewness con CORREZIONE DI BIAS, parity Numerics.TimeSeries.RollingSkewness
         * (oracolo IL): deviazione standard CAMPIONARIA sqrt(Σd²/(n-1)) e formula
         *   n/((n-1)(n-2)) · Σd³ / s³.
         * Prima si usava la skewness grezza (m3/m2^1.5): valori diversi su ogni input non
         * degenere (es. n=5: fattore 3.333 vs 2.236), quindi le gambe divergevano. */
        double nn = (double)window;
        double var = s2 / (nn - 1.0);
        double sd = sqrt(var);
        out[i] = (sd < 1e-12) ? 0.0 : (nn / ((nn - 1.0) * (nn - 2.0))) * (s3 / (sd * sd * sd));
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
        double mean = s / window - rf, dd = 0;
        gmb_int_t cnt = 0;
        for (gmb_int_t j = 0; j < window; j++) {
            double d = data[i + j] - target;
            if (d < 0) { dd += d * d; cnt++; }
        }
        /* Parity Numerics.RollingSortino (oracolo IL): la downside deviation è la media sui
         * SOLI rendimenti negativi (non su tutta la finestra) e senza downside il risultato è
         * +∞, non 0. Prima si divideva per `window` e si restituiva 0 → valori diversi. */
        double ds = cnt > 0 ? sqrt(dd / (double)cnt) : 0.0;
        out[i] = (ds > 0.0) ? mean / ds : INFINITY;
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

/* Hurst exponent via DFA (detrended fluctuation analysis), parity gmbstats gs_hurst_dfa. */
double gmb_hurst_dfa(const double* x, int n, int min_box)
{
    if (!x || n < 20 || min_box < 2) return NAN;
    double* y = (double*)malloc((size_t)n * sizeof(double));
    double* ls = (double*)malloc(sizeof(double) * 64);
    double* lx = (double*)malloc(sizeof(double) * 64);
    if (!y || !ls || !lx) { free(y); free(ls); free(lx); return NAN; }
    double mean = 0;
    for (int i = 0; i < n; i++) mean += x[i];
    mean /= n;
    y[0] = x[0] - mean;
    for (int i = 1; i < n; i++) y[i] = y[i - 1] + (x[i] - mean);
    int nb = 0;
    for (int t = min_box; t <= n / 2 && nb < 64; t *= 2) {
        int nboxes = n / t;
        double F = 0;
        for (int j = 0; j < nboxes; j++) {
            double mx = 0, my = 0, sxy = 0, sxx = 0;
            for (int k = 0; k < t; k++) { mx += k; my += y[j * t + k]; }
            mx /= t; my /= t;
            for (int k = 0; k < t; k++) {
                double dx = k - mx, dy = y[j * t + k] - my;
                sxy += dx * dy; sxx += dx * dx;
            }
            double slope = sxx > 0 ? sxy / sxx : 0;
            double inter = my - slope * mx;
            for (int k = 0; k < t; k++) {
                double r = y[j * t + k] - (slope * k + inter);
                F += r * r;
            }
        }
        F = sqrt(F / (nboxes * t));
        ls[nb] = log((double)t); lx[nb] = log(F); nb++;
    }
    double m1 = 0, m2 = 0;
    for (int i = 0; i < nb; i++) { m1 += ls[i]; m2 += lx[i]; }
    m1 /= nb; m2 /= nb;
    double sxy = 0, sxx = 0;
    for (int i = 0; i < nb; i++) { sxy += (ls[i] - m1) * (lx[i] - m2); sxx += (ls[i] - m1) * (ls[i] - m1); }
    double h = sxx > 0 ? sxy / sxx : NAN;
    free(y); free(ls); free(lx);
    return h;
}

/* Fractional differencing (gmbstats gs_frac_diff). */
double* gmb_ts_fracdiff(const double* x, int n, double d, int* out_len)
{
    if (!x || n < 1 || d < 0) { *out_len = 0; return NULL; }
    double* w = (double*)malloc((size_t)n * sizeof(double));
    double* out = (double*)malloc((size_t)n * sizeof(double));
    if (!w || !out) { free(w); free(out); *out_len = 0; return NULL; }
    w[0] = 1.0;
    for (int k = 1; k < n; k++) w[k] = -w[k - 1] * (d - k + 1) / k;
    for (int i = 0; i < n; i++) {
        double sum = 0;
        for (int k = 0; k <= i; k++) sum += w[k] * x[i - k];
        out[i] = sum;
    }
    free(w); *out_len = n; return out;
}

/* Gaussian elimination with partial pivoting for small dense systems. */
static int gmb_ts_solve_lin(int n, double* A, double* b, double* x)
{
    return gmb_solve_lin_tol(n, A, b, x, 1e-14) ? 0 : -1;
}

static gmb_val_t gmb_ts_nan_dict(void)
{
    gmb_dict_t d = gmb_dict_new();
    gmb_dict_set(&d, "Statistic", gmb_make_val_double(NAN));
    gmb_dict_set(&d, "PValue", gmb_make_val_double(NAN));
    gmb_dict_set(&d, "DegreesOfFreedom", gmb_make_val_double(NAN));
    return gmb_make_val_dict_take(&d);
}
/* Granger causality y -> x (F-test), parity gmbstats gs_granger. */
gmb_val_t gmb_ts_granger_flat(const double* x, int nx, const double* y, int ny, int p)
{
    int n = nx < ny ? nx : ny;
    if (!x || !y || p < 1 || n < 3 * p + 2) return gmb_ts_nan_dict();
    int m = n - p, nr = p + 1, nu = 2 * p + 1;
    double* Xr = (double*)malloc((size_t)m * nr * sizeof(double));
    double* Xu = (double*)malloc((size_t)m * nu * sizeof(double));
    double* Ar = (double*)calloc((size_t)nr * nr, sizeof(double));
    double* Au = (double*)calloc((size_t)nu * nu, sizeof(double));
    double* br = (double*)calloc((size_t)nr, sizeof(double));
    double* bu = (double*)calloc((size_t)nu, sizeof(double));
    double* cr = (double*)calloc((size_t)nr, sizeof(double));
    double* cu = (double*)calloc((size_t)nu, sizeof(double));
    if (!Xr || !Xu || !Ar || !Au || !br || !bu || !cr || !cu) {
        free(Xr); free(Xu); free(Ar); free(Au); free(br); free(bu); free(cr); free(cu);
        return gmb_ts_nan_dict();
    }
    for (int i = p; i < n; i++) {
        int r = i - p;
        Xr[r*nr] = 1.0; Xu[r*nu] = 1.0;
        for (int j = 1; j <= p; j++) {
            Xr[r*nr+j] = x[i-j]; Xu[r*nu+j] = x[i-j];
            Xu[r*nu+p+j] = y[i-j];
        }
    }
    for (int i = 0; i < m; i++)
        for (int j = 0; j < nr; j++) {
            for (int k = 0; k < nr; k++) Ar[j*nr+k] += Xr[i*nr+j] * Xr[i*nr+k];
            br[j] += Xr[i*nr+j] * x[i+p];
        }
    if (gmb_ts_solve_lin(nr, Ar, br, cr) != 0) {
        free(Xr); free(Xu); free(Ar); free(Au); free(br); free(bu); free(cr); free(cu);
        return gmb_ts_nan_dict();
    }
    double rss_r = 0;
    for (int i = 0; i < m; i++) {
        double r = x[i+p];
        for (int j = 0; j < nr; j++) r -= cr[j] * Xr[i*nr+j];
        rss_r += r * r;
    }
    for (int i = 0; i < m; i++)
        for (int j = 0; j < nu; j++) {
            for (int k = 0; k < nu; k++) Au[j*nu+k] += Xu[i*nu+j] * Xu[i*nu+k];
            bu[j] += Xu[i*nu+j] * x[i+p];
        }
    if (gmb_ts_solve_lin(nu, Au, bu, cu) != 0) {
        free(Xr); free(Xu); free(Ar); free(Au); free(br); free(bu); free(cr); free(cu);
        return gmb_ts_nan_dict();
    }
    double rss_u = 0;
    for (int i = 0; i < m; i++) {
        double r = x[i+p];
        for (int j = 0; j < nu; j++) r -= cu[j] * Xu[i*nu+j];
        rss_u += r * r;
    }
    free(Xr); free(Xu); free(Ar); free(Au); free(br); free(bu); free(cr); free(cu);
    if (rss_u <= 0) return gmb_ts_nan_dict();
    double df1 = p, df2 = m - nu;
    double F = ((rss_r - rss_u) / df1) / (rss_u / df2);
    double xb = df1 * F / (df1 * F + df2);
    double pval = 1.0 - gmb_dist_beta_cdf(df1 / 2.0, df2 / 2.0, xb);
    {
        gmb_dict_t d = gmb_dict_new();
        gmb_dict_set(&d, "Statistic", gmb_make_val_double(F));
        gmb_dict_set(&d, "PValue", gmb_make_val_double(pval));
        gmb_dict_set(&d, "DegreesOfFreedom", gmb_make_val_double(df1));
        return gmb_make_val_dict_take(&d);
    }
}

static double gmb_ts_garch_nll(const double* p, const double* r, int n)
{
    double w = p[0], al = p[1], be = p[2], s2 = r[0] * r[0], ll = 0;
    if (s2 <= 0) s2 = 1e-6;
    for (int t = 1; t < n; t++) {
        s2 = w + al * r[t-1] * r[t-1] + be * s2;
        if (s2 < 1e-12) s2 = 1e-12;
        ll += log(s2) + r[t] * r[t] / s2;
    }
    return 0.5 * ll;
}
gmb_val_t gmb_ts_garch11_flat(const double* r, int n)
{
    if (!r || n < 5) return gmb_ts_nan_dict();
    double p[3], h = 1e-5, step = 0.05, v = 0;
    for (int i = 0; i < n; i++) v += r[i] * r[i];
    v /= n;
    p[0] = 0.1 * v; p[1] = 0.1; p[2] = 0.8;
    for (int it = 0; it < 2000; it++) {
        double f0 = gmb_ts_garch_nll(p, r, n), grad[3], np[3], gn = 0;
        for (int i = 0; i < 3; i++) {
            double save = p[i];
            p[i] = save + h;
            grad[i] = (gmb_ts_garch_nll(p, r, n) - f0) / h;
            p[i] = save;
            gn += grad[i] * grad[i];
        }
        gn = sqrt(gn);
        if (gn < 1e-10) break;
        for (int i = 0; i < 3; i++) np[i] = p[i] - step * grad[i] / gn;
        if (np[0] < 1e-8) np[0] = 1e-8;
        if (np[1] < 0) np[1] = 0;
        if (np[2] < 0) np[2] = 0;
        if (np[1] + np[2] > 0.999) { double ss = 0.999 / (np[1] + np[2]); np[1] *= ss; np[2] *= ss; }
        if (gmb_ts_garch_nll(np, r, n) < f0) { p[0]=np[0]; p[1]=np[1]; p[2]=np[2]; step *= 1.05; }
        else step *= 0.5;
        if (step < 1e-12) break;
    }
    {
        gmb_dict_t d = gmb_dict_new();
        gmb_dict_set(&d, "Omega", gmb_make_val_double(p[0]));
        gmb_dict_set(&d, "Alpha", gmb_make_val_double(p[1]));
        gmb_dict_set(&d, "Beta", gmb_make_val_double(p[2]));
        return gmb_make_val_dict_take(&d);
    }
}

/* Kalman 1D (gmbstats gs_kalman1d), x0=z[0], p0=1. */
double* gmb_ts_kalman1d(const double* z, int n, double a, double h, double q, double r, int* out_len)
{
    if (!z || n <= 0) { *out_len = 0; return NULL; }
    double* out = (double*)malloc((size_t)n * sizeof(double));
    if (!out) { *out_len = 0; return NULL; }
    double x = z[0], P = 1.0;
    for (int k = 0; k < n; k++) {
        double xp = a * x, Pp = a * P * a + q;
        double K = Pp * h / (h * Pp * h + r);
        x = xp + K * (z[k] - h * xp);
        P = (1.0 - K * h) * Pp;
        out[k] = x;
    }
    *out_len = n; return out;
}

/* Stationary bootstrap (gmbstats gs_stationary_bootstrap). */
double* gmb_ts_stationary_bootstrap(const double* x, int n, int window, int seed, int* out_len)
{
    if (!x || n < 1 || window < 1) { *out_len = 0; return NULL; }
    double* out = (double*)malloc((size_t)n * sizeof(double));
    if (!out) { *out_len = 0; return NULL; }
    double p = 1.0 / window;
    unsigned s = seed ? (unsigned)seed : 12345u;
    int idx = (int)((s = s * 1103515245u + 12345u) % (unsigned)n);
    out[0] = x[idx];
    for (int i = 1; i < n; i++) {
        double u = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
        if (u > p) {
            double u2 = (double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0;
            idx = (int)(u2 * (double)n) % n;
        } else {
            idx = (idx + 1) % n;
        }
        out[i] = x[idx];
    }
    *out_len = n; return out;
}

/* Sample entropy (gmbstats gs_entropy_sample). */
static double gmb_ts_entropy_dist(const double* x, int i, int j, int m)
{
    double d = 0;
    for (int k = 0; k < m; k++) { double v = fabs(x[i+k] - x[j+k]); if (v > d) d = v; }
    return d;
}
double gmb_ts_sample_entropy(const double* x, int n, int m, double r)
{
    if (!x || n < m + 1 || m < 1 || r <= 0) return NAN;
    double B = 0, A = 0;
    for (int i = 0; i < n - m; i++)
        for (int j = 0; j < n - m; j++) {
            if (i == j) continue;
            if (gmb_ts_entropy_dist(x, i, j, m) <= r) B += 1;
        }
    for (int i = 0; i < n - m - 1; i++)
        for (int j = 0; j < n - m - 1; j++) {
            if (i == j) continue;
            if (gmb_ts_entropy_dist(x, i, j, m + 1) <= r) A += 1;
        }
    if (B == 0 || A == 0) return NAN;
    return -log(A / B);
}

double gmb_ts_approx_entropy(const double* x, int n, int m, double r)
{
    if (!x || n < m + 1 || m < 1 || r <= 0) return NAN;
    double B = 0, A = 0;
    for (int i = 0; i <= n - m; i++)
        for (int j = 0; j <= n - m; j++)
            if (gmb_ts_entropy_dist(x, i, j, m) <= r) B += 1;
    for (int i = 0; i <= n - m - 1; i++)
        for (int j = 0; j <= n - m - 1; j++)
            if (gmb_ts_entropy_dist(x, i, j, m + 1) <= r) A += 1;
    if (B == 0 || A == 0) return NAN;
    return log(B / (double)(n - m + 1)) - log(A / (double)(n - m));
}

/* Permutation entropy (gmbstats gs_entropy_perm): diversità di pattern ordinali. */
double gmb_ts_perm_entropy(const double* x, int n, int m, int lag)
{
    if (!x || m < 2 || lag < 1 || n < m * lag) return NAN;
    int* seen = (int*)malloc((size_t)m * (size_t)n * sizeof(int));
    int* order = (int*)malloc((size_t)m * sizeof(int));
    int* seq = (int*)malloc((size_t)m * sizeof(int));
    if (!seen || !order || !seq) { free(seen); free(order); free(seq); return NAN; }
    int cnt = 0;
    for (int i = 0; i <= n - m * lag; i++) {
        for (int k = 0; k < m; k++) order[k] = k;
        for (int j = 0; j < m; j++)
            for (int k = j + 1; k < m; k++)
                if (x[i + order[k] * lag] < x[i + order[j] * lag]) { int t = order[j]; order[j] = order[k]; order[k] = t; }
        for (int j = 0; j < m; j++) seq[j] = order[j];
        int is_new = 1;
        for (int j = 0; j < cnt && is_new; j++) {
            int same = 1;
            for (int q = 0; q < m; q++) if (seen[q * n + j] != seq[q]) { same = 0; break; }
            if (same) is_new = 0;
        }
        if (is_new) {
            for (int j = 0; j < m; j++) seen[j * n + cnt] = seq[j];
            cnt++;
        }
    }
    free(seen); free(order); free(seq);
    if (cnt <= 1) return 0.0;
    int npat = 1;
    for (int q = 2; q <= m; q++) npat *= q;
    return log((double)cnt) / log((double)npat);
}

/* Ornstein-Uhlenbeck fit (gmbstats gs_ou_fit). */
gmb_val_t gmb_ts_ou_fit_flat(const double* x, int n, double dt)
{
    gmb_dict_t d = gmb_dict_new();
    if (!x || n < 3 || dt <= 0) {
        gmb_dict_set(&d, "Theta", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Mu", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Sigma", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Phi", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "HalfLife", gmb_make_val_double(NAN));
        return gmb_make_val_dict_take(&d);
    }
    double mean_x = 0, mean_y = 0, sxx = 0, sxy = 0, sse = 0;
    int cnt = 0;
    for (int i = 1; i < n; i++) {
        double xv = x[i-1], yv = x[i], dx, dy, inv;
        cnt++; dx = xv - mean_x; dy = yv - mean_y; inv = 1.0 / cnt;
        mean_x += dx * inv; mean_y += dy * inv;
        sxx += dx * (xv - mean_x); sxy += dx * (yv - mean_y);
    }
    double phi = sxx > 0 ? sxy / sxx : NAN;
    double alpha = mean_y - phi * mean_x;
    double logphi = phi > 0 && phi < 1 ? log(phi) : NAN;
    double omphi = -expm1(logphi);
    double mu = alpha / omphi;
    double theta = -logphi / dt;
    for (int i = 1; i < n; i++) { double r = x[i] - alpha - phi * x[i-1]; sse += r * r; }
    double vf = -expm1(2 * logphi) / (2 * theta);
    double iv = sse / cnt;
    double sigma = sqrt(iv / vf);
    gmb_dict_set(&d, "Theta", gmb_make_val_double(theta));
    gmb_dict_set(&d, "Mu", gmb_make_val_double(mu));
    gmb_dict_set(&d, "Sigma", gmb_make_val_double(sigma));
    gmb_dict_set(&d, "Phi", gmb_make_val_double(phi));
    gmb_dict_set(&d, "HalfLife", gmb_make_val_double(log(2.0) / theta));
    return gmb_make_val_dict_take(&d);
}

/* Lempel-Ziv 76 complexity (gmbstats gs_entropy_lz). */
double gmb_ts_lz_complexity(const double* x, int n)
{
    if (!x || n < 2) return NAN;
    double* sorted = (double*)malloc((size_t)n * sizeof(double));
    int* b = (int*)malloc((size_t)n * sizeof(int));
    if (!sorted || !b) { free(sorted); free(b); return NAN; }
    for (int i = 0; i < n; i++) sorted[i] = x[i];
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (sorted[j] < sorted[i]) { double t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
    double med = sorted[n / 2];
    for (int i = 0; i < n; i++) b[i] = x[i] >= med ? 1 : 0;
    free(sorted);
    int c = 0, ii = 0;
    while (ii < n) {
        int maxlen = 0;
        for (int start = 0; start < ii; start++) {
            int m = 0;
            while (ii + m < n && start + m < ii && b[start + m] == b[ii + m]) m++;
            if (m > maxlen) maxlen = m;
        }
        c++; ii += maxlen + 1;
    }
    free(b);
    return (double)c;
}

/* BMA weights: w_i ∝ exp(-0.5*(IC_i - min IC)) (gmbstats gs_bma_weights). */
double* gmb_ts_bma_weights(const double* ic, int n, int* out_len)
{
    if (!ic || n < 1) { *out_len = 0; return NULL; }
    double* out = (double*)malloc((size_t)n * sizeof(double));
    if (!out) { *out_len = 0; return NULL; }
    double minic = ic[0], sum = 0;
    for (int i = 1; i < n; i++) if (ic[i] < minic) minic = ic[i];
    for (int i = 0; i < n; i++) { out[i] = exp(-0.5 * (ic[i] - minic)); sum += out[i]; }
    if (sum > 0) for (int i = 0; i < n; i++) out[i] /= sum;
    *out_len = n; return out;
}

/* Largest Lyapunov exponent (gmbstats gs_lyapunov). */
double gmb_ts_lyapunov(const double* x, int n, int m, int tau)
{
    if (!x || n < m * tau + 8 || m < 2 || tau < 1) return NAN;
    int M = n - (m - 1) * tau;
    int theiler = m * tau > 10 ? m * tau : 10;
    double* d0 = (double*)malloc((size_t)M * sizeof(double));
    int* nbr = (int*)malloc((size_t)M * sizeof(int));
    double* lambdas = (double*)calloc((size_t)(M / 2 + 1), sizeof(double));
    if (!d0 || !nbr || !lambdas) { free(d0); free(nbr); free(lambdas); return NAN; }
    for (int i = 0; i < M; i++) {
        double best = 1e300; int bj = -1;
        for (int j = 0; j < M; j++) {
            if (abs(i - j) < theiler) continue;
            double d2 = 0;
            for (int q = 0; q < m; q++) { double dd = x[i+q*tau] - x[j+q*tau]; d2 += dd*dd; }
            if (d2 > 0 && d2 < best) { best = d2; bj = j; }
        }
        d0[i] = best > 0 ? sqrt(best) : 1e-9;
        nbr[i] = bj;
    }
    for (int k = 1; k < M / 2; k++) {
        double sum = 0; int cnt = 0;
        for (int i = 0; i < M - k; i++) {
            int jj = nbr[i];
            if (jj >= 0 && jj + k < M) {
                double d2 = 0;
                for (int q = 0; q < m; q++) { double dd = x[i+k+q*tau] - x[jj+k+q*tau]; d2 += dd*dd; }
                if (d2 > 0) { sum += 0.5 * log(d2 / (d0[i] * d0[i])); cnt++; }
            }
        }
        lambdas[k] = cnt > 0 ? sum / cnt : 0;
    }
    int k0 = 1, k1 = 8 < M / 2 ? 8 : M / 2;
    if (k1 > M / 2 - 1) k1 = M / 2 - 1;
    if (k1 <= k0) k1 = k0 + 1;
    double mx = 0, my = 0; int cnt = 0;
    for (int k = k0; k < k1; k++) { mx += k; my += lambdas[k]; cnt++; }
    if (cnt == 0) { free(d0); free(nbr); free(lambdas); return 0; }
    mx /= cnt; my /= cnt;
    double sxy = 0, sxx = 0;
    for (int k = k0; k < k1; k++) { double dx = k - mx, dy = lambdas[k] - my; sxy += dx*dy; sxx += dx*dx; }
    double lam = sxx > 0 ? sxy / sxx : 0;
    free(d0); free(nbr); free(lambdas);
    return lam;
}

/* ==================== Hidden Markov Model gaussiano ==================== */
static double gmb_hmm_logadd(double a, double b)
{
    double m;
    if (a == -INFINITY) return b;
    if (b == -INFINITY) return a;
    m = a > b ? a : b;
    return m + log(exp(a - m) + exp(b - m));
}

double gmb_ts_hmm_forward(const double* lpi, const double* lA, const double* lE, int K, int n)
{
    double* alpha; double* scale; double loglik = 0; int t, i, j;
    if (!lpi || !lA || !lE || K < 1 || n < 1) return NAN;
    alpha = (double*)malloc((size_t)n * K * sizeof(double));
    scale = (double*)calloc((size_t)n, sizeof(double));
    if (!alpha || !scale) { free(alpha); free(scale); return NAN; }
    for (i = 0; i < K; i++) { alpha[i] = lpi[i] + lE[i]; scale[0] += alpha[i]; }
    if (scale[0] < -1e300) { free(alpha); free(scale); return -INFINITY; }
    for (i = 0; i < K; i++) alpha[i] -= scale[0];
    loglik += scale[0];
    for (t = 1; t < n; t++) {
        scale[t] = 0;
        for (j = 0; j < K; j++) {
            double best = -INFINITY;
            for (i = 0; i < K; i++) { double v = alpha[(t-1)*K+i] + lA[i*K+j]; if (v > best) best = v; }
            alpha[t*K+j] = best + lE[t*K+j];
            scale[t] += alpha[t*K+j];
        }
        if (scale[t] < -1e300) { free(alpha); free(scale); return -INFINITY; }
        for (j = 0; j < K; j++) alpha[t*K+j] -= scale[t];
        loglik += scale[t];
    }
    free(alpha); free(scale);
    return loglik;
}

double* gmb_ts_hmm_viterbi(const double* lpi, const double* lA, const double* lE, int K, int n, int* outn)
{
    double* delta; int* psi; double* path; double best; int t, i, j;
    if (outn) *outn = 0;
    if (!lpi || !lA || !lE || K < 1 || n < 1) return NULL;
    delta = (double*)malloc((size_t)n * K * sizeof(double));
    psi = (int*)malloc((size_t)n * K * sizeof(int));
    path = (double*)malloc((size_t)n * sizeof(double));
    if (!delta || !psi || !path) { free(delta); free(psi); free(path); return NULL; }
    for (i = 0; i < K; i++) delta[i] = lpi[i] + lE[i];
    for (t = 1; t < n; t++) {
        for (j = 0; j < K; j++) {
            double bj = -INFINITY; int bi = 0;
            for (i = 0; i < K; i++) {
                double v = delta[(t-1)*K+i] + lA[i*K+j];
                if (v > bj) { bj = v; bi = i; }
            }
            delta[t*K+j] = bj + lE[t*K+j];
            psi[t*K+j] = bi;
        }
    }
    best = -INFINITY;
    for (j = 0; j < K; j++)
        if (delta[(n-1)*K+j] > best) { best = delta[(n-1)*K+j]; path[n-1] = j; }
    for (t = n - 2; t >= 0; t--) path[t] = psi[(t+1)*K + (int)path[t+1]];
    free(delta); free(psi);
    if (outn) *outn = n;
    return path;
}

static gmb_val_t gmb_hmm_list(const double* a, int n)
{
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_double(a[i]));
    return gmb_make_val_list_take(&l);
}

static gmb_val_t gmb_hmm_dict(gmb_val_t pi, gmb_val_t A, gmb_val_t means, gmb_val_t covs, gmb_val_t ll)
{
    gmb_dict_t d = gmb_dict_new();
    gmb_dict_set(&d, "Pi", pi);
    gmb_dict_set(&d, "A", A);
    gmb_dict_set(&d, "Means", means);
    gmb_dict_set(&d, "Covs", covs);
    gmb_dict_set(&d, "LogLikelihood", ll);
    return gmb_make_val_dict_take(&d);
}

static gmb_val_t gmb_hmm_nan_dict(void)
{
    return gmb_hmm_dict(gmb_hmm_list(NULL, 0), gmb_hmm_list(NULL, 0),
                        gmb_hmm_list(NULL, 0), gmb_hmm_list(NULL, 0),
                        gmb_make_val_double(NAN));
}

gmb_val_t gmb_ts_hmm_fit(const double* X, int n, int d, int K, int n_iter)
{
    double *logE = NULL, *lpi = NULL, *lA = NULL, *al = NULL, *be = NULL, *gam = NULL, *xi = NULL;
    double *pi = NULL, *A = NULL, *means = NULL, *covs = NULL;
    double *gmean = NULL, *gcov = NULL;
    gmb_val_t out;
    int t, i, j, it, k;

    if (!X || n < K || d < 1 || K < 1 || K > 32 || d > 32) return gmb_hmm_nan_dict();
    if (n_iter < 1) n_iter = 100;
    logE = (double*)malloc((size_t)n * K * sizeof(double));
    lpi = (double*)malloc((size_t)K * sizeof(double));
    lA = (double*)malloc((size_t)K * K * sizeof(double));
    al = (double*)malloc((size_t)n * K * sizeof(double));
    be = (double*)malloc((size_t)n * K * sizeof(double));
    gam = (double*)malloc((size_t)n * K * sizeof(double));
    xi = (double*)malloc((size_t)n * K * K * sizeof(double));
    pi = (double*)malloc((size_t)K * sizeof(double));
    A = (double*)malloc((size_t)K * K * sizeof(double));
    means = (double*)calloc((size_t)K * d, sizeof(double));
    covs = (double*)calloc((size_t)K * d * d, sizeof(double));
    gmean = (double*)calloc((size_t)d, sizeof(double));
    gcov = (double*)calloc((size_t)d * d, sizeof(double));
    if (!logE || !lpi || !lA || !al || !be || !gam || !xi || !pi || !A || !means || !covs || !gmean || !gcov) goto fail;

    /* init deterministico: pi/A uniformi, medie a blocchi, cov globale + ridge */
    for (i = 0; i < K; i++) { pi[i] = 1.0 / K; for (j = 0; j < K; j++) A[i*K+j] = 1.0 / K; }
    for (k = 0; k < d; k++) { double s = 0; for (t = 0; t < n; t++) s += X[t*d+k]; gmean[k] = s / n; }
    for (k = 0; k < d; k++)
        for (j = 0; j < d; j++) {
            double s = 0;
            for (t = 0; t < n; t++) s += (X[t*d+k] - gmean[k]) * (X[t*d+j] - gmean[j]);
            gcov[k*d+j] = s / n;
        }
    for (i = 0; i < K; i++) {
        int t0 = (int)((long)i * n / K), t1 = (int)((long)(i+1) * n / K);
        if (t1 <= t0) t1 = t0 + 1;
        if (t1 > n) t1 = n;
        for (k = 0; k < d; k++) {
            double s = 0;
            for (t = t0; t < t1; t++) s += X[t*d+k];
            means[i*d+k] = s / (t1 - t0);
        }
        for (k = 0; k < d * d; k++) covs[i*d*d+k] = gcov[k];
        for (k = 0; k < d; k++) covs[i*d*d + k*d + k] += 1e-6;
    }

    for (t = 0; t < n; t++)
        for (i = 0; i < K; i++)
            logE[t*K+i] = gmb_mv_gauss_logpdf(d, &X[t*d], &means[i*d], &covs[i*d*d]);

    for (it = 0; it < n_iter; it++) {
        double *newpi, *newA, *newmeans, *newcovs;
        for (i = 0; i < K; i++) lpi[i] = log(pi[i] > 0 ? pi[i] : 1e-300);
        for (i = 0; i < K; i++)
            for (j = 0; j < K; j++)
                lA[i*K+j] = log(A[i*K+j] > 0 ? A[i*K+j] : 1e-300);
        for (i = 0; i < K; i++) al[i] = lpi[i] + logE[i];
        for (t = 1; t < n; t++)
            for (j = 0; j < K; j++) {
                double bj = -INFINITY;
                for (i = 0; i < K; i++) bj = gmb_hmm_logadd(bj, al[(t-1)*K+i] + lA[i*K+j]);
                al[t*K+j] = bj + logE[t*K+j];
            }
        for (i = 0; i < K; i++) be[(n-1)*K+i] = 0;
        for (t = n - 2; t >= 0; t--)
            for (i = 0; i < K; i++) {
                double bj = -INFINITY;
                for (j = 0; j < K; j++)
                    bj = gmb_hmm_logadd(bj, lA[i*K+j] + logE[(t+1)*K+j] + be[(t+1)*K+j]);
                be[t*K+i] = bj;
            }
        for (t = 0; t < n; t++) {
            double s = -INFINITY;
            for (i = 0; i < K; i++) s = gmb_hmm_logadd(s, al[t*K+i] + be[t*K+i]);
            for (i = 0; i < K; i++) gam[t*K+i] = exp(al[t*K+i] + be[t*K+i] - s);
        }
        for (t = 0; t < n - 1; t++) {
            double s = -INFINITY;
            for (i = 0; i < K; i++)
                for (j = 0; j < K; j++) {
                    xi[(t*K+i)*K+j] = al[t*K+i] + lA[i*K+j] + logE[(t+1)*K+j] + be[(t+1)*K+j];
                    s = gmb_hmm_logadd(s, xi[(t*K+i)*K+j]);
                }
            for (i = 0; i < K; i++)
                for (j = 0; j < K; j++)
                    xi[(t*K+i)*K+j] = exp(xi[(t*K+i)*K+j] - s);
        }
        newpi = (double*)calloc((size_t)K, sizeof(double));
        newA = (double*)calloc((size_t)K * K, sizeof(double));
        newmeans = (double*)calloc((size_t)K * d, sizeof(double));
        newcovs = (double*)calloc((size_t)K * d * d, sizeof(double));
        if (!newpi || !newA || !newmeans || !newcovs) {
            free(newpi); free(newA); free(newmeans); free(newcovs); goto fail;
        }
        for (i = 0; i < K; i++) {
            double g0 = 0;
            for (t = 0; t < n; t++) g0 += gam[t*K+i];
            newpi[i] = gam[i];
            for (j = 0; j < K; j++) {
                double s = 0;
                for (t = 0; t < n - 1; t++) s += xi[(t*K+i)*K+j];
                newA[i*K+j] = s;
            }
            for (k = 0; k < d; k++) {
                double s = 0;
                for (t = 0; t < n; t++) s += gam[t*K+i] * X[t*d+k];
                newmeans[i*d+k] = g0 > 0 ? s / g0 : 0;
            }
            for (k = 0; k < d*d; k++) {
                double s = 0; int kk = k / d, ll = k % d;
                for (t = 0; t < n; t++)
                    s += gam[t*K+i] * (X[t*d+kk] - newmeans[i*d+kk]) * (X[t*d+ll] - newmeans[i*d+ll]);
                newcovs[i*d*d+k] = g0 > 0 ? s / g0 + 1e-6 : 1e-6;
            }
        }
        { double s = 0;
          for (i = 0; i < K; i++) s += newpi[i];
          for (i = 0; i < K; i++) newpi[i] = s > 0 ? newpi[i] / s : 1.0 / K;
          for (i = 0; i < K; i++) {
              double s2 = 0;
              for (j = 0; j < K; j++) s2 += newA[i*K+j];
              for (j = 0; j < K; j++) newA[i*K+j] = s2 > 0 ? newA[i*K+j] / s2 : 1.0 / K;
          }
        }
        memcpy(pi, newpi, (size_t)K * sizeof(double));
        memcpy(A, newA, (size_t)K * K * sizeof(double));
        memcpy(means, newmeans, (size_t)K * d * sizeof(double));
        memcpy(covs, newcovs, (size_t)K * d * d * sizeof(double));
        for (t = 0; t < n; t++)
            for (i = 0; i < K; i++)
                logE[t*K+i] = gmb_mv_gauss_logpdf(d, &X[t*d], &means[i*d], &covs[i*d*d]);
        free(newpi); free(newA); free(newmeans); free(newcovs);
    }

    for (i = 0; i < K; i++) lpi[i] = log(pi[i] > 0 ? pi[i] : 1e-300);
    for (i = 0; i < K; i++)
        for (j = 0; j < K; j++)
            lA[i*K+j] = log(A[i*K+j] > 0 ? A[i*K+j] : 1e-300);
    out = gmb_hmm_dict(gmb_hmm_list(pi, K), gmb_hmm_list(A, K*K),
                       gmb_hmm_list(means, K*d), gmb_hmm_list(covs, K*d*d),
                       gmb_make_val_double(gmb_ts_hmm_forward(lpi, lA, logE, K, n)));
    free(logE); free(lpi); free(lA); free(al); free(be); free(gam); free(xi);
    free(pi); free(A); free(means); free(covs); free(gmean); free(gcov);
    return out;
fail:
    free(logE); free(lpi); free(lA); free(al); free(be); free(gam); free(xi);
    free(pi); free(A); free(means); free(covs); free(gmean); free(gcov);
    return gmb_hmm_nan_dict();
}

gmb_val_t gmb_ts_hmm_fit_jagged(const double* const* X, int n, int d, int K, int n_iter)
{
    double* flat; gmb_val_t r; int t, k;
    if (!X || n < 1 || d < 1) return gmb_hmm_nan_dict();
    flat = (double*)malloc((size_t)n * d * sizeof(double));
    if (!flat) return gmb_hmm_nan_dict();
    for (t = 0; t < n; t++)
        for (k = 0; k < d; k++) flat[t*d+k] = X[t][k];
    r = gmb_ts_hmm_fit(flat, n, d, K, n_iter);
    free(flat);
    return r;
}

gmb_val_t gmb_ts_hmm_viterbi_list(const double* lpi, const double* lA, const double* lE, int K, int n)
{
    int outn = 0;
    double* p = gmb_ts_hmm_viterbi(lpi, lA, lE, K, n, &outn);
    gmb_val_t r;
    if (!p) return gmb_hmm_list(NULL, 0);
    r = gmb_hmm_list(p, outn);
    free(p);
    return r;
}

/* ==================== KPSS ==================== */
static double gmb_ts_bartlett_lrv(const double* e, int n, int bw)
{
    double total = 0.0;
    int t, lag;
    for (t = 0; t < n; t++) total += e[t] * e[t];
    total /= n;
    for (lag = 1; lag <= bw; lag++) {
        double cov = 0.0;
        for (t = lag; t < n; t++) cov += e[t] * e[t - lag];
        cov /= n;
        total += 2.0 * (1.0 - (double)lag / (bw + 1.0)) * cov;
    }
    return total;
}

gmb_val_t gmb_ts_kpss_flat(const double* series, int n, int bandwidth, int deterministic)
{
    double* e; double partial = 0.0, ss = 0.0, lrv, stat;
    int t;
    gmb_dict_t d;
    if (!series || n <= 1) {
        d = gmb_dict_new();
        gmb_dict_set(&d, "Statistic", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Bandwidth", gmb_make_val_double(0));
        gmb_dict_set(&d, "Observations", gmb_make_val_double(n));
        gmb_dict_set(&d, "Deterministic", gmb_make_val_double(deterministic));
        gmb_dict_set(&d, "LongRunVariance", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Critical10", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Critical5", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Critical25", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Critical1", gmb_make_val_double(NAN));
        return gmb_make_val_dict_take(&d);
    }
    if (bandwidth < 0) bandwidth = (int)floor(4.0 * pow((double)n / 100.0, 0.25));
    if (bandwidth >= n) bandwidth = n - 1;
    e = (double*)malloc((size_t)n * sizeof(double));
    if (!e) return gmb_hmm_nan_dict();
    if (deterministic == 1) {
        double st = 0, sy = 0, stt = 0, sty = 0, b, a;
        for (t = 0; t < n; t++) { double tt = t + 1; st += tt; sy += series[t]; stt += tt * tt; sty += tt * series[t]; }
        b = (n * sty - st * sy) / (n * stt - st * st);
        a = (sy - b * st) / n;
        for (t = 0; t < n; t++) e[t] = series[t] - a - b * (t + 1);
    } else {
        double mean = 0;
        for (t = 0; t < n; t++) mean += series[t];
        mean /= n;
        for (t = 0; t < n; t++) e[t] = series[t] - mean;
    }
    for (t = 0; t < n; t++) { partial += e[t]; ss += partial * partial; }
    lrv = gmb_ts_bartlett_lrv(e, n, bandwidth);
    stat = (lrv > 0.0) ? ss / ((double)n * n * lrv) : NAN;
    free(e);
    d = gmb_dict_new();
    gmb_dict_set(&d, "Statistic", gmb_make_val_double(stat));
    gmb_dict_set(&d, "Bandwidth", gmb_make_val_double(bandwidth));
    gmb_dict_set(&d, "Observations", gmb_make_val_double(n));
    gmb_dict_set(&d, "Deterministic", gmb_make_val_double(deterministic));
    gmb_dict_set(&d, "LongRunVariance", gmb_make_val_double(lrv));
    if (deterministic == 1) {
        gmb_dict_set(&d, "Critical10", gmb_make_val_double(0.119));
        gmb_dict_set(&d, "Critical5", gmb_make_val_double(0.146));
        gmb_dict_set(&d, "Critical25", gmb_make_val_double(0.176));
        gmb_dict_set(&d, "Critical1", gmb_make_val_double(0.216));
    } else {
        gmb_dict_set(&d, "Critical10", gmb_make_val_double(0.347));
        gmb_dict_set(&d, "Critical5", gmb_make_val_double(0.463));
        gmb_dict_set(&d, "Critical25", gmb_make_val_double(0.574));
        gmb_dict_set(&d, "Critical1", gmb_make_val_double(0.739));
    }
    return gmb_make_val_dict_take(&d);
}

/* ==================== QLR ==================== */
gmb_val_t gmb_ts_qlr_flat(const double* l_beta, int n_beta, double l0, double kappa_hat)
{
    int i, best = -1;
    double qlr_t, qlr_tilde;
    gmb_dict_t d;
    if (!l_beta || n_beta <= 0) return gmb_hmm_nan_dict();
    for (i = 0; i < n_beta; i++)
        if (best < 0 || l_beta[i] > l_beta[best]) best = i;
    qlr_t = 2.0 * (l_beta[best] - l0);
    qlr_tilde = (kappa_hat != 0.0) ? qlr_t / kappa_hat : qlr_t;
    d = gmb_dict_new();
    gmb_dict_set(&d, "QLRT", gmb_make_val_double(qlr_t));
    gmb_dict_set(&d, "QLRTilde", gmb_make_val_double(qlr_tilde));
    gmb_dict_set(&d, "BestIndex", gmb_make_val_double(best));
    return gmb_make_val_dict_take(&d);
}

/* ==================== nucleo ADF + OLS (parità gs_infer.c) ==================== */
typedef struct { double statistic, coefficient, standard_error, aic, bic;
                 int lags, observations, deterministic; } gmb_infer_adf_t;

static int gmb_infer_solve(int n, double* A, double* b, double* x)
{
    return gmb_solve_lin(n, A, b, x);
}


static int gmb_infer_ols(int rows, int cols, const double* X, const double* y,
                         double* beta, double* ssr, int leverage_col, double* leverage_out)
{
    double XtX[64*64], XtX0[64*64], Xty[64], z[64], e[64];
    int i, j, k;
    if (rows <= cols || cols > 64) return 0;
    memset(XtX, 0, sizeof(XtX)); memset(Xty, 0, sizeof(Xty));
    for (i = 0; i < rows; i++)
        for (j = 0; j < cols; j++) {
            Xty[j] += X[i*cols+j] * y[i];
            for (k = 0; k < cols; k++) XtX[j*cols+k] += X[i*cols+j] * X[i*cols+k];
        }
    /* la fattorizzazione e' in-place: serve una copia per il secondo solve (leverage). */
    memcpy(XtX0, XtX, sizeof(XtX));
    if (!gmb_infer_solve(cols, XtX, Xty, beta)) return 0;
    *ssr = 0.0;
    for (i = 0; i < rows; i++) {
        double f = 0.0, r;
        for (j = 0; j < cols; j++) f += X[i*cols+j] * beta[j];
        r = y[i] - f; *ssr += r * r;
    }
    if (leverage_col >= 0 && leverage_out) {
        for (j = 0; j < cols; j++) e[j] = (j == leverage_col) ? 1.0 : 0.0;
        memcpy(XtX, XtX0, sizeof(XtX));
        if (!gmb_infer_solve(cols, XtX, e, z)) return 0;
        *leverage_out = z[leverage_col];
    }
    return 1;
}

static int gmb_infer_adf_core(int n, const double* series, int lags, int first_obs,
                              int deterministic, gmb_infer_adf_t* out)
{
    int nd = deterministic, cols = nd + 1 + lags, level_col = nd;
    int rows = n - first_obs, i, row, l;
    double *X, *y, *beta, ssr = 0.0, lev = 0.0;
    if (rows <= cols || first_obs < 1 + lags) return 0;
    X = (double*)calloc((size_t)rows * cols, sizeof(double));
    y = (double*)malloc((size_t)rows * sizeof(double));
    beta = (double*)malloc((size_t)cols * sizeof(double));
    if (!X || !y || !beta) { free(X); free(y); free(beta); return 0; }
    for (row = 0; row < rows; row++) {
        int t = first_obs + row;
        y[row] = series[t] - series[t-1];
        if (nd >= 1) X[row*cols+0] = 1.0;
        if (nd >= 2) X[row*cols+1] = t + 1;
        X[row*cols+level_col] = series[t-1];
        for (l = 1; l <= lags; l++)
            X[row*cols+level_col+l] = series[t-l] - series[t-l-1];
    }
    if (!gmb_infer_ols(rows, cols, X, y, beta, &ssr, level_col, &lev)) { free(X); free(y); free(beta); return 0; }
    {
        double var = ssr / (rows - cols);
        double se = sqrt(var * lev);
        double ms = ssr / rows;
        out->coefficient = beta[level_col];
        out->standard_error = se;
        out->statistic = beta[level_col] / se;
        out->lags = lags;
        out->observations = rows;
        out->deterministic = deterministic;
        out->aic = rows * log(ms) + 2.0 * cols;
        out->bic = rows * log(ms) + cols * log((double)rows);
        (void)i;
    }
    free(X); free(y); free(beta);
    return 1;
}

/* ==================== DF-GLS ==================== */
gmb_val_t gmb_ts_dfgls_flat(const double* series, int n, int lags, int deterministic)
{
    int nd = (deterministic == 1) ? 2 : 1;
    double c_bar = (nd == 2) ? 13.5 : 7.0;
    double alpha_bar = 1.0 - c_bar / n;
    double *qd_det, *qd_ser, *det, *psi, *detrended;
    gmb_infer_adf_t ar;
    gmb_dict_t d;
    int t, j;
    if (n <= nd + lags + 4 || !series) {
        d = gmb_dict_new();
        gmb_dict_set(&d, "Statistic", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Lags", gmb_make_val_double(lags));
        gmb_dict_set(&d, "Observations", gmb_make_val_double(0));
        gmb_dict_set(&d, "Deterministic", gmb_make_val_double(deterministic));
        gmb_dict_set(&d, "AlphaBar", gmb_make_val_double(NAN));
        return gmb_make_val_dict_take(&d);
    }
    qd_det = (double*)malloc((size_t)n * nd * sizeof(double));
    qd_ser = (double*)malloc((size_t)n * sizeof(double));
    det = (double*)malloc((size_t)n * nd * sizeof(double));
    psi = (double*)malloc((size_t)nd * sizeof(double));
    detrended = (double*)malloc((size_t)n * sizeof(double));
    if (!qd_det || !qd_ser || !det || !psi || !detrended) {
        free(qd_det); free(qd_ser); free(det); free(psi); free(detrended);
        return gmb_hmm_nan_dict();
    }
    for (t = 0; t < n; t++) {
        det[t*nd+0] = 1.0;
        if (nd == 2) det[t*nd+1] = t + 1;
    }
    for (t = 0; t < n; t++) {
        for (j = 0; j < nd; j++)
            qd_det[t*nd+j] = (t == 0) ? det[j] : det[t*nd+j] - alpha_bar * det[(t-1)*nd+j];
        qd_ser[t] = (t == 0) ? series[0] : series[t] - alpha_bar * series[t-1];
    }
    {
        double ssr = 0.0, lev = -1;
        if (!gmb_infer_ols(n, nd, qd_det, qd_ser, psi, &ssr, -1, &lev)) {
            free(qd_det); free(qd_ser); free(det); free(psi); free(detrended);
            return gmb_hmm_nan_dict();
        }
    }
    for (t = 0; t < n; t++) {
        double fit = 0.0;
        for (j = 0; j < nd; j++) fit += psi[j] * det[t*nd+j];
        detrended[t] = series[t] - fit;
    }
    if (!gmb_infer_adf_core(n, detrended, lags, 1 + lags, 0, &ar)) {
        free(qd_det); free(qd_ser); free(det); free(psi); free(detrended);
        return gmb_hmm_nan_dict();
    }
    free(qd_det); free(qd_ser); free(det); free(psi); free(detrended);
    d = gmb_dict_new();
    gmb_dict_set(&d, "Statistic", gmb_make_val_double(ar.statistic));
    gmb_dict_set(&d, "Lags", gmb_make_val_double(ar.lags));
    gmb_dict_set(&d, "Observations", gmb_make_val_double(ar.observations));
    gmb_dict_set(&d, "Deterministic", gmb_make_val_double(deterministic));
    gmb_dict_set(&d, "AlphaBar", gmb_make_val_double(alpha_bar));
    return gmb_make_val_dict_take(&d);
}

/* ==================== Engle-Granger ==================== */
gmb_val_t gmb_ts_engle_granger_flat(const double* Y, int n, int k, int dependent,
                                    int lags, int with_constant)
{
    int cols = (k - 1) + (with_constant ? 1 : 0);
    double *X, *resid, *beta, *yc, ssr = 0.0, lev = -1;
    gmb_infer_adf_t ar;
    gmb_dict_t d;
    int i, j, row;
    if (n <= cols + lags + 2 || k < 2 || !Y || dependent < 0 || dependent >= k) {
        d = gmb_dict_new();
        gmb_dict_set(&d, "Statistic", gmb_make_val_double(NAN));
        gmb_dict_set(&d, "Lags", gmb_make_val_double(lags));
        gmb_dict_set(&d, "Observations", gmb_make_val_double(0));
        gmb_dict_set(&d, "NRegressors", gmb_make_val_double(cols));
        gmb_dict_set(&d, "WithConstant", gmb_make_val_double(with_constant));
        return gmb_make_val_dict_take(&d);
    }
    X = (double*)calloc((size_t)n * cols, sizeof(double));
    resid = (double*)malloc((size_t)n * sizeof(double));
    beta = (double*)malloc((size_t)cols * sizeof(double));
    yc = (double*)malloc((size_t)n * sizeof(double));
    if (!X || !resid || !beta || !yc) {
        free(X); free(resid); free(beta); free(yc);
        return gmb_hmm_nan_dict();
    }
    for (i = 0; i < n; i++) {
        int c = 0;
        if (with_constant) X[i*cols+c++] = 1.0;
        for (j = 0; j < k; j++) if (j != dependent) X[i*cols+c++] = Y[i*k+j];
        yc[i] = Y[i*k+dependent];
    }
    if (!gmb_infer_ols(n, cols, X, yc, beta, &ssr, -1, &lev)) {
        free(X); free(resid); free(beta); free(yc);
        return gmb_hmm_nan_dict();
    }
    for (row = 0; row < n; row++) {
        double f = 0.0;
        for (j = 0; j < cols; j++) f += X[row*cols+j] * beta[j];
        resid[row] = Y[row*k+dependent] - f;
    }
    if (!gmb_infer_adf_core(n, resid, lags, 1 + lags, with_constant ? 1 : 0, &ar)) {
        free(X); free(resid); free(beta); free(yc);
        return gmb_hmm_nan_dict();
    }
    free(X); free(resid); free(beta); free(yc);
    d = gmb_dict_new();
    gmb_dict_set(&d, "Statistic", gmb_make_val_double(ar.statistic));
    gmb_dict_set(&d, "Lags", gmb_make_val_double(ar.lags));
    gmb_dict_set(&d, "Observations", gmb_make_val_double(ar.observations));
    gmb_dict_set(&d, "NRegressors", gmb_make_val_double(cols));
    gmb_dict_set(&d, "WithConstant", gmb_make_val_double(with_constant));
    return gmb_make_val_dict_take(&d);
}

/* ==================== Johansen ==================== */
static int gmb_joh_cholesky(int n, const double* A, double* L)
{
    int i, j, k;
    memset(L, 0, (size_t)n * n * sizeof(double));
    for (i = 0; i < n; i++)
        for (j = 0; j <= i; j++) {
            double s = A[i*n+j];
            for (k = 0; k < j; k++) s -= L[i*n+k] * L[j*n+k];
            if (i == j) { if (s <= 0) return 0; L[i*n+j] = sqrt(s); }
            else L[i*n+j] = s / L[j*n+j];
        }
    return 1;
}
static void gmb_joh_lower_solve(int n, int ncol, const double* L, const double* B, double* X)
{
    int i, j, c;
    for (c = 0; c < ncol; c++)
        for (i = 0; i < n; i++) {
            double s = B[i*ncol+c];
            for (j = 0; j < i; j++) s -= L[i*n+j] * X[j*ncol+c];
            X[i*ncol+c] = s / L[i*n+i];
        }
}
/* autovalori di una matrice simmetrica (Jacobi ciclico), ordinati crescenti. */
static void gmb_joh_eig_sym(int n, double* A, double* eval)
{
    int i, j, k, sweep;
    for (sweep = 0; sweep < 100; sweep++) {
        double off = 0.0, diag = 0.0;
        for (i = 0; i < n; i++) {
            diag += A[i*n+i] * A[i*n+i];
            for (j = i + 1; j < n; j++) off += A[i*n+j] * A[i*n+j];
        }
        if (off <= 1e-30 * (diag + 1e-300)) break;
        for (i = 0; i < n; i++)
            for (j = i + 1; j < n; j++) {
                double apq = A[i*n+j];
                if (fabs(apq) < 1e-300) continue;
                double app = A[i*n+i], aqq = A[j*n+j];
                double tau = (aqq - app) / (2.0 * apq);
                double t = (tau >= 0 ? 1.0 : -1.0) / (fabs(tau) + sqrt(1.0 + tau * tau));
                double c = 1.0 / sqrt(1.0 + t * t), sn = t * c;
                for (k = 0; k < n; k++) {
                    double aik = A[i*n+k], ajk = A[j*n+k];
                    A[i*n+k] = c * aik - sn * ajk;
                    A[j*n+k] = sn * aik + c * ajk;
                }
                for (k = 0; k < n; k++) {
                    double aki = A[k*n+i], akj = A[k*n+j];
                    A[k*n+i] = c * aki - sn * akj;
                    A[k*n+j] = sn * aki + c * akj;
                }
                A[i*n+j] = 0.0; A[j*n+i] = 0.0;
            }
    }
    for (i = 0; i < n; i++) eval[i] = A[i*n+i];
    for (i = 1; i < n; i++) {
        double v = eval[i];
        for (j = i - 1; j >= 0 && eval[j] > v; j--) eval[j+1] = eval[j];
        eval[j+1] = v;
    }
}

static void gmb_joh_residualize(int rows, int ncol, int nreg, const double* Y,
                                const double* X, double* R)
{
    int c, i, j;
    double* beta = (double*)malloc((size_t)nreg * sizeof(double));
    double* Xc = (double*)malloc((size_t)rows * nreg * sizeof(double));
    double* yc = (double*)malloc((size_t)rows * sizeof(double));
    if (!beta || !Xc || !yc) { free(beta); free(Xc); free(yc); return; }
    for (c = 0; c < ncol; c++) {
        double ssr = 0.0, lev = -1;
        for (i = 0; i < rows; i++) {
            for (j = 0; j < nreg; j++) Xc[i*nreg+j] = X[i*nreg+j];
            yc[i] = Y[i*ncol+c];
        }
        if (!gmb_infer_ols(rows, nreg, Xc, yc, beta, &ssr, -1, &lev)) { R[c] = 0; continue; }
        for (i = 0; i < rows; i++) {
            double f = 0.0;
            for (j = 0; j < nreg; j++) f += X[i*nreg+j] * beta[j];
            R[i*ncol+c] = Y[i*ncol+c] - f;
        }
    }
    free(beta); free(Xc); free(yc);
}

gmb_val_t gmb_ts_johansen_flat(const double* Y, int n_obs, int k, int lags)
{
    int rows = n_obs - lags - 1, cc = k * lags + 1;
    int i, j, r, row, l, v;
    double *D, *Lv, *COV, *Dr, *Lr, *S00, *S01, *S11, *W, *P, *Lc, *half, *halfT, *sym;
    double *eval, *trace_s, *max_s, *col, *sol;
    gmb_dict_t d;
    if (k < 1 || k > 32 || lags < 0 || !Y || rows <= k + cc) return gmb_hmm_nan_dict();
    D = (double*)calloc((size_t)rows * k, sizeof(double));
    Lv = (double*)calloc((size_t)rows * k, sizeof(double));
    COV = (double*)calloc((size_t)rows * cc, sizeof(double));
    Dr = (double*)calloc((size_t)rows * k, sizeof(double));
    Lr = (double*)calloc((size_t)rows * k, sizeof(double));
    S00 = (double*)malloc((size_t)k * k * sizeof(double));
    S01 = (double*)malloc((size_t)k * k * sizeof(double));
    S11 = (double*)malloc((size_t)k * k * sizeof(double));
    W = (double*)malloc((size_t)k * k * sizeof(double));
    P = (double*)malloc((size_t)k * k * sizeof(double));
    Lc = (double*)malloc((size_t)k * k * sizeof(double));
    half = (double*)malloc((size_t)k * k * sizeof(double));
    halfT = (double*)malloc((size_t)k * k * sizeof(double));
    sym = (double*)malloc((size_t)k * k * sizeof(double));
    eval = (double*)malloc((size_t)k * sizeof(double));
    trace_s = (double*)malloc((size_t)k * sizeof(double));
    max_s = (double*)malloc((size_t)k * sizeof(double));
    if (!D || !Lv || !COV || !Dr || !Lr || !S00 || !S01 || !S11 || !W || !P || !Lc
        || !half || !halfT || !sym || !eval || !trace_s || !max_s) goto fail;

    /* Y e' n_obs x k (osservazioni x variabili). */
    for (row = 0; row < rows; row++) {
        int t = lags + 1 + row;
        for (v = 0; v < k; v++) {
            D[row*k+v] = Y[t*k+v] - Y[(t-1)*k+v];
            Lv[row*k+v] = Y[(t-1)*k+v];
        }
        for (l = 1; l <= lags; l++)
            for (v = 0; v < k; v++)
                COV[row*cc+(l-1)*k+v] = Y[(t-l)*k+v] - Y[(t-l-1)*k+v];
        COV[row*cc+cc-1] = 1.0;
    }
    gmb_joh_residualize(rows, k, cc, D, COV, Dr);
    gmb_joh_residualize(rows, k, cc, Lv, COV, Lr);
    for (i = 0; i < k; i++)
        for (j = 0; j < k; j++) {
            double s00 = 0, s01 = 0, s11 = 0;
            for (row = 0; row < rows; row++) {
                s00 += Dr[row*k+i] * Dr[row*k+j];
                s01 += Dr[row*k+i] * Lr[row*k+j];
                s11 += Lr[row*k+i] * Lr[row*k+j];
            }
            S00[i*k+j] = s00 / rows;
            S01[i*k+j] = s01 / rows;
            S11[i*k+j] = s11 / rows;
        }
    col = (double*)malloc((size_t)k * sizeof(double));
    sol = (double*)malloc((size_t)k * sizeof(double));
    if (!col || !sol) { free(col); free(sol); goto fail; }
    for (j = 0; j < k; j++) {
        double cm[64*64];
        memcpy(cm, S00, (size_t)k * k * sizeof(double));
        for (i = 0; i < k; i++) col[i] = S01[i*k+j];
        if (!gmb_infer_solve(k, cm, col, sol)) { free(col); free(sol); goto fail; }
        for (i = 0; i < k; i++) W[i*k+j] = sol[i];
    }
    free(col); free(sol);
    for (i = 0; i < k; i++)
        for (j = 0; j < k; j++) {
            double s = 0.0;
            for (v = 0; v < k; v++) s += S01[v*k+i] * W[v*k+j];
            P[i*k+j] = s;
        }
    memcpy(Lc, S11, (size_t)k * k * sizeof(double));
    if (!gmb_joh_cholesky(k, S11, Lc)) goto fail;
    gmb_joh_lower_solve(k, k, Lc, P, half);
    for (i = 0; i < k; i++) for (j = 0; j < k; j++) halfT[i*k+j] = half[j*k+i];
    gmb_joh_lower_solve(k, k, Lc, halfT, sym);
    gmb_joh_eig_sym(k, sym, eval);
    for (i = 0; i < k; i++) {
        double lam = eval[i];
        if (lam < 0.0) lam = 0.0;
        if (lam > 1.0 - 1e-15) lam = 1.0 - 1e-15;
        eval[i] = lam;
    }
    for (r = 0; r < k; r++) {
        double tot = 0.0;
        for (i = r; i < k; i++) tot += -log(1.0 - eval[i]);
        trace_s[r] = rows * tot;
        max_s[r] = rows * (-log(1.0 - eval[r]));
    }
    d = gmb_dict_new();
    gmb_dict_set(&d, "EigenValues", gmb_hmm_list(eval, k));
    gmb_dict_set(&d, "TraceStatistic", gmb_hmm_list(trace_s, k));
    gmb_dict_set(&d, "MaxStatistic", gmb_hmm_list(max_s, k));
    gmb_dict_set(&d, "Observations", gmb_make_val_double(rows));
    gmb_dict_set(&d, "Lags", gmb_make_val_double(lags));
    free(D); free(Lv); free(COV); free(Dr); free(Lr); free(S00); free(S01); free(S11);
    free(W); free(P); free(Lc); free(half); free(halfT); free(sym); free(eval);
    free(trace_s); free(max_s);
    return gmb_make_val_dict_take(&d);
fail:
    free(D); free(Lv); free(COV); free(Dr); free(Lr); free(S00); free(S01); free(S11);
    free(W); free(P); free(Lc); free(half); free(halfT); free(sym); free(eval);
    free(trace_s); free(max_s);
    return gmb_hmm_nan_dict();
}

/* OUTLIERS metodo IQR (CommonLib.Numerics.Outliers, method="IQR"): Q1=sorted[n/4],
 * Q3=sorted[3n/4], fuori da [Q1-th*IQR, Q3+th*IQR] → indice (come double, come il metodo Z). */
gmb_double_t* gmb_ts_outliers_iqr(gmb_double_t* data, gmb_int_t n, double threshold, gmb_int_t* out_len) {
    *out_len = 0;
    if (!data || n < 3) return NULL;
    double* s = (double*)malloc((size_t)n * sizeof(double));
    if (!s) return NULL;
    for (gmb_int_t i = 0; i < n; i++) s[i] = data[i];
    qsort(s, (size_t)n, sizeof(double), gmb_dcmp_dbl);
    double q1 = s[n / 4];
    double q3 = s[(3 * n) / 4];
    double iqr = q3 - q1;
    double low = q1 - threshold * iqr;
    double high = q3 + threshold * iqr;
    free(s);
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)n, sizeof(gmb_double_t));
    if (!out) return NULL;
    gmb_int_t cnt = 0;
    for (gmb_int_t i = 0; i < n; i++)
        if (data[i] < low || data[i] > high) out[cnt++] = (double)i;
    *out_len = cnt;
    return out;
}

/* ROLLINGDELTA con ORDINE (CommonLib.Numerics.RollingDelta): differenza ultima-prima della
 * finestra per order=1; per order>1 differenzia ripetutamente la copia della finestra e prende
 * slice[window-order]. Il kernel storico gmb_ts_rolldelta copre solo order=1. */
gmb_double_t* gmb_ts_rolldelta_order(gmb_double_t* series, gmb_int_t n, gmb_int_t window,
                                     gmb_int_t order, gmb_int_t* out_len) {
    *out_len = 0;
    if (!series || window < 2 || order < 1 || order > window) {
        ts_err("Dati, finestra o ordine non validi.");
        return NULL;
    }
    gmb_int_t len = n - window + 1;
    if (len <= 0) return NULL;
    gmb_double_t* out = (gmb_double_t*)calloc((size_t)len, sizeof(gmb_double_t));
    double* slice = (double*)malloc((size_t)window * sizeof(double));
    if (!out || !slice) { free(out); free(slice); return NULL; }
    for (gmb_int_t i = 0; i < len; i++) {
        for (gmb_int_t j = 0; j < window; j++) slice[j] = series[i + j];
        double value = slice[window - 1] - slice[0];
        for (gmb_int_t o = 1; o < order; o++) {
            for (gmb_int_t j = 1; j < window; j++) slice[j - 1] = slice[j] - slice[j - 1];
            value = slice[window - order];
        }
        out[i] = value;
    }
    free(slice);
    *out_len = len;
    return out;
}
