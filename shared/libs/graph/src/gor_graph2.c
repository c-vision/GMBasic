/* gor_graph2.c — algoritmi avanzati su grafi. */
#include "gor_graph2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define G2_INF 1e300

/* ============================== SCC (Tarjan) ============================== */

typedef struct {
    int n;
    const int *adj;
    int *index, *low, *onstack, *comp, *stack;
    int idx, top, ncomp;
} tarjan_ctx;

static void tarjan_dfs(tarjan_ctx *c, int u)
{
    int v;
    c->index[u] = c->low[u] = c->idx++;
    c->stack[c->top++] = u; c->onstack[u] = 1;
    for (v = 0; v < c->n; v++) {
        if (!c->adj[(size_t)u * (size_t)c->n + (size_t)v]) continue;
        if (c->index[v] < 0) { tarjan_dfs(c, v); if (c->low[v] < c->low[u]) c->low[u] = c->low[v]; }
        else if (c->onstack[v] && c->index[v] < c->low[u]) c->low[u] = c->index[v];
    }
    if (c->low[u] == c->index[u]) {
        int w;
        do { w = c->stack[--c->top]; c->onstack[w] = 0; c->comp[w] = c->ncomp; } while (w != u);
        c->ncomp++;
    }
}

int gor_scc_tarjan(int n, const int *adj, int *comp)
{
    tarjan_ctx c;
    int i;
    if (n <= 0 || adj == NULL || comp == NULL) return 0;
    c.n = n; c.adj = adj; c.comp = comp;
    c.index = (int *)malloc((size_t)n * sizeof(int));
    c.low = (int *)malloc((size_t)n * sizeof(int));
    c.onstack = (int *)calloc((size_t)n, sizeof(int));
    c.stack = (int *)malloc((size_t)n * sizeof(int));
    if (!c.index || !c.low || !c.onstack || !c.stack) {
        free(c.index); free(c.low); free(c.onstack); free(c.stack); return 0;
    }
    for (i = 0; i < n; i++) c.index[i] = -1;
    c.idx = 0; c.top = 0; c.ncomp = 0;
    for (i = 0; i < n; i++) if (c.index[i] < 0) tarjan_dfs(&c, i);
    free(c.index); free(c.low); free(c.onstack); free(c.stack);
    return c.ncomp;
}

/* ============================== ponti / articolazione ============================== */

typedef struct {
    int n;
    const int *adj;
    int *disc, *low, *parent, *art;
    int *is_bridge;
    int timer;
} bip_ctx;

static void bip_dfs(bip_ctx *c, int u)
{
    int v, children = 0;
    c->disc[u] = c->low[u] = c->timer++;
    for (v = 0; v < c->n; v++) {
        if (!c->adj[(size_t)u * (size_t)c->n + (size_t)v]) continue;
        if (c->disc[v] < 0) {
            children++;
            c->parent[v] = u;
            bip_dfs(c, v);
            if (c->low[v] < c->low[u]) c->low[u] = c->low[v];
            if (c->low[v] > c->disc[u] && c->is_bridge != NULL) {
                c->is_bridge[(size_t)u * (size_t)c->n + (size_t)v] = 1;
                c->is_bridge[(size_t)v * (size_t)c->n + (size_t)u] = 1;
            }
            if (c->parent[u] != -1 && c->low[v] >= c->disc[u]) c->art[u] = 1;
        } else if (v != c->parent[u] && c->disc[v] < c->low[u]) {
            c->low[u] = c->disc[v];
        }
    }
    if (c->parent[u] == -1 && children > 1) c->art[u] = 1;
}

static int bip_run(int n, const int *adj, int *is_bridge, int *art, int count_bridges)
{
    bip_ctx c;
    int i, res = 0;
    c.n = n; c.adj = adj;
    c.disc = (int *)malloc((size_t)n * sizeof(int));
    c.low = (int *)malloc((size_t)n * sizeof(int));
    c.parent = (int *)malloc((size_t)n * sizeof(int));
    c.art = (int *)calloc((size_t)n, sizeof(int));
    c.is_bridge = is_bridge;
    if (!c.disc || !c.low || !c.parent || !c.art) {
        free(c.disc); free(c.low); free(c.parent); free(c.art); return 0;
    }
    for (i = 0; i < n; i++) { c.disc[i] = -1; c.parent[i] = -1; }
    c.timer = 0;
    for (i = 0; i < n; i++) if (c.disc[i] < 0) bip_dfs(&c, i);
    if (art) for (i = 0; i < n; i++) art[i] = c.art[i];
    if (count_bridges) {
        res = 0;
        for (i = 0; i < n * n; i++) if (is_bridge[i]) res++;
        res /= 2;
    } else {
        for (i = 0; i < n; i++) res += c.art[i];
    }
    free(c.disc); free(c.low); free(c.parent); free(c.art);
    return res;
}

