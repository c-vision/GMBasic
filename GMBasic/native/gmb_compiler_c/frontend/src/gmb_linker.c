/* native/gmb_runtime_c/src/gmb_linker.c — linker + dump AST (Fase E). */
#include "gmb_linker.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── tabella simboli ── */
static int sym_find(const gmb_link_result_t* r, const char* name)
{
    for (int i = 0; i < r->count; i++)
        if (strcasecmp(r->syms[i].name, name) == 0) return i;
    return -1;
}

static gmb_symbol_t* sym_add(gmb_link_result_t* r, const char* name, gmb_sym_kind_t kind, int line)
{
    if (r->count >= GMB_LINK_MAX_SYMS) return NULL;
    gmb_symbol_t* s = &r->syms[r->count++];
    s->name = strdup(name);
    s->kind = kind;
    s->decl_line = line;
    s->used = false;
    s->implicit = false;
    return s;
}

static void sym_declare(gmb_link_result_t* r, const char* name, gmb_sym_kind_t kind, int line)
{
    int i = sym_find(r, name);
    if (i >= 0) {
        gmb_symbol_t* s = &r->syms[i];
        if (s->implicit) {
            /* forward reference: l'uso precedeva la dichiarazione — aggiorna a dichiarazione reale */
            s->implicit = false;
            s->kind = kind;
            s->decl_line = line;
            if (r->undefined_count > 0) r->undefined_count--;
            if (r->error && strcmp(r->error, "riferimento a simbolo non dichiarato") == 0) {
                free(r->error); r->error = NULL; r->error_line = 0;
            }
            return;
        }
        if (s->kind == kind && !r->error) {
            r->error = strdup("definizione duplicata");
            r->error_line = line;
            r->duplicate_count++;
        }
        return;
    }
    sym_add(r, name, kind, line);
}

static void sym_use(gmb_link_result_t* r, const char* name, int line)
{
    int i = sym_find(r, name);
    if (i < 0) {
        /* forward reference / variabile non dichiarata: aggiungi come uso implicito;
         * se una dichiarazione arriverà dopo, verrà aggiornata (sym_declare) */
        gmb_symbol_t* s = sym_add(r, name, GMB_SYM_VARIABLE, line);
        if (s) { s->used = true; s->implicit = true; r->undefined_count++; if (!r->error) { r->error = strdup("riferimento a simbolo non dichiarato"); r->error_line = line; } }
        return;
    }
    r->syms[i].used = true;
}

/* ── visita espressioni ── */
static void link_expr(gmb_link_result_t* r, gmb_expr_t* e)
{
    if (!e) return;
    if (e->kind == GMB_EXPR_VAR) sym_use(r, e->var, e->line);
    else if (e->kind == GMB_EXPR_BINARY) { link_expr(r, e->left); link_expr(r, e->right); }
    else if (e->kind == GMB_EXPR_UNARY) link_expr(r, e->left);
    else if (e->kind == GMB_EXPR_CALL) {
        sym_use(r, e->var, e->line);
        for (int i = 0; i < e->nargs; i++) link_expr(r, e->args[i]);
    }
}

static void link_stmt(gmb_link_result_t* r, gmb_stmt_t* s);

static void link_body(gmb_link_result_t* r, gmb_stmt_t** arr, int n)
{
    for (int i = 0; i < n; i++) link_stmt(r, arr[i]);
}

