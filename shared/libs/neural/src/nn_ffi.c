/*
 * nn_ffi.c — shim ABI piatto per il binding BASIC del namespace NEURAL.
 *
 * Copre la SLICE RICORRENTE di gmbneural: RNN, LSTM, GRU, BiRNN (che wrappa i tre)
 * e i momenti PBP. Le struct C (gn_rnn, gn_lstm, gn_gru, gn_birnn) hanno stato:
 * qui c'è una tabella di handle (max 32 vivi per processo), come per il pacchetto PGM.
 *
 * Matrici: array piatti row-major DOUBLE, come in gmbneural.
 *   x  : T x in     y  : T x out     grad_y : T x out
 *
 * STEP applica un ottimizzatore (0=SGD, 1=momentum, 2=Adam) sui parametri del
 * modulo usando i gradienti accumulati da BACKWARD; i buffer di stato m/v sono
 * allocati al primo STEP. Adam usa un contatore di passo GLOBALE (non per-parametro).
 *
 * Uso sequenziale, non thread-safe (documentato nel README del pacchetto).
 */
#include "gn_rnn.h"
#include "gn_lstm.h"
#include "gn_gru.h"
#include "gn_birnn.h"
#include "gn_pbp.h"
#include "gn_conv.h"
#include "gn_fusedmm.h"
#include "gn_prune.h"
#include "gn_quant.h"
#include "gn_ternary.h"
#include "gn_tree.h"
#include "gn_forest.h"
#include "gn_adaboost.h"
#include "gn_snn.h"
#include "gn_bnn.h"
#include "gn_pinn.h"
#include "gn_detect.h"
#include "gn_knn.h"
#include "gn_metrics.h"
#include "gn_memplan.h"
#include "gn_act.h"
#include "gn_dense.h"
#include "gn_loss.h"
#include "gn_dropout.h"
#include "gn_normlayer.h"
#include "gn_chuck.h"
#include "gn_autograd.h"
#include "gn_gp.h"
#include "gn_norm.h"
#include "gn_ops.h"
#include "gn_opt.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef long long i64;

#define NN_MAX 32
#define NN_MAXP 40   /* massimo numero di matrici di parametri (BiRNN LSTM = 30) */

typedef struct nn_entry {
    int kind;        /* 1=RNN 2=LSTM 3=GRU 4=BiRNN 5=PBP-layer */
    int used;
    union {
        gn_rnn r;
        gn_lstm l;
        gn_gru g;
        gn_birnn b;
    } u;
    gn_opt opt;
    int step;
    int npar;
    gn_mat *m;
    gn_mat *v;
} nn_entry;

static nn_entry g_nn[NN_MAX];

static nn_entry *nn_get(i64 h)
{
    if (h < 1 || h > NN_MAX) return NULL;
    if (!g_nn[h - 1].used) return NULL;
    return &g_nn[h - 1];
}

static i64 nn_alloc(int kind)
{
    int i;
    for (i = 0; i < NN_MAX; i++) {
        if (g_nn[i].used) continue;
        memset(&g_nn[i], 0, sizeof(g_nn[i]));
        g_nn[i].used = 1;
        g_nn[i].kind = kind;
        return (i64)(i + 1);
    }
    return -1;
}

/* ── raccolta parametri/gradienti per tipo ─────────────────────────────────── */
static int nn_params(nn_entry *e, gn_mat **p, gn_mat **g)
{
    int n = 0;
    if (e->kind == 1) {
        gn_rnn *r = &e->u.r;
        p[n] = &r->Wxh; g[n++] = &r->gWxh;
        p[n] = &r->Whh; g[n++] = &r->gWhh;
        p[n] = &r->Why; g[n++] = &r->gWhy;
        p[n] = &r->bh;  g[n++] = &r->gbh;
        p[n] = &r->by;  g[n++] = &r->gby;
    } else if (e->kind == 2) {
        gn_lstm *l = &e->u.l;
        p[n] = &l->Wi; g[n++] = &l->gWi;
        p[n] = &l->Ui; g[n++] = &l->gUi;
        p[n] = &l->bi; g[n++] = &l->gbi;
        p[n] = &l->Wf; g[n++] = &l->gWf;
        p[n] = &l->Uf; g[n++] = &l->gUf;
        p[n] = &l->bf; g[n++] = &l->gbf;
        p[n] = &l->Wo; g[n++] = &l->gWo;
        p[n] = &l->Uo; g[n++] = &l->gUo;
        p[n] = &l->bo; g[n++] = &l->gbo;
        p[n] = &l->Wg; g[n++] = &l->gWg;
        p[n] = &l->Ug; g[n++] = &l->gUg;
        p[n] = &l->bg; g[n++] = &l->gbg;
        p[n] = &l->Wy; g[n++] = &l->gWy;
        p[n] = &l->by; g[n++] = &l->gby;
    } else if (e->kind == 3) {
        gn_gru *q = &e->u.g;
        p[n] = &q->Wz; g[n++] = &q->gWz;
        p[n] = &q->Uz; g[n++] = &q->gUz;
        p[n] = &q->bz; g[n++] = &q->gbz;
        p[n] = &q->Wr; g[n++] = &q->gWr;
        p[n] = &q->Ur; g[n++] = &q->gUr;
        p[n] = &q->br; g[n++] = &q->gbr;
        p[n] = &q->Wn; g[n++] = &q->gWn;
        p[n] = &q->Un; g[n++] = &q->gUn;
        p[n] = &q->bn; g[n++] = &q->gbn;
        p[n] = &q->Wo; g[n++] = &q->gWo;
        p[n] = &q->bo; g[n++] = &q->gbo;
    } else if (e->kind == 4) {
        gn_birnn *b = &e->u.b;
        int sub;
        p[n] = &b->Wy; g[n++] = &b->gWy;
        p[n] = &b->by; g[n++] = &b->gby;
        /* parametri delle due direzioni */
        for (sub = 0; sub < 2; sub++) {
            if (b->kind == GN_BIRNN_RNN) {
                gn_rnn *r = sub ? &b->rb : &b->rf;
                p[n] = &r->Wxh; g[n++] = &r->gWxh;
                p[n] = &r->Whh; g[n++] = &r->gWhh;
                p[n] = &r->Why; g[n++] = &r->gWhy;
                p[n] = &r->bh;  g[n++] = &r->gbh;
                p[n] = &r->by;  g[n++] = &r->gby;
            } else if (b->kind == GN_BIRNN_LSTM) {
                gn_lstm *l = sub ? &b->lb : &b->lf;
                p[n] = &l->Wi; g[n++] = &l->gWi;
                p[n] = &l->Ui; g[n++] = &l->gUi;
                p[n] = &l->bi; g[n++] = &l->gbi;
                p[n] = &l->Wf; g[n++] = &l->gWf;
                p[n] = &l->Uf; g[n++] = &l->gUf;
                p[n] = &l->bf; g[n++] = &l->gbf;
                p[n] = &l->Wo; g[n++] = &l->gWo;
                p[n] = &l->Uo; g[n++] = &l->gUo;
                p[n] = &l->bo; g[n++] = &l->gbo;
                p[n] = &l->Wg; g[n++] = &l->gWg;
                p[n] = &l->Ug; g[n++] = &l->gUg;
                p[n] = &l->bg; g[n++] = &l->gbg;
                p[n] = &l->Wy; g[n++] = &l->gWy;
                p[n] = &l->by; g[n++] = &l->gby;
            } else {
                gn_gru *q = sub ? &b->gb : &b->gf;
                p[n] = &q->Wz; g[n++] = &q->gWz;
                p[n] = &q->Uz; g[n++] = &q->gUz;
                p[n] = &q->bz; g[n++] = &q->gbz;
                p[n] = &q->Wr; g[n++] = &q->gWr;
                p[n] = &q->Ur; g[n++] = &q->gUr;
                p[n] = &q->br; g[n++] = &q->gbr;
                p[n] = &q->Wn; g[n++] = &q->gWn;
                p[n] = &q->Un; g[n++] = &q->gUn;
                p[n] = &q->bn; g[n++] = &q->gbn;
                p[n] = &q->Wo; g[n++] = &q->gWo;
                p[n] = &q->bo; g[n++] = &q->gbo;
            }
        }
    }
    return n;
}

/* ── STEP (ottimizzatore) ──────────────────────────────────────────────────── */
static i64 nn_step(i64 h, i64 okind, double lr)
{
    nn_entry *e = nn_get(h);
    gn_mat *p[NN_MAXP];
    gn_mat *g[NN_MAXP];
    int i, n;
    if (!e) return -1;
    if (okind < 0 || okind > 2 || lr <= 0.0) return -1;

    n = nn_params(e, p, g);
    if (n <= 0) return -1;

    if (e->m == NULL) {
        e->m = (gn_mat *)calloc((size_t)n, sizeof(gn_mat));
        e->v = (gn_mat *)calloc((size_t)n, sizeof(gn_mat));
        if (!e->m || !e->v) return -1;
        for (i = 0; i < n; i++) {
            if (gn_mat_alloc(&e->m[i], p[i]->rows, p[i]->cols) != GN_OK) return -1;
            if (gn_mat_alloc(&e->v[i], p[i]->rows, p[i]->cols) != GN_OK) return -1;
            gn_mat_zero(&e->m[i]);
            gn_mat_zero(&e->v[i]);
        }
    }

    e->opt = gn_opt_make((int)okind, (gn_real)lr);
    for (i = 0; i < n; i++) {
        /* Adam: t globale di passo (gn_opt_apply incrementa t ad ogni chiamata) */
        e->opt.t = e->step;
        if (gn_opt_apply(&e->opt, p[i], g[i], &e->m[i], &e->v[i]) != GN_OK) return -1;
    }
    e->step++;
    return 0;
}

static i64 nn_zerograd(i64 h)
{
    nn_entry *e = nn_get(h);
    gn_mat *p[NN_MAXP];
    gn_mat *g[NN_MAXP];
    int i, n;
    if (!e) return -1;
    n = nn_params(e, p, g);
    for (i = 0; i < n; i++) gn_mat_zero(g[i]);
    return 0;
}

static i64 nn_free_entry(i64 h)
{
    nn_entry *e = nn_get(h);
    int i;
    if (!e) return -1;
    if (e->kind == 1) gn_rnn_free(&e->u.r);
    else if (e->kind == 2) gn_lstm_free(&e->u.l);
    else if (e->kind == 3) gn_gru_free(&e->u.g);
    else if (e->kind == 4) gn_birnn_free(&e->u.b);
    if (e->m) {
        for (i = 0; i < e->npar; i++) gn_mat_free(&e->m[i]);
        free(e->m);
    }
    if (e->v) {
        for (i = 0; i < e->npar; i++) gn_mat_free(&e->v[i]);
        free(e->v);
    }
    memset(e, 0, sizeof(*e));
    return 0;
}

/* ── RNN ───────────────────────────────────────────────────────────────────── */
i64 ofx_nn_rnn_init(i64 in, i64 hidden, i64 out, i64 seed)
{
    i64 h = nn_alloc(1);
    if (h < 0) return -1;
    if (gn_rnn_init(&g_nn[h - 1].u.r, (int)in, (int)hidden, (int)out,
                    (unsigned long long)seed) != GN_OK) {
        memset(&g_nn[h - 1], 0, sizeof(g_nn[h - 1]));
        return -1;
    }
    g_nn[h - 1].npar = 5;
    return h;
}

i64 ofx_nn_rnn_forward(i64 h, const double *x, i64 T, double *y)
{
    nn_entry *e = nn_get(h);
    gn_mat mx, my;
    if (!e || e->kind != 1 || !x || !y || T <= 0) return -1;
    mx.rows = (int)T; mx.cols = e->u.r.in;   mx.data = (gn_real *)x;
    my.rows = (int)T; my.cols = e->u.r.out;  my.data = (gn_real *)y;
    return (i64)gn_rnn_forward(&e->u.r, &mx, &my);
}

i64 ofx_nn_rnn_backward(i64 h, const double *gy, i64 T)
{
    nn_entry *e = nn_get(h);
    gn_mat mg;
    if (!e || e->kind != 1 || !gy || T <= 0) return -1;
    mg.rows = (int)T; mg.cols = e->u.r.out; mg.data = (gn_real *)gy;
    return (i64)gn_rnn_backward(&e->u.r, &mg);
}

/* ── LSTM ──────────────────────────────────────────────────────────────────── */
i64 ofx_nn_lstm_init(i64 in, i64 hidden, i64 out, i64 seed)
{
    i64 h = nn_alloc(2);
    if (h < 0) return -1;
    if (gn_lstm_init(&g_nn[h - 1].u.l, (int)in, (int)hidden, (int)out,
                     (unsigned long long)seed) != GN_OK) {
        memset(&g_nn[h - 1], 0, sizeof(g_nn[h - 1]));
        return -1;
    }
    g_nn[h - 1].npar = 14;
    return h;
}

i64 ofx_nn_lstm_forward(i64 h, const double *x, i64 T, double *y)
{
    nn_entry *e = nn_get(h);
    gn_mat mx, my;
    if (!e || e->kind != 2 || !x || !y || T <= 0) return -1;
    mx.rows = (int)T; mx.cols = e->u.l.in;  mx.data = (gn_real *)x;
    my.rows = (int)T; my.cols = e->u.l.out; my.data = (gn_real *)y;
    return (i64)gn_lstm_forward(&e->u.l, &mx, &my);
}

i64 ofx_nn_lstm_backward(i64 h, const double *gy, i64 T)
{
    nn_entry *e = nn_get(h);
    gn_mat mg;
    if (!e || e->kind != 2 || !gy || T <= 0) return -1;
    mg.rows = (int)T; mg.cols = e->u.l.out; mg.data = (gn_real *)gy;
    return (i64)gn_lstm_backward(&e->u.l, &mg);
}

