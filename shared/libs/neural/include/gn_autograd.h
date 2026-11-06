/*
 * gn_autograd.h — motore di autodiff scalare (reverse-mode) da micrograd.c.
 *
 * Grafo computazionale dinamico di nodi scalari (gn_value). Ogni operazione
 * crea un nodo con i figli e una funzione backward; gn_value_backward fa
 * l'ordinamento topologico e la backpropagation in reverse-mode.
 */
#ifndef GMBNEURAL_GN_AUTOGRAD_H
#define GMBNEURAL_GN_AUTOGRAD_H

#include "gn_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gn_value gn_value;
struct gn_value {
    gn_real data;
    gn_real grad;
    gn_value **prev;       /* nodi figli (input dell'operazione) */
    int nprev;
    void (*backward)(gn_value *);
    const char *op;
};

/* Crea un nodo foglia (children=NULL) o di operazione. */
gn_value *gn_value_new(gn_real data, gn_value **children, int n, const char *op);
void gn_value_free(gn_value *v);

gn_value *gn_value_add(gn_value *a, gn_value *b);
gn_value *gn_value_mul(gn_value *a, gn_value *b);
gn_value *gn_value_sub(gn_value *a, gn_value *b);
gn_value *gn_value_div(gn_value *a, gn_value *b);
gn_value *gn_value_pow(gn_value *a, gn_real p);
gn_value *gn_value_neg(gn_value *a);
gn_value *gn_value_relu(gn_value *a);
gn_value *gn_value_tanh(gn_value *a);
gn_value *gn_value_exp(gn_value *a);

/* Backprop: azzera i grad, ordina topologicamente da v e accumula i gradienti. */
void gn_value_backward(gn_value *v);

#ifdef __cplusplus
}
#endif

#endif /* GMBNEURAL_GN_AUTOGRAD_H */
