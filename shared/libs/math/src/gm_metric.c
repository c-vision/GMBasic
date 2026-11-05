/*
 * gm_metric.c — metriche tra vettori (da scipy.spatial.distance).
 */
#include "gm_metric.h"
#include <math.h>

gm_real gm_metric_euclidean(const gm_real *a, const gm_real *b, int n)
{
    gm_real s = 0.0; int i;
    for (i = 0; i < n; i++) { gm_real d = a[i] - b[i]; s += d * d; }
    return sqrt(s);
}
gm_real gm_metric_manhattan(const gm_real *a, const gm_real *b, int n)
{
    gm_real s = 0.0; int i;
    for (i = 0; i < n; i++) s += fabs(a[i] - b[i]);
    return s;
}
gm_real gm_metric_chebyshev(const gm_real *a, const gm_real *b, int n)
{
    gm_real m = 0.0; int i;
    for (i = 0; i < n; i++) { gm_real d = fabs(a[i] - b[i]); if (d > m) m = d; }
    return m;
}
gm_real gm_metric_minkowski(const gm_real *a, const gm_real *b, int n, gm_real p)
{
    gm_real s = 0.0; int i;
    if (p <= 0.0) return 0.0;
    for (i = 0; i < n; i++) s += pow(fabs(a[i] - b[i]), p);
    return pow(s, 1.0 / p);
}
gm_real gm_metric_cosine(const gm_real *a, const gm_real *b, int n)
{
    gm_real dot = 0.0, na = 0.0, nb = 0.0; int i;
    for (i = 0; i < n; i++) { dot += a[i] * b[i]; na += a[i] * a[i]; nb += b[i] * b[i]; }
    if (na <= 0 || nb <= 0) return 0.0;
    return 1.0 - dot / (sqrt(na) * sqrt(nb));
}
static gm_real dist_p(const gm_real *a, const gm_real *b, int d, gm_real p)
{
    if (p == 1.0) return gm_metric_manhattan(a, b, d);
    if (p == 2.0) return gm_metric_euclidean(a, b, d);
    return gm_metric_minkowski(a, b, d, p);
}
gm_status gm_metric_cdist(const gm_real *a, int m, const gm_real *b, int n, int d,
                          gm_real p, gm_real *out)
{
    int i, j;
    if (!a || !b || !out || m < 1 || n < 1 || d < 1) return GM_ERR_ARG;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++)
            out[(size_t)i * n + j] = dist_p(a + (size_t)i * d, b + (size_t)j * d, d, p);
    return GM_OK;
}
gm_status gm_metric_pdist(const gm_real *a, int m, int d, gm_real p, gm_real *out)
{
    int i, j, k = 0;
    if (!a || !out || m < 1 || d < 1) return GM_ERR_ARG;
    for (i = 0; i < m; i++)
        for (j = i + 1; j < m; j++)
            out[k++] = dist_p(a + (size_t)i * d, a + (size_t)j * d, d, p);
    return GM_OK;
}
