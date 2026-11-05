/* gm_sparse_linalg.c - dense/sparse linear algebra (LU, Cholesky) */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gm_sparse_linalg.h"
int gm_sparse_debug = 0;

/* Row-major dense matrix, rows allocated separately (A->v[i] is a row of n
 * doubles, ZERO FILLED).  The zero fill is load bearing: several callers build
 * a matrix by writing only the entries they know about -- the KKT blocks in
 * particular -- and read back the whole square.  A width or height of 0 yields
 * a one-element row so the pointer is never NULL and never freed twice. */
DMat *dmat_new(int m, int n) {
    DMat *A = (DMat *)malloc(sizeof(DMat));
    if (!A) return NULL;
    A->m = m; A->n = n;
    A->v = (double **)calloc((size_t)(m > 0 ? m : 1), sizeof(double *));
    if (!A->v) { free(A); return NULL; }
    for (int i = 0; i < m; i++) {
        A->v[i] = (double *)calloc((size_t)(n > 0 ? n : 1), sizeof(double));
        if (!A->v[i]) { dmat_free(A); return NULL; }
    }
    return A;
}

/* NULL-safe.  Frees the rows it was told about (i < A->m), so it is also the
 * cleanup for a matrix whose allocation failed half way -- dmat_new calls it
 * for exactly that. */
void dmat_free(DMat *A) {
    if (!A) return;
    if (A->v) {
        for (int i = 0; i < A->m; i++) free(A->v[i]);
        free(A->v);
    }
    free(A);
}

/* One-shot LU with partial pivoting: solves A x = b and OVERWRITES both A and
 * b.  A ends up holding L in its strictly lower triangle (unit diagonal
 * implicit) and U in its upper triangle and diagonal; the pivot search
 * exchanges row POINTERS, so A->v is permuted while the row buffers stay the
 * same allocations dmat_free will release.  Returns 1 when a pivot is below
 * 1e-14 -- an ABSOLUTE threshold, with no reference to the scale of A, which is
 * why a matrix legitimately built around 1e-20 reads as singular here.  The
 * solver does not call this: it needs one factorisation and several
 * right-hand sides, which is what LuFact is for.  Measured over the tree, its
 * callers are three samples using it as an independent reference value. */
int dmat_solve_lu(DMat *A, double *b, int n) {
    for (int k = 0; k < n; k++) {
        int piv = k;
        double maxv = fabs(A->v[k][k]);
        for (int i = k + 1; i < n; i++)
            if (fabs(A->v[i][k]) > maxv) { maxv = fabs(A->v[i][k]); piv = i; }
        if (maxv < 1e-14) return 1;
        if (piv != k) { double *t = A->v[piv]; A->v[piv] = A->v[k]; A->v[k] = t; }
        for (int i = k + 1; i < n; i++) {
            double f = A->v[i][k] / A->v[k][k];
            A->v[i][k] = f;
            for (int j = k + 1; j < n; j++) A->v[i][j] -= f * A->v[k][j];
        }
    }
    for (int i = 0; i < n; i++) {
        double s = b[i];
        for (int j = 0; j < i; j++) s -= A->v[i][j] * b[j];
        b[i] = s;
    }
    for (int i = n - 1; i >= 0; i--) {
        double s = b[i];
        for (int j = i + 1; j < n; j++) s -= A->v[i][j] * b[j];
        b[i] = s / A->v[i][i];
    }
    return 0;
}

/* Cholesky K = L L' of a symmetric matrix, no pivoting and no permutation:
 * the LOWER triangle of A is read and L replaces it in place, the upper
 * triangle is then zeroed so the buffer can be used as a full matrix by the
 * caller.  Because nothing is permuted, the sparsity pattern is irrelevant
 * here -- this is the dense reference, spchol_factor is the fill-aware one.
 * Returns 1 on a pivot that is not > 1e-300, i.e. "not numerically positive
 * definite"; A is left half factored, so a caller that wants to retry with a
 * regularisation must rebuild it. */
int dmat_cholesky(DMat *A, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            double s = A->v[i][j];
            for (int k = 0; k < j; k++) s -= A->v[i][k] * A->v[j][k];
            if (i == j) {
                if (s <= 1e-300) return 1;
                A->v[i][i] = sqrt(s);
            } else {
                A->v[i][j] = s / A->v[j][j];
            }
        }
        for (int j = i + 1; j < n; j++) A->v[i][j] = 0.0;
    }
    return 0;
}

/* Two triangular sweeps against the factor produced by dmat_cholesky.  Note
 * which triangle each reads: the forward sweep uses L->v[i][j] (lower), the
 * backward one uses L->v[j][i] for j > i -- that is the UPPER triangle of the
 * buffer, which the factor zeroed.  A lower-triangle-only half of a symmetric
 * matrix would therefore be solved WRONG, not approximately: the upper entries
 * would be counted as part of L'.  Always returns 0 -- singularity is
 * dmat_cholesky's question, and a zero diagonal here divides instead of
 * reporting one. */
