/*
 * gor_meta.h — meta-euristiche e ottimizzazione globale.
 *
 * Hill climbing, simulated annealing, tabu, algoritmi genetici, PSO,
 * differential evolution, ant colony (TSP). Funzione obiettivo generica.
 */
#ifndef GMBORTOOLS_GOR_META_H
#define GMBORTOOLS_GOR_META_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* f(x, d, ctx) da minimizzare. x in [lo, hi]^d. */
typedef gor_real (*gor_meta_fn)(const gor_real *x, int d, void *ctx);

gor_real gor_meta_hill_climb(gor_meta_fn f, void *ctx, gor_real *x, int d,
                             const gor_real *lo, const gor_real *hi, int maxit, unsigned long seed);
gor_real gor_meta_sa(gor_meta_fn f, void *ctx, gor_real *x, int d,
                     const gor_real *lo, const gor_real *hi, int maxit,
                     gor_real t0, gor_real alpha, unsigned long seed);
gor_real gor_meta_tabu(gor_meta_fn f, void *ctx, gor_real *x, int d,
                       const gor_real *lo, const gor_real *hi, int maxit, int tenure, unsigned long seed);
gor_real gor_meta_ga(gor_meta_fn f, void *ctx, gor_real *x, int d,
                     const gor_real *lo, const gor_real *hi, int pop, int maxgen, unsigned long seed);
gor_real gor_meta_pso(gor_meta_fn f, void *ctx, gor_real *x, int d,
                      const gor_real *lo, const gor_real *hi, int swarm, int maxit, unsigned long seed);
gor_real gor_meta_de(gor_meta_fn f, void *ctx, gor_real *x, int d,
                     const gor_real *lo, const gor_real *hi, int pop, int maxgen, unsigned long seed);

/* CMA-ES (clean-room da JQSakaJoo/Population-optimization-algorithms-MQL5, BSD-3): (mu/mu_w,lambda)-ES,
 * path evolution, rank-one+rank-mu, eigen Jacobi, clamp al box. maxit = generazioni. Ritorna il best. */
gor_real gor_meta_cmaes(gor_meta_fn f, void *ctx, gor_real *x, int d,
                        const gor_real *lo, const gor_real *hi, int maxit, unsigned long seed);

/* Schedulazione della tenure tabu (da danielcharles0/OR2, formule standard).
 * kind: 0=costante, 1=triangolare, 2=quadrata, 3=sawtooth.
 * it = iterazione, n = dimensione istanza. Ritorna la tenure. */
int gor_tabu_tenure(int kind, int it, int n);

/* Evolution Strategies (da siekmanj/sieknet): perturbazioni gaussiane,
 * aggiornamento theta += alpha/(N*sigma) * sum (R(+d)-R(-d)) * d.
 * pop = N perturbazioni per iterazione. Ritorna il miglior valore trovato. */
gor_real gor_meta_es(gor_meta_fn f, void *ctx, gor_real *x, int d,
                     gor_real sigma, gor_real alpha, int pop, int maxit, unsigned long long seed);
/* Augmented Random Search (ARS): come ES ma scala per la dev.std dei reward e usa
 * solo le top_b direzioni. Ritorna il miglior valore trovato. */
gor_real gor_meta_ars(gor_meta_fn f, void *ctx, gor_real *x, int d,
                      gor_real sigma, gor_real alpha, int top_b, int pop, int maxit,
                      unsigned long long seed);

/* ACO per TSP: dist n×n. tour[n]. Ritorna la lunghezza. */
gor_real gor_meta_aco_tsp(int n, const gor_real *dist, int *tour, int n_ants, int maxit, unsigned long seed);

/* NSGA-II (2 obiettivi, minimizzazione). Ritorna il numero di soluzioni del front
 * di Pareto; pareto[k*d] = soluzione k, fv1[k]/fv2[k] = obiettivi. */
int gor_meta_nsga2(gor_meta_fn f1, void *ctx1, gor_meta_fn f2, void *ctx2,
                   int d, const gor_real *lo, const gor_real *hi,
                   int pop, int maxgen, unsigned long seed,
                   gor_real *pareto, gor_real *fv1, gor_real *fv2);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_META_H */
