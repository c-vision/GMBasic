/*
 * gs_rolling.c — statistiche rolling e time series.
 */
#include "gs_rolling.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int cmp_real(const void *a, const void *b) { gc_real x = *(const gc_real *)a, y = *(const gc_real *)b; return x < y ? -1 : (x > y ? 1 : 0); }

static gc_real win_mean(const gc_real *x, int w) { gc_real s = 0; int i; for (i = 0; i < w; i++) s += x[i]; return s / w; }
static gc_real win_var(const gc_real *x, int w) {
    gc_real m = win_mean(x, w), s = 0; int i;
    for (i = 0; i < w; i++) { gc_real d = x[i] - m; s += d * d; }
    return s / w;
}

void gs_rolling_mean(const gc_real *x, int n, int w, gc_real *out)
{
    gc_real s = 0.0; int i;
    if (!x || !out || n < 1 || w < 1) return;
    for (i = 0; i < n; i++) {
        s += x[i];
        if (i >= w) s -= x[i - w];
        out[i] = (i >= w - 1) ? s / (gc_real)w : NAN;
    }
}
void gs_rolling_sum(const gc_real *x, int n, int w, gc_real *out)
{
    gc_real s = 0.0; int i;
    if (!x || !out || n < 1 || w < 1) return;
    for (i = 0; i < n; i++) {
        s += x[i];
        if (i >= w) s -= x[i - w];
        out[i] = (i >= w - 1) ? s : NAN;
    }
}
/* Kernel momenti streaming (da bagol1000/fastwindow): somme deviate da un
 * anchor K con re-anchor periodico (REINIT=4096) per stabilita' numerica.
 * emette mean/var(ddof0)/skew(Fisher)/kurt(eccesso corretto); puntatori
 * opzionali. */
#define MOM_REINIT 4096
static void moments_stream(const gc_real *x, int n, int w,
                           gc_real *mean, gc_real *var,
                           gc_real *skew, gc_real *kurt)
{
    gc_real K = 0.0, s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0;
    int i, reinit = 0;
    if (!x || n < 1 || w < 1) return;
    for (i = 0; i < n; i++) {
        gc_real d, d2;
        if (i >= w) { d = x[i - w] - K; d2 = d * d; s1 -= d; s2 -= d2; s3 -= d2 * d; s4 -= d2 * d2; }
        d = x[i] - K; d2 = d * d;
        s1 += d; s2 += d2; s3 += d2 * d; s4 += d2 * d2;
        if (++reinit >= MOM_REINIT) {
            int lo = (i >= w - 1) ? i - w + 1 : 0, hi = i, j;
            gc_real s = 0.0;
            reinit = 0;
            for (j = lo; j <= hi; j++) s += x[j];
            K = s / (gc_real)(hi - lo + 1);
            s1 = s2 = s3 = s4 = 0.0;
            for (j = lo; j <= hi; j++) { d = x[j] - K; d2 = d * d; s1 += d; s2 += d2; s3 += d2 * d; s4 += d2 * d2; }
        }
        if (i < w - 1) { if (mean) mean[i] = NAN; if (var) var[i] = NAN; if (skew) skew[i] = NAN; if (kurt) kurt[i] = NAN; continue; }
        {
            gc_real dm = (gc_real)w, m = K + s1 / dm;
            gc_real M2 = s2 - dm * m * m;
            if (mean) mean[i] = m;
            if (var) var[i] = M2 > 0.0 ? M2 / dm : 0.0;
            if (skew || kurt) {
                gc_real M3 = s3 - 3.0 * m * s2 + 2.0 * dm * m * m * m;
                gc_real M4 = s4 - 4.0 * m * s3 + 6.0 * m * m * s2 - 3.0 * dm * m * m * m * m;
                if (skew) {
                    gc_real g1 = M3 / dm / pow(M2 / dm, 1.5);
                    skew[i] = (M2 > 0.0 && dm > 2.0) ? sqrt(dm * (dm - 1.0)) / (dm - 2.0) * g1 : NAN;
                }
                if (kurt) {
                    gc_real g2 = dm * M4 / (M2 * M2) - 3.0;
                    kurt[i] = (M2 > 0.0 && dm > 3.0) ? (dm - 1.0) / ((dm - 2.0) * (dm - 3.0)) * ((dm + 1.0) * g2 + 6.0) : NAN;
                }
            }
        }
    }
}

