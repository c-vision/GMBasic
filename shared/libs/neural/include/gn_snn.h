/*
 * gn_snn.h — utilità per Spiking Neural Networks (da izhikevich/SNN).
 *
 * Neuroni a look-up table (LUT) indicizzati da confronti con anchor, con
 * funzione surrogata e gradiente surrogato per il passaggio all'indietro.
 */
#ifndef GMBNEURAL_GN_SNN_H
#define GMBNEURAL_GN_SNN_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Surrogato (da izhikevich/SNN): U(x) = 0.5/(1+|x|). */
gn_real gn_snn_surrogate(gn_real x);
/* Gradiente surrogato: Up(x) = -0.5*sign(x)/(1+|x|)^2 (sign(0) = -1). */
gn_real gn_snn_surrogate_grad(gn_real x);

/* Indice LUT da N_C confronti con anchor: bit r = 1 se x[a[r]] - x[b[r]] > 0.
 * Ritorna l'indice intero (0..2^N_C-1). */
int gn_snn_lut_index(const gn_real *x, const int *a, const int *b, int n_c);

/* Forward di un neurone LUT: y[k] += S[i][j[i]*y_dim + k] per i=0..n_t-1.
 * S: n_t tabelle (ciascuna 2^n_c × y_dim); j: n_t indici; y: y_dim (accumulato). */
void gn_snn_lut_forward(const gn_real *const *S, const int *j, int n_t, int y_dim, gn_real *y);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_SNN_H */
