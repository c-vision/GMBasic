/* gor_cp.c — SAT (DPLL) e constraint programming. */
#include "gor_cp.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ============================== SAT (DPLL) ============================== */

/* valore di un letterale: 1 vero, 0 falso, -1 non assegnato */
static int lit_val(int lit, const int *assign)
{
    int v = abs(lit) - 1;
    if (assign[v] < 0) return -1;
    return (lit > 0) ? assign[v] : !assign[v];
}

static int dpll(int nvars, int nclauses, const int *clauses,
                const int *clause_start, const int *clause_len, int *assign)
{
    int k, j, unit_lit = 0, all_sat = 1;
    /* unit propagation + controllo clausole */
    for (k = 0; k < nclauses; k++) {
        int sat = 0, unassigned = 0, last_unassigned = 0;
        for (j = 0; j < clause_len[k]; j++) {
            int lit = clauses[clause_start[k] + j];
            int v = lit_val(lit, assign);
            if (v == 1) { sat = 1; break; }
            if (v == -1) { unassigned++; last_unassigned = lit; }
        }
        if (sat) continue;
        all_sat = 0;
        if (unassigned == 0) return 0; /* clausola falsa */
        if (unassigned == 1) { unit_lit = last_unassigned; break; }
    }
    if (all_sat) return 1;
    if (unit_lit != 0) {
        int v = abs(unit_lit) - 1, save = assign[v];
        assign[v] = (unit_lit > 0) ? 1 : 0;
        if (dpll(nvars, nclauses, clauses, clause_start, clause_len, assign)) return 1;
        assign[v] = save;
        return 0;
    }
    /* branch sulla prima variabile non assegnata */
    for (k = 0; k < nvars; k++) {
        if (assign[k] < 0) {
            int save = assign[k];
            assign[k] = 1;
            if (dpll(nvars, nclauses, clauses, clause_start, clause_len, assign)) return 1;
            assign[k] = 0;
            if (dpll(nvars, nclauses, clauses, clause_start, clause_len, assign)) return 1;
            assign[k] = save;
            return 0;
        }
    }
    return 1;
}

int gor_sat_dpll(int nvars, int nclauses, const int *clauses,
                 const int *clause_start, const int *clause_len, int *assignment)
{
    int *assign, i, res;
    if (nvars <= 0 || nclauses <= 0 || clauses == NULL || clause_start == NULL || clause_len == NULL || assignment == NULL) return 0;
    assign = (int *)malloc((size_t)nvars * sizeof(int));
    if (!assign) return 0;
    for (i = 0; i < nvars; i++) assign[i] = -1;
    res = dpll(nvars, nclauses, clauses, clause_start, clause_len, assign);
    if (res) for (i = 0; i < nvars; i++) assignment[i] = (assign[i] == 1) ? 1 : 0;
    free(assign);
    return res;
}

/* ============================== all-different ============================== */

static int ad_rec(int n, int D, const int *dom, int *sol, int *used, int i)
{
    int d;
    if (i == n) return 1;
    for (d = 0; d < D; d++) {
        int v = dom[(size_t)i * (size_t)D + (size_t)d];
        if (!used[v + 1000000]) {
            used[v + 1000000] = 1;
            sol[i] = v;
            if (ad_rec(n, D, dom, sol, used, i + 1)) return 1;
            used[v + 1000000] = 0;
        }
    }
    return 0;
}

int gor_cp_alldiff(int n, int D, const int *dom, int *sol)
{
    int *used;
    int res;
    if (n <= 0 || D <= 0 || dom == NULL || sol == NULL) return 0;
    used = (int *)calloc(2000001, sizeof(int));
    if (!used) return 0;
    res = ad_rec(n, D, dom, sol, used, 0);
    free(used);
    return res;
}

/* ============================== CSP binario ============================== */

static int bin_rec(int n, int D, const int *allowed, int *idx, int i)
{
    int d, j;
    if (i == n) return 1;
    for (d = 0; d < D; d++) {
        int ok = 1;
        for (j = 0; j < i && ok; j++)
            if (!allowed[((size_t)j * (size_t)n + (size_t)i) * (size_t)D * (size_t)D + (size_t)idx[j] * (size_t)D + (size_t)d]) ok = 0;
        if (!ok) continue;
        idx[i] = d;
        if (bin_rec(n, D, allowed, idx, i + 1)) return 1;
    }
    return 0;
}

int gor_cp_binary(int n, int D, const int *dom, const int *allowed, int *sol)
{
    int *idx, i, res;
    if (n <= 0 || D <= 0 || dom == NULL || allowed == NULL || sol == NULL) return 0;
    idx = (int *)malloc((size_t)n * sizeof(int));
    if (!idx) return 0;
    res = bin_rec(n, D, allowed, idx, 0);
    if (res) for (i = 0; i < n; i++) sol[i] = dom[(size_t)i * (size_t)D + (size_t)idx[i]];
    free(idx);
    return res;
}
