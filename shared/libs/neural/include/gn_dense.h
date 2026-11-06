/*
 * gn_dense.h — strato denso (fully connected): y = x W^T + b.
 *
 * x: batch × in ; W: out × in ; b: 1 × out ; y: batch × out.
 * backward accumula gradW/gradb e propaga grad_x = grad_y W.
 */
#ifndef GMBNEURAL_GN_DENSE_H
#define GMBNEURAL_GN_DENSE_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_dense {
    int in, out;
    gn_mat W, b;          /* parametri */
    gn_mat gradW, gradb;  /* gradienti accumulati */
    gn_mat x;             /* cache dell'ultimo input */
} gn_dense;

gn_status gn_dense_init(gn_dense *l, int in, int out, unsigned long long seed);
void gn_dense_free(gn_dense *l);
/* y = x W^T + b. */
gn_status gn_dense_forward(gn_dense *l, const gn_mat *x, gn_mat *y);
/* dati grad_y, accumula gradW/gradb e scrive grad_x (può essere NULL). */
gn_status gn_dense_backward(gn_dense *l, const gn_mat *grad_y, gn_mat *grad_x);
/* azzera i gradienti accumulati. */
gn_status gn_dense_zero_grad(gn_dense *l);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_DENSE_H */
