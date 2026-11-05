/*
 * gs_kelly.h — criterio di Kelly generale (fondazione): dimensione ottimale che
 * massimizza la crescita logaritmica attesa. Versione generica (no dominio betting);
 * la variante betting-specifica (quote/commissioni/multi-esito) sta in gmbbetting.
 */
#ifndef GMBSTATS_GS_KELLY_H
#define GMBSTATS_GS_KELLY_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Scommessa binaria: p = prob. vincita, b = profitto netto per unità puntata
 * (quota decimale - 1). f* = (p*b - (1-p))/b. Ritorna 0 se edge <= 0. */
double gs_kelly_binary(double p, double b);

/* Da quota decimale: f* = (p*dec - 1)/(dec - 1). */
double gs_kelly_from_odds(double p, double dec);

/* Kelly continuo (approssimazione mean-variance): f* = (mu - rf)/sigma2. */
double gs_kelly_continuous(double mu, double sigma2, double rf);

/* Kelly multi-asset: f = Cov^{-1} (mu - rf*1). Cov d x d row-major, mu[0..d-1].
 * Ritorna GC_OK / GC_ERR_SINGULAR / GC_ERR_ARG. */
gc_status gs_kelly_multi(const double *mu, const double *cov, int d, double rf, double *f);

/* Crescita logaritmica attesa di una frazione f su scommessa binaria. */
double gs_kelly_growth(double f, double p, double b);

/* Frazione di Kelly (fractional Kelly) e Kelly con cap. */
double gs_kelly_fraction(double f, double frac);
double gs_kelly_binary_capped(double p, double b, double cap);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_KELLY_H */