static void link_stmt(gmb_link_result_t* r, gmb_stmt_t* s)
{
    if (!s) return;
    switch (s->kind) {
        case GMB_STMT_PRINT:
            for (int i = 0; i < s->nprint; i++) link_expr(r, s->print_args[i]);
            break;
        case GMB_STMT_ASSIGN:
        case GMB_STMT_LET:
            sym_use(r, s->target, s->line);
            link_expr(r, s->value);
            break;
        case GMB_STMT_IF:
            link_expr(r, s->cond);
            link_body(r, s->then_body, s->n_then);
            link_body(r, s->else_body, s->n_else);
            break;
        case GMB_STMT_FOR:
            sym_declare(r, s->for_var, GMB_SYM_VARIABLE, s->line);
            link_expr(r, s->for_from); link_expr(r, s->for_to); link_expr(r, s->for_step);
            link_body(r, s->for_body, s->n_for);
            break;
        case GMB_STMT_WHILE:
            link_expr(r, s->cond);
            link_body(r, s->while_body, s->n_while);
            break;
        case GMB_STMT_DO:
            link_body(r, s->do_body, s->n_do);
            link_expr(r, s->do_until);
            break;
        case GMB_STMT_FUNCTION:
            sym_declare(r, s->func_name, GMB_SYM_FUNCTION, s->line);
            for (int i = 0; i < s->n_params; i++) sym_declare(r, s->func_params[i], GMB_SYM_VARIABLE, s->line);
            link_body(r, s->func_body, s->n_funcbody);
            break;
        case GMB_STMT_DIM:
            sym_declare(r, s->dim_var, GMB_SYM_VARIABLE, s->line);
            break;
        case GMB_STMT_RETURN:
            link_expr(r, s->ret_value);
            break;
        case GMB_STMT_GOTO: case GMB_STMT_GOSUB:
            sym_use(r, s->target_label, s->line);
            break;
        case GMB_STMT_EXPR: case GMB_STMT_CALL:
            link_expr(r, s->expr);
            break;
        case GMB_STMT_SELECT:
            link_expr(r, s->cond);
            for (int i = 0; i < s->n_select_cases; i++) link_stmt(r, s->select_cases[i]);
            break;
        case GMB_STMT_LABEL:
            sym_declare(r, s->target_label, GMB_SYM_VARIABLE, s->line);
            break;
        case GMB_STMT_CASE:
            for (int i = 0; i < s->n_case_tests; i++) link_expr(r, s->case_tests[i]);
            link_body(r, s->case_body, s->n_case_body);
            break;
        case GMB_STMT_TRY:
            link_body(r, s->try_body, s->n_try);
            for (int i = 0; i < s->n_catch; i++) link_stmt(r, s->catch_body[i]);
            link_body(r, s->finally_body, s->n_finally);
            break;
        case GMB_STMT_CATCH:
            if (s->catch_var) sym_declare(r, s->catch_var, GMB_SYM_VARIABLE, s->line);
            link_body(r, s->catch_body, s->n_catch);
            break;
        case GMB_STMT_ERROR: link_expr(r, s->error_code); break;
        case GMB_STMT_DATA: for (int i = 0; i < s->n_data; i++) link_expr(r, s->data_values[i]); break;
        case GMB_STMT_READ: for (int i = 0; i < s->n_read; i++) sym_use(r, s->read_vars[i], s->line); break;
        case GMB_STMT_RESTORE: break;
        case GMB_STMT_REDIM:
            if (s->dim_var) sym_declare(r, s->dim_var, GMB_SYM_VARIABLE, s->line);
            break;
        case GMB_STMT_SWAP:
            if (s->swap_a) sym_use(r, s->swap_a, s->line);
            if (s->swap_b) sym_use(r, s->swap_b, s->line);
            break;
        case GMB_STMT_STOP: case GMB_STMT_RUN: case GMB_STMT_REQUIRE: case GMB_STMT_YIELD: break;
        case GMB_STMT_AADD: case GMB_STMT_AINS: case GMB_STMT_ADEL: case GMB_STMT_ASHUFFLE: case GMB_STMT_ASORT:
            if (s->array_var) sym_use(r, s->array_var, s->line);
            link_expr(r, s->array_value);
            link_expr(r, s->array_position);
            break;
        case GMB_STMT_CLS: case GMB_STMT_INKEY: break;
        case GMB_STMT_LOCATE: case GMB_STMT_COLOR:
            link_expr(r, s->row); link_expr(r, s->col); link_expr(r, s->fg); link_expr(r, s->bg);
            break;
        case GMB_STMT_INPUT:
            link_expr(r, s->input_prompt);
            for (int i = 0; i < s->n_read; i++) sym_use(r, s->read_vars[i], s->line);
            break;
        case GMB_STMT_SET: link_expr(r, s->row); break;
        case GMB_STMT_OPEN: link_expr(r, s->file_num); break;
        case GMB_STMT_CLOSE: link_expr(r, s->file_num); break;
        case GMB_STMT_SEEK: link_expr(r, s->file_num); link_expr(r, s->file_pos); break;
        case GMB_STMT_PRINTFILE: link_expr(r, s->file_num); for (int i = 0; i < s->nprint; i++) link_expr(r, s->print_args[i]); break;
        case GMB_STMT_INPUTFILE: case GMB_STMT_LINEINPUT:
            link_expr(r, s->file_num);
            for (int i = 0; i < s->n_read; i++) sym_use(r, s->read_vars[i], s->line);
            break;
        case GMB_STMT_GET: case GMB_STMT_PUT: break;
        case GMB_STMT_CONTROL: for (int i = 0; i < s->n_control_args; i++) link_expr(r, s->control_args[i]); break;
        case GMB_STMT_GO:
            link_expr(r, s->proc_name);
            for (int i = 0; i < s->n_go_args; i++) link_expr(r, s->go_args[i]);
            if (s->go_wait) link_expr(r, s->go_wait);
            if (s->go_into) sym_use(r, s->go_into, s->line);
            break;
        case GMB_STMT_GOSELECT:
            for (int i = 0; i < s->n_go_cases; i++) {
                if (s->go_cases[i].comm) link_stmt(r, s->go_cases[i].comm);
                for (int j = 0; j < s->go_cases[i].n_body; j++) link_stmt(r, s->go_cases[i].body[j]);
            }
            break;
        case GMB_STMT_TIMEOUT: case GMB_STMT_WAIT: case GMB_STMT_DONE: case GMB_STMT_SLEEP:
            link_expr(r, s->channel); break;
        case GMB_STMT_DEFER: if (s->go_comm) link_stmt(r, s->go_comm); break;
        case GMB_STMT_SEND: link_expr(r, s->send_value); link_expr(r, s->channel); break;
        case GMB_STMT_RECEIVE:
            if (s->receive_dest) sym_use(r, s->receive_dest, s->line);
            link_expr(r, s->channel);
            if (s->ok_flag) sym_use(r, s->ok_flag->var ? s->ok_flag->var : "", s->line);
            break;
        case GMB_STMT_CHANNEL: case GMB_STMT_MUTEX: case GMB_STMT_COND:
            if (s->channel) link_expr(r, s->channel);
            if (s->cond_mutex) link_expr(r, s->cond_mutex);
            break;
        case GMB_STMT_ONCE:
            link_expr(r, s->channel);
            if (s->proc_name) sym_use(r, s->proc_name->var ? s->proc_name->var : "", s->line);
            break;
        case GMB_STMT_RAISE_EVENT:
            if (s->proc_name) sym_use(r, s->proc_name->var ? s->proc_name->var : "", s->line);
            for (int i = 0; i < s->n_go_args; i++) link_expr(r, s->go_args[i]);
            break;
        case GMB_STMT_ADD_HANDLER: case GMB_STMT_REMOVE_HANDLER:
            link_expr(r, s->channel); link_expr(r, s->send_value);
            break;
        case GMB_STMT_WINDOW_THEME: case GMB_STMT_WINDOW_DESKTOP: case GMB_STMT_WINDOW_SETTITLE:
        case GMB_STMT_WINDOW_APPEND: case GMB_STMT_WINDOW_CONTENT: case GMB_STMT_WINDOW_SCROLL:
        case GMB_STMT_WINDOW_MOVEDESKTOP:
            link_expr(r, s->channel); break;
        case GMB_STMT_WINDOW_DEFINE:
            for (int i = 0; i < 4; i++) if (s->win_coords[i]) link_expr(r, s->win_coords[i]);
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_WINDOW_FRAME: case GMB_STMT_WINDOW_SHADOW: case GMB_STMT_WINDOW_SELECT:
        case GMB_STMT_WINDOW_CLEAR: case GMB_STMT_WINDOW_EDIT: case GMB_STMT_WINDOW_ACTION:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_MOUSE_SET: for (int i = 0; i < 2; i++) link_expr(r, s->win_coords[i]); break;
        case GMB_STMT_MOUSE_LIMIT: for (int i = 0; i < 4; i++) if (s->win_coords[i]) link_expr(r, s->win_coords[i]); break;
        case GMB_STMT_MOUSE_CURSOR: link_expr(r, s->channel); break;
        case GMB_STMT_MOUSE_CONFIG: case GMB_STMT_MOUSE_VISIBILITY: case GMB_STMT_MOUSE_WAIT: break;
        case GMB_STMT_ZIP: case GMB_STMT_UNZIP: link_expr(r, s->channel); link_expr(r, s->send_value); break;
        case GMB_STMT_JSON_SAVE: case GMB_STMT_JSON_WRITESECTION: case GMB_STMT_JSON_OPEN:
            link_expr(r, s->channel); if (s->send_value) link_expr(r, s->send_value); break;
        case GMB_STMT_JSON_WRITEVALUE:
            for (int i = 0; i < s->n_go_args; i++) link_expr(r, s->go_args[i]);
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_MCP:
            for (int i = 0; i < s->n_go_args; i++) link_expr(r, s->go_args[i]);
            break;
        case GMB_STMT_TEAM_CREATE: case GMB_STMT_TEAM_SAVE: case GMB_STMT_TEAM_LOAD: break;
        case GMB_STMT_TEAM_ADDAGENT: case GMB_STMT_TEAM_REMOVEAGENT: case GMB_STMT_TEAM_RUN:
        case GMB_STMT_TEAM_MESSAGE:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_SETROLE:
            if (s->channel) link_expr(r, s->channel);
            if (s->send_value) link_expr(r, s->send_value);
            break;
        case GMB_STMT_TEAM_SETSTRATEGY:
            if (s->send_value) link_expr(r, s->send_value);
            break;
        case GMB_STMT_TEAM_SETCOORDINATOR:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_SETRECURSION:
        case GMB_STMT_TEAM_SETTHOUGHTS:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_SETLATENT:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_SETLINK:
            if (s->channel) link_expr(r, s->channel);
            if (s->send_value) link_expr(r, s->send_value);
            break;
        case GMB_STMT_TEAM_TRAININNER:
            if (s->channel) link_expr(r, s->channel);
            if (s->send_value) link_expr(r, s->send_value);
            break;
        case GMB_STMT_TEAM_TRAINOUTER:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_SETDAG:
            if (s->channel) link_expr(r, s->channel);
            if (s->send_value) link_expr(r, s->send_value);
            if (s->cond_mutex) link_expr(r, s->cond_mutex);
            break;
        case GMB_STMT_TEAM_SETCOMPLEXITY:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_SETREFLECTION:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_SETCRITIC:
            if (s->channel) link_expr(r, s->channel);
            if (s->send_value) link_expr(r, s->send_value);
            break;
        case GMB_STMT_TEAM_SETPRUNE:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_TRAINTHINKING:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_TRAINACTORCRITIC:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_TEAM_DISTILLTHINKING:
            if (s->channel) link_expr(r, s->channel);
            break;
        case GMB_STMT_DOMAIN:
            for (int i = 0; i < s->n_go_args; i++) link_expr(r, s->go_args[i]);
            break;
        case GMB_STMT_THROW: link_expr(r, s->throw_expr); break;
        case GMB_STMT_FINALLY: break;
        case GMB_STMT_ON_ERROR: case GMB_STMT_ON_KEY: case GMB_STMT_ON_MOUSE: case GMB_STMT_ON_WINDOW:
            link_expr(r, s->on_expr);
            if (s->target_label) sym_use(r, s->target_label, s->line);
            link_expr(r, s->on_call);
            break;
        case GMB_STMT_EXIT: case GMB_STMT_CONTINUE: case GMB_STMT_END: case GMB_STMT_WEND:
        case GMB_STMT_LOOP: case GMB_STMT_NEXT: case GMB_STMT_REM:
        case GMB_STMT_END_FUNCTION:
            break;
    }
}

