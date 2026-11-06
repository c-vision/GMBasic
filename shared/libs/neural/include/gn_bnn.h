/*
 * gn_bnn.h — Binarized Neural Network (1-bit) con XNOR + popcount.
 *
 * Da cornell-zhang/bnn-fpga: pesi e attivazioni binarizzati (±1), prodotto
 * scalare come XNOR + popcount; il batch-norm di inferenza è ripiegato in una
 * soglia intera (thresh = -(h/k)).
 */
#ifndef GMBNEURAL_GN_BNN_H
#define GMBNEURAL_GN_BNN_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Byte necessari per n bit. */
int gn_bnn_bytes(int n);
/* Numero di parole (unsigned long long) per n bit. */
int gn_bnn_words(int n);

/* Binarizza x[n] in bit packed (1 se x>=0, 0 altrimenti; convenzione bnn-fpga).
 * out: gn_bnn_words(n) parole. */
void gn_bnn_pack(unsigned long long *out, const gn_real *x, int n);

/* Soglia da parametri batch-norm (k, h): thresh = -(h/k); gestisce k=0. */
gn_real gn_bnn_thresh(gn_real k, gn_real h);

/* Dot binario via XNOR + popcount: conta i bit uguali (0..n). */
int gn_bnn_xnor_popcount(const unsigned long long *a, const unsigned long long *b, int n);

/* Prodotto scalare di vettori ±1: 2*popcount(XNOR) - n. */
int gn_bnn_dot(const unsigned long long *a, const unsigned long long *b, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_BNN_H */
