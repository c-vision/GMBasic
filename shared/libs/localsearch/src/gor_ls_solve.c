/* gor_ls_solve.c — ciclo di ricerca locale, mosse, nearby e ALNS. */
#include "gor_ls.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Configurazione                                                             */
/* ------------------------------------------------------------------------- */

void gor_ls_config_default(gor_ls_config *cfg, gor_ls_shape shape, int n,
                           gor_ls_eval_fn eval, void *ctx)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->shape = shape;
    cfg->n = n;
    cfg->value_count = 2;
    cfg->eval = eval;
    cfg->ctx = ctx;

    if (shape == GOR_LS_SHAPE_PERMUTATION) {
        cfg->w_relocate = 0.5;
        cfg->w_swap = 0.3;
        cfg->w_two_opt = 0.2;
        cfg->w_or_opt = 0.0;
    } else {
        cfg->w_relocate = 0.75; /* change */
        cfg->w_swap = 0.25;
        cfg->w_two_opt = 0.0;
        cfg->w_or_opt = 0.0;
    }
    cfg->or_opt_max = 3;

    cfg->nearby_k = 0;
    cfg->use_nearby = 0;

    cfg->alns = 0;
    cfg->alns_trigger = 5000;
    cfg->ruin_min_fraction = 0.10;
    cfg->ruin_max_fraction = 0.30;
    cfg->max_ruin = 60;
    cfg->polish_steps = 50;
    cfg->alns_adaptive = 1;

    gor_ls_acceptor_init(&cfg->acceptor, GOR_LS_ACCEPT_LATE_ACCEPTANCE,
                         (gor_ls_score){0, 0.0}, 1u);

    memset(&cfg->termination, 0, sizeof(cfg->termination));
    cfg->termination.spent_ms = 1000;

    cfg->starts = 1;
    cfg->seed = 1u;
}

/* ------------------------------------------------------------------------- */
/* Contesto interno                                                           */
/* ------------------------------------------------------------------------- */

typedef struct ls_ctx {
    const gor_ls_config *cfg;
    int n;
    int *cur;
    int *cand;
    int *best;
    int *scratch;
    int *near;            /* n * k, NULL se assente */
    int k;
    char *mark;
    gor_ls_rng rng;
    double ruin_w[3];     /* random, related, worst */
    int has_dist;
} ls_ctx;

static gor_ls_score ls_eval(const ls_ctx *c, const int *state)
{
    return c->cfg->eval(state, c->cfg->n, c->cfg->ctx);
}

static void copy_state(int *dst, const int *src, int n)
{
    memcpy(dst, src, (size_t)n * sizeof(int));
}

static int pos_of(const int *state, int n, int elem)
{
    int i;
    for (i = 0; i < n; i++) if (state[i] == elem) return i;
    return 0;
}

/* Liste dei k vicini (solo permutazione con dist). */
static void build_near(ls_ctx *c)
{
    int i, j, t;

    c->k = 0;
    c->near = NULL;
    if (!c->has_dist || c->cfg->shape != GOR_LS_SHAPE_PERMUTATION) return;

    c->k = c->cfg->nearby_k;
    if (c->k <= 0) return;
    if (c->k > GOR_LS_NEARBY_MAX) c->k = GOR_LS_NEARBY_MAX;
    if (c->k > c->n - 1) c->k = c->n - 1;
    if (c->k <= 0) return;

    c->near = (int *)malloc((size_t)c->n * (size_t)c->k * sizeof(int));
    if (c->near == NULL) { c->k = 0; return; }

    for (i = 0; i < c->n; i++) {
        int *row = c->near + (size_t)i * (size_t)c->k;
        for (j = 0; j < c->k; j++) row[j] = -1;
        for (j = 0; j < c->n; j++) {
            gor_real d;
            if (j == i) continue;
            d = c->cfg->dist(i, j, c->cfg->ctx);
            for (t = 0; t < c->k; t++) {
                if (row[t] < 0) { row[t] = j; break; }
                if (d < c->cfg->dist(i, row[t], c->cfg->ctx)) {
                    int u;
                    for (u = c->k - 1; u > t; u--) row[u] = row[u - 1];
                    row[t] = j;
                    break;
                }
            }
        }
    }
}

static int pick_near(ls_ctx *c, int elem)
{
    int *row;
    if (c->near == NULL || c->k <= 0) return -1;
    row = c->near + (size_t)elem * (size_t)c->k;
    return row[gor_ls_rng_below(&c->rng, c->k)];
}

/* ------------------------------------------------------------------------- */
/* Mosse (permutazione)                                                       */
/* ------------------------------------------------------------------------- */

