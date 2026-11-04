/*
 * gor_graph3.c — proprietà strutturali dei grafi.
 *
 * Porting delle funzioni del motore nativo GMBasic (gmb_graph.c / gmb_graph_alg.c)
 * che non avevano un equivalente in gor_graph/gor_graph2:
 *   gmb_graph_has_cycle, gmb_graph_is_bipartite, gmb_graph_diameter,
 *   gmb_graph_eccentricity, gmb_graph_clustering_coefficient, sottografo.
 *
 * Rappresentazione: adiacenza densa n×n row-major (vedi gor_graph3.h).
 */
#include "gor_graph3.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GOR_INF (INFINITY)

/* ---------------------------------------------------------------- has_cycle */
int gor_has_cycle(int n, const int *adj)
{
    int i, j, head = 0, tail = 0, seen = 0;
    int *indeg = NULL;
    int *queue = NULL;

    if (n <= 0 || adj == NULL) return 0;
    indeg = (int *)calloc((size_t)n, sizeof(int));
    queue = (int *)malloc((size_t)n * sizeof(int));
    if (indeg == NULL || queue == NULL) { free(indeg); free(queue); return 0; }

    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            if (adj[i * n + j]) indeg[j]++;

    for (i = 0; i < n; i++)
        if (indeg[i] == 0) queue[tail++] = i;

    while (head < tail) {
        int u = queue[head++];
        seen++;
        for (j = 0; j < n; j++)
            if (adj[u * n + j] && --indeg[j] == 0)
                queue[tail++] = j;
    }

    free(indeg);
    free(queue);
    return seen < n;
}

/* ------------------------------------------------------------ is_bipartite */
int gor_is_bipartite(int n, const int *adj)
{
    int i, j, head = 0, tail = 0, ok = 1;
    int *color = NULL;
    int *queue = NULL;

    if (n <= 0 || adj == NULL) return 1;
    color = (int *)malloc((size_t)n * sizeof(int));
    queue = (int *)malloc((size_t)n * sizeof(int));
    if (color == NULL || queue == NULL) { free(color); free(queue); return 0; }
    for (i = 0; i < n; i++) color[i] = -1;

    for (i = 0; i < n && ok; i++) {
        if (color[i] != -1) continue;
        color[i] = 0;
        head = tail = 0;
        queue[tail++] = i;
        while (head < tail && ok) {
            int u = queue[head++];
            for (j = 0; j < n; j++) {
                /* grafo non orientato: un arco in una sola direzione conta comunque */
                if (!adj[u * n + j] && !adj[j * n + u]) continue;
                if (j == u) continue;
                if (color[j] == -1) { color[j] = 1 - color[u]; queue[tail++] = j; }
                else if (color[j] == color[u]) { ok = 0; break; }
            }
        }
    }

    free(color);
    free(queue);
    return ok;
}

/* --------------------------------------------------------------- diameter */
gor_real gor_diameter(int n, const gor_real *w)
{
    int i, j, k;
    gor_real best = 0.0;
    gor_real *d = NULL;

    if (n < 2 || w == NULL) return 0.0;
    d = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    if (d == NULL) return 0.0;
    memcpy(d, w, (size_t)n * (size_t)n * sizeof(gor_real));

    for (k = 0; k < n; k++)
        for (i = 0; i < n; i++) {
            if (d[i * n + k] == GOR_INF) continue;
            for (j = 0; j < n; j++) {
                if (d[k * n + j] == GOR_INF) continue;
                if (d[i * n + k] + d[k * n + j] < d[i * n + j])
                    d[i * n + j] = d[i * n + k] + d[k * n + j];
            }
        }

    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            if (i != j && d[i * n + j] != GOR_INF && d[i * n + j] > best)
                best = d[i * n + j];

    free(d);
    return best;
}

/* ----------------------------------------------------------- eccentricity */
gor_real gor_eccentricity(int n, const gor_real *w, int v)
{
    int i, u, it;
    gor_real best = 0.0;
    gor_real *dist = NULL;
    int *done = NULL;

    if (n <= 0 || w == NULL || v < 0 || v >= n) return -1.0;
    dist = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    done = (int *)calloc((size_t)n, sizeof(int));
    if (dist == NULL || done == NULL) { free(dist); free(done); return -1.0; }

    for (i = 0; i < n; i++) dist[i] = GOR_INF;
    dist[v] = 0.0;

    for (it = 0; it < n; it++) {
        int pick = -1;
        gor_real bestd = GOR_INF;
        for (i = 0; i < n; i++)
            if (!done[i] && dist[i] < bestd) { bestd = dist[i]; pick = i; }
        if (pick < 0) break;
        done[pick] = 1;
        for (u = 0; u < n; u++) {
            gor_real wv = w[pick * n + u];
            if (wv == GOR_INF) continue;
            if (dist[pick] + wv < dist[u]) dist[u] = dist[pick] + wv;
        }
    }

    for (i = 0; i < n; i++)
        if (i != v && dist[i] != GOR_INF && dist[i] > best) best = dist[i];

    free(dist);
    free(done);
    return best;
}

/* --------------------------------------------------- clustering coefficient */
gor_real gor_clustering_coefficient(int n, const int *adj, int v)
{
    int i, j, k = 0, links = 0;
    int *nb = NULL;

    if (n <= 0 || adj == NULL || v < 0 || v >= n) return 0.0;
    nb = (int *)malloc((size_t)n * sizeof(int));
    if (nb == NULL) return 0.0;

    for (i = 0; i < n; i++)
        if (i != v && (adj[v * n + i] || adj[i * n + v])) nb[k++] = i;

    if (k < 2) { free(nb); return 0.0; }

    for (i = 0; i < k; i++)
        for (j = i + 1; j < k; j++) {
            int a = nb[i], b = nb[j];
            if (adj[a * n + b] || adj[b * n + a]) links++;
        }

    free(nb);
    return (2.0 * (gor_real)links) / ((gor_real)k * (gor_real)(k - 1));
}

/* ---------------------------------------------------------------- subgraph */
gor_status gor_subgraph(int n, const gor_real *w, const int *nodes, int k, gor_real *out)
{
    int i, j;

    if (n <= 0 || w == NULL || nodes == NULL || out == NULL || k < 0) return GOR_ERR_ARG;
    for (i = 0; i < k; i++)
        if (nodes[i] < 0 || nodes[i] >= n) return GOR_ERR_RANGE;

    for (i = 0; i < k; i++)
        for (j = 0; j < k; j++)
            out[i * k + j] = w[nodes[i] * n + nodes[j]];

    return GOR_OK;
}
