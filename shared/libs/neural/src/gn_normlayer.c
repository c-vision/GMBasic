/*
 * gn_normlayer.c — BatchNorm e LayerNorm (da attractivechaos/kann, MIT).
 */
#include "gn_normlayer.h"
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

/* ===================== BatchNorm ===================== */

gn_status gn_batchnorm_init(gn_batchnorm *bn, int features, gn_real eps, gn_real momentum)
{
    int i;
    if (!bn || features < 1) return GN_ERR_ARG;
    memset(bn, 0, sizeof(*bn));
    bn->features = features;
    bn->eps = eps > 0 ? eps : 1e-5;
    bn->momentum = momentum > 0 ? momentum : 0.1;
    bn->train = 1;
    if (gn_mat_alloc(&bn->gamma, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&bn->beta, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&bn->ggamma, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&bn->gbeta, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&bn->rmean, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&bn->rvar, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    for (i = 0; i < features; i++) { bn->gamma.data[i] = 1.0; bn->rvar.data[i] = 1.0; }
    return GN_OK;
}

void gn_batchnorm_free(gn_batchnorm *bn)
{
    if (!bn) return;
    gn_mat_free(&bn->gamma); gn_mat_free(&bn->beta);
    gn_mat_free(&bn->ggamma); gn_mat_free(&bn->gbeta);
    gn_mat_free(&bn->rmean); gn_mat_free(&bn->rvar);
    gn_mat_free(&bn->xhat); gn_mat_free(&bn->mean); gn_mat_free(&bn->istd);
    memset(bn, 0, sizeof(*bn));
}

gn_status gn_batchnorm_zero_grad(gn_batchnorm *bn)
{
    gn_mat_zero(&bn->ggamma); gn_mat_zero(&bn->gbeta);
    return GN_OK;
}

gn_status gn_batchnorm_forward(gn_batchnorm *bn, const gn_mat *x, gn_mat *y, int train)
{
    int B = x->rows, F = x->cols, i, j;
    if (!bn || !x || !y || F != bn->features || y->rows != B || y->cols != F) return GN_ERR_ARG;
    bn->B = B; bn->train = train;
    gn_mat_free(&bn->xhat); gn_mat_free(&bn->mean); gn_mat_free(&bn->istd);
    if (gn_mat_alloc(&bn->xhat, B, F) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&bn->mean, 1, F) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&bn->istd, 1, F) != GN_OK) return GN_ERR_NO_MEM;
    for (j = 0; j < F; j++) {
        gn_real mean = 0.0, var = 0.0, istd;
        if (train) {
            for (i = 0; i < B; i++) mean += x->data[(size_t)i * F + j];
            mean /= (gn_real)B;
            for (i = 0; i < B; i++) { gn_real d = x->data[(size_t)i * F + j] - mean; var += d * d; }
            var /= (gn_real)B;
            bn->rmean.data[j] = (1.0 - bn->momentum) * bn->rmean.data[j] + bn->momentum * mean;
            bn->rvar.data[j] = (1.0 - bn->momentum) * bn->rvar.data[j] + bn->momentum * var;
        } else { mean = bn->rmean.data[j]; var = bn->rvar.data[j]; }
        istd = 1.0 / sqrt(var + bn->eps);
        bn->mean.data[j] = mean;
        bn->istd.data[j] = istd;
        for (i = 0; i < B; i++) {
            gn_real xh = (x->data[(size_t)i * F + j] - mean) * istd;
            bn->xhat.data[(size_t)i * F + j] = xh;
            y->data[(size_t)i * F + j] = bn->gamma.data[j] * xh + bn->beta.data[j];
        }
    }
    return GN_OK;
}

gn_status gn_batchnorm_backward(gn_batchnorm *bn, const gn_mat *grad_y, gn_mat *grad_x)
{
    int B = bn->B, F = bn->features, i, j;
    gn_real *s1, *s2;
    if (!bn || !grad_y || grad_y->rows != B || grad_y->cols != F) return GN_ERR_ARG;
    s1 = (gn_real *)calloc((size_t)F, sizeof(gn_real));   /* somma dxhat */
    s2 = (gn_real *)calloc((size_t)F, sizeof(gn_real));   /* somma dxhat*xhat */
    if (!s1 || !s2) { free(s1); free(s2); return GN_ERR_NO_MEM; }
    for (j = 0; j < F; j++) {
        gn_real dg = 0.0, db = 0.0;
        for (i = 0; i < B; i++) {
            gn_real gy = grad_y->data[(size_t)i * F + j];
            gn_real dxh = gy * bn->gamma.data[j];
            dg += gy * bn->xhat.data[(size_t)i * F + j];
            db += gy;
            s1[j] += dxh;
            s2[j] += dxh * bn->xhat.data[(size_t)i * F + j];
        }
        bn->ggamma.data[j] += dg;
        bn->gbeta.data[j] += db;
    }
    if (grad_x) {
        for (j = 0; j < F; j++) {
            gn_real inv = bn->istd.data[j] / (gn_real)B;
            for (i = 0; i < B; i++) {
                gn_real dxh = grad_y->data[(size_t)i * F + j] * bn->gamma.data[j];
                grad_x->data[(size_t)i * F + j] = inv * ((gn_real)B * dxh - s1[j]
                    - bn->xhat.data[(size_t)i * F + j] * s2[j]);
            }
        }
    }
    free(s1); free(s2);
    return GN_OK;
}

