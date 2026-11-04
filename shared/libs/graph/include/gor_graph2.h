/*
 * gor_graph2.h — algoritmi avanzati su grafi.
 *
 * SCC (Tarjan), ponti/punti di articolazione, A*, betweenness (Brandes),
 * PageRank, max clique (Bron-Kerbosch), colorazione (DSATUR), chiusura transitiva.
 */
#ifndef GMBORTOOLS_GOR_GRAPH2_H
#define GMBORTOOLS_GOR_GRAPH2_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SCC (Tarjan): adj n×n (1=arco). comp[n]=componente. Ritorna #componenti. */
int gor_scc_tarjan(int n, const int *adj, int *comp);
/* Ponti (grafo non orientato, adj simmetrica). is_bridge[i*n+j]=1. Ritorna #ponti. */
int gor_bridges(int n, const int *adj, int *is_bridge);
/* Punti di articolazione: art[n]=1. Ritorna il numero. */
int gor_articulation(int n, const int *adj, int *art);

/* A*: w n×n (INF/assente), h[n] euristica ammissibile. path[n], *plen.
 * Ritorna il costo del cammino o -1 se irraggiungibile. */
gor_real gor_astar(int n, const gor_real *w, const gor_real *h, int s, int t, int *path, int *plen);

/* Betweenness (Brandes). Ritorna il valore massimo; bc[n] normalizzato. */
gor_real gor_betweenness(int n, const gor_real *w, gor_real *bc);
/* Closeness centrality (BFS non pesato): cc[n] = (n_reach-1)/sum_dist. */
void gor_closeness(int n, const gor_real *adj, gor_real *cc);
/* PageRank: adj n×n (pesi o 1). rank[n] normalizzato. */
void gor_pagerank(int n, const gor_real *adj, gor_real damping, int iters, gor_real *rank);

/* Max clique (Bron-Kerbosch con pivoting). clique[n], *size = dimensione. */
void gor_max_clique(int n, const int *adj, int *clique, int *size);
/* Colorazione DSATUR. color[n] = 0..k-1. Ritorna #colori. */
int gor_graph_coloring(int n, const int *adj, int *color);
/* Weisfeiler-Lehman (1-WL) color refinement (da SEAL/palette_wl): itera
 * label[v] = hash(label[v], multiset ordinato dei label dei vicini) fino a
 * stabilita'. colors[v] = classi compatte 0..k-1. Ritorna il numero di classi. */
int gor_graph_wl(int n, const int *adj, int *colors);
/* Min path cover di un DAG (da AirScheduling): numero minimo di cammini che
 * coprono tutti i nodi = n - massimo matching bipartito del DAG. adj n×n. */
int gor_min_path_cover(int n, const int *adj);
/* Double-Radius Node Labeling (DRNL, SEAL) per il link (u,v): distanze BFS da u
 * e v, label = 1 + min(dx,dy) + (d/2)((d/2)+(d%2)-1) con d=dx+dy.
 * label[n] (0 se irraggiungibile). Ritorna il numero di etichette distinte. */
int gor_graph_drnl(int n, const int *adj, int u, int v, int *label);
/* Chiusura transitiva (Warshall): adj -> closure (n×n, 0/1). */
void gor_transitive_closure(int n, const int *adj, int *closure);

/* Global min-cut (Stoer-Wagner). w n×n (pesi, simmetrica). Ritorna il valore. */
gor_real gor_stoer_wagner(int n, const gor_real *w);
/* K cammini minimi senza cicli (Yen): w n×n. paths[K*n], plens[K]. Ritorna K'. */
int gor_k_shortest_yen(int n, const gor_real *w, int s, int t, int K, int *paths, int *plens);
/* Isomorfismo di grafi (VF2). mapping[n1]=nodo di g2. Ritorna 1 se isomorfi. */
int gor_vf2_isomorphism(int n1, const int *adj1, int n2, const int *adj2, int *mapping);
/* Community detection (Louvain, un livello). comm[n] = comunità. Ritorna #comunità. */
int gor_louvain(int n, const gor_real *adj, int *comm);

/* BFS direction-optimizing (Beamer): top-down se il frontier è rado
 * (< thr*n), bottom-up se denso. adj n×n (1=arco). dist[n] = profondità (-1 se
 * non raggiunto). Ritorna il numero di nodi visitati. (da TurboBFS) */
int gor_bfs_direction_optimizing(int n, const int *adj, int src, gor_real thr, int *dist);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_GRAPH2_H */
