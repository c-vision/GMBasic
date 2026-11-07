/*
 * or_ffi.c — shim ABI piatto per il binding BASIC del namespace OPTIMIZE (dominio OR).
 *
 * Copre pack / assign / cover / cp (prima slice di ortools.inc). gmbortools usa int/double;
 * l'FFI di GMBasic passa LONG come int64, array LONG come int64* e array DOUBLE come double*.
 * Dove la firma C supera gli 8 argomenti FFI, i parametri "di servizio" sono compattati
 * o fissati nello shim (documentato in ortools.inc).
 */
#include "gor_assign.h"
#include "gor_cover.h"
#include "gor_cp.h"
#include "gor_pack.h"

#include <stdlib.h>
#include <stdint.h>

typedef long long i64;

static int *to_iarr(const i64 *a, i64 n)
{
    i64 i;
    int *r;
    if (n <= 0 || a == NULL) return NULL;
    r = (int *)malloc((size_t)n * sizeof(int));
    if (r == NULL) return NULL;
    for (i = 0; i < n; i++) r[i] = (int)a[i];
    return r;
}

static void out_iarr(const int *a, i64 *out, i64 n)
{
    i64 i;
    for (i = 0; i < n; i++) out[i] = (i64)a[i];
}

/* ═══════════════════════════════ PACK ═══════════════════════════════ */

i64 ofx_bin_packing(const double *items, int64_t items_n, i64 n, double cap, i64 algo, i64 *bin_of, int64_t bin_of_n)
{
    int *b;
    int r;
    if (n <= 0 || items == NULL || bin_of == NULL) return -1;
    b = (int *)malloc((size_t)n * sizeof(int));
    if (b == NULL) return -1;
    r = gor_bin_packing((int)n, (const gor_real *)items, (gor_real)cap, (int)algo, b);
    out_iarr(b, bin_of, n);
    free(b);
    return (i64)r;
}

i64 ofx_bin_packing_lb(const double *items, int64_t items_n, i64 n, double cap)
{
    if (n <= 0 || items == NULL) return -1;
    return (i64)gor_bin_packing_lb((int)n, (const gor_real *)items, (gor_real)cap);
}

double ofx_knapsack_frac(const double *w, int64_t w_n, const double *v, int64_t v_n, i64 n, double cap, double *x, int64_t x_n)
{
    if (n <= 0 || w == NULL || v == NULL || x == NULL) return -1.0;
    return (double)gor_knapsack_frac((int)n, (const gor_real *)w, (const gor_real *)v,
                                     (gor_real)cap, (gor_real *)x);
}

