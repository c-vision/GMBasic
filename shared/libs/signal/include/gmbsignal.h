/* gmbsignal.h — API pubblica di libgmbsignal (dominio SIGNAL via FFI).
 *
 * Elaborazione dei segnali (scipy.signal): finestre, convoluzione/correlazione,
 * design e applicazione di filtri IIR (butter/cheby1/cheby2/ellip +
 * lfilter/filtfilt/freqz), FIR (firwin), picchi (find_peaks), ricampionamento
 * (resample/decimate), smoothing (savgol) e densita' spettrale (welch).
 *
 * Nessuna dipendenza esterna: solo libm. Sorgente canonico del pacchetto
 * `signal` (GMBasicMonoRepo/packages/signal/<ver>), come gmbcrypto: la copia
 * nel pacchetto deve restare IDENTICA a questa.
 *
 * Convenzioni (le stesse di gmbcrypto):
 *  - chi scrive in un buffer del chiamante riceve `cap` (numero di double
 *    disponibili) e ritorna gli elementi scritti, oppure OFX_SIGNAL_ERR (-1,
 *    errore) / OFX_SIGNAL_SMALL (-2, buffer troppo piccolo);
 *  - il design IIR ritorna un handle > 0 (registro interno della lib) da
 *    passare a lfilter/filtfilt/freqz; 0 = errore;
 *  - welch scrive le righe [freq, psd] in un buffer piatto (rows*2 double) e
 *    ritorna rows*2.
 *
 * NON sono qui:  (a) i FILTER.* del runtime (gmb_filter_lowpass/highpass/
 * bandpass/bandstop), che restano nel runtime C perche' servono a un altro
 * dominio;  (b) il comando SIGNAL / ON SIGNAL (bus eventi + GPM), che non e'
 * calcolo e resta builtin.
 */
#ifndef GMB_SIGNAL_H
#define GMB_SIGNAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OFX_SIGNAL_ERR   (-1)
#define OFX_SIGNAL_SMALL (-2)

/* ── finestre (kind: 0 hann, 1 hamming, 2 blackman) ─────────────────────────── */
long ofx_signal_window(int kind, long n, double* out, long cap);
long ofx_signal_hann(long n, double* out, long cap);
long ofx_signal_hamming(long n, double* out, long cap);
long ofx_signal_blackman(long n, double* out, long cap);

/* ── convoluzione / correlazione ("full": nx+nk-1 elementi) ─────────────────── */
long ofx_signal_convolve(const double* x, long nx, const double* k, long nk,
                         double* out, long cap);
long ofx_signal_correlate(const double* x, long nx, const double* k, long nk,
                          double* out, long cap);

/* ── design IIR: coefficienti (b,a) da ordine + frequenze + tipo ────────────── */
long ofx_signal_butter(long n, const double* wn, long nwn, const char* btype);
long ofx_signal_cheby1(long n, double rp, const double* wn, long nwn, const char* btype);
long ofx_signal_cheby2(long n, double rs, const double* wn, long nwn, const char* btype);
long ofx_signal_ellip(long n, double rp, double rs, const double* wn, long nwn, const char* btype);

/* ── applicazione del filtro (handle dal design) ────────────────────────────── */
long ofx_signal_lfilter(long h, const double* x, long nx, double* out, long cap);
long ofx_signal_filtfilt(long h, const double* x, long nx, double* out, long cap);
long ofx_signal_freqz(long h, long npts, double* out, long cap);

/* ── FIR, picchi, spettro, smoothing, ricampionamento ───────────────────────── */
long ofx_signal_firwin(long numtaps, const double* cutoff, long ncut,
                       const char* btype, const char* window, double* out, long cap);
long ofx_signal_find_peaks(const double* x, long n, double distance, double* out, long cap);
long ofx_signal_savgol(const double* x, long n, long window, long polyorder,
                       double* out, long cap);
long ofx_signal_resample(const double* x, long n, long num, double* out, long cap);
long ofx_signal_decimate(const double* x, long n, long q, double* out, long cap);
long ofx_signal_welch(const double* x, long n, long nperseg, double fs,
                      double* out, long cap);

/* ── diagnostica ────────────────────────────────────────────────────────────── */
/* Numero di handle filtro attualmente allocati (test/diagnostica). */
long ofx_signal_filter_count(void);

#ifdef __cplusplus
}
#endif
#endif /* GMB_SIGNAL_H */
