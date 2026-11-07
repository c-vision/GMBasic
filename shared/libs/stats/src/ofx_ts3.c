/* ofx_ts3.c — shim FFI per le 5 funzioni TIMESERIES ancora builtin a fine 2026-11-05.
 *
 * Implementazioni COPIATE dal runtime C condiviso (gmb_math.c) e rinominate fx_*:
 * sono il canone bit-exact del builtin su C99/VM-C (ARIMA Hannan-Rissanen,
 * Holt-Winters additivo, ADF con p-value MacKinnon, Yule-Walker, MA).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double* fx_dense_solve(double* a, int n)
{
    /* a è la matrice n×n seguita dal rhs: risolvi a x = rhs (ultima colonna). */
    double* m = (double*)malloc(sizeof(double) * (size_t)n);
    if (!m) return NULL;
    for (int col = 0; col < n; col++)
    {
        int piv = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(a[r * (n + 1) + col]) > fabs(a[piv * (n + 1) + col])) piv = r;
        if (piv != col)
        {
            for (int j = col; j <= n; j++)
            {
                double t = a[col * (n + 1) + j]; a[col * (n + 1) + j] = a[piv * (n + 1) + j]; a[piv * (n + 1) + j] = t;
            }
        }
        for (int r = col + 1; r < n; r++)
        {
            double f = a[r * (n + 1) + col] / a[col * (n + 1) + col];
            for (int j = col; j <= n; j++) a[r * (n + 1) + j] -= f * a[col * (n + 1) + j];
        }
    }
    for (int r = n - 1; r >= 0; r--)
    {
        double s = a[r * (n + 1) + n];
        for (int j = r + 1; j < n; j++) s -= a[r * (n + 1) + j] * m[j];
        m[r] = s / a[r * (n + 1) + r];
    }
    free(a);
    return m;
}

static int fx_mat_inv(const double* m, int n, double* out)
{
    double *a;
    int i, j, col, r;
    if (n <= 0) return 0;
    a = (double*)malloc((size_t)(n * n) * sizeof(double));
    if (!a) return 0;
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) a[i*n+j] = m[i*n+j];
    memset(out, 0, (size_t)(n * n) * sizeof(double));
    for (i = 0; i < n; i++) out[i*n+i] = 1;
    for (col = 0; col < n; col++)
    {
        double piv = fabs(a[col*n+col]); int pr = col;
        for (r = col+1; r < n; r++) if (fabs(a[r*n+col]) > piv) { piv = fabs(a[r*n+col]); pr = r; }
        if (piv < 1e-15) { free(a); return 0; }
        if (pr != col)
        {
            for (j = 0; j < n; j++) { double t = a[col*n+j]; a[col*n+j] = a[pr*n+j]; a[pr*n+j] = t; t = out[col*n+j]; out[col*n+j] = out[pr*n+j]; out[pr*n+j] = t; }
        }
        double d = a[col*n+col];
        for (j = 0; j < n; j++) { a[col*n+j] /= d; out[col*n+j] /= d; }
        for (r = 0; r < n; r++)
        {
            if (r == col) continue;
            double f = a[r*n+col];
            if (f == 0) continue;
            for (j = 0; j < n; j++) { a[r*n+j] -= f*a[col*n+j]; out[r*n+j] -= f*out[col*n+j]; }
        }
    }
    free(a);
    return 1;
}

static double fx_adf_pvalue(double tau)
{
    static const double st[8] = { -4.5, -3.43, -2.86, -2.57, -1.6, -0.5, 0.5, 2.0 };
    static const double pv[8] = { 0.0001, 0.01, 0.05, 0.10, 0.30, 0.60, 0.90, 0.999 };
    if (tau <= st[0]) return pv[0];
    if (tau >= st[7]) return pv[7];
    for (int i = 0; i < 7; i++)
    {
        if (tau < st[i] || tau > st[i + 1]) continue;
        double frac = (tau - st[i]) / (st[i + 1] - st[i]);
        return pv[i] + frac * (pv[i + 1] - pv[i]);
    }
    return 0.5;
}