int dmat_chol_solve(const DMat *L, double *b, int n) {
    for (int i = 0; i < n; i++) {
        double s = b[i];
        for (int j = 0; j < i; j++) s -= L->v[i][j] * b[j];
        b[i] = s / L->v[i][i];
    }
    for (int i = n - 1; i >= 0; i--) {
        double s = b[i];
        for (int j = i + 1; j < n; j++) s -= L->v[j][i] * b[j];
        b[i] = s / L->v[i][i];
    }
    return 0;
}

/* Plain reductions over a contiguous double array.  A length of 0 is a no-op
 * that returns 0: an empty vector has norm zero, which is a different
 * statement from "unknown" -- the conic code's own reductions keep the same
 * convention when they report the residual of a block of dimension zero.
 * Measured over the tree, nothing inside the library calls them; they are the
 * public surface for embedders and the reference those files' hand-written
 * loops are read against. */
void dvec_axpy(int n, double a, const double *x, double *y) {
    for (int i = 0; i < n; i++) y[i] += a * x[i];
}

double dvec_dot(int n, const double *x, const double *y) {
    double s = 0.0;
    for (int i = 0; i < n; i++) s += x[i] * y[i];
    return s;
}

double dvec_norm2(int n, const double *x) {
    return sqrt(dvec_dot(n, x, x));
}

double dvec_norm_inf(int n, const double *x) {
    double m = 0.0;
    for (int i = 0; i < n; i++) { double a = fabs(x[i]); if (a > m) m = a; }
    return m;
}

/* ---------------- LuFact ---------------- */

/* Factors a COPY of A: the caller keeps its matrix, which is what the conic
 * and exp/power paths need -- they rebuild the augmented system every iteration
 * and the residual of the previous factorisation is not recoverable from it.
 * Partial pivoting by ABSOLUTE magnitude, with a threshold of 1e-300 rather
 * than dmat_solve_lu's 1e-14: the point is to catch an exactly empty pivot,
 * because these systems carry a diagonal regularisation whose size is the
 * caller's business, and refusing a 1e-15 pivot here would refuse a solve the
 * caller deliberately asked for.
 * A singular input frees the struct and returns NULL, so a returned factor
 * always has ok == 1 and no caller reads the field.  The `mlt != 0` guard skips
 * the row update -- not the store of the multiplier, which happens either way
 * -- so it is a saving in work, not a change in the factor. */
LuFact *dmat_lu_factor(const double *A, int n) {
    if (n <= 0) return NULL;
    LuFact *f = (LuFact *)malloc(sizeof(LuFact));
    if (!f) return NULL;
    f->n = n;
    f->lu = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    f->piv = (int *)malloc((size_t)n * sizeof(int));
    if (!f->lu || !f->piv) { dmat_lu_free(f); return NULL; }
    memcpy(f->lu, A, (size_t)n * (size_t)n * sizeof(double));
    f->ok = 1;
    for (int k = 0; k < n; k++) {
        int p = k;
        double maxv = fabs(f->lu[k * n + k]);
        for (int i = k + 1; i < n; i++) {
            double a = fabs(f->lu[i * n + k]);
            if (a > maxv) { maxv = a; p = i; }
        }
        if (maxv < 1e-300) { f->ok = 0; break; }
        f->piv[k] = p;
        if (p != k)
            for (int j = 0; j < n; j++) {
                double t = f->lu[k * n + j];
                f->lu[k * n + j] = f->lu[p * n + j];
                f->lu[p * n + j] = t;
            }
        double pivv = f->lu[k * n + k];
        for (int i = k + 1; i < n; i++) {
            double mlt = f->lu[i * n + k] / pivv;
            f->lu[i * n + k] = mlt;
            if (mlt != 0.0)
                for (int j = k + 1; j < n; j++)
                    f->lu[i * n + j] -= mlt * f->lu[k * n + j];
        }
    }
    if (!f->ok) { dmat_lu_free(f); return NULL; }
    return f;
}

/* Solves with the factorisation, reading rhs in the ORIGINAL row ordering and
 * writing the solution back in the same ordering: the pivot swaps are applied
 * to the caller's own buffer, so handing over a vector that has already been
 * permuted would shuffle it twice.  The factor is const here, which is the
 * whole point of the type -- the predictor and the corrector solve the same
 * matrix, and iterative refinement solves it once per round.
 * Returns -1 only for a NULL or failed factor: a numerically nearly singular
 * system does NOT come back as an error, it comes back as a large answer, and
 * judging that answer is the caller's job (sdp.c measures the residual it
 * produces before keeping the direction). */
int dmat_lu_solve(const LuFact *f, double *rhs) {
    if (!f || !f->ok) return -1;
    int n = f->n;
    /* apply row swaps */
    for (int k = 0; k < n; k++) {
        int p = f->piv[k];
        if (p != k) { double t = rhs[k]; rhs[k] = rhs[p]; rhs[p] = t; }
    }
    /* forward: L y = P b (L unit lower) */
    for (int i = 0; i < n; i++) {
        double s = rhs[i];
        for (int j = 0; j < i; j++) s -= f->lu[i * n + j] * rhs[j];
        rhs[i] = s;
    }
    /* back: U x = y */
    for (int i = n - 1; i >= 0; i--) {
        double s = rhs[i];
        for (int j = i + 1; j < n; j++) s -= f->lu[i * n + j] * rhs[j];
        rhs[i] = s / f->lu[i * n + i];
    }
    return 0;
}

