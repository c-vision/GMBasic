/*
 * gm_fuzzy.h — insiemi e logica fuzzy (riscrittura).
 */
#ifndef GMMATH_GM_FUZZY_H
#define GMMATH_GM_FUZZY_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Numero fuzzy triangolare (lo, mid, hi). */
typedef struct gm_tfn { gm_real lo, mid, hi; } gm_tfn;

gm_tfn gm_tfn_make(gm_real lo, gm_real mid, gm_real hi);
gm_real gm_fuzzy_membership_tri(gm_tfn t, gm_real x);
gm_tfn gm_fuzzy_union(gm_tfn a, gm_tfn b);
gm_tfn gm_fuzzy_intersection(gm_tfn a, gm_tfn b);
/* Defuzzificazione su campioni (x, mu). */
gm_real gm_fuzzy_defuzz_centroid(const gm_real *x, const gm_real *mu, int n);
gm_real gm_fuzzy_defuzz_bisector(const gm_real *x, const gm_real *mu, int n);
gm_real gm_fuzzy_defuzz_mom(const gm_real *x, const gm_real *mu, int n);
/* Fuzzy C-means: data n×d row-major, c cluster, fuzzificatore m>1.
 * centers c×d, U c×n (righe = cluster). */
gm_status gm_fuzzy_fcm(const gm_real *data, int n, int d, int c, gm_real m,
                       gm_real *centers, gm_real *U, int maxit, gm_real tol);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_FUZZY_H */
