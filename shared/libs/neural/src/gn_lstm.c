/*
 * gn_lstm.c — LSTM forward/BPTT (equazioni standard, implementazione originale).
 */
#include "gn_lstm.h"
#include "gn_act.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static gn_status lp(gn_mat *w, gn_mat *g, int r, int c)
{
    if (gn_mat_alloc(w, r, c) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(g, r, c) != GN_OK) { gn_mat_free(w); return GN_ERR_NO_MEM; }
    return GN_OK;
}
static void fp(gn_mat *w, gn_mat *g) { gn_mat_free(w); gn_mat_free(g); }
static void rfill(gn_mat *m, gn_real sc, unsigned long long *s)
{
    int i, n = m->rows * m->cols;
    for (i = 0; i < n; i++) {
        *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
        m->data[i] = sc * (((gn_real)((*s >> 11) & 0x1FFFFF)) / 1048576.0 - 1.0);
    }
}

gn_status gn_lstm_init(gn_lstm *l, int in, int hidden, int out, unsigned long long seed)
{
    unsigned long long s = seed ? seed : 1;
    gn_real sc = 0.5;
    if (!l || in < 1 || hidden < 1 || out < 1) return GN_ERR_ARG;
    memset(l, 0, sizeof(*l));
    l->in = in; l->hidden = hidden; l->out = out;
    if (lp(&l->Wi, &l->gWi, hidden, in) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->Ui, &l->gUi, hidden, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->bi, &l->gbi, 1, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->Wf, &l->gWf, hidden, in) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->Uf, &l->gUf, hidden, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->bf, &l->gbf, 1, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->Wo, &l->gWo, hidden, in) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->Uo, &l->gUo, hidden, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->bo, &l->gbo, 1, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->Wg, &l->gWg, hidden, in) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->Ug, &l->gUg, hidden, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->bg, &l->gbg, 1, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->Wy, &l->gWy, out, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (lp(&l->by, &l->gby, 1, out) != GN_OK) return GN_ERR_NO_MEM;
    rfill(&l->Wi, sc, &s); rfill(&l->Ui, sc, &s); gn_mat_zero(&l->bi);
    rfill(&l->Wf, sc, &s); rfill(&l->Uf, sc, &s); gn_mat_zero(&l->bf);
    rfill(&l->Wo, sc, &s); rfill(&l->Uo, sc, &s); gn_mat_zero(&l->bo);
    rfill(&l->Wg, sc, &s); rfill(&l->Ug, sc, &s); gn_mat_zero(&l->bg);
    rfill(&l->Wy, sc, &s); gn_mat_zero(&l->by);
    /* forget gate bias a 1 per default (aiuta il training) */
    { int i; for (i = 0; i < hidden; i++) l->bf.data[i] = 1.0; }
    gn_lstm_zero_grad(l);
    return GN_OK;
}

void gn_lstm_free(gn_lstm *l)
{
    if (!l) return;
    fp(&l->Wi,&l->gWi); fp(&l->Ui,&l->gUi); fp(&l->bi,&l->gbi);
    fp(&l->Wf,&l->gWf); fp(&l->Uf,&l->gUf); fp(&l->bf,&l->gbf);
    fp(&l->Wo,&l->gWo); fp(&l->Uo,&l->gUo); fp(&l->bo,&l->gbo);
    fp(&l->Wg,&l->gWg); fp(&l->Ug,&l->gUg); fp(&l->bg,&l->gbg);
    fp(&l->Wy,&l->gWy); fp(&l->by,&l->gby);
    gn_mat_free(&l->X); gn_mat_free(&l->H); gn_mat_free(&l->C);
    gn_mat_free(&l->I); gn_mat_free(&l->F); gn_mat_free(&l->O); gn_mat_free(&l->G);
    memset(l, 0, sizeof(*l));
}

gn_status gn_lstm_zero_grad(gn_lstm *l)
{
    gn_mat_zero(&l->gWi); gn_mat_zero(&l->gUi); gn_mat_zero(&l->gbi);
    gn_mat_zero(&l->gWf); gn_mat_zero(&l->gUf); gn_mat_zero(&l->gbf);
    gn_mat_zero(&l->gWo); gn_mat_zero(&l->gUo); gn_mat_zero(&l->gbo);
    gn_mat_zero(&l->gWg); gn_mat_zero(&l->gUg); gn_mat_zero(&l->gbg);
    gn_mat_zero(&l->gWy); gn_mat_zero(&l->gby);
    return GN_OK;
}

