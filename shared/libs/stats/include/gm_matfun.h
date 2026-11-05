/*
 * gm_matfun.h — funzioni matriciali (da detectum).
 *
 * Esponenziale, logaritmo e radice quadrata di matrici (principali), e
 * pseudoinversa di Moore-Penrose. Algoritmi: scaling-and-squaring (exp),
 * Denman-Beavers (sqrt), inverse scaling-and-squaring (log), SVD (pinv).
 */
#ifndef GMMATH_GM_MATFUN_H
#define GMMATH_GM_MATFUN_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* E = exp(A), A (n×n row-major). */
gm_status gm_mat_exp(int n, const gm_real *A, gm_real *E);
/* S = A^{1/2} (radice principale). */
gm_status gm_mat_sqrt(int n, const gm_real *A, gm_real *S);
/* L = log(A) (logaritmo principale). */
gm_status gm_mat_log(int n, const gm_real *A, gm_real *L);
/* P = A^+ (Moore-Penrose), A (m×n row-major), P (n×m row-major). */
gm_status gm_mat_pinv(int m, int n, const gm_real *A, gm_real *P);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_MATFUN_H */
