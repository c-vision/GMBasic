/* gor_lp.c — LP (simplesso) e MILP (branch & bound). */
#include "gor_lp.h"
#include "simplex.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int gor_lp_solve(int m, int n, const double *A, const double *b, const double *c,
                 double *x, double *y, double *obj, int max_iter)
{
    double *A2, *b2, *c2, *x2, *y2;
    int st;
    if (m <= 0 || n <= 0 || A == NULL || b == NULL || c == NULL || x == NULL) return 1;
    /* copia: il simplesso distrugge A */
    A2 = (double *)malloc((size_t)m * (size_t)n * sizeof(double));
    b2 = (double *)malloc((size_t)m * sizeof(double));
    c2 = (double *)malloc((size_t)n * sizeof(double));
    x2 = (double *)malloc((size_t)n * sizeof(double));
    y2 = (double *)malloc((size_t)m * sizeof(double));
    if (!A2 || !b2 || !c2 || !x2 || !y2) { free(A2); free(b2); free(c2); free(x2); free(y2); return 4; }
    memcpy(A2, A, (size_t)m * (size_t)n * sizeof(double));
    memcpy(b2, b, (size_t)m * sizeof(double));
    memcpy(c2, c, (size_t)n * sizeof(double));
    st = simplex_solve_std(A2, m, n, b2, c2, max_iter > 0 ? max_iter : 10000,
                           x2, y2, NULL, NULL);
    if (st == 0) {
        int i;
        double v = 0.0;
        for (i = 0; i < n; i++) { x[i] = x2[i]; v += c[i] * x2[i]; }
        if (y) for (i = 0; i < m; i++) y[i] = y2[i];
        if (obj) *obj = v;
    }
    free(A2); free(b2); free(c2); free(x2); free(y2);
    return st;
}

/* ============================== MILP branch & bound ============================== */

typedef struct { int var; int type; double bound; } gor_br; /* type 0: <=, 1: >= */

typedef struct {
    int nbr;
    gor_br *br;
} gor_node;

typedef struct {
    int m, n, nint;
    const double *A, *b, *c;
    const int *int_idx;
    double *xbest, best;
    int have_best, nodes, maxnodes;
    double tol;
} gor_bb_ctx;

/* risolve il LP rilassato con i vincoli di branch del nodo; ritorna status
 * (0 ok, 1 infeas, 2 unb, 4 mem). x (n), obj. */
static int bb_solve_lp(const gor_bb_ctx *ctx, const gor_node *node, double *x, double *obj)
{
    int m = ctx->m + node->nbr, n = ctx->n + node->nbr;
    double *A2, *b2, *c2, *x2, *y2;
    int i, j, st;
    A2 = (double *)calloc((size_t)m * (size_t)n, sizeof(double));
    b2 = (double *)malloc((size_t)m * sizeof(double));
    c2 = (double *)calloc((size_t)n, sizeof(double));
    x2 = (double *)malloc((size_t)n * sizeof(double));
    y2 = (double *)malloc((size_t)m * sizeof(double));
    if (!A2 || !b2 || !c2 || !x2 || !y2) { free(A2); free(b2); free(c2); free(x2); free(y2); return 4; }
    for (i = 0; i < ctx->m; i++) {
        for (j = 0; j < ctx->n; j++) A2[(size_t)i * (size_t)n + (size_t)j] = ctx->A[(size_t)i * (size_t)ctx->n + (size_t)j];
        b2[i] = ctx->b[i];
    }
    for (j = 0; j < ctx->n; j++) c2[j] = ctx->c[j];
    for (i = 0; i < node->nbr; i++) {
        int row = ctx->m + i;
        int var = node->br[i].var;
        int slack = ctx->n + i;
        if (node->br[i].type == 0) { /* x_var + s = bound */
            A2[(size_t)row * (size_t)n + (size_t)var] = 1.0;
            A2[(size_t)row * (size_t)n + (size_t)slack] = 1.0;
        } else { /* x_var - s = bound */
            A2[(size_t)row * (size_t)n + (size_t)var] = 1.0;
            A2[(size_t)row * (size_t)n + (size_t)slack] = -1.0;
        }
        b2[row] = node->br[i].bound;
    }
    st = simplex_solve_std(A2, m, n, b2, c2, 5000, x2, y2, NULL, NULL);
    if (st == 0) {
        double v = 0.0;
        for (j = 0; j < ctx->n; j++) { x[j] = x2[j]; v += ctx->c[j] * x2[j]; }
        if (obj) *obj = v;
    }
    free(A2); free(b2); free(c2); free(x2); free(y2);
    return st;
}

