/*
 * gor_game.h — teoria dei giochi a somma zero (2 giocatori).
 *
 * Saddle point puro (da diptanilsaha/operation_research) e valore del gioco a
 * strategie miste risolto come LP (maximin = minimax, von Neumann).
 */
#ifndef GMBORTOOLS_GOR_GAME_H
#define GMBORTOOLS_GOR_GAME_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Gioco a somma zero con matrice dei pagamenti payoff (m righe = strategie del
 * giocatore di riga, n colonne = colonne), row-major m*n.
 * Saddle point in strategie PURE: esiste se max delle minime di riga == min
 * delle massime di colonna. Ritorna 1 se esiste (value = valore del gioco,
 * row_idx/col_idx = posizione), 0 altrimenti (value = maximin). */
int gor_game_saddle_point(int m, int n, const gor_real *payoff,
                          gor_real *value, int *row_idx, int *col_idx);

/* Valore del gioco a strategie MISTE (von Neumann) via LP.
 * row_strat[m], col_strat[n] ricevono le strategie ottime (somma 1, >= 0).
 * Ritorna il valore del gioco; NaN se l'LP fallisce. */
gor_real gor_game_value_lp(int m, int n, const gor_real *payoff,
                           gor_real *row_strat, gor_real *col_strat);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_GAME_H */
