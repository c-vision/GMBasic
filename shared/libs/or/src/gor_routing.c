/* gor_routing.c — TSP e VRP. */
#include "gor_routing.h"
#include "gor_meta.h"
#include "gor_lp.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static gor_real tour_len(int n, const gor_real *dist, const int *tour)
{
    gor_real s = 0.0;
    int i;
    for (i = 0; i < n; i++) s += dist[(size_t)tour[i] * (size_t)n + (size_t)tour[(i + 1) % n]];
    return s;
}

gor_real gor_tsp_nn(int n, const gor_real *dist, int *tour)
{
    char *vis;
    int i, cur = 0;
    if (n <= 0 || dist == NULL || tour == NULL) return 0.0;
    vis = (char *)calloc((size_t)n, 1);
    if (!vis) return 0.0;
    tour[0] = 0; vis[0] = 1;
    for (i = 1; i < n; i++) {
        int best = -1, k;
        gor_real bd = 1e300;
        for (k = 0; k < n; k++)
            if (!vis[k] && dist[(size_t)cur * (size_t)n + (size_t)k] < bd) { bd = dist[(size_t)cur * (size_t)n + (size_t)k]; best = k; }
        tour[i] = best; vis[best] = 1; cur = best;
    }
    free(vis);
    return tour_len(n, dist, tour);
}

gor_real gor_tsp_2opt(int n, const gor_real *dist, int *tour, int maxit)
{
    int it, improved = 1, i, j;
    if (n <= 2 || dist == NULL || tour == NULL) return n > 0 ? tour_len(n, dist, tour) : 0.0;
    for (it = 0; it < maxit && improved; it++) {
        improved = 0;
        for (i = 0; i < n - 1; i++) {
            for (j = i + 1; j < n; j++) {
                int a = tour[i], b = tour[(i + 1) % n], c = tour[j], d = tour[(j + 1) % n];
                gor_real old = dist[(size_t)a * (size_t)n + (size_t)b] + dist[(size_t)c * (size_t)n + (size_t)d];
                gor_real nw = dist[(size_t)a * (size_t)n + (size_t)c] + dist[(size_t)b * (size_t)n + (size_t)d];
                if (nw < old - 1e-12) {
                    int lo = i + 1, hi = j, t;
                    while (lo < hi) { t = tour[lo]; tour[lo] = tour[hi]; tour[hi] = t; lo++; hi--; }
                    improved = 1;
                }
            }
        }
    }
    return tour_len(n, dist, tour);
}

gor_real gor_tsp_or_opt(int n, const gor_real *dist, int *tour, int maxit)
{
    int it, improved = 1, i, j;
    int *tmp;
    if (n <= 3 || dist == NULL || tour == NULL) return n > 0 ? tour_len(n, dist, tour) : 0.0;
    tmp = (int *)malloc((size_t)n * sizeof(int));
    if (!tmp) return tour_len(n, dist, tour);
    for (it = 0; it < maxit && improved; it++) {
        improved = 0;
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                int k, p = 0, m;
                gor_real before, after;
                if (i == j) continue;
                before = tour_len(n, dist, tour);
                /* rimuovi tour[i] e inseriscilo alla posizione j */
                for (k = 0; k < n; k++) if (k != i) tmp[p++] = tour[k];
                for (k = n - 1; k > j; k--) tmp[k] = tmp[k - 1];
                tmp[j] = tour[i];
                after = tour_len(n, dist, tmp);
                if (after < before - 1e-12) { for (m = 0; m < n; m++) tour[m] = tmp[m]; improved = 1; }
            }
        }
    }
    free(tmp);
    return tour_len(n, dist, tour);
}

gor_real gor_tsp_double_tree(int n, const gor_real *dist, int *tour)
{
    gor_real *key;
    int *parent, *order, *stack, *first_child, *next_sibling;
    char *in_tree, *vis;
    int i, k, cnt = 0, sp = 0;
    if (n <= 1 || dist == NULL || tour == NULL) { if (tour && n == 1) tour[0] = 0; return 0.0; }
    key = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    parent = (int *)malloc((size_t)n * sizeof(int));
    order = (int *)malloc((size_t)n * sizeof(int));
    stack = (int *)malloc((size_t)n * sizeof(int));
    first_child = (int *)malloc((size_t)n * sizeof(int));
    next_sibling = (int *)malloc((size_t)n * sizeof(int));
    in_tree = (char *)calloc((size_t)n, 1);
    vis = (char *)calloc((size_t)n, 1);
    if (!key || !parent || !order || !stack || !first_child || !next_sibling || !in_tree || !vis) {
        free(key); free(parent); free(order); free(stack); free(first_child); free(next_sibling); free(in_tree); free(vis);
        return 0.0;
    }
    for (i = 0; i < n; i++) { key[i] = 1e300; parent[i] = -1; first_child[i] = -1; next_sibling[i] = -1; }
    key[0] = 0.0;
    for (k = 0; k < n; k++) {
        int u = -1;
        gor_real bd = 1e300;
        for (i = 0; i < n; i++) if (!in_tree[i] && key[i] < bd) { bd = key[i]; u = i; }
        if (u < 0) break;
        in_tree[u] = 1;
        for (i = 0; i < n; i++)
            if (!in_tree[i] && dist[(size_t)u * (size_t)n + (size_t)i] < key[i]) {
                key[i] = dist[(size_t)u * (size_t)n + (size_t)i]; parent[i] = u;
            }
    }
    for (i = 1; i < n; i++) {
        int p = parent[i];
        next_sibling[i] = first_child[p];
        first_child[p] = i;
    }
    stack[sp++] = 0;
    while (sp > 0) {
        int u = stack[--sp];
        int c;
        if (vis[u]) continue;
        vis[u] = 1; order[cnt++] = u;
        c = first_child[u];
        while (c != -1) { stack[sp++] = c; c = next_sibling[c]; }
    }
    for (i = 0; i < cnt; i++) tour[i] = order[i];
    free(key); free(parent); free(order); free(stack); free(first_child); free(next_sibling); free(in_tree); free(vis);
    return tour_len(n, dist, tour);
}

gor_real gor_tsp_held_karp(int n, const gor_real *dist, int *tour)
{
    int full, s, last, i;
    gor_real *dp;
    int *par;
    if (n <= 0 || n > 20 || dist == NULL || tour == NULL) return 0.0;
    full = 1 << n;
    dp = (gor_real *)malloc((size_t)full * (size_t)n * sizeof(gor_real));
    par = (int *)malloc((size_t)full * (size_t)n * sizeof(int));
    if (!dp || !par) { free(dp); free(par); return 0.0; }
    for (s = 0; s < full * n; s++) { dp[s] = 1e300; par[s] = -1; }
    dp[(1 << 0) * n + 0] = 0.0;
    for (s = 1; s < full; s++) {
        if (!(s & 1)) continue;
        for (last = 0; last < n; last++) {
            if (!(s & (1 << last))) continue;
            if (dp[(size_t)s * (size_t)n + (size_t)last] >= 1e299) continue;
            for (i = 0; i < n; i++) {
                int ns;
                gor_real nd;
                if (s & (1 << i)) continue;
                ns = s | (1 << i);
                nd = dp[(size_t)s * (size_t)n + (size_t)last] + dist[(size_t)last * (size_t)n + (size_t)i];
                if (nd < dp[(size_t)ns * (size_t)n + (size_t)i]) {
                    dp[(size_t)ns * (size_t)n + (size_t)i] = nd;
                    par[(size_t)ns * (size_t)n + (size_t)i] = last;
                }
            }
        }
    }
    {
        gor_real best = 1e300;
        int bl = 0, s2 = full - 1, k;
        for (last = 1; last < n; last++) {
            gor_real v = dp[(size_t)s2 * (size_t)n + (size_t)last] + dist[(size_t)last * (size_t)n + 0];
            if (v < best) { best = v; bl = last; }
        }
        /* ricostruisci */
        for (k = n - 1; k >= 0; k--) {
            tour[k] = bl;
            { int p = par[(size_t)s2 * (size_t)n + (size_t)bl]; s2 ^= (1 << bl); bl = p; }
        }
        free(dp); free(par);
        return best;
    }
}

