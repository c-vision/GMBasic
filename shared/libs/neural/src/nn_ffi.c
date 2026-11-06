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
#include "gn_opt.h"

#include <stdlib.h>
#include <string.h>

typedef long long i64;

#define NN_MAX 32
#define NN_LAYER_MAX 32
#define NN_LAYER_BASE 1000

typedef struct nn_layer nn_layer;
static nn_layer *lay_get(i64 h);
static void lay_reset(i64 h);
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
    /* kind 5 (PBP): copia dei parametri medi/varianza dello strato */
    int pbp_in, pbp_out;
    double *pbp_mw, *pbp_vw;
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
    else if (e->kind == 5) { free(e->pbp_mw); free(e->pbp_vw); }
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
/* Libera un handle NEURAL (rete/cella) o un handle di layer (conv/pool/fusedmm). */
i64 ofx_nn_free(i64 h)
{
    if (h > NN_LAYER_BASE) {
        lay_reset(h);
        return 0;
    }
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

/* PBP a handle: i 10 argomenti della funzione C superano il limite di 8 del
 * marshalling FFI. INIT copia m_w/v_w (out×in) nello handle; RUN prende 7 argomenti. */
i64 ofx_nn_pbp_init(const double *m_w, const double *v_w, i64 in, i64 out)
{
    i64 h;
    size_t n;
    nn_entry *e;
    if (!m_w || !v_w || in <= 0 || out <= 0) return -1;
    h = nn_alloc(5);
    if (h < 0) return -1;
    e = &g_nn[h - 1];
    n = (size_t)in * (size_t)out;
    e->pbp_mw = (double *)malloc(n * sizeof(double));
    e->pbp_vw = (double *)malloc(n * sizeof(double));
    if (!e->pbp_mw || !e->pbp_vw) {
        free(e->pbp_mw);
        free(e->pbp_vw);
        memset(e, 0, sizeof(*e));
        return -1;
    }
    memcpy(e->pbp_mw, m_w, n * sizeof(double));
    memcpy(e->pbp_vw, v_w, n * sizeof(double));
    e->pbp_in = (int)in;
    e->pbp_out = (int)out;
    return h;
}

i64 ofx_nn_pbp_run(i64 h, const double *m_z_prev, const double *v_z_prev,
                   double scaling, i64 relu, double *m_z, double *v_z)
{
    nn_entry *e = nn_get(h);
    if (!e || e->kind != 5 || !m_z_prev || !v_z_prev || !m_z || !v_z) return -1;
    return (i64)gn_pbp_layer_forward((const gn_real *)e->pbp_mw, (const gn_real *)e->pbp_vw,
                                     (const gn_real *)m_z_prev, (const gn_real *)v_z_prev,
                                     e->pbp_in, e->pbp_out, (gn_real)scaling, (int)relu,
                                     (gn_real *)m_z, (gn_real *)v_z);
}


/* ── CNN / FusedMM: configurazioni di layer su handle (spazio 1001+) ──────────
 * Le funzioni C di conv/pool/fusedmm hanno fino a 13 argomenti: il marshalling FFI
 * ne supporta 8. La configurazione (dimensioni, stride, pad, alpha/beta) vive in un
 * handle; il FORWARD prende solo gli array. */

struct nn_layer {
    int used;
    int kind;   /* 1 im2col 2 col2im 3 conv 4 deconv 5 winograd 6 lrn 7 resize
                 * 8 maxpool 9 avgpool 10 unpool 11 fusedmm 12 aggregate */
    int c, h, w, oc, kh, kw, stride, pad, dil, tile, size, oh, ow;
    int rows, cols, kk, nnz;
    double alpha, beta, scale;
};

static nn_layer g_layer[NN_LAYER_MAX];

static nn_layer *lay_get(i64 h)
{
    if (h <= NN_LAYER_BASE || h > NN_LAYER_BASE + NN_LAYER_MAX) return NULL;
    if (!g_layer[h - NN_LAYER_BASE - 1].used) return NULL;
    return &g_layer[h - NN_LAYER_BASE - 1];
}

static void lay_reset(i64 h)
{
    nn_layer *L = lay_get(h);
    if (L) memset(L, 0, sizeof(*L));
}

static i64 lay_alloc(int kind)
{
    int i;
    for (i = 0; i < NN_LAYER_MAX; i++) {
        if (g_layer[i].used) continue;
        memset(&g_layer[i], 0, sizeof(g_layer[i]));
        g_layer[i].used = 1;
        g_layer[i].kind = kind;
        g_layer[i].dil = 1;
        g_layer[i].tile = 2;
        g_layer[i].alpha = 1.0;
        g_layer[i].beta = 1.0;
        return (i64)(NN_LAYER_BASE + i + 1);
    }
    return -1;
}

/* Dimensione di uscita di una conv/pool: floor((dim + 2*pad - k)/stride) + 1. */
i64 ofx_nn_outsize(i64 dim, i64 k, i64 stride, i64 pad)
{
    if (dim <= 0 || k <= 0 || stride <= 0) return -1;
    return (dim + 2 * pad - k) / stride + 1;
}

i64 ofx_nn_conv_init(i64 c, i64 h, i64 w, i64 oc, i64 kh, i64 kw, i64 stride, i64 pad)
{
    i64 lh;
    nn_layer *L;
    if (c <= 0 || h <= 0 || w <= 0 || oc <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    lh = lay_alloc(3);
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w; L->oc = (int)oc;
    L->kh = (int)kh; L->kw = (int)kw; L->stride = (int)stride; L->pad = (int)pad;
    L->oh = (int)((h + 2 * pad - kh) / stride + 1);
    L->ow = (int)((w + 2 * pad - kw) / stride + 1);
    return lh;
}

i64 ofx_nn_conv_set_dil(i64 lh, i64 dil)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 3 || dil <= 0) return -1;
    L->dil = (int)dil;
    return 0;
}

