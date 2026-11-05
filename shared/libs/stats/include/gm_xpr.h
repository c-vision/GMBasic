/*
 * gm_xpr.h — aritmetica a precisione estesa (riscrittura, da ccmath).
 *
 * Doppio-doppio (double-double, ~106 bit) basato su Dekker/Knuth + FMA.
 */
#ifndef GMMATH_GM_XPR_H
#define GMMATH_GM_XPR_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_dd { gm_real hi, lo; } gm_dd;

gm_dd gm_dd_make(gm_real hi, gm_real lo);
gm_dd gm_dd_from_double(gm_real x);
gm_real gm_dd_to_double(gm_dd x);
gm_dd gm_dd_add(gm_dd a, gm_dd b);
gm_dd gm_dd_sub(gm_dd a, gm_dd b);
gm_dd gm_dd_mul(gm_dd a, gm_dd b);
gm_dd gm_dd_div(gm_dd a, gm_dd b);
gm_dd gm_dd_neg(gm_dd a);
gm_dd gm_dd_abs(gm_dd a);
gm_dd gm_dd_sqrt(gm_dd a);
gm_dd gm_dd_exp(gm_dd a);
gm_dd gm_dd_log(gm_dd a);
gm_dd gm_dd_sin(gm_dd a);
gm_dd gm_dd_cos(gm_dd a);
gm_dd gm_dd_pow(gm_dd a, int n);
int gm_dd_cmp(gm_dd a, gm_dd b);
gm_dd gm_dd_parse(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_XPR_H */
