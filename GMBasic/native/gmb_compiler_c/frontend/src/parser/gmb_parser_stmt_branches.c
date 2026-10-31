/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_branches.c — speculare a
 * Parser.Stmt.Branches.cs: IF/THEN/ELSE/END IF e SELECT CASE. */
#include "gmb_parser_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const gmb_tokenkind_t gmb_tk_nx2_end_toks[] = { GMB_TOK_Newline, GMB_TOK_Colon, GMB_TOK_Eof };
static int gmb_tk_nx2_end_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_tk_nx2_end_toks) / sizeof(gmb_tk_nx2_end_toks[0]); i++)
        if (gmb_tk_nx2_end_toks[i] == k) return 1;
    return 0;
}

/* IF <cond> THEN ... [ELSE ...] END IF (multiline) o IF <cond> THEN <stmt> [ELSE <stmt>] */
gmb_stmt_t* gmb_p_parse_if(gmb_parser_t* p, int line, bool* multiline)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_IF, line);
    s->cond = gmb_p_parse_expr(p);
    int had_then = 0;
    if (!gmb_p_tok_is_keyword(gmb_p_cur(p), "Then") && !gmb_p_check(p, GMB_TOK_Then)) {
        /* IF cond GOTO label / GOSUB ... (senza THEN) — forma Clipper */
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Goto || gmb_p_cur(p)->kind == GMB_TOK_Gosub)) {
            /* inline a statement singolo: il resto della riga dopo ':' è fuori dall'IF */
        } else { gmb_p_set_err(p, "atteso THEN", line); return s; }
    } else {
        gmb_p_adv(p);
        had_then = 1;
    }
    gmb_stmt_list_t then_list = gmb_p_stmt_list_new();
    if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_at_end(p)) {
        *multiline = true;
        gmb_p_parse_block(p, &then_list, GMB_TOK_Else, "Else", line);
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_ElseIf || gmb_p_tok_is_keyword(gmb_p_cur(p), "ElseIf"))) {
            /* ELSEIF: chain come IF annidato nel ramo else (consuma fino a ELSE/END IF) */
            gmb_stmt_list_t else_list = gmb_p_stmt_list_new();
            for (;;) {
                if (!(gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_ElseIf || gmb_p_tok_is_keyword(gmb_p_cur(p), "ElseIf")))) break;
                gmb_p_adv(p);   /* ELSEIF */
                bool mlx;
                gmb_stmt_t* nested = gmb_p_parse_if(p, p->pos > 0 ? p->toks[p->pos - 1].line : line, &mlx);
                gmb_p_stmt_list_add(&else_list, nested);
            }
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Else || gmb_p_tok_is_keyword(gmb_p_cur(p), "Else"))) {
                gmb_p_adv(p);
                gmb_stmt_list_t more = gmb_p_stmt_list_new();
                gmb_p_parse_block(p, &more, GMB_TOK_EndIf, "EndIf", line);
                for (int i = 0; i < more.n; i++) gmb_p_stmt_list_add(&else_list, more.items[i]);
                free(more.items);
            }
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_EndIf || gmb_p_tok_is_keyword(gmb_p_cur(p), "EndIf"))) gmb_p_adv(p);
            else if (gmb_p_at_end_compound(p, GMB_TOK_If)) { gmb_p_adv(p); gmb_p_adv(p); }
            s->else_body = else_list.items; s->n_else = else_list.n;
        } else if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Else || gmb_p_tok_is_keyword(gmb_p_cur(p), "Else"))) {
            gmb_p_adv(p);
            gmb_stmt_list_t else_list = gmb_p_stmt_list_new();
            gmb_p_parse_block(p, &else_list, GMB_TOK_EndIf, "EndIf", line);
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_EndIf || gmb_p_tok_is_keyword(gmb_p_cur(p), "EndIf"))) gmb_p_adv(p);
            else if (gmb_p_at_end_compound(p, GMB_TOK_If)) { gmb_p_adv(p); gmb_p_adv(p); }
            s->else_body = else_list.items; s->n_else = else_list.n;
        } else {
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_EndIf || gmb_p_tok_is_keyword(gmb_p_cur(p), "EndIf"))) gmb_p_adv(p);
            else if (gmb_p_at_end_compound(p, GMB_TOK_If)) { gmb_p_adv(p); gmb_p_adv(p); }
        }
    } else if (had_then) {
        bool ml2 = false;
        /* ramo THEN inline: statement separati da ':' finché ELSE / END IF / fine riga
         * (tutta la sequenza appartiene al ramo — verificato su oracolo PCodeVM) */
        for (;;) {
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Else || gmb_p_tok_is_keyword(gmb_p_cur(p), "Else"))) break;
            /* THEN <numero> = GOTO implicito a riga (parity PCodeVM) */
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Number) {
                gmb_stmt_t* g = gmb_stmt_new(GMB_STMT_GOTO, line);
                g->target_label = gmb_p_tok_strdup(gmb_p_cur(p));
                gmb_p_adv(p);
                gmb_p_stmt_list_add(&then_list, g);
                continue;   /* eventuale ': ELSE' seguente ancora gestito dal loop */
            }
            /* THEN <identificatore nudo> = GOTO implicito a etichetta */
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Identifier) {
                const gmb_token_t* nx2 = gmb_p_peek(p, 1);
                if (!nx2 || gmb_tk_nx2_end_in(nx2->kind) || gmb_p_tok_is_keyword(nx2, "Else")) {
                    gmb_stmt_t* g = gmb_stmt_new(GMB_STMT_GOTO, line);
                    g->target_label = gmb_p_tok_strdup(gmb_p_cur(p));
                    gmb_p_adv(p);
                    gmb_p_stmt_list_add(&then_list, g);
                    continue;
                }
            }
            gmb_stmt_t* inline_stmt = gmb_p_parse_stmt(p, &ml2);
            if (inline_stmt) gmb_p_stmt_list_add(&then_list, inline_stmt);
            if (gmb_p_check(p, GMB_TOK_Colon)) {
                gmb_p_adv(p);
                if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Else || gmb_p_tok_is_keyword(gmb_p_cur(p), "Else"))) break;
                continue;
            }
            break;
        }
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Else || gmb_p_tok_is_keyword(gmb_p_cur(p), "Else"))) {
            gmb_p_adv(p);
            if (gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
            gmb_stmt_list_t else_list = gmb_p_stmt_list_new();
            for (;;) {
                /* ELSE <numero> = GOTO implicito a riga (parity PCodeVM) */
                if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Number) {
                    gmb_stmt_t* g = gmb_stmt_new(GMB_STMT_GOTO, line);
                    g->target_label = gmb_p_tok_strdup(gmb_p_cur(p));
                    gmb_p_adv(p);
                    gmb_p_stmt_list_add(&else_list, g);
                    break;
                }
                /* ELSE <identificatore nudo> = GOTO implicito a etichetta */
                if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Identifier) {
                    const gmb_token_t* nx2 = gmb_p_peek(p, 1);
                    if (!nx2 || gmb_tk_nx2_end_in(nx2->kind) || gmb_p_tok_is_keyword(nx2, "Else")) {
                        gmb_stmt_t* g = gmb_stmt_new(GMB_STMT_GOTO, line);
                        g->target_label = gmb_p_tok_strdup(gmb_p_cur(p));
                        gmb_p_adv(p);
                        gmb_p_stmt_list_add(&else_list, g);
                        break;
                    }
                }
                gmb_stmt_t* el = gmb_p_parse_stmt(p, &ml2);
                if (el) gmb_p_stmt_list_add(&else_list, el);
                if (gmb_p_check(p, GMB_TOK_Colon)) { gmb_p_adv(p); continue; }
                break;
            }
            s->else_body = else_list.items; s->n_else = else_list.n;
        }
        /* END IF opzionale sulla stessa riga (inline IF con terminatore esplicito) */
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_EndIf || gmb_p_tok_is_keyword(gmb_p_cur(p), "EndIf"))) gmb_p_adv(p);
        else if (gmb_p_at_end_compound(p, GMB_TOK_If)) { gmb_p_adv(p); gmb_p_adv(p); }
    } else {
        bool ml2 = false;
        /* forma Clipper senza THEN (parity PCodeVM): SOLO il primo statement è
         * condizionato; il resto della riga dopo ':' è incondizionato (in coda) */
        gmb_stmt_t* first = gmb_p_parse_stmt(p, &ml2);
        if (first) gmb_p_stmt_list_add(&then_list, first);
        for (;;) {
            if (gmb_p_at_end(p)) break;
            const gmb_token_t* ct = gmb_p_cur(p);
            if (ct->kind == GMB_TOK_Newline || ct->kind == GMB_TOK_Eof) break;
            if (gmb_p_check(p, GMB_TOK_Colon)) { gmb_p_adv(p); continue; }
            bool mlx = false;
            gmb_stmt_t* extra = gmb_p_parse_stmt(p, &mlx);
            if (extra) gmb_p_queue_stmt(p, extra);
            if (!gmb_p_check(p, GMB_TOK_Colon)) break;
        }
    }
    s->then_body = then_list.items; s->n_then = then_list.n;
    return s;
}

