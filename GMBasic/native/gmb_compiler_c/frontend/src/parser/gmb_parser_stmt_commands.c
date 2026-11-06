/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_commands.c — speculare a
 * Parser.Stmt.Commands.cs: REDIM, SWAP, STOP, RUN, REQUIRE, YIELD. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

static char* read_name(gmb_parser_t* p)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return NULL;
    char* s = (char*)malloc((size_t)t->length + 1);
    memcpy(s, t->lexeme, (size_t)t->length); s[t->length] = 0;
    gmb_p_adv(p);
    return s;
}

/* USING <var> = <expr> ... END USING — la risorsa è legata a var (deve essere un'istanza di
 * classe con DESTRUCTOR per avere effetto; per le risorse file OPEN è no-op come in PCodeVM
 * headless). Rappresentazione: target=var, value=expr, try_body/n_try=corpo del blocco. */
gmb_stmt_t* gmb_p_parse_using(gmb_parser_t* p, int line, bool* multiline)
{
    (void)multiline;
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_USING, line);
    const gmb_token_t* tv = gmb_p_cur(p);
    if (tv && tv->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) { s->target = gmb_p_tok_strdup(tv); gmb_p_adv(p); }
    gmb_p_match(p, GMB_TOK_Equal);
    if (gmb_p_check(p, GMB_TOK_Open)) {
        /* `USING F = OPEN(path, MODE)` — forma FUNZIONE di OPEN (path, MODE tra parentesi).
         * La modalità è un TOKEN (OUTPUT/INPUT/APPEND/...). Salvo path e mode negli stessi campi
         * dello statement OPEN (file_path_expr/file_mode_tk), così st_using emette l'apertura del
         * file legando il file number a F e la chiusura a END USING. Senza, `OPEN(...)` veniva
         * valutato come CALL "OPEN" → risolto a CONVERT.OPEN → "simbolo non trovato". */
        gmb_p_adv(p); /* OPEN */
        if (gmb_p_match(p, GMB_TOK_LeftParen)) {
            if (!gmb_p_check(p, GMB_TOK_RightParen) && !gmb_p_at_end(p))
                s->file_path_expr = gmb_p_parse_expr(p);
            if (gmb_p_match(p, GMB_TOK_Comma)) {
                const gmb_token_t* m = gmb_p_cur(p);
                if (m && m->kind != GMB_TOK_RightParen && m->kind != GMB_TOK_Newline) {
                    s->file_mode_tk = (int)m->kind;
                    gmb_p_adv(p);
                }
            }
            gmb_p_match(p, GMB_TOK_RightParen);
        }
    } else if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
        s->value = gmb_p_parse_expr(p);
    }
    gmb_stmt_list_t body = gmb_p_stmt_list_new();
    gmb_p_parse_block(p, &body, GMB_TOK_EndUsing, "EndUsing", line);
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_EndUsing
        || gmb_p_tok_is_keyword(gmb_p_cur(p), "EndUsing"))) gmb_p_adv(p);
    else if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_End && gmb_p_peek(p, 1)
        && gmb_p_peek(p, 1)->kind == GMB_TOK_Using) { gmb_p_adv(p); gmb_p_adv(p); }
    s->try_body = body.items; s->n_try = body.n;
    return s;
}

/* REDIM [PRESERVE] <var>(<dims...>) */
gmb_stmt_t* gmb_p_parse_redim(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_REDIM, line);
    /* PRESERVE (flag=1): conserva i dati; senza PRESERVE il REDIM è distruttivo */
    if (gmb_p_check(p, GMB_TOK_Preserve)) { s->flag = 1; gmb_p_adv(p); }
    s->dim_var = read_name(p);
    /* membro di record/UDT: REDIM a.b(i) → nome puntato completo "a.b" (prima si leggeva
     * solo "a" e il resto era scartato come "membri", colpendo l'INTERO record). */
    while (gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1)
           && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof) {
        gmb_p_adv(p);
        const gmb_token_t* seg = gmb_p_cur(p);
        if (!seg || seg->kind == GMB_TOK_Newline || seg->kind == GMB_TOK_Eof) break;
        size_t olen = strlen(s->dim_var);
        size_t slen = (size_t)seg->length;
        s->dim_var = (char*)realloc(s->dim_var, olen + slen + 2);
        s->dim_var[olen] = '.';
        memcpy(s->dim_var + olen + 1, seg->lexeme, slen);
        s->dim_var[olen + 1 + slen] = 0;
        gmb_p_adv(p);
    }
    if (gmb_p_check(p, GMB_TOK_LeftParen)) {
        gmb_p_adv(p);
        if (!gmb_p_check(p, GMB_TOK_RightParen)) s->array_size = gmb_p_parse_expr(p);
        while (gmb_p_match(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_RightParen) && !gmb_p_at_end(p)) {
            gmb_expr_t* d = gmb_p_parse_expr(p);
            gmb_expr_t* d_lo = NULL;
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "To") || gmb_p_check(p, GMB_TOK_To)) { gmb_p_adv(p); d_lo = d; d = gmb_p_parse_expr(p); }
            s->dim_extra = (gmb_expr_t**)realloc(s->dim_extra, (size_t)(s->n_dim_extra + 1) * sizeof(gmb_expr_t*));
            s->dim_extra_lo = (gmb_expr_t**)realloc(s->dim_extra_lo, (size_t)(s->n_dim_extra + 1) * sizeof(gmb_expr_t*));
            s->dim_extra[s->n_dim_extra] = d;
            s->dim_extra_lo[s->n_dim_extra] = d_lo;
            s->n_dim_extra++;
        }
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);
        gmb_p_match(p, GMB_TOK_RightParen);
    }
    /* inizializzatore: REDIM v[(N)] = <expr> (parity oracolo: letterale {..}) */
    if (gmb_p_match(p, GMB_TOK_Equal)) {
        s->dim_init = gmb_p_parse_expr(p);
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        return s;
    }
    /* membri (REDIM a.b(i)) — consuma fino a fine riga */
    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) {
        gmb_p_adv(p);
    }
    return s;
}