/* Also the rollback inside dmat_lu_factor, which calls it after allocating one
 * of the two arrays -- hence the per-field freedom rather than one release. */
void dmat_lu_free(LuFact *f) {
    if (!f) return;
    free(f->lu);
    free(f->piv);
    free(f);
}

/* ---------------- Jacobi eigenvalue (symmetric) ---------------- */
/* Autodecomposizione simmetrica con rotazioni di Jacobi (cyclic, sweep).
 * Convergenza: off-diagonale scende quadraticamente; tolleranza 1e-14. */
void dmat_eig_jacobi(int n, const double *A, double *eval, double *evec) {
    int nn = n > 0 ? n : 1;
    double *W = (double *)calloc((size_t)nn * (size_t)nn, sizeof(double));
    double *V = (double *)calloc((size_t)nn * (size_t)nn, sizeof(double));
    if (!W || !V) { free(W); free(V); return; }
    for (int i = 0; i < n * n; i++) W[i] = A[i];
    for (int i = 0; i < n; i++) V[i * n + i] = 1.0;
    for (int sweep = 0; sweep < 100; sweep++) {
        double off = 0.0;
        for (int p = 0; p < n; p++)
            for (int q = p + 1; q < n; q++) off += W[p * n + q] * W[p * n + q];
        if (off < 1e-30) break;
        for (int p = 0; p < n; p++) {
            for (int q = p + 1; q < n; q++) {
                double apq = W[p * n + q];
                if (fabs(apq) < 1e-32) continue;
                double theta = (W[q * n + q] - W[p * n + p]) / (2.0 * apq);
                double t = (theta >= 0.0 ? 1.0 : -1.0) /
                           (fabs(theta) + sqrt(theta * theta + 1.0));
                double c = 1.0 / sqrt(t * t + 1.0), sn = t * c;
                for (int k = 0; k < n; k++) {   /* righe (J^T W) */
                    double wp = W[p * n + k], wq = W[q * n + k];
                    W[p * n + k] = c * wp - sn * wq;
                    W[q * n + k] = sn * wp + c * wq;
                }
                for (int k = 0; k < n; k++) {   /* colonne (W J) */
                    double wp = W[k * n + p], wq = W[k * n + q];
                    W[k * n + p] = c * wp - sn * wq;
                    W[k * n + q] = sn * wp + c * wq;
                }
                for (int k = 0; k < n; k++) {   /* V J */
                    double vp = V[k * n + p], vq = V[k * n + q];
                    V[k * n + p] = c * vp - sn * vq;
                    V[k * n + q] = sn * vp + c * vq;
                }
            }
        }
    }
    for (int i = 0; i < n; i++) eval[i] = W[i * n + i];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) evec[i * n + j] = V[i * n + j];
    free(W); free(V);
}

/* ---------------- sparse Cholesky (left-looking) ----------------
 * K = L L', K simmetrica definita positiva data in CSC (triangolo
 * inferiore: colonna j con righe i >= j). Per ogni colonna j:
 *   w = K(:,j) - sum_{k<j, L(j,k)!=0} L(j,k) * L(:,k)
 *   L(j,j) = sqrt(w_j),  L(i,j) = w_i / L(j,j)
 * L'adiacenza per riga di L (lista dei k<j con L(j,k) != 0) e' mantenuta
 * incrementalmente: quando la colonna k e' completata, ogni (i,k) viene
 * accodato alla lista della riga i. Costo ~ O(sum_k nnz(L(:,k))^2) con
 * fill-in dinamico. */