int gor_bridges(int n, const int *adj, int *is_bridge)
{
    int i;
    if (n <= 0 || adj == NULL || is_bridge == NULL) return 0;
    for (i = 0; i < n * n; i++) is_bridge[i] = 0;
    return bip_run(n, adj, is_bridge, NULL, 1);
}

int gor_articulation(int n, const int *adj, int *art)
{
    if (n <= 0 || adj == NULL || art == NULL) return 0;
    return bip_run(n, adj, NULL, art, 0);
}

/* ============================== A* ============================== */

gor_real gor_astar(int n, const gor_real *w, const gor_real *h, int s, int t, int *path, int *plen)
{
    gor_real *g;
    int *prev, *closed;
    int i;
    if (n <= 0 || w == NULL || s < 0 || t < 0 || s >= n || t >= n) return -1.0;
    g = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    prev = (int *)malloc((size_t)n * sizeof(int));
    closed = (int *)calloc((size_t)n, sizeof(int));
    if (!g || !prev || !closed) { free(g); free(prev); free(closed); return -1.0; }
    for (i = 0; i < n; i++) { g[i] = G2_INF; prev[i] = -1; }
    g[s] = 0.0;
    for (;;) {
        int bu = -1;
        gor_real bf = G2_INF;
        for (i = 0; i < n; i++) if (!closed[i] && g[i] < G2_INF) {
            gor_real f = g[i] + (h ? h[i] : 0.0);
            if (f < bf) { bf = f; bu = i; }
        }
        if (bu < 0) break;
        if (bu == t) break;
        closed[bu] = 1;
        for (i = 0; i < n; i++) {
            gor_real wuv = w[(size_t)bu * (size_t)n + (size_t)i];
            if (wuv < G2_INF && g[bu] + wuv < g[i]) { g[i] = g[bu] + wuv; prev[i] = bu; }
        }
    }
    if (g[t] >= G2_INF) { free(g); free(prev); free(closed); return -1.0; }
    if (path && plen) {
        int *rev = (int *)malloc((size_t)n * sizeof(int));
        int k = 0, x = t;
        if (rev) {
            while (x != -1) { rev[k++] = x; x = prev[x]; }
            { int j; for (j = 0; j < k; j++) path[j] = rev[k - 1 - j]; }
            *plen = k;
            free(rev);
        }
    }
    { gor_real res = g[t]; free(g); free(prev); free(closed); return res; }
}

/* ============================== betweenness (Brandes) ============================== */

gor_real gor_betweenness(int n, const gor_real *w, gor_real *bc)
{
    int s, v, wnode;
    gor_real maxv = 0.0;
    if (n <= 0 || w == NULL || bc == NULL) return 0.0;
    for (v = 0; v < n; v++) bc[v] = 0.0;
    for (s = 0; s < n; s++) {
        int *stack = (int *)malloc((size_t)n * sizeof(int));
        int *pred_count = (int *)calloc((size_t)n, sizeof(int));
        int **preds = (int **)calloc((size_t)n, sizeof(int *));
        gor_real *sigma = (gor_real *)calloc((size_t)n, sizeof(gor_real));
        gor_real *dist = (gor_real *)malloc((size_t)n * sizeof(gor_real));
        gor_real *delta = (gor_real *)calloc((size_t)n, sizeof(gor_real));
        char *done = (char *)calloc((size_t)n, 1);
        int sp = 0;
        if (!stack || !pred_count || !preds || !sigma || !dist || !delta || !done) {
            free(stack); free(pred_count); free(preds); free(sigma); free(dist); free(delta); free(done);
            break;
        }
        for (v = 0; v < n; v++) { dist[v] = G2_INF; preds[v] = (int *)malloc((size_t)n * sizeof(int)); }
        sigma[s] = 1.0; dist[s] = 0.0;
        for (;;) {
            int u = -1;
            gor_real bd = G2_INF;
            for (v = 0; v < n; v++) if (!done[v] && dist[v] < bd) { bd = dist[v]; u = v; }
            if (u < 0) break;
            done[u] = 1; stack[sp++] = u;
            for (v = 0; v < n; v++) {
                gor_real wuv = w[(size_t)u * (size_t)n + (size_t)v];
                if (wuv >= G2_INF) continue;
                if (dist[u] + wuv < dist[v] - 1e-12) {
                    dist[v] = dist[u] + wuv; sigma[v] = sigma[u]; pred_count[v] = 0; preds[v][pred_count[v]++] = u;
                } else if (fabs(dist[u] + wuv - dist[v]) < 1e-12) {
                    sigma[v] += sigma[u]; preds[v][pred_count[v]++] = u;
                }
            }
        }
        while (sp > 0) {
            int wnode2 = stack[--sp];
            int p;
            for (p = 0; p < pred_count[wnode2]; p++) {
                int u = preds[wnode2][p];
                if (sigma[wnode2] > 0) delta[u] += (sigma[u] / sigma[wnode2]) * (1.0 + delta[wnode2]);
            }
            if (wnode2 != s) bc[wnode2] += delta[wnode2];
        }
        for (v = 0; v < n; v++) free(preds[v]);
        free(stack); free(pred_count); free(preds); free(sigma); free(dist); free(delta); free(done);
    }
    for (v = 0; v < n; v++) { if (bc[v] > maxv) maxv = bc[v]; }
    (void)wnode;
    return maxv;
}

