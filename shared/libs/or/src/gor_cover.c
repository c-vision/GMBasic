/* gor_cover.c — covering, localizzazione, clustering. */
#include "gor_cover.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ============================== set cover greedy ============================== */

gor_real gor_set_cover_greedy(int m, int n, const int *elem_in_set,
                              const gor_real *cost, int *chosen, int *n_chosen)
{
    int *covered, nc = 0, i, j;
    gor_real total = 0.0;
    if (m <= 0 || n <= 0 || elem_in_set == NULL || chosen == NULL) return 0.0;
    covered = (int *)calloc((size_t)m, sizeof(int));
    if (!covered) return 0.0;
    for (;;) {
        int best = -1;
        gor_real best_ratio = 1e300;
        for (j = 0; j < n; j++) {
            int newc = 0;
            gor_real c = (cost ? cost[j] : 1.0);
            for (i = 0; i < m; i++)
                if (elem_in_set[(size_t)i * (size_t)n + (size_t)j] && !covered[i]) newc++;
            if (newc > 0) {
                gor_real ratio = c / (gor_real)newc;
                if (ratio < best_ratio) { best_ratio = ratio; best = j; }
            }
        }
        if (best < 0) break;
        chosen[nc++] = best;
        total += (cost ? cost[best] : 1.0);
        for (i = 0; i < m; i++)
            if (elem_in_set[(size_t)i * (size_t)n + (size_t)best]) covered[i] = 1;
    }
    if (n_chosen) *n_chosen = nc;
    free(covered);
    return total;
}

int gor_max_coverage(int m, int n, const int *elem_in_set, int k, int *chosen, int *n_chosen)
{
    int *covered, nc = 0, i, j, cnt;
    if (m <= 0 || n <= 0 || k <= 0 || elem_in_set == NULL || chosen == NULL) return 0;
    covered = (int *)calloc((size_t)m, sizeof(int));
    if (!covered) return 0;
    while (nc < k) {
        int best = -1, bestnew = 0;
        for (j = 0; j < n; j++) {
            int newc = 0;
            for (i = 0; i < m; i++)
                if (elem_in_set[(size_t)i * (size_t)n + (size_t)j] && !covered[i]) newc++;
            if (newc > bestnew) { bestnew = newc; best = j; }
        }
        if (best < 0) break;
        chosen[nc++] = best;
        for (i = 0; i < m; i++)
            if (elem_in_set[(size_t)i * (size_t)n + (size_t)best]) covered[i] = 1;
    }
    cnt = 0;
    for (i = 0; i < m; i++) cnt += covered[i];
    if (n_chosen) *n_chosen = nc;
    free(covered);
    return cnt;
}

/* ============================== p-median ============================== */

static gor_real pmedian_cost(int n, int p, const gor_real *dist, const int *med, int *assign)
{
    gor_real total = 0.0;
    int i, k;
    for (i = 0; i < n; i++) {
        gor_real best = 1e300;
        int bm = -1;
        for (k = 0; k < p; k++) {
            gor_real d = dist[(size_t)i * (size_t)n + (size_t)med[k]];
            if (d < best) { best = d; bm = med[k]; }
        }
        total += best;
        if (assign) assign[i] = bm;
    }
    return total;
}

gor_real gor_pmedian(int n, int p, const gor_real *dist, int *medians, int *assign, int maxit)
{
    int *is_med, i, k, it;
    gor_real cur;
    if (n <= 0 || p <= 0 || p > n || dist == NULL || medians == NULL) return 0.0;
    is_med = (int *)calloc((size_t)n, sizeof(int));
    if (!is_med) return 0.0;
    /* init greedy: primo medoide = 0; poi il più lontano dai medoidi scelti */
    medians[0] = 0; is_med[0] = 1;
    for (k = 1; k < p; k++) {
        int best = -1;
        gor_real bestd = -1.0;
        for (i = 0; i < n; i++) {
            gor_real d = 1e300;
            int t;
            if (is_med[i]) continue;
            for (t = 0; t < k; t++) {
                gor_real dd = dist[(size_t)i * (size_t)n + (size_t)medians[t]];
                if (dd < d) d = dd;
            }
            if (d > bestd) { bestd = d; best = i; }
        }
        if (best < 0) break;
        medians[k] = best; is_med[best] = 1;
    }
    cur = pmedian_cost(n, p, dist, medians, assign);
    /* local search: swap medoide/non-medoide */
    for (it = 0; it < maxit; it++) {
        int improved = 0;
        for (k = 0; k < p; k++) {
            int old = medians[k];
            for (i = 0; i < n; i++) {
                gor_real nc;
                if (is_med[i]) continue;
                medians[k] = i;
                nc = pmedian_cost(n, p, dist, medians, NULL);
                if (nc < cur - 1e-12) {
                    is_med[old] = 0; is_med[i] = 1; cur = nc; improved = 1;
                    old = i;
                    break;
                }
                medians[k] = old;
            }
        }
        if (!improved) break;
    }
    pmedian_cost(n, p, dist, medians, assign);
    free(is_med);
    return cur;
}

