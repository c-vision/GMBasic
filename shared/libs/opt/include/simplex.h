/* simplex.h - two-phase primal simplex on the standard form
 *   min c'x  s.t.  A x = b, x >= 0, b >= 0
 * statuses: 0 optimal, 1 infeasible (Farkas witness in dray),
 *           2 unbounded (recession direction in pray),
 *           3 max iterations, 4 out of memory.
 * dray (m) and pray (n) may be NULL and are written only by the status that
 * owns them, so on any other status they hold nothing the caller may read.
 * dray is unnormalized (y'A <= 0, y'b > 0); pray is normalized to 1 in the
 * entering column (rho >= 0, A rho = 0, c'rho < 0). */
#ifndef SIMPLEX_H
#define SIMPLEX_H

int simplex_solve_std(const double *A, int m, int n,
                      const double *b, const double *c,
                      int max_iter, double *x /*n*/, double *y /*m*/,
                      double *dray /*m, status 1*/, double *pray /*n, status 2*/);
/* Come sopra, ma restituisce anche la base finale (`basis_out`, m indici di
 * colonna) e il tableau (`tab_out`, m righe di stride n+m+1: coefficienti
 * ridotti nelle prime n+m colonne, RHS in [n+m]). Servono ai tagli di Gomory.
 * Entrambi opzionali (NULL). */
int simplex_solve_std_tab(const double *A, int m, int n,
                          const double *b, const double *c,
                          int max_iter, double *x, double *y,
                          double *dray, double *pray,
                          int *basis_out, double *tab_out);

/* Crash basis: m colonne di A linearmente indipendenti (eliminazione di Gauss
 * con pivoting di colonna). Ritorna 1 e riempie `basis[m]` (indici di colonna)
 * se rank(A) == m, 0 altrimenti. Serve a dare al simplesso revised una base
 * valida senza fase 1. */
int simplex_crash_basis(const double *A, int m, int n, int *basis /*m*/);

#endif /* SIMPLEX_H */