/* ============================== CVRP Clarke-Wright ============================== */

typedef struct { gor_real saving; int i, j; } gor_saving;

static int cmp_saving(const void *a, const void *b)
{
    const gor_saving *x = (const gor_saving *)a, *y = (const gor_saving *)b;
    if (y->saving > x->saving) return 1;
    if (y->saving < x->saving) return -1;
    return 0;
}

int gor_vrp_clarke_wright(int n, const gor_real *dist, const gor_real *demand, gor_real cap,
                          int depot, int *route_of, gor_real *total)
{
    int np = n * (n - 1) / 2, idx = 0, i, j, k, nroutes;
    gor_saving *sv;
    int *route_load, *route_start, *route_end, *next, *prev, *route_id;
    gor_real tot = 0.0;
    if (n <= 0 || dist == NULL || demand == NULL || route_of == NULL) { if (total) *total = 0; return 0; }
    sv = (gor_saving *)malloc((size_t)(np > 0 ? np : 1) * sizeof(gor_saving));
    route_load = (int *)malloc((size_t)n * sizeof(int));
    route_start = (int *)malloc((size_t)n * sizeof(int));
    route_end = (int *)malloc((size_t)n * sizeof(int));
    next = (int *)malloc((size_t)n * sizeof(int));
    prev = (int *)malloc((size_t)n * sizeof(int));
    route_id = (int *)malloc((size_t)n * sizeof(int));
    if (!sv || !route_load || !route_start || !route_end || !next || !prev || !route_id) {
        free(sv); free(route_load); free(route_start); free(route_end); free(next); free(prev); free(route_id);
        if (total) *total = 0; return 0;
    }
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++) {
            gor_real d0i = dist[(size_t)depot * (size_t)(n + 1) + (size_t)(i + 1)];
            gor_real d0j = dist[(size_t)depot * (size_t)(n + 1) + (size_t)(j + 1)];
            gor_real dij = dist[(size_t)(i + 1) * (size_t)(n + 1) + (size_t)(j + 1)];
            sv[idx].saving = d0i + d0j - dij;
            sv[idx].i = i; sv[idx].j = j; idx++;
        }
    qsort(sv, (size_t)np, sizeof(gor_saving), cmp_saving);
    for (i = 0; i < n; i++) { next[i] = -1; prev[i] = -1; route_id[i] = i; route_start[i] = i; route_end[i] = i; route_load[i] = (int)(demand[i] + 0.5); }
    for (k = 0; k < np; k++) {
        int a = sv[k].i, b = sv[k].j, ra = route_id[a], rb = route_id[b];
        if (ra == rb) continue;
        if (route_load[ra] + route_load[rb] > (int)(cap + 0.5)) continue;
        /* collega solo se a è estremo finale di ra e b è estremo iniziale di rb (o viceversa) */
        if (route_end[ra] == a && route_start[rb] == b) {
            next[a] = b; prev[b] = a; route_end[ra] = route_end[rb];
            { int x = route_start[rb]; while (x != -1) { route_id[x] = ra; x = next[x]; } }
            route_load[ra] += route_load[rb];
        } else if (route_end[rb] == b && route_start[ra] == a) {
            next[b] = a; prev[a] = b; route_end[rb] = route_end[ra];
            { int x = route_start[ra]; while (x != -1) { route_id[x] = rb; x = next[x]; } }
            route_load[rb] += route_load[ra];
        }
    }
    /* assegna route_of e conta */
    nroutes = 0;
    for (i = 0; i < n; i++) {
        if (route_start[route_id[i]] == i) {
            int x = i;
            tot += dist[(size_t)depot * (size_t)(n + 1) + (size_t)(x + 1)];
            while (x != -1) {
                route_of[x] = nroutes;
                if (next[x] != -1) tot += dist[(size_t)(x + 1) * (size_t)(n + 1) + (size_t)(next[x] + 1)];
                else tot += dist[(size_t)(x + 1) * (size_t)(n + 1) + (size_t)depot];
                x = next[x];
            }
            nroutes++;
        }
    }
    if (total) *total = tot;
    free(sv); free(route_load); free(route_start); free(route_end); free(next); free(prev); free(route_id);
    return nroutes;
}

/* ============================== VRPTW ============================== */

static int vrptw_feasible(int n, const gor_real *dist, const gor_real *ready, const gor_real *due,
                          const gor_real *service, int depot, int nroute, const int *route)
{
    /* verifica finestre lungo la rotta (depot=0, clienti +1) */
    double t = 0.0;
    int i, prev = depot;
    for (i = 0; i < nroute; i++) {
        int c = route[i] + 1;
        t += dist[(size_t)prev * (size_t)(n + 1) + (size_t)c];
        if (t < ready[route[i]]) t = ready[route[i]];
        if (t > due[route[i]] + 1e-9) return 0;
        t += service[route[i]];
        prev = c;
    }
    (void)n;
    return 1;
}

