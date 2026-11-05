/* gs_infer.c — inferenza econometrica: KPSS (da et_al.). */
#include "gs_infer.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int gs_kpss_bandwidth(int n)
{
    if (n <= 0) return 0;
    return (int)floor(4.0 * pow((double)n / 100.0, 0.25));
}

/* varianza di lungo periodo con kernel di Bartlett. */
static gc_real bartlett_lrv(const gc_real *e, int n, int bw)
{
    gc_real total = 0.0;
    int t, lag;
    for (t = 0; t < n; t++) total += e[t] * e[t];
    total /= (gc_real)n;
    for (lag = 1; lag <= bw; lag++) {
        gc_real cov = 0.0;
        for (t = lag; t < n; t++) cov += e[t] * e[t - lag];
        cov /= (gc_real)n;
        total += 2.0 * (1.0 - (gc_real)lag / (gc_real)(bw + 1)) * cov;
    }
    return total;
}

gc_real gs_kpss(int n, const gc_real *series, int bandwidth, int deterministic,
                gs_kpss_result *out)
{
    gc_real *e;
    gc_real partial = 0.0, ss = 0.0, lrv, stat;
    int t;
    if (n <= 1 || !series) return NAN;
    if (bandwidth < 0) bandwidth = gs_kpss_bandwidth(n);
    if (bandwidth >= n) bandwidth = n - 1;
    e = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!e) return NAN;
    if (deterministic == GS_KPSS_TREND) {
        /* OLS y = a + b*t (t = 1..n) */
        gc_real st = 0.0, sy = 0.0, stt = 0.0, sty = 0.0, b, a;
        for (t = 0; t < n; t++) {
            gc_real tt = (gc_real)(t + 1);
            st += tt; sy += series[t]; stt += tt * tt; sty += tt * series[t];
        }
        b = ((gc_real)n * sty - st * sy) / ((gc_real)n * stt - st * st);
        a = (sy - b * st) / (gc_real)n;
        for (t = 0; t < n; t++) e[t] = series[t] - a - b * (gc_real)(t + 1);
    } else {
        gc_real mean = 0.0;
        for (t = 0; t < n; t++) mean += series[t];
        mean /= (gc_real)n;
        for (t = 0; t < n; t++) e[t] = series[t] - mean;
    }
    for (t = 0; t < n; t++) { partial += e[t]; ss += partial * partial; }
    lrv = bartlett_lrv(e, n, bandwidth);
    stat = (lrv > 0.0) ? ss / ((gc_real)n * (gc_real)n * lrv) : NAN;
    if (out) {
        out->statistic = stat;
        out->bandwidth = bandwidth;
        out->observations = n;
        out->deterministic = deterministic;
        out->long_run_variance = lrv;
        if (deterministic == GS_KPSS_TREND) {
            out->critical[0] = 0.119; out->critical[1] = 0.146;
            out->critical[2] = 0.176; out->critical[3] = 0.216;
        } else {
            out->critical[0] = 0.347; out->critical[1] = 0.463;
            out->critical[2] = 0.574; out->critical[3] = 0.739;
        }
    }
    free(e);
    return stat;
}

/* ===================== helper OLS (equazioni normali) ===================== */

static int ols_fit(int rows, int cols, const gc_real *X, const gc_real *y,
                   gc_real *beta, gc_real *ssr, int leverage_col, gc_real *leverage_out)
{
    gc_real *XtX, *Xty, *z, *e;
    int i, j, k;
    if (rows <= cols) return 0;
    XtX = (gc_real *)calloc((size_t)cols * cols, sizeof(gc_real));
    Xty = (gc_real *)calloc((size_t)cols, sizeof(gc_real));
    z = (gc_real *)malloc((size_t)cols * sizeof(gc_real));
    e = (gc_real *)malloc((size_t)cols * sizeof(gc_real));
    if (!XtX || !Xty || !z || !e) { free(XtX); free(Xty); free(z); free(e); return 0; }
    for (i = 0; i < rows; i++)
        for (j = 0; j < cols; j++) {
            Xty[j] += X[i * cols + j] * y[i];
            for (k = 0; k < cols; k++) XtX[j * cols + k] += X[i * cols + j] * X[i * cols + k];
        }
    if (gm_linalg_solve(cols, XtX, Xty, beta) != GM_OK) { free(XtX); free(Xty); free(z); free(e); return 0; }
    *ssr = 0.0;
    for (i = 0; i < rows; i++) {
        gc_real f = 0.0, r;
        for (j = 0; j < cols; j++) f += X[i * cols + j] * beta[j];
        r = y[i] - f;
        *ssr += r * r;
    }
    if (leverage_col >= 0 && leverage_out) {
        for (j = 0; j < cols; j++) e[j] = (j == leverage_col) ? 1.0 : 0.0;
        if (gm_linalg_solve(cols, XtX, e, z) != GM_OK) { free(XtX); free(Xty); free(z); free(e); return 0; }
        *leverage_out = z[leverage_col];
    }
    free(XtX); free(Xty); free(z); free(e);
    return 1;
}

