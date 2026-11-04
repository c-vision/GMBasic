/* gor_graph.c — grafi: percorsi minimi, MST, connettività, topologico. */
#include "gor_graph.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GOR_INF (INFINITY)

/* ============================ costruzione ============================ */

gor_status gor_graph_init(gor_graph *g, int n, int directed)
{
    int i;
    if (g == NULL || n <= 0) return GOR_ERR_ARG;
    g->n = n;
    g->directed = directed ? 1 : 0;
    g->w = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    if (g->w == NULL) {
        g->n = 0;
        return GOR_ERR_NOMEM;
    }
    for (i = 0; i < n * n; i++) g->w[i] = GOR_INF;
    return GOR_OK;
}

void gor_graph_free(gor_graph *g)
{
    if (g == NULL) return;
    free(g->w);
    g->w = NULL;
    g->n = 0;
}

static int gor_graph_valid(const gor_graph *g)
{
    return g != NULL && g->w != NULL && g->n > 0;
}

void gor_graph_set_edge(gor_graph *g, int u, int v, gor_real weight)
{
    if (!gor_graph_valid(g)) return;
    if (u < 0 || u >= g->n || v < 0 || v >= g->n) return;
    g->w[(size_t)u * (size_t)g->n + (size_t)v] = weight;
    if (!g->directed && u != v) {
        g->w[(size_t)v * (size_t)g->n + (size_t)u] = weight;
    }
}

int gor_graph_has_edge(const gor_graph *g, int u, int v)
{
    if (!gor_graph_valid(g)) return 0;
    if (u < 0 || u >= g->n || v < 0 || v >= g->n) return 0;
    return isinf((double)g->w[(size_t)u * (size_t)g->n + (size_t)v]) ? 0 : 1;
}

gor_real gor_graph_edge(const gor_graph *g, int u, int v)
{
    if (!gor_graph_valid(g)) return GOR_INF;
    if (u < 0 || u >= g->n || v < 0 || v >= g->n) return GOR_INF;
    return g->w[(size_t)u * (size_t)g->n + (size_t)v];
}

int gor_graph_degree(const gor_graph *g, int u)
{
    int v;
    int deg = 0;
    if (!gor_graph_valid(g) || u < 0 || u >= g->n) return 0;
    for (v = 0; v < g->n; v++) {
        if (v != u && gor_graph_has_edge(g, u, v)) deg++;
    }
    return deg;
}

/* ============================= union-find ============================= */

gor_status gor_uf_init(gor_uf *uf, int n)
{
    int i;
    if (uf == NULL || n <= 0) return GOR_ERR_ARG;
    uf->n = n;
    uf->parent = (int *)malloc((size_t)n * sizeof(int));
    uf->rank = (int *)malloc((size_t)n * sizeof(int));
    if (uf->parent == NULL || uf->rank == NULL) {
        free(uf->parent);
        free(uf->rank);
        uf->parent = NULL;
        uf->rank = NULL;
        uf->n = 0;
        return GOR_ERR_NOMEM;
    }
    for (i = 0; i < n; i++) {
        uf->parent[i] = i;
        uf->rank[i] = 0;
    }
    return GOR_OK;
}

void gor_uf_free(gor_uf *uf)
{
    if (uf == NULL) return;
    free(uf->parent);
    free(uf->rank);
    uf->parent = NULL;
    uf->rank = NULL;
    uf->n = 0;
}

int gor_uf_find(gor_uf *uf, int x)
{
    if (uf == NULL || uf->parent == NULL || x < 0 || x >= uf->n) return -1;
    while (uf->parent[x] != x) {
        uf->parent[x] = uf->parent[uf->parent[x]]; /* path halving */
        x = uf->parent[x];
    }
    return x;
}

int gor_uf_union(gor_uf *uf, int a, int b)
{
    int ra = gor_uf_find(uf, a);
    int rb = gor_uf_find(uf, b);
    if (ra < 0 || rb < 0 || ra == rb) return 0;
    if (uf->rank[ra] < uf->rank[rb]) {
        uf->parent[ra] = rb;
    } else if (uf->rank[ra] > uf->rank[rb]) {
        uf->parent[rb] = ra;
    } else {
        uf->parent[rb] = ra;
        uf->rank[ra]++;
    }
    return 1;
}

/* ============================== visite ============================== */

