/*
 * gm_core.h — gmbmath: tipi, stato, versione.
 *
 * gmbmath è la libreria C99 della matematica del lato C di GMBasic.
 *
 * I tipi base sono quelli di gmbcompute (`gc_*`): gm_* sono alias per
 * compatibilità. La libreria dipende da gmbcompute per tipi, SIMD e kernel.
 *
 * C99 puro, sole dipendenze libc/libm.
 */
#ifndef GMMATH_GM_CORE_H
#define GMMATH_GM_CORE_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Alias dei tipi base di gmbcompute. */
typedef gc_real   gm_real;
typedef gc_int    gm_int;
typedef gc_status gm_status;

/* Alias degli stati (gmbmath usa la stessa tassonomia di gmbcompute). */
#define GM_OK           GC_OK
#define GM_ERR_ARG      GC_ERR_ARG
#define GM_ERR_SINGULAR GC_ERR_SINGULAR
#define GM_ERR_NO_MEM   GC_ERR_NO_MEM
#define GM_ERR_NOCONV   GC_ERR_NOCONV
#define GM_ERR_RANGE    GC_ERR_RANGE
#define GM_ERR_INTERNAL GC_ERR_INTERNAL

#define GMMATH_VERSION_MAJOR 0
#define GMMATH_VERSION_MINOR 1
#define GMMATH_VERSION_PATCH 0

/* Versione "MAJOR.MINOR.PATCH" come stringa statica. */
const char *gm_version(void);

/* Descrizione stabile di uno stato (mai NULL). */
const char *gm_status_str(gm_status status);

/* Funzioni trigonometriche "sicure" (da poly3d_omp): evitano le singolarita'
 * a +-pi/2 e i rapporti y/x infiniti. */
gm_real gm_safe_tan(gm_real alpha);
gm_real gm_safe_atan(gm_real y, gm_real x);
gm_real gm_safe_atan2(gm_real y, gm_real x);

/* Epsilon macchina per gm_real. */
gm_real gm_eps(void);

/* 1 se x è finito (né NaN né infinito), altrimenti 0. */
int gm_is_finite(gm_real x);

/* Confronto con tolleranza relativa+assoluta: |a-b| <= tol*max(1,|a|,|b|). */
int gm_near(gm_real a, gm_real b, gm_real tol);

/* Interpolazione smoothstep/smootherstep (Hermite, da c3dlas). */
gm_real gm_smoothstep(gm_real a, gm_real b, gm_real t);
gm_real gm_smootherstep(gm_real a, gm_real b, gm_real t);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_CORE_H */
