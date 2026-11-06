/*
 * gmbcompute.h — API pubblica aggregata di gmbcompute.
 *
 * Superficie di calcolo portabile delle librerie native di GMBasic: tipi base,
 * SIMD, strutture sparse, kernel e backend di calcolo.
 *
 * C99 puro, sole dipendenze libc/libm (backend GPU opzionali).
 */
#ifndef GMBCOMPUTE_H
#define GMBCOMPUTE_H

#include "gc_core.h"
#include "gmi_data.h"
#include "gc_simd.h"
#include "gc_sparse.h"
#include "gc_compute.h"
#include "gc_array.h"
#include "gc_gemm.h"
#include "gc_dd.h"
#include "gc_blas1.h"
#include "gc_blas2.h"
#include "gc_blas3.h"
#include "gc_graphblas.h"
#include "gc_pool.h"
#include "gc_checksum.h"

#endif /* GMBCOMPUTE_H */
