/*
 * gor_flow2.h — flussi avanzati e matching.
 *
 * Dinic (max-flow veloce), min-cost max-flow (SSP con potenziali),
 * matching bipartito (Kuhn/Hopcroft-Karp).
 */
#ifndef GMBORTOOLS_GOR_FLOW2_H
#define GMBORTOOLS_GOR_FLOW2_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dinic max-flow: cap n×n (row-major, 0 = assente). Ritorna il valore. */
gor_real gor_dinic(int n, const gor_real *cap, int s, int t);

/* Min-cost max-flow (SSP con potenziali di Bellman-Ford/Dijkstra):
 * cap n×n, cost n×n. Ritorna il costo minimo per il max-flow; *flow_value
 * (se non NULL) riceve il valore del flusso. */
gor_real gor_min_cost_flow(int n, const gor_real *cap, const gor_real *cost, int s, int t,
                           gor_real *flow_value);

/* Matching bipartito massimo (Kuhn): adj nL×nR (1=arco). matchL[nL]=colonna,
 * matchR[nR]=riga (-1 se libero). Ritorna la cardinalità. */
int gor_bipartite_matching(int nL, int nR, const int *adj, int *matchL, int *matchR);

/* Matching massimo su grafo GENERALE (blossom). adj n×n (1=arco). match[n]=partner o -1.
 * Ritorna la cardinalità. */
int gor_general_matching(int n, const int *adj, int *match);

/* Circolazione ammissibile con lower/upper bounds e domande ai nodi.
 * lower/upper/demand n×n (demand[i*n+j] = flusso richiesto sul nodo j da i? uso
 * b[n] = domanda netta per nodo). flow n×n. Ritorna 1 se ammissibile. */
int gor_circulation_lower(int n, const double *lower, const double *upper,
                          const double *b, double *flow);

/* Gomory-Hu tree (cut tree): parent[n], weight[n] per gli archi (i, parent[i]).
 * Ritorna n-1. */
int gor_gomory_hu(int n, const gor_real *w, int *parent, gor_real *weight);
/* Maximum weight matching su grafo generale (euristica greedy + local search).
 * w n×n (pesi >= 0, 0=assente). match[n]. Ritorna il peso totale. */
gor_real gor_weighted_matching(int n, const gor_real *w, int *match);
/* Multi-commodity flow (greedy shortest-path con capacità residue).
 * cap n×n, commodities[k*3] = (src, dst, demand). Ritorna il flusso totale
 * instradato; flow n×n = archi usati (capacità consumata). */
gor_real gor_multicommodity(int n, const gor_real *cap, int ncomm, const gor_real *commodities,
                            gor_real *flow);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_FLOW2_H */
