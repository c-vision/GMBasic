/* gn_tree.c — albero CART di regressione/classificazione. */
#include "gn_tree.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { const double *X; const double *y; const int *yc; int n, d, n_classes; int min_split, min_leaf, max_depth; int *idx; } tree_ctx;

static gn_tree_node *new_node(int is_leaf, int n_samples)
{
    gn_tree_node *no = (gn_tree_node *)calloc(1, sizeof(gn_tree_node));
    if (!no) return NULL;
    no->is_leaf = is_leaf;
    no->n_samples = n_samples;
    return no;
}

static double node_value(const tree_ctx *c, int *idx, int m, double *var_out)
{
    double s = 0.0, s2 = 0.0;
    int i;
    for (i = 0; i < m; i++) { double v = c->y[idx[i]]; s += v; s2 += v * v; }
    if (var_out) *var_out = s2 / m - (s / m) * (s / m);
    return s / m;
}

static double node_gini(const tree_ctx *c, int *idx, int m)
{
    double *cnt = (double *)calloc((size_t)c->n_classes, sizeof(double));
    double g = 1.0;
    int i;
    if (!cnt) return 1.0;
    for (i = 0; i < m; i++) cnt[c->yc[idx[i]]] += 1.0;
    for (i = 0; i < c->n_classes; i++) g -= (cnt[i] / m) * (cnt[i] / m);
    free(cnt);
    return g;
}

/* Miglior split su una feature: regressione = riduzione di varianza. */
static void best_split_reg(const tree_ctx *c, int *idx, int m, int *feat,
                           double *thr, double *gain)
{
    int j, i, k;
    double best_g = -1e300;
    int best_f = -1;
    double best_t = 0.0;
    int *order = (int *)malloc((size_t)m * sizeof(int));
    if (!order) return;
    for (j = 0; j < c->d; j++) {
        double *xs = (double *)malloc((size_t)m * sizeof(double));
        if (!xs) { free(order); return; }
        for (i = 0; i < m; i++) xs[i] = c->X[idx[i] * c->d + j];
        /* ordina per valore */
        for (k = 0; k < m; k++) order[k] = k;
        for (i = 0; i < m; i++)
            for (k = i + 1; k < m; k++)
                if (xs[order[k]] < xs[order[i]]) { int t2 = order[i]; order[i] = order[k]; order[k] = t2; }
        {
            double *px = (double *)malloc((size_t)m * sizeof(double));
            double *py = (double *)malloc((size_t)m * sizeof(double));
            if (!px || !py) { free(px); free(py); free(xs); free(order); return; }
            for (i = 0; i < m; i++) { px[i] = xs[order[i]]; py[i] = c->y[idx[order[i]]]; }
            {
                double sl = 0.0, s2l = 0.0, sr = 0.0, s2r = 0.0;
                for (i = 0; i < m; i++) { sr += py[i]; s2r += py[i] * py[i]; }
                for (i = 0; i < m - 1; i++) {
                    sl += py[i]; s2l += py[i] * py[i];
                    sr -= py[i]; s2r -= py[i] * py[i];
                    if (px[i] == px[i + 1]) continue;
                    {
                        int nl = i + 1, nr = m - nl;
                        double vl = s2l / nl - (sl / nl) * (sl / nl);
                        double vr = s2r / nr - (sr / nr) * (sr / nr);
                        double var_total = 0.0;
                        double g;
                        {
                            double s = 0.0, s2 = 0.0; int k2;
                            for (k2 = 0; k2 < m; k2++) { s += py[k2]; s2 += py[k2] * py[k2]; }
                            var_total = s2 / m - (s / m) * (s / m);
                        }
                        g = var_total - ((double)nl / m) * vl - ((double)nr / m) * vr;
                        if (g > 1e-15 && g > best_g) { best_g = g; best_f = j; best_t = (px[i] + px[i + 1]) / 2.0; }
                    }
                }
            }
            free(px); free(py);
        }
        free(xs);
    }
    free(order);
    *feat = best_f;
    *thr = best_t;
    *gain = best_g;
}

/* Miglior split classificazione: riduzione di Gini. */
static void best_split_cls(const tree_ctx *c, int *idx, int m, int *feat,
                           double *thr, double *gain)
{
    int j, i, k;
    double best_g = -1e300;
    int best_f = -1;
    double best_t = 0.0;
    double g0 = node_gini(c, idx, m);
    int *order = (int *)malloc((size_t)m * sizeof(int));
    if (!order) return;
    for (j = 0; j < c->d; j++) {
        double *xs = (double *)malloc((size_t)m * sizeof(double));
        if (!xs) { free(order); return; }
        for (i = 0; i < m; i++) xs[i] = c->X[idx[i] * c->d + j];
        for (k = 0; k < m; k++) order[k] = k;
        for (i = 0; i < m; i++)
            for (k = i + 1; k < m; k++)
                if (xs[order[k]] < xs[order[i]]) { int t2 = order[i]; order[i] = order[k]; order[k] = t2; }
        {
            int *left_cnt = (int *)calloc((size_t)c->n_classes, sizeof(int));
            int *right_cnt = (int *)calloc((size_t)c->n_classes, sizeof(int));
            if (!left_cnt || !right_cnt) { free(left_cnt); free(right_cnt); free(xs); free(order); return; }
            for (i = 0; i < m; i++) right_cnt[c->yc[idx[order[i]]]]++;
            for (i = 0; i < m - 1; i++) {
                double gl, gr, g;
                int nl, nr, cl;
                left_cnt[c->yc[idx[order[i]]]]++;
                right_cnt[c->yc[idx[order[i]]]]--;
                if (xs[i] == xs[i + 1]) continue;
                nl = i + 1; nr = m - nl;
                gl = 1.0; gr = 1.0;
                for (cl = 0; cl < c->n_classes; cl++) {
                    gl -= ((double)left_cnt[cl] / nl) * ((double)left_cnt[cl] / nl);
                    gr -= ((double)right_cnt[cl] / nr) * ((double)right_cnt[cl] / nr);
                }
                g = g0 - ((double)nl / m) * gl - ((double)nr / m) * gr;
                if (g > 1e-15 && g > best_g) { best_g = g; best_f = j; best_t = (xs[i] + xs[i + 1]) / 2.0; }
            }
            free(left_cnt); free(right_cnt);
        }
        free(xs);
    }
    free(order);
    *feat = best_f;
    *thr = best_t;
    *gain = best_g;
}

