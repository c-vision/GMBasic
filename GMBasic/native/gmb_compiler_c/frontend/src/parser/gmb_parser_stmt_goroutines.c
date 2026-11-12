/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_goroutines.c — speculare a
 * Parser.Stmt.GoRoutines.cs: GO (SUB/FUNCTION, EXIT, SELECT), SEND, RECEIVE,
 * CHANNEL CLOSE, MUTEX, COND, ONCE, WAIT, DONE, SLEEP, DEFER, TIMEOUT. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* lookup set (P2bis): token che terminano il target di RECEIVE. */
static const gmb_tokenkind_t gmb_recv_end_toks[] = {
    GMB_TOK_Newline, GMB_TOK_Colon, GMB_TOK_Eof, GMB_TOK_From, GMB_TOK_Ok, GMB_TOK_Comma
};
static int gmb_recv_end_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_recv_end_toks) / sizeof(gmb_recv_end_toks[0]); i++)
        if (gmb_recv_end_toks[i] == k) return 1;
    return 0;
}

static gmb_stmt_t* gmb_p_parse_send(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SEND, line);
    gmb_p_adv(p);   /* SEND */
    s->send_value = gmb_p_parse_expr(p);
    if (!gmb_p_match(p, GMB_TOK_To)) { gmb_p_set_err(p, "Atteso 'TO' dopo il valore in SEND", line); return s; }
    s->channel = gmb_p_parse_expr(p);
    return s;
}

static gmb_stmt_t* gmb_p_parse_receive(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_RECEIVE, line);
    gmb_p_adv(p);   /* RECEIVE */
    const gmb_token_t* dst = gmb_p_cur(p);
    if (!dst || gmb_recv_end_in(dst->kind)) { gmb_p_set_err(p, "Atteso target dopo RECEIVE", line); return s; }
    s->receive_dest = gmb_p_tok_strdup(dst);
    gmb_p_adv(p);
    if (!gmb_p_match(p, GMB_TOK_From)) { gmb_p_set_err(p, "Atteso 'FROM' dopo la destinazione in RECEIVE", line); return s; }
    s->channel = gmb_p_parse_expr(p);
    if (gmb_p_match(p, GMB_TOK_Ok)) {
        const gmb_token_t* ok = gmb_p_cur(p);
        if (ok && ok->lexeme && ok->length) { s->ok_flag = gmb_expr_var(gmb_p_tok_strdup(ok), ok->line); gmb_p_adv(p); }
    }
    return s;
}

static gmb_stmt_t* gmb_p_parse_mutex(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_MUTEX, line);
    gmb_p_adv(p);   /* MUTEX */
    const gmb_token_t* sub = gmb_p_cur(p);
    if (!sub) { gmb_p_set_err(p, "Atteso LOCK/UNLOCK/RLOCK/RUNLOCK dopo MUTEX", line); return s; }
    const char* act = NULL;
    if (sub->kind == GMB_TOK_Lock) act = "LOCK";
    else if (sub->kind == GMB_TOK_Unlock) act = "UNLOCK";
    else if (sub->kind == GMB_TOK_RLock) act = "RLOCK";
    else if (sub->kind == GMB_TOK_Runlock) act = "RUNLOCK";
    else { gmb_p_set_err(p, "Sottocomando MUTEX non riconosciuto", line); }
    s->mutex_action = act;
    gmb_p_adv(p);
    s->channel = gmb_p_parse_expr(p);
    return s;
}

static gmb_stmt_t* gmb_p_parse_cond(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_COND, line);
    gmb_p_adv(p);   /* COND */
    const gmb_token_t* sub = gmb_p_cur(p);
    if (!sub) { gmb_p_set_err(p, "Atteso WAIT/SIGNAL/BROADCAST dopo COND", line); return s; }
    if (sub->kind == GMB_TOK_Wait) { s->mutex_action = "WAIT"; gmb_p_adv(p); s->channel = gmb_p_parse_expr(p); if (gmb_p_match(p, GMB_TOK_Comma)) s->cond_mutex = gmb_p_parse_expr(p); }
    else if (gmb_p_tok_is_soft_keyword(sub, "SIGNAL")) { s->mutex_action = "SIGNAL"; gmb_p_adv(p); s->channel = gmb_p_parse_expr(p); }
    else if (gmb_p_tok_is_soft_keyword(sub, "BROADCAST")) { s->mutex_action = "BROADCAST"; gmb_p_adv(p); s->channel = gmb_p_parse_expr(p); }
    else gmb_p_set_err(p, "Sottocomando COND non riconosciuto (attesi WAIT, SIGNAL, BROADCAST)", line);
    return s;
}

static gmb_stmt_t* gmb_p_parse_wait(gmb_parser_t* p, int line, int kind)
{
    gmb_stmt_t* s = gmb_stmt_new(kind, line);
    gmb_p_adv(p);
    /* WAIT nuda (fine riga): nessun operando → channel NULL = attesa figli impliciti
     * (bug 2026-10-01). Solo per WAIT: SLEEP/DONE nudi mantengono il vecchio phantom. */
    if (kind == GMB_STMT_WAIT) {
        const gmb_token_t* tg = gmb_p_cur(p);
        if (!tg || tg->kind == GMB_TOK_Newline || tg->kind == GMB_TOK_Eof || tg->kind == GMB_TOK_Colon)
            return s;
    }
    s->channel = gmb_p_parse_expr(p);
    return s;
}

