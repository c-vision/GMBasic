/* signal_ffi.c — superficie FFI (ABI C) di libgmbsignal: wrapper `ofx_signal_*`.
 *
 * Le primitive BASIC non possono ricevere un array come ritorno di una DECLARE:
 * ogni funzione scrive in un buffer del chiamante (cap = elementi disponibili) e
 * ritorna il numero di elementi scritti. -1 = errore, -2 = buffer troppo piccolo.
 * Il wrapper BASIC del pacchetto (signal.inc) alloca il buffer della dimensione
 * giusta e ritorna AS ARRAY OF DOUBLE, cosi' la firma pubblica resta quella
 * storica (SIGNAL.HANN(5), SIGNAL.LFILTER(h, x), ...).
 */
#include "signal_internal.h"
#include "gmbsignal.h"

#include <stdlib.h>
#include <string.h>

/* Copia il risultato (malloc'd dagli internal) nel buffer del chiamante.
 * src NULL -> 0 elementi (esito vuoto legittimo: find_peaks senza picchi). */
static long gs_emit(double* src, long n, double* out, long cap)
{
    if (!src) return 0;
    if (!out || cap < n) { free(src); return OFX_SIGNAL_SMALL; }
    if (n > 0) memcpy(out, src, sizeof(double) * (size_t)n);
    free(src);
    return n;
}

/* ── finestre ──────────────────────────────────────────────────────────────── */

long ofx_signal_window(int kind, long n, double* out, long cap)
{
    long on = 0;
    double* r = gs_signal_window(n, kind, &on);
    return gs_emit(r, on, out, cap);
}

long ofx_signal_hann(long n, double* out, long cap)     { return ofx_signal_window(0, n, out, cap); }
long ofx_signal_hamming(long n, double* out, long cap)  { return ofx_signal_window(1, n, out, cap); }
long ofx_signal_blackman(long n, double* out, long cap) { return ofx_signal_window(2, n, out, cap); }

/* ── convoluzione / correlazione ───────────────────────────────────────────── */

long ofx_signal_convolve(const double* x, long nx, const double* k, long nk,
                         double* out, long cap)
{
    if (!x || !k || nx <= 0 || nk <= 0) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_signal_convolve(x, nx, k, nk, &on);
    return gs_emit(r, on, out, cap);
}

long ofx_signal_correlate(const double* x, long nx, const double* k, long nk,
                          double* out, long cap)
{
    if (!x || !k || nx <= 0 || nk <= 0) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_signal_correlate(x, nx, k, nk, &on);
    return gs_emit(r, on, out, cap);
}

/* ── design IIR ────────────────────────────────────────────────────────────── */

long ofx_signal_butter(long n, const double* wn, long nwn, const char* btype)
{
    if (!wn || nwn < 1 || n < 1) return 0;
    return gs_filter_butter(n, wn, (int)nwn, btype ? btype : "low");
}

long ofx_signal_cheby1(long n, double rp, const double* wn, long nwn, const char* btype)
{
    if (!wn || nwn < 1 || n < 1) return 0;
    return gs_filter_cheby1(n, rp, wn, (int)nwn, btype ? btype : "low");
}

long ofx_signal_cheby2(long n, double rs, const double* wn, long nwn, const char* btype)
{
    if (!wn || nwn < 1 || n < 1) return 0;
    return gs_filter_cheby2(n, rs, wn, (int)nwn, btype ? btype : "low");
}

long ofx_signal_ellip(long n, double rp, double rs, const double* wn, long nwn, const char* btype)
{
    if (!wn || nwn < 1 || n < 1) return 0;
    return gs_filter_ellip(n, rp, rs, wn, (int)nwn, btype ? btype : "low");
}

/* ── applicazione ──────────────────────────────────────────────────────────── */

long ofx_signal_lfilter(long h, const double* x, long nx, double* out, long cap)
{
    if (!x || nx <= 0) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_filter_lfilter(h, x, (int)nx, &on);
    return gs_emit(r, on, out, cap);
}

long ofx_signal_filtfilt(long h, const double* x, long nx, double* out, long cap)
{
    if (!x || nx <= 0) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_filter_filtfilt(h, x, (int)nx, &on);
    return gs_emit(r, on, out, cap);
}

long ofx_signal_freqz(long h, long npts, double* out, long cap)
{
    if (npts <= 0) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_filter_freqz(h, (int)npts, &on);
    return gs_emit(r, on, out, cap);
}

/* ── FIR, picchi, smoothing, ricampionamento, spettro ──────────────────────── */

long ofx_signal_firwin(long numtaps, const double* cutoff, long ncut,
                       const char* btype, const char* window, double* out, long cap)
{
    if (numtaps <= 0 || !cutoff || ncut < 1) return OFX_SIGNAL_ERR;
    double* r = gs_firwin((int)numtaps, cutoff, (int)ncut, btype, window);
    return gs_emit(r, r ? numtaps : 0, out, cap);
}

long ofx_signal_find_peaks(const double* x, long n, double distance, double* out, long cap)
{
    if (!x || n <= 0) return OFX_SIGNAL_ERR;
    int on = 0;
    double* r = gs_find_peaks(x, (int)n, distance, &on);
    return gs_emit(r, on, out, cap);
}

long ofx_signal_savgol(const double* x, long n, long window, long polyorder,
                       double* out, long cap)
{
    if (!x || n <= 0) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_savgol_filter(x, (int)n, (int)window, (int)polyorder, &on);
    return gs_emit(r, on, out, cap);
}

long ofx_signal_resample(const double* x, long n, long num, double* out, long cap)
{
    if (!x || n <= 0 || num <= 0) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_resample(x, (int)n, (int)num, &on);
    return gs_emit(r, on, out, cap);
}

long ofx_signal_decimate(const double* x, long n, long q, double* out, long cap)
{
    if (!x || n <= 0 || q < 1) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_decimate(x, (int)n, (int)q, &on);
    return gs_emit(r, on, out, cap);
}

long ofx_signal_welch(const double* x, long n, long nperseg, double fs,
                      double* out, long cap)
{
    if (!x || n <= 0) return OFX_SIGNAL_ERR;
    int rows = 0;
    double* r = gs_welch(x, (int)n, (int)nperseg, fs, &rows);
    return gs_emit(r, (long)rows * 2, out, cap);
}

/* ── diagnostica ───────────────────────────────────────────────────────────── */

long ofx_signal_filter_count(void) { return gs_filter_count(); }

/* ── FFT reale (scipy.fft.rfft/irfft) ──────────────────────────────────────── */

long ofx_signal_rfft(const double* x, long n, double* out, long cap)
{
    if (!x || n <= 0) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_rfft(x, (int)n, &on);
    return gs_emit(r, on, out, cap);
}

long ofx_signal_irfft(const double* re, const double* im, long m, double* out, long cap)
{
    if (!re || !im || m < 2) return OFX_SIGNAL_ERR;
    long on = 0;
    double* r = gs_irfft(re, im, (int)m, &on);
    return gs_emit(r, on, out, cap);
}