double ofx_knapsack_01(const double *w, int64_t w_n, const double *v, int64_t v_n, i64 n, i64 cap, i64 *take, int64_t take_n)
{
    int *t;
    double r;
    if (n <= 0 || w == NULL || v == NULL || take == NULL) return -1.0;
    t = (int *)malloc((size_t)n * sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_knapsack_01((int)n, (const gor_real *)w, (const gor_real *)v, (int)cap, t);
    out_iarr(t, take, n);
    free(t);
    return r;
}

double ofx_knapsack_01_mitm(const double *w, int64_t w_n, const double *v, int64_t v_n, i64 n, double cap, i64 *take, int64_t take_n)
{
    int *t;
    double r;
    if (n <= 0 || w == NULL || v == NULL || take == NULL) return -1.0;
    t = (int *)malloc((size_t)n * sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_knapsack_01_mitm((int)n, (const gor_real *)w, (const gor_real *)v,
                                     (gor_real)cap, t);
    out_iarr(t, take, n);
    free(t);
    return r;
}

double ofx_knapsack_unbounded(const double *w, int64_t w_n, const double *v, int64_t v_n, i64 n, i64 cap, i64 *counts, int64_t counts_n)
{
    int *c;
    double r;
    if (n <= 0 || w == NULL || v == NULL || counts == NULL) return -1.0;
    c = (int *)malloc((size_t)n * sizeof(int));
    if (c == NULL) return -1.0;
    r = (double)gor_knapsack_unbounded((int)n, (const gor_real *)w, (const gor_real *)v,
                                       (int)cap, c);
    out_iarr(c, counts, n);
    free(c);
    return r;
}

i64 ofx_subset_sum(const double *items, int64_t items_n, i64 n, double target, i64 *take, int64_t take_n)
{
    int *t;
    int r;
    if (n <= 0 || items == NULL || take == NULL) return -1;
    t = (int *)malloc((size_t)n * sizeof(int));
    if (t == NULL) return -1;
    r = gor_subset_sum((int)n, (const gor_real *)items, (gor_real)target, t);
    out_iarr(t, take, n);
    free(t);
    return (i64)r;
}

i64 ofx_cutting_stock(const double *pieces, int64_t pieces_n, i64 n, double stock, i64 *pattern, int64_t pattern_n)
{
    int *p;
    int r;
    if (n <= 0 || pieces == NULL || pattern == NULL) return -1;
    p = (int *)malloc((size_t)n * sizeof(int));
    if (p == NULL) return -1;
    r = gor_cutting_stock_ffd((int)n, (const gor_real *)pieces, (gor_real)stock, p);
    out_iarr(p, pattern, n);
    free(p);
    return (i64)r;
}

i64 ofx_bin_packing_3d(const double *items, int64_t items_n, i64 n, double W, double H, double D, i64 *bin_of, int64_t bin_of_n)
{
    int *b;
    int r;
    if (n <= 0 || items == NULL || bin_of == NULL) return -1;
    b = (int *)malloc((size_t)n * sizeof(int));
    if (b == NULL) return -1;
    r = gor_bin_packing_3d((int)n, (const gor_real *)items, (gor_real)W, (gor_real)H,
                           (gor_real)D, b);
    out_iarr(b, bin_of, n);
    free(b);
    return (i64)r;
}

/* ═══════════════════════════════ ASSIGN ═══════════════════════════════ */

double ofx_hungarian(const double *cost, int64_t cost_n, i64 n, i64 m, i64 *assignment, int64_t assignment_n)
{
    int *a;
    double r;
    if (n <= 0 || m <= 0 || cost == NULL || assignment == NULL) return -1.0;
    a = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL) return -1.0;
    r = (double)gor_assign_hungarian((int)n, (int)m, (const gor_real *)cost, a);
    out_iarr(a, assignment, n);
    free(a);
    return r;
}

double ofx_auction(const double *cost, int64_t cost_n, i64 n, i64 m, i64 *assignment, int64_t assignment_n, double eps)
{
    int *a;
    double r;
    if (n <= 0 || m <= 0 || cost == NULL || assignment == NULL) return -1.0;
    a = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL) return -1.0;
    r = (double)gor_assign_auction((int)n, (int)m, (const gor_real *)cost, a, (gor_real)eps);
    out_iarr(a, assignment, n);
    free(a);
    return r;
}

double ofx_bottleneck(const double *cost, int64_t cost_n, i64 n, i64 *assignment, int64_t assignment_n)
{
    int *a;
    double r;
    if (n <= 0 || cost == NULL || assignment == NULL) return -1.0;
    a = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL) return -1.0;
    r = (double)gor_assign_bottleneck((int)n, (const gor_real *)cost, a);
    out_iarr(a, assignment, n);
    free(a);
    return r;
}

i64 ofx_stable_marriage(const i64 *pref_men, int64_t pref_men_n, const i64 *pref_women, int64_t pref_women_n, i64 n, i64 *match_men, int64_t match_men_n)
{
    int *pm, *pw, *mm;
    gor_status r;
    if (n <= 0 || pref_men == NULL || pref_women == NULL || match_men == NULL) return 0;
    pm = to_iarr(pref_men, n * n);
    pw = to_iarr(pref_women, n * n);
    mm = (int *)malloc((size_t)n * sizeof(int));
    if (pm == NULL || pw == NULL || mm == NULL) { free(pm); free(pw); free(mm); return 0; }
    r = gor_stable_marriage((int)n, pm, pw, mm);
    out_iarr(mm, match_men, n);
    free(pm);
    free(pw);
    free(mm);
    return (r == GOR_OK) ? 1 : 0;
}

double ofx_transport_vam(const double *supply, int64_t supply_n, const double *demand, int64_t demand_n, const double *cost, int64_t cost_n,
                         i64 m, i64 n, double *x, int64_t x_n)
{
    if (m <= 0 || n <= 0 || supply == NULL || demand == NULL || cost == NULL || x == NULL) return -1.0;
    return (double)gor_transport_vam((int)m, (int)n, (const gor_real *)supply,
                                     (const gor_real *)demand, (const gor_real *)cost,
                                     (gor_real *)x);
}

