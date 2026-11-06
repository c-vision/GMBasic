/*
 * gn_chuck.h — ottimizzatore Chuck (da ariannamethod/notorch).
 *
 * Variante "self-aware" di Adam: aggiornamento base Adam con dampening
 * per-parametro adattivo (in [DAMP_LO, DAMP_HI] in base al trend del gradiente),
 * freeze dei parametri con gradiente trascurabile, e adattamento macro del
 * learning rate in base al trend della loss (con iniezione di rumore su stallo).
 */
#ifndef GMBNEURAL_GN_CHUCK_H
#define GMBNEURAL_GN_CHUCK_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GN_CHUCK_WINDOW 16
#define GN_CHUCK_DAMP_LO 0.3
#define GN_CHUCK_DAMP_HI 2.0
#define GN_CHUCK_DAMP_DOWN 0.97
#define GN_CHUCK_DAMP_UP 1.03
#define GN_CHUCK_TREND_BRAKE 0.02
#define GN_CHUCK_TREND_PUSH (-0.02)
#define GN_CHUCK_STAG_THRESH 0.001
#define GN_CHUCK_STAG_STEPS 8
#define GN_CHUCK_NOISE_MAG 0.001
#define GN_CHUCK_NOISE_DECAY 0.9
#define GN_CHUCK_FREEZE_THRESH 0.01

typedef struct {
    gn_real grad_hist[GN_CHUCK_WINDOW];
    gn_real dampen;
    int pos, full, stag, frozen;
} gn_chuck_pstate;

typedef struct {
    gn_real loss_hist[GN_CHUCK_WINDOW];
    gn_real dampen, noise, loss_ema, lr_scale;
    int pos, full, stag, global_step, initialized;
} gn_chuck;

void gn_chuck_init(gn_chuck *c);
void gn_chuck_pstate_init(gn_chuck_pstate *p);
/* Aggiorna lo stato macro con la loss corrente (chiamare una volta per step). */
void gn_chuck_observe_loss(gn_chuck *c, gn_real loss);
/* Passo su un parametro (m/v = stato Adam). lr = learning rate base. */
gn_status gn_chuck_step(gn_chuck *c, gn_chuck_pstate *p, gn_mat *param,
                        const gn_mat *grad, gn_mat *m, gn_mat *v,
                        gn_real lr, gn_real beta1, gn_real beta2, gn_real eps);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_CHUCK_H */
