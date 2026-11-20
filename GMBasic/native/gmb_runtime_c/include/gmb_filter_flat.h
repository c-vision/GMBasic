/* gmb_filter_flat.h — PONTE dei FILTRI IIR per i backend MANAGED (P/Invoke C#→C).
 *
 * PERCHÉ. `CommonLib/Numerics/SignalFilters.cs` implementava in C# design e applicazione degli
 * stessi filtri che `gmb_filters.c` implementa per VM/C e C99Native. Qui il managed chiama i
 * kernel del runtime (unica sorgente).
 *
 * HANDLE. Il design ritorna un handle (> 0) della tabella del runtime; `gmb_flf_from_ba` la crea
 * da coefficienti già calcolati, `gmb_flf_taps`/`gmb_flf_coeffs` li rileggono, `gmb_flf_free`
 * rilascia lo slot. I buffer ritornati sono allocati dal runtime: il chiamante copia e libera. */
#ifndef GMB_FILTER_FLAT_H
#define GMB_FILTER_FLAT_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

int64_t gmb_flf_butter(int64_t n, const double* wn, int nwn, const char* btype);
int64_t gmb_flf_cheby1(int64_t n, double rp, const double* wn, int nwn, const char* btype);
int64_t gmb_flf_cheby2(int64_t n, double rs, const double* wn, int nwn, const char* btype);
int64_t gmb_flf_ellip(int64_t n, double rp, double rs, const double* wn, int nwn, const char* btype);

double* gmb_flf_lfilter(int64_t h, const double* x, int nx, int64_t* outn);
double* gmb_flf_filtfilt(int64_t h, const double* x, int nx, int64_t* outn);
double* gmb_flf_freqz(int64_t h, int npts, int64_t* outn);

int64_t gmb_flf_from_ba(const double* b, int nb, const double* a, int na);
int     gmb_flf_taps(int64_t h, int* nb, int* na);
int     gmb_flf_coeffs(int64_t h, double* b, int nb, double* a, int na);
void    gmb_flf_free(int64_t h);

double  gmb_flf_ellipk(double m);
double  gmb_flf_ellip_sn(double u, double m);
void    gmb_flf_ellip_scndn(double u, double m, double* sn, double* cn, double* dn);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FILTER_FLAT_H */
