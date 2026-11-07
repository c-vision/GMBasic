/* gn_forest.c — Random Forest regressione/classificazione. */
#include "gn_forest.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

static unsigned rng_next(unsigned *s)
{
    *s = *s * 1103515245u + 12345u;
    return *s;
}

static void bootstrap(const double *X, const double *y, const int *yc,
                      int n, int d, unsigned *seed,
                      double *Xb, double *yb, int *ycb)
{
    int i, j;
    unsigned s = *seed;
    for (i = 0; i < n; i++) {
        int r = (int)(rng_next(&s) % (unsigned)n);
        for (j = 0; j < d; j++) Xb[i * d + j] = X[r * d + j];
        if (y) yb[i] = y[r];
        if (yc) ycb[i] = yc[r];
    }
    *seed = s;
}

/* Permuta le feature e ne prende le prime max_features. */
static void feat_sample(int d, unsigned *seed, int *feat_perm)
{
    int i;
    unsigned s = *seed;
    for (i = 0; i < d; i++) feat_perm[i] = i;
    for (i = d - 1; i > 0; i--) {
        int jr = (int)(rng_next(&s) % (unsigned)(i + 1));
        int t = feat_perm[i]; feat_perm[i] = feat_perm[jr]; feat_perm[jr] = t;
    }
    *seed = s;
}

static int forest_fit(gn_forest *f, const double *X, const void *y,
                      const int *yc, int n, int d, int n_classes, int is_cls)
{
    double *Xb;
    double *yb = NULL;
    int *ycb = NULL;
    unsigned seed;
    int e;
    if (!f || !X || !y || n < 1 || d < 1) return -1;
    f->n_estimators = f->n_estimators == 0 ? 100 : f->n_estimators;
    f->max_depth = f->max_depth == 0 ? 32 : f->max_depth;
    f->seed = f->seed == 0 ? 12345u : f->seed;
    seed = f->seed;
    f->n_features = d;
    f->n_classes = n_classes <= 0 ? 1 : n_classes;
    f->trees = (gn_tree *)calloc((size_t)f->n_estimators, sizeof(gn_tree));
    if (!f->trees) return -1;
    Xb = (double *)malloc((size_t)n * d * sizeof(double));
    if (is_cls) ycb = (int *)malloc((size_t)n * sizeof(int));
    else yb = (double *)malloc((size_t)n * sizeof(double));
    if (!Xb || (is_cls && !ycb) || (!is_cls && !yb)) {
        free(Xb); free(yb); free(ycb);
        return -1;
    }
    for (e = 0; e < f->n_estimators; e++) {
        gn_tree *t = &f->trees[e];
        double *Xs;
        int *perm, mf, ii, jj;
        bootstrap(X, is_cls ? NULL : (const double *)y, yc, n, d,
                  &seed, Xb, yb, ycb);
        mf = f->max_features > 0 ? f->max_features
             : (is_cls ? (int)(sqrt((double)d)) : (d / 3 + 1));
        if (mf > d) mf = d;
        if (mf < 1) mf = 1;
        Xs = (double *)malloc((size_t)n * mf * sizeof(double));
        perm = (int *)malloc((size_t)d * sizeof(int));
        if (!Xs || !perm) { free(Xs); free(perm); free(Xb); free(yb); free(ycb); return -1; }
        feat_sample(d, &seed, perm);
        for (ii = 0; ii < n; ii++)
            for (jj = 0; jj < mf; jj++) Xs[ii * mf + jj] = Xb[ii * d + perm[jj]];
        t->max_depth = f->max_depth;
        t->min_samples_split = f->min_samples_split == 0 ? 2 : f->min_samples_split;
        t->perm = perm; t->n_perm = mf;
        if (is_cls) gn_tree_fit_cls(t, Xs, ycb, n, mf, n_classes);
        else gn_tree_fit_reg(t, Xs, yb, n, mf);
        free(Xs);
    }
    free(Xb); free(yb); free(ycb);
    return 0;
}

