/*
 * ofx_roll.c — shim FFI per la famiglia rolling/trasformazioni di TIMESERIES.
 *
 * Semantica allineata al builtin storico (CommonLib/Numerics/TimeSeries.cs):
 * le statistiche mobili restituiscono n-w+1 valori (finestra completa), non n
 * con prefisso NaN. I buffer out sono allocati dal wrapper BASIC.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RXF_NAN (0.0 / 0.0)

static int cmp_real(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static double median_copy(const double *src, int n)
{
    double *b = (double *)malloc((size_t)n * sizeof(double));
    double v;
    int mid;
    if (!b) return RXF_NAN;
    memcpy(b, src, (size_t)n * sizeof(double));
    qsort(b, (size_t)n, sizeof(double), cmp_real);
    mid = n / 2;
    v = (n % 2) ? b[mid] : 0.5 * (b[mid - 1] + b[mid]);
    free(b);
    return v;
}

static double win_mean(const double *x, int w)
{
    double s = 0.0;
    int i;
    for (i = 0; i < w; i++) s += x[i];
    return s / (double)w;
}

static double win_var_pop(const double *x, int w)
{
    double m = win_mean(x, w), s = 0.0;
    int i;
    for (i = 0; i < w; i++) { double d = x[i] - m; s += d * d; }
    return s / (double)w;
}

/* ── trasformazioni ─────────────────────────────────────────────────────── */

/* Media mobile (TSMA/TSROLLMEAN): out(n-w+1). */
void ofx_ts_ma(const double *x, int n, int w, double *out)
{
    double s = 0.0;
    int i, k;
    if (!x || !out || w < 1 || n < w) return;
    for (i = 0; i < w; i++) s += x[i];
    out[0] = s / (double)w;
    for (i = w, k = 1; i < n; i++, k++) {
        s += x[i] - x[i - w];
        out[k] = s / (double)w;
    }
}

/* EMA con periodo (alpha = 2/(period+1)): out(n). */
void ofx_ts_ema(const double *x, int n, int period, double *out)
{
    double alpha;
    int i;
    if (!x || !out || n < 1 || period < 1) return;
    alpha = 2.0 / ((double)period + 1.0);
    out[0] = x[0];
    for (i = 1; i < n; i++) out[i] = alpha * x[i] + (1.0 - alpha) * out[i - 1];
}

/* TSMINMAX: out(2) = {min, max}. */
void ofx_ts_minmax(const double *x, int n, double *out)
{
    double mn, mx;
    int i;
    if (!x || !out || n < 1) return;
    mn = mx = x[0];
    for (i = 1; i < n; i++) { if (x[i] < mn) mn = x[i]; if (x[i] > mx) mx = x[i]; }
    out[0] = mn;
    out[1] = mx;
}

/* Autocorrelazione a un lag (scalare). */
double ofx_ts_autocorr(const double *x, int n, int lag)
{
    double m = 0.0, num = 0.0, den = 0.0;
    int i;
    if (!x || n < 2 || lag < 1 || lag >= n) return RXF_NAN;
    for (i = 0; i < n; i++) m += x[i];
    m /= (double)n;
    for (i = 0; i < n; i++) den += (x[i] - m) * (x[i] - m);
    for (i = lag; i < n; i++) num += (x[i] - m) * (x[i - lag] - m);
    return den != 0.0 ? num / den : 0.0;
}

/* Volatilità storica: std di popolazione dei rendimenti semplici. */
double ofx_ts_volatility(const double *x, int n)
{
    double m = 0.0, var = 0.0;
    int i;
    if (!x || n < 2) return 0.0;
    for (i = 1; i < n; i++) {
        double r = (x[i] - x[i - 1]) / x[i - 1];
        m += r;
    }
    m /= (double)(n - 1);
    for (i = 1; i < n; i++) {
        double r = (x[i] - x[i - 1]) / x[i - 1];
        var += (r - m) * (r - m);
    }
    return sqrt(var / (double)(n - 1));
}

/* Differenziazione di ordine `order`: out(n-order), 0 se resta < 2 elementi. */
int ofx_ts_difference(const double *x, int n, int order, double *out)
{
    double *cur, *nxt;
    int len, k, i;
    if (!x || !out || n < 1 || order < 1) return 0;
    len = n;
    cur = (double *)malloc((size_t)n * sizeof(double));
    nxt = (double *)malloc((size_t)n * sizeof(double));
    if (!cur || !nxt) { free(cur); free(nxt); return 0; }
    memcpy(cur, x, (size_t)n * sizeof(double));
    for (k = 0; k < order; k++) {
        if (len < 2) { len = 0; break; }
        for (i = 1; i < len; i++) nxt[i - 1] = cur[i] - cur[i - 1];
        len--;
        memcpy(cur, nxt, (size_t)len * sizeof(double));
    }
    for (i = 0; i < len; i++) out[i] = cur[i];
    free(cur);
    free(nxt);
    return len;
}

