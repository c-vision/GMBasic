/*
 * game_ffi.c — shim ABI per la TEORIA DEI GIOCHI (namespace OPTIMIZE).
 *
 * Le funzioni C usano int/double e una callback `v(unsigned int mask, void*)`
 * per Shapley: l'FFI passa la callback come `double (*)(long long)`, quindi qui
 * si fa da ponte con un puntatore globale valido durante la chiamata.
 */
#include "gor_game.h"
#include "gor_game_minimax.h"

#include <stdlib.h>

typedef long long i64;
typedef double (*ffx_mask_cb)(double);

static ffx_mask_cb g_mask_cb;

static gor_real ffx_mask_bridge(unsigned int mask, void *ctx)
{
    (void)ctx;
    return (gor_real)g_mask_cb((double)mask);
}

/* out(0)=value, out(1)=riga, out(2)=colonna. Ritorna 1 se il saddle esiste. */
i64 ofx_game_saddle(const double *payoff, i64 m, i64 n, double *out)
{
    gor_real value = 0.0;
    int ri = -1, ci = -1;
    int r;
    if (m <= 0 || n <= 0 || payoff == NULL || out == NULL) return -1;
    r = gor_game_saddle_point((int)m, (int)n, (const gor_real *)payoff, &value, &ri, &ci);
    out[0] = (double)value;
    out[1] = (double)ri;
    out[2] = (double)ci;
    return (i64)r;
}

double ofx_game_value(const double *payoff, i64 m, i64 n, double *row_strat, double *col_strat)
{
    if (m <= 0 || n <= 0 || payoff == NULL || row_strat == NULL || col_strat == NULL) return -1.0;
    return (double)gor_game_value_lp((int)m, (int)n, (const gor_real *)payoff,
                                     (gor_real *)row_strat, (gor_real *)col_strat);
}

/* out() = value | p[m] | q[n] */
i64 ofx_game_minimax(const double *A, i64 m, i64 n, double *out)
{
    gor_real value = 0.0;
    if (m <= 0 || n <= 0 || A == NULL || out == NULL) return -1;
    if (gor_game_minimax((const gor_real *)A, (int)m, (int)n, &value, (gor_real *)(out + 1),
                         (gor_real *)(out + 1 + m)) != GOR_OK) return 0;
    out[0] = (double)value;
    return 1;
}

/* out() = p[m] | q[n] */
i64 ofx_nash_2p(const double *A, const double *B, i64 m, i64 n, double *out)
{
    if (m <= 0 || n <= 0 || A == NULL || B == NULL || out == NULL) return -1;
    return (i64)gor_nash_2p((const gor_real *)A, (const gor_real *)B, (int)m, (int)n,
                            (gor_real *)out, (gor_real *)(out + m), 1e-9);
}

i64 ofx_shapley(ffx_mask_cb v, i64 n, double *phi)
{
    if (v == NULL || n <= 0 || phi == NULL) return -1;
    g_mask_cb = v;
    return (i64)gor_shapley((int)n, ffx_mask_bridge, NULL, (gor_real *)phi);
}
