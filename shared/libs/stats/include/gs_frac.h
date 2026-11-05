/*
 * gs_frac.h — calcolo frazionario (Grünwald-Letnikov, Riemann-Liouville).
 * Progetto gmbstats-timeseries.
 */
#ifndef GMBSTATS_GS_FRAC_H
#define GMBSTATS_GS_FRAC_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

double gs_gamma_godfrey(double z);
int gs_frac_gl_coeffs(double alpha, int n, double *b);
int gs_frac_gl(double alpha, const double *f, int n, double h, double *out);
int gs_frac_rl(double alpha, const double *f, int n, double h, double *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_FRAC_H */
