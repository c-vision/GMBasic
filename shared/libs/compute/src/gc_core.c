/* gc_core.c — versione, stato, utilità di confronto. */
#include "gc_core.h"

#include <float.h>
#include <math.h>

const char *gc_version(void)
{
    return "0.1.0";
}

const char *gc_status_str(gc_status status)
{
    switch (status) {
    case GC_OK: return "ok";
    case GC_ERR_ARG: return "invalid argument";
    case GC_ERR_SINGULAR: return "singular matrix";
    case GC_ERR_NO_MEM: return "allocation failed";
    case GC_ERR_NOCONV: return "no convergence";
    case GC_ERR_RANGE: return "index out of range";
    case GC_ERR_INTERNAL: return "internal error";
    }
    return "unknown status";
}

gc_real gc_eps(void)
{
    return (gc_real)DBL_EPSILON;
}

int gc_is_finite(gc_real x)
{
    return isfinite((double)x) ? 1 : 0;
}

int gc_near(gc_real a, gc_real b, gc_real tol)
{
    gc_real scale = 1.0;
    gc_real diff;
    if (fabs(a) > scale) scale = fabs(a);
    if (fabs(b) > scale) scale = fabs(b);
    diff = fabs(a - b);
    return (diff <= tol * scale) ? 1 : 0;
}

/* ---- allocazione allineata (da AxonLA) ---- */
#include <stdlib.h>

void *gc_aligned_alloc(size_t alignment, size_t size)
{
    void *ptr = NULL;
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    if (size == 0) size = 1;
    /* arrotonda la dimensione al multiplo dell'allineamento */
    size = (size + alignment - 1) & ~(alignment - 1);
#if defined(_MSC_VER) || defined(_WIN32)
    return _aligned_malloc(size, alignment);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    ptr = aligned_alloc(alignment, size);
    return ptr;
#else
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    return ptr;
#endif
}

void gc_aligned_free(void *ptr)
{
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}