/* Rendimenti semplici: out(n-1). */
void ofx_ts_returns(const double *x, int n, double *out)
{
    int i;
    if (!x || !out || n < 2) return;
    for (i = 1; i < n; i++) out[i - 1] = (x[i] - x[i - 1]) / x[i - 1];
}

/* Destagionalizzazione (media centrata su +/- period/2): out(n). */
void ofx_ts_deseasonalize(const double *x, int n, int period, double *out)
{
    int half, i, j;
    if (!x || !out || n < 1 || period < 2) return;
    half = period / 2;
    for (i = 0; i < n; i++) {
        int left = i - half < 0 ? 0 : i - half;
        int right = i + half > n - 1 ? n - 1 : i + half;
        double s = 0.0;
        int cnt = 0;
        for (j = left; j <= right; j++) { s += x[j]; cnt++; }
        out[i] = x[i] - s / (double)cnt;
    }
}

/* Decomposizione additiva: trend/seasonal/resid, n elementi ciascuno. */
void ofx_ts_decompose(const double *x, int n, int period,
                      double *trend, double *seasonal, double *resid)
{
    double *smean;
    int *scount;
    int half, i, j;
    if (!x || !trend || !seasonal || !resid || n < 1 || period < 2) return;
    half = period / 2;
    for (i = 0; i < n; i++) {
        int left = i - half < 0 ? 0 : i - half;
        int right = i + half > n - 1 ? n - 1 : i + half;
        double s = 0.0;
        int cnt = 0;
        for (j = left; j <= right; j++) { s += x[j]; cnt++; }
        trend[i] = s / (double)cnt;
    }
    smean = (double *)calloc((size_t)period, sizeof(double));
    scount = (int *)calloc((size_t)period, sizeof(int));
    if (!smean || !scount) { free(smean); free(scount); return; }
    for (i = 0; i < n; i++) {
        int idx = i % period;
        smean[idx] += x[i] - trend[i];
        scount[idx]++;
    }
    for (i = 0; i < period; i++) if (scount[i] > 0) smean[i] /= (double)scount[i];
    for (i = 0; i < n; i++) seasonal[i] = smean[i % period];
    for (i = 0; i < n; i++) resid[i] = x[i] - trend[i] - seasonal[i];
    free(smean);
    free(scount);
}

/* Matrice di covarianza campionaria: data nobs×m row-major, out m×m. */
void ofx_ts_covmatrix(const double *data, int nobs, int m, double *out)
{
    double *mean;
    int a, b, i;
    if (!data || !out || nobs < 2 || m < 1) return;
    mean = (double *)calloc((size_t)m, sizeof(double));
    if (!mean) return;
    for (i = 0; i < nobs; i++)
        for (a = 0; a < m; a++) mean[a] += data[i * m + a];
    for (a = 0; a < m; a++) mean[a] /= (double)nobs;
    for (a = 0; a < m; a++)
        for (b = 0; b < m; b++) {
            double s = 0.0;
            for (i = 0; i < nobs; i++)
                s += (data[i * m + a] - mean[a]) * (data[i * m + b] - mean[b]);
            out[a * m + b] = s / (double)(nobs - 1);
        }
    free(mean);
}

/* Outlier Z-score: out(indici) LONG, ritorna il numero scritto. */
int ofx_ts_outliers(const double *x, int n, double threshold, long long *out)
{
    double mean = 0.0, sd = 0.0;
    int i, cnt = 0;
    if (!x || !out || n < 3) return 0;
    for (i = 0; i < n; i++) mean += x[i];
    mean /= (double)n;
    for (i = 0; i < n; i++) sd += (x[i] - mean) * (x[i] - mean);
    sd = sqrt(sd / (double)n);
    for (i = 0; i < n; i++) {
        double z = sd > 0.0 ? fabs((x[i] - mean) / sd) : 0.0;
        if (z > threshold) out[cnt++] = (long long)i;
    }
    return cnt;
}

/* ── statistiche rolling (out n-w+1) ────────────────────────────────────── */

void ofx_roll_mean(const double *x, int n, int w, double *out)
{
    ofx_ts_ma(x, n, w, out);
}

void ofx_roll_std(const double *x, int n, int w, double *out)
{
    int i, k;
    if (!x || !out || w < 1 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) out[k] = sqrt(win_var_pop(x + i, w));
}

