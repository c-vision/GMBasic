/*
 * gn_norm.c — normalizzazione online (da antirez/neural-redis).
 */
#include "gn_norm.h"
#include <stdlib.h>
#include <math.h>

gn_status gn_norm_init(gn_norm *n, int dim)
{
    int i;
    if (!n || dim < 1) return GN_ERR_ARG;
    n->dim = dim;
    n->count = 0;
    n->scale = (gn_real *)malloc((size_t)dim * sizeof(gn_real));
    n->mean = (gn_real *)calloc((size_t)dim, sizeof(gn_real));
    n->m2 = (gn_real *)calloc((size_t)dim, sizeof(gn_real));
    if (!n->scale || !n->mean || !n->m2) { gn_norm_free(n); return GN_ERR_NO_MEM; }
    for (i = 0; i < dim; i++) n->scale[i] = 1.0;
    return GN_OK;
}

void gn_norm_free(gn_norm *n)
{
    if (!n) return;
    free(n->scale); free(n->mean); free(n->m2);
    n->scale = n->mean = n->m2 = NULL;
    n->dim = 0; n->count = 0;
}

gn_status gn_norm_observe(gn_norm *n, const gn_real *x)
{
    int i;
    if (!n || !x || n->dim < 1) return GN_ERR_ARG;
    n->count++;
    for (i = 0; i < n->dim; i++) {
        gn_real v = fabs(x[i]);
        gn_real d, d2;
        if (v > n->scale[i]) n->scale[i] = v;      /* max-abs */
        d = x[i] - n->mean[i];                      /* Welford */
        n->mean[i] += d / (gn_real)n->count;
        d2 = x[i] - n->mean[i];
        n->m2[i] += d * d2;
    }
    return GN_OK;
}

void gn_norm_finalize(gn_norm *n, gn_real margin)
{
    int i;
    if (!n || n->dim < 1) return;
    if (margin <= 0.0) margin = 1.0;
    for (i = 0; i < n->dim; i++) if (n->scale[i] != 1.0) n->scale[i] *= margin;
}

gn_status gn_norm_apply(const gn_norm *n, const gn_real *x, gn_real *out)
{
    int i;
    if (!n || !x || !out || n->dim < 1) return GN_ERR_ARG;
    for (i = 0; i < n->dim; i++) out[i] = x[i] / (n->scale[i] != 0.0 ? n->scale[i] : 1.0);
    return GN_OK;
}

gn_status gn_norm_denormalize(const gn_norm *n, const gn_real *x, gn_real *out)
{
    int i;
    if (!n || !x || !out || n->dim < 1) return GN_ERR_ARG;
    for (i = 0; i < n->dim; i++) out[i] = x[i] * n->scale[i];
    return GN_OK;
}

gn_status gn_norm_zscore(const gn_norm *n, const gn_real *x, gn_real *out)
{
    int i;
    if (!n || !x || !out || n->dim < 1) return GN_ERR_ARG;
    for (i = 0; i < n->dim; i++) {
        gn_real var = (n->count > 1) ? n->m2[i] / (gn_real)(n->count - 1) : 0.0;
        gn_real sd = sqrt(var);
        out[i] = (sd > 1e-12) ? (x[i] - n->mean[i]) / sd : 0.0;
    }
    return GN_OK;
}