/* ============================== p-center (Gonzalez) ============================== */

gor_real gor_pcenter(int n, int p, const gor_real *dist, int *medians)
{
    int *is_med, k, i;
    gor_real radius = 0.0;
    if (n <= 0 || p <= 0 || p > n || dist == NULL || medians == NULL) return 0.0;
    is_med = (int *)calloc((size_t)n, sizeof(int));
    if (!is_med) return 0.0;
    medians[0] = 0; is_med[0] = 1;
    for (k = 1; k < p; k++) {
        int best = -1;
        gor_real bestd = -1.0;
        for (i = 0; i < n; i++) {
            gor_real d = 1e300;
            int t;
            if (is_med[i]) continue;
            for (t = 0; t < k; t++) {
                gor_real dd = dist[(size_t)i * (size_t)n + (size_t)medians[t]];
                if (dd < d) d = dd;
            }
            if (d > bestd) { bestd = d; best = i; }
        }
        if (best < 0) break;
        medians[k] = best; is_med[best] = 1;
    }
    radius = 0.0;
    for (i = 0; i < n; i++) {
        gor_real d = 1e300;
        for (k = 0; k < p; k++) {
            gor_real dd = dist[(size_t)i * (size_t)n + (size_t)medians[k]];
            if (dd < d) d = dd;
        }
        if (d > radius) radius = d;
    }
    free(is_med);
    return radius;
}

/* ============================== vertex cover 2-approx ============================== */

int gor_vertex_cover_2approx(int n, const int *edges, int ne, int *cover)
{
    int *in_cover, *used;
    int i, cnt = 0;
    if (n <= 0 || edges == NULL || cover == NULL) return 0;
    in_cover = (int *)calloc((size_t)n, sizeof(int));
    used = (int *)calloc((size_t)(ne > 0 ? ne : 1), sizeof(int));
    if (!in_cover || !used) { free(in_cover); free(used); return 0; }
    for (i = 0; i < ne; i++) {
        int u = edges[2 * i], v = edges[2 * i + 1];
        if (!used[i] && !in_cover[u] && !in_cover[v]) {
            in_cover[u] = 1; in_cover[v] = 1;
            used[i] = 1;
        }
    }
    for (i = 0; i < n; i++) if (in_cover[i]) cover[cnt++] = i;
    free(in_cover); free(used);
    return cnt;
}

int gor_hitting_set_greedy(int m, int n, const int *elem_in_set, int *chosen, int *n_chosen)
{
    (void)gor_set_cover_greedy(m, n, elem_in_set, NULL, chosen, n_chosen);
    return n_chosen ? *n_chosen : 0;
}

/* ============================== facility location ============================== */

