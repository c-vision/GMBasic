/*
 * gs_armagarch.h — ARMA(p,q) con varianza condizionale costante / GARCH(1,1) /
 * EGARCH(1,1), stima di massima verosimiglianza gaussiana (CSS) e selezione
 * dell'ordine/modello via AIC (da paper Forecasting/Radha).
 */
#ifndef GMBSTATS_GS_ARMAGARCH_H
#define GMBSTATS_GS_ARMAGARCH_H

#include "gc_core.h"

#define GS_AG_MAXP 4
#define GS_AG_VAR_CONST  0
#define GS_AG_VAR_GARCH  1
#define GS_AG_VAR_EGARCH 2

typedef struct {
    int p, q, varmodel;             /* varmodel: 0=const, 1=GARCH(1,1), 2=EGARCH(1,1) */
    gc_real c;
    gc_real phi[GS_AG_MAXP];
    gc_real theta[GS_AG_MAXP];
    gc_real sigma2;                 /* var costante (varmodel 0) o ultima var condizionale */
    gc_real omega, alpha, beta, gamma;
    gc_real loglik, aic;
    int nparams;
} gs_armagarch;

/* Stima ARMA(p,q) + varmodel. Ritorna GC_OK / GC_ERR_ARG. */
gc_status gs_armagarch_fit(const gc_real *x, int n, int p, int q, int varmodel,
                           gs_armagarch *out);

/* Seleziona (p,q,varmodel) con p in [0,pmax], q in [0,qmax] minimizzando l'AIC. */
gc_status gs_armagarch_select(const gc_real *x, int n, int pmax, int qmax,
                              gs_armagarch *out_best);

/* Forecast a 1 passo della media (e varianza in out_sigma2, opzionale). */
gc_real gs_armagarch_forecast(const gs_armagarch *m, const gc_real *x, int n,
                              gc_real *out_sigma2);

#endif /* GMBSTATS_GS_ARMAGARCH_H */
