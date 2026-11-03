/* native/gmb_runtime_c/src/gmb_ast.c — allocazione/free dei nodi AST. */
#include "gmb_ast.h"
#include "gmb_tokenkind.h"
#include "gmb_fn_lookup.h"
#include "gmb_members.h"
#include "gmb_lexer.h"
#include <stdlib.h>
#include <string.h>

static void free_stmt_array(gmb_stmt_t** arr, int n);

/* Risoluzione di frontiera nome → TokenKind alla costruzione dell'AST (mai
 * gmb_tokenkind_from_name, tabella globale disallineata — direttiva 2026-09-08):
 * prima le grafie StdLib/nomi canonici (fn_lookup), poi i membri noti dopo il punto
 * (member-table), infine le keyword del lexer (True/False/...). */
static gmb_tokenkind_t gmb_ast_name_tok(const char* n)
{
    gmb_tokenkind_t t = gmb_fn_tok(n);
    if (t != GMB_TOK_Unknown) return t;
    t = gmb_member_tok(n);
    if (t != GMB_TOK_Unknown && (int)t != 0) return t;
    int k = gmb_keyword_tok(n);
    return k > 0 ? (gmb_tokenkind_t)k : GMB_TOK_Unknown;
}

/* Nome che è l'ULTIMO segmento di un nome qualificato (dopo il punto) = MEMBRO, non un
 * builtin globale: la member-table ha la PRECEDENZA su fn_lookup. I metodi HTTPRESPONSE/
 * HTTPREQUEST (SetStatus/SendJSON/SetHeader/SetCookie/Redirect/ParseForm/…) NON hanno un
 * TokenKind nel frontend nativo — li riconoscono per LEXEME il VM/C (OP_CALL_MEMBER) e gli
 * emitter. Per questi si torna 0 (by-name) e NON si consulta fn_lookup: altrimenti
 * `res.SetStatus(code)` diventava il builtin AI `ProviderSetStatus` (arity 2) → errore di
 * emissione "AI.SETSTATUS: attesi (name, status)" e i sample MVC non giravano su C99Native. */
static gmb_tokenkind_t gmb_ast_member_seg_tok(const char* n)
{
    gmb_tokenkind_t t = gmb_member_tok(n);
    if ((int)t != 0) return t;
    static const char* http_members[] = {
        "SetStatus","SendJSON","SendJson","SendBytes","SetHeader","SetCookie",
        "Redirect","ParseMultipartForm","ParseForm","SendFile", NULL
    };
    for (int i = 0; http_members[i]; i++)
        if (strcasecmp(n, http_members[i]) == 0) return (gmb_tokenkind_t)0;
    return gmb_fn_tok(n);
}

