/* gmb_parser_stmt_assign.c — parser assegnamento/chiamata (LET, CALL).
 * Estratto da gmb_parser_statements.c (2026-08-30 — split, file <= 1200 righe). */
#include "gmb_parser_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Costruisce `rd = rd ± 1` (ASSIGN), consumando `rd` (ownership trasferita). Supporta un
 * target VAR (variabile/campo nudo) e un CALL (accesso indicizzato: `a(i)`, `SELF.arr(i)`):
 * per il CALL gli indici sono DUPLICATI (clone) — la write li mette in array_index, la
 * read resta in a->value (`gmb_expr_free` libera array_index e value SEPARATAMENTE, quindi
 * condividerli sarebbe un doppio-free). Usata da `++`/`--` (postfix nel parser assign,
 * prefix nel dispatcher statement). */
gmb_stmt_t* gmb_p_make_incdec(gmb_expr_t* rd, int plus, int line)
{
    gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
    if (rd && rd->kind == GMB_EXPR_CALL) {
        a->target = strdup(rd->var ? rd->var : "?");
        if (rd->nargs >= 1) a->array_index = gmb_expr_clone(rd->args[0]);
        for (int k = 1; k < rd->nargs; k++) {
            a->array_idx_extra = (gmb_expr_t**)realloc(a->array_idx_extra, (size_t)(a->n_array_idx_extra + 1) * sizeof(gmb_expr_t*));
            a->array_idx_extra[a->n_array_idx_extra++] = gmb_expr_clone(rd->args[k]);
        }
    } else {
        a->target = strdup(rd && rd->kind == GMB_EXPR_VAR && rd->var ? rd->var : "?");
    }
    a->value = gmb_expr_binary(plus ? "+" : "-", rd, gmb_expr_num(1.0, line), line);
    return a;
}

