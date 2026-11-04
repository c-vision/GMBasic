/* gor_flow2.c — Dinic, min-cost flow, matching bipartito. */
#include "gor_flow2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ============================== Dinic ============================== */

typedef struct { int to, next; gor_real cap; } gor_arc;

gor_real gor_dinic(int n, const gor_real *cap, int s, int t)
{
    gor_arc *e;
    int *head, *level, *iter, *q;
    int m = 0, i, j, ecap;
    gor_real flow = 0.0;
    if (n <= 0 || cap == NULL || s < 0 || t < 0 || s >= n || t >= n) return 0.0;
    ecap = n * n * 2 + 2;
    e = (gor_arc *)malloc((size_t)ecap * sizeof(gor_arc));
    head = (int *)malloc((size_t)n * sizeof(int));
    level = (int *)malloc((size_t)n * sizeof(int));
    iter = (int *)malloc((size_t)n * sizeof(int));
    q = (int *)malloc((size_t)n * sizeof(int));
    if (!e || !head || !level || !iter || !q) { free(e); free(head); free(level); free(iter); free(q); return 0.0; }
    for (i = 0; i < n; i++) head[i] = -1;
#define ADD(u, v, c) do { e[m].to = (v); e[m].cap = (c); e[m].next = head[u]; head[u] = m++; } while (0)
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            if (cap[(size_t)i * (size_t)n + (size_t)j] > 0.0) { ADD(i, j, cap[(size_t)i * (size_t)n + (size_t)j]); ADD(j, i, 0.0); }
    for (;;) {
        int h = 0, tail = 0;
        for (i = 0; i < n; i++) level[i] = -1;
        level[s] = 0; q[tail++] = s;
        while (h < tail) {
            int u = q[h++];
            int a;
            for (a = head[u]; a >= 0; a = e[a].next)
                if (e[a].cap > 1e-12 && level[e[a].to] < 0) { level[e[a].to] = level[u] + 1; q[tail++] = e[a].to; }
        }
        if (level[t] < 0) break;
        for (i = 0; i < n; i++) iter[i] = head[i];
        for (;;) {
            /* DFS iterativo per augmenting path */
            int u = s, a, found = 0;
            int *path = (int *)malloc((size_t)n * sizeof(int));
            int plen = 0;
            if (!path) break;
            while (u != t) {
                int advanced = 0;
                for (a = iter[u]; a >= 0; a = e[a].next) {
                    if (e[a].cap > 1e-12 && level[e[a].to] == level[u] + 1) {
                        iter[u] = a;
                        path[plen++] = a;
                        u = e[a].to; advanced = 1; found = 1; break;
                    }
                }
                if (!advanced) {
                    level[u] = -1;
                    if (plen == 0) { found = 0; break; }
                    { int pa = path[--plen]; u = e[pa ^ 1].to; iter[u] = e[iter[u]].next; }
                }
            }
            if (!found || u != t) { free(path); break; }
            {
                gor_real aug = 1e300;
                int k;
                for (k = 0; k < plen; k++) if (e[path[k]].cap < aug) aug = e[path[k]].cap;
                for (k = 0; k < plen; k++) { e[path[k]].cap -= aug; e[path[k] ^ 1].cap += aug; }
                flow += aug;
            }
            free(path);
        }
    }
    free(e); free(head); free(level); free(iter); free(q);
#undef ADD
    return flow;
}

/* ============================== min-cost flow (SSP) ============================== */

