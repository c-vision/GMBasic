/*
 * gm_clinalg.h — algebra lineare complessa (riscrittura, da ccmath).
 *
 * LU complesso, solve/inversa, autodecomposizione hermitiana (via
 * realificazione + Jacobi reale), potenza, prodotto, norme.
 */
#ifndef GMMATH_GM_CLINALG_H
#define GMMATH_GM_CLINALG_H

#include "gm_core.h"
#include "gm_complex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A (n×n row-major) x = b, LU con pivoting. */
gm_status gm_cpx_solve(int n, const gm_cpx *A, const gm_cpx *b, gm_cpx *x);
gm_status gm_cpx_inverse(int n, const gm_cpx *A, gm_cpx *inv);

/* Prodotto hermitiano sum conj(a_i) b_i. */
gm_cpx gm_cpx_dot(const gm_cpx *a, const gm_cpx *b, int n);
gm_real gm_cpx_norm(const gm_cpx *a, int n);

/* C = A B (n×n row-major). */
void gm_cpx_matmul(int n, const gm_cpx *A, const gm_cpx *B, gm_cpx *C);
/* C = A^H (coniugata trasposta). */
void gm_cpx_conjtranspose(int n, const gm_cpx *A, gm_cpx *C);

/* A hermitiana (n×n): autovalori reali crescenti in eval[n], autovettori
 * (colonna k) in evec[n×n row-major]. */
gm_status gm_cpx_herm_eig(int n, const gm_cpx *A, gm_real *eval, gm_cpx *evec);
/* Autovalore massimo (metodo delle potenze) di A hermitiana; v = autovettore. */
gm_real gm_cpx_herm_max(int n, const gm_cpx *A, gm_cpx *v, int iters);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_CLINALG_H */
