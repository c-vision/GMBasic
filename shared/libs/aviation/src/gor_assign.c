/* gor_assign.c — assegnazione, matching, trasporto. */
#include "gor_assign.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GOR_INF 1e300

/* ============================== Hungarian (Kuhn-Munkres) ============================== */

gor_real gor_assign_hungarian(int n, int m, const gor_real *cost, int *assignment)
{
    int i, j, j0, j1, i0;
    gor_real *u, *v, *minv;
    int *p, *way, *used;
    gor_real total = 0.0;
    if (cost == NULL || assignment == NULL || n <= 0 || m <= 0 || n > m) return 0.0;
    u = (gor_real *)calloc((size_t)(n + 1), sizeof(gor_real));
    v = (gor_real *)calloc((size_t)(m + 1), sizeof(gor_real));
    p = (int *)calloc((size_t)(m + 1), sizeof(int));
    way = (int *)calloc((size_t)(m + 1), sizeof(int));
    minv = (gor_real *)malloc((size_t)(m + 1) * sizeof(gor_real));
    used = (int *)malloc((size_t)(m + 1) * sizeof(int));
    if (!u || !v || !p || !way || !minv || !used) {
        free(u); free(v); free(p); free(way); free(minv); free(used);
        return 0.0;
    }
    for (i = 1; i <= n; i++) {
        p[0] = i; j0 = 0;
        for (j = 0; j <= m; j++) { minv[j] = GOR_INF; used[j] = 0; }
        do {
            gor_real delta = GOR_INF;
            used[j0] = 1; i0 = p[j0]; j1 = -1;
            for (j = 1; j <= m; j++) if (!used[j]) {
                gor_real cur = cost[(size_t)(i0 - 1) * (size_t)m + (size_t)(j - 1)] - u[i0] - v[j];
                if (cur < minv[j]) { minv[j] = cur; way[j] = j0; }
                if (minv[j] < delta) { delta = minv[j]; j1 = j; }
            }
            for (j = 0; j <= m; j++) {
                if (used[j]) { u[p[j]] += delta; v[j] -= delta; }
                else minv[j] -= delta;
            }
            j0 = j1;
        } while (p[j0] != 0);
        do { j1 = way[j0]; p[j0] = p[j1]; j0 = j1; } while (j0);
    }
    for (i = 0; i < n; i++) assignment[i] = -1;
    for (j = 1; j <= m; j++) if (p[j]) assignment[p[j] - 1] = j - 1;
    for (i = 0; i < n; i++) if (assignment[i] >= 0) total += cost[(size_t)i * (size_t)m + (size_t)assignment[i]];
    free(u); free(v); free(p); free(way); free(minv); free(used);
    return total;
}

/* ============================== Auction ============================== */

gor_real gor_assign_auction(int n, int m, const gor_real *cost, int *assignment, gor_real eps)
{
    gor_real *price;
    int *owner;
    int *unassigned, nu, it, i, j;
    gor_real total = 0.0;
    if (cost == NULL || assignment == NULL || n <= 0 || m <= 0 || n > m) return 0.0;
    if (eps <= 0.0) eps = 1e-6;
    price = (gor_real *)calloc((size_t)m, sizeof(gor_real));
    owner = (int *)malloc((size_t)m * sizeof(int));
    unassigned = (int *)malloc((size_t)n * sizeof(int));
    if (!price || !owner || !unassigned) { free(price); free(owner); free(unassigned); return 0.0; }
    for (j = 0; j < m; j++) owner[j] = -1;
    nu = n;
    for (i = 0; i < n; i++) unassigned[i] = i;
    for (it = 0; it < 100000 && nu > 0; it++) {
        int idx = it % nu;
        int ii = unassigned[idx];
        gor_real best = -GOR_INF, second = -GOR_INF;
        int bj = -1;
        for (j = 0; j < m; j++) {
            gor_real val = -cost[(size_t)ii * (size_t)m + (size_t)j] - price[j];
            if (val > best) { second = best; best = val; bj = j; }
            else if (val > second) second = val;
        }
        if (second == -GOR_INF) second = best;
        price[bj] += best - second + eps;
        if (owner[bj] != -1) {
            unassigned[idx] = owner[bj];
            owner[bj] = ii;
        } else {
            owner[bj] = ii;
            unassigned[idx] = unassigned[nu - 1];
            nu--;
        }
    }
    for (i = 0; i < n; i++) assignment[i] = -1;
    for (j = 0; j < m; j++) if (owner[j] >= 0) assignment[owner[j]] = j;
    for (i = 0; i < n; i++) if (assignment[i] >= 0) total += cost[(size_t)i * (size_t)m + (size_t)assignment[i]];
    free(price); free(owner); free(unassigned);
    return total;
}

