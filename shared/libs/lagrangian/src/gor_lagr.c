/*
 * gor_lagr.c — rilassamento lagrangiano con subgradiente (da Location_problem_FLP).
 *
 * Tecnica classica di decomposizione: si rilassano i vincoli difficili in
 * funzione obiettivo con moltiplicatori non negativi e si itera aggiornando i
 * moltiplicatori lungo il subgradiente, con passo di Polyak.
 */
#include "gor_lagr.h"
#include <stdlib.h>
#include <math.h>

gor_real gor_lagr_subgradient(int m, int nx, const gor_real *A, const gor_real *b,
                              gor_lagr_sub sub, void *ctx, int maxit, gor_real ub,
                              gor_real *lambda, gor_real *best_x)
{
    gor_real *x, *g;
    gor_real best = -1e300, alpha = 1.0;
    int i, it, noimp = 0;
    if (m < 1 || nx < 1 || !A || !b || !sub || !lambda || !best_x) return 0.0;
    if (maxit < 1) maxit = 100;
    x = (gor_real *)malloc((size_t)nx * sizeof(gor_real));
    g = (gor_real *)malloc((size_t)m * sizeof(gor_real));
    if (!x || !g) { free(x); free(g); return 0.0; }
    for (i = 0; i < m; i++) lambda[i] = 0.0;
    for (it = 0; it < maxit; it++) {
        gor_real lb = sub(lambda, m, ctx, x);
        gor_real norm2 = 0.0, step, scale = 1.0 / (gor_real)(it + 1);
        int feasible = 1;
        if (lb > best) {
            best = lb;
            for (i = 0; i < nx; i++) best_x[i] = x[i];
            noimp = 0;
        } else {
            noimp++;
        }
        for (i = 0; i < m; i++) {
            gor_real s = b[i];
            int k;
            for (k = 0; k < nx; k++) s -= A[(size_t)i * nx + k] * x[k];
            g[i] = s;
            if (s > 1e-7) feasible = 0;
            norm2 += s * s;
        }
        if (feasible && (ub <= 0.0 || lb < ub)) ub = lb;   /* incumbent ammissibile */
        if (norm2 < 1e-20) break;
        {   /* passo di Polyak con decadimento: alpha/(it+1)*(UB-LB)/||g||^2 */
            gor_real ubv = (ub > 0.0) ? ub : lb + fabs(lb) * 0.1 + 1.0;
            step = scale * alpha * (ubv - lb) / norm2;
            if (step < 0.0) step = 0.0;
        }
        for (i = 0; i < m; i++) {
            lambda[i] += step * g[i];
            if (lambda[i] < 0.0) lambda[i] = 0.0;
        }
        if (ub > 0.0 && ub - best < 1e-7) break;           /* gap chiuso */
        if (noimp >= 10) { alpha *= 0.5; noimp = 0; if (alpha < 1e-6) break; }
    }
    free(x); free(g);
    return best;
}
