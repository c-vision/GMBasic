/*
 * gm_fourier.h — analisi di Fourier (riscrittura, da ccmath).
 */
#ifndef GMMATH_GM_FOURIER_H
#define GMMATH_GM_FOURIER_H

#include "gm_core.h"
#include "gm_complex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FFT complessa in-place (radix-2; fallback DFT per n non potenza di 2).
 * inverse != 0 esegue la trasformata inversa (senza normalizzazione 1/n). */
gm_status gm_fft_cpx(gm_cpx *a, int n, int inverse);
/* Spettro di potenza di un segnale reale: psd[n/2+1]. */
gm_status gm_fft_power_spectrum(const gm_real *x, int n, gm_real *psd);
/* Smoothing a media mobile (finestra 2m+1). */
void gm_fft_smooth(gm_real *x, int n, int m);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_FOURIER_H */