/* ============================== PageRank ============================== */

void gor_pagerank(int n, const gor_real *adj, gor_real damping, int iters, gor_real *rank)
{
    gor_real *nr;
    int i, j, it;
    if (n <= 0 || adj == NULL || rank == NULL) return;
    if (damping <= 0.0 || damping >= 1.0) damping = 0.85;
    nr = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    if (!nr) return;
    for (i = 0; i < n; i++) rank[i] = 1.0 / (gor_real)n;
    for (it = 0; it < iters; it++) {
        for (i = 0; i < n; i++) nr[i] = (1.0 - damping) / (gor_real)n;
        for (j = 0; j < n; j++) {
            gor_real out = 0.0;
            for (i = 0; i < n; i++) out += adj[(size_t)j * (size_t)n + (size_t)i];
            if (out <= 0.0) { for (i = 0; i < n; i++) nr[i] += damping * rank[j] / (gor_real)n; continue; }
            for (i = 0; i < n; i++)
                if (adj[(size_t)j * (size_t)n + (size_t)i] > 0.0)
                    nr[i] += damping * rank[j] * adj[(size_t)j * (size_t)n + (size_t)i] / out;
        }
        memcpy(rank, nr, (size_t)n * sizeof(gor_real));
    }
    free(nr);
}

/* ============================== max clique (Bron-Kerbosch) ============================== */

static void bk_expand(int n, const int *adj, int *R, int rsize, int *P, int psize,
                      int *best, int *bestsize)
{
    int i;
    if (psize == 0) {
        if (rsize > *bestsize) { *bestsize = rsize; memcpy(best, R, (size_t)rsize * sizeof(int)); }
        return;
    }
    /* pivoting */
    {
        int pivot = P[0], maxdeg = -1, u;
        for (u = 0; u < psize; u++) {
            int deg = 0, v;
            for (v = 0; v < n; v++) if (adj[(size_t)P[u] * (size_t)n + (size_t)v]) deg++;
            if (deg > maxdeg) { maxdeg = deg; pivot = P[u]; }
        }
        for (i = 0; i < psize; i++) {
            int v = P[i], u;
            if (adj[(size_t)pivot * (size_t)n + (size_t)v]) continue;
            R[rsize] = v;
            {
                int *np = (int *)malloc((size_t)psize * sizeof(int));
                int nps = 0;
                for (u = 0; u < psize; u++) if (adj[(size_t)v * (size_t)n + (size_t)P[u]]) np[nps++] = P[u];
                bk_expand(n, adj, R, rsize + 1, np, nps, best, bestsize);
                free(np);
            }
        }
    }
}

void gor_max_clique(int n, const int *adj, int *clique, int *size)
{
    int *P, *R;
    int i, bestsize = 0;
    if (n <= 0 || adj == NULL || clique == NULL || size == NULL) return;
    P = (int *)malloc((size_t)n * sizeof(int));
    R = (int *)malloc((size_t)n * sizeof(int));
    if (!P || !R) { free(P); free(R); return; }
    for (i = 0; i < n; i++) P[i] = i;
    bk_expand(n, adj, R, 0, P, n, clique, &bestsize);
    *size = bestsize;
    free(P); free(R);
}

/* ============================== DSATUR coloring ============================== */