static int bb_search(gor_bb_ctx *ctx, gor_node *node)
{
    double *x, obj;
    int st, i, frac_var = -1;
    double frac_val = 0.0, frac_dist = ctx->tol;
    if (ctx->nodes >= ctx->maxnodes) return 3;
    ctx->nodes++;
    x = (double *)malloc((size_t)ctx->n * sizeof(double));
    if (!x) return 4;
    st = bb_solve_lp(ctx, node, x, &obj);
    if (st != 0) { free(x); return st == 1 ? 1 : 0; } /* infeasible o unbounded: pota */
    if (ctx->have_best && obj >= ctx->best - 1e-9) { free(x); return 0; } /* bound */
    /* trova la variabile intera più frazionaria */
    for (i = 0; i < ctx->nint; i++) {
        int v = ctx->int_idx[i];
        double f = fabs(x[v] - floor(x[v] + 0.5));
        if (f > frac_dist) { frac_dist = f; frac_var = v; frac_val = x[v]; }
    }
    if (frac_var < 0) {
        /* soluzione intera: aggiorna incumbent */
        ctx->best = obj; ctx->have_best = 1;
        memcpy(ctx->xbest, x, (size_t)ctx->n * sizeof(double));
        free(x);
        return 0;
    }
    /* branch: <= floor e >= ceil */
    {
        gor_node child;
        int nbr = node->nbr + 1;
        child.br = (gor_br *)malloc((size_t)nbr * sizeof(gor_br));
        if (!child.br) { free(x); return 4; }
        if (node->nbr > 0) memcpy(child.br, node->br, (size_t)node->nbr * sizeof(gor_br));
        /* figlio 1: x <= floor */
        child.nbr = nbr;
        child.br[nbr - 1].var = frac_var;
        child.br[nbr - 1].type = 0;
        child.br[nbr - 1].bound = floor(frac_val);
        bb_search(ctx, &child);
        /* figlio 2: x >= ceil */
        child.br[nbr - 1].type = 1;
        child.br[nbr - 1].bound = ceil(frac_val);
        bb_search(ctx, &child);
        free(child.br);
    }
    free(x);
    return 0;
}

int gor_milp_bb(int m, int n, const double *A, const double *b, const double *c,
                int nint, const int *int_idx, double *x, double *obj,
                int maxnodes, double tol)
{
    gor_bb_ctx ctx;
    gor_node root;
    int st;
    if (m <= 0 || n <= 0 || A == NULL || b == NULL || c == NULL || x == NULL) return 1;
    if (tol <= 0.0) tol = 1e-6;
    ctx.m = m; ctx.n = n; ctx.nint = nint;
    ctx.A = A; ctx.b = b; ctx.c = c; ctx.int_idx = int_idx;
    ctx.xbest = (double *)malloc((size_t)n * sizeof(double));
    ctx.best = 1e300; ctx.have_best = 0; ctx.nodes = 0;
    ctx.maxnodes = maxnodes > 0 ? maxnodes : 10000;
    ctx.tol = tol;
    if (!ctx.xbest) return 4;
    root.nbr = 0; root.br = NULL;
    st = bb_search(&ctx, &root);
    if (ctx.have_best) {
        memcpy(x, ctx.xbest, (size_t)n * sizeof(double));
        if (obj) *obj = ctx.best;
        st = 0;
    } else if (st == 0) {
        st = 1; /* nessuna soluzione intera trovata -> inammissibile */
    }
    free(ctx.xbest);
    return st;
}

/* ============================== taglio di Gomory ============================== */

int gor_gomory_cut(int ncols, const double *tabrow, int rhs_col, double *cut)
{
    /* Taglio mixed-integer di Gomory da una riga del tableau con RHS frazionario.
     * cut[0..ncols-1] = coefficienti, cut[ncols] = RHS. Ritorna 1 se il taglio è
     * valido (RHS frazionario), 0 altrimenti. */
    double rhs;
    int j;
    if (tabrow == NULL || cut == NULL || ncols <= 0) return 0;
    rhs = tabrow[rhs_col];
    if (fabs(rhs - floor(rhs + 0.5)) < 1e-9) return 0;
    for (j = 0; j < ncols; j++) {
        double a = tabrow[j];
        double f = a - floor(a);
        if (f > 0.5) f -= 1.0;
        cut[j] = f;
    }
    cut[ncols] = rhs - floor(rhs);
    return 1;
}

