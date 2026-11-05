/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * sym.c — gmbsym: nodi, parser ricorsivo, simplify, diff, eval, tostring. */
#include "gm_sym.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

typedef enum { S_NUM, S_VAR, S_ADD, S_SUB, S_MUL, S_DIV, S_POW, S_NEG, S_SIN, S_COS, S_EXP, S_LOG } gm_sym_op;

struct gm_sym_node {
    gm_sym_op     op;
    double     num;
    char       name[32];
    gm_sym_node*  a;
    gm_sym_node*  b;
};

static gm_sym_node* nd(gm_sym_op op, gm_sym_node* a, gm_sym_node* b) {
    gm_sym_node* n = (gm_sym_node*)calloc(1, sizeof *n);
    n->op = op; n->a = a; n->b = b;
    return n;
}
gm_sym_node* gm_sym_const(double v) { gm_sym_node* n = nd(S_NUM, NULL, NULL); n->num = v; return n; }
gm_sym_node* gm_sym_var(const char* name) { gm_sym_node* n = nd(S_VAR, NULL, NULL); snprintf(n->name, sizeof n->name, "%s", name ? name : "x"); return n; }
gm_sym_node* gm_sym_add(const gm_sym_node* a, const gm_sym_node* b) { return nd(S_ADD, (gm_sym_node*)a, (gm_sym_node*)b); }
gm_sym_node* gm_sym_sub(const gm_sym_node* a, const gm_sym_node* b) { return nd(S_SUB, (gm_sym_node*)a, (gm_sym_node*)b); }
gm_sym_node* gm_sym_mul(const gm_sym_node* a, const gm_sym_node* b) { return nd(S_MUL, (gm_sym_node*)a, (gm_sym_node*)b); }
gm_sym_node* gm_sym_div(const gm_sym_node* a, const gm_sym_node* b) { return nd(S_DIV, (gm_sym_node*)a, (gm_sym_node*)b); }
gm_sym_node* gm_sym_pow(const gm_sym_node* a, const gm_sym_node* b) { return nd(S_POW, (gm_sym_node*)a, (gm_sym_node*)b); }
gm_sym_node* gm_sym_neg(const gm_sym_node* a) { return nd(S_NEG, (gm_sym_node*)a, NULL); }
gm_sym_node* gm_sym_sin(const gm_sym_node* a) { return nd(S_SIN, (gm_sym_node*)a, NULL); }
gm_sym_node* gm_sym_cos(const gm_sym_node* a) { return nd(S_COS, (gm_sym_node*)a, NULL); }
gm_sym_node* gm_sym_exp(const gm_sym_node* a) { return nd(S_EXP, (gm_sym_node*)a, NULL); }
gm_sym_node* gm_sym_log(const gm_sym_node* a) { return nd(S_LOG, (gm_sym_node*)a, NULL); }

void gm_sym_free(gm_sym_node* e) { if (!e) return; gm_sym_free(e->a); gm_sym_free(e->b); free(e); }

/* ── parser ricorsivo ── */
typedef struct { const char* p; char* err; int cap; int bad; } sp_t;
static void serr(sp_t* s, const char* m) { if (!s->bad && s->err && s->cap > 0) snprintf(s->err, (size_t)s->cap, "%s", m); s->bad = 1; }
static void skip(sp_t* s) { while (*s->p == ' ' || *s->p == '\t') s->p++; }

static gm_sym_node* p_expr(sp_t* s);
static gm_sym_node* p_factor(sp_t* s);