int gor_graph_coloring(int n, const int *adj, int *color)
{
    int *satdeg;
    char *colored;
    int i, j, k, ncolors = 0;
    if (n <= 0 || adj == NULL || color == NULL) return 0;
    satdeg = (int *)calloc((size_t)n, sizeof(int));
    colored = (char *)calloc((size_t)n, 1);
    if (!satdeg || !colored) { free(satdeg); free(colored); return 0; }
    for (i = 0; i < n; i++) color[i] = -1;
    for (k = 0; k < n; k++) {
        int bu = -1, bs = -1, bd = -1;
        for (i = 0; i < n; i++) {
            if (colored[i]) continue;
            {
                int deg = 0;
                for (j = 0; j < n; j++) if (adj[(size_t)i * (size_t)n + (size_t)j]) deg++;
                if (satdeg[i] > bs || (satdeg[i] == bs && deg > bd)) { bs = satdeg[i]; bd = deg; bu = i; }
            }
        }
        if (bu < 0) break;
        {
            char *used = (char *)calloc((size_t)(n + 1), 1);
            int c;
            for (j = 0; j < n; j++)
                if (adj[(size_t)bu * (size_t)n + (size_t)j] && colored[j] && color[j] >= 0) used[color[j]] = 1;
            for (c = 0; c < n; c++) if (!used[c]) break;
            color[bu] = c;
            if (c + 1 > ncolors) ncolors = c + 1;
            colored[bu] = 1;
            for (j = 0; j < n; j++) if (adj[(size_t)bu * (size_t)n + (size_t)j] && !colored[j]) satdeg[j]++;
            free(used);
        }
    }
    free(satdeg); free(colored);
    return ncolors;
}

/* ============================== transitive closure ============================== */

void gor_transitive_closure(int n, const int *adj, int *closure)
{
    int i, j, k;
    if (n <= 0 || adj == NULL || closure == NULL) return;
    for (i = 0; i < n * n; i++) closure[i] = adj[i] ? 1 : 0;
    for (i = 0; i < n; i++) closure[(size_t)i * (size_t)n + (size_t)i] = 1;
    for (k = 0; k < n; k++)
        for (i = 0; i < n; i++)
            if (closure[(size_t)i * (size_t)n + (size_t)k])
                for (j = 0; j < n; j++)
                    if (closure[(size_t)k * (size_t)n + (size_t)j]) closure[(size_t)i * (size_t)n + (size_t)j] = 1;
}

/* ============================== Stoer-Wagner (global min-cut) ============================== */

gor_real gor_stoer_wagner(int n, const gor_real *w)
{
    double *g;
    char *merged;
    int i, j, phase;
    double best = 1e300;
    if (n <= 1 || w == NULL) return 0.0;
    g = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    merged = (char *)calloc((size_t)n, 1);
    if (!g || !merged) { free(g); free(merged); return 0.0; }
    for (i = 0; i < n * n; i++) g[i] = w[i];
    for (phase = 0; phase < n - 1; phase++) {
        char *in_a = (char *)calloc((size_t)n, 1);
        double *wgt = (double *)calloc((size_t)n, sizeof(double));
        int prev = -1, last = -1, cnt = 0, total = 0;
        if (!in_a || !wgt) { free(in_a); free(wgt); break; }
        for (i = 0; i < n; i++) if (!merged[i]) total++;
        while (cnt < total) {
            int sel = -1;
            double bw = -1.0;
            for (j = 0; j < n; j++) if (!merged[j] && !in_a[j] && wgt[j] > bw) { bw = wgt[j]; sel = j; }
            if (sel < 0) break;
            in_a[sel] = 1; cnt++;
            prev = last; last = sel;
            for (j = 0; j < n; j++) if (!merged[j] && !in_a[j]) wgt[j] += g[(size_t)sel * (size_t)n + (size_t)j];
        }
        if (last >= 0) {
            double cut = 0.0;
            for (j = 0; j < n; j++) if (!merged[j] && j != last) cut += g[(size_t)last * (size_t)n + (size_t)j];
            if (cut < best) best = cut;
            if (prev >= 0) {
                for (j = 0; j < n; j++) if (!merged[j] && j != last && j != prev) {
                    g[(size_t)prev * (size_t)n + (size_t)j] += g[(size_t)last * (size_t)n + (size_t)j];
                    g[(size_t)j * (size_t)n + (size_t)prev] = g[(size_t)prev * (size_t)n + (size_t)j];
                }
                merged[last] = 1;
            }
        }
        free(in_a); free(wgt);
    }
    free(g); free(merged);
    return best;
}

/* ============================== Yen k-shortest ============================== */

