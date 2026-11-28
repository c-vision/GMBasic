/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_loops.c — speculare a Parser.Stmt.Loops.cs:
 * FOR/TO/STEP/NEXT, WHILE/WEND, DO/LOOP/UNTIL. Ogni costrutto è una funzione che
 * consuma i token e ritorna lo statement (chiamata dal dispatcher in parse_stmt). */
#include "gmb_parser_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* extract_name(gmb_parser_t* p)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return NULL;
    char* name = (char*)malloc((size_t)t->length + 1);
    memcpy(name, t->lexeme, (size_t)t->length); name[t->length] = 0;
    return name;
}

/* FOR <var> = <from> TO <to> [STEP <step>] ... NEXT */
/* body inline: statement separati da ':' fino a <stop> (es. FOR ... : a=1 : b=2 : NEXT) */
static void inline_body(gmb_parser_t* p, gmb_stmt_list_t* out, gmb_tokenkind_t stop_kw, const char* stop_name)
{
    for (;;) {
        if (gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        gmb_p_skip_eol(p);
        if (gmb_p_at_end(p)) break;
        if (stop_kw == GMB_TOK_Next && p->pending_for_ends > 0) { p->pending_for_ends--; break; }
        const gmb_token_t* tt = gmb_p_cur(p);
        if (tt->kind == stop_kw || (stop_name && gmb_p_tok_is_keyword(tt, stop_name))) break;
        bool ml;
        gmb_stmt_t* st = gmb_p_parse_stmt(p, &ml);
        if (st) gmb_p_stmt_list_add(out, st);
        gmb_p_drain_queued(p, out);
        if (p->error) break;
        if (!ml && !gmb_p_check(p, GMB_TOK_Colon)) {
            gmb_p_skip_eol(p);
            if (gmb_p_at_end(p)) break;
            const gmb_token_t* tt2 = gmb_p_cur(p);
            if (tt2->kind == stop_kw || (stop_name && gmb_p_tok_is_keyword(tt2, stop_name))) break;
        }
    }
}

gmb_stmt_t* gmb_p_parse_for(gmb_parser_t* p, int line, bool* multiline)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_FOR, line);
    /* FOR EACH var [AS T] IN coll ... NEXT */
    if (gmb_p_check(p, GMB_TOK_Each)) {
        gmb_p_adv(p);
        /* FOR EACH KEY var IN coll — marker solo per la parola esatta 'key' (length 3)
         * SEGUITA da un nome (FOR EACH KEY country): 'key$' o 'key,' sono la variabile. */
        {
            const gmb_token_t* ktok = gmb_p_cur(p);
            const gmb_token_t* knext = gmb_p_peek(p, 1);
            if (gmb_p_check(p, GMB_TOK_Key) && ktok && ktok->length == 3
                && knext && knext->kind != GMB_TOK_Comma && knext->kind != GMB_TOK_In
                && !gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "In")
                && knext->kind != GMB_TOK_Newline) {
                gmb_p_adv(p);
                /* sentinella: iterazione SOLO chiavi su dict (for_to == "_keys_") */
                s->for_to = gmb_expr_var(strdup("_keys_"), line);
            }
        }
        /* FOR EACH VALUE v IN coll — solo valori (sentinella "_values_") */
        {
            const gmb_token_t* vtok = gmb_p_cur(p);
            const gmb_token_t* vnext = gmb_p_peek(p, 1);
            if (vnext && vnext->kind != GMB_TOK_Comma && vnext->kind != GMB_TOK_In
                && !gmb_p_tok_is_keyword(vnext, "In") && vnext->kind != GMB_TOK_Newline
                && (vtok && (gmb_p_check(p, GMB_TOK_Value) || gmb_p_tok_is_keyword(vtok, "Value")))) {
                gmb_p_adv(p);
                s->for_to = gmb_expr_var(strdup("_values_"), line);
            }
        }
        s->for_var = extract_name(p);
        gmb_p_adv(p);
        /* FOR EACH k, v IN dict — coppie chiave/valore: conserva il secondo nome in
         * for_to (expr VAR, campo non usato in modalità EACH) per l'emissione. */
        if (gmb_p_check(p, GMB_TOK_Comma)) {
            /* FOR EACH a, b [, c] IN coll — fino a 3 variabili (for_to = 2°, for_step = 3°) */
            for (int extra = 0; extra < 2 && gmb_p_check(p, GMB_TOK_Comma); extra++) {
                gmb_p_adv(p);
                const gmb_token_t* t2 = gmb_p_cur(p);
                if (!t2 || !t2->lexeme || gmb_p_tok_is_keyword(gmb_p_cur(p), "In")
                    || gmb_p_check(p, GMB_TOK_In) || gmb_p_check(p, GMB_TOK_Newline))
                    break;
                if (extra == 0) s->for_to = gmb_expr_var(gmb_p_tok_strdup(t2), t2->line);
                else s->for_step = gmb_expr_var(gmb_p_tok_strdup(t2), t2->line);
                gmb_p_adv(p);
            }
            while (gmb_p_match(p, GMB_TOK_Comma)) {
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "In") || gmb_p_check(p, GMB_TOK_In)) break;
                gmb_p_adv(p);
            }
        }
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
            gmb_p_adv(p);
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_In) && !gmb_p_tok_is_keyword(gmb_p_cur(p), "In") && !gmb_p_check(p, GMB_TOK_Newline)) gmb_p_adv(p);
        }
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "In") || gmb_p_check(p, GMB_TOK_In)) gmb_p_adv(p);
        s->for_from = gmb_p_parse_expr(p);   /* la collection */
        s->flag = 1;                         /* FOR EACH */
        *multiline = true;
        gmb_stmt_list_t body = gmb_p_stmt_list_new();
        gmb_p_parse_block(p, &body, GMB_TOK_Next, "Next", line);
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Next || gmb_p_tok_is_keyword(gmb_p_cur(p), "Next"))) {
            gmb_p_adv(p);
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        }
        s->for_body = body.items; s->n_for = body.n;
        s->for_next_bas = p->bas_fresh ? p->last_bas_line : 0;   /* 2.8: riga del NEXT */
        return s;
    }
    s->for_var = extract_name(p);
    gmb_p_adv(p);
    if (!gmb_p_match(p, GMB_TOK_Equal)) { gmb_p_set_err(p, "atteso = in FOR", line); return s; }
    s->for_from = gmb_p_parse_expr(p);
    if (!gmb_p_tok_is_keyword(gmb_p_cur(p), "To") && !gmb_p_check(p, GMB_TOK_To)) { gmb_p_set_err(p, "atteso TO", line); return s; }
    gmb_p_adv(p);
    s->for_to = gmb_p_parse_expr(p);
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Step") || gmb_p_check(p, GMB_TOK_Step)) { gmb_p_adv(p); s->for_step = gmb_p_parse_expr(p); }
    gmb_stmt_list_t body = gmb_p_stmt_list_new();
    if (gmb_p_check(p, GMB_TOK_Colon)) {
        /* body inline sulla stessa riga: a=1 : b=2 : NEXT */
        inline_body(p, &body, GMB_TOK_Next, "Next");
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Next || gmb_p_tok_is_keyword(gmb_p_cur(p), "Next"))) {
            gmb_p_adv(p);
            int extras = 0;
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) {
                if (gmb_p_check(p, GMB_TOK_Comma)) extras++;   /* NEXT Y, X — chiude anche il FOR esterno */
                gmb_p_adv(p);
            }
            if (extras > 0) p->pending_for_ends += extras;
        }
    } else {
        *multiline = true;
        gmb_p_parse_block(p, &body, GMB_TOK_Next, "Next", line);
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Next || gmb_p_tok_is_keyword(gmb_p_cur(p), "Next"))) {
            gmb_p_adv(p);
            int extras = 0;
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)) {
                if (gmb_p_check(p, GMB_TOK_Comma)) extras++;   /* NEXT J, I — chiude anche il FOR esterno */
                gmb_p_adv(p);
            }
            if (extras > 0) p->pending_for_ends += extras;
        }
    }
    s->for_body = body.items; s->n_for = body.n;
    s->for_next_bas = p->bas_fresh ? p->last_bas_line : 0;   /* 2.8: riga del NEXT (o del NEXT condiviso) */
    return s;
}