double ofx_transport_northwest(const double *supply, int64_t supply_n, const double *demand, int64_t demand_n, const double *cost, int64_t cost_n,
                               i64 m, i64 n, double *x, int64_t x_n)
{
    if (m <= 0 || n <= 0 || supply == NULL || demand == NULL || cost == NULL || x == NULL) return -1.0;
    return (double)gor_transport_northwest((int)m, (int)n, (const gor_real *)supply,
                                           (const gor_real *)demand, (const gor_real *)cost,
                                           (gor_real *)x);
}

double ofx_transport_least_cost(const double *supply, int64_t supply_n, const double *demand, int64_t demand_n, const double *cost, int64_t cost_n,
                                i64 m, i64 n, double *x, int64_t x_n)
{
    if (m <= 0 || n <= 0 || supply == NULL || demand == NULL || cost == NULL || x == NULL) return -1.0;
    return (double)gor_transport_least_cost((int)m, (int)n, (const gor_real *)supply,
                                            (const gor_real *)demand, (const gor_real *)cost,
                                            (gor_real *)x);
}

double ofx_transport_modi(const double *supply, int64_t supply_n, const double *demand, int64_t demand_n, const double *cost, int64_t cost_n,
                          i64 m, i64 n, double *x, int64_t x_n, i64 maxit)
{
    if (m <= 0 || n <= 0 || supply == NULL || demand == NULL || cost == NULL || x == NULL) return -1.0;
    return (double)gor_transport_modi((int)m, (int)n, (const gor_real *)supply,
                                      (const gor_real *)demand, (const gor_real *)cost,
                                      (gor_real *)x, (int)maxit);
}

double ofx_qap_tabu(const double *A, int64_t A_n, const double *B, int64_t B_n, i64 n, i64 *p, int64_t p_n, i64 maxit, i64 seed)
{
    int *pp;
    double r;
    if (n <= 0 || A == NULL || B == NULL || p == NULL) return -1.0;
    pp = to_iarr(p, n);
    if (pp == NULL) return -1.0;
    r = (double)gor_assign_qap_tabu((int)n, (const gor_real *)A, (const gor_real *)B, pp,
                                    (int)maxit, (unsigned long)seed);
    out_iarr(pp, p, n);
    free(pp);
    return r;
}

/* ═══════════════════════════════ COVER ═══════════════════════════════ */

double ofx_set_cover(const i64 *elem, int64_t elem_n, const double *cost, int64_t cost_n, i64 m, i64 n,
                     i64 *chosen, int64_t chosen_n, i64 *n_chosen, int64_t n_chosen_n)
{
    int *e, *c;
    double r;
    int nc = 0;
    if (m <= 0 || n <= 0 || elem == NULL || cost == NULL || chosen == NULL || n_chosen == NULL) return -1.0;
    e = to_iarr(elem, m * n);
    c = (int *)malloc((size_t)n * sizeof(int));
    if (e == NULL || c == NULL) { free(e); free(c); return -1.0; }
    r = (double)gor_set_cover_greedy((int)m, (int)n, e, (const gor_real *)cost, c, &nc);
    out_iarr(c, chosen, nc);
    n_chosen[0] = (i64)nc;
    free(e);
    free(c);
    return r;
}

double ofx_set_cover_soft(const i64 *elem, int64_t elem_n, const double *cost, int64_t cost_n, i64 m, i64 n,
                          const i64 *chosen, int64_t chosen_n, double pen, i64 *uncovered, int64_t uncovered_n)
{
    int *e;
    char *ch;
    double r;
    int u = -1;
    i64 i;
    if (m <= 0 || n <= 0 || elem == NULL || cost == NULL || chosen == NULL || uncovered == NULL) return -1.0;
    e = to_iarr(elem, m * n);
    ch = (char *)malloc((size_t)n);
    if (e == NULL || ch == NULL) { free(e); free(ch); return -1.0; }
    for (i = 0; i < n; i++) ch[i] = chosen[i] ? 1 : 0;
    r = (double)gor_set_cover_soft_cost((int)m, (int)n, e, (const gor_real *)cost, ch,
                                        (gor_real)pen, &u);
    uncovered[0] = (i64)u;
    free(e);
    free(ch);
    return r;
}

