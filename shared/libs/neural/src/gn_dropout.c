/*
 * gn_dropout.c — dropout (da Deyht/CIANNA).
 */
#include "gn_dropout.h"
#include <stdlib.h>

gn_status gn_dropout_init(gn_dropout *d, int n, gn_real rate)
{
    if (!d || n < 1) return GN_ERR_ARG;
    if (rate < 0.0) rate = 0.0;
    if (rate >= 1.0) rate = 0.999999;
    d->rate = rate;
    d->n = n;
    d->mask = (unsigned char *)calloc((size_t)n, 1);
    if (!d->mask) return GN_ERR_NO_MEM;
    { int i; for (i = 0; i < n; i++) d->mask[i] = 1; }
    return GN_OK;
}

void gn_dropout_free(gn_dropout *d)
{
    if (!d) return;
    free(d->mask);
    d->mask = NULL;
    d->n = 0;
}

gn_status gn_dropout_forward(gn_dropout *d, const gn_real *x, gn_real *out,
                             int train, unsigned long long seed)
{
    int i;
    unsigned long long s = seed ? seed : 88172645463325252ULL;
    if (!d || !x || !out) return GN_ERR_ARG;
    if (!train || d->rate <= 0.0) {
        for (i = 0; i < d->n; i++) { out[i] = x[i]; d->mask[i] = 1; }
        return GN_OK;
    }
    {
        gn_real keep = 1.0 - d->rate;
        gn_real scale = 1.0 / keep;
        for (i = 0; i < d->n; i++) {
            s = s * 6364136223846793005ULL + 1442695040888963407ULL;
            if ((gn_real)(s >> 11) / 9007199254740992.0 >= d->rate) {
                d->mask[i] = 1;
                out[i] = x[i] * scale;
            } else {
                d->mask[i] = 0;
                out[i] = 0.0;
            }
        }
    }
    return GN_OK;
}

gn_status gn_dropout_backward(const gn_dropout *d, const gn_real *grad_out, gn_real *grad_x)
{
    int i;
    gn_real scale;
    if (!d || !grad_out || !grad_x) return GN_ERR_ARG;
    scale = (d->rate <= 0.0) ? 1.0 : 1.0 / (1.0 - d->rate);
    for (i = 0; i < d->n; i++) grad_x[i] = d->mask[i] ? grad_out[i] * scale : 0.0;
    return GN_OK;
}
