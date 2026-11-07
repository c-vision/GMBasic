/* gn_metrics.c — metriche di classificazione e regressione. Solo libc. */
#include "gn_metrics.h"
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

int gn_metrics_confusion(const int *y_true, const int *y_pred, int n, int k, int *cm)
{
    int i;
    if (!y_true || !y_pred || !cm || n < 0 || k <= 0) return -1;
    for (i = 0; i < k*k; i++) cm[i] = 0;
    for (i = 0; i < n; i++) {
        int t = y_true[i], p = y_pred[i];
        if (t < 0 || t >= k || p < 0 || p >= k) continue;
        cm[t*k + p]++;
    }
    return 0;
}

double gn_metrics_accuracy(const int *y_true, const int *y_pred, int n)
{
    int i, c = 0;
    if (!y_true || !y_pred || n <= 0) return 0.0;
    for (i = 0; i < n; i++) if (y_true[i] == y_pred[i]) c++;
    return (double)c / (double)n;
}

double gn_metrics_accuracy_cm(const int *cm, int k)
{
    int i, diag = 0, tot = 0;
    if (!cm || k <= 0) return 0.0;
    for (i = 0; i < k; i++) { diag += cm[i*k + i]; }
    for (i = 0; i < k*k; i++) tot += cm[i];
    return tot > 0 ? (double)diag / (double)tot : 0.0;
}

static void col_sum(const int *cm, int k, int *out){ int i,j; for(j=0;j<k;j++){ int s=0; for(i=0;i<k;i++) s+=cm[i*k+j]; out[j]=s; } }
static void row_sum(const int *cm, int k, int *out){ int i,j; for(i=0;i<k;i++){ int s=0; for(j=0;j<k;j++) s+=cm[i*k+j]; out[i]=s; } }

int gn_metrics_precision(const int *cm, int k, double *out, double *out_macro)
{
    int *col; int i; double m = 0.0; int cnt = 0;
    if (!cm || k <= 0) return -1;
    col = malloc((size_t)k*sizeof(int)); if(!col) return -1;
    col_sum(cm,k,col);
    for (i = 0; i < k; i++) {
        double p = col[i] > 0 ? (double)cm[i*k+i]/(double)col[i] : 0.0;
        if (out) out[i] = p;
        if (col[i] > 0) { m += p; cnt++; }
    }
    if (out_macro) *out_macro = cnt > 0 ? m/(double)cnt : 0.0;
    free(col); return 0;
}

int gn_metrics_recall(const int *cm, int k, double *out, double *out_macro)
{
    int *row; int i; double m = 0.0; int cnt = 0;
    if (!cm || k <= 0) return -1;
    row = malloc((size_t)k*sizeof(int)); if(!row) return -1;
    row_sum(cm,k,row);
    for (i = 0; i < k; i++) {
        double r = row[i] > 0 ? (double)cm[i*k+i]/(double)row[i] : 0.0;
        if (out) out[i] = r;
        if (row[i] > 0) { m += r; cnt++; }
    }
    if (out_macro) *out_macro = cnt > 0 ? m/(double)cnt : 0.0;
    free(row); return 0;
}

int gn_metrics_f1(const int *cm, int k, double *out, double *out_macro)
{
    int *col,*row; int i; double m = 0.0; int cnt = 0;
    if (!cm || k <= 0) return -1;
    col = malloc((size_t)k*sizeof(int)); row = malloc((size_t)k*sizeof(int));
    if(!col||!row){ free(col); free(row); return -1; }
    col_sum(cm,k,col); row_sum(cm,k,row);
    for (i = 0; i < k; i++) {
        double p = col[i] > 0 ? (double)cm[i*k+i]/(double)col[i] : 0.0;
        double r = row[i] > 0 ? (double)cm[i*k+i]/(double)row[i] : 0.0;
        double f = (p + r) > 0.0 ? 2.0*p*r/(p + r) : 0.0;
        if (out) out[i] = f;
        if (row[i] > 0 || col[i] > 0) { m += f; cnt++; }
    }
    if (out_macro) *out_macro = cnt > 0 ? m/(double)cnt : 0.0;
    free(col); free(row); return 0;
}

double gn_metrics_mse(const gn_real *y_true, const gn_real *y_pred, int n)
{
    int i; double s = 0.0;
    if (!y_true || !y_pred || n <= 0) return 0.0;
    for (i = 0; i < n; i++) { double e = (double)y_true[i] - (double)y_pred[i]; s += e * e; }
    return s / (double)n;
}

double gn_metrics_mae(const gn_real *y_true, const gn_real *y_pred, int n)
{
    int i; double s = 0.0;
    if (!y_true || !y_pred || n <= 0) return 0.0;
    for (i = 0; i < n; i++) s += fabs((double)y_true[i] - (double)y_pred[i]);
    return s / (double)n;
}

double gn_metrics_r2(const gn_real *y_true, const gn_real *y_pred, int n)
{
    int i; double mean = 0.0, sst = 0.0, ssr = 0.0;
    if (!y_true || !y_pred || n <= 0) return 0.0;
    for (i = 0; i < n; i++) mean += (double)y_true[i];
    mean /= (double)n;
    for (i = 0; i < n; i++) { double dt = (double)y_true[i] - mean, e = (double)y_true[i] - (double)y_pred[i]; sst += dt * dt; ssr += e * e; }
    return sst > 1e-300 ? 1.0 - ssr / sst : 0.0;
}
