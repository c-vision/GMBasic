/*
 * opt_linalg.h — algebra minima per il pacchetto `opt` (nessuna dipendenza esterna).
 * Sostituisce gm_linalg.h di gmbmath nella copia pacchetto: serve solo a
 * gor_newton_damped/gor_dogleg (non usati dalle 34 funzioni, ma referenziati
 * da gmi_opt.c) e a gmi_optfit.c (minimi quadrati).
 */
#ifndef OPT_LINALG_H
#define OPT_LINALG_H

typedef int gm_status;
typedef double gm_real;

#define GM_OK 0
#define GM_ERR_ARG 1
#define GM_ERR_SINGULAR 2

/* Risolve A x = b (A n×n row-major) con LU e pivoting parziale. */
int gm_linalg_solve(int n, const gm_real *A, const gm_real *b, gm_real *x);

/* Cholesky A x = b (A simmetrica definita positiva). 1 = ok, 0 = non SPD. */
int gmi_cholesky_solve(const gm_real *A, const gm_real *b, int n, gm_real *x);

#endif /* OPT_LINALG_H */
