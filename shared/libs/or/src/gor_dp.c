/*
 * gor_dp.c — programmazione dinamica classica (da Nelson6203/IO-Project).
 *
 * Tre problemi DP ricorrenti nei corsi di Ricerca Operativa, reimplementati
 * in forma riusabile: optimal BST, sostituzione attrezzature, serie sportive.
 */
#include "gor_dp.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Optimal binary search tree (Knuth 1971; O(n^3), O(n^2) con Knuth).  */
/* ------------------------------------------------------------------ */
gor_real gor_dp_optimal_bst(int n, const gor_real *p, int *root)
{
    gor_real *e, *w;
    int i, j, l, r;
    gor_real best;
    if (n <= 0) return 0.0;
    e = (gor_real *)calloc((size_t)(n + 1) * (n + 1), sizeof(gor_real));
    w = (gor_real *)calloc((size_t)(n + 1) * (n + 1), sizeof(gor_real));
    if (!e || !w) { free(e); free(w); return -1.0; }
    if (root) for (i = 0; i < n * n; i++) root[i] = -1;
    /* w[i][j] = somma p[i..j-1] (chiavi indicizzate 0..n-1) */
    for (i = 0; i < n; i++) {
        w[(size_t)i * (n + 1) + i + 1] = p[i];
        e[(size_t)i * (n + 1) + i] = 0.0;      /* albero vuoto */
    }
    for (l = 1; l <= n; l++) {
        for (i = 0; i + l <= n; i++) {
            j = i + l;
            w[(size_t)i * (n + 1) + j] = w[(size_t)i * (n + 1) + j - 1] + p[j - 1];
            e[(size_t)i * (n + 1) + j] = 1e300;
            for (r = i; r < j; r++) {
                gor_real c = e[(size_t)i * (n + 1) + r]
                           + e[(size_t)(r + 1) * (n + 1) + j]
                           + w[(size_t)i * (n + 1) + j];
                if (c < e[(size_t)i * (n + 1) + j]) {
                    e[(size_t)i * (n + 1) + j] = c;
                    if (root) root[(size_t)i * n + (j - 1)] = r;
                }
            }
        }
    }
    best = e[(size_t)0 * (n + 1) + n];
    free(e); free(w);
    return best;
}

/* ------------------------------------------------------------------ */
/* Sostituzione di attrezzature (equipment replacement).               */
/* ------------------------------------------------------------------ */
gor_real gor_dp_equipment_replacement(int years, gor_real buy_cost, int max_age,
                                      const gor_real *op_cost,
                                      const gor_real *resale, int *schedule)
{
    gor_real *g, *keep;
    int *nxt;
    int t, j, a;
    if (years <= 0 || max_age < 1 || !op_cost || !resale) return 0.0;
    g = (gor_real *)calloc((size_t)years + 1, sizeof(gor_real));
    keep = (gor_real *)calloc((size_t)years + 1, sizeof(gor_real));
    nxt = (int *)calloc((size_t)years + 1, sizeof(int));
    if (!g || !keep || !nxt) { free(g); free(keep); free(nxt); return -1.0; }
    /* g[T] = 0; g[t] = costo minimo coprendo gli anni t..T-1 con macchina
     * nuova all'inizio dell'anno t. Si sceglie il prossimo rimpiazzo in j. */
    g[years] = 0.0;
    for (t = years - 1; t >= 0; t--) {
        gor_real run = 0.0;
        g[t] = 1e300;
        nxt[t] = years;
        for (j = t + 1; j <= years; j++) {
            gor_real repl = 0.0;
            /* costo operativo anni t..j-1 con eta' 0..j-t-1 */
            run = 0.0;
            for (a = 0; a < j - t; a++) {
                if (a < max_age) run += op_cost[a];
            }
            if (j < years) {
                int age = j - t;
                repl = buy_cost - (age < max_age ? resale[age] : 0.0);
            }
            {
                gor_real c = run + repl + g[j];
                if (c < g[t] - 1e-9) { g[t] = c; nxt[t] = j; }
            }
        }
        keep[t] = run;
    }
    if (schedule) {
        for (t = 0; t <= years; t++) schedule[t] = -1;
        t = 0;
        while (t < years) { schedule[t] = 0; t = nxt[t]; }
    }
    {
        gor_real res = g[0];
        free(g); free(keep); free(nxt);
        return res;
    }
}

/* ------------------------------------------------------------------ */
/* Serie sportive "al meglio di N".                                    */
/* ------------------------------------------------------------------ */
gor_real gor_dp_sports_series(int n_games, int needed, gor_real p_home,
                              gor_real p_away, const int *is_home)
{
    gor_real *cur, *nxt;
    int wa, wb, g, W;
    gor_real ans = 0.0;
    if (n_games <= 0 || needed <= 0) return 0.0;
    W = needed;
    cur = (gor_real *)calloc((size_t)W * (size_t)W, sizeof(gor_real));
    nxt = (gor_real *)calloc((size_t)W * (size_t)W, sizeof(gor_real));
    if (!cur || !nxt) { free(cur); free(nxt); return -1.0; }
    cur[0] = 1.0;   /* stato (0,0) prima della prima partita */
    for (g = 0; g < n_games; g++) {
        gor_real pa = (is_home && is_home[g]) ? p_home : p_away;
        memset(nxt, 0, (size_t)W * (size_t)W * sizeof(gor_real));
        for (wa = 0; wa < W; wa++) {
            for (wb = 0; wb < W; wb++) {
                gor_real pr = cur[(size_t)wa * W + wb];
                if (pr <= 0.0) continue;
                if (wa + 1 == W) ans += pr * pa;                 /* A vince la serie */
                else nxt[(size_t)(wa + 1) * W + wb] += pr * pa;
                if (wb + 1 < W)                                  /* B non ancora campione */
                    nxt[(size_t)wa * W + (wb + 1)] += pr * (1.0 - pa);
            }
        }
        memcpy(cur, nxt, (size_t)W * (size_t)W * sizeof(gor_real));
    }
    free(cur); free(nxt);
    return ans;
}