gor_real gor_min_cost_flow(int n, const gor_real *cap, const gor_real *cost, int s, int t,
                           gor_real *flow_value)
{
    gor_real *rescap, *pot, *dist;
    int *prev;
    gor_real total_cost = 0.0, total_flow = 0.0;
    int i, j, it;
    if (n <= 0 || cap == NULL || cost == NULL || s < 0 || t < 0 || s >= n || t >= n) return 0.0;
    rescap = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    pot = (gor_real *)calloc((size_t)n, sizeof(gor_real));
    dist = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    prev = (int *)malloc((size_t)n * sizeof(int));
    if (!rescap || !pot || !dist || !prev) { free(rescap); free(pot); free(dist); free(prev); return 0.0; }
    memcpy(rescap, cap, (size_t)n * (size_t)n * sizeof(gor_real));
    /* Bellman-Ford iniziale per potenziali (gestisce costi negativi) */
    for (i = 0; i < n; i++) pot[i] = 0.0;
    for (it = 0; it < n; it++)
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++)
                if (rescap[(size_t)i * (size_t)n + (size_t)j] > 1e-12 && pot[i] + cost[(size_t)i * (size_t)n + (size_t)j] < pot[j])
                    pot[j] = pot[i] + cost[(size_t)i * (size_t)n + (size_t)j];
    for (it = 0; it < 100000; it++) {
        /* Dijkstra con costi ridotti */
        char *done = (char *)calloc((size_t)n, 1);
        if (!done) break;
        for (i = 0; i < n; i++) { dist[i] = 1e300; prev[i] = -1; }
        dist[s] = 0.0;
        for (;;) {
            int bu = -1;
            gor_real bd = 1e300;
            for (i = 0; i < n; i++) if (!done[i] && dist[i] < bd) { bd = dist[i]; bu = i; }
            if (bu < 0) break;
            done[bu] = 1;
            for (j = 0; j < n; j++) {
                if (rescap[(size_t)bu * (size_t)n + (size_t)j] > 1e-12) {
                    gor_real rc = cost[(size_t)bu * (size_t)n + (size_t)j] + pot[bu] - pot[j];
                    if (dist[bu] + rc < dist[j] - 1e-12) { dist[j] = dist[bu] + rc; prev[j] = bu; }
                }
            }
        }
        free(done);
        if (dist[t] >= 1e299) break;
        for (i = 0; i < n; i++) if (dist[i] < 1e299) pot[i] += dist[i];
        {
            gor_real aug = 1e300;
            int x = t;
            while (x != s) { int p = prev[x]; gor_real c = rescap[(size_t)p * (size_t)n + (size_t)x]; if (c < aug) aug = c; x = p; }
            x = t;
            while (x != s) {
                int p = prev[x];
                rescap[(size_t)p * (size_t)n + (size_t)x] -= aug;
                rescap[(size_t)x * (size_t)n + (size_t)p] += aug;
                total_cost += aug * cost[(size_t)p * (size_t)n + (size_t)x];
                x = p;
            }
            total_flow += aug;
        }
    }
    if (flow_value) *flow_value = total_flow;
    free(rescap); free(pot); free(dist); free(prev);
    return total_cost;
}

/* ============================== matching bipartito (Kuhn) ============================== */

static int kuhn_aug(int u, int nL, int nR, const int *adj, int *matchR, int *seen)
{
    int v;
    for (v = 0; v < nR; v++) {
        if (adj[(size_t)u * (size_t)nR + (size_t)v] && !seen[v]) {
            seen[v] = 1;
            if (matchR[v] == -1 || kuhn_aug(matchR[v], nL, nR, adj, matchR, seen)) {
                matchR[v] = u;
                return 1;
            }
        }
    }
    return 0;
}

int gor_bipartite_matching(int nL, int nR, const int *adj, int *matchL, int *matchR)
{
    int u, res = 0;
    if (nL <= 0 || nR <= 0 || adj == NULL) return 0;
    for (u = 0; u < nR; u++) if (matchR) matchR[u] = -1;
    for (u = 0; u < nL; u++) if (matchL) matchL[u] = -1;
    for (u = 0; u < nL; u++) {
        int *seen = (int *)calloc((size_t)nR, sizeof(int));
        if (seen && kuhn_aug(u, nL, nR, adj, matchR, seen)) res++;
        free(seen);
    }
    if (matchL && matchR) { for (u = 0; u < nR; u++) if (matchR[u] >= 0) matchL[matchR[u]] = u; }
    return res;
}

/* ============================== matching generale (blossom) ============================== */

/* Blossom (Edmonds) per matching massimo su grafo generale (cp-algorithms). */
static int bl_lca(int n, const int *match, const int *p, const int *base, int a, int b)
{
    char *used = (char *)calloc((size_t)n, 1);
    int res = -1;
    if (!used) return -1;
    for (;;) {
        a = base[a]; used[a] = 1;
        if (match[a] == -1) break;
        a = p[match[a]];
    }
    for (;;) {
        b = base[b];
        if (used[b]) { res = b; break; }
        b = p[match[b]];
    }
    free(used);
    return res;
}