gmb_expr_t* gmb_expr_num(double v, int line)
{
    gmb_expr_t* e = (gmb_expr_t*)calloc(1, sizeof(*e));
    e->kind = GMB_EXPR_NUM; e->num = v; e->line = line;
    return e;
}
gmb_expr_t* gmb_expr_str(const char* s, int line)
{
    gmb_expr_t* e = (gmb_expr_t*)calloc(1, sizeof(*e));
    e->kind = GMB_EXPR_STR; e->str = s ? strdup(s) : NULL; e->line = line;
    return e;
}
gmb_expr_t* gmb_expr_var(const char* name, int line)
{
    gmb_expr_t* e = (gmb_expr_t*)calloc(1, sizeof(*e));
    e->kind = GMB_EXPR_VAR; e->var = name ? strdup(name) : NULL; e->line = line;
    /* chiave canonica del nome risolta UNA volta qui (True/False/Pi/Count e membri noti):
     * mai dispatch per lexeme a valle, stesso schema delle CALL (ultimo segmento). */
    if (name) {
        const char* dt = strrchr(name, '.');
        e->tk = dt ? gmb_ast_member_seg_tok(dt + 1) : gmb_ast_name_tok(name);
    }
    return e;
}
gmb_expr_t* gmb_expr_comp(gmb_expr_t* src, const char* var, gmb_expr_t* where,
                          gmb_expr_t* order, int desc, gmb_expr_t* sel, int line)
{
    gmb_expr_t* e = (gmb_expr_t*)calloc(1, sizeof(*e));
    e->kind = GMB_EXPR_COMPREHENSION; e->comp_src = src; e->comp_var = var ? strdup(var) : NULL;
    e->comp_where = where; e->comp_order = order; e->comp_desc = desc; e->comp_sel = sel;
    e->line = line;
    return e;
}
gmb_expr_t* gmb_expr_binary(const char* op, gmb_expr_t* l, gmb_expr_t* r, int line)
{
    gmb_expr_t* e = (gmb_expr_t*)calloc(1, sizeof(*e));
    e->kind = GMB_EXPR_BINARY; e->op = op; e->left = l; e->right = r; e->line = line;
    return e;
}
gmb_expr_t* gmb_expr_unary(const char* op, gmb_expr_t* x, int line)
{
    gmb_expr_t* e = (gmb_expr_t*)calloc(1, sizeof(*e));
    e->kind = GMB_EXPR_UNARY; e->op = op; e->left = x; e->line = line;
    return e;
}
gmb_expr_t* gmb_expr_call(const char* name, gmb_expr_t** args, int n, int line)
{
    gmb_expr_t* e = (gmb_expr_t*)calloc(1, sizeof(*e));
    e->kind = GMB_EXPR_CALL; e->var = name ? strdup(name) : NULL;
    e->args = args; e->nargs = n; e->line = line;
    /* chiave canonica alla creazione (regola d'oro: mai dispatch per lexeme a valle):
     * il token del nome funzione viene risolto UNA volta qui; i chiamanti usano e->tk.
     * Nomi con dominio fuso (NS.Fn) → token dall'ultimo segmento, come c99_name_tok. */
    if (name) {
        const char* dt = strrchr(name, '.');
        e->tk = dt ? gmb_ast_member_seg_tok(dt + 1) : gmb_ast_name_tok(name);
    }
    return e;
}
gmb_expr_t* gmb_expr_call_typed(const char* name, gmb_tokenkind_t tk, gmb_expr_t** args, int n, int line)
{
    gmb_expr_t* e = gmb_expr_call(name, args, n, line);
    e->tk = tk;
    return e;
}
gmb_expr_t* gmb_expr_call_ns(const char* name, const char* ns, gmb_expr_t** args, int n, int line)
{
    gmb_expr_t* e = gmb_expr_call(name, args, n, line);
    if (ns) {
        e->ns = strdup(ns);
        e->ns_tk = gmb_ast_name_tok(ns);
    }
    return e;
}
gmb_expr_t* gmb_expr_lambda(char** params, int nparams, gmb_expr_t* expr,
                            gmb_stmt_t** body, int nbody, int is_sub, int line)
{
    gmb_expr_t* e = (gmb_expr_t*)calloc(1, sizeof(*e));
    e->kind = GMB_EXPR_LAMBDA; e->line = line;
    e->lambda_params = params; e->n_lambda_params = nparams;
    e->lambda_expr = expr; e->lambda_body = body; e->n_lambda_body = nbody;
    e->lambda_is_sub = is_sub;
    return e;
}

/* Clone "shallow-deep" di un'espressione: copia i campi scalari, duplica str/var/ns,
 * clona left/right/args. NON clona i campi lambda/comp (nulli per un indice/operando
 * semplice) -> restano NULL (calloc). Usato per riusare un indice in `++`/`--`
 * (es. `a(i)++` → `a(i) = a(i) + 1` con l'indice duplicato, per non fare doppio-free). */
