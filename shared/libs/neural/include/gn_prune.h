/*
 * gn_prune.h — sparsificazione e pruning (da daniel89710/lightNet).
 *
 * 2:4 structured sparsity (NVIDIA): in ogni finestra di 4 pesi consecutivi si
 * tengono solo i 2 di magnitudine maggiore. Channel pruning: si tengono i canali
 * con score (norma L1) più alto.
 */
#ifndef GMBNEURAL_GN_PRUNE_H
#define GMBNEURAL_GN_PRUNE_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Applica la sparsità strutturata: in ogni finestra di `window` pesi consecutivi
 * tiene i `nonzeros` di |w| maggiore, azzera gli altri. Ritorna il numero di
 * pesi azzerati. (window=4, nonzeros=2 -> "2:4"). */
int gn_sparsify_2_4(gn_real *w, int n, int window, int nonzeros);

/* Maschera di channel pruning: dato lo score l1[out_c] (es. norma L1 dei pesi di
 * ogni canale), tiene i canali con score nei top `keep_frac` (0..1).
 * mask[out_c]: 1 = tieni. Ritorna il numero di canali tenuti. */
int gn_prune_channels_mask(const gn_real *l1, int out_c, gn_real keep_frac,
                           unsigned char *mask);

/* Norma L1 dei pesi di ogni canale: w out_c × per_ch -> l1[out_c]. */
void gn_prune_channel_l1(const gn_real *w, int out_c, int per_ch, gn_real *l1);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_PRUNE_H */