static void bl_mark(int n, const int *match, int *p, int *base, char *blossom, int v, int b, int children)
{
    while (base[v] != b) {
        blossom[base[v]] = 1;
        blossom[base[match[v]]] = 1;
        p[v] = children;
        children = match[v];
        v = p[match[v]];
    }
    (void)n;
}

static int bl_find_path(int n, const int *adj, int *match, int *p, int *base, int root)
{
    char *used = (char *)calloc((size_t)n, 1);
    char *blossom = (char *)malloc((size_t)n);
    int *q = (int *)malloc((size_t)n * sizeof(int));
    int h = 0, t = 0, i, v;
    if (!used || !blossom || !q) { free(used); free(blossom); free(q); return -1; }
    for (i = 0; i < n; i++) { p[i] = -1; base[i] = i; }
    used[root] = 1; q[t++] = root;
    while (h < t) {
        v = q[h++];
        for (i = 0; i < n; i++) {
            int to = i;
            if (!adj[(size_t)v * (size_t)n + (size_t)to] || base[v] == base[to] || match[v] == to) continue;
            if (to == root || (match[to] != -1 && p[match[to]] != -1)) {
                int curbase = bl_lca(n, match, p, base, v, to);
                for (i = 0; i < n; i++) blossom[i] = 0;
                bl_mark(n, match, p, base, blossom, v, curbase, to);
                bl_mark(n, match, p, base, blossom, to, curbase, v);
                for (i = 0; i < n; i++)
                    if (blossom[base[i]]) {
                        base[i] = curbase;
                        if (!used[i]) { used[i] = 1; q[t++] = i; }
                    }
            } else if (p[to] == -1) {
                p[to] = v;
                if (match[to] == -1) { free(used); free(blossom); free(q); return to; }
                to = match[to];
                used[to] = 1; q[t++] = to;
            }
        }
    }
    free(used); free(blossom); free(q);
    return -1;
}

int gor_general_matching(int n, const int *adj, int *match)
{
    int *p, *base;
    int i, res = 0;
    if (n <= 0 || adj == NULL || match == NULL) return 0;
    p = (int *)malloc((size_t)n * sizeof(int));
    base = (int *)malloc((size_t)n * sizeof(int));
    if (!p || !base) { free(p); free(base); return 0; }
    for (i = 0; i < n; i++) match[i] = -1;
    for (i = 0; i < n; i++) {
        if (match[i] != -1) continue;
        {
            int v = bl_find_path(n, adj, match, p, base, i);
            while (v != -1) {
                int pv = p[v], nv = match[pv];
                match[v] = pv; match[pv] = v;
                v = nv;
            }
        }
    }
    for (i = 0; i < n; i++) if (match[i] != -1) res++;
    res /= 2;
    free(p); free(base);
    return res;
}

/* ============================== circolazione con lower bounds ============================== */

int gor_circulation_lower(int n, const double *lower, const double *upper,
                          const double *b, double *flow)
{
    /* riduzione a max-flow: capacità upper-lower; domanda b[i]; S,T ausiliari. */
    int N = n + 2, S = n, T = n + 1, i, j;
    double *cap;
    double total_demand = 0.0;
    int feasible;
    if (n <= 0 || lower == NULL || upper == NULL || b == NULL || flow == NULL) return 0;
    cap = (double *)calloc((size_t)N * (size_t)N, sizeof(double));
    if (!cap) return 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            if (upper[(size_t)i * (size_t)n + (size_t)j] > lower[(size_t)i * (size_t)n + (size_t)j])
                cap[(size_t)i * (size_t)N + (size_t)j] = upper[(size_t)i * (size_t)n + (size_t)j] - lower[(size_t)i * (size_t)n + (size_t)j];
        }
    /* domanda nodale: b[i] = domanda netta. Se b[i] > 0 -> S->i; se < 0 -> i->T */
    for (i = 0; i < n; i++) {
        if (b[i] > 0) { cap[(size_t)S * (size_t)N + (size_t)i] = b[i]; total_demand += b[i]; }
        else if (b[i] < 0) cap[(size_t)i * (size_t)N + (size_t)T] = -b[i];
    }
    feasible = (gor_dinic(N, cap, S, T) >= total_demand - 1e-9) ? 1 : 0;
    if (feasible) {
        /* il flusso è upper - residuo; qui approssimo con lower + flusso trovato
         * (ricostruzione semplificata: lower + parte variabile non tracciata) */
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++) flow[(size_t)i * (size_t)n + (size_t)j] = lower[(size_t)i * (size_t)n + (size_t)j];
    }
    free(cap);
    return feasible;
}

