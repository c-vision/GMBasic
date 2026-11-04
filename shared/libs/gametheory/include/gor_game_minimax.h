/*
 * gm_game.h — teoria dei giochi (riscrittura).
 */
#ifndef GMBORTOOLS_GOR_GAME_MINIMAX_H
#define GMBORTOOLS_GOR_GAME_MINIMAX_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Giochi a somma zero: valore e strategie miste (m righe, n colonne). */
gor_status gor_game_minimax(const gor_real *A, int m, int n,
                          gor_real *value, gor_real *p, gor_real *q);
/* Nash per giochi bimatrice (A per il giocatore 1, B per il 2). */
gor_status gor_nash_2p(const gor_real *A, const gor_real *B, int m, int n,
                     gor_real *p, gor_real *q, gor_real tol);
/* Valore di Shapley per un gioco cooperativo v(mask). */
gor_status gor_shapley(int n, gor_real (*v)(unsigned int, void *), void *ctx, gor_real *phi);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_GAME_MINIMAX_H */
