/* gs_forecast_core.c — forecast e smoothing puri (gmbstats-timeseries).
 * Estratti da gs_forecast.c; le funzioni finance/ML restano in gmbstats. */
#include "gs_sim.h"
#include "gs_stats.h"
#include "gs_dist.h"
#include "gs_dist2.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- SSA economy SVD helper: per A m×n ritorna U (m×k), s (k), V (n×k) con
 * A = U diag(s) V^T, s decrescente. Gestisce m<n trasponendo. */
int ssa_svd_econ(int m, int n, const double *A, int kmax,
                        double *U, double *s, double *V)
{
    double *B, *Uu, *Vt, *Vt2;
    int i, j;
    if (m >= n) {
        Uu = (double *)malloc((size_t)m * n * sizeof(double));
        Vt = (double *)malloc((size_t)n * n * sizeof(double));
        if (!Uu || !Vt) { free(Uu); free(Vt); return -1; }
        if (gm_linalg_svd(m, n, A, Uu, s, Vt) != GM_OK) { free(Uu); free(Vt); return -1; }
        /* U = Uu (m×n), V = Vt^T (n×n), prendi prime k */
        for (i = 0; i < m; i++)
            for (j = 0; j < kmax; j++) U[i * kmax + j] = Uu[i * n + j];
        for (i = 0; i < n; i++)
            for (j = 0; j < kmax; j++) V[i * kmax + j] = Vt[j * n + i];
        free(Uu); free(Vt);
        return 0;
    } else {
        /* A^T e' n×m, n>=m: svd(A^T)=U2 S Vt2^T -> A = Vt2 S U2^T
         * quindi U_A = Vt2^T (n×m), V_A = U2 (m×m) */
        B = (double *)malloc((size_t)n * m * sizeof(double));
        Uu = (double *)malloc((size_t)n * m * sizeof(double));
        Vt2 = (double *)malloc((size_t)m * m * sizeof(double));
        if (!B || !Uu || !Vt2) { free(B); free(Uu); free(Vt2); return -1; }
        for (i = 0; i < n; i++)
            for (j = 0; j < m; j++) B[i * m + j] = A[j * n + i];
if (gm_linalg_svd(n, m, B, Uu, s, Vt2) != GM_OK) { free(B); free(Uu); free(Vt2); return -1; }
        /* X = Vt2^T S Uu^T: U_A = Vt2^T (m×m), V_A = Uu (n×m) */
        for (i = 0; i < m; i++)
            for (j = 0; j < kmax; j++) U[i * kmax + j] = Vt2[j * m + i];
        for (i = 0; i < n; i++)
            for (j = 0; j < kmax; j++) V[i * kmax + j] = Uu[i * m + j];
        free(B); free(Uu); free(Vt2);
        return 0;
    }
}

