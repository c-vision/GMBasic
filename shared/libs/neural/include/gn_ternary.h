/*
 * gn_ternary.h — inferenza ternaria (1.58-bit) con bit-slicing (da aethelarch).
 *
 * I pesi W ∈ {-1, 0, +1} sono memorizzati su due bitplane disgiunti (w_pos,
 * w_neg); l'attivazione è binarizzata (1 se > 0). Il prodotto scalare è
 *   popcount(a & w_pos) - popcount(a & w_neg)
 * senza unpacking in registro. out = scale * (W ⊙ a_bin).
 */
#ifndef GMBNEURAL_GN_TERNARY_H
#define GMBNEURAL_GN_TERNARY_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_ternary {
    int rows, cols;
    size_t stride;              /* byte per riga (multiplo di 8) */
    unsigned char *w_pos, *w_neg;
    gn_real scale;
} gn_ternary;

/* Quantizza W (rows×cols) a ternario: scale = media(|W|); w = clamp(round(W/scale), -1, 1). */
gn_status gn_ternary_from_mat(gn_ternary *t, const gn_mat *W);
void gn_ternary_free(gn_ternary *t);

/* Numero di byte per il vettore di attivazione binaria di `dim` bit. */
size_t gn_ternary_act_bytes(int dim);

/* Prodotto scalare bit-sliced: popcount(a & w_pos) - popcount(a & w_neg). */
int gn_ternary_dot(const unsigned char *w_pos, const unsigned char *w_neg,
                   const unsigned char *a_bits, int nbits);

/* forward: x (batch × cols) -> out (batch × rows) = scale * (W ⊙ sign(x)). */
gn_status gn_ternary_forward(const gn_ternary *t, const gn_mat *x, gn_mat *out);

/* Dequantizza in una matrice float (rows×cols). */
gn_status gn_ternary_to_mat(const gn_ternary *t, gn_mat *W);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_TERNARY_H */