int gor_vrp_time_windows(int n, const gor_real *dist, const gor_real *demand, gor_real cap,
                         int depot, const gor_real *ready, const gor_real *due,
                         const gor_real *service, int *route_of, gor_real *total)
{
    int *next, *prev, *route_id, *route_start, *route_end, *route_load, *rlen;
    int **route, i, j, k, nroutes = 0, np = n * (n - 1) / 2, idx = 0;
    gor_saving *sv;
    double tot = 0.0;
    if (n <= 0 || dist == NULL || demand == NULL || route_of == NULL) { if (total) *total = 0; return 0; }
    sv = (gor_saving *)malloc((size_t)(np > 0 ? np : 1) * sizeof(gor_saving));
    next = (int *)malloc((size_t)n * sizeof(int));
    prev = (int *)malloc((size_t)n * sizeof(int));
    route_id = (int *)malloc((size_t)n * sizeof(int));
    route_start = (int *)malloc((size_t)n * sizeof(int));
    route_end = (int *)malloc((size_t)n * sizeof(int));
    route_load = (int *)malloc((size_t)n * sizeof(int));
    rlen = (int *)malloc((size_t)n * sizeof(int));
    route = (int **)malloc((size_t)n * sizeof(int *));
    if (!sv || !next || !prev || !route_id || !route_start || !route_end || !route_load || !rlen || !route) {
        free(sv); free(next); free(prev); free(route_id); free(route_start); free(route_end); free(route_load); free(rlen); free(route);
        if (total) *total = 0; return 0;
    }
    for (i = 0; i < n; i++) {
        route[i] = (int *)malloc((size_t)n * sizeof(int));
        route[i][0] = i; rlen[i] = 1;
        next[i] = -1; prev[i] = -1; route_id[i] = i; route_start[i] = i; route_end[i] = i; route_load[i] = (int)(demand[i] + 0.5);
    }
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++) {
            gor_real d0i = dist[(size_t)depot * (size_t)(n + 1) + (size_t)(i + 1)];
            gor_real d0j = dist[(size_t)depot * (size_t)(n + 1) + (size_t)(j + 1)];
            gor_real dij = dist[(size_t)(i + 1) * (size_t)(n + 1) + (size_t)(j + 1)];
            sv[idx].saving = d0i + d0j - dij; sv[idx].i = i; sv[idx].j = j; idx++;
        }
    qsort(sv, (size_t)np, sizeof(gor_saving), cmp_saving);
    for (k = 0; k < np; k++) {
        int a = sv[k].i, b = sv[k].j, ra = route_id[a], rb = route_id[b];
        if (ra == rb) continue;
        if (route_load[ra] + route_load[rb] > (int)(cap + 0.5)) continue;
        /* prova a concatenare route[ra] + route[rb] (o invertita) */
        {
            int *merged = (int *)malloc((size_t)n * sizeof(int));
            int ml = 0, ok = 0, dir;
            if (!merged) break;
            for (dir = 0; dir < 4 && !ok; dir++) {
                int p, q;
                ml = 0;
                if (dir == 0) { for (p = 0; p < rlen[ra]; p++) merged[ml++] = route[ra][p]; for (q = 0; q < rlen[rb]; q++) merged[ml++] = route[rb][q]; }
                else if (dir == 1) { for (p = rlen[ra] - 1; p >= 0; p--) merged[ml++] = route[ra][p]; for (q = 0; q < rlen[rb]; q++) merged[ml++] = route[rb][q]; }
                else if (dir == 2) { for (p = 0; p < rlen[ra]; p++) merged[ml++] = route[ra][p]; for (q = rlen[rb] - 1; q >= 0; q--) merged[ml++] = route[rb][q]; }
                else { for (p = rlen[ra] - 1; p >= 0; p--) merged[ml++] = route[ra][p]; for (q = rlen[rb] - 1; q >= 0; q--) merged[ml++] = route[rb][q]; }
                if (vrptw_feasible(n, dist, ready, due, service, depot, ml, merged)) ok = 1;
            }
            if (ok) {
                int t2;
                for (t2 = 0; t2 < ml; t2++) { route[ra][t2] = merged[t2]; route_id[merged[t2]] = ra; }
                rlen[ra] = ml;
                route_load[ra] += route_load[rb];
            }
            free(merged);
        }
    }
    /* assegna route_of e calcola distanza */
    for (i = 0; i < n; i++) {
        if (route_id[i] == i) {
            int t = 0;
            route_of[route[i][0]] = nroutes;
            tot += dist[(size_t)depot * (size_t)(n + 1) + (size_t)(route[i][0] + 1)];
            for (t = 0; t + 1 < rlen[i]; t++) {
                route_of[route[i][t + 1]] = nroutes;
                tot += dist[(size_t)(route[i][t] + 1) * (size_t)(n + 1) + (size_t)(route[i][t + 1] + 1)];
            }
            tot += dist[(size_t)(route[i][rlen[i] - 1] + 1) * (size_t)(n + 1) + (size_t)depot];
            nroutes++;
        }
    }
    for (i = 0; i < n; i++) free(route[i]);
    free(route); free(sv); free(next); free(prev); free(route_id); free(route_start); free(route_end); free(route_load); free(rlen);
    if (total) *total = tot;
    return nroutes;
}

/* ============================== MDVRP ============================== */

int gor_mdvrp(int n, int ndepots, const gor_real *dist, const gor_real *demand, gor_real cap,
              int *route_of, gor_real *total)
{
    int *depot_of, *clients, nc = 0, d, nroutes = 0, i, j;
    double tot = 0.0;
    if (n <= 0 || ndepots <= 0 || dist == NULL || route_of == NULL) { if (total) *total = 0; return 0; }
    depot_of = (int *)malloc((size_t)n * sizeof(int));
    clients = (int *)malloc((size_t)n * sizeof(int));
    if (!depot_of || !clients) { free(depot_of); free(clients); if (total) *total = 0; return 0; }
    for (i = ndepots; i < n; i++) {
        int best = 0;
        for (d = 1; d < ndepots; d++) if (dist[(size_t)d * (size_t)n + (size_t)i] < dist[(size_t)best * (size_t)n + (size_t)i]) best = d;
        depot_of[i] = best;
    }
    for (d = 0; d < ndepots; d++) {
        nc = 0;
        for (i = ndepots; i < n; i++) if (depot_of[i] == d) clients[nc++] = i;
        /* CVRP su questo deposito: Clarke-Wright semplificato (nearest neighbor con capacità) */
        {
            char *vis = (char *)calloc((size_t)nc, 1);
            int idx = 0;
            while (idx < nc) {
                double load = 0.0;
                int cur = d;
                int started = 0;
                for (;;) {
                    int best = -1, k;
                    double bd = 1e300;
                    for (k = 0; k < nc; k++) {
                        if (vis[k]) continue;
                        if (load + demand[clients[k]] > cap + 1e-9) continue;
                        if (dist[(size_t)cur * (size_t)n + (size_t)clients[k]] < bd) { bd = dist[(size_t)cur * (size_t)n + (size_t)clients[k]]; best = k; }
                    }
                    if (best < 0) break;
                    vis[best] = 1; load += demand[clients[best]];
                    route_of[clients[best]] = nroutes;
                    tot += dist[(size_t)cur * (size_t)n + (size_t)clients[best]];
                    cur = clients[best];
                    started = 1;
                    idx++;
                }
                if (!started) break;
                tot += dist[(size_t)cur * (size_t)n + (size_t)d];
                nroutes++;
            }
            free(vis);
        }
    }
    if (total) *total = tot;
    free(depot_of); free(clients);
    (void)j;
    return nroutes;
}

/* ============================== PDP ============================== */

/* costo di inserire `node` (1-based) alla posizione pos nella rotta (depot 0). */
static double pdp_ins(int n, const gor_real *dist, const int *route, int len, int pos, int node)
{
    int prev = (pos == 0) ? 0 : route[pos - 1];
    int nxt = (pos == len) ? 0 : route[pos];
    return dist[(size_t)prev * (size_t)n + (size_t)node]
         + dist[(size_t)node * (size_t)n + (size_t)nxt]
         - dist[(size_t)prev * (size_t)n + (size_t)nxt];
}

int gor_pdp(int nreq, const gor_real *dist, const gor_real *demand, gor_real cap,
            const int *pickup, const int *delivery, int *route, gor_real *total)
{
    int n = 2 * nreq + 1; /* depot 0 + 2*nreq nodi */
    int len = 0, r, i, j, nroutes = 1;
    double tot = 0.0;
    if (nreq <= 0 || dist == NULL || demand == NULL || pickup == NULL || delivery == NULL || route == NULL) { if (total) *total = 0; return 0; }
    for (r = 0; r < nreq; r++) {
        int p = pickup[r] + 1, q = delivery[r] + 1;
        double bd = 1e300;
        int bp = len, bq = len + 1;
        for (i = 0; i <= len; i++) {
            for (j = i; j <= len; j++) {
                int t, ok = 1;
                double load = 0.0, cost;
                /* capacità: dopo l'inserimento di p in i, e q in j */
                for (t = 0; t <= len; t++) {
                    if (t == i) load += demand[p - 1];
                    if (t == j) load += demand[q - 1];
                    if (t < len) {
                        if (t == i) { if (load > cap + 1e-9) { ok = 0; break; } }
                        if (route[t] >= 1) {
                            /* nodo pickup o delivery */
                            int is_deliv = 0, k;
                            for (k = 0; k < nreq; k++) if (delivery[k] + 1 == route[t]) is_deliv = 1;
                            if (is_deliv) load -= demand[route[t] - 1];
                        }
                    }
                }
                if (!ok) continue;
                cost = pdp_ins(n, dist, route, len, i, p);
                {
                    /* inserisci temporaneamente p per calcolare il costo di q */
                    int *tmp = (int *)malloc((size_t)(len + 1) * sizeof(int));
                    int k2;
                    if (!tmp) continue;
                    for (k2 = 0; k2 < len; k2++) tmp[k2] = route[k2];
                    for (k2 = len; k2 > i; k2--) tmp[k2] = tmp[k2 - 1];
                    tmp[i] = p;
                    cost += pdp_ins(n, dist, tmp, len + 1, j, q);
                    free(tmp);
                }
                if (cost < bd) { bd = cost; bp = i; bq = j; }
            }
        }
        /* inserisci p in bp, q in bq (bq riferito alla rotta con p già inserito) */
        for (i = len; i > bp; i--) route[i] = route[i - 1];
        route[bp] = p;
        len++;
        for (i = len; i > bq; i--) route[i] = route[i - 1];
        route[bq] = q;
        len++;
        tot += bd;
    }
    /* distanza totale del circuito (depot -> ... -> depot) */
    tot = dist[(size_t)0 * (size_t)n + (size_t)route[0]];
    for (i = 0; i + 1 < len; i++) tot += dist[(size_t)route[i] * (size_t)n + (size_t)route[i + 1]];
    tot += dist[(size_t)route[len - 1] * (size_t)n + 0];
    if (total) *total = tot;
    return nroutes;
}