void fx_adf_test(const double* y, int n, double* statistic, double* pvalue)
{
    /* ADF aumentato (un lag): regressione OLS Δy_t = a + g·y_{t-1} + d·Δy_{t-1} + eps,
     * per t = 2..n-1 (righe = n-2). Canone CommonLib.Numerics.AdfTest: statistic = t di g,
     * seGamma da sigma2 * inv(AtA)[1,1]. */
    if (n < 5) { *statistic = 0; *pvalue = 0.5; return; }
    int rows = n - 2;
    if (rows < 4) { *statistic = 0; *pvalue = 0.5; return; }
    double* dy = (double*)malloc((size_t)(n - 1) * sizeof(double));
    if (!dy) { *statistic = 0; *pvalue = 0.5; return; }
    for (int t = 1; t < n; t++) dy[t - 1] = y[t] - y[t - 1];
    /* AtA (3×3) e Atb (3) delle colonne A_i = [1, y_{t-1}, dy_{t-2}] */
    double ata[9] = {0,0,0,0,0,0,0,0,0};
    double atb[3] = {0,0,0};
    for (int i = 0; i < rows; i++)
    {
        int t = i + 2;
        double a0 = 1.0, a1 = y[t - 1], a2 = dy[t - 2];
        double b = dy[t - 1];
        double cols[3] = { a0, a1, a2 };
        for (int r = 0; r < 3; r++)
        {
            atb[r] += cols[r] * b;
            for (int c = 0; c < 3; c++) ata[r * 3 + c] += cols[r] * cols[c];
        }
    }
    free(dy);
    /* risolvi (AtA) x = Atb con fx_dense_solve (matrice 3×4: AtA + rhs) */
    double* aug = (double*)malloc(12 * sizeof(double));
    if (!aug) { *statistic = 0; *pvalue = 0.5; return; }
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) aug[r * 4 + c] = ata[r * 3 + c];
    for (int r = 0; r < 3; r++) aug[r * 4 + 3] = atb[r];
    double* coeffs = fx_dense_solve(aug, 3);
    if (!coeffs) { *statistic = 0; *pvalue = 0.5; return; }
    double gamma = coeffs[1];
    /* rss = Σ(b - A·x)², dof = rows - 3 */
    double rss = 0;
    double* dy2 = (double*)malloc((size_t)(n - 1) * sizeof(double));
    if (dy2)
    {
        for (int t = 1; t < n; t++) dy2[t - 1] = y[t] - y[t - 1];
        for (int i = 0; i < rows; i++)
        {
            int t = i + 2;
            double a0 = 1.0, a1 = y[t - 1], a2 = dy2[t - 2];
            double pred = coeffs[0] * a0 + coeffs[1] * a1 + coeffs[2] * a2;
            double e = dy2[t - 1] - pred;
            rss += e * e;
        }
        free(dy2);
    }
    int dof = rows - 3;
    double sigma2 = dof > 0 ? rss / (double)dof : 0.0;
    /* inv(AtA)[1,1] via fx_mat_inv (layout n×n) */
    double inv[9];
    double stat = 0;
    if (fx_mat_inv(ata, 3, inv))
    {
        double varG = sigma2 * inv[1 * 3 + 1];
        double seGamma = varG > 0 ? sqrt(varG) : 0.0;
        stat = seGamma > 0 ? gamma / seGamma : 0.0;
    }
    *statistic = stat;
    *pvalue = fx_adf_pvalue(stat);
    free(coeffs);
}

