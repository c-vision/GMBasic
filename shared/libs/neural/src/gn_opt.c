/* gn_opt.c — ottimizzatori. */
#include "gn_opt.h"

#include <math.h>

gn_opt gn_opt_make(int kind, gn_real lr)
{
    gn_opt o;
    o.kind = kind;
    o.lr = lr;
    o.momentum = 0.9;
    o.beta1 = 0.9;
    o.beta2 = 0.999;
    o.eps = 1e-8;
    o.t = 0;
    o.lr0 = lr;
    o.search_time = 0.0;
    o.l2 = 0.0;
    o.epoch = 0;
    return o;
}

gn_status gn_opt_apply(gn_opt *opt, gn_mat *param, const gn_mat *grad, gn_mat *m, gn_mat *v)
{
    int i, n;
    if (!opt || !param || !grad || param->rows != grad->rows || param->cols != grad->cols)
        return GN_ERR_ARG;
    n = param->rows * param->cols;
    if (opt->kind == GN_OPT_SGD) {
        for (i = 0; i < n; i++) {
            gn_real g = grad->data[i] + (opt->l2 > 0.0 ? opt->l2 * param->data[i] : 0.0);
            param->data[i] -= opt->lr * g;
        }
    } else if (opt->kind == GN_OPT_MOMENTUM) {
        if (!m) return GN_ERR_ARG;
        for (i = 0; i < n; i++) {
            gn_real g = grad->data[i] + (opt->l2 > 0.0 ? opt->l2 * param->data[i] : 0.0);
            m->data[i] = opt->momentum * m->data[i] + g;
            param->data[i] -= opt->lr * m->data[i];
        }
    } else if (opt->kind == GN_OPT_ADAM) {
        gn_real b1t, b2t;
        if (!m || !v) return GN_ERR_ARG;
        opt->t++;
        b1t = 1.0 - pow(opt->beta1, (gn_real)opt->t);
        b2t = 1.0 - pow(opt->beta2, (gn_real)opt->t);
        for (i = 0; i < n; i++) {
            gn_real g = grad->data[i] + (opt->l2 > 0.0 ? opt->l2 * param->data[i] : 0.0);
            m->data[i] = opt->beta1 * m->data[i] + (1.0 - opt->beta1) * g;
            v->data[i] = opt->beta2 * v->data[i] + (1.0 - opt->beta2) * g * g;
            {
                gn_real mh = m->data[i] / b1t, vh = v->data[i] / b2t;
                param->data[i] -= opt->lr * mh / (sqrt(vh) + opt->eps);
            }
        }
    } else return GN_ERR_ARG;
    return GN_OK;
}

void gn_opt_set_epoch(gn_opt *opt, int epoch)
{
    if (!opt) return;
    opt->epoch = epoch;
    if (opt->search_time > 0.0)
        opt->lr = opt->lr0 / (1.0 + (gn_real)epoch / opt->search_time);
    else
        opt->lr = opt->lr0;
}

void gn_opt_set_l2(gn_opt *opt, gn_real l2)
{
    if (opt) opt->l2 = l2 > 0.0 ? l2 : 0.0;
}

/* ===================== Gradient clipping (da recurrent-neural-net) ===================== */

int gn_opt_clip_value(gn_mat *grad, gn_real limit)
{
    int i, n, clipped = 0;
    if (!grad || limit <= 0.0) return 0;
    n = grad->rows * grad->cols;
    for (i = 0; i < n; i++) {
        if (grad->data[i] > limit) { grad->data[i] = limit; clipped = 1; }
        else if (grad->data[i] < -limit) { grad->data[i] = -limit; clipped = 1; }
    }
    return clipped;
}

int gn_opt_clip_norm(gn_mat *grad, gn_real max_norm)
{
    int i, n;
    gn_real s = 0.0, norm;
    if (!grad || max_norm <= 0.0) return 0;
    n = grad->rows * grad->cols;
    for (i = 0; i < n; i++) s += grad->data[i] * grad->data[i];
    norm = sqrt(s);
    if (norm <= max_norm || norm < 1e-12) return 0;
    { gn_real sc = max_norm / norm; for (i = 0; i < n; i++) grad->data[i] *= sc; }
    return 1;
}

int gn_opt_clip_global_norm(gn_mat *const *grads, int n, gn_real max_norm)
{
    int i, j, k;
    gn_real s = 0.0, norm;
    if (!grads || n < 1 || max_norm <= 0.0) return 0;
    for (i = 0; i < n; i++) {
        if (!grads[i]) continue;
        for (j = 0; j < grads[i]->rows * grads[i]->cols; j++)
            s += grads[i]->data[j] * grads[i]->data[j];
    }
    norm = sqrt(s);
    if (norm <= max_norm || norm < 1e-12) return 0;
    { gn_real sc = max_norm / norm;
      for (i = 0; i < n; i++) {
          if (!grads[i]) continue;
          for (k = 0; k < grads[i]->rows * grads[i]->cols; k++) grads[i]->data[k] *= sc;
      } }
    return 1;
}
