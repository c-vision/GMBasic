/*
 * gm_elastic.h — elasticità isotropa (da poly3d_omp).
 *
 * Costanti elastiche (modulo di taglio μ, Poisson ν, Young E, bulk K, Lamé λ):
 * compilazione delle costanti mancanti date due qualsiasi, e legge di Hooke
 * isotropa (strain <-> stress). Matrici 3×3 row-major.
 */
#ifndef GMMATH_GM_ELASTIC_H
#define GMMATH_GM_ELASTIC_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_elas {
    gm_real sm;  /* modulo di taglio μ */
    gm_real pr;  /* rapporto di Poisson ν */
    gm_real ym;  /* modulo di Young E */
    gm_real bm;  /* modulo di bulk K */
    gm_real ll;  /* primo parametro di Lamé λ */
} gm_elas;

/* Compila le costanti mancanti: esattamente due campi devono essere noti
 * (gli altri NaN). Ritorna GM_OK o GM_ERR_ARG. */
gm_status gm_elas_complete(gm_elas *e);

/* Hooke isotropo: stress = λ tr(strain) I + 2μ strain. strain/stress: 3×3 row-major. */
void gm_strain_to_stress(const gm_real strain[9], gm_real shear_mod,
                         gm_real lame_lambda, gm_real stress[9]);
/* Inversa: strain = (1/E)[(1+ν) stress - ν tr(stress) I]. */
void gm_stress_to_strain(const gm_real stress[9], gm_real youngs_mod,
                         gm_real poisson, gm_real strain[9]);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_ELASTIC_H */
