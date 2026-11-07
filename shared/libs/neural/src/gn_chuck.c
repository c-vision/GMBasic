/*
 * gn_chuck.c — ottimizzatore Chuck (da notorch).
 */
#include "gn_chuck.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

void gn_chuck_init(gn_chuck *c)
{
    int i;
    if (!c) return;
    for (i = 0; i < GN_CHUCK_WINDOW; i++) c->loss_hist[i] = 0.0;
    c->dampen = 1.0;
    c->noise = GN_CHUCK_NOISE_MAG;
    c->loss_ema = 0.0;
    c->lr_scale = 1.0;
    c->pos = c->full = c->stag = c->global_step = c->initialized = 0;
}

void gn_chuck_pstate_init(gn_chuck_pstate *p)
{
    int i;
    if (!p) return;
    for (i = 0; i < GN_CHUCK_WINDOW; i++) p->grad_hist[i] = 0.0;
    p->dampen = 1.0;
    p->pos = p->full = p->stag = p->frozen = 0;
}

void gn_chuck_observe_loss(gn_chuck *c, gn_real loss)
{
    gn_real prev;
    if (!c) return;
    if (!c->initialized) { c->loss_ema = loss; c->initialized = 1; }
    prev = c->loss_ema;
    c->loss_ema = 0.9 * c->loss_ema + 0.1 * loss;
    c->loss_hist[c->pos] = loss;
    c->pos = (c->pos + 1) % GN_CHUCK_WINDOW;
    if (c->pos == 0) c->full = 1;
    if (c->full) {
        gn_real trend = (prev > 1e-12) ? (c->loss_ema - prev) / prev : 0.0;
        if (trend > GN_CHUCK_TREND_BRAKE) c->lr_scale *= 0.99;         /* loss sale: rallenta */
        else if (trend < GN_CHUCK_TREND_PUSH) c->lr_scale *= 1.005;    /* loss scende: accelera */
        if (c->lr_scale < 0.1) c->lr_scale = 0.1;
        if (c->lr_scale > 4.0) c->lr_scale = 4.0;
        /* stallo: inietta rumore decrescente */
        if (fabs(trend) < GN_CHUCK_STAG_THRESH) {
            c->stag++;
            if (c->stag >= GN_CHUCK_STAG_STEPS) { c->noise = GN_CHUCK_NOISE_MAG; c->stag = 0; }
        } else c->stag = 0;
        c->noise *= GN_CHUCK_NOISE_DECAY;
    }
    c->global_step++;
}

gn_status gn_chuck_step(gn_chuck *c, gn_chuck_pstate *p, gn_mat *param,
                        const gn_mat *grad, gn_mat *m, gn_mat *v,
                        gn_real lr, gn_real beta1, gn_real beta2, gn_real eps)
{
    int i, n;
    gn_real b1t, b2t, sc;
    unsigned long long s;
    if (!c || !p || !param || !grad || !m || !v) return GN_ERR_ARG;
    if (param->rows != grad->rows || param->cols != grad->cols) return GN_ERR_ARG;
    n = param->rows * param->cols;
    c->global_step++;
    b1t = 1.0 - pow(beta1, (gn_real)c->global_step);
    b2t = 1.0 - pow(beta2, (gn_real)c->global_step);
    sc = lr * c->lr_scale * p->dampen;
    s = 88172645463325252ULL + (unsigned long long)c->global_step * 2654435761ULL;
    for (i = 0; i < n; i++) {
        gn_real g = grad->data[i];
        gn_real noise = 0.0;
        /* aggiorna la storia del gradiente per il dampening */
        p->grad_hist[p->pos] = fabs(g);
        /* trend del gradiente: confronta la media recente con la precedente */
        {
            int j; gn_real rec = 0.0, old = 0.0;
            for (j = 0; j < GN_CHUCK_WINDOW; j++) {
                if (j < GN_CHUCK_WINDOW / 2) rec += p->grad_hist[j];
                else old += p->grad_hist[j];
            }
            if (p->full) {
                if (rec > old * 1.1) p->dampen *= GN_CHUCK_DAMP_DOWN;
                else if (rec < old * 0.9) p->dampen *= GN_CHUCK_DAMP_UP;
                if (p->dampen < GN_CHUCK_DAMP_LO) p->dampen = GN_CHUCK_DAMP_LO;
                if (p->dampen > GN_CHUCK_DAMP_HI) p->dampen = GN_CHUCK_DAMP_HI;
            }
        }
        if (fabs(g) < GN_CHUCK_FREEZE_THRESH * 1e-3) p->frozen = 1; else p->frozen = 0;
        /* rumore su stallo (iniezione) */
        if (c->noise > 0.0) {
            s = s * 6364136223846793005ULL + 1442695040888963407ULL;
            noise = c->noise * (((gn_real)(s >> 11) / 9007199254740992.0) - 0.5);
        }
        if (!p->frozen) {
            m->data[i] = beta1 * m->data[i] + (1.0 - beta1) * g;
            v->data[i] = beta2 * v->data[i] + (1.0 - beta2) * g * g;
            { gn_real mh = m->data[i] / b1t, vh = v->data[i] / b2t;
              param->data[i] -= sc * mh / (sqrt(vh) + eps); }
        }
        param->data[i] += noise;
        p->pos = (p->pos + 1) % GN_CHUCK_WINDOW;
        if (p->pos == 0) p->full = 1;
    }
    return GN_OK;
}
