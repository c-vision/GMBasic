/* gmb_pkg_graph.h — grafo dipendenze (per `why`) + peer mancanti (riferimento C del PM). */
#ifndef GMB_PKG_GRAPH_H
#define GMB_PKG_GRAPH_H

#include "gmb_runtime.h"

typedef struct gmb_pkg_tree gmb_pkg_tree_t;

/* Costruisce l'albero (parent → figli diretti: dependencies + features default). */
gmb_pkg_tree_t* gmb_pkg_dep_tree(int n, const char* const* names, const char* const* install_dirs);
int gmb_pkg_tree_count(gmb_pkg_tree_t* t);
int gmb_pkg_tree_row(gmb_pkg_tree_t* t, int i, char* parent, size_t pcap, char* children_csv, size_t ccap);
void gmb_pkg_tree_free(gmb_pkg_tree_t* t);

/* Peer dependency mancanti: una riga ("<pkg> richiede il peer '<peer>' <range>") per ciascuno.
   Ritorna il numero di peer mancanti; scrive le righe (separate da '\n') in out_csv. */
int gmb_pkg_check_peers(int n, const char* const* names, const char* const* install_dirs, char* out_csv, size_t cap);

#endif /* GMB_PKG_GRAPH_H */
