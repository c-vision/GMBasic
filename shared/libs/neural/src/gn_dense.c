/* gn_dense.c — strato denso. */
#include "gn_dense.h"
#include "gc_gemm.h"

#include <math.h>
#include <stdlib.h>

/* RNG xorshift64 per l'inizializzazione. */
static unsigned long long rng_next(unsigned long long *s)
{
    *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17;
    return *s;
}
/* normale standard (Box-Muller). */
static gn_real rng_normal(unsigned long long *s)
{
    gn_real u1, u2;
    do { u1 = (gn_real)(rng_next(s) >> 11) / 9007199254740992.0; } while (u1 <= 1e-300);
    u2 = (gn_real)(rng_next(s) >> 11) / 9007199254740992.0;
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

gn_status gn_dense_init(gn_dense *l, int in, int out, unsigned long long seed)
{
    int i, n;
    gn_real scale;
    if (!l || in <= 0 || out <= 0) return GN_ERR_ARG;
    l->in = in; l->out = out;
    l->x.rows = l->x.cols = 0; l->x.data = NULL;
    if (gn_mat_alloc(&l->W, out, in) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&l->b, 1, out) != GN_OK) { gn_mat_free(&l->W); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&l->gradW, out, in) != GN_OK) { gn_mat_free(&l->W); gn_mat_free(&l->b); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&l->gradb, 1, out) != GN_OK) {
        gn_mat_free(&l->W); gn_mat_free(&l->b); gn_mat_free(&l->gradW); return GN_ERR_NO_MEM;
    }
    if (seed == 0) seed = 88172645463325252ULL;
    scale = sqrt(2.0 / (gn_real)in);   /* He / Glorot-like */
    n = out * in;
    for (i = 0; i < n; i++) l->W.data[i] = scale * rng_normal(&seed);
    return GN_OK;
}

void gn_dense_free(gn_dense *l)
{
    if (!l) return;
    gn_mat_free(&l->W); gn_mat_free(&l->b);
    gn_mat_free(&l->gradW); gn_mat_free(&l->gradb);
    gn_mat_free(&l->x);
}

gn_status gn_dense_forward(gn_dense *l, const gn_mat *x, gn_mat *y)
{
    int i, j, k;
    if (!l || !x || !y || x->cols != l->in || y->rows != x->rows || y->cols != l->out)
        return GN_ERR_ARG;
    gn_mat_free(&l->x);
    if (gn_mat_alloc(&l->x, x->rows, x->cols) != GN_OK) return GN_ERR_NO_MEM;
    gn_mat_copy(&l->x, x);
    gn_mat_mul_nt(y, x, &l->W);   /* y = x W^T */
    for (i = 0; i < y->rows; i++)
        for (j = 0; j < l->out; j++) GN_AT(*y, i, j) += l->b.data[j];
    (void)k;
    return GN_OK;
}

gn_status gn_dense_backward(gn_dense *l, const gn_mat *grad_y, gn_mat *grad_x)
{
    int i, j, k;
    if (!l || !grad_y || grad_y->rows != l->x.rows || grad_y->cols != l->out) return GN_ERR_ARG;
    /* gradW += grad_y^T * x  (out × in) */
    for (i = 0; i < l->out; i++)
        for (j = 0; j < l->in; j++) {
            gn_real s = 0.0;
            for (k = 0; k < grad_y->rows; k++) s += GN_AT(*grad_y, k, i) * GN_AT(l->x, k, j);
            l->gradW.data[(size_t)i * l->in + j] += s;
        }
    /* gradb += somma su batch */
    for (i = 0; i < grad_y->rows; i++)
        for (j = 0; j < l->out; j++) l->gradb.data[j] += GN_AT(*grad_y, i, j);
    /* grad_x = grad_y * W (batch × in) */
    if (grad_x) {
        if (grad_x->rows != grad_y->rows || grad_x->cols != l->in) return GN_ERR_ARG;
        for (i = 0; i < grad_y->rows; i++)
            for (j = 0; j < l->in; j++) {
                gn_real s = 0.0;
                for (k = 0; k < l->out; k++) s += GN_AT(*grad_y, i, k) * l->W.data[(size_t)k * l->in + j];
                GN_AT(*grad_x, i, j) = s;
            }
    }
    return GN_OK;
}

gn_status gn_dense_zero_grad(gn_dense *l)
{
    if (!l) return GN_ERR_ARG;
    gn_mat_zero(&l->gradW);
    gn_mat_zero(&l->gradb);
    return GN_OK;
}