void gmb_link_resolve(gmb_link_result_t* r, gmb_stmt_t** stmts, int n)
{
    memset(r, 0, sizeof(*r));
    for (int i = 0; i < n; i++) link_stmt(r, stmts[i]);
}

void gmb_link_free(gmb_link_result_t* r)
{
    for (int i = 0; i < r->count; i++) free(r->syms[i].name);
    if (r->error) free(r->error);
    r->count = 0; r->error = NULL;
}

/* ── dump AST ── */
static void dump_expr(gmb_expr_t* e, FILE* out, int indent)
{
    for (int i = 0; i < indent; i++) fputc(' ', out);
    if (!e) { fprintf(out, "(null)\n"); return; }
    switch (e->kind) {
        case GMB_EXPR_NUM: fprintf(out, "NUM %.6g\n", e->num); break;
        case GMB_EXPR_STR: fprintf(out, "STR \"%s\"\n", e->str ? e->str : ""); break;
        case GMB_EXPR_VAR: fprintf(out, "VAR %s\n", e->var); break;
        case GMB_EXPR_BINARY:
            fprintf(out, "BINARY %s\n", e->op);
            dump_expr(e->left, out, indent + 2);
            dump_expr(e->right, out, indent + 2);
            break;
        case GMB_EXPR_UNARY:
            fprintf(out, "UNARY %s\n", e->op);
            dump_expr(e->left, out, indent + 2);
            break;
        case GMB_EXPR_CALL:
            fprintf(out, "CALL %s (args %d)\n", e->var, e->nargs);
            for (int i = 0; i < e->nargs; i++) dump_expr(e->args[i], out, indent + 2);
            break;
    }
}

