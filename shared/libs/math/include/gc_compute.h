/*
 * gc_compute.h — superficie di calcolo portabile (backend + kernel).
 *
 * Backend: CPU scalare (sempre), SIMD (vettorizzato), OpenMP (multi-core),
 * CUDA (NVIDIA), HIP (AMD ROCm / NVIDIA), Metal (Apple). Solo CPU/SIMD sono
 * sempre disponibili; gli altri sono opzionali (flag di build).
 *
 * I kernel operano su buffer dell'host: la superficie è identica su ogni
 * backend (il dispatch è trasparente al chiamante).
 */
#ifndef GMBCOMPUTE_GC_COMPUTE_H
#define GMBCOMPUTE_GC_COMPUTE_H

#include "gc_core.h"
#include "gc_compress.h"
#include "gc_sparse.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gc_backend {
    GC_BACKEND_CPU = 0,  /* scalare C99, sempre */
    GC_BACKEND_SIMD,     /* CPU vettorizzata (NEON/AVX/SSE) */
    GC_BACKEND_OPENMP,   /* CPU multi-core */
    GC_BACKEND_CUDA,     /* NVIDIA */
    GC_BACKEND_HIP,      /* AMD ROCm / NVIDIA (HIP, portabile) */
    GC_BACKEND_METAL,    /* Apple GPU */
    GC_BACKEND__COUNT
} gc_backend;

/* ---- registry backend ---- */
const char *gc_backend_name(gc_backend b);
const char *gc_backend_str(void);       /* nome del backend attivo */
gc_backend gc_active_backend(void);     /* backend attivo */
unsigned gc_available(void);            /* bitmask (1u<<b) dei backend compilati */
gc_status gc_set_backend(gc_backend b); /* forza un backend (per test/bench) */
int gc_device_count(void);
const char *gc_device_name(int i);

/* ---- kernel: SpMV / SpMSpV / SpMM ---- */
/* y = A x (A CSR). */
gc_status gc_spmv_csr(const gc_csr *A, const gc_real *x, gc_real *y);
/* y = A x (A CSC). */
gc_status gc_spmv_csc(const gc_csc *A, const gc_real *x, gc_real *y);
/* y = Aᵀ x (A CSR). */
gc_status gc_spmv_csr_t(const gc_csr *A, const gc_real *x, gc_real *y);
/* Frontier SpMSpV: y = A x con x sparsa (indici idx[0..k-1]) — BFS bottom-up. */
gc_status gc_spmspv_csc(const gc_csc *A, const int *idx, int k, gc_real *y);
/* C = A B (A CSR m×k, B denso k×n, C denso m×n row-major). */
gc_status gc_spmm_csr(const gc_csr *A, const gc_real *B, int n, gc_real *C);
/* Semiring: y_i = op_j ( A_ij ⊗ x_j ). */
gc_status gc_spmv_minplus(const gc_csr *A, const gc_real *x, gc_real *y, gc_real identity);
gc_status gc_spmv_maxplus(const gc_csr *A, const gc_real *x, gc_real *y, gc_real identity);

/* ---- kernel: vettoriali ---- */
/* Scan (prefix sum) inclusivo/esclusivo. */
gc_status gc_scan(int n, const int *x, int *y);
gc_status gc_scan_exclusive(int n, const int *x, int *y);
/* Riduzioni. */
gc_real gc_sum(const gc_real *x, int n);
int gc_count_nonzero(const gc_real *x, int n);
/* Compatta gli indici con x[i] != 0; ritorna il conteggio. */
int gc_select(const gc_real *x, int n, int *out);
/* x[i] *= a. */
void gc_scale(gc_real *x, int n, gc_real a);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_COMPUTE_H */