/* ============================== Bottleneck ============================== */

/* Kuhn ricorsivo su grafo con soglia. */
static int kuhn_try(int u, int m, const gor_real *cost, gor_real thr,
                    int *matchR, int *seen)
{
    int v;
    for (v = 0; v < m; v++) {
        if (cost[(size_t)u * (size_t)m + (size_t)v] <= thr && !seen[v]) {
            seen[v] = 1;
            if (matchR[v] == -1 || kuhn_try(matchR[v], m, cost, thr, matchR, seen)) {
                matchR[v] = u;
                return 1;
            }
        }
    }
    return 0;
}

gor_real gor_assign_bottleneck(int n, const gor_real *cost, int *assignment)
{
    gor_real lo, hi, mid, best = GOR_INF;
    gor_real *sorted;
    int i, j, k, found = 0;
    int *matchR;
    if (cost == NULL || assignment == NULL || n <= 0) return 0.0;
    sorted = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    matchR = (int *)malloc((size_t)n * sizeof(int));
    if (!sorted || !matchR) { free(sorted); free(matchR); return 0.0; }
    for (i = 0; i < n * n; i++) sorted[i] = cost[i];
    /* sort */
    for (i = 0; i < n * n; i++)
        for (j = i + 1; j < n * n; j++)
            if (sorted[j] < sorted[i]) { gor_real t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
    lo = 0.0; hi = (gor_real)(n * n - 1);
    while (lo <= hi) {
        int feasible = 1;
        mid = floor((lo + hi) / 2.0);
        for (j = 0; j < n; j++) matchR[j] = -1;
        for (i = 0; i < n && feasible; i++) {
            int *seen = (int *)calloc((size_t)n, sizeof(int));
            if (!seen || !kuhn_try(i, n, cost, sorted[(int)mid], matchR, seen)) feasible = 0;
            free(seen);
        }
        if (feasible) { best = sorted[(int)mid]; hi = mid - 1; found = 1; }
        else lo = mid + 1;
    }
    if (found) {
        for (i = 0; i < n; i++) assignment[i] = -1;
        for (j = 0; j < n; j++) if (matchR[j] >= 0) assignment[matchR[j]] = j;
        /* ricalcola matchR al valore best */
        for (j = 0; j < n; j++) matchR[j] = -1;
        for (i = 0; i < n; i++) {
            int *seen = (int *)calloc((size_t)n, sizeof(int));
            if (seen) { kuhn_try(i, n, cost, best, matchR, seen); free(seen); }
        }
        for (i = 0; i < n; i++) assignment[i] = -1;
        for (j = 0; j < n; j++) if (matchR[j] >= 0) assignment[matchR[j]] = j;
    }
    free(sorted); free(matchR);
    (void)k;
    return best;
}

/* ============================== Gale-Shapley ============================== */

gor_status gor_stable_marriage(int n, const int *pref_men, const int *pref_women, int *match_men)
{
    int *next, *match_women, *rank_women;
    int i, w, m;
    if (n <= 0 || pref_men == NULL || pref_women == NULL || match_men == NULL) return GOR_ERR_ARG;
    next = (int *)calloc((size_t)n, sizeof(int));
    match_women = (int *)malloc((size_t)n * sizeof(int));
    rank_women = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    if (!next || !match_women || !rank_women) { free(next); free(match_women); free(rank_women); return GOR_ERR_NOMEM; }
    for (i = 0; i < n; i++) match_men[i] = -1;
    for (w = 0; w < n; w++) match_women[w] = -1;
    for (w = 0; w < n; w++)
        for (i = 0; i < n; i++) rank_women[(size_t)w * (size_t)n + (size_t)pref_women[(size_t)w * (size_t)n + (size_t)i]] = i;
    /* coda di uomini liberi */
    for (i = 0; i < n; i++) {
        m = i;
        while (m != -1 && next[m] < n) {
            w = pref_men[(size_t)m * (size_t)n + (size_t)next[m]];
            next[m]++;
            if (match_women[w] == -1) {
                match_women[w] = m; match_men[m] = w; m = -1;
            } else if (rank_women[(size_t)w * (size_t)n + (size_t)m] < rank_women[(size_t)w * (size_t)n + (size_t)match_women[w]]) {
                int old = match_women[w];
                match_men[old] = -1;
                match_women[w] = m; match_men[m] = w; m = old;
            }
        }
    }
    free(next); free(match_women); free(rank_women);
    return GOR_OK;
}

/* ============================== Trasporto: VAM ============================== */

gor_real gor_transport_vam(int m, int n, const gor_real *supply, const gor_real *demand,
                           const gor_real *cost, gor_real *x)
{
    gor_real *s, *d;
    int *row_done, *col_done;
    int i, j, it;
    gor_real total = 0.0;
    if (m <= 0 || n <= 0 || supply == NULL || demand == NULL || cost == NULL || x == NULL) return 0.0;
    s = (gor_real *)malloc((size_t)m * sizeof(gor_real));
    d = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    row_done = (int *)calloc((size_t)m, sizeof(int));
    col_done = (int *)calloc((size_t)n, sizeof(int));
    if (!s || !d || !row_done || !col_done) { free(s); free(d); free(row_done); free(col_done); return 0.0; }
    for (i = 0; i < m; i++) s[i] = supply[i];
    for (j = 0; j < n; j++) d[j] = demand[j];
    for (i = 0; i < m * n; i++) x[i] = 0.0;
    for (it = 0; it < m * n + m + n; it++) {
        int br = -1, bc = -1, is_row = 1;
        gor_real best_pen = -1.0;
        /* penalità righe */
        for (i = 0; i < m; i++) {
            if (row_done[i] || s[i] <= 1e-12) continue;
            {
                gor_real c1 = GOR_INF, c2 = GOR_INF;
                int j1 = -1;
                for (j = 0; j < n; j++) {
                    if (col_done[j]) continue;
                    gor_real c = cost[(size_t)i * (size_t)n + (size_t)j];
                    if (c < c1) { c2 = c1; c1 = c; j1 = j; }
                    else if (c < c2) c2 = c;
                }
                if (j1 >= 0 && c2 - c1 > best_pen) { best_pen = c2 - c1; br = i; bc = j1; is_row = 1; }
            }
        }
        /* penalità colonne */
        for (j = 0; j < n; j++) {
            if (col_done[j] || d[j] <= 1e-12) continue;
            {
                gor_real c1 = GOR_INF, c2 = GOR_INF;
                int i1 = -1;
                for (i = 0; i < m; i++) {
                    if (row_done[i]) continue;
                    gor_real c = cost[(size_t)i * (size_t)n + (size_t)j];
                    if (c < c1) { c2 = c1; c1 = c; i1 = i; }
                    else if (c < c2) c2 = c;
                }
                if (i1 >= 0 && c2 - c1 > best_pen) { best_pen = c2 - c1; br = i1; bc = j; is_row = 0; }
            }
        }
        if (br < 0) break;
        {
            gor_real q = (s[br] < d[bc]) ? s[br] : d[bc];
            x[(size_t)br * (size_t)n + (size_t)bc] += q;
            s[br] -= q; d[bc] -= q;
            if (s[br] <= 1e-12) row_done[br] = 1;
            if (d[bc] <= 1e-12) col_done[bc] = 1;
        }
        (void)is_row;
    }
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) total += x[(size_t)i * (size_t)n + (size_t)j] * cost[(size_t)i * (size_t)n + (size_t)j];
    free(s); free(d); free(row_done); free(col_done);
    return total;
}

