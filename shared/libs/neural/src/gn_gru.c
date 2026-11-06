/*
 * gn_gru.c — GRU forward/backward (da xiph/rnnoise, BSD-3).
 */
#include "gn_gru.h"
#include "gn_act.h"
#include <stdlib.h>
#include <string.h>

static gn_status alloc_pair(gn_mat *w, gn_mat *g, int r, int c)
{
    if (gn_mat_alloc(w, r, c) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(g, r, c) != GN_OK) { gn_mat_free(w); return GN_ERR_NO_MEM; }
    return GN_OK;
}

static void free_pair(gn_mat *w, gn_mat *g) { gn_mat_free(w); gn_mat_free(g); }

static void rand_fill(gn_mat *m, gn_real scale, unsigned long long *s)
{
    int i, n = m->rows * m->cols;
    for (i = 0; i < n; i++) {
        *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
        m->data[i] = scale * (((gn_real)((*s >> 11) & 0x1FFFFF)) / 1048576.0 - 1.0);
    }
}

gn_status gn_gru_init(gn_gru *g, int in, int hidden, int out, unsigned long long seed)
{
    unsigned long long s = seed ? seed : 1;
    gn_real sc = 0.5;
    if (!g || in < 1 || hidden < 1 || out < 1) return GN_ERR_ARG;
    memset(g, 0, sizeof(*g));
    g->in = in; g->hidden = hidden; g->out = out;
    if (alloc_pair(&g->Wz, &g->gWz, hidden, in) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->Uz, &g->gUz, hidden, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->bz, &g->gbz, 1, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->Wr, &g->gWr, hidden, in) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->Ur, &g->gUr, hidden, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->br, &g->gbr, 1, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->Wn, &g->gWn, hidden, in) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->Un, &g->gUn, hidden, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->bn, &g->gbn, 1, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->Wo, &g->gWo, out, hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (alloc_pair(&g->bo, &g->gbo, 1, out) != GN_OK) return GN_ERR_NO_MEM;
    rand_fill(&g->Wz, sc, &s); rand_fill(&g->Uz, sc, &s); gn_mat_zero(&g->bz);
    rand_fill(&g->Wr, sc, &s); rand_fill(&g->Ur, sc, &s); gn_mat_zero(&g->br);
    rand_fill(&g->Wn, sc, &s); rand_fill(&g->Un, sc, &s); gn_mat_zero(&g->bn);
    rand_fill(&g->Wo, sc, &s); gn_mat_zero(&g->bo);
    gn_gru_zero_grad(g);
    return GN_OK;
}

void gn_gru_free(gn_gru *g)
{
    if (!g) return;
    free_pair(&g->Wz, &g->gWz); free_pair(&g->Uz, &g->gUz); free_pair(&g->bz, &g->gbz);
    free_pair(&g->Wr, &g->gWr); free_pair(&g->Ur, &g->gUr); free_pair(&g->br, &g->gbr);
    free_pair(&g->Wn, &g->gWn); free_pair(&g->Un, &g->gUn); free_pair(&g->bn, &g->gbn);
    free_pair(&g->Wo, &g->gWo); free_pair(&g->bo, &g->gbo);
    gn_mat_free(&g->X); gn_mat_free(&g->Z); gn_mat_free(&g->R); gn_mat_free(&g->N); gn_mat_free(&g->H);
    memset(g, 0, sizeof(*g));
}

gn_status gn_gru_zero_grad(gn_gru *g)
{
    gn_mat_zero(&g->gWz); gn_mat_zero(&g->gUz); gn_mat_zero(&g->gbz);
    gn_mat_zero(&g->gWr); gn_mat_zero(&g->gUr); gn_mat_zero(&g->gbr);
    gn_mat_zero(&g->gWn); gn_mat_zero(&g->gUn); gn_mat_zero(&g->gbn);
    gn_mat_zero(&g->gWo); gn_mat_zero(&g->gbo);
    return GN_OK;
}

