/* ofx_fit.c — shim FFI per FITTING (gs_fit.c). */
#include "gs_fit.h"

double ofx_fit_polyfit(const double* x, const double* y, int n, int m, double* coef)
{
    return gs_fit_poly_ls(x, y, n, m, coef);
}

void ofx_fit_rsquared(const double* data, int n, double* out)
{
    double r2 = 0.0, angle = 0.0;
    gs_rsquared(data, n, &r2, &angle);
    out[0] = r2;
    out[1] = angle;
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
double ofx_fit_polyfit__ffi(const double* x, int64_t x_ext, const double* y, int64_t y_ext, int n, int m, double* coef, int64_t coef_ext) { return ofx_fit_polyfit(x, y, n, m, coef); }
void ofx_fit_rsquared__ffi(const double* data, int64_t data_ext, int n, double* out, int64_t out_ext) { ofx_fit_rsquared(data, n, out); }