gor_status gor_bfs(const gor_graph *g, int src, int *order, int *count)
{
    int *queue;
    int *visited;
    int head = 0, tail = 0, cnt = 0;
    if (!gor_graph_valid(g) || order == NULL || src < 0 || src >= g->n) return GOR_ERR_ARG;
    queue = (int *)malloc((size_t)g->n * sizeof(int));
    visited = (int *)calloc((size_t)g->n, sizeof(int));
    if (queue == NULL || visited == NULL) {
        free(queue);
        free(visited);
        return GOR_ERR_NOMEM;
    }
    visited[src] = 1;
    queue[tail++] = src;
    while (head < tail) {
        int u = queue[head++];
        int v;
        order[cnt++] = u;
        for (v = 0; v < g->n; v++) {
            if (!visited[v] && gor_graph_has_edge(g, u, v)) {
                visited[v] = 1;
                queue[tail++] = v;
            }
        }
    }
    if (count != NULL) *count = cnt;
    free(queue);
    free(visited);
    return GOR_OK;
}

gor_status gor_dfs(const gor_graph *g, int src, int *order, int *count)
{
    int *stack;
    int *visited;
    int top = 0, cnt = 0;
    if (!gor_graph_valid(g) || order == NULL || src < 0 || src >= g->n) return GOR_ERR_ARG;
    stack = (int *)malloc((size_t)g->n * sizeof(int));
    visited = (int *)calloc((size_t)g->n, sizeof(int));
    if (stack == NULL || visited == NULL) {
        free(stack);
        free(visited);
        return GOR_ERR_NOMEM;
    }
    stack[top++] = src;
    while (top > 0) {
        int u = stack[--top];
        int v;
        if (visited[u]) continue;
        visited[u] = 1;
        order[cnt++] = u;
        for (v = g->n - 1; v >= 0; v--) {
            if (!visited[v] && gor_graph_has_edge(g, u, v)) stack[top++] = v;
        }
    }
    if (count != NULL) *count = cnt;
    free(stack);
    free(visited);
    return GOR_OK;
}

gor_status gor_connected_components(const gor_graph *g, int *comp, int *ncomp)
{
    int *visited;
    int c = 0, i;
    if (!gor_graph_valid(g) || comp == NULL) return GOR_ERR_ARG;
    visited = (int *)calloc((size_t)g->n, sizeof(int));
    if (visited == NULL) return GOR_ERR_NOMEM;
    for (i = 0; i < g->n; i++) {
        int *stack;
        int top = 0;
        if (visited[i]) continue;
        stack = (int *)malloc((size_t)g->n * sizeof(int));
        if (stack == NULL) {
            free(visited);
            return GOR_ERR_NOMEM;
        }
        stack[top++] = i;
        visited[i] = 1;
        while (top > 0) {
            int u = stack[--top];
            int v;
            comp[u] = c;
            for (v = 0; v < g->n; v++) {
                /* componenti debolmente connesse: arco in una qualunque direzione */
                if (!visited[v] && (gor_graph_has_edge(g, u, v) || gor_graph_has_edge(g, v, u))) {
                    visited[v] = 1;
                    stack[top++] = v;
                }
            }
        }
        free(stack);
        c++;
    }
    free(visited);
    if (ncomp != NULL) *ncomp = c;
    return GOR_OK;
}

gor_status gor_topological_sort(const gor_graph *g, int *order)
{
    int *indeg;
    int *queue;
    int head = 0, tail = 0, cnt = 0, u, v;
    if (!gor_graph_valid(g) || order == NULL) return GOR_ERR_ARG;
    indeg = (int *)calloc((size_t)g->n, sizeof(int));
    queue = (int *)malloc((size_t)g->n * sizeof(int));
    if (indeg == NULL || queue == NULL) {
        free(indeg);
        free(queue);
        return GOR_ERR_NOMEM;
    }
    for (u = 0; u < g->n; u++)
        for (v = 0; v < g->n; v++)
            if (u != v && gor_graph_has_edge(g, u, v)) indeg[v]++;
    for (u = 0; u < g->n; u++) if (indeg[u] == 0) queue[tail++] = u;
    while (head < tail) {
        u = queue[head++];
        order[cnt++] = u;
        for (v = 0; v < g->n; v++) {
            if (u != v && gor_graph_has_edge(g, u, v)) {
                if (--indeg[v] == 0) queue[tail++] = v;
            }
        }
    }
    free(indeg);
    free(queue);
    if (cnt != g->n) return GOR_ERR_CYCLE;
    return GOR_OK;
}