static gn_tree_node *build(tree_ctx *c, int *idx, int m, int depth)
{
    gn_tree_node *no;
    int feat, i;
    double thr, gain;
    if (c->n_classes <= 1) best_split_reg(c, idx, m, &feat, &thr, &gain);
    else best_split_cls(c, idx, m, &feat, &thr, &gain);
    no = new_node(0, m);
    if (!no) return NULL;
    no->feature = feat;
    no->threshold = thr;
    if (feat < 0 || m < c->min_split || depth >= c->max_depth) {
        no->is_leaf = 1;
        if (c->n_classes <= 1) {
            no->value = node_value(c, idx, m, NULL);
        } else {
            double *cnt = (double *)calloc((size_t)c->n_classes, sizeof(double));
            int bestc = 0;
            if (cnt) {
                for (i = 0; i < m; i++) cnt[c->yc[idx[i]]] += 1.0;
                for (i = 0; i < c->n_classes; i++) if (cnt[i] > cnt[bestc]) bestc = i;
                free(cnt);
            }
            no->value = (double)bestc;
        }
        return no;
    }
    /* partiziona */
    {
        int *li = (int *)malloc((size_t)m * sizeof(int));
        int *ri = (int *)malloc((size_t)m * sizeof(int));
        int nl = 0, nr = 0;
        if (!li || !ri) { free(li); free(ri); free(no); return NULL; }
        for (i = 0; i < m; i++) {
            if (c->X[idx[i] * c->d + feat] <= thr) li[nl++] = idx[i];
            else ri[nr++] = idx[i];
        }
        if (nl == 0 || nr == 0) {
            free(li); free(ri);
            no->is_leaf = 1;
            no->value = c->n_classes <= 1 ? node_value(c, idx, m, NULL) : 0.0;
            return no;
        }
        no->left = build(c, li, nl, depth + 1);
        no->right = build(c, ri, nr, depth + 1);
        free(li); free(ri);
    }
    return no;
}

static int tree_fit(gn_tree *t, const double *X, const void *y, const int *yc,
                    int n, int d, int n_classes, int is_cls)
{
    tree_ctx c;
    int *idx;
    int i;
    if (!t || !X || !y || n < 1 || d < 1) return -1;
    {
        int save_md = t->max_depth, save_ms = t->min_samples_split, save_ml = t->min_samples_leaf;
        memset(t, 0, sizeof(*t));
        t->max_depth = save_md; t->min_samples_split = save_ms; t->min_samples_leaf = save_ml;
    }
    t->n_features = d;
    t->n_classes = n_classes <= 0 ? 1 : n_classes;
    t->max_depth = t->max_depth == 0 ? 32 : t->max_depth;
    t->min_samples_split = t->min_samples_split == 0 ? 2 : t->min_samples_split;
    t->min_samples_leaf = t->min_samples_leaf == 0 ? 1 : t->min_samples_leaf;
    idx = (int *)malloc((size_t)n * sizeof(int));
    if (!idx) return -1;
    for (i = 0; i < n; i++) idx[i] = i;
    c.X = X; c.y = is_cls ? NULL : (const double *)y;
    c.yc = yc; c.n = n; c.d = d; c.n_classes = t->n_classes;
    c.min_split = t->min_samples_split; c.min_leaf = t->min_samples_leaf; c.max_depth = t->max_depth; c.idx = idx;
    t->root = build(&c, idx, n, 0);
    free(idx);
    return t->root ? 0 : -1;
}

int gn_tree_fit_reg(gn_tree *t, const double *X, const double *y, int n, int d)
{
    return tree_fit(t, X, y, NULL, n, d, 1, 0);
}

int gn_tree_fit_cls(gn_tree *t, const double *X, const int *y, int n, int d,
                    int n_classes)
{
    return tree_fit(t, X, y, y, n, d, n_classes, 1);
}

double gn_tree_predict(const gn_tree *t, const double *x)
{
    gn_tree_node *no = t->root;
    while (no && !no->is_leaf) {
        no = x[no->feature] <= no->threshold ? no->left : no->right;
    }
    return no ? no->value : 0.0;
}

static void free_node(gn_tree_node *no)
{
    if (!no) return;
    free_node(no->left);
    free_node(no->right);
    free(no->proba);
    free(no);
}

void gn_tree_free(gn_tree *t)
{
    if (!t) return;
    free_node(t->root);
    t->root = NULL;
}