gn_status gn_lstm_forward(gn_lstm *l, const gn_mat *x, gn_mat *y)
{
    int t, i, j, T;
    if (!l || !x || !y) return GN_ERR_ARG;
    T = x->rows;
    if (x->cols != l->in || y->rows != T || y->cols != l->out) return GN_ERR_ARG;
    gn_mat_free(&l->X); gn_mat_free(&l->H); gn_mat_free(&l->C);
    gn_mat_free(&l->I); gn_mat_free(&l->F); gn_mat_free(&l->O); gn_mat_free(&l->G);
    if (gn_mat_alloc(&l->X, T, l->in) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&l->H, T + 1, l->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&l->C, T + 1, l->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&l->I, T, l->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&l->F, T, l->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&l->O, T, l->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&l->G, T, l->hidden) != GN_OK) return GN_ERR_NO_MEM;
    gn_mat_copy(&l->X, x);
    gn_mat_zero(&l->H); gn_mat_zero(&l->C);
    l->T = T;
    for (t = 0; t < T; t++) {
        const gn_real *xt = x->data + (size_t)t * l->in;
        const gn_real *hp = l->H.data + (size_t)t * l->hidden;
        const gn_real *cp = l->C.data + (size_t)t * l->hidden;
        gn_real *h = l->H.data + (size_t)(t + 1) * l->hidden;
        gn_real *c = l->C.data + (size_t)(t + 1) * l->hidden;
        for (j = 0; j < l->hidden; j++) {
            gn_real ai = l->bi.data[j], af = l->bf.data[j], ao = l->bo.data[j], ag = l->bg.data[j];
            for (i = 0; i < l->in; i++) {
                gn_real xv = xt[i];
                ai += l->Wi.data[(size_t)j * l->in + i] * xv;
                af += l->Wf.data[(size_t)j * l->in + i] * xv;
                ao += l->Wo.data[(size_t)j * l->in + i] * xv;
                ag += l->Wg.data[(size_t)j * l->in + i] * xv;
            }
            for (i = 0; i < l->hidden; i++) {
                gn_real hv = hp[i];
                ai += l->Ui.data[(size_t)j * l->hidden + i] * hv;
                af += l->Uf.data[(size_t)j * l->hidden + i] * hv;
                ao += l->Uo.data[(size_t)j * l->hidden + i] * hv;
                ag += l->Ug.data[(size_t)j * l->hidden + i] * hv;
            }
            l->I.data[(size_t)t * l->hidden + j] = gn_sigmoid(ai);
            l->F.data[(size_t)t * l->hidden + j] = gn_sigmoid(af);
            l->O.data[(size_t)t * l->hidden + j] = gn_sigmoid(ao);
            l->G.data[(size_t)t * l->hidden + j] = gn_tanh(ag);
            c[j] = l->F.data[(size_t)t * l->hidden + j] * cp[j]
                 + l->I.data[(size_t)t * l->hidden + j] * l->G.data[(size_t)t * l->hidden + j];
            h[j] = l->O.data[(size_t)t * l->hidden + j] * gn_tanh(c[j]);
        }
        for (j = 0; j < l->out; j++) {
            gn_real v = l->by.data[j];
            for (i = 0; i < l->hidden; i++) v += l->Wy.data[(size_t)j * l->hidden + i] * h[i];
            y->data[(size_t)t * l->out + j] = v;
        }
    }
    return GN_OK;
}

