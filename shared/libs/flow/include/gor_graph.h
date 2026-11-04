/*
 * gor_graph.h — grafi: percorsi minimi, MST, connettività, topologico.
 *
 * Rappresentazione: matrice di adiacenza densa n x n (row-major).
 * Peso GOR_INF (INFINITY) = arco assente. I pesi possono essere negativi
 * (Bellman-Ford / Floyd-Warshall); Dijkstra richiede pesi non negativi.
 */
#ifndef GMBORTOOLS_GOR_GRAPH_H
#define GMBORTOOLS_GOR_GRAPH_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Arco con peso (usato da MST e algoritmi su liste di archi). */
typedef struct gor_edge {
    int u;
    int v;
    gor_real w;
} gor_edge;

/* Grafo denso. `w` è n*n row-major; `directed` != 0 per grafo orientato. */
typedef struct gor_graph {
    int n;
    int directed;
    gor_real *w;
} gor_graph;

gor_status gor_graph_init(gor_graph *g, int n, int directed);
void gor_graph_free(gor_graph *g);
void gor_graph_set_edge(gor_graph *g, int u, int v, gor_real weight);
int gor_graph_has_edge(const gor_graph *g, int u, int v);
gor_real gor_graph_edge(const gor_graph *g, int u, int v);
int gor_graph_degree(const gor_graph *g, int u);

/* Union-find (insiemi disgiunti) con union by rank + path compression. */
typedef struct gor_uf {
    int n;
    int *parent;
    int *rank;
} gor_uf;

gor_status gor_uf_init(gor_uf *uf, int n);
void gor_uf_free(gor_uf *uf);
int gor_uf_find(gor_uf *uf, int x);
int gor_uf_union(gor_uf *uf, int a, int b);

/* Visite. `order` ha capacità n; `count` riceve il numero di nodi visitati. */
gor_status gor_bfs(const gor_graph *g, int src, int *order, int *count);
gor_status gor_dfs(const gor_graph *g, int src, int *order, int *count);
gor_status gor_connected_components(const gor_graph *g, int *comp, int *ncomp);
gor_status gor_topological_sort(const gor_graph *g, int *order);

/* Percorsi minimi. dist/prev hanno capacità n (prev con -1 = nessun padre). */
gor_status gor_dijkstra(const gor_graph *g, int src, gor_real *dist, int *prev);
gor_status gor_bellman_ford(const gor_graph *g, int src, gor_real *dist, int *prev);
gor_status gor_floyd_warshall(const gor_graph *g, gor_real *dist);

/* Ricostruisce il cammino da src a dst da `prev`. Ritorna 1 se raggiungibile.
 * `out` ha capacità n; `out_len` riceve il numero di nodi (inclusi src e dst). */
int gor_path_reconstruct(const int *prev, int src, int n, int dst, int *out, int *out_len);

/* Minimum spanning tree. `edges` ha capacità n-1; ritorna il numero di archi. */
gor_status gor_mst_kruskal(const gor_graph *g, gor_edge *edges, int *n_edges, gor_real *total);
gor_status gor_mst_prim(const gor_graph *g, gor_edge *edges, int *n_edges, gor_real *total);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_GRAPH_H */