void ofx_roll_skew(const double *x, int n, int w, double *out)
{
    int i, k;
    if (!x || !out || w < 3 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double m = win_mean(x + i, w), sum2 = 0.0, sum3 = 0.0, sd;
        int j;
        for (j = 0; j < w; j++) {
            double d = x[i + j] - m;
            sum2 += d * d;
            sum3 += d * d * d;
        }
        sd = sqrt(sum2 / (double)(w - 1));
        if (sd < 1e-12) { out[k] = 0.0; continue; }
        out[k] = ((double)w / ((double)(w - 1) * (double)(w - 2))) * (sum3 / (sd * sd * sd));
    }
}

void ofx_roll_kurt(const double *x, int n, int w, double *out)
{
    int i, k;
    if (!x || !out || w < 4 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double m = win_mean(x + i, w), sum2 = 0.0, sum4 = 0.0, var2;
        double dn = (double)w;
        int j;
        for (j = 0; j < w; j++) {
            double d = x[i + j] - m, d2 = d * d;
            sum2 += d2;
            sum4 += d2 * d2;
        }
        var2 = sum2 / (dn - 1.0);
        if (var2 < 1e-12) { out[k] = 0.0; continue; }
        out[k] = (dn * (dn + 1.0) * sum4) / ((dn - 1.0) * (dn - 2.0) * (dn - 3.0) * var2 * var2)
                 - (3.0 * (dn - 1.0) * (dn - 1.0)) / ((dn - 2.0) * (dn - 3.0));
    }
}

void ofx_roll_median(const double *x, int n, int w, double *out)
{
    int i, k;
    if (!x || !out || w < 1 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) out[k] = median_copy(x + i, w);
}

void ofx_roll_quantile(const double *x, int n, int w, double q, double *out)
{
    int i, k;
    if (!x || !out || w < 1 || n < w || q < 0.0 || q > 1.0) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double *b = (double *)malloc((size_t)w * sizeof(double));
        double idx, low, high;
        int il, ih;
        if (!b) return;
        memcpy(b, x + i, (size_t)w * sizeof(double));
        qsort(b, (size_t)w, sizeof(double), cmp_real);
        idx = q * (double)(w - 1);
        il = (int)floor(idx);
        ih = (int)ceil(idx);
        low = b[il];
        high = b[ih];
        out[k] = (il == ih) ? low : low + (high - low) * (idx - (double)il);
        free(b);
    }
}

void ofx_roll_sharpe(const double *x, int n, int w, double rf, double *out)
{
    int i, k;
    if (!x || !out || w < 2 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double m = win_mean(x + i, w) - rf;
        double sd = sqrt(win_var_pop(x + i, w));
        out[k] = sd != 0.0 ? m / sd : 0.0;
    }
}

void ofx_roll_mad(const double *x, int n, int w, double *out)
{
    int i, k;
    if (!x || !out || w < 1 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double med = median_copy(x + i, w);
        double *dev = (double *)malloc((size_t)w * sizeof(double));
        int j;
        if (!dev) return;
        for (j = 0; j < w; j++) dev[j] = fabs(x[i + j] - med);
        out[k] = median_copy(dev, w);
        free(dev);
    }
}

void ofx_roll_zscore(const double *x, int n, int w, double *out)
{
    int i, k;
    if (!x || !out || w < 2 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double m = win_mean(x + i, w);
        double sd = sqrt(win_var_pop(x + i, w));
        out[k] = sd < 1e-12 ? 0.0 : (x[i + w - 1] - m) / sd;
    }
}

void ofx_roll_autocorr(const double *x, int n, int w, int lag, double *out)
{
    int i, k;
    if (!x || !out || w < lag + 1 || lag < 1 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double m = win_mean(x + i, w), num = 0.0, den = 0.0;
        int j;
        for (j = 0; j < w; j++) den += (x[i + j] - m) * (x[i + j] - m);
        for (j = lag; j < w; j++) num += (x[i + j] - m) * (x[i + j - lag] - m);
        out[k] = den != 0.0 ? num / den : 0.0;
    }
}

void ofx_roll_cov(const double *x, const double *y, int n, int w, double *out)
{
    int i, k, j;
    if (!x || !y || !out || w < 2 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double mx = win_mean(x + i, w), my = win_mean(y + i, w), s = 0.0;
        for (j = 0; j < w; j++) s += (x[i + j] - mx) * (y[i + j] - my);
        out[k] = s / (double)(w - 1);
    }
}

void ofx_roll_corr(const double *x, const double *y, int n, int w, double *out)
{
    int i, k, j;
    if (!x || !y || !out || w < 2 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double mx = win_mean(x + i, w), my = win_mean(y + i, w);
        double sxy = 0.0, sxx = 0.0, syy = 0.0;
        for (j = 0; j < w; j++) {
            double dx = x[i + j] - mx, dy = y[i + j] - my;
            sxy += dx * dy;
            sxx += dx * dx;
            syy += dy * dy;
        }
        out[k] = (sxx > 0.0 && syy > 0.0) ? sxy / sqrt(sxx * syy) : 0.0;
    }
}