void fx_holt_winters(const double* y, int n, double alpha, double beta, double gamma, int period,
                      double* fitted_out, int fitted_n, double* mse_out)
{
    /* Canone CommonLib.Numerics.HoltWintersFit (Advanced.cs): smoothing esponenziale
     * stagionale ADDITIVO. fitted = forecast in-sample (zero prima di `period`, dove il
     * modello non ha ancora stagionalita'), mse = errore quadratico medio su [period, n).
     * NOTA: diversa dalla vecchia versione "level/trend/season a coda" — qui seasonal[t]
     * cresce a ogni t (stagionalita' espansa, non ad anello) e il forecast in-sample usa
     * i componenti al passo t-1 con seasonal[t-period]. */
    if (mse_out) *mse_out = 0;
    if (n <= 0) return;
    if (n == 1)
    {
        if (fitted_out && fitted_n >= 1) fitted_out[0] = 0;
        return;
    }
    int sp = period;
    if (sp <= 0) sp = 1;
    double* level = (double*)malloc((size_t)n * sizeof(double));
    double* trend = (double*)malloc((size_t)n * sizeof(double));
    double* seasonal = (double*)malloc((size_t)n * sizeof(double));
    double* fcast = (double*)malloc((size_t)n * sizeof(double));
    if (!level || !trend || !seasonal || !fcast) { free(level); free(trend); free(seasonal); free(fcast); return; }
    level[0] = y[0];
    trend[0] = y[1] - y[0];
    for (int i = 0; i < sp && i < n; i++) seasonal[i] = y[i] - level[0];
    for (int i = 0; i < n; i++) fcast[i] = 0;
    for (int t = sp; t < n; t++)
    {
        double prevLevel = level[t - 1];
        double prevTrend = trend[t - 1];
        double prevSeasonal = seasonal[t - sp];
        level[t] = alpha * (y[t] - prevSeasonal) + (1 - alpha) * (prevLevel + prevTrend);
        trend[t] = beta * (level[t] - prevLevel) + (1 - beta) * prevTrend;
        seasonal[t] = gamma * (y[t] - level[t] - trend[t]) + (1 - gamma) * prevSeasonal;
        fcast[t] = prevLevel + prevTrend + prevSeasonal;
    }
    if (fitted_out && fitted_n >= n)
        for (int i = 0; i < n; i++) fitted_out[i] = fcast[i];
    if (mse_out)
    {
        double acc = 0; int cnt = 0;
        for (int t = sp; t < n; t++) { double d = y[t] - fcast[t]; acc += d * d; cnt++; }
        *mse_out = cnt > 0 ? acc / (double)cnt : 0;
    }
    free(level); free(trend); free(seasonal); free(fcast);
}

