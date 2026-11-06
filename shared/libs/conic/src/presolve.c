/* presolve.c - LP presolve reductions on the standard form
 *   min c'x  s.t. A x = b, x >= 0   (A SPARSE, CSC)
 *
 * Reductions (triggered by the exact zero STRUCTURE of A; only the scalar value
 * checks use tol):
 *  - empty row    (no alive col, |b_i| <= tol): redundant, removed; dual y_i = 0.
 *  - empty column (no alive row, c_j >= -tol): x_j = 0 optimal, removed.
 *  - singleton row (one alive nonzero A_ij): fixes x_j = b_i/A_ij, substitutes it
 *    into the other rows, removes row i and column j.
 *  - duplicate rows (identical alive pattern + equal RHS): keep one, drop the
 *    rest (dropped dual = 0; the kept row carries the combined dual).
 *  - duplicate columns (identical alive pattern): keep the cheapest, drop the
 *    rest with x = 0 (the kept min-cost column reproduces the original optimum).
 * Ambiguous cases (|b_i| > tol, x_j < -tol, c_j < -tol, tiny pivot) are LEFT to
 * the solver, so presolve never mis-declares infeasibility/unboundedness.
 *
 * Postsolve replays the reduction log in REVERSE. Primal: kept columns take the
 * reduced value, removed columns their recorded fixed value (0 for empty/dup).
 * Dual: kept rows take the reduced dual; a removed empty/duplicate row gets 0; a
 * removed singleton row i (which fixed column j) is recovered from KKT
 * stationarity of column j with reduced cost z_j = 0:
 *     y_i = (c_j - sum_{k != i} A_kj y_k) / A_ij.
 * Every row k != i touching column j is still alive when row i is reduced (two
 * singleton rows cannot share a live column), so the reverse pass has y_k. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "presolve.h"

enum { OP_EMPTY_ROW = 0, OP_SINGLETON = 1 };

typedef struct { int type, i, j; double Aij, cj; int nk; int *ks; double *Akj; } POp;

struct Presolve {
    int m, n, mr, nr, changed;
    int *Arptr, *Arrow; double *Arval;   /* reduced A (CSC) */
    double *br, *cr;
    int *row_map, *col_map;
    double *col_fix;
    POp *ops; int nops, opcap;
};

static unsigned long long hfold(unsigned long long h, int idx, double v) {
    unsigned long long vb; memcpy(&vb, &v, sizeof(vb));
    h = (h ^ (unsigned long long)idx) * 1099511628211ULL;
    h = (h ^ vb) * 1099511628211ULL;
    return h;
}

static int push_op(Presolve *p, int type, int i, int j, double Aij, double cj,
                   int nk, int *ks, double *Akj) {
    if (p->nops == p->opcap) {
        int nc = p->opcap ? p->opcap * 2 : 64;
        POp *no = (POp *)realloc(p->ops, (size_t)nc * sizeof(POp));
        if (!no) { free(ks); free(Akj); return 0; }
        p->ops = no; p->opcap = nc;
    }
    POp *o = &p->ops[p->nops++];
    o->type = type; o->i = i; o->j = j; o->Aij = Aij; o->cj = cj; o->nk = nk; o->ks = ks; o->Akj = Akj;
    return 1;
}

