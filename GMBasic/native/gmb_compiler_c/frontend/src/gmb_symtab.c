/* gmb_symtab.c — Symbol Table ID-based + Scope tree in C (pari al C# SymbolTable.cs).
 * Compilatore C autonomo: solo C, nessuna dipendenza da C#. */
#include "gmb_symtab.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void* xgrow(void* p, int* cap, int need, size_t elem) {
    if (need <= *cap) return p;
    int nc = *cap ? *cap * 2 : 16;
    while (nc < need) nc *= 2;
    void* q = realloc(p, (size_t)nc * elem);
    if (!q) return p;
    *cap = nc; return q;
}
static char* dupz(const char* s) { if (!s) return NULL; char* d = strdup(s); return d; }

void gmb_symtab_init(gmb_symtab_t* t) { if (t) memset(t, 0, sizeof *t); }

void gmb_symtab_free(gmb_symtab_t* t) {
    if (!t) return;
    for (int i = 0; i < t->n_syms; i++) { free(t->syms[i].name); free(t->syms[i].ns); free(t->syms[i].type_name); free(t->syms[i].signature); }
    for (int i = 0; i < t->n_scopes; i++) free(t->scopes[i].sym_ids);
    free(t->syms); free(t->scopes); free(t->stack);
    memset(t, 0, sizeof *t);
}

int gmb_symtab_push_scope(gmb_symtab_t* t, int owner) {
    if (!t) return -1;
    t->scopes = xgrow(t->scopes, &t->cap_scopes, t->n_scopes + 1, sizeof(gmb_scope_t));
    gmb_scope_t* s = &t->scopes[t->n_scopes];
    memset(s, 0, sizeof *s);
    s->id = t->n_scopes++;
    s->parent_id = t->cur_scope;
    s->owner_sym_id = owner;
    t->stack = xgrow(t->stack, &t->cap_stack, t->n_stack + 1, sizeof(int));
    t->stack[t->n_stack++] = t->cur_scope;
    t->cur_scope = s->id;
    return s->id;
}

void gmb_symtab_pop_scope(gmb_symtab_t* t) {
    if (!t || t->n_stack <= 0) return;
    t->cur_scope = t->stack[--t->n_stack];
}

int gmb_symtab_declare(gmb_symtab_t* t, gmb_sym_kind_t kind, const char* name, const char* ns,
                       const char* type_name, const char* signature, int line, int col,
                       unsigned flags, int is_exported, gmb_stmt_t* decl_node) {
    if (!t || !name || !*name) return -1;
    t->syms = xgrow(t->syms, &t->cap_syms, t->n_syms + 1, sizeof(gmb_sym_t));
    int id = t->n_syms;
    gmb_sym_t* s = &t->syms[t->n_syms++];
    memset(s, 0, sizeof *s);
    s->id = id; s->kind = kind;
    s->name = dupz(name); s->ns = dupz(ns ? ns : ""); 
    s->scope_id = t->cur_scope;
    s->container_sym_id = -1;
    s->type_name = dupz(type_name); s->signature = dupz(signature);
    s->decl_line = line; s->decl_col = col;
    s->flags = flags; s->is_exported = is_exported;
    s->decl_node = decl_node;
    if (decl_node) decl_node->scope_id = s->scope_id;
    /* registra nello scope corrente */
    gmb_scope_t* sc = &t->scopes[t->cur_scope];
    sc->sym_ids = xgrow(sc->sym_ids, &sc->cap_syms, sc->n_syms + 1, sizeof(int));
    sc->sym_ids[sc->n_syms++] = id;
    return id;
}

int gmb_symtab_resolve(gmb_symtab_t* t, const char* name) {
    if (!t || !name) return -1;
    int sc = t->cur_scope;
    while (sc >= 0) {
        gmb_scope_t* s = &t->scopes[sc];
        for (int i = s->n_syms - 1; i >= 0; i--) {   /* ultimo dichiarato vince (shadowing) */
            gmb_sym_t* sym = &t->syms[s->sym_ids[i]];
            if (sym->name && strcasecmp(sym->name, name) == 0) return sym->id;
        }
        sc = s->parent_id;
    }
    return -1;
}

int gmb_symtab_find_any(gmb_symtab_t* t, const char* name) {
    if (!t || !name) return -1;
    for (int i = t->n_syms - 1; i >= 0; i--)   /* ultimo dichiarato vince */
        if (t->syms[i].name && strcasecmp(t->syms[i].name, name) == 0) return i;
    return -1;
}

int gmb_symtab_find_all(gmb_symtab_t* t, const char* name, int* out, int max) {
    if (!t || !name) return 0;
    int n = 0;
    for (int i = 0; i < t->n_syms && n < max; i++)
        if (t->syms[i].name && strcasecmp(t->syms[i].name, name) == 0) out[n++] = i;
    return n;
}

gmb_sym_t*   gmb_symtab_get(gmb_symtab_t* t, int id) { return (t && id >= 0 && id < t->n_syms) ? &t->syms[id] : NULL; }
gmb_scope_t* gmb_symtab_scope(gmb_symtab_t* t, int id) { return (t && id >= 0 && id < t->n_scopes) ? &t->scopes[id] : NULL; }