/* ===================== matheuristics: hard fixing / local branching ===================== */
/* Forma standard: A x = b, x >= 0 (slack esplicite). */

static unsigned long long mh_next(unsigned long long *s)
{ *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; return *s; }

int gor_matheuristic_hard_fixing(int m, int n, const double *A, const double *b,
                                 const double *c, int nint, const int *int_idx,
                                 double fix_frac, int rounds, double *x, double *obj,
                                 int maxnodes, double tol, unsigned long long seed)
{
    double *xb, *A2, *b2;
    double bestobj = 1e300, cur;
    int st, r, i, j;
    unsigned long long sd = seed ? seed : 12345ULL;
    if (m <= 0 || n <= 0 || !A || !b || !c || !x) return 1;
    xb = (double *)malloc((size_t)n * sizeof(double));
    if (!xb) return 4;
    st = gor_milp_bb(m, n, A, b, c, nint, int_idx, xb, &cur, maxnodes, tol);
    if (st == 0 && cur < bestobj) { bestobj = cur; for (j = 0; j < n; j++) x[j] = xb[j]; }
    for (r = 0; r < rounds; r++) {
        int nfix = (int)(fix_frac * nint); if (nfix < 1) nfix = 1; if (nfix > nint) nfix = nint;
        A2 = (double *)calloc((size_t)(m + nfix) * n, sizeof(double));
        b2 = (double *)malloc((size_t)(m + nfix) * sizeof(double));
        if (!A2 || !b2) { free(A2); free(b2); break; }
        for (i = 0; i < m; i++) { for (j = 0; j < n; j++) A2[(size_t)i * n + j] = A[(size_t)i * n + j]; b2[i] = b[i]; }
        { int k;
          for (k = 0; k < nfix; k++) {
              int idx = int_idx[mh_next(&sd) % (unsigned long long)nint];
              double v = floor(xb[idx] + 0.5);   /* x_idx = v (uguaglianza) */
              A2[(size_t)(m + k) * n + idx] = 1.0; b2[m + k] = v;
          } }
        st = gor_milp_bb(m + nfix, n, A2, b2, c, nint, int_idx, xb, &cur, maxnodes, tol);
        if (st == 0 && cur < bestobj - 1e-9) { bestobj = cur; for (j = 0; j < n; j++) x[j] = xb[j]; }
        free(A2); free(b2);
    }
    if (obj) *obj = bestobj;
    free(xb);
    return (bestobj < 1e300) ? 0 : 1;
}

int gor_matheuristic_local_branching(int m, int n, const double *A, const double *b,
                                     const double *c, int nint, const int *int_idx,
                                     int k_start, int rounds, double *x, double *obj,
                                     int maxnodes, double tol)
{
    double *xb, *A2, *b2, *c2;
    double bestobj = 1e300, cur;
    int st, r, i, j, k;
    if (m <= 0 || n <= 0 || !A || !b || !c || !x) return 1;
    xb = (double *)malloc((size_t)(n + 1) * sizeof(double));
    if (!xb) return 4;
    st = gor_milp_bb(m, n, A, b, c, nint, int_idx, xb, &cur, maxnodes, tol);
    if (st == 0) { bestobj = cur; for (j = 0; j < n; j++) x[j] = xb[j]; }
    for (r = 0; r < rounds; r++) {
        int s1 = 0, np1 = n + 1;
        for (k = 0; k < nint; k++) if (xb[int_idx[k]] > 0.5) s1++;
        A2 = (double *)calloc((size_t)(m + 1) * (size_t)np1, sizeof(double));
        b2 = (double *)malloc((size_t)(m + 1) * sizeof(double));
        c2 = (double *)calloc((size_t)np1, sizeof(double));
        if (!A2 || !b2 || !c2) { free(A2); free(b2); free(c2); break; }
        for (i = 0; i < m; i++) { for (j = 0; j < n; j++) A2[(size_t)i * np1 + j] = A[(size_t)i * n + j]; b2[i] = b[i]; }
        for (j = 0; j < n; j++) c2[j] = c[j];
        /* sum_{x*_j=1}(1-x_j) + sum_{x*_j=0} x_j + s = k - s1, s>=0 */
        for (k = 0; k < nint; k++) {
            int idx = int_idx[k];
            A2[(size_t)m * np1 + idx] = (xb[idx] > 0.5) ? -1.0 : 1.0;
        }
        A2[(size_t)m * np1 + n] = 1.0;
        b2[m] = (double)(k_start + r) - s1;
        st = gor_milp_bb(m + 1, np1, A2, b2, c2, nint, int_idx, xb, &cur, maxnodes, tol);
        if (st == 0 && cur < bestobj - 1e-9) { bestobj = cur; for (j = 0; j < n; j++) x[j] = xb[j]; }
        free(A2); free(b2); free(c2);
    }
    if (obj) *obj = bestobj;
    free(xb);
    return (bestobj < 1e300) ? 0 : 1;
}

