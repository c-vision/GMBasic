/*
 * gc_graphblas.h — operazioni sparse stile GraphBLAS (da SuiteSparse:GraphBLAS).
 *
 * Semiring generali (add/mul come puntatori a funzione) su CSR, con:
 * mxv (SpMV), mxm (SpGEMM), Kronecker, apply (operatore unario), reduce,
 * eadd (somma elemento a elemento). I semiring predefiniti coprono i casi
 * classici (plus_times, min_plus, max_plus, max_min, ...).
 */
#ifndef GMBCOMPUTE_GC_GRAPHBLAS_H
#define GMBCOMPUTE_GC_GRAPHBLAS_H

#include "gc_sparse.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gc_real (*gc_binop)(gc_real, gc_real);
typedef gc_real (*gc_unop)(gc_real);

typedef struct gc_semiring {
    gc_binop add;
    gc_binop mul;
    gc_real identity;   /* identità additiva (valore iniziale dell'accumulatore) */
} gc_semiring;

/* Semiring predefiniti. */
const gc_semiring *gc_semiring_plus_times(void);
const gc_semiring *gc_semiring_min_plus(void);
const gc_semiring *gc_semiring_max_plus(void);
const gc_semiring *gc_semiring_max_min(void);
const gc_semiring *gc_semiring_min_max(void);
const gc_semiring *gc_semiring_plus_min(void);
const gc_semiring *gc_semiring_plus_max(void);
const gc_semiring *gc_semiring_any_pair(void);
const gc_semiring *gc_semiring_times_plus(void);

/* y = A (semiring) x, A (m×n) CSR, x (n), y (m). */
gc_status gc_mxv(const gc_csr *A, const gc_real *x, gc_real *y, const gc_semiring *s);

/* C = A (semiring) B, CSR × CSR. C va liberata con gc_csr_free. */
gc_status gc_mxm(const gc_csr *A, const gc_csr *B, gc_csr *C, const gc_semiring *s);

/* C = A ⊗ B (Kronecker). C va liberata con gc_csr_free. */
gc_status gc_kronecker(const gc_csr *A, const gc_csr *B, gc_csr *C);

/* out->val = op(A->val) applicato a ogni elemento (stessa struttura). */
gc_status gc_apply(const gc_csr *A, gc_unop op, gc_csr *out);

/* Riduzione a scalare: s->add su tutti gli elementi; *result = identità se vuota. */
gc_status gc_reduce(const gc_csr *A, const gc_semiring *s, gc_real *result);

/* C = A ⊕ B (somma elemento a elemento, unione delle strutture). */
gc_status gc_eadd(const gc_csr *A, const gc_csr *B, gc_csr *C);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_GRAPHBLAS_H */
