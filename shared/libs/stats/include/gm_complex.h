/*
 * gm_complex.h — aritmetica complessa (riscrittura, da ccmath).
 *
 * Tipo `gm_cpx` + aritmetica e funzioni trascendenti complesse.
 */
#ifndef GMMATH_GM_COMPLEX_H
#define GMMATH_GM_COMPLEX_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_cpx { gm_real re, im; } gm_cpx;

gm_cpx gm_cpx_make(gm_real re, gm_real im);
gm_cpx gm_cpx_add(gm_cpx a, gm_cpx b);
gm_cpx gm_cpx_sub(gm_cpx a, gm_cpx b);
gm_cpx gm_cpx_mul(gm_cpx a, gm_cpx b);
gm_cpx gm_cpx_div(gm_cpx a, gm_cpx b);
gm_cpx gm_cpx_neg(gm_cpx a);
gm_cpx gm_cpx_conj(gm_cpx a);
gm_cpx gm_cpx_scale(gm_cpx a, gm_real s);
gm_real gm_cpx_abs(gm_cpx a);
gm_real gm_cpx_arg(gm_cpx a);
gm_real gm_cpx_norm2(gm_cpx a);
int gm_cpx_eq(gm_cpx a, gm_cpx b, gm_real tol);

gm_cpx gm_cpx_exp(gm_cpx z);
gm_cpx gm_cpx_log(gm_cpx z);
gm_cpx gm_cpx_sqrt(gm_cpx z);
gm_cpx gm_cpx_pow(gm_cpx z, gm_cpx w);
gm_cpx gm_cpx_sin(gm_cpx z);
gm_cpx gm_cpx_cos(gm_cpx z);
gm_cpx gm_cpx_tan(gm_cpx z);
gm_cpx gm_cpx_sinh(gm_cpx z);
gm_cpx gm_cpx_cosh(gm_cpx z);
gm_cpx gm_cpx_tanh(gm_cpx z);
gm_cpx gm_cpx_asin(gm_cpx z);
gm_cpx gm_cpx_acos(gm_cpx z);
gm_cpx gm_cpx_atan(gm_cpx z);
gm_cpx gm_cpx_asinh(gm_cpx z);
gm_cpx gm_cpx_acosh(gm_cpx z);
gm_cpx gm_cpx_atanh(gm_cpx z);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_COMPLEX_H */
