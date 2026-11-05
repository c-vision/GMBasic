/*
 * gm_sort.h — ordinamenti, ricerche e hash (riscrittura, da ccmath).
 */
#ifndef GMMATH_GM_SORT_H
#define GMMATH_GM_SORT_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*gm_cmp_fn)(const void *a, const void *b);

void gm_sort_quick(void *v, int n, size_t size, gm_cmp_fn cmp);
void gm_sort_heap(void *v, int n, size_t size, gm_cmp_fn cmp);
void gm_sort_shell(void *v, int n, size_t size, gm_cmp_fn cmp);
void gm_sort_merge(void *v, int n, size_t size, gm_cmp_fn cmp);
/* Ricerca binaria su array ordinato: indice o -1. */
int gm_sort_bsearch(const void *key, const void *v, int n, size_t size, gm_cmp_fn cmp);

/* Comparatori pronti. */
int gm_cmp_double(const void *a, const void *b);
int gm_cmp_int(const void *a, const void *b);

/* Hash table chiave stringa -> puntatore. */
typedef struct gm_hash gm_hash;
gm_hash *gm_hash_new(int nbuckets);
void gm_hash_free(gm_hash *h);
gm_status gm_hash_put(gm_hash *h, const char *key, void *val);
void *gm_hash_get(const gm_hash *h, const char *key);
int gm_hash_del(gm_hash *h, const char *key);
int gm_hash_count(const gm_hash *h);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_SORT_H */
