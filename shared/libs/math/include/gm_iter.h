/*
 * gm_iter.h — solver iterativi per sistemi lineari (riscrittura).
 *
 * Metodi di rilassamento (Jacobi/Gauss-Seidel/SOR/Richardson), Krylov
 * (steepest descent, CG, GMRES, BiCGSTAB), Lanczos/Arnoldi.
 */
#ifndef GMMATH_GM_ITER_H
#define GMMATH_GM_ITER_H

#include "gm_core.h"
#include "gc_sparse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A (n×n row-major) x = b. Ritorna GM_OK se converge, GM_ERR_NOCONV altrimenti.
 * *iters = iterazioni eseguite. */
gm_status gm_iter_jacobi(int n, const gm_real *A, const gm_real *b, gm_real *x,
                         int maxit, gm_real tol, int *iters);
gm_status gm_iter_gauss_seidel(int n, const gm_real *A, const gm_real *b, gm_real *x,
                               int maxit, gm_real tol, int *iters);
gm_status gm_iter_sor(int n, const gm_real *A, const gm_real *b, gm_real *x,
                      gm_real omega, int maxit, gm_real tol, int *iters);
gm_status gm_iter_richardson(int n, const gm_real *A, const gm_real *b, gm_real *x,
                             gm_real omega, int maxit, gm_real tol, int *iters);
gm_status gm_iter_steepest_descent(int n, const gm_real *A, const gm_real *b, gm_real *x,
                                   int maxit, gm_real tol, int *iters);
/* Gradiente coniugato (A simmetrica definita positiva). */
gm_status gm_iter_cg(int n, const gm_real *A, const gm_real *b, gm_real *x,
                     int maxit, gm_real tol, int *iters);
/* Gradiente coniugato PRECONDIZIONATO (da LinearSystemsToolkit).
 * prec: 0=nessuno, 1=Jacobi (diagonale), 2=SSOR (omega in (0,2)). */
gm_status gm_iter_pcg(int n, const gm_real *A, const gm_real *b, gm_real *x,
                      int prec, gm_real omega, int maxit, gm_real tol, int *iters);
gm_status gm_iter_gmres(int n, const gm_real *A, const gm_real *b, gm_real *x,
                        int restart, int maxit, gm_real tol, int *iters);
gm_status gm_iter_bicgstab(int n, const gm_real *A, const gm_real *b, gm_real *x,
                           int maxit, gm_real tol, int *iters);

/* Varianti sparse (CSR). */
gm_status gm_iter_cg_sparse(const gc_csr *A, const gm_real *b, gm_real *x,
                            int maxit, gm_real tol, int *iters);
gm_status gm_iter_bicgstab_sparse(const gc_csr *A, const gm_real *b, gm_real *x,
                                  int maxit, gm_real tol, int *iters);

/* CGS (Conjugate Gradient Squared). */
gm_status gm_iter_cgs(int n, const gm_real *A, const gm_real *b, gm_real *x,
                      int maxit, gm_real tol, int *iters);
/* QMR (Quasi-Minimal Residual, Lanczos biorthogonal). */
gm_status gm_iter_qmr(int n, const gm_real *A, const gm_real *b, gm_real *x,
                      int maxit, gm_real tol, int *iters);
/* CR (Conjugate Residual, A simmetrica anche indefinita). */
gm_status gm_iter_cr(int n, const gm_real *A, const gm_real *b, gm_real *x,
                     int maxit, gm_real tol, int *iters);
/* Iterazione di Chebyshev (autovalori in [lmin,lmax]). */
gm_status gm_iter_chebyshev(int n, const gm_real *A, const gm_real *b, gm_real *x,
                            gm_real lmin, gm_real lmax, int maxit, gm_real tol, int *iters);
/* Omega ottimale per SOR (stima del raggio spettrale di Jacobi). */
gm_real gm_iter_optimal_omega(int n, const gm_real *A);

/* Fattorizzazione di Krylov: A (n×n) -> V (n×(m+1)) ortonormale, H (m+1×m) di Hessenberg. */
gm_status gm_arnoldi(int n, const gm_real *A, int m, gm_real *V, gm_real *H);
/* Lanczos (A simmetrica): V (n×(m+1)), alpha/beta (m). */
gm_status gm_lanczos(int n, const gm_real *A, int m, gm_real *V, gm_real *alpha, gm_real *beta);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_ITER_H */
