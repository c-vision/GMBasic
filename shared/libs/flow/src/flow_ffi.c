/*
 * flow_ffi.c — shim ABI piatto per il binding BASIC del namespace GRAPH (dominio flussi).
 *
 * gmbortools lavora su `gor_graph`/`int`/`double`; l'FFI di GMBasic passa scalari LONG
 * come int64, array LONG come int64* e array DOUBLE come double*. Qui si adattano i tipi
 * e si costruisce il `gor_graph` per max-flow/min-cut, senza stato globale.
 */
#include "gor_flow.h"
#include "gor_flow2.h"

#include <stdlib.h>

typedef long long i64;

/* Capacità >= 0 su archi u->v (0 = assente); grafo orientato. */
static int build_cap(gor_graph *g, const double *cap, i64 n)
{
    i64 i, j;
    if (n <= 0 || cap == NULL) return 0;
    if (gor_graph_init(g, (int)n, 1) != GOR_OK) return 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            double c = cap[(size_t)i * (size_t)n + (size_t)j];
            if (c > 0.0) gor_graph_set_edge(g, (int)i, (int)j, (gor_real)c);
        }
    return 1;
}

static void copy_out_i(const int *src, i64 *dst, i64 n)
{
    i64 i;
    for (i = 0; i < n; i++) dst[i] = (i64)src[i];
}

double ffx_max_flow(const double *cap, i64 n, i64 s, i64 t)
{
    gor_graph g;
    gor_real value = 0.0;
    gor_status r;
    if (n <= 0 || cap == NULL || s < 0 || s >= n || t < 0 || t >= n) return -1.0;
    if (!build_cap(&g, cap, n)) return -1.0;
    r = gor_max_flow(&g, (int)s, (int)t, &value, NULL);
    gor_graph_free(&g);
    return (r == GOR_OK) ? (double)value : -1.0;
}

/* Min-cut: riempie side() (0 = sorgente, 1 = pozzo) e ritorna il valore del taglio. */
double ffx_min_cut(const double *cap, i64 n, i64 s, i64 t, i64 *side)
{
    gor_graph g;
    int *sd;
    double cut = 0.0;
    i64 i, j;
    if (n <= 0 || cap == NULL || side == NULL || s < 0 || s >= n || t < 0 || t >= n) return -1.0;
    sd = (int *)malloc((size_t)n * sizeof(int));
    if (sd == NULL) return -1.0;
    if (!build_cap(&g, cap, n)) { free(sd); return -1.0; }
    if (gor_min_cut(&g, (int)s, (int)t, sd) != GOR_OK) {
        gor_graph_free(&g);
        free(sd);
        return -1.0;
    }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            if (sd[i] == 0 && sd[j] == 1) cut += cap[(size_t)i * (size_t)n + (size_t)j];
    copy_out_i(sd, side, n);
    gor_graph_free(&g);
    free(sd);
    return cut;
}

double ffx_dinic(const double *cap, i64 n, i64 s, i64 t)
{
    if (n <= 0 || cap == NULL || s < 0 || s >= n || t < 0 || t >= n) return -1.0;
    return (double)gor_dinic((int)n, (const gor_real *)cap, (int)s, (int)t);
}

/* Min-cost max-flow: costo minimo; flowValue(0) riceve il valore del flusso. */
double ffx_min_cost_flow(const double *cap, const double *cost, i64 n, i64 s, i64 t,
                         double *flow_value)
{
    gor_real fv = 0.0;
    double r;
    if (n <= 0 || cap == NULL || cost == NULL || s < 0 || s >= n || t < 0 || t >= n) return -1.0;
    r = (double)gor_min_cost_flow((int)n, (const gor_real *)cap, (const gor_real *)cost,
                                  (int)s, (int)t, &fv);
    if (flow_value != NULL) flow_value[0] = (double)fv;
    return r;
}

i64 ffx_bip_match(const i64 *adj, i64 nL, i64 nR, i64 *matchL, i64 *matchR)
{
    i64 i, k;
    int *a, *ml, *mr;
    if (nL <= 0 || nR <= 0 || adj == NULL || matchL == NULL || matchR == NULL) return -1;
    a = (int *)malloc((size_t)(nL * nR) * sizeof(int));
    ml = (int *)malloc((size_t)nL * sizeof(int));
    mr = (int *)malloc((size_t)nR * sizeof(int));
    if (a == NULL || ml == NULL || mr == NULL) { free(a); free(ml); free(mr); return -1; }
    for (i = 0; i < nL * nR; i++) a[i] = adj[i] ? 1 : 0;
    k = (i64)gor_bipartite_matching((int)nL, (int)nR, a, ml, mr);
    copy_out_i(ml, matchL, nL);
    copy_out_i(mr, matchR, nR);
    free(a);
    free(ml);
    free(mr);
    return k;
}