gn_status gn_lstm_backward(gn_lstm *l, const gn_mat *grad_y)
{
    int t, i, j, T = l->T;
    gn_mat dH, dC, dh, do_, dc, di, df, dg;
    if (!l || !grad_y || grad_y->rows != T || grad_y->cols != l->out) return GN_ERR_ARG;
    if (gn_mat_alloc(&dH, T + 1, l->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&dC, T + 1, l->hidden) != GN_OK) { gn_mat_free(&dH); return GN_ERR_NO_MEM; }
    gn_mat_zero(&dH); gn_mat_zero(&dC);
    for (t = 0; t < T; t++)
        for (j = 0; j < l->out; j++) l->gby.data[j] += grad_y->data[(size_t)t * l->out + j];
    for (t = 0; t < T; t++)
        for (j = 0; j < l->out; j++) {
            gn_real gy = grad_y->data[(size_t)t * l->out + j];
            if (gy == 0.0) continue;
            for (i = 0; i < l->hidden; i++) {
                l->gWy.data[(size_t)j * l->hidden + i] += gy * l->H.data[(size_t)(t + 1) * l->hidden + i];
                dH.data[(size_t)(t + 1) * l->hidden + i] += gy * l->Wy.data[(size_t)j * l->hidden + i];
            }
        }
    if (gn_mat_alloc(&dh, 1, l->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dC); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&do_, 1, l->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dC); gn_mat_free(&dh); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&dc, 1, l->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dC); gn_mat_free(&dh); gn_mat_free(&do_); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&di, 1, l->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dC); gn_mat_free(&dh); gn_mat_free(&do_); gn_mat_free(&dc); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&df, 1, l->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dC); gn_mat_free(&dh); gn_mat_free(&do_); gn_mat_free(&dc); gn_mat_free(&di); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&dg, 1, l->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dC); gn_mat_free(&dh); gn_mat_free(&do_); gn_mat_free(&dc); gn_mat_free(&di); gn_mat_free(&df); return GN_ERR_NO_MEM; }
    for (t = T - 1; t >= 0; t--) {
        const gn_real *xt = l->X.data + (size_t)t * l->in;
        const gn_real *hp = l->H.data + (size_t)t * l->hidden;
        const gn_real *cp = l->C.data + (size_t)t * l->hidden;
        const gn_real *c = l->C.data + (size_t)(t + 1) * l->hidden;
        const gn_real *it = l->I.data + (size_t)t * l->hidden;
        const gn_real *ft = l->F.data + (size_t)t * l->hidden;
        const gn_real *ot = l->O.data + (size_t)t * l->hidden;
        const gn_real *gt = l->G.data + (size_t)t * l->hidden;
        gn_real *dHt = dH.data + (size_t)(t + 1) * l->hidden;
        gn_real *dHp = dH.data + (size_t)t * l->hidden;
        gn_real *dCt = dC.data + (size_t)(t + 1) * l->hidden;
        gn_real *dCp = dC.data + (size_t)t * l->hidden;
        for (j = 0; j < l->hidden; j++) dh.data[j] = dHt[j];
        /* do = dh . tanh(c) ; dc = dh . o . (1-tanh^2(c)) + dC[t+1] */
        for (j = 0; j < l->hidden; j++) {
            gn_real th = gn_tanh(c[j]);
            do_.data[j] = dh.data[j] * th;
            dc.data[j] = dh.data[j] * ot[j] * (1.0 - th * th) + dCt[j];
        }
        /* dC[t] += dc . f */
        for (j = 0; j < l->hidden; j++) dCp[j] += dc.data[j] * ft[j];
        /* gate pre-attivazioni */
        for (j = 0; j < l->hidden; j++) {
            gn_real dci = dc.data[j];
            di.data[j] = dci * gt[j] * it[j] * (1.0 - it[j]);
            df.data[j] = dci * cp[j] * ft[j] * (1.0 - ft[j]);
            dg.data[j] = dci * it[j] * (1.0 - gt[j] * gt[j]);
        }
        for (j = 0; j < l->hidden; j++) do_.data[j] *= ot[j] * (1.0 - ot[j]);
        /* accumula gradienti pesi/bias */
        for (j = 0; j < l->hidden; j++) {
            gn_real di_, df_, do2, dg_;
            l->gbi.data[j] += di.data[j];
            l->gbf.data[j] += df.data[j];
            l->gbo.data[j] += do_.data[j];
            l->gbg.data[j] += dg.data[j];
            di_ = di.data[j]; df_ = df.data[j]; do2 = do_.data[j]; dg_ = dg.data[j];
            for (i = 0; i < l->in; i++) {
                gn_real xv = xt[i];
                l->gWi.data[(size_t)j * l->in + i] += di_ * xv;
                l->gWf.data[(size_t)j * l->in + i] += df_ * xv;
                l->gWo.data[(size_t)j * l->in + i] += do2 * xv;
                l->gWg.data[(size_t)j * l->in + i] += dg_ * xv;
            }
            for (i = 0; i < l->hidden; i++) {
                gn_real hv = hp[i];
                l->gUi.data[(size_t)j * l->hidden + i] += di_ * hv;
                l->gUf.data[(size_t)j * l->hidden + i] += df_ * hv;
                l->gUo.data[(size_t)j * l->hidden + i] += do2 * hv;
                l->gUg.data[(size_t)j * l->hidden + i] += dg_ * hv;
            }
        }
        /* dH[t] += U^T d(pre) */
        for (i = 0; i < l->hidden; i++) {
            gn_real s = 0.0;
            for (j = 0; j < l->hidden; j++)
                s += l->Ui.data[(size_t)j * l->hidden + i] * di.data[j]
                   + l->Uf.data[(size_t)j * l->hidden + i] * df.data[j]
                   + l->Uo.data[(size_t)j * l->hidden + i] * do_.data[j]
                   + l->Ug.data[(size_t)j * l->hidden + i] * dg.data[j];
            dHp[i] += s;
        }
    }
    gn_mat_free(&dH); gn_mat_free(&dC); gn_mat_free(&dh); gn_mat_free(&do_);
    gn_mat_free(&dc); gn_mat_free(&di); gn_mat_free(&df); gn_mat_free(&dg);
    return GN_OK;
}