void gs_rolling_var(const gc_real *x, int n, int w, gc_real *out)
{
    if (!out) return;
    moments_stream(x, n, w, NULL, out, NULL, NULL);
}
void gs_rolling_std(const gc_real *x, int n, int w, gc_real *out)
{
    int i;
    if (!out) return;
    moments_stream(x, n, w, NULL, out, NULL, NULL);
    for (i = 0; i < n; i++) if (!isnan(out[i])) out[i] = sqrt(out[i]);
}
/* Monotonic deque O(1)/passo per min/max (da bagol1000/fastwindow). */
static void rolling_extreme(const gc_real *x, int n, int w, int is_min, gc_real *out)
{
    int *dq, head = 0, tail = 0, i;
    if (!x || !out || n < 1 || w < 1) return;
    dq = (int *)malloc((size_t)n * sizeof(int));
    if (!dq) return;
    for (i = 0; i < n; i++) {
        while (head < tail && (is_min ? x[dq[tail - 1]] >= x[i] : x[dq[tail - 1]] <= x[i])) tail--;
        dq[tail++] = i;
        while (dq[head] <= i - w) head++;
        out[i] = (i >= w - 1) ? x[dq[head]] : NAN;
    }
    free(dq);
}
void gs_rolling_min(const gc_real *x, int n, int w, gc_real *out) { rolling_extreme(x, n, w, 1, out); }
void gs_rolling_max(const gc_real *x, int n, int w, gc_real *out) { rolling_extreme(x, n, w, 0, out); }
void gs_rolling_median(const gc_real *x, int n, int w, gc_real *out)
{
    gc_real *buf; int i;
    if (!x || !out || n < 1 || w < 1) return;
    buf = (gc_real *)malloc((size_t)w * sizeof(gc_real));
    if (!buf) return;
    for (i = 0; i < n; i++) {
        if (i >= w - 1) { memcpy(buf, x + i - w + 1, (size_t)w * sizeof(gc_real)); qsort(buf, (size_t)w, sizeof(gc_real), cmp_real); out[i] = (w % 2) ? buf[w / 2] : 0.5 * (buf[w / 2 - 1] + buf[w / 2]); }
        else out[i] = NAN;
    }
    free(buf);
}
void gs_rolling_cov(const gc_real *x, const gc_real *y, int n, int w, gc_real *out)
{
    int i, j; if (!x || !y || !out || n < 1 || w < 1) return;
    for (i = 0; i < n; i++) {
        if (i >= w - 1) {
            gc_real mx = win_mean(x + i - w + 1, w), my = win_mean(y + i - w + 1, w), s = 0;
            for (j = i - w + 1; j <= i; j++) s += (x[j] - mx) * (y[j] - my);
            out[i] = s / w;
        } else out[i] = NAN;
    }
}
void gs_rolling_corr(const gc_real *x, const gc_real *y, int n, int w, gc_real *out)
{
    int i, j; if (!x || !y || !out || n < 1 || w < 1) return;
    for (i = 0; i < n; i++) {
        if (i >= w - 1) {
            gc_real mx = win_mean(x + i - w + 1, w), my = win_mean(y + i - w + 1, w);
            gc_real sxy = 0, sxx = 0, syy = 0;
            for (j = i - w + 1; j <= i; j++) { gc_real dx = x[j] - mx, dy = y[j] - my; sxy += dx * dy; sxx += dx * dx; syy += dy * dy; }
            out[i] = (sxx > 0 && syy > 0) ? sxy / sqrt(sxx * syy) : NAN;
        } else out[i] = NAN;
    }
}
void gs_rolling_hampel(const gc_real *x, int n, int w, gc_real k,
                       gc_real *out, int *flag)
{
    gc_real *buf;
    int i, j;
    if (!x || !out || n < 1 || w < 1) return;
    buf = (gc_real *)malloc((size_t)w * sizeof(gc_real));
    if (!buf) return;
    for (i = 0; i < n; i++) {
        gc_real med, mad;
        if (i < w - 1) { out[i] = x[i]; if (flag) flag[i] = 0; continue; }
        memcpy(buf, x + i - w + 1, (size_t)w * sizeof(gc_real));
        qsort(buf, (size_t)w, sizeof(gc_real), cmp_real);
        med = (w % 2) ? buf[w / 2] : 0.5 * (buf[w / 2 - 1] + buf[w / 2]);
        for (j = 0; j < w; j++) buf[j] = fabs(x[i - w + 1 + j] - med);
        qsort(buf, (size_t)w, sizeof(gc_real), cmp_real);
        mad = (w % 2) ? buf[w / 2] : 0.5 * (buf[w / 2 - 1] + buf[w / 2]);
        if (mad > 0.0 && fabs(x[i] - med) > k * 1.4826 * mad) {
            out[i] = med; if (flag) flag[i] = 1;
        } else {
            out[i] = x[i]; if (flag) flag[i] = 0;
        }
    }
    free(buf);
}