static gor_real yen_dijkstra(int n, const double *w, int s, int t, const char *removed_node,
                             const char *removed_edge, int *path, int *plen)
{
    double *dist;
    int *prev, *done;
    int i;
    if (n <= 0) return -1.0;
    dist = (double *)malloc((size_t)n * sizeof(double));
    prev = (int *)malloc((size_t)n * sizeof(int));
    done = (int *)calloc((size_t)n, sizeof(int));
    if (!dist || !prev || !done) { free(dist); free(prev); free(done); return -1.0; }
    for (i = 0; i < n; i++) { dist[i] = 1e300; prev[i] = -1; }
    dist[s] = 0.0;
    for (;;) {
        int bu = -1, j;
        double bd = 1e300;
        for (i = 0; i < n; i++) if (!done[i] && !(removed_node && removed_node[i]) && dist[i] < bd) { bd = dist[i]; bu = i; }
        if (bu < 0 || bu == t) break;
        done[bu] = 1;
        for (j = 0; j < n; j++) {
            if (removed_node && removed_node[j]) continue;
            if (removed_edge && removed_edge[(size_t)bu * (size_t)n + (size_t)j]) continue;
            if (w[(size_t)bu * (size_t)n + (size_t)j] < 1e299 && dist[bu] + w[(size_t)bu * (size_t)n + (size_t)j] < dist[j]) {
                dist[j] = dist[bu] + w[(size_t)bu * (size_t)n + (size_t)j]; prev[j] = bu;
            }
        }
    }
    if (dist[t] >= 1e299) { free(dist); free(prev); free(done); return -1.0; }
    {
        int *rev = (int *)malloc((size_t)n * sizeof(int));
        int k = 0, x = t;
        while (x != -1) { rev[k++] = x; x = prev[x]; }
        for (i = 0; i < k; i++) path[i] = rev[k - 1 - i];
        *plen = k;
        free(rev);
    }
    { double res = dist[t]; free(dist); free(prev); free(done); return res; }
}

int gor_k_shortest_yen(int n, const gor_real *w, int s, int t, int K, int *paths, int *plens)
{
    int k, cnt = 0;
    double *cand_cost;
    int **cand;
    if (n <= 0 || w == NULL || K <= 0 || paths == NULL || plens == NULL) return 0;
    cand = (int **)malloc((size_t)K * sizeof(int *));
    cand_cost = (double *)malloc((size_t)K * sizeof(double));
    if (!cand || !cand_cost) { free(cand); free(cand_cost); return 0; }
    /* A[0] = shortest path */
    {
        int *p = (int *)malloc((size_t)n * sizeof(int));
        int pl;
        if (!p) { free(cand); free(cand_cost); return 0; }
        cand_cost[0] = yen_dijkstra(n, w, s, t, NULL, NULL, p, &pl);
        if (cand_cost[0] < 0) { free(p); free(cand); free(cand_cost); return 0; }
        cand[0] = p; plens[0] = pl; cnt = 1;
        memcpy(paths, p, (size_t)pl * sizeof(int));
    }
    for (k = 1; k < K; k++) {
        int *prevp = cand[k - 1];
        int pl = plens[k - 1];
        int spur;
        double best = 1e300;
        int *bestp = NULL, bestl = 0;
        for (spur = 0; spur < pl - 1; spur++) {
            int spurNode = prevp[spur];
            char *removed_edge = (char *)calloc((size_t)n * (size_t)n, 1);
            char *removed_node = (char *)calloc((size_t)n, 1);
            int *sp = (int *)malloc((size_t)n * sizeof(int));
            int spl = 0, i;
            if (!removed_edge || !removed_node || !sp) { free(removed_edge); free(removed_node); free(sp); break; }
            /* rimuovi gli archi usati dai cammini precedenti con lo stesso spur */
            {
                int kk;
                for (kk = 0; kk < cnt; kk++) {
                    if (plens[kk] > spur) {
                        int ok = 1;
                        for (i = 0; i <= spur; i++) if (cand[kk][i] != prevp[i]) { ok = 0; break; }
                        if (ok && plens[kk] > spur + 1) removed_edge[(size_t)cand[kk][spur] * (size_t)n + (size_t)cand[kk][spur + 1]] = 1;
                    }
                }
            }
            for (i = 0; i < spur; i++) removed_node[prevp[i]] = 1;
            if (yen_dijkstra(n, w, spurNode, t, removed_node, removed_edge, sp, &spl) >= 0) {
                /* combina: prevp[0..spur] + sp[1..] */
                int *full = (int *)malloc((size_t)n * sizeof(int));
                int fl = 0;
                if (full) {
                    double c = 0.0;
                    for (i = 0; i <= spur; i++) full[fl++] = prevp[i];
                    for (i = 1; i < spl; i++) full[fl++] = sp[i];
                    for (i = 0; i + 1 < fl; i++) c += w[(size_t)full[i] * (size_t)n + (size_t)full[i + 1]];
                    if (c < best) { best = c; free(bestp); bestp = full; bestl = fl; } else free(full);
                }
            }
            free(removed_edge); free(removed_node); free(sp);
        }
        if (!bestp) break;
        cand[cnt] = bestp; plens[cnt] = bestl; cnt++;
        for (spur = 0; spur < bestl; spur++) paths[(size_t)k * (size_t)n + (size_t)spur] = bestp[spur];
    }
    { int i; for (i = 0; i < cnt; i++) if (i < K) free(cand[i]); }
    free(cand); free(cand_cost);
    return cnt;
}

