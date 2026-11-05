/*
 * gs_infer.h — inferenza econometrica (da et_al.).
 *
 * Test di stazionarietà KPSS (Kwiatkowski-Phillips-Schmidt-Shin, 1992):
 * H0 = serie stazionaria attorno a una costante (LEVEL) o a un trend (TREND).
 * Statistica sopra il valore critico => rifiuto di stazionarietà.
 */
#ifndef GMBSTATS_GS_INFER_H
#define GMBSTATS_GS_INFER_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GS_KPSS_LEVEL 0
#define GS_KPSS_TREND 1

typedef struct {
    gc_real statistic;
    int bandwidth;
    int observations;
    int deterministic;
    gc_real long_run_variance;
    gc_real critical[4]; /* 10%, 5%, 2.5%, 1% */
} gs_kpss_result;

/* Bandwidth di Newey-West/Bartlett consigliata: floor(4*(n/100)^(1/4)). */
int gs_kpss_bandwidth(int n);

/* Calcola la statistica KPSS. bandwidth < 0 => automatico.
 * Ritorna la statistica (NaN su errore); se out != NULL lo popola. */
gc_real gs_kpss(int n, const gc_real *series, int bandwidth, int deterministic,
                gs_kpss_result *out);

/* ============================ ADF ============================ */
#define GS_ADF_NONE 0
#define GS_ADF_CONSTANT 1
#define GS_ADF_TREND 2

typedef struct {
    gc_real statistic;      /* t-ratio sul coefficiente del livello */
    gc_real coefficient;
    gc_real standard_error;
    int lags, observations, deterministic;
    gc_real aic, bic;
    gc_real critical[3];    /* 1%, 5%, 10% */
} gs_adf_result;

/* Schwert: floor(12 (n/100)^(1/4)). */
int gs_adf_max_lags(int n);
/* Valori critici MacKinnon (1996): level_index 0=1%, 1=5%, 2=10%. */
gc_real gs_adf_critical(int n, int level_index, int deterministic);
/* Regressione ADF su serie[0..n-1]. Ritorna 1 se ok. */
int gs_adf(int n, const gc_real *series, int lags, int deterministic, gs_adf_result *out);

/* =========================== DF-GLS =========================== */
#define GS_DFGLS_CONSTANT 0
#define GS_DFGLS_TREND 1

typedef struct {
    gc_real statistic;
    int lags, observations, deterministic;
    gc_real alpha_bar;
} gs_dfgls_result;

int gs_dfgls(int n, const gc_real *series, int lags, int deterministic, gs_dfgls_result *out);

/* ======================= Engle-Granger ======================= */
typedef struct {
    gc_real statistic;
    int lags, observations, n_regressors, with_constant;
} gs_eg_result;

/* Y: n×k row-major (n osservazioni, k variabili). dependent = colonna dipendente.
 * Regredisce Y[:,dependent] sulle altre + costante, poi ADF sul residuo. */
int gs_engle_granger(int n, int k, const gc_real *Y, int dependent, int lags,
                     int with_constant, gs_eg_result *out);

/* =========================== Johansen =========================== */
typedef struct {
    int n, lags, observations;
    gc_real *eigenvalue;       /* n, decrescenti (correlazioni canoniche al quadrato) */
    gc_real *trace_statistic;  /* n, entry r testa rango <= r */
    gc_real *max_statistic;    /* n, entry r testa rango r vs r+1 */
} gs_johansen_result;

/* data: n×periods row-major (data[k*periods + t]). */
int gs_johansen(int n, int periods, const gc_real *data, int lags, gs_johansen_result *out);
void gs_johansen_free(gs_johansen_result *r);

/* ============================= QLR ============================= */
typedef struct {
    gc_real qlr_t;       /* 2 (sup l_beta - l0) */
    gc_real qlr_tilde;   /* qlr_t / kappa_hat */
    int best_index;
} gs_qlr_result;

int gs_qlr(gc_real l0, const gc_real *l_beta, int n_beta, gc_real kappa_hat, gs_qlr_result *out);
const char *gs_qlr_verdict(gc_real qlr_tilde, gc_real cv10, gc_real cv5, gc_real cv1);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_INFER_H */