i64 ffx_gen_match(const i64 *adj, i64 n, i64 *match)
{
    i64 i, k;
    int *a, *m;
    if (n <= 0 || adj == NULL || match == NULL) return -1;
    a = (int *)malloc((size_t)(n * n) * sizeof(int));
    m = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL || m == NULL) { free(a); free(m); return -1; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    k = (i64)gor_general_matching((int)n, a, m);
    copy_out_i(m, match, n);
    free(a);
    free(m);
    return k;
}

i64 ffx_circulation(const double *lower, const double *upper, i64 n, const double *b,
                    double *flow)
{
    if (n <= 0 || lower == NULL || upper == NULL || b == NULL || flow == NULL) return -1;
    return (i64)gor_circulation_lower((int)n, lower, upper, b, flow);
}

i64 ffx_gomory_hu(const double *w, i64 n, i64 *parent, double *weight)
{
    int *p;
    gor_real *wt;
    i64 i, k;
    if (n <= 0 || w == NULL || parent == NULL || weight == NULL) return -1;
    p = (int *)malloc((size_t)n * sizeof(int));
    wt = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    if (p == NULL || wt == NULL) { free(p); free(wt); return -1; }
    k = (i64)gor_gomory_hu((int)n, (const gor_real *)w, p, wt);
    for (i = 0; i < n; i++) { parent[i] = (i64)p[i]; weight[i] = (double)wt[i]; }
    free(p);
    free(wt);
    return k;
}

double ffx_weighted_match(const double *w, i64 n, i64 *match)
{
    int *m;
    gor_real r;
    if (n <= 0 || w == NULL || match == NULL) return -1.0;
    m = (int *)malloc((size_t)n * sizeof(int));
    if (m == NULL) return -1.0;
    r = gor_weighted_matching((int)n, (const gor_real *)w, m);
    copy_out_i(m, match, n);
    free(m);
    return (double)r;
}

double ffx_multicommodity(const double *cap, i64 n, i64 ncomm, const double *commodities,
                          double *flow)
{
    if (n <= 0 || cap == NULL || ncomm < 0 || commodities == NULL || flow == NULL) return -1.0;
    return (double)gor_multicommodity((int)n, (const gor_real *)cap, (int)ncomm,
                                      (const gor_real *)commodities, (gor_real *)flow);
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
i64 ffx_bip_match__ffi(const i64 *adj, int64_t adj_ext, i64 nL, i64 nR, i64 *matchL, int64_t matchL_ext, i64 *matchR, int64_t matchR_ext) { return ffx_bip_match(adj, nL, nR, matchL, matchR); }
i64 ffx_circulation__ffi(const double *lower, int64_t lower_ext, const double *upper, int64_t upper_ext, i64 n, const double *b, int64_t b_ext, double *flow, int64_t flow_ext) { return ffx_circulation(lower, upper, n, b, flow); }
double ffx_dinic__ffi(const double *cap, int64_t cap_ext, i64 n, i64 s, i64 t) { return ffx_dinic(cap, n, s, t); }
i64 ffx_gen_match__ffi(const i64 *adj, int64_t adj_ext, i64 n, i64 *match, int64_t match_ext) { return ffx_gen_match(adj, n, match); }
i64 ffx_gomory_hu__ffi(const double *w, int64_t w_ext, i64 n, i64 *parent, int64_t parent_ext, double *weight, int64_t weight_ext) { return ffx_gomory_hu(w, n, parent, weight); }
double ffx_max_flow__ffi(const double *cap, int64_t cap_ext, i64 n, i64 s, i64 t) { return ffx_max_flow(cap, n, s, t); }
double ffx_min_cost_flow__ffi(const double *cap, int64_t cap_ext, const double *cost, int64_t cost_ext, i64 n, i64 s, i64 t, double *flow_value, int64_t flow_value_ext) { return ffx_min_cost_flow(cap, cost, n, s, t, flow_value); }
double ffx_min_cut__ffi(const double *cap, int64_t cap_ext, i64 n, i64 s, i64 t, i64 *side, int64_t side_ext) { return ffx_min_cut(cap, n, s, t, side); }
double ffx_multicommodity__ffi(const double *cap, int64_t cap_ext, i64 n, i64 ncomm, const double *commodities, int64_t commodities_ext, double *flow, int64_t flow_ext) { return ffx_multicommodity(cap, n, ncomm, commodities, flow); }
double ffx_weighted_match__ffi(const double *w, int64_t w_ext, i64 n, i64 *match, int64_t match_ext) { return ffx_weighted_match(w, n, match); }
