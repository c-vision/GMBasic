/* gmb_opt_pso.c — parità CommonLib/Trading/PsoOptimizer.cs:307 (Particle Swarm Optimization)
 * Minimize(returns[periodi x asset], alpha, objective CVaR|CDaR, swarmSize, maxIter)
 * Deterministico (xorshift64 seed fisso) per test parity.
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum { GMB_PSO_CVAR = 0, GMB_PSO_CDAR = 1 };

static uint64_t g_pso_seed = 0x9E3779B97F4A7C15ULL;
static uint64_t g_pso_rng_next(void)
{
    uint64_t x = g_pso_seed;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    g_pso_seed = x;
    return x * 2685821657736338717ULL;
}
static double g_pso_rnd01(void) { return (double)(g_pso_rng_next() & 0xFFFFFFFFu) / 4294967296.0; }

static void g_pso_normalize(double* w, int n)
{
    for (int i = 0; i < n; i++) if (w[i] < 0) w[i] = 0;
    double sum = 0;
    for (int i = 0; i < n; i++) sum += w[i];
    if (sum > 1e-9) { for (int i = 0; i < n; i++) w[i] /= sum; }
    else { for (int i = 0; i < n; i++) w[i] = 1.0 / n; }
}

/* CVaR: -media dei rendimenti di portafoglio <= varIndex */
static double g_pso_cvar(const double* w, const double* returns, int nSim, int nAssets, double alpha)
{
    double* pr = (double*)malloc((size_t)nSim * sizeof(double));
    if (!pr) return 1e18;
    for (int i = 0; i < nSim; i++) {
        double sum = 0;
        for (int j = 0; j < nAssets; j++) sum += returns[(size_t)i * nAssets + j] * w[j];
        pr[i] = sum;
    }
    /* insertion sort */
    for (int i = 1; i < nSim; i++) { double t = pr[i]; int k = i - 1; while (k >= 0 && pr[k] > t) { pr[k+1] = pr[k]; k--; } pr[k+1] = t; }
    int varIndex = (int)floor(nSim * alpha);
    if (varIndex < 0) varIndex = 0;
    if (varIndex >= nSim) varIndex = nSim - 1;
    double varValue = pr[varIndex];
    double acc = 0; int cnt = 0;
    for (int i = 0; i < nSim; i++) if (pr[i] <= varValue) { acc += pr[i]; cnt++; }
    double r = (cnt > 0) ? -acc / cnt : 0;
    free(pr);
    return r;
}

/* CDaR: media dei drawdown >= varIndex */
static double g_pso_cdar(const double* w, const double* returns, int nSim, int nAssets, double alpha)
{
    double* pr = (double*)malloc((size_t)nSim * sizeof(double));
    double* dd = (double*)malloc((size_t)nSim * sizeof(double));
    if (!pr || !dd) { free(pr); free(dd); return 1e18; }
    for (int i = 0; i < nSim; i++) {
        double sum = 0;
        for (int j = 0; j < nAssets; j++) sum += returns[(size_t)i * nAssets + j] * w[j];
        pr[i] = sum;
    }
    double cum = 1 + pr[0];
    double peak = cum > 1.0 ? cum : 1.0;
    dd[0] = (peak - cum) / peak;
    for (int i = 1; i < nSim; i++) {
        cum *= (1 + pr[i]);
        if (cum > peak) peak = cum;
        dd[i] = (peak - cum) / peak;
    }
    for (int i = 1; i < nSim; i++) { double t = dd[i]; int k = i - 1; while (k >= 0 && dd[k] > t) { dd[k+1] = dd[k]; k--; } dd[k+1] = t; }
    int varIndex = (int)floor(nSim * (1 - alpha));
    if (varIndex < 0) varIndex = 0;
    if (varIndex >= nSim) varIndex = nSim - 1;
    double varValue = dd[varIndex];
    double acc = 0; int cnt = 0;
    for (int i = 0; i < nSim; i++) if (dd[i] >= varValue) { acc += dd[i]; cnt++; }
    double r = (cnt > 0) ? acc / cnt : 0;
    free(pr); free(dd);
    return r;
}

