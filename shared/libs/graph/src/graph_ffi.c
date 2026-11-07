/*
 * graph_ffi.c — shim ABI piatto per il binding BASIC del namespace GRAPH.
 *
 * gmbortools lavora su `gor_graph` (struct) e su matrici dense con tipi C `int`/`double`.
 * L'FFI di GMBasic passa invece:
 *   - scalari LONG come int64 (gmb_int_t),
 *   - array LONG come int64* (gmb_int_t*),
 *   - array DOUBLE come double*.
 * Questo file adatta i tipi e costruisce il `gor_graph` quando serve, senza stato globale.
 */
#include <stdint.h>
#include "gor_graph.h"
#include "gor_graph2.h"
#include "gor_graph3.h"
#include "gor_linkpred.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;
#define GOR_FFI_INF (INFINITY)

/* Costruisce un gor_graph non orientato/diretto da adiacenza 0/1 (int64).
 * Il chiamante libera g.w con free(). Ritorna 0 su errore. */
static int wrap_adj(const i64 *adj, i64 n, int directed, gor_graph *g)
{
    i64 i;
    if (n <= 0 || adj == NULL) return 0;
    g->n = (int)n;
    g->directed = directed;
    g->w = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    if (g->w == NULL) return 0;
    for (i = 0; i < n * n; i++)
        g->w[i] = adj[i] ? 1.0 : GOR_FFI_INF;
    return 1;
}

/* BASIC non ha la costante INFINITY: il binding usa `GRAPH.ABSENT()` (1e30).
 * Ogni peso >= 1e29 viene normalizzato a INFINITY, che è il sentinella di gmbortools. */
static double *norm_w(const double *w, i64 n)
{
    i64 i;
    double *c = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    if (c == NULL) return NULL;
    for (i = 0; i < n * n; i++) c[i] = (w[i] >= 1e29) ? GOR_FFI_INF : w[i];
    return c;
}

/* Costruisce un gor_graph sui pesi normalizzati (il chiamante libera g.w). */
static gor_graph wrap_w(const double *w, i64 n, int directed)
{
    gor_graph g;
    g.n = (int)n;
    g.directed = directed;
    g.w = (gor_real *)norm_w(w, n);
    if (g.w == NULL) g.n = 0;
    return g;
}

static void copy_out_i(const int *src, i64 *dst, i64 n)
{
    i64 i;
    for (i = 0; i < n; i++) dst[i] = (i64)src[i];
}

static void copy_out_d(const gor_real *src, double *dst, i64 n)
{
    i64 i;
    for (i = 0; i < n; i++) dst[i] = (double)src[i];
}

/* ------------------------------------------------------------ struttura */

i64 gfx_degree(const i64 *adj, int64_t adj_n, i64 n, i64 v)
{
    i64 i, d = 0;
    if (adj == NULL || n <= 0 || v < 0 || v >= n) return -1;
    for (i = 0; i < n; i++) if (adj[v * n + i] || adj[i * n + v]) d++;
    return d;
}

i64 gfx_has_cycle(const i64 *adj, int64_t adj_n, i64 n)
{
    i64 i, r;
    int *a;
    if (n <= 0 || adj == NULL) return 0;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    if (a == NULL) return 0;
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    r = (i64)gor_has_cycle((int)n, a);
    free(a);
    return r;
}

i64 gfx_is_bipartite(const i64 *adj, int64_t adj_n, i64 n)
{
    i64 i, r;
    int *a;
    if (n <= 0 || adj == NULL) return 1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    if (a == NULL) return 0;
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    r = (i64)gor_is_bipartite((int)n, a);
    free(a);
    return r;
}

/* ---------------------------------------------------------- traversamento */

i64 gfx_bfs(const i64 *adj, int64_t adj_n, i64 n, i64 src, i64 *order, int64_t order_n)
{
    gor_graph g;
    int *o, cnt = 0;
    if (src < 0 || src >= n || !wrap_adj(adj, n, 1, &g)) return -1;
    o = (int *)malloc((size_t)n * sizeof(int));
    if (o == NULL) { free(g.w); return -1; }
    if (gor_bfs(&g, (int)src, o, &cnt) != GOR_OK) cnt = -1;
    else copy_out_i(o, order, cnt);
    free(o);
    free(g.w);
    return (i64)cnt;
}

