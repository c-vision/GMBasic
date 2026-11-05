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
