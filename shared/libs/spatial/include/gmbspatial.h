/* gmbspatial.h — API della lib `spatial` (shared/libs/spatial).
 *
 * scipy.spatial.distance (pdist, cdist, distance_matrix) e ConvexHull.
 * Self-contained: solo <math.h>/<stdlib.h>/<string.h>.
 * Le matrici sono flat row-major; out e' un buffer del chiamante.
 */
#ifndef GMBSPATIAL_H
#define GMBSPATIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* PDIST: distanze a coppie condensate (triangolo superiore i<j).
 * out deve avere capienza n*(n-1)/2. Ritorna m (numero di elementi) o -1. */
int ofx_sp_pdist(const double* X, int n, int d, const char* metric, double* out, int out_cap);

/* CDIST: matrice nx*ny (row-major). out deve avere capienza nx*ny.
 * Ritorna 1 se ok, 0 su errore. */
int ofx_sp_cdist(const double* X, int nx, int dx, const double* Y, int ny, int dy,
                 const char* metric, double* out, int out_cap);

/* DISTANCEMATRIX: come CDIST ma sempre euclidea. */
int ofx_sp_distance_matrix(const double* X, int nx, int dx, const double* Y, int ny, int dy,
                           double* out, int out_cap);

/* CONVEXHULL (2D, stride = colonne del flat): indici dei vertici in ordine.
 * out deve avere capienza n. Ritorna k (numero di vertici) o -1. */
int ofx_sp_convex_hull(const double* X, int n, int stride, double* out, int out_cap);

#ifdef __cplusplus
}
#endif

#endif /* GMBSPATIAL_H */
