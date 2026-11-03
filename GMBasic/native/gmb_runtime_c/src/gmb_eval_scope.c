/* ============================================================================
 * gmb_eval_scope.c — scope transiente S_eval per & / EVAL (condiviso VM/C e C99).
 * Ogni esecuzione EVAL gira in un nuovo frame figlio: DIM/LET e le assegnazioni
 * implicite su nomi ignoti creano QUI e muoiono col pop (mai leak in host/globali);
 * le assegnazioni implicite su nomi esterni scrivono in place via backend.
 * Nessun globale/TLS: il frame vive nello stack di chiamata del task (GPM-safe).
 * Ownership: le stringhe sono copiate (gmb_make_val_string), il resto è alias
 * (il tree-walk non libera mai i temporanei durante l'eval — nessun dangling
 * dentro la vita dello scope; lo scope non sopravvive all'eval).
 * ============================================================================ */
#include "gmb_eval_scope.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SCOPE_INIT_CAP 8

struct gmb_expr_scope {
    gmb_expr_scope_t* parent;
    char** names;
    gmb_val_t* vals;
    int n, cap;
};

gmb_expr_scope_t* gmb_expr_scope_push(gmb_expr_scope_t* parent)
{
    gmb_expr_scope_t* s = (gmb_expr_scope_t*)calloc(1, sizeof(*s));
    if (s) s->parent = parent;
    return s;
}

static void val_free(gmb_val_t* v)
{
    if (v && v->kind == GMB_TYPE_STRING) gmb_string_free(&v->v_string);
}

void gmb_expr_scope_pop(gmb_expr_scope_t* s)
{
    if (!s) return;
    for (int i = 0; i < s->n; i++) {
        free(s->names[i]);
        val_free(&s->vals[i]);
    }
    free(s->names);
    free(s->vals);
    free(s);
}

static gmb_val_t val_copy(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_STRING)
        return gmb_make_val_string(v.v_string.data ? v.v_string.data : "");
    return v;
}

static int frame_find(gmb_expr_scope_t* s, const char* name)
{
    if (!s || !name) return -1;
    for (int i = 0; i < s->n; i++)
        if (s->names[i] && strcasecmp(s->names[i], name) == 0) return i;
    return -1;
}

int gmb_expr_scope_get(gmb_expr_scope_t* s, const char* name, gmb_val_t* out)
{
    for (gmb_expr_scope_t* f = s; f; f = f->parent) {
        int i = frame_find(f, name);
        if (i >= 0) {
            if (out) *out = val_copy(f->vals[i]);
            return 1;
        }
    }
    return 0;
}

void gmb_expr_scope_set(gmb_expr_scope_t* s, const char* name, gmb_val_t v)
{
    if (!s || !name || !name[0]) return;
    int i = frame_find(s, name);
    if (i >= 0) {
        val_free(&s->vals[i]);
        s->vals[i] = val_copy(v);
        return;
    }
    if (s->n >= s->cap) {
        int nc = s->cap ? s->cap * 2 : SCOPE_INIT_CAP;
        char** nn = (char**)realloc(s->names, (size_t)nc * sizeof(char*));
        gmb_val_t* nv = (gmb_val_t*)realloc(s->vals, (size_t)nc * sizeof(gmb_val_t));
        if (!nn || !nv) return;
        s->names = nn;
        s->vals = nv;
        s->cap = nc;
    }
    s->names[s->n] = strdup(name);
    if (!s->names[s->n]) return;
    s->vals[s->n] = val_copy(v);
    s->n++;
}

int gmb_expr_scope_update(gmb_expr_scope_t* s, const char* name, gmb_val_t v)
{
    for (gmb_expr_scope_t* f = s; f; f = f->parent) {
        int i = frame_find(f, name);
        if (i >= 0) {
            val_free(&f->vals[i]);
            f->vals[i] = val_copy(v);
            return 1;
        }
    }
    return 0;
}
