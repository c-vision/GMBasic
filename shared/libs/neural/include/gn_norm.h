/*
 * gn_norm.h — normalizzazione online delle feature (da antirez/neural-redis).
 *
 * Osserva i dati in streaming e mantiene statistiche correnti: massimo assoluto
 * per dimensione (scaling max-abs, con margine) e media/varianza (Welford) per
 * la standardizzazione z-score. Utile come pre/post-processing di una rete.
 */
#ifndef GMBNEURAL_GN_NORM_H
#define GMBNEURAL_GN_NORM_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_norm {
    int dim;
    int count;
    gn_real *scale;   /* fattore max-abs per dimensione (>= 1) */
    gn_real *mean;    /* media corrente (Welford) */
    gn_real *m2;      /* somma degli scarti quadratici (Welford) */
} gn_norm;

gn_status gn_norm_init(gn_norm *n, int dim);
void gn_norm_free(gn_norm *n);
/* Aggiorna le statistiche con un campione x[dim] (streaming). */
gn_status gn_norm_observe(gn_norm *n, const gn_real *x);
/* Applica il margine ai fattori di scala (es. 1.2 come in neural-redis). */
void gn_norm_finalize(gn_norm *n, gn_real margin);

/* x/scale -> out (normalizzazione max-abs). */
gn_status gn_norm_apply(const gn_norm *n, const gn_real *x, gn_real *out);
/* out*scale (denormalizzazione). */
gn_status gn_norm_denormalize(const gn_norm *n, const gn_real *x, gn_real *out);
/* (x - mean)/std -> out (standardizzazione; std = sqrt(var)). */
gn_status gn_norm_zscore(const gn_norm *n, const gn_real *x, gn_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_NORM_H */