double ofx_set_cover_pricing(const double *y, int64_t y_n, i64 m, double base_cost, double cost_per_elem,
                             i64 *chosen_set, int64_t chosen_set_n)
{
    char *c;
    double r;
    i64 i;
    if (m <= 0 || y == NULL || chosen_set == NULL) return -1.0;
    c = (char *)malloc((size_t)m);
    if (c == NULL) return -1.0;
    r = (double)gor_set_cover_pricing_greedy((int)m, (const gor_real *)y, (gor_real)base_cost,
                                             (gor_real)cost_per_elem, c);
    for (i = 0; i < m; i++) chosen_set[i] = c[i] ? 1 : 0;
    free(c);
    return r;
}

double ofx_set_cover_sa(const i64 *elem, int64_t elem_n, const double *cost, int64_t cost_n, i64 m, i64 n,
                        double pen, i64 maxit, i64 seed, i64 *chosen, int64_t chosen_n)
{
    int *e;
    char *ch;
    double r;
    i64 i;
    if (m <= 0 || n <= 0 || elem == NULL || cost == NULL || chosen == NULL) return -1.0;
    e = to_iarr(elem, m * n);
    ch = (char *)malloc((size_t)n);
    if (e == NULL || ch == NULL) { free(e); free(ch); return -1.0; }
    for (i = 0; i < n; i++) ch[i] = chosen[i] ? 1 : 0;
    r = (double)gor_set_cover_sa((int)m, (int)n, e, (const gor_real *)cost, (gor_real)pen,
                                 (int)maxit, 1.0, 0.995, (unsigned long long)seed, ch);
    for (i = 0; i < n; i++) chosen[i] = ch[i] ? 1 : 0;
    free(e);
    free(ch);
    return r;
}

double ofx_set_cover_local(const i64 *elem, int64_t elem_n, const double *cost, int64_t cost_n, i64 m, i64 n,
                           i64 *chosen, int64_t chosen_n, i64 *n_chosen, int64_t n_chosen_n, i64 maxit)
{
    int *e, *c;
    double r;
    int nc;
    if (m <= 0 || n <= 0 || elem == NULL || cost == NULL || chosen == NULL || n_chosen == NULL) return -1.0;
    e = to_iarr(elem, m * n);
    c = (int *)malloc((size_t)n * sizeof(int));
    if (e == NULL || c == NULL) { free(e); free(c); return -1.0; }
    nc = (int)n_chosen[0];
    { i64 i; for (i = 0; i < n; i++) c[i] = (int)chosen[i]; }
    r = (double)gor_set_cover_local_search((int)m, (int)n, e, (const gor_real *)cost, c, &nc,
                                           (int)maxit);
    out_iarr(c, chosen, nc);
    n_chosen[0] = (i64)nc;
    free(e);
    free(c);
    return r;
}

double ofx_set_cover_grasp(const i64 *elem, int64_t elem_n, const double *cost, int64_t cost_n, i64 m, i64 n,
                           double alpha, i64 maxit, i64 *chosen, int64_t chosen_n, i64 *n_chosen, int64_t n_chosen_n)
{
    int *e, *c;
    double r;
    int nc = 0;
    if (m <= 0 || n <= 0 || elem == NULL || cost == NULL || chosen == NULL || n_chosen == NULL) return -1.0;
    e = to_iarr(elem, m * n);
    c = (int *)malloc((size_t)n * sizeof(int));
    if (e == NULL || c == NULL) { free(e); free(c); return -1.0; }
    r = (double)gor_set_cover_grasp((int)m, (int)n, e, (const gor_real *)cost, (gor_real)alpha,
                                    (int)maxit, 12345ULL, c, &nc);
    out_iarr(c, chosen, nc);
    n_chosen[0] = (i64)nc;
    free(e);
    free(c);
    return r;
}

/* alphas() = [nalpha, a0, a1, ...] (nalpha compatto in testa; seed fisso). */
double ofx_set_cover_rgrasp(const i64 *elem, int64_t elem_n, const double *cost, int64_t cost_n, i64 m, i64 n,
                            const double *alphas, int64_t alphas_n, i64 maxit, i64 *chosen, int64_t chosen_n, i64 *n_chosen, int64_t n_chosen_n)
{
    int *e, *c;
    double r;
    int nc = 0;
    int nalpha;
    if (m <= 0 || n <= 0 || elem == NULL || cost == NULL || alphas == NULL ||
        chosen == NULL || n_chosen == NULL) return -1.0;
    nalpha = (int)alphas[0];
    if (nalpha <= 0) return -1.0;
    e = to_iarr(elem, m * n);
    c = (int *)malloc((size_t)n * sizeof(int));
    if (e == NULL || c == NULL) { free(e); free(c); return -1.0; }
    r = (double)gor_set_cover_reactive_grasp((int)m, (int)n, e, (const gor_real *)cost,
                                             (const gor_real *)(alphas + 1), nalpha,
                                             (int)maxit, 12345ULL, c, &nc);
    out_iarr(c, chosen, nc);
    n_chosen[0] = (i64)nc;
    free(e);
    free(c);
    return r;
}

