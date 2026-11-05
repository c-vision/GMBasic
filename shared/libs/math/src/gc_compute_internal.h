/* gc_compute_internal.h — tabella di dispatch dei kernel (interna). */
#ifndef GMBCOMPUTE_GC_COMPUTE_INTERNAL_H
#define GMBCOMPUTE_GC_COMPUTE_INTERNAL_H

#include "gc_compute.h"

typedef struct gc_compute_ops {
    gc_status (*spmv_csr)(const gc_csr *, const gc_real *, gc_real *);
    gc_status (*spmv_csc)(const gc_csc *, const gc_real *, gc_real *);
    gc_status (*spmv_csr_t)(const gc_csr *, const gc_real *, gc_real *);
    gc_status (*spmspv_csc)(const gc_csc *, const int *, int, gc_real *);
    gc_status (*spmm_csr)(const gc_csr *, const gc_real *, int, gc_real *);
    gc_status (*spmv_minplus)(const gc_csr *, const gc_real *, gc_real *, gc_real);
    gc_status (*spmv_maxplus)(const gc_csr *, const gc_real *, gc_real *, gc_real);
    gc_status (*scan)(int, const int *, int *);
    gc_status (*scan_exclusive)(int, const int *, int *);
    gc_real   (*sum)(const gc_real *, int);
    int       (*count_nonzero)(const gc_real *, int);
    int       (*select)(const gc_real *, int, int *);
    void      (*scale)(gc_real *, int, gc_real);
} gc_compute_ops;

/* Backend compilati. I backend opzionali registrano le loro ops qui. */
const gc_compute_ops *gc_compute_ops_cpu(void);
const gc_compute_ops *gc_compute_ops_simd(void);
#ifdef _OPENMP
const gc_compute_ops *gc_compute_ops_omp(void);
#endif
#ifdef GC_HAS_CUDA
const gc_compute_ops *gc_compute_ops_cuda(void);
#endif
#ifdef GC_HAS_HIP
const gc_compute_ops *gc_compute_ops_hip(void);
#endif
#ifdef GC_HAS_METAL
const gc_compute_ops *gc_compute_ops_metal(void);
#endif

#endif /* GMBCOMPUTE_GC_COMPUTE_INTERNAL_H */