SpChol *spchol_factor(int n, const int *Kp, const int *Ki, const double *Kx) {
    if (n <= 0 || !Kp || !Ki || !Kx) return NULL;
    int **ci = (int **)calloc((size_t)n, sizeof(int *));
    double **cv = (double **)calloc((size_t)n, sizeof(double *));
    int *cn = (int *)calloc((size_t)n, sizeof(int));
    int *cc = (int *)calloc((size_t)n, sizeof(int));
    int **ri = (int **)calloc((size_t)n, sizeof(int *));     /* L' row adjacency */
    double **rv = (double **)calloc((size_t)n, sizeof(double *));
    int *rn = (int *)calloc((size_t)n, sizeof(int));
    int *rc = (int *)calloc((size_t)n, sizeof(int));
    double *w = (double *)calloc((size_t)n, sizeof(double));
    int *mark = (int *)calloc((size_t)n, sizeof(int));
    int *touched = (int *)malloc((size_t)n * sizeof(int));
    if (!ci || !cv || !cn || !cc || !ri || !rv || !rn || !rc || !w || !mark || !touched) {
        for (int j = 0; j < n; j++) { free(ci[j]); free(cv[j]); free(ri[j]); free(rv[j]); }
        free(ci); free(cv); free(cn); free(cc);
        free(ri); free(rv); free(rn); free(rc);
        free(w); free(mark); free(touched);
        return NULL;
    }

    for (int j = 0; j < n; j++) {
        int nt = 0;
        /* scatter del triangolo inferiore di K(:,j) */
        for (int p = Kp[j]; p < Kp[j + 1]; p++) {
            int i = Ki[p];
            if (i < j) continue;
            if (mark[i] != j + 1) { mark[i] = j + 1; touched[nt++] = i; w[i] = 0.0; }
            w[i] += Kx[p];
        }
        /* sottrai i contributi delle colonne precedenti */
        for (int q = 0; q < rn[j]; q++) {
            int k = ri[j][q];
            double ljk = rv[j][q];
            for (int p = 0; p < cn[k]; p++) {
                int i = ci[k][p];
                if (i < j) continue;
                if (mark[i] != j + 1) { mark[i] = j + 1; touched[nt++] = i; w[i] = 0.0; }
                w[i] -= ljk * cv[k][p];
            }
        }
        double dj = w[j];
        if (!(dj > 1e-300)) {   /* non definita positiva */
            for (int q = 0; q < n; q++) { free(ci[q]); free(cv[q]); free(ri[q]); free(rv[q]); }
            free(ci); free(cv); free(cn); free(cc);
            free(ri); free(rv); free(rn); free(rc);
            free(w); free(mark); free(touched);
            return NULL;
        }
        double ljj = sqrt(dj);
        if (cc[j] < nt + 1) {
            int cap = nt + 2;
            int *ti = (int *)realloc(ci[j], (size_t)cap * sizeof(int));
            double *tv = (double *)realloc(cv[j], (size_t)cap * sizeof(double));
            if (!ti || !tv) { free(ti); free(tv);
                for (int q = 0; q < n; q++) { free(ci[q]); free(cv[q]); free(ri[q]); free(rv[q]); }
                free(ci); free(cv); free(cn); free(cc);
                free(ri); free(rv); free(rn); free(rc);
                free(w); free(mark); free(touched);
                return NULL; }
            ci[j] = ti; cv[j] = tv; cc[j] = cap;
        }
        ci[j][0] = j; cv[j][0] = ljj; cn[j] = 1;
        for (int q = 0; q < nt; q++) {
            int i = touched[q];
            double v = w[i];
            w[i] = 0.0;
            if (i == j) continue;
            v /= ljj;
            if (v == 0.0) continue;
            ci[j][cn[j]] = i; cv[j][cn[j]] = v; cn[j]++;
            if (rc[i] == rn[i]) {
                int cap = rc[i] ? rc[i] * 2 : 4;
                int *ti = (int *)realloc(ri[i], (size_t)cap * sizeof(int));
                double *tv = (double *)realloc(rv[i], (size_t)cap * sizeof(double));
                if (!ti || !tv) { free(ti); free(tv);
                    for (int q2 = 0; q2 < n; q2++) { free(ci[q2]); free(cv[q2]); free(ri[q2]); free(rv[q2]); }
                    free(ci); free(cv); free(cn); free(cc);
                    free(ri); free(rv); free(rn); free(rc);
                    free(w); free(mark); free(touched);
                    return NULL; }
                ri[i] = ti; rv[i] = tv; rc[i] = cap;
            }
            ri[i][rn[i]] = j; rv[i][rn[i]] = v; rn[i]++;
        }
        w[j] = 0.0;
    }

    /* compatta in CSC */
    SpChol *L = (SpChol *)malloc(sizeof(SpChol));
    if (!L) {
        for (int q = 0; q < n; q++) { free(ci[q]); free(cv[q]); free(ri[q]); free(rv[q]); }
        free(ci); free(cv); free(cn); free(cc);
        free(ri); free(rv); free(rn); free(rc);
        free(w); free(mark); free(touched);
        return NULL;
    }
    int total = 0;
    for (int j = 0; j < n; j++) total += cn[j];
    if (gm_sparse_debug) fprintf(stderr,"spchol n=%d nnzK=%d nnzL=%d fill=%.2f\n",n,Kp[n],total,(double)total/(Kp[n]>0?Kp[n]:1));
    L->n = n;
    L->Lp = (int *)malloc((size_t)(n + 1) * sizeof(int));
    L->Li = (int *)malloc((size_t)(total > 0 ? total : 1) * sizeof(int));
    L->Lx = (double *)malloc((size_t)(total > 0 ? total : 1) * sizeof(double));
    if (!L->Lp || !L->Li || !L->Lx) {
        free(L->Lp); free(L->Li); free(L->Lx); free(L);
        L = NULL;
    } else {
        int off = 0;
        for (int j = 0; j < n; j++) {
            L->Lp[j] = off;
            for (int p = 0; p < cn[j]; p++) { L->Li[off] = ci[j][p]; L->Lx[off] = cv[j][p]; off++; }
        }
        L->Lp[n] = off;
    }
    for (int q = 0; q < n; q++) { free(ci[q]); free(cv[q]); free(ri[q]); free(rv[q]); }
    free(ci); free(cv); free(cn); free(cc);
    free(ri); free(rv); free(rn); free(rc);
    free(w); free(mark); free(touched);
    return L;
}