/* ===================== Ottimo alternativo (da Project4IO-Simplex) ===================== */

int gor_lp_alt_optimum(int m, int n, const double *A, const double *b,
                       const double *c, double *x2, double *z2)
{
    int ntot, stride, i, j, rc, lev = -1, ent = -1;
    double *T, *x, *y, *red, best = 1e300;
    int *basis;
    if (m < 1 || n < 1 || !A || !b || !c || !x2) return -1;
    ntot = n + m;
    stride = ntot + 1;
    T = (double *)malloc((size_t)(m + 1) * (size_t)stride * sizeof(double));
    x = (double *)malloc((size_t)n * sizeof(double));
    y = (double *)malloc((size_t)m * sizeof(double));
    basis = (int *)malloc((size_t)m * sizeof(int));
    if (!T || !x || !y || !basis) { free(T); free(x); free(y); free(basis); return -1; }
    rc = simplex_solve_std_tab(A, m, n, b, c, 10000, x, y, NULL, NULL, basis, T);
    if (rc != 0) { free(T); free(x); free(y); free(basis); return -1; }
    red = T + (size_t)m * stride;
    for (j = 0; j < ntot && ent < 0; j++) {
        int isbasic = 0;
        for (i = 0; i < m; i++) if (basis[i] == j) isbasic = 1;
        if (!isbasic && fabs(red[j]) < 1e-9) ent = j;
    }
    if (ent < 0) { free(T); free(x); free(y); free(basis); return 0; }  /* ottimo unico */
    for (i = 0; i < m; i++) {
        double a = T[(size_t)i * stride + ent];
        if (a > 1e-12) {
            double r = T[(size_t)i * stride + ntot] / a;
            if (r < best - 1e-12) { best = r; lev = i; }
        }
    }
    if (lev < 0) { free(T); free(x); free(y); free(basis); return 0; }  /* raggio, nessun vertice adiacente */
    /* pivot su (lev, ent) */
    {
        double piv = T[(size_t)lev * stride + ent];
        for (j = 0; j < stride; j++) T[(size_t)lev * stride + j] /= piv;
        for (i = 0; i < m + 1; i++) {
            double f;
            if (i == lev) continue;
            f = T[(size_t)i * stride + ent];
            if (f == 0.0) continue;
            for (j = 0; j < stride; j++) T[(size_t)i * stride + j] -= f * T[(size_t)lev * stride + j];
        }
        basis[lev] = ent;
    }
    for (i = 0; i < n; i++) x2[i] = 0.0;
    for (i = 0; i < m; i++) if (basis[i] >= 0 && basis[i] < n) x2[basis[i]] = T[(size_t)i * stride + ntot];
    if (z2) {
        double z = 0.0;
        for (i = 0; i < n; i++) z += c[i] * x2[i];
        *z2 = z;
    }
    {
        int diff = 0;
        for (i = 0; i < n; i++) if (fabs(x2[i] - x[i]) > 1e-9) diff = 1;
        free(T); free(x); free(y); free(basis);
        return diff;
    }
}

/* ===================== Dual simplex (da Operation_Research_Lab) ===================== */

