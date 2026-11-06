/*
 * gn_core.h — tipi di base di gmbneural (rete neurale).
 *
 * Matrice densa dinamica row-major (gn_mat) e operazioni di base. I kernel
 * pesanti (gemm) riusano gmbcompute (gc_gemm); gmbneural dipende da
 * gmbcompute e gmbmath.
 */
#ifndef GMBNEURAL_GN_CORE_H
#define GMBNEURAL_GN_CORE_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gc_real gn_real;
typedef gc_status gn_status;

#define GN_OK GC_OK
#define GN_ERR_ARG GC_ERR_ARG
#define GN_ERR_NO_MEM GC_ERR_NO_MEM
#define GN_ERR_NOCONV GC_ERR_NOCONV
#define GN_ERR_RANGE GC_ERR_RANGE

/* Matrice densa row-major: data[i*cols + j]. */
typedef struct gn_mat {
    int rows, cols;
    gn_real *data;
} gn_mat;

gn_status gn_mat_alloc(gn_mat *m, int rows, int cols);
void gn_mat_free(gn_mat *m);
gn_status gn_mat_zero(gn_mat *m);
gn_status gn_mat_fill(gn_mat *m, gn_real v);
gn_status gn_mat_copy(gn_mat *dst, const gn_mat *src);

/* accessor */
#define GN_AT(m, i, j) ((m).data[(size_t)(i) * (size_t)(m).cols + (size_t)(j)])

/* C = A * B (dimensioni coerenti). */
gn_status gn_mat_mul(gn_mat *C, const gn_mat *A, const gn_mat *B);
/* C = A * B^T ; C = A^T * B ; C = A^T * B^T. */
gn_status gn_mat_mul_nt(gn_mat *C, const gn_mat *A, const gn_mat *B);
gn_status gn_mat_mul_tn(gn_mat *C, const gn_mat *A, const gn_mat *B);
gn_status gn_mat_mul_tt(gn_mat *C, const gn_mat *A, const gn_mat *B);

/* C = A + B ; C = A + s*B ; C = s*A ; C += B. */
gn_status gn_mat_add(gn_mat *C, const gn_mat *A, const gn_mat *B);
gn_status gn_mat_add_scaled(gn_mat *C, const gn_mat *A, gn_real s, const gn_mat *B);
gn_status gn_mat_scale(gn_mat *A, gn_real s);
gn_status gn_mat_add_inplace(gn_mat *A, const gn_mat *B);

/* applica f a ogni elemento (in place o out). */
typedef gn_real (*gn_unary)(gn_real);
gn_status gn_mat_apply(gn_mat *out, const gn_mat *A, gn_unary f);

/* massimo/massimo in valore assoluto. */
gn_real gn_mat_max(const gn_mat *A);
gn_real gn_mat_absmax(const gn_mat *A);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_CORE_H */
