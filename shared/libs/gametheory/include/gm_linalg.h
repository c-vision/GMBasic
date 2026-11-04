/* gm_linalg.h — sottoinsieme minimo per il pacchetto gmbgametheory.
 * Solo `gm_linalg_solve` (usato da gor_game_minimax.c per Nash/Shapley):
 * l'implementazione locale evita di trascinare gmbmath/gmbcompute nel pacchetto. */
#ifndef GMMATH_GM_LINALG_H
#define GMMATH_GM_LINALG_H
#ifdef __cplusplus
extern "C" {
#endif
typedef double gm_real;
typedef enum gm_status { GM_OK = 0, GM_ERR_ARG = 1, GM_ERR_NO_MEM = 2, GM_ERR_SINGULAR = 3 } gm_status;
gm_status gm_linalg_solve(int n, const gm_real *A, const gm_real *b, gm_real *x);
#ifdef __cplusplus
}
#endif
#endif
