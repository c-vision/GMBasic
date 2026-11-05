/*
 * gc_sparse.h — strutture sparse: COO/CSR/CSC e conversioni.
 *
 * Solo strutture dati e conversioni. I kernel (SpMV/SpMM/semiring) sono in
 * gc_compute; i solver sparsi diretti restano in gmbmath.
 */
#ifndef GMBCOMPUTE_GC_SPARSE_H
#define GMBCOMPUTE_GC_SPARSE_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Triplette (COO). */
typedef struct gc_coo {
    int rows, cols, nnz;
    int *row;
    int *col;
    gc_real *val;
} gc_coo;

/* CSR: rowptr[rows+1], colidx[nnz], val[nnz] (indici di colonna ordinati). */
typedef struct gc_csr {
    int rows, cols, nnz;
    int *rowptr;
    int *colidx;
    gc_real *val;
} gc_csr;

/* CSC: colptr[cols+1], rowidx[nnz], val[nnz] (indici di riga ordinati). */
typedef struct gc_csc {
    int rows, cols, nnz;
    int *colptr;
    int *rowidx;
    gc_real *val;
} gc_csc;

/* ---- ciclo di vita ---- */
gc_status gc_coo_init(gc_coo *A, int rows, int cols, int nnz);
void gc_coo_free(gc_coo *A);
gc_status gc_csr_init(gc_csr *A, int rows, int cols, int nnz);
void gc_csr_free(gc_csr *A);
gc_status gc_csc_init(gc_csc *A, int rows, int cols, int nnz);
void gc_csc_free(gc_csc *A);

/* ---- conversione ---- */
/* COO -> CSR/CSC: somma i duplicati e ordina gli indici. */
gc_status gc_coo_to_csr(const gc_coo *A, gc_csr *out);
gc_status gc_coo_to_csc(const gc_coo *A, gc_csc *out);
/* Trasposizione: CSR -> CSC e viceversa (stesso pattern, ruoli scambiati). */
gc_status gc_csr_transpose(const gc_csr *A, gc_csc *out);
gc_status gc_csc_transpose(const gc_csc *A, gc_csr *out);
/* Conversioni tra formati (stessa matrice). */
gc_status gc_csr_to_csc(const gc_csr *A, gc_csc *out);
gc_status gc_csc_to_csr(const gc_csc *A, gc_csr *out);
/* Densa row-major (lda) -> CSR, scartando gli zeri. */
gc_status gc_dense_to_csr(int rows, int cols, const gc_real *A, int lda, gc_csr *out);

/* Numero di non-zeri e grado (numero di non-zeri per riga). */
int gc_csr_nnz(const gc_csr *A);
void gc_csr_row_nnz(const gc_csr *A, int *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_SPARSE_H */
