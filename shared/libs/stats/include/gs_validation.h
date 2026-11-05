/*
 * gs_validation.h — validazione statistica di strategie / data snooping.
 *
 * Da Sources/C++ (Libs-Data-Snooping-Tests, root-palvalidator):
 *  - White Reality Check (bootstrap stazionario di Politis-Romano);
 *  - Benjamini-Hochberg FDR (step-up) e Adaptive BH (p-value slope);
 *  - stimatore robusto di scala Qn (Rousseeuw-Croux).
 */
#ifndef GMBSTATS_GS_VALIDATION_H
#define GMBSTATS_GS_VALIDATION_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* White Reality Check. L: matrice T x K row-major (righe=periodi, colonne=
 * strategie), valore piu' alto = performance migliore. H0: max_k E[L_k] <= 0.
 * Bootstrap stazionario (blocchi geometrici, P(nuovo blocco)=1/window).
 * Restituisce 0 e scrive statistica V e p-value. */
int gs_white_reality_check(const double *L, int T, int K, int B, int window,
                           unsigned seed, double *out_v, double *out_pvalue);

/* Hansen SPA (versione studentizzata semplificata). Stessa matrice di White.
 * out_v = statistica max_k sqrt(T)*mean_k/omega_k, p-value dal bootstrap
 * ricentrato su max(0, Z_k). */
int gs_hansen_spa(const double *L, int T, int K, int B, int window,
                  unsigned seed, double *out_v, double *out_pvalue);

/* Benjamini-Hochberg step-up. pvals: m p-value. Scrive reject[k] (1/0) e il
 * numero di rifiuti. Restituisce 0. */
int gs_fdr_bh(const double *pvals, int m, double alpha, int *reject, int *out_nreject);

/* Adaptive Benjamini-Hochberg (Benjamini-Yekutieli 2000, stima m0 dalla pendenza
 * dei p-value ordinati). */
int gs_fdr_bh_adaptive(const double *pvals, int m, double alpha, int *reject, int *out_nreject);

/* Qn di Rousseeuw-Croux (consistente per sigma, 2.2219 * quantile dei |xi-xj|). */
double gs_robust_qn(const double *x, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_VALIDATION_H */

/* Profit hurdle (Harvey-Liu-Zhu): t-soglia per N trial a livello alpha con
 * correlazione rho (N_eff = 1+(N-1)(1-rho)). */
double gs_profit_hurdle(int N, double alpha, double rho);

/* Cold Blood Index (Financial Hacker / Zorro CBI): probabilità in % (0..100) che un
 * drawdown di profondità `depth` e larghezza `width` compaia per caso nella curva
 * `balance` (n punti), su orizzonte `horizon`. P=1-C(M-N,T)/C(M,T) se horizon>width,
 * altrimenti N/M; M=n-width+1, T=horizon-width+1, N=#{G<=-depth}. */
double gs_cold_blood_index(const double *balance, int n, int width, double depth, int horizon);
