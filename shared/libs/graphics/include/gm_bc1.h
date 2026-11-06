/*
 * gm_bc1.h — compressione texture BC1/DXT1 (da LA-Graphics).
 *
 * Ogni blocco 4×4 RGB (48 byte) è compresso in 8 byte: due colori endpoint
 * RGB565 + 16 indici da 2 bit (palette di 4 colori interpolati). L'encoder
 * sceglie gli endpoint lungo l'asse principale (PCA) del blocco.
 */
#ifndef GMMATH_GM_BC1_H
#define GMMATH_GM_BC1_H

#include "gm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Comprimi un blocco 4×4 RGB (48 byte, row-major) in 8 byte. */
void gm_bc1_encode_block(const unsigned char *rgb, unsigned char *out);
/* Decomprimi 8 byte in un blocco 4×4 RGB (48 byte). */
void gm_bc1_decode_block(const unsigned char *in, unsigned char *rgb);
/* Intera immagine (width e height multipli di 4). Ritorna il numero di byte
 * scritti, o -1 su errore. */
int gm_bc1_encode(int width, int height, const unsigned char *rgb, unsigned char *out);
int gm_bc1_decode(int width, int height, const unsigned char *in, unsigned char *rgb);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_BC1_H */
