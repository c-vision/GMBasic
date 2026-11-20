/* gmb_filter_flat.c — PONTE SHARED dei filtri IIR del runtime condiviso.
 *
 * Nessuna formula qui: solo inoltro a gmb_filters.c (una sola implementazione). */
#include "gmb_filter_flat.h"

int64_t gmb_flf_butter(int64_t n, const double* wn, int nwn, const char* btype) {
    return gmb_filter_butter(n, wn, nwn, btype);
}

int64_t gmb_flf_cheby1(int64_t n, double rp, const double* wn, int nwn, const char* btype) {
    return gmb_filter_cheby1(n, rp, wn, nwn, btype);
}

int64_t gmb_flf_cheby2(int64_t n, double rs, const double* wn, int nwn, const char* btype) {
    return gmb_filter_cheby2(n, rs, wn, nwn, btype);
}

int64_t gmb_flf_ellip(int64_t n, double rp, double rs, const double* wn, int nwn, const char* btype) {
    return gmb_filter_ellip(n, rp, rs, wn, nwn, btype);
}

double* gmb_flf_lfilter(int64_t h, const double* x, int nx, int64_t* outn) {
    return gmb_filter_lfilter(h, x, nx, outn);
}

double* gmb_flf_filtfilt(int64_t h, const double* x, int nx, int64_t* outn) {
    return gmb_filter_filtfilt(h, x, nx, outn);
}

double* gmb_flf_freqz(int64_t h, int npts, int64_t* outn) {
    return gmb_filter_freqz(h, npts, outn);
}

int64_t gmb_flf_from_ba(const double* b, int nb, const double* a, int na) {
    return gmb_filter_from_ba(b, nb, a, na);
}

int gmb_flf_taps(int64_t h, int* nb, int* na) { return gmb_filter_taps(h, nb, na); }
int gmb_flf_coeffs(int64_t h, double* b, int nb, double* a, int na) { return gmb_filter_coeffs(h, b, nb, a, na); }
void gmb_flf_free(int64_t h) { gmb_filter_free(h); }

double gmb_flf_ellipk(double m) { return gmb_ellipk(m); }
double gmb_flf_ellip_sn(double u, double m) { return gmb_ellip_sn(u, m); }
void gmb_flf_ellip_scndn(double u, double m, double* sn, double* cn, double* dn) {
    gmb_ellip_scndn(u, m, sn, cn, dn);
}
