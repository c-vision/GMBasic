/*
 * gn_snn.c — utilità SNN (da izhikevich/SNN).
 */
#include "gn_snn.h"
#include <math.h>

gn_real gn_snn_surrogate(gn_real x)
{
    return 0.5 / (1.0 + fabs(x));
}

gn_real gn_snn_surrogate_grad(gn_real x)
{
    gn_real sg = (x > 0.0) ? 1.0 : -1.0;   /* sign(0) = -1 come nell'originale */
    gn_real d = 1.0 + fabs(x);
    return -0.5 * sg / (d * d);
}

int gn_snn_lut_index(const gn_real *x, const int *a, const int *b, int n_c)
{
    int idx = 0, r;
    if (!x || !a || !b || n_c < 0 || n_c > 30) return 0;
    for (r = 0; r < n_c; r++)
        if (x[a[r]] - x[b[r]] > 0.0) idx |= (1 << r);
    return idx;
}

void gn_snn_lut_forward(const gn_real *const *S, const int *j, int n_t, int y_dim, gn_real *y)
{
    int i, k;
    if (!S || !j || !y || n_t < 1 || y_dim < 1) return;
    for (i = 0; i < n_t; i++) {
        const gn_real *tab = S[i] + (size_t)j[i] * y_dim;
        for (k = 0; k < y_dim; k++) y[k] += tab[k];
    }
}
