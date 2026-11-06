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
#include "gn_opt.h"

#include <stdlib.h>
#include <string.h>

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
i64 ofx_nn_free(i64 h) { return nn_free_entry(h); }

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