int gs_ssa(const double *x, int n, int window, int rank_mode, int k_in,
           double threshold, double *rec, int *k_out)
{
    double *X, *U, *s, *V, *Xh;
    int N = window, M, i, j, k, kmax;
    if (!x || !rec || n < 4 || N < 2 || N > n) return -1;
    M = n / N;
    if (M < 1) return -1;
    kmax = N < M ? N : M;
    if (kmax < 1) return -1;
    X = (double *)malloc((size_t)N * M * sizeof(double));
    U = (double *)malloc((size_t)N * kmax * sizeof(double));
    s = (double *)malloc((size_t)kmax * sizeof(double));
    V = (double *)malloc((size_t)M * kmax * sizeof(double));
    Xh = (double *)malloc((size_t)N * M * sizeof(double));
    if (!X || !U || !s || !V || !Xh) { free(X); free(U); free(s); free(V); free(Xh); return -1; }
    for (j = 0; j < M; j++)
        for (i = 0; i < N; i++) X[i * M + j] = x[j * N + i];
    if (ssa_svd_econ(N, M, X, kmax, U, s, V) != 0) { free(X); free(U); free(s); free(V); free(Xh); return -1; }
    if (rank_mode == 1) {
        double b = (double)N / M, omega, thre, med;
        double *ss;
        ss = (double *)malloc((size_t)kmax * sizeof(double));
        for (i = 0; i < kmax; i++) ss[i] = s[i];
        /* mediana */
        for (i = 0; i < kmax; i++) for (j = i + 1; j < kmax; j++) if (ss[j] < ss[i]) { double t = ss[i]; ss[i] = ss[j]; ss[j] = t; }
        med = kmax > 0 ? (kmax % 2 ? ss[kmax / 2] : 0.5 * (ss[kmax / 2 - 1] + ss[kmax / 2])) : 0.0;
        omega = 0.56 * b * b * b - 0.95 * b * b + 1.43 + 1.82 * b;
        thre = omega * med;
        k = 0;
        for (i = 0; i < kmax; i++) if (s[i] > thre) k++;
        if (k < 1) k = 1;
        if (k > kmax) k = kmax;
        free(ss);
    } else if (rank_mode == 2) {
        double tot = 0.0, acc = 0.0;
        k = kmax;
        for (i = 0; i < kmax; i++) tot += s[i] * s[i];
        for (i = 0; i < kmax; i++) {
            acc += s[i] * s[i];
            if (tot > 0 && acc / tot > threshold) { k = i + 1; break; }
        }
    } else {
        k = k_in;
        if (k < 1) k = 1;
        if (k > kmax) k = kmax;
    }
    if (k_out) *k_out = k;
    /* Xh = U_k diag(s_k) V_k^T */
    for (i = 0; i < N; i++)
        for (j = 0; j < M; j++) {
            double v = 0.0; int t;
            for (t = 0; t < k; t++) v += U[i * kmax + t] * s[t] * V[j * kmax + t];
            Xh[i * M + j] = v;
        }
    for (i = 0; i < n; i++) rec[i] = 0.0;
    for (j = 0; j < M; j++)
        for (i = 0; i < N; i++) {
            int idx = j * N + i;
            if (idx < n) rec[idx] = Xh[i * M + j];
        }
    free(X); free(U); free(s); free(V); free(Xh);
    return 0;
}

/* ---- SSA forecast ricorrente (mSSA): ricostruisce, calcola pesi w =
 * A^+^T b (A = prime N-1 righe di Xh, b = ultima riga), poi
 * y_{t+h} = sum_j w_j z_{t+h-j} + bias, bias=(1-sum w)*mu (mu=0 raw). */
