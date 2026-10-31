/* gmb_c99_emit_dynamic.c — SCOPE DINAMICO (variabili DYNAMIC) per l'emitter C99 nativo.
 *
 * Semantica canonica: Guide/Variables-Scope-Spec.md §2.5 (visibilità a CASCATA per nome,
 * shadowing, ripristino automatico all'uscita di procedura). La visibilità è per NOME (non
 * lessicale): un nome DYNAMIC dichiarato da un chiamante è leggibile/scrivibile da una
 * procedura chiamata che non lo dichiara. Implementazione: runtime condiviso `gmb_dscope`
 * (native/gmb_runtime_c/include/gmb_dscope.h, AGENTS §0bis), parità con emitter C#
 * (C99CodeEmitter.Scope.Dynamic.cs).
 *
 * Il codegen:
 *   - pre-scansiona i nomi DYNAMIC (gmb_c99_collect_dynamic) PRIMA della registrazione, così
 *     un nome DYNAMIC non diventa una variabile/globale C (reg_dim/reg_let_assign lo saltano);
 *   - emette `gmb_dscope_push()` in testa alla procedura che dichiara DYNAMIC e
 *     `gmb_dscope_pop()` a ogni uscita (emit_fn_return);
 *   - `DYNAMIC x [AS STRING] [= v]` → `gmb_dscope_declare("x", <val>)`;
 *   - letture/scritture su un nome DYNAMIC (non ombreggiato da locale/param/globale C) →
 *     `gmb_dscope_get_string/get_double` e `gmb_dscope_set(...)`.
 *
 * Le funzioni di supporto condivise (sb_put, emit_expr, etype, lookup_var, sb_ident_to, ...)
 * vivono in gmb_c99_emit.c e sono esposte non-static (prototipi in gmb_c99_emit_internal.h). */
#include "gmb_c99_emit_internal.h"

/* TY_VAL non è nell'enum condiviso (gmb_c99_emit_internal.h definisce solo TY_NUM/TY_STR):
 * il valore coincide con quello di gmb_c99_emit.c (enum { TY_NUM=0, TY_STR=1, TY_VAL=2 }). */
#define DYN_TY_VAL 2

int gmb_c99_dyn_is(ctx_t* c, const char* name) {
    if (!c || !name || !name[0]) return 0;
    for (int i = 0; i < c->n_dyn_names; i++)
        if (strcasecmp(c->dyn_names[i], name) == 0) return 1;
    return 0;
}

int gmb_c99_dyn_is_str(ctx_t* c, const char* name) {
    if (!c || !name) return 0;
    for (int i = 0; i < c->n_dyn_names; i++)
        if (strcasecmp(c->dyn_names[i], name) == 0) return c->dyn_isstr[i];
    return 0;
}

static void dyn_record(ctx_t* c, const char* name, int isstr) {
    if (!c || !name || !name[0] || c->n_dyn_names >= 128) return;
    if (gmb_c99_dyn_is(c, name)) return;
    snprintf(c->dyn_names[c->n_dyn_names], 48, "%s", name);
    c->dyn_isstr[c->n_dyn_names] = isstr ? 1 : 0;
    c->n_dyn_names++;
}

static int dyn_dim_is_str(ctx_t* c, gmb_stmt_t* s) {
    if (s->dim_type_tk == (int)GMB_TOK_StringType) return 1;
    if (s->dim_type && strcasecmp(s->dim_type, "STRING") == 0) return 1;
    if (s->dim_init && etype(c, s->dim_init) == TY_STR) return 1;
    return 0;
}

/* Ricorre nell'AST per raccogliere TUTTI i nomi DYNAMIC (anche dentro funzione annidate e
 * metodi di classe: la visibilità è per nome a runtime, non lessicale). */
static void dyn_scan_list(ctx_t* c, gmb_stmt_t** list, int n);
static void dyn_scan_stmt(ctx_t* c, gmb_stmt_t* s) {
    if (!s) return;
    if (s->kind == GMB_STMT_DIM && s->dim_is_dynamic && s->dim_var)
        dyn_record(c, s->dim_var, dyn_dim_is_str(c, s));
    dyn_scan_list(c, s->then_body, s->n_then);
    dyn_scan_list(c, s->else_body, s->n_else);
    dyn_scan_list(c, s->for_body, s->n_for);
    dyn_scan_list(c, s->while_body, s->n_while);
    dyn_scan_list(c, s->do_body, s->n_do);
    dyn_scan_list(c, s->try_body, s->n_try);
    for (int k = 0; k < s->n_catch; k++)
        if (s->catch_body[k]) dyn_scan_list(c, s->catch_body[k]->catch_body, s->catch_body[k]->n_catch);
    dyn_scan_list(c, s->finally_body, s->n_finally);
    for (int k = 0; k < s->n_select_cases; k++)
        if (s->select_cases[k]) dyn_scan_list(c, s->select_cases[k]->case_body, s->select_cases[k]->n_case_body);
    for (int k = 0; k < s->n_go_cases; k++)
        dyn_scan_list(c, s->go_cases[k].body, s->go_cases[k].n_body);
    dyn_scan_list(c, s->func_body, s->n_funcbody);
    dyn_scan_list(c, s->cls_methods, s->n_cls_methods);
}