void gs_rolling_zscore(const gc_real *x, int n, int w, gc_real *out)
{
    int i; if (!x || !out || n < 1 || w < 1) return;
    for (i = 0; i < n; i++) {
        if (i >= w - 1) { gc_real m = win_mean(x + i - w + 1, w), v = win_var(x + i - w + 1, w); out[i] = v > 0 ? (x[i] - m) / sqrt(v) : 0.0; }
        else out[i] = NAN;
    }
}
void gs_rolling_sharpe(const gc_real *x, int n, int w, gc_real *out)
{
    int i; if (!x || !out || n < 1 || w < 1) return;
    for (i = 0; i < n; i++) {
        if (i >= w - 1) { gc_real m = win_mean(x + i - w + 1, w), s = sqrt(win_var(x + i - w + 1, w)); out[i] = s > 0 ? m / s : 0.0; }
        else out[i] = NAN;
    }
}
void gs_ema(const gc_real *x, int n, gc_real alpha, gc_real *out)
{
    int i; if (!x || !out || n < 1) return;
    out[0] = x[0];
    for (i = 1; i < n; i++) out[i] = alpha * x[i] + (1.0 - alpha) * out[i - 1];
}
void gs_returns(const gc_real *x, int n, int log_ret, gc_real *out)
{
    int i; if (!x || !out || n < 1) return;
    out[0] = 0.0;
    for (i = 1; i < n; i++) {
        if (x[i - 1] == 0) out[i] = 0.0;
        else out[i] = log_ret ? log(x[i] / x[i - 1]) : (x[i] - x[i - 1]) / x[i - 1];
    }
}
void gs_volatility(const gc_real *x, int n, int w, int periods, gc_real *out)
{
    gc_real *ret;
    int i;
    if (!x || !out || n < 1) return;
    ret = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!ret) return;
    gs_returns(x, n, 1, ret);
    for (i = 0; i < n; i++) out[i] = (i >= w - 1 && i >= 1) ? sqrt(win_var(ret + i - w + 1, w)) * sqrt((gc_real)periods) : NAN;
    free(ret);
}
gc_real gs_max_drawdown(const gc_real *x, int n)
{
    gc_real peak, mdd = 0.0;
    int i;
    if (!x || n < 1) return 0.0;
    peak = x[0];
    for (i = 1; i < n; i++) {
        if (x[i] > peak) peak = x[i];
        if (peak > 0) { gc_real dd = (x[i] - peak) / peak; if (dd < mdd) mdd = dd; }
    }
    return mdd;
}
gc_real gs_yule_walker(const gc_real *x, int n, int p, gc_real *ar)
{
    /* Yule-Walker via Levinson-Durbin sull'autocorrelazione (Biased). */
    gc_real *r, *a, *tmp;
    gc_real m, E;
    int i, k;
    if (!x || !ar || n < 2 || p < 1 || p >= n) return 0.0;
    r = (gc_real *)calloc((size_t)(p + 1), sizeof(gc_real));
    a = (gc_real *)calloc((size_t)(p + 1), sizeof(gc_real));
    tmp = (gc_real *)calloc((size_t)(p + 1), sizeof(gc_real));
    if (!r || !a || !tmp) { free(r); free(a); free(tmp); return 0.0; }
    m = 0.0;
    for (i = 0; i < n; i++) m += x[i];
    m /= n;
    for (k = 0; k <= p; k++) {
        gc_real s = 0.0;
        for (i = 0; i + k < n; i++) s += (x[i] - m) * (x[i + k] - m);
        r[k] = s / n;
    }
    a[0] = 1.0;
    E = r[0];
    for (k = 1; k <= p; k++) {
        gc_real acc = r[k], refl;
        int j;
        for (j = 1; j < k; j++) acc += a[j] * r[k - j];
        refl = (E > 1e-30) ? -acc / E : 0.0;
        for (j = 0; j <= p; j++) tmp[j] = a[j];
        for (j = 1; j < k; j++) a[j] = tmp[j] + refl * tmp[k - j];
        a[k] = refl;
        E *= (1.0 - refl * refl);
        if (E < 1e-30) E = 1e-30;
    }
    for (i = 1; i <= p; i++) ar[i - 1] = a[i];
    free(r); free(a); free(tmp);
    return E;
}
void gs_ma_coefficients(const gc_real *x, int n, int q, gc_real *ma)
{
    /* Stima MA(q) via inversione di Durbin (semplificata): usa le autocorrelazioni. */
    int i, k;
    gc_real m, *r;
    if (!x || !ma || n < 2 || q < 1) return;
    r = (gc_real *)calloc((size_t)(q + 1), sizeof(gc_real));
    if (!r) return;
    m = 0.0;
    for (i = 0; i < n; i++) m += x[i];
    m /= n;
    for (k = 0; k <= q; k++) {
        gc_real s = 0.0;
        for (i = 0; i + k < n; i++) s += (x[i] - m) * (x[i + k] - m);
        r[k] = s / n;
    }
    /* MA(1): theta ~ r1/r0 (approssimazione); per q>1 usiamo r_k/r_0. */
    for (k = 1; k <= q; k++) ma[k - 1] = (r[0] > 1e-30) ? r[k] / r[0] : 0.0;
    free(r);
}

