/*
 * or_ffi_routing.c — shim ABI per il dominio ROUTING del namespace OPTIMIZE
 * (TSP/VRP/PDP/arc routing). Come or_ffi.c: int64 <-> int, array di servizio
 * compattati, seed fissi dove la firma C supererebbe gli 8 argomenti FFI.
 */
#include "gor_routing.h"

#include <stdlib.h>
#include <stdint.h>

typedef long long i64;

static int *to_iarr(const i64 *a, i64 n)
{
    i64 i;
    int *r;
    if (n <= 0 || a == NULL) return NULL;
    r = (int *)calloc((size_t)n, sizeof(int));
    if (r == NULL) return NULL;
    for (i = 0; i < n; i++) r[i] = (int)a[i];
    return r;
}

static void out_iarr(const int *a, i64 *out, i64 n)
{
    i64 i;
    for (i = 0; i < n; i++) out[i] = (i64)a[i];
}


/* ── TSP ─────────────────────────────────────────────────────────────────── */

double ofx_tsp_nn(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_tsp_nn((int)n, (const gor_real *)dist, t);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_2opt(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, i64 maxit)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    { i64 i; for (i = 0; i < n; i++) t[i] = (int)tour[i]; }
    r = (double)gor_tsp_2opt((int)n, (const gor_real *)dist, t, (int)maxit);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_tour_cost(const double *dist, int64_t dist_n, i64 n, const i64 *tour, int64_t tour_n)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = to_iarr(tour, n);
    if (t == NULL) return -1.0;
    r = (double)gor_tsp_tour_cost((int)n, (const gor_real *)dist, t);
    free(t);
    return r;
}

