/*
 * gn_gru.h — layer GRU (Gated Recurrent Unit) con BPTT.
 *
 * Da xiph/rnnoise (BSD-3, Valin 2018): convenzione standard
 *   z_t = sigmoid(Wz x_t + Uz h_{t-1} + bz)          (update gate)
 *   r_t = sigmoid(Wr x_t + Ur h_{t-1} + br)          (reset gate)
 *   n_t = tanh(Wn x_t + Un (r_t . h_{t-1}) + bn)     (candidate)
 *   h_t = (1 - z_t) . n_t + z_t . h_{t-1}
 *   y_t = Wo h_t + bo
 * x: T×in -> y: T×out. Usa le attivazioni di gn_act.
 */
#ifndef GMBNEURAL_GN_GRU_H
#define GMBNEURAL_GN_GRU_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_gru {
    int in, hidden, out;
    gn_mat Wz, Uz, bz;
    gn_mat Wr, Ur, br;
    gn_mat Wn, Un, bn;
    gn_mat Wo, bo;
    gn_mat gWz, gUz, gbz, gWr, gUr, gbr, gWn, gUn, gbn, gWo, gbo;
    /* cache */
    int T;
    gn_mat X;   /* T×in */
    gn_mat Z, R, N;  /* T×hidden */
    gn_mat H;   /* (T+1)×hidden, H[0]=0 */
} gn_gru;

gn_status gn_gru_init(gn_gru *g, int in, int hidden, int out, unsigned long long seed);
void gn_gru_free(gn_gru *g);
gn_status gn_gru_forward(gn_gru *g, const gn_mat *x, gn_mat *y);
gn_status gn_gru_backward(gn_gru *g, const gn_mat *grad_y);
gn_status gn_gru_zero_grad(gn_gru *g);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_GRU_H */