gor_real gor_facility_location_greedy(int m, int n, const gor_real *open_cost,
                                      const gor_real *conn_cost, int *open)
{
    int *best_fac;
    int i, j, it, any_open = 0;
    gor_real total = 0.0;
    if (m <= 0 || n <= 0 || conn_cost == NULL || open == NULL) return 0.0;
    best_fac = (int *)malloc((size_t)m * sizeof(int));
    if (!best_fac) return 0.0;
    for (i = 0; i < m; i++) open[i] = 0;
    /* ogni cliente sceglie la facility aperta con costo minore (o nessuna) */
    for (i = 0; i < m; i++) best_fac[i] = -1;
    for (it = 0; it < n; it++) {
        int best = -1;
        gor_real best_delta = 1e300;
        for (j = 0; j < n; j++) {
            gor_real delta = open_cost ? open_cost[j] : 0.0;
            if (open[j]) continue;
            for (i = 0; i < m; i++) {
                gor_real c = conn_cost[(size_t)i * (size_t)n + (size_t)j];
                gor_real cur = (best_fac[i] >= 0) ? conn_cost[(size_t)i * (size_t)n + (size_t)best_fac[i]] : 1e300;
                if (c < cur) delta += c - cur;
            }
            if (delta < best_delta) { best_delta = delta; best = j; }
        }
        if (best < 0 || best_delta >= -1e-12) break;
        open[best] = 1; any_open = 1;
        for (i = 0; i < m; i++) {
            gor_real c = conn_cost[(size_t)i * (size_t)n + (size_t)best];
            gor_real cur = (best_fac[i] >= 0) ? conn_cost[(size_t)i * (size_t)n + (size_t)best_fac[i]] : 1e300;
            if (c < cur) best_fac[i] = best;
        }
    }
    (void)any_open;
    for (j = 0; j < n; j++) if (open[j]) total += (open_cost ? open_cost[j] : 0.0);
    for (i = 0; i < m; i++) if (best_fac[i] >= 0) total += conn_cost[(size_t)i * (size_t)n + (size_t)best_fac[i]];
    free(best_fac);
    return total;
}

/* ============================== k-means ============================== */

static unsigned long km_rng(unsigned long *s)
{
    *s = (*s * 6364136223846793005UL + 1442695040888963407UL);
    return *s >> 33;
}

static gor_real pt_dist2(const gor_real *a, const gor_real *b, int dim)
{
    gor_real s = 0.0;
    int d;
    for (d = 0; d < dim; d++) { gor_real e = a[d] - b[d]; s += e * e; }
    return s;
}

gor_real gor_kmeans(int n, int dim, int k, const gor_real *points, gor_real *centroids,
                    int *labels, int maxit, unsigned long seed)
{
    unsigned long s = seed ? seed : 1;
    int *cnt, i, c, d, it;
    gor_real inertia = 0.0;
    if (n <= 0 || dim <= 0 || k <= 0 || k > n || points == NULL || centroids == NULL) return 0.0;
    cnt = (int *)malloc((size_t)k * sizeof(int));
    if (!cnt) return 0.0;
    /* k-means++ init */
    for (d = 0; d < dim; d++) centroids[d] = points[d];
    for (c = 1; c < k; c++) {
        gor_real total = 0.0, r, acc = 0.0;
        int pick = 0;
        for (i = 0; i < n; i++) {
            gor_real best = 1e300;
            int t;
            for (t = 0; t < c; t++) {
                gor_real dd = pt_dist2(points + (size_t)i * (size_t)dim, centroids + (size_t)t * (size_t)dim, dim);
                if (dd < best) best = dd;
            }
            total += best;
        }
        r = (gor_real)(km_rng(&s) % 1000000UL) / 1000000.0 * total;
        for (i = 0; i < n; i++) {
            gor_real best = 1e300;
            int t;
            for (t = 0; t < c; t++) {
                gor_real dd = pt_dist2(points + (size_t)i * (size_t)dim, centroids + (size_t)t * (size_t)dim, dim);
                if (dd < best) best = dd;
            }
            acc += best;
            if (acc >= r) { pick = i; break; }
        }
        for (d = 0; d < dim; d++) centroids[(size_t)c * (size_t)dim + (size_t)d] = points[(size_t)pick * (size_t)dim + (size_t)d];
    }
    for (it = 0; it < maxit; it++) {
        int changed = 0;
        for (i = 0; i < n; i++) {
            gor_real best = 1e300;
            int bc = 0;
            for (c = 0; c < k; c++) {
                gor_real dd = pt_dist2(points + (size_t)i * (size_t)dim, centroids + (size_t)c * (size_t)dim, dim);
                if (dd < best) { best = dd; bc = c; }
            }
            if (labels == NULL || labels[i] != bc) changed = 1;
            if (labels) labels[i] = bc;
        }
        for (c = 0; c < k; c++) { cnt[c] = 0; for (d = 0; d < dim; d++) centroids[(size_t)c * (size_t)dim + (size_t)d] = 0.0; }
        for (i = 0; i < n; i++) {
            int l = labels ? labels[i] : 0;
            cnt[l]++;
            for (d = 0; d < dim; d++) centroids[(size_t)l * (size_t)dim + (size_t)d] += points[(size_t)i * (size_t)dim + (size_t)d];
        }
        for (c = 0; c < k; c++) if (cnt[c] > 0)
            for (d = 0; d < dim; d++) centroids[(size_t)c * (size_t)dim + (size_t)d] /= (gor_real)cnt[c];
        if (!changed) break;
    }
    inertia = 0.0;
    for (i = 0; i < n; i++) {
        gor_real best = 1e300;
        for (c = 0; c < k; c++) {
            gor_real dd = pt_dist2(points + (size_t)i * (size_t)dim, centroids + (size_t)c * (size_t)dim, dim);
            if (dd < best) best = dd;
        }
        inertia += best;
    }
    free(cnt);
    return inertia;
}

