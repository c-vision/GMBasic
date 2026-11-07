/* gq_portfolio.c — ottimizzazione e metriche di portafoglio.
 *
 * Porting fedele delle formule del runtime condiviso (gmb_math.c / gmb_finance.c):
 * sono le stesse che usano VM/C e C99Native, quindi il risultato coincide con i
 * builtin TRADING.PORTFOLIO (parita' storica fra le gambe C).
 */
#include "gm_quant_portfolio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── utilita' ────────────────────────────────────────────────────────────── */

static void sort_asc(double *a, int n)
{
    for (int i = 1; i < n; i++)
    {
        double k = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > k) { a[j + 1] = a[j]; j--; }
        a[j + 1] = k;
    }
}

/* Inversione n*n row-major (Gauss-Jordan con pivot parziale). 1 = ok. */
static int mat_inv(const double *m, int n, double *out)
{
    double *a;
    if (n <= 0) return 0;
    a = (double *)malloc((size_t)(n * n) * sizeof(double));
    if (!a) return 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) a[i * n + j] = m[i * n + j];
    memset(out, 0, (size_t)(n * n) * sizeof(double));
    for (int i = 0; i < n; i++) out[i * n + i] = 1.0;
    for (int col = 0; col < n; col++)
    {
        double piv = fabs(a[col * n + col]);
        int pr = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(a[r * n + col]) > piv) { piv = fabs(a[r * n + col]); pr = r; }
        if (piv < 1e-15) { free(a); return 0; }
        if (pr != col)
        {
            for (int j = 0; j < n; j++)
            {
                double t = a[col * n + j];
                a[col * n + j] = a[pr * n + j];
                a[pr * n + j] = t;
                t = out[col * n + j];
                out[col * n + j] = out[pr * n + j];
                out[pr * n + j] = t;
            }
        }
        double d = a[col * n + col];
        for (int j = 0; j < n; j++) { a[col * n + j] /= d; out[col * n + j] /= d; }
        for (int r = 0; r < n; r++)
        {
            if (r == col) continue;
            double f = a[r * n + col];
            if (f == 0.0) continue;
            for (int j = 0; j < n; j++)
            {
                a[r * n + j] -= f * a[col * n + j];
                out[r * n + j] -= f * out[col * n + j];
            }
        }
    }
    free(a);
    return 1;
}

/* ── metriche scalari ────────────────────────────────────────────────────── */

double gq_port_expected_return(const double *w, const double *mean, int n)
{
    double s = 0.0;
    if (!w || !mean || n <= 0) return 0.0;
    for (int i = 0; i < n; i++) s += w[i] * mean[i];
    return s;
}

double gq_port_stdev(const double *w, const double *cov, int n)
{
    double v = 0.0;
    if (!w || !cov || n <= 0) return 0.0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) v += w[i] * cov[i * n + j] * w[j];
    return v > 0.0 ? sqrt(v) : 0.0;
}

double gq_port_sharpe(double ret, double sd, double risk_free)
{
    if (sd == 0.0) return 0.0;
    return (ret - risk_free) / sd;
}

double gq_port_is_long_only(const double *w, int n)
{
    if (!w || n <= 0) return -1.0;
    for (int i = 0; i < n; i++) if (w[i] < 0.0) return 0.0;
    return -1.0; /* BIT GMBasic: vero = -1 */
}

double gq_port_mad(const double *r, int n)
{
    if (!r || n < 1) return 0.0;
    double *s = (double *)malloc(sizeof(double) * (size_t)n);
    if (!s) return 0.0;
    memcpy(s, r, sizeof(double) * (size_t)n);
    sort_asc(s, n);
    double med = s[n / 2];
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += fabs(r[i] - med);
    free(s);
    return sum / n;
}

double gq_port_semimad(const double *r, int n, double target)
{
    if (!r || n < 1) return 0.0;
    double *b = (double *)malloc(sizeof(double) * (size_t)n);
    if (!b) return 0.0;
    int bn = 0;
    for (int i = 0; i < n; i++) if (r[i] < target) b[bn++] = r[i];
    if (bn == 0) { free(b); return 0.0; }
    sort_asc(b, bn);
    double med = b[bn / 2];
    double sum = 0.0;
    for (int i = 0; i < bn; i++) sum += fabs(b[i] - med);
    free(b);
    return sum / bn;
}

