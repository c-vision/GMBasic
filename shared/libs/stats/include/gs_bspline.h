/*
 * gs_bspline.h — base di B-spline (da et_al. basis/spline.h).
 *
 * Funzioni base N_{i,p}(x) via ricorrenza di Cox-de Boor (algoritmo A2.2,
 * The NURBS Book) e matrice di disegno (design matrix) su un vettore di nodi.
 */
#ifndef GMBSTATS_GS_BSPLINE_H
#define GMBSTATS_GS_BSPLINE_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Numero di funzioni base: nknots - degree - 1. */
int gs_bspline_nbasis(int nknots, int degree);

/* Valuta le nbasis funzioni base di grado `degree` sulla griglia di nodi
 * `knots` (non decrescenti) nel punto x. out ha gs_bspline_nbasis(...) valori. */
int gs_bspline_basis(int nknots, const gc_real *knots, int degree, gc_real x, gc_real *out);

/* Matrice di disegno nx × nbasis (row-major) per i punti x[0..nx-1]. */
int gs_bspline_design(int nknots, const gc_real *knots, int degree,
                      int nx, const gc_real *x, gc_real *out);

/* Vettore di nodi uniforme "clamped" su [xmin, xmax] con n_internal nodi
 * interni equispaziati e grado `degree`. out ha 2*(degree+1) + n_internal
 * valori. */
int gs_bspline_knots_uniform(int n_internal, int degree, gc_real xmin,
                             gc_real xmax, gc_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_BSPLINE_H */
