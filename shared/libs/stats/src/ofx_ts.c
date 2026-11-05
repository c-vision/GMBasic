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
