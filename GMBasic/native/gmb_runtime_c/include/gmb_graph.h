/* ============================================================================
 * gmb_graph.h — Grafo orientato pesato (Graph<TNode> del C#).
 * Un registry di handle → grafo. Nodi rappresentati come stringhe (l'uso BASIC
 * è GRAPH<STRING>); archi orientati pesati. Implementa BFS/DFS/Dijkstra.
 * ============================================================================ */
#ifndef GMB_GRAPH_H
#define GMB_GRAPH_H

#include <stdint.h>
#include <stddef.h>
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Registry ---- */
void gmb_graph_init(void);
void gmb_graph_shutdown(void);

/* Crea un grafo vuoto. Ritorna handle (>0) o -1. */
int64_t gmb_graph_create(void);
void gmb_graph_free(int64_t h);

/* ---- Nodi/archi ---- */
/* Aggiunge un nodo. Ritorna -1 (vero) se nuovo, 0 se già esiste, -2 errore. */
int gmb_graph_add_node(int64_t h, const char* node);
/* F1 Incongruenze (§0bis): AddNodeConsistent — idempotente, -1 se il nodo esiste
 * dopo la chiamata (nuovo o già presente), 0 su errore. Condivisa VM/C-C99-C99Native. */
int gmb_graph_add_node_consistent(int64_t h, const char* node);
int gmb_graph_add_edge(int64_t h, const char* from, const char* to, double weight);
/* Come add_edge ma NON dedupplica (edge paralleli): RichGraph. */
int gmb_graph_add_edge_multi(int64_t h, const char* from, const char* to, double weight);
int gmb_graph_remove_node(int64_t h, const char* node);
int gmb_graph_remove_edge(int64_t h, const char* from, const char* to);
int gmb_graph_has_node(int64_t h, const char* node);
int gmb_graph_has_edge(int64_t h, const char* from, const char* to);
int gmb_graph_get_edge_weight(int64_t h, const char* from, const char* to, double* weight);
int gmb_graph_get_indegree(int64_t h, const char* node);
int gmb_graph_node_count(int64_t h);

/* ---- Attraversamento ---- */
/* Ritorna array di nodi (allocato) e len in out_len. Liberare con free(). */
char** gmb_graph_bfs(int64_t h, const char* start, int* out_len);
char** gmb_graph_dfs(int64_t h, const char* start, int* out_len);
/* Ritorna l'elenco di TUTTI i nodi. Liberare con free(). */
char** gmb_graph_nodes(int64_t h, int* out_len);
/* Vicini di un nodo: coppie (nodo, peso). out = array di {node, weight}. */
typedef struct { char* node; double weight; } gmb_graph_edge_t;
gmb_graph_edge_t* gmb_graph_neighbors(int64_t h, const char* node, int* out_len);

/* Dijkstra: ritorna distanze {nodo, distanza} per i nodi raggiungibili. */
typedef struct { char* node; double dist; } gmb_graph_dist_t;
gmb_graph_dist_t* gmb_graph_dijkstra(int64_t h, const char* start, int* out_len);

/* PageRank: ritorna {nodo, rank} per TUTTI i nodi, ordinati per rank decrescente.
 * damping = 0.85 tipico; iterations = numero di iterazioni (50-100). out_len = numero nodi.
 * Liberare l'array con free() (i nodi sono puntatori interni al grafo, non copiati). */
typedef struct { const char* node; double rank; } gmb_graph_pagerank_t;
gmb_graph_pagerank_t* gmb_graph_pagerank(int64_t h, double damping, int iterations, int* out_len);

/* ---- Algoritmi canonici (L5, parità con Graph<TNode> del C#) ---- */

/* Grado uscente / totale di un nodo (-1 se non esiste). */
int gmb_graph_get_outdegree(int64_t h, const char* node);
int gmb_graph_get_degree(int64_t h, const char* node);

/* Path più corto in NUMERO di archi (BFS) tra from e to. Ritorna [from,...,to] o NULL. */
char** gmb_graph_shortest_path(int64_t h, const char* from, const char* to, int* out_len);

/* Path a costo minimo (Dijkstra) tra from e to. Ritorna [from,...,to] o NULL. */
char** gmb_graph_dijkstra_path(int64_t h, const char* from, const char* to, int* out_len);

/* FindPath con strategia: "bfs" | "dijkstra" | "astar". Ritorna [from,...,to] o NULL. */
char** gmb_graph_find_path(int64_t h, const char* from, const char* to, const char* strategy, int* out_len);