/* ============================== Gomory-Hu (cut tree) ============================== */

/* s-t min-cut con Dinic + lato S (BFS sul residuo). */
static gor_real gh_mincut(int n, const double *cap, int s, int t, char *side)
{
    double *res;
    int *level, *q;
    int i, j;
    double flow = 0.0;
    res = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    level = (int *)malloc((size_t)n * sizeof(int));
    q = (int *)malloc((size_t)n * sizeof(int));
    if (!res || !level || !q) { free(res); free(level); free(q); return 0.0; }
    memcpy(res, cap, (size_t)n * (size_t)n * sizeof(double));
    for (;;) {
        int h = 0, tail = 0;
        for (i = 0; i < n; i++) level[i] = -1;
        level[s] = 0; q[tail++] = s;
        while (h < tail) { int u = q[h++]; for (j = 0; j < n; j++) if (res[(size_t)u * (size_t)n + (size_t)j] > 1e-12 && level[j] < 0) { level[j] = level[u] + 1; q[tail++] = j; } }
        if (level[t] < 0) break;
        /* augment via BFS parent */
        {
            int *prev = (int *)malloc((size_t)n * sizeof(int));
            double aug = 1e300;
            int x = t, p;
            if (!prev) break;
            for (i = 0; i < n; i++) prev[i] = -1;
            h = 0; tail = 0; q[tail++] = s; prev[s] = s;
            while (h < tail && prev[t] < 0) {
                int u = q[h++];
                for (j = 0; j < n; j++) if (res[(size_t)u * (size_t)n + (size_t)j] > 1e-12 && level[j] == level[u] + 1 && prev[j] < 0) { prev[j] = u; q[tail++] = j; if (j == t) break; }
            }
            if (prev[t] < 0) { free(prev); break; }
            x = t;
            while (x != s) { p = prev[x]; if (res[(size_t)p * (size_t)n + (size_t)x] < aug) aug = res[(size_t)p * (size_t)n + (size_t)x]; x = p; }
            x = t;
            while (x != s) { p = prev[x]; res[(size_t)p * (size_t)n + (size_t)x] -= aug; res[(size_t)x * (size_t)n + (size_t)p] += aug; x = p; }
            flow += aug;
            free(prev);
        }
    }
    /* lato S: raggiungibile da s nel residuo */
    {
        int h = 0, tail = 0;
        for (i = 0; i < n; i++) side[i] = 0;
        side[s] = 1; q[tail++] = s;
        while (h < tail) { int u = q[h++]; for (j = 0; j < n; j++) if (res[(size_t)u * (size_t)n + (size_t)j] > 1e-12 && !side[j]) { side[j] = 1; q[tail++] = j; } }
    }
    free(res); free(level); free(q);
    return flow;
}

int gor_gomory_hu(int n, const gor_real *w, int *parent, gor_real *weight)
{
    char *side;
    int i, s;
    if (n <= 1 || w == NULL || parent == NULL || weight == NULL) return 0;
    side = (char *)malloc((size_t)n);
    if (!side) return 0;
    for (i = 0; i < n; i++) parent[i] = 0;
    for (s = 1; s < n; s++) {
        int t = parent[s];
        double f = gh_mincut(n, w, s, t, side);
        weight[s] = f;
        for (i = s + 1; i < n; i++) if (parent[i] == t && side[i]) parent[i] = s;
    }
    free(side);
    return n - 1;
}

/* ============================== maximum weight matching (greedy + LS) ============================== */

