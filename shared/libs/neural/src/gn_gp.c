/* gn_gp.c — Genetic Programming generico (alberi in ordine prefisso). */
#include "gn_gp.h"

#include <stdlib.h>
#include <string.h>

gn_gp_rng gn_gp_rng_make(unsigned long long seed)
{
    gn_gp_rng r;
    r.s = seed ? seed : 0x9e3779b97f4a7c15ULL;
    return r;
}

int gn_gp_rand_int(gn_gp_rng *r, int n)
{
    if (n <= 1) return 0;
    r->s = r->s * 6364136223846793005ULL + 1442695040888963407ULL;
    return (int)((r->s >> 33) % (unsigned long long)n);
}

double gn_gp_rand_uniform(gn_gp_rng *r)
{
    r->s = r->s * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(r->s >> 11) / 9007199254740992.0;
}

static void gp_build(const gn_gp_spec *s, void *ctx, gn_gp_rng *r, gn_gp_genome *g,
                     int *pos, int depth, int maxd, int grow)
{
    int p, code, isfunc;
    if (*pos >= GN_GP_MAXN) return;
    p = (*pos)++;
    if (depth >= maxd) {
        do { code = s->random_code(r, ctx); } while (s->arity(code) > 0);
        isfunc = 0;
    } else if (!grow) {
        do { code = s->random_code(r, ctx); } while (s->arity(code) == 0);
        isfunc = 1;
    } else {
        code = s->random_code(r, ctx);
        isfunc = s->arity(code) > 0;
    }
    g->code[p] = code;
    if (isfunc && *pos < GN_GP_MAXN) {
        gp_build(s, ctx, r, g, pos, depth + 1, maxd, grow);
        if (s->arity(code) == 2) gp_build(s, ctx, r, g, pos, depth + 1, maxd, grow);
    }
}

static int gp_sub_size(const gn_gp_spec *s, const gn_gp_genome *g, int pos)
{
    int need = 1, i = pos;
    while (need > 0 && i < g->n) { need += s->arity(g->code[i]) - 1; i++; }
    return i - pos;
}

static int gp_tournament(const double *fit, int pop, gn_gp_rng *r, int k)
{
    int best = gn_gp_rand_int(r, pop), i;
    for (i = 1; i < k; i++) { int c = gn_gp_rand_int(r, pop); if (fit[c] > fit[best]) best = c; }
    return best;
}

static void gp_crossover(const gn_gp_spec *s, const gn_gp_genome *a, const gn_gp_genome *b,
                         gn_gp_genome *ch, gn_gp_rng *r)
{
    int pa = gn_gp_rand_int(r, a->n), pb = gn_gp_rand_int(r, b->n);
    int sa = gp_sub_size(s, a, pa), sb = gp_sub_size(s, b, pb);
    int tail = a->n - pa - sa, nc = pa + sb + tail, i;
    if (nc > GN_GP_MAXN || sb < 1) { *ch = *a; return; }
    ch->n = nc;
    for (i = 0; i < pa; i++) ch->code[i] = a->code[i];
    for (i = 0; i < sb; i++) ch->code[pa + i] = b->code[pb + i];
    for (i = 0; i < tail; i++) ch->code[pa + sb + i] = a->code[pa + sa + i];
}

static void gp_mutate(const gn_gp_spec *s, void *ctx, gn_gp_genome *g, gn_gp_rng *r, int maxd)
{
    int p = gn_gp_rand_int(r, g->n);
    int sz = gp_sub_size(s, g, p), nc, i;
    gn_gp_genome sub;
    int pos = 0;
    sub.n = 0;
    gp_build(s, ctx, r, &sub, &pos, 0, maxd, 1);
    sub.n = pos;
    nc = g->n - sz + sub.n;
    if (nc > GN_GP_MAXN || sub.n < 1) return;
    for (i = g->n - p - sz - 1; i >= 0; i--) g->code[p + sub.n + i] = g->code[p + sz + i];
    for (i = 0; i < sub.n; i++) g->code[p + i] = sub.code[i];
    g->n = nc;
}

double gn_gp_eval(const gn_gp_spec *spec, const gn_gp_genome *g, void *ctx)
{
    return spec->fitness(g, ctx);
}

int gn_gp_evolve(const gn_gp_spec *spec, void *ctx, int pop, int gens,
                 unsigned long long seed, int max_depth,
                 gn_gp_genome *out_best, double *out_fit)
{
    gn_gp_rng r;
    gn_gp_genome *P, *nx;
    double *fit;
    int i, g, b1, b2;
    if (!spec || !spec->arity || !spec->random_code || !spec->fitness
        || !out_best || pop < 2 || gens < 1) return -1;
    if (max_depth < 1) max_depth = 1;
    if (max_depth > 5) max_depth = 5;
    r = gn_gp_rng_make(seed);
    P = (gn_gp_genome *)malloc((size_t)pop * sizeof(gn_gp_genome));
    nx = (gn_gp_genome *)malloc((size_t)pop * sizeof(gn_gp_genome));
    fit = (double *)malloc((size_t)pop * sizeof(double));
    if (!P || !nx || !fit) { free(P); free(nx); free(fit); return -1; }
    for (i = 0; i < pop; i++) {
        int pos = 0;
        P[i].n = 0;
        gp_build(spec, ctx, &r, &P[i], &pos, 0, max_depth, i % 2);
        P[i].n = pos;
        fit[i] = spec->fitness(&P[i], ctx);
    }
    for (g = 0; g < gens; g++) {
        b1 = 0; for (i = 1; i < pop; i++) if (fit[i] > fit[b1]) b1 = i;
        b2 = (b1 == 0) ? 1 : 0; for (i = 0; i < pop; i++) if (i != b1 && fit[i] > fit[b2]) b2 = i;
        nx[0] = P[b1]; nx[1] = P[b2];
        for (i = 2; i < pop; i++) {
            int pa = gp_tournament(fit, pop, &r, 4), pb = gp_tournament(fit, pop, &r, 4);
            gn_gp_genome ch;
            if (gn_gp_rand_uniform(&r) < 0.9) gp_crossover(spec, &P[pa], &P[pb], &ch, &r);
            else ch = P[pa];
            if (gn_gp_rand_uniform(&r) < 0.1) gp_mutate(spec, ctx, &ch, &r, 3);
            if (ch.n < 1 || ch.n > GN_GP_MAXN) ch = P[pa];
            nx[i] = ch;
        }
        memcpy(P, nx, (size_t)pop * sizeof(gn_gp_genome));
        for (i = 0; i < pop; i++) fit[i] = spec->fitness(&P[i], ctx);
    }
    b1 = 0; for (i = 1; i < pop; i++) if (fit[i] > fit[b1]) b1 = i;
    *out_best = P[b1];
    if (out_fit) *out_fit = fit[b1];
    free(P); free(nx); free(fit);
    return 0;
}
