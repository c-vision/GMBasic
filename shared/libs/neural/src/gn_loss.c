/* gn_loss.c — funzioni di perdita. */
#include "gn_loss.h"

#include <math.h>
#include <stdint.h>

gn_real gn_loss_mse(const gn_mat *y, const gn_mat *t, gn_mat *grad)
{
    int i, n;
    gn_real loss = 0.0;
    if (!y || !t || y->rows != t->rows || y->cols != t->cols) return 0.0;
    n = y->rows * y->cols;
    for (i = 0; i < n; i++) {
        gn_real d = y->data[i] - t->data[i];
        loss += d * d;
    }
    loss /= (gn_real)n;
    if (grad) {
        if (grad->rows != y->rows || grad->cols != y->cols) return loss;
        for (i = 0; i < n; i++) grad->data[i] = 2.0 * (y->data[i] - t->data[i]) / (gn_real)n;
    }
    return loss;
}

gn_real gn_loss_mae(const gn_mat *y, const gn_mat *t, gn_mat *grad)
{
    int i, n;
    gn_real loss = 0.0;
    if (!y || !t || y->rows != t->rows || y->cols != t->cols) return 0.0;
    n = y->rows * y->cols;
    for (i = 0; i < n; i++) {
        gn_real d = y->data[i] - t->data[i];
        loss += fabs(d);
        if (grad && grad->rows == y->rows && grad->cols == y->cols)
            grad->data[i] = (d > 0.0 ? 1.0 : (d < 0.0 ? -1.0 : 0.0)) / (gn_real)n;
    }
    return loss / (gn_real)n;
}

gn_real gn_loss_cross_entropy(const gn_mat *y, const gn_mat *t, gn_mat *grad)
{
    int i, n;
    gn_real loss = 0.0;
    if (!y || !t || y->rows != t->rows || y->cols != t->cols) return 0.0;
    n = y->rows * y->cols;
    for (i = 0; i < n; i++) {
        gn_real p = y->data[i];
        if (p < 1e-12) p = 1e-12;
        loss -= t->data[i] * log(p);
    }
    loss /= (gn_real)y->rows;
    if (grad) {
        if (grad->rows != y->rows || grad->cols != y->cols) return loss;
        for (i = 0; i < n; i++) grad->data[i] = (y->data[i] - t->data[i]) / (gn_real)y->rows;
    }
    return loss;
}

gn_real gn_loss_binary_cross_entropy(const gn_mat *y, const gn_mat *t, gn_mat *grad)
{
    int i, n;
    gn_real loss = 0.0;
    if (!y || !t || y->rows != t->rows || y->cols != t->cols) return 0.0;
    n = y->rows * y->cols;
    for (i = 0; i < n; i++) {
        gn_real p = y->data[i];
        if (p < 1e-12) p = 1e-12;
        if (p > 1.0 - 1e-12) p = 1.0 - 1e-12;
        loss -= t->data[i] * log(p) + (1.0 - t->data[i]) * log(1.0 - p);
    }
    loss /= (gn_real)n;
    if (grad) {
        if (grad->rows != y->rows || grad->cols != y->cols) return loss;
        for (i = 0; i < n; i++) {
            gn_real p = y->data[i];
            if (p < 1e-12) p = 1e-12;
            if (p > 1.0 - 1e-12) p = 1.0 - 1e-12;
            grad->data[i] = (p - t->data[i]) / ((gn_real)n * p * (1.0 - p));
        }
    }
    return loss;
}