/* WHILE <cond> ... WEND */
gmb_stmt_t* gmb_p_parse_while(gmb_parser_t* p, int line, bool* multiline)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_WHILE, line);
    s->cond = gmb_p_parse_expr(p);
    gmb_stmt_list_t body = gmb_p_stmt_list_new();
    if (gmb_p_check(p, GMB_TOK_Colon)) {
        /* body inline: WHILE c : a=1 : b=2 : WEND */
        inline_body(p, &body, GMB_TOK_Wend, "Wend");
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Wend || gmb_p_tok_is_keyword(gmb_p_cur(p), "Wend"))) gmb_p_adv(p);
    } else {
        *multiline = true;
        gmb_p_parse_block(p, &body, GMB_TOK_Wend, "Wend", line);
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Wend || gmb_p_tok_is_keyword(gmb_p_cur(p), "Wend"))) gmb_p_adv(p);
    }
    s->while_body = body.items; s->n_while = body.n;
    return s;
}

/* TICK [rate] ... END TICK [rate] — game loop. Parità emitter C# (TickStmt/TickEndStmt
 * no-op): reso come DO WHILE 1 (loop infinito) con uscita via `EXIT TICK` → break.
 * Il rate (frame pacing) viaggia in `s->value` e il codegen C99Native lo emette come
 * `gmb_tick_pace(rate)` + un present per frame; rate assente = 0 = nessun limiter
 * (present una volta per frame, ritmo del ciclo). */
