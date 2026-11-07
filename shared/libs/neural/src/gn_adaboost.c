/* gn_adaboost.c — AdaBoost SAMME con stump (Gini ponderato), clean-room. */
#include "gn_adaboost.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int feature; double threshold; int lc, rc; } stump_t;

/* Stump: split singolo che minimizza il Gini ponderato. y in {0,1}. */
static void stump_fit(stump_t *s, const double *X, const int *y, const double *w,
                      int n, int d)
{
    double best_g = 1e300;
    int j, i, k;
    s->feature = 0; s->threshold = 0.0; s->lc = 0; s->rc = 0;
    for (j = 0; j < d; j++) {
        double *xs = (double *)malloc((size_t)n * sizeof(double));
        int *order = (int *)malloc((size_t)n * sizeof(int));
        double *wx = (double *)malloc((size_t)n * sizeof(double));
        int *wy = (int *)malloc((size_t)n * sizeof(int));
        if (!xs || !order || !wx || !wy) { free(xs); free(order); free(wx); free(wy); continue; }
        for (i = 0; i < n; i++) xs[i] = X[i * d + j];
        for (k = 0; k < n; k++) order[k] = k;
        for (i = 0; i < n; i++)
            for (k = i + 1; k < n; k++)
                if (xs[order[k]] < xs[order[i]]) { int t = order[i]; order[i] = order[k]; order[k] = t; }
        {
            double wl0 = 0, wl1 = 0, wr0 = 0, wr1 = 0;
            for (i = 0; i < n; i++) {
                if (y[order[i]] == 0) wr0 += w[order[i]]; else wr1 += w[order[i]];
            }
            for (i = 0; i < n - 1; i++) {
                double gl, gr, g;
                int c;
                if (y[order[i]] == 0) { wl0 += w[order[i]]; wr0 -= w[order[i]]; }
                else { wl1 += w[order[i]]; wr1 -= w[order[i]]; }
                if (xs[i] == xs[i + 1]) continue;
                {
                    double nl = wl0 + wl1, nr = wr0 + wr1;
                    gl = 1.0 - (wl0 / nl) * (wl0 / nl) - (wl1 / nl) * (wl1 / nl);
                    gr = 1.0 - (wr0 / nr) * (wr0 / nr) - (wr1 / nr) * (wr1 / nr);
                    g = (nl * gl + nr * gr) / (nl + nr);
                }
                if (g < best_g) {
                    best_g = g;
                    s->feature = j;
                    s->threshold = (xs[order[i]] + xs[order[i + 1]]) / 2.0;
                    s->lc = wl1 >= wl0 ? 1 : 0;
                    s->rc = wr1 >= wr0 ? 1 : 0;
                }
                (void)c;
            }
        }
        free(xs); free(order); free(wx); free(wy);
    }
}

static int stump_predict(const stump_t *s, const double *x)
{
    return x[s->feature] <= s->threshold ? s->lc : s->rc;
}

int gn_adaboost_fit(gn_adaboost *b, const double *X, const int *y, int n, int d)
{
    double *w, err;
    int m, i, k;
    stump_t *stumps;
    if (!b || !X || !y || n < 2 || d < 1) return -1;
    b->n_estimators = b->n_estimators == 0 ? 50 : b->n_estimators;
    b->learning_rate = b->learning_rate == 0.0 ? 1.0 : b->learning_rate;
    b->n_features = d;
    stumps = (stump_t *)calloc((size_t)b->n_estimators, sizeof(stump_t));
    b->alphas = (double *)calloc((size_t)b->n_estimators, sizeof(double));
    if (!stumps || !b->alphas) { free(stumps); return -1; }
    w = (double *)malloc((size_t)n * sizeof(double));
    if (!w) { free(stumps); return -1; }
    for (i = 0; i < n; i++) w[i] = 1.0 / n;
    for (m = 0; m < b->n_estimators; m++) {
        stump_t *s = &stumps[m];
        double norm = 0.0;
        stump_fit(s, X, y, w, n, d);
        err = 0.0;
        for (i = 0; i < n; i++) {
            double xr[64];
            for (k = 0; k < d; k++) xr[k] = X[i * d + k];
            if (stump_predict(s, xr) != y[i]) err += w[i];
        }
        if (err > 0.5) err = 1.0 - err;
        err = err < 1e-12 ? 1e-12 : err;
        b->alphas[m] = b->learning_rate * 0.5 * log((1.0 - err) / err);
        for (i = 0; i < n; i++) {
            double xr[64];
            for (k = 0; k < d; k++) xr[k] = X[i * d + k];
            w[i] *= exp(-b->alphas[m] * (stump_predict(s, xr) == y[i] ? 1.0 : -1.0));
            norm += w[i];
        }
        for (i = 0; i < n; i++) w[i] /= norm;
    }
    b->stumps = (gn_tree *)stumps;   /* riutilizzo il campo per conservare i dati */
    b->n_stumps = b->n_estimators;
    free(w);
    return 0;
}

int gn_adaboost_predict(const gn_adaboost *b, const double *x)
{
    double acc = 0.0;
    int m;
    stump_t *stumps = (stump_t *)b->stumps;
    for (m = 0; m < b->n_estimators; m++) {
        int p = stump_predict(&stumps[m], x);
        acc += b->alphas[m] * (p == 1 ? 1.0 : -1.0);
    }
    return acc > 0.0 ? 1 : -1;
}

void gn_adaboost_free(gn_adaboost *b)
{
    if (!b) return;
    free(b->stumps); free(b->alphas);
    b->stumps = NULL; b->alphas = NULL;
}