/* conv (dil=1) / conv dilatata (dil>1); bias opzionale: _run senza, _runb con. */
i64 ofx_nn_conv_run(i64 lh, const double *in, const double *weight, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 3 || !in || !weight || !out) return -1;
    if (L->dil <= 1)
        return (i64)gn_conv2d((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                              (const gn_real *)weight, NULL, L->oc, L->kh, L->kw,
                              L->stride, L->pad);
    return (i64)gn_conv2d_dilated((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                                  (const gn_real *)weight, NULL, L->oc, L->kh, L->kw,
                                  L->stride, L->pad, L->dil);
}

i64 ofx_nn_conv_runb(i64 lh, const double *in, const double *weight, const double *bias, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 3 || !in || !weight || !bias || !out) return -1;
    if (L->dil <= 1)
        return (i64)gn_conv2d((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                              (const gn_real *)weight, (const gn_real *)bias, L->oc,
                              L->kh, L->kw, L->stride, L->pad);
    return (i64)gn_conv2d_dilated((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                                  (const gn_real *)weight, (const gn_real *)bias, L->oc,
                                  L->kh, L->kw, L->stride, L->pad, L->dil);
}

i64 ofx_nn_deconv_init(i64 c, i64 h, i64 w, i64 oc, i64 kh, i64 kw, i64 stride, i64 pad)
{
    i64 lh;
    nn_layer *L;
    if (c <= 0 || h <= 0 || w <= 0 || oc <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    lh = lay_alloc(4);
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w; L->oc = (int)oc;
    L->kh = (int)kh; L->kw = (int)kw; L->stride = (int)stride; L->pad = (int)pad;
    L->oh = (int)((h - 1) * stride - 2 * pad + kh);
    L->ow = (int)((w - 1) * stride - 2 * pad + kw);
    return lh;
}

i64 ofx_nn_deconv_run(i64 lh, const double *in, const double *weight, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 4 || !in || !weight || !out) return -1;
    return (i64)gn_deconv2d((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                            (const gn_real *)weight, NULL, L->oc, L->kh, L->kw,
                            L->stride, L->pad);
}

i64 ofx_nn_deconv_runb(i64 lh, const double *in, const double *weight, const double *bias, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 4 || !in || !weight || !bias || !out) return -1;
    return (i64)gn_deconv2d((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                            (const gn_real *)weight, (const gn_real *)bias, L->oc,
                            L->kh, L->kw, L->stride, L->pad);
}

i64 ofx_nn_wino_init(i64 c, i64 h, i64 w, i64 oc, i64 tile)
{
    i64 lh = lay_alloc(5);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w; L->oc = (int)oc;
    L->kh = 3; L->kw = 3; L->stride = 1; L->pad = 1;
    L->tile = (int)((tile == 4) ? 4 : 2);
    L->oh = (int)h; L->ow = (int)w;
    return lh;
}

i64 ofx_nn_wino_run(i64 lh, const double *in, const double *weight, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 5 || !in || !weight || !out) return -1;
    return (i64)gn_winograd_conv2d((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                                   (const gn_real *)weight, NULL, L->oc, L->tile);
}

i64 ofx_nn_wino_runb(i64 lh, const double *in, const double *weight, const double *bias, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 5 || !in || !weight || !bias || !out) return -1;
    return (i64)gn_winograd_conv2d((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                                   (const gn_real *)weight, (const gn_real *)bias, L->oc, L->tile);
}

i64 ofx_nn_im2col_init(i64 c, i64 h, i64 w, i64 kh, i64 kw, i64 stride, i64 pad)
{
    i64 lh = lay_alloc(1);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w;
    L->kh = (int)kh; L->kw = (int)kw; L->stride = (int)stride; L->pad = (int)pad;
    L->oh = (int)((h + 2 * pad - kh) / stride + 1);
    L->ow = (int)((w + 2 * pad - kw) / stride + 1);
    return lh;
}

i64 ofx_nn_im2col_run(i64 lh, const double *in, double *col)
{
    nn_layer *L = lay_get(lh);
    int oh = 0, ow = 0;
    if (!L || L->kind != 1 || !in || !col) return -1;
    return (i64)gn_im2col((const gn_real *)in, L->c, L->h, L->w, L->kh, L->kw,
                          L->stride, L->pad, (gn_real *)col, &oh, &ow);
}

i64 ofx_nn_col2im_init(i64 c, i64 h, i64 w, i64 kh, i64 kw, i64 stride, i64 pad)
{
    i64 lh = lay_alloc(2);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w;
    L->kh = (int)kh; L->kw = (int)kw; L->stride = (int)stride; L->pad = (int)pad;
    return lh;
}

i64 ofx_nn_col2im_run(i64 lh, const double *col, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 2 || !col || !out) return -1;
    return (i64)gn_col2im((const gn_real *)col, L->c, L->h, L->w, L->kh, L->kw,
                          L->stride, L->pad, (gn_real *)out);
}

i64 ofx_nn_lrn_init(i64 c, i64 h, i64 w, i64 size, double alpha, double beta, double biasv)
{
    i64 lh = lay_alloc(6);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w; L->size = (int)size;
    L->alpha = alpha; L->beta = beta; L->scale = biasv;
    return lh;
}

i64 ofx_nn_lrn_run(i64 lh, const double *in, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 6 || !in || !out) return -1;
    return (i64)gn_lrn((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                       L->size, L->alpha, L->beta, L->scale);
}

i64 ofx_nn_resize_init(i64 c, i64 h, i64 w, i64 oh, i64 ow)
{
    i64 lh = lay_alloc(7);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w; L->oh = (int)oh; L->ow = (int)ow;
    return lh;
}

/* kind: 0 nearest, 1 bilinear. */
i64 ofx_nn_resize_run(i64 lh, i64 kind, const double *in, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || L->kind != 7 || !in || !out) return -1;
    if (kind == 0)
        return (i64)gn_resize_nearest((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w, L->oh, L->ow);
    return (i64)gn_resize_bilinear((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w, L->oh, L->ow);
}

i64 ofx_nn_maxpool_init(i64 c, i64 h, i64 w, i64 size, i64 stride)
{
    i64 lh = lay_alloc(8);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w; L->size = (int)size; L->stride = (int)stride;
    L->oh = (int)((h - size) / stride + 1);
    L->ow = (int)((w - size) / stride + 1);
    return lh;
}

i64 ofx_nn_avgpool_init(i64 c, i64 h, i64 w, i64 size, i64 stride)
{
    i64 lh = lay_alloc(9);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w; L->size = (int)size; L->stride = (int)stride;
    L->oh = (int)((h - size) / stride + 1);
    L->ow = (int)((w - size) / stride + 1);
    return lh;
}

i64 ofx_nn_pool_run(i64 lh, const double *in, double *out)
{
    nn_layer *L = lay_get(lh);
    if (!L || (L->kind != 8 && L->kind != 9) || !in || !out) return -1;
    if (L->kind == 8)
        return (i64)gn_maxpool2d((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                                 L->size, L->stride, NULL);
    return (i64)gn_avgpool2d((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                             L->size, L->stride);
}

/* maxpool con argmax (LONG array, out×). */
i64 ofx_nn_pool_run_arg(i64 lh, const double *in, double *out, i64 *argmax)
{
    nn_layer *L = lay_get(lh);
    int *am = NULL;
    int n, i, rc;
    if (!L || L->kind != 8 || !in || !out || !argmax) return -1;
    n = L->c * L->oh * L->ow;
    am = (int *)calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    if (!am) return -1;
    rc = gn_maxpool2d((gn_real *)out, (const gn_real *)in, L->c, L->h, L->w,
                      L->size, L->stride, am);
    if (rc == GN_OK)
        for (i = 0; i < n; i++) argmax[i] = (i64)am[i];
    free(am);
    return (i64)rc;
}

i64 ofx_nn_unpool_init(i64 c, i64 h, i64 w, i64 size, i64 stride)
{
    i64 lh = lay_alloc(10);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->c = (int)c; L->h = (int)h; L->w = (int)w; L->size = (int)size; L->stride = (int)stride;
    L->oh = (int)((h - size) / stride + 1);
    L->ow = (int)((w - size) / stride + 1);
    return lh;
}

i64 ofx_nn_unpool_run(i64 lh, const double *in, const i64 *argmax, double fill, double *out)
{
    nn_layer *L = lay_get(lh);
    int *am = NULL;
    int n, i, rc;
    if (!L || L->kind != 10 || !in || !argmax || !out) return -1;
    n = L->c * L->oh * L->ow;
    am = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    if (!am) return -1;
    for (i = 0; i < n; i++) am[i] = (int)argmax[i];
    rc = gn_maxunpool2d((gn_real *)out, (const gn_real *)in, am, L->c, L->h, L->w,
                        L->size, L->stride, (gn_real)fill);
    free(am);
    return (i64)rc;
}

i64 ofx_nn_fusedmm_init(i64 rows, i64 cols, i64 k, i64 nnz, double alpha, double beta)
{
    i64 lh = lay_alloc(11);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->rows = (int)rows; L->cols = (int)cols; L->kk = (int)k; L->nnz = (int)nnz;
    L->alpha = alpha; L->beta = beta;
    return lh;
}

i64 ofx_nn_fusedmm_run(i64 lh, const i64 *rowptr, const i64 *colidx, const double *val,
                       const double *X, const double *Y, double *Z)
{
    nn_layer *L = lay_get(lh);
    int *rp = NULL, *ci = NULL;
    int i, rc;
    if (!L || L->kind != 11 || !rowptr || !colidx || !X || !Y || !Z) return -1;
    rp = (int *)malloc((size_t)(L->rows + 1) * sizeof(int));
    ci = (int *)malloc((size_t)(L->nnz > 0 ? L->nnz : 1) * sizeof(int));
    if (!rp || !ci) { free(rp); free(ci); return -1; }
    for (i = 0; i <= L->rows; i++) rp[i] = (int)rowptr[i];
    for (i = 0; i < L->nnz; i++) ci[i] = (int)colidx[i];
    rc = gn_fusedmm_csr(rp, ci, (const gn_real *)val, L->rows, L->cols, L->kk,
                        (const gn_real *)X, (const gn_real *)Y, L->alpha, L->beta, (gn_real *)Z);
    free(rp);
    free(ci);
    return (i64)rc;
}

i64 ofx_nn_aggregate_init(i64 rows, i64 cols, i64 k, i64 nnz, double alpha, double beta)
{
    i64 lh = lay_alloc(12);
    nn_layer *L;
    if (lh < 0) return -1;
    L = lay_get(lh);
    L->rows = (int)rows; L->cols = (int)cols; L->kk = (int)k; L->nnz = (int)nnz;
    L->alpha = alpha; L->beta = beta;
    return lh;
}

i64 ofx_nn_aggregate_run(i64 lh, const i64 *rowptr, const i64 *colidx, const double *val,
                         const double *Y, double *Z)
{
    nn_layer *L = lay_get(lh);
    int *rp = NULL, *ci = NULL;
    int i, rc;
    if (!L || L->kind != 12 || !rowptr || !colidx || !Y || !Z) return -1;
    rp = (int *)malloc((size_t)(L->rows + 1) * sizeof(int));
    ci = (int *)malloc((size_t)(L->nnz > 0 ? L->nnz : 1) * sizeof(int));
    if (!rp || !ci) { free(rp); free(ci); return -1; }
    for (i = 0; i <= L->rows; i++) rp[i] = (int)rowptr[i];
    for (i = 0; i < L->nnz; i++) ci[i] = (int)colidx[i];
    rc = gn_aggregate_csr(rp, ci, (const gn_real *)val, L->rows, L->cols, L->kk,
                          (const gn_real *)Y, L->alpha, L->beta, (gn_real *)Z);
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

/* GEMM quantizzato uint8: acc int32, correzione zero-point, bias int32 (n), requant.
 * par(0..5) = scale_a, zp_a, scale_b, zp_b, scale_out, zp_out — la firma resta a 8
 * argomenti: oltre 8 il marshalling VM/C non esegue la chiamata (limite noto). */
i64 ofx_nn_quant_gemm_u8(i64 *out, const i64 *a, const i64 *b, const i64 *bias,
                         const double *par, i64 m, i64 k, i64 n)
{
    unsigned char *ao = NULL, *bo = NULL, *co = NULL;
    int *bi = NULL;
    int i, total;
    double scale_a, scale_b, scale_out;
    int zp_a, zp_b, zp_out;
    if (!out || !a || !b || !par || m <= 0 || k <= 0 || n <= 0) return -1;
    scale_a = par[0]; zp_a = (int)par[1];
    scale_b = par[2]; zp_b = (int)par[3];
    scale_out = par[4]; zp_out = (int)par[5];
    total = (int)(m * n);
    ao = u8_from_longs(a, (int)(m * k));
    bo = u8_from_longs(b, (int)(k * n));
    co = (unsigned char *)calloc((size_t)total, 1);
    if (bias) bi = i32_from_longs(bias, (int)n);
    if (!ao || !bo || !co || (bias && !bi)) { free(ao); free(bo); free(co); free(bi); return -1; }
    gn_quant_gemm_u8(co, ao, bo, (int)m, (int)k, (int)n, (gn_real)scale_a, zp_a,
                     (gn_real)scale_b, zp_b, bi, (gn_real)scale_out, zp_out);
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
i64 ofx_nn_quant_conv2d_i8(i64 *out, const i64 *in, const i64 *weight,
                           const i64 *bias, const i64 *cfg)
{
    unsigned char *ib = NULL;
    signed char *wb = NULL, *ob = NULL;
    int *bi = NULL;
    int c, h, w, out_c, kh, kw, stride, pad, bias_shift, out_shift;
    int oh, ow, total, i;
    i64 rc;
    if (!out || !in || !weight || !cfg) return -1;
    c = (int)cfg[0]; h = (int)cfg[1]; w = (int)cfg[2]; out_c = (int)cfg[3];
    kh = (int)cfg[4]; kw = (int)cfg[5]; stride = (int)cfg[6]; pad = (int)cfg[7];
    bias_shift = (int)cfg[8]; out_shift = (int)cfg[9];
    if (c <= 0 || h <= 0 || w <= 0 || out_c <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    oh = (h + 2 * pad - kh) / stride + 1;
    ow = (w + 2 * pad - kw) / stride + 1;
    if (oh <= 0 || ow <= 0) return -1;
    total = out_c * oh * ow;
    ib = u8_from_longs(in, c * h * w);
    wb = i8_from_longs(weight, out_c * c * kh * kw);
    ob = (signed char *)calloc((size_t)total, 1);
    if (bias) bi = i32_from_longs(bias, out_c);
    if (!ib || !wb || !ob || (bias && !bi)) { free(ib); free(wb); free(ob); free(bi); return -1; }
    rc = (i64)gn_quant_conv2d_i8(ob, ib, c, h, w, wb, bi, out_c,
                                 kh, kw, stride, pad, bias_shift, out_shift);
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
                                   const i64 *bias, const double *M, const i64 *cfg)
{
    unsigned char *ib = NULL, *ob = NULL;
    signed char *wb = NULL;
    int *bi = NULL;
    int c, h, w, out_c, kh, kw, stride, pad, zp_in, zp_out;
    int oh, ow, total, i;
    i64 rc;
    if (!out || !in || !weight || !M || !cfg) return -1;
    c = (int)cfg[0]; h = (int)cfg[1]; w = (int)cfg[2]; out_c = (int)cfg[3];
    kh = (int)cfg[4]; kw = (int)cfg[5]; stride = (int)cfg[6]; pad = (int)cfg[7];
    zp_in = (int)cfg[8]; zp_out = (int)cfg[9];
    if (c <= 0 || h <= 0 || w <= 0 || out_c <= 0 || kh <= 0 || kw <= 0 || stride <= 0) return -1;
    oh = (h + 2 * pad - kh) / stride + 1;
    ow = (w + 2 * pad - kw) / stride + 1;
    if (oh <= 0 || ow <= 0) return -1;
    total = out_c * oh * ow;
    ib = u8_from_longs(in, c * h * w);
    wb = i8_from_longs(weight, out_c * c * kh * kw);
    ob = (unsigned char *)calloc((size_t)total, 1);
    if (bias) bi = i32_from_longs(bias, out_c);
    if (!ib || !wb || !ob || (bias && !bi)) { free(ib); free(wb); free(ob); free(bi); return -1; }
    rc = (i64)gn_quant_conv2d_i8_channel(ob, ib, c, h, w, wb, bi,
                                         (const gn_real *)M, out_c, kh, kw,
                                         stride, pad, zp_in, zp_out);
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