int fx_arima_fit(const double* y, int n, int p, int d, int q,
                  double* ar_c, double* ma_c, double* aic, double* fitted)
{
    for (int i = 0; i < p; i++) ar_c[i] = 0;
    for (int i = 0; i < q; i++) ma_c[i] = 0;
    if (aic) *aic = 0;
    if (n <= 0) return -1;
    int nd = n;
    double* diff = (double*)malloc((size_t)n * sizeof(double));
    if (!diff) return -1;
    for (int i = 0; i < n; i++) diff[i] = y[i];
    for (int k = 0; k < d; k++)
    {
        if (nd < 2) { free(diff); return -1; }
        for (int i = 0; i < nd - 1; i++) diff[i] = diff[i + 1] - diff[i];
        nd--;
    }
    if (fitted) for (int i = 0; i < nd; i++) fitted[i] = 0;
    if (nd < p + q + 5) { free(diff); return -1; }

    /* Passo 1: AR lungo per i residui (proxy MA). */
    double* residuals = (double*)calloc((size_t)nd, sizeof(double));
    if (!residuals) { free(diff); return -1; }
    int m = p + q + 2; if (m < 8) m = 8; if (m > nd / 3) m = nd / 3;
    if (m > 0 && nd > m + 1)
    {
        int rn = nd - m;
        /* AtA (m×m) e Atb (m): Am[i, lag] = diff[t-1-lag], bm[i] = diff[t], t = i+m */
        double* ata = (double*)calloc((size_t)m * m, sizeof(double));
        double* atb = (double*)calloc((size_t)m, sizeof(double));
        if (ata && atb)
        {
            for (int i = 0; i < rn; i++)
            {
                int t = i + m;
                for (int lag = 0; lag < m; lag++)
                {
                    double col = diff[t - 1 - lag];
                    atb[lag] += col * diff[t];
                    for (int lag2 = 0; lag2 < m; lag2++) ata[lag * m + lag2] += col * diff[t - 1 - lag2];
                }
            }
            double* aug = (double*)malloc((size_t)m * (m + 1) * sizeof(double));
            if (aug)
            {
                for (int r = 0; r < m; r++)
                {
                    for (int c = 0; c < m; c++) aug[r * (m + 1) + c] = ata[r * m + c];
                    aug[r * (m + 1) + m] = atb[r];
                }
                double* arLong = fx_dense_solve(aug, m);
                if (arLong)
                {
                    for (int i = 0; i < rn; i++)
                    {
                        int t = i + m;
                        double pred = 0;
                        for (int lag = 0; lag < m; lag++) pred += arLong[lag] * diff[t - 1 - lag];
                        residuals[t] = diff[t] - pred;
                    }
                    free(arLong);
                }
            }
        }
        free(ata); free(atb);
    }

    /* Passo 2: OLS finale p lag AR + q lag residui, con ridge. */
    int start = p; if (q > start) start = q; if (m > start) start = m;
    int rows = nd - start;
    if (rows < p + q + 1) { free(residuals); free(diff); return -1; }
    int cols = p + q;
    double rss = 0;
    if (cols == 0)
    {
        for (int i = start; i < nd; i++) rss += diff[i] * diff[i];
    }
    else
    {
        /* AtA (cols×cols), AtB (cols) */
        double* ata = (double*)calloc((size_t)cols * cols, sizeof(double));
        double* atb = (double*)calloc((size_t)cols, sizeof(double));
        if (!ata || !atb) { free(ata); free(atb); free(residuals); free(diff); return -1; }
        for (int i = 0; i < rows; i++)
        {
            int t = i + start;
            double* rcol = (double*)malloc((size_t)cols * sizeof(double));
            if (!rcol) { free(ata); free(atb); free(residuals); free(diff); return -1; }
            int c = 0;
            for (int lag = 1; lag <= p; lag++) rcol[c++] = diff[t - lag];
            for (int lag = 1; lag <= q; lag++) rcol[c++] = residuals[t - lag];
            for (int r = 0; r < cols; r++)
            {
                atb[r] += rcol[r] * diff[t];
                for (int c2 = 0; c2 < cols; c2++) ata[r * cols + c2] += rcol[r] * rcol[c2];
            }
            free(rcol);
        }
        double trace = 0;
        for (int i = 0; i < cols; i++) trace += ata[i * cols + i];
        double lambda = (cols > 0) ? 1e-8 * (trace / cols) : 0;
        for (int i = 0; i < cols; i++) ata[i * cols + i] += lambda;
            double* aug = (double*)malloc((size_t)cols * (cols + 1) * sizeof(double));
            double* coeffs = NULL;
            if (aug)
            {
                for (int r = 0; r < cols; r++)
                {
                    for (int c = 0; c < cols; c++) aug[r * (cols + 1) + c] = ata[r * cols + c];
                    aug[r * (cols + 1) + cols] = atb[r];
                }
                coeffs = fx_dense_solve(aug, cols);
            }
            if (coeffs)
            {
                for (int i = 0; i < p; i++) ar_c[i] = coeffs[i];
                for (int i = 0; i < q; i++) ma_c[i] = coeffs[p + i];
                /* rss dei residui del fit finale e fitted[start..nd) */
                for (int i = 0; i < rows; i++)
                {
                    int t = i + start;
                    double pred = 0;
                    int c = 0;
                    for (int lag = 1; lag <= p; lag++) pred += coeffs[c++] * diff[t - lag];
                    for (int lag = 1; lag <= q; lag++) pred += coeffs[c++] * residuals[t - lag];
                    double e = diff[t] - pred;
                    rss += e * e;
                    if (fitted) fitted[t] = pred;
                }
                free(coeffs);
            }
            free(ata); free(atb);
    }
    int k = p + q + 1;
    double sigma2 = rows > 0 ? rss / rows : 0.0;
    if (aic && rows > 0 && sigma2 > 0) *aic = (double)rows * log(sigma2) + 2.0 * k;
    free(residuals);
    free(diff);
    return 0;
}

