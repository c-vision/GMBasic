/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gs_boost_boost.c — core di gmbboost: alberi di regressione (3 strategie di crescita) + boosting. */
#include "gs_boost.h"
#include <stdlib.h>
#include <math.h>

typedef struct gs_boost_node {
    int      feat;    /* feature di split, -1 = foglia */
    double   thr;     /* soglia */
    double   leaf;    /* valore (foglia) */
    struct gs_boost_node* l;
    struct gs_boost_node* r;
} gs_boost_node;

struct gs_boost_model {
    gs_boost_engine engine;
    int       n_estimators, max_depth, min_leaf;
    double    lr, base;
    gs_boost_node** trees;
    int       n_trees, n_features;
};

const char* gs_boost_engine_name(gs_boost_engine e) {
    switch (e) { case GS_BOOST_XGB: return "xgb"; case GS_BOOST_LGBM: return "lgbm"; case GS_BOOST_CATBOOST: return "catboost"; }
    return "?";
}

static double mean_y(const double* y, const int* idx, int n) {
    double s = 0; for (int i = 0; i < n; i++) s += y[idx[i]];
    return n > 0 ? s / (double)n : 0.0;
}

/* Miglior split (feature, soglia) che minimizza la SSE dei figli; ritorna gain. */
static int best_split(const double* X, const double* y, const int* idx, int n, int p,
                      int* bf, double* bt, double* bgain) {
    double s = 0, ss = 0;
    for (int i = 0; i < n; i++) { double v = y[idx[i]]; s += v; ss += v * v; }
    double sse_p = ss - s * s / (double)n;
    double best = -1; int bf_ = -1; double bt_ = 0;
    for (int f = 0; f < p; f++) {
        for (int i = 0; i < n; i++) {
            double thr = X[(size_t)idx[i] * p + f];
            double sl = 0, sll = 0, sr = 0, srr = 0; int nl = 0, nr = 0;
            for (int j = 0; j < n; j++) {
                double v = X[(size_t)idx[j] * p + f], yy = y[idx[j]];
                if (v <= thr) { sl += yy; sll += yy * yy; nl++; }
                else          { sr += yy; srr += yy * yy; nr++; }
            }
            if (nl < 1 || nr < 1) continue;
            double sse = (sll - sl * sl / nl) + (srr - sr * sr / nr);
            double gain = sse_p - sse;
            if (gain > best) { best = gain; bf_ = f; bt_ = thr; }
        }
    }
    *bf = bf_; *bt = bt_; *bgain = best;
    return bf_ >= 0;
}

static void partition(const double* X, const int* idx, int n, int p, int f, double t, int* li, int* nl, int* ri, int* nr) {
    *nl = 0; *nr = 0;
    for (int j = 0; j < n; j++) { if (X[(size_t)idx[j] * p + f] <= t) li[(*nl)++] = idx[j]; else ri[(*nr)++] = idx[j]; }
}

/* ── strategia DEPTHWISE (XGB) ── */
static gs_boost_node* build_depthwise(const double* X, const double* y, const int* idx, int n, int p, int depth, int maxd, int minleaf) {
    gs_boost_node* nd = (gs_boost_node*)calloc(1, sizeof *nd); nd->feat = -1; nd->leaf = mean_y(y, idx, n);
    if (depth >= maxd || n < 2 * minleaf) return nd;
    int bf; double bt, g;
    if (!best_split(X, y, idx, n, p, &bf, &bt, &g) || g <= 0.0) return nd;
    int* li = (int*)malloc((size_t)n * sizeof(int)); int* ri = (int*)malloc((size_t)n * sizeof(int)); int nl, nr;
    partition(X, idx, n, p, bf, bt, li, &nl, ri, &nr);
    nd->feat = bf; nd->thr = bt;
    nd->l = build_depthwise(X, y, li, nl, p, depth + 1, maxd, minleaf);
    nd->r = build_depthwise(X, y, ri, nr, p, depth + 1, maxd, minleaf);
    free(li); free(ri);
    return nd;
}

/* ── strategia LEAFWISE / best-first (LGBM) ── */
typedef struct { gs_boost_node* nd; int* idx; int n; int depth; } leaf_t;