double ofx_tsp_extra_mileage(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_tsp_extra_mileage((int)n, (const gor_real *)dist, t);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_grasp(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, i64 maxit)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_tsp_grasp((int)n, (const gor_real *)dist, t, (int)maxit, 0.3, 12345ULL);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_vns(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, i64 maxit)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    { i64 i; for (i = 0; i < n; i++) t[i] = (int)tour[i]; }
    r = (double)gor_tsp_vns((int)n, (const gor_real *)dist, t, (int)maxit);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_3opt(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, i64 maxit)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    { i64 i; for (i = 0; i < n; i++) t[i] = (int)tour[i]; }
    r = (double)gor_tsp_3opt((int)n, (const gor_real *)dist, t, (int)maxit);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_sa(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, double t0, double cooling, i64 iters)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    { i64 i; for (i = 0; i < n; i++) t[i] = (int)tour[i]; }
    r = (double)gor_tsp_sa((int)n, (const gor_real *)dist, t, (gor_real)t0,
                           (gor_real)cooling, (int)iters, 12345ULL);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_tabu(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, i64 maxit, i64 tenure_kind)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    { i64 i; for (i = 0; i < n; i++) t[i] = (int)tour[i]; }
    r = (double)gor_tsp_tabu((int)n, (const gor_real *)dist, t, (int)maxit,
                             (int)tenure_kind, 12345ULL);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_one_tree(const double *dist, int64_t dist_n, i64 n, i64 root, i64 *edges, int64_t edges_n)
{
    int *e; double r;
    if (n <= 0 || dist == NULL || edges == NULL) return -1.0;
    e = (int *)malloc((size_t)(2 * n) * sizeof(int));
    if (e == NULL) return -1.0;
    r = (double)gor_tsp_one_tree((int)n, (const gor_real *)dist, (int)root, e);
    out_iarr(e, edges, 2 * n);
    free(e);
    return r;
}

double ofx_tsp_hk_bound(const double *dist, int64_t dist_n, i64 n, i64 maxit, double *pi, int64_t pi_n)
{
    if (n <= 0 || dist == NULL || pi == NULL) return -1.0;
    return (double)gor_tsp_hk_bound((int)n, (const gor_real *)dist, (int)maxit, (gor_real *)pi);
}

i64 ofx_tsp_subtours(const i64 *succ, int64_t succ_n, i64 n, i64 *comp, int64_t comp_n, i64 *sizes, int64_t sizes_n)
{
    int *s, *c, *sz; int r;
    if (n <= 0 || succ == NULL || comp == NULL || sizes == NULL) return -1;
    s = to_iarr(succ, n);
    c = (int *)calloc((size_t)n, sizeof(int));
    sz = (int *)calloc((size_t)n, sizeof(int));
    if (s == NULL || c == NULL || sz == NULL) { free(s); free(c); free(sz); return -1; }
    r = gor_tsp_subtours((int)n, s, c, sz);
    out_iarr(c, comp, n);
    out_iarr(sz, sizes, n);
    free(s); free(c); free(sz);
    return (i64)r;
}

double ofx_tsp_dfj(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, i64 maxcuts)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_tsp_dfj((int)n, (const gor_real *)dist, t, (int)maxcuts);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

i64 ofx_tsp_erx(const i64 *p1, int64_t p1_n, const i64 *p2, int64_t p2_n, i64 n, i64 *child, int64_t child_n)
{
    int *a, *b, *c;
    if (n <= 0 || p1 == NULL || p2 == NULL || child == NULL) return 0;
    a = to_iarr(p1, n);
    b = to_iarr(p2, n);
    c = (int *)calloc((size_t)n, sizeof(int));
    if (a == NULL || b == NULL || c == NULL) { free(a); free(b); free(c); return 0; }
    gor_tsp_erx((int)n, a, b, c, 12345ULL);
    out_iarr(c, child, n);
    free(a); free(b); free(c);
    return 1;
}

double ofx_tsp_ga(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, i64 pop, i64 gens)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_tsp_ga((int)n, (const gor_real *)dist, t, (int)pop, (int)gens, 12345ULL);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_or_opt(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n, i64 maxit)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    { i64 i; for (i = 0; i < n; i++) t[i] = (int)tour[i]; }
    r = (double)gor_tsp_or_opt((int)n, (const gor_real *)dist, t, (int)maxit);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_double_tree(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_tsp_double_tree((int)n, (const gor_real *)dist, t);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

double ofx_tsp_held_karp(const double *dist, int64_t dist_n, i64 n, i64 *tour, int64_t tour_n)
{
    int *t; double r;
    if (n <= 0 || dist == NULL || tour == NULL) return -1.0;
    t = (int *)calloc((size_t)n, sizeof(int));
    if (t == NULL) return -1.0;
    r = (double)gor_tsp_held_karp((int)n, (const gor_real *)dist, t);
    out_iarr(t, tour, n);
    free(t);
    return r;
}

/* ── VRP / PDP / arc routing ────────────────────────────────────────────── */

i64 ofx_vrp_clarke_wright(const double *dist, int64_t dist_n, const double *demand, int64_t demand_n, i64 n, double cap,
                          i64 depot, i64 *route_of, int64_t route_of_n, double *total, int64_t total_n)
{
    int *r;
    gor_real tot = 0.0;
    int k;
    if (n <= 0 || dist == NULL || demand == NULL || route_of == NULL) return -1;
    r = (int *)calloc((size_t)n, sizeof(int));
    if (r == NULL) return -1;
    k = gor_vrp_clarke_wright((int)n, (const gor_real *)dist, (const gor_real *)demand,
                              (gor_real)cap, (int)depot, r, &tot);
    out_iarr(r, route_of, n);
    if (total != NULL) total[0] = (double)tot;
    free(r);
    return (i64)k;
}

/* tw() = ready[n] | due[n] | service[n] (parametri compattati: C ne ha 11). */
i64 ofx_vrp_time_windows(const double *dist, int64_t dist_n, const double *demand, int64_t demand_n, i64 n, double cap,
                         i64 depot, const double *tw, int64_t tw_n, i64 *route_of, int64_t route_of_n, double *total, int64_t total_n)
{
    int *r;
    gor_real tot = 0.0;
    int k;
    if (n <= 0 || dist == NULL || demand == NULL || tw == NULL || route_of == NULL) return -1;
    r = (int *)calloc((size_t)n, sizeof(int));
    if (r == NULL) return -1;
    k = gor_vrp_time_windows((int)n, (const gor_real *)dist, (const gor_real *)demand,
                             (gor_real)cap, (int)depot, (const gor_real *)tw,
                             (const gor_real *)(tw + n), (const gor_real *)(tw + 2 * n),
                             r, &tot);
    out_iarr(r, route_of, n);
    if (total != NULL) total[0] = (double)tot;
    free(r);
    return (i64)k;
}

i64 ofx_mdvrp(const double *dist, int64_t dist_n, const double *demand, int64_t demand_n, i64 n, i64 ndepots, double cap,
              i64 *route_of, int64_t route_of_n, double *total, int64_t total_n)
{
    int *r;
    gor_real tot = 0.0;
    int k;
    if (n <= 0 || ndepots <= 0 || dist == NULL || demand == NULL || route_of == NULL) return -1;
    /* calloc: `gor_mdvrp` scrive route_of solo per i clienti VISITATI; i depositi (0..ndepots-1)
     * restavano con il contenuto del malloc (garbage copiato in BASIC). 2026-11-04. */
    r = (int *)calloc((size_t)n, sizeof(int));
    if (r == NULL) return -1;
    k = gor_mdvrp((int)n, (int)ndepots, (const gor_real *)dist, (const gor_real *)demand,
                  (gor_real)cap, r, &tot);
    out_iarr(r, route_of, n);
    if (total != NULL) total[0] = (double)tot;
    free(r);
    return (i64)k;
}

i64 ofx_pdp(const double *dist, int64_t dist_n, const double *demand, int64_t demand_n, i64 nreq, double cap,
            const i64 *pickup, int64_t pickup_n, const i64 *delivery, int64_t delivery_n, i64 *route, int64_t route_n, double *total, int64_t total_n)
{
    int *p, *d, *r;
    gor_real tot = 0.0;
    int k;
    if (nreq <= 0 || dist == NULL || demand == NULL || pickup == NULL || delivery == NULL ||
        route == NULL) return -1;
    p = to_iarr(pickup, nreq);
    d = to_iarr(delivery, nreq);
    r = (int *)calloc((size_t)(2 * nreq), sizeof(int));
    if (p == NULL || d == NULL || r == NULL) { free(p); free(d); free(r); return -1; }
    k = gor_pdp((int)nreq, (const gor_real *)dist, (const gor_real *)demand, (gor_real)cap,
                p, d, r, &tot);
    out_iarr(r, route, 2 * nreq);
    if (total != NULL) total[0] = (double)tot;
    free(p); free(d); free(r);
    return (i64)k;
}

double ofx_arc_routing(const double *w, int64_t w_n, i64 n, i64 *euler, int64_t euler_n, i64 *elen, int64_t elen_n)
{
    int *e;
    int len = 0;
    double r;
    if (n <= 0 || w == NULL || euler == NULL || elen == NULL) return -1.0;
    e = (int *)malloc((size_t)(n * n + 2) * sizeof(int));
    if (e == NULL) return -1.0;
    r = (double)gor_arc_routing((int)n, (const gor_real *)w, e, &len);
    out_iarr(e, euler, len);
    elen[0] = (i64)len;
    free(e);
    return r;
}

#include <stdint.h>
/* Trampolini DECLARE NATIVE (ffi_ext_native.pl): firma originale, estensioni = INT64_MAX. */
double ofx_arc_routing__nat(const double *w, i64 n, i64 *euler, i64 *elen) { return ofx_arc_routing(w, INT64_MAX, n, euler, INT64_MAX, elen, INT64_MAX); }
i64 ofx_mdvrp__nat(const double *dist, const double *demand, i64 n, i64 ndepots, double cap, i64 *route_of, double *total) { return ofx_mdvrp(dist, INT64_MAX, demand, INT64_MAX, n, ndepots, cap, route_of, INT64_MAX, total, INT64_MAX); }
i64 ofx_pdp__nat(const double *dist, const double *demand, i64 nreq, double cap, const i64 *pickup, const i64 *delivery, i64 *route, double *total) { return ofx_pdp(dist, INT64_MAX, demand, INT64_MAX, nreq, cap, pickup, INT64_MAX, delivery, INT64_MAX, route, INT64_MAX, total, INT64_MAX); }
double ofx_tsp_2opt__nat(const double *dist, i64 n, i64 *tour, i64 maxit) { return ofx_tsp_2opt(dist, INT64_MAX, n, tour, INT64_MAX, maxit); }
double ofx_tsp_3opt__nat(const double *dist, i64 n, i64 *tour, i64 maxit) { return ofx_tsp_3opt(dist, INT64_MAX, n, tour, INT64_MAX, maxit); }
double ofx_tsp_dfj__nat(const double *dist, i64 n, i64 *tour, i64 maxcuts) { return ofx_tsp_dfj(dist, INT64_MAX, n, tour, INT64_MAX, maxcuts); }
double ofx_tsp_double_tree__nat(const double *dist, i64 n, i64 *tour) { return ofx_tsp_double_tree(dist, INT64_MAX, n, tour, INT64_MAX); }
i64 ofx_tsp_erx__nat(const i64 *p1, const i64 *p2, i64 n, i64 *child) { return ofx_tsp_erx(p1, INT64_MAX, p2, INT64_MAX, n, child, INT64_MAX); }
double ofx_tsp_extra_mileage__nat(const double *dist, i64 n, i64 *tour) { return ofx_tsp_extra_mileage(dist, INT64_MAX, n, tour, INT64_MAX); }
double ofx_tsp_ga__nat(const double *dist, i64 n, i64 *tour, i64 pop, i64 gens) { return ofx_tsp_ga(dist, INT64_MAX, n, tour, INT64_MAX, pop, gens); }
double ofx_tsp_grasp__nat(const double *dist, i64 n, i64 *tour, i64 maxit) { return ofx_tsp_grasp(dist, INT64_MAX, n, tour, INT64_MAX, maxit); }
double ofx_tsp_held_karp__nat(const double *dist, i64 n, i64 *tour) { return ofx_tsp_held_karp(dist, INT64_MAX, n, tour, INT64_MAX); }
double ofx_tsp_hk_bound__nat(const double *dist, i64 n, i64 maxit, double *pi) { return ofx_tsp_hk_bound(dist, INT64_MAX, n, maxit, pi, INT64_MAX); }
double ofx_tsp_nn__nat(const double *dist, i64 n, i64 *tour) { return ofx_tsp_nn(dist, INT64_MAX, n, tour, INT64_MAX); }
double ofx_tsp_one_tree__nat(const double *dist, i64 n, i64 root, i64 *edges) { return ofx_tsp_one_tree(dist, INT64_MAX, n, root, edges, INT64_MAX); }
double ofx_tsp_or_opt__nat(const double *dist, i64 n, i64 *tour, i64 maxit) { return ofx_tsp_or_opt(dist, INT64_MAX, n, tour, INT64_MAX, maxit); }
double ofx_tsp_sa__nat(const double *dist, i64 n, i64 *tour, double t0, double cooling, i64 iters) { return ofx_tsp_sa(dist, INT64_MAX, n, tour, INT64_MAX, t0, cooling, iters); }
i64 ofx_tsp_subtours__nat(const i64 *succ, i64 n, i64 *comp, i64 *sizes) { return ofx_tsp_subtours(succ, INT64_MAX, n, comp, INT64_MAX, sizes, INT64_MAX); }
double ofx_tsp_tabu__nat(const double *dist, i64 n, i64 *tour, i64 maxit, i64 tenure_kind) { return ofx_tsp_tabu(dist, INT64_MAX, n, tour, INT64_MAX, maxit, tenure_kind); }
double ofx_tsp_tour_cost__nat(const double *dist, i64 n, const i64 *tour) { return ofx_tsp_tour_cost(dist, INT64_MAX, n, tour, INT64_MAX); }
double ofx_tsp_vns__nat(const double *dist, i64 n, i64 *tour, i64 maxit) { return ofx_tsp_vns(dist, INT64_MAX, n, tour, INT64_MAX, maxit); }
i64 ofx_vrp_clarke_wright__nat(const double *dist, const double *demand, i64 n, double cap, i64 depot, i64 *route_of, double *total) { return ofx_vrp_clarke_wright(dist, INT64_MAX, demand, INT64_MAX, n, cap, depot, route_of, INT64_MAX, total, INT64_MAX); }
i64 ofx_vrp_time_windows__nat(const double *dist, const double *demand, i64 n, double cap, i64 depot, const double *tw, i64 *route_of, double *total) { return ofx_vrp_time_windows(dist, INT64_MAX, demand, INT64_MAX, n, cap, depot, tw, INT64_MAX, route_of, INT64_MAX, total, INT64_MAX); }