static gmb_stmt_t* gmb_p_parse_defer(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_DEFER, line);
    gmb_p_adv(p);   /* DEFER */
    bool ml = false;
    s->go_comm = gmb_p_parse_stmt(p, &ml);
    return s;
}

static gmb_stmt_t* gmb_p_parse_timeout(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TIMEOUT, line);
    gmb_p_adv(p);
    s->channel = gmb_p_parse_expr(p);
    return s;
}

static bool gmb_p_at_end_select(gmb_parser_t* p)
{
    const gmb_token_t* a = gmb_p_cur(p);
    if (!a) return false;
    if (a->kind == GMB_TOK_EndSelect) return true;
    return gmb_p_at_end_compound(p, GMB_TOK_Select);
}
static bool gmb_p_match_end_select(gmb_parser_t* p)
{
    if (!gmb_p_at_end_select(p)) return false;
    if (gmb_p_cur(p)->kind == GMB_TOK_EndSelect) { gmb_p_adv(p); return true; }
    gmb_p_adv(p); gmb_p_adv(p);
    return true;
}

static gmb_stmt_t* gmb_p_parse_go_select_case(gmb_parser_t* p, int line)
{
    (void)line;
    /* CASE <RECEIVE|SEND|TIMEOUT> op : body (fino a CASE/END SELECT) */
    gmb_stmt_t* comm = NULL;
    const gmb_token_t* t = gmb_p_cur(p);
    if (t && t->kind == GMB_TOK_Receive) comm = gmb_p_parse_receive(p, t->line);
    else if (t && t->kind == GMB_TOK_Send) comm = gmb_p_parse_send(p, t->line);
    else if (t && t->kind == GMB_TOK_Timeout) comm = gmb_p_parse_timeout(p, t->line);
    else if (t && t->kind == GMB_TOK_Default) { gmb_p_adv(p); comm = NULL; /* caso default: nessuna operazione di canale */ }
    else { gmb_p_set_err(p, "Atteso RECEIVE, SEND o TIMEOUT dopo CASE in GO SELECT", line); }
    if (!gmb_p_match(p, GMB_TOK_Colon)) { gmb_p_set_err(p, "Atteso ':' dopo l'operazione di canale", line); }
    int cap = 0, n = 0;
    gmb_stmt_t** body = NULL;
    for (;;) {
        gmb_p_skip_eol(p);
        if (gmb_p_at_end(p)) break;
        if (gmb_p_check(p, GMB_TOK_Case) || gmb_p_at_end_select(p) || gmb_p_check(p, GMB_TOK_Else)) break;
        int before = p->pos;
        bool ml2 = false;
        gmb_stmt_t* st = gmb_p_parse_stmt(p, &ml2);
        if (!st || p->pos == before) break;   /* protezione da loop infinito */
        if (n >= cap) { cap = cap ? cap * 2 : 4; body = (gmb_stmt_t**)realloc(body, (size_t)cap * sizeof(gmb_stmt_t*)); }
        body[n++] = st;
    }
    gmb_go_case_t* c = (gmb_go_case_t*)malloc(sizeof(gmb_go_case_t));
    c->comm = comm; c->body = body; c->n_body = n;
    gmb_stmt_t* wrap = gmb_stmt_new(GMB_STMT_GOSELECT, 0);
    wrap->go_cases = (gmb_go_case_t*)malloc(sizeof(gmb_go_case_t));
    wrap->go_cases[0] = *c; wrap->n_go_cases = 1;
    free(c);
    return wrap;
}

static gmb_stmt_t* gmb_p_parse_go_select(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_GOSELECT, line);
    gmb_p_adv(p);   /* SELECT */
    gmb_p_match(p, GMB_TOK_Newline);
    int cap = 0, n = 0;
    gmb_go_case_t* cases = NULL;
    while (!gmb_p_at_end_select(p) && !gmb_p_at_end(p)) {
        gmb_p_skip_eol(p);
        if (gmb_p_at_end(p)) break;
        if (gmb_p_at_end_select(p)) break;
        if (!gmb_p_match(p, GMB_TOK_Case)) { gmb_p_set_err(p, "Atteso CASE o END SELECT", line); break; }
        gmb_stmt_t* cc = gmb_p_parse_go_select_case(p, line);
        if (cc) {
            if (n >= cap) { cap = cap ? cap * 2 : 4; cases = (gmb_go_case_t*)realloc(cases, (size_t)cap * sizeof(gmb_go_case_t)); }
            cases[n++] = cc->go_cases[0];
            free(cc->go_cases); free(cc);
        }
        if (p->error) break;
    }
    gmb_p_match_end_select(p);
    s->go_cases = cases; s->n_go_cases = n;
    return s;
}

