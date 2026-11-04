/*
 * gor_assign.h — assegnazione, matching e trasporto.
 *
 * Assegnazione lineare (Hungarian/Kuhn-Munkres, auction, bottleneck),
 * matching stabile (Gale-Shapley), trasporto (Vogel/MODI).
 * Base per fleet/tail/crew assignment e slot allocation.
 */
#ifndef GMBORTOOLS_GOR_ASSIGN_H
#define GMBORTOOLS_GOR_ASSIGN_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Assegnazione lineare somma (LSAP): cost n×n row-major.
 * assignment[i] = colonna assegnata alla riga i (-1 se non assegnata).
 * Ritorna il costo totale; n righe, m colonne (rettangolare: assegna min(n,m)). */
gor_real gor_assign_hungarian(int n, int m, const gor_real *cost, int *assignment);
/* Auction algorithm (Bertsekas) con epsilon di scaling. */
gor_real gor_assign_auction(int n, int m, const gor_real *cost, int *assignment, gor_real eps);
/* Bottleneck assignment: minimizza il costo massimo assegnato. */
gor_real gor_assign_bottleneck(int n, const gor_real *cost, int *assignment);

/* Matching stabile Gale-Shapley (n uomini, n donne).
 * pref_men[i*n+j] = j-esima scelta dell'uomo i; pref_women idem.
 * match_men[i] = donna assegnata all'uomo i. */
gor_status gor_stable_marriage(int n, const int *pref_men, const int *pref_women, int *match_men);

/* Trasporto: supply[m], demand[n], cost m×n -> x[m*n].
 * Ritorna il costo totale. */
gor_real gor_transport_vam(int m, int n, const gor_real *supply, const gor_real *demand,
                           const gor_real *cost, gor_real *x);
/* Soluzione iniziale di base col metodo dell'angolo nord-ovest (da Antsurf).
 * Ritorna il costo totale. */
gor_real gor_transport_northwest(int m, int n, const gor_real *supply,
                                 const gor_real *demand, const gor_real *cost,
                                 gor_real *x);
/* Soluzione iniziale col metodo del costo minimo (Least Cost, da Abinash).
 * Ritorna il costo totale. */
gor_real gor_transport_least_cost(int m, int n, const gor_real *supply,
                                  const gor_real *demand, const gor_real *cost,
                                  gor_real *x);
gor_real gor_transport_modi(int m, int n, const gor_real *supply, const gor_real *demand,
                            const gor_real *cost, gor_real *x, int maxit);

/* Assegnazione quadratica (QAP): euristica robust-tabu semplificata.
 * A e B n×n; p è la permutazione (0..n-1). Ritorna il costo. */
gor_real gor_assign_qap_tabu(int n, const gor_real *A, const gor_real *B, int *p,
                             int maxit, unsigned long seed);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_ASSIGN_H */