/* Regressione mobile su due serie: slope[] e intercept[] (n-w+1). */
void ofx_roll_regression(const double *x, const double *y, int n, int w,
                         double *slope, double *intercept)
{
    int i, k, j;
    if (!x || !y || !slope || !intercept || w < 2 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0, denom;
        for (j = 0; j < w; j++) {
            double xv = x[i + j], yv = y[i + j];
            sx += xv; sy += yv; sxx += xv * xv; sxy += xv * yv;
        }
        denom = (double)w * sxx - sx * sx;
        slope[k] = denom == 0.0 ? 0.0 : ((double)w * sxy - sx * sy) / denom;
        intercept[k] = ((double)w == 0.0) ? 0.0 : (sy - slope[k] * sx) / (double)w;
    }
}

/* Regressione mobile su indice 0..w-1: slope[] e intercept[]. */
void ofx_roll_regression_single(const double *y, int n, int w,
                                double *slope, double *intercept)
{
    int i, k, j;
    if (!y || !slope || !intercept || w < 2 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0, denom;
        for (j = 0; j < w; j++) {
            double v = y[i + j];
            sx += (double)j; sy += v;
            sxx += (double)(j * j); sxy += (double)j * v;
        }
        denom = (double)w * sxx - sx * sx;
        slope[k] = denom == 0.0 ? 0.0 : ((double)w * sxy - sx * sy) / denom;
        intercept[k] = (sy - slope[k] * sx) / (double)w;
    }
}

/* Delta mobile (order=1): out(n-w+1) = x[i+w-1]-x[i]. */
void ofx_roll_delta(const double *x, int n, int w, double *out)
{
    int i, k;
    if (!x || !out || w < 2 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) out[k] = x[i + w - 1] - x[i];
}

void ofx_roll_sortino(const double *x, int n, int w, double rf, double target, double *out)
{
    int i, k;
    if (!x || !out || w < 2 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double m = win_mean(x + i, w) - rf;
        double s = 0.0;
        int j, cnt = 0;
        for (j = 0; j < w; j++)
            if (x[i + j] < target) { double d = x[i + j] - target; s += d * d; cnt++; }
        /* Il builtin restituisce 0 quando non ci sono osservazioni sotto target. */
        out[k] = (cnt > 0 && s > 0.0) ? m / sqrt(s / (double)cnt) : 0.0;
    }
}

/* Minimo/massimo mobili: min[] e max[] (n-w+1). */
void ofx_roll_minmax(const double *x, int n, int w, double *mn, double *mx)
{
    int i, k, j;
    if (!x || !mn || !mx || w < 1 || n < w) return;
    for (i = 0, k = 0; i + w <= n; i++, k++) {
        double lo = x[i], hi = x[i];
        for (j = 1; j < w; j++) {
            double v = x[i + j];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        mn[k] = lo;
        mx[k] = hi;
    }
}

/* ── filtri Butterworth (gm_signal) ─────────────────────────────────────── */

#include "gm_signal.h"

static int apply_filter(long h, const double *x, int n, double *out)
{
    int olen = 0;
    gm_real *y;
    if (!x || !out || n < 1 || h <= 0) return 0;
    y = gm_signal_lfilter(h, x, n, &olen);
    if (!y) return 0;
    if (olen >= n) memcpy(out, y, (size_t)n * sizeof(double));
    free(y);
    return olen >= n;
}

int ofx_ts_lowpass(const double *x, int n, double wn, int order, double *out)
{
    long h = gm_signal_butter(order, &wn, 1, "low");
    return apply_filter(h, x, n, out);
}

int ofx_ts_highpass(const double *x, int n, double wn, int order, double *out)
{
    long h = gm_signal_butter(order, &wn, 1, "high");
    return apply_filter(h, x, n, out);
}

int ofx_ts_bandpass(const double *x, int n, double w1, double w2, int order, double *out)
{
    double wn[2];
    long h;
    wn[0] = w1;
    wn[1] = w2;
    h = gm_signal_butter(order, wn, 2, "bandpass");
    return apply_filter(h, x, n, out);
}

int ofx_ts_bandstop(const double *x, int n, double w1, double w2, int order, double *out)
{
    double wn[2];
    long h;
    wn[0] = w1;
    wn[1] = w2;
    h = gm_signal_butter(order, wn, 2, "bandstop");
    return apply_filter(h, x, n, out);
}
