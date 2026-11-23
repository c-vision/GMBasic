/* signal_internal.h — helper privati di libgmbsignal (non esportati).
 *
 * Le funzioni gs_* ritornano buffer malloc'd (convenzione identica a quella del
 * runtime C da cui il codice e' stato spostato): l'FFI copia nel buffer del
 * chiamante e libera. Le lunghezze in uscita sono `long` (int64 su macOS/LP64).
 */
#ifndef GS_INTERNAL_H
#define GS_INTERNAL_H

#include <stddef.h>

#define GS_FILTER_MAX      256
#define GS_FILTER_MAXTAPS  64
#define GS_PI              3.14159265358979323846

/* FFT radix-2 in-place (re/im separati) + Bluestein per n non potenza di 2.
 * Copia fedele del kernel del runtime C (gmb_math.c): WELCH/RESAMPLE devono
 * dare gli stessi numeri delle gambe native. */
void gs_fft(double* re, double* im, int n, int inverse);

/* signal_core.c */
double* gs_signal_convolve(const double* x, long nx, const double* k, long nk, long* outn);
double* gs_signal_correlate(const double* x, long nx, const double* k, long nk, long* outn);
double* gs_signal_window(long n, int kind, long* outn);
double* gs_welch(const double* x, int n, int nperseg, double fs, int* out_rows);
double* gs_rfft(const double* x, int n, long* outn);
double* gs_irfft(const double* re, const double* im, int m, long* outn);
double* gs_resample(const double* x, int n, int num, long* outn);

/* signal_filters.c */
long    gs_filter_butter(long n, const double* wn, int nwn, const char* btype);
long    gs_filter_cheby1(long n, double rp, const double* wn, int nwn, const char* btype);
long    gs_filter_cheby2(long n, double rs, const double* wn, int nwn, const char* btype);
long    gs_filter_ellip(long n, double rp, double rs, const double* wn, int nwn, const char* btype);
double* gs_filter_lfilter(long h, const double* x, int nx, long* outn);
double* gs_filter_filtfilt(long h, const double* x, int nx, long* outn);
double* gs_filter_freqz(long h, int npts, long* outn);
long    gs_filter_count(void);

/* signal_fir.c */
double* gs_firwin(int numtaps, const double* cutoff, int ncut,
                  const char* btype, const char* window);
double* gs_find_peaks(const double* x, int n, double distance, int* outn);
double* gs_savgol_filter(const double* x, int n, int window, int polyorder, long* outn);
double* gs_decimate(const double* x, int n, int q, long* outn);

#endif /* GS_INTERNAL_H */
