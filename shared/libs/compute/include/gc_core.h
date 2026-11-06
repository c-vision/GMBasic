/*
 * gc_core.h — gmbcompute: tipi, stato, versione.
 *
 * gmbcompute è la superficie di calcolo portabile delle librerie native di
 * GMBasic: tipi base, SIMD, kernel densi/sparsi e backend di calcolo
 * (CPU/SIMD/OpenMP/CUDA/HIP/Metal). Foglia del grafo: non dipende da gmbmath
 * né da gmbortools.
 *
 * C99 puro, sole dipendenze libc/libm (i backend GPU sono opzionali).
 */
#ifndef GMBCOMPUTE_GC_CORE_H
#define GMBCOMPUTE_GC_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tipo reale della libreria (GMBasic è double-based). */
typedef double gc_real;

/* Tipo intero: 64 bit come il runtime di GMBasic. */
typedef long long gc_int;

#define GMBCOMPUTE_VERSION_MAJOR 0
#define GMBCOMPUTE_VERSION_MINOR 1
#define GMBCOMPUTE_VERSION_PATCH 0

/* Stato/errore restituito dalle funzioni che possono fallire. */
typedef enum gc_status {
    GC_OK = 0,
    GC_ERR_ARG = 1,      /* argomento non valido */
    GC_ERR_SINGULAR = 2, /* matrice singolare */
    GC_ERR_NO_MEM = 3,   /* allocazione fallita */
    GC_ERR_NOCONV = 4,   /* non convergenza */
    GC_ERR_RANGE = 5,    /* indice/valore fuori range */
    GC_ERR_INTERNAL = 6  /* stato interno incoerente */
} gc_status;

const char *gc_version(void);
const char *gc_status_str(gc_status status);
gc_real gc_eps(void);
int gc_is_finite(gc_real x);
int gc_near(gc_real a, gc_real b, gc_real tol);

/* Allocazione allineata (da AxonLA): SIMD-friendly. L'allineamento deve essere
 * una potenza di 2 >= sizeof(void*). gc_aligned_free libera il puntatore. */
void *gc_aligned_alloc(size_t alignment, size_t size);
void  gc_aligned_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_CORE_H */