static void dyn_scan_list(ctx_t* c, gmb_stmt_t** list, int n) {
    for (int i = 0; i < n; i++) dyn_scan_stmt(c, list[i]);
}

void gmb_c99_collect_dynamic(ctx_t* c, gmb_stmt_t** list, int n) {
    dyn_scan_list(c, list, n);
}

/* La procedura dichiara DYNAMIC nel PROPRIO corpo (senza scendere nelle funzioni/metodi
 * annidati: quelli emettono il proprio push/pop). */
static int dyn_list_local(gmb_stmt_t** list, int n) {
    for (int i = 0; i < n; i++) {
        gmb_stmt_t* s = list[i];
        if (!s) continue;
        if (s->kind == GMB_STMT_DIM && s->dim_is_dynamic) return 1;
        if (s->kind == GMB_STMT_FUNCTION || s->kind == GMB_STMT_CLASS) continue;
        if (dyn_list_local(s->then_body, s->n_then)) return 1;
        if (dyn_list_local(s->else_body, s->n_else)) return 1;
        if (dyn_list_local(s->for_body, s->n_for)) return 1;
        if (dyn_list_local(s->while_body, s->n_while)) return 1;
        if (dyn_list_local(s->do_body, s->n_do)) return 1;
        if (dyn_list_local(s->try_body, s->n_try)) return 1;
        for (int k = 0; k < s->n_catch; k++)
            if (s->catch_body[k] && dyn_list_local(s->catch_body[k]->catch_body, s->catch_body[k]->n_catch)) return 1;
        if (dyn_list_local(s->finally_body, s->n_finally)) return 1;
        for (int k = 0; k < s->n_select_cases; k++)
            if (s->select_cases[k] && dyn_list_local(s->select_cases[k]->case_body, s->select_cases[k]->n_case_body)) return 1;
        for (int k = 0; k < s->n_go_cases; k++)
            if (dyn_list_local(s->go_cases[k].body, s->go_cases[k].n_body)) return 1;
    }
    return 0;
}

int gmb_c99_list_declares_dynamic(gmb_stmt_t** list, int n) {
    return dyn_list_local(list, n);
}

/* DYNAMIC attivo per NOME: dichiarato e NON ombreggiato da un locale/parametro/globale C
 * omonimo (parità emitter C# TryEmitDynamicRead: `!_declaredVars && !_paramCName && !_globalTypes`). */
static int dyn_active(ctx_t* c, const char* name) {
    return gmb_c99_dyn_is(c, name) && (c->force_dyn || !lookup_var(c, name));
}

static void dyn_key(ctx_t* c, const char* name, char* out, int cap) {
    (void)c;
    int i = 0;
    for (const char* s = name; s && *s && i < cap - 1; s++) {
        if (*s == '"' || *s == '\\') continue;   /* safety: mai in un identificatore BASIC */
        out[i++] = *s;
    }
    out[i] = 0;
}

void gmb_c99_emit_dyn_val(ctx_t* c, gmb_expr_t* e) {
    if (!e) { sb_put(&c->out, "gmb_val_nil()"); return; }
    int et = etype(c, e);
    if (et == TY_STR) {
        sb_put(&c->out, "gmb_make_val_string((");
        emit_str_operand(c, e);
        sb_put(&c->out, ").data)");
    } else if (et == DYN_TY_VAL) {
        emit_expr(c, e);   /* già gmb_val_t */
    } else {
        sb_put(&c->out, "gmb_make_val_double((gmb_double_t)(");
        emit_expr(c, e);
        sb_put(&c->out, "))");
    }
}

void gmb_c99_emit_dyn_declare(ctx_t* c, gmb_stmt_t* s) {
    if (!s->dim_var) return;
    char key[64];
    dyn_key(c, s->dim_var, key, sizeof(key));
    ind(c);
    sb_printf(&c->out, "gmb_dscope_declare(\"%s\", ", key);
    gmb_c99_emit_dyn_val(c, s->dim_init);
    sb_put(&c->out, ");\n");
}

int gmb_c99_try_emit_dyn_read(ctx_t* c, const char* name) {
    if (!dyn_active(c, name)) return 0;
    char key[64];
    dyn_key(c, name, key, sizeof(key));
    sb_printf(&c->out, "gmb_dscope_get_%s(\"%s\")", gmb_c99_dyn_is_str(c, name) ? "string" : "double", key);
    return 1;
}

int gmb_c99_try_emit_dyn_write(ctx_t* c, const char* target, gmb_expr_t* value) {
    if (!dyn_active(c, target)) return 0;
    char key[64];
    dyn_key(c, target, key, sizeof(key));
    ind(c);
    sb_printf(&c->out, "gmb_dscope_set(\"%s\", ", key);
    gmb_c99_emit_dyn_val(c, value);
    sb_put(&c->out, ");\n");
    return 1;
}
