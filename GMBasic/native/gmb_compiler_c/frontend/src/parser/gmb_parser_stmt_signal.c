/* gmb_parser_stmt_signal.c — SIGNAL (namespace SIGNAL): segnali con filtro strutturato.
 *
 *   SIGNAL SUBSCRIBE [TYPES(..)] [ASSET(..)] [MARKET(..)] [SYMBOLS(..)] [TIMEFRAME(..)] AS "id"
 *   SIGNAL EMIT tipo, simbolo, valore [ASSET(..)] [MARKET(..)] [TIMEFRAME(..)]
 *   SIGNAL UNSUBSCRIBE ["id"]
 *   SIGNAL PING
 *
 * TYPES/SYMBOLS accettano una LISTA fra parentesi. Con argomenti costanti la lista è
 * piegata in un CSV a compile-time ("a,b"); con un solo argomento si usa il valore
 * dell'espressione così com'è (CSV dinamico). Vedi gmb_signal.h per la semantica dei filtri.
 */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int sig_kw(const gmb_token_t* t, const char* k)
{
    return t && t->length == (int)strlen(k) && strncasecmp(t->lexeme, k, (size_t)t->length) == 0;
}

static char* sig_updup(const char* s, int len)
{
    char* o = (char*)malloc((size_t)len + 1); int i;
    if (!o) return NULL;
    for (i = 0; i < len; i++) { char c = s[i]; o[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }
    o[len] = 0; return o;
}

static gmb_expr_t* sig_str_expr(const char* s, int line)
{
    gmb_expr_t* e = (gmb_expr_t*)calloc(1, sizeof(gmb_expr_t));
    if (!e) return NULL;
    e->kind = GMB_EXPR_STR; e->line = line;
    e->str = (char*)s;
    return e;
}

/* Legge una clausola TYPES/ASSET/MARKET/SYMBOLS/TIMEFRAME(...).
 * Lista fra parentesi: se TUTTI gli argomenti sono costanti stringa la si piega in
 * "a,b"; un solo argomento non costante resta un'espressione (CSV dinamico).
 * Errore esplicito se la lista contiene argomenti non costanti (la leg C deve generare
 * codice identico: un CSV costruito da espressioni non ha parità con il parser C#,
 * che accetta solo letterali). */
static int sig_clause(gmb_parser_t* p, gmb_expr_t** out, int line)
{
    if (!gmb_p_check(p, GMB_TOK_LeftParen)) { *out = gmb_p_parse_expr(p); return 1; }
    gmb_p_adv(p);
    {
        char buf[512]; size_t used = 0; int n = 0;
        buf[0] = 0;
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) {
            gmb_expr_t* a = gmb_p_parse_expr(p);
            if (!a) { gmb_p_set_err(p, "SIGNAL: argomento di filtro mancante", line); return 0; }
            if (a->kind != GMB_EXPR_STR || !a->str) {
                if (n == 0) {
                    /* Unico argomento non costante: CSV dinamico. */
                    if (gmb_p_check(p, GMB_TOK_RightParen)) { gmb_p_adv(p); *out = a; return 1; }
                }
                gmb_p_set_err(p, "SIGNAL: i filtri TYPES/SYMBOLS accettano solo stringhe costanti", line);
                return 0;
            }
            if (n > 0 && used + 1 < sizeof(buf)) buf[used++] = ',';
            {
                size_t l = strlen(a->str);
                if (used + l >= sizeof(buf)) l = sizeof(buf) - used - 1;
                memcpy(buf + used, a->str, l); used += l; buf[used] = 0;
            }
            n++;
            if (gmb_p_check(p, GMB_TOK_Comma)) { gmb_p_adv(p); continue; }
            break;
        }
        if (!gmb_p_check(p, GMB_TOK_RightParen)) { gmb_p_set_err(p, "SIGNAL: attesa ')'", line); return 0; }
        gmb_p_adv(p);
        {
            char* dup = (char*)malloc(used + 1);
            if (!dup) return 0;
            memcpy(dup, buf, used + 1);
            *out = sig_str_expr(dup, line);
        }
        return 1;
    }
}