static void dump_stmt(gmb_stmt_t* s, FILE* out, int indent)
{
    for (int i = 0; i < indent; i++) fputc(' ', out);
    if (!s) { fprintf(out, "(null)\n"); return; }
    switch (s->kind) {
        case GMB_STMT_PRINT: fprintf(out, "PRINT (L%d)\n", s->line); for (int i = 0; i < s->nprint; i++) dump_expr(s->print_args[i], out, indent + 2); break;
        case GMB_STMT_LET: case GMB_STMT_ASSIGN: fprintf(out, "%s %s = (L%d)\n", s->kind == GMB_STMT_LET ? "LET" : "ASSIGN", s->target, s->line); dump_expr(s->value, out, indent + 2); break;
        case GMB_STMT_IF:
            fprintf(out, "IF (L%d)\n", s->line);
            dump_expr(s->cond, out, indent + 2);
            fprintf(out, "  THEN\n"); for (int i = 0; i < s->n_then; i++) dump_stmt(s->then_body[i], out, indent + 4);
            fprintf(out, "  ELSE\n"); for (int i = 0; i < s->n_else; i++) dump_stmt(s->else_body[i], out, indent + 4);
            break;
        case GMB_STMT_FOR:
            fprintf(out, "FOR %s (L%d)\n", s->for_var, s->line);
            dump_expr(s->for_from, out, indent + 2); dump_expr(s->for_to, out, indent + 2); dump_expr(s->for_step, out, indent + 2);
            for (int i = 0; i < s->n_for; i++) dump_stmt(s->for_body[i], out, indent + 2);
            break;
        case GMB_STMT_WHILE:
            fprintf(out, "WHILE (L%d)\n", s->line); dump_expr(s->cond, out, indent + 2);
            for (int i = 0; i < s->n_while; i++) dump_stmt(s->while_body[i], out, indent + 2);
            break;
        case GMB_STMT_DO:
            fprintf(out, "DO (L%d)\n", s->line);
            for (int i = 0; i < s->n_do; i++) dump_stmt(s->do_body[i], out, indent + 2);
            fprintf(out, "  UNTIL "); dump_expr(s->do_until, out, indent + 2);
            break;
        case GMB_STMT_FUNCTION:
            fprintf(out, "FUNCTION %s(%d params) (L%d)\n", s->func_name, s->n_params, s->line);
            for (int i = 0; i < s->n_funcbody; i++) dump_stmt(s->func_body[i], out, indent + 2);
            break;
        case GMB_STMT_DIM: fprintf(out, "DIM %s (L%d)\n", s->dim_var, s->line); break;
        case GMB_STMT_RETURN: fprintf(out, "RETURN (L%d)\n", s->line); dump_expr(s->ret_value, out, indent + 2); break;
        case GMB_STMT_GOTO: fprintf(out, "GOTO %s (L%d)\n", s->target_label, s->line); break;
        case GMB_STMT_GOSUB: fprintf(out, "GOSUB %s (L%d)\n", s->target_label, s->line); break;
        case GMB_STMT_END: fprintf(out, "END (L%d)\n", s->line); break;
        case GMB_STMT_EXIT: fprintf(out, "EXIT %s (L%d)\n", s->exit_target, s->line); break;
        case GMB_STMT_CONTINUE: fprintf(out, "CONTINUE %s (L%d)\n", s->exit_target ? s->exit_target : "", s->line); break;
        case GMB_STMT_CALL: fprintf(out, "CALL (L%d)\n", s->line); dump_expr(s->expr, out, indent + 2); break;
        case GMB_STMT_EXPR: fprintf(out, "EXPR (L%d)\n", s->line); dump_expr(s->expr, out, indent + 2); break;
        case GMB_STMT_REM: fprintf(out, "REM (L%d)\n", s->line); break;
        case GMB_STMT_LABEL: fprintf(out, "LABEL %s (L%d)\n", s->target_label, s->line); break;
        case GMB_STMT_ON_ERROR: fprintf(out, "ON ERROR %s (L%d)\n", s->on_kind ? s->on_kind : "", s->line); if (s->target_label) fprintf(out, "  -> %s\n", s->target_label); dump_expr(s->on_call, out, indent + 2); break;
        case GMB_STMT_ON_KEY: fprintf(out, "ON KEY (L%d)\n", s->line); dump_expr(s->on_expr, out, indent + 2); if (s->target_label) fprintf(out, "  %s %s\n", s->on_goto ? "GOTO" : "GOSUB", s->target_label); break;
        case GMB_STMT_ON_MOUSE: fprintf(out, "ON MOUSE (L%d)\n", s->line); break;
        case GMB_STMT_ON_WINDOW: fprintf(out, "ON WINDOW (L%d)\n", s->line); break;
        case GMB_STMT_TRY:
            fprintf(out, "TRY (L%d)\n", s->line);
            for (int i = 0; i < s->n_try; i++) dump_stmt(s->try_body[i], out, indent + 2);
            for (int i = 0; i < s->n_catch; i++) dump_stmt(s->catch_body[i], out, indent + 2);
            fprintf(out, "  FINALLY\n");
            for (int i = 0; i < s->n_finally; i++) dump_stmt(s->finally_body[i], out, indent + 4);
            break;
        case GMB_STMT_CATCH:
            fprintf(out, "CATCH %s (L%d)\n", s->catch_var ? s->catch_var : "", s->line);
            for (int i = 0; i < s->n_catch; i++) dump_stmt(s->catch_body[i], out, indent + 2);
            break;
        case GMB_STMT_FINALLY: fprintf(out, "FINALLY (L%d)\n", s->line); break;
        case GMB_STMT_ERROR: fprintf(out, "ERROR (L%d)\n", s->line); dump_expr(s->error_code, out, indent + 2); break;
        case GMB_STMT_THROW: fprintf(out, "THROW (L%d)\n", s->line); dump_expr(s->throw_expr, out, indent + 2); break;
        case GMB_STMT_DATA: fprintf(out, "DATA (L%d)\n", s->line); for (int i = 0; i < s->n_data; i++) dump_expr(s->data_values[i], out, indent + 2); break;
        case GMB_STMT_READ: fprintf(out, "READ (L%d):", s->line); for (int i = 0; i < s->n_read; i++) fprintf(out, " %s", s->read_vars[i]); fprintf(out, "\n"); break;
        case GMB_STMT_RESTORE: fprintf(out, "RESTORE %s (L%d)\n", s->target_label ? s->target_label : "", s->line); break;
        case GMB_STMT_REDIM: fprintf(out, "REDIM %s (L%d)\n", s->dim_var, s->line); break;
        case GMB_STMT_SWAP: fprintf(out, "SWAP %s, %s (L%d)\n", s->swap_a ? s->swap_a : "", s->swap_b ? s->swap_b : "", s->line); break;
        case GMB_STMT_STOP: fprintf(out, "STOP (L%d)\n", s->line); break;
        case GMB_STMT_RUN: fprintf(out, "RUN %s (L%d)\n", s->run_file ? s->run_file : "", s->line); break;
        case GMB_STMT_REQUIRE: fprintf(out, "REQUIRE %s (L%d)\n", s->require_pkg ? s->require_pkg : "", s->line); break;
        case GMB_STMT_YIELD: fprintf(out, "YIELD (L%d)\n", s->line); break;
        case GMB_STMT_AADD: fprintf(out, "AADD %s (L%d)\n", s->array_var ? s->array_var : "", s->line); dump_expr(s->array_value, out, indent + 2); break;
        case GMB_STMT_AINS: fprintf(out, "AINS %s (L%d)\n", s->array_var ? s->array_var : "", s->line); dump_expr(s->array_position, out, indent + 2); dump_expr(s->array_value, out, indent + 2); break;
        case GMB_STMT_ADEL: fprintf(out, "ADEL %s (L%d)\n", s->array_var ? s->array_var : "", s->line); dump_expr(s->array_position, out, indent + 2); break;
        case GMB_STMT_ASHUFFLE: fprintf(out, "ASHUFFLE %s (L%d)\n", s->array_var ? s->array_var : "", s->line); break;
        case GMB_STMT_ASORT: fprintf(out, "ASORT %s (L%d)\n", s->array_var ? s->array_var : "", s->line); dump_expr(s->array_value, out, indent + 2); break;
        case GMB_STMT_CLS: fprintf(out, "CLS (L%d)\n", s->line); break;
        case GMB_STMT_LOCATE: fprintf(out, "LOCATE (L%d)\n", s->line); dump_expr(s->row, out, indent + 2); dump_expr(s->col, out, indent + 2); break;
        case GMB_STMT_COLOR: fprintf(out, "COLOR (L%d)\n", s->line); dump_expr(s->fg, out, indent + 2); dump_expr(s->bg, out, indent + 2); break;
        case GMB_STMT_INKEY: fprintf(out, "INKEY (L%d)\n", s->line); break;
        case GMB_STMT_INPUT: fprintf(out, "INPUT (L%d)\n", s->line); dump_expr(s->input_prompt, out, indent + 2); for (int i = 0; i < s->n_read; i++) fprintf(out, "  %s\n", s->read_vars[i]); break;
        case GMB_STMT_SET: fprintf(out, "SET %s (L%d)\n", s->n_set_attr ? s->set_attr[0] : "", s->line); dump_expr(s->row, out, indent + 2); break;
        case GMB_STMT_OPEN: fprintf(out, "OPEN %s FOR %s AS (L%d)\n", s->file_path ? s->file_path : "", s->file_mode ? s->file_mode : "", s->line); dump_expr(s->file_num, out, indent + 2); break;
        case GMB_STMT_CLOSE: fprintf(out, "CLOSE (L%d)\n", s->line); dump_expr(s->file_num, out, indent + 2); break;
        case GMB_STMT_SEEK: fprintf(out, "SEEK (L%d)\n", s->line); dump_expr(s->file_num, out, indent + 2); dump_expr(s->file_pos, out, indent + 2); break;
        case GMB_STMT_PRINTFILE: fprintf(out, "PRINT# (L%d)\n", s->line); dump_expr(s->file_num, out, indent + 2); for (int i = 0; i < s->nprint; i++) dump_expr(s->print_args[i], out, indent + 2); break;
        case GMB_STMT_INPUTFILE: fprintf(out, "INPUT# (L%d)\n", s->line); dump_expr(s->file_num, out, indent + 2); for (int i = 0; i < s->n_read; i++) fprintf(out, "  %s\n", s->read_vars[i]); break;
        case GMB_STMT_LINEINPUT: fprintf(out, "LINEINPUT# (L%d)\n", s->line); dump_expr(s->file_num, out, indent + 2); for (int i = 0; i < s->n_read; i++) fprintf(out, "  %s\n", s->read_vars[i]); break;
        case GMB_STMT_GET: fprintf(out, "GET (L%d)\n", s->line); break;
        case GMB_STMT_PUT: fprintf(out, "PUT (L%d)\n", s->line); break;
        case GMB_STMT_CONTROL:
            fprintf(out, "CONTROL (L%d)\n", s->line);
            for (int i = 0; i < s->n_control_args; i++) dump_expr(s->control_args[i], out, indent + 2);
            break;
        case GMB_STMT_GO: fprintf(out, "GO (L%d)\n", s->line); dump_expr(s->proc_name, out, indent + 2); for (int i = 0; i < s->n_go_args; i++) dump_expr(s->go_args[i], out, indent + 2); if (s->go_wait) { fprintf(out, "  IN\n"); dump_expr(s->go_wait, out, indent + 2); } if (s->go_into) fprintf(out, "  INTO %s\n", s->go_into); break;
        case GMB_STMT_GOEXIT: fprintf(out, "GO EXIT (L%d)\n", s->line); break;
        case GMB_STMT_GOSELECT:
            fprintf(out, "GO SELECT (L%d, %d casi)\n", s->line, s->n_go_cases);
            for (int i = 0; i < s->n_go_cases; i++) { dump_stmt(s->go_cases[i].comm, out, indent + 2); for (int j = 0; j < s->go_cases[i].n_body; j++) dump_stmt(s->go_cases[i].body[j], out, indent + 2); }
            break;
        case GMB_STMT_TIMEOUT: fprintf(out, "TIMEOUT (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_WAIT: fprintf(out, "WAIT (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_DONE: fprintf(out, "DONE (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_SLEEP: fprintf(out, "SLEEP (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_DEFER: fprintf(out, "DEFER (L%d)\n", s->line); if (s->go_comm) dump_stmt(s->go_comm, out, indent + 2); break;
        case GMB_STMT_SEND: fprintf(out, "SEND (L%d)\n", s->line); dump_expr(s->send_value, out, indent + 2); fprintf(out, "  TO\n"); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_RECEIVE: fprintf(out, "RECEIVE (L%d) %s FROM\n", s->line, s->receive_dest ? s->receive_dest : "?"); dump_expr(s->channel, out, indent + 2); if (s->ok_flag) dump_expr(s->ok_flag, out, indent + 2); break;
        case GMB_STMT_CHANNEL: fprintf(out, "CHANNEL %s (L%d)\n", s->mutex_action ? s->mutex_action : "", s->line); if (s->channel) dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_MUTEX: fprintf(out, "MUTEX %s (L%d)\n", s->mutex_action ? s->mutex_action : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_COND: fprintf(out, "COND %s (L%d)\n", s->mutex_action ? s->mutex_action : "", s->line); dump_expr(s->channel, out, indent + 2); if (s->cond_mutex) dump_expr(s->cond_mutex, out, indent + 2); break;
        case GMB_STMT_ONCE: fprintf(out, "ONCE RUN (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); if (s->proc_name) dump_expr(s->proc_name, out, indent + 2); break;
        case GMB_STMT_RAISE_EVENT: fprintf(out, "RAISE EVENT (L%d)\n", s->line); dump_expr(s->proc_name, out, indent + 2); for (int i = 0; i < s->n_go_args; i++) dump_expr(s->go_args[i], out, indent + 2); break;
        case GMB_STMT_WINDOW_THEME: fprintf(out, "WINDOW THEME (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_WINDOW_DESKTOP: fprintf(out, "WINDOW DESKTOP (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_WINDOW_FRAME: fprintf(out, "WINDOW FRAME %s (L%d)\n", s->mutex_action ? s->mutex_action : "", s->line); break;
        case GMB_STMT_WINDOW_SHADOW: fprintf(out, "WINDOW SHADOW %s (L%d)\n", s->mutex_action ? s->mutex_action : "", s->line); break;
        case GMB_STMT_WINDOW_SELECT: fprintf(out, "WINDOW SELECT %s (L%d)\n", s->win_name ? s->win_name : "", s->line); if (s->channel) dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_WINDOW_DEFINE: fprintf(out, "WINDOW DEFINE %s (L%d)\n", s->win_name ? s->win_name : "", s->line); for (int i = 0; i < 4; i++) if (s->win_coords[i]) dump_expr(s->win_coords[i], out, indent + 2); if (s->channel) { fprintf(out, "  TITLE\n"); dump_expr(s->channel, out, indent + 2); } break;
        case GMB_STMT_WINDOW_SETTITLE: fprintf(out, "WINDOW SET TITLE (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_WINDOW_APPEND: fprintf(out, "WINDOW %s PRINT (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_WINDOW_CONTENT: fprintf(out, "WINDOW %s TEXT (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_WINDOW_CLEAR: fprintf(out, "WINDOW %s CLEAR (L%d)\n", s->win_name ? s->win_name : "", s->line); break;
        case GMB_STMT_WINDOW_SCROLL: fprintf(out, "WINDOW %s SCROLL (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_WINDOW_EDIT: fprintf(out, "WINDOW %s EDIT %s (L%d)\n", s->win_name ? s->win_name : "", s->mutex_action ? s->mutex_action : "", s->line); break;
        case GMB_STMT_WINDOW_MOVEDESKTOP: fprintf(out, "WINDOW %s DESKTOP (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_WINDOW_ACTION: fprintf(out, "WINDOW %s %s (L%d)\n", s->win_name ? s->win_name : "", s->mutex_action ? s->mutex_action : "", s->line); break;
        case GMB_STMT_MOUSE_CONFIG: fprintf(out, "MOUSE %s (L%d)\n", s->mutex_action ? s->mutex_action : "", s->line); break;
        case GMB_STMT_MOUSE_VISIBILITY: fprintf(out, "MOUSE %s (L%d)\n", s->mutex_action ? s->mutex_action : "", s->line); break;
        case GMB_STMT_MOUSE_SET: fprintf(out, "MOUSE SET (L%d)\n", s->line); for (int i = 0; i < 2; i++) dump_expr(s->win_coords[i], out, indent + 2); break;
        case GMB_STMT_MOUSE_LIMIT: fprintf(out, "MOUSE %s (L%d)\n", s->win_coords[0] ? "LIMIT" : "UNLIMIT", s->line); for (int i = 0; i < 4; i++) if (s->win_coords[i]) dump_expr(s->win_coords[i], out, indent + 2); break;
        case GMB_STMT_MOUSE_CURSOR: fprintf(out, "MOUSE CURSOR (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_MOUSE_WAIT: fprintf(out, "MOUSE WAIT (L%d)\n", s->line); break;
        case GMB_STMT_ZIP: fprintf(out, "ZIP (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); fprintf(out, "  TO\n"); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_UNZIP: fprintf(out, "UNZIP (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); fprintf(out, "  TO\n"); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_JSON_SAVE: fprintf(out, "JSON SAVE (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_JSON_WRITEVALUE: fprintf(out, "JSON WRITEVALUE (L%d)\n", s->line); for (int i = 0; i < s->n_go_args; i++) dump_expr(s->go_args[i], out, indent + 2); if (s->channel) dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_JSON_WRITESECTION: fprintf(out, "JSON WRITESECTION (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); if (s->send_value) dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_JSON_OPEN: fprintf(out, "JSON OPEN %s (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_MCP: fprintf(out, "AI MCP %s (L%d)\n", s->mutex_action ? s->mutex_action : "", s->line); for (int i = 0; i < s->n_go_args; i++) dump_expr(s->go_args[i], out, indent + 2); break;
        case GMB_STMT_TEAM_CREATE: fprintf(out, "TEAM CREATE %s (L%d)\n", s->win_name ? s->win_name : "", s->line); break;
        case GMB_STMT_TEAM_SAVE: fprintf(out, "TEAM SAVE %s (L%d)\n", s->win_name ? s->win_name : "", s->line); break;
        case GMB_STMT_TEAM_LOAD: fprintf(out, "TEAM LOAD %s (L%d)\n", s->win_name ? s->win_name : "", s->line); break;
        case GMB_STMT_TEAM_ADDAGENT: fprintf(out, "TEAM %s ADD AGENT (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_REMOVEAGENT: fprintf(out, "TEAM %s REMOVE AGENT (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_RUN: fprintf(out, "TEAM %s RUN (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_SETROLE: fprintf(out, "TEAM %s SET ROLE (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_TEAM_SETSTRATEGY: fprintf(out, "TEAM %s SET STRATEGY (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_TEAM_SETCOORDINATOR: fprintf(out, "TEAM %s SET COORDINATOR (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_MESSAGE: fprintf(out, "TEAM MESSAGE %s (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_SETRECURSION: fprintf(out, "TEAM %s SET RECURSION (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_SETLATENT: fprintf(out, "TEAM %s SET LATENT (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_SETLINK: fprintf(out, "TEAM %s SET LINK (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_TEAM_SETTHOUGHTS: fprintf(out, "TEAM %s SET THOUGHTS (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_TRAININNER: fprintf(out, "TEAM TRAIN INNER %s (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_TEAM_TRAINOUTER: fprintf(out, "TEAM TRAIN OUTER %s (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_SETDAG: fprintf(out, "TEAM %s SET DAG (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); dump_expr(s->send_value, out, indent + 2); dump_expr(s->cond_mutex, out, indent + 2); break;
        case GMB_STMT_TEAM_SETCOMPLEXITY: fprintf(out, "TEAM %s SET COMPLEXITY (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_SETREFLECTION: fprintf(out, "TEAM %s SET REFLECTION (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_SETCRITIC: fprintf(out, "TEAM %s SET CRITIC (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_TEAM_SETPRUNE: fprintf(out, "TEAM %s SET PRUNE (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_TRAINTHINKING: fprintf(out, "TEAM TRAIN THINKING %s (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_TRAINACTORCRITIC: fprintf(out, "TEAM TRAIN ACTOR CRITIC %s (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_TEAM_DISTILLTHINKING: fprintf(out, "TEAM DISTILL THINKING %s (L%d)\n", s->win_name ? s->win_name : "", s->line); dump_expr(s->channel, out, indent + 2); break;
        case GMB_STMT_DOMAIN:
            fprintf(out, "%s %s (L%d)\n", s->win_name ? s->win_name : "", s->mutex_action ? s->mutex_action : "", s->line);
            for (int i = 0; i < s->n_go_args; i++) dump_expr(s->go_args[i], out, indent + 2);
            break;
        case GMB_STMT_ADD_HANDLER: fprintf(out, "ADD HANDLER (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_REMOVE_HANDLER: fprintf(out, "REMOVE HANDLER (L%d)\n", s->line); dump_expr(s->channel, out, indent + 2); dump_expr(s->send_value, out, indent + 2); break;
        case GMB_STMT_SELECT:
            fprintf(out, "SELECT (L%d)\n", s->line); dump_expr(s->cond, out, indent + 2);
            for (int i = 0; i < s->n_select_cases; i++) dump_stmt(s->select_cases[i], out, indent + 2);
            break;
        case GMB_STMT_CASE:
            fprintf(out, "CASE%s (L%d)\n", s->case_else ? " ELSE" : "", s->line);
            for (int i = 0; i < s->n_case_tests; i++) dump_expr(s->case_tests[i], out, indent + 2);
            for (int i = 0; i < s->n_case_body; i++) dump_stmt(s->case_body[i], out, indent + 4);
            break;
        default: fprintf(out, "STMT(%d)\n", s->kind); break;
    }
}

void gmb_ast_dump(gmb_stmt_t** stmts, int n, FILE* out)
{
    for (int i = 0; i < n; i++) dump_stmt(stmts[i], out, 0);
}
