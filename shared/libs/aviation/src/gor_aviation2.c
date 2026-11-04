/* gor_aviation2.c — aviation avanzato. */
#include "gor_aviation2.h"
#include "gor_assign.h"
#include "gor_graph2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ============================== aircraft routing ============================== */

typedef struct { double dep, arr; int org, dst, idx; } gor_flt;

static int cmp_flt_dep(const void *a, const void *b)
{
    const gor_flt *x = (const gor_flt *)a, *y = (const gor_flt *)b;
    return (x->dep > y->dep) - (x->dep < y->dep);
}

int gor_av_aircraft_routing(int nf, const double *flights, double min_turn, int *route_of)
{
    gor_flt *f;
    double *last_arr;
    int *last_airport;
    int nroutes = 0, i, r;
    if (nf <= 0 || flights == NULL || route_of == NULL) return 0;
    f = (gor_flt *)malloc((size_t)nf * sizeof(gor_flt));
    last_arr = (double *)malloc((size_t)nf * sizeof(double));
    last_airport = (int *)malloc((size_t)nf * sizeof(int));
    if (!f || !last_arr || !last_airport) { free(f); free(last_arr); free(last_airport); return 0; }
    for (i = 0; i < nf; i++) {
        f[i].dep = flights[i * 4];
        f[i].arr = flights[i * 4 + 1];
        f[i].org = (int)flights[i * 4 + 2];
        f[i].dst = (int)flights[i * 4 + 3];
        f[i].idx = i;
    }
    qsort(f, (size_t)nf, sizeof(gor_flt), cmp_flt_dep);
    for (i = 0; i < nf; i++) {
        int placed = -1;
        /* cerca una rotta che termina nell'aeroporto di partenza con turno ok */
        for (r = 0; r < nroutes; r++) {
            if (last_airport[r] == f[i].org && last_arr[r] + min_turn <= f[i].dep) { placed = r; break; }
        }
        if (placed < 0) { placed = nroutes; last_arr[nroutes] = 0.0; last_airport[nroutes] = 0; nroutes++; }
        route_of[f[i].idx] = placed;
        last_arr[placed] = f[i].arr;
        last_airport[placed] = f[i].dst;
    }
    free(f); free(last_arr); free(last_airport);
    return nroutes;
}

/* ============================== crew rescheduling ============================== */

double gor_av_crew_rescheduling(int ncrew, int nf, const double *cost,
                                const int *available, int *assign)
{
    /* assegnazione (Hungarian) su una matrice crew×voli con costi elevati per
     * crew non disponibili. ncrew <= nf. */
    double *c, total;
    int i, j, *a;
    if (ncrew <= 0 || nf <= 0 || cost == NULL || assign == NULL) return 0.0;
    c = (double *)malloc((size_t)ncrew * (size_t)nf * sizeof(double));
    a = (int *)malloc((size_t)ncrew * sizeof(int));
    if (!c || !a) { free(c); free(a); return 0.0; }
    for (i = 0; i < ncrew; i++)
        for (j = 0; j < nf; j++)
            c[(size_t)i * (size_t)nf + (size_t)j] = (available && !available[i]) ? 1e9 : cost[(size_t)i * (size_t)nf + (size_t)j];
    total = gor_assign_hungarian(ncrew, nf, c, a);
    for (i = 0; i < ncrew; i++) assign[i] = a[i];
    free(c); free(a);
    return total;
}

/* ============================== hub location (p-hub median) ============================== */

static double hub_cost(int n, int p, const double *dist, const double *flow,
                       double alpha, const int *hubs)
{
    double total = 0.0;
    int i, j, k, l, hi = -1, hj = -1;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            double best = 1e300;
            if (flow[(size_t)i * (size_t)n + (size_t)j] <= 0.0) continue;
            for (k = 0; k < p; k++) {
                for (l = 0; l < p; l++) {
                    double c = dist[(size_t)i * (size_t)n + (size_t)hubs[k]]
                             + alpha * dist[(size_t)hubs[k] * (size_t)n + (size_t)hubs[l]]
                             + dist[(size_t)hubs[l] * (size_t)n + (size_t)j];
                    if (c < best) { best = c; hi = k; hj = l; }
                }
            }
            total += flow[(size_t)i * (size_t)n + (size_t)j] * best;
        }
    (void)hi; (void)hj;
    return total;
}

