/*
 * gor_routing.h — routing: TSP e VRP.
 *
 * TSP: nearest neighbor, 2-opt/Or-opt, double-tree (2-approx), Held-Karp esatto.
 * VRP: Clarke-Wright savings, sweep. Base per aircraft/crew routing.
 */
#ifndef GMBORTOOLS_GOR_ROUTING_H
#define GMBORTOOLS_GOR_ROUTING_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TSP simmetrico: dist n×n. tour[n] = ordine di visita. Ritorna la lunghezza. */
gor_real gor_tsp_nn(int n, const gor_real *dist, int *tour);
gor_real gor_tsp_2opt(int n, const gor_real *dist, int *tour, int maxit);
/* Costo di un tour (ciclo, ritorno al punto di partenza). */
gor_real gor_tsp_tour_cost(int n, const gor_real *dist, const int *tour);
/* Euristica costruttiva Extra-Mileage (da two-tsp): triangolo iniziale +
 * inserimento del nodo con il minimo aumento di costo. */
gor_real gor_tsp_extra_mileage(int n, const gor_real *dist, int *tour);
/* GRASP: greedy randomizzato (RCL) + 2-opt, tiene il migliore. alpha in (0,1]. */
gor_real gor_tsp_grasp(int n, const gor_real *dist, int *tour, int maxit,
                       gor_real alpha, unsigned long long seed);
/* VNS: ricerca a intorni variabili (2-opt / Or-opt / double-bridge). */
gor_real gor_tsp_vns(int n, const gor_real *dist, int *tour, int maxit);
/* Local search 3-opt (da Operations_research_2): riorganizza 3 segmenti del
 * tour nelle 7 riconnessioni non banali finche' migliora. */
gor_real gor_tsp_3opt(int n, const gor_real *dist, int *tour, int maxit);
/* Simulated annealing per TSP (mossa 2-opt; da THEBELLIN): parte dal tour
 * dato, T0 temperatura iniziale, cooling fattore, iters per temperatura.
 * Aggiorna tour col migliore e ritorna il costo. */
gor_real gor_tsp_sa(int n, const gor_real *dist, int *tour, gor_real T0,
                    gor_real cooling, int iters, unsigned long long seed);
/* Tabu search per TSP (mossa 2-opt, lista tabu sulle coppie di citta').
 * tenure_kind: 0=costante,1=triangolare,2=quadrata,3=sawtooth (gor_tabu_tenure).
 * Aspiration: una mossa tabu e' ammessa se migliora il best. */
gor_real gor_tsp_tabu(int n, const gor_real *dist, int *tour, int maxit,
                      int tenure_kind, unsigned long long seed);

/* Minimum 1-tree (Held-Karp): MST sui nodi escluso root + i due archi piu' economici
 * incidenti a root. dist n×n. Ritorna il peso; edges[2n] (se non NULL) le coppie
 * (i,j), -1 se assente. (da albertofranzin/ro2tsp) */
gor_real gor_tsp_one_tree(int n, const gor_real *dist, int root, int *edges);

/* Lower bound di Held-Karp via rilassamento lagrangiano sui 1-tree con
 * subgradiente: costi modificati dist_ij + pi_i + pi_j, g_i = grado_i - 2.
 * maxit iterazioni. Ritorna il miglior lower bound; pi[n] (se non NULL) i
 * moltiplicatori finali. */
gor_real gor_tsp_hk_bound(int n, const gor_real *dist, int maxit, gor_real *pi);

/* Sottotour di una permutazione successore succ[n] (succ[i] = nodo dopo i).
 * comp[i] = id del ciclo di i; sizes[c] = dimensione del ciclo c (se non NULL).
 * Ritorna il numero di sottotour (cicli), -1 su errore. (da Giraudux/operational-research) */
int gor_tsp_subtours(int n, const int *succ, int *comp, int *sizes);

/* TSP esatto via MIP (DFJ): rilassamento di assegnazione + eliminazione iterativa
 * dei sottotour (tagli SEC). dist n×n, tour[n] = ciclo hamiltoniano ottimo.
 * maxcuts = max tagli aggiunti. Ritorna il costo ottimo, -1 su errore. */
gor_real gor_tsp_dfj(int n, const gor_real *dist, int *tour, int maxcuts);

/* Edge recombination crossover (ERX, Whitley) per TSP: ricombina i due tour
 * genitori assemblando archi (rappresentazione a successori, da SiMoM0/TSP).
 * child[n] riceve un tour valido. */
void gor_tsp_erx(int n, const int *p1, const int *p2, int *child,
                 unsigned long long seed);

/* Algoritmo genetico a permutazioni per TSP (da THEBELLIN): popolazione,
 * generazioni, crossover a taglio + mutazione a scambio + elitismo. */
gor_real gor_tsp_ga(int n, const gor_real *dist, int *tour, int pop, int gens,
                    unsigned long long seed);
gor_real gor_tsp_or_opt(int n, const gor_real *dist, int *tour, int maxit);
/* Double-tree: 2-approssimazione (MST + preorder). */
gor_real gor_tsp_double_tree(int n, const gor_real *dist, int *tour);
/* Held-Karp esatto (DP su sottoinsiemi), n <= 20. */
gor_real gor_tsp_held_karp(int n, const gor_real *dist, int *tour);

/* CVRP (Clarke-Wright savings): dist (n+1)×(n+1) col deposito all'indice `depot`,
 * demand[n], capacità `cap`. route_of[i] = indice rotta del cliente i.
 * Ritorna il numero di rotte; *total (se non NULL) = distanza totale. */
int gor_vrp_clarke_wright(int n, const gor_real *dist, const gor_real *demand, gor_real cap,
                          int depot, int *route_of, gor_real *total);
/* VRPTW (Clarke-Wright + finestre temporali): ready[n], due[n], service[n].
 * Ritorna #rotte; route_of[i]; *total (distanza). */
int gor_vrp_time_windows(int n, const gor_real *dist, const gor_real *demand, gor_real cap,
                         int depot, const gor_real *ready, const gor_real *due,
                         const gor_real *service, int *route_of, gor_real *total);
/* MDVRP (multi-depot): ndepots depositi agli indici 0..ndepots-1, clienti dopo.
 * Assegna i clienti al deposito più vicino, poi CVRP per deposito.
 * route_of[i] = deposito/rotta; *total distanza. Ritorna #rotte. */
int gor_mdvrp(int n, int ndepots, const gor_real *dist, const gor_real *demand, gor_real cap,
              int *route_of, gor_real *total);
/* PDP (pickup & delivery): pickup[i]/delivery[i] = coppia richiesta i (0/1).
 * Inserimento greedy con precedenza e capacità. route[n] = sequenza. Ritorna #rotte. */
int gor_pdp(int nreq, const gor_real *dist, const gor_real *demand, gor_real cap,
            const int *pickup, const int *delivery, int *route, gor_real *total);
/* Arc routing (Chinese postman): w n×n pesi (0=assente). euler[] = vertici del
 * circuito; *elen = lunghezza. Ritorna il costo totale del circuito. */
gor_real gor_arc_routing(int n, const gor_real *w, int *euler, int *elen);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_ROUTING_H */
