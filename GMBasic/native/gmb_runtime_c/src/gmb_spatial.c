/* ============================================================================
 * gmb_spatial.c — scipy.spatial.distance (pdist, cdist, distance_matrix) e
 * ConvexHull. API flat: X è un array [n*d] (righe consecutive).
 * ============================================================================ */

#include "gmb_runtime.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double gmb_dist_metric(const double* a, const double* b, int d, const char* metric)
{
    if (metric && strcmp(metric, "cityblock") == 0)
    {
        double s = 0; for (int i = 0; i < d; i++) s += fabs(a[i] - b[i]); return s;
    }
    if (metric && strcmp(metric, "chebyshev") == 0)
    {
        double s = 0; for (int i = 0; i < d; i++) { double v = fabs(a[i] - b[i]); if (v > s) s = v; } return s;
    }
    if (metric && strcmp(metric, "cosine") == 0)
    {
        double ab = 0, aa = 0, bb = 0;
        for (int i = 0; i < d; i++) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
        double denom = sqrt(aa * bb);
        return denom > 0 ? 1.0 - ab / denom : 0;
    }
    double s = 0; for (int i = 0; i < d; i++) { double v = a[i] - b[i]; s += v * v; }
    return sqrt(s);
}

double* gmb_pdist_flat(const double* X, int n, int d, const char* metric, gmb_int_t* outn)
{
    int m = n * (n - 1) / 2;
    double* out = (double*)malloc(sizeof(double) * (size_t)(m > 0 ? m : 1));
    if (!out) { *outn = 0; return NULL; }
    int k = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            out[k++] = gmb_dist_metric(X + i * d, X + j * d, d, metric);
    *outn = m;
    return out;
}

double* gmb_cdist_flat(const double* X, int nx, int dx, const double* Y, int ny, int dy,
    const char* metric, int* out_rows, int* out_cols)
{
    *out_rows = nx; *out_cols = ny;
    if (nx <= 0 || ny <= 0) return NULL;
    double* out = (double*)malloc(sizeof(double) * (size_t)nx * (size_t)ny);
    if (!out) return NULL;
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < ny; j++)
            out[i * ny + j] = gmb_dist_metric(X + i * dx, Y + j * dy, dx < dy ? dx : dy, metric);
    return out;
}

double* gmb_distance_matrix_flat(const double* X, int nx, int dx, const double* Y, int ny, int dy,
    int* out_rows, int* out_cols)
{
    return gmb_cdist_flat(X, nx, dx, Y, ny, dy, "euclidean", out_rows, out_cols);
}

static double gmb_hull_cross(const double* flat, const int* idx, int o, int a, int b, int stride)
{
    double x1 = flat[idx[a] * stride] - flat[idx[o] * stride], y1 = flat[idx[a] * stride + 1] - flat[idx[o] * stride + 1];
    double x2 = flat[idx[b] * stride] - flat[idx[o] * stride], y2 = flat[idx[b] * stride + 1] - flat[idx[o] * stride + 1];
    return x1 * y2 - y1 * x2;
}

double* gmb_convex_hull_flat(const double* X, int n, int stride, gmb_int_t* outn)
{
    if (n < 3 || stride < 2) { *outn = 0; return NULL; }
    int* idx = (int*)malloc(sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
        {
            double ax = X[idx[j] * stride], ay = X[idx[j] * stride + 1];
            double bx = X[idx[i] * stride], by = X[idx[i] * stride + 1];
            if (ax < bx || (ax == bx && ay < by)) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        }
    int* lower = (int*)malloc(sizeof(int) * (size_t)(n + 1));
    int nl = 0;
    for (int i = 0; i < n; i++)
    {
        while (nl >= 2 && gmb_hull_cross(X, idx, lower[nl - 2], lower[nl - 1], i, stride) <= 0) nl--;
        lower[nl++] = i;
    }
    int* upper = (int*)malloc(sizeof(int) * (size_t)(n + 1));
    int nu = 0;
    for (int i = n - 1; i >= 0; i--)
    {
        while (nu >= 2 && gmb_hull_cross(X, idx, upper[nu - 2], upper[nu - 1], i, stride) <= 0) nu--;
        upper[nu++] = i;
    }
    nl--; nu--;
    double* out = (double*)malloc(sizeof(double) * (size_t)(nl + nu));
    if (!out) { free(idx); free(lower); free(upper); *outn = 0; return NULL; }
    for (int i = 0; i < nl; i++) out[i] = idx[lower[i]];
    for (int i = 0; i < nu; i++) out[nl + i] = idx[upper[i]];
    *outn = nl + nu;
    free(idx); free(lower); free(upper);
    return out;
}

/* ---- Bridge C99: input jagged (gmb_double_t**) e output alloc/matrice ---- */
static double* gmb_jagged_to_flat_c99(gmb_double_t** jag, int n, int d)
{
    double* flat = (double*)malloc(sizeof(double) * (size_t)n * (size_t)d);
    if (!flat) return NULL;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < d; j++)
            flat[i * d + j] = jag[i][j];
    return flat;
}

double* gmb_pdist_c99(gmb_double_t** X, int n, int d, const char* metric, gmb_int_t* outn)
{
    double* flat = gmb_jagged_to_flat_c99(X, n, d);
    if (!flat) { *outn = 0; return NULL; }
    double* r = gmb_pdist_flat(flat, n, d, metric, outn);
    free(flat);
    return r;
}

gmb_int_t gmb_cdist_c99(gmb_double_t** X, int nx, int dx, gmb_double_t** Y, int ny, int dy, const char* metric)
{
    double* fx = gmb_jagged_to_flat_c99(X, nx, dx);
    double* fy = gmb_jagged_to_flat_c99(Y, ny, dy);
    if (!fx || !fy) { free(fx); free(fy); return 0; }
    int rows = 0, cols = 0;
    double* m = gmb_cdist_flat(fx, nx, dx, fy, ny, dy, metric, &rows, &cols);
    free(fx); free(fy);
    if (!m || rows <= 0 || cols <= 0) { free(m); return 0; }
    gmb_int_t h = gmb_matrix_from_flat(m, rows, cols);
    free(m);
    return h;
}

gmb_int_t gmb_distance_matrix_c99(gmb_double_t** X, int nx, int dx, gmb_double_t** Y, int ny, int dy)
{
    return gmb_cdist_c99(X, nx, dx, Y, ny, dy, "euclidean");
}

double* gmb_convex_hull_c99(gmb_double_t** X, int n, gmb_int_t* outn)
{
    double* flat = gmb_jagged_to_flat_c99(X, n, 2);
    if (!flat) { *outn = 0; return NULL; }
    double* r = gmb_convex_hull_flat(flat, n, 2, outn);
    free(flat);
    return r;
}