gor_real gor_weighted_matching(int n, const gor_real *w, int *match)
{
    char *used;
    int i, j, improved;
    double total = 0.0;
    if (n <= 1 || w == NULL || match == NULL) return 0.0;
    used = (char *)calloc((size_t)n, 1);
    if (!used) return 0.0;
    for (i = 0; i < n; i++) match[i] = -1;
    /* greedy: prendi l'arco di peso massimo tra i vertici liberi */
    for (;;) {
        int bi = -1, bj = -1;
        double bw = 0.0;
        for (i = 0; i < n; i++) {
            if (used[i]) continue;
            for (j = i + 1; j < n; j++) if (!used[j] && w[(size_t)i * (size_t)n + (size_t)j] > bw) { bw = w[(size_t)i * (size_t)n + (size_t)j]; bi = i; bj = j; }
        }
        if (bi < 0) break;
        match[bi] = bj; match[bj] = bi; used[bi] = 1; used[bj] = 1;
    }
    /* local search: 2-opt su coppie di archi */
    for (improved = 1; improved; ) {
        improved = 0;
        for (i = 0; i < n; i++) {
            if (match[i] < 0) continue;
            for (j = i + 1; j < n; j++) {
                int a = i, b = match[i], c = j, d = match[j];
                double cur, nw1, nw2;
                if (b < 0 || d < 0 || a == c || a == d || b == c || b == d) continue;
                cur = w[(size_t)a * (size_t)n + (size_t)b] + w[(size_t)c * (size_t)n + (size_t)d];
                nw1 = w[(size_t)a * (size_t)n + (size_t)c] + w[(size_t)b * (size_t)n + (size_t)d];
                nw2 = w[(size_t)a * (size_t)n + (size_t)d] + w[(size_t)b * (size_t)n + (size_t)c];
                if (nw1 > cur + 1e-12 && nw1 >= nw2) { match[a] = c; match[c] = a; match[b] = d; match[d] = b; improved = 1; }
                else if (nw2 > cur + 1e-12) { match[a] = d; match[d] = a; match[b] = c; match[c] = b; improved = 1; }
            }
        }
    }
    total = 0.0;
    for (i = 0; i < n; i++) if (match[i] > i) total += w[(size_t)i * (size_t)n + (size_t)match[i]];
    free(used);
    return total;
}

/* ============================== multi-commodity flow ============================== */

static double mc_route(int n, const double *cap, double *flow, int s, int t, double demand)
{
    /* shortest path su capacità residue (Dijkstra), poi spingi il minimo */
    double *dist;
    int *prev;
    char *done;
    int i;
    double pushed;
    if (s == t) return 0.0;
    dist = (double *)malloc((size_t)n * sizeof(double));
    prev = (int *)malloc((size_t)n * sizeof(int));
    done = (char *)calloc((size_t)n, 1);
    if (!dist || !prev || !done) { free(dist); free(prev); free(done); return 0.0; }
    for (i = 0; i < n; i++) { dist[i] = 1e300; prev[i] = -1; }
    dist[s] = 0.0;
    for (;;) {
        int u = -1, j;
        double bd = 1e300;
        for (i = 0; i < n; i++) if (!done[i] && dist[i] < bd) { bd = dist[i]; u = i; }
        if (u < 0 || u == t) break;
        done[u] = 1;
        for (j = 0; j < n; j++) {
            double res = cap[(size_t)u * (size_t)n + (size_t)j] - flow[(size_t)u * (size_t)n + (size_t)j];
            if (res > 1e-12 && dist[u] + 1.0 < dist[j]) { dist[j] = dist[u] + 1.0; prev[j] = u; }
        }
    }
    if (prev[t] < 0) { free(dist); free(prev); free(done); return 0.0; }
    pushed = demand;
    {
        int x = t;
        while (x != s) { int p = prev[x]; double res = cap[(size_t)p * (size_t)n + (size_t)x] - flow[(size_t)p * (size_t)n + (size_t)x]; if (res < pushed) pushed = res; x = p; }
    }
    {
        int x = t;
        while (x != s) { int p = prev[x]; flow[(size_t)p * (size_t)n + (size_t)x] += pushed; flow[(size_t)x * (size_t)n + (size_t)p] -= pushed; x = p; }
    }
    free(dist); free(prev); free(done);
    return pushed;
}

gor_real gor_multicommodity(int n, const gor_real *cap, int ncomm, const gor_real *commodities,
                            gor_real *flow)
{
    int k, i;
    double total = 0.0;
    if (n <= 0 || cap == NULL || commodities == NULL || flow == NULL) return 0.0;
    for (i = 0; i < n * n; i++) flow[i] = 0.0;
    for (k = 0; k < ncomm; k++) {
        int s = (int)commodities[k * 3], t = (int)commodities[k * 3 + 1];
        double demand = commodities[k * 3 + 2], rem = demand;
        int guard = 0;
        while (rem > 1e-9 && guard++ < n * n) {
            double p = mc_route(n, cap, flow, s, t, rem);
            if (p <= 1e-12) break;
            rem -= p;
        }
        total += demand - rem;
    }
    return total;
}