void gs_rolling_skew(const gc_real *x, int n, int w, gc_real *out)
{
    if (!out) return;
    moments_stream(x, n, w, NULL, NULL, out, NULL);
}
void gs_rolling_kurt(const gc_real *x, int n, int w, gc_real *out)
{
    if (!out) return;
    moments_stream(x, n, w, NULL, NULL, NULL, out);
}

/* Rolling OLS streaming (da bagol1000/fastwindow): pendenza, intercetta e R2
 * di y ~ a + b*x su ogni finestra. NaN per i<w-1 o design singolare. */
void gs_rolling_regression(const gc_real *x, const gc_real *y, int n, int w,
                           gc_real *slope, gc_real *intercept, gc_real *r2)
{
    gc_real sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;
    int i;
    if (!x || !y || !slope || !intercept || n < 1 || w < 1) return;
    for (i = 0; i < n; i++) {
        if (i >= w) { gc_real xo = x[i - w], yo = y[i - w]; sx -= xo; sy -= yo; sxx -= xo * xo; sxy -= xo * yo; syy -= yo * yo; }
        { gc_real xn = x[i], yn = y[i]; sx += xn; sy += yn; sxx += xn * xn; sxy += xn * yn; syy += yn * yn; }
        if (i < w - 1) { slope[i] = NAN; intercept[i] = NAN; if (r2) r2[i] = NAN; continue; }
        {
            gc_real dn = (gc_real)w, den = dn * sxx - sx * sx;
            if (den > 1e-12) {
                gc_real b1 = (dn * sxy - sx * sy) / den;
                gc_real b0 = (sy - b1 * sx) / dn;
                slope[i] = b1;
                intercept[i] = b0;
                if (r2) {
                    gc_real syy2 = dn * syy - sy * sy, sxy2 = dn * sxy - sx * sy, r;
                    r = (syy2 > 1e-12) ? sxy2 * sxy2 / (den * syy2) : 0.0;
                    r2[i] = r > 1.0 ? 1.0 : (r < 0.0 ? 0.0 : r);
                }
            } else { slope[i] = NAN; intercept[i] = NAN; if (r2) r2[i] = NAN; }
        }
    }
}

/* Expanding (Kahan-compensated) mean/sum. */
void gs_expanding_mean(const gc_real *x, int n, gc_real *out)
{
    gc_real sum = 0.0, comp = 0.0;
    int i;
    if (!x || !out || n < 1) return;
    for (i = 0; i < n; i++) {
        gc_real yk = x[i] - comp, t = sum + yk;
        comp = (t - sum) - yk;
        sum = t;
        out[i] = sum / (gc_real)(i + 1);
    }
}
void gs_expanding_sum(const gc_real *x, int n, gc_real *out)
{
    gc_real sum = 0.0, comp = 0.0;
    int i;
    if (!x || !out || n < 1) return;
    for (i = 0; i < n; i++) {
        gc_real yk = x[i] - comp, t = sum + yk;
        comp = (t - sum) - yk;
        sum = t;
        out[i] = sum;
    }
}
/* Expanding variance (ddof=1, campionaria, convenzione pandas/R). */
void gs_expanding_var(const gc_real *x, int n, gc_real *out)
{
    gc_real m = 0.0, m2 = 0.0;
    int i;
    if (!x || !out || n < 1) return;
    for (i = 0; i < n; i++) {
        gc_real v = x[i], d = v - m;
        m += d / (gc_real)(i + 1);
        m2 += d * (v - m);
        out[i] = (i >= 1) ? m2 / (gc_real)i : NAN;
    }
}
void gs_expanding_std(const gc_real *x, int n, gc_real *out)
{
    int i;
    if (!out) return;
    gs_expanding_var(x, n, out);
    for (i = 0; i < n; i++) if (!isnan(out[i])) out[i] = sqrt(out[i]);
}

