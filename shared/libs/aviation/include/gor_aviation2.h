/*
 * gor_aviation2.h — aviation avanzato.
 *
 * Aircraft routing (rotazioni), crew rescheduling (recupero), hub location
 * (p-hub median), network design (Steiner), tail continuity.
 */
#ifndef GMBORTOOLS_GOR_AVIATION2_H
#define GMBORTOOLS_GOR_AVIATION2_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Aircraft routing / tail assignment: flights[nf*4] = (dep, arr, origin, dest).
 * Costruisce rotazioni con turno minimo `min_turn` (minuti) e stesso aeroporto.
 * route_of[i] = rotta (0..k-1). Ritorna il numero di rotte. */
int gor_av_aircraft_routing(int nf, const double *flights, double min_turn, int *route_of);

/* Crew rescheduling: riassegna ncrew crew a nf voli. cost[crew*nf] (costo di
 * assegnare crew i al volo j). available[crew]=1 se disponibile. assign[crew]=volo
 * o -1. Ritorna il costo totale (minimo). */
double gor_av_crew_rescheduling(int ncrew, int nf, const double *cost,
                                const int *available, int *assign);

/* Hub location (p-hub median): dist n×n, flow n×n, scegli p hub.
 * hubs[p] = indici. Ritorna il costo totale di trasporto. */
double gor_av_hub_location(int n, int p, const double *dist, const double *flow,
                           double alpha, int *hubs);

/* Network design: Steiner tree approssimato (2-approssimazione via MST della
 * chiusura metrica + shortest path). w n×n (INF assente), terminals[nt].
 * in_tree[i*n+j]=1 per gli archi usati. Ritorna il costo. */
double gor_av_network_steiner(int n, const double *w, const int *terminals, int nt, int *in_tree);

/* Tail continuity: assegna n aircraft a nf flights minimizzando il costo di
 * discontinuità (aerei che cambiano base). cost[aircraft*nf]. assign[aircraft]. */
double gor_av_tail_continuity(int na, int nf, const double *cost, int *assign);

/* Column generation (crew pairing / aircraft routing): set cover con generazione
 * di colonne. voli_in_pairing nf×np, cost[np]. chosen[np]=1 per le colonne scelte.
 * Ritorna il costo; *n_chosen aggiornato. */
double gor_av_column_generation(int nf, int np, const int *voli_in_pairing,
                                const double *cost, int *chosen, int *n_chosen, int maxiter);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_AVIATION2_H */
