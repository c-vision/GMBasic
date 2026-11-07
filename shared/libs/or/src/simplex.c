/* simplex.c - dense two-phase primal simplex
 *
 * Tableau layout: (m+1) x (n+m+1): rows 0..m-1 constraints, row m = reduced
 * cost row; columns 0..n-1 structural (incl. slack columns of the std form),
 * n..n+m-1 artificials, column n+m = rhs. Cost row holds (red | -z).
 *
 * Phase 1: artificials form the initial basis, cost 1 each; entering columns
 * restricted to structural ones. Termination with z1 > 0 proves infeasibility
 * (Farkas certificate: y = c_B B^-1, y'A <= 0, y'b = z1 > 0).
 * Artificials leaving the basis are driven out (pivot on any nonzero entry);
 * rows where this is impossible are redundant (artificial stays basic at 0).
 *
 * Phase 2: cost row rebuilt exactly from c for the current basis. Anti
 * cycling: Dantzig rule with fallback to Bland's rule after stalling.
 * Duals: y_std[r] = -costrow[n+r] (phase-2 artificial cost is 0).
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "simplex.h"

#define PIV_EPS     1e-9
#define RED_EPS     1e-9
#define STALL_LIMIT 40

enum { SIMP_OPTIMAL = 0, SIMP_INFEASIBLE, SIMP_UNBOUNDED, SIMP_MAXITER, SIMP_MEMORY };

typedef struct {
    int m, n, ntot, stride;
    double *T;
    int *basis;
} Tab;

static void tab_pivot(Tab *t, int lev, int ent) {
    int stride = t->stride, ntot = t->ntot;
    double *T = t->T;
    double piv = T[lev * stride + ent];
    double *rl = T + lev * stride;
    for (int j = 0; j <= ntot; j++) rl[j] /= piv;
    rl[ent] = 1.0;
    for (int i = 0; i <= t->m; i++) {
        if (i == lev) continue;
        double *ri = T + i * stride;
        double f = ri[ent];
        if (f == 0.0) continue;
        for (int j = 0; j <= ntot; j++) ri[j] -= f * rl[j];
        ri[ent] = 0.0;
    }
    t->basis[lev] = ent;
}

static int enter_col(const double *red, int n, int bland) {
    if (bland) {
        for (int j = 0; j < n; j++)
            if (red[j] < -RED_EPS) return j;
        return -1;
    }
    int best = -1;
    for (int j = 0; j < n; j++)
        if (red[j] < -RED_EPS && (best < 0 || red[j] < red[best])) best = j;
    return best;
}

static int leave_row(const Tab *t, int ent, int bland, double *theta_out) {
    const double *T = t->T;
    int best = -1;
    double best_ratio = 0.0, best_piv = 0.0;
    for (int i = 0; i < t->m; i++) {
        double a = T[i * t->stride + ent];
        if (a <= PIV_EPS) continue;
        double v = T[i * t->stride + t->ntot];
        if (v < 0.0) v = 0.0;
        double r = v / a;
        if (best < 0) { best = i; best_ratio = r; best_piv = a; continue; }
        if (r < best_ratio - 1e-12) {
            best = i; best_ratio = r; best_piv = a;
        } else if (r <= best_ratio + 1e-12) {
            int take;
            if (bland) take = (t->basis[i] < t->basis[best]);
            else       take = (fabs(a) > best_piv + 1e-12);
            if (take) { best = i; best_ratio = r; best_piv = a; }
        }
    }
    if (theta_out) *theta_out = best_ratio;
    return best;
}

/* run one phase on tableau t; cost row already built. */
static int run_phase(Tab *t, int max_iter, int *ent_out) {
    int bland = 0, stall = 0;
    for (int it = 0; it < max_iter; it++) {
        double *red = t->T + t->m * t->stride;
        int ent = enter_col(red, t->n, bland);
        if (ent < 0) return SIMP_OPTIMAL;
        int lev = leave_row(t, ent, bland, NULL);
        if (lev < 0) { if (ent_out) *ent_out = ent; return SIMP_UNBOUNDED; }
        double zprev = red[t->ntot];
        tab_pivot(t, lev, ent);
        double znow = red[t->ntot];
        if (fabs(znow - zprev) > 1e-11 * (1.0 + fabs(znow))) { stall = 0; bland = 0; }
        else if (++stall > STALL_LIMIT) bland = 1;
    }
    return SIMP_MAXITER;
}

