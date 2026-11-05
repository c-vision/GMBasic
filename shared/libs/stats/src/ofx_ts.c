/* ofx_ts.c — shim FFI per TIMESERIES (gs_infer.h): out piatti, dimensioni esplicite. */
#include "gs_infer.h"

/* out(9): statistic, bandwidth, observations, deterministic, long_run_variance,
 *         critical10, critical5, critical25, critical1. */
void ofx_ts_kpss(const double* series, int n, int bandwidth, int deterministic, double* out)
{
    gs_kpss_result r;
    if (!series || !out || n <= 1) return;
    gs_kpss(n, series, bandwidth, deterministic, &r);
    out[0] = r.statistic;
    out[1] = (double)r.bandwidth;
    out[2] = (double)r.observations;
    out[3] = (double)r.deterministic;
    out[4] = r.long_run_variance;
    out[5] = r.critical[0];
    out[6] = r.critical[1];
    out[7] = r.critical[2];
    out[8] = r.critical[3];
}

/* out(3): qlr_t, qlr_tilde, best_index. */
void ofx_ts_qlr(double l0, const double* l_beta, int n_beta, double kappa, double* out)
{
    gs_qlr_result r;
    if (!l_beta || !out || n_beta <= 0) return;
    if (gs_qlr(l0, l_beta, n_beta, kappa, &r)) {
        out[0] = r.qlr_t;
        out[1] = r.qlr_tilde;
        out[2] = (double)r.best_index;
    }
}

/* out(5): statistic, lags, observations, deterministic, alpha_bar. */
void ofx_ts_dfgls(const double* series, int n, int lags, int deterministic, double* out)
{
    gs_dfgls_result r;
    if (!series || !out) return;
    if (gs_dfgls(n, series, lags, deterministic, &r)) {
        out[0] = r.statistic;
        out[1] = (double)r.lags;
        out[2] = (double)r.observations;
        out[3] = (double)r.deterministic;
        out[4] = r.alpha_bar;
    }
}

/* HMM forward: adatta l'ordine BASIC (logpi, logA, logE, K, n) a gs_hmm_forward(n,K,...). */
#include "gs_hmm.h"
double ofx_ts_hmm_forward(const double* logpi, const double* logA, const double* logE, int K, int n)
{
    if (!logpi || !logA || !logE || K < 1 || n < 1) return 0.0 / 0.0;
    return gs_hmm_forward(n, K, logpi, logA, logE);
}

/* Engle-Granger: Y n×k row-major (layout BASIC), out(5) =
 * statistic, lags, observations, n_regressors, with_constant. */
void ofx_ts_engle_granger(const double* Y, int n, int k, int dependent, int lags,
                          int with_constant, double* out)
{
    gs_eg_result r;
    if (!Y || !out || n <= 0 || k < 2) return;
    if (gs_engle_granger(n, k, Y, dependent, lags, with_constant, &r)) {
        out[0] = r.statistic;
        out[1] = (double)r.lags;
        out[2] = (double)r.observations;
        out[3] = (double)r.n_regressors;
        out[4] = (double)r.with_constant;
    }
}

/* Johansen: data n×periods row-major (layout BASIC), transpose to variable-major
 * (data[k*periods+t]) for gs_johansen. out(3 + 3*nvars) =
 * nvars, lags, observations, eigenvalues[nvars], trace[nvars], max[nvars]. */
#include <stdlib.h>
void ofx_ts_johansen(const double* Y, int nvars, int periods, int lags, double* out)
{
    gs_johansen_result r;
    double* data;
    int t, k;
    if (!Y || !out || nvars < 1 || periods <= 0) return;
    data = (double*)malloc((size_t)nvars * (size_t)periods * sizeof(double));
    if (!data) return;
    for (t = 0; t < periods; t++)
        for (k = 0; k < nvars; k++)
            data[k * periods + t] = Y[t * nvars + k];
    if (gs_johansen(nvars, periods, data, lags, &r)) {
        out[0] = (double)r.n;
        out[1] = (double)r.lags;
        out[2] = (double)r.observations;
        for (k = 0; k < nvars; k++) out[3 + k] = r.eigenvalue[k];
        for (k = 0; k < nvars; k++) out[3 + nvars + k] = r.trace_statistic[k];
        for (k = 0; k < nvars; k++) out[3 + 2 * nvars + k] = r.max_statistic[k];
        gs_johansen_free(&r);
    }
    free(data);
}

/* HMM Viterbi: path n interi (LONG a 64 bit lato BASIC). Ritorna il log-lik del path. */
double ofx_ts_hmm_viterbi(const double* logpi, const double* logA, const double* logE,
                          int K, int n, long long* path)
{
    int* tmp;
    double ll;
    int t;
    if (!logpi || !logA || !logE || !path || K < 1 || n < 1) return 0.0 / 0.0;
    tmp = (int*)malloc((size_t)n * sizeof(int));
    if (!tmp) return 0.0 / 0.0;
    ll = gs_hmm_viterbi(n, K, logpi, logA, logE, tmp);
    for (t = 0; t < n; t++) path[t] = (long long)tmp[t];
    free(tmp);
    return ll;
}