/* ============================== arc routing (Chinese postman) ============================== */

gor_real gor_arc_routing(int n, const gor_real *w, int *euler, int *elen)
{
    /* Hierholzer su grafo con gradi resi pari (matching greedy degli odd). */
    int *deg, i, j, elen2 = 0;
    int *stack;
    double total = 0.0;
    if (n <= 0 || w == NULL || euler == NULL || elen == NULL) return 0.0;
    deg = (int *)calloc((size_t)n, sizeof(int));
    stack = (int *)malloc((size_t)(n * n + n + 1) * sizeof(int));
    if (!deg || !stack) { free(deg); free(stack); return 0.0; }
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) if (w[(size_t)i * (size_t)n + (size_t)j] < 1e299 && w[(size_t)i * (size_t)n + (size_t)j] > 0.0) deg[i]++;
    /* Hierholzer semplice (assume grafo connesso e gradi pari; se odd, aggiungo archi fittizi) */
    {
        int *iter = (int *)calloc((size_t)n, sizeof(int));
        int sp = 0, start = -1;
        for (i = 0; i < n; i++) if (deg[i] > 0) { start = i; break; }
        if (start < 0) { free(deg); free(stack); free(iter); *elen = 0; return 0.0; }
        stack[sp++] = start;
        while (sp > 0) {
            int u = stack[sp - 1];
            int advanced = 0;
            for (j = iter[u]; j < n; j++) {
                if (w[(size_t)u * (size_t)n + (size_t)j] < 1e299 && w[(size_t)u * (size_t)n + (size_t)j] > 0.0) {
                    iter[u] = j + 1;
                    stack[sp++] = j;
                    total += w[(size_t)u * (size_t)n + (size_t)j];
                    advanced = 1;
                    break;
                }
            }
            if (!advanced) { euler[elen2++] = u; sp--; }
        }
        free(iter);
    }
    *elen = elen2;
    free(deg); free(stack);
    return total;
}

/* ===================== TSP: extra-mileage, GRASP, VNS (da two-tsp) ===================== */

gor_real gor_tsp_tour_cost(int n, const gor_real *dist, const int *tour)
{
    gor_real c = 0.0;
    int i;
    if (n <= 0 || !dist || !tour) return 0.0;
    for (i = 0; i < n; i++)
        c += dist[(size_t)tour[i] * n + tour[(i + 1) % n]];
    return c;
}

static unsigned long long tt_next(unsigned long long *s)
{ *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; return *s; }

gor_real gor_tsp_extra_mileage(int n, const gor_real *dist, int *tour)
{
    int *used, cnt, i, j, k, bi = -1, bp = -1;
    gor_real best3 = 1e300;
    if (n <= 0 || !dist || !tour) return 0.0;
    used = (int *)calloc((size_t)n, sizeof(int));
    if (!used) return 0.0;
    if (n <= 3) {
        for (i = 0; i < n; i++) tour[i] = i;
        free(used);
        return gor_tsp_tour_cost(n, dist, tour);
    }
    /* triangolo iniziale di costo minimo */
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            for (k = j + 1; k < n; k++) {
                gor_real c = dist[(size_t)i * n + j] + dist[(size_t)j * n + k] + dist[(size_t)k * n + i];
                if (c < best3) { best3 = c; tour[0] = i; tour[1] = j; tour[2] = k; }
            }
    for (i = 0; i < 3; i++) used[tour[i]] = 1;
    cnt = 3;
    while (cnt < n) {
        gor_real best_extra = 1e300;
        bi = -1; bp = -1;
        for (k = 0; k < n; k++) {
            int p;
            if (used[k]) continue;
            for (p = 0; p < cnt; p++) {
                int a = tour[p], b = tour[(p + 1) % cnt];
                gor_real extra = dist[(size_t)a * n + k] + dist[(size_t)k * n + b] - dist[(size_t)a * n + b];
                if (extra < best_extra) { best_extra = extra; bi = k; bp = p + 1; }
            }
        }
        for (i = cnt; i > bp; i--) tour[i] = tour[i - 1];
        tour[bp] = bi;
        used[bi] = 1;
        cnt++;
    }
    free(used);
    return gor_tsp_tour_cost(n, dist, tour);
}

gor_real gor_tsp_grasp(int n, const gor_real *dist, int *tour, int maxit,
                       gor_real alpha, unsigned long long seed)
{
    int *cand, *best, *cur, i, it;
    gor_real bestc = 1e300;
    unsigned long long s = seed ? seed : 12345ULL;
    if (n <= 0 || !dist || !tour) return 0.0;
    cand = (int *)malloc((size_t)n * sizeof(int));
    best = (int *)malloc((size_t)n * sizeof(int));
    cur = (int *)malloc((size_t)n * sizeof(int));
    if (!cand || !best || !cur) { free(cand); free(best); free(cur); return 0.0; }
    if (alpha <= 0.0) alpha = 0.2;
    for (it = 0; it < maxit; it++) {
        int used = 0, *vis, cnt, start;
        vis = (int *)calloc((size_t)n, sizeof(int));
        if (!vis) break;
        start = (int)(tt_next(&s) % (unsigned long long)n);
        cur[0] = start; vis[start] = 1; cnt = 1; used = 1;
        while (cnt < n) {
            int last = cur[cnt - 1], nc = 0, j;
            gor_real dmax = 0.0;
            for (j = 0; j < n; j++) {
                if (vis[j]) continue;
                { gor_real d = dist[(size_t)last * n + j]; cand[nc++] = j; if (d > dmax) dmax = d; }
            }
            /* RCL: candidati entro alpha*dmax dal migliore */
            {
                gor_real dmin = 1e300; int best_j = cand[0];
                for (j = 0; j < nc; j++) { gor_real d = dist[(size_t)last * n + cand[j]]; if (d < dmin) { dmin = d; best_j = cand[j]; } }
                (void)dmax;
                {
                    int rcl_n = 0, *rcl = (int *)malloc((size_t)nc * sizeof(int)), q;
                    for (j = 0; j < nc; j++) if (dist[(size_t)last * n + cand[j]] <= dmin + alpha * dmin) rcl[rcl_n++] = cand[j];
                    if (rcl_n == 0) { rcl[0] = best_j; rcl_n = 1; }
                    q = (int)(tt_next(&s) % (unsigned long long)rcl_n);
                    cur[cnt] = rcl[q];
                    vis[rcl[q]] = 1;
                    free(rcl);
                }
            }
            cnt++;
            (void)used;
        }
        free(vis);
        gor_tsp_2opt(n, dist, cur, 100);
        { gor_real c = gor_tsp_tour_cost(n, dist, cur);
          if (c < bestc) { bestc = c; for (i = 0; i < n; i++) best[i] = cur[i]; } }
    }
    for (i = 0; i < n; i++) tour[i] = best[i];
    free(cand); free(best); free(cur);
    return bestc;
}