/* ── GRU ───────────────────────────────────────────────────────────────────── */
i64 ofx_nn_gru_init(i64 in, i64 hidden, i64 out, i64 seed)
{
    i64 h = nn_alloc(3);
    if (h < 0) return -1;
    if (gn_gru_init(&g_nn[h - 1].u.g, (int)in, (int)hidden, (int)out,
                    (unsigned long long)seed) != GN_OK) {
        memset(&g_nn[h - 1], 0, sizeof(g_nn[h - 1]));
        return -1;
    }
    g_nn[h - 1].npar = 11;
    return h;
}

i64 ofx_nn_gru_forward(i64 h, const double *x, i64 T, double *y)
{
    nn_entry *e = nn_get(h);
    gn_mat mx, my;
    if (!e || e->kind != 3 || !x || !y || T <= 0) return -1;
    mx.rows = (int)T; mx.cols = e->u.g.in;  mx.data = (gn_real *)x;
    my.rows = (int)T; my.cols = e->u.g.out; my.data = (gn_real *)y;
    return (i64)gn_gru_forward(&e->u.g, &mx, &my);
}

i64 ofx_nn_gru_backward(i64 h, const double *gy, i64 T)
{
    nn_entry *e = nn_get(h);
    gn_mat mg;
    if (!e || e->kind != 3 || !gy || T <= 0) return -1;
    mg.rows = (int)T; mg.cols = e->u.g.out; mg.data = (gn_real *)gy;
    return (i64)gn_gru_backward(&e->u.g, &mg);
}

/* ── BiRNN (kind: 0=RNN, 1=LSTM, 2=GRU) ────────────────────────────────────── */
i64 ofx_nn_birnn_init(i64 kind, i64 in, i64 hidden, i64 out, i64 seed)
{
    i64 h;
    if (kind < 0 || kind > 2) return -1;
    h = nn_alloc(4);
    if (h < 0) return -1;
    if (gn_birnn_init(&g_nn[h - 1].u.b, (int)kind, (int)in, (int)hidden, (int)out,
                      (unsigned long long)seed) != GN_OK) {
        memset(&g_nn[h - 1], 0, sizeof(g_nn[h - 1]));
        return -1;
    }
    g_nn[h - 1].npar = (kind == 0) ? 12 : (kind == 1) ? 30 : 24;
    return h;
}

i64 ofx_nn_birnn_forward(i64 h, const double *x, i64 T, double *y)
{
    nn_entry *e = nn_get(h);
    gn_mat mx, my;
    if (!e || e->kind != 4 || !x || !y || T <= 0) return -1;
    mx.rows = (int)T; mx.cols = e->u.b.in;  mx.data = (gn_real *)x;
    my.rows = (int)T; my.cols = e->u.b.out; my.data = (gn_real *)y;
    return (i64)gn_birnn_forward(&e->u.b, &mx, &my);
}

i64 ofx_nn_birnn_backward(i64 h, const double *gy, i64 T)
{
    nn_entry *e = nn_get(h);
    gn_mat mg;
    if (!e || e->kind != 4 || !gy || T <= 0) return -1;
    mg.rows = (int)T; mg.cols = e->u.b.out; mg.data = (gn_real *)gy;
    return (i64)gn_birnn_backward(&e->u.b, &mg);
}

/* ── comuni ────────────────────────────────────────────────────────────────── */
i64 ofx_nn_zerograd(i64 h) { return nn_zerograd(h); }
i64 ofx_nn_step(i64 h, i64 okind, double lr) { return nn_step(h, okind, lr); }
/* Libera un handle NEURAL (rete, cella o strato con stato). */
i64 ofx_nn_free(i64 h)
{
    return nn_free_entry(h);
}

/* ── PBP ───────────────────────────────────────────────────────────────────── */
i64 ofx_nn_pbp_moments(double m_a, double v_a, double *out)
{
    gn_real m_z = 0.0, v_z = 0.0;
    if (!out) return -1;
    gn_pbp_relu_moments((gn_real)m_a, (gn_real)v_a, &m_z, &v_z);
    out[0] = (double)m_z;
    out[1] = (double)v_z;
    return 0;
}

/* PBP su strato denso, senza handle: m_w/v_w sono i momenti dei pesi (out×in),
 * m_prev/v_prev i momenti in ingresso, m_z/v_z quelli in uscita. relu <> 0 applica
 * i momenti ReLU, altrimenti il layer resta lineare. */
i64 ofx_nn_pbp_player(const double *m_w, const double *v_w, const double *m_prev,
                      const double *v_prev, double *m_z, double *v_z,
                      i64 in, i64 out, double scaling, i64 relu)
{
    if (!m_w || !v_w || !m_prev || !v_prev || !m_z || !v_z || in <= 0 || out <= 0) return -1;
    return (i64)gn_pbp_layer_forward((const gn_real *)m_w, (const gn_real *)v_w,
                                     (const gn_real *)m_prev, (const gn_real *)v_prev,
                                     (int)in, (int)out, (gn_real)scaling, (int)relu,
                                     (gn_real *)m_z, (gn_real *)v_z);
}

/* ── CNN, pooling e FusedMM: chiamate DIRETTE (FFI fino a 64 argomenti) ───────
 * Le funzioni stateless non hanno piu' un handle di configurazione: dimensioni,
 * stride, pad e dilatazione sono parametri espliciti. Layout tensori: CHW
 * (canale, altezza, larghezza), DOUBLE row-major. */

/* Dimensione di uscita di una conv/pool: floor((dim + 2*pad - k)/stride) + 1. */
i64 ofx_nn_outsize(i64 dim, i64 k, i64 stride, i64 pad)
{
    if (dim <= 0 || k <= 0 || stride <= 0) return -1;
    return (dim + 2 * pad - k) / stride + 1;
}

