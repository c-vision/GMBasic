/* gor_ls.c — punteggio, RNG, terminazioni, acceptors, costruttive. */
#include "gor_ls.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------------- */
/* Punteggio                                                                  */
/* ------------------------------------------------------------------------- */

int gor_ls_score_cmp(gor_ls_score a, gor_ls_score b)
{
    if (a.hard != b.hard) return (a.hard > b.hard) ? 1 : -1;
    if (a.soft > b.soft) return 1;
    if (a.soft < b.soft) return -1;
    return 0;
}

int gor_ls_score_feasible(gor_ls_score s) { return s.hard >= 0; }

gor_ls_score gor_ls_score_add(gor_ls_score a, gor_ls_score b)
{
    gor_ls_score r;
    r.hard = a.hard + b.hard;
    r.soft = a.soft + b.soft;
    return r;
}

/* ------------------------------------------------------------------------- */
/* RNG                                                                        */
/* ------------------------------------------------------------------------- */

void gor_ls_rng_seed(gor_ls_rng *r, unsigned long long seed)
{
    r->state = seed ? seed : 0x9E3779B97F4A7C15ULL;
}

unsigned long long gor_ls_rng_next(gor_ls_rng *r)
{
    unsigned long long x = r->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

int gor_ls_rng_below(gor_ls_rng *r, int n)
{
    if (n <= 0) return 0;
    return (int)(gor_ls_rng_next(r) % (unsigned long long)n);
}

gor_real gor_ls_rng_unit(gor_ls_rng *r)
{
    return (gor_real)(gor_ls_rng_next(r) >> 11) / 9007199254740992.0;
}

long long gor_ls_now_ms(void)
{
    return (long long)((double)clock() * 1000.0 / (double)CLOCKS_PER_SEC);
}

/* ------------------------------------------------------------------------- */
/* Terminazioni                                                               */
/* ------------------------------------------------------------------------- */

void gor_ls_termination_init(gor_ls_termination_state *st,
                             const gor_ls_termination *cfg,
                             gor_ls_score start_best)
{
    st->cfg = *cfg;
    st->start_ms = gor_ls_now_ms();
    st->last_improve_ms = st->start_ms;
    st->steps = 0;
    st->last_improve_step = 0;
    st->best = start_best;
}

int gor_ls_termination_done(gor_ls_termination_state *st, long long now_ms,
                            gor_ls_score current_best)
{
    const gor_ls_termination *c = &st->cfg;

    st->steps++;
    if (gor_ls_score_cmp(current_best, st->best) > 0) {
        st->best = current_best;
        st->last_improve_ms = now_ms;
        st->last_improve_step = st->steps;
    }

    if (c->spent_ms > 0 && now_ms - st->start_ms >= c->spent_ms) return 1;
    if (c->unimproved_ms > 0 && now_ms - st->last_improve_ms >= c->unimproved_ms) return 1;
    if (c->step_count > 0 && st->steps >= c->step_count) return 1;
    if (c->unimproved_step_count > 0 &&
        st->steps - st->last_improve_step >= c->unimproved_step_count) return 1;
    if (c->has_best_score_limit &&
        gor_ls_score_cmp(current_best, c->best_score_limit) >= 0) return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Acceptors                                                                  */
/* ------------------------------------------------------------------------- */

void gor_ls_acceptor_init(gor_ls_acceptor *a, gor_ls_acceptor_kind kind,
                          gor_ls_score start, unsigned long long seed)
{
    int i;

    memset(a, 0, sizeof(*a));
    gor_ls_rng_seed(&a->rng, seed);
    a->kind = kind;

    a->la_size = GOR_LS_LA_SIZE;
    for (i = 0; i < GOR_LS_LA_SIZE; i++) a->la_history[i] = start;
    a->la_pos = 0;

    a->tabu_size = 7;
    a->tabu_count = 0;
    a->tabu_pos = 0;

    a->sa_temperature = 1.0;
    a->sa_min_temperature = 1e-9;

    a->gd_water = start;
    a->gd_increment.hard = 0;
    a->gd_increment.soft = 1.0;

    a->schc_size = 400;
    a->schc_threshold = start;
    a->schc_step = 0;

    /* deviazione proporzionale alla magnitudine del soft iniziale. */
    a->rrt_deviation = fabs(start.soft) * 0.05;
    if (a->rrt_deviation <= 0.0) a->rrt_deviation = 1.0;
    a->rrt_compare_to_best = 1;
}

static int tabu_contains(const gor_ls_acceptor *a, int id)
{
    int i;
    for (i = 0; i < a->tabu_count; i++)
        if (a->tabu_ids[i] == id) return 1;
    return 0;
}

static void tabu_push(gor_ls_acceptor *a, int id)
{
    if (a->tabu_size <= 0) return;
    if (a->tabu_size > GOR_LS_TABU_SIZE) a->tabu_size = GOR_LS_TABU_SIZE;
    a->tabu_ids[a->tabu_pos % a->tabu_size] = id;
    a->tabu_pos++;
    if (a->tabu_count < a->tabu_size) a->tabu_count++;
}

int gor_ls_acceptor_accept(gor_ls_acceptor *a, gor_ls_score candidate,
                           gor_ls_score current, gor_ls_score best,
                           const int *move_entities, int n_entities,
                           double time_gradient)
{
    if (time_gradient < 0.0) time_gradient = 0.0;
    if (time_gradient > 1.0) time_gradient = 1.0;

    switch (a->kind) {
    case GOR_LS_ACCEPT_HILL_CLIMBING:
        return gor_ls_score_cmp(candidate, current) >= 0;

    case GOR_LS_ACCEPT_LATE_ACCEPTANCE: {
        int size = a->la_size;
        if (size <= 0) size = 1;
        if (size > GOR_LS_LA_SIZE) size = GOR_LS_LA_SIZE;
        return gor_ls_score_cmp(candidate, a->la_history[a->la_pos % size]) >= 0;
    }

    case GOR_LS_ACCEPT_ENTITY_TABU: {
        int i, is_tabu = 0;
        for (i = 0; i < n_entities; i++) {
            if (tabu_contains(a, move_entities[i])) { is_tabu = 1; break; }
        }
        /* aspirazione: una mossa tabu si accetta se batte il best. */
        if (is_tabu && gor_ls_score_cmp(candidate, best) <= 0) return 0;
        for (i = 0; i < n_entities; i++) tabu_push(a, move_entities[i]);
        return 1;
    }

    case GOR_LS_ACCEPT_SIMULATED_ANNEALING: {
        gor_real delta, t, u;
        if (gor_ls_score_cmp(candidate, current) >= 0) return 1;
        delta = (current.soft - candidate.soft) +
                1e9 * (gor_real)(current.hard - candidate.hard);
        if (delta <= 0.0) return 1;
        t = a->sa_temperature * (1.0 - time_gradient);
        if (t < a->sa_min_temperature) t = a->sa_min_temperature;
        if (t <= 0.0) return 0;
        u = gor_ls_rng_unit(&a->rng);
        return u < exp(-delta / t);
    }

    case GOR_LS_ACCEPT_GREAT_DELUGE:
        return gor_ls_score_cmp(candidate, a->gd_water) >= 0;

    case GOR_LS_ACCEPT_STEP_COUNTING_HILL_CLIMBING:
        return gor_ls_score_cmp(candidate, a->schc_threshold) >= 0;

    case GOR_LS_ACCEPT_RECORD_TO_RECORD: {
        gor_ls_score base = a->rrt_compare_to_best ? best : current;
        gor_real allowed = (1.0 - time_gradient) * a->rrt_deviation;
        if (candidate.hard != base.hard) return candidate.hard > base.hard;
        return candidate.soft >= base.soft - allowed;
    }

    default:
        return gor_ls_score_cmp(candidate, current) >= 0;
    }
}

void gor_ls_acceptor_step_ended(gor_ls_acceptor *a, gor_ls_score current)
{
    int size;

    switch (a->kind) {
    case GOR_LS_ACCEPT_LATE_ACCEPTANCE:
        size = a->la_size;
        if (size <= 0) size = 1;
        if (size > GOR_LS_LA_SIZE) size = GOR_LS_LA_SIZE;
        a->la_history[a->la_pos % size] = current;
        a->la_pos++;
        break;

    case GOR_LS_ACCEPT_GREAT_DELUGE:
        a->gd_water.hard -= a->gd_increment.hard;
        a->gd_water.soft -= a->gd_increment.soft;
        break;

    case GOR_LS_ACCEPT_STEP_COUNTING_HILL_CLIMBING:
        a->schc_step++;
        if (a->schc_size > 0 && a->schc_step >= a->schc_size) {
            a->schc_threshold = current;
            a->schc_step = 0;
        }
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* Costruttive                                                                */
/* ------------------------------------------------------------------------- */

int gor_ls_construct_nearest(int n, gor_ls_dist_fn dist, void *ctx, int start,
                             int *perm)
{
    int i, k, cur;
    char *used;

    if (n <= 0 || perm == NULL || dist == NULL) return GOR_ERR_ARG;
    used = (char *)calloc((size_t)n, sizeof(char));
    if (used == NULL) return GOR_ERR_NOMEM;

    cur = (start >= 0 && start < n) ? start : 0;
    perm[0] = cur;
    used[cur] = 1;
    for (k = 1; k < n; k++) {
        int best = -1;
        gor_real bestd = 0.0;
        for (i = 0; i < n; i++) {
            gor_real d;
            if (used[i]) continue;
            d = dist(cur, i, ctx);
            if (best < 0 || d < bestd) { best = i; bestd = d; }
        }
        perm[k] = best;
        used[best] = 1;
        cur = best;
    }
    free(used);
    return GOR_OK;
}

int gor_ls_construct_cheapest_insertion(int n, gor_ls_dist_fn dist, void *ctx,
                                        int *perm)
{
    int i, k, len;
    char *used;

    if (n <= 0 || perm == NULL || dist == NULL) return GOR_ERR_ARG;
    used = (char *)calloc((size_t)n, sizeof(char));
    if (used == NULL) return GOR_ERR_NOMEM;

    perm[0] = 0;
    used[0] = 1;
    len = 1;

    while (len < n) {
        int best_node = -1, best_pos = 0;
        gor_real best_delta = 0.0;
        for (i = 0; i < n; i++) {
            int pos;
            if (used[i]) continue;
            for (pos = 0; pos < len; pos++) {
                int a = perm[pos];
                int b = perm[(pos + 1) % len];
                gor_real delta;
                if (len == 1) {
                    delta = 2.0 * dist(a, i, ctx);
                } else {
                    delta = dist(a, i, ctx) + dist(i, b, ctx) - dist(a, b, ctx);
                }
                if (best_node < 0 || delta < best_delta) {
                    best_node = i;
                    best_pos = pos + 1;
                    best_delta = delta;
                }
            }
        }
        if (best_node < 0) break;
        for (k = len; k > best_pos; k--) perm[k] = perm[k - 1];
        perm[best_pos] = best_node;
        used[best_node] = 1;
        len++;
    }
    free(used);
    return GOR_OK;
}