i64 gfx_dfs(const i64 *adj, int64_t adj_n, i64 n, i64 src, i64 *order, int64_t order_n)
{
    gor_graph g;
    int *o, cnt = 0;
    if (src < 0 || src >= n || !wrap_adj(adj, n, 1, &g)) return -1;
    o = (int *)malloc((size_t)n * sizeof(int));
    if (o == NULL) { free(g.w); return -1; }
    if (gor_dfs(&g, (int)src, o, &cnt) != GOR_OK) cnt = -1;
    else copy_out_i(o, order, cnt);
    free(o);
    free(g.w);
    return (i64)cnt;
}

i64 gfx_toposort(const i64 *adj, int64_t adj_n, i64 n, i64 *order, int64_t order_n)
{
    gor_graph g;
    int *o;
    i64 r = 1;
    if (!wrap_adj(adj, n, 1, &g)) return -1;
    o = (int *)malloc((size_t)n * sizeof(int));
    if (o == NULL) { free(g.w); return -1; }
    if (gor_topological_sort(&g, o) != GOR_OK) r = 0;
    else copy_out_i(o, order, n);
    free(o);
    free(g.w);
    return r;
}

i64 gfx_components(const i64 *adj, int64_t adj_n, i64 n, i64 *comp, int64_t comp_n)
{
    gor_graph g;
    int *c, ncomp = 0;
    if (!wrap_adj(adj, n, 0, &g)) return -1;
    c = (int *)malloc((size_t)n * sizeof(int));
    if (c == NULL) { free(g.w); return -1; }
    if (gor_connected_components(&g, c, &ncomp) != GOR_OK) ncomp = -1;
    else copy_out_i(c, comp, n);
    free(c);
    free(g.w);
    return (i64)ncomp;
}

i64 gfx_scc(const i64 *adj, int64_t adj_n, i64 n, i64 *comp, int64_t comp_n)
{
    i64 i, k;
    int *a, *c;
    if (n <= 0 || adj == NULL) return -1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    c = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL || c == NULL) { free(a); free(c); return -1; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    k = (i64)gor_scc_tarjan((int)n, a, c);
    copy_out_i(c, comp, n);
    free(a);
    free(c);
    return k;
}

/* -------------------------------------------------------------- percorsi */

i64 gfx_dijkstra(const double *w, int64_t w_n, i64 n, i64 src, double *dist, int64_t dist_n, i64 *prev, int64_t prev_n)
{
    gor_graph g;
    gor_real *d;
    int *p, i, r;
    if (n <= 0 || w == NULL || src < 0 || src >= n) return -1;
    g = wrap_w(w, n, 1);
    if (g.n == 0) return -1;
    d = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    p = (int *)malloc((size_t)n * sizeof(int));
    if (d == NULL || p == NULL) { free(d); free(p); free(g.w); return -1; }
    r = gor_dijkstra(&g, (int)src, d, p);
    if (r != GOR_OK) { free(d); free(p); free(g.w); return -1; }
    copy_out_d(d, dist, n);
    for (i = 0; i < n; i++) prev[i] = (i64)p[i];
    free(d);
    free(p);
    free(g.w);
    return 1;
}

i64 gfx_bellman_ford(const double *w, int64_t w_n, i64 n, i64 src, double *dist, int64_t dist_n, i64 *prev, int64_t prev_n)
{
    gor_graph g;
    gor_real *d;
    int *p, i, r;
    if (n <= 0 || w == NULL || src < 0 || src >= n) return -1;
    g = wrap_w(w, n, 1);
    if (g.n == 0) return -1;
    d = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    p = (int *)malloc((size_t)n * sizeof(int));
    if (d == NULL || p == NULL) { free(d); free(p); free(g.w); return -1; }
    r = gor_bellman_ford(&g, (int)src, d, p);
    if (r != GOR_OK) { free(d); free(p); free(g.w); return (i64)r; }
    copy_out_d(d, dist, n);
    for (i = 0; i < n; i++) prev[i] = (i64)p[i];
    free(d);
    free(p);
    free(g.w);
    return 1;
}