/* Quantile approssimato streaming P^2 (Jain-Chlamtac 1985, da
 * bagol1000/fastwindow): expanding, O(1)/passo, memoria costante. */
void gs_expanding_quantile_p2(const gc_real *x, int n, gc_real q, gc_real *out)
{
    gc_real h[5], pos[5], dpos[5], incr[5];
    gc_real init_buf[5];
    int count = 0, i;
    if (!x || !out || n < 1 || q < 0.0 || q > 1.0) return;
    incr[0] = 0.0; incr[1] = q / 2.0; incr[2] = q; incr[3] = (1.0 + q) / 2.0; incr[4] = 1.0;
    for (i = 0; i < n; i++) {
        gc_real v = x[i];
        int k;
        if (count < 5) {
            init_buf[count++] = v;
            if (count == 5) {
                int a, b;
                for (a = 0; a < 5; a++) for (b = a + 1; b < 5; b++) if (init_buf[b] < init_buf[a]) { gc_real t = init_buf[a]; init_buf[a] = init_buf[b]; init_buf[b] = t; }
                for (k = 0; k < 5; k++) { h[k] = init_buf[k]; pos[k] = (gc_real)(k + 1); }
                dpos[0] = 1.0; dpos[1] = 1.0 + 2.0 * q; dpos[2] = 1.0 + 4.0 * q;
                dpos[3] = 3.0 + 2.0 * q; dpos[4] = 5.0;
            }
            out[i] = (count < 5) ? init_buf[0] : NAN;
            continue;
        }
        count++;
        if (v < h[0])       { h[0] = v; k = 0; }
        else if (v >= h[4]) { h[4] = v; k = 3; }
        else { k = 0; while (k < 3 && v >= h[k + 1]) k++; }
        for (k = k + 1; k < 5; k++) pos[k] += 1.0;
        for (k = 0; k < 5; k++) dpos[k] += incr[k];
        for (k = 1; k <= 3; k++) {
            gc_real d = dpos[k] - pos[k], s, hp;
            if (!((d >= 1.0 && pos[k + 1] - pos[k] > 1.0) ||
                  (d <= -1.0 && pos[k - 1] - pos[k] < -1.0))) continue;
            s = (d >= 0.0) ? 1.0 : -1.0;
            hp = h[k] + s / (pos[k + 1] - pos[k - 1]) *
                 ((pos[k] - pos[k - 1] + s) * (h[k + 1] - h[k]) / (pos[k + 1] - pos[k]) +
                  (pos[k + 1] - pos[k] - s) * (h[k] - h[k - 1]) / (pos[k] - pos[k - 1]));
            if (!(h[k - 1] < hp && hp < h[k + 1]))
                hp = h[k] + s * (h[k + (int)s] - h[k]) / (pos[k + (int)s] - pos[k]);
            h[k] = hp;
            pos[k] += s;
        }
        out[i] = h[2];
    }
}

/* --- Serie temporali: ACF/PACF, stagionalita', streak, robust z, trend filter --- */

void gs_acf(const gc_real *x, int n, int maxlag, gc_real *out)
{
    gc_real m = 0.0, den = 0.0;
    int i, k;
    if (!x || !out || n < 2 || maxlag < 0) return;
    for (i = 0; i < n; i++) m += x[i];
    m /= (gc_real)n;
    for (i = 0; i < n; i++) { gc_real d = x[i] - m; den += d * d; }
    for (k = 0; k <= maxlag; k++) {
        gc_real num = 0.0;
        if (k >= n) { out[k] = NAN; continue; }
        for (i = k; i < n; i++) num += (x[i] - m) * (x[i - k] - m);
        out[k] = den > 0.0 ? num / den : NAN;
    }
}

