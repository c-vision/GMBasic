/*
 * gor_graph3.h — proprietà strutturali dei grafi (porting da GMBasic gmb_graph*).
 *
 * Funzioni presenti nel motore nativo GMBasic ma assenti in gor_graph/gor_graph2:
 * cicli, bipartizione, diametro, eccentricità, coefficiente di clustering, sottografo.
 *
 * Rappresentazione: come gor_graph2 — adiacenza densa n×n row-major.
 *  - `adj` è 0/1 (arco presente/assente), usata da has_cycle/is_bipartite/clustering;
 *  - `w`   è pesata con GOR_INF/INFINITY = arco assente, usata da diameter/eccentricity/subgraph.
 */
#ifndef GMBORTOOLS_GOR_GRAPH3_H
#define GMBORTOOLS_GOR_GRAPH3_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1 se il grafo ORIENTATO contiene almeno un ciclo (Kahn: nodi processati < n). */
int gor_has_cycle(int n, const int *adj);
/* 1 se il grafo (trattato non orientato) è bipartito (2-colorazione BFS). */
int gor_is_bipartite(int n, const int *adj);

/* Diametro pesato (max distanza minima su Floyd-Warshall, archi assenti = INF).
 * 0 se n < 2 o se nessuna coppia è raggiungibile. */
gor_real gor_diameter(int n, const gor_real *w);
/* Eccentricità del nodo v (max distanza minima da v). -1 se v fuori range. */
gor_real gor_eccentricity(int n, const gor_real *w, int v);

/* Coefficiente di clustering locale di v (archi non orientati): 2*triangoli/(k*(k-1)).
 * 0 se v ha meno di 2 vicini. */
gor_real gor_clustering_coefficient(int n, const int *adj, int v);

/* Sottografo indotto dai nodi `nodes[0..k-1]`: out è k×k row-major.
 * Ritorna GOR_ERR_ARG se un indice è fuori range. */
gor_status gor_subgraph(int n, const gor_real *w, const int *nodes, int k, gor_real *out);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_GRAPH3_H */
