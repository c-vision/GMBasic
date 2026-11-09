/* ============================================================================
 * spatial.c — namespace SPATIAL (scipy.spatial.distance + ConvexHull).
 *
 * API a buffer (self-contained, nessuna dipendenza dal runtime):
 *   X, Y      : matrici flat row-major [righe * colonne]
 *   out       : buffer fornito dal chiamante (cap in elementi)
 * I wrapper BASIC in `shared/include/spatial.inc` appiattiscono gli array
 * jagged e ricostruiscono la MATRIX (per CDIST/DISTANCEMATRIX) con MATRIX.
 * ============================================================================ */
#include "gmbspatial.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Metrica standard. p e' fissato a 2 (minkowski == euclidean), come il
 * contratto storico della superficie BASIC (nessun parametro p). */
static double sp_metric(const double* a, const double* b, int d, const char* metric)
{
    if (metric && (strcmp(metric, "cityblock") == 0 || strcmp(metric, "manhattan") == 0))
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

int ofx_sp_pdist(const double* X, int n, int d, const char* metric, double* out, int out_cap)
{
    if (!X || !out || n < 1 || d < 1) return -1;
    int m = n * (n - 1) / 2;
    if (m > out_cap) return -1;
    int k = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            out[k++] = sp_metric(X + i * d, X + j * d, d, metric);
    return m;
}

int ofx_sp_cdist(const double* X, int nx, int dx, const double* Y, int ny, int dy,
                 const char* metric, double* out, int out_cap)
{
    if (!X || !Y || !out || nx < 1 || ny < 1 || dx < 1 || dy < 1) return 0;
    if (nx * ny > out_cap) return 0;
    int d = dx < dy ? dx : dy;
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < ny; j++)
            out[i * ny + j] = sp_metric(X + i * dx, Y + j * dy, d, metric);
    return 1;
}

int ofx_sp_distance_matrix(const double* X, int nx, int dx, const double* Y, int ny, int dy,
                           double* out, int out_cap)
{
    return ofx_sp_cdist(X, nx, dx, Y, ny, dy, "euclidean", out, out_cap);
}

static double sp_hull_cross(const double* flat, const int* idx, int o, int a, int b, int stride)
{
    double x1 = flat[idx[a] * stride] - flat[idx[o] * stride], y1 = flat[idx[a] * stride + 1] - flat[idx[o] * stride + 1];
    double x2 = flat[idx[b] * stride] - flat[idx[o] * stride], y2 = flat[idx[b] * stride + 1] - flat[idx[o] * stride + 1];
    return x1 * y2 - y1 * x2;
}

int ofx_sp_convex_hull(const double* X, int n, int stride, double* out, int out_cap)
{
    if (!X || !out || n < 3 || stride < 2) return 0;
    if (n > out_cap) return -1;
    int* idx = (int*)malloc(sizeof(int) * (size_t)n);
    if (!idx) return -1;
    for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
        {
            double ax = X[idx[j] * stride], ay = X[idx[j] * stride + 1];
            double bx = X[idx[i] * stride], by = X[idx[i] * stride + 1];
            if (ax < bx || (ax == bx && ay < by)) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        }
    int* lower = (int*)malloc(sizeof(int) * (size_t)(n + 1));
    int* upper = (int*)malloc(sizeof(int) * (size_t)(n + 1));
    if (!lower || !upper) { free(idx); free(lower); free(upper); return -1; }
    int nl = 0;
    for (int i = 0; i < n; i++)
    {
        while (nl >= 2 && sp_hull_cross(X, idx, lower[nl - 2], lower[nl - 1], i, stride) <= 0) nl--;
        lower[nl++] = i;
    }
    int nu = 0;
    for (int i = n - 1; i >= 0; i--)
    {
        while (nu >= 2 && sp_hull_cross(X, idx, upper[nu - 2], upper[nu - 1], i, stride) <= 0) nu--;
        upper[nu++] = i;
    }
    nl--; nu--;
    int k = 0;
    for (int i = 0; i < nl; i++) out[k++] = idx[lower[i]];
    for (int i = 0; i < nu; i++) out[k++] = idx[upper[i]];
    free(idx); free(lower); free(upper);
    return k;
}
