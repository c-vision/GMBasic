/*
 * gn_opt.h — ottimizzatori: SGD, SGD con momentum, Adam.
 *
 * L'ottimizzatore è senza stato interno per-parametro: il chiamante fornisce
 * i buffer di stato m/v (stessa forma del parametro). gn_opt mantiene il
 * contatore dei passi t (per la correzione di bias di Adam).
 */
#ifndef GMBNEURAL_GN_OPT_H
#define GMBNEURAL_GN_OPT_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GN_OPT_SGD 0
#define GN_OPT_MOMENTUM 1
#define GN_OPT_ADAM 2

typedef struct gn_opt {
    int kind;
    gn_real lr;
    gn_real momentum;          /* momentum (GN_OPT_MOMENTUM) */
    gn_real beta1, beta2, eps; /* Adam */
    int t;                     /* passo corrente */
    gn_real lr0;               /* lr iniziale (per annealing) */
    gn_real search_time;       /* costante di annealing (0 = nessuno) */
    gn_real l2;                /* coefficiente L2 (0 = nessuno) */
    int epoch;                 /* epoca corrente (per annealing) */
} gn_opt;

gn_opt gn_opt_make(int kind, gn_real lr);
/* param -= aggiornamento; m/v sono buffer di stato (possono essere NULL per SGD). */
gn_status gn_opt_apply(gn_opt *opt, gn_mat *param, const gn_mat *grad, gn_mat *m, gn_mat *v);
/* Annealing del learning rate (da 100/Cranium): lr = lr0 / (1 + epoch/search_time)
 * se search_time > 0. Aggiorna opt->epoch e opt->lr. */
void gn_opt_set_epoch(gn_opt *opt, int epoch);
/* Gradient clipping (da Ricardicus/recurrent-neural-net).
 * Value: clamp di ogni elemento a [-limit, limit]; ritorna 1 se ha tagliato.
 * Norm: se ||g||_2 > max_norm riscala g *= max_norm/||g||; ritorna 1 se riscalato. */
int gn_opt_clip_value(gn_mat *grad, gn_real limit);
int gn_opt_clip_norm(gn_mat *grad, gn_real max_norm);
/* Global norm su piu' matrici: riscala tutte se la norma globale supera max_norm. */
int gn_opt_clip_global_norm(gn_mat *const *grads, int n, gn_real max_norm);

/* Imposta L2 (weight decay): il gradiente effettivo diventa grad + l2*param. */
void gn_opt_set_l2(gn_opt *opt, gn_real l2);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_OPT_H */