gmb_stmt_t* gmb_p_parse_signal(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_SIGNAL, line);
    const gmb_token_t* v;
    gmb_p_adv(p); /* SIGNAL */
    v = gmb_p_cur(p);
    if (!v) { gmb_p_set_err(p, "Atteso verbo dopo SIGNAL", line); return s; }
    s->mutex_action = sig_updup(v->lexeme, v->length);
    gmb_p_adv(p);

    if (strcmp(s->mutex_action, "SUBSCRIBE") == 0) {
        s->net_args = (gmb_expr_t**)calloc(6, sizeof(gmb_expr_t*));
        s->n_netargs = 6;
        if (sig_kw(gmb_p_cur(p), "TYPES"))     { gmb_p_adv(p); if (!sig_clause(p, &s->net_args[0], line)) return s; }
        if (sig_kw(gmb_p_cur(p), "ASSET"))     { gmb_p_adv(p); if (!sig_clause(p, &s->net_args[1], line)) return s; }
        if (sig_kw(gmb_p_cur(p), "MARKET"))    { gmb_p_adv(p); if (!sig_clause(p, &s->net_args[2], line)) return s; }
        if (sig_kw(gmb_p_cur(p), "SYMBOLS"))   { gmb_p_adv(p); if (!sig_clause(p, &s->net_args[3], line)) return s; }
        if (sig_kw(gmb_p_cur(p), "TIMEFRAME")) { gmb_p_adv(p); if (!sig_clause(p, &s->net_args[4], line)) return s; }
        if (sig_kw(gmb_p_cur(p), "AS")) { gmb_p_adv(p); s->net_args[5] = gmb_p_parse_expr(p); }
    } else if (strcmp(s->mutex_action, "EMIT") == 0) {
        s->net_args = (gmb_expr_t**)calloc(6, sizeof(gmb_expr_t*));
        s->n_netargs = 6;
        s->net_args[0] = gmb_p_parse_expr(p);
        if (gmb_p_check(p, GMB_TOK_Comma)) gmb_p_adv(p);
        else { gmb_p_set_err(p, "SIGNAL EMIT: attesa ',' dopo il tipo", line); return s; }
        s->net_args[1] = gmb_p_parse_expr(p);
        if (gmb_p_check(p, GMB_TOK_Comma)) gmb_p_adv(p);
        else { gmb_p_set_err(p, "SIGNAL EMIT: attesa ',' dopo il simbolo", line); return s; }
        s->net_args[2] = gmb_p_parse_expr(p);
        /* La forma canonica separa le clausole con una virgola: `EMIT "t","s",85, ASSET(..)` */
        if (gmb_p_check(p, GMB_TOK_Comma)) gmb_p_adv(p);
        if (sig_kw(gmb_p_cur(p), "ASSET"))     { gmb_p_adv(p); if (!sig_clause(p, &s->net_args[3], line)) return s; }
        if (sig_kw(gmb_p_cur(p), "MARKET"))    { gmb_p_adv(p); if (!sig_clause(p, &s->net_args[4], line)) return s; }
        if (sig_kw(gmb_p_cur(p), "TIMEFRAME")) { gmb_p_adv(p); if (!sig_clause(p, &s->net_args[5], line)) return s; }
    } else if (strcmp(s->mutex_action, "UNSUBSCRIBE") == 0) {
        if (gmb_p_cur(p) && !gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) &&
            !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_check(p, GMB_TOK_Eof)) {
            s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
            s->net_args[0] = gmb_p_parse_expr(p); s->n_netargs = 1;
        }
    } else if (strcmp(s->mutex_action, "PING") != 0) {
        gmb_p_set_err(p, "Atteso SUBSCRIBE/EMIT/UNSUBSCRIBE/PING dopo SIGNAL", line);
    }
    return s;
}

/* ON SIGNAL <id> CALL <handler>: l'arità dell'handler (4 o 5 parametri) è decisa dal
 * frontend che conosce le SUB (la VM risolve il nome a runtime) — qui si registra il
 * nome; l'arità la calcola il chiamante dall'AST (gmb_p_parse_on_signal non la conosce). */
gmb_stmt_t* gmb_p_parse_on_signal(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_ON_SIGNAL, line);
    gmb_p_adv(p); /* SIGNAL */
    s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
    s->n_netargs = 1;
    s->net_args[0] = gmb_p_parse_expr(p);
    if (sig_kw(gmb_p_cur(p), "CALL") || gmb_p_check(p, GMB_TOK_Call)) { gmb_p_adv(p); s->flag = 0; }
    else if (sig_kw(gmb_p_cur(p), "GOSUB") || gmb_p_check(p, GMB_TOK_Gosub)) { gmb_p_adv(p); s->flag = 1; }
    else if (sig_kw(gmb_p_cur(p), "STOP") || gmb_p_check(p, GMB_TOK_Stop)) { gmb_p_adv(p); s->flag = 2; return s; }
    {
        const gmb_token_t* h = gmb_p_cur(p);
        if (!h) { gmb_p_set_err(p, "ON SIGNAL: atteso il nome dell'handler", line); return s; }
        s->target_label = (char*)malloc((size_t)h->length + 1);
        memcpy(s->target_label, h->lexeme, (size_t)h->length);
        s->target_label[h->length] = 0;
        gmb_p_adv(p);
        if (gmb_p_check(p, GMB_TOK_LeftParen)) {
            int depth = 0;
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) {
                int k = gmb_p_cur(p) ? gmb_p_cur(p)->kind : GMB_TOK_Newline;
                if (k == GMB_TOK_LeftParen) depth++;
                else if (k == GMB_TOK_RightParen) depth--;
                gmb_p_adv(p);
                if (depth <= 0) break;
            }
        }
    }
    return s;
}