int gs_ssa_forecast(const double *x, int n, int window, int k, int h,
                    double *forecast)
{
    int N = window, M, kmax, i, j, no_coeff;
    double *X, *U, *s, *V, *Xh, *w, *seed;
    if (!x || !forecast || n < 4 || N < 2 || N > n || h < 1 || k < 1) return -1;
    M = n / N;
    kmax = N < M ? N : M;
    if (k > kmax) k = kmax;
    no_coeff = N - 1;
    X = (double *)malloc((size_t)N * M * sizeof(double));
    U = (double *)malloc((size_t)N * kmax * sizeof(double));
    s = (double *)malloc((size_t)kmax * sizeof(double));
    V = (double *)malloc((size_t)M * kmax * sizeof(double));
    Xh = (double *)malloc((size_t)N * M * sizeof(double));
    w = (double *)malloc((size_t)no_coeff * sizeof(double));
    seed = (double *)malloc((size_t)(no_coeff + h) * sizeof(double));
    if (!X || !U || !s || !V || !Xh || !w || !seed) {
        free(X); free(U); free(s); free(V); free(Xh); free(w); free(seed);
        return -1;
    }
    for (j = 0; j < M; j++)
        for (i = 0; i < N; i++) X[i * M + j] = x[j * N + i];
    if (ssa_svd_econ(N, M, X, kmax, U, s, V) != 0) { free(X); free(U); free(s); free(V); free(Xh); free(w); free(seed); return -1; }
    for (i = 0; i < N; i++)
        for (j = 0; j < M; j++) {
            double v = 0.0; int t;
            for (t = 0; t < k; t++) v += U[i * kmax + t] * s[t] * V[j * kmax + t];
            Xh[i * M + j] = v;
        }
    /* A = prime N-1 righe di Xh (N-1)×M; pseudo-inversa di A via SVD */
    {
        int n1 = N - 1;
        double *Ap, *Ua, *sa, *Va, *pinv, *bvec2;
        Ap = (double *)malloc((size_t)n1 * M * sizeof(double));
        Ua = (double *)malloc((size_t)n1 * kmax * sizeof(double));
        sa = (double *)malloc((size_t)kmax * sizeof(double));
        Va = (double *)malloc((size_t)M * kmax * sizeof(double));
        pinv = (double *)malloc((size_t)M * n1 * sizeof(double));
        bvec2 = (double *)malloc((size_t)M * sizeof(double));
        if (!Ap || !Ua || !sa || !Va || !pinv || !bvec2) {
            free(Ap); free(Ua); free(sa); free(Va); free(pinv); free(bvec2);
            free(X); free(U); free(s); free(V); free(Xh); free(w); free(seed);
            return -1;
        }
        for (i = 0; i < n1; i++)
            for (j = 0; j < M; j++) Ap[i * M + j] = Xh[i * M + j];
        if (ssa_svd_econ(n1, M, Ap, kmax, Ua, sa, Va) != 0) {
            free(Ap); free(Ua); free(sa); free(Va); free(pinv); free(bvec2);
            free(X); free(U); free(s); free(V); free(Xh); free(w); free(seed);
            return -1;
        }
        /* pinv(A) = V_k diag(1/sa) U_k^T  (M×n1);  w = pinv(A)^T b = U_k diag(1/sa) V_k^T b */
        for (j = 0; j < M; j++) bvec2[j] = Xh[(N - 1) * M + j];
        for (i = 0; i < no_coeff; i++) {
            double v = 0.0; int t;
            for (t = 0; t < k; t++) {
                double inv = sa[t] > 1e-12 ? 1.0 / sa[t] : 0.0;
                double d = 0.0; int q;
                for (q = 0; q < M; q++) d += Va[q * kmax + t] * bvec2[q];
                v += Ua[i * kmax + t] * inv * d;
            }
            w[i] = v;
        }
        free(Ap); free(Ua); free(sa); free(Va); free(pinv); free(bvec2);
    }
    /* seed = ultimi no_coeff valori grezzi */
    for (i = 0; i < no_coeff; i++) seed[i] = x[n - no_coeff + i];
    for (j = 0; j < h; j++) {
        double v = 0.0;
        for (i = 0; i < no_coeff; i++) v += w[i] * seed[j + i];
        seed[no_coeff + j] = v;   /* bias=0 (dati raw, non z-scored) */
    }
    for (j = 0; j < h; j++) forecast[j] = seed[no_coeff + j];
    free(X); free(U); free(s); free(V); free(Xh); free(w); free(seed);
    return 0;
}

/* ---- SES (simple exponential smoothing) ricorsivo (timemachines). rho: rate
 * (0..1), alpha=1-rho. Ritorna la previsione costante h passi. */
double gs_ses_forecast(const double *x, int n, double rho)
{
    double s;
    int i;
    if (!x || n < 1 || rho < 0.0 || rho > 1.0) return 0.0 / 0.0;
    s = x[0];
    for (i = 1; i < n; i++) s = rho * s + (1.0 - rho) * x[i];
    return s;
}

/* ---- Regressione lineare online O(1) (timemachines regress_one_helper).
 * Richiede stato: x_avg, y_avg, Sx, Sxy. Ogni passo aggiunge coppia (a,y).
 * Ritorna alpha (intercetta) e beta (pendenza) aggiornati. */
void gs_online_ols_init(double *state)
{
    state[0] = 0.0;  /* x_avg */
    state[1] = 0.0;  /* y_avg */
    state[2] = 0.0;  /* Sx */
    state[3] = 0.0;  /* Sxy */
    state[4] = 0.0;  /* n */
}
void gs_online_ols_update(double *state, double a, double y)
{
    double n = state[4] + 1.0;
    double x_avg = state[0], y_avg = state[1];
    double new_x = x_avg + (a - x_avg) / n;
    double new_y = y_avg + (y - y_avg) / n;
    double xc = a - new_x;
    state[2] += xc * xc;
    state[3] += xc * (y - new_y);
    state[0] = new_x; state[1] = new_y; state[4] = n;
}
void gs_online_ols_fit(const double *state, double *alpha, double *beta)
{
    double Sx = state[2], Sxy = state[3];
    if (Sx > 1e-15) {
        *beta = Sxy / Sx;
        *alpha = state[1] - *beta * state[0];
    } else {
        *beta = 0.0;
        *alpha = state[1];
    }
}