double* fx_yule_walker(const double* acf, int n, int p, long long* out_len)
{
    /* Canone C# (CommonLib.Numerics.YuleWalker, oracolo IL/PCode): l'input è l'ACF;
     * risolve il sistema denso R·c = r con R[i,j] = acf[|i-j|], r[i] = acf[i+1]
     * (eliminazione di Gauss a pivot parziale, come MathNet). NON Levinson-Durbin:
     * quello normalizza acf0 e diverge quando acf[0] è piccolo/zero. */
    if (p == 0) { *out_len = 0; return NULL; }
    if (acf == NULL || n < p + 1) { *out_len = 0; return NULL; }
    double* m = (double*)calloc((size_t)(p * p), sizeof(double));
    double* rhs = (double*)calloc((size_t)p, sizeof(double));
    double* c = (double*)calloc((size_t)p, sizeof(double));
    if (!m || !rhs || !c) { free(m); free(rhs); free(c); *out_len = 0; return NULL; }
    for (int i = 0; i < p; i++) {
        rhs[i] = acf[i + 1];
        for (int j = 0; j < p; j++) {
            int d = i - j; if (d < 0) d = -d;
            m[i * p + j] = acf[d];
        }
    }
    /* forward elimination con pivot parziale */
    for (int col = 0; col < p; col++) {
        int piv = col;
        for (int r0 = col + 1; r0 < p; r0++)
            if (fabs(m[r0 * p + col]) > fabs(m[piv * p + col])) piv = r0;
        if (piv != col) {
            for (int j = col; j < p; j++) { double t = m[col * p + j]; m[col * p + j] = m[piv * p + j]; m[piv * p + j] = t; }
            double t = rhs[col]; rhs[col] = rhs[piv]; rhs[piv] = t;
        }
        double diag = m[col * p + col];
        if (diag == 0.0) { free(m); free(rhs); free(c); *out_len = 0; return NULL; }
        for (int r0 = col + 1; r0 < p; r0++) {
            double f = m[r0 * p + col] / diag;
            if (f == 0.0) continue;
            for (int j = col; j < p; j++) m[r0 * p + j] -= f * m[col * p + j];
            rhs[r0] -= f * rhs[col];
        }
    }
    /* back substitution */
    for (int i = p - 1; i >= 0; i--) {
        double s = rhs[i];
        for (int j = i + 1; j < p; j++) s -= m[i * p + j] * c[j];
        double diag = m[i * p + i];
        if (diag == 0.0) { free(m); free(rhs); free(c); *out_len = 0; return NULL; }
        c[i] = s / diag;
    }
    free(m); free(rhs);
    *out_len = p;
    return c;
}

double* fx_ma_coefficients(const double* res, int n, int q, long long* out_len)
{
    if (n <= 0 || q <= 0 || q >= n) { *out_len = 0; return NULL; }
    double mean = 0;
    for (int i = 0; i < n; i++) mean += res[i];
    mean /= n;
    double var = 0;
    for (int i = 0; i < n; i++) var += (res[i] - mean) * (res[i] - mean);
    var /= n;
    if (var <= 0) { *out_len = 0; return NULL; }
    double* ac = (double*)calloc((size_t)(q + 1), sizeof(double));
    double* out = (double*)calloc((size_t)q, sizeof(double));
    if (!ac || !out) { free(ac); free(out); *out_len = 0; return NULL; }
    for (int l = 0; l <= q; l++)
    {
        double s = 0;
        for (int i = 0; i < n - l; i++) s += (res[i] - mean) * (res[i + l] - mean);
        ac[l] = s / n / var;
    }
    for (int k = 0; k < q; k++) out[k] = ac[k + 1];
    free(ac);
    *out_len = q;
    return out;
}

/* ── wrapper FFI ────────────────────────────────────────────────────────── */
int ofx_ts_yule(const double *acf, int n, int order, double *out)
{
    long long got = 0; double *r; int i;
    if (!out || order < 1) return 0;
    r = fx_yule_walker(acf, n, order, &got);
    if (!r || got < order) { free(r); return 0; }
    for (i = 0; i < order; i++) out[i] = r[i];
    free(r);
    return 1;
}

int ofx_ts_macoeff(const double *res, int n, int q, double *out)
{
    long long got = 0; double *r; int i;
    if (!out || q < 1) return 0;
    r = fx_ma_coefficients(res, n, q, &got);
    if (!r || got < q) { free(r); return 0; }
    for (i = 0; i < q; i++) out[i] = r[i];
    free(r);
    return 1;
}

int ofx_ts_adf(const double *y, int n, double *out)
{
    if (!y || !out || n < 5) return 0;
    fx_adf_test(y, n, &out[0], &out[1]);
    return 1;
}

double ofx_ts_hw(const double *y, int n, double alpha, double beta, double gamma,
                 int period, double *forecast)
{
    double mse = 0.0;
    if (!y || !forecast || n < 1) return 0.0;
    fx_holt_winters(y, n, alpha, beta, gamma, period, forecast, n, &mse);
    return mse;
}

int ofx_ts_arima(const double *y, int n, int p, int d, int q, double *out)
{
    int nd = n, k;
    if (!y || !out || p < 0 || q < 0 || d < 0) return 0;
    for (k = 0; k < d; k++) nd = nd > 1 ? nd - 1 : nd;
    if (nd < 0) nd = 0;
    return fx_arima_fit(y, n, p, d, q, out + 1, out + 1 + p, &out[0],
                        out + 1 + p + q) == 0 ? 1 : 0;
}
