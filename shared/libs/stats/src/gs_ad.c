/*
 * gs_ad.c — nastro AAD reverse-mode (operazioni scalari).
 */
#include "gs_ad.h"
#include <math.h>

void gs_ad_init(gs_ad *t)
{
    int i;
    if (!t) return;
    t->n = 0;
    for (i = 0; i < GS_AD_MAXNODE; i++) { t->op[i]=0; t->val[i]=0.0; t->adj[i]=0.0; t->a[i]=-1; t->b[i]=-1; t->c[i]=0.0; }
}
static int gs_ad_push(gs_ad *t, int op, double v, int a, int b, double c)
{
    int id;
    if (!t || t->n >= GS_AD_MAXNODE) return -1;
    id = t->n++;
    t->op[id]=op; t->val[id]=v; t->adj[id]=0.0; t->a[id]=a; t->b[id]=b; t->c[id]=c;
    return id;
}
int gs_ad_var(gs_ad *t, double v) { return gs_ad_push(t, 0, v, -1, -1, 0.0); }
int gs_ad_add(gs_ad *t, int a, int b) { return gs_ad_push(t, 1, t->val[a]+t->val[b], a, b, 0.0); }
int gs_ad_sub(gs_ad *t, int a, int b) { return gs_ad_push(t, 2, t->val[a]-t->val[b], a, b, 0.0); }
int gs_ad_mul(gs_ad *t, int a, int b) { return gs_ad_push(t, 3, t->val[a]*t->val[b], a, b, 0.0); }
int gs_ad_div(gs_ad *t, int a, int b) { return gs_ad_push(t, 4, t->val[a]/t->val[b], a, b, 0.0); }
int gs_ad_scale(gs_ad *t, int a, double k) { return gs_ad_push(t, 3, t->val[a]*k, a, -1, k); }
int gs_ad_exp(gs_ad *t, int a) { return gs_ad_push(t, 5, exp(t->val[a]), a, -1, 0.0); }
int gs_ad_log(gs_ad *t, int a) { return gs_ad_push(t, 6, log(t->val[a]), a, -1, 0.0); }
int gs_ad_sqrt(gs_ad *t, int a) { return gs_ad_push(t, 7, sqrt(t->val[a]), a, -1, 0.0); }
int gs_ad_pow(gs_ad *t, int a, double p) { return gs_ad_push(t, 8, pow(t->val[a], p), a, -1, p); }
int gs_ad_neg(gs_ad *t, int a) { return gs_ad_push(t, 9, -t->val[a], a, -1, 0.0); }
int gs_ad_sin(gs_ad *t, int a) { return gs_ad_push(t, 10, sin(t->val[a]), a, -1, 0.0); }
int gs_ad_cos(gs_ad *t, int a) { return gs_ad_push(t, 11, cos(t->val[a]), a, -1, 0.0); }

void gs_ad_backward(gs_ad *t, int root)
{
    int k;
    if (!t || root < 0 || root >= t->n) return;
    for (k = 0; k < t->n; k++) t->adj[k] = 0.0;
    t->adj[root] = 1.0;
    for (k = t->n - 1; k >= 0; k--) {
        double g = t->adj[k], va, vb;
        if (t->op[k] == 0) continue;
        va = (t->a[k] >= 0) ? t->val[t->a[k]] : 0.0;
        vb = (t->b[k] >= 0) ? t->val[t->b[k]] : t->c[k];
        switch (t->op[k]) {
        case 1: t->adj[t->a[k]] += g; t->adj[t->b[k]] += g; break;
        case 2: t->adj[t->a[k]] += g; t->adj[t->b[k]] -= g; break;
        case 3: t->adj[t->a[k]] += g*vb; if (t->b[k] >= 0) t->adj[t->b[k]] += g*va; break;
        case 4: t->adj[t->a[k]] += g/vb; t->adj[t->b[k]] -= g*va/(vb*vb); break;
        case 5: t->adj[t->a[k]] += g*t->val[k]; break;
        case 6: t->adj[t->a[k]] += g/va; break;
        case 7: t->adj[t->a[k]] += g*0.5/t->val[k]; break;
        case 8: t->adj[t->a[k]] += g*t->c[k]*pow(va, t->c[k]-1.0); break;
        case 9: t->adj[t->a[k]] -= g; break;
        case 10: t->adj[t->a[k]] += g*cos(va); break;
        case 11: t->adj[t->a[k]] -= g*sin(va); break;
        default: break;
        }
    }
}
double gs_ad_val(const gs_ad *t, int id) { return (t && id>=0 && id<t->n) ? t->val[id] : 0.0/0.0; }
double gs_ad_grad(const gs_ad *t, int id) { return (t && id>=0 && id<t->n) ? t->adj[id] : 0.0/0.0; }