/* compare two alive rows over alive columns (CSR) + RHS, using a stamp marker */
static int rows_equal_sp(const int *rptr, const int *ridx, const double *rval,
                         const char *alive_col, const double *bw,
                         int *mstamp, double *mval, int stamp, int i1, int i2) {
    if (bw[i1] != bw[i2]) return 0;
    int c1 = 0;
    for (int p = rptr[i1]; p < rptr[i1 + 1]; p++) {
        int j = ridx[p]; if (!alive_col[j]) continue;
        mstamp[j] = stamp; mval[j] = rval[p]; c1++;
    }
    int c2 = 0;
    for (int p = rptr[i2]; p < rptr[i2 + 1]; p++) {
        int j = ridx[p]; if (!alive_col[j]) continue;
        c2++;
        if (mstamp[j] != stamp || mval[j] != rval[p]) return 0;
    }
    return c1 == c2;
}
static int cols_equal_sp(const int *Aptr, const int *Arow, const double *Aval,
                         const char *alive_row, int *mstamp, double *mval, int stamp,
                         int j1, int j2) {
    int c1 = 0;
    for (int p = Aptr[j1]; p < Aptr[j1 + 1]; p++) {
        int i = Arow[p]; if (!alive_row[i]) continue;
        mstamp[i] = stamp; mval[i] = Aval[p]; c1++;
    }
    int c2 = 0;
    for (int p = Aptr[j2]; p < Aptr[j2 + 1]; p++) {
        int i = Arow[p]; if (!alive_row[i]) continue;
        c2++;
        if (mstamp[i] != stamp || mval[i] != Aval[p]) return 0;
    }
    return c1 == c2;
}