/* ============================== VF2 isomorphism ============================== */

static int vf2_rec(int n1, const int *adj1, int n2, const int *adj2, int *map12, int *map21, int depth)
{
    int u, v;
    if (depth == n1) return 1;
    u = depth; /* ordine semplice: nodo 0,1,... */
    for (v = 0; v < n2; v++) {
        int ok = 1, w;
        if (map21[v] != -1) continue;
        for (w = 0; w < depth && ok; w++)
            if (adj1[(size_t)u * (size_t)n1 + (size_t)w] != adj2[(size_t)v * (size_t)n2 + (size_t)map12[w]]) ok = 0;
        if (!ok) continue;
        map12[u] = v; map21[v] = u;
        if (vf2_rec(n1, adj1, n2, adj2, map12, map21, depth + 1)) return 1;
        map12[u] = -1; map21[v] = -1;
    }
    return 0;
}

int gor_vf2_isomorphism(int n1, const int *adj1, int n2, const int *adj2, int *mapping)
{
    int *map12, *map21, i, res;
    if (n1 != n2 || adj1 == NULL || adj2 == NULL || mapping == NULL) return 0;
    map12 = (int *)malloc((size_t)n1 * sizeof(int));
    map21 = (int *)malloc((size_t)n2 * sizeof(int));
    if (!map12 || !map21) { free(map12); free(map21); return 0; }
    for (i = 0; i < n1; i++) map12[i] = -1;
    for (i = 0; i < n2; i++) map21[i] = -1;
    res = vf2_rec(n1, adj1, n2, adj2, map12, map21, 0);
    if (res) for (i = 0; i < n1; i++) mapping[i] = map12[i];
    free(map12); free(map21);
    return res;
}

/* ============================== Louvain (un livello) ============================== */

int gor_louvain(int n, const gor_real *adj, int *comm)
{
    int i, j, it, changed, ncomm;
    if (n <= 0 || adj == NULL || comm == NULL) return 0;
    for (i = 0; i < n; i++) comm[i] = i;
    for (it = 0; it < 100; it++) {
        changed = 0;
        for (i = 0; i < n; i++) {
            int best = comm[i];
            double bestw = -1.0;
            int c;
            for (c = 0; c < n; c++) {
                double wsum = 0.0;
                for (j = 0; j < n; j++) if (comm[j] == c) wsum += adj[(size_t)i * (size_t)n + (size_t)j];
                if (wsum > bestw) { bestw = wsum; best = c; }
            }
            if (best != comm[i]) { comm[i] = best; changed = 1; }
        }
        if (!changed) break;
    }
    {
        int *remap = (int *)malloc((size_t)n * sizeof(int));
        ncomm = 0;
        if (remap) {
            for (i = 0; i < n; i++) remap[i] = -1;
            for (i = 0; i < n; i++) { if (remap[comm[i]] < 0) remap[comm[i]] = ncomm++; comm[i] = remap[comm[i]]; }
            free(remap);
        }
    }
    return ncomm;
}

/* ============================== BFS direction-optimizing (Beamer, da TurboBFS) ============================== */

int gor_bfs_direction_optimizing(int n, const int *adj, int src, gor_real thr, int *dist)
{
    char *in_frontier, *in_next;
    int *frontier, *next, d = 0, visited = 0, i, j;
    int fsize;
    if (n <= 0 || adj == NULL || dist == NULL || src < 0 || src >= n) return 0;
    if (thr <= 0.0) thr = 0.001;
    in_frontier = (char *)calloc((size_t)n, 1);
    in_next = (char *)calloc((size_t)n, 1);
    frontier = (int *)malloc((size_t)n * sizeof(int));
    next = (int *)malloc((size_t)n * sizeof(int));
    if (!in_frontier || !in_next || !frontier || !next) {
        free(in_frontier); free(in_next); free(frontier); free(next); return 0;
    }
    for (i = 0; i < n; i++) dist[i] = -1;
    frontier[0] = src; fsize = 1; in_frontier[src] = 1; dist[src] = 0; visited = 1;
    while (fsize > 0) {
        int nsize = 0;
        d++;
        if ((gor_real)fsize <= thr * (gor_real)n) {
            /* top-down: push — per ogni vertice non visitato, se ha un vicino nel frontier */
            for (i = 0; i < n; i++) {
                if (dist[i] != -1) continue;
                for (j = 0; j < n; j++)
                    if (adj[(size_t)i * (size_t)n + (size_t)j] && in_frontier[j]) {
                        next[nsize++] = i; in_next[i] = 1; dist[i] = d; visited++;
                        break;
                    }
            }
        } else {
            /* bottom-up: pull — per ogni vertice non visitato, se un vicino ha dist == d-1 */
            for (i = 0; i < n; i++) {
                if (dist[i] != -1) continue;
                for (j = 0; j < n; j++)
                    if (adj[(size_t)i * (size_t)n + (size_t)j] && dist[j] == d - 1) {
                        next[nsize++] = i; in_next[i] = 1; dist[i] = d; visited++;
                        break;
                    }
            }
        }
        /* swap frontier/next */
        for (i = 0; i < fsize; i++) in_frontier[frontier[i]] = 0;
        { int *t = frontier; frontier = next; next = t; }
        { char *t = in_frontier; in_frontier = in_next; in_next = t; }
        fsize = nsize;
    }
    free(in_frontier); free(in_next); free(frontier); free(next);
    return visited;
}