double gq_port_var(const double *flat, int n, double conf)
{
    if (!flat || n <= 0) return 0.0;
    double *s = (double *)malloc(sizeof(double) * (size_t)n);
    if (!s) return 0.0;
    memcpy(s, flat, sizeof(double) * (size_t)n);
    sort_asc(s, n);
    int idx = (int)((1.0 - conf) * n);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    double res = -s[idx] * 100.0;
    free(s);
    return res;
}

double gq_port_cvar(const double *flat, int n, double conf)
{
    if (!flat || n <= 0) return 0.0;
    double *s = (double *)malloc(sizeof(double) * (size_t)n);
    if (!s) return 0.0;
    memcpy(s, flat, sizeof(double) * (size_t)n);
    sort_asc(s, n);
    int cutoff = (int)((1.0 - conf) * n);
    if (cutoff <= 0) cutoff = 1;
    double sum = 0.0;
    for (int i = 0; i < cutoff; i++) sum += s[i];
    free(s);
    /* Parita' gmb_trd_portfolio_cvar_jagged: valore assoluto della coda. */
    double mean = sum / cutoff;
    return mean < 0.0 ? -mean : mean;
}

double gq_port_max_return_value(const double *mean, int n)
{
    double m = 0.0;
    if (!mean || n <= 0) return 0.0;
    for (int i = 0; i < n; i++) if (mean[i] > m) m = mean[i];
    return m;
}

double gq_port_kelly_value(const double *r, int n)
{
    if (!r || n <= 0) return 0.0;
    int wins = 0, nw = 0, nl = 0;
    double sum_w = 0.0, sum_l = 0.0;
    for (int i = 0; i < n; i++)
    {
        if (r[i] > 0.0) { wins++; sum_w += r[i]; nw++; }
        else if (r[i] < 0.0) { sum_l += fabs(r[i]); nl++; }
    }
    double win_rate = (double)wins / n;
    double avg_win = nw > 0 ? sum_w / nw : 0.0;
    double avg_loss = nl > 0 ? sum_l / nl : 0.0;
    if (avg_loss > 0.0 && avg_win > 0.0)
        return win_rate - (1.0 - win_rate) / (avg_win / avg_loss);
    return 0.0;
}

double gq_port_min_cvar_value(const double *r, int n, double conf)
{
    if (!r || n <= 0) return 0.0;
    double *s = (double *)malloc(sizeof(double) * (size_t)n);
    if (!s) return 0.0;
    memcpy(s, r, sizeof(double) * (size_t)n);
    sort_asc(s, n);
    int tail = (int)((1.0 - conf) * n);
    if (tail < 1) tail = 1;
    double sum = 0.0;
    for (int i = 0; i < tail; i++) sum += s[i];
    free(s);
    return -sum / tail;
}

double gq_port_min_cdar_value(const double *r, int n, double conf)
{
    if (!r || n <= 0) return 0.0;
    double *dd = (double *)malloc(sizeof(double) * (size_t)n);
    if (!dd) return 0.0;
    double cum = 0.0, peak = 0.0;
    for (int i = 0; i < n; i++)
    {
        cum += r[i];
        if (cum > peak) peak = cum;
        dd[i] = peak - cum;
    }
    sort_asc(dd, n);
    int tail = (int)((1.0 - conf) * n);
    if (tail < 1) tail = 1;
    double sum = 0.0;
    for (int i = n - tail; i < n; i++) sum += dd[i];
    free(dd);
    return sum / tail;
}

/* ── ottimizzatori ───────────────────────────────────────────────────────── */

int gq_port_equally_weighted(int n, double *w)
{
    if (!w || n <= 0) return 0;
    for (int i = 0; i < n; i++) w[i] = 1.0 / n;
    return 1;
}