int lp_presolve(const int *Aptr, const int *Arow, const double *Aval,
                const double *b, const double *c, int m, int n, double tol,
                Presolve **out) {
    if (out) *out = NULL;
    if (m < 0 || n < 0) return -1;
    int nnz = Aptr[n];
    Presolve *p = (Presolve *)calloc(1, sizeof(Presolve));
    if (!p) return -1;
    p->m = m; p->n = n; p->mr = m; p->nr = n;
    p->row_map = (int *)malloc((size_t)(m > 0 ? m : 1) * sizeof(int));
    p->col_map = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    p->col_fix = (double *)calloc((size_t)(n > 0 ? n : 1), sizeof(double));
    char *alive_row = (char *)malloc((size_t)(m > 0 ? m : 1));
    char *alive_col = (char *)malloc((size_t)(n > 0 ? n : 1));
    double *bw = (double *)malloc((size_t)(m > 0 ? m : 1) * sizeof(double));
    int *rptr = (int *)calloc((size_t)(m + 1), sizeof(int));
    int *ridx = (int *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(int));
    double *rval = (double *)malloc((size_t)(nnz > 0 ? nnz : 1) * sizeof(double));
    int *mstamp = (int *)calloc((size_t)(m > n ? m : n) + 1, sizeof(int));
    double *mval = (double *)calloc((size_t)(m > n ? m : n) + 1, sizeof(double));
    if (!p->row_map || !p->col_map || !p->col_fix || !alive_row || !alive_col ||
        !bw || !rptr || !ridx || !rval || !mstamp || !mval) {
        free(alive_row); free(alive_col); free(bw); free(rptr); free(ridx); free(rval);
        free(mstamp); free(mval); presolve_free(p); return -1;
    }
    for (int i = 0; i < m; i++) { alive_row[i] = 1; bw[i] = b[i]; }
    for (int j = 0; j < n; j++) alive_col[j] = 1;
    /* CSR of A */
    for (int j = 0; j < n; j++)
        for (int q = Aptr[j]; q < Aptr[j + 1]; q++) rptr[Arow[q] + 1]++;
    for (int i = 0; i < m; i++) rptr[i + 1] += rptr[i];
    { int *fill = (int *)calloc((size_t)(m > 0 ? m : 1), sizeof(int));
      if (!fill) { free(alive_row); free(alive_col); free(bw); free(rptr); free(ridx); free(rval); free(mstamp); free(mval); presolve_free(p); return -1; }
      for (int j = 0; j < n; j++)
        for (int q = Aptr[j]; q < Aptr[j + 1]; q++) { int i = Arow[q]; int pos = rptr[i] + fill[i]++; ridx[pos] = j; rval[pos] = Aval[q]; }
      free(fill); }

    int changed = 0, progress = 1, stamp = 1;
    while (progress) {
        progress = 0;
        /* ---- rows: empty + singleton ---- */
        for (int i = 0; i < m; i++) {
            if (!alive_row[i]) continue;
            int cnt = 0, sj = -1; double sv = 0.0;
            for (int q = rptr[i]; q < rptr[i + 1]; q++) {
                int j = ridx[q]; if (!alive_col[j]) continue;
                cnt++; sj = j; sv = rval[q]; if (cnt > 1) break;
            }
            if (cnt == 0) {
                if (fabs(bw[i]) <= tol) {
                    alive_row[i] = 0;
                    if (!push_op(p, OP_EMPTY_ROW, i, -1, 0, 0, 0, NULL, NULL)) goto fail;
                    changed = progress = 1;
                }
            } else if (cnt == 1) {
                int j = sj; double v = sv;
                if (fabs(v) < 1e-12) continue;
                double xj = bw[i] / v;
                if (xj < -tol) continue;
                if (xj < 0.0) xj = 0.0;
                int nk = 0;
                for (int q = Aptr[j]; q < Aptr[j + 1]; q++) { int k = Arow[q]; if (k != i && alive_row[k]) nk++; }
                int *ks = nk ? (int *)malloc((size_t)nk * sizeof(int)) : NULL;
                double *Akj = nk ? (double *)malloc((size_t)nk * sizeof(double)) : NULL;
                if (nk && (!ks || !Akj)) { free(ks); free(Akj); goto fail; }
                int t = 0;
                for (int q = Aptr[j]; q < Aptr[j + 1]; q++) {
                    int k = Arow[q]; if (k == i || !alive_row[k]) continue;
                    ks[t] = k; Akj[t] = Aval[q]; t++; bw[k] -= Aval[q] * xj;
                }
                p->col_fix[j] = xj;
                alive_row[i] = 0; alive_col[j] = 0;
                if (!push_op(p, OP_SINGLETON, i, j, v, c[j], nk, ks, Akj)) goto fail;
                changed = progress = 1;
            }
        }
        /* ---- columns: empty ---- */
        for (int j = 0; j < n; j++) {
            if (!alive_col[j]) continue;
            int cnt = 0;
            for (int q = Aptr[j]; q < Aptr[j + 1]; q++) if (alive_row[Arow[q]]) { cnt = 1; break; }
            if (cnt == 0 && c[j] >= -tol) {
                p->col_fix[j] = 0.0; alive_col[j] = 0;
                changed = progress = 1;
            }
        }
        /* ---- duplicate rows ---- */
        {
            int TS = 1; while (TS < 2 * (m + 2)) TS <<= 1;
            int *tab = (int *)malloc((size_t)TS * sizeof(int));
            unsigned long long *rh = (unsigned long long *)malloc((size_t)(m > 0 ? m : 1) * sizeof(unsigned long long));
            if (!tab || !rh) { free(tab); free(rh); goto fail; }
            for (int s = 0; s < TS; s++) tab[s] = -1;
            for (int i = 0; i < m; i++) {
                if (!alive_row[i]) continue;
                unsigned long long h = 1469598103934665603ULL; int nz = 0;
                for (int q = rptr[i]; q < rptr[i + 1]; q++) if (alive_col[ridx[q]]) { nz++; h = hfold(h, ridx[q], rval[q]); }
                rh[i] = h;
                if (nz == 0) continue;
                int pos = (int)(h & (unsigned long long)(TS - 1)), probes = 0, placed = 0;
                while (tab[pos] != -1 && probes < TS) {
                    int i0 = tab[pos];
                    if (rh[i0] == h && alive_row[i0] && rows_equal_sp(rptr, ridx, rval, alive_col, bw, mstamp, mval, ++stamp, i0, i)) {
                        alive_row[i] = 0; changed = progress = 1; placed = 1; break;
                    }
                    pos = (pos + 1) & (TS - 1); probes++;
                }
                if (!placed && tab[pos] == -1) tab[pos] = i;
            }
            free(tab); free(rh);
        }
        /* ---- duplicate columns (keep cheapest) ---- */
        {
            int TS = 1; while (TS < 2 * (n + 2)) TS <<= 1;
            int *tab = (int *)malloc((size_t)TS * sizeof(int));
            unsigned long long *ch = (unsigned long long *)malloc((size_t)(n > 0 ? n : 1) * sizeof(unsigned long long));
            if (!tab || !ch) { free(tab); free(ch); goto fail; }
            for (int s = 0; s < TS; s++) tab[s] = -1;
            for (int j = 0; j < n; j++) {
                if (!alive_col[j]) continue;
                unsigned long long h = 1469598103934665603ULL; int nz = 0;
                for (int q = Aptr[j]; q < Aptr[j + 1]; q++) if (alive_row[Arow[q]]) { nz++; h = hfold(h, Arow[q], Aval[q]); }
                ch[j] = h;
                if (nz == 0) continue;
                int pos = (int)(h & (unsigned long long)(TS - 1)), probes = 0, done = 0;
                while (tab[pos] != -1 && probes < TS) {
                    int j0 = tab[pos];
                    if (ch[j0] == h && alive_col[j0] && cols_equal_sp(Aptr, Arow, Aval, alive_row, mstamp, mval, ++stamp, j0, j)) {
                        int keep = (c[j0] <= c[j]) ? j0 : j;
                        int drop = (keep == j0) ? j : j0;
                        alive_col[drop] = 0; p->col_fix[drop] = 0.0;
                        tab[pos] = keep; changed = progress = 1; done = 1; break;
                    }
                    pos = (pos + 1) & (TS - 1); probes++;
                }
                if (!done && tab[pos] == -1) tab[pos] = j;
            }
            free(tab); free(ch);
        }
    }

    p->changed = changed;
    if (!changed) {
        for (int i = 0; i < m; i++) p->row_map[i] = i;
        for (int j = 0; j < n; j++) p->col_map[j] = j;
        free(alive_row); free(alive_col); free(bw); free(rptr); free(ridx); free(rval); free(mstamp); free(mval);
        if (out) *out = p;
        return 0;
    }
    int mr = 0, nr = 0;
    for (int i = 0; i < m; i++) p->row_map[i] = alive_row[i] ? mr++ : -1;
    for (int j = 0; j < n; j++) p->col_map[j] = alive_col[j] ? nr++ : -1;
    p->mr = mr; p->nr = nr;
    p->br = (double *)malloc((size_t)(mr > 0 ? mr : 1) * sizeof(double));
    p->cr = (double *)malloc((size_t)(nr > 0 ? nr : 1) * sizeof(double));
    p->Arptr = (int *)calloc((size_t)(nr + 1), sizeof(int));
    if (!p->br || !p->cr || !p->Arptr) { free(alive_row); free(alive_col); free(bw); free(rptr); free(ridx); free(rval); free(mstamp); free(mval); presolve_free(p); return -1; }
    for (int i = 0; i < m; i++) if (alive_row[i]) p->br[p->row_map[i]] = bw[i];
    for (int j = 0; j < n; j++) if (alive_col[j]) p->cr[p->col_map[j]] = c[j];
    /* reduced CSC: count then fill */
    for (int j = 0; j < n; j++) {
        if (!alive_col[j]) continue;
        for (int q = Aptr[j]; q < Aptr[j + 1]; q++) if (alive_row[Arow[q]]) p->Arptr[p->col_map[j] + 1]++;
    }
    for (int jj = 0; jj < nr; jj++) p->Arptr[jj + 1] += p->Arptr[jj];
    int rtot = p->Arptr[nr];
    p->Arrow = (int *)malloc((size_t)(rtot > 0 ? rtot : 1) * sizeof(int));
    p->Arval = (double *)malloc((size_t)(rtot > 0 ? rtot : 1) * sizeof(double));
    if (!p->Arrow || !p->Arval) { free(alive_row); free(alive_col); free(bw); free(rptr); free(ridx); free(rval); free(mstamp); free(mval); presolve_free(p); return -1; }
    { int *w = (int *)malloc((size_t)(nr > 0 ? nr : 1) * sizeof(int));
      if (!w) { free(alive_row); free(alive_col); free(bw); free(rptr); free(ridx); free(rval); free(mstamp); free(mval); presolve_free(p); return -1; }
      for (int jj = 0; jj < nr; jj++) w[jj] = p->Arptr[jj];
      for (int j = 0; j < n; j++) {
          if (!alive_col[j]) continue;
          int jj = p->col_map[j];
          for (int q = Aptr[j]; q < Aptr[j + 1]; q++) {
              int i = Arow[q]; if (!alive_row[i]) continue;
              p->Arrow[w[jj]] = p->row_map[i]; p->Arval[w[jj]] = Aval[q]; w[jj]++;
          }
      }
      free(w); }
    free(alive_row); free(alive_col); free(bw); free(rptr); free(ridx); free(rval); free(mstamp); free(mval);
    if (out) *out = p;
    return 0;

fail:
    free(alive_row); free(alive_col); free(bw); free(rptr); free(ridx); free(rval); free(mstamp); free(mval);
    presolve_free(p);
    return -1;
}