/* ============================== Trasporto: MODI ============================== */

gor_real gor_transport_modi(int m, int n, const gor_real *supply, const gor_real *demand,
                            const gor_real *cost, gor_real *x, int maxit)
{
    gor_real total;
    int *basic;
    int i, j, it;
    if (m <= 0 || n <= 0 || cost == NULL || x == NULL) return 0.0;
    total = gor_transport_vam(m, n, supply, demand, cost, x);
    basic = (int *)calloc((size_t)m * (size_t)n, sizeof(int));
    if (!basic) return total;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) basic[(size_t)i * (size_t)n + (size_t)j] = (x[(size_t)i * (size_t)n + (size_t)j] > 1e-9);
    for (it = 0; it < maxit; it++) {
        gor_real *u = (gor_real *)malloc((size_t)m * sizeof(gor_real));
        gor_real *v = (gor_real *)malloc((size_t)n * sizeof(gor_real));
        int *vis = (int *)calloc((size_t)(m + n), sizeof(int));
        int *q = (int *)malloc((size_t)(m + n) * sizeof(int));
        int h = 0, t = 0, bi = -1, bj = -1;
        gor_real bestrc = -1e-9;
        if (!u || !v || !vis || !q) { free(u); free(v); free(vis); free(q); break; }
        for (i = 0; i < m; i++) u[i] = GOR_INF;
        for (j = 0; j < n; j++) v[j] = GOR_INF;
        u[0] = 0.0; vis[0] = 1; q[t++] = 0;
        while (h < t) {
            int node = q[h++];
            if (node < m) {
                for (j = 0; j < n; j++)
                    if (basic[(size_t)node * (size_t)n + (size_t)j] && !vis[m + j]) {
                        v[j] = cost[(size_t)node * (size_t)n + (size_t)j] - u[node];
                        vis[m + j] = 1; q[t++] = m + j;
                    }
            } else {
                int col = node - m;
                for (i = 0; i < m; i++)
                    if (basic[(size_t)i * (size_t)n + (size_t)col] && !vis[i]) {
                        u[i] = cost[(size_t)i * (size_t)n + (size_t)col] - v[col];
                        vis[i] = 1; q[t++] = i;
                    }
            }
        }
        for (i = 0; i < m; i++)
            for (j = 0; j < n; j++)
                if (!basic[(size_t)i * (size_t)n + (size_t)j] && u[i] < GOR_INF / 2 && v[j] < GOR_INF / 2) {
                    gor_real rc = cost[(size_t)i * (size_t)n + (size_t)j] - u[i] - v[j];
                    if (rc < bestrc) { bestrc = rc; bi = i; bj = j; }
                }
        if (bi < 0) { free(u); free(v); free(vis); free(q); break; }
        /* ciclo: BFS da riga bi a colonna m+bj sulle celle basiche */
        {
            int *prev = (int *)malloc((size_t)(m + n) * sizeof(int));
            int *vis2 = (int *)calloc((size_t)(m + n), sizeof(int));
            int *q2 = (int *)malloc((size_t)(m + n) * sizeof(int));
            int h2 = 0, t2 = 0;
            for (i = 0; i < m + n; i++) prev[i] = -1;
            vis2[bi] = 1; q2[t2++] = bi;
            while (h2 < t2) {
                int node = q2[h2++];
                if (node < m) {
                    for (j = 0; j < n; j++)
                        if (basic[(size_t)node * (size_t)n + (size_t)j] && !vis2[m + j]) {
                            vis2[m + j] = 1; prev[m + j] = node; q2[t2++] = m + j;
                        }
                } else {
                    int col = node - m;
                    for (i = 0; i < m; i++)
                        if (basic[(size_t)i * (size_t)n + (size_t)col] && !vis2[i]) {
                            vis2[i] = 1; prev[i] = node; q2[t2++] = i;
                        }
                }
            }
            if (vis2[m + bj]) {
                int *path = (int *)malloc((size_t)(m + n) * sizeof(int));
                int plen = 0, node = m + bj;
                gor_real theta = GOR_INF;
                while (node != -1) { path[plen++] = node; node = prev[node]; }
                /* path[0..plen-1] = da col bj a riga bi; inverti */
                /* celle del ciclo: entering (bi,bj) segno +, poi celle del path (da riga bi) segno alternato */
                /* calcola theta sui "-" (indice dispari nella sequenza di celle) */
                /* sequenza nodi invertita: bi, ..., m+bj */
                {
                    int *nodes = (int *)malloc((size_t)plen * sizeof(int));
                    int k;
                    for (k = 0; k < plen; k++) nodes[k] = path[plen - 1 - k];
                    /* nodes[0]=bi, nodes[plen-1]=m+bj; celle del path: (nodes[k],nodes[k+1]) */
                    for (k = 0; k + 1 < plen; k++) {
                        int a = nodes[k], b2 = nodes[k + 1];
                        int ri = (a < m) ? a : b2;
                        int cj = (a < m) ? (b2 - m) : (a - m);
                        /* segno: entering è +, celle del path partono da - */
                        if (k % 2 == 0) { /* "-" */
                            if (x[(size_t)ri * (size_t)n + (size_t)cj] < theta) theta = x[(size_t)ri * (size_t)n + (size_t)cj];
                        }
                    }
                    if (theta < GOR_INF) {
                        /* pivot */
                        for (k = 0; k + 1 < plen; k++) {
                            int a = nodes[k], b2 = nodes[k + 1];
                            int ri = (a < m) ? a : b2;
                            int cj = (a < m) ? (b2 - m) : (a - m);
                            if (k % 2 == 0) {
                                x[(size_t)ri * (size_t)n + (size_t)cj] -= theta;
                                if (x[(size_t)ri * (size_t)n + (size_t)cj] < 1e-12) basic[(size_t)ri * (size_t)n + (size_t)cj] = 0;
                            } else {
                                x[(size_t)ri * (size_t)n + (size_t)cj] += theta;
                            }
                        }
                        x[(size_t)bi * (size_t)n + (size_t)bj] += theta;
                        basic[(size_t)bi * (size_t)n + (size_t)bj] = 1;
                    }
                    free(nodes);
                }
                free(path);
            }
            free(prev); free(vis2); free(q2);
        }
        free(u); free(v); free(vis); free(q);
    }
    total = 0.0;
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++) total += x[(size_t)i * (size_t)n + (size_t)j] * cost[(size_t)i * (size_t)n + (size_t)j];
    free(basic);
    return total;
}