/* ============================== ADF ============================== */

int gs_adf_max_lags(int n) { return (int)floor(12.0 * pow((double)n / 100.0, 0.25)); }

gc_real gs_adf_critical(int n, int level_index, int deterministic)
{
    static const double asymptote[2][3] = {{-3.43035,-2.86154,-2.56677},{-3.95877,-3.41049,-3.12705}};
    static const double first[2][3] = {{-6.5393,-2.8903,-1.5384},{-9.0531,-4.3904,-2.5856}};
    static const double second[2][3] = {{-16.786,-4.234,-2.809},{-28.428,-9.036,-3.925}};
    static const double third[2][3] = {{-79.433,-40.040,0.0},{-134.155,-45.374,-22.380}};
    int row = (deterministic == GS_ADF_TREND) ? 1 : 0;
    double nn = (double)n;
    if (n <= 0 || level_index < 0 || level_index > 2) return NAN;
    return (gc_real)(asymptote[row][level_index] + first[row][level_index] / nn
                   + second[row][level_index] / (nn * nn)
                   + third[row][level_index] / (nn * nn * nn));
}

/* ADF su un array già "detrendizzato" (nessun termine deterministico). */
static int adf_core(int n, const gc_real *series, int lags, int first_obs,
                    int deterministic, gs_adf_result *out)
{
    int nd = deterministic, cols = nd + 1 + lags, level_col = nd;
    int rows = n - first_obs, i, row, l;
    gc_real *X, *y, *beta;
    gc_real ssr = 0.0, lev;
    if (rows <= cols || first_obs < 1 + lags) return 0;
    X = (gc_real *)calloc((size_t)rows * cols, sizeof(gc_real));
    y = (gc_real *)malloc((size_t)rows * sizeof(gc_real));
    beta = (gc_real *)malloc((size_t)cols * sizeof(gc_real));
    if (!X || !y || !beta) { free(X); free(y); free(beta); return 0; }
    for (row = 0; row < rows; row++) {
        int t = first_obs + row;
        y[row] = series[t] - series[t - 1];
        if (nd >= 1) X[row * cols + 0] = 1.0;
        if (nd >= 2) X[row * cols + 1] = (gc_real)(t + 1);
        X[row * cols + level_col] = series[t - 1];
        for (l = 1; l <= lags; l++)
            X[row * cols + level_col + l] = series[t - l] - series[t - l - 1];
    }
    if (!ols_fit(rows, cols, X, y, beta, &ssr, level_col, &lev)) { free(X); free(y); free(beta); return 0; }
    {
        gc_real var = ssr / (gc_real)(rows - cols);
        gc_real se = sqrt(var * lev);
        double ms = (double)ssr / (double)rows;
        out->coefficient = beta[level_col];
        out->standard_error = se;
        out->statistic = beta[level_col] / se;
        out->lags = lags;
        out->observations = rows;
        out->deterministic = deterministic;
        out->aic = (gc_real)((double)rows * log(ms) + 2.0 * (double)cols);
        out->bic = (gc_real)((double)rows * log(ms) + (double)cols * log((double)rows));
        for (i = 0; i < 3; i++)
            out->critical[i] = nd >= 1 ? gs_adf_critical(rows, i, deterministic) : NAN;
    }
    free(X); free(y); free(beta);
    return 1;
}

int gs_adf(int n, const gc_real *series, int lags, int deterministic, gs_adf_result *out)
{
    if (n <= 0 || !series || !out) return 0;
    return adf_core(n, series, lags, 1 + lags, deterministic, out);
}

/* ============================= DF-GLS ============================= */

