/*
 * gc_simd.h — livello SIMD di gmbcompute.
 *
 * Astrazione del backend vettoriale CPU e classificazione compute/memory-bound
 * (metodo SIMD.txt). Backend: NEON (aarch64), AVX2/AVX512/SSE2 (x86), fallback
 * scalare.
 */
#ifndef GMBCOMPUTE_GC_SIMD_H
#define GMBCOMPUTE_GC_SIMD_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Nome del backend SIMD: "neon", "avx512", "avx2", "sse2", "scalar". */
const char *gc_simd_backend(void);

/* Numero di `gc_real` per vettore SIMD. */
int gc_simd_lanes(void);

/* 1 se il kernel (flops, bytes) è compute-bound, 0 se memory-bound. */
int gc_kernel_is_compute_bound(double flops, double bytes);

/* Soglia di intensità aritmetica (FLOP/byte). Default 8.0. */
void gc_simd_set_intensity_threshold(double flops_per_byte);
double gc_simd_intensity_threshold(void);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_SIMD_H */
