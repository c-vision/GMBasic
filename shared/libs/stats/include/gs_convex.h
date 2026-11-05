/*
 * gs_convex.h — convessità di una serie di rendimenti rispetto a un benchmark.
 *  - regressione quadratica  y = a + b*x + g*x^2  (g = convessità), errori HC0;
 *  - capture up/down (upside/downside capture, %).
 * Statistica pura (nessun tipo/semantica di dominio).
 */
#ifndef GMBSTATS_GS_CONVEX_H
#define GMBSTATS_GS_CONVEX_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gc_real a;     /* intercetta */
    gc_real b;     /* coefficiente lineare (beta) */
    gc_real g;     /* convessità (coefficiente quadratico) */
    gc_real r2;    /* R^2 in-sample */
    gc_real se_g;  /* errore standard robusto (HC0/White) di g */
    gc_real t_g;   /* t = g / se_g  (0 se se_g == 0) */
    int     n;     /* osservazioni usate */
} gs_convex_fit;

/* Fit OLS di y = a + b*x + g*x^2 con errori standard HC0 (White).
 * Ritorna GC_OK, GC_ERR_ARG (input non valido) o GC_ERR_SINGULAR (X'X non invertibile). */
gc_status gs_convex_fit_ols(const gc_real *y, const gc_real *x, int n, gs_convex_fit *out);

typedef struct {
    gc_real up_capture;    /* % = 100 * sum(y | x>0) / sum(x | x>0) */
    gc_real down_capture;  /* % = 100 * sum(y | x<0) / sum(x | x<0) */
    int     n_up;          /* numero osservazioni con x>0 */
    int     n_down;        /* numero osservazioni con x<0 */
} gs_convex_capture;

/* Upside/downside capture. Se un lato è vuoto, capture = 0 e conteggio = 0. */
gc_status gs_convex_capture_calc(const gc_real *y, const gc_real *x, int n, gs_convex_capture *out);

/* Punteggio di convessità cross-sectional: coeff. quadratico di logRet(titolo) ~ indice. */
typedef struct {
    gc_real g;     /* convessità (coefficiente quadratico) */
    gc_real t_g;   /* t = g / se_g (0 se se_g == 0) */
    gc_real r2;    /* R^2 in-sample */
    int     n;     /* osservazioni usate (0 se il fit è degenerato) */
} gs_convex_score;

/* Indice cross-sectional = media per asset dei rendimenti.
 * Layout `rets` row-major [t*N + a], t in [0,n_obs), a in [0,n_assets).
 * Richiede n_obs >= 3, n_assets >= 1. `out_index` ha n_obs elementi. */
gc_status gs_convex_index_mean(const gc_real *rets, int n_obs, int n_assets, gc_real *out_index);

/* Per ciascun asset: fit quadratico logRet(a) ~ indice cross-section (interno).
 * `out_scores` ha n_assets elementi; un fit degenerato produce score a zero con n=0. */
gc_status gs_convex_cross_section(const gc_real *rets, int n_obs, int n_assets, gs_convex_score *out_scores);

/* Ordina gli indici 0..n-1 per convessità **decrescente** (i più convessi prima).
 * Tie-break per indice crescente (deterministico). `out_order` ha n elementi. */
gc_status gs_convex_rank_desc(const gs_convex_score *scores, int n, int *out_order);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_CONVEX_H */
