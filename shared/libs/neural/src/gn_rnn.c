/* gn_rnn.c — RNN vanilla + BPTT. */
#include "gn_rnn.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static unsigned long long rr_next(unsigned long long *s)
{ *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; return *s; }
static gn_real rr_normal(unsigned long long *s)
{
    gn_real u1, u2;
    do { u1 = (gn_real)(rr_next(s) >> 11) / 9007199254740992.0; } while (u1 <= 1e-300);
    u2 = (gn_real)(rr_next(s) >> 11) / 9007199254740992.0;
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

static gn_status alloc_pair(gn_mat *p, gn_mat *g, int r, int c)
{
    if (gn_mat_alloc(p, r, c) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(g, r, c) != GN_OK) { gn_mat_free(p); return GN_ERR_NO_MEM; }
    return GN_OK;
}

gn_status gn_rnn_init(gn_rnn *r, int in, int hidden, int out, unsigned long long seed)
{
    int i, n;
    gn_real s1, s2, s3;
    if (!r || in <= 0 || hidden <= 0 || out <= 0) return GN_ERR_ARG;
    r->in = in; r->hidden = hidden; r->out = out; r->T = 0;
    r->H.rows = r->H.cols = 0; r->H.data = NULL;
    r->X.rows = r->X.cols = 0; r->X.data = NULL;
    if (seed == 0) seed = 88172645463325252ULL;
    if (alloc_pair(&r->Wxh, &r->gWxh, hidden, in) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&r->Whh, &r->gWhh, hidden, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&r->Why, &r->gWhy, out, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&r->bh, &r->gbh, 1, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&r->by, &r->gby, 1, out) != GN_OK) return GN_ERR_NO_MEM;
    s1 = sqrt(1.0 / (gn_real)in);
    s2 = sqrt(1.0 / (gn_real)hidden);
    s3 = sqrt(1.0 / (gn_real)hidden);
    n = hidden * in;  for (i = 0; i < n; i++) r->Wxh.data[i] = s1 * rr_normal(&seed);
    n = hidden * hidden; for (i = 0; i < n; i++) r->Whh.data[i] = s2 * rr_normal(&seed);
    n = out * hidden; for (i = 0; i < n; i++) r->Why.data[i] = s3 * rr_normal(&seed);
    return GN_OK;
}

void gn_rnn_free(gn_rnn *r)
{
    if (!r) return;
    gn_mat_free(&r->Wxh); gn_mat_free(&r->Whh); gn_mat_free(&r->Why);
    gn_mat_free(&r->bh); gn_mat_free(&r->by);
    gn_mat_free(&r->gWxh); gn_mat_free(&r->gWhh); gn_mat_free(&r->gWhy);
    gn_mat_free(&r->gbh); gn_mat_free(&r->gby);
    gn_mat_free(&r->H); gn_mat_free(&r->X);
}

gn_status gn_rnn_forward(gn_rnn *r, const gn_mat *x, gn_mat *y)
{
    int t, i, j, k;
    if (!r || !x || !y || x->cols != r->in || y->rows != x->rows || y->cols != r->out)
        return GN_ERR_ARG;
    r->T = x->rows;
    gn_mat_free(&r->H); gn_mat_free(&r->X);
    if (gn_mat_alloc(&r->H, r->T + 1, r->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&r->X, r->T, r->in) != GN_OK) return GN_ERR_NO_MEM;
    gn_mat_copy(&r->X, x);
    /* H[0] = 0 */
    for (t = 0; t < r->T; t++) {
        for (i = 0; i < r->hidden; i++) {
            gn_real z = r->bh.data[i];
            for (j = 0; j < r->in; j++) z += r->Wxh.data[(size_t)i * r->in + j] * GN_AT(*x, t, j);
            for (j = 0; j < r->hidden; j++) z += r->Whh.data[(size_t)i * r->hidden + j] * GN_AT(r->H, t, j);
            GN_AT(r->H, t + 1, i) = tanh(z);
        }
        for (k = 0; k < r->out; k++) {
            gn_real o = r->by.data[k];
            for (j = 0; j < r->hidden; j++) o += r->Why.data[(size_t)k * r->hidden + j] * GN_AT(r->H, t + 1, j);
            GN_AT(*y, t, k) = o;
        }
    }
    return GN_OK;
}

gn_status gn_rnn_backward(gn_rnn *r, const gn_mat *grad_y)
{
    int t, i, j, k;
    gn_real *dh_next, *dh, *dz;
    if (!r || !grad_y || grad_y->rows != r->T || grad_y->cols != r->out) return GN_ERR_ARG;
    dh_next = (gn_real *)calloc((size_t)r->hidden, sizeof(gn_real));
    dh = (gn_real *)malloc((size_t)r->hidden * sizeof(gn_real));
    dz = (gn_real *)malloc((size_t)r->hidden * sizeof(gn_real));
    if (!dh_next || !dh || !dz) { free(dh_next); free(dh); free(dz); return GN_ERR_NO_MEM; }
    for (t = r->T - 1; t >= 0; t--) {
        /* Why, by */
        for (k = 0; k < r->out; k++) {
            gn_real dy = GN_AT(*grad_y, t, k);
            r->gby.data[k] += dy;
            for (j = 0; j < r->hidden; j++)
                r->gWhy.data[(size_t)k * r->hidden + j] += dy * GN_AT(r->H, t + 1, j);
        }
        /* dh = Why^T dy + dh_next */
        for (j = 0; j < r->hidden; j++) {
            gn_real s = dh_next[j];
            for (k = 0; k < r->out; k++) s += r->Why.data[(size_t)k * r->hidden + j] * GN_AT(*grad_y, t, k);
            dh[j] = s;
        }
        /* dz = dh * (1 - h^2) */
        for (j = 0; j < r->hidden; j++) {
            gn_real h = GN_AT(r->H, t + 1, j);
            dz[j] = dh[j] * (1.0 - h * h);
        }
        /* gWxh, gWhh, gbh */
        for (i = 0; i < r->hidden; i++) {
            r->gbh.data[i] += dz[i];
            for (j = 0; j < r->in; j++)
                r->gWxh.data[(size_t)i * r->in + j] += dz[i] * GN_AT(r->X, t, j);
            for (j = 0; j < r->hidden; j++)
                r->gWhh.data[(size_t)i * r->hidden + j] += dz[i] * GN_AT(r->H, t, j);
        }
        /* dh_next = Whh^T dz */
        for (j = 0; j < r->hidden; j++) {
            gn_real s = 0.0;
            for (i = 0; i < r->hidden; i++) s += r->Whh.data[(size_t)i * r->hidden + j] * dz[i];
            dh_next[j] = s;
        }
    }
    free(dh_next); free(dh); free(dz);
    return GN_OK;
}

gn_status gn_rnn_zero_grad(gn_rnn *r)
{
    if (!r) return GN_ERR_ARG;
    gn_mat_zero(&r->gWxh); gn_mat_zero(&r->gWhh); gn_mat_zero(&r->gWhy);
    gn_mat_zero(&r->gbh); gn_mat_zero(&r->gby);
    return GN_OK;
}