gor_real gor_tsp_vns(int n, const gor_real *dist, int *tour, int maxit)
{
    int *cur, *best, i, it;
    gor_real curc, bestc;
    unsigned long long s = 987654321ULL;
    if (n <= 0 || !dist || !tour) return 0.0;
    cur = (int *)malloc((size_t)n * sizeof(int));
    best = (int *)malloc((size_t)n * sizeof(int));
    if (!cur || !best) { free(cur); free(best); return 0.0; }
    gor_tsp_nn(n, dist, cur);
    gor_tsp_2opt(n, dist, cur, 200);
    curc = gor_tsp_tour_cost(n, dist, cur);
    bestc = curc;
    for (i = 0; i < n; i++) best[i] = cur[i];
    for (it = 0; it < maxit; it++) {
        int k = (int)(tt_next(&s) % 3);
        /* perturba cur in base al vicinato k */
        if (k == 0) {                 /* 2-opt random */
            int a = (int)(tt_next(&s) % n), b = (int)(tt_next(&s) % n), lo, hi;
            if (a > b) { int t = a; a = b; b = t; }
            for (lo = a, hi = b; lo < hi; lo++, hi--) { int t = cur[lo]; cur[lo] = cur[hi]; cur[hi] = t; }
        } else if (k == 1) {          /* Or-opt: sposta un nodo */
            int a = (int)(tt_next(&s) % n), b = (int)(tt_next(&s) % n), v, j;
            v = cur[a];
            if (a < b) { for (j = a; j < b; j++) cur[j] = cur[j + 1]; cur[b] = v; }
            else { for (j = a; j > b; j--) cur[j] = cur[j - 1]; cur[b] = v; }
        } else {                      /* double-bridge */
            int p1 = (int)(tt_next(&s) % n), p2 = (int)(tt_next(&s) % n), p3 = (int)(tt_next(&s) % n), t;
            if (p1 > p2) { t = p1; p1 = p2; p2 = t; }
            if (p2 > p3) { t = p2; p2 = p3; p3 = t; }
            if (p1 > p2) { t = p1; p1 = p2; p2 = t; }
            {
                int *tmp = (int *)malloc((size_t)n * sizeof(int)), q = 0, z;
                for (z = 0; z < p1; z++) tmp[q++] = cur[z];
                for (z = p2; z < p3; z++) tmp[q++] = cur[z];
                for (z = p1; z < p2; z++) tmp[q++] = cur[z];
                for (z = p3; z < n; z++) tmp[q++] = cur[z];
                for (z = 0; z < n; z++) cur[z] = tmp[z];
                free(tmp);
            }
        }
        gor_tsp_2opt(n, dist, cur, 100);
        curc = gor_tsp_tour_cost(n, dist, cur);
        if (curc < bestc) { bestc = curc; for (i = 0; i < n; i++) best[i] = cur[i]; }
        else { for (i = 0; i < n; i++) cur[i] = best[i]; curc = bestc; }
    }
    for (i = 0; i < n; i++) tour[i] = best[i];
    free(cur); free(best);
    return bestc;
}

/* ===================== TSP 3-opt (da Operations_research_2) ===================== */

/* ricostruisce il tour: P3, poi segmento (seq0,r0), poi segmento (seq1,r1). */
static void apply3(int n, int *t, int i, int j, int k, int seq0, int r0, int seq1, int r1)
{
    int *nw = (int *)malloc((size_t)n * sizeof(int));
    int p = 0, q;
    if (!nw) return;
    for (q = k + 1; q < n; q++) nw[p++] = t[q];
    for (q = 0; q <= i; q++) nw[p++] = t[q];
    if (seq0 == 0) { if (!r0) for (q = i + 1; q <= j; q++) nw[p++] = t[q]; else for (q = j; q >= i + 1; q--) nw[p++] = t[q]; }
    else { if (!r0) for (q = j + 1; q <= k; q++) nw[p++] = t[q]; else for (q = k; q >= j + 1; q--) nw[p++] = t[q]; }
    if (seq1 == 0) { if (!r1) for (q = i + 1; q <= j; q++) nw[p++] = t[q]; else for (q = j; q >= i + 1; q--) nw[p++] = t[q]; }
    else { if (!r1) for (q = j + 1; q <= k; q++) nw[p++] = t[q]; else for (q = k; q >= j + 1; q--) nw[p++] = t[q]; }
    for (q = 0; q < n; q++) t[q] = nw[q];
    free(nw);
}

