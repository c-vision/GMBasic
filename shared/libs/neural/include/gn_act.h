/*
 * gn_act.h — funzioni di attivazione (elemento per elemento) e derivate.
 */
#ifndef GMBNEURAL_GN_ACT_H
#define GMBNEURAL_GN_ACT_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

gn_real gn_sigmoid(gn_real x);
gn_real gn_sigmoid_grad(gn_real y);   /* y = sigmoid(x) */
gn_real gn_tanh(gn_real x);
gn_real gn_tanh_grad(gn_real y);      /* y = tanh(x) */
gn_real gn_relu(gn_real x);
gn_real gn_relu_grad(gn_real y);
gn_real gn_leaky_relu(gn_real x);
gn_real gn_leaky_relu_grad(gn_real y);
gn_real gn_elu(gn_real x);
gn_real gn_elu_grad(gn_real y);
gn_real gn_identity(gn_real x);
gn_real gn_identity_grad(gn_real y);

/* Soglia (step): 1 se x > 0, altrimenti 0 (grad 0). */
gn_real gn_threshold(gn_real x);
gn_real gn_threshold_grad(gn_real x);

/* Attivazioni avanzate (da google/XNNPACK). I _grad prendono x (pre-attivazione),
 * tranne dove y basta. */
gn_real gn_hardsigmoid(gn_real x);      /* clamp((x+3)/6, 0, 1) */
gn_real gn_hardsigmoid_grad(gn_real x);
gn_real gn_hardswish(gn_real x);        /* x * hardsigmoid(x) */
gn_real gn_hardswish_grad(gn_real x);
gn_real gn_silu(gn_real x);             /* x * sigmoid(x) */
gn_real gn_silu_grad(gn_real x);
gn_real gn_gelu(gn_real x);             /* 0.5 x (1 + erf(x/sqrt(2))) */
gn_real gn_gelu_grad(gn_real x);
gn_real gn_mish(gn_real x);             /* x * tanh(softplus(x)) */
gn_real gn_mish_grad(gn_real x);

/* Attivazioni da nnabla (sony/nnabla-c-runtime).
 * CELU: max(0,x) + min(0, a*(exp(x/a)-1)). CRELU: raddoppia i canali
 * [relu(x), relu(-x)]. PReLU: max(0,x) + a*min(0,x). */
gn_real gn_celu(gn_real x, gn_real alpha);
gn_real gn_celu_grad(gn_real x, gn_real alpha);
gn_real gn_prelu(gn_real x, gn_real alpha);
gn_real gn_prelu_grad(gn_real x, gn_real alpha);
/* CRELU: out[2n] da in[n] (prima meta' relu(x), seconda relu(-x)). */
void gn_crelu(gn_real *out, const gn_real *in, int n);
void gn_crelu_grad(gn_real *grad_in, const gn_real *grad_out, const gn_real *in, int n);

/* Attivazioni da darknet (sowson/darknet). I _grad prendono x (pre-attivazione). */
gn_real gn_selu(gn_real x);          /* 1.0507*(x se x>=0, 1.6732*(e^x-1) se x<0) */
gn_real gn_selu_grad(gn_real x);
gn_real gn_plse(gn_real x);          /* piecewise linear squashing */
gn_real gn_plse_grad(gn_real x);
gn_real gn_loggy(gn_real x);         /* 2*sigmoid(x) - 1 */
gn_real gn_loggy_grad(gn_real x);
gn_real gn_hardtan(gn_real x);       /* clamp(x, -1, 1) */
gn_real gn_hardtan_grad(gn_real x);
gn_real gn_lhtan(gn_real x);         /* lineare, satura a 0/1 */
gn_real gn_lhtan_grad(gn_real x);
gn_real gn_relie(gn_real x);         /* x>0 ? x : 0.01x */
gn_real gn_relie_grad(gn_real x);
gn_real gn_ramp(gn_real x);          /* x*(x>0) + 0.1x */
gn_real gn_ramp_grad(gn_real x);
gn_real gn_stair(gn_real x);         /* funzione a gradini */
gn_real gn_stair_grad(gn_real x);

/* softmax con temperatura (da recurrent-neural-net): out = softmax(z/T). */
gn_status gn_softmax_temp(gn_mat *out, const gn_mat *z, gn_real temperature);

/* softmax riga per riga (stabile): out (m×n) da z (m×n). */
gn_status gn_softmax(gn_mat *out, const gn_mat *z);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_ACT_H */
