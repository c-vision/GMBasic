/*
 * interp_ffi.c — shim ABI per il namespace BASIC INTERP (spline cubica not-a-knot,
 * scipy.interpolate.splrep/splev).
 *
 * ABI del pacchetto: buffer del chiamante + cap, ritorno = elementi scritti
 * (-1 = argomento/errore, -2 = buffer troppo piccolo).
 * La matematica vive in gmi_interp.c (legacy condiviso); qui solo copia del risultato.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern double* gmi_splrep(const double* x, const double* y, int n, int64_t* out_len);
extern double* gmi_splev(const double* spline, int slen, const double* xq, int nq, int64_t* outn);

int64_t ofx_interp_splrep(const double* x, const double* y, int64_t n, double* out, int64_t cap)
{
    if (!x || !y || !out || n < 3) return -1;
    int64_t len = 0;
    double* r = gmi_splrep(x, y, (int)n, &len);
    if (!r || len <= 0) { free(r); return -1; }
    if (cap < len) { free(r); return -2; }
    memcpy(out, r, sizeof(double) * (size_t)len);
    free(r);
    return len;
}

int64_t ofx_interp_splev(const double* spline, int64_t slen, const double* xq, int64_t nq,
                         double* out, int64_t cap)
{
    if (!spline || !xq || !out || slen < 1 || nq < 0) return -1;
    int64_t on = 0;
    double* r = gmi_splev(spline, (int)slen, xq, (int)nq, &on);
    if (!r || on < 0) { free(r); return -1; }
    if (cap < on) { free(r); return -2; }
    if (on > 0) memcpy(out, r, sizeof(double) * (size_t)on);
    free(r);
    return on;
}