i64 gfx_floyd_warshall(const double *w, int64_t w_n, i64 n, double *dist, int64_t dist_n)
{
    gor_graph g;
    gor_real *d;
    int r;
    if (n <= 0 || w == NULL) return -1;
    g = wrap_w(w, n, 1);
    if (g.n == 0) return -1;
    d = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    if (d == NULL) { free(g.w); return -1; }
    r = gor_floyd_warshall(&g, d);
    if (r == GOR_OK) copy_out_d(d, dist, n * n);
    free(d);
    free(g.w);
    return r == GOR_OK ? 1 : (i64)r;
}

/* ------------------------------------------------------ centralità/community */

i64 gfx_pagerank(const double *w, int64_t w_n, i64 n, double *rank, int64_t rank_n, double damping, i64 iters)
{
    double *c;
    if (n <= 0 || w == NULL || rank == NULL) return 0;
    c = norm_w(w, n);
    if (c == NULL) return 0;
    { i64 i; for (i = 0; i < n * n; i++) c[i] = (c[i] == GOR_FFI_INF) ? 0.0 : 1.0; }   /* PageRank: adiacenza binaria */
    gor_pagerank((int)n, (const gor_real *)c, (gor_real)damping, (int)iters, (gor_real *)rank);
    free(c);
    return 1;
}

double gfx_betweenness(const double *w, int64_t w_n, i64 n, double *bc, int64_t bc_n)
{
    double *c;
    double r;
    if (n <= 0 || w == NULL || bc == NULL) return 0.0;
    c = norm_w(w, n);
    if (c == NULL) return 0.0;
    r = (double)gor_betweenness((int)n, (const gor_real *)c, (gor_real *)bc);
    free(c);
    return r;
}

i64 gfx_closeness(const i64 *adj, int64_t adj_n, i64 n, double *cc, int64_t cc_n)
{
    i64 i;
    gor_real *a, *out;
    if (n <= 0 || adj == NULL || cc == NULL) return 0;
    a = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    out = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    if (a == NULL || out == NULL) { free(a); free(out); return 0; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1.0 : 0.0;
    gor_closeness((int)n, a, out);
    copy_out_d(out, cc, n);
    free(a);
    free(out);
    return 1;
}

i64 gfx_louvain(const double *w, int64_t w_n, i64 n, i64 *comm, int64_t comm_n, double resolution)
{
    gor_real *a;
    int *c, k, i;
    (void)resolution;
    if (n <= 0 || w == NULL || comm == NULL) return -1;
    a = (gor_real *)norm_w(w, n);
    c = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL || c == NULL) { free(a); free(c); return -1; }
    for (i = 0; i < n * n; i++) if (a[i] == GOR_FFI_INF) a[i] = 0.0;
    k = gor_louvain((int)n, a, c);
    copy_out_i(c, comm, n);
    free(a);
    free(c);
    return (i64)k;
}

/* ------------------------------------- colorazione/clique/chiusura/struttura */

i64 gfx_coloring(const i64 *adj, int64_t adj_n, i64 n, i64 *color, int64_t color_n)
{
    i64 i;
    int *a, *c, k;
    if (n <= 0 || adj == NULL || color == NULL) return -1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    c = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL || c == NULL) { free(a); free(c); return -1; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    k = gor_graph_coloring((int)n, a, c);
    copy_out_i(c, color, n);
    free(a);
    free(c);
    return (i64)k;
}

i64 gfx_max_clique(const i64 *adj, int64_t adj_n, i64 n, i64 *clique, int64_t clique_n)
{
    i64 i;
    int *a, *c, size = 0;
    if (n <= 0 || adj == NULL || clique == NULL) return -1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    c = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL || c == NULL) { free(a); free(c); return -1; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    gor_max_clique((int)n, a, c, &size);
    copy_out_i(c, clique, size);
    free(a);
    free(c);
    return (i64)size;
}

