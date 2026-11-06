/*
 * gn_birnn.h — layer RICORRENTE BIDIREZIONALE (clean-room, concetto Graves 2005).
 * Wrappa gn_rnn/gn_lstm/gn_gru: una direzione sul tempo diretto, una sul tempo inverso;
 * gli output (T x hidden) sono concatenati (T x 2*hidden) e proiettati a `out`.
 *   y_t = Wy [h_fwd_t ; h_bwd_t] + by
 * Backward: il gradiente di uscita e' diviso tra le due direzioni (la bwd riceve il tempo
 * invertito). x: T x in -> y: T x out.
 *
 * Riferimento (solo concetto, nessuna licenza): kartikmadan11/MetaTraderForecast (Bi-LSTM/Bi-GRU).
 */
#ifndef GMBNEURAL_GN_BIRNN_H
#define GMBNEURAL_GN_BIRNN_H

#include "gn_core.h"
#include "gn_rnn.h"
#include "gn_lstm.h"
#include "gn_gru.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GN_BIRNN_RNN  0
#define GN_BIRNN_LSTM 1
#define GN_BIRNN_GRU  2

typedef struct gn_birnn {
    int kind, in, hidden, out, T;
    gn_rnn  rf, rb;
    gn_lstm lf, lb;
    gn_gru  gf, gb;
    gn_mat Wy, by, gWy, gby;      /* Wy: out x (2h) */
    gn_mat X, Xrev;
    gn_mat Yf, Yb, Ybrev, Ycat;   /* T x h / T x h / T x h / T x 2h */
    gn_mat tmpW, grad_cat, grad_f, grad_b_al, grad_b_rev;
} gn_birnn;

gn_status gn_birnn_init(gn_birnn *b, int kind, int in, int hidden, int out, unsigned long long seed);
void gn_birnn_free(gn_birnn *b);
gn_status gn_birnn_forward(gn_birnn *b, const gn_mat *x, gn_mat *y);
gn_status gn_birnn_backward(gn_birnn *b, const gn_mat *grad_y);
gn_status gn_birnn_zero_grad(gn_birnn *b);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_BIRNN_H */