static void move_relocate(ls_ctx *c, const int *from, int *to, int *touched, int *nt)
{
    int i = gor_ls_rng_below(&c->rng, c->n);
    int j;
    int elem, p;

    if (c->near != NULL) {
        int e = pick_near(c, from[i]);
        j = (e >= 0) ? pos_of(from, c->n, e) : gor_ls_rng_below(&c->rng, c->n);
    } else {
        j = gor_ls_rng_below(&c->rng, c->n);
    }

    copy_state(to, from, c->n);
    elem = from[i];
    if (j > i) {
        for (p = i; p < j; p++) to[p] = from[p + 1];
        to[j] = elem;
    } else if (j < i) {
        for (p = i; p > j; p--) to[p] = from[p - 1];
        to[j] = elem;
    }
    touched[0] = elem;
    *nt = 1;
}

static void move_swap(ls_ctx *c, const int *from, int *to, int *touched, int *nt)
{
    int i = gor_ls_rng_below(&c->rng, c->n);
    int j;

    if (c->near != NULL) {
        int e = pick_near(c, from[i]);
        j = (e >= 0) ? pos_of(from, c->n, e) : gor_ls_rng_below(&c->rng, c->n);
    } else {
        j = gor_ls_rng_below(&c->rng, c->n);
    }
    if (j == i) j = (i + 1) % c->n;

    copy_state(to, from, c->n);
    to[i] = from[j];
    to[j] = from[i];
    touched[0] = from[i];
    touched[1] = from[j];
    *nt = 2;
}

static void move_two_opt(ls_ctx *c, const int *from, int *to, int *touched, int *nt)
{
    int i = gor_ls_rng_below(&c->rng, c->n);
    int j = gor_ls_rng_below(&c->rng, c->n);
    int a, b, t;

    if (i > j) { t = i; i = j; j = t; }
    if (j - i < 2) {
        j = i + 2;
        if (j >= c->n) { j = c->n - 1; i = (j >= 2) ? j - 2 : 0; }
    }

    copy_state(to, from, c->n);
    a = i;
    b = j;
    while (a < b) {
        t = to[a];
        to[a] = to[b];
        to[b] = t;
        a++;
        b--;
    }
    touched[0] = from[i];
    touched[1] = from[j];
    *nt = 2;
}

static void move_or_opt(ls_ctx *c, const int *from, int *to, int *touched, int *nt)
{
    int maxl = c->cfg->or_opt_max;
    int len, i, j, p;
    int seg[8];

    if (maxl < 2) maxl = 2;
    if (maxl > 8) maxl = 8;
    if (maxl > c->n) maxl = c->n;
    len = 2 + gor_ls_rng_below(&c->rng, maxl - 1);
    i = gor_ls_rng_below(&c->rng, c->n);
    if (i + len > c->n) i = c->n - len;
    j = gor_ls_rng_below(&c->rng, c->n - len + 1); /* posizione nel compattato */
    if (j == i) j = (i + 1) % (c->n - len + 1);
    if (j == i) return;

    for (p = 0; p < len; p++) seg[p] = from[i + p];

    copy_state(to, from, c->n);
    for (p = i; p < c->n - len; p++) to[p] = from[p + len];
    for (p = c->n - 1; p >= j + len; p--) to[p] = to[p - len];
    for (p = 0; p < len; p++) to[j + p] = seg[p];

    for (p = 0; p < len; p++) touched[p] = seg[p];
    *nt = len;
}

/* ------------------------------------------------------------------------- */
/* Mosse (assegnazione)                                                       */
/* ------------------------------------------------------------------------- */

static void move_change(ls_ctx *c, const int *from, int *to, int *touched, int *nt)
{
    int e = gor_ls_rng_below(&c->rng, c->n);
    int v = gor_ls_rng_below(&c->rng, c->cfg->value_count);
    int old;

    copy_state(to, from, c->n);
    old = from[e];
    if (v == old && c->cfg->value_count > 1)
        v = (old + 1) % c->cfg->value_count;
    to[e] = v;
    touched[0] = e;
    *nt = 1;
}

static void move_assign_swap(ls_ctx *c, const int *from, int *to, int *touched, int *nt)
{
    int e1 = gor_ls_rng_below(&c->rng, c->n);
    int e2 = gor_ls_rng_below(&c->rng, c->n);
    int t;

    if (e2 == e1) e2 = (e1 + 1) % c->n;
    copy_state(to, from, c->n);
    t = to[e1];
    to[e1] = to[e2];
    to[e2] = t;
    touched[0] = e1;
    touched[1] = e2;
    *nt = 2;
}

