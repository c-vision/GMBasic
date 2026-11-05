/*
 * gm_transform.h — trasformate (riscrittura).
 *
 * Laplace inversa (Stehfest), Z-trasformata, wavelet di Haar, Hilbert, DCT/DST.
 */
#ifndef GMMATH_GM_TRANSFORM_H
#define GMMATH_GM_TRANSFORM_H

#include "gm_core.h"
#include "gm_complex.h"
#include "gm_quad.h" /* gm_fn1 */

#ifdef __cplusplus
extern "C" {
#endif

/* Antitrasformata di Laplace numerica di F(s) in t (n termine, n pari). */
gm_real gm_laplace_inverse_stehfest(gm_fn1 F, void *ctx, gm_real t, int n);
/* Z-trasformata X(z) = Σ x[m] z^{-m}, z = re + i im. */
gm_status gm_ztransform(const gm_real *x, int n, gm_real re, gm_real im,
                        gm_real *ore, gm_real *oim);
/* Trasformata wavelet di Haar (n potenza di 2), output in out[0..n-1]. */
gm_status gm_wavelet_haar(const gm_real *x, int n, gm_real *out);
/* Trasformata di Hilbert discreta (via DFT). */
gm_status gm_hilbert_transform(const gm_real *x, int n, gm_real *out);
/* DCT-II ortonormale e DST-II ortonormale. */
gm_status gm_dct(const gm_real *x, int n, gm_real *out);
gm_status gm_dst(const gm_real *x, int n, gm_real *out);
/* IDCT-II (inversa di gm_dct). */
gm_status gm_idct(const gm_real *X, int n, gm_real *out);
/* DCT-II 2D (rows x cols, row-major) e la sua inversa, separabili. */
gm_status gm_dct2(const gm_real *x, int rows, int cols, gm_real *out);
gm_status gm_idct2(const gm_real *X, int rows, int cols, gm_real *out);

/* Finestra di Hann: periodic=1 -> w[n]=0.5(1-cos(2*pi*n/N)) (come
 * torch.hann_window(N, periodic=True)); periodic=0 -> N-1 al denominatore. */
void gm_dsp_hann(int n, gm_real *w, int periodic);

/* STFT: x[n] reale, finestra w[N], hop. out: nframes*(N/2+1) gm_cpx (row-major,
 * frame per riga). *nframes aggiornato. Ritorna 0 su successo. */
int gm_dsp_stft(int n, const gm_real *x, int N, const gm_real *w, int hop,
                gm_cpx *out, int *nframes);
/* iSTFT con overlap-add: ricostruisce x[n] da nframes frame (N/2+1 bin). */
int gm_dsp_istft(int nframes, const gm_cpx *X, int N, const gm_real *w, int hop,
                 gm_real *x, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_TRANSFORM_H */
