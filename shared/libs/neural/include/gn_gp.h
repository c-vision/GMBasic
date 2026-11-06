/*
 * gn_gp.h — Genetic Programming generico (alberi in ordine prefisso).
 *
 * Genoma = array di codici interi (semantica definita dal chiamante). Il motore
 * non conosce operatori/terminali: li fornisce la spec (arity + random_code) e
 * valuta con un callback di fitness. Deterministico a parità di seed.
 */
#ifndef GMBNEURAL_GN_GP_H
#define GMBNEURAL_GN_GP_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GN_GP_MAXN 63

typedef struct { int n; int code[GN_GP_MAXN]; } gn_gp_genome;

typedef struct gn_gp_rng { unsigned long long s; } gn_gp_rng;

/* RNG deterministico (LCG) per init/operatori. */
gn_gp_rng gn_gp_rng_make(unsigned long long seed);
int gn_gp_rand_int(gn_gp_rng *r, int n);
double gn_gp_rand_uniform(gn_gp_rng *r);

typedef struct {
    /* arità del codice: 1 o 2 se funzione, 0 se terminale. */
    int (*arity)(int code);
    /* genera un codice casuale (funzione o terminale). */
    int (*random_code)(gn_gp_rng *r, void *ctx);
    /* valuta l'albero sul contesto e ritorna la fitness (da massimizzare). */
    double (*fitness)(const gn_gp_genome *g, void *ctx);
} gn_gp_spec;

/* Evolve. pop/gens >= 1; max_depth in [1, ~5]; elitismo 2, tournament 4,
 * crossover 0.9, mutazione 0.1. Best in out_best, fitness in out_fit. 0/-1. */
int gn_gp_evolve(const gn_gp_spec *spec, void *ctx, int pop, int gens,
                 unsigned long long seed, int max_depth,
                 gn_gp_genome *out_best, double *out_fit);

/* Fitness di un genoma gia' formato (utile per out-of-sample). */
double gn_gp_eval(const gn_gp_spec *spec, const gn_gp_genome *g, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_GP_H */
