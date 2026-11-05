/*
 * gm_signal.h — elaborazione dei segnali (SciPy signal/spatial), wrapper legacy.
 *
 * Convoluzione/correlazione, finestre, firwin, find_peaks, filtri IIR
 * (butter/cheby/ellip + lfilter/filtfilt/freqz) e utilità scipy (cdist/pdist,
 * convex_hull, savgol, welch, resample, decimate). Le funzioni *_alloc
 * allocano l'output (liberare con free).
 */
#ifndef GMMATH_GM_SIGNAL_H
#define GMMATH_GM_SIGNAL_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Convoluzione/correlazione "full" (output nx+nk-1). out_len se non NULL. */
gm_real *gm_signal_convolve(const gm_real *x, int nx, const gm_real *k, int nk, int *out_len);
gm_real *gm_signal_correlate(const gm_real *x, int nx, const gm_real *k, int nk, int *out_len);
/* Finestra (kind: 0=rect,1=hann,2=hamming,3=blackman,4=bartlett). */
gm_real *gm_signal_window(int n, int kind, int *out_len);

/* FIR (firwin): numtaps, cutoff (1 o 2 valori), btype, window. */
gm_real *gm_signal_firwin(int numtaps, const gm_real *cutoff, int ncut,
                          const char *btype, const char *window, int *out_len);
/* Indici dei picchi (array allocato, lunghezza *out_len). */
int *gm_signal_find_peaks(const gm_real *x, int n, gm_real distance, int *out_len);

/* Filtri IIR: progettazione (ritorna handle) e applicazione. */
long gm_signal_butter(int order, const gm_real *wn, int nwn, const char *btype);
long gm_signal_cheby1(int order, gm_real rp, const gm_real *wn, int nwn, const char *btype);
long gm_signal_cheby2(int order, gm_real rs, const gm_real *wn, int nwn, const char *btype);
long gm_signal_ellip(int order, gm_real rp, gm_real rs, const gm_real *wn, int nwn, const char *btype);
/* Applica il filtro (handle): output allocato. */
gm_real *gm_signal_lfilter(long h, const gm_real *x, int nx, int *out_len);
gm_real *gm_signal_filtfilt(long h, const gm_real *x, int nx, int *out_len);
gm_real *gm_signal_freqz(long h, int npts, int *out_len);

/* scipy.spatial / signal. */
gm_real *gm_signal_cdist(const gm_real *x, int nx, int dx, const gm_real *y, int ny, int dy,
                         const char *metric, int *out_len);
gm_real *gm_signal_pdist(const gm_real *x, int n, int d, const char *metric, int *out_len);
gm_real *gm_signal_convex_hull(const gm_real *pts, int n, int stride, int *out_len);
gm_real *gm_signal_savgol(const gm_real *x, int n, int window, int polyorder, int *out_len);
gm_real *gm_signal_resample(const gm_real *x, int n, int num, int *out_len);
gm_real *gm_signal_decimate(const gm_real *x, int n, int q, int *out_len);
gm_real *gm_signal_welch(const gm_real *x, int n, int nperseg, gm_real fs, int *out_len);

/* Gramian Angular Field (Wang & Oates 2015, da Moses #171, clean-room):
 * serie -> matrice n×n row-major allocata (n*n elementi, free con free).
 * min-max -> [-1,1] (clamp, range < 1e-6 -> 1), phi = acos(x),
 * GASF: G[i,j] = cos(phi_i + phi_j) = x_i*x_j - sqrt(1-x_i^2)*sqrt(1-x_j^2);
 * GADF: G[i,j] = sin(phi_i - phi_j) = x_j*sqrt(1-x_i^2) - x_i*sqrt(1-x_j^2). */
gm_real *gm_signal_gasf(const gm_real *x, int n, int *out_len);
gm_real *gm_signal_gadf(const gm_real *x, int n, int *out_len);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_SIGNAL_H */
