/*
 * gs_alpha.h — metodi da AI/Alpha/Researches:
 *  - Stochastic Dominance (FSD/SSD/TSD) tra due distribuzioni di rendimenti
 *    (da "Enhancing Betting Against Beta with Stochastic Dominance").
 */
#ifndef GMBSTATS_GS_ALPHA_H
#define GMBSTATS_GS_ALPHA_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dominanza stocastica di ordine `order` (1=FSD, 2=SSD, 3=TSD) di a su b.
 * Ritorna: +k se a domina b di ordine k (k minimo tra 1..order), -k se b domina a,
 * 0 se nessuna dominanza. Ritorna 0 su input non validi. */
int gs_stochastic_dominance(const gc_real *a, int na, const gc_real *b, int nb, int order);

/* Alpha decay (da "Understanding Alpha Decay"): stima il decadimento esponenziale
 * |alpha(h)| = |alpha_0| e^{-lambda h} via OLS di ln|alpha| su h (IC per orizzonte).
 * alpha[h] a h=0..nh-1. Ritorna 0 se ok (lambda, half-life in out_*), -1 altrimenti. */
int gs_alpha_decay_rate(const gc_real *alpha, int nh, gc_real *out_lambda, gc_real *out_halflife);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_ALPHA_H */
