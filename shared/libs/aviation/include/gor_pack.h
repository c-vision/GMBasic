/*
 * gor_pack.h — packing, knapsack, cutting stock.
 *
 * Bin packing (NF/FF/BF/WF/FFD/BFD + lower bound), knapsack (frazionario/0-1/
 * unbounded/subset-sum/meet-in-the-middle), cutting stock, 3D bin packing.
 * Base per cargo (AWB)/ULD packing.
 */
#ifndef GMBORTOOLS_GOR_PACK_H
#define GMBORTOOLS_GOR_PACK_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bin packing 1D. algo: 0=NF, 1=FF, 2=BF, 3=WF, 4=FFD, 5=BFD.
 * bin_of[i] = indice del bin dell'item i. Ritorna il numero di bin. */
int gor_bin_packing(int n, const gor_real *items, gor_real cap, int algo, int *bin_of);
/* Lower bound L1 (Martello-Toth) sul numero di bin. */
int gor_bin_packing_lb(int n, const gor_real *items, gor_real cap);

/* Knapsack frazionario: x[i] ∈ [0,1]. Ritorna il valore massimo. */
gor_real gor_knapsack_frac(int n, const gor_real *w, const gor_real *v, gor_real cap, gor_real *x);
/* 0/1 knapsack (capacità intera, DP). take[i] = 0/1. Ritorna il valore. */
gor_real gor_knapsack_01(int n, const gor_real *w, const gor_real *v, int cap, int *take);
/* 0/1 knapsack meet-in-the-middle (capacità reale). */
gor_real gor_knapsack_01_mitm(int n, const gor_real *w, const gor_real *v, gor_real cap, int *take);
/* Unbounded knapsack (capacità intera). counts[i] = copie. Ritorna il valore. */
gor_real gor_knapsack_unbounded(int n, const gor_real *w, const gor_real *v, int cap, int *counts);
/* Subset sum: 1 se un sottoinsieme somma a target. take[i] = 0/1. */
int gor_subset_sum(int n, const gor_real *items, gor_real target, int *take);

/* Cutting stock 1D (FFD): pezzi -> barre di lunghezza `stock`.
 * pattern[i] = indice barra del pezzo i. Ritorna il numero di barre. */
int gor_cutting_stock_ffd(int n, const gor_real *pieces, gor_real stock, int *pattern);
/* Bin packing 3D (euristica FFD per volume + first-fit). Ritorna il numero di bin. */
int gor_bin_packing_3d(int n, const gor_real *items, gor_real W, gor_real H, gor_real D,
                       int *bin_of);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_PACK_H */