int presolve_changed(const Presolve *p) { return p ? p->changed : 0; }

void presolve_reduced(const Presolve *p, const int **Arptr, const int **Arrow,
                      const double **Arval, const double **br, const double **cr,
                      int *mr, int *nr) {
    if (!p) return;
    if (Arptr) *Arptr = p->Arptr;
    if (Arrow) *Arrow = p->Arrow;
    if (Arval) *Arval = p->Arval;
    if (br) *br = p->br;
    if (cr) *cr = p->cr;
    if (mr) *mr = p->mr;
    if (nr) *nr = p->nr;
}

void presolve_postsolve(const Presolve *p, const double *xred, const double *yred,
                        double *xfull, double *yfull) {
    if (!p) return;
    for (int j = 0; j < p->n; j++)
        xfull[j] = (p->col_map[j] >= 0) ? xred[p->col_map[j]] : p->col_fix[j];
    for (int i = 0; i < p->m; i++)
        yfull[i] = (p->row_map[i] >= 0) ? yred[p->row_map[i]] : 0.0;
    for (int o = p->nops - 1; o >= 0; o--) {
        const POp *op = &p->ops[o];
        if (op->type == OP_EMPTY_ROW) yfull[op->i] = 0.0;
        else if (op->type == OP_SINGLETON) {
            double sum = 0.0;
            for (int q = 0; q < op->nk; q++) sum += op->Akj[q] * yfull[op->ks[q]];
            yfull[op->i] = (op->cj - sum) / op->Aij;
        }
    }
}

void presolve_postsolve_dir(const Presolve *p, const double *rred, const double *yred,
                            double *rfull, double *yfull) {
    if (!p) return;
    for (int j = 0; j < p->n; j++)
        rfull[j] = (rred && p->col_map[j] >= 0) ? rred[p->col_map[j]] : 0.0;
    for (int i = 0; i < p->m; i++)
        yfull[i] = (yred && p->row_map[i] >= 0) ? yred[p->row_map[i]] : 0.0;
    for (int o = p->nops - 1; o >= 0; o--) {
        const POp *op = &p->ops[o];
        if (op->type != OP_SINGLETON) continue;   /* empty/duplicate row: stays 0 */
        double sum = 0.0;
        for (int q = 0; q < op->nk; q++) sum += op->Akj[q] * yfull[op->ks[q]];
        yfull[op->i] = -sum / op->Aij;
    }
}

void presolve_free(Presolve *p) {
    if (!p) return;
    for (int o = 0; o < p->nops; o++) { free(p->ops[o].ks); free(p->ops[o].Akj); }
    free(p->ops);
    free(p->Arptr); free(p->Arrow); free(p->Arval); free(p->br); free(p->cr);
    free(p->row_map); free(p->col_map); free(p->col_fix);
    free(p);
}