i64 ofx_max_coverage(const i64 *elem, int64_t elem_n, i64 m, i64 n, i64 k, i64 *chosen, int64_t chosen_n, i64 *n_chosen, int64_t n_chosen_n)
{
    int *e, *c;
    int nc = 0;
    int r;
    if (m <= 0 || n <= 0 || elem == NULL || chosen == NULL || n_chosen == NULL) return -1;
    e = to_iarr(elem, m * n);
    c = (int *)malloc((size_t)n * sizeof(int));
    if (e == NULL || c == NULL) { free(e); free(c); return -1; }
    r = gor_max_coverage((int)m, (int)n, e, (int)k, c, &nc);
    out_iarr(c, chosen, nc);
    n_chosen[0] = (i64)nc;
    free(e);
    free(c);
    return (i64)r;
}

double ofx_pmedian(const double *dist, int64_t dist_n, i64 n, i64 p, i64 *medians, int64_t medians_n, i64 *assign, int64_t assign_n, i64 maxit)
{
    int *md, *as;
    double r;
    if (n <= 0 || p <= 0 || dist == NULL || medians == NULL || assign == NULL) return -1.0;
    md = (int *)malloc((size_t)p * sizeof(int));
    as = (int *)malloc((size_t)n * sizeof(int));
    if (md == NULL || as == NULL) { free(md); free(as); return -1.0; }
    r = (double)gor_pmedian((int)n, (int)p, (const gor_real *)dist, md, as, (int)maxit);
    out_iarr(md, medians, p);
    out_iarr(as, assign, n);
    free(md);
    free(as);
    return r;
}

double ofx_pcenter(const double *dist, int64_t dist_n, i64 n, i64 p, i64 *medians, int64_t medians_n)
{
    int *md;
    double r;
    if (n <= 0 || p <= 0 || dist == NULL || medians == NULL) return -1.0;
    md = (int *)malloc((size_t)p * sizeof(int));
    if (md == NULL) return -1.0;
    r = (double)gor_pcenter((int)n, (int)p, (const gor_real *)dist, md);
    out_iarr(md, medians, p);
    free(md);
    return r;
}

i64 ofx_vertex_cover(const i64 *edges, int64_t edges_n, i64 n, i64 ne, i64 *cover, int64_t cover_n)
{
    int *e, *c;
    int r;
    if (n <= 0 || ne < 0 || edges == NULL || cover == NULL) return -1;
    e = to_iarr(edges, 2 * ne);
    c = (int *)malloc((size_t)n * sizeof(int));
    if (e == NULL || c == NULL) { free(e); free(c); return -1; }
    r = gor_vertex_cover_2approx((int)n, e, (int)ne, c);
    out_iarr(c, cover, n);
    free(e);
    free(c);
    return (i64)r;
}

i64 ofx_hitting_set(const i64 *elem, int64_t elem_n, i64 m, i64 n, i64 *chosen, int64_t chosen_n, i64 *n_chosen, int64_t n_chosen_n)
{
    int *e, *c;
    int nc = 0;
    int r;
    if (m <= 0 || n <= 0 || elem == NULL || chosen == NULL || n_chosen == NULL) return -1;
    e = to_iarr(elem, m * n);
    c = (int *)malloc((size_t)n * sizeof(int));
    if (e == NULL || c == NULL) { free(e); free(c); return -1; }
    r = gor_hitting_set_greedy((int)m, (int)n, e, c, &nc);
    out_iarr(c, chosen, nc);
    n_chosen[0] = (i64)nc;
    free(e);
    free(c);
    return (i64)r;
}

double ofx_facility_location(const double *open_cost, int64_t open_cost_n, const double *conn_cost, int64_t conn_cost_n, i64 m, i64 n,
                             i64 *open, int64_t open_n)
{
    int *o;
    double r;
    if (m <= 0 || n <= 0 || open_cost == NULL || conn_cost == NULL || open == NULL) return -1.0;
    o = (int *)malloc((size_t)n * sizeof(int));
    if (o == NULL) return -1.0;
    r = (double)gor_facility_location_greedy((int)m, (int)n, (const gor_real *)open_cost,
                                             (const gor_real *)conn_cost, o);
    out_iarr(o, open, n);
    free(o);
    return r;
}

