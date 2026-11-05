/*
 * gm_tensor.h — tensore N-dimensionale strided (da libtensr).
 *
 * Array N-D row-major con strides espliciti: permette view zero-copy,
 * broadcasting e riduzioni. Dati in double (gm_real).
 */
#ifndef GMMATH_GM_TENSOR_H
#define GMMATH_GM_TENSOR_H

#include "gm_core.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GM_TENSOR_MAX_DIM 8

typedef struct gm_tensor {
    int ndim;
    int *shape;          /* ndim dimensioni */
    ptrdiff_t *strides;  /* ndim stride in elementi */
    gm_real *data;
    int owns;            /* 1 se possiede `data` (da liberare) */
} gm_tensor;

/* Crea un tensore di forma `shape` (dati non inizializzati). NULL su errore. */
gm_tensor *gm_tensor_create(int ndim, const int *shape);
/* Crea un tensore di zeri. */
gm_tensor *gm_tensor_zeros(int ndim, const int *shape);
/* Crea un tensore riempiendo con `value`. */
gm_tensor *gm_tensor_full(int ndim, const int *shape, gm_real value);
void gm_tensor_free(gm_tensor *t);

/* Numero totale di elementi. */
size_t gm_tensor_size(const gm_tensor *t);
/* Indice lineare (row-major) di un multi-indice. */
ptrdiff_t gm_tensor_offset(const gm_tensor *t, const int *idx);
gm_real gm_tensor_get(const gm_tensor *t, const int *idx);
void gm_tensor_set(gm_tensor *t, const int *idx, gm_real v);
void gm_tensor_fill(gm_tensor *t, gm_real v);

/* View zero-copy su un sottoblocco (shape/strides dati). */
gm_tensor *gm_tensor_view(const gm_tensor *src, int ndim, const int *shape,
                          const ptrdiff_t *strides, ptrdiff_t offset);
/* Reshape (view) con nuova forma; il numero di elementi deve combaciare. */
gm_tensor *gm_tensor_reshape(const gm_tensor *src, int ndim, const int *shape);
/* Permutazione degli assi (view). perm ha ndim elementi. */
gm_tensor *gm_tensor_permute(const gm_tensor *src, const int *perm);

/* Operazioni elemento per elemento con broadcasting. out deve avere la forma
 * broadcast risultante. */
gm_status gm_tensor_add(gm_tensor *out, const gm_tensor *a, const gm_tensor *b);
gm_status gm_tensor_sub(gm_tensor *out, const gm_tensor *a, const gm_tensor *b);
gm_status gm_tensor_mul(gm_tensor *out, const gm_tensor *a, const gm_tensor *b);

/* Riduzioni su tutti gli elementi. */
gm_real gm_tensor_sum(const gm_tensor *t);
gm_real gm_tensor_max(const gm_tensor *t);
gm_real gm_tensor_min(const gm_tensor *t);
gm_real gm_tensor_mean(const gm_tensor *t);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_TENSOR_H */
