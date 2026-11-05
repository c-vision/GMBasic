/*
 * gm_linalg.h — algebra lineare densa (SIMD).
 *
 * Row-major. LU con pivoting parziale, Cholesky, solve, inversa, determinante,
 * trasposta. I loop interni usano AXPY/dot vettorizzati (gc_array).
 */
#ifndef GMMATH_GM_LINALG_H
#define GMMATH_GM_LINALG_H

#include "gm_core.h"
#include "gc_gemm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LU in-place con pivoting parziale. `piv[k]` = riga scambiata con k.
 * Ritorna GM_OK o GM_ERR_SINGULAR. */
gm_status gm_linalg_lu(int n, gm_real *A, int *piv);
/* Risolve LU x = P b (LU e piv da gm_linalg_lu). */
/* Doolittle LU senza pivoting: A = L*U, L unit lower triangular, U upper
 * triangular (da LU-DecompEngine). L e U sono n×n row-major separati. */
gm_status gm_linalg_lu_doolittle(int n, const gm_real *A, gm_real *L, gm_real *U);
gm_status gm_linalg_lu_solve(int n, const gm_real *LU, const int *piv,
                             const gm_real *b, gm_real *x);
/* A x = b (copia interna + LU). */
gm_status gm_linalg_solve(int n, const gm_real *A, const gm_real *b, gm_real *x);

/* Cholesky in-place (triangolo inferiore). GM_OK o GM_ERR_ARG (non SPD). */
gm_status gm_linalg_cholesky(int n, gm_real *A);
/* Risolve L Lᵀ x = b. */
gm_status gm_linalg_chol_solve(int n, const gm_real *L, const gm_real *b, gm_real *x);

gm_real gm_linalg_det(int n, const gm_real *A);
/* Minore M_ij: determinante della sottomatrice (n-1)x(n-1) senza riga i, colonna j. */
gm_real gm_linalg_minor(int n, const gm_real *A, int i, int j);
/* Matrice dei cofattori: C_ij = (-1)^(i+j) M_ij. */
gm_status gm_linalg_cofactor(int n, const gm_real *A, gm_real *C);
/* Aggiunta: trasposta dei cofattori (A^{-1} = adj(A)/det(A)). */
gm_status gm_linalg_adjugate(int n, const gm_real *A, gm_real *Adj);
gm_status gm_linalg_inverse(int n, const gm_real *A, gm_real *inv);

/* B = Aᵀ. A: m×n (lda), B: n×m (ldb). */
/* Norma di Frobenius: sqrt(sum a_ij^2). */
/* Ridotta a scala per righe (RREF) di A (m×n, row-major) in place.
 * Ritorna il rango; pivots (opzionale, min(m,n)) riceve gli indici delle
 * colonne pivot. */
int gm_linalg_rref(int m, int n, gm_real *A, int *pivots);
/* Rango via RREF. */
int gm_linalg_rank(int m, int n, const gm_real *A);
/* Base dello spazio delle colonne (le colonne pivot di A): basis (m×r), r=rank. */
int gm_linalg_col_space(int m, int n, const gm_real *A, gm_real *basis);
/* Base dello spazio delle righe (righe non nulle della RREF): basis (r×n). */
int gm_linalg_row_space(int m, int n, const gm_real *A, gm_real *basis);
gm_real gm_linalg_norm_frobenius(int m, int n, const gm_real *A);
/* Traccia (n×n): somma della diagonale. */
gm_real gm_linalg_trace(int n, const gm_real *A);
/* Prodotto interno di Frobenius: sum a_ij * b_ij. */
gm_real gm_linalg_dot(int m, int n, const gm_real *A, const gm_real *B);
void gm_linalg_transpose(int m, int n, const gm_real *A, int lda, gm_real *B, int ldb);
void gm_linalg_identity(int n, gm_real *A, int lda);

/* ---- fattorizzazioni di Householder (da plasma/numruby) ---- */

/* QR: A (m×n, m>=n, lda) -> R nel triangolo superiore, vettori di Householder
 * sotto la diagonale, tau[n] (beta). */
gm_status gm_linalg_qr(int m, int n, gm_real *A, int lda, gm_real *tau);
/* Minimi quadrati via QR: min ||A x - b||, A m×n (m>=n). */
gm_status gm_linalg_lstsq(int m, int n, const gm_real *A, const gm_real *b, gm_real *x);

/* LQ: A (m×n, m<=n, lda) -> L nel triangolo inferiore, tau[m]. */
gm_status gm_linalg_lq(int m, int n, gm_real *A, int lda, gm_real *tau);
/* Soluzione a norma minima di A x = b (m<=n) via LQ. */
gm_status gm_linalg_lq_minnorm(int m, int n, const gm_real *A, const gm_real *b, gm_real *x);

