/*
 * gn_tree.h — albero di regressione/classificazione CART (da Math/Decision
 * Tree from Scratch). Split binario su feature/threshold che minimizza la
 * varianza (regressione) o l'indice di Gini (classificazione).
 */
#ifndef GMBNEURAL_GN_TREE_H
#define GMBNEURAL_GN_TREE_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_tree_node {
    int is_leaf;
    int feature;            /* feature di split (non foglia) */
    double threshold;       /* soglia di split (non foglia) */
    double value;           /* regressione: media del segmento; classif.: classe dominante */
    double *proba;          /* classificazione: distribuzione di classe nella foglia (n_classes) */
    int n_samples;          /* osservazioni nel nodo */
    struct gn_tree_node *left, *right;
} gn_tree_node;

typedef struct {
    gn_tree_node *root;
    int n_features;
    int n_classes;          /* 1 = regressione, >1 = classificazione */
    int max_depth;
    int min_samples_split;
    int min_samples_leaf;
    int *perm;              /* permutazione feature (Random Forest), o NULL */
    int n_perm;
} gn_tree;

/* Fit CART regression (y = targets, n×d dati row-major). Ritorna 0/-1. */
int gn_tree_fit_reg(gn_tree *t, const double *X, const double *y, int n, int d);
/* Fit CART classificazione (y = 0..n_classes-1). */
int gn_tree_fit_cls(gn_tree *t, const double *X, const int *y, int n, int d,
                    int n_classes);
/* Predizione su una riga (regressione: valore; classificazione: classe argmax). */
double gn_tree_predict(const gn_tree *t, const double *x);
void gn_tree_free(gn_tree *t);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_TREE_H */