gmb_stmt_t* gmb_p_parse_tick(gmb_parser_t* p, int line, bool* multiline)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_DO, line);
    *multiline = true;
    /* TICK <rate>: frame limiter (prima l'argomento era letto e SCARTATO → il loop
     * girava a migliaia di fps: present instabile/finestra nera). */
    if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_End) && !gmb_p_at_end(p))
    {
        gmb_expr_t* rate = gmb_p_parse_expr(p);
        /* Parità con le gambe IL/PCode/VM-C: il rate viaggia come int nell'AST (TickStmt),
         * quindi deve essere una COSTANTE numerica. Un'espressione veniva valutata solo qui e
         * scartata in silenzio dalle altre gambe: divergenza. */
        if (rate && rate->kind == GMB_EXPR_NUM) s->value = rate;
        else { if (rate) gmb_expr_free(rate); gmb_p_set_err(p, "TICK: il rate deve essere un numero letterale", line); }
    }
    /* Rate di default 0 = NESSUN limiter, con il present comunque una volta per frame.
     * Senza questo un `TICK` senza argomento lasciava `value` NULL e il codegen non
     * emetteva né pace né present (indistinguibile da un `DO WHILE 1` scritto a mano:
     * il TICK è l'unico ciclo DO/WHILE che valorizza `value`). */
    if (!s->value) s->value = gmb_expr_num(0, line);
    s->do_while = gmb_expr_num(1, line);   /* DO WHILE 1 */
    /* flag DEVE restare 0 (WHILE): il codegen usa flag!=0 come UNTIL/exit-if-true e il
     * vecchio marker 2 faceva uscire subito dal loop senza eseguire il corpo. */
    s->flag = 0;
    gmb_stmt_list_t body = gmb_p_stmt_list_new();
    gmb_p_parse_block(p, &body, GMB_TOK_EndTick, NULL, line);
    /* END TICK [rate] (il lexer produce END + TICK) */
    if (gmb_p_check(p, GMB_TOK_End) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Tick) {
        gmb_p_adv(p); gmb_p_adv(p);
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
            gmb_expr_t* r2 = gmb_p_parse_expr(p);
            if (r2 && r2->kind == GMB_EXPR_NUM) { if (s->value) gmb_expr_free(s->value); s->value = r2; }
            else { if (r2) gmb_expr_free(r2); gmb_p_set_err(p, "END TICK: il rate deve essere un numero letterale", line); }
        }
    }
    s->do_body = body.items; s->n_do = body.n;
    return s;
}

/* DO ... LOOP [UNTIL <cond>] */
gmb_stmt_t* gmb_p_parse_do(gmb_parser_t* p, int line, bool* multiline)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_DO, line);
    *multiline = true;
    /* DO WHILE <cond> — test in cima (parità While); DO UNTIL <cond> — test in cima con negazione */
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "While") || gmb_p_check(p, GMB_TOK_While)) {
        gmb_p_adv(p);
        s->do_while = gmb_p_parse_expr(p);
    } else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Until") || gmb_p_check(p, GMB_TOK_Until)) {
        gmb_p_adv(p);
        s->do_while = gmb_p_parse_expr(p);
        s->flag = 1;   /* UNTIL: esci quando vero */
    }
    gmb_stmt_list_t body = gmb_p_stmt_list_new();
    gmb_p_parse_block(p, &body, GMB_TOK_Loop, "Loop", line);
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Loop || gmb_p_tok_is_keyword(gmb_p_cur(p), "Loop"))) {
        gmb_p_adv(p);
        /* LOOP UNTIL/WHILE <cond> — test in coda */
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Until") || gmb_p_check(p, GMB_TOK_Until)) { gmb_p_adv(p); s->do_until = gmb_p_parse_expr(p); s->flag = 1; }
        else if (gmb_p_tok_is_keyword(gmb_p_cur(p), "While") || gmb_p_check(p, GMB_TOK_While)) { gmb_p_adv(p); s->do_until = gmb_p_parse_expr(p); }
    }
    s->do_body = body.items; s->n_do = body.n;
    return s;
}