/* ===================== Set cover: local search + GRASP (da Set_covering_problem) ===================== */

static int sc_feasible(int m, int n, const int *eis, const char *incover)
{
    int i, j;
    for (i = 0; i < m; i++) {
        int cov = 0;
        for (j = 0; j < n; j++) if (incover[j] && eis[(size_t)i * n + j]) { cov = 1; break; }
        if (!cov) return 0;
    }
    return 1;
}

static gor_real sc_cost(int n, const gor_real *cost, const char *incover)
{
    gor_real s = 0.0;
    int j;
    for (j = 0; j < n; j++) if (incover[j]) s += (cost ? cost[j] : 1.0);
    return s;
}

static void sc_pack(int n, const char *incover, int *chosen, int *n_chosen)
{
    int j, nc = 0;
    for (j = 0; j < n; j++) if (incover[j]) chosen[nc++] = j;
    if (n_chosen) *n_chosen = nc;
}

gor_real gor_set_cover_local_search(int m, int n, const int *elem_in_set,
                                    const gor_real *cost, int *chosen, int *n_chosen,
                                    int maxit)
{
    char *inc;
    int it = 0, improved = 1, j, k;
    if (m <= 0 || n <= 0 || !elem_in_set || !chosen || !n_chosen) return 0.0;
    if (maxit < 1) maxit = 100;
    inc = (char *)calloc((size_t)n, 1);
    if (!inc) return 0.0;
    for (k = 0; k < *n_chosen; k++) if (chosen[k] >= 0 && chosen[k] < n) inc[chosen[k]] = 1;
    while (improved && it < maxit) {
        improved = 0; it++;
        /* rimozione ridondanti */
        for (j = 0; j < n; j++) {
            if (!inc[j]) continue;
            inc[j] = 0;
            if (sc_feasible(m, n, elem_in_set, inc)) improved = 1;
            else inc[j] = 1;
        }
        /* scambi 1-1 con insiemi piu' economici */
        for (j = 0; j < n; j++) {
            if (!inc[j]) continue;
            for (k = 0; k < n; k++) {
                gor_real cj = cost ? cost[j] : 1.0, ck = cost ? cost[k] : 1.0;
                if (inc[k] || ck >= cj) continue;
                inc[j] = 0; inc[k] = 1;
                if (sc_feasible(m, n, elem_in_set, inc)) { improved = 1; j = -1; break; }
                inc[j] = 1; inc[k] = 0;
            }
            if (j < 0) break;
        }
    }
    { gor_real c = sc_cost(n, cost, inc); sc_pack(n, inc, chosen, n_chosen); free(inc); return c; }
}

