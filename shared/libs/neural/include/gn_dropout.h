/*
 * gn_dropout.h — dropout con inverted scaling (da Deyht/CIANNA).
 *
 * In training azzera casualmente una frazione `rate` delle attivazioni e scala
 * le rimanenti per 1/(1-rate) (inverted dropout), così l'inferenza è identità.
 * La maschera è memorizzata per il backward.
 */
#ifndef GMBNEURAL_GN_DROPOUT_H
#define GMBNEURAL_GN_DROPOUT_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_dropout {
    gn_real rate;
    int n;
    unsigned char *mask;   /* 1 = tenuto */
} gn_dropout;

gn_status gn_dropout_init(gn_dropout *d, int n, gn_real rate);
void gn_dropout_free(gn_dropout *d);
/* train!=0: applica dropout (maschera + scala 1/(1-rate)); train==0: identità. */
gn_status gn_dropout_forward(gn_dropout *d, const gn_real *x, gn_real *out,
                             int train, unsigned long long seed);
/* Backward: propaga grad_out attraverso la maschera/scala dell'ultimo forward. */
gn_status gn_dropout_backward(const gn_dropout *d, const gn_real *grad_out, gn_real *grad_x);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_DROPOUT_H */
