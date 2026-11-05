/* gs_forecast_finance.c — funzioni finance/quant (gmbstats-finance). */
#include "gs_sim.h"
#include "gs_dist.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int gs_accrual_wc_nco(double ni, double dep, double cfo, double cfi, double cff,
                      double div, double *wc, double *nco, double *tacc, double *acc_income)
{
    double dcash;
    if (!wc || !nco || !tacc || !acc_income) return -1;
    dcash = cfo + cff + cfi;
    *wc = ni + dep - cfo;
    *nco = ni - cfo - cfi;
    *tacc = ni - dcash - div;
    *acc_income = ni != 0 ? *tacc / ni : 0.0;
    return 0;
}

/* ---- DTW + inverse-distance-weighted next-step prediction. ---- */
double gs_amihud(const double *ret, const double *dollar_vol, int n)
{
    double s = 0.0;
    int i, cnt = 0;
    if (!ret || !dollar_vol || n < 1) return 0.0 / 0.0;
    for (i = 0; i < n; i++) {
        if (dollar_vol[i] > 0) { s += fabs(ret[i]) / dollar_vol[i]; cnt++; }
    }
    return cnt > 0 ? 1e6 * s / cnt : 0.0;
}
/* ---- Detector anomalie quantile (pyanomaly): bounds da mediana e quantile
 * di coda. mode: 0 = value, 1 = change (due variazioni consecutive). ---- */
double gs_coskew(const double *res, const double *mkt, int n)
{
    double mr, mm, a = 0.0, b = 0.0, c = 0.0;
    int i;
    if (!res || !mkt || n < 3) return 0.0 / 0.0;
    mr = 0.0; for (i = 0; i < n; i++) mr += res[i]; mr /= n;
    mm = 0.0; for (i = 0; i < n; i++) mm += mkt[i]; mm /= n;
    for (i = 0; i < n; i++) {
        double r = res[i] - mr, x = mkt[i] - mm;
        a += r * x * x;
        b += r * r;
        c += x * x;
    }
    a /= n; b = sqrt(b / n); c = c / n;
    return b > 0 && c > 0 ? a / (b * c) : 0.0;
}
/* ---- MAX-5 (Bali): media dei 5 ritorni giornalieri piu' alti del mese. ---- */
double gs_effective_spread(const double *bid, const double *ask, const double *last, int n)
{
    double s = 0.0;
    int i, cnt = 0;
    if (!bid || !ask || !last || n < 2) return 0.0 / 0.0;
    for (i = 1; i < n; i++) {
        double mid = 0.5 * (bid[i] + ask[i]);
        double q = last[i] >= last[i - 1] ? 1.0 : -1.0;
        s += q * (last[i] - mid);
        cnt++;
    }
    return cnt > 0 ? 2.0 * s / cnt : 0.0;
}

/* ---- RiskMetrics EWMA covariance (lambda=0.94): pesi (1-l) l^(T-i)/sum. ---- */
double gs_log_trin(double adv, double dec, double adv_vol, double dec_vol)
{
    if (dec <= 0 || dec_vol <= 0) return 0.0 / 0.0;
    return log((adv / dec) / (adv_vol / dec_vol));
}

/* ---- Volume profile / Point of Control. ---- */
double gs_max5(const double *x, int n, int k)
{
    double *copy;
    int i, j;
    if (!x || n < k || k < 1) return 0.0 / 0.0;
    copy = (double *)malloc((size_t)n * sizeof(double));
    if (!copy) return 0.0 / 0.0;
    for (i = 0; i < n; i++) copy[i] = x[i];
    for (i = 0; i < k; i++) {
        int bi = i;
        for (j = i + 1; j < n; j++) if (copy[j] > copy[bi]) bi = j;
        { double t = copy[i]; copy[i] = copy[bi]; copy[bi] = t; }
    }
    {
        double s = 0.0;
        for (i = 0; i < k; i++) s += copy[i];
        free(copy);
        return s / k;
    }
}
/* ---- Amihud illiquidity: mean(|r| / dollar_vol) * 1e6. ---- */
int gs_probabilistic_obv(const double *c, const double *v, int n, double *out)
{
    double *dc, m = 0.0, sd = 0.0, obv = 0.0;
    int i;
    if (!c || !v || !out || n < 2) return -1;
    dc = (double *)malloc((size_t)n * sizeof(double));
    if (!dc) return -1;
    for (i = 1; i < n; i++) { dc[i] = c[i] - c[i - 1]; m += dc[i]; }
    m /= (n - 1);
    for (i = 1; i < n; i++) sd += (dc[i] - m) * (dc[i] - m);
    sd = sqrt(sd / (n - 2));
    out[0] = 0.0;
    for (i = 1; i < n; i++) {
        double z = sd > 1e-12 ? (dc[i] - m) / sd : 0.0;
        double buy = v[i] * gs_dist_normal_cdf(0, 1, z);
        double bs = fabs(buy - (v[i] - buy));
        obv += dc[i] > 0 ? bs : (dc[i] < 0 ? -bs : 0.0);
        out[i] = obv;
    }
    free(dc);
    return 0;
}

