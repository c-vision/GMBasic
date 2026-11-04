/*
 * gor_cover.h — covering, localizzazione, clustering OR.
 *
 * Set cover (greedy, LP), max coverage, facility location, p-median, p-center,
 * vertex cover. Base per crew scheduling, network design, hub location.
 */
#ifndef GMBORTOOLS_GOR_COVER_H
#define GMBORTOOLS_GOR_COVER_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Set cover greedy: elem_in_set è m×n (elemento i in insieme j = 1).
 * chosen[] = indici degli insiemi scelti; *n_chosen aggiornato. Ritorna il costo. */
gor_real gor_set_cover_greedy(int m, int n, const int *elem_in_set,
                              const gor_real *cost, int *chosen, int *n_chosen);
/* Set covering "soft" (da IA2_SA, crew scheduling): costo = somma dei costi
 * degli insiemi scelti + pen * (numero di elementi non coperti).
 * chosen[n] booleano. Ritorna il costo; *uncovered (se non NULL) riceve il
 * numero di elementi non coperti. */
gor_real gor_set_cover_soft_cost(int m, int n, const int *elem_in_set,
                                 const gor_real *cost, const char *chosen,
                                 gor_real uncovered_pen, int *uncovered);
/* Pricing greedy per set covering (da branch-and-price): dati i duali y[m],
 * costruisce una nuova colonna (insieme) selezionando greedy gli elementi con
 * contributo y_i > 0, con costo = costo_base + costo_per_elemento per elemento.
 * chosen_set[m] riceve 1 per gli elementi inclusi. Ritorna il costo ridotto
 * della colonna (negativo = utile per la master). */
gor_real gor_set_cover_pricing_greedy(int m, const gor_real *y, gor_real base_cost,
                                      gor_real cost_per_elem, char *chosen_set);

/* Simulated annealing per set covering "soft" (vicinato bit-flip: aggiungi/
 * rimuovi un insieme). chosen[n] inizializzato/aggiornato; ritorna il costo. */
gor_real gor_set_cover_sa(int m, int n, const int *elem_in_set, const gor_real *cost,
                          gor_real uncovered_pen, int maxit, gor_real t0, gor_real alpha,
                          unsigned long long seed, char *chosen);

/* Local search per set cover (0-1 exchange): rimuove insiemi ridondanti e prova
 * scambi 1-1 con insiemi piu' economici. chosen[]/n_chosen aggiornati in place.
 * Ritorna il costo migliorato. (da seydou-coulibaly/Set_covering_problem) */
gor_real gor_set_cover_local_search(int m, int n, const int *elem_in_set,
                                    const gor_real *cost, int *chosen, int *n_chosen,
                                    int maxit);
/* GRASP per set cover: greedy randomizzato (RCL con parametro alpha) + local
 * search, maxit ristart. Ritorna il miglior costo; chosen[]/n_chosen aggiornati. */
gor_real gor_set_cover_grasp(int m, int n, const int *elem_in_set,
                             const gor_real *cost, gor_real alpha, int maxit,
                             unsigned long long seed, int *chosen, int *n_chosen);
/* Reactive GRASP: adatta le probabilita' di scelta di alpha (alphas[nalpha]) in
 * base al successo. Ritorna il miglior costo. */
gor_real gor_set_cover_reactive_grasp(int m, int n, const int *elem_in_set,
                                      const gor_real *cost, const gor_real *alphas,
                                      int nalpha, int maxit, unsigned long long seed,
                                      int *chosen, int *n_chosen);

/* Max coverage: sceglie k insiemi che coprono più elementi. Ritorna la copertura. */
int gor_max_coverage(int m, int n, const int *elem_in_set, int k, int *chosen, int *n_chosen);

/* p-median: dist n×n, sceglie p medoidi (greedy + local search).
 * medians[p] = indici; assign[n] = medoide assegnato. Ritorna il costo. */
gor_real gor_pmedian(int n, int p, const gor_real *dist, int *medians, int *assign, int maxit);
/* p-center: 2-approssimazione di Gonzalez. Ritorna il raggio. */
gor_real gor_pcenter(int n, int p, const gor_real *dist, int *medians);

/* Vertex cover 2-approssimazione (matching massimale). edge list 2*ne. Ritorna la size. */
int gor_vertex_cover_2approx(int n, const int *edges, int ne, int *cover);
/* Hitting set greedy: insiemi = elementi da colpire; ritorna la size. */
int gor_hitting_set_greedy(int m, int n, const int *elem_in_set, int *chosen, int *n_chosen);

/* Facility location non capacitato (greedy): open_cost[n], conn_cost[m*n].
 * open[j] = 1 se aperto. Ritorna il costo totale. */
gor_real gor_facility_location_greedy(int m, int n, const gor_real *open_cost,
                                      const gor_real *conn_cost, int *open);

/* k-center capacitato / clustering: assegna n punti (dim) a k centroidi (k-means++/Lloyd). */
gor_real gor_kmeans(int n, int dim, int k, const gor_real *points, gor_real *centroids,
                    int *labels, int maxit, unsigned long seed);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_COVER_H */