static gm_sym_node* p_primary(sp_t* s) {
    skip(s);
    if (*s->p == '(') { s->p++; gm_sym_node* e = p_expr(s); skip(s); if (*s->p == ')') s->p++; else serr(s, "parentesi non chiusa"); return e; }
    if (isdigit((unsigned char)*s->p) || *s->p == '.') {
        char* end; double v = strtod(s->p, &end); s->p = end; return gm_sym_const(v);
    }
    if (isalpha((unsigned char)*s->p) || *s->p == '_') {
        char id[32]; int i = 0;
        while ((isalnum((unsigned char)*s->p) || *s->p == '_') && i < 31) id[i++] = *s->p++;
        id[i] = 0; skip(s);
        if (*s->p == '(') {
            s->p++; gm_sym_node* arg = p_expr(s); skip(s); if (*s->p == ')') s->p++; else serr(s, "parentesi non chiusa");
            if (!strcmp(id, "sin")) return gm_sym_sin(arg);
            if (!strcmp(id, "cos")) return gm_sym_cos(arg);
            if (!strcmp(id, "exp")) return gm_sym_exp(arg);
            if (!strcmp(id, "log")) return gm_sym_log(arg);
            serr(s, "funzione sconosciuta"); return arg;
        }
        return gm_sym_var(id);
    }
    serr(s, "token inatteso");
    return gm_sym_const(0);
}
static gm_sym_node* p_factor(sp_t* s) {
    gm_sym_node* base = p_primary(s);
    skip(s);
    if (*s->p == '^') { s->p++; return gm_sym_pow(base, p_factor(s)); }   /* right-assoc */
    return base;
}
static gm_sym_node* p_unary(sp_t* s) {
    skip(s);
    if (*s->p == '-') { s->p++; return gm_sym_neg(p_factor(s)); }
    if (*s->p == '+') { s->p++; }
    return p_factor(s);
}
static gm_sym_node* p_term(sp_t* s) {
    gm_sym_node* n = p_unary(s);
    for (;;) { skip(s); int c = *s->p; if (c == '*') { s->p++; n = gm_sym_mul(n, p_unary(s)); } else if (c == '/') { s->p++; n = gm_sym_div(n, p_unary(s)); } else break; }
    return n;
}
static gm_sym_node* p_expr(sp_t* s) {
    gm_sym_node* n = p_term(s);
    for (;;) { skip(s); int c = *s->p; if (c == '+') { s->p++; n = gm_sym_add(n, p_term(s)); } else if (c == '-') { s->p++; n = gm_sym_sub(n, p_term(s)); } else break; }
    return n;
}
gm_sym_node* gm_sym_parse(const char* text, char* err, int cap) {
    if (err && cap > 0) err[0] = 0;
    if (!text) return NULL;
    sp_t s = { text, err, cap, 0 };
    gm_sym_node* e = p_expr(&s);
    skip(&s);
    if (!s.bad && *s.p != 0) serr(&s, "caratteri residui");
    if (s.bad) { gm_sym_free(e); return NULL; }
    return e;
}

/* ── tostring (precedenza) ── */
typedef struct { char* s; size_t len, cap; } buf_t;
static void bput(buf_t* b, const char* t) {
    size_t n = strlen(t);
    if (b->len + n + 1 > b->cap) { b->cap = (b->len + n + 1) * 2; b->s = (char*)realloc(b->s, b->cap); }
    memcpy(b->s + b->len, t, n + 1); b->len += n;
}
static int prec(const gm_sym_node* e) {
    switch (e->op) { case S_ADD: case S_SUB: return 1; case S_MUL: case S_DIV: return 2; case S_POW: return 3; case S_NEG: return 4; default: return 5; }
}
static void emit(buf_t* b, const gm_sym_node* e, int parent) {
    char tmp[64]; int my = prec(e);
    if (my < parent) bput(b, "(");
    switch (e->op) {
        case S_NUM: snprintf(tmp, sizeof tmp, "%.10g", e->num); bput(b, tmp); break;
        case S_VAR: bput(b, e->name); break;
        case S_NEG: bput(b, "-"); emit(b, e->a, my); break;
        case S_SIN: bput(b, "sin("); emit(b, e->a, 0); bput(b, ")"); break;
        case S_COS: bput(b, "cos("); emit(b, e->a, 0); bput(b, ")"); break;
        case S_EXP: bput(b, "exp("); emit(b, e->a, 0); bput(b, ")"); break;
        case S_LOG: bput(b, "log("); emit(b, e->a, 0); bput(b, ")"); break;
        default:
            emit(b, e->a, my);
            if (e->op == S_ADD) bput(b, "+");
            else if (e->op == S_SUB) bput(b, "-");
            else if (e->op == S_MUL) bput(b, "*");
            else if (e->op == S_DIV) bput(b, "/");
            else if (e->op == S_POW) bput(b, "^");
            emit(b, e->b, e->op == S_POW ? my : my + 1);   /* pow right-assoc */
    }
    if (my < parent) bput(b, ")");
}
char* gm_sym_tostring(const gm_sym_node* e) {
    buf_t b = { NULL, 0, 0 }; b.s = (char*)malloc(1); b.s[0] = 0; b.cap = 1;
    emit(&b, e, 0);
    return b.s;
}

