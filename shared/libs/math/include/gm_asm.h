/*
 * gm_asm.h — kernel hot-path in assembler inline AArch64 NEON.
 *
 * Obiettivo: controllare il scheduling delle FMA/load (oltre agli intrinsics).
 * Disponibili solo su aarch64; altrove il chiamante usa i kernel gc_array.
 * Compilare con -DGM_NO_ASM per disattivarli.
 */
#ifndef GMMATH_GM_ASM_H
#define GMMATH_GM_ASM_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1 se i kernel asm sono compilati/attivi. */
int gm_asm_available(void);

/* y += a * x (n double). */
void gm_asm_axpy(int n, gm_real a, const gm_real *x, gm_real *y);
/* sum_i x_i * y_i. */
gm_real gm_asm_dot(int n, const gm_real *x, const gm_real *y);
/* sum_i x_i. */
gm_real gm_asm_sum(int n, const gm_real *x);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_ASM_H */