/* =========================== percorsi minimi =========================== */

gor_status gor_dijkstra(const gor_graph *g, int src, gor_real *dist, int *prev)
{
    int *visited;
    int i, u, v;
    if (!gor_graph_valid(g) || dist == NULL || src < 0 || src >= g->n) return GOR_ERR_ARG;
    for (u = 0; u < g->n; u++) {
        for (v = 0; v < g->n; v++) {
            gor_real w = gor_graph_edge(g, u, v);
            if (u != v && !isinf((double)w) && w < 0.0) return GOR_ERR_ARG;
        }
    }
    visited = (int *)calloc((size_t)g->n, sizeof(int));
    if (visited == NULL) return GOR_ERR_NOMEM;
    for (i = 0; i < g->n; i++) {
        dist[i] = GOR_INF;
        if (prev != NULL) prev[i] = -1;
    }
    dist[src] = 0.0;
    for (i = 0; i < g->n; i++) {
        gor_real best = GOR_INF;
        u = -1;
        for (v = 0; v < g->n; v++) {
            if (!visited[v] && dist[v] < best) {
                best = dist[v];
                u = v;
            }
        }
        if (u < 0) break;
        visited[u] = 1;
        for (v = 0; v < g->n; v++) {
            gor_real w = gor_graph_edge(g, u, v);
            if (u != v && !isinf((double)w) && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                if (prev != NULL) prev[v] = u;
            }
        }
    }
    free(visited);
    return GOR_OK;
}

gor_status gor_bellman_ford(const gor_graph *g, int src, gor_real *dist, int *prev)
{
    int i, u, v;
    if (!gor_graph_valid(g) || dist == NULL || src < 0 || src >= g->n) return GOR_ERR_ARG;
    for (i = 0; i < g->n; i++) {
        dist[i] = GOR_INF;
        if (prev != NULL) prev[i] = -1;
    }
    dist[src] = 0.0;
    for (i = 0; i < g->n - 1; i++) {
        int changed = 0;
        for (u = 0; u < g->n; u++) {
            if (isinf((double)dist[u])) continue;
            for (v = 0; v < g->n; v++) {
                gor_real w = gor_graph_edge(g, u, v);
                if (u != v && !isinf((double)w) && dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    if (prev != NULL) prev[v] = u;
                    changed = 1;
                }
            }
        }
        if (!changed) break;
    }
    for (u = 0; u < g->n; u++) {
        if (isinf((double)dist[u])) continue;
        for (v = 0; v < g->n; v++) {
            gor_real w = gor_graph_edge(g, u, v);
            if (u != v && !isinf((double)w) && dist[u] + w < dist[v]) return GOR_ERR_NOCONV;
        }
    }
    return GOR_OK;
}

gor_status gor_floyd_warshall(const gor_graph *g, gor_real *dist)
{
    int i, j, k;
    if (!gor_graph_valid(g) || dist == NULL) return GOR_ERR_ARG;
    for (i = 0; i < g->n; i++) {
        for (j = 0; j < g->n; j++) {
            if (i == j) dist[(size_t)i * (size_t)g->n + (size_t)j] = 0.0;
            else dist[(size_t)i * (size_t)g->n + (size_t)j] = gor_graph_edge(g, i, j);
        }
    }
    for (k = 0; k < g->n; k++) {
        for (i = 0; i < g->n; i++) {
            if (isinf((double)dist[(size_t)i * (size_t)g->n + (size_t)k])) continue;
            for (j = 0; j < g->n; j++) {
                gor_real via = dist[(size_t)i * (size_t)g->n + (size_t)k]
                             + dist[(size_t)k * (size_t)g->n + (size_t)j];
                if (via < dist[(size_t)i * (size_t)g->n + (size_t)j]) {
                    dist[(size_t)i * (size_t)g->n + (size_t)j] = via;
                }
            }
        }
    }
    for (i = 0; i < g->n; i++) {
        if (dist[(size_t)i * (size_t)g->n + (size_t)i] < 0.0) return GOR_ERR_NOCONV;
    }
    return GOR_OK;
}