/* Soglia di significativita' dell'ACF: 1.96/sqrt(n). */
gc_real gs_acf_band(int n) { return n > 0 ? 1.96 / sqrt((gc_real)n) : NAN; }

void gs_pacf(const gc_real *x, int n, int maxlag, gc_real *out)
{
    gc_real *r, *phi, *prev;
    int k, j;
    if (!x || !out || n < 2 || maxlag < 0) return;
    r = (gc_real *)malloc((size_t)(maxlag + 1) * sizeof(gc_real));
    phi = (gc_real *)malloc((size_t)(maxlag + 1) * sizeof(gc_real));
    prev = (gc_real *)malloc((size_t)(maxlag + 1) * sizeof(gc_real));
    if (!r || !phi || !prev) { free(r); free(phi); free(prev); return; }
    /* Yule-Walker: autocovarianza non distorta gamma_k = sum/(n-k), come
     * statsmodels pacf(method='yw'). */
    {
        gc_real m = 0.0, g0 = 0.0;
        for (j = 0; j < n; j++) m += x[j];
        m /= (gc_real)n;
        for (k = 0; k <= maxlag; k++) {
            gc_real s = 0.0; int i;
            if (k >= n) { r[k] = NAN; continue; }
            for (i = k; i < n; i++) s += (x[i] - m) * (x[i - k] - m);
            s /= (gc_real)(n - k);
            if (k == 0) g0 = s;
            r[k] = g0 != 0.0 ? s / g0 : NAN;
        }
    }
    out[0] = 1.0;
    for (k = 1; k <= maxlag; k++) out[k] = NAN;
    if (maxlag >= 1) {
        phi[1] = r[1];
        out[1] = phi[1];
        for (j = 1; j <= maxlag; j++) prev[j] = phi[j];
        for (k = 2; k <= maxlag; k++) {
            gc_real num = r[k], den = 1.0;
            for (j = 1; j <= k - 1; j++) { num -= prev[j] * r[k - j]; den -= prev[j] * r[j]; }
            phi[k] = den != 0.0 ? num / den : 0.0;
            for (j = 1; j <= k - 1; j++) phi[j] = prev[j] - phi[k] * prev[k - j];
            out[k] = phi[k];
            for (j = 1; j <= k; j++) prev[j] = phi[j];
        }
    }
    free(r); free(phi); free(prev);
}

/* Decomposizione classica in trend + stagionale (+ residuo). model: 0 additivo,
 * 1 moltiplicativo. trend/seasonal/resid possono essere NULL. I primi/ultimi
 * (period-1)/2 valori del trend sono NaN (finestra incompleta). */
int gs_seasonal_decompose(const gc_real *x, int n, int period, int model,
                          gc_real *trend, gc_real *seasonal, gc_real *resid)
{
    gc_real *savg, *scnt;
    gc_real gmean = 0.0;
    int i, s, h;
    if (!x || n < 2 * period || period < 2) return -1;
    savg = (gc_real *)calloc((size_t)period, sizeof(gc_real));
    scnt = (gc_real *)calloc((size_t)period, sizeof(gc_real));
    if (!savg || !scnt) { free(savg); free(scnt); return -1; }
    h = period / 2;
    for (i = 0; i < n; i++) {
        gc_real t = NAN;
        if (period % 2 == 1) {
            if (i - h >= 0 && i + h < n) { int j; gc_real sm = 0.0; for (j = i - h; j <= i + h; j++) sm += x[j]; t = sm / period; }
        } else {
            if (i - h >= 0 && i + h < n) {
                int j; gc_real sm = 0.0;
                for (j = i - h; j <= i + h; j++) sm += x[j];
                sm -= 0.5 * (x[i - h] + x[i + h]);           /* pesi: 0.5 agli estremi */
                t = sm / period;                             /* pesi totali = period */
            }
        }
        if (trend) trend[i] = t;
        if (!isnan(t)) {
            gc_real d = (model == 1) ? (t != 0.0 ? x[i] / t : NAN) : x[i] - t;
            if (!isnan(d)) { savg[i % period] += d; scnt[i % period] += 1.0; }
        }
    }
    for (s = 0; s < period; s++) savg[s] = scnt[s] > 0.0 ? savg[s] / scnt[s] : 0.0;
    for (s = 0; s < period; s++) gmean += savg[s];
    gmean /= (gc_real)period;
    for (s = 0; s < period; s++) savg[s] = (model == 1) ? (gmean != 0.0 ? savg[s] / gmean : 1.0) : savg[s] - gmean;
    for (i = 0; i < n; i++) {
        gc_real se = savg[i % period];
        if (seasonal) seasonal[i] = se;
        if (resid) {
            if (isnan(trend[i])) resid[i] = NAN;
            else if (model == 1) resid[i] = (trend[i] * se != 0.0) ? x[i] / (trend[i] * se) : NAN;
            else resid[i] = x[i] - trend[i] - se;
        }
    }
    free(savg); free(scnt);
    return 0;
}

