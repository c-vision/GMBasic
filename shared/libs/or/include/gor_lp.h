/*
 * gor_lp.h — PL/PLI: LP (simplesso) e MILP (branch & bound + tagli di Gomory).
 *
 * Forma standard: min c'x  s.t.  A x = b,  x >= 0,  b >= 0.
 * Variabili intere indicate da int_idx; il rilassamento LP usa il simplesso
 * importato (simplex_solve_std).
 */
#ifndef GMBORTOOLS_GOR_LP_H
#define GMBORTOOLS_GOR_LP_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LP: min c'x s.t. A x=b, x>=0, b>=0. Ritorna 0 ottimo, 1 inammissibile,
 * 2 illimitato. x[n], y[m] (duali), *obj. */
int gor_lp_solve(int m, int n, const double *A, const double *b, const double *c,
                 double *x, double *y, double *obj, int max_iter);

/* Revised simplex: min c'x s.t. Ax=b, x>=0, dalla base primal-ammissibile
 * basis[m]. Mantiene B^-1 e i moltiplicatori y=c_B^T B^-1; aggiornamento eta.
 * Ritorna 0 ottimo, 1 base inammissibile, 2 illimitato, 3 max iter, 4 memoria.
 * x[n]. (da athityakumar/or_lab) */
int gor_lp_revised_simplex(int m, int n, const double *A, const double *b,
                           const double *c, const int *basis, int max_iter, double *x);

/* LP a bound bilaterali (forma generale, da FOSSEE-ECOS-OR Q3):
 *   min c'x  s.t.  s <= A x <= b,  l <= x <= u   (A m×n row-major)
 * I bound possono essere +-HUGE_VAL (infinito). Ritorna 0 ottimo, 1 inammissibile,
 * 2 illimitato, -1 errore. x[n]. */
int gor_lp_solve_bounds(int m, int n, const double *A, const double *s,
                        const double *b, const double *l, const double *u,
                        const double *c, double *x);

/* Dual simplex: min c'x s.t. Ax=b, x>=0, partendo da una base dual-ammissibile
 * basis[m] (indici di colonna). b puo' avere componenti negative.
 * Ritorna 0 ottimo, 1 inammissibile, 2 illimitato, 3 max iter, 4 memoria.
 * x[n] = soluzione ottima (se 0). (da apurvasijaria/Operation_Research_Lab) */
int gor_lp_dual_simplex(int m, int n, const double *A, const double *b,
                        const double *c, const int *basis, int max_iter, double *x);

/* Costi ridotti di un LP: red[j] = c[j] - sum_i y[i]*A[i][j], dati i duali y.
 * (da Tanches94Q/...BranchAndPrice, column generation) */
void gor_lp_reduced_costs(int m, int n, const double *A, const double *c,
                          const double *y, double *red);

/* Ottimo alternativo: dato l'LP min c'x s.t. Ax=b, x>=0, b>=0, risolve e cerca
 * un altro vertice ottimo (colonna non-base a costo ridotto nullo nel tableau).
 * x2[n] riceve l'ottimo alternativo, *z2 il suo valore.
 * Ritorna 1 se ne esiste uno distinto, 0 se l'ottimo è unico, -1 su errore.
 * (da randymorales/Project4IO-Simplex, search_another_optimal_solution) */
int gor_lp_alt_optimum(int m, int n, const double *A, const double *b,
                       const double *c, double *x2, double *z2);

/* MILP: come sopra ma x[j] intero per j in int_idx[0..nint-1].
 * Branch & bound (depth-first) con incumbent. maxnodes limita i nodi.
 * Ritorna 0 ottimo, 1 inammissibile, 2 illimitato, 3 nodi esauriti. */
int gor_milp_bb(int m, int n, const double *A, const double *b, const double *c,
                int nint, const int *int_idx, double *x, double *obj,
                int maxnodes, double tol);

/* Matheuristics (da OperationsResearch2).
 * Hard fixing: risolve il MILP, fissa una frazione fix_frac delle variabili
 * intere ai valori correnti, ri-risolve; ripete per `rounds`. Aggiorna x/obj
 * col migliore. Ritorna lo status del milp (0 = ottimo trovato). */
int gor_matheuristic_hard_fixing(int m, int n, const double *A, const double *b,
                                 const double *c, int nint, const int *int_idx,
                                 double fix_frac, int rounds, double *x, double *obj,
                                 int maxnodes, double tol, unsigned long long seed);
/* Local branching: aggiunge il vincolo di distanza di Hamming <= k dalla
 * soluzione corrente e ri-risolve; k cresce se non migliora. */
int gor_matheuristic_local_branching(int m, int n, const double *A, const double *b,
                                     const double *c, int nint, const int *int_idx,
                                     int k_start, int rounds, double *x, double *obj,
                                     int maxnodes, double tol);

/* Gomory mixed-integer cut dalla riga del tableau (esposta per uso avanzato). */
int gor_gomory_cut(int ncols, const double *tabrow, int rhs_col, double *cut);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_LP_H */
