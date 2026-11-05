/*
 * gm_metric.h — metriche tra vettori (SciPy scipy.spatial.distance).
 *
 * Chebyshev, cosine, manhattan, minkowski, euclidea, cdist/pdist.
 */
#ifndef GMMATH_GM_METRIC_H
#define GMMATH_GM_METRIC_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Distanze tra vettori a[n], b[n]. */
gm_real gm_metric_euclidean(const gm_real *a, const gm_real *b, int n);
gm_real gm_metric_manhattan(const gm_real *a, const gm_real *b, int n);
gm_real gm_metric_chebyshev(const gm_real *a, const gm_real *b, int n);
gm_real gm_metric_minkowski(const gm_real *a, const gm_real *b, int n, gm_real p);
gm_real gm_metric_cosine(const gm_real *a, const gm_real *b, int n);

/* Matrici di distanze: points n×d. cdist out[m*n], pdist out[m*(m-1)/2]. */
gm_status gm_metric_cdist(const gm_real *a, int m, const gm_real *b, int n, int d,
                          gm_real p, gm_real *out);
gm_status gm_metric_pdist(const gm_real *a, int m, int d, gm_real p, gm_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_METRIC_H */
