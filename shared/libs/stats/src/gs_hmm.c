/* gs_hmm.c — Hidden Markov Model gaussiano (log-domain). */
#include "gs_hmm.h"
#include "gs_mvdist.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

gc_real gs_hmm_forward(int n, int n_states, const gc_real *logpi,
                       const gc_real *logA, const gc_real *logE)
{
    gc_real *alpha = (gc_real *)malloc((size_t)n * n_states * sizeof(gc_real));
    gc_real *scale = (gc_real *)calloc((size_t)n, sizeof(gc_real));
    gc_real loglik = 0.0;
    int t, i, j;
    if (!alpha || !scale) { free(alpha); free(scale); return -INFINITY; }
    for (i = 0; i < n_states; i++) {
        alpha[i] = logpi[i] + logE[i];
        scale[0] += alpha[i];
    }
    if (scale[0] < -1e300) { free(alpha); free(scale); return -INFINITY; }
    for (i = 0; i < n_states; i++) alpha[i] -= scale[0];
    loglik += scale[0];
    for (t = 1; t < n; t++) {
        scale[t] = 0.0;
        for (j = 0; j < n_states; j++) {
            gc_real best = -INFINITY;
            for (i = 0; i < n_states; i++) {
                gc_real v = alpha[(t - 1) * n_states + i] + logA[i * n_states + j];
                if (v > best) best = v;
            }
            alpha[t * n_states + j] = best + logE[t * n_states + j];
            scale[t] += alpha[t * n_states + j];
        }
        if (scale[t] < -1e300) { free(alpha); free(scale); return -INFINITY; }
        for (j = 0; j < n_states; j++) alpha[t * n_states + j] -= scale[t];
        loglik += scale[t];
    }
    free(alpha); free(scale);
    return loglik;
}

gc_real gs_hmm_viterbi(int n, int n_states, const gc_real *logpi,
                       const gc_real *logA, const gc_real *logE, int *path)
{
    gc_real *delta = (gc_real *)malloc((size_t)n * n_states * sizeof(gc_real));
    int *psi = (int *)malloc((size_t)n * n_states * sizeof(int));
    int t, i, j;
    gc_real best = -INFINITY;
    if (!delta || !psi || !path) { free(delta); free(psi); return -INFINITY; }
    for (i = 0; i < n_states; i++) delta[i] = logpi[i] + logE[i];
    for (t = 1; t < n; t++) {
        for (j = 0; j < n_states; j++) {
            gc_real bj = -INFINITY;
            int bi = 0;
            for (i = 0; i < n_states; i++) {
                gc_real v = delta[(t - 1) * n_states + i] + logA[i * n_states + j];
                if (v > bj) { bj = v; bi = i; }
            }
            delta[t * n_states + j] = bj + logE[t * n_states + j];
            psi[t * n_states + j] = bi;
        }
    }
    best = -INFINITY;
    for (j = 0; j < n_states; j++) {
        if (delta[(n - 1) * n_states + j] > best) { best = delta[(n - 1) * n_states + j]; path[n - 1] = j; }
    }
    for (t = n - 2; t >= 0; t--) path[t] = psi[(t + 1) * n_states + path[t + 1]];
    free(delta); free(psi);
    return best;
}

static gc_real logadd(gc_real a, gc_real b)
{
    gc_real m;
    if (a == -INFINITY) return b;
    if (b == -INFINITY) return a;
    m = a > b ? a : b;
    return m + log(exp(a - m) + exp(b - m));
}