/* Forza della stagionalita' (additiva): max(0, 1 - Var(resid)/Var(resid+seasonal)). */
gc_real gs_seasonal_strength(const gc_real *seasonal, const gc_real *resid, int n)
{
    gc_real mr = 0.0, mv = 0.0, vr = 0.0, vv = 0.0;
    int i, c = 0;
    if (!seasonal || !resid || n < 2) return NAN;
    for (i = 0; i < n; i++) if (!isnan(resid[i]) && !isnan(seasonal[i])) {
        mr += resid[i]; mv += resid[i] + seasonal[i]; c++;
    }
    if (c < 2) return NAN;
    mr /= c; mv /= c;
    for (i = 0; i < n; i++) if (!isnan(resid[i]) && !isnan(seasonal[i])) {
        gc_real dr = resid[i] - mr, dv = resid[i] + seasonal[i] - mv;
        vr += dr * dr; vv += dv * dv;
    }
    vr /= c; vv /= c;
    if (vv <= 0.0) return 0.0;
    { gc_real st = 1.0 - vr / vv; return st > 0.0 ? st : 0.0; }
}

/* Streak: out_len[i] = lunghezza della sequenza corrente di variazioni di segno
 * concorde fino a i (out_len[0]=0), out_sign = segno della variazione corrente
 * (+1 su, -1 giu'; out_sign[0]=0). Ritorna il massimo streak. */
int gs_streaks(const gc_real *x, int n, int *out_len, int *out_sign)
{
    int i, mx = 0, cur = 0, sgn = 0;
    if (!x || n < 1) return -1;
    if (out_len) out_len[0] = 0;
    if (out_sign) out_sign[0] = 0;
    for (i = 1; i < n; i++) {
        int s = (x[i] > x[i - 1]) ? 1 : ((x[i] < x[i - 1]) ? -1 : 0);
        if (s != 0 && s == sgn) cur++; else { cur = (s != 0) ? 1 : 0; sgn = s; }
        if (out_len) out_len[i] = cur;
        if (out_sign) out_sign[i] = sgn;
        if (cur > mx) mx = cur;
    }
    return mx;
}

/* Robust z-score (Qlib): (x - mediana)/(1.4826*MAD), troncato a [-clip, clip]. */
void gs_robust_zscore(const gc_real *x, int n, gc_real clip, gc_real *out)
{
    gc_real *tmp, med, mad;
    int i;
    if (!x || !out || n < 1) return;
    tmp = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!tmp) return;
    memcpy(tmp, x, (size_t)n * sizeof(gc_real));
    qsort(tmp, (size_t)n, sizeof(gc_real), cmp_real);
    med = (n % 2) ? tmp[n / 2] : 0.5 * (tmp[n / 2 - 1] + tmp[n / 2]);
    for (i = 0; i < n; i++) tmp[i] = fabs(x[i] - med);
    qsort(tmp, (size_t)n, sizeof(gc_real), cmp_real);
    mad = (n % 2) ? tmp[n / 2] : 0.5 * (tmp[n / 2 - 1] + tmp[n / 2]);
    free(tmp);
    mad *= 1.4826;
    for (i = 0; i < n; i++) {
        gc_real z = mad > 0.0 ? (x[i] - med) / mad : 0.0;
        if (clip > 0.0) { if (z > clip) z = clip; else if (z < -clip) z = -clip; }
        out[i] = z;
    }
}

/* Trend filter L2 (Hodrick-Prescott quando order=2): minimizza
 * 0.5||y-x||^2 + lambda*||D^k x||^2, D = matrice delle differenze di ordine k.
 * Soluzione (I + lambda D'D) x = y via Cholesky a banda (k=1 tridiagonale,
 * k=2 pentadiagonale). out[n]. Ritorna 0/-1. */