int gq_port_min_volatility(const double *cov, int n, double *w)
{
    if (!cov || !w || n <= 0) return 0;
    double *inv = (double *)malloc((size_t)(n * n) * sizeof(double));
    if (!inv) { gq_port_equally_weighted(n, w); return 0; }
    if (!mat_inv(cov, n, inv)) { free(inv); gq_port_equally_weighted(n, w); return 0; }
    double wsum = 0.0;
    for (int i = 0; i < n; i++)
    {
        double row = 0.0;
        for (int j = 0; j < n; j++) row += inv[i * n + j];
        w[i] = row;
        wsum += row;
    }
    if (wsum != 0.0) { for (int i = 0; i < n; i++) w[i] /= wsum; }
    else { gq_port_equally_weighted(n, w); }
    free(inv);
    return 1;
}

int gq_port_max_sharpe(const double *mean, const double *cov, int n, double rf, double *w)
{
    if (!mean || !cov || !w || n <= 0) return 0;
    double *inv = (double *)malloc((size_t)(n * n) * sizeof(double));
    if (!inv) { gq_port_equally_weighted(n, w); return 0; }
    if (!mat_inv(cov, n, inv)) { free(inv); gq_port_equally_weighted(n, w); return 0; }
    double wsum = 0.0;
    for (int i = 0; i < n; i++)
    {
        double row = 0.0;
        for (int j = 0; j < n; j++) row += inv[i * n + j] * (mean[j] - rf);
        w[i] = row;
        wsum += row;
    }
    if (wsum != 0.0) { for (int i = 0; i < n; i++) w[i] /= wsum; }
    else { gq_port_equally_weighted(n, w); }
    free(inv);
    return 1;
}

int gq_port_mean_variance(const double *mean, const double *cov, int n, double *w)
{
    return gq_port_max_sharpe(mean, cov, n, 0.0, w);
}

int gq_port_risk_parity(const double *cov, int n, double *w)
{
    if (!cov || !w || n <= 0) return 0;
    double *marg = (double *)malloc(sizeof(double) * (size_t)n);
    double *nw = (double *)malloc(sizeof(double) * (size_t)n);
    if (!marg || !nw) { free(marg); free(nw); gq_port_equally_weighted(n, w); return 0; }
    for (int i = 0; i < n; i++) w[i] = 1.0 / n;
    for (int iter = 0; iter < 1000; iter++)
    {
        double pv = 0.0, sum = 0.0, diff = 0.0;
        for (int i = 0; i < n; i++)
        {
            marg[i] = 0.0;
            for (int j = 0; j < n; j++) marg[i] += cov[i * n + j] * w[j];
        }
        for (int i = 0; i < n; i++) pv += w[i] * marg[i];
        if (pv <= 0.0) break;
        for (int i = 0; i < n; i++)
        {
            nw[i] = w[i] * pv / (marg[i] != 0.0 ? fabs(marg[i]) * n : 1e-12);
            sum += nw[i];
        }
        if (sum == 0.0) break;
        for (int i = 0; i < n; i++) nw[i] /= sum;
        for (int i = 0; i < n; i++) { diff += fabs(nw[i] - w[i]); w[i] = nw[i]; }
        if (diff < 1e-8) break;
    }
    free(marg);
    free(nw);
    return 1;
}

int gq_port_max_return_weights(const double *mean, int n, double *w)
{
    if (!mean || !w || n <= 0) return 0;
    int bi = 0;
    for (int i = 1; i < n; i++) if (mean[i] > mean[bi]) bi = i;
    for (int i = 0; i < n; i++) w[i] = (i == bi) ? 1.0 : 0.0;
    return 1;
}

int gq_port_kelly_weights(const double *r, int n, double *w)
{
    if (!r || !w || n <= 0) return 0;
    w[0] = gq_port_kelly_value(r, n);
    return 1;
}

int gq_port_min_cvar_weights(const double *r, int n, double conf, double *w)
{
    if (!r || !w || n <= 0) return 0;
    (void)conf;
    w[0] = 1.0;
    return 1;
}

int gq_port_min_cdar_weights(const double *r, int n, double conf, double *w)
{
    if (!r || !w || n <= 0) return 0;
    (void)conf;
    w[0] = 1.0;
    return 1;
}

int gq_port_evolve(const double *cov, int n, double *w)
{
    return gq_port_min_volatility(cov, n, w);
}

