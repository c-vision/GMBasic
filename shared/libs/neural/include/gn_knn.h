/*
 * gn_knn.h — classificatore k-nearest-neighbours (lazy, distanza euclidea).
 * Generico: Xtr n x d (row-major), ytr classi in [0, n_classes).
 */
#ifndef GMBNEURAL_GN_KNN_H
#define GMBNEURAL_GN_KNN_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Classe predetta per x (voto di maggioranza dei k vicini; uniformi). */
int gn_knn_classify(const double *Xtr, const int *ytr, int ntr, int d,
                    int n_classes, int k, const double *x);

/* Accuratezza su un test set. */
double gn_knn_accuracy(const double *Xtr, const int *ytr, int ntr,
                       const double *Xte, const int *yte, int nte, int d,
                       int n_classes, int k);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_KNN_H */
