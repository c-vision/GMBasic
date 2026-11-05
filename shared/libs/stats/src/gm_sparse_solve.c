/* gm_sparse_solve.c — solver sparso diretto (splu). */
#include "gm_sparse_solve.h"
#include "gm_sparse_linalg.h"

gm_status gm_csc_solve(const gc_csc *A, const gc_real *b, gc_real *x)
{
    SpluFact *F;
    int i;
    if (A == NULL || b == NULL || x == NULL || A->rows != A->cols) return GM_ERR_ARG;
    F = splu_factor(A->rows, A->colptr, A->rowidx, A->val);
    if (F == NULL) return GM_ERR_SINGULAR;
    for (i = 0; i < A->rows; i++) x[i] = b[i];
    if (splu_solve(F, x) != 0) { splu_free(F); return GM_ERR_SINGULAR; }
    splu_free(F);
    return GM_OK;
}
