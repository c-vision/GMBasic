/* gor_flow.c — max-flow (Edmonds-Karp) e min-cut. */
#include "gor_flow.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GOR_INF (INFINITY)

static gor_real flow_cap(const gor_graph *g, int u, int v)
{
    gor_real w = gor_graph_edge(g, u, v);
    if (isinf((double)w)) return 0.0;
    return w;
}

/* Edmonds-Karp su matrice di capacità residua `res` (n*n, preallocata). */
static gor_status edmonds_karp(const gor_graph *g, int s, int t, gor_real *value, gor_real *res)
{
    int n = g->n;
    int *parent;
    int *visited;
    int *queue;
    gor_real total = 0.0;
    int u, v;

    parent = (int *)malloc((size_t)n * sizeof(int));
    visited = (int *)malloc((size_t)n * sizeof(int));
    queue = (int *)malloc((size_t)n * sizeof(int));
    if (parent == NULL || visited == NULL || queue == NULL) {
        free(parent);
        free(visited);
        free(queue);
        return GOR_ERR_NOMEM;
    }

    for (;;) {
        int head = 0, tail = 0;
        int reached = 0;
        for (u = 0; u < n; u++) {
            visited[u] = 0;
            parent[u] = -1;
        }
        visited[s] = 1;
        queue[tail++] = s;
        while (head < tail && !reached) {
            u = queue[head++];
            for (v = 0; v < n; v++) {
                if (!visited[v] && res[(size_t)u * (size_t)n + (size_t)v] > 0.0) {
                    visited[v] = 1;
                    parent[v] = u;
                    if (v == t) {
                        reached = 1;
                        break;
                    }
                    queue[tail++] = v;
                }
            }
        }
        if (!reached) break;
        {
            gor_real bottleneck = GOR_INF;
            for (v = t; v != s; v = parent[v]) {
                gor_real r = res[(size_t)parent[v] * (size_t)n + (size_t)v];
                if (r < bottleneck) bottleneck = r;
            }
            for (v = t; v != s; v = parent[v]) {
                u = parent[v];
                res[(size_t)u * (size_t)n + (size_t)v] -= bottleneck;
                res[(size_t)v * (size_t)n + (size_t)u] += bottleneck;
            }
            total += bottleneck;
        }
    }

    free(parent);
    free(visited);
    free(queue);
    if (value != NULL) *value = total;
    return GOR_OK;
}

gor_status gor_max_flow(const gor_graph *g, int s, int t, gor_real *value, gor_real *flow)
{
    int n, u, v;
    gor_real *res;
    gor_status st;
    if (g == NULL || g->w == NULL || g->n <= 0) return GOR_ERR_ARG;
    if (s < 0 || s >= g->n || t < 0 || t >= g->n || s == t) return GOR_ERR_ARG;
    n = g->n;
    res = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    if (res == NULL) return GOR_ERR_NOMEM;
    for (u = 0; u < n; u++) {
        for (v = 0; v < n; v++) {
            gor_real c = flow_cap(g, u, v);
            if (c < 0.0) {
                free(res);
                return GOR_ERR_ARG;
            }
            res[(size_t)u * (size_t)n + (size_t)v] = (u == v) ? 0.0 : c;
        }
    }
    st = edmonds_karp(g, s, t, value, res);
    if (st == GOR_OK && flow != NULL) {
        for (u = 0; u < n; u++) {
            for (v = 0; v < n; v++) {
                gor_real cap0 = flow_cap(g, u, v);
                flow[(size_t)u * (size_t)n + (size_t)v] =
                    cap0 - res[(size_t)u * (size_t)n + (size_t)v];
            }
        }
    }
    free(res);
    return st;
}

gor_status gor_min_cut(const gor_graph *g, int s, int t, int *side)
{
    int n, u, v;
    gor_real *res;
    int *visited;
    int *queue;
    gor_status st;
    if (g == NULL || g->w == NULL || g->n <= 0 || side == NULL) return GOR_ERR_ARG;
    if (s < 0 || s >= g->n || t < 0 || t >= g->n || s == t) return GOR_ERR_ARG;
    n = g->n;
    res = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    visited = (int *)malloc((size_t)n * sizeof(int));
    queue = (int *)malloc((size_t)n * sizeof(int));
    if (res == NULL || visited == NULL || queue == NULL) {
        free(res);
        free(visited);
        free(queue);
        return GOR_ERR_NOMEM;
    }
    for (u = 0; u < n; u++)
        for (v = 0; v < n; v++)
            res[(size_t)u * (size_t)n + (size_t)v] = (u == v) ? 0.0 : flow_cap(g, u, v);

    st = edmonds_karp(g, s, t, NULL, res);
    if (st != GOR_OK) {
        free(res);
        free(visited);
        free(queue);
        return st;
    }
    /* lato S = raggiungibile da s nel residuo */
    for (u = 0; u < n; u++) visited[u] = 0;
    {
        int head = 0, tail = 0;
        visited[s] = 1;
        queue[tail++] = s;
        while (head < tail) {
            u = queue[head++];
            for (v = 0; v < n; v++) {
                if (!visited[v] && res[(size_t)u * (size_t)n + (size_t)v] > 0.0) {
                    visited[v] = 1;
                    queue[tail++] = v;
                }
            }
        }
    }
    for (u = 0; u < n; u++) side[u] = visited[u] ? 0 : 1;
    free(res);
    free(visited);
    free(queue);
    return GOR_OK;
}