gmb_expr_t* gmb_expr_clone(const gmb_expr_t* e)
{
    if (!e) return NULL;
    gmb_expr_t* c = (gmb_expr_t*)calloc(1, sizeof(*c));
    c->kind = e->kind; c->line = e->line; c->num = e->num;
    c->str = e->str ? strdup(e->str) : NULL;
    c->var = e->var ? strdup(e->var) : NULL;
    c->ns = e->ns ? strdup(e->ns) : NULL;
    c->ns_tk = e->ns_tk; c->pf_tk = e->pf_tk; c->op = e->op; c->tk = e->tk;
    c->left = gmb_expr_clone(e->left);
    c->right = gmb_expr_clone(e->right);
    if (e->args && e->nargs > 0) {
        c->args = (gmb_expr_t**)malloc((size_t)e->nargs * sizeof(gmb_expr_t*));
        for (int i = 0; i < e->nargs; i++) c->args[i] = gmb_expr_clone(e->args[i]);
        c->nargs = e->nargs;
    }
    return c;
}

void gmb_expr_free(gmb_expr_t* e)
{
    if (!e) return;
    if (e->kind == GMB_EXPR_BINARY) { gmb_expr_free(e->left); gmb_expr_free(e->right); }
    else if (e->kind == GMB_EXPR_UNARY) gmb_expr_free(e->left);
    else if (e->kind == GMB_EXPR_CALL) {
        for (int i = 0; i < e->nargs; i++) gmb_expr_free(e->args[i]);
        free(e->args);
        gmb_expr_free(e->callee);
    }
    else if (e->kind == GMB_EXPR_LAMBDA) {
        for (int i = 0; i < e->n_lambda_params; i++) free(e->lambda_params[i]);
        free(e->lambda_params);
        free(e->lambda_params_str);
        free(e->lambda_param_cls);
        free(e->lambda_param_cls2);
        gmb_expr_free(e->lambda_expr);
        free_stmt_array(e->lambda_body, e->n_lambda_body);
    }
    else if (e->kind == GMB_EXPR_COMPREHENSION) {
        gmb_expr_free(e->comp_src); gmb_expr_free(e->comp_where);
        gmb_expr_free(e->comp_order); gmb_expr_free(e->comp_sel);
        gmb_expr_free(e->comp_key);
        if (e->comp_var) free(e->comp_var);
        for (int i = 0; i < e->n_comp_more; i++) {
            gmb_expr_free(e->comp_more_src[i]);
            if (e->comp_more_var[i]) free(e->comp_more_var[i]);
        }
        free(e->comp_more_src); free(e->comp_more_var);
    }
    if (e->str) free(e->str);
    if (e->var) free(e->var);
    free(e);
}

gmb_stmt_t* gmb_stmt_new(gmb_stmt_kind_t kind, int line)
{
    gmb_stmt_t* s = (gmb_stmt_t*)calloc(1, sizeof(*s));
    s->kind = kind; s->line = line;
    return s;
}

void gmb_stmt_add(gmb_stmt_t*** list, int* n, gmb_stmt_t* s, int* cap)
{
    if (*n >= *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *list = (gmb_stmt_t**)realloc(*list, (size_t)(*cap) * sizeof(gmb_stmt_t*));
    }
    (*list)[*n] = s;
    (*n)++;
}

static void free_stmt_array(gmb_stmt_t** arr, int n)
{
    for (int i = 0; i < n; i++) gmb_stmt_free(arr[i]);
    free(arr);
}

