/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_team.c — speculare a
 * Parser.Stmt.Team.cs: TEAM CREATE/SAVE/LOAD name, TEAM name ADD/REMOVE AGENT,
 * RUN task, SET ROLE/STRATEGY/COORDINATOR, TEAM MESSAGE name, msg. */
#include "gmb_parser_internal.h"
#include <stdlib.h>

static char* gmb_p_ident(gmb_parser_t* p, const char* what, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->length == 0) { gmb_p_set_err(p, what, line); return NULL; }
    char* s = gmb_p_tok_strdup(t);
    gmb_p_adv(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_team(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return NULL;
    if (t->kind == GMB_TOK_Message || (t->kind == GMB_TOK_TeamCreate && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Message)) {
        gmb_p_adv(p);
        if (t->kind == GMB_TOK_TeamCreate) gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_MESSAGE, line);
        s->win_name = gmb_p_ident(p, "Atteso nome team dopo TEAM MESSAGE", line);
        if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo il nome team", line); return s; }
        s->channel = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_TeamCreate) gmb_p_adv(p);

    else return NULL;
    t = gmb_p_cur(p);
    if (!t) return NULL;
    /* TEAM TRAIN INNER team, agent, dataset | TEAM TRAIN OUTER team, dataset */
    if (t->kind == GMB_TOK_Train) {
        gmb_p_adv(p);
        t = gmb_p_cur(p);
        if (t && t->kind == GMB_TOK_Inner) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_TRAININNER, line);
            s->win_name = gmb_p_ident(p, "Atteso nome team dopo TRAIN INNER", line);
            if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo il nome team", line); return s; }
            s->channel = gmb_p_parse_expr(p);
            if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo l'agente", line); return s; }
            s->send_value = gmb_p_parse_expr(p);
            return s;
        }
        if (t && t->kind == GMB_TOK_Outer) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_TRAINOUTER, line);
            s->win_name = gmb_p_ident(p, "Atteso nome team dopo TRAIN OUTER", line);
            if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo il nome team", line); return s; }
            s->channel = gmb_p_parse_expr(p);
            return s;
        }
        if (t && t->kind == GMB_TOK_Thinking) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_TRAINTHINKING, line);
            s->win_name = gmb_p_ident(p, "Atteso nome team dopo TRAIN THINKING", line);
            if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo il nome team", line); return s; }
            s->channel = gmb_p_parse_expr(p);
            return s;
        }
        if (t && t->kind == GMB_TOK_Actor) {
            gmb_p_adv(p);
            if (!gmb_p_match(p, GMB_TOK_Critic)) { gmb_p_set_err(p, "Atteso CRITIC dopo TRAIN ACTOR", line); return NULL; }
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_TRAINACTORCRITIC, line);
            s->win_name = gmb_p_ident(p, "Atteso nome team dopo TRAIN ACTOR CRITIC", line);
            if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo il nome team", line); return s; }
            s->channel = gmb_p_parse_expr(p);
            return s;
        }
        gmb_p_set_err(p, "Atteso INNER, OUTER, THINKING o ACTOR CRITIC dopo TEAM TRAIN", line);
        return NULL;
    }
    /* TEAM DISTILL THINKING team, dataset — Quiet-STaR 9.3 */
    if (t->kind == GMB_TOK_Distill) {
        gmb_p_adv(p);
        if (!gmb_p_match(p, GMB_TOK_Thinking)) { gmb_p_set_err(p, "Atteso THINKING dopo TEAM DISTILL", line); return NULL; }
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_DISTILLTHINKING, line);
        s->win_name = gmb_p_ident(p, "Atteso nome team dopo DISTILL THINKING", line);
        if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo il nome team", line); return s; }
        s->channel = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Create) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_CREATE, line);
        s->win_name = gmb_p_ident(p, "Atteso nome team dopo TEAM CREATE", line);
        return s;
    }
    if (t->kind == GMB_TOK_Save) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SAVE, line);
        s->win_name = gmb_p_ident(p, "Atteso nome team dopo TEAM SAVE", line);
        return s;
    }
    if (t->kind == GMB_TOK_Load) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_LOAD, line);
        s->win_name = gmb_p_ident(p, "Atteso nome team dopo TEAM LOAD", line);
        return s;
    }
    /* TEAM <nome> <subcomando> */
    char* team = gmb_p_ident(p, "Atteso nome team o CREATE dopo TEAM", line);
    t = gmb_p_cur(p);
    if (!t || !team) { free(team); return NULL; }
    if (t->kind == GMB_TOK_Add) {
        gmb_p_adv(p);
        if (!gmb_p_match(p, GMB_TOK_Agent)) { gmb_p_set_err(p, "Atteso AGENT dopo ADD", line); free(team); return NULL; }
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_ADDAGENT, line);
        s->win_name = team;
        s->channel = gmb_expr_str(gmb_p_ident(p, "Atteso nome agente", line), line);
        return s;
    }
    if (t->kind == GMB_TOK_Remove) {
        gmb_p_adv(p);
        if (!gmb_p_match(p, GMB_TOK_Agent)) { gmb_p_set_err(p, "Atteso AGENT dopo REMOVE", line); free(team); return NULL; }
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_REMOVEAGENT, line);
        s->win_name = team;
        s->channel = gmb_expr_str(gmb_p_ident(p, "Atteso nome agente", line), line);
        return s;
    }
    if (t->kind == GMB_TOK_Run || gmb_p_tok_is_soft_keyword(t, "RUN")) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_RUN, line);
        s->win_name = team;
        s->channel = gmb_p_parse_expr(p);
        return s;
    }
    /* TEAM <nome> BROADCAST <msg> — un messaggio a tutti i membri (TeamBroadcast.bas) */
    if (t->kind == GMB_TOK_Broadcast) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_BROADCAST, line);
        s->win_name = team;
        s->channel = gmb_p_parse_expr(p);
        return s;
    }
    /* TEAM <nome> REDEFINE <definition> — imposta di nuovo la lista membri (TeamBroadcast.bas) */
    if (t->kind == GMB_TOK_Redefine) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_REDEFINE, line);
        s->win_name = team;
        s->channel = gmb_p_parse_expr(p);
        return s;
    }
    if (t->kind == GMB_TOK_Set) {
        gmb_p_adv(p);
        t = gmb_p_cur(p);
        if (t && t->kind == GMB_TOK_Role) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETROLE, line);
            s->win_name = team;
            s->channel = gmb_expr_str(gmb_p_ident(p, "Atteso nome agente", line), line);
            s->send_value = gmb_expr_str(gmb_p_ident(p, "Atteso ruolo (COORDINATOR, WORKER, CRITIC, OBSERVER)", line), line);
            return s;
        }
        if (t && t->kind == GMB_TOK_Strategy) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETSTRATEGY, line);
            s->win_name = team;
            s->send_value = gmb_expr_str(gmb_p_ident(p, "Attesa strategia (SEQUENTIAL, ROUNDROBIN, COORDINATOR)", line), line);
            return s;
        }
        if (t && t->kind == GMB_TOK_Coordinator) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETCOORDINATOR, line);
            s->win_name = team;
            s->channel = gmb_expr_str(gmb_p_ident(p, "Atteso nome agente", line), line);
            return s;
        }
        if (t && t->kind == GMB_TOK_Recursion) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETRECURSION, line);
            s->win_name = team;
            s->channel = gmb_p_parse_expr(p);
            return s;
        }
        if (t && t->kind == GMB_TOK_Latent) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETLATENT, line);
            s->win_name = team;
            s->channel = gmb_expr_str(gmb_p_ident(p, "Attesa modalita' (text|embed|native)", line), line);
            return s;
        }
        if (t && t->kind == GMB_TOK_Link) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETLINK, line);
            s->win_name = team;
            s->channel = gmb_expr_str(gmb_p_ident(p, "Atteso tipo link (inner|outer|distill)", line), line);
            if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo il tipo link", line); return s; }
            s->send_value = gmb_p_parse_expr(p);
            return s;
        }
        if (t && t->kind == GMB_TOK_Thoughts) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETTHOUGHTS, line);
            s->win_name = team;
            s->channel = gmb_p_parse_expr(p);
            return s;
        }
        if (t && t->kind == GMB_TOK_Dag) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETDAG, line);
            s->win_name = team;
            s->channel = gmb_p_parse_expr(p);            /* d */
            if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo d_max", line); return s; }
            s->send_value = gmb_p_parse_expr(p);          /* l */
            if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo l_max", line); return s; }
            s->cond_mutex = gmb_p_parse_expr(p);          /* n */
            return s;
        }
        if (t && t->kind == GMB_TOK_Complexity) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETCOMPLEXITY, line);
            s->win_name = team;
            s->channel = gmb_p_parse_expr(p);
            return s;
        }
        if (t && t->kind == GMB_TOK_Reflection) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETREFLECTION, line);
            s->win_name = team;
            s->channel = gmb_expr_str(gmb_p_ident(p, "Attesa modalita' (intra|inter|both|off)", line), line);
            return s;
        }
        if (t && t->kind == GMB_TOK_Critic) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETCRITIC, line);
            s->win_name = team;
            s->channel = gmb_expr_str(gmb_p_ident(p, "Attese persone critiche (spec|edge|logic)", line), line);
            if (!gmb_p_match(p, GMB_TOK_Comma)) { gmb_p_set_err(p, "Attesa ',' dopo le persone critiche", line); return s; }
            /* judge: nel canone è un nome agente (identificatore semplice → StringExpr,
             * parità Parser.Stmt.Team.cs §9.5); solo un identificatore nudo è convertito,
             * un'espressione resta tale. */
            s->send_value = gmb_p_parse_expr(p);
            if (s->send_value && s->send_value->kind == GMB_EXPR_VAR)
                s->send_value = gmb_expr_str(s->send_value->var, line);
            return s;
        }
        if (t && t->kind == GMB_TOK_Prune) {
            gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_TEAM_SETPRUNE, line);
            s->win_name = team;
            s->channel = gmb_p_parse_expr(p);
            return s;
        }
        free(team);
        gmb_p_set_err(p, "Atteso SET ROLE/STRATEGY/COORDINATOR/RECURSION/LATENT/LINK/THOUGHTS/DAG/COMPLEXITY/REFLECTION/CRITIC/PRUNE", line);
        return NULL;
    }
    free(team);
    gmb_p_set_err(p, "Atteso CREATE, ADD, REMOVE, RUN o SET dopo TEAM", line);
    return NULL;
}
