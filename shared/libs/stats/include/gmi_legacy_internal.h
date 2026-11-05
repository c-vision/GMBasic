/*
 * gmi_legacy_internal.h — prototipi cross-modulo dei moduli estratti da GMBasic.
 * Nel repo GMBasic provenivano da header condivisi; qui sono raccolti insieme.
 */
#ifndef GMB_LEGACY_INTERNAL_H
#define GMB_LEGACY_INTERNAL_H

#include "gmi_runtime.h"

gmb_int_t gmi_matrix_from_flat(const double* flat, int rows, int cols);
void gmi_fft(double* re, double* im, int n, int inverse);
double* gmi_rfft(const double* x, int n, gmb_int_t* outn);
double* gmi_irfft(const double* re, const double* im, int m, gmb_int_t* outn);
double gm_special_betainc(double a, double b, double x);

#endif /* GMB_LEGACY_INTERNAL_H */
