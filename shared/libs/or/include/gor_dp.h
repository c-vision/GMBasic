/*
 * gor_dp.h — programmazione dinamica classica (ricerca operativa).
 *
 * Albero binario di ricerca ottimo, sostituzione di attrezzature, serie
 * sportive al meglio di N. Problemi DP da Nelson6203/IO-Project.
 */
#ifndef GMBORTOOLS_GOR_DP_H
#define GMBORTOOLS_GOR_DP_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Albero binario di ricerca ottimo (optimal BST).
 * Chiavi 0..n-1 con probabilita' di ricerca p[k] >= 0 (p[0..n-1]).
 * root e' n*n row-major: root[i*n+j] = radice del sottoalbero [i,j] (-1 se
 * vuoto). Ritorna il costo atteso ottimo. */
gor_real gor_dp_optimal_bst(int n, const gor_real *p, int *root);

/* Sostituzione di attrezzature (equipment replacement).
 * Un macchinario nuovo costa buy_cost; invecchiando di a anni il costo
 * operativo annuo e' op_cost[a] e il valore di rivendita e' resale[a]
 * (a = 0..max_age-1). Orizzonte di years anni. schedule[y] (se non NULL,
 * years interi) riceve l'anno (0-based) di acquisto, -1 altrove; la prima
 * entrata e' sempre 0 (si parte con un macchinario nuovo).
 * Ritorna il costo totale minimo. */
gor_real gor_dp_equipment_replacement(int years, gor_real buy_cost, int max_age,
                                      const gor_real *op_cost,
                                      const gor_real *resale, int *schedule);

/* Serie "al meglio di n_games" (best-of): servono needed = n_games/2+1 vittorie.
 * p_home / p_away = probabilita' che A vinca in casa / in trasferta.
 * is_home[g] != 0 se A gioca in casa la partita g (g = 0..n_games-1).
 * Ritorna la probabilita' che A vinca la serie. */
gor_real gor_dp_sports_series(int n_games, int needed, gor_real p_home,
                              gor_real p_away, const int *is_home);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_DP_H */
