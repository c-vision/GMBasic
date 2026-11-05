/* gm_sparse_linalg.h - dense/sparse linear algebra (LU, Cholesky, sparse LU/Cholesky) */
#ifndef GMMATH_GM_SPARSE_LINALG_H
#define GMMATH_GM_SPARSE_LINALG_H

typedef struct { int m, n; double **v; } DMat;

DMat *dmat_new(int m, int n);
void  dmat_free(DMat *A);

/* solve A x = b by LU with partial pivoting (destroys A, b). returns 0 ok. */
int dmat_solve_lu(DMat *A, double *b, int n);
/* Cholesky factor of symmetric positive-definite A (in place, lower). 0 ok. */
int dmat_cholesky(DMat *A, int n);
/* solve using Cholesky factor L: A x = b. 0 ok. */
int dmat_chol_solve(const DMat *L, double *b, int n);

/* vector helpers */
void dvec_axpy(int n, double a, const double *x, double *y); /* y += a*x */
double dvec_dot(int n, const double *x, const double *y);
double dvec_norm2(int n, const double *x);
double dvec_norm_inf(int n, const double *x);

/* ---- reusable LU factorization (used by the interior-point solver) ---- */
typedef struct {
    int n;
    int ok;        /* 1 if factorization succeeded */
    double *lu;    /* n x n row-major, factored in place */
    int *piv;      /* pivot rows */
} LuFact;

/* factor a copy of A (n x n, row-major).  Returns NULL when A is singular
 * (pivot below 1e-300, i.e. exactly zero) and on allocation failure; a
 * non-NULL factor always has ok == 1, so the pointer test IS the singularity
 * test and ok is read only by dmat_lu_solve.  */
LuFact *dmat_lu_factor(const double *A, int n);
/* solve in place: rhs has length n. returns 0 ok, -1 singular. */
int dmat_lu_solve(const LuFact *f, double *rhs);
void dmat_lu_free(LuFact *f);

/* ---- symmetric eigenvalues (Jacobi rotations) ----
 * A: n x n row-major symmetric (modificato? no: copia interna).
 * eval[n] = autovalori in ordine di interesse (ascendente NON garantito,
 * chiamare cerca min), evec = n x n row-major colonna k = autovettore k. */
void dmat_eig_jacobi(int n, const double *A, double *eval, double *evec);

/* ---- sparse Cholesky  K = L L'  (K simmetrica definita positiva) ----
 * K in CSC: solo il triangolo INFERIORE (Ki[p] >= colonna). L e' lower
 * triangular in CSC (righe non ordinate dentro la colonna). Fattorizzazione
 * left-looking colonna per colonna; il fill-in e' gestito dinamicamente
 * (adiacenza di L' mantenuta incrementalmente). Ritorna NULL se K non e'
 * numericamente definita positiva (il chiamante regolarizza e riprova).
 * Un repeated (row,column) position is merged, not dropped: the column is
 * scattered into the dense work vector with w[i] += Kx[p]. */
typedef struct {
    int n;
    int *Lp;      /* n+1 column pointers */
    int *Li;      /* row indices */
    double *Lx;   /* values */
} SpChol;

SpChol *spchol_factor(int n, const int *Kp, const int *Ki, const double *Kx);
/* risolve K u = rhs in place. 0 ok, -1 singolare. */
int spchol_solve(const SpChol *L, double *rhs);
void spchol_free(SpChol *L);

/* ---- sparse LU with partial pivoting (non-symmetric A), P A = L U ----
 * A given in CSC (Ap[n+1], Ai, Ax).  L is CSC with the unit diagonal implicit;
 * U is CSR BY POSITION ROW (Up indexes rows, Ui holds column indices), which
 * is what the extraction loop and the back substitution both do.
 * perm[k] = original row placed at position k; qperm[k] = original column
 * placed at position k (the fill-reducing ordering; the natural order is kept
 * when sym_amd cannot run).  Returns NULL if singular (pivot below 1e-300) or
 * if a row cannot grow.
 * REQUIREMENT: within a column the row indices are DISTINCT.  The elimination
 * stages a row update with w[col] = rows[i].cv[a] and reads a pivot with
 * srow_get, which returns the FIRST match -- a repeated position therefore
 * contributes its last value to the update and its first value to the pivot
 * search, never their sum.  The caller must merge (tri3_to_csc in socp.c
 * assembles the conic KKT and does).
 * Solve applies P, then forward/back substitution, then maps back through q. */
typedef struct {
    int n;
    int *perm;     /* n: row permutation (position -> original row) */
    int *qperm;    /* n: column permutation (position -> original column), fill-reducing */
    int *Lp, *Li; double *Lx;   /* L in CSC, unit diagonal implicit */
    int *Up, *Ui; double *Ux;   /* U in CSR by position row: Up[i]..Up[i+1] are row i's columns */
} SpluFact;

SpluFact *splu_factor(int n, const int *Ap, const int *Ai, const double *Ax);
/* solves A x = rhs in place (rhs overwritten with x). 0 ok, -1 singular. */
int splu_solve(const SpluFact *F, double *rhs);
void splu_free(SpluFact *F);

#endif /* GMMATH_GM_SPARSE_LINALG_H */
extern int gm_sparse_debug;  /* ex GMB_DBG_SPCHOL: trace sparse chol/LU */
