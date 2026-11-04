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

i64 ofx_bin_packing(const double *items, i64 n, double cap, i64 algo, i64 *bin_of)
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

i64 ofx_bin_packing_lb(const double *items, i64 n, double cap)
{
    if (n <= 0 || items == NULL) return -1;
    return (i64)gor_bin_packing_lb((int)n, (const gor_real *)items, (gor_real)cap);
}

double ofx_knapsack_frac(const double *w, const double *v, i64 n, double cap, double *x)
{
    if (n <= 0 || w == NULL || v == NULL || x == NULL) return -1.0;
    return (double)gor_knapsack_frac((int)n, (const gor_real *)w, (const gor_real *)v,
                                     (gor_real)cap, (gor_real *)x);
}

double ofx_knapsack_01(const double *w, const double *v, i64 n, i64 cap, i64 *take)
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

double ofx_knapsack_01_mitm(const double *w, const double *v, i64 n, double cap, i64 *take)
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

double ofx_knapsack_unbounded(const double *w, const double *v, i64 n, i64 cap, i64 *counts)
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

i64 ofx_subset_sum(const double *items, i64 n, double target, i64 *take)
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

i64 ofx_cutting_stock(const double *pieces, i64 n, double stock, i64 *pattern)
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

i64 ofx_bin_packing_3d(const double *items, i64 n, double W, double H, double D, i64 *bin_of)
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

double ofx_hungarian(const double *cost, i64 n, i64 m, i64 *assignment)
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

double ofx_auction(const double *cost, i64 n, i64 m, i64 *assignment, double eps)
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

double ofx_bottleneck(const double *cost, i64 n, i64 *assignment)
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

i64 ofx_stable_marriage(const i64 *pref_men, const i64 *pref_women, i64 n, i64 *match_men)
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

double ofx_transport_vam(const double *supply, const double *demand, const double *cost,
                         i64 m, i64 n, double *x)
{
    if (m <= 0 || n <= 0 || supply == NULL || demand == NULL || cost == NULL || x == NULL) return -1.0;
    return (double)gor_transport_vam((int)m, (int)n, (const gor_real *)supply,
                                     (const gor_real *)demand, (const gor_real *)cost,
                                     (gor_real *)x);
}

double ofx_transport_northwest(const double *supply, const double *demand, const double *cost,
                               i64 m, i64 n, double *x)
{
    if (m <= 0 || n <= 0 || supply == NULL || demand == NULL || cost == NULL || x == NULL) return -1.0;
    return (double)gor_transport_northwest((int)m, (int)n, (const gor_real *)supply,
                                           (const gor_real *)demand, (const gor_real *)cost,
                                           (gor_real *)x);
}

double ofx_transport_least_cost(const double *supply, const double *demand, const double *cost,
                                i64 m, i64 n, double *x)
{
    if (m <= 0 || n <= 0 || supply == NULL || demand == NULL || cost == NULL || x == NULL) return -1.0;
    return (double)gor_transport_least_cost((int)m, (int)n, (const gor_real *)supply,
                                            (const gor_real *)demand, (const gor_real *)cost,
                                            (gor_real *)x);
}

double ofx_transport_modi(const double *supply, const double *demand, const double *cost,
                          i64 m, i64 n, double *x, i64 maxit)
{
    if (m <= 0 || n <= 0 || supply == NULL || demand == NULL || cost == NULL || x == NULL) return -1.0;
    return (double)gor_transport_modi((int)m, (int)n, (const gor_real *)supply,
                                      (const gor_real *)demand, (const gor_real *)cost,
                                      (gor_real *)x, (int)maxit);
}

double ofx_qap_tabu(const double *A, const double *B, i64 n, i64 *p, i64 maxit, i64 seed)
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

double ofx_set_cover(const i64 *elem, const double *cost, i64 m, i64 n,
                     i64 *chosen, i64 *n_chosen)
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

double ofx_set_cover_soft(const i64 *elem, const double *cost, i64 m, i64 n,
                          const i64 *chosen, double pen, i64 *uncovered)
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

double ofx_set_cover_pricing(const double *y, i64 m, double base_cost, double cost_per_elem,
                             i64 *chosen_set)
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

double ofx_set_cover_sa(const i64 *elem, const double *cost, i64 m, i64 n,
                        double pen, i64 maxit, i64 seed, i64 *chosen)
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

double ofx_set_cover_local(const i64 *elem, const double *cost, i64 m, i64 n,
                           i64 *chosen, i64 *n_chosen, i64 maxit)
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

double ofx_set_cover_grasp(const i64 *elem, const double *cost, i64 m, i64 n,
                           double alpha, i64 maxit, i64 *chosen, i64 *n_chosen)
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
double ofx_set_cover_rgrasp(const i64 *elem, const double *cost, i64 m, i64 n,
                            const double *alphas, i64 maxit, i64 *chosen, i64 *n_chosen)
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

i64 ofx_max_coverage(const i64 *elem, i64 m, i64 n, i64 k, i64 *chosen, i64 *n_chosen)
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

double ofx_pmedian(const double *dist, i64 n, i64 p, i64 *medians, i64 *assign, i64 maxit)
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

double ofx_pcenter(const double *dist, i64 n, i64 p, i64 *medians)
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

i64 ofx_vertex_cover(const i64 *edges, i64 n, i64 ne, i64 *cover)
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

i64 ofx_hitting_set(const i64 *elem, i64 m, i64 n, i64 *chosen, i64 *n_chosen)
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

double ofx_facility_location(const double *open_cost, const double *conn_cost, i64 m, i64 n,
                             i64 *open)
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

double ofx_kmeans(const double *points, i64 n, i64 dim, i64 k, double *centroids,
                  i64 *labels, i64 maxit)
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

i64 ofx_sat(const i64 *clauses, const i64 *clause_start, const i64 *clause_len,
            i64 nvars, i64 nclauses, i64 *assignment)
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

i64 ofx_alldiff(const i64 *dom, i64 n, i64 D, i64 *sol)
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

i64 ofx_cp_binary(const i64 *dom, const i64 *allowed, i64 n, i64 D, i64 *sol)
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
