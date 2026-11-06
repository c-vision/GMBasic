/*
 * gn_memplan.h — pianificazione di memoria per grafi (da zhaozhixu/LightNet).
 *
 * Dati i tensori di un grafo con dimensione e intervallo di vita [birth,death]
 * (indici di passo), assegna a ciascuno un offset riutilizzando i buffer quando
 * le vite non si sovrappongono (greedy best-fit decrescente). Serve a ridurre la
 * memoria temporanea di un grafo di inferenza.
 */
#ifndef GMBNEURAL_GN_MEMPLAN_H
#define GMBNEURAL_GN_MEMPLAN_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_memplan_tensor {
    gn_real size;       /* dimensione del buffer (unità arbitrarie, es. byte) */
    int birth, death;   /* passo di nascita e morte (death >= birth) */
} gn_memplan_tensor;

/* Assegna offsets[i] a ciascun tensore. Ritorna la memoria totale richiesta
 * (max(offset+size)). offsets: n valori. */
gn_real gn_mem_plan(const gn_memplan_tensor *t, int n, gn_real *offsets);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_MEMPLAN_H */