/* K u = rhs through the two triangular sweeps, rhs overwritten.  The diagonal
 * is read by a scan of the column rather than assumed at slot 0: the factor
 * does write ci[j][0] = j, and the non-zero off-diagonal rows land after it in
 * the order the column was assembled (scatter, then one block per previously
 * completed column of L' that touches this row) -- which is NOT sorted by row
 * index.  Solve therefore depends on neither the slot convention nor the
 * ordering, and each sweep is O(nnz(L)) with a linear search inside it.
 * Returns -1 when a diagonal is missing or exactly zero; a factor that
 * spchol_factor accepted cannot normally reach that, and the test is there
 * because the alternative is dividing by zero and returning a number that looks
 * like an answer. */
int spchol_solve(const SpChol *L, double *rhs) {
    if (!L || !rhs) return -1;
    int n = L->n;
    /* forward: L v = rhs (colonne ascendenti) */
    for (int k = 0; k < n; k++) {
        int p0 = L->Lp[k], p1 = L->Lp[k + 1];
        double lkk = 0.0;
        for (int p = p0; p < p1; p++)
            if (L->Li[p] == k) { lkk = L->Lx[p]; break; }
        if (lkk == 0.0) return -1;
        rhs[k] /= lkk;
        double vk = rhs[k];
        for (int p = p0; p < p1; p++) {
            int i = L->Li[p];
            if (i > k) rhs[i] -= L->Lx[p] * vk;
        }
    }
    /* backward: L' u = v (colonne discendenti) */
    for (int k = n - 1; k >= 0; k--) {
        int p0 = L->Lp[k], p1 = L->Lp[k + 1];
        double lkk = 0.0, sum = 0.0;
        for (int p = p0; p < p1; p++) {
            if (L->Li[p] == k) lkk = L->Lx[p];
            else sum += L->Lx[p] * rhs[L->Li[p]];
        }
        if (lkk == 0.0) return -1;
        rhs[k] = (rhs[k] - sum) / lkk;
    }
    return 0;
}

void spchol_free(SpChol *L) {
    if (!L) return;
    free(L->Lp); free(L->Li); free(L->Lx);
    free(L);
}

/* ===================== sparse LU (partial pivoting) =====================
 * Right-looking Gaussian elimination on a sparse CSC matrix with partial
 * (row) pivoting. The working matrix is kept as sparse rows; L multipliers are
 * stored in-place in the strictly-lower positions so a row swap exchanges L and
 * U parts together. Fill-in is handled with a dense marker (w/stamp) per axpy.
 * Result: P A = L U with L unit-lower (CSC) and U upper (CSR by position-row). */
typedef struct { int nz, cap; int *ci; double *cv; } SRow;

/* A row of the working matrix is an unordered list of (column, value) slots,
 * a single namespace for both halves of the factor: slots at a column already
 * passed hold the L multipliers, slots ahead hold the working U row, which is
 * what lets a pivot swap exchange L and U content together.
 * srow_get finds the FIRST slot with a given column, and that is a contract
 * rather than an accident of the loop: a row carrying the same column twice
 * would be read as its first copy by the pivot search while the axpy update
 * stages the row as w[col] = rows[i].cv[a], which keeps the LAST copy.  The
 * factorisation would then be of a matrix that is neither copy, and no error
 * would say so.  Rows are built from the input CSC one entry at a time with no
 * summation, so the distinct-positions requirement of the header is enforced by
 * the caller: the conic KKT comes through tri3_to_csc, which merges repeated
 * (row,column) triplets into their sum before handing the matrix over. */
static int srow_get(const SRow *r, int col) {
    for (int q = 0; q < r->nz; q++) if (r->ci[q] == col) return q;
    return -1;
}

/* ---- fill-reducing ordering (minimum degree) ---- */
/* Set insert with a linear duplicate scan: returns 0 when x was already in the
 * list (so an adjacency list really is a set) and -1 only when the growth
 * failed.  The dedup is what lets the ORDERING tolerate a sparsity pattern that
 * repeats an edge -- the factorisation cannot, see srow_get.  A caller that
 * gets -1 abandons the ordering instead of truncating it, and sym_amd returns
 * NULL, which sends splu_factor back to the natural column order: a worse fill,
 * never a wrong answer. */
static int amd_add(int **a, int *cnt, int *cap, int x) {
    for (int q = 0; q < *cnt; q++) if ((*a)[q] == x) return 0;
    if (*cnt == *cap) {
        int nc = *cap ? *cap * 2 : 8;
        int *na = (int *)realloc(*a, (size_t)nc * sizeof(int));
        if (!na) return -1;
        *a = na; *cap = nc;
    }
    (*a)[(*cnt)++] = x;
    return 0;
}

/* Minimum-degree fill-reducing ordering of a symmetric sparsity pattern.
 * (Ap,Ai) is CSC; each entry (i,j) is treated as an undirected edge i-j.
 * Returns perm[n] (perm[k] = original index placed at position k), or NULL.
 * Explicit-fill minimum degree with a linear min search; used to order the
 * augmented conic KKT system before the sparse LU (the natural order can give
 * 80x fill-in there). */