int gor_path_reconstruct(const int *prev, int src, int n, int dst, int *out, int *out_len)
{
    int tmp[1];
    int *rev = out;
    int len = 0, node;
    (void)tmp;
    if (prev == NULL || out == NULL || dst < 0 || dst >= n) return 0;
    node = dst;
    while (node != -1 && len < n) {
        rev[len++] = node;
        if (node == src) break;
        node = prev[node];
    }
    if (len == 0 || rev[len - 1] != src) return 0;
    /* inverti in place */
    {
        int i;
        for (i = 0; i < len / 2; i++) {
            int t = rev[i];
            rev[i] = rev[len - 1 - i];
            rev[len - 1 - i] = t;
        }
    }
    if (out_len != NULL) *out_len = len;
    return 1;
}

/* =============================== MST =============================== */

static int edge_cmp(const void *a, const void *b)
{
    gor_real wa = ((const gor_edge *)a)->w;
    gor_real wb = ((const gor_edge *)b)->w;
    if (wa < wb) return -1;
    if (wa > wb) return 1;
    return 0;
}

gor_status gor_mst_kruskal(const gor_graph *g, gor_edge *edges, int *n_edges, gor_real *total)
{
    gor_edge *all;
    gor_uf uf;
    int cap = 0, i, j, used = 0;
    gor_real sum = 0.0;
    if (!gor_graph_valid(g) || edges == NULL) return GOR_ERR_ARG;
    if (g->directed) return GOR_ERR_ARG; /* MST definito su grafi non orientati */
    all = (gor_edge *)malloc((size_t)g->n * (size_t)g->n * sizeof(gor_edge));
    if (all == NULL) return GOR_ERR_NOMEM;
    for (i = 0; i < g->n; i++) {
        for (j = i + 1; j < g->n; j++) {
            if (gor_graph_has_edge(g, i, j)) {
                all[cap].u = i;
                all[cap].v = j;
                all[cap].w = gor_graph_edge(g, i, j);
                cap++;
            }
        }
    }
    qsort(all, (size_t)cap, sizeof(gor_edge), edge_cmp);
    if (gor_uf_init(&uf, g->n) != GOR_OK) {
        free(all);
        return GOR_ERR_NOMEM;
    }
    for (i = 0; i < cap && used < g->n - 1; i++) {
        if (gor_uf_union(&uf, all[i].u, all[i].v)) {
            edges[used++] = all[i];
            sum += all[i].w;
        }
    }
    gor_uf_free(&uf);
    free(all);
    if (n_edges != NULL) *n_edges = used;
    if (total != NULL) *total = sum;
    return (used == g->n - 1) ? GOR_OK : GOR_ERR_INFEASIBLE;
}

gor_status gor_mst_prim(const gor_graph *g, gor_edge *edges, int *n_edges, gor_real *total)
{
    gor_real *key;
    int *parent;
    int *in_mst;
    int i, u, v, used = 0;
    gor_real sum = 0.0;
    if (!gor_graph_valid(g) || edges == NULL) return GOR_ERR_ARG;
    if (g->directed) return GOR_ERR_ARG;
    key = (gor_real *)malloc((size_t)g->n * sizeof(gor_real));
    parent = (int *)malloc((size_t)g->n * sizeof(int));
    in_mst = (int *)calloc((size_t)g->n, sizeof(int));
    if (key == NULL || parent == NULL || in_mst == NULL) {
        free(key);
        free(parent);
        free(in_mst);
        return GOR_ERR_NOMEM;
    }
    for (i = 0; i < g->n; i++) {
        key[i] = GOR_INF;
        parent[i] = -1;
    }
    key[0] = 0.0;
    for (i = 0; i < g->n; i++) {
        gor_real best = GOR_INF;
        u = -1;
        for (v = 0; v < g->n; v++) {
            if (!in_mst[v] && key[v] < best) {
                best = key[v];
                u = v;
            }
        }
        if (u < 0) break;
        in_mst[u] = 1;
        for (v = 0; v < g->n; v++) {
            gor_real w = gor_graph_edge(g, u, v);
            if (!in_mst[v] && !isinf((double)w) && w < key[v]) {
                key[v] = w;
                parent[v] = u;
            }
        }
    }
    for (v = 1; v < g->n; v++) {
        if (parent[v] >= 0) {
            edges[used].u = parent[v];
            edges[used].v = v;
            edges[used].w = gor_graph_edge(g, parent[v], v);
            sum += edges[used].w;
            used++;
        }
    }
    free(key);
    free(parent);
    free(in_mst);
    if (n_edges != NULL) *n_edges = used;
    if (total != NULL) *total = sum;
    return (used == g->n - 1) ? GOR_OK : GOR_ERR_INFEASIBLE;
}