const char* gmb_sym_kind_name(gmb_sym_kind_t k) {
    switch (k) {
        case GMB_SYM_NAMESPACE: return "namespace"; case GMB_SYM_MODULE: return "module";
        case GMB_SYM_PROCEDURE: return "procedure"; case GMB_SYM_FUNCTION: return "function";
        case GMB_SYM_METHOD: return "method"; case GMB_SYM_VARIABLE: return "variable";
        case GMB_SYM_PARAMETER: return "parameter"; case GMB_SYM_CONSTANT: return "constant";
        case GMB_SYM_TYPE: return "type"; case GMB_SYM_CLASS: return "class";
        case GMB_SYM_INTERFACE: return "interface"; case GMB_SYM_ENUM: return "enum";
        case GMB_SYM_FIELD: return "field"; case GMB_SYM_PROPERTY: return "property";
        case GMB_SYM_OPERATOR: return "operator"; default: return "unknown";
    }
}

/* ---------- build dall'AST ---------- */
static void build_stmt(gmb_symtab_t* t, gmb_stmt_t* s, int container);

static void build_body(gmb_symtab_t* t, gmb_stmt_t** body, int n, int container) {
    for (int i = 0; i < n; i++) build_stmt(t, body[i], container);
}

static void build_stmt(gmb_symtab_t* t, gmb_stmt_t* s, int container) {
    if (!s) return;
    switch (s->kind) {
        case GMB_STMT_FUNCTION: {
            unsigned fl = GMB_SYMF_PRIVATE | (s->is_public ? GMB_SYMF_PUBLIC : 0);
            char fsig[256]; size_t fo = 0; fsig[0] = 0;
            for (int i = 0; i < s->n_params; i++)
                fo += (size_t)snprintf(fsig + fo, sizeof fsig - fo, "%s%s", i ? "," : "",
                                       s->func_params ? s->func_params[i] : "p");
            int id = gmb_symtab_declare(t, s->is_sub ? GMB_SYM_PROCEDURE : GMB_SYM_FUNCTION,
                                        s->func_name, s->func_ns, s->func_ret_type, fsig,
                                        s->line, 0, fl, s->is_public, s);
            if (id >= 0 && container >= 0) { t->syms[id].container_sym_id = container; }
            gmb_symtab_push_scope(t, id);
            for (int i = 0; i < s->n_params; i++) {
                unsigned pf = GMB_SYMF_PARAM
                            | (s->param_byref && s->param_byref[i] ? GMB_SYMF_BYREF : 0)
                            | (s->param_is_array && s->param_is_array[i] ? GMB_SYMF_ARRAY : 0)
                            | (s->param_optional && s->param_optional[i] ? GMB_SYMF_OPTIONAL : 0);
                gmb_symtab_declare(t, GMB_SYM_PARAMETER, s->func_params ? s->func_params[i] : "?",
                                   NULL, s->param_types ? s->param_types[i] : NULL, NULL,
                                   s->line, 0, pf, 0, NULL);
            }
            build_body(t, s->func_body, s->n_funcbody, id);
            gmb_symtab_pop_scope(t);
            break;
        }
        case GMB_STMT_DIM:
            if (s->dim_var && *s->dim_var) {
                unsigned fl = GMB_SYMF_PRIVATE | (s->array_size ? GMB_SYMF_ARRAY : 0);
                int id = gmb_symtab_declare(t, GMB_SYM_VARIABLE, s->dim_var, NULL, s->dim_type, NULL, s->line, 0, fl, 0, s);
                if (id >= 0 && container >= 0) t->syms[id].container_sym_id = container;
            }
            break;
        case GMB_STMT_CONST:
            if (s->target) {
                int id = gmb_symtab_declare(t, GMB_SYM_CONSTANT, s->target, NULL, NULL, NULL, s->line, 0, GMB_SYMF_CONST, 0, s);
                if (id >= 0 && container >= 0) t->syms[id].container_sym_id = container;
            }
            break;
        case GMB_STMT_CLASS:
            if (s->target) {
                int id = gmb_symtab_declare(t, GMB_SYM_CLASS,
                                            s->target, NULL, NULL, NULL, s->line, 0, GMB_SYMF_PRIVATE, 0, s);
                if (id >= 0 && container >= 0) t->syms[id].container_sym_id = container;
            }
            break;
        case GMB_STMT_IF:
            gmb_symtab_push_scope(t, container);
            build_body(t, s->then_body, s->n_then, container);
            build_body(t, s->else_body, s->n_else, container);
            gmb_symtab_pop_scope(t);
            break;
        case GMB_STMT_FOR:
            gmb_symtab_push_scope(t, container);
            if (s->for_var) gmb_symtab_declare(t, GMB_SYM_VARIABLE, s->for_var, NULL, NULL, NULL, s->line, 0, GMB_SYMF_PRIVATE, 0, s);
            build_body(t, s->for_body, s->n_for, container);
            gmb_symtab_pop_scope(t);
            break;
        case GMB_STMT_WHILE:
            gmb_symtab_push_scope(t, container); build_body(t, s->while_body, s->n_while, container); gmb_symtab_pop_scope(t);
            break;
        case GMB_STMT_DO:
            gmb_symtab_push_scope(t, container); build_body(t, s->do_body, s->n_do, container); gmb_symtab_pop_scope(t);
            break;
        default: break;
    }
}

int gmb_symtab_build(gmb_symtab_t* t, gmb_stmt_t** stmts, int n) {
    if (!t) return 0;
    /* scope RADICE (globale) */
    t->cur_scope = -1;
    gmb_symtab_push_scope(t, -1);
    for (int i = 0; i < n; i++) build_stmt(t, stmts[i], -1);
    t->cur_scope = 0;   /* per le query: parti dalla radice */
    return t->n_syms;
}