gn_status gn_gru_forward(gn_gru *g, const gn_mat *x, gn_mat *y)
{
    int t, i, j, T;
    gn_mat az, ar, an, tmp;
    if (!g || !x || !y) return GN_ERR_ARG;
    T = x->rows;
    if (x->cols != g->in || y->rows != T || y->cols != g->out) return GN_ERR_ARG;
    gn_mat_free(&g->X); gn_mat_free(&g->Z); gn_mat_free(&g->R); gn_mat_free(&g->N); gn_mat_free(&g->H);
    if (gn_mat_alloc(&g->X, T, g->in) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&g->Z, T, g->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&g->R, T, g->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&g->N, T, g->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&g->H, T + 1, g->hidden) != GN_OK) return GN_ERR_NO_MEM;
    gn_mat_copy(&g->X, x);
    gn_mat_zero(&g->H);
    if (gn_mat_alloc(&az, 1, g->hidden) != GN_OK) return GN_ERR_NO_MEM;
    if (gn_mat_alloc(&ar, 1, g->hidden) != GN_OK) { gn_mat_free(&az); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&an, 1, g->hidden) != GN_OK) { gn_mat_free(&az); gn_mat_free(&ar); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&tmp, 1, g->hidden) != GN_OK) { gn_mat_free(&az); gn_mat_free(&ar); gn_mat_free(&an); return GN_ERR_NO_MEM; }
    g->T = T;
    for (t = 0; t < T; t++) {
        const gn_real *xt = x->data + (size_t)t * g->in;
        const gn_real *hp = g->H.data + (size_t)t * g->hidden;
        gn_real *h = g->H.data + (size_t)(t + 1) * g->hidden;
        /* az = Wz x + Uz h + bz */
        for (j = 0; j < g->hidden; j++) {
            gn_real v = g->bz.data[j];
            for (i = 0; i < g->in; i++) v += g->Wz.data[(size_t)j * g->in + i] * xt[i];
            for (i = 0; i < g->hidden; i++) v += g->Uz.data[(size_t)j * g->hidden + i] * hp[i];
            az.data[j] = v;
        }
        for (j = 0; j < g->hidden; j++) g->Z.data[(size_t)t * g->hidden + j] = gn_sigmoid(az.data[j]);
        /* ar = Wr x + Ur h + br */
        for (j = 0; j < g->hidden; j++) {
            gn_real v = g->br.data[j];
            for (i = 0; i < g->in; i++) v += g->Wr.data[(size_t)j * g->in + i] * xt[i];
            for (i = 0; i < g->hidden; i++) v += g->Ur.data[(size_t)j * g->hidden + i] * hp[i];
            ar.data[j] = v;
        }
        for (j = 0; j < g->hidden; j++) g->R.data[(size_t)t * g->hidden + j] = gn_sigmoid(ar.data[j]);
        /* an = Wn x + Un (r . h) + bn */
        for (j = 0; j < g->hidden; j++) tmp.data[j] = g->R.data[(size_t)t * g->hidden + j] * hp[j];
        for (j = 0; j < g->hidden; j++) {
            gn_real v = g->bn.data[j];
            for (i = 0; i < g->in; i++) v += g->Wn.data[(size_t)j * g->in + i] * xt[i];
            for (i = 0; i < g->hidden; i++) v += g->Un.data[(size_t)j * g->hidden + i] * tmp.data[i];
            an.data[j] = v;
        }
        for (j = 0; j < g->hidden; j++) g->N.data[(size_t)t * g->hidden + j] = gn_tanh(an.data[j]);
        /* h = (1-z) . n + z . hp */
        for (j = 0; j < g->hidden; j++) {
            gn_real zt = g->Z.data[(size_t)t * g->hidden + j];
            gn_real nt = g->N.data[(size_t)t * g->hidden + j];
            h[j] = (1.0 - zt) * nt + zt * hp[j];
        }
        /* y_t = Wo h + bo */
        for (j = 0; j < g->out; j++) {
            gn_real v = g->bo.data[j];
            for (i = 0; i < g->hidden; i++) v += g->Wo.data[(size_t)j * g->hidden + i] * h[i];
            y->data[(size_t)t * g->out + j] = v;
        }
    }
    gn_mat_free(&az); gn_mat_free(&ar); gn_mat_free(&an); gn_mat_free(&tmp);
    return GN_OK;
}

