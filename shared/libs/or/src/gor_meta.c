/* gor_meta.c — meta-euristiche. */
#include "gor_meta.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned long mrng(unsigned long *s)
{
    *s = (*s * 6364136223846793005UL + 1442695040888963407UL);
    return *s >> 33;
}

static gor_real urand(unsigned long *s) { return (gor_real)(mrng(s) % 1000000UL) / 1000000.0; }

static gor_real grand(unsigned long *s)
{
    gor_real u1 = urand(s) + 1e-12, u2 = urand(s);
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

static void clamp_vec(gor_real *x, int d, const gor_real *lo, const gor_real *hi)
{
    int i;
    for (i = 0; i < d; i++) { if (x[i] < lo[i]) x[i] = lo[i]; if (x[i] > hi[i]) x[i] = hi[i]; }
}

gor_real gor_meta_hill_climb(gor_meta_fn f, void *ctx, gor_real *x, int d,
                             const gor_real *lo, const gor_real *hi, int maxit, unsigned long seed)
{
    unsigned long s = seed ? seed : 1;
    gor_real best, *xn;
    int it;
    if (f == NULL || x == NULL || d <= 0) return 0.0;
    xn = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    if (!xn) return 0.0;
    best = f(x, d, ctx);
    for (it = 0; it < maxit; it++) {
        gor_real v;
        int k;
        for (k = 0; k < d; k++) xn[k] = x[k] + grand(&s) * (hi[k] - lo[k]) * 0.1;
        clamp_vec(xn, d, lo, hi);
        v = f(xn, d, ctx);
        if (v < best) { memcpy(x, xn, (size_t)d * sizeof(gor_real)); best = v; }
    }
    free(xn);
    return best;
}

gor_real gor_meta_sa(gor_meta_fn f, void *ctx, gor_real *x, int d,
                     const gor_real *lo, const gor_real *hi, int maxit,
                     gor_real t0, gor_real alpha, unsigned long seed)
{
    unsigned long s = seed ? seed : 1;
    gor_real cur, *xn;
    int it, k;
    if (f == NULL || x == NULL || d <= 0) return 0.0;
    if (t0 <= 0.0) t0 = 1.0;
    if (alpha <= 0.0 || alpha >= 1.0) alpha = 0.995;
    xn = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    if (!xn) return 0.0;
    cur = f(x, d, ctx);
    for (it = 0; it < maxit; it++) {
        gor_real v, t = t0 * pow(alpha, (gor_real)it), delta;
        for (k = 0; k < d; k++) xn[k] = x[k] + grand(&s) * (hi[k] - lo[k]) * 0.2;
        clamp_vec(xn, d, lo, hi);
        v = f(xn, d, ctx);
        delta = v - cur;
        if (delta < 0.0 || urand(&s) < exp(-delta / (t + 1e-12))) {
            memcpy(x, xn, (size_t)d * sizeof(gor_real));
            cur = v;
        }
    }
    free(xn);
    return cur;
}

gor_real gor_meta_tabu(gor_meta_fn f, void *ctx, gor_real *x, int d,
                       const gor_real *lo, const gor_real *hi, int maxit, int tenure, unsigned long seed)
{
    unsigned long s = seed ? seed : 1;
    gor_real best, *xn, *bestx;
    int it, k;
    if (f == NULL || x == NULL || d <= 0) return 0.0;
    if (tenure < 1) tenure = 10;
    xn = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    bestx = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    if (!xn || !bestx) { free(xn); free(bestx); return 0.0; }
    best = f(x, d, ctx);
    memcpy(bestx, x, (size_t)d * sizeof(gor_real));
    for (it = 0; it < maxit; it++) {
        gor_real local = 1e300;
        int bi = 0;
        for (k = 0; k < d; k++) {
            gor_real save = x[k];
            x[k] = save + ((urand(&s) < 0.5) ? -1.0 : 1.0) * (hi[k] - lo[k]) * 0.05;
            clamp_vec(x, d, lo, hi);
            {
                gor_real v = f(x, d, ctx);
                if (v < local) { local = v; bi = k; }
            }
            x[k] = save;
        }
        {
            gor_real save = x[bi];
            x[bi] = save + ((urand(&s) < 0.5) ? -1.0 : 1.0) * (hi[bi] - lo[bi]) * 0.05;
            clamp_vec(x, d, lo, hi);
            (void)tenure;
        }
        if (local < best) { best = local; memcpy(bestx, x, (size_t)d * sizeof(gor_real)); }
    }
    memcpy(x, bestx, (size_t)d * sizeof(gor_real));
    free(xn); free(bestx);
    return best;
}

gor_real gor_meta_ga(gor_meta_fn f, void *ctx, gor_real *x, int d,
                     const gor_real *lo, const gor_real *hi, int pop, int maxgen, unsigned long seed)
{
    unsigned long s = seed ? seed : 1;
    gor_real *P, *fit, *bestx;
    int i, k, g;
    gor_real best = 1e300;
    if (f == NULL || x == NULL || d <= 0) return 0.0;
    if (pop < 4) pop = 20;
    P = (gor_real *)malloc((size_t)pop * (size_t)d * sizeof(gor_real));
    fit = (gor_real *)malloc((size_t)pop * sizeof(gor_real));
    bestx = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    if (!P || !fit || !bestx) { free(P); free(fit); free(bestx); return 0.0; }
    for (i = 0; i < pop; i++)
        for (k = 0; k < d; k++) P[(size_t)i * (size_t)d + (size_t)k] = lo[k] + urand(&s) * (hi[k] - lo[k]);
    for (g = 0; g < maxgen; g++) {
        for (i = 0; i < pop; i++) fit[i] = f(P + (size_t)i * (size_t)d, d, ctx);
        for (i = 0; i < pop; i++) if (fit[i] < best) { best = fit[i]; memcpy(bestx, P + (size_t)i * (size_t)d, (size_t)d * sizeof(gor_real)); }
        /* nuova generazione: elitismo + tournament + blend crossover + mutazione */
        {
            gor_real *NP = (gor_real *)malloc((size_t)pop * (size_t)d * sizeof(gor_real));
            if (!NP) break;
            memcpy(NP, bestx, (size_t)d * sizeof(gor_real));
            for (i = 1; i < pop; i++) {
                int a1 = (int)(mrng(&s) % (unsigned long)pop), a2 = (int)(mrng(&s) % (unsigned long)pop);
                int p1 = fit[a1] < fit[a2] ? a1 : a2;
                int b1 = (int)(mrng(&s) % (unsigned long)pop), b2 = (int)(mrng(&s) % (unsigned long)pop);
                int p2 = fit[b1] < fit[b2] ? b1 : b2;
                for (k = 0; k < d; k++) {
                    gor_real alpha = -0.1 + urand(&s) * 1.2;
                    gor_real c = P[(size_t)p1 * (size_t)d + (size_t)k] + alpha * (P[(size_t)p2 * (size_t)d + (size_t)k] - P[(size_t)p1 * (size_t)d + (size_t)k]);
                    if (urand(&s) < 0.1) c += grand(&s) * (hi[k] - lo[k]) * 0.1;
                    NP[(size_t)i * (size_t)d + (size_t)k] = c;
                }
                clamp_vec(NP + (size_t)i * (size_t)d, d, lo, hi);
            }
            memcpy(P, NP, (size_t)pop * (size_t)d * sizeof(gor_real));
            free(NP);
        }
    }
    memcpy(x, bestx, (size_t)d * sizeof(gor_real));
    free(P); free(fit); free(bestx);
    return best;
}

gor_real gor_meta_pso(gor_meta_fn f, void *ctx, gor_real *x, int d,
                      const gor_real *lo, const gor_real *hi, int swarm, int maxit, unsigned long seed)
{
    unsigned long s = seed ? seed : 1;
    gor_real *X, *V, *pb, *pbfit, *gb;
    int i, k, it;
    gor_real gbfit = 1e300;
    if (f == NULL || x == NULL || d <= 0) return 0.0;
    if (swarm < 4) swarm = 30;
    X = (gor_real *)malloc((size_t)swarm * (size_t)d * sizeof(gor_real));
    V = (gor_real *)calloc((size_t)swarm * (size_t)d, sizeof(gor_real));
    pb = (gor_real *)malloc((size_t)swarm * (size_t)d * sizeof(gor_real));
    pbfit = (gor_real *)malloc((size_t)swarm * sizeof(gor_real));
    gb = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    if (!X || !V || !pb || !pbfit || !gb) { free(X); free(V); free(pb); free(pbfit); free(gb); return 0.0; }
    for (i = 0; i < swarm; i++) {
        for (k = 0; k < d; k++) X[(size_t)i * (size_t)d + (size_t)k] = lo[k] + urand(&s) * (hi[k] - lo[k]);
        memcpy(pb + (size_t)i * (size_t)d, X + (size_t)i * (size_t)d, (size_t)d * sizeof(gor_real));
        pbfit[i] = f(X + (size_t)i * (size_t)d, d, ctx);
        if (pbfit[i] < gbfit) { gbfit = pbfit[i]; memcpy(gb, X + (size_t)i * (size_t)d, (size_t)d * sizeof(gor_real)); }
    }
    for (it = 0; it < maxit; it++) {
        for (i = 0; i < swarm; i++) {
            for (k = 0; k < d; k++) {
                gor_real r1 = urand(&s), r2 = urand(&s);
                V[(size_t)i * (size_t)d + (size_t)k] = 0.729 * V[(size_t)i * (size_t)d + (size_t)k]
                    + 1.494 * r1 * (pb[(size_t)i * (size_t)d + (size_t)k] - X[(size_t)i * (size_t)d + (size_t)k])
                    + 1.494 * r2 * (gb[k] - X[(size_t)i * (size_t)d + (size_t)k]);
                X[(size_t)i * (size_t)d + (size_t)k] += V[(size_t)i * (size_t)d + (size_t)k];
            }
            clamp_vec(X + (size_t)i * (size_t)d, d, lo, hi);
            {
                gor_real v = f(X + (size_t)i * (size_t)d, d, ctx);
                if (v < pbfit[i]) { pbfit[i] = v; memcpy(pb + (size_t)i * (size_t)d, X + (size_t)i * (size_t)d, (size_t)d * sizeof(gor_real)); }
                if (v < gbfit) { gbfit = v; memcpy(gb, X + (size_t)i * (size_t)d, (size_t)d * sizeof(gor_real)); }
            }
        }
    }
    memcpy(x, gb, (size_t)d * sizeof(gor_real));
    free(X); free(V); free(pb); free(pbfit); free(gb);
    return gbfit;
}

gor_real gor_meta_de(gor_meta_fn f, void *ctx, gor_real *x, int d,
                     const gor_real *lo, const gor_real *hi, int pop, int maxgen, unsigned long seed)
{
    unsigned long s = seed ? seed : 1;
    gor_real *P, *fit, *trial, *bestx;
    int i, k, g;
    gor_real best = 1e300;
    if (f == NULL || x == NULL || d <= 0) return 0.0;
    if (pop < 4) pop = 20;
    P = (gor_real *)malloc((size_t)pop * (size_t)d * sizeof(gor_real));
    fit = (gor_real *)malloc((size_t)pop * sizeof(gor_real));
    trial = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    bestx = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    if (!P || !fit || !trial || !bestx) { free(P); free(fit); free(trial); free(bestx); return 0.0; }
    for (i = 0; i < pop; i++)
        for (k = 0; k < d; k++) P[(size_t)i * (size_t)d + (size_t)k] = lo[k] + urand(&s) * (hi[k] - lo[k]);
    for (i = 0; i < pop; i++) fit[i] = f(P + (size_t)i * (size_t)d, d, ctx);
    for (g = 0; g < maxgen; g++) {
        for (i = 0; i < pop; i++) {
            int a = (int)(mrng(&s) % (unsigned long)pop);
            int b = (int)(mrng(&s) % (unsigned long)pop);
            int c = (int)(mrng(&s) % (unsigned long)pop);
            int jr = (int)(mrng(&s) % (unsigned long)d);
            gor_real tv;
            for (k = 0; k < d; k++) {
                if (urand(&s) < 0.9 || k == jr)
                    trial[k] = P[(size_t)a * (size_t)d + (size_t)k] + 0.8 * (P[(size_t)b * (size_t)d + (size_t)k] - P[(size_t)c * (size_t)d + (size_t)k]);
                else
                    trial[k] = P[(size_t)i * (size_t)d + (size_t)k];
            }
            clamp_vec(trial, d, lo, hi);
            tv = f(trial, d, ctx);
            if (tv < fit[i]) { fit[i] = tv; memcpy(P + (size_t)i * (size_t)d, trial, (size_t)d * sizeof(gor_real)); }
        }
        for (i = 0; i < pop; i++) if (fit[i] < best) { best = fit[i]; memcpy(bestx, P + (size_t)i * (size_t)d, (size_t)d * sizeof(gor_real)); }
    }
    memcpy(x, bestx, (size_t)d * sizeof(gor_real));
    free(P); free(fit); free(trial); free(bestx);
    return best;
}

gor_real gor_meta_aco_tsp(int n, const gor_real *dist, int *tour, int n_ants, int maxit, unsigned long seed)
{
    unsigned long s = seed ? seed : 1;
    gor_real *pheromone, best = 1e300;
    int i, it, a, k;
    if (n <= 1 || dist == NULL || tour == NULL) return 0.0;
    if (n_ants < 1) n_ants = n;
    pheromone = (gor_real *)malloc((size_t)n * (size_t)n * sizeof(gor_real));
    if (!pheromone) return 0.0;
    for (i = 0; i < n * n; i++) pheromone[i] = 1.0;
    for (i = 0; i < n; i++) tour[i] = i;
    for (it = 0; it < maxit; it++) {
        for (a = 0; a < n_ants; a++) {
            int *t = (int *)malloc((size_t)n * sizeof(int));
            char *vis = (char *)calloc((size_t)n, 1);
            gor_real len = 0.0;
            int cur = (int)(mrng(&s) % (unsigned long)n), step;
            if (!t || !vis) { free(t); free(vis); break; }
            t[0] = cur; vis[cur] = 1;
            for (step = 1; step < n; step++) {
                gor_real *prob = (gor_real *)malloc((size_t)n * sizeof(gor_real));
                gor_real tot = 0.0;
                int nxt = -1;
                if (!prob) break;
                for (k = 0; k < n; k++) {
                    if (vis[k]) { prob[k] = 0.0; continue; }
                    prob[k] = pow(pheromone[(size_t)cur * (size_t)n + (size_t)k], 1.0) * pow(1.0 / (dist[(size_t)cur * (size_t)n + (size_t)k] + 1e-9), 2.0);
                    tot += prob[k];
                }
                {
                    gor_real r = urand(&s) * tot, acc = 0.0;
                    for (k = 0; k < n; k++) { acc += prob[k]; if (acc >= r) { nxt = k; break; } }
                    if (nxt < 0) for (k = 0; k < n; k++) if (!vis[k]) { nxt = k; break; }
                }
                t[step] = nxt; vis[nxt] = 1;
                len += dist[(size_t)cur * (size_t)n + (size_t)nxt];
                cur = nxt;
                free(prob);
            }
            len += dist[(size_t)cur * (size_t)n + (size_t)t[0]];
            if (len < best) { best = len; memcpy(tour, t, (size_t)n * sizeof(int)); }
            /* deposito feromone */
            for (step = 0; step < n; step++) {
                int u = t[step], v = t[(step + 1) % n];
                pheromone[(size_t)u * (size_t)n + (size_t)v] += 1.0 / (len + 1e-9);
                pheromone[(size_t)v * (size_t)n + (size_t)u] += 1.0 / (len + 1e-9);
            }
            free(t); free(vis);
        }
        /* evaporazione */
        for (i = 0; i < n * n; i++) pheromone[i] *= 0.9;
    }
    free(pheromone);
    return best;
}

/* ============================== NSGA-II (2 obiettivi) ============================== */

static int nsga_dominates(double a1, double a2, double b1, double b2)
{
    return (a1 <= b1 && a2 <= b2) && (a1 < b1 || a2 < b2);
}

int gor_meta_nsga2(gor_meta_fn f1, void *ctx1, gor_meta_fn f2, void *ctx2,
                   int d, const gor_real *lo, const gor_real *hi,
                   int pop, int maxgen, unsigned long seed,
                   gor_real *pareto, gor_real *fv1, gor_real *fv2)
{
    unsigned long s = seed ? seed : 1;
    double *X, *F1, *F2, *NP;
    int i, j, k, g;
    if (f1 == NULL || f2 == NULL || d <= 0 || pop < 4) return 0;
    X = (double *)malloc((size_t)pop * (size_t)d * sizeof(double));
    NP = (double *)malloc((size_t)pop * (size_t)d * sizeof(double));
    F1 = (double *)malloc((size_t)pop * sizeof(double));
    F2 = (double *)malloc((size_t)pop * sizeof(double));
    if (!X || !NP || !F1 || !F2) { free(X); free(NP); free(F1); free(F2); return 0; }
    for (i = 0; i < pop; i++)
        for (k = 0; k < d; k++) X[(size_t)i * (size_t)d + (size_t)k] = lo[k] + urand(&s) * (hi[k] - lo[k]);
    for (i = 0; i < pop; i++) { F1[i] = f1(X + (size_t)i * (size_t)d, d, ctx1); F2[i] = f2(X + (size_t)i * (size_t)d, d, ctx2); }
    for (g = 0; g < maxgen; g++) {
        int *rank = (int *)malloc((size_t)pop * sizeof(int));
        double *crowd = (double *)malloc((size_t)pop * sizeof(double));
        if (!rank || !crowd) { free(rank); free(crowd); break; }
        /* non-dominated sort (semplice: rank = #di soluzioni che dominano i) */
        for (i = 0; i < pop; i++) {
            rank[i] = 0;
            for (j = 0; j < pop; j++) if (i != j && nsga_dominates(F1[j], F2[j], F1[i], F2[i])) rank[i]++;
            crowd[i] = 0.0;
        }
        /* crowding distance per front */
        for (j = 0; j < pop; j++) {
            /* per ogni front, calcola distanza (approssimata su tutto il rank) */
        }
        {
            int *ord = (int *)malloc((size_t)pop * sizeof(int));
            int c;
            if (ord) {
                for (k = 0; k < 2; k++) {
                    for (i = 0; i < pop; i++) ord[i] = i;
                    for (i = 0; i < pop; i++)
                        for (j = i + 1; j < pop; j++) {
                            double vi = (k == 0) ? F1[ord[i]] : F2[ord[i]];
                            double vj = (k == 0) ? F1[ord[j]] : F2[ord[j]];
                            if (vj < vi) { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
                        }
                    for (c = 1; c < pop - 1; c++) {
                        double a = (k == 0) ? F1[ord[c + 1]] : F2[ord[c + 1]];
                        double b = (k == 0) ? F1[ord[c - 1]] : F2[ord[c - 1]];
                        crowd[ord[c]] += a - b;
                    }
                    crowd[ord[0]] += 1e12; crowd[ord[pop - 1]] += 1e12;
                }
                free(ord);
            }
        }
        /* selezione (tournament su rank poi crowding) + crossover + mutazione */
        for (i = 0; i < pop; i++) {
            int a = (int)(mrng(&s) % (unsigned long)pop), b = (int)(mrng(&s) % (unsigned long)pop);
            int p1 = (rank[a] < rank[b]) ? a : ((rank[b] < rank[a]) ? b : ((crowd[a] > crowd[b]) ? a : b));
            int c1 = (int)(mrng(&s) % (unsigned long)pop), c2 = (int)(mrng(&s) % (unsigned long)pop);
            int p2 = (rank[c1] < rank[c2]) ? c1 : ((rank[c2] < rank[c1]) ? c2 : ((crowd[c1] > crowd[c2]) ? c1 : c2));
            for (k = 0; k < d; k++) {
                double alpha = -0.1 + urand(&s) * 1.2;
                double c = X[(size_t)p1 * (size_t)d + (size_t)k] + alpha * (X[(size_t)p2 * (size_t)d + (size_t)k] - X[(size_t)p1 * (size_t)d + (size_t)k]);
                if (urand(&s) < 0.1) c += grand(&s) * (hi[k] - lo[k]) * 0.1;
                NP[(size_t)i * (size_t)d + (size_t)k] = c;
            }
            clamp_vec(NP + (size_t)i * (size_t)d, d, lo, hi);
        }
        memcpy(X, NP, (size_t)pop * (size_t)d * sizeof(double));
        for (i = 0; i < pop; i++) { F1[i] = f1(X + (size_t)i * (size_t)d, d, ctx1); F2[i] = f2(X + (size_t)i * (size_t)d, d, ctx2); }
        free(rank); free(crowd);
    }
    /* front di Pareto = rank 0 */
    {
        int nf = 0;
        for (i = 0; i < pop; i++) {
            int dominated = 0;
            for (j = 0; j < pop; j++) if (i != j && nsga_dominates(F1[j], F2[j], F1[i], F2[i])) { dominated = 1; break; }
            if (!dominated && pareto) {
                memcpy(pareto + (size_t)nf * (size_t)d, X + (size_t)i * (size_t)d, (size_t)d * sizeof(double));
                if (fv1) fv1[nf] = F1[i];
                if (fv2) fv2[nf] = F2[i];
                nf++;
            }
        }
        free(X); free(NP); free(F1); free(F2);
        return nf;
    }
}

/* ===================== Schedulazioni di tenure tabu (da OR2) ===================== */
#define GOR_TENURE_DENOM 10
#define GOR_MIN_TENURE   5

int gor_tabu_tenure(int kind, int it, int n)
{
    int base = n / GOR_TENURE_DENOM;
    if (base < GOR_MIN_TENURE) base = GOR_MIN_TENURE;
    if (kind <= 0) return base;
    {
        int max_tenure = base + GOR_MIN_TENURE;
        int h = max_tenure - GOR_MIN_TENURE;   /* ampiezza */
        int win, val;
        if (h < 1) h = 1;
        win = it / h; val = it % h;
        switch (kind) {
        case 1:  /* triangolare */
            return GOR_MIN_TENURE + ((win % 2) ? (h - val) : val);
        case 2:  /* quadrata */
            return (win % 2) ? max_tenure : GOR_MIN_TENURE;
        case 3:  /* sawtooth */
            return GOR_MIN_TENURE + (it % (3 * h)) / 3;
        default:
            return base;
        }
    }
}

/* ===================== Evolution Strategies e ARS (da sieknet) ===================== */

static gor_real es_randn(unsigned long long *s)
{
    /* Box-Muller da due uniformi [0,1) */
    gor_real u1, u2;
    *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
    u1 = (gor_real)(*s >> 11) / 9007199254740992.0;
    *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
    u2 = (gor_real)(*s >> 11) / 9007199254740992.0;
    if (u1 < 1e-12) u1 = 1e-12;
    return sqrt(-2.0 * log(u1)) * cos(6.28318530717958647692 * u2);
}

gor_real gor_meta_es(gor_meta_fn f, void *ctx, gor_real *x, int d,
                     gor_real sigma, gor_real alpha, int pop, int maxit, unsigned long long seed)
{
    gor_real *delta, *xp, *best_x, best = 1e300;
    unsigned long long s = seed ? seed : 12345ULL;
    int i, j, it;
    if (!f || !x || d < 1) return 0.0;
    if (pop < 1) pop = 10;
    if (maxit < 1) maxit = 100;
    delta = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    xp = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    best_x = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    if (!delta || !xp || !best_x) { free(delta); free(xp); free(best_x); return 0.0; }
    for (i = 0; i < d; i++) best_x[i] = x[i];
    { gor_real v = f(x, d, ctx); if (v < best) best = v; }
    for (it = 0; it < maxit; it++) {
        gor_real *grad = (gor_real *)calloc((size_t)d, sizeof(gor_real));
        if (!grad) break;
        for (i = 0; i < pop; i++) {
            gor_real rp, rn;
            for (j = 0; j < d; j++) { delta[j] = sigma * es_randn(&s); xp[j] = x[j] + delta[j]; }
            rp = f(xp, d, ctx);
            for (j = 0; j < d; j++) xp[j] = x[j] - delta[j];
            rn = f(xp, d, ctx);
            for (j = 0; j < d; j++) grad[j] += (rp - rn) * delta[j];
        }
        for (j = 0; j < d; j++) x[j] -= (alpha / ((gor_real)pop * sigma)) * grad[j];
        { gor_real v = f(x, d, ctx); if (v < best) { best = v; for (j = 0; j < d; j++) best_x[j] = x[j]; } }
        free(grad);
    }
    for (i = 0; i < d; i++) x[i] = best_x[i];
    free(delta); free(xp); free(best_x);
    return best;
}

gor_real gor_meta_ars(gor_meta_fn f, void *ctx, gor_real *x, int d,
                      gor_real sigma, gor_real alpha, int top_b, int pop, int maxit,
                      unsigned long long seed)
{
    gor_real *delta, *xp, *best_x, best = 1e300;
    gor_real *rewards;
    unsigned long long s = seed ? seed : 12345ULL;
    int i, j, it;
    if (!f || !x || d < 1) return 0.0;
    if (pop < 1) pop = 10;
    if (top_b < 1 || top_b > pop) top_b = pop;
    if (maxit < 1) maxit = 100;
    delta = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    xp = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    best_x = (gor_real *)malloc((size_t)d * sizeof(gor_real));
    rewards = (gor_real *)malloc((size_t)pop * sizeof(gor_real));
    if (!delta || !xp || !best_x || !rewards) { free(delta); free(xp); free(best_x); free(rewards); return 0.0; }
    for (i = 0; i < d; i++) best_x[i] = x[i];
    { gor_real v = f(x, d, ctx); if (v < best) best = v; }
    for (it = 0; it < maxit; it++) {
        gor_real *deltas = (gor_real *)malloc((size_t)pop * d * sizeof(gor_real));
        gor_real *grad = (gor_real *)calloc((size_t)d, sizeof(gor_real));
        int *ord = (int *)malloc((size_t)pop * sizeof(int));
        if (!deltas || !grad || !ord) { free(deltas); free(grad); free(ord); break; }
        for (i = 0; i < pop; i++) {
            gor_real rp, rn;
            for (j = 0; j < d; j++) { deltas[(size_t)i * d + j] = sigma * es_randn(&s); xp[j] = x[j] + deltas[(size_t)i * d + j]; }
            rp = f(xp, d, ctx);
            for (j = 0; j < d; j++) xp[j] = x[j] - deltas[(size_t)i * d + j];
            rn = f(xp, d, ctx);
            rewards[i] = rp - rn;
        }
        /* dev.std dei reward (0 se tutti uguali) */
        { gor_real mean = 0.0, var = 0.0, sd;
          for (i = 0; i < pop; i++) mean += rewards[i];
          mean /= (gor_real)pop;
          for (i = 0; i < pop; i++) var += (rewards[i] - mean) * (rewards[i] - mean);
          var /= (gor_real)pop;
          sd = sqrt(var);
          if (sd < 1e-9) sd = 1.0;
          /* top_b per |reward| */
          for (i = 0; i < pop; i++) ord[i] = i;
          for (i = 1; i < pop; i++) { int key = ord[i], q = i - 1;
              while (q >= 0 && fabs(rewards[ord[q]]) < fabs(rewards[key])) { ord[q+1] = ord[q]; q--; }
              ord[q+1] = key; }
          for (i = 0; i < top_b; i++) {
              int k = ord[i];
              for (j = 0; j < d; j++) grad[j] += rewards[k] * deltas[(size_t)k * d + j];
          }
          for (j = 0; j < d; j++) x[j] -= (alpha / ((gor_real)top_b * sd)) * grad[j];
        }
        { gor_real v = f(x, d, ctx); if (v < best) { best = v; for (j = 0; j < d; j++) best_x[j] = x[j]; } }
        free(deltas); free(grad); free(ord);
    }
    for (i = 0; i < d; i++) x[i] = best_x[i];
    free(delta); free(xp); free(best_x); free(rewards);
    return best;
}
