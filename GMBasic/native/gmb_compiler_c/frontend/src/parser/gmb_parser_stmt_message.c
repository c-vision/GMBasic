/* gmb_parser_stmt_message.c — MESSAGE (namespace MESSAGE): verbi CONNECT/SUBSCRIBE/
 * UNSUBSCRIBE/PUBLISH/PING/CLOSE (sintassi a spazio, come WINDOW). */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int kw(const gmb_token_t* t, const char* k)
{
    return t && t->length == (int)strlen(k) && strncasecmp(t->lexeme, k, (size_t)t->length) == 0;
}

int gmb_p_is_message_verb(const gmb_token_t* t)
{
    return kw(t, "CONNECT") || kw(t, "SUBSCRIBE") || kw(t, "UNSUBSCRIBE") ||
           kw(t, "PUBLISH") || kw(t, "PING") || kw(t, "CLOSE");
}

static char* updup(const char* s, int len)
{
    char* o = (char*)malloc((size_t)len + 1); int i;
    if (!o) return NULL;
    for (i = 0; i < len; i++) { char c = s[i]; o[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }
    o[len] = 0; return o;
}

static int peek_kw(gmb_parser_t* p, const char* k) { return kw(gmb_p_cur(p), k); }

static gmb_expr_t* paren_expr(gmb_parser_t* p, int line)
{
    if (gmb_p_check(p, GMB_TOK_LeftParen)) gmb_p_adv(p); else { gmb_p_set_err(p, "Attesa '('", line); return NULL; }
    gmb_expr_t* e = gmb_p_parse_expr(p);
    if (gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p); else { gmb_p_set_err(p, "Attesa ')'", line); }
    return e;
}

gmb_stmt_t* gmb_p_parse_message(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_MESSAGE, line);
    const gmb_token_t* v;
    gmb_p_adv(p); /* MESSAGE */
    v = gmb_p_cur(p);
    if (!v || !gmb_p_is_message_verb(v)) { gmb_p_set_err(p, "Atteso verbo dopo MESSAGE", line); return s; }
    s->mutex_action = updup(v->lexeme, v->length);
    gmb_p_adv(p);

    if (strcmp(s->mutex_action, "CONNECT") == 0) {
        s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
        s->net_args[0] = gmb_p_parse_expr(p); s->n_netargs = 1;
        if (peek_kw(p, "AS")) { gmb_p_adv(p); if (gmb_p_cur(p)) { s->target_label = (char*)malloc((size_t)gmb_p_cur(p)->length + 1); memcpy(s->target_label, gmb_p_cur(p)->lexeme, (size_t)gmb_p_cur(p)->length); s->target_label[gmb_p_cur(p)->length] = 0; gmb_p_adv(p); } }
    } else if (strcmp(s->mutex_action, "SUBSCRIBE") == 0) {
        s->net_args = (gmb_expr_t**)calloc(4, sizeof(gmb_expr_t*));
        s->net_args[0] = gmb_p_parse_expr(p); s->n_netargs = 1;
        if (peek_kw(p, "TOPIC")) { gmb_p_adv(p); s->net_args[1] = paren_expr(p, line); s->n_netargs = 2; }
        if (peek_kw(p, "QUEUE")) { gmb_p_adv(p); s->net_args[2] = paren_expr(p, line); s->n_netargs = 3; }
        if (peek_kw(p, "AS")) { gmb_p_adv(p); s->net_args[3] = gmb_p_parse_expr(p); s->n_netargs = 4; }
    } else if (strcmp(s->mutex_action, "PUBLISH") == 0) {
        s->net_args = (gmb_expr_t**)calloc(3, sizeof(gmb_expr_t*));
        s->net_args[0] = gmb_p_parse_expr(p); s->n_netargs = 1;
        if (peek_kw(p, "TOPIC")) { gmb_p_adv(p); s->net_args[1] = paren_expr(p, line); s->n_netargs = 2; }
        if (peek_kw(p, "DATA")) { gmb_p_adv(p); s->net_args[2] = paren_expr(p, line); }
        else s->net_args[2] = gmb_p_parse_expr(p);
        s->n_netargs = 3;
    } else if (strcmp(s->mutex_action, "UNSUBSCRIBE") == 0) {
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline && gmb_p_cur(p)->kind != GMB_TOK_Colon && gmb_p_cur(p)->kind != GMB_TOK_Eof) {
            s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
            s->net_args[0] = gmb_p_parse_expr(p); s->n_netargs = 1;
        }
    } else { /* PING / CLOSE [target] */
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline && gmb_p_cur(p)->kind != GMB_TOK_Colon && gmb_p_cur(p)->kind != GMB_TOK_Eof) {
            s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
            s->net_args[0] = gmb_p_parse_expr(p); s->n_netargs = 1;
        }
    }
    return s;
}