gn_status gn_gru_backward(gn_gru *g, const gn_mat *grad_y)
{
    int t, i, j, T = g->T;
    gn_mat dH, dh, dz, dn, dan, drh, dr, dar, daz, tmp;
    if (!g || !grad_y || grad_y->rows != T || grad_y->cols != g->out) return GN_ERR_ARG;
    if (gn_mat_alloc(&dH, T + 1, g->hidden) != GN_OK) return GN_ERR_NO_MEM;
    gn_mat_zero(&dH);
    /* gradienti uscita */
    for (j = 0; j < g->out; j++) g->gbo.data[j] += 0.0;
    for (t = 0; t < T; t++)
        for (j = 0; j < g->out; j++) g->gbo.data[j] += grad_y->data[(size_t)t * g->out + j];
    /* gWo += grad_y^T H[1..T] */
    for (t = 0; t < T; t++)
        for (j = 0; j < g->out; j++) {
            gn_real gy = grad_y->data[(size_t)t * g->out + j];
            if (gy == 0.0) continue;
            for (i = 0; i < g->hidden; i++)
                g->gWo.data[(size_t)j * g->hidden + i] += gy * g->H.data[(size_t)(t + 1) * g->hidden + i];
        }
    /* dH[T] += grad_y Wo */
    for (t = 0; t < T; t++)
        for (i = 0; i < g->hidden; i++) {
            gn_real s = 0.0;
            for (j = 0; j < g->out; j++) s += grad_y->data[(size_t)t * g->out + j] * g->Wo.data[(size_t)j * g->hidden + i];
            dH.data[(size_t)(t + 1) * g->hidden + i] += s;
        }
    if (gn_mat_alloc(&dh, 1, g->hidden) != GN_OK) { gn_mat_free(&dH); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&dz, 1, g->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dh); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&dn, 1, g->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dh); gn_mat_free(&dz); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&dan, 1, g->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dh); gn_mat_free(&dz); gn_mat_free(&dn); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&drh, 1, g->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dh); gn_mat_free(&dz); gn_mat_free(&dn); gn_mat_free(&dan); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&dr, 1, g->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dh); gn_mat_free(&dz); gn_mat_free(&dn); gn_mat_free(&dan); gn_mat_free(&drh); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&dar, 1, g->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dh); gn_mat_free(&dz); gn_mat_free(&dn); gn_mat_free(&dan); gn_mat_free(&drh); gn_mat_free(&dr); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&daz, 1, g->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dh); gn_mat_free(&dz); gn_mat_free(&dn); gn_mat_free(&dan); gn_mat_free(&drh); gn_mat_free(&dr); gn_mat_free(&dar); return GN_ERR_NO_MEM; }
    if (gn_mat_alloc(&tmp, 1, g->hidden) != GN_OK) { gn_mat_free(&dH); gn_mat_free(&dh); gn_mat_free(&dz); gn_mat_free(&dn); gn_mat_free(&dan); gn_mat_free(&drh); gn_mat_free(&dr); gn_mat_free(&dar); gn_mat_free(&daz); return GN_ERR_NO_MEM; }
    for (t = T - 1; t >= 0; t--) {
        const gn_real *xt = g->X.data + (size_t)t * g->in;
        const gn_real *hp = g->H.data + (size_t)t * g->hidden;
        const gn_real *zt = g->Z.data + (size_t)t * g->hidden;
        const gn_real *rt = g->R.data + (size_t)t * g->hidden;
        const gn_real *nt = g->N.data + (size_t)t * g->hidden;
        gn_real *dHt = dH.data + (size_t)(t + 1) * g->hidden;
        gn_real *dHp = dH.data + (size_t)t * g->hidden;
        for (j = 0; j < g->hidden; j++) dh.data[j] = dHt[j];
        /* dz_pre = dh . (hp - n) ; dn = dh . (1 - z) */
        for (j = 0; j < g->hidden; j++) { dz.data[j] = dh.data[j] * (hp[j] - nt[j]); dn.data[j] = dh.data[j] * (1.0 - zt[j]); }
        /* h_prev riceve dh . z */
        for (j = 0; j < g->hidden; j++) dHp[j] += dh.data[j] * zt[j];
        /* candidate: dan = dn . (1 - n^2) */
        for (j = 0; j < g->hidden; j++) dan.data[j] = dn.data[j] * (1.0 - nt[j] * nt[j]);
        for (j = 0; j < g->hidden; j++) {
            gn_real da = dan.data[j];
            g->gbn.data[j] += da;
            if (da == 0.0) continue;
            for (i = 0; i < g->in; i++) g->gWn.data[(size_t)j * g->in + i] += da * xt[i];
            for (i = 0; i < g->hidden; i++) g->gUn.data[(size_t)j * g->hidden + i] += da * (rt[i] * hp[i]);
        }
        /* d(r . h) = Un^T dan */
        for (i = 0; i < g->hidden; i++) {
            gn_real s = 0.0;
            for (j = 0; j < g->hidden; j++) s += g->Un.data[(size_t)j * g->hidden + i] * dan.data[j];
            drh.data[i] = s;
        }
        /* dr = d(r.h) . h ; h_prev += d(r.h) . r */
        for (j = 0; j < g->hidden; j++) { dr.data[j] = drh.data[j] * hp[j]; dHp[j] += drh.data[j] * rt[j]; }
        for (j = 0; j < g->hidden; j++) dar.data[j] = dr.data[j] * rt[j] * (1.0 - rt[j]);
        for (j = 0; j < g->hidden; j++) {
            gn_real da = dar.data[j];
            g->gbr.data[j] += da;
            if (da == 0.0) continue;
            for (i = 0; i < g->in; i++) g->gWr.data[(size_t)j * g->in + i] += da * xt[i];
            for (i = 0; i < g->hidden; i++) g->gUr.data[(size_t)j * g->hidden + i] += da * hp[i];
        }
        /* h_prev += Ur^T dar */
        for (i = 0; i < g->hidden; i++) {
            gn_real s = 0.0;
            for (j = 0; j < g->hidden; j++) s += g->Ur.data[(size_t)j * g->hidden + i] * dar.data[j];
            dHp[i] += s;
        }
        /* update gate: daz = dz . z . (1 - z) */
        for (j = 0; j < g->hidden; j++) daz.data[j] = dz.data[j] * zt[j] * (1.0 - zt[j]);
        for (j = 0; j < g->hidden; j++) {
            gn_real da = daz.data[j];
            g->gbz.data[j] += da;
            if (da == 0.0) continue;
            for (i = 0; i < g->in; i++) g->gWz.data[(size_t)j * g->in + i] += da * xt[i];
            for (i = 0; i < g->hidden; i++) g->gUz.data[(size_t)j * g->hidden + i] += da * hp[i];
        }
        for (i = 0; i < g->hidden; i++) {
            gn_real s = 0.0;
            for (j = 0; j < g->hidden; j++) s += g->Uz.data[(size_t)j * g->hidden + i] * daz.data[j];
            dHp[i] += s;
        }
        (void)tmp;
    }
    gn_mat_free(&dH); gn_mat_free(&dh); gn_mat_free(&dz); gn_mat_free(&dn);
    gn_mat_free(&dan); gn_mat_free(&drh); gn_mat_free(&dr); gn_mat_free(&dar);
    gn_mat_free(&daz); gn_mat_free(&tmp);
    return GN_OK;
}
