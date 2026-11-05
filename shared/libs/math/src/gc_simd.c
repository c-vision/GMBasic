/* gc_simd.c — selezione backend SIMD + classificazione del muro. */
#include "gc_simd.h"

static double g_intensity_threshold = 8.0;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
const char *gc_simd_backend(void) { return "neon"; }
int gc_simd_lanes(void) { return 2; }
#elif defined(__AVX512F__)
const char *gc_simd_backend(void) { return "avx512"; }
int gc_simd_lanes(void) { return 8; }
#elif defined(__AVX2__)
const char *gc_simd_backend(void) { return "avx2"; }
int gc_simd_lanes(void) { return 4; }
#elif defined(__SSE2__)
const char *gc_simd_backend(void) { return "sse2"; }
int gc_simd_lanes(void) { return 2; }
#else
const char *gc_simd_backend(void) { return "scalar"; }
int gc_simd_lanes(void) { return 1; }
#endif

int gc_kernel_is_compute_bound(double flops, double bytes)
{
    double intensity;
    if (bytes <= 0.0) return 1;
    intensity = flops / bytes;
    return intensity >= g_intensity_threshold ? 1 : 0;
}

void gc_simd_set_intensity_threshold(double flops_per_byte)
{
    if (flops_per_byte > 0.0) g_intensity_threshold = flops_per_byte;
}

double gc_simd_intensity_threshold(void)
{
    return g_intensity_threshold;
}