/* ===================== WL labeling e DRNL (da SEAL) ===================== */

static unsigned long long wl_hash(unsigned long long x)
{
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

int gor_graph_wl(int n, const int *adj, int *colors)
{
    unsigned long long *lab, *key;
    int *idx, i, j, k, nclass = 1;
    if (n < 1 || !adj || !colors) return -1;
    lab = (unsigned long long *)calloc((size_t)n, sizeof(unsigned long long));
    key = (unsigned long long *)malloc((size_t)n * sizeof(unsigned long long));
    idx = (int *)malloc((size_t)n * sizeof(int));
    if (!lab || !key || !idx) { free(lab); free(key); free(idx); return -1; }
    for (i = 0; i < n; i++) lab[i] = 0;
    for (;;) {
        int prevclass = nclass;
        for (i = 0; i < n; i++) {
            /* vicini + label proprio, ordinati per hash del label */
            unsigned long long h = wl_hash(lab[i] + 0x9e3779b97f4a7c15ULL);
            /* raccogli i label dei vicini in un array temporaneo ordinato */
            unsigned long long nb[1024];
            int cnt = 0;
            for (j = 0; j < n; j++) if (adj[(size_t)i * n + j] && j != i) {
                if (cnt < 1024) nb[cnt++] = lab[j];
            }
            /* insertion sort dei label dei vicini */
            for (j = 1; j < cnt; j++) {
                unsigned long long key2 = nb[j]; int q = j - 1;
                while (q >= 0 && nb[q] > key2) { nb[q + 1] = nb[q]; q--; }
                nb[q + 1] = key2;
            }
            for (j = 0; j < cnt; j++) h = wl_hash(h ^ wl_hash(nb[j] + 0x100000001b3ULL));
            key[i] = h;
        }
        /* assegna classi compatte ordinando per (hash) */
        for (i = 0; i < n; i++) idx[i] = i;
        for (i = 1; i < n; i++) {
            int key3 = idx[i], q = i - 1;
            while (q >= 0 && key[idx[q]] > key[key3]) { idx[q + 1] = idx[q]; q--; }
            idx[q + 1] = key3;
        }
        nclass = 0;
        for (i = 0; i < n; i++) {
            if (i > 0 && key[idx[i]] == key[idx[i - 1]]) colors[idx[i]] = colors[idx[i - 1]];
            else colors[idx[i]] = nclass++;
            lab[idx[i]] = (unsigned long long)colors[idx[i]];
        }
        if (nclass == prevclass) break;   /* stabilizzato */
    }
    free(lab); free(key); free(idx);
    (void)k;
    return nclass;
}

static void drnl_bfs(int n, const int *adj, int src, int *dist)
{
    int *q = (int *)malloc((size_t)n * sizeof(int));
    int head = 0, tail = 0, i, v;
    if (!q) return;
    for (i = 0; i < n; i++) dist[i] = -1;
    dist[src] = 0; q[tail++] = src;
    while (head < tail) {
        v = q[head++];
        for (i = 0; i < n; i++)
            if (adj[(size_t)v * n + i] && i != v && dist[i] < 0) { dist[i] = dist[v] + 1; q[tail++] = i; }
    }
    free(q);
}

int gor_graph_drnl(int n, const int *adj, int u, int v, int *label)
{
    int *du, *dv, i, nlab = 0;
    if (n < 1 || !adj || !label) return -1;
    du = (int *)malloc((size_t)n * sizeof(int));
    dv = (int *)malloc((size_t)n * sizeof(int));
    if (!du || !dv) { free(du); free(dv); return -1; }
    drnl_bfs(n, adj, u, du);
    drnl_bfs(n, adj, v, dv);
    for (i = 0; i < n; i++) {
        int a = du[i], b = dv[i];
        if (a < 0 || b < 0) { label[i] = 0; continue; }
        if (a + b == 0) { label[i] = 1; }
        else {
            int mn = a < b ? a : b, d = a + b;
            label[i] = 1 + mn + (d / 2) * ((d / 2) + (d % 2) - 1);
        }
    }
    free(du); free(dv);
    (void)nlab;
    return 0;
}

/* ===================== Min path cover (da AirScheduling) ===================== */

static int mpc_try(int u, int n, const int *adj, int *matchR, char *vis)
{
    int v;
    for (v = 0; v < n; v++) {
        if (!adj[(size_t)u * n + v] || vis[v]) continue;
        vis[v] = 1;
        if (matchR[v] < 0 || mpc_try(matchR[v], n, adj, matchR, vis)) { matchR[v] = u; return 1; }
    }
    return 0;
}

int gor_min_path_cover(int n, const int *adj)
{
    int *matchR, i, matching = 0;
    if (n < 1 || !adj) return 0;
    matchR = (int *)malloc((size_t)n * sizeof(int));
    if (!matchR) return n;
    for (i = 0; i < n; i++) matchR[i] = -1;
    for (i = 0; i < n; i++) {
        char *vis = (char *)calloc((size_t)n, 1);
        if (vis) { if (mpc_try(i, n, adj, matchR, vis)) matching++; free(vis); }
    }
    free(matchR);
    return n - matching;
}

/* ===================== Cammini con finestre temporali (da AirlineSchedule) ===================== */

static int ptw_dfs(int n, const int *adj, const gor_real *start, const gor_real *end,
                   gor_real mc, int u, int t, char *used, int *cur, int curlen,
                   int *paths, int *path_len, int max_len, int max_paths, int *found)
{
    int v;
    if (u == t) {
        if (*found < max_paths) {
            int i, L = curlen < max_len ? curlen : max_len;
            for (i = 0; i < L; i++) paths[(size_t)(*found) * max_len + i] = cur[i];
            path_len[*found] = curlen;
            (*found)++;
        }
        return 1;
    }
    if (curlen >= max_len) return 0;
    for (v = 0; v < n; v++) {
        if (!adj[(size_t)u * n + v] || used[v]) continue;
        if (end[u] + mc > start[v]) continue;   /* connessione non ammissibile */
        used[v] = 1; cur[curlen] = v;
        ptw_dfs(n, adj, start, end, mc, v, t, used, cur, curlen + 1, paths, path_len, max_len, max_paths, found);
        used[v] = 0;
    }
    return 0;
}

int gor_paths_time_window(int n, const int *adj, const gor_real *start,
                          const gor_real *end, gor_real min_connect, int s, int t,
                          int *paths, int *path_len, int max_len, int max_paths)
{
    char *used;
    int *cur, found = 0;
    if (n < 1 || !adj || !start || !end || !paths || !path_len || max_len < 1 || max_paths < 1) return 0;
    if (s < 0 || s >= n || t < 0 || t >= n) return 0;
    used = (char *)calloc((size_t)n, 1);
    cur = (int *)malloc((size_t)max_len * sizeof(int));
    if (!used || !cur) { free(used); free(cur); return 0; }
    used[s] = 1; cur[0] = s;
    ptw_dfs(n, adj, start, end, min_connect, s, t, used, cur, 1, paths, path_len, max_len, max_paths, &found);
    free(used); free(cur);
    return found;
}

/* Closeness centrality (unweighted BFS over nonzero edges). */
void gor_closeness(int n, const gor_real *adj, gor_real *cc)
{
    int *dist, *q, i, s;
    if (n < 1 || !adj || !cc) return;
    dist = (int *)malloc((size_t)n * sizeof(int));
    q = (int *)malloc((size_t)n * sizeof(int));
    if (!dist || !q) { free(dist); free(q); return; }
    for (s = 0; s < n; s++) {
        int head = 0, tail = 0, reach = 0; double sum = 0.0;
        for (i = 0; i < n; i++) dist[i] = -1;
        dist[s] = 0; q[tail++] = s;
        while (head < tail) {
            int u = q[head++], v;
            for (v = 0; v < n; v++) {
                if (v == u) continue;
                if (adj[u*n+v] != 0.0 && dist[v] < 0) { dist[v] = dist[u]+1; reach++; sum += dist[v]; q[tail++] = v; }
            }
        }
        cc[s] = (reach > 0 && sum > 0.0) ? (double)reach / sum : 0.0;
    }
    free(dist); free(q);
}