/* ---- Effective spread: (1/N) sum q_t (P_t - M_t)*2, q da segno dP. ---- */
int gs_riskmetrics_cov(const double *rets, int n, int d, double lambda, double *out_cov)
{
    double *w, *mu, *s;
    double wsum = 0.0;
    int i, j, t;
    if (!rets || !out_cov || n < 2 || d < 1 || lambda <= 0 || lambda >= 1) return -1;
    w = (double *)malloc((size_t)n * sizeof(double));
    mu = (double *)malloc((size_t)d * sizeof(double));
    s = (double *)malloc((size_t)d * sizeof(double));
    if (!w || !mu || !s) { free(w); free(mu); free(s); return -1; }
    for (t = 0; t < n; t++) {
        w[t] = pow(lambda, (double)(n - 1 - t));
        wsum += w[t];
    }
    for (t = 0; t < n; t++) w[t] /= wsum;
    for (j = 0; j < d; j++) { mu[j] = 0.0; for (t = 0; t < n; t++) mu[j] += w[t] * rets[t * d + j]; }
    for (i = 0; i < d; i++)
        for (j = 0; j < d; j++) {
            double c = 0.0;
            for (t = 0; t < n; t++) c += w[t] * (rets[t * d + i] - mu[i]) * (rets[t * d + j] - mu[j]);
            out_cov[i * d + j] = c;
        }
    /* s: std per-asse */
    for (j = 0; j < d; j++) { double v = 0.0; for (t = 0; t < n; t++) v += w[t] * (rets[t * d + j] - mu[j]) * (rets[t * d + j] - mu[j]); s[j] = sqrt(v); }
    free(w); free(mu); free(s);
    return 0;
}

/* ---- Savitzky-Golay smoothing (filtro FIR quadratico), per S/R detection. ---- */
void gs_sentiment_decay_reweight(const double *s, const int *is_retweet, int n, double *out)
{
    int i, run = 0;
    if (!s || !is_retweet || !out) return;
    for (i = 0; i < n; i++) {
        if (is_retweet[i]) { out[i] = s[i] > 0 ? 0.5 : -0.5; run = 0; continue; }
        if (i > 0 && (s[i] > 0) == (s[i - 1] > 0) && s[i] != 0) run++;
        else run = 0;
        if (run == 0) out[i] = s[i] > 0 ? 1.0 : (s[i] < 0 ? -1.0 : 0.0);
        else if (run == 1) out[i] = s[i] > 0 ? 0.5 : -0.5;
        else out[i] = 0.0;
    }
}
/* ---- Ticker aggregation: comment score / word_count, mention bias 2.0. ---- */
double gs_sentiment_fusion(double tb, double vd, int *label)
{
    double pol = 0.5 * (tb + vd);
    if (label) {
        if (tb < 0 && vd <= -0.05) *label = -1;
        else if (tb > 0 && vd >= 0.05) *label = 1;
        else *label = 0;
    }
    return pol;
}
/* ---- Author-decay & retweet re-weighting: consecutivi same-sign decay,
 * retweet -> 0.5. out: pesi. ---- */