void gmb_stmt_free(gmb_stmt_t* s)
{
    if (!s) return;
    for (int i = 0; i < s->nprint; i++) gmb_expr_free(s->print_args[i]);
    free(s->print_args);
    free(s->print_comma);
    if (s->target) free(s->target);
    gmb_expr_free(s->value);
    gmb_expr_free(s->array_index);
    for (int i = 0; i < s->n_array_idx_extra; i++) gmb_expr_free(s->array_idx_extra[i]);
    free(s->array_idx_extra);
    gmb_expr_free(s->cond);
    free_stmt_array(s->then_body, s->n_then);
    free_stmt_array(s->else_body, s->n_else);
    if (s->for_var) free(s->for_var);
    gmb_expr_free(s->for_from); gmb_expr_free(s->for_to); gmb_expr_free(s->for_step);
    free_stmt_array(s->for_body, s->n_for);
    free_stmt_array(s->while_body, s->n_while);
    gmb_expr_free(s->do_until);
    free_stmt_array(s->do_body, s->n_do);
    if (s->func_name) free(s->func_name);
    if (s->func_ret_type) free(s->func_ret_type);
    for (int i = 0; i < s->n_params; i++) free(s->func_params[i]);
    free(s->func_params);
    free(s->param_byref);
    free(s->param_is_array);
    free(s->param_ndims);
    if (s->param_types) {
        for (int i = 0; i < s->n_params; i++) free(s->param_types[i]);
        free(s->param_types);
    }
    free(s->param_variadic);
    free(s->param_optional);
    free(s->param_fn_str_mask);
    if (s->param_defaults) {
        for (int i = 0; i < s->n_params; i++) gmb_expr_free(s->param_defaults[i]);
        free(s->param_defaults);
    }
    free_stmt_array(s->func_body, s->n_funcbody);
    if (s->dim_var) free(s->dim_var);
    gmb_expr_free(s->dim_to_hi);
    for (int i = 0; i < s->n_dim_extra; i++) gmb_expr_free(s->dim_extra[i]);
    free(s->dim_extra);
    gmb_expr_free(s->dim_init);
    if (s->dim_type) free(s->dim_type);
    gmb_expr_free(s->ret_value);
    gmb_expr_free(s->file_num);
    gmb_expr_free(s->file_pos);
    gmb_expr_free(s->file_path_expr);
    if (s->exit_target) free(s->exit_target);
    if (s->target_label) free(s->target_label);
    gmb_expr_free(s->expr);
    for (int i = 0; i < s->n_case_tests; i++) {
        gmb_expr_free(s->case_tests[i]);
        gmb_expr_free(s->case_hi ? s->case_hi[i] : NULL);
        if (s->case_cmp) free(s->case_cmp[i]);
    }
    free(s->case_tests);
    free(s->case_hi);
    free(s->case_cmp);
    free_stmt_array(s->case_body, s->n_case_body);
    free_stmt_array(s->select_cases, s->n_select_cases);
    for (int i = 0; i < s->n_netargs; i++) gmb_expr_free(s->net_args[i]);
    free(s->net_args);
    free(s->net_coderef);
    free(s->net_assign);
    free(s->net_assign2);
    free(s->net_assign3);
    gmb_expr_free(s->channel);
    gmb_expr_free(s->send_value);
    free(s->receive_dest);
    gmb_expr_free(s->ok_flag);
    gmb_expr_free(s->on_expr);
    gmb_expr_free(s->on_call);
    gmb_expr_free(s->error_code);
    gmb_expr_free(s->throw_expr);
    if (s->catch_var) free(s->catch_var);
    if (s->catch_type) free(s->catch_type);
    gmb_expr_free(s->catch_when);
    free_stmt_array(s->try_body, s->n_try);
    free_stmt_array(s->catch_body, s->n_catch);
    free_stmt_array(s->finally_body, s->n_finally);
    for (int i = 0; i < s->n_go_cases; i++) {
        gmb_stmt_free(s->go_cases[i].comm);
        free_stmt_array(s->go_cases[i].body, s->go_cases[i].n_body);
    }
    free(s->go_cases);
    gmb_stmt_free(s->go_comm);
    gmb_expr_free(s->cond_mutex);
    gmb_expr_free(s->go_wait);
    free(s->go_into);
    gmb_expr_free(s->proc_name);
    for (int i = 0; i < s->n_go_args; i++) gmb_expr_free(s->go_args[i]);
    free(s->go_args);
    /* CLASS (2.7) */
    if (s->class_name) free(s->class_name);
    if (s->class_base) free(s->class_base);
    for (int i = 0; i < s->n_cls_implements; i++) free(s->cls_implements[i]);
    free(s->cls_implements);
    for (int i = 0; i < s->n_cls_fields; i++) { free(s->cls_field_names[i]); free(s->cls_field_types[i]); }
    free(s->cls_field_names);
    free(s->cls_field_types);
    for (int i = 0; i < s->n_cls_events; i++) free(s->cls_event_names[i]);
    free(s->cls_event_names);
    free_stmt_array(s->cls_methods, s->n_cls_methods);
    free(s);
}