int simplex_solve_std_tab(const double *A, int m, int n,
                          const double *b, const double *c,
                          int max_iter, double *x, double *y,
                          double *dray, double *pray,
                          int *basis_out, double *tab_out)
{
    Tab t;
    t.m = m; t.n = n;
    t.ntot = n + m;
    t.stride = t.ntot + 1;
    t.T = (double *)calloc((size_t)(m + 1) * (size_t)t.stride, sizeof(double));
    t.basis = (int *)malloc((size_t)(m > 0 ? m : 1) * sizeof(int));
    if (!t.T || !t.basis) { free(t.T); free(t.basis); return SIMP_MEMORY; }

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) t.T[i * t.stride + j] = A[i * n + j];
        t.T[i * t.stride + n + i] = 1.0;
        double bv = b[i];
        if (bv < 0.0 && bv > -1e-9) bv = 0.0;
        t.T[i * t.stride + t.ntot] = bv;
        t.basis[i] = n + i;
    }
    double *red = t.T + m * t.stride;

    /* ---------- phase 1 ---------- */
    for (int j = 0; j < t.ntot; j++) {
        double s = (j >= n) ? 1.0 : 0.0;
        for (int i = 0; i < m; i++) s -= t.T[i * t.stride + j]; /* all basic costs are 1 */
        red[j] = s;
    }
    {
        double s = 0.0;
        for (int i = 0; i < m; i++) s -= t.T[i * t.stride + t.ntot];
        red[t.ntot] = s;
    }
    int status = run_phase(&t, max_iter, NULL);
    if (status == SIMP_MAXITER) goto done;
    if (status == SIMP_UNBOUNDED) { status = SIMP_MAXITER; goto done; } /* cannot happen */

    {
        double z1 = -red[t.ntot];
        double bscale = 1.0;
        for (int i = 0; i < m; i++) { double a = fabs(b[i]); if (a > bscale) bscale = a; }
        if (z1 > 1e-7 * bscale) {
            /* the phase-1 cost row is still in place: red[n+r] = 1 - y_r */
            if (dray) for (int r = 0; r < m; r++) dray[r] = 1.0 - red[n + r];
            status = SIMP_INFEASIBLE; goto done;
        }
    }

    /* drive artificials out of the basis */
    for (int i = 0; i < m; i++) {
        if (t.basis[i] < n) continue;
        int bj = -1;
        double bmax = 1e-7;
        for (int j = 0; j < n; j++) {
            double a = fabs(t.T[i * t.stride + j]);
            if (a > bmax) { bmax = a; bj = j; }
        }
        if (bj >= 0) tab_pivot(&t, i, bj);
        /* else: redundant row, artificial stays basic at ~0 */
    }

    /* ---------- phase 2: rebuild cost row for current basis ---------- */
    for (int j = 0; j < t.ntot; j++) {
        double s = (j < n) ? c[j] : 0.0;
        for (int i = 0; i < m; i++) {
            int bi = t.basis[i];
            double cb = (bi < n) ? c[bi] : 0.0;
            if (cb != 0.0) s -= cb * t.T[i * t.stride + j];
        }
        red[j] = s;
    }
    {
        double s = 0.0;
        for (int i = 0; i < m; i++) {
            int bi = t.basis[i];
            double cb = (bi < n) ? c[bi] : 0.0;
            if (cb != 0.0) s -= cb * t.T[i * t.stride + t.ntot];
        }
        red[t.ntot] = s;
    }
    int uent = -1;
    status = run_phase(&t, max_iter, &uent);

    /* ---------- extract ---------- */
    if (status == SIMP_UNBOUNDED && pray && uent >= 0) {
        /* x = x* + theta*d with d[ent] = 1 and d[basis[i]] = -(B^-1 A_ent)_i.
         * No leaving row exists, so every tableau entry of that column is
         * <= 0: d >= 0, A d = 0 and c'd = red[ent] < 0. */
        for (int j = 0; j < n; j++) pray[j] = 0.0;
        pray[uent] = 1.0;
        for (int i = 0; i < m; i++) {
            int bi = t.basis[i];
            if (bi >= 0 && bi < n) pray[bi] = -t.T[i * t.stride + uent];
        }
    }
    if (status == SIMP_OPTIMAL || status == SIMP_UNBOUNDED) {
        for (int j = 0; j < n; j++) x[j] = 0.0;
        for (int i = 0; i < m; i++) {
            int bi = t.basis[i];
            double v = t.T[i * t.stride + t.ntot];
            if (v < 0.0 && v > -1e-7) v = 0.0;
            if (bi < n) x[bi] = v;
        }
    }
    for (int r = 0; r < m; r++) y[r] = -red[n + r];

