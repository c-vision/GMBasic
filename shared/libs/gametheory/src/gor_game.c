/*
 * gor_game.c — teoria dei giochi a somma zero (da diptanilsaha/operation_research).
 *
 * Il saddle point puro è la ricerca maximin/minimax classica; il valore a
 * strategie miste si ottiene dal LP duale del gioco (von Neumann 1928).
 */
#include "gor_game.h"
#include "gor_lp.h"
#include <stdlib.h>
#include <math.h>

int gor_game_saddle_point(int m, int n, const gor_real *payoff,
                          gor_real *value, int *row_idx, int *col_idx)
{
    int i, j, mi = 0, mj = 0;
    gor_real maximin = -1e300, minimax = 1e300;
    if (m < 1 || n < 1 || !payoff) return 0;
    for (i = 0; i < m; i++) {
        gor_real rmin = payoff[(size_t)i * n];
        for (j = 1; j < n; j++)
            if (payoff[(size_t)i * n + j] < rmin) rmin = payoff[(size_t)i * n + j];
        if (rmin > maximin) { maximin = rmin; mi = i; }
    }
    for (j = 0; j < n; j++) {
        gor_real cmax = payoff[j];
        for (i = 1; i < m; i++)
            if (payoff[(size_t)i * n + j] > cmax) cmax = payoff[(size_t)i * n + j];
        if (cmax < minimax) { minimax = cmax; mj = j; }
    }
    if (value) *value = maximin;
    if (row_idx) *row_idx = mi;
    if (col_idx) *col_idx = mj;
    if (fabs(maximin - minimax) < 1e-9) {
        if (value) *value = maximin;
        return 1;
    }
    return 0;
}

gor_real gor_game_value_lp(int m, int n, const gor_real *payoff,
                           gor_real *row_strat, gor_real *col_strat)
{
    double *A, *b, *c, *z, *y, *xd;
    double mn = payoff[0], K, sumx, vshift, v;
    int i, j, nz, rc;
    if (m < 1 || n < 1 || !payoff) return NAN;
    for (i = 1; i < (int)((size_t)m * n); i++) if (payoff[i] < mn) mn = payoff[i];
    K = (mn < 1.0) ? (1.0 - mn) : 0.0;
    /* min sum x_i  s.t.  sum_i (payoff[i][j]+K) x_i - s_j = 1, x,s >= 0 */
    nz = m + n;
    A = (double *)calloc((size_t)n * nz, sizeof(double));
    b = (double *)calloc((size_t)n, sizeof(double));
    c = (double *)calloc((size_t)nz, sizeof(double));
    z = (double *)calloc((size_t)nz, sizeof(double));
    y = (double *)calloc((size_t)n, sizeof(double));
    xd = (double *)calloc((size_t)nz, sizeof(double));
    if (!A || !b || !c || !z || !y || !xd) {
        free(A); free(b); free(c); free(z); free(y); free(xd); return NAN;
    }
    for (j = 0; j < n; j++) {
        for (i = 0; i < m; i++) A[(size_t)j * nz + i] = (double)payoff[(size_t)i * n + j] + K;
        A[(size_t)j * nz + (m + j)] = -1.0;
        b[j] = 1.0;
    }
    for (i = 0; i < m; i++) c[i] = 1.0;
    rc = gor_lp_solve(n, nz, A, b, c, z, y, xd, 2000);
    if (rc != 0) {
        free(A); free(b); free(c); free(z); free(y); free(xd); return NAN;
    }
    sumx = 0.0;
    for (i = 0; i < m; i++) sumx += z[i];
    if (sumx <= 1e-12) { free(A); free(b); free(c); free(z); free(y); free(xd); return NAN; }
    vshift = 1.0 / sumx;
    v = vshift - K;
    if (row_strat) for (i = 0; i < m; i++) row_strat[i] = (gor_real)(z[i] / sumx);
    {
        double sumy = 0.0;
        for (j = 0; j < n; j++) sumy += y[j];
        if (col_strat) {
            if (sumy > 1e-12) for (j = 0; j < n; j++) col_strat[j] = (gor_real)(y[j] / sumy);
            else for (j = 0; j < n; j++) col_strat[j] = (gor_real)(1.0 / n);
        }
    }
    free(A); free(b); free(c); free(z); free(y); free(xd);
    return (gor_real)v;
}