int gs_dfgls(int n, const gc_real *series, int lags, int deterministic, gs_dfgls_result *out)
{
    int nd = (deterministic == GS_DFGLS_TREND) ? 2 : 1;
    gc_real c_bar = (nd == 2) ? 13.5 : 7.0;
    gc_real alpha_bar = 1.0 - c_bar / (gc_real)n;
    gc_real *qd_det, *qd_ser, *det, *psi, *beta;
    gc_real *detrended;
    gs_adf_result ar;
    int t, j, k, i;
    if (n <= nd + lags + 4 || !series || !out) return 0;
    qd_det = (gc_real *)malloc((size_t)n * nd * sizeof(gc_real));
    qd_ser = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    det = (gc_real *)malloc((size_t)n * nd * sizeof(gc_real));
    psi = (gc_real *)malloc((size_t)nd * sizeof(gc_real));
    beta = (gc_real *)malloc((size_t)nd * sizeof(gc_real));
    detrended = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!qd_det || !qd_ser || !det || !psi || !beta || !detrended) {
        free(qd_det); free(qd_ser); free(det); free(psi); free(beta); free(detrended); return 0;
    }
    for (t = 0; t < n; t++) {
        det[t * nd + 0] = 1.0;
        if (nd == 2) det[t * nd + 1] = (gc_real)(t + 1);
    }
    for (t = 0; t < n; t++) {
        for (j = 0; j < nd; j++)
            qd_det[t * nd + j] = (t == 0) ? det[j] : det[t * nd + j] - alpha_bar * det[(t - 1) * nd + j];
        qd_ser[t] = (t == 0) ? series[0] : series[t] - alpha_bar * series[t - 1];
    }
    {
        gc_real ssr, lev = -1;
        if (!ols_fit(n, nd, qd_det, qd_ser, psi, &ssr, -1, &lev)) {
            free(qd_det); free(qd_ser); free(det); free(psi); free(beta); free(detrended); return 0;
        }
    }
    for (t = 0; t < n; t++) {
        gc_real fit = 0.0;
        for (j = 0; j < nd; j++) fit += psi[j] * det[t * nd + j];
        detrended[t] = series[t] - fit;
    }
    (void)k; (void)beta; (void)i;
    if (!adf_core(n, detrended, lags, 1 + lags, GS_ADF_NONE, &ar)) {
        free(qd_det); free(qd_ser); free(det); free(psi); free(beta); free(detrended); return 0;
    }
    out->statistic = ar.statistic;
    out->lags = lags;
    out->observations = ar.observations;
    out->deterministic = deterministic;
    out->alpha_bar = alpha_bar;
    free(qd_det); free(qd_ser); free(det); free(psi); free(beta); free(detrended);
    return 1;
}

/* ========================= Engle-Granger ========================= */

int gs_engle_granger(int n, int k, const gc_real *Y, int dependent, int lags,
                     int with_constant, gs_eg_result *out)
{
    int cols = (k - 1) + (with_constant ? 1 : 0), i, j, row;
    gc_real *X, *resid, *beta, *yc;
    gc_real ssr = 0.0, lev = -1;
    gs_adf_result ar;
    if (n <= cols + lags + 2 || k < 2 || !Y || !out || dependent < 0 || dependent >= k) return 0;
    X = (gc_real *)calloc((size_t)n * cols, sizeof(gc_real));
    resid = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    beta = (gc_real *)malloc((size_t)cols * sizeof(gc_real));
    yc = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!X || !resid || !beta || !yc) { free(X); free(resid); free(beta); free(yc); return 0; }
    for (i = 0; i < n; i++) {
        int c = 0;
        if (with_constant) X[i * cols + c++] = 1.0;
        for (j = 0; j < k; j++) if (j != dependent) X[i * cols + c++] = Y[i * k + j];
        yc[i] = Y[i * k + dependent];
    }
    if (!ols_fit(n, cols, X, yc, beta, &ssr, -1, &lev)) {
        free(X); free(resid); free(beta); free(yc); return 0;
    }
    for (row = 0; row < n; row++) {
        gc_real f = 0.0;
        for (j = 0; j < cols; j++) f += X[row * cols + j] * beta[j];
        resid[row] = Y[row * k + dependent] - f;
    }
    if (!adf_core(n, resid, lags, 1 + lags, with_constant ? GS_ADF_CONSTANT : GS_ADF_NONE, &ar)) {
        free(X); free(resid); free(beta); free(yc); return 0;
    }
    out->statistic = ar.statistic;
    out->lags = lags;
    out->observations = ar.observations;
    out->n_regressors = cols;
    out->with_constant = with_constant;
    free(X); free(resid); free(beta); free(yc);
    return 1;
}

