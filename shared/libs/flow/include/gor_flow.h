/*
 * gor_flow.h — flussi di rete: max-flow, min-cut.
 *
 * Capacità come grafo denso (gor_graph) con pesi >= 0. Sorgente `s`, pozzo `t`.
 */
#ifndef GMBORTOOLS_GOR_FLOW_H
#define GMBORTOOLS_GOR_FLOW_H

#include "gor_core.h"
#include "gor_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Max-flow con Edmonds-Karp (BFS augmenting).
 * `value` riceve il valore del flusso. `flow` (capacità n*n, può essere NULL)
 * riceve il flusso per arco. */
gor_status gor_max_flow(const gor_graph *g, int s, int t, gor_real *value, gor_real *flow);

/* Min-cut: partiziona i nodi in S (side=0) e T (side=1) dal taglio minimo.
 * `side` ha capacità n. Calcola internamente il max-flow. */
gor_status gor_min_cut(const gor_graph *g, int s, int t, int *side);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_FLOW_H */