/* ============================== QAP (tabu semplificato) ============================== */

static unsigned long qap_rng(unsigned long *s)
{
    *s = (*s * 6364136223846793005UL + 1442695040888963407UL);
    return *s >> 33;
}

static gor_real qap_cost(int n, const gor_real *A, const gor_real *B, const int *p)
{
    gor_real c = 0.0;
    int i, j;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            c += A[(size_t)i * (size_t)n + (size_t)j] * B[(size_t)p[i] * (size_t)n + (size_t)p[j]];
    return c;
}

gor_real gor_assign_qap_tabu(int n, const gor_real *A, const gor_real *B, int *p,
                             int maxit, unsigned long seed)
{
    int i, k;
    gor_real best;
    unsigned long s = seed ? seed : 1;
    if (n <= 0 || A == NULL || B == NULL || p == NULL) return 0.0;
    for (i = 0; i < n; i++) p[i] = i;
    best = qap_cost(n, A, B, p);
    for (k = 0; k < maxit; k++) {
        int a = (int)(qap_rng(&s) % (unsigned long)n);
        int b = (int)(qap_rng(&s) % (unsigned long)n);
        int t;
        gor_real nc;
        if (a == b) continue;
        t = p[a]; p[a] = p[b]; p[b] = t;
        nc = qap_cost(n, A, B, p);
        if (nc <= best) best = nc;
        else { t = p[a]; p[a] = p[b]; p[b] = t; } /* rollback */
    }
    return best;
}

