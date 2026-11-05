/*
 * gor_linkpred.h — euristiche di link prediction (da muhanzhang/LinkPrediction).
 *
 * Dato il grafo con matrice di adiacenza adj (n×n, 0/1 o pesata), punteggi per
 * la probabilità che esista un arco (u,v). Usate come baseline per WLNM/SEAL.
 */
#ifndef GMBORTOOLS_GOR_LINKPRED_H
#define GMBORTOOLS_GOR_LINKPRED_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Numero di vicini in comune (common neighbors). */
int gor_lp_common_neighbors(const int *adj, int n, int u, int v);
/* Jaccard: |CN| / |Gamma(u) ∪ Gamma(v)|. */
gor_real gor_lp_jaccard(const int *adj, int n, int u, int v);
/* Adamic-Adar: sum_{w in CN} 1/log(deg(w)). */
gor_real gor_lp_adamic_adar(const int *adj, int n, int u, int v);
/* Resource Allocation: sum_{w in CN} 1/deg(w). */
gor_real gor_lp_resource_allocation(const int *adj, int n, int u, int v);
/* Preferential Attachment: deg(u)*deg(v). */
int gor_lp_preferential_attachment(const int *adj, int n, int u, int v);
/* Katz index: sum_{l>=1} beta^l * (A^l)_{uv} = ((I - beta A)^{-1} - I)_{uv}.
 * Ritorna 0 su successo (score in *score). */
int gor_lp_katz(const gor_real *adj, int n, gor_real beta, int u, int v, gor_real *score);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_LINKPRED_H */
