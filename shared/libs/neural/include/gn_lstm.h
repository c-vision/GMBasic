/*
 * gn_lstm.h — layer LSTM (Long Short-Term Memory) con BPTT.
 *
 * Equazioni standard (Hochreiter-Schmidhuber 1997; Graves 2012), reimplementate
 * indipendentemente (rnnlib è GPL-3.0):
 *   i_t = sigmoid(Wi x_t + Ui h_{t-1} + bi)     (input gate)
 *   f_t = sigmoid(Wf x_t + Uf h_{t-1} + bf)     (forget gate)
 *   o_t = sigmoid(Wo x_t + Uo h_{t-1} + bo)     (output gate)
 *   g_t = tanh(Wg x_t + Ug h_{t-1} + bg)        (cell input)
 *   c_t = f_t . c_{t-1} + i_t . g_t
 *   h_t = o_t . tanh(c_t)
 *   y_t = Wy h_t + by
 * x: T×in -> y: T×out.
 */
#ifndef GMBNEURAL_GN_LSTM_H
#define GMBNEURAL_GN_LSTM_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_lstm {
    int in, hidden, out;
    gn_mat Wi, Ui, bi;
    gn_mat Wf, Uf, bf;
    gn_mat Wo, Uo, bo;
    gn_mat Wg, Ug, bg;
    gn_mat Wy, by;
    gn_mat gWi, gUi, gbi, gWf, gUf, gbf, gWo, gUo, gbo, gWg, gUg, gbg, gWy, gby;
    int T;
    gn_mat X;       /* T×in */
    gn_mat H;       /* (T+1)×hidden */
    gn_mat C;       /* (T+1)×hidden */
    gn_mat I, F, O, G;  /* T×hidden (gate attivazioni) */
} gn_lstm;

gn_status gn_lstm_init(gn_lstm *l, int in, int hidden, int out, unsigned long long seed);
void gn_lstm_free(gn_lstm *l);
gn_status gn_lstm_forward(gn_lstm *l, const gn_mat *x, gn_mat *y);
gn_status gn_lstm_backward(gn_lstm *l, const gn_mat *grad_y);
gn_status gn_lstm_zero_grad(gn_lstm *l);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_LSTM_H */