i64 gfx_transitive_closure(const i64 *adj, int64_t adj_n, i64 n, i64 *out, int64_t out_n)
{
    i64 i;
    int *a, *c;
    if (n <= 0 || adj == NULL || out == NULL) return 0;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    c = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    if (a == NULL || c == NULL) { free(a); free(c); return 0; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    gor_transitive_closure((int)n, a, c);
    copy_out_i(c, out, n * n);
    free(a);
    free(c);
    return 1;
}

/* ------------------------------------------------------------- proprietà */

double gfx_diameter(const double *w, int64_t w_n, i64 n)
{
    double *c;
    double r;
    if (n <= 0 || w == NULL) return 0.0;
    c = norm_w(w, n);
    if (c == NULL) return 0.0;
    r = (double)gor_diameter((int)n, (const gor_real *)c);
    free(c);
    return r;
}

double gfx_eccentricity(const double *w, int64_t w_n, i64 n, i64 v)
{
    double *c;
    double r;
    if (n <= 0 || w == NULL || v < 0 || v >= n) return -1.0;
    c = norm_w(w, n);
    if (c == NULL) return -1.0;
    r = (double)gor_eccentricity((int)n, (const gor_real *)c, (int)v);
    free(c);
    return r;
}

double gfx_clustering(const i64 *adj, int64_t adj_n, i64 n, i64 v)
{
    i64 i;
    int *a;
    double r;
    if (n <= 0 || adj == NULL || v < 0 || v >= n) return 0.0;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    if (a == NULL) return 0.0;
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    r = (double)gor_clustering_coefficient((int)n, a, (int)v);
    free(a);
    return r;
}

i64 gfx_subgraph(const double *w, int64_t w_n, i64 n, const i64 *nodes, int64_t nodes_n, i64 k, double *out, int64_t out_n)
{
    i64 i;
    int *idx;
    gor_status r;
    if (n <= 0 || w == NULL || nodes == NULL || out == NULL || k < 0) return GOR_ERR_ARG;
    idx = (int *)malloc((size_t)(k ? k : 1) * sizeof(int));
    if (idx == NULL) return GOR_ERR_NOMEM;
    for (i = 0; i < k; i++) idx[i] = (int)nodes[i];
    { double *c = norm_w(w, n);
      if (c == NULL) { free(idx); return GOR_ERR_NOMEM; }
      r = gor_subgraph((int)n, (const gor_real *)c, idx, (int)k, (gor_real *)out);
      free(c); }
    free(idx);
    return (i64)r;
}

i64 gfx_mst_kruskal(const double *w, int64_t w_n, i64 n, i64 *eu, int64_t eu_n, i64 *ev, int64_t ev_n, double *ew, int64_t ew_n)
{
    gor_graph g;
    gor_edge *e;
    int ne = 0, i;
    gor_real total = 0.0;
    if (n <= 0 || w == NULL) return -1;
    g = wrap_w(w, n, 0);   /* MST: grafo non orientato (parità gmb_graph_mst) */
    if (g.n == 0) return -1;
    e = (gor_edge *)malloc((size_t)(n ? n : 1) * sizeof(gor_edge));
    if (e == NULL) return -1;
    if (gor_mst_kruskal(&g, e, &ne, &total) != GOR_OK) ne = -1;
    else for (i = 0; i < ne; i++) { eu[i] = (i64)e[i].u; ev[i] = (i64)e[i].v; ew[i] = (double)e[i].w; }
    free(e);
    free(g.w);
    return (i64)ne;
}

/* ═══════════════════════ seconda slice: algoritmi avanzati ═══════════════════ */

i64 gfx_bridges(const i64 *adj, int64_t adj_n, i64 n, i64 *is_bridge, int64_t is_bridge_n)
{
    i64 i;
    int *a, *b, k;
    if (n <= 0 || adj == NULL || is_bridge == NULL) return -1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    b = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    if (a == NULL || b == NULL) { free(a); free(b); return -1; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    k = gor_bridges((int)n, a, b);
    copy_out_i(b, is_bridge, n * n);
    free(a);
    free(b);
    return (i64)k;
}

i64 gfx_articulation(const i64 *adj, int64_t adj_n, i64 n, i64 *art, int64_t art_n)
{
    i64 i;
    int *a, *b, k;
    if (n <= 0 || adj == NULL || art == NULL) return -1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    b = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL || b == NULL) { free(a); free(b); return -1; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    k = gor_articulation((int)n, a, b);
    copy_out_i(b, art, n);
    free(a);
    free(b);
    return (i64)k;
}

double gfx_astar(const double *w, int64_t w_n, i64 n, const double *h, int64_t h_n, i64 s, i64 t, i64 *path, int64_t path_n, i64 *info, int64_t info_n)
{
    double *wn, *hn;
    int *p, plen = 0;
    gor_real cost;
    if (n <= 0 || w == NULL || h == NULL || path == NULL || info == NULL) return -1.0;
    if (s < 0 || s >= n || t < 0 || t >= n) return -1.0;
    wn = norm_w(w, n);
    hn = (double *)malloc((size_t)n * sizeof(double));
    p = (int *)malloc((size_t)n * sizeof(int));
    if (wn == NULL || hn == NULL || p == NULL) { free(wn); free(hn); free(p); return -1.0; }
    { i64 i; for (i = 0; i < n; i++) hn[i] = h[i]; }
    cost = gor_astar((int)n, (const gor_real *)wn, (const gor_real *)hn, (int)s, (int)t, p, &plen);
    if (cost >= 0) copy_out_i(p, path, plen);
    info[0] = (i64)plen;
    info[1] = (cost >= 0) ? 1 : 0;
    free(wn);
    free(hn);
    free(p);
    return (double)cost;
}

double gfx_stoer_wagner(const double *w, int64_t w_n, i64 n)
{
    double *c;
    double r;
    if (n <= 0 || w == NULL) return 0.0;
    c = norm_w(w, n);
    if (c == NULL) return 0.0;
    { i64 i; for (i = 0; i < n * n; i++) if (c[i] == GOR_FFI_INF) c[i] = 0.0; }
    r = (double)gor_stoer_wagner((int)n, (const gor_real *)c);
    free(c);
    return r;
}

i64 gfx_k_shortest(const double *w, int64_t w_n, i64 n, i64 s, i64 t, i64 K, i64 *paths, int64_t paths_n, i64 *plens, int64_t plens_n)
{
    double *wn;
    int *p, *pl, k;
    if (n <= 0 || w == NULL || K <= 0 || paths == NULL || plens == NULL) return -1;
    if (s < 0 || s >= n || t < 0 || t >= n) return -1;
    wn = norm_w(w, n);
    p = (int *)malloc((size_t)(K * n) * sizeof(int));
    pl = (int *)malloc((size_t)K * sizeof(int));
    if (wn == NULL || p == NULL || pl == NULL) { free(wn); free(p); free(pl); return -1; }
    k = gor_k_shortest_yen((int)n, (const gor_real *)wn, (int)s, (int)t, (int)K, p, pl);
    if (k > 0) {
        i64 i, j;
        for (i = 0; i < k; i++) {
            plens[i] = (i64)pl[i];
            for (j = 0; j < pl[i]; j++) paths[i * n + j] = (i64)p[i * n + j];
        }
    }
    free(wn);
    free(p);
    free(pl);
    return (i64)k;
}

i64 gfx_isomorphic(const i64 *adj1, int64_t adj1_n, i64 n1, const i64 *adj2, int64_t adj2_n, i64 n2, i64 *mapping, int64_t mapping_n)
{
    i64 i;
    int *a, *b, *m, r;
    if (n1 <= 0 || n2 <= 0 || adj1 == NULL || adj2 == NULL || mapping == NULL) return 0;
    a = (int *)malloc((size_t)n1 * (size_t)n1 * sizeof(int));
    b = (int *)malloc((size_t)n2 * (size_t)n2 * sizeof(int));
    m = (int *)malloc((size_t)n1 * sizeof(int));
    if (a == NULL || b == NULL || m == NULL) { free(a); free(b); free(m); return 0; }
    for (i = 0; i < n1 * n1; i++) a[i] = adj1[i] ? 1 : 0;
    for (i = 0; i < n2 * n2; i++) b[i] = adj2[i] ? 1 : 0;
    r = gor_vf2_isomorphism((int)n1, a, (int)n2, b, m);
    copy_out_i(m, mapping, n1);
    free(a);
    free(b);
    free(m);
    return (i64)r;
}

i64 gfx_drnl(const i64 *adj, int64_t adj_n, i64 n, i64 u, i64 v, i64 *label, int64_t label_n)
{
    i64 i;
    int *a, *l, k;
    if (n <= 0 || adj == NULL || label == NULL || u < 0 || u >= n || v < 0 || v >= n) return -1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    l = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL || l == NULL) { free(a); free(l); return -1; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    (void)gor_graph_drnl((int)n, a, (int)u, (int)v, l);   /* il motore ritorna sempre 0 */
    copy_out_i(l, label, n);
    free(a);
    free(l);
    return 1;
}

i64 gfx_wl(const i64 *adj, int64_t adj_n, i64 n, i64 *colors, int64_t colors_n)
{
    i64 i;
    int *a, *c, k;
    if (n <= 0 || adj == NULL || colors == NULL) return -1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    c = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL || c == NULL) { free(a); free(c); return -1; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    k = gor_graph_wl((int)n, a, c);
    copy_out_i(c, colors, n);
    free(a);
    free(c);
    return (i64)k;
}

i64 gfx_min_path_cover(const i64 *adj, int64_t adj_n, i64 n)
{
    i64 i;
    int *a, k;
    if (n <= 0 || adj == NULL) return -1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    if (a == NULL) return -1;
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    k = gor_min_path_cover((int)n, a);
    free(a);
    return (i64)k;
}

i64 gfx_bfs_direction_optimizing(const i64 *adj, int64_t adj_n, i64 n, i64 src, double thr, double *dist, int64_t dist_n)
{
    i64 i;
    int *a, *d, k;
    if (n <= 0 || adj == NULL || dist == NULL || src < 0 || src >= n) return -1;
    a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    d = (int *)malloc((size_t)n * sizeof(int));
    if (a == NULL || d == NULL) { free(a); free(d); return -1; }
    for (i = 0; i < n * n; i++) a[i] = adj[i] ? 1 : 0;
    k = gor_bfs_direction_optimizing((int)n, a, (int)src, (gor_real)thr, d);
    for (i = 0; i < n; i++) dist[i] = (double)d[i];
    free(a);
    free(d);
    return (i64)k;
}

/* ── Link prediction (gmbortools `gor_linkpred`): baseline su adiacenza densa.
 * Le funzioni C lavorano su `int*`; qui si converte dall'array LONG del binding. */
static int *lp_adj(const i64 *adj, i64 n)
{
    i64 i;
    int *a = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    if (a == NULL) return NULL;
    for (i = 0; i < n * n; i++) a[i] = (int)adj[i];
    return a;
}

i64 gfx_link_common_neighbors(const i64 *adj, int64_t adj_n, i64 n, i64 u, i64 v)
{
    int *a;
    int r;
    if (n <= 0 || adj == NULL || u < 0 || v < 0 || u >= n || v >= n) return -1;
    a = lp_adj(adj, n);
    if (a == NULL) return -1;
    r = gor_lp_common_neighbors(a, (int)n, (int)u, (int)v);
    free(a);
    return (i64)r;
}

double gfx_link_jaccard(const i64 *adj, int64_t adj_n, i64 n, i64 u, i64 v)
{
    int *a;
    double r;
    if (n <= 0 || adj == NULL || u < 0 || v < 0 || u >= n || v >= n) return 0.0;
    a = lp_adj(adj, n);
    if (a == NULL) return 0.0;
    r = (double)gor_lp_jaccard(a, (int)n, (int)u, (int)v);
    free(a);
    return r;
}

double gfx_link_adamic_adar(const i64 *adj, int64_t adj_n, i64 n, i64 u, i64 v)
{
    int *a;
    double r;
    if (n <= 0 || adj == NULL || u < 0 || v < 0 || u >= n || v >= n) return 0.0;
    a = lp_adj(adj, n);
    if (a == NULL) return 0.0;
    r = (double)gor_lp_adamic_adar(a, (int)n, (int)u, (int)v);
    free(a);
    return r;
}

double gfx_link_resource_allocation(const i64 *adj, int64_t adj_n, i64 n, i64 u, i64 v)
{
    int *a;
    double r;
    if (n <= 0 || adj == NULL || u < 0 || v < 0 || u >= n || v >= n) return 0.0;
    a = lp_adj(adj, n);
    if (a == NULL) return 0.0;
    r = (double)gor_lp_resource_allocation(a, (int)n, (int)u, (int)v);
    free(a);
    return r;
}

i64 gfx_link_preferential_attachment(const i64 *adj, int64_t adj_n, i64 n, i64 u, i64 v)
{
    int *a;
    int r;
    if (n <= 0 || adj == NULL || u < 0 || v < 0 || u >= n || v >= n) return 0;
    a = lp_adj(adj, n);
    if (a == NULL) return 0;
    r = gor_lp_preferential_attachment(a, (int)n, (int)u, (int)v);
    free(a);
    return (i64)r;
}

/* Katz: la matrice è DOUBLE (pesata); ritorna lo score, 0.0 su errore. */
double gfx_link_katz(const double *w, int64_t w_n, i64 n, double beta, i64 u, i64 v)
{
    gor_real score = 0.0;
    if (n <= 0 || w == NULL || u < 0 || v < 0 || u >= n || v >= n) return 0.0;
    if (gor_lp_katz((const gor_real *)w, (int)n, (gor_real)beta, (int)u, (int)v, &score) != 0)
        return 0.0;
    return (double)score;
}

/* ── Helper di contenitore: cose che la matrice non dà in una chiamata ────── */

/* Ricostruisce il cammino src→dst dai predecessori prodotti da DIJKSTRA/BELLMANFORD
 * (prev(i) = predecessore di i, -1 per radice/assente). out() riceve i nodi da src
 * a dst. Ritorna il numero di nodi (0 = non raggiungibile, -1 = argomenti). */
i64 gfx_path_reconstruct(const i64 *prev, int64_t prev_n, i64 n, i64 src, i64 dst, i64 *out, int64_t out_n)
{
    i64 cap = (i64)out_n;
    (void)prev_n;
    int *p, *o;
    int len = 0, ok;
    i64 i, m;
    if (!prev || !out || n <= 0 || src < 0 || src >= n || dst < 0 || dst >= n || cap < 1) return -1;
    p = (int *)malloc((size_t)n * sizeof(int));
    o = (int *)malloc((size_t)n * sizeof(int));
    if (!p || !o) { free(p); free(o); return -1; }
    for (i = 0; i < n; i++) p[i] = (int)prev[i];
    ok = gor_path_reconstruct(p, (int)src, (int)n, (int)dst, o, &len);
    free(p);
    if (!ok || len <= 0) { free(o); return 0; }
    m = (len < (int)cap) ? len : cap;
    for (i = 0; i < m; i++) out[i] = (i64)o[i];
    free(o);
    return m;
}

/* Elenca gli archi di una matrice pesata n×n (triangolo superiore, arco presente
 * se w < 1e29). Scrive eu/ev/ew; ritorna il numero di archi (o -1 se cap troppo
 * piccolo / argomenti non validi). */
i64 gfx_edge_list(const double *w, int64_t w_n, i64 n, i64 *eu, int64_t eu_n, i64 *ev, int64_t ev_n, double *ew, int64_t ew_n)
{
    i64 cap = (i64)(eu_n < ev_n ? (ew_n < eu_n ? ew_n : eu_n) : (ew_n < ev_n ? ew_n : ev_n));
    (void)w_n;
    i64 i, j, k = 0;
    if (!w || !eu || !ev || !ew || n <= 0 || cap < 1) return -1;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++) {
            double x = w[i * n + j];
            if (x < 1e29) {
                if (k >= cap) return -1;
                eu[k] = i;
                ev[k] = j;
                ew[k] = x;
                k++;
            }
        }
    return k;
}

/* Vicini di v nella matrice di adiacenza 0/1 (riga v). Ritorna quanti (o -1). */
i64 gfx_neighbors(const i64 *adj, int64_t adj_n, i64 n, i64 v, i64 *out, int64_t out_n)
{
    i64 cap = (i64)out_n;
    (void)adj_n;
    i64 u, k = 0;
    if (!adj || !out || n <= 0 || v < 0 || v >= n || cap < 1) return -1;
    for (u = 0; u < n; u++)
        if (adj[v * n + u]) {
            if (k >= cap) return -1;
            out[k++] = u;
        }
    return k;
}