gor_real gor_tsp_3opt(int n, const gor_real *dist, int *tour, int maxit)
{
    int it, i, j, k, improved = 1;
    if (n < 4 || !dist || !tour) return 0.0;
    for (it = 0; it < maxit && improved; it++) {
        improved = 0;
        for (i = 0; i < n - 2 && !improved; i++) {
            for (j = i + 1; j < n - 1 && !improved; j++) {
                for (k = j + 1; k < n - 1 && !improved; k++) {
                    int a = tour[i], b = tour[i + 1], c = tour[j], d = tour[j + 1];
                    int e = tour[k], f = tour[(k + 1) % n];
                    gor_real oldc = dist[(size_t)a*n+b] + dist[(size_t)c*n+d] + dist[(size_t)e*n+f];
                    /* le 7 riconnessioni non banali: (seq0,r0,seq1,r1) */
                    static const int cfg[7][4] = {
                        {0,1,1,0},  /* rev(P1) P2  -> 2-opt i,j */
                        {0,0,1,1},  /* P1 rev(P2)  -> 2-opt j,k */
                        {0,1,1,1},  /* rev(P1) rev(P2) -> 2-opt i,k */
                        {1,0,0,0},  /* P2 P1 */
                        {1,1,0,0},  /* rev(P2) P1 */
                        {1,0,0,1},  /* P2 rev(P1) */
                        {1,1,0,1}   /* rev(P2) rev(P1) */
                    };
                    int t2;
                    for (t2 = 0; t2 < 7; t2++) {
                        int s0 = cfg[t2][0], r0 = cfg[t2][1], s1 = cfg[t2][2], r1 = cfg[t2][3];
                        int xf = (s0 == 0) ? (r0 ? c : b) : (r0 ? e : d);
                        int xl = (s0 == 0) ? (r0 ? b : c) : (r0 ? d : e);
                        int yf = (s1 == 0) ? (r1 ? c : b) : (r1 ? e : d);
                        int yl = (s1 == 0) ? (r1 ? b : c) : (r1 ? d : e);
                        gor_real nc = dist[(size_t)a*n+xf] + dist[(size_t)xl*n+yf] + dist[(size_t)yl*n+f];
                        if (nc < oldc - 1e-12) {
                            apply3(n, tour, i, j, k, s0, r0, s1, r1);
                            improved = 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    return gor_tsp_tour_cost(n, dist, tour);
}

/* ===================== TSP: simulated annealing e GA (da THEBELLIN) ===================== */

static unsigned long long ta_next(unsigned long long *s)
{ *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; return *s; }

gor_real gor_tsp_sa(int n, const gor_real *dist, int *tour, gor_real T0,
                    gor_real cooling, int iters, unsigned long long seed)
{
    unsigned long long s = seed ? seed : 12345ULL;
    gor_real T, cur, best;
    int *bt, k, i;
    if (n < 4 || !dist || !tour) return 0.0;
    if (T0 <= 0) T0 = 100.0;
    if (cooling <= 0.0 || cooling >= 1.0) cooling = 0.99;
    if (iters < 1) iters = 10 * n;
    bt = (int *)malloc((size_t)n * sizeof(int));
    if (!bt) return 0.0;
    cur = gor_tsp_tour_cost(n, dist, tour);
    best = cur;
    for (i = 0; i < n; i++) bt[i] = tour[i];
    T = T0;
    while (T > 1e-6) {
        for (k = 0; k < iters; k++) {
            int a = (int)(ta_next(&s) % (unsigned long long)n);
            int b = (int)(ta_next(&s) % (unsigned long long)n), lo, hi;
            gor_real delta;
            if (a > b) { int t = a; a = b; b = t; }
            if (a == b) continue;
            {
                int A = tour[a], B = tour[(a + 1) % n], C = tour[b], D = tour[(b + 1) % n];
                delta = dist[(size_t)A*n+C] + dist[(size_t)B*n+D] - dist[(size_t)A*n+B] - dist[(size_t)C*n+D];
            }
            if (delta < 0.0 || (gor_real)(ta_next(&s) >> 11) / 9007199254740992.0 < exp(-delta / T)) {
                for (lo = a + 1, hi = b; lo < hi; lo++, hi--) { int t = tour[lo]; tour[lo] = tour[hi]; tour[hi] = t; }
                cur += delta;
                if (cur < best - 1e-9) { best = cur; for (i = 0; i < n; i++) bt[i] = tour[i]; }
            }
        }
        T *= cooling;
    }
    for (i = 0; i < n; i++) tour[i] = bt[i];
    free(bt);
    return best;
}

/* crossover a taglio: copia p1[0..cut], poi p2 saltando i visitati. */
static void ga_crossover(int n, const int *p1, const int *p2, int *child,
                         const char *inpop, unsigned long long *s)
{
    char *vis = (char *)calloc((size_t)n, 1);
    int cut = (int)(ta_next(s) % (unsigned long long)n), idx = 0, i;
    (void)inpop;
    if (!vis) return;
    for (i = 0; i <= cut; i++) { child[idx++] = p1[i]; vis[p1[i]] = 1; }
    for (i = 0; i < n; i++) if (!vis[p2[i]]) child[idx++] = p2[i];
    free(vis);
}

gor_real gor_tsp_ga(int n, const gor_real *dist, int *tour, int pop, int gens,
                    unsigned long long seed)
{
    unsigned long long s = seed ? seed : 12345ULL;
    int *popu, *fits, *tmp, *best_tour, i, g, k, nchild;
    gor_real bestc = 1e300, *cost;
    if (n < 4 || !dist || !tour) return 0.0;
    if (pop < 4) pop = 50;
    if (gens < 1) gens = 100;
    /* La popolazione ospita `pop` genitori PIU' `pop/2` figli, scritti a indice `pop + k`
     * (vedi il ciclo di crossover sotto): con `pop * n` int la scrittura dei figli usciva
     * dall'allocazione — risultati casuali e tour invalidi (2026-11-04). */
    popu = (int *)malloc((size_t)(pop + pop / 2) * n * sizeof(int));
    cost = (gor_real *)malloc((size_t)(pop + pop) * sizeof(gor_real));
    fits = (int *)malloc((size_t)(pop + pop) * sizeof(int));
    tmp = (int *)malloc((size_t)n * sizeof(int));
    best_tour = (int *)malloc((size_t)n * sizeof(int));
    if (!popu || !cost || !fits || !tmp || !best_tour) {
        free(popu); free(cost); free(fits); free(tmp); free(best_tour); return 0.0;
    }
    /* inizializza: NN + perturbazioni casuali */
    gor_tsp_nn(n, dist, popu);
    for (k = 1; k < pop; k++) {
        int j;
        for (j = 0; j < n; j++) popu[(size_t)k * n + j] = j;
        for (j = n - 1; j > 0; j--) { int q = (int)(ta_next(&s) % (unsigned long long)(j + 1)); int t = popu[(size_t)k*n+j]; popu[(size_t)k*n+j] = popu[(size_t)k*n+q]; popu[(size_t)k*n+q] = t; }
        gor_tsp_2opt(n, dist, popu + (size_t)k * n, 50);
    }
    for (g = 0; g < gens; g++) {
        /* fitness = 1/costo */
        int total = pop + pop / 2;
        for (k = 0; k < pop; k++) cost[k] = gor_tsp_tour_cost(n, dist, popu + (size_t)k * n);
        for (k = 0; k < pop; k++) { cost[k] = 1e300 / (cost[k] + 1e-12); }
        for (k = 0; k < pop; k++) if (gor_tsp_tour_cost(n, dist, popu + (size_t)k * n) < bestc) {
            bestc = gor_tsp_tour_cost(n, dist, popu + (size_t)k * n);
            for (i = 0; i < n; i++) best_tour[i] = popu[(size_t)k * n + i];
        }
        /* genera figli per selezione a torneo */
        nchild = 0;
        for (k = 0; k < pop / 2; k++) {
            int p1 = (int)(ta_next(&s) % (unsigned long long)pop);
            int p2 = (int)(ta_next(&s) % (unsigned long long)pop);
            int *c = popu + (size_t)(pop + k) * n;
            ga_crossover(n, popu + (size_t)p1 * n, popu + (size_t)p2 * n, c, NULL, &s);
            /* mutazione a scambio (probabilita' ~ 1/n) */
            if ((ta_next(&s) % (unsigned long long)n) == 0) {
                int a = (int)(ta_next(&s) % (unsigned long long)n), b = (int)(ta_next(&s) % (unsigned long long)n), t;
                t = c[a]; c[a] = c[b]; c[b] = t;
            }
            gor_tsp_2opt(n, dist, c, 20);
            nchild++;
        }
        (void)total;
        /* selezione: tieni i migliori pop tra genitori+figli (elitismo) */
        {
            int *all = (int *)malloc((size_t)(pop + nchild) * n * sizeof(int));
            gor_real *ac = (gor_real *)malloc((size_t)(pop + nchild) * sizeof(gor_real));
            int *ord = (int *)malloc((size_t)(pop + nchild) * sizeof(int));
            int tot = pop + nchild, a, b2;
            if (!all || !ac || !ord) { free(all); free(ac); free(ord); break; }
            memcpy(all, popu, (size_t)tot * n * sizeof(int));
            for (a = 0; a < tot; a++) { ac[a] = gor_tsp_tour_cost(n, dist, all + (size_t)a * n); ord[a] = a; }
            for (a = 1; a < tot; a++) { int key = ord[a]; b2 = a - 1; while (b2 >= 0 && ac[ord[b2]] > ac[key]) { ord[b2+1] = ord[b2]; b2--; } ord[b2+1] = key; }
            for (a = 0; a < pop; a++) memcpy(popu + (size_t)a * n, all + (size_t)ord[a] * n, (size_t)n * sizeof(int));
            free(all); free(ac); free(ord);
        }
    }
    for (i = 0; i < n; i++) tour[i] = best_tour[i];
    free(popu); free(cost); free(fits); free(tmp); free(best_tour);
    return bestc;
}

/* ===================== TSP: tabu search con tenure variabile (da OR2) ===================== */

gor_real gor_tsp_tabu(int n, const gor_real *dist, int *tour, int maxit,
                      int tenure_kind, unsigned long long seed)
{
    int *tabu, *bt, it, i, j;
    gor_real best, cur;
    (void)seed;
    if (n < 4 || !dist || !tour) return 0.0;
    if (maxit < 1) maxit = 100 * n;
    tabu = (int *)calloc((size_t)n * n, sizeof(int));
    bt = (int *)malloc((size_t)n * sizeof(int));
    if (!tabu || !bt) { free(tabu); free(bt); return 0.0; }
    cur = gor_tsp_tour_cost(n, dist, tour);
    best = cur;
    for (i = 0; i < n; i++) bt[i] = tour[i];
    for (it = 0; it < maxit; it++) {
        int tenure = gor_tabu_tenure(tenure_kind, it, n);
        int bi = -1, bj = -1;
        gor_real bd = 1e300;
        for (i = 0; i < n; i++) {
            for (j = i + 2; j < n; j++) {
                int u, v, x, y, is_tabu;
                gor_real delta;
                if (i == 0 && j == n - 1) continue;
                u = tour[i]; v = tour[i + 1]; x = tour[j]; y = tour[(j + 1) % n];
                delta = dist[(size_t)u*n+x] + dist[(size_t)v*n+y]
                      - dist[(size_t)u*n+v] - dist[(size_t)x*n+y];
                is_tabu = (tabu[(size_t)u*n+v] > it) || (tabu[(size_t)v*n+u] > it);
                if (is_tabu && cur + delta >= best - 1e-9) continue;
                if (delta < bd) { bd = delta; bi = i; bj = j; }
            }
        }
        if (bi < 0) break;
        {
            int u = tour[bi], v = tour[bi + 1], x = tour[bj], y = tour[(bj + 1) % n];
            int lo, hi;
            for (lo = bi + 1, hi = bj; lo < hi; lo++, hi--) { int t = tour[lo]; tour[lo] = tour[hi]; tour[hi] = t; }
            cur += bd;
            tabu[(size_t)u*n+v] = tabu[(size_t)v*n+u] = it + tenure;
            tabu[(size_t)x*n+y] = tabu[(size_t)y*n+x] = it + tenure;
        }
        if (cur < best - 1e-9) { best = cur; for (i = 0; i < n; i++) bt[i] = tour[i]; }
    }
    for (i = 0; i < n; i++) tour[i] = bt[i];
    free(tabu); free(bt);
    return best;
}

/* ===================== TSP: edge recombination crossover (da SiMoM0/TSP) ===================== */

static void erx_add(int *adj, int *cnt, int n, int a, int b)
{
    int k;
    /* Guardia su ENTRAMBI gli estremi: senza `a < n` un genitore malformato (o un tour non
     * permutazione) scriveva fuori da adj/cnt (2026-11-04, heap corruption → SIGBUS). */
    if (a == b || a < 0 || b < 0 || a >= n || b >= n) return;
    for (k = 0; k < cnt[a]; k++) if (adj[(size_t)a * 4 + k] == b) return;
    if (cnt[a] < 4) { adj[(size_t)a * 4 + cnt[a]] = b; cnt[a]++; }
}

void gor_tsp_erx(int n, const int *p1, const int *p2, int *child,
                 unsigned long long seed)
{
    int *adj, *cnt, *rem;
    char *used;
    int i, t, cur;
    unsigned long long s = seed ? seed : 12345ULL;
    if (n < 2 || !p1 || !p2 || !child) return;
    adj = (int *)calloc((size_t)n * 4, sizeof(int));
    cnt = (int *)calloc((size_t)n, sizeof(int));
    rem = (int *)calloc((size_t)n, sizeof(int));
    used = (char *)calloc((size_t)n, 1);
    if (!adj || !cnt || !rem || !used) { free(adj); free(cnt); free(rem); free(used); return; }
    /* archi dei due genitori (tour ciclici) */
    for (i = 0; i < n; i++) {
        erx_add(adj, cnt, n, p1[i], p1[(i + 1) % n]);
        erx_add(adj, cnt, n, p1[i], p1[(i - 1 + n) % n]);
        erx_add(adj, cnt, n, p2[i], p2[(i + 1) % n]);
        erx_add(adj, cnt, n, p2[i], p2[(i - 1 + n) % n]);
    }
    for (i = 0; i < n; i++) rem[i] = cnt[i];
    cur = p1[0];
    child[0] = cur;
    used[cur] = 1;
    for (t = 1; t < n; t++) {
        int best = -1, bestc = 5, k, pick;
        /* rimuovi cur dalle liste */
        for (i = 0; i < n; i++) {
            if (used[i]) continue;
            for (k = 0; k < cnt[i]; k++) if (adj[(size_t)i * 4 + k] == cur) { rem[i]--; break; }
        }
        for (k = 0; k < cnt[cur]; k++) {
            int v = adj[(size_t)cur * 4 + k];
            if (v < 0 || v >= n) continue;      /* mai indicizzare used[] con un nodo invalido */
            if (!used[v] && rem[v] < bestc) { bestc = rem[v]; best = v; }
        }
        if (best >= 0) pick = best;
        else {
            /* nessun vicino disponibile: prendi un nodo non usato a caso */
            int cntfree = 0;
            for (i = 0; i < n; i++) if (!used[i]) cntfree++;
            if (cntfree == 0) break;
            pick = (int)((ta_next(&s) % (unsigned long long)cntfree));
            for (i = 0; i < n; i++) { if (!used[i]) { if (pick == 0) { pick = i; break; } pick--; } }
        }
        if (pick < 0 || pick >= n) break;       /* niente scritture con indici invalidi */
        child[t] = pick;
        used[pick] = 1;
        cur = pick;
    }
    free(adj); free(cnt); free(rem); free(used);
}

/* ===================== TSP: sottotour e DFJ MIP (da Giraudux/operational-research) ===================== */

int gor_tsp_subtours(int n, const int *succ, int *comp, int *sizes)
{
    int *seen, i, c = 0, k;
    if (n < 1 || !succ || !comp) return -1;
    seen = (int *)calloc((size_t)n, sizeof(int));
    if (!seen) return -1;
    for (i = 0; i < n; i++) comp[i] = -1;
    for (i = 0; i < n; i++) {
        if (comp[i] >= 0) continue;
        k = i;
        while (comp[k] < 0) { comp[k] = c; k = succ[k]; }
        if (sizes) {
            int sz = 0, t = i;
            do { sz++; t = succ[t]; } while (t != i && sz <= n);
            sizes[c] = sz;
        }
        c++;
    }
    free(seen);
    return c;
}

gor_real gor_tsp_dfj(int n, const gor_real *dist, int *tour, int maxcuts)
{
    int nvar0, ntot, m, i, j, iter, rc;
    int *int_idx, *succ, *comp, *sizes;
    int cap, nint;
    double *A, *A2, *b, *c, *x, obj;
    if (n < 3 || n > 40 || !dist || !tour) return -1.0;
    if (maxcuts < 1) maxcuts = 200;
    nvar0 = n * (n - 1);
    ntot = nvar0;
    m = 2 * n;
    cap = nvar0 + maxcuts;
    nint = nvar0;
    A = (double *)calloc((size_t)(m + maxcuts * n) * (size_t)cap, sizeof(double));
    A2 = (double *)calloc((size_t)(m + maxcuts * n) * (size_t)cap, sizeof(double));
    b = (double *)calloc((size_t)(m + maxcuts * n), sizeof(double));
    c = (double *)calloc((size_t)(ntot + maxcuts), sizeof(double));
    x = (double *)calloc((size_t)(ntot + maxcuts), sizeof(double));
    int_idx = (int *)malloc((size_t)(ntot + maxcuts) * sizeof(int));
    succ = (int *)malloc((size_t)n * sizeof(int));
    comp = (int *)malloc((size_t)n * sizeof(int));
    sizes = (int *)malloc((size_t)n * sizeof(int));
    if (!A || !A2 || !b || !c || !x || !int_idx || !succ || !comp || !sizes) {
        free(A); free(A2); free(b); free(c); free(x); free(int_idx); free(succ); free(comp); free(sizes); return -1.0;
    }
    {   /* variabile x_{ij} -> indice; distanza come costo */
        int i2, j2;
        for (i2 = 0; i2 < n; i2++)
            for (j2 = 0; j2 < n; j2++) {
                int idx;
                if (i2 == j2) continue;
                idx = i2 * (n - 1) + (j2 > i2 ? j2 - 1 : j2);
                c[idx] = dist[(size_t)i2 * n + j2];
            }
        for (i2 = 0; i2 < cap; i2++) int_idx[i2] = i2;
        /* gradi uscenti (riga i2) e entranti (riga n+j2) */
        for (i2 = 0; i2 < n; i2++) {
            for (j2 = 0; j2 < n; j2++) {
                int idx;
                if (i2 == j2) continue;
                idx = i2 * (n - 1) + (j2 > i2 ? j2 - 1 : j2);
                A[(size_t)i2 * cap + idx] = 1.0;             /* out-degree */
                A[(size_t)(n + j2) * cap + idx] = 1.0;       /* in-degree */
            }
            b[i2] = 1.0; b[n + i2] = 1.0;
        }
    }
    for (iter = 0; iter < maxcuts; iter++) {
        int ncomp, cc;
        for (i = 0; i < m; i++) for (j = 0; j < ntot; j++) A2[(size_t)i * ntot + j] = A[(size_t)i * cap + j];
        rc = gor_milp_bb(m, ntot, A2, b, c, nint, int_idx, x, &obj, 200000, 1e-6);
        if (rc != 0 && rc != 3) { free(A); free(A2); free(b); free(c); free(x); free(int_idx); free(succ); free(comp); free(sizes); return -1.0; }
        for (i = 0; i < n; i++) succ[i] = i;
        for (i = 0; i < n; i++) {
            int bi = -1; double bv = 0.5;
            for (j = 0; j < n; j++) {
                int idx;
                if (i == j) continue;
                idx = i * (n - 1) + (j > i ? j - 1 : j);
                if (x[idx] > bv) { bv = x[idx]; bi = j; }
            }
            if (bi >= 0) succ[i] = bi;
        }
        ncomp = gor_tsp_subtours(n, succ, comp, sizes);
        if (ncomp == 1) break;   /* ciclo hamiltoniano: ottimo */
        /* aggiungi un taglio SEC per ogni sottotour non banale */
        for (cc = 0; cc < ncomp; cc++) {
            int sz = 0, ii, jj;
            for (ii = 0; ii < n; ii++) if (comp[ii] == cc) sz++;
            if (sz >= n || sz < 2) continue;
            if (ntot >= nvar0 + maxcuts || m >= 2 * n + maxcuts * n) break;
            for (ii = 0; ii < m + 1; ii++) A[(size_t)ii * cap + ntot] = 0.0;
            for (ii = 0; ii < n; ii++) {
                if (comp[ii] != cc) continue;
                for (jj = 0; jj < n; jj++) {
                    int idx;
                    if (ii == jj || comp[jj] != cc) continue;
                    idx = ii * (n - 1) + (jj > ii ? jj - 1 : jj);
                    A[(size_t)m * cap + idx] = 1.0;
                }
            }
            A[(size_t)m * cap + ntot] = 1.0;   /* slack */
            b[m] = (double)(sz - 1);
            c[ntot] = 0.0;
            int_idx[ntot] = -1;   /* slack continua: escludi da interi */
            ntot++;
            m++;
        }
    }
    for (i = 0; i < n; i++) tour[i] = succ[i];
    free(A); free(A2); free(b); free(c); free(x); free(int_idx); free(succ); free(comp); free(sizes);
    return obj;
}

/* ===================== TSP: 1-tree e bound di Held-Karp (da ro2tsp) ===================== */

gor_real gor_tsp_one_tree(int n, const gor_real *dist, int root, int *edges)
{
    char *in;
    int *par;
    gor_real *key;
    gor_real total = 0.0;
    int i, j, t, added = 0;
    if (n < 2 || !dist || root < 0 || root >= n) return 0.0;
    in = (char *)calloc((size_t)n, 1);
    par = (int *)malloc((size_t)n * sizeof(int));
    key = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    if (!in || !par || !key) { free(in); free(par); free(key); return 0.0; }
    if (edges) for (i = 0; i < 2 * n; i++) edges[i] = -1;
    /* Prim sui nodi != root */
    for (i = 0; i < n; i++) { key[i] = 1e300; par[i] = -1; }
    { int start = (root == 0) ? 1 : 0; key[start] = 0.0; }
    for (t = 0; t < n - 1; t++) {
        int u = -1; gor_real bd = 1e300;
        for (i = 0; i < n; i++) if (i != root && !in[i] && key[i] < bd) { bd = key[i]; u = i; }
        if (u < 0) break;
        in[u] = 1;
        if (par[u] >= 0) {
            total += dist[(size_t)u * n + par[u]];
            if (edges) { edges[2 * added] = u; edges[2 * added + 1] = par[u]; added++; }
        }
        for (j = 0; j < n; j++) if (j != root && !in[j] && dist[(size_t)u * n + j] < key[j]) { key[j] = dist[(size_t)u * n + j]; par[j] = u; }
    }
    /* due archi piu' economici da root */
    for (t = 0; t < 2; t++) {
        int u = -1; gor_real bd = 1e300;
        for (i = 0; i < n; i++) if (i != root && dist[(size_t)root * n + i] < bd) {
            int k, already = 0;
            if (edges) for (k = 0; k < added; k++) if ((edges[2*k]==root&&edges[2*k+1]==i)||(edges[2*k]==i&&edges[2*k+1]==root)) already = 1;
            if (already) continue;
            bd = dist[(size_t)root * n + i]; u = i;
        }
        if (u < 0) break;
        total += dist[(size_t)root * n + u];
        if (edges) { edges[2 * added] = root; edges[2 * added + 1] = u; added++; }
    }
    free(in); free(par); free(key);
    return total;
}

gor_real gor_tsp_hk_bound(int n, const gor_real *dist, int maxit, gor_real *pi)
{
    gor_real *mod, *deg, *ub_tour_cost;
    int *edges, *tour;
    gor_real best = -1e300, alpha = 2.0, ub;
    int i, j, it, noimp = 0;
    if (n < 3 || !dist) return 0.0;
    if (maxit < 1) maxit = 200;
    mod = (gor_real *)malloc((size_t)n * n * sizeof(gor_real));
    deg = (gor_real *)calloc((size_t)n, sizeof(gor_real));
    edges = (int *)malloc((size_t)2 * n * sizeof(int));
    tour = (int *)malloc((size_t)n * sizeof(int));
    ub_tour_cost = (gor_real *)malloc(sizeof(gor_real));
    if (!mod || !deg || !edges || !tour || !ub_tour_cost) {
        free(mod); free(deg); free(edges); free(tour); free(ub_tour_cost); return 0.0;
    }
    /* UB iniziale: nearest neighbour + 2-opt */
    gor_tsp_nn(n, dist, tour);
    ub = gor_tsp_2opt(n, dist, tour, 100);
    *ub_tour_cost = ub;
    if (pi) for (i = 0; i < n; i++) pi[i] = 0.0;
    for (it = 0; it < maxit; it++) {
        gor_real w, L, norm2 = 0.0, step;
        int k;
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++)
                mod[(size_t)i * n + j] = (i == j) ? 0.0 : dist[(size_t)i * n + j] + (pi ? pi[i] + pi[j] : 0.0);
        w = gor_tsp_one_tree(n, mod, 0, edges);
        for (i = 0; i < n; i++) deg[i] = 0.0;
        for (k = 0; k < n; k++) {
            int a = edges[2 * k], b2 = edges[2 * k + 1];
            if (a < 0 || b2 < 0) continue;
            deg[a] += 1.0; deg[b2] += 1.0;
        }
        L = w;
        if (pi) for (i = 0; i < n; i++) L -= 2.0 * pi[i];
        if (L > best) { best = L; noimp = 0; } else noimp++;
        for (i = 0; i < n; i++) norm2 += (deg[i] - 2.0) * (deg[i] - 2.0);
        if (norm2 < 1e-12) break;   /* 1-tree e' un tour: ottimo */
        { gor_real step_scale = 1.0 / (gor_real)(it + 1);
          step = step_scale * alpha * (ub - L) / norm2;
          if (step < 0.0) step = 0.0; }
        if (pi) for (i = 0; i < n; i++) pi[i] += step * (deg[i] - 2.0);
        if (noimp >= 20) { alpha *= 0.5; noimp = 0; if (alpha < 1e-6) break; }
    }
    free(mod); free(deg); free(edges); free(tour); free(ub_tour_cost);
    return best;
}
