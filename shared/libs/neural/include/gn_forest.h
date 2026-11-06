/*
 * gn_forest.h — Random Forest (bagging + subsampling feature, da Math/Building
 * Random Forest From Scratch). Regressione: media delle predizioni dei tree;
 * classificazione: voto di maggioranza. Bootstrap con seed deterministico.
 */
#ifndef GMBNEURAL_GN_FOREST_H
#define GMBNEURAL_GN_FOREST_H

#include "gn_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gn_tree *trees;     /* array di n_estimators tree */
    int n_estimators;
    int n_features;
    int n_classes;
    int max_depth;
    int min_samples_split;
    int max_features;   /* feature random per split (0 = sqrt/3 rule) */
    unsigned seed;
} gn_forest;

int gn_forest_fit_reg(gn_forest *f, const double *X, const double *y, int n, int d);
int gn_forest_fit_cls(gn_forest *f, const double *X, const int *y, int n, int d,
                      int n_classes);
double gn_forest_predict(const gn_forest *f, const double *x);

/* AUC (Mann-Whitney U con tied ranks, da Bayes-Methods-for-Hackers). */
double gn_auc(const double *actual, const double *score, int n);
void gn_forest_free(gn_forest *f);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_FOREST_H */