/* ERASE <var> — libera l'array (FASE 1: una variabile; il resto della riga è consumato) */
gmb_stmt_t* gmb_p_parse_erase(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ERASE, line);
    s->dim_var = read_name(p);
    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) {
        gmb_p_adv(p);
    }
    return s;
}

/* SWAP <a>, <b> */
gmb_stmt_t* gmb_p_parse_swap(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SWAP, line);
    s->swap_a = read_name(p);
    if (gmb_p_match(p, GMB_TOK_Comma)) s->swap_b = read_name(p);
    return s;
}

/* STOP */
gmb_stmt_t* gmb_p_parse_stop(gmb_parser_t* p, int line)
{
    return gmb_stmt_new(GMB_STMT_STOP, line);
}

/* RUN [<file>] */
gmb_stmt_t* gmb_p_parse_run(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_RUN, line);
    const gmb_token_t* t = gmb_p_cur(p);
    if (t && (t->kind == GMB_TOK_StringLiteral || t->kind == GMB_TOK_Identifier)) {
        /* RUN <expr>: se dopo il primo token c'è un operatore (es. `RUN "a" + x$`),
         * l'argomento è un'ESPRESSIONE stringa → si parsa l'intera espressione (prima
         * si prendeva solo il primo token e il resto dava "statement non riconosciuto"). */
        const gmb_token_t* nx = gmb_p_peek(p, 1);
        int is_op = nx && (nx->kind == GMB_TOK_Plus || nx->kind == GMB_TOK_Minus
                        || nx->kind == GMB_TOK_Multiply || nx->kind == GMB_TOK_Divide
                        || nx->kind == GMB_TOK_Mod || nx->kind == GMB_TOK_Ampersand
                        || nx->kind == GMB_TOK_Equal);
        if (is_op) {
            s->run_expr = gmb_p_parse_expr(p);
        } else {
            s->run_file = (char*)malloc((size_t)t->length + 1);
            memcpy(s->run_file, t->lexeme, (size_t)t->length); s->run_file[t->length] = 0;
            gmb_p_adv(p);
        }
    }
    return s;
}

/* REQUIRE "package" [@ver] */
gmb_stmt_t* gmb_p_parse_require(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_REQUIRE, line);
    const gmb_token_t* t = gmb_p_cur(p);
    if (t && t->kind == GMB_TOK_StringLiteral) {
        s->require_pkg = (char*)malloc((size_t)t->length + 1);
        memcpy(s->require_pkg, t->lexeme, (size_t)t->length); s->require_pkg[t->length] = 0;
        gmb_p_adv(p);
        /* eventuale @versione */
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_AtSign) gmb_p_adv(p);
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
    }
    return s;
}

/* YIELD */
gmb_stmt_t* gmb_p_parse_yield(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_YIELD, line);
    if (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) s->value = gmb_p_parse_expr(p);
    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
    return s;
}

/* PROCESS <file> [PROMPT [:]<istruzioni>] — E3: documento → agente "doc" con prompt.
 * Statement-espressione sul builtin PROCESS (func_code 1227), speculare a
 * Parser.Statements.cs ParseProcessStatement (C#) e a C99CodeEmitter (NsKindRow
 * TokenKind.Process): la semantica vive nel runtime C condiviso
 * (gmb_process_document, gmb_ai.c) — qui si costruisce solo la chiamata (§0bis). */
gmb_stmt_t* gmb_p_parse_process(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_EXPR, line);
    gmb_expr_t* args[2];
    int n = 0;
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_at_end(p))
        args[n++] = gmb_p_parse_expr(p);
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Prompt
        || gmb_p_tok_is_keyword(gmb_p_cur(p), "Prompt"))) {
        gmb_p_adv(p);
        if (gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);   /* ':' opzionale dopo PROMPT */
        if (n < 2 && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p))
            args[n++] = gmb_p_parse_expr(p);
    }
    gmb_expr_t** call_args = NULL;
    if (n > 0) {
        call_args = (gmb_expr_t**)malloc((size_t)n * sizeof(gmb_expr_t*));
        for (int i = 0; i < n; i++) call_args[i] = args[i];
    }
    s->expr = gmb_expr_call_typed(strdup("PROCESS"), GMB_TOK_Process, call_args, n, line);
    while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) gmb_p_adv(p);
    return s;
}