int gs_hmm_fit(const gc_real *X, int n, int d, int n_states, int n_iter,
               gc_real *pi, gc_real *A, gc_real *means, gc_real *covs)
{
    gc_real *logE = (gc_real *)malloc((size_t)n * n_states * sizeof(gc_real));
    gc_real *lpi, *lA;
    gc_real *al, *be, *gam, *xi;
    int t, i, j, it, k;
    if (!X || !pi || !A || !means || !covs || n < n_states || d < 1 || n_states < 1) return -1;
    if (n_iter < 1) n_iter = 100;
    lpi = (gc_real *)malloc((size_t)n_states * sizeof(gc_real));
    lA = (gc_real *)malloc((size_t)n_states * n_states * sizeof(gc_real));
    al = (gc_real *)malloc((size_t)n * n_states * sizeof(gc_real));
    be = (gc_real *)malloc((size_t)n * n_states * sizeof(gc_real));
    gam = (gc_real *)malloc((size_t)n * n_states * sizeof(gc_real));
    xi = (gc_real *)malloc((size_t)n * n_states * n_states * sizeof(gc_real));
    if (!logE || !lpi || !lA || !al || !be || !gam || !xi) {
        free(logE); free(lpi); free(lA); free(al); free(be); free(gam); free(xi);
        return -1;
    }
    /* emissioni log per i parametri correnti */
    for (t = 0; t < n; t++)
        for (i = 0; i < n_states; i++)
            logE[t * n_states + i] = gs_mvgauss_logpdf(d, &X[t * d], &means[i * d], &covs[i * d * d]);
    for (it = 0; it < n_iter; it++) {
        gc_real *newpi, *newA, *newmeans, *newcovs;
        for (i = 0; i < n_states; i++) lpi[i] = log(pi[i] > 0.0 ? pi[i] : 1e-300);
        for (i = 0; i < n_states; i++)
            for (j = 0; j < n_states; j++)
                lA[i * n_states + j] = log(A[i * n_states + j] > 0.0 ? A[i * n_states + j] : 1e-300);
        /* forward (scaled) */
        for (i = 0; i < n_states; i++) al[i] = lpi[i] + logE[i];
        for (t = 1; t < n; t++)
            for (j = 0; j < n_states; j++) {
                gc_real bj = -INFINITY;
                for (i = 0; i < n_states; i++)
                    bj = logadd(bj, al[(t - 1) * n_states + i] + lA[i * n_states + j]);
                al[t * n_states + j] = bj + logE[t * n_states + j];
            }
        /* backward (scaled, dall'ultima) */
        for (i = 0; i < n_states; i++) be[(n - 1) * n_states + i] = 0.0;
        for (t = n - 2; t >= 0; t--)
            for (i = 0; i < n_states; i++) {
                gc_real bj = -INFINITY;
                for (j = 0; j < n_states; j++)
                    bj = logadd(bj, lA[i * n_states + j] + logE[(t + 1) * n_states + j] + be[(t + 1) * n_states + j]);
                be[t * n_states + i] = bj;
            }
        /* gamma: al+be normalizzato (per t, somma su i = 1 in log approx) */
        for (t = 0; t < n; t++) {
            gc_real s = -INFINITY;
            for (i = 0; i < n_states; i++) s = logadd(s, al[t * n_states + i] + be[t * n_states + i]);
            for (i = 0; i < n_states; i++) gam[t * n_states + i] = exp(al[t * n_states + i] + be[t * n_states + i] - s);
        }
        /* xi */
        for (t = 0; t < n - 1; t++) {
            gc_real s = -INFINITY;
            for (i = 0; i < n_states; i++)
                for (j = 0; j < n_states; j++) {
                    xi[(t * n_states + i) * n_states + j] =
                        al[t * n_states + i] + lA[i * n_states + j]
                        + logE[(t + 1) * n_states + j] + be[(t + 1) * n_states + j];
                    s = logadd(s, xi[(t * n_states + i) * n_states + j]);
                }
            for (i = 0; i < n_states; i++)
                for (j = 0; j < n_states; j++)
                    xi[(t * n_states + i) * n_states + j] = exp(xi[(t * n_states + i) * n_states + j] - s);
        }
        /* M-step */
        newpi = (gc_real *)calloc((size_t)n_states, sizeof(gc_real));
        newA = (gc_real *)calloc((size_t)n_states * n_states, sizeof(gc_real));
        newmeans = (gc_real *)calloc((size_t)n_states * d, sizeof(gc_real));
        newcovs = (gc_real *)calloc((size_t)n_states * d * d, sizeof(gc_real));
        if (!newpi || !newA || !newmeans || !newcovs) {
            free(newpi); free(newA); free(newmeans); free(newcovs);
            free(logE); free(lpi); free(lA); free(al); free(be); free(gam); free(xi);
            return -1;
        }
        for (i = 0; i < n_states; i++) {
            gc_real g0 = 0.0;
            for (t = 0; t < n; t++) g0 += gam[t * n_states + i];
            newpi[i] = gam[i];
            for (j = 0; j < n_states; j++) {
                gc_real s = 0.0;
                for (t = 0; t < n - 1; t++) s += xi[(t * n_states + i) * n_states + j];
                newA[i * n_states + j] = s;
            }
            for (k = 0; k < d; k++) {
                gc_real s = 0.0;
                for (t = 0; t < n; t++) s += gam[t * n_states + i] * X[t * d + k];
                newmeans[i * d + k] = g0 > 0.0 ? s / g0 : 0.0;
            }
            for (k = 0; k < d * d; k++) {
                gc_real s = 0.0;
                int kk = k / d, ll = k % d;
                for (t = 0; t < n; t++)
                    s += gam[t * n_states + i]
                         * (X[t * d + kk] - newmeans[i * d + kk])
                         * (X[t * d + ll] - newmeans[i * d + ll]);
                newcovs[i * d * d + k] = g0 > 0.0 ? s / g0 + 1e-6 : 1e-6;
            }
        }
        /* normalizza */
        {
            gc_real s = 0.0;
            for (i = 0; i < n_states; i++) s += newpi[i];
            for (i = 0; i < n_states; i++) newpi[i] = s > 0.0 ? newpi[i] / s : 1.0 / n_states;
            for (i = 0; i < n_states; i++) {
                gc_real s2 = 0.0;
                for (j = 0; j < n_states; j++) s2 += newA[i * n_states + j];
                for (j = 0; j < n_states; j++) newA[i * n_states + j] = s2 > 0.0 ? newA[i * n_states + j] / s2 : 1.0 / n_states;
            }
        }

        memcpy(pi, newpi, (size_t)n_states * sizeof(gc_real));
        memcpy(A, newA, (size_t)n_states * n_states * sizeof(gc_real));
        memcpy(means, newmeans, (size_t)n_states * d * sizeof(gc_real));
        memcpy(covs, newcovs, (size_t)n_states * d * d * sizeof(gc_real));
        for (t = 0; t < n; t++)
            for (i = 0; i < n_states; i++)
                logE[t * n_states + i] = gs_mvgauss_logpdf(d, &X[t * d], &means[i * d], &covs[i * d * d]);
        free(newpi); free(newA); free(newmeans); free(newcovs);
    }
    free(logE); free(lpi); free(lA); free(al); free(be); free(gam); free(xi);
    return 0;
}