static gs_boost_node* build_leafwise(const double* X, const double* y, const int* idx0, int n0, int p, int maxd, int minleaf) {
    gs_boost_node* root = (gs_boost_node*)calloc(1, sizeof *root); root->feat = -1; root->leaf = mean_y(y, idx0, n0);
    int cap = 1 << (maxd + 1);
    leaf_t* lv = (leaf_t*)malloc((size_t)cap * sizeof(leaf_t)); int nl = 0;
    int* a = (int*)malloc((size_t)n0 * sizeof(int)); for (int i = 0; i < n0; i++) a[i] = idx0[i];
    lv[nl].nd = root; lv[nl].idx = a; lv[nl].n = n0; lv[nl].depth = 0; nl++;
    int max_nodes = (1 << maxd) - 1, nodes = 1;
    while (nodes < max_nodes) {
        int best = -1, bbf = -1; double bbt = 0, bg = 0;
        for (int k = 0; k < nl; k++) {
            if (lv[k].depth >= maxd || lv[k].n < 2 * minleaf) continue;
            int bf; double bt, g;
            if (best_split(X, y, lv[k].idx, lv[k].n, p, &bf, &bt, &g) && g > bg) { bg = g; best = k; bbf = bf; bbt = bt; }
        }
        if (best < 0 || bg <= 0) break;
        leaf_t sp = lv[best];
        int* li = (int*)malloc((size_t)sp.n * sizeof(int)); int* ri = (int*)malloc((size_t)sp.n * sizeof(int)); int nli, nri;
        partition(X, sp.idx, sp.n, p, bbf, bbt, li, &nli, ri, &nri);
        sp.nd->feat = bbf; sp.nd->thr = bbt;
        sp.nd->l = (gs_boost_node*)calloc(1, sizeof(gs_boost_node)); sp.nd->l->feat = -1; sp.nd->l->leaf = mean_y(y, li, nli);
        sp.nd->r = (gs_boost_node*)calloc(1, sizeof(gs_boost_node)); sp.nd->r->feat = -1; sp.nd->r->leaf = mean_y(y, ri, nri);
        lv[best].nd = sp.nd->l; lv[best].idx = li; lv[best].n = nli; lv[best].depth = sp.depth + 1;
        lv[nl].nd = sp.nd->r;   lv[nl].idx = ri;  lv[nl].n = nri;  lv[nl].depth = sp.depth + 1; nl++;
        free(sp.idx);
        nodes++;
    }
    for (int k = 0; k < nl; k++) free(lv[k].idx);
    free(lv);
    return root;
}

/* ── strategia SYMMETRIC / oblivious (CatBoost): una soglia per LIVELLO ── */
static gs_boost_node* build_symmetric(const double* X, const double* y, const int* idx0, int n0, int p, int maxd, int minleaf) {
    gs_boost_node* root = (gs_boost_node*)calloc(1, sizeof *root); root->feat = -1; root->leaf = mean_y(y, idx0, n0);
    int cap = 1 << (maxd + 1);
    leaf_t* lv = (leaf_t*)malloc((size_t)cap * sizeof(leaf_t)); int nl = 0;
    int* a = (int*)malloc((size_t)n0 * sizeof(int)); for (int i = 0; i < n0; i++) a[i] = idx0[i];
    lv[nl].nd = root; lv[nl].idx = a; lv[nl].n = n0; lv[nl].depth = 0; nl++;
    for (int depth = 0; depth < maxd; depth++) {
        /* concatena gli indici del livello e scegle UNA soglia valida per tutto il livello */
        int tot = 0; for (int k = 0; k < nl; k++) tot += lv[k].n;
        if (tot == 0) break;
        int* comb = (int*)malloc((size_t)tot * sizeof(int)); int c = 0;
        for (int k = 0; k < nl; k++) for (int i = 0; i < lv[k].n; i++) comb[c++] = lv[k].idx[i];
        int bf; double bt, g;
        int ok = (tot >= 2 * minleaf) && best_split(X, y, comb, tot, p, &bf, &bt, &g) && g > 0;
        free(comb);
        if (!ok) break;
        leaf_t* nxt = (leaf_t*)malloc((size_t)cap * sizeof(leaf_t)); int nn = 0;
        for (int k = 0; k < nl; k++) {
            leaf_t sp = lv[k];
            int* li = (int*)malloc((size_t)sp.n * sizeof(int)); int* ri = (int*)malloc((size_t)sp.n * sizeof(int)); int nli, nri;
            partition(X, sp.idx, sp.n, p, bf, bt, li, &nli, ri, &nri);
            sp.nd->feat = bf; sp.nd->thr = bt;
            sp.nd->l = (gs_boost_node*)calloc(1, sizeof(gs_boost_node)); sp.nd->l->feat = -1; sp.nd->l->leaf = mean_y(y, li, nli);
            sp.nd->r = (gs_boost_node*)calloc(1, sizeof(gs_boost_node)); sp.nd->r->feat = -1; sp.nd->r->leaf = mean_y(y, ri, nri);
            nxt[nn].nd = sp.nd->l; nxt[nn].idx = li; nxt[nn].n = nli; nxt[nn].depth = depth + 1; nn++;
            nxt[nn].nd = sp.nd->r; nxt[nn].idx = ri; nxt[nn].n = nri; nxt[nn].depth = depth + 1; nn++;
            free(sp.idx);
        }
        free(lv); lv = nxt; nl = nn;
    }
    for (int k = 0; k < nl; k++) free(lv[k].idx);
    free(lv);
    return root;
}