static int *sym_amd(int n, const int *Ap, const int *Ai) {
    if (n <= 0 || !Ap || !Ai) return NULL;
    int **adj = (int **)calloc((size_t)n, sizeof(int *));
    int *deg = (int *)calloc((size_t)n, sizeof(int));
    int *cap = (int *)calloc((size_t)n, sizeof(int));
    int *rem = (int *)calloc((size_t)n, sizeof(int));
    int *seen = (int *)calloc((size_t)n, sizeof(int));
    int *nbr = (int *)malloc((size_t)n * sizeof(int));
    int *perm = (int *)malloc((size_t)n * sizeof(int));
    if (!adj || !deg || !cap || !rem || !seen || !nbr || !perm) {
        for (int i = 0; i < n; i++) free(adj[i]);
        free(adj); free(deg); free(cap); free(rem); free(seen); free(nbr); free(perm);
        return NULL;
    }
    int ok = 1;
    for (int j = 0; j < n && ok; j++)
        for (int p = Ap[j]; p < Ap[j + 1] && ok; p++) {
            int i = Ai[p];
            if (i == j || i < 0 || i >= n) continue;
            if (amd_add(&adj[i], &deg[i], &cap[i], j) || amd_add(&adj[j], &deg[j], &cap[j], i)) ok = 0;
        }
    int sst = 0;
    for (int step = 0; step < n && ok; step++) {
        int v = -1;
        for (int i = 0; i < n; i++) if (!rem[i] && (v < 0 || deg[i] < deg[v])) v = i;
        if (v < 0) { ok = 0; break; }
        perm[step] = v; rem[v] = 1;
        int nn = 0;
        for (int q = 0; q < deg[v]; q++) { int u = adj[v][q]; if (!rem[u]) nbr[nn++] = u; }
        for (int a = 0; a < nn; a++) { int u = nbr[a];   /* detach v */
            for (int q = 0; q < deg[u]; q++) if (adj[u][q] == v) { adj[u][q] = adj[u][deg[u] - 1]; deg[u]--; break; } }
        for (int a = 0; a < nn && ok; a++) { int u = nbr[a];   /* fill: neighbourhood -> clique */
            sst++;
            for (int q = 0; q < deg[u]; q++) seen[adj[u][q]] = sst;
            for (int b = 0; b < nn; b++) { int w = nbr[b];
                if (w == u || seen[w] == sst) continue;
                if (amd_add(&adj[u], &deg[u], &cap[u], w) || amd_add(&adj[w], &deg[w], &cap[w], u)) { ok = 0; break; } }
        }
    }
    for (int i = 0; i < n; i++) free(adj[i]);
    free(adj); free(deg); free(cap); free(rem); free(seen); free(nbr);
    if (!ok) { free(perm); return NULL; }
    return perm;
}

/* Factor A in CSC with partial (row) pivoting and a fill-reducing COLUMN
 * ordering.  The ordering is applied while the rows are scattered, not as a
 * later renumbering: the working matrix is already A with its columns
 * permuted, and the row swaps move whole SRow records, so the factor held on
 * return is of P A Q, with perm[] naming the rows and qperm[] the columns.
 * splu_solve is what undoes both, so a caller hands in an UNPERMUTED rhs and
 * reads an unpermuted solution -- the permutations are internal bookkeeping,
 * never a contract the caller has to remember.
 * Failure is always NULL and never a partial factor: either the column has no
 * pivot left above 1e-300 (singular, and the caller regularises and retries) or
 * a row could not grow to hold its fill.  The second mode is the one to know
 * about, because it breaks out with the working rows half updated -- an
 * inconsistent factor -- which is why every exit frees `rows` and why nothing
 * is published on that path.  On success the rows are scanned once more to
 * split the factor into L (CSC, unit diagonal implicit) and U (CSR by
 * position row), and from there the struct owns every array. */