/* ---- Random walk con drift: regressione OLS sulle differenze prime, forecast
 * x_{n+h} = x_n + h*drift, std accumulato sqrt(h)*sigma. */
int gs_drift_forecast(const double *x, int n, int h, double *forecast)
{
    double sum_t = 0.0, sum_y = 0.0, sum_tt = 0.0, sum_ty = 0.0;
    double beta, alpha, resid = 0.0;
    int i, m = n - 1;
    if (!x || !forecast || n < 3 || h < 1) return -1;
    for (i = 0; i < m; i++) {
        double t = i + 1.0, y = x[i + 1] - x[i];
        sum_t += t; sum_y += y; sum_tt += t * t; sum_ty += t * y;
    }
    beta = (m * sum_ty - sum_t * sum_y) / (m * sum_tt - sum_t * sum_t + 1e-30);
    alpha = (sum_y - beta * sum_t) / m;
    for (i = 0; i < m; i++) {
        double y = x[i + 1] - x[i], e = y - (alpha + beta * (i + 1.0));
        resid += e * e;
    }
    resid = sqrt(resid / (m - 1 > 0 ? m - 1 : 1));
    (void)resid;
    for (i = 0; i < h; i++) forecast[i] = x[n - 1] + (i + 1.0) * beta;
    return 0;
}

/* ---- Location robusta Huber generalizzata (timemachines hubermean):
 * minimizza f(mu) = mean(1/a * log(exp(a(x-mu)) + exp(-a(x-mu)) + b)),
 * gradiente g(mu) = -mean((e^{ay}-e^{-ay})/(e^{ay}+e^{-ay}+b)). Newton. */
int gs_wiener_hopf(const double *x, const double *y, int n, int order,
                   double *w, double *mse)
{
    double *r, *p, *ac;
    int i, k, lag;
    if (!x || !y || !w || n < order + 2 || order < 1) return -1;
    ac = (double *)malloc((size_t)(order + 1) * sizeof(double));
    r = (double *)malloc((size_t)(order + 1) * sizeof(double));
    p = (double *)malloc((size_t)order * sizeof(double));
    if (!ac || !r || !p) { free(ac); free(r); free(p); return -1; }
    for (lag = 0; lag <= order; lag++) {
        double s = 0.0; int cnt = 0;
        for (i = 0; i < n - order; i++) { s += x[i + lag] * x[i]; cnt++; }
        ac[lag] = cnt > 0 ? s / cnt : 0.0;
    }
    for (lag = 0; lag < order; lag++) {
        double s = 0.0; int cnt = 0;
        for (i = 0; i < n - order; i++) { s += y[i + lag + 1] * x[i]; cnt++; }
        p[lag] = cnt > 0 ? s / cnt : 0.0;
    }
    {
        double *A;
        A = (double *)malloc((size_t)order * order * sizeof(double));
        if (!A) { free(ac); free(r); free(p); return -1; }
        for (i = 0; i < order; i++)
            for (k = 0; k < order; k++) A[i * order + k] = ac[i > k ? i - k : k - i];
        if (gm_linalg_solve(order, A, p, w) != GM_OK) {
            free(A); free(ac); free(r); free(p);
            return -1;
        }
        free(A);
    }
    if (mse) {
        double e = 0.0;
        for (i = 0; i < n - order; i++) {
            double pred = 0.0;
            for (k = 0; k < order; k++) pred += w[k] * x[i + k];
            e += (y[i + 1] - pred) * (y[i + 1] - pred);
        }
        *mse = e / (n - order);
    }
    free(ac); free(r); free(p);
    return 0;
}

/* ---- Smoothing con l'equazione del calore (FTCS esplicito, PDF): u_i^{j+1} =
 * u_i^j + a*k*(u_{i+1}^j - 2u_i^j + u_{i-1}^j), estremi fissi. */
int gs_heat_smooth(const double *x, int n, double a, double k, int steps,
                   double *out)
{
    double *u, *v;
    int i, s;
    if (!x || !out || n < 3 || a <= 0 || k <= 0 || a * k > 0.5 || steps < 1) return -1;
    u = (double *)malloc((size_t)n * sizeof(double));
    v = (double *)malloc((size_t)n * sizeof(double));
    if (!u || !v) { free(u); free(v); return -1; }
    for (i = 0; i < n; i++) u[i] = x[i];
    for (s = 0; s < steps; s++) {
        v[0] = u[0]; v[n - 1] = u[n - 1];
        for (i = 1; i < n - 1; i++)
            v[i] = u[i] + a * k * (u[i + 1] - 2.0 * u[i] + u[i - 1]);
        for (i = 0; i < n; i++) u[i] = v[i];
    }
    for (i = 0; i < n; i++) out[i] = u[i];
    free(u); free(v);
    return 0;
}