/* SELECT CASE <expr> ... CASE <tests> ... CASE ELSE ... END SELECT */
gmb_stmt_t* gmb_p_parse_select_case(gmb_parser_t* p, int line, bool* multiline)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SELECT, line);
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Case || gmb_p_tok_is_keyword(gmb_p_cur(p), "Case"))) gmb_p_adv(p);
    s->cond = gmb_p_parse_expr(p);
    *multiline = true;
    int cap_cases = 0;
    for (;;) {
        gmb_p_skip_eol(p);
        if (gmb_p_at_end(p)) break;
        const gmb_token_t* t = gmb_p_cur(p);
        if (t->kind == GMB_TOK_End) {
            gmb_p_adv(p);
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Select || gmb_p_tok_is_keyword(gmb_p_cur(p), "Select"))) gmb_p_adv(p);
            break;
        }
        if (t->kind == GMB_TOK_Case || gmb_p_tok_is_keyword(t, "Case")) {
            gmb_p_adv(p);
            gmb_stmt_t* c = gmb_stmt_new(GMB_STMT_CASE, line);
            int cap_tests = 0;
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Else")) {
                gmb_p_adv(p);
                c->case_else = true;
            } else {
                for (;;) {
                    if (c->n_case_tests >= cap_tests) {
                        cap_tests = cap_tests ? cap_tests * 2 : 4;
                        c->case_tests = (gmb_expr_t**)realloc(c->case_tests, (size_t)cap_tests * sizeof(gmb_expr_t*));
                        c->case_hi = (gmb_expr_t**)realloc(c->case_hi, (size_t)cap_tests * sizeof(gmb_expr_t*));
                        c->case_cmp = (char**)realloc(c->case_cmp, (size_t)cap_tests * sizeof(char*));
                    }
                    const char* is_op = NULL;
                    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Is) {
                        /* CASE IS <op> <val> — comparazione esplicita (=, <>, >, <, >=, <=).
                         * Prima di questo fix `IS > 20` veniva ingerito da gmb_p_parse_expr
                         * come espressione qualunque (case_cmp restava sempre NULL, mai
                         * popolato) e il test cadeva sempre in CASE ELSE — vedi AiWiki
                         * "(open) - C99Native-Select-Case-IS-Comparison.md". */
                        gmb_p_adv(p);
                        const gmb_token_t* ot = gmb_p_cur(p);
                        if (ot) {
                            switch (ot->kind) {
                                case GMB_TOK_GreaterThan: is_op = ">"; break;
                                case GMB_TOK_GreaterEqual: is_op = ">="; break;
                                case GMB_TOK_LessThan: is_op = "<"; break;
                                case GMB_TOK_LessEqual: is_op = "<="; break;
                                case GMB_TOK_NotEqual: is_op = "<>"; break;
                                case GMB_TOK_Equal: is_op = "="; break;
                                default: break;
                            }
                            if (is_op) gmb_p_adv(p);
                        }
                    }
                    c->case_tests[c->n_case_tests] = gmb_p_parse_expr(p);
                    c->case_hi[c->n_case_tests] = NULL;
                    c->case_cmp[c->n_case_tests] = is_op ? strdup(is_op) : NULL;
                    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_To || gmb_p_tok_is_keyword(gmb_p_cur(p), "To"))) {
                        /* CASE 5 TO 10 — range: preserva il limite superiore */
                        gmb_p_adv(p);
                        c->case_hi[c->n_case_tests] = gmb_p_parse_expr(p);
                    }
                    c->n_case_tests++;
                    if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                }
            }
            /* Clipper SWITCH: CASE <val> => body (senza END SELECT) — estensione nativa
             * intenzionale, non tocca la regola sotto. */
            bool clipper_arrow = gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_FatArrow || gmb_p_cur(p)->kind == GMB_TOK_Arrow);
            if (clipper_arrow) gmb_p_adv(p);
            /* Parità col parser canonico (common/Parser/Parser.Stmt.Branches.cs:502,
             * "Expected newline after CASE conditions."): il body di un CASE standard è
             * SEMPRE block-form, mai su riga singola con ':' dopo la condizione — IL/PCode
             * rifiutano `CASE IS > 20: r = "gt"` con un errore di parse dedicato. Prima di
             * questo fix il C99Native accettava silenziosamente quella sintassi (con
             * risultato CORRETTO, ma divergente: un programma valido su C99Native falliva
             * la compilazione su IL/PCode) — vedi AiWiki
             * "(open) - C99Native-Select-Case-IS-Comparison.md", richiesta utente di
             * coerenza multi-backend. Non tocca `CASE ... => body` (Clipper, sopra).*/
            if (!clipper_arrow && gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Colon) {
                gmb_p_set_err(p, c->case_else ? "Expected newline after 'CASE ELSE'." : "Expected newline after CASE conditions.", gmb_p_cur(p)->line);
                return s;
            }
            gmb_stmt_list_t body = gmb_p_stmt_list_new();
            gmb_p_parse_block(p, &body, GMB_TOK_Case, "Case", line);
            c->case_body = body.items; c->n_case_body = body.n;
            if (s->n_select_cases >= cap_cases) { cap_cases = cap_cases ? cap_cases * 2 : 4; s->select_cases = (gmb_stmt_t**)realloc(s->select_cases, (size_t)cap_cases * sizeof(gmb_stmt_t*)); }
            s->select_cases[s->n_select_cases++] = c;
            continue;
        }
        gmb_p_adv(p);
    }
    return s;
}
