/*
 * gor_aviation.h — dominio aviation (airline).
 *
 * Revenue management (EMSR-b, overbooking, bid price), fleet/tail assignment,
 * crew pairing, slot allocation, irregular-ops recovery, ULD packing.
 * Riferimento: AiMathWiki 1998 "Operations Research in the Airline Industry".
 */
#ifndef GMBORTOOLS_GOR_AVIATION_H
#define GMBORTOOLS_GOR_AVIATION_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* EMSR-b: protezione per classi tariffarie (n classi, dalla più alta).
 * fares[n] (decrescente), demand_mean[n], demand_std[n], capacity.
 * protection[i] = posti da proteggere per le classi 0..i. Ritorna il booking limit. */
gor_real gor_av_emsr_b(int n, const gor_real *fares, const gor_real *demand_mean,
                       const gor_real *demand_std, gor_real capacity, gor_real *protection);

/* Overbooking (newsvendor): capacità C, tariffa fare, costo denied-boarding denied,
 * probabilità di presentarsi show_prob (0..1). Ritorna il livello di overbooking
 * (posti vendibili oltre C) e imposta *bookings al totale. */
gor_real gor_av_overbooking(gor_real capacity, gor_real fare, gor_real denied,
                            gor_real show_prob, gor_real show_std, gor_real *bookings);

/* Bid price (opportunità): costo marginale del posto sull'unica gamba data la domanda. */
gor_real gor_av_bid_price(gor_real capacity, gor_real demand, gor_real fare_low, gor_real fare_high);

/* Fleet assignment: profitto[flight*types], capacity[types], demand[flights],
 * avail[types] (numero aerei per tipo). assign[flights]=tipo. Ritorna il profitto. */
gor_real gor_av_fleet_assignment(int nf, int nt, const gor_real *profit, const gor_real *capacity,
                                 const gor_real *demand, const int *avail, int *assign);

/* Crew pairing: seleziona coppie (pairings) a costo minimo che coprono tutti i voli.
 * voli_in_pairing è n_flights × n_pairings (0/1). cost[n_pairings].
 * chosen[] = pairings scelti. Ritorna il costo. */
gor_real gor_av_crew_pairing(int nf, int np, const int *voli_in_pairing,
                             const gor_real *cost, int *chosen, int *n_chosen);

/* Tail assignment: assegna n aerei a n voli minimizzando il costo (Hungarian).
 * cost[aerei*voli]. assign[aerei]=volo. Ritorna il costo. */
gor_real gor_av_tail_assignment(int na, int nf, const gor_real *cost, int *assign);

/* Slot allocation: assegna n_slots a n_airlines massimizzando il valore delle offerte.
 * bid[airlines*slots]. alloc[airlines]=slot. Ritorna il valore totale. */
gor_real gor_av_slot_allocation(int na, int ns, const gor_real *bid, int *alloc);

/* Irregular ops recovery: riassegna gli aerei ai voli (con costi di ritardo).
 * delay_cost[flight*aircraft]. Ritorna il costo totale; assign aggiornato. */
gor_real gor_av_recovery(int nf, int na, const gor_real *delay_cost, int *assign);

/* ULD packing: carica n colli (items 3×: w,h,d) in ULD di dimensioni W,H,D.
 * Ritorna il numero di ULD; uld_of[i] = ULD del collo i. */
int gor_av_uld_packing(int n, const gor_real *items, gor_real W, gor_real H, gor_real D,
                       int *uld_of);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_AVIATION_H */