int gs_trend_filter(const gc_real *y, int n, gc_real lambda, int order, gc_real *out)
{
    gc_real *diag, *b1, *b2, *L0, *L1, *L2, c[3];
    int m = order, i;
    if (!y || !out || n < 3 || (m != 1 && m != 2) || lambda < 0.0) return -1;
    diag = (gc_real *)calloc((size_t)n, sizeof(gc_real));
    b1 = (gc_real *)calloc((size_t)n, sizeof(gc_real));
    b2 = (gc_real *)calloc((size_t)n, sizeof(gc_real));
    L0 = (gc_real *)calloc((size_t)n, sizeof(gc_real));
    L1 = (gc_real *)calloc((size_t)n, sizeof(gc_real));
    L2 = (gc_real *)calloc((size_t)n, sizeof(gc_real));
    if (!diag || !b1 || !b2 || !L0 || !L1 || !L2) {
        free(diag); free(b1); free(b2); free(L0); free(L1); free(L2); return -1; }
    /* stencil differenze: c_j = (-1)^j C(m,j), j=0..m */
    c[0] = 1.0;
    if (m >= 1) c[1] = -1.0;
    if (m >= 2) c[2] = 1.0;          /* m=2: [1,-2,1] -> c1=-2, correzione sotto */
    if (m == 2) { c[1] = -2.0; }
    /* D[r][col] = c_{col-r} se 0<=col-r<=m, righe r=0..n-m-1 */
    for (i = 0; i < n; i++) {
        int r, rlo, rhi;
        rlo = i - m; if (rlo < 0) rlo = 0;
        rhi = i; if (rhi > n - m - 1) rhi = n - m - 1;
        for (r = rlo; r <= rhi; r++) {
            gc_real d = c[i - r];
            diag[i] += lambda * d * d;
        }
        diag[i] += 1.0;
        /* b1[i] = (D'D)[i+1][i] */
        if (i + 1 < n) {
            int j, jlo, jhi; jlo = i + 1 - m; if (jlo < 0) jlo = 0;
            jhi = i + 1; if (jhi > n - m - 1) jhi = n - m - 1;
            for (j = jlo; j <= jhi; j++) {
                int a = i - j, b = (i + 1) - j;
                if (a >= 0 && a <= m && b >= 0 && b <= m) b1[i] += lambda * c[a] * c[b];
            }
        }
        /* b2[i] = (D'D)[i+2][i] */
        if (m == 2 && i + 2 < n) {
            int j, jlo, jhi; jlo = i + 2 - m; if (jlo < 0) jlo = 0;
            jhi = i + 2; if (jhi > n - m - 1) jhi = n - m - 1;
            for (j = jlo; j <= jhi; j++) {
                int a = i - j, b = (i + 2) - j;
                if (a >= 0 && a <= m && b >= 0 && b <= m) b2[i] += lambda * c[a] * c[b];
            }
        }
    }
    /* Cholesky a banda: A = L L^T, L0=diag, L1=1a sotto, L2=2a sotto */
    for (i = 0; i < n; i++) {
        gc_real d = diag[i];
        if (i >= 1) d -= L1[i - 1] * L1[i - 1];
        if (i >= 2) d -= L2[i - 2] * L2[i - 2];
        if (d <= 1e-15) { free(diag); free(b1); free(b2); free(L0); free(L1); free(L2); return -1; }
        L0[i] = sqrt(d);
        if (i + 1 < n) {
            gc_real v = b1[i];
            if (i >= 1) v -= L2[i - 1] * L1[i - 1];
            L1[i] = v / L0[i];
        }
        if (i + 2 < n) L2[i] = b2[i] / L0[i];
    }
    /* forward L z = y */
    for (i = 0; i < n; i++) {
        gc_real v = y[i];
        if (i >= 1) v -= L1[i - 1] * out[i - 1];
        if (i >= 2) v -= L2[i - 2] * out[i - 2];
        out[i] = v / L0[i];
    }
    /* backward L^T x = z */
    for (i = n - 1; i >= 0; i--) {
        gc_real v = out[i];
        if (i + 1 < n) v -= L1[i] * out[i + 1];
        if (i + 2 < n) v -= L2[i] * out[i + 2];
        out[i] = v / L0[i];
    }
    free(diag); free(b1); free(b2); free(L0); free(L1); free(L2);
    return 0;
}
