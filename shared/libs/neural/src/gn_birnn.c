/* gn_birnn.c — layer ricorrente bidirezionale (clean-room). */
#include "gn_birnn.h"
#include <string.h>
#include <stdint.h>

static gn_status aseg(gn_mat *m, int r, int c)
{
    if (m->rows == r && m->cols == c && m->data) return GN_OK;
    gn_mat_free(m);
    return gn_mat_alloc(m, r, c);
}

static void rev_rows(gn_mat *dst, const gn_mat *src)
{
    int R = src->rows, C = src->cols;
    for (int i = 0; i < R; i++)
        for (int j = 0; j < C; j++)
            GN_AT(*dst, i, j) = GN_AT(*src, R - 1 - i, j);
}

static void rfill(gn_mat *m, gn_real sc, unsigned long long *s)
{
    int n = m->rows * m->cols;
    for (int i = 0; i < n; i++) {
        *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
        m->data[i] = sc * (((gn_real)((*s >> 11) & 0x1FFFFF)) / 1048576.0 - 1.0);
    }
}

gn_status gn_birnn_init(gn_birnn *b, int kind, int in, int hidden, int out, unsigned long long seed)
{
    if (!b || in < 1 || hidden < 1 || out < 1) return GN_ERR_ARG;
    if (kind != GN_BIRNN_RNN && kind != GN_BIRNN_LSTM && kind != GN_BIRNN_GRU) return GN_ERR_ARG;
    memset(b, 0, sizeof *b);
    b->kind = kind; b->in = in; b->hidden = hidden; b->out = out;
    unsigned long long s = seed ? seed : 1, s2 = s ^ 0x9E3779B97F4A7C15ULL;
    if (kind == GN_BIRNN_RNN) {
        if (gn_rnn_init(&b->rf, in, hidden, hidden, s)  != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
        if (gn_rnn_init(&b->rb, in, hidden, hidden, s2) != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
    } else if (kind == GN_BIRNN_LSTM) {
        if (gn_lstm_init(&b->lf, in, hidden, hidden, s)  != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
        if (gn_lstm_init(&b->lb, in, hidden, hidden, s2) != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
    } else {
        if (gn_gru_init(&b->gf, in, hidden, hidden, s)  != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
        if (gn_gru_init(&b->gb, in, hidden, hidden, s2) != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
    }
    if (gn_mat_alloc(&b->Wy, out, 2 * hidden) != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&b->by, 1, out) != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&b->gWy, out, 2 * hidden) != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&b->gby, 1, out) != GN_OK) { gn_birnn_free(b); return GN_ERR_NO_MEM; }
    unsigned long long sr = s ^ 0xD1B54A32D192ED03ULL;
    rfill(&b->Wy, 1.0 / (gn_real)hidden, &sr);
    gn_mat_zero(&b->by);
    gn_mat_zero(&b->gWy);
    gn_mat_zero(&b->gby);
    return GN_OK;
}

void gn_birnn_free(gn_birnn *b)
{
    if (!b) return;
    if (b->kind == GN_BIRNN_RNN) { gn_rnn_free(&b->rf); gn_rnn_free(&b->rb); }
    else if (b->kind == GN_BIRNN_LSTM) { gn_lstm_free(&b->lf); gn_lstm_free(&b->lb); }
    else if (b->kind == GN_BIRNN_GRU) { gn_gru_free(&b->gf); gn_gru_free(&b->gb); }
    gn_mat_free(&b->Wy); gn_mat_free(&b->by);
    gn_mat_free(&b->gWy); gn_mat_free(&b->gby);
    gn_mat_free(&b->X); gn_mat_free(&b->Xrev);
    gn_mat_free(&b->Yf); gn_mat_free(&b->Yb); gn_mat_free(&b->Ybrev); gn_mat_free(&b->Ycat);
    gn_mat_free(&b->tmpW); gn_mat_free(&b->grad_cat);
    gn_mat_free(&b->grad_f); gn_mat_free(&b->grad_b_al); gn_mat_free(&b->grad_b_rev);
}

static gn_status dir_fwd(gn_birnn *b, const gn_mat *x, gn_mat *y)
{
    if (b->kind == GN_BIRNN_RNN) return gn_rnn_forward(&b->rf, x, y);
    if (b->kind == GN_BIRNN_LSTM) return gn_lstm_forward(&b->lf, x, y);
    return gn_gru_forward(&b->gf, x, y);
}
static gn_status dir_bwd(gn_birnn *b, const gn_mat *x, gn_mat *y)
{
    if (b->kind == GN_BIRNN_RNN) return gn_rnn_forward(&b->rb, x, y);
    if (b->kind == GN_BIRNN_LSTM) return gn_lstm_forward(&b->lb, x, y);
    return gn_gru_forward(&b->gb, x, y);
}
static gn_status dir_fwd_b(gn_birnn *b, const gn_mat *g)
{
    if (b->kind == GN_BIRNN_RNN) return gn_rnn_backward(&b->rf, g);
    if (b->kind == GN_BIRNN_LSTM) return gn_lstm_backward(&b->lf, g);
    return gn_gru_backward(&b->gf, g);
}
static gn_status dir_bwd_b(gn_birnn *b, const gn_mat *g)
{
    if (b->kind == GN_BIRNN_RNN) return gn_rnn_backward(&b->rb, g);
    if (b->kind == GN_BIRNN_LSTM) return gn_lstm_backward(&b->lb, g);
    return gn_gru_backward(&b->gb, g);
}
static void dir_zero(gn_birnn *b)
{
    if (b->kind == GN_BIRNN_RNN) { gn_rnn_zero_grad(&b->rf); gn_rnn_zero_grad(&b->rb); }
    else if (b->kind == GN_BIRNN_LSTM) { gn_lstm_zero_grad(&b->lf); gn_lstm_zero_grad(&b->lb); }
    else { gn_gru_zero_grad(&b->gf); gn_gru_zero_grad(&b->gb); }
}

gn_status gn_birnn_forward(gn_birnn *b, const gn_mat *x, gn_mat *y)
{
    if (!b || !x || !y) return GN_ERR_ARG;
    int T = x->rows, h = b->hidden;
    if (x->cols != b->in || y->rows != T || y->cols != b->out) return GN_ERR_ARG;
    b->T = T;
    if (aseg(&b->X, T, b->in) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_copy(&b->X, x) != GN_OK) return GN_ERR_NO_MEM;
    if (aseg(&b->Xrev, T, b->in) != GN_OK) return GN_ERR_NO_MEM;
    rev_rows(&b->Xrev, x);
    if (aseg(&b->Yf, T, h) != GN_OK) return GN_ERR_NO_MEM;
    if (aseg(&b->Ybrev, T, h) != GN_OK) return GN_ERR_NO_MEM;
    if (aseg(&b->Yb, T, h) != GN_OK) return GN_ERR_NO_MEM;
    if (aseg(&b->Ycat, T, 2 * h) != GN_OK) return GN_ERR_NO_MEM;

    if (dir_fwd(b, x, &b->Yf) != GN_OK) return GN_ERR_RANGE;
    if (dir_bwd(b, &b->Xrev, &b->Ybrev) != GN_OK) return GN_ERR_RANGE;
    rev_rows(&b->Yb, &b->Ybrev);
    for (int t = 0; t < T; t++) {
        for (int j = 0; j < h; j++) { GN_AT(b->Ycat, t, j) = GN_AT(b->Yf, t, j); GN_AT(b->Ycat, t, h + j) = GN_AT(b->Yb, t, j); }
    }
    if (gn_mat_mul_nt(y, &b->Ycat, &b->Wy) != GN_OK) return GN_ERR_RANGE;
    for (int t = 0; t < T; t++)
        for (int o = 0; o < b->out; o++) GN_AT(*y, t, o) += GN_AT(b->by, 0, o);
    return GN_OK;
}

gn_status gn_birnn_backward(gn_birnn *b, const gn_mat *grad_y)
{
    if (!b || !grad_y) return GN_ERR_ARG;
    int T = b->T, h = b->hidden;
    if (grad_y->rows != T || grad_y->cols != b->out) return GN_ERR_ARG;
    if (aseg(&b->tmpW, b->out, 2 * h) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_mul_tn(&b->tmpW, grad_y, &b->Ycat) != GN_OK) return GN_ERR_RANGE;
    gn_mat_add_inplace(&b->gWy, &b->tmpW);
    for (int t = 0; t < T; t++)
        for (int o = 0; o < b->out; o++) GN_AT(b->gby, 0, o) += GN_AT(*grad_y, t, o);

    if (aseg(&b->grad_cat, T, 2 * h) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_mul(&b->grad_cat, grad_y, &b->Wy) != GN_OK) return GN_ERR_RANGE;
    if (aseg(&b->grad_f, T, h) != GN_OK) return GN_ERR_NO_MEM;
    if (aseg(&b->grad_b_al, T, h) != GN_OK) return GN_ERR_NO_MEM;
    for (int t = 0; t < T; t++) for (int j = 0; j < h; j++) {
        GN_AT(b->grad_f, t, j) = GN_AT(b->grad_cat, t, j);
        GN_AT(b->grad_b_al, t, j) = GN_AT(b->grad_cat, t, h + j);
    }
    if (aseg(&b->grad_b_rev, T, h) != GN_OK) return GN_ERR_NO_MEM;
    rev_rows(&b->grad_b_rev, &b->grad_b_al);
    if (dir_fwd_b(b, &b->grad_f) != GN_OK) return GN_ERR_RANGE;
    if (dir_bwd_b(b, &b->grad_b_rev) != GN_OK) return GN_ERR_RANGE;
    return GN_OK;
}

gn_status gn_birnn_zero_grad(gn_birnn *b)
{
    if (!b) return GN_ERR_ARG;
    dir_zero(b);
    gn_mat_zero(&b->gWy);
    gn_mat_zero(&b->gby);
    return GN_OK;
}
