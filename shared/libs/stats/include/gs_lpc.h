/*
 * gs_lpc.h — analisi/sintesi LPC (linear predictive coding).
 *
 * Da xiph/LPCNet (BSD-3) e Silk/Opus (burg.c): stima dei coefficienti LPC con il
 * metodo di Burg (diretto dal segnale), espansione di banda (weighting), filtro
 * di analisi (residuo) e di sintesi. Utile per codec vocali, pitch, DSP.
 */
#ifndef GMBSTATS_GS_LPC_H
#define GMBSTATS_GS_LPC_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Metodo di Burg: stima i coefficienti LPC di ordine p dal segnale x[n].
 * a[0]=1, a[1..p] coefficienti. *err = energia residua finale (se non NULL).
 * Ritorna 0 su successo, -1 su errore. */
int gs_lpc_burg(int n, const gc_real *x, int p, gc_real *a, gc_real *err);

/* Espansione di banda: a[i] *= gamma^i (i=1..p). Stabilizza i filtri LPC. */
void gs_lpc_weighting(int p, gc_real *a, gc_real gamma);

/* Filtro di analisi (errore di predizione): e[t] = x[t] + sum a[i] x[t-i].
 * e[0..n-1]. Ritorna 0 su successo. */
int gs_lpc_residual(int n, const gc_real *x, int p, const gc_real *a, gc_real *e);

/* Filtro di sintesi (all-pole): x[t] = e[t] - sum a[i] x[t-i]. x[0..n-1]. */
int gs_lpc_synthesis(int n, const gc_real *e, int p, const gc_real *a, gc_real *x);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_LPC_H */
