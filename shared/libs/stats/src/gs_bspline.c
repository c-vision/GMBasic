/* gs_bspline.c — base di B-spline (Cox-de Boor; da et_al.). */
#include "gs_bspline.h"

#include <stdlib.h>
#include <string.h>

int gs_bspline_nbasis(int nknots, int degree)
{
    return nknots - degree - 1;
}

/* span k tale che knots[k] <= x < knots[k+1], con clamp all'ultimo span. */
static int find_span(int nknots, const gc_real *knots, int degree, gc_real x)
{
    int lo = degree, hi = nknots - degree - 1, mid;
    if (x >= knots[nknots - 1]) return nknots - degree - 2;
    if (x <= knots[degree]) return degree;
    while (lo < hi) {
        mid = (lo + hi) / 2;
        if (x < knots[mid] || x >= knots[mid + 1]) {
            if (x < knots[mid]) hi = mid; else lo = mid + 1;
        } else return mid;
    }
    return lo;
}

int gs_bspline_basis(int nknots, const gc_real *knots, int degree, gc_real x, gc_real *out)
{
    int nb, span, j, r, base;
    gc_real *N, *left, *right, saved, temp;
    if (nknots <= degree + 1 || !knots || !out || degree < 0) return -1;
    nb = nknots - degree - 1;
    N = (gc_real *)calloc((size_t)(degree + 1), sizeof(gc_real));
    left = (gc_real *)calloc((size_t)(degree + 1), sizeof(gc_real));
    right = (gc_real *)calloc((size_t)(degree + 1), sizeof(gc_real));
    if (!N || !left || !right) { free(N); free(left); free(right); return -1; }
    span = find_span(nknots, knots, degree, x);
    N[0] = 1.0;
    for (j = 1; j <= degree; j++) {
        left[j] = x - knots[span + 1 - j];
        right[j] = knots[span + j] - x;
        saved = 0.0;
        for (r = 0; r < j; r++) {
            temp = N[r] / (right[r + 1] + left[j - r]);
            N[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        N[j] = saved;
    }
    memset(out, 0, (size_t)nb * sizeof(gc_real));
    base = span - degree;
    for (j = 0; j <= degree; j++)
        if (base + j >= 0 && base + j < nb) out[base + j] = N[j];
    free(N); free(left); free(right);
    return 0;
}

int gs_bspline_design(int nknots, const gc_real *knots, int degree,
                      int nx, const gc_real *x, gc_real *out)
{
    int i, nb = nknots - degree - 1;
    if (nb <= 0 || !x || !out) return -1;
    for (i = 0; i < nx; i++)
        if (gs_bspline_basis(nknots, knots, degree, x[i], out + (size_t)i * nb) != 0) return -1;
    return 0;
}

int gs_bspline_knots_uniform(int n_internal, int degree, gc_real xmin,
                             gc_real xmax, gc_real *out)
{
    int nknots = 2 * (degree + 1) + n_internal, i, j = 0;
    gc_real step;
    if (!out || n_internal < 0 || degree < 0) return -1;
    for (i = 0; i <= degree; i++) out[j++] = xmin;
    step = (n_internal > 0) ? (xmax - xmin) / (gc_real)(n_internal + 1) : 0.0;
    for (i = 1; i <= n_internal; i++) out[j++] = xmin + step * (gc_real)i;
    for (i = 0; i <= degree; i++) out[j++] = xmax;
    return nknots;
}
