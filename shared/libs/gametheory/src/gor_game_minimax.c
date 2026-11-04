/* gm_game.c — teoria dei giochi (riscrittura). */
#include "gor_game_minimax.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

gor_status gor_game_minimax(const gor_real *A, int m, int n,
                          gor_real *value, gor_real *p, gor_real *q)
{
    /* apprendimento per gioco fittizio (Brown-Robinson) */
    double *ci, *cj;
    long long T = 200000, t;
    int i, j;
    if (A == NULL || p == NULL || q == NULL || m <= 0 || n <= 0) return GOR_ERR_ARG;
    ci = (double *)malloc((size_t)m * sizeof(double));
    cj = (double *)malloc((size_t)n * sizeof(double));
    if (!ci || !cj) { free(ci); free(cj); return GOR_ERR_NOMEM; }
    for (i = 0; i < m; i++) ci[i] = 1.0;
    for (j = 0; j < n; j++) cj[j] = 1.0;
    for (t = 0; t < T; t++) {
        int bi = 0, bj = 0;
        double best = -1e300, worst = 1e300;
        for (i = 0; i < m; i++) {
            double s = 0.0;
            for (j = 0; j < n; j++) s += A[(size_t)i * (size_t)n + (size_t)j] * cj[j];
            if (s > best) { best = s; bi = i; }
        }
        ci[bi] += 1.0;
        for (j = 0; j < n; j++) {
            double s = 0.0;
            for (i = 0; i < m; i++) s += ci[i] * A[(size_t)i * (size_t)n + (size_t)j];
            if (s < worst) { worst = s; bj = j; }
        }
        cj[bj] += 1.0;
    }
    {
        double toti = 0.0, totj = 0.0, v = 0.0;
        for (i = 0; i < m; i++) toti += ci[i];
        for (j = 0; j < n; j++) totj += cj[j];
        for (i = 0; i < m; i++) p[i] = (gor_real)(ci[i] / toti);
        for (j = 0; j < n; j++) q[j] = (gor_real)(cj[j] / totj);
        for (i = 0; i < m; i++)
            for (j = 0; j < n; j++)
                v += p[i] * A[(size_t)i * (size_t)n + (size_t)j] * q[j];
        if (value) *value = (gor_real)v;
    }
    free(ci); free(cj);
    return GOR_OK;
}

static int popcount_mask(unsigned int x)
{
    int c = 0;
    while (x) { c += (int)(x & 1u); x >>= 1; }
    return c;
}

