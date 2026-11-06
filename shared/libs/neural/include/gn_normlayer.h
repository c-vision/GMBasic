/*
 * gn_normlayer.h — normalizzazione di strato: BatchNorm e LayerNorm.
 *
 * Da attractivechaos/kann (MIT): normalizzazione con parametri appresi gamma/beta.
 * BatchNorm normalizza per-feature lungo il batch (con statistiche correnti);
 * LayerNorm normalizza per-campione lungo le feature.
 */
#ifndef GMBNEURAL_GN_NORMLAYER_H
#define GMBNEURAL_GN_NORMLAYER_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_batchnorm {
    int features;
    gn_real eps, momentum;
    int train;
    gn_mat gamma, beta, ggamma, gbeta;
    gn_mat rmean, rvar;     /* statistiche correnti (inferenza) */
    int B;
    gn_mat xhat;            /* cache B×features */
    gn_mat mean, istd;      /* cache 1×features */
} gn_batchnorm;

gn_status gn_batchnorm_init(gn_batchnorm *bn, int features, gn_real eps, gn_real momentum);
void gn_batchnorm_free(gn_batchnorm *bn);
/* y = gamma*xhat + beta. x: B×features. train: usa statistiche del batch. */
gn_status gn_batchnorm_forward(gn_batchnorm *bn, const gn_mat *x, gn_mat *y, int train);
gn_status gn_batchnorm_backward(gn_batchnorm *bn, const gn_mat *grad_y, gn_mat *grad_x);
gn_status gn_batchnorm_zero_grad(gn_batchnorm *bn);

typedef struct gn_layernorm {
    int features;
    gn_real eps;
    gn_mat gamma, beta, ggamma, gbeta;
    int B;
    gn_mat xhat, mean, istd;   /* xhat B×features; mean/istd B×1 */
} gn_layernorm;

gn_status gn_layernorm_init(gn_layernorm *ln, int features, gn_real eps);
void gn_layernorm_free(gn_layernorm *ln);
gn_status gn_layernorm_forward(gn_layernorm *ln, const gn_mat *x, gn_mat *y);
gn_status gn_layernorm_backward(gn_layernorm *ln, const gn_mat *grad_y, gn_mat *grad_x);
gn_status gn_layernorm_zero_grad(gn_layernorm *ln);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_NORMLAYER_H */