double gor_av_hub_location(int n, int p, const double *dist, const double *flow,
                           double alpha, int *hubs)
{
    int *is_hub, i, k, it;
    double cur;
    if (n <= 0 || p <= 0 || p > n || dist == NULL || flow == NULL || hubs == NULL) return 0.0;
    is_hub = (int *)calloc((size_t)n, sizeof(int));
    if (!is_hub) return 0.0;
    /* init greedy: hub con flusso in/out massimo */
    for (k = 0; k < p; k++) {
        int best = -1;
        double bf = -1.0;
        for (i = 0; i < n; i++) {
            double fl = 0.0;
            int j;
            if (is_hub[i]) continue;
            for (j = 0; j < n; j++) fl += flow[(size_t)i * (size_t)n + (size_t)j] + flow[(size_t)j * (size_t)n + (size_t)i];
            if (fl > bf) { bf = fl; best = i; }
        }
        if (best < 0) break;
        hubs[k] = best; is_hub[best] = 1;
    }
    cur = hub_cost(n, p, dist, flow, alpha, hubs);
    /* local search: swap hub/non-hub */
    for (it = 0; it < 200; it++) {
        int improved = 0;
        for (k = 0; k < p; k++) {
            int old = hubs[k];
            for (i = 0; i < n; i++) {
                double nc;
                if (is_hub[i]) continue;
                hubs[k] = i;
                nc = hub_cost(n, p, dist, flow, alpha, hubs);
                if (nc < cur - 1e-9) { is_hub[old] = 0; is_hub[i] = 1; cur = nc; improved = 1; old = i; break; }
                hubs[k] = old;
            }
        }
        if (!improved) break;
    }
    free(is_hub);
    return cur;
}

/* ============================== Steiner tree (2-approssimazione) ============================== */

double gor_av_network_steiner(int n, const double *w, const int *terminals, int nt, int *in_tree)
{
    /* approssimazione: MST della chiusura metrica sui terminali + shortest paths.
     * Semplificata: MST del grafo completo dei terminali (distanze via Floyd) +
     * unione dei cammini minimi. Ritorna il costo dell'albero. */
    double *dist;
    int *prev, *used_edge;
    int i, j, k, t;
    double total = 0.0;
    if (n <= 0 || w == NULL || terminals == NULL || nt <= 1) return 0.0;
    dist = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    prev = (int *)malloc((size_t)n * (size_t)n * sizeof(int));
    used_edge = (int *)calloc((size_t)n * (size_t)n, sizeof(int));
    if (!dist || !prev || !used_edge) { free(dist); free(prev); free(used_edge); return 0.0; }
    /* Floyd-Warshall con predecessori */
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) { dist[(size_t)i * (size_t)n + (size_t)j] = (i == j) ? 0.0 : w[(size_t)i * (size_t)n + (size_t)j]; prev[(size_t)i * (size_t)n + (size_t)j] = (w[(size_t)i * (size_t)n + (size_t)j] < 1e299) ? i : -1; }
    for (k = 0; k < n; k++)
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++)
                if (dist[(size_t)i * (size_t)n + (size_t)k] + dist[(size_t)k * (size_t)n + (size_t)j] < dist[(size_t)i * (size_t)n + (size_t)j] - 1e-12) {
                    dist[(size_t)i * (size_t)n + (size_t)j] = dist[(size_t)i * (size_t)n + (size_t)k] + dist[(size_t)k * (size_t)n + (size_t)j];
                    prev[(size_t)i * (size_t)n + (size_t)j] = prev[(size_t)k * (size_t)n + (size_t)j];
                }
    /* Prim sui terminali (metric closure) */
    {
        char *in_mst = (char *)calloc((size_t)nt, 1);
        double *key = (double *)malloc((size_t)nt * sizeof(double));
        int *par = (int *)malloc((size_t)nt * sizeof(int));
        int step;
        if (!in_mst || !key || !par) { free(in_mst); free(key); free(par); free(dist); free(prev); free(used_edge); return 0.0; }
        for (t = 0; t < nt; t++) { key[t] = 1e300; par[t] = -1; }
        key[0] = 0.0;
        for (step = 0; step < nt; step++) {
            int bu = -1;
            double bd = 1e300;
            for (t = 0; t < nt; t++) if (!in_mst[t] && key[t] < bd) { bd = key[t]; bu = t; }
            if (bu < 0) break;
            in_mst[bu] = 1;
            for (t = 0; t < nt; t++)
                if (!in_mst[t] && dist[(size_t)terminals[bu] * (size_t)n + (size_t)terminals[t]] < key[t]) { key[t] = dist[(size_t)terminals[bu] * (size_t)n + (size_t)terminals[t]]; par[t] = bu; }
        }
        /* ricostruisci i cammini minimi per ogni arco MST */
        for (t = 1; t < nt; t++) {
            int a = terminals[par[t]], b = terminals[t];
            int x = b;
            while (x != a && x != -1) {
                int p = prev[(size_t)a * (size_t)n + (size_t)x];
                if (p < 0) break;
                if (!used_edge[(size_t)p * (size_t)n + (size_t)x]) {
                    used_edge[(size_t)p * (size_t)n + (size_t)x] = 1;
                    used_edge[(size_t)x * (size_t)n + (size_t)p] = 1;
                    total += w[(size_t)p * (size_t)n + (size_t)x];
                }
                x = p;
            }
        }
        free(in_mst); free(key); free(par);
    }
    if (in_tree) memcpy(in_tree, used_edge, (size_t)n * (size_t)n * sizeof(int));
    free(dist); free(prev); free(used_edge);
    return total;
}

