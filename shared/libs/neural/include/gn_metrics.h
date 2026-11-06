/*
 * gn_metrics.h — metriche di classificazione (confusion matrix, precision/recall/F1).
 *
 * Promosso da MegaJoctan/ML5 (Sklearn/metrics.mqh). C99 puri, nessuna dipendenza ML.
 * NB: gmbneural ha già KNN + accuracy (gn_knn); questo copre le metriche per-class e macro
 * usate nella valutazione dei classificatori.
 */
#ifndef GMBNEURAL_GN_METRICS_H
#define GMBNEURAL_GN_METRICS_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Matrice di confusione k x k (row-major cm[true*k + pred]++), classi in [0,k).
 * Ritorna 0, o -1 su input non validi. */
int gn_metrics_confusion(const int *y_true, const int *y_pred, int n, int k, int *cm);

/* Accuratezza = corrette / n. 0.0 se n<=0. */
double gn_metrics_accuracy(const int *y_true, const int *y_pred, int n);

/* Precision/recall/F1 per classe (out[k]) e macro-media (out inizializzato a 0 se la
 * classe non ha predizioni/istanze). out o out_macro possono essere NULL. Ritorna 0/-1. */
int gn_metrics_precision(const int *cm, int k, double *out, double *out_macro);
int gn_metrics_recall(const int *cm, int k, double *out, double *out_macro);
int gn_metrics_f1(const int *cm, int k, double *out, double *out_macro);

/* accuratezza a partire dalla confusion matrix. */
double gn_metrics_accuracy_cm(const int *cm, int k);

/* ── metriche di regressione ── */
double gn_metrics_mse(const gn_real *y_true, const gn_real *y_pred, int n);   /* errore quadratico medio */
double gn_metrics_mae(const gn_real *y_true, const gn_real *y_pred, int n);   /* errore assoluto medio */
double gn_metrics_r2(const gn_real *y_true, const gn_real *y_pred, int n);    /* coefficiente di determinazione */

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_METRICS_H */