int gq_port_black_litterman(const double *caps, const double *cov, int n,
                            double risk_aversion, double tau,
                            const double *P, int k, const double *Q,
                            const double *omega, double rf, double *w)
{
    if (!caps || !cov || !w || n <= 0) return 0;
    if (k < 0) k = 0;

    double *Sigma = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *wmkt = (double *)malloc((size_t)n * sizeof(double));
    double *Pi = (double *)calloc((size_t)n, sizeof(double));
    double *tSigma = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *tSigma_inv = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *Om = (double *)calloc((size_t)(k > 0 ? k * k : 1), sizeof(double));
    double *Om_inv = (double *)calloc((size_t)(k > 0 ? k * k : 1), sizeof(double));
    double *PtOmP = (double *)calloc((size_t)(n * n), sizeof(double));
    double *PtOmQ = (double *)calloc((size_t)n, sizeof(double));
    double *A = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *A_inv = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *bb = (double *)malloc((size_t)n * sizeof(double));
    double *post = (double *)calloc((size_t)n, sizeof(double));
    int ok = 0;

#define PF_FREE() do { free(Sigma); free(wmkt); free(Pi); free(tSigma); free(tSigma_inv); \
    free(Om); free(Om_inv); free(PtOmP); free(PtOmQ); free(A); free(A_inv); free(bb); \
    free(post); } while (0)
    if (!Sigma || !wmkt || !Pi || !tSigma || !tSigma_inv || !Om || !Om_inv || !PtOmP ||
        !PtOmQ || !A || !A_inv || !bb || !post) { PF_FREE(); return 0; }

    memcpy(Sigma, cov, (size_t)(n * n) * sizeof(double));

    double total = 0.0;
    for (int i = 0; i < n; i++) total += caps[i];
    for (int i = 0; i < n; i++) wmkt[i] = total > 0.0 ? caps[i] / total : 1.0 / n;

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++) Pi[i] += Sigma[i * n + j] * wmkt[j];
        Pi[i] *= risk_aversion;
    }

    for (int i = 0; i < n * n; i++) tSigma[i] = tau * Sigma[i];
    if (!mat_inv(tSigma, n, tSigma_inv)) { PF_FREE(); return 0; }

    if (k > 0)
    {
        memcpy(Om, omega, (size_t)(k * k) * sizeof(double));
        if (!mat_inv(Om, k, Om_inv)) { PF_FREE(); return 0; }
    }

    for (int a = 0; a < n; a++)
    {
        for (int b = 0; b < n; b++)
        {
            double s = 0.0;
            for (int u = 0; u < k; u++)
                for (int v = 0; v < k; v++)
                    s += P[u * n + a] * Om_inv[u * k + v] * P[v * n + b];
            PtOmP[a * n + b] = s;
        }
        double s2 = 0.0;
        for (int u = 0; u < k; u++)
            for (int v = 0; v < k; v++)
                s2 += P[u * n + a] * Om_inv[u * k + v] * Q[v];
        PtOmQ[a] = s2;
    }

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) A[i * n + j] = tSigma_inv[i * n + j] + PtOmP[i * n + j];
    if (!mat_inv(A, n, A_inv)) { PF_FREE(); return 0; }

    for (int i = 0; i < n; i++)
    {
        double s = 0.0;
        for (int j = 0; j < n; j++) s += tSigma_inv[i * n + j] * Pi[j];
        bb[i] = s + PtOmQ[i];
    }
    for (int i = 0; i < n; i++)
    {
        double s = 0.0;
        for (int j = 0; j < n; j++) s += A_inv[i * n + j] * bb[j];
        post[i] = s;
    }

    ok = gq_port_max_sharpe(post, Sigma, n, rf, w);
    PF_FREE();
#undef PF_FREE
    return ok;
}

/* ── packer FFI ──────────────────────────────────────────────────────────── */

void gq_port_pack(const double *w, int n, const double *mean, const double *cov,
                  double rf, double *out)
{
    if (!out) return;
    if (!w || n <= 0)
    {
        out[0] = out[1] = out[2] = 0.0;
        return;
    }
    double ret = mean ? gq_port_expected_return(w, mean, n) : 0.0;
    double vol = cov ? gq_port_stdev(w, cov, n) : 0.0;
    double sh = gq_port_sharpe(ret, vol, rf);
    out[0] = ret;
    out[1] = vol;
    out[2] = sh;
    for (int i = 0; i < n; i++) out[3 + i] = w[i];
}