/* ── eval ── */
double gm_sym_eval(const gm_sym_node* e, const char* var, double value) {
    switch (e->op) {
        case S_NUM: return e->num;
        case S_VAR: return (var && !strcmp(e->name, var)) ? value : 0.0;
        case S_ADD: return gm_sym_eval(e->a, var, value) + gm_sym_eval(e->b, var, value);
        case S_SUB: return gm_sym_eval(e->a, var, value) - gm_sym_eval(e->b, var, value);
        case S_MUL: return gm_sym_eval(e->a, var, value) * gm_sym_eval(e->b, var, value);
        case S_DIV: return gm_sym_eval(e->a, var, value) / gm_sym_eval(e->b, var, value);
        case S_POW: return pow(gm_sym_eval(e->a, var, value), gm_sym_eval(e->b, var, value));
        case S_NEG: return -gm_sym_eval(e->a, var, value);
        case S_SIN: return sin(gm_sym_eval(e->a, var, value));
        case S_COS: return cos(gm_sym_eval(e->a, var, value));
        case S_EXP: return exp(gm_sym_eval(e->a, var, value));
        case S_LOG: return log(gm_sym_eval(e->a, var, value));
    }
    return 0.0;
}

/* ── diff ── */
static gm_sym_node* cp(const gm_sym_node* e) {   /* copia (immutabile a valle) */
    gm_sym_node* n = nd(e->op, NULL, NULL);
    n->num = e->num; memcpy(n->name, e->name, sizeof n->name);
    if (e->a) n->a = cp(e->a);
    if (e->b) n->b = cp(e->b);
    return n;
}
gm_sym_node* gm_sym_diff(const gm_sym_node* e, const char* var) {
    switch (e->op) {
        case S_NUM: return gm_sym_const(0);
        case S_VAR: return gm_sym_const(!strcmp(e->name, var) ? 1.0 : 0.0);
        case S_ADD: return gm_sym_add(gm_sym_diff(e->a, var), gm_sym_diff(e->b, var));
        case S_SUB: return gm_sym_sub(gm_sym_diff(e->a, var), gm_sym_diff(e->b, var));
        case S_MUL: return gm_sym_add(gm_sym_mul(gm_sym_diff(e->a, var), cp(e->b)), gm_sym_mul(cp(e->a), gm_sym_diff(e->b, var)));
        case S_DIV: { gm_sym_node* num = gm_sym_sub(gm_sym_mul(gm_sym_diff(e->a, var), cp(e->b)), gm_sym_mul(cp(e->a), gm_sym_diff(e->b, var)));
                      gm_sym_node* den = gm_sym_pow(cp(e->b), gm_sym_const(2)); return gm_sym_div(num, den); }
        case S_POW: {   /* d/dx u^c = c*u^(c-1)*u'  (esponente costante) */
            if (e->b->op == S_NUM) { gm_sym_node* c1 = gm_sym_const(e->b->num - 1.0);
                return gm_sym_mul(gm_sym_mul(gm_sym_const(e->b->num), gm_sym_pow(cp(e->a), c1)), gm_sym_diff(e->a, var)); }
            return gm_sym_const(0);  /* esponente variabile: roadmap */
        }
        case S_NEG: return gm_sym_neg(gm_sym_diff(e->a, var));
        case S_SIN: return gm_sym_mul(gm_sym_cos(cp(e->a)), gm_sym_diff(e->a, var));
        case S_COS: return gm_sym_mul(gm_sym_neg(gm_sym_sin(cp(e->a))), gm_sym_diff(e->a, var));
        case S_EXP: return gm_sym_mul(gm_sym_exp(cp(e->a)), gm_sym_diff(e->a, var));
        case S_LOG: return gm_sym_div(gm_sym_diff(e->a, var), cp(e->a));
    }
    return gm_sym_const(0);
}