/* Tridiagonalizzazione simmetrica (Householder): d[n] diagonale, e[n] subdiag. */
/* Riduzione a banda simmetrica (da dplasma, zhbrdt): Q^T A Q = T con T a
 * banda (|i-j| <= b) e Q ortogonale. b=1 equivale alla tridiagonalizzazione. */
gm_status gm_linalg_sym_band_reduce(int n, int b, const gm_real *A, gm_real *T, gm_real *Q);
gm_status gm_linalg_sym_tridiag(int n, const gm_real *A, gm_real *d, gm_real *e);
/* Autovalori di una matrice tridiagonale simmetrica (QL, shift implicito).
 * d ed e vengono modificati; eval[n] = autovalori (non ordinati). */
gm_status gm_linalg_tridiag_eig(int n, gm_real *d, gm_real *e, gm_real *eval);

/* SVD sottile (Jacobi unilaterale): A m×n (m>=n) -> U m×n, S[n], Vt n×n.
 * Autovalori singolari S ordinati in modo decrescente. */
gm_status gm_linalg_svd(int m, int n, const gm_real *A, gm_real *U, gm_real *S, gm_real *Vt);

/* QR con Gram-Schmidt (da pola): A m×n (m>=n) -> Q m×n ortonormale, R n×n
 * triangolare superiore, A = Q R. MGS = modificato (stabile), CGS = classico. */
gm_status gm_linalg_qr_mgs(int m, int n, const gm_real *A, gm_real *Q, gm_real *R);
gm_status gm_linalg_qr_cgs(int m, int n, const gm_real *A, gm_real *Q, gm_real *R);
/* Bidiagonalizzazione superiore (Householder, da pola gebd2): A m×n -> d[min], e. */
gm_status gm_linalg_bidiag(int m, int n, const gm_real *A, gm_real *d, gm_real *e);

/* ---- metodi diretti SLE ---- */
/* Thomas (tridiagonale): a[n-1] sub, b[n] diag, c[n-1] super, d[n] rhs -> x[n]. */
gm_status gm_linalg_thomas(int n, const gm_real *a, const gm_real *b, const gm_real *c,
                           const gm_real *d, gm_real *x);
/* LDL^T (simmetrica, no pivoting): A (n×n) -> L (triangolo inf., diag=1) + d[n]. */
gm_status gm_linalg_ldlt(int n, gm_real *A, gm_real *d);
gm_status gm_linalg_ldlt_solve(int n, const gm_real *L, const gm_real *d,
                               const gm_real *b, gm_real *x);
/* Gauss-Jordan (eliminazione completa). A non modificato; x[n]. */
gm_status gm_linalg_gauss_jordan(int n, const gm_real *A, const gm_real *b, gm_real *x);
/* Cramer 2×2 / 3×3. */
gm_status gm_linalg_cramer2(const gm_real *A, const gm_real *b, gm_real *x);
gm_status gm_linalg_cramer3(const gm_real *A, const gm_real *b, gm_real *x);
/* Matrice a banda: A in storage a banda (lda=2*kl+ku+1, come LAPACK dgbtrf). */
gm_status gm_linalg_band_solve(int n, int kl, int ku, const gm_real *A, int lda,
                               const gm_real *b, gm_real *x);

/* Levinson-Durbin: risolve il sistema di Toeplitz simmetrico per i coefficienti
 * LPC da un'autocorrelazione r[0..p]. a[0]=1, a[1..p] coefficienti; *err =
 * errore di predizione finale. Ritorna 0 su successo. (da xiph/LPCNet) */
int gm_linalg_levinson_durbin(int p, const gm_real *r, gm_real *a, gm_real *err);

/* Enumera le soluzioni di BASE del sistema A x = b (A m×n, m<=n): sceglie ogni
 * sottoinsieme di m colonne, risolve il sistema quadrato (se non singolare) e
 * registra la soluzione (variabili non-base = 0).
 * sols: max_sols×n row-major; feas[k]: 1 se tutte le componenti >= 0.
 * Ritorna il numero di soluzioni di base (-1 su errore).
 * (da shubhamtibra/OR-Lab: basic solutions di un sistema lineare) */
int gm_linalg_basic_solutions(int m, int n, const gm_real *A, const gm_real *b,
                              gm_real *sols, int *feas, int max_sols);

/* Levinson-Durbin: risolve il sistema Toeplitz simmetrico T x = b dove T ha
 * prima colonna (e prima riga) c[0..n-1]. Ritorna GM_OK o GM_ERR_SINGULAR. */
gm_status gm_linalg_levinson(int n, const gm_real *c, const gm_real *b, gm_real *x);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_LINALG_H */