SpluFact *splu_factor(int n, const int *Ap, const int *Ai, const double *Ax) {
    if (n < 0 || !Ap) return NULL;
    SpluFact *F = (SpluFact *)calloc(1, sizeof(SpluFact));
    SRow *rows = (SRow *)calloc((size_t)(n > 0 ? n : 1), sizeof(SRow));
    int *rc = (int *)calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    if (!F || !rows || !rc) { free(F); free(rows); free(rc); return NULL; }
    F->n = n;
    /* fill-reducing column ordering (falls back to the natural order) */
    int *qperm = sym_amd(n, Ap, Ai);
    int *iperm = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    if (!iperm) { free(qperm); free(F); free(rows); free(rc); return NULL; }
    if (qperm) { for (int k = 0; k < n; k++) iperm[qperm[k]] = k; }
    else {
        qperm = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
        if (!qperm) { free(iperm); free(F); free(rows); free(rc); return NULL; }
        for (int k = 0; k < n; k++) { qperm[k] = k; iperm[k] = k; }
    }
    F->qperm = qperm;
    for (int j = 0; j < n; j++) for (int p = Ap[j]; p < Ap[j + 1]; p++) rc[Ai[p]]++;
    for (int i = 0; i < n; i++) {
        rows[i].cap = rc[i] > 0 ? rc[i] : 1; rows[i].nz = 0;
        rows[i].ci = (int *)malloc((size_t)rows[i].cap * sizeof(int));
        rows[i].cv = (double *)malloc((size_t)rows[i].cap * sizeof(double));
        if (!rows[i].ci || !rows[i].cv) { free(rc); for (int q=0;q<=i;q++){free(rows[q].ci);free(rows[q].cv);} free(rows); free(F); return NULL; }
    }
    for (int j = 0; j < n; j++) for (int p = Ap[j]; p < Ap[j + 1]; p++) {
        int i = Ai[p]; rows[i].ci[rows[i].nz] = iperm[j]; rows[i].cv[rows[i].nz] = Ax[p]; rows[i].nz++;
    }
    free(iperm); free(rc);
    F->perm = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    double *w = (double *)calloc((size_t)(n > 0 ? n : 1), sizeof(double));
    int *wst = (int *)calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    int *touch = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    if (!F->perm || !w || !wst || !touch) { free(w);free(wst);free(touch); for(int i=0;i<n;i++){free(rows[i].ci);free(rows[i].cv);} free(rows); splu_free(F); return NULL; }
    for (int i = 0; i < n; i++) F->perm[i] = i;
    int stamp = 0, singular = 0;
    for (int k = 0; k < n; k++) {
        int pk = srow_get(&rows[k], k);
        int p = k; double best = pk >= 0 ? fabs(rows[k].cv[pk]) : 0.0;
        for (int i = k + 1; i < n; i++) { int q = srow_get(&rows[i], k); double v = q >= 0 ? fabs(rows[i].cv[q]) : 0.0; if (v > best) { best = v; p = i; } }
        if (best < 1e-300) { singular = 1; break; }
        if (p != k) { SRow tr = rows[k]; rows[k] = rows[p]; rows[p] = tr; int tp = F->perm[k]; F->perm[k] = F->perm[p]; F->perm[p] = tp; }
        int qk = srow_get(&rows[k], k);
        double piv = rows[k].cv[qk];
        for (int i = k + 1; i < n; i++) {
            int qi = srow_get(&rows[i], k);
            if (qi < 0) continue;
            double aik = rows[i].cv[qi];
            if (aik == 0.0) { continue; }
            double mult = aik / piv;
            rows[i].cv[qi] = mult;                 /* store L[i][k] in place */
            /* rows[i](cols>k) -= mult * rows[k](cols>k), with fill via marker */
            stamp++;
            int nt = 0;
            for (int a = 0; a < rows[i].nz; a++) { int col = rows[i].ci[a]; if (col > k) { w[col] = rows[i].cv[a]; wst[col] = stamp; touch[nt++] = col; } }
            for (int a = 0; a < rows[k].nz; a++) {
                int col = rows[k].ci[a]; if (col <= k) continue;
                double sub = mult * rows[k].cv[a];
                if (wst[col] != stamp) { wst[col] = stamp; w[col] = 0.0; touch[nt++] = col; }
                w[col] -= sub;
            }
            /* rebuild rows[i]: keep cols<=k (L part + the mult at col k), then cols>k from w */
            int newnz = 0;
            for (int a = 0; a < rows[i].nz; a++) if (rows[i].ci[a] <= k) { rows[i].ci[newnz] = rows[i].ci[a]; rows[i].cv[newnz] = rows[i].cv[a]; newnz++; }
            int need = newnz + nt;
            if (need > rows[i].cap) {
                int nc = need * 2 + 4;
                int *ni = (int *)realloc(rows[i].ci, (size_t)nc * sizeof(int));
                double *nv = (double *)realloc(rows[i].cv, (size_t)nc * sizeof(double));
                if (!ni || !nv) { free(ni); free(nv); singular = 2; break; }
                rows[i].ci = ni; rows[i].cv = nv; rows[i].cap = nc;
            }
            for (int a = 0; a < nt; a++) { int col = touch[a]; double v = w[col]; if (v != 0.0) { rows[i].ci[newnz] = col; rows[i].cv[newnz] = v; newnz++; } }
            rows[i].nz = newnz;
        }
        if (singular) break;
    }
    free(w); free(wst); free(touch);
    if (singular) {
        for (int i = 0; i < n; i++) { free(rows[i].ci); free(rows[i].cv); }
        free(rows); splu_free(F);
        return NULL;
    }
    /* extract L (CSC, unit diagonal implicit) and U (CSR by position-row) */
    int *lcnt = (int *)calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    int *ucnt = (int *)calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    if (!lcnt || !ucnt) { free(lcnt);free(ucnt); for(int i=0;i<n;i++){free(rows[i].ci);free(rows[i].cv);} free(rows); splu_free(F); return NULL; }
    for (int i = 0; i < n; i++)
        for (int a = 0; a < rows[i].nz; a++) { int col = rows[i].ci[a]; if (col < i) lcnt[col]++; else ucnt[i]++; }
    F->Lp = (int *)malloc((size_t)(n + 1) * sizeof(int));
    F->Up = (int *)malloc((size_t)(n + 1) * sizeof(int));
    if (!F->Lp || !F->Up) { free(lcnt);free(ucnt); for(int i=0;i<n;i++){free(rows[i].ci);free(rows[i].cv);} free(rows); splu_free(F); return NULL; }
    F->Lp[0] = 0; for (int j = 0; j < n; j++) F->Lp[j + 1] = F->Lp[j] + lcnt[j];
    F->Up[0] = 0; for (int i = 0; i < n; i++) F->Up[i + 1] = F->Up[i] + ucnt[i];
    int lnz = F->Lp[n], unz = F->Up[n];
    if (gm_sparse_debug) fprintf(stderr,"splu n=%d nnzA=%d nnzLU=%d fill=%.2f\n",n,Ap[n],lnz+unz,(double)(lnz+unz)/(Ap[n]>0?Ap[n]:1));
    F->Li = (int *)malloc((size_t)(lnz > 0 ? lnz : 1) * sizeof(int));
    F->Lx = (double *)malloc((size_t)(lnz > 0 ? lnz : 1) * sizeof(double));
    F->Ui = (int *)malloc((size_t)(unz > 0 ? unz : 1) * sizeof(int));
    F->Ux = (double *)malloc((size_t)(unz > 0 ? unz : 1) * sizeof(double));
    if (!F->Li || !F->Lx || !F->Ui || !F->Ux) { free(lcnt);free(ucnt); for(int i=0;i<n;i++){free(rows[i].ci);free(rows[i].cv);} free(rows); splu_free(F); return NULL; }
    int *lc = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    int *uc = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    if (!lc || !uc) { free(lc);free(uc);free(lcnt);free(ucnt); for(int i=0;i<n;i++){free(rows[i].ci);free(rows[i].cv);} free(rows); splu_free(F); return NULL; }
    for (int j = 0; j < n; j++) lc[j] = F->Lp[j];
    for (int i = 0; i < n; i++) uc[i] = F->Up[i];
    for (int i = 0; i < n; i++)
        for (int a = 0; a < rows[i].nz; a++) {
            int col = rows[i].ci[a]; double v = rows[i].cv[a];
            if (col < i) { F->Li[lc[col]] = i; F->Lx[lc[col]] = v; lc[col]++; }
            else { F->Ui[uc[i]] = col; F->Ux[uc[i]] = v; uc[i]++; }
        }
    free(lc); free(uc); free(lcnt); free(ucnt);
    for (int i = 0; i < n; i++) { free(rows[i].ci); free(rows[i].cv); }
    free(rows);
    return F;
}