done:
    if (basis_out) for (int i = 0; i < m; i++) basis_out[i] = t.basis[i];
    if (tab_out) memcpy(tab_out, t.T, (size_t)(m + 1) * (size_t)t.stride * sizeof(double));
    free(t.T);
    free(t.basis);
    return status;
}

int simplex_solve_std(const double *A, int m, int n,
                      const double *b, const double *c,
                      int max_iter, double *x, double *y,
                      double *dray, double *pray)
{
    return simplex_solve_std_tab(A, m, n, b, c, max_iter, x, y, dray, pray, NULL, NULL);
}

/* Crash basis: m colonne linearmente indipendenti di A (Gauss con pivoting di
 * colonna). Ritorna 1 e riempie basis[m] se rank(A) == m, 0 altrimenti. */
int simplex_crash_basis(const double *A, int m, int n, int *basis)
{
    if (m < 1 || n < m || !A || !basis) return 0;
    double *W = (double *)malloc((size_t)m * (size_t)n * sizeof(double));
    int *col = (int *)malloc((size_t)n * sizeof(int));
    if (!W || !col) { free(W); free(col); return 0; }
    memcpy(W, A, (size_t)m * (size_t)n * sizeof(double));
    for (int j = 0; j < n; j++) col[j] = j;
    int ok = 1;
    for (int r = 0; r < m; r++) {
        int pi = r, pj = r; double mx = 0.0;
        for (int i = r; i < m; i++)
            for (int c = r; c < n; c++) { double v = fabs(W[(size_t)i * n + c]); if (v > mx) { mx = v; pi = i; pj = c; } }
        if (mx < 1e-9) { ok = 0; break; }   /* rank < m */
        if (pj != r) {
            for (int i = 0; i < m; i++) { double t = W[(size_t)i * n + r]; W[(size_t)i * n + r] = W[(size_t)i * n + pj]; W[(size_t)i * n + pj] = t; }
            int tc = col[r]; col[r] = col[pj]; col[pj] = tc;
        }
        if (pi != r) for (int c = 0; c < n; c++) { double t = W[(size_t)r * n + c]; W[(size_t)r * n + c] = W[(size_t)pi * n + c]; W[(size_t)pi * n + c] = t; }
        for (int i = r + 1; i < m; i++) {
            double f = W[(size_t)i * n + r] / W[(size_t)r * n + r];
            if (f != 0.0) for (int c = r; c < n; c++) W[(size_t)i * n + c] -= f * W[(size_t)r * n + c];
        }
    }
    if (ok) for (int r = 0; r < m; r++) basis[r] = col[r];
    free(W); free(col);
    return ok;
}