/* ── simplify ── */
static int is_num(const gm_sym_node* e) { return e->op == S_NUM; }
static double nval(const gm_sym_node* e) { return e->num; }
gm_sym_node* gm_sym_simplify(const gm_sym_node* e) {
    gm_sym_node* a = e->a ? gm_sym_simplify(e->a) : NULL;
    gm_sym_node* b = e->b ? gm_sym_simplify(e->b) : NULL;
    gm_sym_node* r;
    switch (e->op) {
        case S_NUM: r = gm_sym_const(e->num); break;
        case S_VAR: r = gm_sym_var(e->name); break;
        case S_NEG: r = (is_num(a)) ? gm_sym_const(-nval(a)) : gm_sym_neg(a); break;
        case S_SIN: case S_COS: case S_EXP: case S_LOG:
            r = (is_num(a)) ? gm_sym_const(e->op == S_SIN ? sin(nval(a)) : e->op == S_COS ? cos(nval(a)) : e->op == S_EXP ? exp(nval(a)) : log(nval(a))) : nd(e->op, a, NULL);
            break;
        case S_ADD:
            if (is_num(a) && is_num(b)) r = gm_sym_const(nval(a) + nval(b));
            else if (is_num(a) && nval(a) == 0) r = b;
            else if (is_num(b) && nval(b) == 0) r = a;
            else r = gm_sym_add(a, b);
            break;
        case S_SUB:
            if (is_num(a) && is_num(b)) r = gm_sym_const(nval(a) - nval(b));
            else if (is_num(b) && nval(b) == 0) r = a;
            else r = gm_sym_sub(a, b);
            break;
        case S_MUL:
            if (is_num(a) && is_num(b)) r = gm_sym_const(nval(a) * nval(b));
            else if ((is_num(a) && nval(a) == 0) || (is_num(b) && nval(b) == 0)) r = gm_sym_const(0);
            else if (is_num(a) && nval(a) == 1) r = b;
            else if (is_num(b) && nval(b) == 1) r = a;
            else r = gm_sym_mul(a, b);
            break;
        case S_DIV:
            if (is_num(a) && is_num(b) && nval(b) != 0) r = gm_sym_const(nval(a) / nval(b));
            else if (is_num(b) && nval(b) == 1) r = a;
            else r = gm_sym_div(a, b);
            break;
        case S_POW:
            if (is_num(a) && is_num(b)) r = gm_sym_const(pow(nval(a), nval(b)));
            else if (is_num(b) && nval(b) == 0) r = gm_sym_const(1);
            else if (is_num(b) && nval(b) == 1) r = a;
            else r = gm_sym_pow(a, b);
            break;
        default: r = cp(e);
    }
    /* libera i sottoalberi intermedi non usati dal risultato */
    if (a && a != r && r->a != a && r->b != a) gm_sym_free(a);
    if (b && b != r && r->a != b && r->b != b) gm_sym_free(b);
    return r;
}