/* Solves A x = rhs, rhs overwritten with x.  A scratch copy receives the row
 * permutation, so the caller's buffer is read once (through perm) and written
 * once (through qperm) and the factor stays const -- socp.c factors the
 * augmented sparse system once per Newton iteration and solves it twice, the
 * affine direction and the corrector.  The forward sweep updates
 * the scratch in place as it walks the columns of L, which is why the unit
 * diagonal is never stored.
 * Returns -1 if some row of U has no diagonal slot; the caller's rhs is then
 * UNTOUCHED, still holding the right-hand side, so a -1 must never be read as
 * an answer. */
int splu_solve(const SpluFact *F, double *rhs) {
    if (!F || !rhs) return -1;
    int n = F->n;
    double *pb = (double *)malloc((size_t)(n > 0 ? n : 1) * sizeof(double));
    if (!pb) return -1;
    for (int k = 0; k < n; k++) pb[k] = rhs[F->perm[k]];
    /* forward: L y = pb (L unit-lower, CSC) */
    for (int j = 0; j < n; j++) {
        double yj = pb[j];
        for (int p = F->Lp[j]; p < F->Lp[j + 1]; p++) pb[F->Li[p]] -= F->Lx[p] * yj;
    }
    /* back: U x = y (U upper, CSR by position-row) */
    for (int i = n - 1; i >= 0; i--) {
        double diag = 0.0, sum = 0.0;
        for (int p = F->Up[i]; p < F->Up[i + 1]; p++) {
            if (F->Ui[p] == i) diag = F->Ux[p];
            else sum += F->Ux[p] * pb[F->Ui[p]];
        }
        if (diag == 0.0) { free(pb); return -1; }
        pb[i] = (pb[i] - sum) / diag;
    }
    for (int k = 0; k < n; k++) rhs[F->qperm[k]] = pb[k];
    free(pb);
    return 0;
}

/* NULL-safe, and safe on a HALF-BUILT struct: splu_factor calloc's the record
 * and calls this on its error paths, where only some of the arrays exist.  A
 * new field added to SpluFact has to be released here as well, or every one of
 * those paths leaks it. */
void splu_free(SpluFact *F) {
    if (!F) return;
    free(F->perm); free(F->qperm); free(F->Lp); free(F->Li); free(F->Lx); free(F->Up); free(F->Ui); free(F->Ux);
    free(F);
}
