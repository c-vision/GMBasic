/*
 * gm_sparse_solve.h — solver sparso diretto (resta in gmbmath).
 *
 * Le strutture e i kernel sparsi sono in gmbcompute (gc_sparse/gc_compute).
 * Qui resta solo il solve diretto A x = b, che riusa `splu` (da `gm_sparse_linalg`).
 */
#ifndef GMMATH_GM_SPARSE_SOLVE_H
#define GMMATH_GM_SPARSE_SOLVE_H

#include "gm_core.h"
#include "gc_sparse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Solve sparso diretto A x = b (A CSC quadrata) via LU con pivoting (splu). */
gm_status gm_csc_solve(const gc_csc *A, const gc_real *b, gc_real *x);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_SPARSE_SOLVE_H */