/* ---- p-value test di permutazione Monte Carlo (ETF_predictions). */
int gs_savgol_smooth(const double *x, int n, int win, int order, double *out)
{
    double *a;
    int i, j, w0;
    if (!x || !out || n < win || win < 3 || order < 0) return -1;
    w0 = win / 2;
    /* fit polinomiale per ogni finestra (minimi quadrati) */
    a = (double *)malloc((size_t)(win * win) * sizeof(double));
    if (!a) return -1;
    for (i = 0; i < n; i++) {
        int lo = i - w0, hi = i + w0, cnt = 0;
        double m[3], sumsq = 0;
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        if (lo < 0) { lo = 0; }
        if (hi >= n) { hi = n - 1; }
        /* regressione lineare locale per la derivata (order 1 usato qui) */
        for (j = lo; j <= hi; j++) { double t = j - i; sx += t; sy += x[j]; sxx += t * t; sxy += t * x[j]; cnt++; }
        if (cnt < 2) { out[i] = x[i]; continue; }
        {
            double den = cnt * sxx - sx * sx;
            double slope = den > 1e-12 ? (cnt * sxy - sx * sy) / den : 0.0;
            double inter = (sy - slope * sx) / cnt;
            m[0] = inter; m[1] = slope;
            out[i] = m[0];  /* valore smussato */
            (void)sumsq;
        }
        (void)a; (void)order;
    }
    free(a);
    return 0;
}

/* ==================== Sentiment, ML, scoring (Sentiment/) ==================== */

/* ---- Proiezione sul simplesso: w = max(v - theta, 0), somma = z. ---- */
/* ---- SVM multiclass one-vs-rest SGD (hinge loss). w_c: kxd. ---- */
double gs_dtw_idw_predict(const double *price, int n, int L, int k)
{
    double *best_d;
    double *best_ret;
    double *varP, *varV;
    double sum1 = 0.0, sum2 = 0.0, pred;
    double m;
    int i, j;
    if (!price || n < 2 * L + 2 || L < 2 || k < 1) return 0.0 / 0.0;
    best_d = (double *)malloc((size_t)k * sizeof(double));
    best_ret = (double *)malloc((size_t)k * sizeof(double));
    varP = (double *)malloc((size_t)(n - L + 1) * sizeof(double));
    varV = (double *)malloc((size_t)(n - L + 1) * sizeof(double));
    if (!best_d || !best_ret || !varP || !varV) { free(best_d); free(best_ret); free(varP); free(varV); return 0.0; }
    for (i = 0; i < k; i++) { best_d[i] = 1e300; best_ret[i] = 0.0; }
    for (i = 0; i <= n - L; i++) { /* finestra storica [i, i+L) */
        double mn = price[i], mx = price[i];
        int a, b, t;
        for (t = 0; t < L; t++) { double p = price[i + t]; if (p < mn) mn = p; if (p > mx) mx = p; }
        /* normalizzazione [0,1] per la varianza del prezzo (proxy) */
        varP[i] = mx - mn > 0 ? (mx - mn) : 1.0;
        varV[i] = 1.0;
        /* DTW vs ultima finestra */
        {
            double *D = (double *)malloc((size_t)(L + 1) * (L + 1) * sizeof(double));
            double *q = (double *)malloc((size_t)L * sizeof(double));
            for (t = 0; t < L; t++) q[t] = price[n - L + t];
            if (!D || !q) { free(D); free(q); continue; }
            for (a = 0; a <= L; a++) for (b = 0; b <= L; b++) D[a * (L + 1) + b] = 1e300;
            D[0] = 0.0;
            for (a = 1; a <= L; a++)
                for (b = 1; b <= L; b++) {
                    double d2 = (price[i + a - 1] - q[b - 1]) * (price[i + a - 1] - q[b - 1]) / varP[i];
                    double v1 = D[(a - 1) * (L + 1) + b], v2 = D[a * (L + 1) + b - 1], v3 = D[(a - 1) * (L + 1) + b - 1];
                    double m2 = v1 < v2 ? v1 : v2;
                    if (v3 < m2) m2 = v3;
                    D[a * (L + 1) + b] = d2 + m2;
                }
            m = D[L * (L + 1) + L];
            free(D); free(q);
        }
        /* conserva i k migliori (distanza minima) */
        for (int jj = 0; jj < k; jj++) {
            if (m < best_d[jj]) {
                int t2;
                for (t2 = k - 1; t2 > jj; t2--) { best_d[t2] = best_d[t2 - 1]; best_ret[t2] = best_ret[t2 - 1]; }
                best_d[jj] = m;
                best_ret[jj] = i + L < n ? price[i + L] / price[i + L - 1] - 1.0 : 0.0;
                break;
            }
        }
    }
    for (j = 0; j < k; j++) {
        if (best_d[j] < 1e299 && best_d[j] > 1e-9) { sum1 += best_ret[j] / best_d[j]; sum2 += 1.0 / best_d[j]; }
    }
    pred = sum2 > 0 ? sum1 / sum2 : 0.0;
    free(best_d); free(best_ret); free(varP); free(varV);
    return pred;
}