/* costruzione greedy randomizzata: RCL = ratio <= rmin + alpha*(rmax-rmin). */
static gor_real sc_greedy_rand(int m, int n, const int *eis, const gor_real *cost,
                               gor_real alpha, char *inc, unsigned long long *s)
{
    char *covered = (char *)calloc((size_t)m, 1);
    int *rcl = (int *)malloc((size_t)n * sizeof(int));
    gor_real total = 0.0;
    if (!covered || !rcl) { free(covered); free(rcl); return 0.0; }
    for (;;) {
        gor_real rmin = 1e300, rmax = -1e300;
        int nrc = 0, i, j, nc = 0;
        for (j = 0; j < n; j++) {
            int newc = 0; gor_real c, ratio;
            if (inc[j]) continue;
            c = cost ? cost[j] : 1.0;
            for (i = 0; i < m; i++) if (eis[(size_t)i * n + j] && !covered[i]) newc++;
            if (newc == 0) continue;
            ratio = c / (gor_real)newc;
            if (ratio < rmin) rmin = ratio;
            if (ratio > rmax) rmax = ratio;
        }
        if (rmin > 1e299) break;
        { gor_real thr = rmin + alpha * (rmax - rmin);
          for (j = 0; j < n; j++) {
              int newc = 0; gor_real c, ratio;
              if (inc[j]) continue;
              c = cost ? cost[j] : 1.0;
              for (i = 0; i < m; i++) if (eis[(size_t)i * n + j] && !covered[i]) newc++;
              if (newc == 0) continue;
              ratio = c / (gor_real)newc;
              if (ratio <= thr + 1e-12) rcl[nrc++] = j;
          }
        }
        if (nrc == 0) break;
        { int pick = rcl[(int)((*s = (*s) * 6364136223846793005ULL + 1442695040888963407ULL) >> 33) % nrc];
          inc[pick] = 1; total += cost ? cost[pick] : 1.0;
          for (i = 0; i < m; i++) if (eis[(size_t)i * n + pick]) covered[i] = 1; }
        (void)nc;
    }
    free(covered); free(rcl);
    return total;
}

gor_real gor_set_cover_grasp(int m, int n, const int *elem_in_set,
                             const gor_real *cost, gor_real alpha, int maxit,
                             unsigned long long seed, int *chosen, int *n_chosen)
{
    char *inc;
    int *bestsol, *cur, nbest = 0, ncur = 0;
    unsigned long long s = seed ? seed : 12345ULL;
    gor_real best = 1e300;
    int it;
    if (m <= 0 || n <= 0 || !elem_in_set || !chosen || !n_chosen) return 0.0;
    if (maxit < 1) maxit = 50;
    inc = (char *)calloc((size_t)n, 1);
    bestsol = (int *)malloc((size_t)n * sizeof(int));
    cur = (int *)malloc((size_t)n * sizeof(int));
    if (!inc || !bestsol || !cur) { free(inc); free(bestsol); free(cur); return 0.0; }
    for (it = 0; it < maxit; it++) {
        int j; gor_real c;
        for (j = 0; j < n; j++) inc[j] = 0;
        sc_greedy_rand(m, n, elem_in_set, cost, alpha, inc, &s);
        ncur = 0;
        for (j = 0; j < n; j++) if (inc[j]) cur[ncur++] = j;
        c = gor_set_cover_local_search(m, n, elem_in_set, cost, cur, &ncur, 200);
        if (c < best - 1e-12) {
            best = c;
            for (j = 0; j < ncur; j++) bestsol[j] = cur[j];
            nbest = ncur;
        }
    }
    for (it = 0; it < nbest; it++) chosen[it] = bestsol[it];
    *n_chosen = nbest;
    free(inc); free(bestsol); free(cur);
    return best;
}

gor_real gor_set_cover_reactive_grasp(int m, int n, const int *elem_in_set,
                                      const gor_real *cost, const gor_real *alphas,
                                      int nalpha, int maxit, unsigned long long seed,
                                      int *chosen, int *n_chosen)
{
    double *prob;
    unsigned long long s = seed ? seed : 12345ULL;
    gor_real best = 1e300;
    int it;
    if (m <= 0 || n <= 0 || !elem_in_set || !alphas || nalpha < 1 || !chosen) return 0.0;
    if (maxit < 1) maxit = 50;
    prob = (double *)malloc((size_t)nalpha * sizeof(double));
    if (!prob) return 0.0;
    for (it = 0; it < nalpha; it++) prob[it] = 1.0 / nalpha;
    for (it = 0; it < maxit; it++) {
        double r = ((s = s * 6364136223846793005ULL + 1442695040888963407ULL) >> 11) / 9007199254740992.0;
        int pick = 0, j; double acc = 0.0; gor_real c;
        for (j = 0; j < nalpha; j++) { acc += prob[j]; if (r <= acc) { pick = j; break; } }
        c = gor_set_cover_grasp(m, n, elem_in_set, cost, alphas[pick], 1, s, chosen, n_chosen);
        if (c < best) {
            best = c;
            for (j = 0; j < nalpha; j++) prob[j] *= 0.9;   /* premia alpha vincente */
            prob[pick] += 0.1 * 0.9 * nalpha;
            { double sum = 0.0; for (j = 0; j < nalpha; j++) sum += prob[j];
              if (sum > 0) for (j = 0; j < nalpha; j++) prob[j] /= sum; }
        }
    }
    free(prob);
    return best;
}