static gs_boost_node* tree_fit(gs_boost_engine e, const double* X, const double* y, const int* idx, int n, int p, int maxd, int minleaf) {
    switch (e) {
        case GS_BOOST_LGBM:    return build_leafwise(X, y, idx, n, p, maxd, minleaf);
        case GS_BOOST_CATBOOST: return build_symmetric(X, y, idx, n, p, maxd, minleaf);
        default:          return build_depthwise(X, y, idx, n, p, 0, maxd, minleaf);
    }
}

static double tree_predict(const gs_boost_node* n, const double* x) {
    while (n->feat >= 0) n = (x[n->feat] <= n->thr) ? n->l : n->r;
    return n->leaf;
}

static void tree_free(gs_boost_node* n) { if (!n) return; tree_free(n->l); tree_free(n->r); free(n); }

/* ── modello ── */
gs_boost_model* gs_boost_new(gs_boost_engine engine, int n_estimators, double lr, int max_depth) {
    if (n_estimators <= 0 || max_depth < 1 || max_depth > 20) return NULL;
    gs_boost_model* m = (gs_boost_model*)calloc(1, sizeof *m);
    if (!m) return NULL;
    m->engine = engine; m->n_estimators = n_estimators;
    m->lr = (lr > 0.0 && lr <= 1.0) ? lr : 0.1;
    m->max_depth = max_depth; m->min_leaf = 1;
    return m;
}

void gs_boost_free(gs_boost_model* m) {
    if (!m) return;
    for (int i = 0; i < m->n_trees; i++) tree_free(m->trees[i]);
    free(m->trees); free(m);
}

int gs_boost_fit(gs_boost_model* m, const double* X, const double* y, int n, int p) {
    if (!m || !X || !y || n <= 1 || p <= 0) return -1;
    m->n_features = p;
    double s = 0; for (int i = 0; i < n; i++) s += y[i]; m->base = s / (double)n;

    double* f = (double*)malloc((size_t)n * sizeof(double));
    double* r = (double*)malloc((size_t)n * sizeof(double));
    int* idx = (int*)malloc((size_t)n * sizeof(int));
    if (!f || !r || !idx) { free(f); free(r); free(idx); return -1; }
    for (int i = 0; i < n; i++) { f[i] = m->base; idx[i] = i; }
    m->trees = (gs_boost_node**)malloc((size_t)m->n_estimators * sizeof(gs_boost_node*));
    m->n_trees = 0;
    for (int t = 0; t < m->n_estimators; t++) {
        for (int i = 0; i < n; i++) r[i] = y[i] - f[i];
        gs_boost_node* tree = tree_fit(m->engine, X, r, idx, n, p, m->max_depth, m->min_leaf);
        if (!tree) break;
        m->trees[m->n_trees++] = tree;
        for (int i = 0; i < n; i++) f[i] += m->lr * tree_predict(tree, X + (size_t)i * p);
    }
    free(f); free(r); free(idx);
    return 0;
}

double gs_boost_predict(const gs_boost_model* m, const double* x) {
    if (!m || !x) return 0.0;
    double v = m->base;
    for (int i = 0; i < m->n_trees; i++) v += m->lr * tree_predict(m->trees[i], x);
    return v;
}

double gs_boost_score(const gs_boost_model* m, const double* X, const double* y, int n, int p) {
    if (!m || !X || !y || n <= 0) return (double)NAN;
    double ym = 0; for (int i = 0; i < n; i++) ym += y[i]; ym /= (double)n;
    double sst = 0, sse = 0;
    for (int i = 0; i < n; i++) {
        double pr = gs_boost_predict(m, X + (size_t)i * p);
        sst += (y[i] - ym) * (y[i] - ym);
        sse += (y[i] - pr) * (y[i] - pr);
    }
    return sst > 0 ? 1.0 - sse / sst : 0.0;
}

int gs_boost_n_trees(const gs_boost_model* m) { return m ? m->n_trees : 0; }
