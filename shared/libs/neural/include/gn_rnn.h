/*
 * gn_rnn.h — cella RNN vanilla e backpropagation through time (BPTT).
 *
 *   z_t = Wxh x_t + Whh h_{t-1} + bh
 *   h_t = tanh(z_t)
 *   y_t = Why h_t + by
 *
 * x: T×in, y: T×out. Wxh: hidden×in, Whh: hidden×hidden, Why: out×hidden.
 */
#ifndef GMBNEURAL_GN_RNN_H
#define GMBNEURAL_GN_RNN_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_rnn {
    int in, hidden, out;
    gn_mat Wxh, Whh, Why, bh, by;
    gn_mat gWxh, gWhh, gWhy, gbh, gby;
    /* cache */
    int T;
    gn_mat H;   /* (T+1)×hidden : H[0]=h_0=0, H[t+1]=h_t */
    gn_mat X;   /* T×in */
} gn_rnn;

gn_status gn_rnn_init(gn_rnn *r, int in, int hidden, int out, unsigned long long seed);
void gn_rnn_free(gn_rnn *r);
/* x: T×in -> y: T×out (predizioni). */
gn_status gn_rnn_forward(gn_rnn *r, const gn_mat *x, gn_mat *y);
/* grad_y: T×out; accumula i gradienti (azzera prima con gn_rnn_zero_grad). */
gn_status gn_rnn_backward(gn_rnn *r, const gn_mat *grad_y);
gn_status gn_rnn_zero_grad(gn_rnn *r);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_RNN_H */
