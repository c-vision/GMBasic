/*
 * gm_trig.h — trigonometria piana/sferica/iperbolica (riscrittura, da ccmath).
 * Solver di triangoli e aree.
 */
#ifndef GMMATH_GM_TRIG_H
#define GMMATH_GM_TRIG_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Piana: area di Erone. */
gm_real gm_trig_area(gm_real a, gm_real b, gm_real c);
/* Piana SAS: dati a, l'angolo incluso gamma, b -> lato c e area. */
gm_status gm_trig_sas(gm_real a, gm_real gamma, gm_real b, gm_real *c, gm_real *area);
/* Piana SSS: lati -> tre angoli (radianti). */
gm_status gm_trig_sss(gm_real a, gm_real b, gm_real c,
                      gm_real *alpha, gm_real *beta, gm_real *gamma);
/* Piana SSA: a, b, angolo beta -> angolo alpha (1 soluzione, 0 se nessuna). */
gm_status gm_trig_ssa(gm_real a, gm_real b, gm_real beta, gm_real *alpha);

/* Sferica: eccesso sferico (area su sfera unitaria). */
gm_real gm_strig_area(gm_real a, gm_real b, gm_real c);
/* Sferica SSS: lati (angoli al centro) -> angoli del triangolo. */
gm_status gm_strig_sss(gm_real a, gm_real b, gm_real c,
                       gm_real *A, gm_real *B, gm_real *C);
/* Sferica SAS: a, angolo C, b -> lato c e area. */
gm_status gm_strig_sas(gm_real a, gm_real C, gm_real b, gm_real *c, gm_real *area);

/* Iperbolica: area di un triangolo iperbolico (difetto). */
gm_real gm_htrig_area(gm_real a, gm_real b, gm_real c);
/* Iperbolica SSS: lati -> angoli. */
gm_status gm_htrig_sss(gm_real a, gm_real b, gm_real c,
                       gm_real *A, gm_real *B, gm_real *C);
/* Iperbolica SAS: a, angolo C, b -> lato c e area. */
gm_status gm_htrig_sas(gm_real a, gm_real C, gm_real b, gm_real *c, gm_real *area);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_TRIG_H */