gor_status gor_nash_2p(const gor_real *A, const gor_real *B, int m, int n,
                     gor_real *p, gor_real *q, gor_real tol)
{
    /* enumerazione dei supporti di uguale cardinalità (m,n piccoli) */
    unsigned int sm, sn;
    int k;
    if (A == NULL || B == NULL || p == NULL || q == NULL || m <= 0 || n <= 0) return GOR_ERR_ARG;
    if (m > 12 || n > 12) return GOR_ERR_RANGE;
    for (k = 1; k <= (m < n ? m : n); k++) {
        for (sm = 1u; sm < (1u << m); sm++) {
            if (popcount_mask(sm) != k) continue;
            for (sn = 1u; sn < (1u << n); sn++) {
                int idx[12], jdx[12], nk = 0, nk2 = 0, i, j;
                gor_real *M, *rhs, *sol;
                if (popcount_mask(sn) != k) continue;
                for (i = 0; i < m; i++) if (sm & (1u << i)) idx[nk++] = i;
                for (j = 0; j < n; j++) if (sn & (1u << j)) jdx[nk2++] = j;
                /* q: A[i][j] q_j - u = 0 per i in S; Σ q = 1 */
                M = (gor_real *)calloc((size_t)(k + 1) * (size_t)(k + 1), sizeof(gor_real));
                rhs = (gor_real *)calloc((size_t)(k + 1), sizeof(gor_real));
                sol = (gor_real *)calloc((size_t)(k + 1), sizeof(gor_real));
                if (!M || !rhs || !sol) { free(M); free(rhs); free(sol); return GOR_ERR_NOMEM; }
                for (i = 0; i < k; i++) {
                    for (j = 0; j < k; j++) M[(size_t)i * (size_t)(k + 1) + (size_t)j] = A[(size_t)idx[i] * (size_t)n + (size_t)jdx[j]];
                    M[(size_t)i * (size_t)(k + 1) + (size_t)k] = -1.0;
                }
                for (j = 0; j < k; j++) M[(size_t)k * (size_t)(k + 1) + (size_t)j] = 1.0;
                rhs[k] = 1.0;
                if (gm_linalg_solve(k + 1, M, rhs, sol) == GM_OK) {
                    gor_real u = sol[k];
                    int ok = 1;
                    for (j = 0; j < k; j++) if (sol[j] < -tol) ok = 0;
                    for (i = 0; i < m && ok; i++) {
                        gor_real s = 0.0;
                        for (j = 0; j < k; j++) s += A[(size_t)i * (size_t)n + (size_t)jdx[j]] * sol[j];
                        if (s > u + tol) ok = 0;
                    }
                    if (ok) {
                        /* risolvi p dal gioco del giocatore 2 */
                        gor_real *M2 = (gor_real *)calloc((size_t)(k + 1) * (size_t)(k + 1), sizeof(gor_real));
                        gor_real *rhs2 = (gor_real *)calloc((size_t)(k + 1), sizeof(gor_real));
                        gor_real *sol2 = (gor_real *)calloc((size_t)(k + 1), sizeof(gor_real));
                        int ok2 = 0;
                        if (M2 && rhs2 && sol2) {
                            for (j = 0; j < k; j++) {
                                for (i = 0; i < k; i++) M2[(size_t)j * (size_t)(k + 1) + (size_t)i] = B[(size_t)idx[i] * (size_t)n + (size_t)jdx[j]];
                                M2[(size_t)j * (size_t)(k + 1) + (size_t)k] = -1.0;
                            }
                            for (i = 0; i < k; i++) M2[(size_t)k * (size_t)(k + 1) + (size_t)i] = 1.0;
                            rhs2[k] = 1.0;
                            if (gm_linalg_solve(k + 1, M2, rhs2, sol2) == GM_OK) {
                                gor_real v = sol2[k];
                                ok2 = 1;
                                for (i = 0; i < k; i++) if (sol2[i] < -tol) ok2 = 0;
                                for (j = 0; j < n && ok2; j++) {
                                    gor_real s = 0.0;
                                    for (i = 0; i < k; i++) s += sol2[i] * B[(size_t)idx[i] * (size_t)n + (size_t)j];
                                    if (s > v + tol) ok2 = 0;
                                }
                            }
                        }
                        if (ok2) {
                            for (i = 0; i < m; i++) p[i] = 0.0;
                            for (j = 0; j < n; j++) q[j] = 0.0;
                            for (i = 0; i < k; i++) p[idx[i]] = sol2[i];
                            for (j = 0; j < k; j++) q[jdx[j]] = sol[j];
                            free(M); free(rhs); free(sol);
                            free(M2); free(rhs2); free(sol2);
                            return GOR_OK;
                        }
                        free(M2); free(rhs2); free(sol2);
                    }
                }
                free(M); free(rhs); free(sol);
            }
        }
    }
    return GOR_ERR_NOCONV;
}

gor_status gor_shapley(int n, gor_real (*v)(unsigned int, void *), void *ctx, gor_real *phi)
{
    int i;
    gor_real fn = 1.0;
    if (v == NULL || phi == NULL || n <= 0) return GOR_ERR_ARG;
    for (i = 2; i <= n; i++) fn *= (gor_real)i;
    if (n <= 16) {
        int m1 = n - 1;
        for (i = 0; i < n; i++) {
            gor_real acc = 0.0;
            unsigned int sub;
            for (sub = 0; sub < (1u << m1); sub++) {
                /* costruisci S escludendo i, mappando gli indici */
                unsigned int S = 0, j;
                int idx = 0, s;
                for (j = 0; j < (unsigned int)n; j++) {
                    if ((int)j == i) continue;
                    if (sub & (1u << idx)) S |= (1u << j);
                    idx++;
                }
                s = popcount_mask(S);
                {
                    gor_real w = 1.0;
                    int kk;
                    for (kk = 2; kk <= s; kk++) w *= (gor_real)kk;
                    for (kk = 2; kk <= (n - s - 1); kk++) w *= (gor_real)kk;
                    w /= fn;
                    acc += w * (v(S | (1u << i), ctx) - v(S, ctx));
                }
            }
            phi[i] = acc;
        }
        return GOR_OK;
    }
    return GOR_ERR_RANGE;
}
