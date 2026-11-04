/*
 * gor_cp.h — constraint programming e SAT.
 *
 * SAT: DPLL con unit propagation. CP: all-different (backtracking), forward
 * checking su vincoli binari, ricerca con MRV.
 */
#ifndef GMBORTOOLS_GOR_CP_H
#define GMBORTOOLS_GOR_CP_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SAT: nvars variabili (1..nvars). Clausole in formato flat:
 * clauses[] = letterali concatenati; clause_start[k], clause_len[k].
 * assignment[nvars] = 0/1. Ritorna 1 se soddisfacibile, 0 altrimenti. */
int gor_sat_dpll(int nvars, int nclauses, const int *clauses,
                 const int *clause_start, const int *clause_len, int *assignment);

/* All-different: n variabili, ciascuna con dominio dom[i*D .. i*D+D-1].
 * Ritorna 1 se esiste un'assegnazione a valori distinti; sol[n] = valori. */
int gor_cp_alldiff(int n, int D, const int *dom, int *sol);

/* CSP binario: n variabili, domini dom[i*D..], vincolo binario allowed[i][j]
 * (n*n, 1 = coppia ammessa). Ritorna 1 se esiste soluzione; sol[n]. */
int gor_cp_binary(int n, int D, const int *dom, const int *allowed, int *sol);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_CP_H */