double gs_sentiment_lm_composite(const double *pos, const double *neg, int n)
{
    double sp = 0.0, sn = 0.0;
    int i;
    if (!pos || !neg || n < 1) return 0.0 / 0.0;
    for (i = 0; i < n; i++) { sp += pos[i]; sn += neg[i]; }
    return (sp - sn) / n;
}
/* ---- Norm diff sentiment multi-orizzonte: (PS-NS)/(PS+NS). ---- */
double gs_sentiment_norm_diff(double ps, double ns)
{
    return ps + ns > 0 ? (ps - ns) / (ps + ns) : 0.0;
}
/* ---- Fusion polarity rule: (tb + vd)/2, label AND di soglie. ---- */
int gs_sentiment_ticker_agg(const double *word_scores, const int *word_is_ticker,
                            int n_words, double *out_score, int *out_count)
{
    double s = 0.0;
    int i, cnt = 0, has_ticker = 0;
    if (!word_scores || !word_is_ticker || !out_score || !out_count || n_words < 1) return -1;
    for (i = 0; i < n_words; i++) {
        if (word_is_ticker[i]) has_ticker = 1;
        s += word_scores[i];
        cnt++;
    }
    if (has_ticker) { *out_score = 2.0; *out_count = cnt; return 0; }
    *out_score = cnt > 0 ? s / cnt : 0.0;
    *out_count = cnt;
    return 0;
}

/* ---- Log TRIN (Arms): log((adv/dec)/(adv_vol/dec_vol)). ---- */
int gs_srf_signal(const double *ret, int n, int *horizons, int n_h, double *out)
{
    int i, h, j;
    if (!ret || !horizons || !out || n < 300 || n_h < 1) return -1;
    for (i = 0; i < n; i++) {
        double total = 0.0;
        for (h = 0; h < n_h; h++) {
            int H = horizons[h];
            double s = 0.0;
            int j;
            if (i >= H - 1) { for (j = i - H + 1; j <= i; j++) s += ret[j]; }
            if (s > 0) total += 1.0;
            else if (s < 0) total -= 1.0;
        }
        /* kill-switch: rolling Sharpe su 120 barre < -0.5 */
        {
            double m = 0.0, v = 0.0, sh;
            int w0 = i - 120;
            if (w0 < 0) w0 = 0;
            for (j = w0; j <= i; j++) m += ret[j];
            m /= (i - w0 + 1);
            for (j = w0; j <= i; j++) v += (ret[j] - m) * (ret[j] - m);
            v = sqrt(v / (i - w0 + 1));
            sh = v > 0 ? m / v : 0.0;
            if (sh * 16.0 < -0.5) total = 0.0;
        }
        out[i] = total / n_h;
    }
    return 0;
}
double gs_tfidf_score(const double *counts, const double *df, int n_terms,
                      int n_docs, int method, int normalize)
{
    double s = 0.0, idf;
    int j;
    if (!counts || !df || n_terms < 1 || n_docs < 1) return 0.0 / 0.0;
    for (j = 0; j < n_terms; j++) {
        idf = log((double)n_docs / (df[j] > 0 ? df[j] : 1.0));
        if (method == 1) s += (1.0 + log(counts[j] > 0 ? counts[j] : 1.0)) * idf;
        else s += counts[j] * idf;
    }
    if (normalize) s = s / n_terms;
    return s;
}

/* ---- Accrual components (WC/NCO/TACC). ---- */
int gs_volume_profile(const double *close, const double *vol, int n, int n_bins,
                      double *bin_vol, int *poc_bin)
{
    double mn = close[0], mx = close[0], step;
    int i, b, best = 0;
    if (!close || !vol || !bin_vol || !poc_bin || n < 1 || n_bins < 1) return -1;
    for (i = 0; i < n; i++) { if (close[i] < mn) mn = close[i]; if (close[i] > mx) mx = close[i]; }
    step = (mx - mn) / n_bins;
    for (b = 0; b < n_bins; b++) bin_vol[b] = 0.0;
    for (i = 0; i < n; i++) {
        b = (int)((close[i] - mn) / (step > 0 ? step : 1));
        if (b >= n_bins) b = n_bins - 1;
        if (b < 0) b = 0;
        bin_vol[b] += vol[i];
    }
    for (b = 1; b < n_bins; b++) if (bin_vol[b] > bin_vol[best]) best = b;
    *poc_bin = best;
    return 0;
}

/* ---- TFIDF/WFIDF per-document dimension score. ---- */