int gor_lp_dual_simplex(int m, int n, const double *A, const double *b,
                        const double *c, const int *basis, int max_iter, double *x)
{
    int ncols = n + 1, i, j, k, it, rc = 0;
    double *T, *red, *cB, *M;
    int *bas;
    if (m < 1 || n < 1 || !A || !b || !c || !basis || !x) return 4;
    T = (double *)malloc((size_t)m * ncols * sizeof(double));
    red = (double *)malloc((size_t)n * sizeof(double));
    cB = (double *)malloc((size_t)m * sizeof(double));
    M = (double *)malloc((size_t)m * 2 * m * sizeof(double));   /* [B | I] -> B^-1 */
    bas = (int *)malloc((size_t)m * sizeof(int));
    if (!T || !red || !cB || !M || !bas) { free(T); free(red); free(cB); free(M); free(bas); return 4; }
    /* M e' [B | I]: m righe per 2m colonne, STRIDE 2m (prima era usata con stride
     * m -> per m>1 le righe si sovrapponevano e B^-1 usciva sbagliata, in
     * silenzio; l'allocazione era gia' stata portata a m*2m). */
    for (i = 0; i < m; i++) {
        for (j = 0; j < m; j++) M[(size_t)i * 2 * m + j] = A[(size_t)i * n + basis[j]];
        for (j = 0; j < m; j++) M[(size_t)i * 2 * m + m + j] = (i == j) ? 1.0 : 0.0;
    }
    /* Gauss-Jordan su [B | I] per ottenere B^-1 nella metà destra */
    {
        double *W = (double *)malloc((size_t)m * 2 * m * sizeof(double));
        if (!W) { free(T); free(red); free(cB); free(M); free(bas); return 4; }
        for (i = 0; i < m; i++) for (j = 0; j < 2 * m; j++) W[(size_t)i * 2 * m + j] = M[(size_t)i * 2 * m + j];
        for (k = 0; k < m; k++) {
            int piv = k; double mx = fabs(W[(size_t)k * 2 * m + k]);
            for (i = k + 1; i < m; i++) { double v = fabs(W[(size_t)i * 2 * m + k]); if (v > mx) { mx = v; piv = i; } }
            if (mx < 1e-12) { free(W); free(T); free(red); free(cB); free(M); free(bas); return 1; }
            if (piv != k) for (j = 0; j < 2 * m; j++) { double t = W[(size_t)k*2*m+j]; W[(size_t)k*2*m+j] = W[(size_t)piv*2*m+j]; W[(size_t)piv*2*m+j] = t; }
            { double p = W[(size_t)k * 2 * m + k]; for (j = 0; j < 2 * m; j++) W[(size_t)k * 2 * m + j] /= p; }
            for (i = 0; i < m; i++) {
                double f;
                if (i == k) continue;
                f = W[(size_t)i * 2 * m + k];
                if (f == 0.0) continue;
                for (j = 0; j < 2 * m; j++) W[(size_t)i * 2 * m + j] -= f * W[(size_t)k * 2 * m + j];
            }
        }
        for (i = 0; i < m; i++) for (j = 0; j < m; j++) M[(size_t)i * 2 * m + j] = W[(size_t)i * 2 * m + m + j];
        free(W);
    }
    for (i = 0; i < m; i++) bas[i] = basis[i];
    for (i = 0; i < m; i++) {
        for (j = 0; j < n; j++) {
            double s = 0.0;
            for (k = 0; k < m; k++) s += M[(size_t)i * 2 * m + k] * A[(size_t)k * n + j];
            T[(size_t)i * ncols + j] = s;
        }
        { double s = 0.0; for (k = 0; k < m; k++) s += M[(size_t)i * 2 * m + k] * b[k]; T[(size_t)i * ncols + n] = s; }
    }
    for (it = 0; it < max_iter; it++) {
        int r = -1, ent = -1;
        double worst = -1e-9, bestratio = 1e300;
        for (i = 0; i < m; i++) cB[i] = c[bas[i]];
        for (j = 0; j < n; j++) {
            double s = c[j];
            for (i = 0; i < m; i++) s -= cB[i] * T[(size_t)i * ncols + j];
            red[j] = s;
        }
        for (i = 0; i < m; i++) if (T[(size_t)i * ncols + n] < worst) { worst = T[(size_t)i * ncols + n]; r = i; }
        if (r < 0) { rc = 0; break; }
        for (j = 0; j < n; j++) {
            int isbasic = 0;
            for (i = 0; i < m; i++) if (bas[i] == j) isbasic = 1;
            if (isbasic) continue;
            if (T[(size_t)r * ncols + j] < -1e-9) {
                double ratio = red[j] / (-T[(size_t)r * ncols + j]);
                if (ratio < bestratio) { bestratio = ratio; ent = j; }
            }
        }
        if (ent < 0) { rc = 1; break; }
        {   /* pivot (r, ent) */
            double p = T[(size_t)r * ncols + ent];
            for (j = 0; j < ncols; j++) T[(size_t)r * ncols + j] /= p;
            for (i = 0; i < m; i++) {
                double f;
                if (i == r) continue;
                f = T[(size_t)i * ncols + ent];
                if (f == 0.0) continue;
                for (j = 0; j < ncols; j++) T[(size_t)i * ncols + j] -= f * T[(size_t)r * ncols + j];
            }
            bas[r] = ent;
        }
        if (it == max_iter - 1) rc = 3;
    }
    for (i = 0; i < n; i++) x[i] = 0.0;
    for (i = 0; i < m; i++) if (bas[i] >= 0 && bas[i] < n) x[bas[i]] = T[(size_t)i * ncols + n];
    free(T); free(red); free(cB); free(M); free(bas);
    return rc;
}

