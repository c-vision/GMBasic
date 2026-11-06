/*
 * gn_autograd.c — autodiff scalare (da Jaykef/micrograd.c).
 */
#include "gn_autograd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

gn_value *gn_value_new(gn_real data, gn_value **children, int n, const char *op)
{
    gn_value *v = (gn_value *)malloc(sizeof(gn_value));
    if (!v) return NULL;
    v->data = data;
    v->grad = 0.0;
    v->nprev = n;
    v->op = op;
    v->backward = NULL;
    if (n > 0 && children) {
        v->prev = (gn_value **)malloc((size_t)n * sizeof(gn_value *));
        memcpy(v->prev, children, (size_t)n * sizeof(gn_value *));
    } else {
        v->prev = NULL;
    }
    return v;
}

void gn_value_free(gn_value *v)
{
    if (!v) return;
    free(v->prev);
    free(v);
}

/* --- operazioni --- */

static void add_backward(gn_value *v)
{
    v->prev[0]->grad += v->grad;
    v->prev[1]->grad += v->grad;
}
gn_value *gn_value_add(gn_value *a, gn_value *b)
{
    gn_value *c[2]; gn_value *out;
    c[0] = a; c[1] = b;
    out = gn_value_new(a->data + b->data, c, 2, "+");
    out->backward = add_backward;
    return out;
}

static void mul_backward(gn_value *v)
{
    v->prev[0]->grad += v->grad * v->prev[1]->data;
    v->prev[1]->grad += v->grad * v->prev[0]->data;
}
gn_value *gn_value_mul(gn_value *a, gn_value *b)
{
    gn_value *c[2]; gn_value *out;
    c[0] = a; c[1] = b;
    out = gn_value_new(a->data * b->data, c, 2, "*");
    out->backward = mul_backward;
    return out;
}

gn_value *gn_value_sub(gn_value *a, gn_value *b) { return gn_value_add(a, gn_value_neg(b)); }

static void div_backward(gn_value *v)
{
    gn_value *a = v->prev[0], *b = v->prev[1];
    a->grad += v->grad / b->data;
    b->grad += -v->grad * a->data / (b->data * b->data);
}
gn_value *gn_value_div(gn_value *a, gn_value *b)
{
    gn_value *c[2]; gn_value *out;
    c[0] = a; c[1] = b;
    out = gn_value_new(a->data / b->data, c, 2, "/");
    out->backward = div_backward;
    return out;
}

static gn_real g_pow;
static void pow_backward(gn_value *v)
{
    gn_real base = v->prev[0]->data;
    v->prev[0]->grad += v->grad * g_pow * pow(base, g_pow - 1.0);
}
gn_value *gn_value_pow(gn_value *a, gn_real p)
{
    gn_value *out;
    g_pow = p;
    out = gn_value_new(pow(a->data, p), &a, 1, "pow");
    out->backward = pow_backward;
    return out;
}

static void neg_backward(gn_value *v) { v->prev[0]->grad += -v->grad; }
gn_value *gn_value_neg(gn_value *a)
{
    gn_value *out = gn_value_new(-a->data, &a, 1, "neg");
    out->backward = neg_backward;
    return out;
}

static void relu_backward(gn_value *v) { v->prev[0]->grad += (v->data > 0.0) ? v->grad : 0.0; }
gn_value *gn_value_relu(gn_value *a)
{
    gn_value *out = gn_value_new(a->data > 0.0 ? a->data : 0.0, &a, 1, "relu");
    out->backward = relu_backward;
    return out;
}

static void tanh_backward(gn_value *v) { v->prev[0]->grad += (1.0 - v->data * v->data) * v->grad; }
gn_value *gn_value_tanh(gn_value *a)
{
    gn_value *out = gn_value_new(tanh(a->data), &a, 1, "tanh");
    out->backward = tanh_backward;
    return out;
}

static void exp_backward(gn_value *v) { v->prev[0]->grad += v->data * v->grad; }
gn_value *gn_value_exp(gn_value *a)
{
    gn_value *out = gn_value_new(exp(a->data), &a, 1, "exp");
    out->backward = exp_backward;
    return out;
}

/* --- backward con ordinamento topologico --- */

void gn_value_backward(gn_value *v)
{
    /* raccolta nodi + ordinamento topologico iterativo con stack esplicito */
    gn_value **stack = NULL, **order = NULL;
    int nstack = 0, capstack = 0, norder = 0, caporder = 0;
    int i;
    /* mappa nodo -> indice per "seen" (uso un array di puntatori visitati) */
    gn_value **visited = NULL;
    int nvisited = 0, capvisited = 0;
    if (!v) return;
    capstack = 16; stack = (gn_value **)malloc((size_t)capstack * sizeof(gn_value *));
    stack[nstack++] = v;
    while (nstack > 0) {
        gn_value *u = stack[--nstack];
        int already = 0;
        for (i = 0; i < nvisited; i++) if (visited[i] == u) { already = 1; break; }
        if (already) continue;
        if (nvisited >= capvisited) {
            capvisited = capvisited ? capvisited * 2 : 16;
            visited = (gn_value **)realloc(visited, (size_t)capvisited * sizeof(gn_value *));
        }
        visited[nvisited++] = u;
        if (norder >= caporder) {
            caporder = caporder ? caporder * 2 : 16;
            order = (gn_value **)realloc(order, (size_t)caporder * sizeof(gn_value *));
        }
        order[norder++] = u;
        for (i = 0; i < u->nprev; i++) {
            if (nstack >= capstack) {
                capstack *= 2;
                stack = (gn_value **)realloc(stack, (size_t)capstack * sizeof(gn_value *));
            }
            stack[nstack++] = u->prev[i];
        }
    }
    /* order è in ordine di visita (padri prima dei figli); processare al contrario */
    for (i = 0; i < nvisited; i++) order[i]->grad = 0.0;
    v->grad = 1.0;
    for (i = 0; i < norder; i++) {
        if (order[i]->backward) order[i]->backward(order[i]);
    }
    free(stack); free(order); free(visited);
}