/* Bellman-Ford: distanze minime da start (ammette pesi negativi). Ritorna {nodo, dist} per i
 * nodi raggiungibili; has_neg_cycle = -1 se c'è un ciclo negativo. */
typedef struct { char* node; double dist; } gmb_graph_bellman_t;
gmb_graph_bellman_t* gmb_graph_bellman_ford(int64_t h, const char* start, int* has_neg_cycle, int* out_len);

/* Componenti fortemente connesse (Tarjan). out = array di array di nodi, ognuno terminato da
 * NULL; out_count = numero di SCC. Liberare con gmb_graph_free_str_array_array(). */
char*** gmb_graph_scc(int64_t h, int* out_count);
void gmb_graph_free_str_array_array(char*** arr, int count);

/* True (-1) se il grafo contiene almeno un ciclo (DFS, 3 colori). */
int gmb_graph_has_cycle(int64_t h);

/* Ordinamento topologico (Kahn). Ritorna [nodi] o NULL se c'è un ciclo. */
char** gmb_graph_toposort(int64_t h, int* out_len);

/* True (-1) se il grafo è bipartito (2-colorazione, archi non orientati). */
int gmb_graph_is_bipartite(int64_t h);

/* Componenti (debolmente) connesse. out = array di {nodo, id_componente}; out_len = nodi. */
typedef struct { char* node; int component; } gmb_graph_comp_t;
gmb_graph_comp_t* gmb_graph_connected_components(int64_t h, int* out_len);

/* Betweenness centrality (Brandes, archi non pesati) per ogni nodo.
 * Ritorna {nodo, valore normalizzato}. Liberare con free(). */
typedef struct { char* node; double value; } gmb_graph_centrality_t;
gmb_graph_centrality_t* gmb_graph_betweenness(int64_t h, int* out_len);

/* Diametro (max distanza sui pesi, FloydWarshall). 0 se < 2 nodi. */
double gmb_graph_diameter(int64_t h);

/* Eccentricità di un nodo (max distanza verso i raggiungibili, in archi). -1 se non esiste. */
double gmb_graph_eccentricity(int64_t h, const char* node);

/* Coefficiente di clustering locale di un nodo (archi non orientati). */
double gmb_graph_clustering_coefficient(int64_t h, const char* node);

/* Edge list: tutte le coppie (from, to, weight). Ritorna array di {from,to,weight};
 * out_len = numero archi. Liberare con free(). */
typedef struct { char* from; char* to; double weight; long seq; } gmb_graph_edge_full_t;
gmb_graph_edge_full_t* gmb_graph_edge_list(int64_t h, int* out_len);

/* ---- G4: algoritmi aggiuntivi (MST / max-flow / community) ---- */

/* MST (Kruskal, non orientato; foreste minime se il grafo è disconnesso). Ritorna l'elenco
 * di archi {from,to,weight}; out_len = numero archi. Liberare con free(). */
gmb_graph_edge_full_t* gmb_graph_mst(int64_t h, int* out_len);

/* Peso totale del MST/Minimum Spanning Forest. */
double gmb_graph_mst_weight(int64_t h);

/* Max-flow (Edmonds-Karp) da source a sink. Capacità = pesi degli archi (paralleli sommati).
 * Ritorna il flusso (>=0), o -1.0 se source/sink non esistono. */
double gmb_graph_maxflow(int64_t h, const char* source, const char* sink);

/* Community detection (Louvain, grafo non orientato). resolution = 1.0 tipico.
 * Ritorna {nodo, community} per TUTTI i nodi; out_len = numero nodi. Liberare con free(). */
gmb_graph_comp_t* gmb_graph_louvain(int64_t h, double resolution, int* out_len);

/* ---- Val-boxed (C99Native/C99: i membri GRAPH che ritornano collezioni) ----
 * Un'unica sorgente (§0bis): costruiscono gmb_val_t (lista di stringhe per BFS/DFS/
 * Nodes; lista di dict {node,weight} per Neighbors; lista di dict {node,dist} per
 * Dijkstra) — lo stesso formato che il VM/C materializza come OBJ_LIST di OBJ_DICT.
 * Esempio GRAPH.bas: JSON.Stringify(g.Dijkstra("A")) → gmb_val_to_json(..._val). */
gmb_val_t gmb_graph_bfs_val(int64_t h, const char* start);
gmb_val_t gmb_graph_dfs_val(int64_t h, const char* start);
gmb_val_t gmb_graph_neighbors_val(int64_t h, const char* node);
gmb_val_t gmb_graph_dijkstra_val(int64_t h, const char* start);

#ifdef __cplusplus
}
#endif

#endif /* GMB_GRAPH_H */