/* out (oc×oh×ow) = conv2d(in c×h×w, weight oc×c×kh×kw). dil = 1 → standard. */
i64 ofx_nn_conv2d(double *out, const double *in, const double *weight,
                  i64 c, i64 h, i64 w, i64 oc, i64 kh, i64 kw,
                  i64 stride, i64 pad, i64 dil)
{
    if (!out || !in || !weight) return -1;
    if (c <= 0 || h <= 0 || w <= 0 || oc <= 0 || kh <= 0 || kw <= 0 || stride <= 0 || dil <= 0) return -1;
    if (dil > 1)
        return (i64)gn_conv2d_dilated((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                                      (const gn_real *)weight, NULL, (int)oc, (int)kh, (int)kw,
                                      (int)stride, (int)pad, (int)dil);
    return (i64)gn_conv2d((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                          (const gn_real *)weight, NULL, (int)oc, (int)kh, (int)kw,
                          (int)stride, (int)pad);
}

i64 ofx_nn_conv2db(double *out, const double *in, const double *weight, const double *bias,
                   i64 c, i64 h, i64 w, i64 oc, i64 kh, i64 kw,
                   i64 stride, i64 pad, i64 dil)
{
    if (!out || !in || !weight || !bias) return -1;
    if (c <= 0 || h <= 0 || w <= 0 || oc <= 0 || kh <= 0 || kw <= 0 || stride <= 0 || dil <= 0) return -1;
    if (dil > 1)
        return (i64)gn_conv2d_dilated((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                                      (const gn_real *)weight, (const gn_real *)bias, (int)oc,
                                      (int)kh, (int)kw, (int)stride, (int)pad, (int)dil);
    return (i64)gn_conv2d((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                          (const gn_real *)weight, (const gn_real *)bias, (int)oc,
                          (int)kh, (int)kw, (int)stride, (int)pad);
}

/* Deconvoluzione (transposed conv): out = (h-1)*stride-2*pad+kh per lato. */
i64 ofx_nn_deconv2d(double *out, const double *in, const double *weight,
                    i64 c, i64 h, i64 w, i64 oc, i64 kh, i64 kw, i64 stride, i64 pad)
{
    if (!out || !in || !weight) return -1;
    if (c <= 0 || h <= 0 || w <= 0 || oc <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    return (i64)gn_deconv2d((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                            (const gn_real *)weight, NULL, (int)oc, (int)kh, (int)kw,
                            (int)stride, (int)pad);
}

i64 ofx_nn_deconv2db(double *out, const double *in, const double *weight, const double *bias,
                     i64 c, i64 h, i64 w, i64 oc, i64 kh, i64 kw, i64 stride, i64 pad)
{
    if (!out || !in || !weight || !bias) return -1;
    if (c <= 0 || h <= 0 || w <= 0 || oc <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    return (i64)gn_deconv2d((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                            (const gn_real *)weight, (const gn_real *)bias, (int)oc,
                            (int)kh, (int)kw, (int)stride, (int)pad);
}

/* Winograd F(m×m, 3×3): stride 1, pad 1; tile 2 o 4 (h,w multipli di tile). */
i64 ofx_nn_winograd(double *out, const double *in, const double *weight,
                    i64 c, i64 h, i64 w, i64 oc, i64 tile)
{
    if (!out || !in || !weight) return -1;
    if (c <= 0 || h <= 0 || w <= 0 || oc <= 0) return -1;
    return (i64)gn_winograd_conv2d((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                                   (const gn_real *)weight, NULL, (int)oc, (int)((tile == 4) ? 4 : 2));
}

i64 ofx_nn_winogradb(double *out, const double *in, const double *weight, const double *bias,
                     i64 c, i64 h, i64 w, i64 oc, i64 tile)
{
    if (!out || !in || !weight || !bias) return -1;
    if (c <= 0 || h <= 0 || w <= 0 || oc <= 0) return -1;
    return (i64)gn_winograd_conv2d((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                                   (const gn_real *)weight, (const gn_real *)bias, (int)oc,
                                   (int)((tile == 4) ? 4 : 2));
}

/* im2col / col2im: col = (c*kh*kw) × (oh*ow). */
i64 ofx_nn_im2col(double *col, const double *in, i64 c, i64 h, i64 w,
                  i64 kh, i64 kw, i64 stride, i64 pad)
{
    int oh = 0, ow = 0;
    if (!col || !in || c <= 0 || h <= 0 || w <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    return (i64)gn_im2col((const gn_real *)in, (int)c, (int)h, (int)w, (int)kh, (int)kw,
                          (int)stride, (int)pad, (gn_real *)col, &oh, &ow);
}

i64 ofx_nn_col2im(double *out, const double *col, i64 c, i64 h, i64 w,
                  i64 kh, i64 kw, i64 stride, i64 pad)
{
    if (!out || !col || c <= 0 || h <= 0 || w <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    return (i64)gn_col2im((const gn_real *)col, (int)c, (int)h, (int)w, (int)kh, (int)kw,
                          (int)stride, (int)pad, (gn_real *)out);
}

/* Local Response Normalization. */
i64 ofx_nn_lrn(double *out, const double *in, i64 c, i64 h, i64 w, i64 size,
               double alpha, double beta, double biasv)
{
    if (!out || !in || c <= 0 || h <= 0 || w <= 0 || size <= 0) return -1;
    return (i64)gn_lrn((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                       (int)size, (gn_real)alpha, (gn_real)beta, (gn_real)biasv);
}

/* Resize: kind 0 = nearest, 1 = bilinear. */
i64 ofx_nn_resize(double *out, const double *in, i64 c, i64 h, i64 w, i64 oh, i64 ow, i64 kind)
{
    if (!out || !in || c <= 0 || h <= 0 || w <= 0 || oh <= 0 || ow <= 0) return -1;
    if (kind == 0)
        return (i64)gn_resize_nearest((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                                      (int)oh, (int)ow);
    return (i64)gn_resize_bilinear((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                                   (int)oh, (int)ow);
}

/* Pooling 2D (size/stride). MAXPOOLARG riceve l'indice ky*size+kx per output. */
i64 ofx_nn_maxpool(double *out, const double *in, i64 c, i64 h, i64 w, i64 size, i64 stride)
{
    if (!out || !in || c <= 0 || h <= 0 || w <= 0 || size <= 0 || stride <= 0) return -1;
    return (i64)gn_maxpool2d((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                             (int)size, (int)stride, NULL);
}

i64 ofx_nn_maxpool_arg(double *out, const double *in, i64 *argmax,
                       i64 c, i64 h, i64 w, i64 size, i64 stride)
{
    int *am;
    int oh, ow, n, i, rc;
    if (!out || !in || !argmax || c <= 0 || h <= 0 || w <= 0 || size <= 0 || stride <= 0) return -1;
    oh = (int)((h - size) / stride + 1);
    ow = (int)((w - size) / stride + 1);
    if (oh <= 0 || ow <= 0) return -1;
    n = (int)(c * oh * ow);
    am = (int *)calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    if (!am) return -1;
    rc = gn_maxpool2d((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                      (int)size, (int)stride, am);
    if (rc == GN_OK) for (i = 0; i < n; i++) argmax[i] = (i64)am[i];
    free(am);
    return (i64)rc;
}

i64 ofx_nn_avgpool(double *out, const double *in, i64 c, i64 h, i64 w, i64 size, i64 stride)
{
    if (!out || !in || c <= 0 || h <= 0 || w <= 0 || size <= 0 || stride <= 0) return -1;
    return (i64)gn_avgpool2d((gn_real *)out, (const gn_real *)in, (int)c, (int)h, (int)w,
                             (int)size, (int)stride);
}

i64 ofx_nn_unpool(double *out, const double *in, const i64 *argmax,
                  i64 c, i64 h, i64 w, i64 size, i64 stride, double fill)
{
    int *am;
    int oh, ow, n, i, rc;
    if (!out || !in || !argmax || c <= 0 || h <= 0 || w <= 0 || size <= 0 || stride <= 0) return -1;
    oh = (int)((h - size) / stride + 1);
    ow = (int)((w - size) / stride + 1);
    if (oh <= 0 || ow <= 0) return -1;
    n = (int)(c * oh * ow);
    am = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    if (!am) return -1;
    for (i = 0; i < n; i++) am[i] = (int)argmax[i];
    rc = gn_maxunpool2d((gn_real *)out, (const gn_real *)in, am, (int)c, (int)h, (int)w,
                        (int)size, (int)stride, (gn_real)fill);
    free(am);
    return (i64)rc;
}

/* FusedMM / aggregazione su matrice CSR (GAT/GraphSAGE). */
i64 ofx_nn_fusedmm(double *out, const i64 *rowptr, const i64 *colidx, const double *val,
                   const double *X, const double *Y,
                   i64 rows, i64 cols, i64 k, i64 nnz, double alpha, double beta)
{
    int *rp, *ci, i, rc;
    if (!out || !rowptr || !colidx || !X || !Y || rows <= 0 || cols <= 0 || k <= 0) return -1;
    rp = (int *)malloc((size_t)(rows + 1) * sizeof(int));
    ci = (int *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(int));
    if (!rp || !ci) { free(rp); free(ci); return -1; }
    for (i = 0; i <= (int)rows; i++) rp[i] = (int)rowptr[i];
    for (i = 0; i < (int)nnz; i++) ci[i] = (int)colidx[i];
    rc = gn_fusedmm_csr(rp, ci, (const gn_real *)val, (int)rows, (int)cols, (int)k,
                        (const gn_real *)X, (const gn_real *)Y, (gn_real)alpha, (gn_real)beta,
                        (gn_real *)out);
    free(rp);
    free(ci);
    return (i64)rc;
}

i64 ofx_nn_aggregate(double *out, const i64 *rowptr, const i64 *colidx, const double *val,
                     const double *Y, i64 rows, i64 cols, i64 k, i64 nnz,
                     double alpha, double beta)
{
    int *rp, *ci, i, rc;
    if (!out || !rowptr || !colidx || !Y || rows <= 0 || cols <= 0 || k <= 0) return -1;
    rp = (int *)malloc((size_t)(rows + 1) * sizeof(int));
    ci = (int *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(int));
    if (!rp || !ci) { free(rp); free(ci); return -1; }
    for (i = 0; i <= (int)rows; i++) rp[i] = (int)rowptr[i];
    for (i = 0; i < (int)nnz; i++) ci[i] = (int)colidx[i];
    rc = gn_aggregate_csr(rp, ci, (const gn_real *)val, (int)rows, (int)cols, (int)k,
                          (const gn_real *)Y, (gn_real)alpha, (gn_real)beta, (gn_real *)out);
    free(rp);
    free(ci);
    return (i64)rc;
}

/* ══ Pruning / quantizzazione affine / ternario ══════════════════════════════
 * Gli array a byte (signed/unsigned char) non esistono nel marshalling FFI
 * (solo DOUBLE e LONG): qui sono trasportati come array LONG e convertiti in
 * scratch buffer. I parametri di quantizzazione a uscita multipla sono
 * restituiti per indice in un array (QUANTPARAMS -> out(0)=scale, out(1)=zp).
 */

/* ── Pruning (2:4 strutturato, channel pruning) ───────────────────────────── */

i64 ofx_nn_sparsify24(double *w, i64 n, i64 window, i64 nonzeros)
{
    if (!w || n <= 0 || window <= 0) return -1;
    return (i64)gn_sparsify_2_4((gn_real *)w, (int)n, (int)window, (int)nonzeros);
}

i64 ofx_nn_prune_channel_l1(const double *w, i64 out_c, i64 per_ch, double *l1)
{
    if (!w || !l1 || out_c <= 0 || per_ch <= 0) return -1;
    gn_prune_channel_l1((const gn_real *)w, (int)out_c, (int)per_ch, (gn_real *)l1);
    return 0;
}

i64 ofx_nn_prune_channels_mask(const double *l1, i64 out_c, double keep_frac, i64 *mask)
{
    unsigned char *tmp;
    int i, kept;
    if (!l1 || !mask || out_c <= 0) return -1;
    tmp = (unsigned char *)calloc((size_t)out_c, 1);
    if (!tmp) return -1;
    kept = gn_prune_channels_mask((const gn_real *)l1, (int)out_c, (gn_real)keep_frac, tmp);
    for (i = 0; i < (int)out_c; i++) mask[i] = (i64)tmp[i];
    free(tmp);
    return (i64)kept;
}

/* ── conversione LONG[] <-> byte[] ────────────────────────────────────────── */

static signed char *i8_from_longs(const i64 *src, int n)
{
    signed char *b = (signed char *)malloc((size_t)(n > 0 ? n : 1));
    int i;
    if (!b) return NULL;
    for (i = 0; i < n; i++) b[i] = (signed char)src[i];
    return b;
}

static unsigned char *u8_from_longs(const i64 *src, int n)
{
    unsigned char *b = (unsigned char *)malloc((size_t)(n > 0 ? n : 1));
    int i;
    if (!b) return NULL;
    for (i = 0; i < n; i++) b[i] = (unsigned char)src[i];
    return b;
}

static int *i32_from_longs(const i64 *src, int n)
{
    int *b = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    int i;
    if (!b) return NULL;
    for (i = 0; i < n; i++) b[i] = (int)src[i];
    return b;
}

/* ── Quantizzazione affine INT8 / UINT8 ───────────────────────────────────── */

/* x -> int8 con (scale, zero_point); out LONG con i valori -128..127. */
i64 ofx_nn_quant_i8(const double *x, i64 n, double scale, i64 zero_point, i64 *out)
{
    signed char *q;
    int i;
    if (!x || !out || n <= 0) return -1;
    q = (signed char *)malloc((size_t)n);
    if (!q) return -1;
    gn_quant_i8((const gn_real *)x, (int)n, (gn_real)scale, (int)zero_point, q);
    for (i = 0; i < (int)n; i++) out[i] = (i64)q[i];
    free(q);
    return 0;
}

/* int8 (LONG) -> double. */
i64 ofx_nn_dequant_i8(const i64 *q, i64 n, double scale, i64 zero_point, double *out)
{
    signed char *b;
    if (!q || !out || n <= 0) return -1;
    b = i8_from_longs(q, (int)n);
    if (!b) return -1;
    gn_dequant_i8(b, (int)n, (gn_real)scale, (int)zero_point, (gn_real *)out);
    free(b);
    return 0;
}

/* Requantizzazione int32 -> int8 tra due griglie. */
i64 ofx_nn_requant_i8(const i64 *acc, i64 n, double in_scale, i64 in_zp,
                      double out_scale, i64 out_zp, i64 *out)
{
    int *a;
    signed char *q;
    int i;
    if (!acc || !out || n <= 0) return -1;
    a = i32_from_longs(acc, (int)n);
    q = (signed char *)malloc((size_t)n);
    if (!a || !q) { free(a); free(q); return -1; }
    gn_requant_i8(a, (int)n, (gn_real)in_scale, (int)in_zp,
                  (gn_real)out_scale, (int)out_zp, q);
    for (i = 0; i < (int)n; i++) out[i] = (i64)q[i];
    free(a);
    free(q);
    return 0;
}

/* out(0) = scale, out(1) = zero_point (griglia int8 [-128,127]). */
i64 ofx_nn_quant_params(const double *x, i64 n, double *out)
{
    gn_real scale = 1.0;
    int zp = 0;
    if (!x || !out || n <= 0) return -1;
    gn_quant_params((const gn_real *)x, (int)n, &scale, &zp);
    out[0] = (double)scale;
    out[1] = (double)zp;
    return 0;
}

/* GEMM quantizzato uint8: acc int32, correzione zero-point, bias int32 (n), requant. */
i64 ofx_nn_quant_gemm_u8(i64 *out, const i64 *a, const i64 *b, const i64 *bias,
                         i64 m, i64 k, i64 n,
                         double scale_a, i64 zp_a, double scale_b, i64 zp_b,
                         double scale_out, i64 zp_out)
{
    unsigned char *ao = NULL, *bo = NULL, *co = NULL;
    int *bi = NULL;
    int i, total;
    if (!out || !a || !b || m <= 0 || k <= 0 || n <= 0) return -1;
    total = (int)(m * n);
    ao = u8_from_longs(a, (int)(m * k));
    bo = u8_from_longs(b, (int)(k * n));
    co = (unsigned char *)calloc((size_t)total, 1);
    if (bias) bi = i32_from_longs(bias, (int)n);
    if (!ao || !bo || !co || (bias && !bi)) { free(ao); free(bo); free(co); free(bi); return -1; }
    gn_quant_gemm_u8(co, ao, bo, (int)m, (int)k, (int)n, (gn_real)scale_a, (int)zp_a,
                     (gn_real)scale_b, (int)zp_b, bi, (gn_real)scale_out, (int)zp_out);
    for (i = 0; i < total; i++) out[i] = (i64)co[i];
    free(ao); free(bo); free(co); free(bi);
    return 0;
}

/* Requantizzazione a shift (schema CMSIS-NN/NNoM). */
i64 ofx_nn_quant_requant_shift_i8(const i64 *acc, i64 n, i64 rshift, i64 *out)
{
    int *a;
    signed char *q;
    int i;
    if (!acc || !out || n <= 0) return -1;
    a = i32_from_longs(acc, (int)n);
    q = (signed char *)malloc((size_t)n);
    if (!a || !q) { free(a); free(q); return -1; }
    gn_quant_requant_shift_i8(a, (int)n, (int)rshift, q);
    for (i = 0; i < (int)n; i++) out[i] = (i64)q[i];
    free(a);
    free(q);
    return 0;
}

/* Convoluzione 2D quantizzata uint8->int8 (bias int32 per canale di uscita). */
i64 ofx_nn_quant_conv2d_i8(i64 *out, const i64 *in, const i64 *weight, const i64 *bias,
                           i64 c, i64 h, i64 w, i64 out_c, i64 kh, i64 kw,
                           i64 stride, i64 pad, i64 bias_shift, i64 out_shift)
{
    unsigned char *ib = NULL;
    signed char *wb = NULL, *ob = NULL;
    int *bi = NULL;
    int oh, ow, total, i;
    i64 rc;
    if (!out || !in || !weight) return -1;
    if (c <= 0 || h <= 0 || w <= 0 || out_c <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    oh = (int)((h + 2 * pad - kh) / stride + 1);
    ow = (int)((w + 2 * pad - kw) / stride + 1);
    if (oh <= 0 || ow <= 0) return -1;
    total = (int)(out_c * oh * ow);
    ib = u8_from_longs(in, (int)(c * h * w));
    wb = i8_from_longs(weight, (int)(out_c * c * kh * kw));
    ob = (signed char *)calloc((size_t)total, 1);
    if (bias) bi = i32_from_longs(bias, (int)out_c);
    if (!ib || !wb || !ob || (bias && !bi)) { free(ib); free(wb); free(ob); free(bi); return -1; }
    rc = (i64)gn_quant_conv2d_i8(ob, ib, (int)c, (int)h, (int)w, wb, bi, (int)out_c,
                                 (int)kh, (int)kw, (int)stride, (int)pad,
                                 (int)bias_shift, (int)out_shift);
    for (i = 0; i < total; i++) out[i] = (i64)ob[i];
    free(ib); free(wb); free(ob); free(bi);
    return rc;
}

i64 ofx_nn_quant_clamp_u8(i64 *x, i64 n, i64 lo, i64 hi)
{
    unsigned char *b;
    int i;
    if (!x || n <= 0) return -1;
    b = u8_from_longs(x, (int)n);
    if (!b) return -1;
    gn_quant_clamp_u8(b, (int)n, (int)lo, (int)hi);
    for (i = 0; i < (int)n; i++) x[i] = (i64)b[i];
    free(b);
    return 0;
}

/* Per-canale: scale(out_c) DOUBLE, zp(out_c) LONG (griglia uint8 [0,255]). */
i64 ofx_nn_quant_params_channel(const double *w, i64 out_c, i64 per_ch,
                                double *scale, i64 *zp)
{
    int *z = NULL;
    int i;
    if (!w || !scale || !zp || out_c <= 0 || per_ch <= 0) return -1;
    z = (int *)calloc((size_t)out_c, sizeof(int));
    if (!z) return -1;
    gn_quant_params_channel((const gn_real *)w, (int)out_c, (int)per_ch, (gn_real *)scale, z);
    for (i = 0; i < (int)out_c; i++) zp[i] = (i64)z[i];
    free(z);
    return 0;
}

/* Convoluzione uint8 con scala di uscita per-canale (M double[out_c]). */
i64 ofx_nn_quant_conv2d_i8_channel(i64 *out, const i64 *in, const i64 *weight,
                                   const i64 *bias, const double *M,
                                   i64 c, i64 h, i64 w, i64 out_c, i64 kh, i64 kw,
                                   i64 stride, i64 pad, i64 zp_in, i64 zp_out)
{
    unsigned char *ib = NULL, *ob = NULL;
    signed char *wb = NULL;
    int *bi = NULL;
    int oh, ow, total, i;
    i64 rc;
    if (!out || !in || !weight || !M) return -1;
    if (c <= 0 || h <= 0 || w <= 0 || out_c <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    oh = (int)((h + 2 * pad - kh) / stride + 1);
    ow = (int)((w + 2 * pad - kw) / stride + 1);
    if (oh <= 0 || ow <= 0) return -1;
    total = (int)(out_c * oh * ow);
    ib = u8_from_longs(in, (int)(c * h * w));
    wb = i8_from_longs(weight, (int)(out_c * c * kh * kw));
    ob = (unsigned char *)calloc((size_t)total, 1);
    if (bias) bi = i32_from_longs(bias, (int)out_c);
    if (!ib || !wb || !ob || (bias && !bi)) { free(ib); free(wb); free(ob); free(bi); return -1; }
    rc = (i64)gn_quant_conv2d_i8_channel(ob, ib, (int)c, (int)h, (int)w, wb, bi,
                                         (const gn_real *)M, (int)out_c, (int)kh, (int)kw,
                                         (int)stride, (int)pad, (int)zp_in, (int)zp_out);
    for (i = 0; i < total; i++) out[i] = (i64)ob[i];
    free(ib); free(wb); free(ob); free(bi);
    return rc;
}

/* ── Ternario (1.58-bit, bit-slicing) — handle 2001+ ──────────────────────── */

#define NN_TERN_MAX 32
#define NN_TERN_BASE 2000

static gn_ternary g_tern[NN_TERN_MAX];
static unsigned char g_tern_used[NN_TERN_MAX];

static gn_ternary *tern_get(i64 h)
{
    if (h <= NN_TERN_BASE || h > NN_TERN_BASE + NN_TERN_MAX) return NULL;
    if (!g_tern_used[h - NN_TERN_BASE - 1]) return NULL;
    return &g_tern[h - NN_TERN_BASE - 1];
}

/* Quantizza W (rows×cols, DOUBLE row-major) a ternario {-1,0,+1}. */
i64 ofx_nn_ternary_init(const double *w, i64 rows, i64 cols)
{
    gn_mat W;
    int i;
    i64 h;
    if (!w || rows <= 0 || cols <= 0) return -1;
    for (i = 0; i < NN_TERN_MAX; i++) {
        if (g_tern_used[i]) continue;
        memset(&g_tern[i], 0, sizeof(g_tern[i]));
        W.rows = (int)rows; W.cols = (int)cols; W.data = (gn_real *)w;
        if (gn_ternary_from_mat(&g_tern[i], &W) != GN_OK) return -1;
        g_tern_used[i] = 1;
        h = (i64)(NN_TERN_BASE + i + 1);
        return h;
    }
    return -1;
}

i64 ofx_nn_ternary_free(i64 h)
{
    gn_ternary *t = tern_get(h);
    if (!t) return -1;
    gn_ternary_free(t);
    memset(t, 0, sizeof(*t));
    g_tern_used[h - NN_TERN_BASE - 1] = 0;
    return 0;
}

i64 ofx_nn_ternary_act_bytes(i64 dim)
{
    if (dim <= 0) return -1;
    return (i64)gn_ternary_act_bytes((int)dim);
}

/* Prodotto scalare bit-sliced della riga `row`: a() = byte in LONG, nbits bit. */
i64 ofx_nn_ternary_dotrow(i64 h, i64 row, const i64 *a, i64 nbits)
{
    gn_ternary *t = tern_get(h);
    unsigned char *ab;
    int rc;
    if (!t || !a || row < 0 || row >= t->rows || nbits <= 0) return -1;
    ab = u8_from_longs(a, (int)gn_ternary_act_bytes((int)nbits));
    if (!ab) return -1;
    rc = gn_ternary_dot(t->w_pos + (size_t)row * t->stride,
                        t->w_neg + (size_t)row * t->stride, ab, (int)nbits);
    free(ab);
    return (i64)rc;
}

/* forward: x (batch×cols) -> out (batch×rows) = scale * (W ⊙ sign(x)). */
i64 ofx_nn_ternary_forward(i64 h, const double *x, i64 batch, double *out)
{
    gn_ternary *t = tern_get(h);
    gn_mat X, Y;
    gn_status rc;
    if (!t || !x || !out || batch <= 0) return -1;
    X.rows = (int)batch; X.cols = t->cols; X.data = (gn_real *)x;
    Y.rows = (int)batch; Y.cols = t->rows; Y.data = (gn_real *)out;
    rc = gn_ternary_forward(t, &X, &Y);
    return (i64)rc;
}

/* Dequantizza W in DOUBLE row-major (rows×cols). */
i64 ofx_nn_ternary_tomat(i64 h, double *w)
{
    gn_ternary *t = tern_get(h);
    gn_mat W;
    gn_status rc;
    if (!t || !w) return -1;
    W.rows = t->rows; W.cols = t->cols; W.data = (gn_real *)w;
    rc = gn_ternary_to_mat(t, &W);
    return (i64)rc;
}

/* ── Alberi CART / Random Forest / AdaBoost — handle 3001+/3101+/3201+ ─────
 * Le struct C hanno stato: tabelle di handle separate per tipo (uso sequenziale,
 * non thread-safe come il resto del pacchetto). */

#define NN_TREE_MAX 32
#define NN_TREE_BASE 3000
#define NN_FOR_MAX 16
#define NN_FOR_BASE 3100
#define NN_ADA_MAX 16
#define NN_ADA_BASE 3200

static gn_tree g_tree_tab[NN_TREE_MAX];
static unsigned char g_tree_used[NN_TREE_MAX];
static gn_forest g_for_tab[NN_FOR_MAX];
static unsigned char g_for_used[NN_FOR_MAX];
static gn_adaboost g_ada_tab[NN_ADA_MAX];
static unsigned char g_ada_used[NN_ADA_MAX];

static gn_tree *tree_get(i64 h)
{
    if (h <= NN_TREE_BASE || h > NN_TREE_BASE + NN_TREE_MAX) return NULL;
    if (!g_tree_used[h - NN_TREE_BASE - 1]) return NULL;
    return &g_tree_tab[h - NN_TREE_BASE - 1];
}

static gn_forest *for_get(i64 h)
{
    if (h <= NN_FOR_BASE || h > NN_FOR_BASE + NN_FOR_MAX) return NULL;
    if (!g_for_used[h - NN_FOR_BASE - 1]) return NULL;
    return &g_for_tab[h - NN_FOR_BASE - 1];
}

static gn_adaboost *ada_get(i64 h)
{
    if (h <= NN_ADA_BASE || h > NN_ADA_BASE + NN_ADA_MAX) return NULL;
    if (!g_ada_used[h - NN_ADA_BASE - 1]) return NULL;
    return &g_ada_tab[h - NN_ADA_BASE - 1];
}

/* Albero CART: 0 = default (depth 32, split 2, leaf 1). */
i64 ofx_nn_tree_init(i64 max_depth, i64 min_split, i64 min_leaf)
{
    int i;
    for (i = 0; i < NN_TREE_MAX; i++) {
        if (g_tree_used[i]) continue;
        memset(&g_tree_tab[i], 0, sizeof(g_tree_tab[i]));
        g_tree_tab[i].max_depth = (int)max_depth;
        g_tree_tab[i].min_samples_split = (int)min_split;
        g_tree_tab[i].min_samples_leaf = (int)min_leaf;
        g_tree_used[i] = 1;
        return (i64)(NN_TREE_BASE + i + 1);
    }
    return -1;
}

i64 ofx_nn_tree_fit_reg(i64 h, const double *X, const double *y, i64 n, i64 d)
{
    gn_tree *t = tree_get(h);
    if (!t || !X || !y || n <= 0 || d <= 0) return -1;
    return (i64)gn_tree_fit_reg(t, X, y, (int)n, (int)d);
}

i64 ofx_nn_tree_fit_cls(i64 h, const double *X, const i64 *y, i64 n, i64 d, i64 n_classes)
{
    gn_tree *t = tree_get(h);
    int *yc;
    i64 rc;
    if (!t || !X || !y || n <= 0 || d <= 0 || n_classes <= 0) return -1;
    yc = i32_from_longs(y, (int)n);
    if (!yc) return -1;
    rc = (i64)gn_tree_fit_cls(t, X, yc, (int)n, (int)d, (int)n_classes);
    free(yc);
    return rc;
}

double ofx_nn_tree_predict(i64 h, const double *x)
{
    gn_tree *t = tree_get(h);
    if (!t || !x) return 0.0;
    return gn_tree_predict(t, x);
}

i64 ofx_nn_tree_free(i64 h)
{
    gn_tree *t = tree_get(h);
    if (!t) return -1;
    gn_tree_free(t);
    memset(t, 0, sizeof(*t));
    g_tree_used[h - NN_TREE_BASE - 1] = 0;
    return 0;
}

/* Random Forest: max_features 0 = sqrt(d) (classif.) / d/3+1 (regr.), seed 0 = 12345. */
i64 ofx_nn_forest_init(i64 n_estimators, i64 max_depth, i64 min_split,
                       i64 max_features, i64 seed)
{
    int i;
    for (i = 0; i < NN_FOR_MAX; i++) {
        if (g_for_used[i]) continue;
        memset(&g_for_tab[i], 0, sizeof(g_for_tab[i]));
        g_for_tab[i].n_estimators = (int)n_estimators;
        g_for_tab[i].max_depth = (int)max_depth;
        g_for_tab[i].min_samples_split = (int)min_split;
        g_for_tab[i].max_features = (int)max_features;
        g_for_tab[i].seed = (unsigned)seed;
        g_for_used[i] = 1;
        return (i64)(NN_FOR_BASE + i + 1);
    }
    return -1;
}

i64 ofx_nn_forest_fit_reg(i64 h, const double *X, const double *y, i64 n, i64 d)
{
    gn_forest *f = for_get(h);
    if (!f || !X || !y || n <= 0 || d <= 0) return -1;
    return (i64)gn_forest_fit_reg(f, X, y, (int)n, (int)d);
}

i64 ofx_nn_forest_fit_cls(i64 h, const double *X, const i64 *y, i64 n, i64 d, i64 n_classes)
{
    gn_forest *f = for_get(h);
    int *yc;
    i64 rc;
    if (!f || !X || !y || n <= 0 || d <= 0 || n_classes <= 0) return -1;
    yc = i32_from_longs(y, (int)n);
    if (!yc) return -1;
    rc = (i64)gn_forest_fit_cls(f, X, yc, (int)n, (int)d, (int)n_classes);
    free(yc);
    return rc;
}

double ofx_nn_forest_predict(i64 h, const double *x)
{
    gn_forest *f = for_get(h);
    if (!f || !x) return 0.0;
    return gn_forest_predict(f, x);
}

/* AUC di Mann-Whitney (actual: 0/1 o valori continui, score: punteggio). */
double ofx_nn_auc(const double *actual, const double *score, i64 n)
{
    if (!actual || !score || n <= 0) return 0.0;
    return gn_auc(actual, score, (int)n);
}

i64 ofx_nn_forest_free(i64 h)
{
    gn_forest *f = for_get(h);
    if (!f) return -1;
    gn_forest_free(f);
    memset(f, 0, sizeof(*f));
    g_for_used[h - NN_FOR_BASE - 1] = 0;
    return 0;
}

/* AdaBoost (SAMME) binario: y in {0,1}; 0 = default (50 stump, lr 1.0). */
i64 ofx_nn_adaboost_init(i64 n_estimators, double learning_rate)
{
    int i;
    for (i = 0; i < NN_ADA_MAX; i++) {
        if (g_ada_used[i]) continue;
        memset(&g_ada_tab[i], 0, sizeof(g_ada_tab[i]));
        g_ada_tab[i].n_estimators = (int)n_estimators;
        g_ada_tab[i].learning_rate = learning_rate;
        g_ada_used[i] = 1;
        return (i64)(NN_ADA_BASE + i + 1);
    }
    return -1;
}

i64 ofx_nn_adaboost_fit(i64 h, const double *X, const i64 *y, i64 n, i64 d)
{
    gn_adaboost *b = ada_get(h);
    int *yc;
    i64 rc;
    if (!b || !X || !y || n <= 0 || d <= 0) return -1;
    yc = i32_from_longs(y, (int)n);
    if (!yc) return -1;
    rc = (i64)gn_adaboost_fit(b, X, yc, (int)n, (int)d);
    free(yc);
    return rc;
}

i64 ofx_nn_adaboost_predict(i64 h, const double *x)
{
    gn_adaboost *b = ada_get(h);
    if (!b || !x) return 0;
    return (i64)gn_adaboost_predict(b, x);
}

i64 ofx_nn_adaboost_free(i64 h)
{
    gn_adaboost *b = ada_get(h);
    if (!b) return -1;
    gn_adaboost_free(b);
    memset(b, 0, sizeof(*b));
    g_ada_used[h - NN_ADA_BASE - 1] = 0;
    return 0;
}

/* ── Utilità numeriche: SNN, BNN, PINN, detection, KNN, metriche, memplan ─── */

/* SNN */
double ofx_nn_snn_surrogate(double x) { return (double)gn_snn_surrogate((gn_real)x); }
double ofx_nn_snn_surrogate_grad(double x) { return (double)gn_snn_surrogate_grad((gn_real)x); }

i64 ofx_nn_snn_lut_index(const double *x, const i64 *a, const i64 *b, i64 n_c)
{
    int *ai, *bi;
    int rc;
    if (!x || !a || !b || n_c < 0 || n_c > 30) return 0;
    ai = i32_from_longs(a, (int)n_c);
    bi = i32_from_longs(b, (int)n_c);
    if (!ai || !bi) { free(ai); free(bi); return 0; }
    rc = gn_snn_lut_index((const gn_real *)x, ai, bi, (int)n_c);
    free(ai);
    free(bi);
    return (i64)rc;
}

/* BNN (pacchetti da 64 bit in array LONG) */
i64 ofx_nn_bnn_bytes(i64 n) { return (i64)gn_bnn_bytes((int)n); }
i64 ofx_nn_bnn_words(i64 n) { return (i64)gn_bnn_words((int)n); }

i64 ofx_nn_bnn_pack(i64 *out, const double *x, i64 n)
{
    if (!out || !x || n <= 0) return -1;
    gn_bnn_pack((unsigned long long *)out, (const gn_real *)x, (int)n);
    return 0;
}

double ofx_nn_bnn_thresh(double k, double h)
{
    return (double)gn_bnn_thresh((gn_real)k, (gn_real)h);
}

i64 ofx_nn_bnn_xnor(const i64 *a, const i64 *b, i64 n)
{
    if (!a || !b || n <= 0) return 0;
    return (i64)gn_bnn_xnor_popcount((const unsigned long long *)a,
                                     (const unsigned long long *)b, (int)n);
}

i64 ofx_nn_bnn_dot(const i64 *a, const i64 *b, i64 n)
{
    if (!a || !b || n <= 0) return 0;
    return (i64)gn_bnn_dot((const unsigned long long *)a,
                           (const unsigned long long *)b, (int)n);
}

/* PINN: residui fisici (quadrati) e normalizzazione adattiva delle loss */
double ofx_nn_pinn_heat(double ut, double uxx, double alpha)
{ return (double)gn_pinn_heat((gn_real)ut, (gn_real)uxx, (gn_real)alpha); }
double ofx_nn_pinn_wave(double utt, double uxx, double c)
{ return (double)gn_pinn_wave((gn_real)utt, (gn_real)uxx, (gn_real)c); }
double ofx_nn_pinn_laplace(double uxx, double uyy, double f)
{ return (double)gn_pinn_laplace((gn_real)uxx, (gn_real)uyy, (gn_real)f); }
double ofx_nn_pinn_advection(double ut, double ux, double beta)
{ return (double)gn_pinn_advection((gn_real)ut, (gn_real)ux, (gn_real)beta); }
double ofx_nn_pinn_burgers(double ut, double ux, double uxx, double u, double nu)
{ return (double)gn_pinn_burgers((gn_real)ut, (gn_real)ux, (gn_real)uxx, (gn_real)u, (gn_real)nu); }
double ofx_nn_pinn_boundary(double u, double ubc)
{ return (double)gn_pinn_boundary((gn_real)u, (gn_real)ubc); }
double ofx_nn_pinn_initial(double u, double u0)
{ return (double)gn_pinn_initial((gn_real)u, (gn_real)u0); }

double ofx_nn_pinn_adaptive_norm(const double *losses, i64 n)
{
    if (!losses || n <= 0) return 1.0;
    return (double)gn_pinn_adaptive_norm((const gn_real *)losses, (int)n);
}

/* Detection: IoU, NMS, decode YOLO, NMS multi-classe */
double ofx_nn_iou(const double *a, const double *b)
{
    if (!a || !b) return 0.0;
    return (double)gn_iou((const gn_real *)a, (const gn_real *)b);
}

i64 ofx_nn_nms(const double *boxes, const double *scores, i64 n, double iou_thresh, i64 *keep)
{
    int *k;
    int i, nk;
    if (!boxes || !scores || !keep || n <= 0) return 0;
    k = (int *)calloc((size_t)n, sizeof(int));
    if (!k) return 0;
    nk = gn_nms((const gn_real *)boxes, (const gn_real *)scores, (int)n, (gn_real)iou_thresh, k);
    for (i = 0; i < nk; i++) keep[i] = (i64)k[i];
    free(k);
    return (i64)nk;
}

/* Decode di una cella YOLO (nessun handle: i parametri della cella sono espliciti).
 * out(0..3) = bx, by, bw, bh normalizzati. */
i64 ofx_nn_yolo_box(double *out, double tx, double ty, double tw, double th,
                    double anchor_w, double anchor_h,
                    i64 cx, i64 cy, i64 grid_w, i64 grid_h)
{
    if (!out || anchor_w <= 0.0 || anchor_h <= 0.0 || grid_w <= 0 || grid_h <= 0) return -1;
    gn_yolo_decode_box((gn_real)tx, (gn_real)ty, (gn_real)tw, (gn_real)th,
                       (gn_real)anchor_w, (gn_real)anchor_h, (int)cx, (int)cy,
                       (int)grid_w, (int)grid_h, (gn_real *)out);
    return 0;
}

i64 ofx_nn_nms_multiclass(const double *boxes, double *probs, i64 n, i64 classes, double thresh)
{
    if (!boxes || !probs || n <= 0 || classes <= 0) return -1;
    gn_nms_multiclass((const gn_real *)boxes, (gn_real *)probs, (int)n, (int)classes, (gn_real)thresh);
    return 0;
}

/* KNN senza handle: il training set e' passato a ogni chiamata (Xtr come matrice
 * ntr×d, accettata anche nella forma 2-D di BASIC). */
i64 ofx_nn_knn_classify(const double *Xtr, const i64 *ytr, i64 ntr, i64 d,
                        i64 n_classes, i64 k, const double *x)
{
    int *y;
    i64 rc;
    if (!Xtr || !ytr || !x || ntr <= 0 || d <= 0 || n_classes <= 0) return -1;
    y = i32_from_longs(ytr, (int)ntr);
    if (!y) return -1;
    rc = (i64)gn_knn_classify(Xtr, y, (int)ntr, (int)d, (int)n_classes, (int)k, x);
    free(y);
    return rc;
}

double ofx_nn_knn_accuracy(const double *Xtr, const i64 *ytr, i64 ntr,
                           const double *Xte, const i64 *yte, i64 nte,
                           i64 d, i64 n_classes, i64 k)
{
    int *it, *ie;
    double rc;
    if (!Xtr || !ytr || !Xte || !yte || ntr <= 0 || nte <= 0 || d <= 0 || n_classes <= 0) return 0.0;
    it = i32_from_longs(ytr, (int)ntr);
    ie = i32_from_longs(yte, (int)nte);
    if (!it || !ie) { free(it); free(ie); return 0.0; }
    rc = gn_knn_accuracy(Xtr, it, (int)ntr, Xte, ie, (int)nte, (int)d, (int)n_classes, (int)k);
    free(it);
    free(ie);
    return rc;
}

/* Metriche */
i64 ofx_nn_confusion(const i64 *yt, const i64 *yp, i64 n, i64 k, i64 *cm)
{
    int *a, *b, *c;
    int i, rc;
    if (!yt || !yp || !cm || n < 0 || k <= 0) return -1;
    a = i32_from_longs(yt, (int)n);
    b = i32_from_longs(yp, (int)n);
    c = (int *)calloc((size_t)(k * k), sizeof(int));
    if (!a || !b || !c) { free(a); free(b); free(c); return -1; }
    rc = gn_metrics_confusion(a, b, (int)n, (int)k, c);
    for (i = 0; i < (int)(k * k); i++) cm[i] = (i64)c[i];
    free(a); free(b); free(c);
    return (i64)rc;
}

double ofx_nn_accuracy(const i64 *yt, const i64 *yp, i64 n)
{
    int *a, *b;
    double rc;
    if (!yt || !yp || n <= 0) return 0.0;
    a = i32_from_longs(yt, (int)n);
    b = i32_from_longs(yp, (int)n);
    if (!a || !b) { free(a); free(b); return 0.0; }
    rc = gn_metrics_accuracy(a, b, (int)n);
    free(a); free(b);
    return rc;
}

double ofx_nn_accuracy_cm(const i64 *cm, i64 k)
{
    int *c;
    double rc;
    if (!cm || k <= 0) return 0.0;
    c = i32_from_longs(cm, (int)(k * k));
    if (!c) return 0.0;
    rc = gn_metrics_accuracy_cm(c, (int)k);
    free(c);
    return rc;
}

/* PRECISION/RECALL/F1: per-classe in out(), macro come valore di ritorno. */
double ofx_nn_precision(const i64 *cm, i64 k, double *out)
{
    int *c;
    double macro = 0.0;
    int rc;
    if (!cm || !out || k <= 0) return 0.0;
    c = i32_from_longs(cm, (int)(k * k));
    if (!c) return 0.0;
    rc = gn_metrics_precision(c, (int)k, out, &macro);
    free(c);
    return rc == 0 ? macro : 0.0;
}

double ofx_nn_recall(const i64 *cm, i64 k, double *out)
{
    int *c;
    double macro = 0.0;
    int rc;
    if (!cm || !out || k <= 0) return 0.0;
    c = i32_from_longs(cm, (int)(k * k));
    if (!c) return 0.0;
    rc = gn_metrics_recall(c, (int)k, out, &macro);
    free(c);
    return rc == 0 ? macro : 0.0;
}

double ofx_nn_f1(const i64 *cm, i64 k, double *out)
{
    int *c;
    double macro = 0.0;
    int rc;
    if (!cm || !out || k <= 0) return 0.0;
    c = i32_from_longs(cm, (int)(k * k));
    if (!c) return 0.0;
    rc = gn_metrics_f1(c, (int)k, out, &macro);
    free(c);
    return rc == 0 ? macro : 0.0;
}

double ofx_nn_mse(const double *yt, const double *yp, i64 n)
{
    if (!yt || !yp || n <= 0) return 0.0;
    return (double)gn_metrics_mse((const gn_real *)yt, (const gn_real *)yp, (int)n);
}

double ofx_nn_mae(const double *yt, const double *yp, i64 n)
{
    if (!yt || !yp || n <= 0) return 0.0;
    return (double)gn_metrics_mae((const gn_real *)yt, (const gn_real *)yp, (int)n);
}

double ofx_nn_r2(const double *yt, const double *yp, i64 n)
{
    if (!yt || !yp || n <= 0) return 0.0;
    return (double)gn_metrics_r2((const gn_real *)yt, (const gn_real *)yp, (int)n);
}

/* Memplan: offset per tensore (lifetime) e dimensione totale. */
/* Memplan con array di UDT: TYPE MemTensor { Size AS DOUBLE : Birth AS LONG : Death AS LONG }
 * ha lo stesso layout della struct C (double + int32 + int32, 16 byte), quindi il
 * buffer arriva gia' nella forma di gn_memplan_tensor. */
double ofx_nn_memplan(const void *t, i64 n, double *offsets)
{
    if (!t || !offsets || n <= 0) return 0.0;
    return (double)gn_mem_plan((const gn_memplan_tensor *)t, (int)n, (gn_real *)offsets);
}


/* ══ Attivazioni / gradienti (gn_act) ═══════════════════════════════════════
 * I VALORI di sigmoid/tanh/relu/gelu sono gia' in TORCH: qui si espongono i
 * gradienti (y = attivazione gia' calcolata) e le attivazioni non coperte. */
/* Attivazioni 1-argomento: definizioni ESPLICITE (il gate di audit cerca i
 * simboli ofx_nn_act_* nel sorgente, non l espansione di una macro). */
double ofx_nn_act_sigmoid_grad(double x) { return (double)gn_sigmoid_grad((gn_real)x); }
double ofx_nn_act_tanh_grad(double x) { return (double)gn_tanh_grad((gn_real)x); }
double ofx_nn_act_relu_grad(double x) { return (double)gn_relu_grad((gn_real)x); }
double ofx_nn_act_gelu_grad(double x) { return (double)gn_gelu_grad((gn_real)x); }
double ofx_nn_act_leaky_relu(double x) { return (double)gn_leaky_relu((gn_real)x); }
double ofx_nn_act_leaky_relu_grad(double x) { return (double)gn_leaky_relu_grad((gn_real)x); }
double ofx_nn_act_elu(double x) { return (double)gn_elu((gn_real)x); }
double ofx_nn_act_elu_grad(double x) { return (double)gn_elu_grad((gn_real)x); }
double ofx_nn_act_identity(double x) { return (double)gn_identity((gn_real)x); }
double ofx_nn_act_identity_grad(double x) { return (double)gn_identity_grad((gn_real)x); }
double ofx_nn_act_threshold(double x) { return (double)gn_threshold((gn_real)x); }
double ofx_nn_act_threshold_grad(double x) { return (double)gn_threshold_grad((gn_real)x); }
double ofx_nn_act_hardsigmoid(double x) { return (double)gn_hardsigmoid((gn_real)x); }
double ofx_nn_act_hardsigmoid_grad(double x) { return (double)gn_hardsigmoid_grad((gn_real)x); }
double ofx_nn_act_hardswish(double x) { return (double)gn_hardswish((gn_real)x); }
double ofx_nn_act_hardswish_grad(double x) { return (double)gn_hardswish_grad((gn_real)x); }
double ofx_nn_act_silu(double x) { return (double)gn_silu((gn_real)x); }
double ofx_nn_act_silu_grad(double x) { return (double)gn_silu_grad((gn_real)x); }
double ofx_nn_act_mish(double x) { return (double)gn_mish((gn_real)x); }
double ofx_nn_act_mish_grad(double x) { return (double)gn_mish_grad((gn_real)x); }
double ofx_nn_act_selu(double x) { return (double)gn_selu((gn_real)x); }
double ofx_nn_act_selu_grad(double x) { return (double)gn_selu_grad((gn_real)x); }
double ofx_nn_act_plse(double x) { return (double)gn_plse((gn_real)x); }
double ofx_nn_act_plse_grad(double x) { return (double)gn_plse_grad((gn_real)x); }
double ofx_nn_act_loggy(double x) { return (double)gn_loggy((gn_real)x); }
double ofx_nn_act_loggy_grad(double x) { return (double)gn_loggy_grad((gn_real)x); }
double ofx_nn_act_hardtan(double x) { return (double)gn_hardtan((gn_real)x); }
double ofx_nn_act_hardtan_grad(double x) { return (double)gn_hardtan_grad((gn_real)x); }
double ofx_nn_act_lhtan(double x) { return (double)gn_lhtan((gn_real)x); }
double ofx_nn_act_lhtan_grad(double x) { return (double)gn_lhtan_grad((gn_real)x); }
double ofx_nn_act_relie(double x) { return (double)gn_relie((gn_real)x); }
double ofx_nn_act_relie_grad(double x) { return (double)gn_relie_grad((gn_real)x); }
double ofx_nn_act_ramp(double x) { return (double)gn_ramp((gn_real)x); }
double ofx_nn_act_ramp_grad(double x) { return (double)gn_ramp_grad((gn_real)x); }
double ofx_nn_act_stair(double x) { return (double)gn_stair((gn_real)x); }
double ofx_nn_act_stair_grad(double x) { return (double)gn_stair_grad((gn_real)x); }

double ofx_nn_act_celu(double x, double alpha) { return (double)gn_celu((gn_real)x, (gn_real)alpha); }
double ofx_nn_act_celu_grad(double x, double alpha) { return (double)gn_celu_grad((gn_real)x, (gn_real)alpha); }
double ofx_nn_act_prelu(double x, double alpha) { return (double)gn_prelu((gn_real)x, (gn_real)alpha); }
double ofx_nn_act_prelu_grad(double x, double alpha) { return (double)gn_prelu_grad((gn_real)x, (gn_real)alpha); }

i64 ofx_nn_act_crelu(double *out, const double *in, i64 n)
{
    if (!out || !in || n <= 0) return -1;
    gn_crelu((gn_real *)out, (const gn_real *)in, (int)n);
    return 0;
}

i64 ofx_nn_act_crelu_grad(double *grad_in, const double *grad_out, const double *in, i64 n)
{
    if (!grad_in || !grad_out || !in || n <= 0) return -1;
    gn_crelu_grad((gn_real *)grad_in, (const gn_real *)grad_out, (const gn_real *)in, (int)n);
    return 0;
}

/* ══ Strato denso addestrabile (gn_dense) — handle 5001+ ════════════════════
 * W: out×in, b: 1×out. Il forward memorizza x (batch×in) per il backward;
 * ZEROGRAD/STEP accumulano e applicano SGD/momentum/Adam come le RNN. */

#define NN_DENSE_MAX 16
#define NN_DENSE_BASE 5000

typedef struct {
    int used;
    gn_dense l;
    gn_opt opt;
    int step;
    gn_mat mW, mb, vW, vb;
} nn_dense_e;

static nn_dense_e g_dense[NN_DENSE_MAX];

static nn_dense_e *dense_get(i64 h)
{
    if (h <= NN_DENSE_BASE || h > NN_DENSE_BASE + NN_DENSE_MAX) return NULL;
    if (!g_dense[h - NN_DENSE_BASE - 1].used) return NULL;
    return &g_dense[h - NN_DENSE_BASE - 1];
}

i64 ofx_nn_dense_init(i64 in, i64 out, i64 seed)
{
    int i;
    if (in <= 0 || out <= 0) return -1;
    for (i = 0; i < NN_DENSE_MAX; i++) {
        nn_dense_e *e;
        if (g_dense[i].used) continue;
        e = &g_dense[i];
        memset(e, 0, sizeof(*e));
        if (gn_dense_init(&e->l, (int)in, (int)out, (unsigned long long)seed) != GN_OK) return -1;
        if (gn_mat_alloc(&e->mW, (int)out, (int)in) != GN_OK) { gn_dense_free(&e->l); return -1; }
        if (gn_mat_alloc(&e->vW, (int)out, (int)in) != GN_OK) { gn_dense_free(&e->l); gn_mat_free(&e->mW); return -1; }
        if (gn_mat_alloc(&e->mb, 1, (int)out) != GN_OK) { gn_dense_free(&e->l); gn_mat_free(&e->mW); gn_mat_free(&e->vW); return -1; }
        if (gn_mat_alloc(&e->vb, 1, (int)out) != GN_OK) {
            gn_dense_free(&e->l); gn_mat_free(&e->mW); gn_mat_free(&e->vW); gn_mat_free(&e->mb);
            return -1;
        }
        gn_mat_zero(&e->mW); gn_mat_zero(&e->vW);
        gn_mat_zero(&e->mb); gn_mat_zero(&e->vb);
        e->used = 1;
        return (i64)(NN_DENSE_BASE + i + 1);
    }
    return -1;
}

/* y (batch×out) = x (batch×in) · Wᵀ + b */
i64 ofx_nn_dense_forward(i64 h, const double *x, i64 batch, double *y)
{
    nn_dense_e *e = dense_get(h);
    gn_mat X, Y;
    if (!e || !x || !y || batch <= 0) return -1;
    X.rows = (int)batch; X.cols = e->l.in; X.data = (gn_real *)x;
    Y.rows = (int)batch; Y.cols = e->l.out; Y.data = (gn_real *)y;
    return (i64)gn_dense_forward(&e->l, &X, &Y);
}

/* grad_x (batch×in) = grad_y (batch×out) · W ; accumula gradW/gradb. */
i64 ofx_nn_dense_backward(i64 h, const double *gy, i64 batch, double *gx)
{
    nn_dense_e *e = dense_get(h);
    gn_mat GY, GX;
    if (!e || !gy || !gx || batch <= 0) return -1;
    GY.rows = (int)batch; GY.cols = e->l.out; GY.data = (gn_real *)gy;
    GX.rows = (int)batch; GX.cols = e->l.in; GX.data = (gn_real *)gx;
    return (i64)gn_dense_backward(&e->l, &GY, &GX);
}

/* W (out×in) e b (out). */
i64 ofx_nn_dense_params(i64 h, double *W, double *b)
{
    nn_dense_e *e = dense_get(h);
    int i;
    if (!e || !W || !b) return -1;
    for (i = 0; i < e->l.out * e->l.in; i++) W[i] = (double)e->l.W.data[i];
    for (i = 0; i < e->l.out; i++) b[i] = (double)e->l.b.data[i];
    return 0;
}

i64 ofx_nn_dense_setparams(i64 h, const double *W, const double *b)
{
    nn_dense_e *e = dense_get(h);
    int i;
    if (!e || !W || !b) return -1;
    for (i = 0; i < e->l.out * e->l.in; i++) e->l.W.data[i] = (gn_real)W[i];
    for (i = 0; i < e->l.out; i++) e->l.b.data[i] = (gn_real)b[i];
    return 0;
}

i64 ofx_nn_dense_zerograd(i64 h)
{
    nn_dense_e *e = dense_get(h);
    if (!e) return -1;
    gn_mat_zero(&e->l.gradW);
    gn_mat_zero(&e->l.gradb);
    return 0;
}

/* okind: 0 SGD, 1 momentum, 2 Adam. */
i64 ofx_nn_dense_step(i64 h, i64 okind, double lr)
{
    nn_dense_e *e = dense_get(h);
    if (!e || okind < 0 || okind > 2 || lr <= 0.0) return -1;
    e->opt = gn_opt_make((int)okind, (gn_real)lr);
    e->opt.t = e->step;
    if (gn_opt_apply(&e->opt, &e->l.W, &e->l.gradW, &e->mW, &e->vW) != GN_OK) return -1;
    if (gn_opt_apply(&e->opt, &e->l.b, &e->l.gradb, &e->mb, &e->vb) != GN_OK) return -1;
    e->step++;
    return 0;
}

i64 ofx_nn_dense_free(i64 h)
{
    nn_dense_e *e = dense_get(h);
    if (!e) return -1;
    gn_dense_free(&e->l);
    gn_mat_free(&e->mW); gn_mat_free(&e->vW);
    gn_mat_free(&e->mb); gn_mat_free(&e->vb);
    memset(e, 0, sizeof(*e));
    return 0;
}

/* ══ Perdite con gradiente (gn_loss): y, t e grad sono rows×cols ═══════════ */
i64 ofx_nn_loss_mse(const double *y, const double *t, i64 rows, i64 cols, double *grad, double *loss)
{
    gn_mat Y, T, G;
    if (!y || !t || !loss || rows <= 0 || cols <= 0) return -1;
    Y.rows = T.rows = (int)rows; Y.cols = T.cols = (int)cols;
    Y.data = (gn_real *)y; T.data = (gn_real *)t;
    G.rows = (int)rows; G.cols = (int)cols; G.data = (gn_real *)grad;
    *loss = (double)gn_loss_mse(&Y, &T, grad ? &G : NULL);
    return 0;
}

i64 ofx_nn_loss_mae(const double *y, const double *t, i64 rows, i64 cols, double *grad, double *loss)
{
    gn_mat Y, T, G;
    if (!y || !t || !loss || rows <= 0 || cols <= 0) return -1;
    Y.rows = T.rows = (int)rows; Y.cols = T.cols = (int)cols;
    Y.data = (gn_real *)y; T.data = (gn_real *)t;
    G.rows = (int)rows; G.cols = (int)cols; G.data = (gn_real *)grad;
    *loss = (double)gn_loss_mae(&Y, &T, grad ? &G : NULL);
    return 0;
}

i64 ofx_nn_loss_cross_entropy(const double *y, const double *t, i64 rows, i64 cols, double *grad, double *loss)
{
    gn_mat Y, T, G;
    if (!y || !t || !loss || rows <= 0 || cols <= 0) return -1;
    Y.rows = T.rows = (int)rows; Y.cols = T.cols = (int)cols;
    Y.data = (gn_real *)y; T.data = (gn_real *)t;
    G.rows = (int)rows; G.cols = (int)cols; G.data = (gn_real *)grad;
    *loss = (double)gn_loss_cross_entropy(&Y, &T, grad ? &G : NULL);
    return 0;
}

i64 ofx_nn_loss_binary_cross_entropy(const double *y, const double *t, i64 rows, i64 cols, double *grad, double *loss)
{
    gn_mat Y, T, G;
    if (!y || !t || !loss || rows <= 0 || cols <= 0) return -1;
    Y.rows = T.rows = (int)rows; Y.cols = T.cols = (int)cols;
    Y.data = (gn_real *)y; T.data = (gn_real *)t;
    G.rows = (int)rows; G.cols = (int)cols; G.data = (gn_real *)grad;
    *loss = (double)gn_loss_binary_cross_entropy(&Y, &T, grad ? &G : NULL);
    return 0;
}

/* ══ Dropout (gn_dropout) — handle 6001+ ════════════════════════════════════
 * La maschera e' interna: FORWARD la rigenera (train) e BACKWARD la riusa. */
#define NN_DROP_MAX 16
#define NN_DROP_BASE 6000

typedef struct { int used; gn_dropout d; } nn_drop_e;
static nn_drop_e g_drop[NN_DROP_MAX];

static nn_drop_e *drop_get(i64 h)
{
    if (h <= NN_DROP_BASE || h > NN_DROP_BASE + NN_DROP_MAX) return NULL;
    if (!g_drop[h - NN_DROP_BASE - 1].used) return NULL;
    return &g_drop[h - NN_DROP_BASE - 1];
}

i64 ofx_nn_dropout_init(i64 n, double rate)
{
    int i;
    if (n <= 0) return -1;
    for (i = 0; i < NN_DROP_MAX; i++) {
        if (g_drop[i].used) continue;
        memset(&g_drop[i], 0, sizeof(g_drop[i]));
        if (gn_dropout_init(&g_drop[i].d, (int)n, (gn_real)rate) != GN_OK) return -1;
        g_drop[i].used = 1;
        return (i64)(NN_DROP_BASE + i + 1);
    }
    return -1;
}

i64 ofx_nn_dropout_forward(i64 h, const double *x, double *out, i64 train, i64 seed)
{
    nn_drop_e *e = drop_get(h);
    if (!e || !x || !out) return -1;
    return (i64)gn_dropout_forward(&e->d, (const gn_real *)x, (gn_real *)out,
                                   (int)train, (unsigned long long)seed);
}

i64 ofx_nn_dropout_backward(i64 h, const double *grad_out, double *grad_x)
{
    nn_drop_e *e = drop_get(h);
    if (!e || !grad_out || !grad_x) return -1;
    return (i64)gn_dropout_backward(&e->d, (const gn_real *)grad_out, (gn_real *)grad_x);
}

i64 ofx_nn_dropout_free(i64 h)
{
    nn_drop_e *e = drop_get(h);
    if (!e) return -1;
    gn_dropout_free(&e->d);
    memset(e, 0, sizeof(*e));
    return 0;
}

/* ══ BatchNorm / LayerNorm (gn_normlayer) — handle 6101+ / 6201+ ════════════ */
#define NN_BN_MAX 16
#define NN_BN_BASE 6100
#define NN_LN_MAX 16
#define NN_LN_BASE 6200

typedef struct { int used; gn_batchnorm bn; } nn_bn_e;
typedef struct { int used; gn_layernorm ln; } nn_ln_e;
static nn_bn_e g_bn[NN_BN_MAX];
static nn_ln_e g_ln[NN_LN_MAX];

static nn_bn_e *bn_get(i64 h)
{
    if (h <= NN_BN_BASE || h > NN_BN_BASE + NN_BN_MAX) return NULL;
    if (!g_bn[h - NN_BN_BASE - 1].used) return NULL;
    return &g_bn[h - NN_BN_BASE - 1];
}

static nn_ln_e *ln_get(i64 h)
{
    if (h <= NN_LN_BASE || h > NN_LN_BASE + NN_LN_MAX) return NULL;
    if (!g_ln[h - NN_LN_BASE - 1].used) return NULL;
    return &g_ln[h - NN_LN_BASE - 1];
}

i64 ofx_nn_bn_init(i64 features, double eps, double momentum)
{
    int i;
    if (features <= 0) return -1;
    for (i = 0; i < NN_BN_MAX; i++) {
        if (g_bn[i].used) continue;
        memset(&g_bn[i], 0, sizeof(g_bn[i]));
        if (gn_batchnorm_init(&g_bn[i].bn, (int)features, (gn_real)eps, (gn_real)momentum) != GN_OK) return -1;
        g_bn[i].used = 1;
        return (i64)(NN_BN_BASE + i + 1);
    }
    return -1;
}

/* train <> 0 aggiorna le statistiche correnti; 0 usa rmean/rvar (inferenza). */
i64 ofx_nn_bn_forward(i64 h, const double *x, i64 batch, double *y, i64 train)
{
    nn_bn_e *e = bn_get(h);
    gn_mat X, Y;
    if (!e || !x || !y || batch <= 0) return -1;
    X.rows = (int)batch; X.cols = e->bn.features; X.data = (gn_real *)x;
    Y.rows = (int)batch; Y.cols = e->bn.features; Y.data = (gn_real *)y;
    return (i64)gn_batchnorm_forward(&e->bn, &X, &Y, (int)train);
}

i64 ofx_nn_bn_backward(i64 h, const double *gy, i64 batch, double *gx)
{
    nn_bn_e *e = bn_get(h);
    gn_mat GY, GX;
    if (!e || !gy || !gx || batch <= 0) return -1;
    GY.rows = (int)batch; GY.cols = e->bn.features; GY.data = (gn_real *)gy;
    GX.rows = (int)batch; GX.cols = e->bn.features; GX.data = (gn_real *)gx;
    return (i64)gn_batchnorm_backward(&e->bn, &GY, &GX);
}

i64 ofx_nn_bn_params(i64 h, double *gamma, double *beta)
{
    nn_bn_e *e = bn_get(h);
    int i;
    if (!e || !gamma || !beta) return -1;
    for (i = 0; i < e->bn.features; i++) { gamma[i] = (double)e->bn.gamma.data[i]; beta[i] = (double)e->bn.beta.data[i]; }
    return 0;
}

i64 ofx_nn_bn_setparams(i64 h, const double *gamma, const double *beta)
{
    nn_bn_e *e = bn_get(h);
    int i;
    if (!e || !gamma || !beta) return -1;
    for (i = 0; i < e->bn.features; i++) { e->bn.gamma.data[i] = (gn_real)gamma[i]; e->bn.beta.data[i] = (gn_real)beta[i]; }
    return 0;
}

i64 ofx_nn_bn_zerograd(i64 h)
{
    nn_bn_e *e = bn_get(h);
    if (!e) return -1;
    return (i64)gn_batchnorm_zero_grad(&e->bn);
}

i64 ofx_nn_bn_free(i64 h)
{
    nn_bn_e *e = bn_get(h);
    if (!e) return -1;
    gn_batchnorm_free(&e->bn);
    memset(e, 0, sizeof(*e));
    return 0;
}

i64 ofx_nn_ln_init(i64 features, double eps)
{
    int i;
    if (features <= 0) return -1;
    for (i = 0; i < NN_LN_MAX; i++) {
        if (g_ln[i].used) continue;
        memset(&g_ln[i], 0, sizeof(g_ln[i]));
        if (gn_layernorm_init(&g_ln[i].ln, (int)features, (gn_real)eps) != GN_OK) return -1;
        g_ln[i].used = 1;
        return (i64)(NN_LN_BASE + i + 1);
    }
    return -1;
}

i64 ofx_nn_ln_forward(i64 h, const double *x, i64 batch, double *y)
{
    nn_ln_e *e = ln_get(h);
    gn_mat X, Y;
    if (!e || !x || !y || batch <= 0) return -1;
    X.rows = (int)batch; X.cols = e->ln.features; X.data = (gn_real *)x;
    Y.rows = (int)batch; Y.cols = e->ln.features; Y.data = (gn_real *)y;
    return (i64)gn_layernorm_forward(&e->ln, &X, &Y);
}

i64 ofx_nn_ln_backward(i64 h, const double *gy, i64 batch, double *gx)
{
    nn_ln_e *e = ln_get(h);
    gn_mat GY, GX;
    if (!e || !gy || !gx || batch <= 0) return -1;
    GY.rows = (int)batch; GY.cols = e->ln.features; GY.data = (gn_real *)gy;
    GX.rows = (int)batch; GX.cols = e->ln.features; GX.data = (gn_real *)gx;
    return (i64)gn_layernorm_backward(&e->ln, &GY, &GX);
}

i64 ofx_nn_ln_params(i64 h, double *gamma, double *beta)
{
    nn_ln_e *e = ln_get(h);
    int i;
    if (!e || !gamma || !beta) return -1;
    for (i = 0; i < e->ln.features; i++) { gamma[i] = (double)e->ln.gamma.data[i]; beta[i] = (double)e->ln.beta.data[i]; }
    return 0;
}

i64 ofx_nn_ln_setparams(i64 h, const double *gamma, const double *beta)
{
    nn_ln_e *e = ln_get(h);
    int i;
    if (!e || !gamma || !beta) return -1;
    for (i = 0; i < e->ln.features; i++) { e->ln.gamma.data[i] = (gn_real)gamma[i]; e->ln.beta.data[i] = (gn_real)beta[i]; }
    return 0;
}

i64 ofx_nn_ln_zerograd(i64 h)
{
    nn_ln_e *e = ln_get(h);
    if (!e) return -1;
    return (i64)gn_layernorm_zero_grad(&e->ln);
}

i64 ofx_nn_ln_free(i64 h)
{
    nn_ln_e *e = ln_get(h);
    if (!e) return -1;
    gn_layernorm_free(&e->ln);
    memset(e, 0, sizeof(*e));
    return 0;
}

/* ══ Chuck: Adam con dampening/iniezione di rumore (gn_chuck) — 6301+ ══════
 * Un handle = un parametro (matrice rows×cols) con il suo pstate e i buffer m/v. */
#define NN_CHUCK_MAX 16
#define NN_CHUCK_BASE 6300

typedef struct {
    int used;
    gn_chuck c;
    gn_chuck_pstate p;
    gn_mat m, v;
    int rows, cols;
} nn_chuck_e;

static nn_chuck_e g_chuck[NN_CHUCK_MAX];

static nn_chuck_e *chuck_get(i64 h)
{
    if (h <= NN_CHUCK_BASE || h > NN_CHUCK_BASE + NN_CHUCK_MAX) return NULL;
    if (!g_chuck[h - NN_CHUCK_BASE - 1].used) return NULL;
    return &g_chuck[h - NN_CHUCK_BASE - 1];
}

i64 ofx_nn_chuck_init(void)
{
    int i;
    for (i = 0; i < NN_CHUCK_MAX; i++) {
        if (g_chuck[i].used) continue;
        memset(&g_chuck[i], 0, sizeof(g_chuck[i]));
        gn_chuck_init(&g_chuck[i].c);
        gn_chuck_pstate_init(&g_chuck[i].p);
        g_chuck[i].used = 1;
        return (i64)(NN_CHUCK_BASE + i + 1);
    }
    return -1;
}

i64 ofx_nn_chuck_loss(i64 h, double loss)
{
    nn_chuck_e *e = chuck_get(h);
    if (!e) return -1;
    gn_chuck_observe_loss(&e->c, (gn_real)loss);
    return 0;
}

/* param -= aggiornamento Chuck; m/v allocati al primo passo (o se cambiano le dim). */
i64 ofx_nn_chuck_step(i64 h, double *param, const double *grad, i64 rows, i64 cols,
                      double lr, double beta1, double beta2, double eps)
{
    nn_chuck_e *e = chuck_get(h);
    gn_mat P, G;
    if (!e || !param || !grad || rows <= 0 || cols <= 0 || lr <= 0.0) return -1;
    if (e->rows != (int)rows || e->cols != (int)cols) {
        gn_mat_free(&e->m);
        gn_mat_free(&e->v);
        if (gn_mat_alloc(&e->m, (int)rows, (int)cols) != GN_OK) return -1;
        if (gn_mat_alloc(&e->v, (int)rows, (int)cols) != GN_OK) { gn_mat_free(&e->m); return -1; }
        gn_mat_zero(&e->m);
        gn_mat_zero(&e->v);
        e->rows = (int)rows;
        e->cols = (int)cols;
    }
    P.rows = (int)rows; P.cols = (int)cols; P.data = (gn_real *)param;
    G.rows = (int)rows; G.cols = (int)cols; G.data = (gn_real *)grad;
    return (i64)gn_chuck_step(&e->c, &e->p, &P, &G, &e->m, &e->v,
                              (gn_real)lr, (gn_real)beta1, (gn_real)beta2, (gn_real)eps);
}

/* out(0)=dampen out(1)=loss_ema out(2)=noise out(3)=lr_scale out(4)=global_step out(5)=frozen */
i64 ofx_nn_chuck_state(i64 h, double *out)
{
    nn_chuck_e *e = chuck_get(h);
    if (!e || !out) return -1;
    out[0] = (double)e->c.dampen;
    out[1] = (double)e->c.loss_ema;
    out[2] = (double)e->c.noise;
    out[3] = (double)e->c.lr_scale;
    out[4] = (double)e->c.global_step;
    out[5] = (double)e->p.frozen;
    return 0;
}

i64 ofx_nn_chuck_reset(i64 h)
{
    nn_chuck_e *e = chuck_get(h);
    if (!e) return -1;
    gn_chuck_init(&e->c);
    gn_chuck_pstate_init(&e->p);
    if (e->rows > 0 && e->cols > 0) { gn_mat_zero(&e->m); gn_mat_zero(&e->v); }
    return 0;
}

i64 ofx_nn_chuck_free(i64 h)
{
    nn_chuck_e *e = chuck_get(h);
    if (!e) return -1;
    gn_mat_free(&e->m);
    gn_mat_free(&e->v);
    memset(e, 0, sizeof(*e));
    return 0;
}

/* ══ Autodiff scalare (gn_autograd) — handle 7001+ ══════════════════════════
 * Ogni nodo e' un handle: DATA/GRAD lo leggono, BACKWARD propaga i gradienti.
 * SUB crea un nodo neg interno (non esposto): viene liberato con FREE del nodo
 * risultante (flag own_child1). Gli altri nodi restano di proprieta' dei loro
 * handle: liberarli quando non servono piu' (prima i figli dei padri). */
#define NN_AUTO_MAX 2048
#define NN_AUTO_BASE 7000

typedef struct { gn_value *v; unsigned char own_child1; } nn_auto_e;
static nn_auto_e g_auto[NN_AUTO_MAX];

static gn_value *auto_get(i64 h)
{
    if (h <= NN_AUTO_BASE || h > NN_AUTO_BASE + NN_AUTO_MAX) return NULL;
    return g_auto[h - NN_AUTO_BASE - 1].v;
}

static i64 auto_reg(gn_value *v, int own_child1)
{
    int i;
    if (!v) return -1;
    for (i = 0; i < NN_AUTO_MAX; i++) {
        if (g_auto[i].v) continue;
        g_auto[i].v = v;
        g_auto[i].own_child1 = (unsigned char)own_child1;
        return (i64)(NN_AUTO_BASE + i + 1);
    }
    gn_value_free(v);
    return -1;
}

i64 ofx_nn_auto_value(double x)
{
    return auto_reg(gn_value_new((gn_real)x, NULL, 0, "v"), 0);
}

i64 ofx_nn_auto_add(i64 ha, i64 hb)
{
    gn_value *a = auto_get(ha), *b = auto_get(hb);
    if (!a || !b) return -1;
    return auto_reg(gn_value_add(a, b), 0);
}

/* SUB = add(a, neg(b)): il neg interno viene liberato dal FREE del risultato. */
i64 ofx_nn_auto_sub(i64 ha, i64 hb)
{
    gn_value *a = auto_get(ha), *b = auto_get(hb);
    if (!a || !b) return -1;
    return auto_reg(gn_value_sub(a, b), 1);
}

i64 ofx_nn_auto_mul(i64 ha, i64 hb)
{
    gn_value *a = auto_get(ha), *b = auto_get(hb);
    if (!a || !b) return -1;
    return auto_reg(gn_value_mul(a, b), 0);
}

i64 ofx_nn_auto_div(i64 ha, i64 hb)
{
    gn_value *a = auto_get(ha), *b = auto_get(hb);
    if (!a || !b) return -1;
    return auto_reg(gn_value_div(a, b), 0);
}

i64 ofx_nn_auto_pow(i64 ha, double p)
{
    gn_value *a = auto_get(ha);
    if (!a) return -1;
    return auto_reg(gn_value_pow(a, (gn_real)p), 0);
}

i64 ofx_nn_auto_neg(i64 ha)
{
    gn_value *a = auto_get(ha);
    if (!a) return -1;
    return auto_reg(gn_value_neg(a), 0);
}

i64 ofx_nn_auto_relu(i64 ha)
{
    gn_value *a = auto_get(ha);
    if (!a) return -1;
    return auto_reg(gn_value_relu(a), 0);
}

i64 ofx_nn_auto_tanh(i64 ha)
{
    gn_value *a = auto_get(ha);
    if (!a) return -1;
    return auto_reg(gn_value_tanh(a), 0);
}

i64 ofx_nn_auto_exp(i64 ha)
{
    gn_value *a = auto_get(ha);
    if (!a) return -1;
    return auto_reg(gn_value_exp(a), 0);
}

double ofx_nn_auto_data(i64 h)
{
    gn_value *v = auto_get(h);
    return v ? (double)v->data : 0.0;
}

double ofx_nn_auto_grad(i64 h)
{
    gn_value *v = auto_get(h);
    return v ? (double)v->grad : 0.0;
}

i64 ofx_nn_auto_backward(i64 h)
{
    gn_value *v = auto_get(h);
    if (!v) return -1;
    gn_value_backward(v);
    return 0;
}

i64 ofx_nn_auto_free(i64 h)
{
    gn_value *v = auto_get(h);
    int slot;
    if (!v) return -1;
    slot = (int)(h - NN_AUTO_BASE - 1);
    if (g_auto[slot].own_child1 && v->nprev == 2 && v->prev) gn_value_free(v->prev[1]);
    gn_value_free(v);
    g_auto[slot].v = NULL;
    g_auto[slot].own_child1 = 0;
    return 0;
}

/* ══ Programmazione genetica (gn_gp) — handle 7101+ ═════════════════════════
 * L'interfaccia host del motore C (spec con arity/random_code/fitness) e'
 * adattata a cio' che BASIC sa esprimere: l'alfabeto dei codici con la loro
 * arita' (array) e la fitness come callback che riceve il genoma (array).
 * Il motore e' quello di gn_gp: selezione a torneo, crossover 0.9, mutazione 0.1. */

#define NN_GP_MAX 8
#define NN_GP_BASE 7100
#define NN_GP_MAXCODE 64

typedef struct {
    int code[NN_GP_MAXCODE];
    int arity[NN_GP_MAXCODE];
    int n;
    double (*fit)(double *, long long);
} gp_ctx_t;

typedef struct {
    int used;
    gp_ctx_t ctx;
} nn_gp_e;

static nn_gp_e g_gp[NN_GP_MAX];
static gp_ctx_t *g_gp_cur;   /* correnti durante la chiamata a gn_gp_evolve */

static int gp_arity_cb(int code)
{
    int i;
    if (!g_gp_cur) return 0;
    for (i = 0; i < g_gp_cur->n; i++) if (g_gp_cur->code[i] == code) return g_gp_cur->arity[i];
    return 0;
}

static int gp_random_cb(gn_gp_rng *r, void *ctx)
{
    (void)ctx;
    if (!g_gp_cur || g_gp_cur->n <= 0) return 0;
    return g_gp_cur->code[gn_gp_rand_int(r, g_gp_cur->n)];
}

static double gp_fit_cb(const gn_gp_genome *g, void *ctx)
{
    gp_ctx_t *c = (gp_ctx_t *)ctx;
    double buf[GN_GP_MAXN];
    int i;
    if (!c->fit || !g) return 0.0;
    for (i = 0; i < g->n; i++) buf[i] = (double)g->code[i];
    return c->fit(buf, (long long)g->n);
}

static nn_gp_e *gp_get(i64 h)
{
    if (h <= NN_GP_BASE || h > NN_GP_BASE + NN_GP_MAX) return NULL;
    if (!g_gp[h - NN_GP_BASE - 1].used) return NULL;
    return &g_gp[h - NN_GP_BASE - 1];
}

/* codes/arity: alfabeto (stessa lunghezza); seed 0 = default del motore. */
i64 ofx_nn_gp_init(const i64 *codes, const i64 *arity, i64 n)
{
    int i, j;
    if (!codes || !arity || n <= 0 || n > NN_GP_MAXCODE) return -1;
    for (i = 0; i < NN_GP_MAX; i++) {
        if (g_gp[i].used) continue;
        memset(&g_gp[i], 0, sizeof(g_gp[i]));
        g_gp[i].ctx.n = (int)n;
        for (j = 0; j < (int)n; j++) {
            g_gp[i].ctx.code[j] = (int)codes[j];
            g_gp[i].ctx.arity[j] = (int)arity[j];
        }
        g_gp[i].used = 1;
        return (i64)(NN_GP_BASE + i + 1);
    }
    return -1;
}

/* Ritorna la lunghezza del genoma migliore; out_best() riceve i codici,
 * out_fit(0) la fitness. */
i64 ofx_nn_gp_evolve(i64 h, double (*fit)(double *, long long),
                     i64 *out_best, double *out_fit,
                     i64 pop, i64 gens, i64 seed, i64 max_depth)
{
    nn_gp_e *e = gp_get(h);
    gn_gp_spec spec;
    gn_gp_genome best;
    double bestfit = 0.0;
    int i, rc;
    if (!e || !fit || !out_best || pop < 2 || gens < 1) return -1;
    e->ctx.fit = fit;
    g_gp_cur = &e->ctx;
    spec.arity = gp_arity_cb;
    spec.random_code = gp_random_cb;
    spec.fitness = gp_fit_cb;
    rc = gn_gp_evolve(&spec, &e->ctx, (int)pop, (int)gens,
                      (unsigned long long)seed, (int)max_depth, &best, &bestfit);
    g_gp_cur = NULL;
    e->ctx.fit = NULL;
    if (rc != 0) return -1;
    for (i = 0; i < best.n; i++) out_best[i] = (i64)best.code[i];
    if (out_fit) out_fit[0] = bestfit;
    return (i64)best.n;
}

/* Fitness di un genoma dato (codes, n): utile per valutare/validare l'albero. */
double ofx_nn_gp_eval(i64 h, const i64 *codes, i64 n, double (*fit)(double *, long long))
{
    nn_gp_e *e = gp_get(h);
    gn_gp_genome g;
    double buf[GN_GP_MAXN];
    int i;
    if (!e || !codes || n <= 0 || n > GN_GP_MAXN || !fit) return 0.0;
    g.n = (int)n;
    for (i = 0; i < (int)n; i++) { g.code[i] = (int)codes[i]; buf[i] = (double)codes[i]; }
    return fit(buf, (long long)n);
}

i64 ofx_nn_gp_free(i64 h)
{
    nn_gp_e *e = gp_get(h);
    if (!e) return -1;
    memset(e, 0, sizeof(*e));
    return 0;
}

/* ══ Normalizzazione online (gn_norm) — handle 7201+ ════════════════════════
 * Osserva i campioni (max-abs + Welford), poi applica/denormalizza/z-score con
 * le statistiche addestrate: e' il pezzo che TORCH (una-shot) non copre. */
#define NN_NORM_MAX 16
#define NN_NORM_BASE 7200

typedef struct { int used; gn_norm n; } nn_norm_e;
static nn_norm_e g_norm[NN_NORM_MAX];

static nn_norm_e *norm_get(i64 h)
{
    if (h <= NN_NORM_BASE || h > NN_NORM_BASE + NN_NORM_MAX) return NULL;
    if (!g_norm[h - NN_NORM_BASE - 1].used) return NULL;
    return &g_norm[h - NN_NORM_BASE - 1];
}

i64 ofx_nn_norm_init(i64 dim)
{
    int i;
    if (dim <= 0) return -1;
    for (i = 0; i < NN_NORM_MAX; i++) {
        if (g_norm[i].used) continue;
        memset(&g_norm[i], 0, sizeof(g_norm[i]));
        if (gn_norm_init(&g_norm[i].n, (int)dim) != GN_OK) return -1;
        g_norm[i].used = 1;
        return (i64)(NN_NORM_BASE + i + 1);
    }
    return -1;
}

i64 ofx_nn_norm_observe(i64 h, const double *x)
{
    nn_norm_e *e = norm_get(h);
    if (!e || !x) return -1;
    return (i64)gn_norm_observe(&e->n, (const gn_real *)x);
}

/* margin <= 0 -> 1.0; allarga il fattore max-abs (es. 1.1 per un margine). */
i64 ofx_nn_norm_finalize(i64 h, double margin)
{
    nn_norm_e *e = norm_get(h);
    if (!e) return -1;
    gn_norm_finalize(&e->n, (gn_real)margin);
    return 0;
}

i64 ofx_nn_norm_apply(i64 h, const double *x, double *out)
{
    nn_norm_e *e = norm_get(h);
    if (!e || !x || !out) return -1;
    return (i64)gn_norm_apply(&e->n, (const gn_real *)x, (gn_real *)out);
}

i64 ofx_nn_norm_denormalize(i64 h, const double *x, double *out)
{
    nn_norm_e *e = norm_get(h);
    if (!e || !x || !out) return -1;
    return (i64)gn_norm_denormalize(&e->n, (const gn_real *)x, (gn_real *)out);
}

i64 ofx_nn_norm_zscore(i64 h, const double *x, double *out)
{
    nn_norm_e *e = norm_get(h);
    if (!e || !x || !out) return -1;
    return (i64)gn_norm_zscore(&e->n, (const gn_real *)x, (gn_real *)out);
}

/* out: dim scalari, poi dim medie, poi dim deviazioni standard. */
i64 ofx_nn_norm_stats(i64 h, double *out)
{
    nn_norm_e *e = norm_get(h);
    int i, dim;
    if (!e || !out) return -1;
    dim = e->n.dim;
    for (i = 0; i < dim; i++) out[i] = (double)e->n.scale[i];
    for (i = 0; i < dim; i++) out[dim + i] = (double)e->n.mean[i];
    for (i = 0; i < dim; i++) {
        double var = (e->n.count > 1) ? (double)e->n.m2[i] / (double)(e->n.count - 1) : 0.0;
        out[2 * dim + i] = var > 0.0 ? (double)sqrt(var) : 0.0;
    }
    return 0;
}

i64 ofx_nn_norm_count(i64 h)
{
    nn_norm_e *e = norm_get(h);
    if (!e) return -1;
    return (i64)e->n.count;
}

i64 ofx_nn_norm_free(i64 h)
{
    nn_norm_e *e = norm_get(h);
    if (!e) return -1;
    gn_norm_free(&e->n);
    memset(e, 0, sizeof(*e));
    return 0;
}

/* ══ Operazioni su righe/matrici (gn_ops, gn_core) ══════════════════════════
 * GATHERROWS/COPYBLOCK non hanno equivalente in TORCH (SLICE e' contiguo);
 * SLICEROWS e' l'equivalente diretto di gn_slice_rows. */

i64 ofx_nn_gather_rows(double *out, const double *data, i64 rows, i64 cols,
                       const i64 *indices, i64 n_idx)
{
    int *idx;
    i64 rc;
    if (!out || !data || !indices || rows <= 0 || cols <= 0 || n_idx <= 0) return -1;
    idx = i32_from_longs(indices, (int)n_idx);
    if (!idx) return -1;
    rc = (i64)gn_gather_rows((gn_real *)out, (const gn_real *)data, (int)rows, (int)cols,
                             idx, (int)n_idx);
    free(idx);
    return rc;
}

i64 ofx_nn_slice_rows(double *out, const double *data, i64 rows, i64 cols, i64 start, i64 count)
{
    if (!out || !data || rows <= 0 || cols <= 0) return -1;
    return (i64)gn_slice_rows((gn_real *)out, (const gn_real *)data, (int)rows, (int)cols,
                              (int)start, (int)count);
}

i64 ofx_nn_copy_block(double *out, const double *data, i64 rows, i64 cols,
                      i64 r0, i64 c0, i64 nr, i64 nc)
{
    if (!out || !data || rows <= 0 || cols <= 0) return -1;
    return (i64)gn_copy_block((gn_real *)out, (const gn_real *)data, (int)rows, (int)cols,
                              (int)r0, (int)c0, (int)nr, (int)nc);
}

/* f applicata a ogni elemento (stessa ABI del callback vettoriale: double(*)(double)). */
i64 ofx_nn_mat_apply(double *out, const double *in, i64 n, double (*f)(double))
{
    gn_mat O, I;
    if (!out || !in || n <= 0 || !f) return -1;
    O.rows = 1; O.cols = (int)n; O.data = (gn_real *)out;
    I.rows = 1; I.cols = (int)n; I.data = (gn_real *)in;
    return (i64)gn_mat_apply(&O, &I, (gn_unary)f);
}

double ofx_nn_mat_absmax(const double *in, i64 n)
{
    gn_mat I;
    if (!in || n <= 0) return 0.0;
    I.rows = 1; I.cols = (int)n; I.data = (gn_real *)in;
    return (double)gn_mat_absmax(&I);
}

/* Softmax con temperatura (TORCH.SOFTMAX copre la variante a temperatura 1). */
i64 ofx_nn_softmax_temp(double *out, const double *z, i64 rows, i64 cols, double temp)
{
    gn_mat O, Z;
    if (!out || !z || rows <= 0 || cols <= 0) return -1;
    O.rows = Z.rows = (int)rows; O.cols = Z.cols = (int)cols;
    O.data = (gn_real *)out; Z.data = (gn_real *)z;
    return (i64)gn_softmax_temp(&O, &Z, (gn_real)temp);
}

/* LUT SNN: s() e' lo stack contiguo dei tempi (n_t blocchi di n_tab tabelle da
 * y_dim); S[i] = s + i*n_tab*y_dim, tabella j[i]. y[k] += tab[k] (accumula). */
i64 ofx_nn_snn_lut_forward(const double *s, i64 n_tab, const i64 *j, i64 n_t, i64 y_dim, double *y)
{
    const gn_real **S;
    int *ji;
    int i;
    if (!s || !j || !y || n_tab <= 0 || n_t <= 0 || y_dim <= 0) return -1;
    S = (const gn_real **)malloc((size_t)n_t * sizeof(gn_real *));
    ji = i32_from_longs(j, (int)n_t);
    if (!S || !ji) { free(S); free(ji); return -1; }
    for (i = 0; i < (int)n_t; i++) S[i] = (const gn_real *)s + (size_t)i * (size_t)n_tab * (size_t)y_dim;
    gn_snn_lut_forward(S, ji, (int)n_t, (int)y_dim, (gn_real *)y);
    free(S);
    free(ji);
    return 0;
}
