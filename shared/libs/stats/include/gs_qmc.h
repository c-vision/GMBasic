/*
 * gs_qmc.h — strumenti quasi-Monte Carlo (DAL #174, clean-room MIT):
 * sequenza di Sobol e Brownian bridge. Solo libc/libm.
 */
#ifndef GMBSTATS_GS_QMC_H
#define GMBSTATS_GS_QMC_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Sobol (low-discrepancy, unit-init, 6 dimensioni) ---- */
#define GS_SOBOL_MAXDIM 6
#define GS_SOBOL_MAXBIT 30

typedef struct {
    int maxdim;                                  /* dimensioni attive */
    int in;                                      /* punti emessi finora */
    unsigned long ix[GS_SOBOL_MAXDIM];           /* stato bit per dimensione */
    unsigned long dir[GS_SOBOL_MAXBIT][GS_SOBOL_MAXDIM];  /* direzioni intere */
} gs_sobol;

/* Inizializza con maxdim (1..6) dimensioni. Numeri di direzione generati dai
 * polinomi primitivi con inizializzazione unitaria (m_1..m_s = 1); la
 * dimensione 1 è la sequenza classica (bit-reversal di Gray). 0/-1. */
int gs_sobol_init(gs_sobol *s, int maxdim);

/* Prossimo punto in [0,1)^maxdim (prima chiamata = punto 1). */
void gs_sobol_next(gs_sobol *s, double *out);

/* ---- Brownian bridge ---- */
/* Percorso Browniano su griglia t[0..n] (ordinata, t[0] = 0):
 * w[0] = 0, w[n] = sqrt(t[n])*z[0], punti interni condizionati ai vicini già
 * costruiti (fill in ordine di intervallo più grande). z[0..n-1] = normali
 * standard. n >= 1. */
void gs_mc_brownian_bridge(const gc_real *t, int n, const gc_real *z, gc_real *w);

/* ---- Tauchen ---- */
/* Discretizza l'AR(1) y' = mu*(1-rho) + rho*y + eps (eps ~ N(0, sigma^2)) in
 * una catena di Markov a n stati. A[n] = stati (centrati su mu, ampiezza
 * m*sigma_y), P[n*n] row-major = probabilità di transizione. m = ampiezza in
 * dev. std stazionarie (default 3). Da estimating-dynamic-corporate (#176,
 * clean-room). 0/-1. */
int gs_tauchen(double mu, double rho, double sigma, int n, double m,
               double *A, double *P);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_QMC_H */