/* ===================== angolo nord-ovest (da Antsurf) ===================== */

gor_real gor_transport_northwest(int m, int n, const gor_real *supply,
                                 const gor_real *demand, const gor_real *cost,
                                 gor_real *x)
{
    gor_real *s, *d;
    gor_real total = 0.0;
    int i = 0, j = 0, k;
    if (m <= 0 || n <= 0 || !supply || !demand || !x) return 0.0;
    s = (gor_real *)malloc((size_t)m * sizeof(gor_real));
    d = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    if (!s || !d) { free(s); free(d); return 0.0; }
    for (i = 0; i < m; i++) s[i] = supply[i];
    for (j = 0; j < n; j++) d[j] = demand[j];
    for (k = 0; k < m * n; k++) x[k] = 0.0;
    i = 0; j = 0;
    while (i < m && j < n) {
        gor_real v = (s[i] < d[j]) ? s[i] : d[j];
        x[(size_t)i * n + j] = v;
        s[i] -= v;
        d[j] -= v;
        if (s[i] <= 0.0) i++;
        if (d[j] <= 0.0) j++;
    }
    /* costo totale */
    for (i = 0; i < m; i++)
        for (j = 0; j < n; j++)
            total += x[(size_t)i * n + j] * cost[(size_t)i * n + j];
    free(s); free(d);
    return total;
}