gmb_stmt_t* gmb_p_parse_assign_or_call(gmb_parser_t* p, const char* target, int line)
{
    /* target++ / target-- (postfix) come STATEMENT → equivalente a `target = target ± 1`
     * (il valore di ritorno viene scartato in forma statement). */
    if (gmb_p_check(p, GMB_TOK_PlusPlus) || gmb_p_check(p, GMB_TOK_MinusMinus)) {
        int plus = gmb_p_check(p, GMB_TOK_PlusPlus);
        gmb_p_adv(p);
        gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
        a->target = strdup(target);
        a->value = gmb_expr_binary(plus ? "+" : "-",
                                   gmb_expr_var(strdup(target), line), gmb_expr_num(1.0, line), line);
        return a;
    }
    /* target '=' expr  oppure  CALL name(args)  oppure  expr stmt */
    if (gmb_p_match(p, GMB_TOK_Equal)) {
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ASSIGN, line);
        s->target = strdup(target);
        s->value = gmb_p_parse_expr(p);
        return s;
    }
    if (gmb_p_check(p, GMB_TOK_InlineAssign)) {
        /* x := expr come statement */
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_EXPR, line);
        gmb_expr_t* rhs = gmb_p_parse_expr(p);
        gmb_expr_t** args = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
        args[0] = gmb_expr_var(strdup(target), line); args[1] = rhs;
        s->expr = gmb_expr_call("_assign_", args, 2, line);
        return s;
    }
    /* target con membro (r.Pattern = ...), elemento array (s(0) = ...) o indice a quadre (a[i] = ...) */
    if (gmb_p_check(p, GMB_TOK_Dot) || gmb_p_check(p, GMB_TOK_LeftParen) || gmb_p_check(p, GMB_TOK_LeftBracket)) {
        gmb_stmt_t* es = gmb_stmt_new(GMB_STMT_EXPR, line);
        gmb_expr_t* base = gmb_expr_var(strdup(target), line);
        /* ricostruisci l'accesso: nome.membro... oppure nome(expr) — poi '=' */
        if (gmb_p_match(p, GMB_TOK_LeftParen)) {
            gmb_expr_t** args = NULL; int na = 0, ca = 0;
            if (!gmb_p_check(p, GMB_TOK_RightParen)) {
                for (;;) {
                    if (na >= ca) { ca = ca ? ca * 2 : 4; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                    args[na++] = gmb_p_parse_expr(p);
                    if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                }
            }
            gmb_p_match(p, GMB_TOK_RightParen);
            es->expr = gmb_expr_call(strdup(target), args, na, line);
            gmb_expr_free(base);
            /* LET v() [AS tipo] = expr → dichiarazione array dinamico (parity PCodeVM) */
            if (na == 0 && (gmb_p_check(p, GMB_TOK_As) || gmb_p_tok_is_keyword(gmb_p_cur(p), "As"))) {
                const gmb_token_t* at = gmb_p_peek(p, 1);
                char tb[32]; size_t tl = 0;
                if (at && at->lexeme) { tl = (size_t)at->length < 31 ? (size_t)at->length : 31; memcpy(tb, at->lexeme, tl); }
                tb[tl] = 0;
                gmb_p_adv(p);
                if (at) gmb_p_adv(p);
                if (gmb_p_match(p, GMB_TOK_Equal)) {
                    gmb_stmt_t* d = gmb_stmt_new(GMB_STMT_DIM, line);
                    d->dim_var = strdup(target);
                    d->dim_type = strdup(tb);
                    d->dim_empty_parens = 1;   /* LET v() = ... è una dichiarazione di ARRAY */
                    d->dim_init = gmb_p_parse_expr(p);
                    gmb_expr_free(es->expr); free(es);
                    return d;
                }
            }
            if (na == 0 && gmb_p_check(p, GMB_TOK_Equal)) {
                gmb_p_adv(p);
                gmb_stmt_t* d = gmb_stmt_new(GMB_STMT_DIM, line);
                d->dim_var = strdup(target);
                d->dim_empty_parens = 1;   /* LET v() = ... è una dichiarazione di ARRAY
                                            * (es. `LET Trend() = Decomposed(0)` → la riga
                                            * di un array jagged): senza il flag reg_dim lo
                                            * trattava da SCALARE. */
                d->dim_init = gmb_p_parse_expr(p);
                gmb_expr_free(es->expr); free(es);
                return d;
            }
            if (gmb_p_match(p, GMB_TOK_Equal) && na >= 1) {
                /* array element assignment: v(i) = x / v(i, j) = x */
                gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                a->target = strdup(target);
                a->array_index = es->expr->args ? es->expr->args[0] : NULL;
                if (a->array_index) es->expr->args[0] = NULL;
                for (int k = 1; k < na; k++) {
                    a->array_idx_extra = (gmb_expr_t**)realloc(a->array_idx_extra, (size_t)(a->n_array_idx_extra + 1) * sizeof(gmb_expr_t*));
                    a->array_idx_extra[a->n_array_idx_extra++] = es->expr->args[k];
                    es->expr->args[k] = NULL;
                }
                a->value = gmb_p_parse_expr(p);
                gmb_expr_free(es->expr); free(es);
                return a;
            }
            /* array.membro: Elenco(1).Nome = ... / Elenco(1).Metodo(args) —
             * 2.7(d): PRESERVA gli indici (CALL con indici / ASSIGN con array_index) */
            if (gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1)) {
                gmb_p_adv(p);
                const gmb_token_t* m = gmb_p_cur(p);
                char nm[256];
                snprintf(nm, sizeof(nm), "%s.%s", target, m ? gmb_p_tok_strdup(m) : "?");
                if (m) gmb_p_adv(p);
                gmb_expr_t* icall = es->expr;   /* CALL(target, indici) */
                if (gmb_p_check(p, GMB_TOK_LeftParen)) {
                    gmb_p_adv(p);
                    int mna = icall->nargs;
                    gmb_expr_t** margs = mna > 0 ? (gmb_expr_t**)malloc((size_t)mna * sizeof(gmb_expr_t*)) : NULL;
                    for (int k = 0; k < mna; k++) margs[k] = icall->args[k];
                    free(icall->args); free(icall->var); free(icall);
                    if (!gmb_p_check(p, GMB_TOK_RightParen)) {
                        for (;;) {
                            margs = (gmb_expr_t**)realloc(margs, (size_t)(mna + 1) * sizeof(gmb_expr_t*));
                            margs[mna++] = gmb_p_parse_expr(p);
                            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                        }
                    }
                    gmb_p_match(p, GMB_TOK_RightParen);
                    es->expr = gmb_expr_call(strdup(nm), margs, mna, line);
                } else if (gmb_p_match(p, GMB_TOK_Equal)) {
                    gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                    a->target = strdup(nm);
                    a->array_index = icall->args ? icall->args[0] : NULL;
                    if (a->array_index) icall->args[0] = NULL;
                    for (int k = 1; k < icall->nargs; k++) {
                        a->array_idx_extra = (gmb_expr_t**)realloc(a->array_idx_extra, (size_t)(a->n_array_idx_extra + 1) * sizeof(gmb_expr_t*));
                        a->array_idx_extra[a->n_array_idx_extra++] = icall->args[k];
                        icall->args[k] = NULL;
                    }
                    a->value = gmb_p_parse_expr(p);
                    gmb_expr_free(icall); free(es);
                    return a;
                } else {
                    es->expr = gmb_expr_call(strdup(nm), icall->args, icall->nargs, line);
                    icall->args = NULL; icall->nargs = 0;
                    gmb_expr_free(icall);
                }
            }
        } else if (gmb_p_match(p, GMB_TOK_LeftBracket)) {
            /* v[e1, ..., en] = expr — indice "moderno" a quadre (LIST/DICT/array), stesso
             * ASSIGN con array_index della forma a(i) = expr. */
            gmb_expr_t** args = NULL; int na = 0, ca = 0;
            if (!gmb_p_check(p, GMB_TOK_RightBracket)) {
                for (;;) {
                    if (na >= ca) { ca = ca ? ca * 2 : 4; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                    args[na++] = gmb_p_parse_expr(p);
                    if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                }
            }
            gmb_p_match(p, GMB_TOK_RightBracket);
            es->expr = gmb_expr_call(strdup(target), args, na, line);
            gmb_expr_free(base);
            if (gmb_p_match(p, GMB_TOK_Equal) && na >= 1) {
                gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                a->target = strdup(target);
                a->array_index = es->expr->args ? es->expr->args[0] : NULL;
                if (a->array_index) es->expr->args[0] = NULL;
                for (int k = 1; k < na; k++) {
                    a->array_idx_extra = (gmb_expr_t**)realloc(a->array_idx_extra, (size_t)(a->n_array_idx_extra + 1) * sizeof(gmb_expr_t*));
                    a->array_idx_extra[a->n_array_idx_extra++] = es->expr->args[k];
                    es->expr->args[k] = NULL;
                }
                a->value = gmb_p_parse_expr(p);
                gmb_expr_free(es->expr); free(es);
                return a;
            }
            /* NB: qui NON si libera `es`. Il ramo `[..]` senza `=` (o con `a[] = x`)
             * prosegue nel tail comune, che restituisce `es` (o legge es->expr):
             * liberarlo qui restituiva all'AST uno statement LIBERATO → heap-use-after-free
             * in cls_scan_stmt (crash HttpRoutes.bas, ASan 2026-09-10) e doppio free su
             * `a[] = x`. Lo statement resta di proprietà dell'AST come in ogni altro ramo. */
        } else {
            gmb_p_adv(p);   /* '.' */
            char name[300];
            snprintf(name, sizeof(name), "%s", target);
            for (;;) {
                const gmb_token_t* m = gmb_p_cur(p);
                if (!m) break;
                char seg[128]; int l = m->length < 127 ? m->length : 127;
                memcpy(seg, m->lexeme, (size_t)l); seg[l] = 0;
                gmb_p_adv(p);
                char tmp[340];
                snprintf(tmp, sizeof(tmp), "%s.%s", name, seg);
                strcpy(name, tmp);
                if (gmb_p_match(p, GMB_TOK_LeftBracket)) {
                    /* `a.b[k] = v` (membro JSON/dict indicizzato a quadre): prima l'indice veniva SCARTATO (skip dei token) e lo
                     * statement diventava `a.b = v` — store sbagliato/perso. Se dopo `]` c'e' `=` (e non un altro `.`), si produce
                     * l'ASSIGN con array_index sul membro (stessa forma di `v.Member(i) = x`). Altrimenti comportamento invariato. */
                    int br_pos = p->pos;
                    gmb_expr_t* bidx = gmb_p_parse_expr(p);
                    if (bidx && gmb_p_check(p, GMB_TOK_RightBracket)) {
                        gmb_p_adv(p);   /* ']' */
                        if (!gmb_p_check(p, GMB_TOK_Dot) && gmb_p_check(p, GMB_TOK_Equal)) {
                            gmb_p_adv(p);   /* '=' */
                            gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                            a->target = strdup(name);
                            a->array_index = bidx;
                            a->value = gmb_p_parse_expr(p);
                            gmb_expr_free(base); free(es);
                            return a;
                        }
                        gmb_expr_free(bidx);
                    } else {
                        if (bidx) gmb_expr_free(bidx);
                        p->pos = br_pos;
                        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightBracket)) gmb_p_adv(p);
                        gmb_p_match(p, GMB_TOK_RightBracket);
                    }
                }
                if (!gmb_p_check(p, GMB_TOK_Dot)) break;
                gmb_p_adv(p);
            }
            es->expr = gmb_expr_var(strdup(name), line);
            gmb_expr_free(base);
            if (gmb_p_check(p, GMB_TOK_LeftParen)) {
                /* obj.metodo(args) come statement */
                gmb_p_adv(p);
                gmb_expr_t** cargs = NULL; int nc = 0, cc = 0;
                if (!gmb_p_check(p, GMB_TOK_RightParen)) {
                    for (;;) {
                        if (nc >= cc) { cc = cc ? cc * 2 : 4; cargs = (gmb_expr_t**)realloc(cargs, (size_t)cc * sizeof(gmb_expr_t*)); }
                        cargs[nc++] = gmb_p_parse_expr(p);
                        if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                        gmb_p_skip_eol(p);
                        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_LineNumber) gmb_p_adv(p);
                    }
                }
                gmb_p_match(p, GMB_TOK_RightParen);
                gmb_expr_free(es->expr);
                es->expr = gmb_expr_call(strdup(name), cargs, nc, line);
                /* v.Member(i).Member2 = x — elemento di un array-campo di UDT (es.
                 * SELF.arr(2).campo = 30): ASSIGN su "name.Member2" con array_index; il
                 * target con il campo + indice è gestito dall'emettitore. */
                if (nc >= 1 && gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1)
                    && (gmb_p_peek(p, 1)->kind == GMB_TOK_Identifier
                        || gmb_p_peek(p, 1)->kind >= GMB_TOK_Define) && gmb_p_peek(p, 2)
                    && gmb_p_peek(p, 2)->kind == GMB_TOK_Equal) {
                    gmb_p_adv(p);  /* '.' */
                    const gmb_token_t* m2t = gmb_p_cur(p);
                    char nm2[300];
                    snprintf(nm2, sizeof(nm2), "%s.%s", name, m2t ? gmb_p_tok_strdup(m2t) : "?");
                    if (m2t) gmb_p_adv(p);
                    gmb_p_adv(p);  /* '=' */
                    gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                    a->target = strdup(nm2);
                    a->array_index = es->expr->args[0]; es->expr->args[0] = NULL;
                    for (int k = 1; k < nc; k++) {
                        a->array_idx_extra = (gmb_expr_t**)realloc(a->array_idx_extra, (size_t)(a->n_array_idx_extra + 1) * sizeof(gmb_expr_t*));
                        a->array_idx_extra[a->n_array_idx_extra++] = es->expr->args[k];
                        es->expr->args[k] = NULL;
                    }
                    a->value = gmb_p_parse_expr(p);
                    gmb_expr_free(es->expr); free(es);
                    return a;
                }
                if (gmb_p_match(p, GMB_TOK_Equal) && nc >= 1)
                {
                    /* v.Member(i) = x → ASSIGN sul membro con array_index (elemento del
                     * campo-array/dict del record), specchio di v(i) = x per non-dotted. */
                    gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                    a->target = strdup(es->expr->var ? es->expr->var : name);
                    a->array_index = es->expr->args[0]; es->expr->args[0] = NULL;
                    for (int k = 1; k < nc; k++) {
                        a->array_idx_extra = (gmb_expr_t**)realloc(a->array_idx_extra, (size_t)(a->n_array_idx_extra + 1) * sizeof(gmb_expr_t*));
                        a->array_idx_extra[a->n_array_idx_extra++] = es->expr->args[k];
                        es->expr->args[k] = NULL;
                    }
                    a->value = gmb_p_parse_expr(p);
                    gmb_expr_free(es->expr); free(es);
                    return a;
                }
            }
        }
        if (gmb_p_match(p, GMB_TOK_LeftBracket)) {
            /* a[expr] = value — indice a quadre come target */
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightBracket)) gmb_p_adv(p);
            gmb_p_match(p, GMB_TOK_RightBracket);
            while (gmb_p_check(p, GMB_TOK_LeftBracket)) {
                gmb_p_adv(p);
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightBracket)) gmb_p_adv(p);
                gmb_p_match(p, GMB_TOK_RightBracket);
            }
            if (gmb_p_match(p, GMB_TOK_Equal)) {
                gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
                char buf[300]; snprintf(buf, sizeof(buf), "%s[]", target);
                a->target = strdup(buf);
                a->value = gmb_p_parse_expr(p);
                gmb_expr_free(es->expr); free(es);
                return a;
            }
        }
        /* t++ / t-- (postfix) su una forma GIÀ costruita (`SELF.x`, `a(0)`, `v.Membro(i)`):
         * t = t ± 1 (valore di ritorno scartato in forma statement). */
        if (gmb_p_check(p, GMB_TOK_PlusPlus) || gmb_p_check(p, GMB_TOK_MinusMinus)) {
            int plus = gmb_p_check(p, GMB_TOK_PlusPlus);
            gmb_p_adv(p);
            gmb_expr_t* rd = es->expr; es->expr = NULL;
            gmb_stmt_t* a = gmb_p_make_incdec(rd, plus, line);
            free(es);
            return a;
        }
        if (gmb_p_match(p, GMB_TOK_Equal)) {
            gmb_stmt_t* a = gmb_stmt_new(GMB_STMT_ASSIGN, line);
            a->target = strdup(es->expr->var ? es->expr->var : es->expr->str ? es->expr->str : "?");
            a->value = gmb_p_parse_expr(p);
            gmb_expr_free(es->expr); free(es);
            return a;
        }
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)
            && !gmb_p_check(p, GMB_TOK_RightBrace) && !gmb_p_at_end(p)) {
            /* comando qualificato senza parentesi: IO.RMDIR dir$, TRUE — consuma gli argomenti.
             * La RightBrace termina uno statement anche senza newline: dentro un corpo a
             * blocco (SUB(x){...}/codeblock Clipper) la '}' NON deve essere consumata qui,
             * altrimenti il blocco resta aperto → "blocco non terminato" all'END SUB. */
            gmb_p_consume_statement(p);
        }
        return es;
    }
    /* espressione come statement (es. chiamata funzione) */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_EXPR, line);
    s->expr = gmb_expr_var(target, line);
    if (gmb_p_check(p, GMB_TOK_LeftParen)) {
        /* trasformalo in CALL */
        gmb_p_adv(p);
        gmb_expr_t** args = NULL; int na = 0, ca = 0;
        if (!gmb_p_check(p, GMB_TOK_RightParen)) {
            for (;;) {
                if (na >= ca) { ca = ca ? ca * 2 : 4; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                args[na++] = gmb_p_parse_expr(p);
                if (!gmb_p_match(p, GMB_TOK_Comma)) break;
            }
        }
        if (!gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_set_err(p, "attesa ')' in chiamata", line);
        gmb_expr_free(s->expr);
        s->expr = gmb_expr_call(target, args, na, line);
    }
    return s;
}
