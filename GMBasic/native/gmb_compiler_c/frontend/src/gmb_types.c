/* gmb_types.c — Type Registry + risoluzione membri (C). Compilatore C autonomo. */
#include "gmb_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* const BUILTINS[] = {
    "BYTE","WORD","QUAD","INTEGER","LONG","SINGLE","DOUBLE","CURRENCY","STRING","BOOLEAN","BOOL",
    "VARIANT","OBJECT","DATE","ANY","FUNCTION","SUB","VOID","CHANNEL","WAITGROUP","WINDOW",NULL
};

int gmb_type_is_builtin(const char* name) {
    if (!name) return 0;
    for (int i = 0; BUILTINS[i]; i++) if (strcasecmp(name, BUILTINS[i]) == 0) return 1;
    return 0;
}

const char* gmb_type_kind_name(gmb_type_kind_t k) {
    switch (k) {
        case GMB_TY_BUILTIN: return "builtin"; case GMB_TY_UDT: return "udt";
        case GMB_TY_CLASS: return "class"; case GMB_TY_INTERFACE: return "interface";
        case GMB_TY_ENUM: return "enum"; case GMB_TY_ARRAY: return "array";
        case GMB_TY_FUNCTION: return "function"; default: return "unknown";
    }
}

static void* xgrow(void* p, int* cap, int need, size_t elem) {
    if (need <= *cap) return p;
    int nc = *cap ? *cap * 2 : 16;
    while (nc < need) nc *= 2;
    void* q = realloc(p, (size_t)nc * elem);
    if (!q) return p;
    *cap = nc; return q;
}

void gmb_typereg_init(gmb_typereg_t* r) { if (r) memset(r, 0, sizeof *r); }
void gmb_typereg_free(gmb_typereg_t* r) {
    if (!r) return;
    for (int i = 0; i < r->n_types; i++) free(r->types[i].fields);
    free(r->types); memset(r, 0, sizeof *r);
}

static gmb_type_t* reg_add(gmb_typereg_t* r, const char* name, gmb_type_kind_t kind) {
    r->types = xgrow(r->types, &r->cap_types, r->n_types + 1, sizeof(gmb_type_t));
    gmb_type_t* t = &r->types[r->n_types++];
    memset(t, 0, sizeof *t);
    snprintf(t->name, sizeof t->name, "%s", name ? name : "?");
    t->kind = kind;
    return t;
}

static void type_add_field(gmb_type_t* t, const char* name, const char* type, int is_static) {
    if (!name || !*name) return;
    t->fields = xgrow(t->fields, &t->cap_fields, t->n_fields + 1, sizeof(gmb_field_t));
    gmb_field_t* f = &t->fields[t->n_fields++];
    memset(f, 0, sizeof *f);
    snprintf(f->name, sizeof f->name, "%s", name);
    snprintf(f->type, sizeof f->type, "%s", type ? type : "");
    f->is_static = is_static;
}

static void reg_stmt(gmb_typereg_t* r, gmb_stmt_t* s) {
    if (!s) return;
    if (s->kind == GMB_STMT_CLASS) {
        gmb_type_kind_t k = s->class_is_interface ? GMB_TY_INTERFACE :
                            (s->class_is_udt ? GMB_TY_UDT : GMB_TY_CLASS);
        gmb_type_t* t = reg_add(r, s->class_name ? s->class_name : "?", k);
        t->is_interface = s->class_is_interface;
        if (s->n_cls_implements > 0 && s->cls_implements && s->cls_implements[0])
            snprintf(t->base, sizeof t->base, "%s", s->cls_implements[0]);
        for (int i = 0; i < s->n_cls_fields; i++)
            type_add_field(t, s->cls_field_names ? s->cls_field_names[i] : NULL,
                           s->cls_field_types ? s->cls_field_types[i] : NULL, 0);
        for (int i = 0; i < s->n_cls_statics; i++)
            type_add_field(t, s->cls_static_names ? s->cls_static_names[i] : NULL,
                           s->cls_static_types ? s->cls_static_types[i] : NULL, 1);
        for (int i = 0; i < s->n_cls_methods; i++) {
            gmb_stmt_t* m = s->cls_methods[i];
            if (m && m->kind == GMB_STMT_FUNCTION)
                type_add_field(t, m->func_name, m->func_ret_type, 1);   /* metodo = campo function-typed */
        }
    }
    /* ricorsione nei corpi (i tipi possono essere annidati) */
    for (int i = 0; i < s->n_then; i++) reg_stmt(r, s->then_body[i]);
    for (int i = 0; i < s->n_else; i++) reg_stmt(r, s->else_body[i]);
    for (int i = 0; i < s->n_funcbody; i++) reg_stmt(r, s->func_body[i]);
}

int gmb_typereg_build(gmb_typereg_t* r, gmb_stmt_t** stmts, int n) {
    if (!r) return 0;
    for (int i = 0; i < n; i++) reg_stmt(r, stmts[i]);
    return r->n_types;
}

const gmb_type_t* gmb_typereg_find(const gmb_typereg_t* r, const char* name) {
    if (!r || !name) return NULL;
    for (int i = 0; i < r->n_types; i++)
        if (strcasecmp(r->types[i].name, name) == 0) return &r->types[i];
    return NULL;
}

const gmb_field_t* gmb_typereg_field(const gmb_typereg_t* r, const char* type_name, const char* field) {
    if (!r || !type_name || !field) return NULL;
    /* risale la catena base (INHERITS/IMPLEMENTS) per risolvere il campo. */
    const gmb_type_t* t = gmb_typereg_find(r, type_name);
    int depth = 0;
    while (t && depth++ < 16) {
        for (int i = 0; i < t->n_fields; i++)
            if (strcasecmp(t->fields[i].name, field) == 0) return &t->fields[i];
        if (t->base[0]) t = gmb_typereg_find(r, t->base); else t = NULL;
    }
    return NULL;
}