/* ===================== metodo del costo minimo (da Abinash) ===================== */

gor_real gor_transport_least_cost(int m, int n, const gor_real *supply,
                                  const gor_real *demand, const gor_real *cost,
                                  gor_real *x)
{
    gor_real *s, *d;
    char *doneR, *doneC;
    int i, j, done = 0;
    if (m <= 0 || n <= 0 || !supply || !demand || !cost || !x) return 0.0;
    s = (gor_real *)malloc((size_t)m * sizeof(gor_real));
    d = (gor_real *)malloc((size_t)n * sizeof(gor_real));
    doneR = (char *)calloc((size_t)m, 1);
    doneC = (char *)calloc((size_t)n, 1);
    if (!s || !d || !doneR || !doneC) { free(s); free(d); free(doneR); free(doneC); return 0.0; }
    for (i = 0; i < m; i++) s[i] = supply[i];
    for (j = 0; j < n; j++) d[j] = demand[j];
    for (i = 0; i < m * n; i++) x[i] = 0.0;
    while (done < m + n) {
        int bi = -1, bj = -1; gor_real bc = 1e300;
        for (i = 0; i < m; i++) {
            if (doneR[i]) continue;
            for (j = 0; j < n; j++) {
                if (doneC[j]) continue;
                if (cost[(size_t)i * n + j] < bc) { bc = cost[(size_t)i * n + j]; bi = i; bj = j; }
            }
        }
        if (bi < 0) break;
        { gor_real v = (s[bi] < d[bj]) ? s[bi] : d[bj];
          x[(size_t)bi * n + bj] = v;
          s[bi] -= v; d[bj] -= v;
          if (s[bi] <= 1e-12) { doneR[bi] = 1; done++; }
          if (d[bj] <= 1e-12) { doneC[bj] = 1; done++; } }
    }
    { gor_real tot = 0.0; int k;
      for (k = 0; k < m * n; k++) tot += x[k] * cost[k];
      free(s); free(d); free(doneR); free(doneC); return tot; }
}
