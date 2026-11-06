/*
 * gn_adaboost.h — AdaBoost (SAMME) con alberi di profondita' 1 (stump),
 * da Math/Building AdaBoost from scratch. Classificazione binaria y in {+1,-1}
 * (o 0/1 convertiti internamente).
 */
#ifndef GMBNEURAL_GN_ADABOOST_H
#define GMBNEURAL_GN_ADABOOST_H

#include "gn_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *stumps;           /* n_stumps stump interni (feature/threshold/classi) */
    double *alphas;         /* pesi dell'ensemble */
    int n_estimators;
    double learning_rate;
    int n_features;
    int n_stumps;
} gn_adaboost;

/* Fit AdaBoost binario: y in {0,1}. Ritorna 0/-1. */
int gn_adaboost_fit(gn_adaboost *b, const double *X, const int *y, int n, int d);
/* Predizione: +1 se somma alpha*stump(x) > 0, altrimenti -1. */
int gn_adaboost_predict(const gn_adaboost *b, const double *x);
void gn_adaboost_free(gn_adaboost *b);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_ADABOOST_H */