int gn_forest_fit_reg(gn_forest *f, const double *X, const double *y, int n, int d)
{
    return forest_fit(f, X, y, NULL, n, d, 1, 0);
}

int gn_forest_fit_cls(gn_forest *f, const double *X, const int *y, int n, int d,
                      int n_classes)
{
    return forest_fit(f, X, y, y, n, d, n_classes, 1);
}

double gn_forest_predict(const gn_forest *f, const double *x)
{
    double acc = 0.0;
    int e;
    if (f->n_classes <= 1) {
        for (e = 0; e < f->n_estimators; e++) {
            gn_tree *t = &f->trees[e];
            double xrow[64];
            int ii;
            if (t->perm) for (ii = 0; ii < t->n_perm; ii++) xrow[ii] = x[t->perm[ii]];
            else for (ii = 0; ii < t->n_features; ii++) xrow[ii] = x[ii];
            acc += gn_tree_predict(t, xrow);
        }
        return acc / f->n_estimators;
    } else {
        int *votes = (int *)calloc((size_t)f->n_classes, sizeof(int));
        int best = 0;
        if (!votes) return 0.0;
        for (e = 0; e < f->n_estimators; e++) {
            gn_tree *t = &f->trees[e];
            double xrow[64];
            int ii;
            if (t->perm) for (ii = 0; ii < t->n_perm; ii++) xrow[ii] = x[t->perm[ii]];
            else for (ii = 0; ii < t->n_features; ii++) xrow[ii] = x[ii];
            {
                int c = (int)gn_tree_predict(t, xrow);
                if (c >= 0 && c < f->n_classes) votes[c]++;
            }
        }
        for (e = 1; e < f->n_classes; e++) if (votes[e] > votes[best]) best = e;
        free(votes);
        return (double)best;
    }
}

void gn_forest_free(gn_forest *f)
{
    int e;
    if (!f) return;
    for (e = 0; e < f->n_estimators; e++) gn_tree_free(&f->trees[e]);
    free(f->trees);
    f->trees = NULL;
}
/* ---- AUC (Mann-Whitney U con tied ranks, da Bayes-Methods-for-Hackers).
 * actual: 0/1 binario; score: punteggio continuo. Ritorna AUC in [0,1]. */
double gn_auc(const double *actual, const double *score, int n)
{
    double *order, rsum = 0.0;
    int *ispos;
    int i, npos = 0, nneg = 0;
    double auc;
    if (!actual || !score || n < 2) return 0.0 / 0.0;
    order = (double *)malloc((size_t)n * sizeof(double));
    ispos = (int *)malloc((size_t)n * sizeof(int));
    if (!order || !ispos) { free(order); free(ispos); return 0.0 / 0.0; }
    for (i = 0; i < n; i++) { order[i] = score[i]; ispos[i] = actual[i] > 0.5 ? 1 : 0; }
    /* insertion sort per score crescente (ordina anche ispos) */
    for (i = 1; i < n; i++) {
        double v = order[i]; int p = ispos[i]; int j = i - 1;
        while (j >= 0 && order[j] > v) { order[j + 1] = order[j]; ispos[j + 1] = ispos[j]; j--; }
        order[j + 1] = v; ispos[j + 1] = p;
    }
    i = 0;
    while (i < n) {
        int j = i;
        while (j + 1 < n && order[j + 1] == order[i]) j++;
        {
            double avg = (double)(i + j) / 2.0 + 1.0;
            int k;
            for (k = i; k <= j; k++) if (ispos[k]) rsum += avg;
        }
        i = j + 1;
    }
    for (i = 0; i < n; i++) { if (ispos[i]) npos++; else nneg++; }
    free(order); free(ispos);
    if (npos == 0 || nneg == 0) return 0.5;
    auc = (rsum - npos * (npos + 1) / 2.0) / ((double)npos * nneg);
    return auc;
}