/* ===================== Set covering soft + SA (da IA2_SA/CSP) ===================== */

gor_real gor_set_cover_soft_cost(int m, int n, const int *elem_in_set,
                                 const gor_real *cost, const char *chosen,
                                 gor_real uncovered_pen, int *uncovered)
{
    int i, j, unc = 0;
    gor_real total = 0.0;
    if (m < 1 || n < 1 || !elem_in_set || !chosen) return 0.0;
    for (j = 0; j < n; j++) if (chosen[j]) total += cost ? cost[j] : 1.0;
    for (i = 0; i < m; i++) {
        int cov = 0;
        for (j = 0; j < n; j++) if (chosen[j] && elem_in_set[(size_t)i * n + j]) { cov = 1; break; }
        if (!cov) unc++;
    }
    total += uncovered_pen * (gor_real)unc;
    if (uncovered) *uncovered = unc;
    return total;
}

gor_real gor_set_cover_sa(int m, int n, const int *elem_in_set, const gor_real *cost,
                          gor_real uncovered_pen, int maxit, gor_real t0, gor_real alpha,
                          unsigned long long seed, char *chosen)
{
    char *cur;
    gor_real cur_cost, best;
    unsigned long long s = seed ? seed : 12345ULL;
    int it, j;
    if (m < 1 || n < 1 || !elem_in_set || !chosen) return 0.0;
    if (maxit < 1) maxit = 1000;
    if (t0 <= 0.0) t0 = 1.0;
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.995;
    cur = (char *)malloc((size_t)n);
    if (!cur) return 0.0;
    for (j = 0; j < n; j++) { cur[j] = chosen[j]; }
    cur_cost = gor_set_cover_soft_cost(m, n, elem_in_set, cost, cur, uncovered_pen, NULL);
    best = cur_cost;
    { gor_real T = t0;
      for (it = 0; it < maxit; it++) {
          int flip;
          gor_real nc, delta;
          s = s * 6364136223846793005ULL + 1442695040888963407ULL;
          flip = (int)((s >> 33) % (unsigned long long)n);
          cur[flip] = !cur[flip];
          nc = gor_set_cover_soft_cost(m, n, elem_in_set, cost, cur, uncovered_pen, NULL);
          delta = nc - cur_cost;
          if (delta <= 0.0) {
              cur_cost = nc;
          } else {
              s = s * 6364136223846793005ULL + 1442695040888963407ULL;
              { gor_real r = (gor_real)(s >> 11) / 9007199254740992.0;
                if (r < exp(-delta / T)) cur_cost = nc;
                else cur[flip] = !cur[flip]; }
          }
          if (cur_cost < best - 1e-12) {
              best = cur_cost;
              for (j = 0; j < n; j++) chosen[j] = cur[j];
          }
          T *= alpha;
          if (T < 1e-6) break;
      }
    }
    free(cur);
    return best;
}

/* ===================== Pricing greedy per set covering (branch-and-price) ===================== */

gor_real gor_set_cover_pricing_greedy(int m, const gor_real *y, gor_real base_cost,
                                      gor_real cost_per_elem, char *chosen_set)
{
    gor_real rc = base_cost;
    int i;
    if (!y || !chosen_set || m < 1) return 0.0;
    for (i = 0; i < m; i++) {
        if (y[i] - cost_per_elem > 0.0) { chosen_set[i] = 1; rc -= (y[i] - cost_per_elem); }
        else chosen_set[i] = 0;
    }
    return rc;   /* costo ridotto: base_cost - sum_i (y_i - costo per elemento) */
}