/* ===================== LP a bound bilaterali (da FOSSEE-ECOS-OR Q3) ===================== */

#define GOR_LP_INF 1e300

int gor_lp_solve_bounds(int m, int n, const double *A, const double *s,
                        const double *b, const double *l, const double *u,
                        const double *c, double *x)
{
    int j, i, k, p, nvb = 0, nci = 0, nv, rc;
    int *vof;
    double *cc, *AA, *bb, *yy, *yydual, *row, objconst = 0.0;
    if (m < 0 || n < 1 || !A || !s || !b || !l || !u || !c || !x) return -1;
    vof = (int *)malloc((size_t)n * sizeof(int));
    if (!vof) return -1;
    for (j = 0; j < n; j++) {
        int lf = l[j] > -GOR_LP_INF, uf = u[j] < GOR_LP_INF;
        vof[j] = nvb;
        nvb += (lf || uf) ? 1 : 2;
    }
    for (i = 0; i < m; i++) { if (b[i] < GOR_LP_INF) nci++; if (s[i] > -GOR_LP_INF) nci++; }
    for (j = 0; j < n; j++) if (l[j] > -GOR_LP_INF && u[j] < GOR_LP_INF) nci++;
    nv = nvb + nci;   /* base vars + una slack/surplus per disuguaglianza */
    cc = (double *)calloc((size_t)nv, sizeof(double));
    AA = (double *)calloc((size_t)(nci > 0 ? nci : 1) * (size_t)nv, sizeof(double));
    bb = (double *)calloc((size_t)(nci > 0 ? nci : 1), sizeof(double));
    yy = (double *)calloc((size_t)nv, sizeof(double));
    yydual = (double *)calloc((size_t)(nci > 0 ? nci : 1), sizeof(double));
    row = (double *)calloc((size_t)nv, sizeof(double));
    if (!cc || !AA || !bb || !yy || !yydual || !row) {
        free(vof); free(cc); free(AA); free(bb); free(yy); free(yydual); free(row); return -1;
    }
    for (j = 0; j < n; j++) {
        int lf = l[j] > -GOR_LP_INF, uf = u[j] < GOR_LP_INF;
        if (lf) { objconst += c[j] * l[j]; cc[vof[j]] = c[j]; }
        else if (uf) { objconst += c[j] * u[j]; cc[vof[j]] = -c[j]; }
        else { cc[vof[j]] = c[j]; cc[vof[j] + 1] = -c[j]; }
    }
    k = 0;
    for (i = 0; i < m; i++) {
        int uf = b[i] < GOR_LP_INF, lf = s[i] > -GOR_LP_INF;
        double shift = 0.0;
        if (!uf && !lf) continue;
        for (p = 0; p < nv; p++) row[p] = 0.0;
        for (j = 0; j < n; j++) {
            int lj = l[j] > -GOR_LP_INF, uj = u[j] < GOR_LP_INF;
            double a = A[(size_t)i * n + j];
            if (lj) { row[vof[j]] += a; shift += a * l[j]; }
            else if (uj) { row[vof[j]] += -a; shift += a * u[j]; }
            else { row[vof[j]] += a; row[vof[j] + 1] += -a; }
        }
        if (uf) {   /* Ax <= b : + slack */
            double rhs = b[i] - shift;
            for (p = 0; p < nv; p++) AA[(size_t)k * nv + p] = row[p];
            AA[(size_t)k * nv + nvb + k] = 1.0;
            if (rhs < 0.0) { for (p = 0; p < nv; p++) AA[(size_t)k * nv + p] = -AA[(size_t)k * nv + p]; rhs = -rhs; }
            bb[k] = rhs; k++;
        }
        if (lf) {   /* Ax >= s : - surplus */
            double rhs = s[i] - shift;
            for (p = 0; p < nv; p++) AA[(size_t)k * nv + p] = row[p];
            AA[(size_t)k * nv + nvb + k] = -1.0;
            if (rhs < 0.0) { for (p = 0; p < nv; p++) AA[(size_t)k * nv + p] = -AA[(size_t)k * nv + p]; rhs = -rhs; }
            bb[k] = rhs; k++;
        }
    }
    for (j = 0; j < n; j++) {
        if (l[j] > -GOR_LP_INF && u[j] < GOR_LP_INF) {   /* y_j <= cap */
            for (p = 0; p < nv; p++) AA[(size_t)k * nv + p] = 0.0;
            AA[(size_t)k * nv + vof[j]] = 1.0;
            AA[(size_t)k * nv + nvb + k] = 1.0;
            bb[k] = u[j] - l[j]; k++;
        }
    }
    rc = gor_lp_solve(nci, nv, AA, bb, cc, yy, yydual, &objconst, 20000);
    if (rc == 0) {
        for (j = 0; j < n; j++) {
            int lf = l[j] > -GOR_LP_INF, uf = u[j] < GOR_LP_INF;
            double y = yy[vof[j]];
            if (lf) x[j] = l[j] + y;
            else if (uf) x[j] = u[j] - y;
            else x[j] = yy[vof[j]] - yy[vof[j] + 1];
        }
    }
    free(vof); free(cc); free(AA); free(bb); free(yy); free(yydual); free(row);
    return rc == 0 ? 0 : (rc == 1 ? 1 : (rc == 2 ? 2 : -1));
}