static gmb_stmt_t* gmb_p_parse_go(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (t && t->kind == GMB_TOK_Go) gmb_p_adv(p);
    t = gmb_p_cur(p);
    if (t && t->kind == GMB_TOK_Exit) { gmb_p_adv(p); return gmb_stmt_new(GMB_STMT_GOEXIT, line); }
    if (t && t->kind == GMB_TOK_Select) return gmb_p_parse_go_select(p, line);
    /* GO SUB/FUNCTION proc(args) [IN wg] [INTO target] */
    if (t && t->kind == GMB_TOK_Function) { gmb_p_adv(p); }   /* GO SUB/FUNCTION: SUB mappa su Function */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_GO, line);
    /* flag=1 se la keyword FUNCTION era presente (GO FUNCTION ... INTO → OP_GO_FUNCTION) */
    if (t && t->kind == GMB_TOK_Function) s->flag = 1;
    t = gmb_p_cur(p);
    if (!t || (t->kind != GMB_TOK_Identifier && t->kind != GMB_TOK_Number && !(t->kind >= GMB_TOK_Define))) { gmb_p_set_err(p, "Atteso nome procedura dopo GO", line); return s; }
    s->proc_name = gmb_expr_var(gmb_p_tok_strdup(t), t->line);
    gmb_p_adv(p);
    if (gmb_p_match(p, GMB_TOK_LeftParen)) {
        int cap = 0;
        while (!gmb_p_check(p, GMB_TOK_RightParen) && !gmb_p_at_end(p)) {
            if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 4; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
            s->go_args[s->n_go_args++] = gmb_p_parse_expr(p);
            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
        }
        gmb_p_match(p, GMB_TOK_RightParen);
    }
    if (gmb_p_match(p, GMB_TOK_In)) s->go_wait = gmb_p_parse_expr(p);
    if (gmb_p_match(p, GMB_TOK_Into)) {
        const gmb_token_t* tg = gmb_p_cur(p);
        if (tg && tg->kind != GMB_TOK_Newline && tg->kind != GMB_TOK_Eof && tg->kind != GMB_TOK_Colon) { s->go_into = gmb_p_tok_strdup(tg); gmb_p_adv(p); }
    }
    return s;
}

static gmb_stmt_t* gmb_p_parse_once(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ONCE, line);
    gmb_p_adv(p);   /* ONCE */
    /* RUN non è (ancora) una keyword del lexer C: arriva come Identifier. Lo si accetta
     * anche per grafia (soft keyword), come fa il parser C# con TokenKind.Run; senza,
     * `ONCE RUN o, proc` dava "statement non riconosciuto" (concurrency_once.bas). */
    if (gmb_p_match(p, GMB_TOK_Run) || gmb_p_tok_is_soft_keyword(gmb_p_cur(p), "RUN")) {
        if (gmb_p_check(p, GMB_TOK_Identifier)) gmb_p_adv(p);   /* consuma RUN */
        s->channel = gmb_p_parse_expr(p);
        if (gmb_p_match(p, GMB_TOK_Comma)) {
            const gmb_token_t* t = gmb_p_cur(p);
            if (t) { s->proc_name = gmb_expr_var(gmb_p_tok_strdup(t), t->line); gmb_p_adv(p); }
        }
    }
    return s;
}

gmb_stmt_t* gmb_p_parse_goroutine(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return NULL;
    /* keyword usata come variabile seguita da '=' (es. done = TRUE) → non è un comando GPM */
    if (gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Equal) return NULL;
    switch (t->kind) {
    case GMB_TOK_Go: return gmb_p_parse_go(p, line);
    case GMB_TOK_Send: return gmb_p_parse_send(p, line);
    case GMB_TOK_Receive: return gmb_p_parse_receive(p, line);
    case GMB_TOK_Mutex: return gmb_p_parse_mutex(p, line);
    case GMB_TOK_Cond: return gmb_p_parse_cond(p, line);
    case GMB_TOK_Wait: return gmb_p_parse_wait(p, line, GMB_STMT_WAIT);
    case GMB_TOK_Done: return gmb_p_parse_wait(p, line, GMB_STMT_DONE);
    case GMB_TOK_Sleep: return gmb_p_parse_wait(p, line, GMB_STMT_SLEEP);
    case GMB_TOK_Defer: return gmb_p_parse_defer(p, line);
    case GMB_TOK_Timeout: return gmb_p_parse_timeout(p, line);
    case GMB_TOK_Once: return gmb_p_parse_once(p, line);
    default: break;
    }
    /* CHANNEL [CLOSE ch] */
    if (t->kind == GMB_TOK_Channel) {
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_CHANNEL, line);
        gmb_p_adv(p);
        const gmb_token_t* sub = gmb_p_cur(p);
        if (sub && sub->kind == GMB_TOK_Close) { s->mutex_action = "CLOSE"; gmb_p_adv(p); s->channel = gmb_p_parse_expr(p); }
        else { gmb_p_set_err(p, "Atteso CLOSE dopo CHANNEL", line); }
        return s;
    }
    return NULL;
}