/* Applica una mossa scelta per peso. Ritorna 0 se nessuna mossa è attiva. */
static int apply_move(ls_ctx *c, const int *from, int *to, int *touched, int *nt)
{
    const gor_ls_config *cfg = c->cfg;
    double total, r;
    int buf[16];
    int nbuf = 0;

    if (cfg->shape == GOR_LS_SHAPE_PERMUTATION)
        total = cfg->w_relocate + cfg->w_swap + cfg->w_two_opt + cfg->w_or_opt;
    else
        total = cfg->w_relocate + cfg->w_swap;
    if (total <= 0.0) return 0;

    r = gor_ls_rng_unit(&c->rng) * total;

    if (cfg->shape == GOR_LS_SHAPE_PERMUTATION) {
        if (r < cfg->w_relocate) {
            move_relocate(c, from, to, buf, &nbuf);
        } else if ((r -= cfg->w_relocate) < cfg->w_swap) {
            move_swap(c, from, to, buf, &nbuf);
        } else if ((r -= cfg->w_swap) < cfg->w_two_opt) {
            move_two_opt(c, from, to, buf, &nbuf);
        } else {
            move_or_opt(c, from, to, buf, &nbuf);
        }
    } else {
        if (r < cfg->w_relocate) move_change(c, from, to, buf, &nbuf);
        else move_assign_swap(c, from, to, buf, &nbuf);
    }

    memcpy(touched, buf, (size_t)nbuf * sizeof(int));
    *nt = nbuf;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* ALNS: ruine e repair                                                       */
/* ------------------------------------------------------------------------- */

/* Marca in c->mark gli elementi da rimuovere. op: 0 random, 1 related,
 * 2 worst. Ritorna il numero marcato. */
static int ruin_apply(ls_ctx *c, int op, int k)
{
    int i, r = 0;

    memset(c->mark, 0, (size_t)c->n);

    if (op == 1 && c->has_dist) {
        int seed = gor_ls_rng_below(&c->rng, c->n);
        int elem = c->cur[seed];
        c->mark[seed] = 1;
        r = 1;
        while (r < k) {
            int bestp = -1;
            gor_real bestd = 0.0;
            for (i = 0; i < c->n; i++) {
                gor_real d;
                if (c->mark[i]) continue;
                d = c->cfg->dist(elem, c->cur[i], c->cfg->ctx);
                if (bestp < 0 || d < bestd) { bestp = i; bestd = d; }
            }
            if (bestp < 0) break;
            c->mark[bestp] = 1;
            r++;
        }
        return r;
    }

    if (op == 2 && c->has_dist && c->cfg->shape == GOR_LS_SHAPE_PERMUTATION) {
        int *idx = c->scratch;
        int m;
        for (i = 0; i < c->n; i++) idx[i] = i;
        for (m = 0; m < k; m++) {
            int best = m;
            gor_real bestv = 0.0;
            int t;
            for (i = m; i < c->n; i++) {
                int p = idx[i];
                int prev = c->cur[(p + c->n - 1) % c->n];
                int next = c->cur[(p + 1) % c->n];
                int cur = c->cur[p];
                gor_real saving = c->cfg->dist(prev, cur, c->cfg->ctx) +
                                  c->cfg->dist(cur, next, c->cfg->ctx) -
                                  c->cfg->dist(prev, next, c->cfg->ctx);
                if (i == m || saving > bestv) { best = i; bestv = saving; }
            }
            t = idx[m];
            idx[m] = idx[best];
            idx[best] = t;
            c->mark[idx[m]] = 1;
            r++;
        }
        return r;
    }

    while (r < k) {
        int p = gor_ls_rng_below(&c->rng, c->n);
        if (!c->mark[p]) { c->mark[p] = 1; r++; }
    }
    return r;
}

/* Reinserisce gli elementi rimossi. Permutazione: inserimento greedy per
 * distanza. Assegnazione: valore migliore per eval. */
static void repair(ls_ctx *c)
{
    int i, kept = 0;
    int n = c->n;

    for (i = 0; i < n; i++) if (!c->mark[i]) c->cand[kept++] = c->cur[i];

    if (c->cfg->shape == GOR_LS_SHAPE_PERMUTATION) {
        while (kept < n) {
            int pick = -1;
            int cnt = n - kept;
            int want = gor_ls_rng_below(&c->rng, cnt);
            int seen = 0;
            int pos;
            int e;

            for (i = 0; i < n; i++) {
                if (c->mark[i]) {
                    if (seen == want) { pick = i; break; }
                    seen++;
                }
            }
            if (pick < 0) break;
            e = c->cur[pick];

            if (c->has_dist) {
                int p;
                gor_real bestd = 0.0;
                pos = 0;
                for (p = 0; p < kept; p++) {
                    int a = c->cand[p];
                    int b = c->cand[(p + 1) % kept];
                    gor_real d = (kept == 1)
                        ? 2.0 * c->cfg->dist(a, e, c->cfg->ctx)
                        : c->cfg->dist(a, e, c->cfg->ctx) +
                          c->cfg->dist(e, b, c->cfg->ctx) -
                          c->cfg->dist(a, b, c->cfg->ctx);
                    if (p == 0 || d < bestd) { bestd = d; pos = p + 1; }
                }
            } else {
                pos = gor_ls_rng_below(&c->rng, kept + 1);
            }

            for (i = kept; i > pos; i--) c->cand[i] = c->cand[i - 1];
            c->cand[pos] = e;
            c->mark[pick] = 0;
            kept++;
        }
        return;
    }

    for (i = 0; i < n; i++) {
        if (!c->mark[i]) continue;
        {
            int v, bestv = 0;
            gor_ls_score bs;
            for (v = 0; v < c->cfg->value_count; v++) {
                gor_ls_score s;
                c->cand[i] = v;
                s = ls_eval(c, c->cand);
                if (v == 0 || gor_ls_score_cmp(s, bs) > 0) { bs = s; bestv = v; }
            }
            c->cand[i] = bestv;
        }
        c->mark[i] = 0;
    }
}

/* Un kick: ruine + repair + polish. Ritorna il punteggio; *op_out l'operatore. */
static gor_ls_score alns_kick(ls_ctx *c, int *op_out)
{
    const gor_ls_config *cfg = c->cfg;
    int op, k, i, nt;
    double total, r;
    int touched[16];

    total = c->ruin_w[0] + c->ruin_w[1] + c->ruin_w[2];
    if (total <= 0.0) { c->ruin_w[0] = c->ruin_w[1] = c->ruin_w[2] = 1.0; total = 3.0; }
    r = gor_ls_rng_unit(&c->rng) * total;
    if (r < c->ruin_w[0]) op = 0;
    else if ((r -= c->ruin_w[0]) < c->ruin_w[1]) op = 1;
    else op = 2;
    if (!c->has_dist && op != 0) op = 0;
    *op_out = op;

    {
        double frac = cfg->ruin_min_fraction +
                      gor_ls_rng_unit(&c->rng) *
                      (cfg->ruin_max_fraction - cfg->ruin_min_fraction);
        k = (int)(frac * (double)c->n);
        if (k < 1) k = 1;
        if (cfg->max_ruin > 0 && k > cfg->max_ruin) k = cfg->max_ruin;
        if (k > c->n) k = c->n;
    }

    ruin_apply(c, op, k);
    repair(c);

    /* polish: hill climbing breve */
    for (i = 0; i < cfg->polish_steps; i++) {
        gor_ls_score cs, cc;
        if (!apply_move(c, c->cand, c->scratch, touched, &nt)) break;
        cs = ls_eval(c, c->scratch);
        cc = ls_eval(c, c->cand);
        if (gor_ls_score_cmp(cs, cc) > 0) copy_state(c->cand, c->scratch, c->n);
        else break;
    }

    return ls_eval(c, c->cand);
}

/* ------------------------------------------------------------------------- */
/* Ciclo di una singola partenza                                              */
/* ------------------------------------------------------------------------- */

static gor_ls_score run_start(ls_ctx *c, const int *initial, long long *steps_out)
{
    const gor_ls_config *cfg = c->cfg;
    gor_ls_termination_state ts;
    gor_ls_acceptor acceptor = cfg->acceptor;
    gor_ls_score cur_s, best_s;
    long long last_kick_step = 0;
    int touched[16];
    int nt = 0;

    copy_state(c->cur, initial, c->n);
    cur_s = ls_eval(c, c->cur);
    best_s = cur_s;
    copy_state(c->best, c->cur, c->n);

    gor_ls_termination_init(&ts, &cfg->termination, cur_s);

    for (;;) {
        long long now = gor_ls_now_ms();
        double grad = 0.0;
        int accepted;

        if (cfg->termination.spent_ms > 0)
            grad = (double)(now - ts.start_ms) / (double)cfg->termination.spent_ms;
        else if (cfg->termination.step_count > 0)
            grad = (double)ts.steps / (double)cfg->termination.step_count;
        if (grad < 0.0) grad = 0.0;
        if (grad > 1.0) grad = 1.0;

        if (gor_ls_termination_done(&ts, now, best_s)) break;
        if (!apply_move(c, c->cur, c->cand, touched, &nt)) break;

        {
            gor_ls_score cand_s = ls_eval(c, c->cand);
            accepted = gor_ls_acceptor_accept(&acceptor, cand_s, cur_s, best_s,
                                              touched, nt, grad);
            if (accepted) {
                copy_state(c->cur, c->cand, c->n);
                cur_s = cand_s;
                if (gor_ls_score_cmp(cur_s, best_s) > 0) {
                    best_s = cur_s;
                    copy_state(c->best, c->cur, c->n);
                }
            }
        }
        gor_ls_acceptor_step_ended(&acceptor, cur_s);

        if (cfg->alns && cfg->alns_trigger > 0 &&
            ts.steps - ts.last_improve_step >= cfg->alns_trigger &&
            ts.steps - last_kick_step >= cfg->alns_trigger) {
            gor_ls_score prev_best = best_s;
            gor_ls_score prev_cur = cur_s;
            gor_ls_score kick_s;
            int op = 0;
            double reward;

            last_kick_step = ts.steps;
            kick_s = alns_kick(c, &op);

            if (gor_ls_score_cmp(kick_s, prev_best) > 0) reward = 33.0;
            else if (gor_ls_score_cmp(kick_s, prev_cur) > 0) reward = 9.0;
            else reward = 3.0;
            if (cfg->alns_adaptive)
                c->ruin_w[op] = 0.8 * c->ruin_w[op] + 0.2 * reward;

            /* perturbazione: il kick diventa l'incumbent, il best resta salvato */
            copy_state(c->cur, c->cand, c->n);
            cur_s = kick_s;
            if (gor_ls_score_cmp(cur_s, best_s) > 0) {
                best_s = cur_s;
                copy_state(c->best, c->cur, c->n);
            }
        }
    }

    *steps_out = ts.steps;
    return best_s;
}

/* ------------------------------------------------------------------------- */
/* Solve                                                                      */
/* ------------------------------------------------------------------------- */

int gor_ls_solve(const gor_ls_config *cfg, const int *initial, gor_ls_result *res)
{
    ls_ctx c;
    gor_ls_score overall;
    long long total_steps = 0;
    int s, starts;

    if (cfg == NULL || initial == NULL || res == NULL || res->best_state == NULL)
        return GOR_ERR_ARG;
    if (cfg->n <= 0 || cfg->eval == NULL) return GOR_ERR_ARG;
    if (cfg->shape == GOR_LS_SHAPE_ASSIGNMENT &&
        (cfg->value_count <= 0 || cfg->value_count > 1 << 20))
        return GOR_ERR_ARG;

    memset(&c, 0, sizeof(c));
    c.cfg = cfg;
    c.n = cfg->n;
    c.has_dist = (cfg->dist != NULL);
    c.cur = (int *)malloc((size_t)c.n * sizeof(int));
    c.cand = (int *)malloc((size_t)c.n * sizeof(int));
    c.best = (int *)malloc((size_t)c.n * sizeof(int));
    c.scratch = (int *)malloc((size_t)c.n * sizeof(int));
    c.mark = (char *)malloc((size_t)c.n * sizeof(char));
    if (!c.cur || !c.cand || !c.best || !c.scratch || !c.mark) {
        free(c.cur); free(c.cand); free(c.best); free(c.scratch); free(c.mark);
        return GOR_ERR_NOMEM;
    }

    build_near(&c);
    c.ruin_w[0] = c.ruin_w[1] = c.ruin_w[2] = 1.0;

    starts = cfg->starts > 0 ? cfg->starts : 1;
    overall.hard = 0;
    overall.soft = 0.0;

    for (s = 0; s < starts; s++) {
        gor_ls_score b;
        long long st = 0;

        gor_ls_rng_seed(&c.rng,
                        cfg->seed + (unsigned long long)s * 0x9E3779B97F4A7C15ULL);
        b = run_start(&c, initial, &st);
        total_steps += st;

        if (s == 0 || gor_ls_score_cmp(b, overall) > 0) {
            overall = b;
            copy_state(res->best_state, c.best, c.n);
        }
    }

    res->best = overall;
    res->best_cost = overall.soft;
    res->steps = total_steps;
    res->starts = starts;

    free(c.cur);
    free(c.cand);
    free(c.best);
    free(c.scratch);
    free(c.mark);
    free(c.near);
    return GOR_OK;
}