/* ---- SRF signal: Sharpe-gated multi-horizon TSMOM. ---- */
double gs_diebold_mariano(const double *actual, const double *p1,
                          const double *p2, int n, int h, int crit, double power)
{
    double dbar = 0.0, var = 0.0, dm, *d;
    int T = n, k, i;
    if (!actual || !p1 || !p2 || n < 2 || h < 1) return 0.0 / 0.0;
    d = (double *)malloc((size_t)T * sizeof(double));
    if (!d) return 0.0 / 0.0;
    for (i = 0; i < T; i++) {
        double e1, e2, l1, l2;
        l1 = actual[i] - p1[i]; l2 = actual[i] - p2[i];
        if (crit == 1) { e1 = fabs(l1); e2 = fabs(l2); }
        else if (crit == 2) { e1 = fabs(l1 / (actual[i] + 1e-12)); e2 = fabs(l2 / (actual[i] + 1e-12)); }
        else if (crit == 3) { e1 = pow(fabs(l1), power); e2 = pow(fabs(l2), power); }
        else { e1 = l1 * l1; e2 = l2 * l2; }
        d[i] = e1 - e2;
        dbar += d[i];
    }
    dbar /= T;
    for (k = 0; k < h; k++) {
        double gam = 0.0;
        for (i = 0; i < T - k; i++) gam += (d[i + k] - dbar) * (d[i] - dbar);
        gam /= T;
        if (k == 0) var += gam;
        else var += 2.0 * gam;
    }
    var /= T;
    if (var <= 0) { free(d); return 0.0 / 0.0; }
    dm = dbar / sqrt(var);
    {
        double adj = sqrt((double)(T + 1 - 2 * h + h * (h - 1) / (double)T) / T);
        dm *= adj;
    }
    free(d);
    return dm;
}

/* ---- Predittore FIR Wiener-Hopf (Stocks-master): w = R^{-1} p, R autocorr
 * della feature, p cross-corr col target. R e' Toeplitz -> Levinson. */
int gs_precision_ensemble(const double *y, int k, double e, int nmod,
                          double *forecast)
{
    double y0, x = 0.0, xstd = 1e300, totw = 0.0;
    int i, j;
    if (!y || !forecast || k < 1 || nmod < 1) return -1;
    y0 = y[0];
    for (j = 0; j < nmod; j++) {
        double xj = y[1 + 2 * j], sj = fabs(y[2 + 2 * j]);
        double w = sj > 1e-15 ? pow(1.0 / sj, e) : 1e15;
        x += w * xj;
        if (sj < xstd) xstd = sj;
        totw += w;
    }
    if (totw <= 0) return -1;
    x /= totw;
    for (i = 0; i < k; i++)
        forecast[i] = y0 + ((double)(i + 1) / k) * (x - y0);
    return 0;
}

/* ---- Test Diebold-Mariano (dm_test.py): confronta due forecast.
 * crit: 0=MSE, 1=MAD, 2=MAPE, 3=poly(power). h: orizzonte per HAC.
 * Ritorna statistica DM. */
