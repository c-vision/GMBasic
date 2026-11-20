/* gmb_interp_flat.c — PONTE SHARED delle interpolazioni del runtime condiviso.
 *
 * Nessuna formula qui: solo inoltro ai kernel di gmb_math.c (una sola implementazione). */
#include "gmb_interp_flat.h"

double gmb_ipf_linear(const double* x, const double* y, int n, double tx) {
    return gmb_interp_linear(x, y, n, tx);
}

double gmb_ipf_step(const double* x, const double* y, int n, double tx) {
    return gmb_interp_step(x, y, n, tx);
}

double gmb_ipf_polynomial(const double* x, const double* y, int n, double tx) {
    return gmb_interp_polynomial(x, y, n, tx);
}

double gmb_ipf_akima(const double* x, const double* y, int n, double tx) {
    return gmb_interp_akima(x, y, n, tx);
}

double gmb_ipf_pchip(const double* x, const double* y, int n, double tx) {
    return gmb_interp_pchip(x, y, n, tx);
}

double gmb_ipf_cubic_spline(const double* x, const double* y, int n, double tx) {
    return gmb_cubic_spline(x, y, n, tx);
}