static double g_pso_objective(const double* w, const double* returns, int nSim, int nAssets,
                              double alpha, int objective)
{
    if (objective == GMB_PSO_CDAR) return g_pso_cdar(w, returns, nSim, nAssets, alpha);
    return g_pso_cvar(w, returns, nSim, nAssets, alpha);
}

void gmb_opt_pso(const double* returns, int nSim, int nAssets, double alpha, int objective,
                 int swarmSize, int maxIter, double* out)
{
    if (!returns || !out || nSim <= 0 || nAssets <= 0) return;
    if (swarmSize <= 0) swarmSize = 50;
    if (maxIter <= 0) maxIter = 200;

    double wInertia = 0.729;
    double c1 = 1.494, c2 = 1.494;

    /* storage: particles[swarm][n], vel[swarm][n], bestPos[swarm][n], bestScore[swarm] */
    double* parts = (double*)calloc((size_t)swarmSize * nAssets, sizeof(double));
    double* vels = (double*)calloc((size_t)swarmSize * nAssets, sizeof(double));
    double* bestPos = (double*)calloc((size_t)swarmSize * nAssets, sizeof(double));
    double* bestScore = (double*)malloc((size_t)swarmSize * sizeof(double));
    double* globalBest = (double*)malloc((size_t)nAssets * sizeof(double));
    if (!parts || !vels || !bestPos || !bestScore || !globalBest) {
        free(parts); free(vels); free(bestPos); free(bestScore); free(globalBest);
        for (int i = 0; i < nAssets; i++) out[i] = 1.0 / nAssets;
        return;
    }

    double globalBestScore = 0;
    for (int s = 0; s < swarmSize; s++) {
        double* p = parts + (size_t)s * nAssets;
        double* v = vels + (size_t)s * nAssets;
        for (int j = 0; j < nAssets; j++) { p[j] = g_pso_rnd01(); v[j] = (g_pso_rnd01() - 0.5) * 0.1; }
        g_pso_normalize(p, nAssets);
        memcpy(bestPos + (size_t)s * nAssets, p, (size_t)nAssets * sizeof(double));
        bestScore[s] = g_pso_objective(p, returns, nSim, nAssets, alpha, objective);
        if (s == 0 || bestScore[s] < globalBestScore) {
            memcpy(globalBest, p, (size_t)nAssets * sizeof(double));
            globalBestScore = bestScore[s];
        }
    }

    for (int iter = 0; iter < maxIter; iter++) {
        for (int s = 0; s < swarmSize; s++) {
            double* p = parts + (size_t)s * nAssets;
            double* v = vels + (size_t)s * nAssets;
            double* bp = bestPos + (size_t)s * nAssets;
            for (int j = 0; j < nAssets; j++) {
                v[j] = wInertia * v[j]
                     + c1 * g_pso_rnd01() * (bp[j] - p[j])
                     + c2 * g_pso_rnd01() * (globalBest[j] - p[j]);
            }
            for (int j = 0; j < nAssets; j++) p[j] += v[j];
            g_pso_normalize(p, nAssets);
            double score = g_pso_objective(p, returns, nSim, nAssets, alpha, objective);
            if (score < bestScore[s]) {
                bestScore[s] = score;
                memcpy(bp, p, (size_t)nAssets * sizeof(double));
                if (score < globalBestScore) {
                    memcpy(globalBest, p, (size_t)nAssets * sizeof(double));
                    globalBestScore = score;
                }
            }
        }
    }

    memcpy(out, globalBest, (size_t)nAssets * sizeof(double));
    free(parts); free(vels); free(bestPos); free(bestScore); free(globalBest);
}