/* ===================== Revised simplex (da athityakumar/or_lab) ===================== */

int gor_lp_revised_simplex(int m, int n, const double *A, const double *b,
                           const double *c, const int *basis, int max_iter, double *x)
{
    double *Binv, *xB, *y, *d, *cB;
    int *bas;
    int i, j, k, it, rc = 0;
    if (m < 1 || n < 1 || !A || !b || !c || !basis || !x) return 4;
    Binv = (double *)malloc((size_t)m * m * sizeof(double));
    xB = (double *)malloc((size_t)m * sizeof(double));
    y = (double *)malloc((size_t)m * sizeof(double));
    d = (double *)malloc((size_t)m * sizeof(double));
    cB = (double *)malloc((size_t)m * sizeof(double));
    bas = (int *)malloc((size_t)m * sizeof(int));
    if (!Binv || !xB || !y || !d || !cB || !bas) {
        free(Binv); free(xB); free(y); free(d); free(cB); free(bas); return 4;
    }
    /* [B | I] -> B^-1 nella meta' destra */
    {
        double *W = (double *)malloc((size_t)m * 2 * m * sizeof(double));
        if (!W) { free(Binv); free(xB); free(y); free(d); free(cB); free(bas); return 4; }
        for (i = 0; i < m; i++) {
            for (j = 0; j < m; j++) W[(size_t)i * 2 * m + j] = A[(size_t)i * n + basis[j]];
            for (j = 0; j < m; j++) W[(size_t)i * 2 * m + m + j] = (i == j) ? 1.0 : 0.0;
        }
        for (k = 0; k < m; k++) {
            int piv = k; double mx = fabs(W[(size_t)k * 2 * m + k]);
            for (i = k + 1; i < m; i++) { double v = fabs(W[(size_t)i * 2 * m + k]); if (v > mx) { mx = v; piv = i; } }
            if (mx < 1e-12) { free(W); free(Binv); free(xB); free(y); free(d); free(cB); free(bas); return 4; }
            if (piv != k) for (j = 0; j < 2 * m; j++) { double t = W[(size_t)k*2*m+j]; W[(size_t)k*2*m+j] = W[(size_t)piv*2*m+j]; W[(size_t)piv*2*m+j] = t; }
            { double p = W[(size_t)k * 2 * m + k]; for (j = 0; j < 2 * m; j++) W[(size_t)k * 2 * m + j] /= p; }
            for (i = 0; i < m; i++) {
                double f;
                if (i == k) continue;
                f = W[(size_t)i * 2 * m + k];
                if (f == 0.0) continue;
                for (j = 0; j < 2 * m; j++) W[(size_t)i * 2 * m + j] -= f * W[(size_t)k * 2 * m + j];
            }
        }
        for (i = 0; i < m; i++) for (j = 0; j < m; j++) Binv[(size_t)i * m + j] = W[(size_t)i * 2 * m + m + j];
        free(W);
    }
    for (i = 0; i < m; i++) bas[i] = basis[i];
    /* xB = B^-1 b ; base primal-ammissibile? */
    for (i = 0; i < m; i++) { double s = 0.0; for (k = 0; k < m; k++) s += Binv[(size_t)i * m + k] * b[k]; xB[i] = s; }
    for (i = 0; i < m; i++) if (xB[i] < -1e-7) { free(Binv); free(xB); free(y); free(d); free(cB); free(bas); return 1; }
    for (it = 0; it < max_iter; it++) {
        int ent = -1, lev = -1;
        double bestred = -1e-9, bestratio = 1e300;
        for (i = 0; i < m; i++) cB[i] = c[bas[i]];
        /* y = cB^T B^-1 */
        for (j = 0; j < m; j++) { double s = 0.0; for (i = 0; i < m; i++) s += cB[i] * Binv[(size_t)i * m + j]; y[j] = s; }
        for (j = 0; j < n; j++) {
            int isbasic = 0; double red = c[j];
            for (i = 0; i < m; i++) if (bas[i] == j) isbasic = 1;
            if (isbasic) continue;
            for (k = 0; k < m; k++) red -= y[k] * A[(size_t)k * n + j];
            if (red < bestred) { bestred = red; ent = j; }
        }
        if (ent < 0) { rc = 0; break; }
        /* d = B^-1 A_ent */
        for (i = 0; i < m; i++) { double s = 0.0; for (k = 0; k < m; k++) s += Binv[(size_t)i * m + k] * A[(size_t)k * n + ent]; d[i] = s; }
        for (i = 0; i < m; i++) if (d[i] > 1e-9) { double r = xB[i] / d[i]; if (r < bestratio - 1e-12) { bestratio = r; lev = i; } }
        if (lev < 0) { rc = 2; break; }
        /* eta update: B^-1 <- E B^-1 */
        {
            double piv = d[lev];
            for (j = 0; j < m; j++) Binv[(size_t)lev * m + j] /= piv;
            for (i = 0; i < m; i++) {
                double f;
                if (i == lev) continue;
                f = d[i];
                if (f == 0.0) continue;
                for (j = 0; j < m; j++) Binv[(size_t)i * m + j] -= f * Binv[(size_t)lev * m + j];
            }
        }
        bas[lev] = ent;
        for (i = 0; i < m; i++) { double s = 0.0; for (k = 0; k < m; k++) s += Binv[(size_t)i * m + k] * b[k]; xB[i] = s; }
        if (it == max_iter - 1) rc = 3;
    }
    for (i = 0; i < n; i++) x[i] = 0.0;
    for (i = 0; i < m; i++) if (bas[i] >= 0 && bas[i] < n) x[bas[i]] = xB[i];
    free(Binv); free(xB); free(y); free(d); free(cB); free(bas);
    return rc;
}

/* ===================== Costi ridotti (da column generation) ===================== */

void gor_lp_reduced_costs(int m, int n, const double *A, const double *c,
                          const double *y, double *red)
{
    int i, j;
    if (!A || !c || !y || !red || m < 1 || n < 1) return;
    for (j = 0; j < n; j++) {
        double s = c[j];
        for (i = 0; i < m; i++) s -= y[i] * A[(size_t)i * n + j];
        red[j] = s;
    }
}