double ofx_kmeans(const double *points, int64_t points_n, i64 n, i64 dim, i64 k, double *centroids, int64_t centroids_n,
                  i64 *labels, int64_t labels_n, i64 maxit)
{
    int *l;
    double r;
    if (n <= 0 || dim <= 0 || k <= 0 || points == NULL || centroids == NULL || labels == NULL) return -1.0;
    l = (int *)malloc((size_t)n * sizeof(int));
    if (l == NULL) return -1.0;
    r = (double)gor_kmeans((int)n, (int)dim, (int)k, (const gor_real *)points,
                           (gor_real *)centroids, l, (int)maxit, 12345UL);
    out_iarr(l, labels, n);
    free(l);
    return r;
}

/* ═══════════════════════════════ CP ═══════════════════════════════ */

i64 ofx_sat(const i64 *clauses, int64_t clauses_n, const i64 *clause_start, int64_t clause_start_n, const i64 *clause_len, int64_t clause_len_n,
            i64 nvars, i64 nclauses, i64 *assignment, int64_t assignment_n)
{
    int *cl, *cs, *cln, *as;
    int total, r;
    if (nvars <= 0 || nclauses <= 0 || clauses == NULL || clause_start == NULL ||
        clause_len == NULL || assignment == NULL) return -1;
    total = 0;
    { i64 i; for (i = 0; i < nclauses; i++) total += (int)clause_len[i]; }
    cl = to_iarr(clauses, total);
    cs = to_iarr(clause_start, nclauses);
    cln = to_iarr(clause_len, nclauses);
    as = (int *)malloc((size_t)nvars * sizeof(int));
    if (cl == NULL || cs == NULL || cln == NULL || as == NULL) {
        free(cl); free(cs); free(cln); free(as); return -1;
    }
    r = gor_sat_dpll((int)nvars, (int)nclauses, cl, cs, cln, as);
    out_iarr(as, assignment, nvars);
    free(cl); free(cs); free(cln); free(as);
    return (i64)r;
}

i64 ofx_alldiff(const i64 *dom, int64_t dom_n, i64 n, i64 D, i64 *sol, int64_t sol_n)
{
    int *d, *s;
    int r;
    if (n <= 0 || D <= 0 || dom == NULL || sol == NULL) return -1;
    d = to_iarr(dom, n * D);
    s = (int *)malloc((size_t)n * sizeof(int));
    if (d == NULL || s == NULL) { free(d); free(s); return -1; }
    r = gor_cp_alldiff((int)n, (int)D, d, s);
    out_iarr(s, sol, n);
    free(d);
    free(s);
    return (i64)r;
}

i64 ofx_cp_binary(const i64 *dom, int64_t dom_n, const i64 *allowed, int64_t allowed_n, i64 n, i64 D, i64 *sol, int64_t sol_n)
{
    int *d, *a, *s;
    int r;
    if (n <= 0 || D <= 0 || dom == NULL || allowed == NULL || sol == NULL) return -1;
    d = to_iarr(dom, n * D);
    /* allowed[] e' n*n*D*D: gor_cp_binary lo indicizza come
     * allowed[((j*n + i)*D*D) + v_j*D + v_i] (coppie di variabili x valori). Con n*n
     * il buffer era troppo corto e il motore leggeva fuori allocazione -> nessuna
     * soluzione (0) su C99/VM-C/PCode, esito casuale su IL (2026-11-04). */
    a = to_iarr(allowed, n * n * D * D);
    s = (int *)malloc((size_t)n * sizeof(int));
    if (d == NULL || a == NULL || s == NULL) { free(d); free(a); free(s); return -1; }
    r = gor_cp_binary((int)n, (int)D, d, a, s);
    out_iarr(s, sol, n);
    free(d); free(a); free(s);
    return (i64)r;
}