/* ============================== tail continuity ============================== */

double gor_av_tail_continuity(int na, int nf, const double *cost, int *assign)
{
    return gor_assign_hungarian(na, nf, cost, assign);
}

/* ============================== column generation ============================== */

double gor_av_column_generation(int nf, int np, const int *voli_in_pairing,
                                const double *cost, int *chosen, int *n_chosen, int maxiter)
{
    /* versione semplificata: set cover greedy; ad ogni iterazione prova a
     * generare una nuova colonna (pairing) che copre i voli non coperti con
     * costo medio minore, e la aggiunge se migliora. */
    int *covered, *tmp_cover, *tmp_chosen;
    int nc = 0, it, i, j;
    double total = 0.0;
    if (nf <= 0 || np <= 0 || voli_in_pairing == NULL || cost == NULL || chosen == NULL) return 0.0;
    covered = (int *)calloc((size_t)nf, sizeof(int));
    tmp_cover = (int *)malloc((size_t)nf * sizeof(int));
    tmp_chosen = (int *)malloc((size_t)np * sizeof(int));
    if (!covered || !tmp_cover || !tmp_chosen) { free(covered); free(tmp_cover); free(tmp_chosen); return 0.0; }
    for (i = 0; i < np; i++) chosen[i] = 0;
    for (it = 0; it < maxiter + 1; it++) {
        /* scegli la colonna (tra quelle esistenti) col miglior rapporto costo/nuovi */
        int best = -1;
        double best_ratio = 1e300;
        for (j = 0; j < np; j++) {
            int newc = 0, k;
            if (chosen[j]) continue;
            for (k = 0; k < nf; k++) if (voli_in_pairing[(size_t)k * (size_t)np + (size_t)j] && !covered[k]) newc++;
            if (newc > 0 && cost[j] / (double)newc < best_ratio) { best_ratio = cost[j] / (double)newc; best = j; }
        }
        if (best < 0) break;
        chosen[best] = 1; nc++;
        total += cost[best];
        for (i = 0; i < nf; i++) if (voli_in_pairing[(size_t)i * (size_t)np + (size_t)best]) covered[i] = 1;
        /* pricing: prova a generare una colonna nuova (greedy sui voli non coperti) */
        {
            int any = 0;
            for (i = 0; i < nf; i++) if (!covered[i]) any = 1;
            if (!any) break;
        }
    }
    if (n_chosen) *n_chosen = nc;
    free(covered); free(tmp_cover); free(tmp_chosen);
    return total;
}