/* ===================== LayerNorm ===================== */

gn_status gn_layernorm_init(gn_layernorm *ln, int features, gn_real eps)
{
    int i;
    if (!ln || features < 1) return GN_ERR_ARG;
    memset(ln, 0, sizeof(*ln));
    ln->features = features;
    ln->eps = eps > 0 ? eps : 1e-5;
    if (gn_mat_alloc(&ln->gamma, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&ln->beta, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&ln->ggamma, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&ln->gbeta, 1, features) != GN_OK) return GN_ERR_NO_MEM;
    for (i = 0; i < features; i++) ln->gamma.data[i] = 1.0;
    return GN_OK;
}

void gn_layernorm_free(gn_layernorm *ln)
{
    if (!ln) return;
    gn_mat_free(&ln->gamma); gn_mat_free(&ln->beta);
    gn_mat_free(&ln->ggamma); gn_mat_free(&ln->gbeta);
    gn_mat_free(&ln->xhat); gn_mat_free(&ln->mean); gn_mat_free(&ln->istd);
    memset(ln, 0, sizeof(*ln));
}

gn_status gn_layernorm_zero_grad(gn_layernorm *ln)
{
    gn_mat_zero(&ln->ggamma); gn_mat_zero(&ln->gbeta);
    return GN_OK;
}

gn_status gn_layernorm_forward(gn_layernorm *ln, const gn_mat *x, gn_mat *y)
{
    int B = x->rows, F = x->cols, i, j;
    if (!ln || !x || !y || F != ln->features || y->rows != B || y->cols != F) return GN_ERR_ARG;
    ln->B = B;
    gn_mat_free(&ln->xhat); gn_mat_free(&ln->mean); gn_mat_free(&ln->istd);
    if (gn_mat_alloc(&ln->xhat, B, F) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&ln->mean, B, 1) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&ln->istd, B, 1) != GN_OK) return GN_ERR_NO_MEM;
    for (i = 0; i < B; i++) {
        gn_real mean = 0.0, var = 0.0, istd;
        for (j = 0; j < F; j++) mean += x->data[(size_t)i * F + j];
        mean /= (gn_real)F;
        for (j = 0; j < F; j++) { gn_real d = x->data[(size_t)i * F + j] - mean; var += d * d; }
        var /= (gn_real)F;
        istd = 1.0 / sqrt(var + ln->eps);
        ln->mean.data[i] = mean;
        ln->istd.data[i] = istd;
        for (j = 0; j < F; j++) {
            gn_real xh = (x->data[(size_t)i * F + j] - mean) * istd;
            ln->xhat.data[(size_t)i * F + j] = xh;
            y->data[(size_t)i * F + j] = ln->gamma.data[j] * xh + ln->beta.data[j];
        }
    }
    return GN_OK;
}

gn_status gn_layernorm_backward(gn_layernorm *ln, const gn_mat *grad_y, gn_mat *grad_x)
{
    int B = ln->B, F = ln->features, i, j;
    if (!ln || !grad_y || grad_y->rows != B || grad_y->cols != F) return GN_ERR_ARG;
    for (j = 0; j < F; j++) {
        gn_real dg = 0.0, db = 0.0;
        for (i = 0; i < B; i++) {
            dg += grad_y->data[(size_t)i * F + j] * ln->xhat.data[(size_t)i * F + j];
            db += grad_y->data[(size_t)i * F + j];
        }
        ln->ggamma.data[j] += dg;
        ln->gbeta.data[j] += db;
    }
    if (grad_x) {
        for (i = 0; i < B; i++) {
            gn_real mean = ln->mean.data[i], istd = ln->istd.data[i];
            gn_real s1 = 0.0, s2 = 0.0;
            for (j = 0; j < F; j++) {
                gn_real dxh = grad_y->data[(size_t)i * F + j] * ln->gamma.data[j];
                s1 += dxh;
                s2 += dxh * ln->xhat.data[(size_t)i * F + j];
            }
            for (j = 0; j < F; j++) {
                gn_real dxh = grad_y->data[(size_t)i * F + j] * ln->gamma.data[j];
                grad_x->data[(size_t)i * F + j] = istd / (gn_real)F
                    * ((gn_real)F * dxh - s1 - ln->xhat.data[(size_t)i * F + j] * s2);
            }
            (void)mean;
        }
    }
    return GN_OK;
}