#include <stdint.h>
/* Trampolini DECLARE NATIVE (ffi_ext_native.pl): firma originale, estensioni = INT64_MAX. */
i64 ofx_alldiff__nat(const i64 *dom, i64 n, i64 D, i64 *sol) { return ofx_alldiff(dom, INT64_MAX, n, D, sol, INT64_MAX); }
double ofx_auction__nat(const double *cost, i64 n, i64 m, i64 *assignment, double eps) { return ofx_auction(cost, INT64_MAX, n, m, assignment, INT64_MAX, eps); }
i64 ofx_bin_packing__nat(const double *items, i64 n, double cap, i64 algo, i64 *bin_of) { return ofx_bin_packing(items, INT64_MAX, n, cap, algo, bin_of, INT64_MAX); }
i64 ofx_bin_packing_3d__nat(const double *items, i64 n, double W, double H, double D, i64 *bin_of) { return ofx_bin_packing_3d(items, INT64_MAX, n, W, H, D, bin_of, INT64_MAX); }
i64 ofx_bin_packing_lb__nat(const double *items, i64 n, double cap) { return ofx_bin_packing_lb(items, INT64_MAX, n, cap); }
double ofx_bottleneck__nat(const double *cost, i64 n, i64 *assignment) { return ofx_bottleneck(cost, INT64_MAX, n, assignment, INT64_MAX); }
i64 ofx_cp_binary__nat(const i64 *dom, const i64 *allowed, i64 n, i64 D, i64 *sol) { return ofx_cp_binary(dom, INT64_MAX, allowed, INT64_MAX, n, D, sol, INT64_MAX); }
i64 ofx_cutting_stock__nat(const double *pieces, i64 n, double stock, i64 *pattern) { return ofx_cutting_stock(pieces, INT64_MAX, n, stock, pattern, INT64_MAX); }
double ofx_facility_location__nat(const double *open_cost, const double *conn_cost, i64 m, i64 n, i64 *open) { return ofx_facility_location(open_cost, INT64_MAX, conn_cost, INT64_MAX, m, n, open, INT64_MAX); }
i64 ofx_hitting_set__nat(const i64 *elem, i64 m, i64 n, i64 *chosen, i64 *n_chosen) { return ofx_hitting_set(elem, INT64_MAX, m, n, chosen, INT64_MAX, n_chosen, INT64_MAX); }
double ofx_hungarian__nat(const double *cost, i64 n, i64 m, i64 *assignment) { return ofx_hungarian(cost, INT64_MAX, n, m, assignment, INT64_MAX); }
double ofx_kmeans__nat(const double *points, i64 n, i64 dim, i64 k, double *centroids, i64 *labels, i64 maxit) { return ofx_kmeans(points, INT64_MAX, n, dim, k, centroids, INT64_MAX, labels, INT64_MAX, maxit); }
double ofx_knapsack_01__nat(const double *w, const double *v, i64 n, i64 cap, i64 *take) { return ofx_knapsack_01(w, INT64_MAX, v, INT64_MAX, n, cap, take, INT64_MAX); }
double ofx_knapsack_01_mitm__nat(const double *w, const double *v, i64 n, double cap, i64 *take) { return ofx_knapsack_01_mitm(w, INT64_MAX, v, INT64_MAX, n, cap, take, INT64_MAX); }
double ofx_knapsack_frac__nat(const double *w, const double *v, i64 n, double cap, double *x) { return ofx_knapsack_frac(w, INT64_MAX, v, INT64_MAX, n, cap, x, INT64_MAX); }
double ofx_knapsack_unbounded__nat(const double *w, const double *v, i64 n, i64 cap, i64 *counts) { return ofx_knapsack_unbounded(w, INT64_MAX, v, INT64_MAX, n, cap, counts, INT64_MAX); }
i64 ofx_max_coverage__nat(const i64 *elem, i64 m, i64 n, i64 k, i64 *chosen, i64 *n_chosen) { return ofx_max_coverage(elem, INT64_MAX, m, n, k, chosen, INT64_MAX, n_chosen, INT64_MAX); }
double ofx_pcenter__nat(const double *dist, i64 n, i64 p, i64 *medians) { return ofx_pcenter(dist, INT64_MAX, n, p, medians, INT64_MAX); }
double ofx_pmedian__nat(const double *dist, i64 n, i64 p, i64 *medians, i64 *assign, i64 maxit) { return ofx_pmedian(dist, INT64_MAX, n, p, medians, INT64_MAX, assign, INT64_MAX, maxit); }
double ofx_qap_tabu__nat(const double *A, const double *B, i64 n, i64 *p, i64 maxit, i64 seed) { return ofx_qap_tabu(A, INT64_MAX, B, INT64_MAX, n, p, INT64_MAX, maxit, seed); }
i64 ofx_sat__nat(const i64 *clauses, const i64 *clause_start, const i64 *clause_len, i64 nvars, i64 nclauses, i64 *assignment) { return ofx_sat(clauses, INT64_MAX, clause_start, INT64_MAX, clause_len, INT64_MAX, nvars, nclauses, assignment, INT64_MAX); }
double ofx_set_cover__nat(const i64 *elem, const double *cost, i64 m, i64 n, i64 *chosen, i64 *n_chosen) { return ofx_set_cover(elem, INT64_MAX, cost, INT64_MAX, m, n, chosen, INT64_MAX, n_chosen, INT64_MAX); }
double ofx_set_cover_grasp__nat(const i64 *elem, const double *cost, i64 m, i64 n, double alpha, i64 maxit, i64 *chosen, i64 *n_chosen) { return ofx_set_cover_grasp(elem, INT64_MAX, cost, INT64_MAX, m, n, alpha, maxit, chosen, INT64_MAX, n_chosen, INT64_MAX); }
double ofx_set_cover_local__nat(const i64 *elem, const double *cost, i64 m, i64 n, i64 *chosen, i64 *n_chosen, i64 maxit) { return ofx_set_cover_local(elem, INT64_MAX, cost, INT64_MAX, m, n, chosen, INT64_MAX, n_chosen, INT64_MAX, maxit); }
double ofx_set_cover_pricing__nat(const double *y, i64 m, double base_cost, double cost_per_elem, i64 *chosen_set) { return ofx_set_cover_pricing(y, INT64_MAX, m, base_cost, cost_per_elem, chosen_set, INT64_MAX); }
double ofx_set_cover_rgrasp__nat(const i64 *elem, const double *cost, i64 m, i64 n, const double *alphas, i64 maxit, i64 *chosen, i64 *n_chosen) { return ofx_set_cover_rgrasp(elem, INT64_MAX, cost, INT64_MAX, m, n, alphas, INT64_MAX, maxit, chosen, INT64_MAX, n_chosen, INT64_MAX); }
double ofx_set_cover_sa__nat(const i64 *elem, const double *cost, i64 m, i64 n, double pen, i64 maxit, i64 seed, i64 *chosen) { return ofx_set_cover_sa(elem, INT64_MAX, cost, INT64_MAX, m, n, pen, maxit, seed, chosen, INT64_MAX); }
double ofx_set_cover_soft__nat(const i64 *elem, const double *cost, i64 m, i64 n, const i64 *chosen, double pen, i64 *uncovered) { return ofx_set_cover_soft(elem, INT64_MAX, cost, INT64_MAX, m, n, chosen, INT64_MAX, pen, uncovered, INT64_MAX); }
i64 ofx_stable_marriage__nat(const i64 *pref_men, const i64 *pref_women, i64 n, i64 *match_men) { return ofx_stable_marriage(pref_men, INT64_MAX, pref_women, INT64_MAX, n, match_men, INT64_MAX); }
i64 ofx_subset_sum__nat(const double *items, i64 n, double target, i64 *take) { return ofx_subset_sum(items, INT64_MAX, n, target, take, INT64_MAX); }
double ofx_transport_least_cost__nat(const double *supply, const double *demand, const double *cost, i64 m, i64 n, double *x) { return ofx_transport_least_cost(supply, INT64_MAX, demand, INT64_MAX, cost, INT64_MAX, m, n, x, INT64_MAX); }
double ofx_transport_modi__nat(const double *supply, const double *demand, const double *cost, i64 m, i64 n, double *x, i64 maxit) { return ofx_transport_modi(supply, INT64_MAX, demand, INT64_MAX, cost, INT64_MAX, m, n, x, INT64_MAX, maxit); }
double ofx_transport_northwest__nat(const double *supply, const double *demand, const double *cost, i64 m, i64 n, double *x) { return ofx_transport_northwest(supply, INT64_MAX, demand, INT64_MAX, cost, INT64_MAX, m, n, x, INT64_MAX); }
double ofx_transport_vam__nat(const double *supply, const double *demand, const double *cost, i64 m, i64 n, double *x) { return ofx_transport_vam(supply, INT64_MAX, demand, INT64_MAX, cost, INT64_MAX, m, n, x, INT64_MAX); }
i64 ofx_vertex_cover__nat(const i64 *edges, i64 n, i64 ne, i64 *cover) { return ofx_vertex_cover(edges, INT64_MAX, n, ne, cover, INT64_MAX); }
