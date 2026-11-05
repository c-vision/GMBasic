/*
 * gs_ad.h — differentiated programming a rovescio (AAD) a nastro.
 *
 * Da Sources/C++ (WilmottArticle, QuantResearch): differential programming
 * a rovescio per Greci/sensibilita' rapidi. Nastro di operazioni scalari;
 * gs_ad_backward propaga gli aggiunti. Solo libc.
 */
#ifndef GMBSTATS_GS_AD_H
#define GMBSTATS_GS_AD_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GS_AD_MAXNODE 8192

typedef struct {
    int    n;
    int    op[GS_AD_MAXNODE];    /* 0 const,1 add,2 sub,3 mul,4 div,5 exp,6 log,7 sqrt,8 pow,9 neg,10 sin,11 cos */
    double val[GS_AD_MAXNODE];
    double adj[GS_AD_MAXNODE];
    int    a[GS_AD_MAXNODE];
    int    b[GS_AD_MAXNODE];
    double c[GS_AD_MAXNODE];     /* costante (pow) */
} gs_ad;

void gs_ad_init(gs_ad *t);
/* Nodo costante/variabile; ritorna l'id (>=0) o -1 se pieno. */
int  gs_ad_var(gs_ad *t, double v);
int  gs_ad_add(gs_ad *t, int a, int b);
int  gs_ad_sub(gs_ad *t, int a, int b);
int  gs_ad_mul(gs_ad *t, int a, int b);
int  gs_ad_div(gs_ad *t, int a, int b);
int  gs_ad_scale(gs_ad *t, int a, double k);
int  gs_ad_exp(gs_ad *t, int a);
int  gs_ad_log(gs_ad *t, int a);
int  gs_ad_sqrt(gs_ad *t, int a);
int  gs_ad_pow(gs_ad *t, int a, double p);
int  gs_ad_neg(gs_ad *t, int a);
int  gs_ad_sin(gs_ad *t, int a);
int  gs_ad_cos(gs_ad *t, int a);
/* Propaga gli aggiunti a partire da root (adj=1) e azzera. */
void gs_ad_backward(gs_ad *t, int root);
double gs_ad_val(const gs_ad *t, int id);
double gs_ad_grad(const gs_ad *t, int id);   /* aggiunto dopo backward */

#ifdef __cplusplus
}
#endif
#endif /* GMBSTATS_GS_AD_H */
