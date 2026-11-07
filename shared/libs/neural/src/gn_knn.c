/* gn_knn.c — k-nearest-neighbours classifier. */
#include "gn_knn.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int gn_knn_classify(const double *Xtr, const int *ytr, int ntr, int d,
                    int n_classes, int k, const double *x)
{
    double *dist;
    int *idx;
    int i, j, c, best_c = 0;
    int *votes;
    if (!Xtr || !ytr || !x || ntr < 1 || d < 1 || n_classes < 1) return -1;
    if (k < 1) k = 1;
    if (k > ntr) k = ntr;
    dist = (double *)malloc((size_t)ntr * sizeof(double));
    idx = (int *)malloc((size_t)k * sizeof(int));
    votes = (int *)calloc((size_t)n_classes, sizeof(int));
    if (!dist || !idx || !votes) { free(dist); free(idx); free(votes); return -1; }
    for (i = 0; i < ntr; i++) {
        double s = 0.0;
        for (j = 0; j < d; j++) { double t = Xtr[(size_t)i * d + j] - x[j]; s += t * t; }
        dist[i] = s;
    }
    /* selezione dei k minimi (inserimento ordinato) */
    for (i = 0; i < k; i++) idx[i] = -1;
    for (i = 0; i < ntr; i++) {
        int pos = k;
        for (j = 0; j < k; j++) {
            if (idx[j] < 0) { pos = j; break; }
            if (dist[i] < dist[idx[j]]) { pos = j; break; }
        }
        if (pos < k) {
            for (j = k - 1; j > pos; j--) idx[j] = idx[j - 1];
            idx[pos] = i;
        }
    }
    for (i = 0; i < k; i++) if (idx[i] >= 0) votes[ytr[idx[i]]]++;
    best_c = 0;
    for (c = 1; c < n_classes; c++) if (votes[c] > votes[best_c]) best_c = c;
    free(dist); free(idx); free(votes);
    return best_c;
}

double gn_knn_accuracy(const double *Xtr, const int *ytr, int ntr,
                       const double *Xte, const int *yte, int nte, int d,
                       int n_classes, int k)
{
    int i, ok = 0;
    if (!Xtr || !ytr || !Xte || !yte || nte < 1) return 0.0 / 0.0;
    for (i = 0; i < nte; i++) {
        int p = gn_knn_classify(Xtr, ytr, ntr, d, n_classes, k, Xte + (size_t)i * d);
        if (p == yte[i]) ok++;
    }
    return (double)ok / (double)nte;
}