/* ============================= Johansen ============================= */

/* X tale che L X = B, L triangolare inferiore (n×n), B (n×ncol). */
static int lower_solve(int n, int ncol, const gc_real *L, const gc_real *B, gc_real *X)
{
    int i, j, c;
    for (c = 0; c < ncol; c++)
        for (i = 0; i < n; i++) {
            gc_real s = B[i * ncol + c];
            for (j = 0; j < i; j++) s -= L[i * n + j] * X[j * ncol + c];
            X[i * ncol + c] = s / L[i * n + i];
        }
    return 1;
}

/* residuo di ogni colonna di Y (rows×ncol) su X (rows×nreg): Y - X beta. */
static void residualize(int rows, int ncol, int nreg, const gc_real *Y,
                        const gc_real *X, gc_real *R)
{
    int c, i, j;
    gc_real *beta = (gc_real *)malloc((size_t)nreg * sizeof(gc_real));
    gc_real *Xc = (gc_real *)malloc((size_t)rows * nreg * sizeof(gc_real));
    gc_real *yc = (gc_real *)malloc((size_t)rows * sizeof(gc_real));
    for (c = 0; c < ncol; c++) {
        gc_real ssr, lev = -1;
        for (i = 0; i < rows; i++) {
            for (j = 0; j < nreg; j++) Xc[i * nreg + j] = X[i * nreg + j];
            yc[i] = Y[i * ncol + c];
        }
        if (!ols_fit(rows, nreg, Xc, yc, beta, &ssr, -1, &lev)) { R[c] = 0; continue; }
        for (i = 0; i < rows; i++) {
            gc_real f = 0.0;
            for (j = 0; j < nreg; j++) f += X[i * nreg + j] * beta[j];
            R[i * ncol + c] = Y[i * ncol + c] - f;
        }
    }
    free(beta); free(Xc); free(yc);
}

/* S = left' right / rows. */
static void cross_moment(int rows, int cl, int cr, const gc_real *left,
                         const gc_real *right, gc_real *S)
{
    int i, j, k;
    for (i = 0; i < cl; i++)
        for (j = 0; j < cr; j++) {
            gc_real s = 0.0;
            for (k = 0; k < rows; k++) s += left[k * cl + i] * right[k * cr + j];
            S[i * cr + j] = s / (gc_real)rows;
        }
}

int gs_johansen(int n, int periods, const gc_real *data, int lags, gs_johansen_result *out)
{
    int rows = periods - lags - 1, cc = n * lags + 1;
    int i, j, r, row, k, l;
    gc_real *D, *Lv, *COV, *Dr, *Lr, *S00, *S01, *S11, *W, *P, *Lc, *half, *halfT, *sym;
    gc_real *eval, *tmpd, *tmpe, *ev;
    if (n < 1 || lags < 0 || !data || !out || rows <= n + cc) return 0;
    D = (gc_real *)calloc((size_t)rows * n, sizeof(gc_real));
    Lv = (gc_real *)calloc((size_t)rows * n, sizeof(gc_real));
    COV = (gc_real *)calloc((size_t)rows * cc, sizeof(gc_real));
    Dr = (gc_real *)malloc((size_t)rows * n * sizeof(gc_real));
    Lr = (gc_real *)malloc((size_t)rows * n * sizeof(gc_real));
    S00 = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    S01 = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    S11 = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    W = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    P = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    Lc = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    half = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    halfT = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    sym = (gc_real *)malloc((size_t)n * n * sizeof(gc_real));
    eval = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    tmpd = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    tmpe = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    ev = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    if (!D || !Lv || !COV || !Dr || !Lr || !S00 || !S01 || !S11 || !W || !P || !Lc
        || !half || !halfT || !sym || !eval || !tmpd || !tmpe || !ev) {
        free(D); free(Lv); free(COV); free(Dr); free(Lr); free(S00); free(S01); free(S11);
        free(W); free(P); free(Lc); free(half); free(halfT); free(sym); free(eval);
        free(tmpd); free(tmpe); free(ev); return 0;
    }
    for (row = 0; row < rows; row++) {
        int t = lags + 1 + row;
        for (k = 0; k < n; k++) {
            D[row * n + k] = data[k * periods + t] - data[k * periods + t - 1];
            Lv[row * n + k] = data[k * periods + t - 1];
        }
        for (l = 1; l <= lags; l++)
            for (k = 0; k < n; k++)
                COV[row * cc + (l - 1) * n + k] = data[k * periods + t - l] - data[k * periods + t - l - 1];
        COV[row * cc + cc - 1] = 1.0;
    }
    residualize(rows, n, cc, D, COV, Dr);
    residualize(rows, n, cc, Lv, COV, Lr);
    cross_moment(rows, n, n, Dr, Dr, S00);
    cross_moment(rows, n, n, Dr, Lr, S01);
    cross_moment(rows, n, n, Lr, Lr, S11);
    /* W = S00^{-1} S01 (per colonna) */
    for (j = 0; j < n; j++) {
        gc_real *col = (gc_real *)malloc((size_t)n * sizeof(gc_real));
        gc_real *sol = (gc_real *)malloc((size_t)n * sizeof(gc_real));
        for (i = 0; i < n; i++) col[i] = S01[i * n + j];
        if (gm_linalg_solve(n, S00, col, sol) != GM_OK) { free(col); free(sol); goto fail; }
        for (i = 0; i < n; i++) W[i * n + j] = sol[i];
        free(col); free(sol);
    }
    /* P = S01' W  (= S10 S00^{-1} S01) */
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            gc_real s = 0.0;
            for (k = 0; k < n; k++) s += S01[k * n + i] * W[k * n + j];
            P[i * n + j] = s;
        }
    memcpy(Lc, S11, (size_t)n * n * sizeof(gc_real));
    if (gm_linalg_cholesky(n, Lc) != GM_OK) goto fail;
    lower_solve(n, n, Lc, P, half);
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) halfT[i * n + j] = half[j * n + i];
    lower_solve(n, n, Lc, halfT, sym);
    if (gm_linalg_sym_tridiag(n, sym, tmpd, tmpe) != GM_OK) goto fail;
    if (gm_linalg_tridiag_eig(n, tmpd, tmpe, eval) != GM_OK) goto fail;
    out->n = n; out->lags = lags; out->observations = rows;
    out->eigenvalue = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    out->trace_statistic = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    out->max_statistic = (gc_real *)malloc((size_t)n * sizeof(gc_real));
    for (i = 0; i < n; i++) {
        gc_real lam = eval[i];
        if (lam < 0.0) lam = 0.0;
        if (lam > 1.0 - 1e-15) lam = 1.0 - 1e-15;
        out->eigenvalue[i] = lam;
    }
    for (r = 0; r < n; r++) {
        gc_real tot = 0.0;
        for (i = r; i < n; i++) tot += -log(1.0 - out->eigenvalue[i]);
        out->trace_statistic[r] = (gc_real)rows * tot;
        out->max_statistic[r] = (gc_real)rows * -log(1.0 - out->eigenvalue[r]);
    }
    free(D); free(Lv); free(COV); free(Dr); free(Lr); free(S00); free(S01); free(S11);
    free(W); free(P); free(Lc); free(half); free(halfT); free(sym); free(eval);
    free(tmpd); free(tmpe); free(ev);
    return 1;
fail:
    free(D); free(Lv); free(COV); free(Dr); free(Lr); free(S00); free(S01); free(S11);
    free(W); free(P); free(Lc); free(half); free(halfT); free(sym); free(eval);
    free(tmpd); free(tmpe); free(ev);
    return 0;
}

void gs_johansen_free(gs_johansen_result *r)
{
    if (!r) return;
    free(r->eigenvalue); free(r->trace_statistic); free(r->max_statistic);
    r->eigenvalue = r->trace_statistic = r->max_statistic = NULL;
}

/* =============================== QLR =============================== */

int gs_qlr(gc_real l0, const gc_real *l_beta, int n_beta, gc_real kappa_hat, gs_qlr_result *out)
{
    int i, best = -1;
    if (!l_beta || n_beta <= 0 || !out) return 0;
    for (i = 0; i < n_beta; i++)
        if (best < 0 || l_beta[i] > l_beta[best]) best = i;
    out->best_index = best;
    out->qlr_t = 2.0 * (l_beta[best] - l0);
    out->qlr_tilde = (kappa_hat != 0.0) ? out->qlr_t / kappa_hat : out->qlr_t;
    return 1;
}

const char *gs_qlr_verdict(gc_real qlr_tilde, gc_real cv10, gc_real cv5, gc_real cv1)
{
    if (qlr_tilde >= cv1) return "reject at 1%";
    if (qlr_tilde >= cv5) return "reject at 5%";
    if (qlr_tilde >= cv10) return "reject at 10%";
    return "fail to reject at 10%";
}
