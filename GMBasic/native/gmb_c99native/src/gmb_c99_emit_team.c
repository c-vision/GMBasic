/* gmb_c99_emit_team.c — emitter C99 del dominio TEAM (statement GMB_STMT_TEAM_*), §0quater.
 * UN handler per il dominio (st_team) speculare a st_agent: ogni statement emette la
 * chiamata al runtime C condiviso gmb_team_* (native/gmb_runtime_c/src/gmb_team.c, §0bis:
 * UNA implementazione, gli altri backend chiamano le stesse API).
 *
 * Campi del parser (gmb_parser_stmt_team.c): win_name (char* nome team),
 * channel/send_value/cond_mutex (gmb_expr_t* argomenti).
 * TEAM RUN ritorna char* (risultato task) → free del temporaneo dopo l'uso statement.
 */
#include "gmb_c99_emit_internal.h"
#include <string.h>

/* helper: emette l'operando come stringa `.data` (pattern st_agent) */
static void tm_str(ctx_t* c, gmb_expr_t* e) {
    if (e) { emit_str_operand(c, e); sb_put(&c->out, ".data"); }
    else sb_put(&c->out, "gmb_string_alloc(\"\").data");
}

/* helper: emette l'operando numerico (channel/send_value/cond_mutex numerici) */
static void tm_num(ctx_t* c, gmb_expr_t* e) {
    if (e) emit_expr(c, e);
    else sb_put(&c->out, "0");
}

/* helper: emette il nome team (win_name) come literal stringa C */
static void tm_team_lit(ctx_t* c, const char* name) {
    sb_put(&c->out, "\"");
    if (name) for (const char* p = name; *p; p++) {
        if (*p == '"' || *p == '\\') sb_put(&c->out, "\\");
        sb_printf(&c->out, "%c", *p);
    }
    sb_put(&c->out, "\"");
}

void st_team(ctx_t* c, gmb_stmt_t* s) {
    const char* fn = NULL;
    switch (s->kind) {
    case GMB_STMT_TEAM_CREATE:         fn = "gmb_team_create"; break;
    case GMB_STMT_TEAM_SAVE:           fn = "gmb_team_save"; break;
    case GMB_STMT_TEAM_LOAD:           fn = "gmb_team_load"; break;
    case GMB_STMT_TEAM_ADDAGENT:       fn = "gmb_team_add_agent"; break;
    case GMB_STMT_TEAM_REMOVEAGENT:    fn = "gmb_team_remove_agent"; break;
    case GMB_STMT_TEAM_SETROLE:        fn = "gmb_team_set_role"; break;
    case GMB_STMT_TEAM_SETSTRATEGY:    fn = "gmb_team_set_strategy"; break;
    case GMB_STMT_TEAM_SETCOORDINATOR: fn = "gmb_team_set_coordinator"; break;
    case GMB_STMT_TEAM_SETRECURSION:   fn = "gmb_team_set_recursion"; break;
    case GMB_STMT_TEAM_SETLATENT:      fn = "gmb_team_set_latent"; break;
    case GMB_STMT_TEAM_SETLINK:        fn = "gmb_team_set_link"; break;
    case GMB_STMT_TEAM_SETTHOUGHTS:    fn = "gmb_team_set_thoughts"; break;
    case GMB_STMT_TEAM_SETDAG:         fn = "gmb_team_set_dag"; break;
    case GMB_STMT_TEAM_SETCOMPLEXITY:  fn = "gmb_team_set_complexity"; break;
    case GMB_STMT_TEAM_SETREFLECTION:  fn = "gmb_team_set_reflection"; break;
    case GMB_STMT_TEAM_SETCRITIC:      fn = "gmb_team_set_critic"; break;
    case GMB_STMT_TEAM_SETPRUNE:       fn = "gmb_team_set_prune"; break;
    case GMB_STMT_TEAM_TRAININNER:     fn = "gmb_team_train_inner"; break;
    case GMB_STMT_TEAM_TRAINOUTER:     fn = "gmb_team_train_outer"; break;
    case GMB_STMT_TEAM_TRAINTHINKING:  fn = "gmb_team_train_thinking"; break;
    case GMB_STMT_TEAM_TRAINACTORCRITIC: fn = "gmb_team_train_actor_critic"; break;
    case GMB_STMT_TEAM_DISTILLTHINKING: fn = "gmb_team_distill_thinking"; break;
    case GMB_STMT_TEAM_MESSAGE:        fn = "gmb_team_message"; break;
    case GMB_STMT_TEAM_BROADCAST:      fn = "gmb_team_broadcast"; break;
    case GMB_STMT_TEAM_REDEFINE:       fn = "gmb_team_redefine"; break;
    default: fn = NULL; break;
    }

    /* TEAM RUN: char* gmb_team_run(team, task) — il ritorno va liberato (statement: scarto) */
    if (s->kind == GMB_STMT_TEAM_RUN) {
        ind(c);
        sb_put(&c->out, "{ char* __team_run_r = gmb_team_run(");
        tm_team_lit(c, s->win_name);
        sb_put(&c->out, ", ");
        tm_str(c, s->channel);
        sb_put(&c->out, "); (void)__team_run_r; free(__team_run_r); }\n");
        return;
    }

    if (!fn) { /* statement TEAM non mappato: errore esplicito (mai silenzioso, §0bis) */
        ind(c);
        sb_printf(&c->out, "/* TEAM statement kind=%d non supportato dall'emitter */\n", (int)s->kind);
        return;
    }

    ind(c);
    sb_printf(&c->out, "(void)%s(", fn);
    tm_team_lit(c, s->win_name);

    switch (s->kind) {
    /* (team, agent) */
    case GMB_STMT_TEAM_ADDAGENT:
    case GMB_STMT_TEAM_REMOVEAGENT:
    case GMB_STMT_TEAM_SETCOORDINATOR:
        sb_put(&c->out, ", "); tm_str(c, s->channel);
        break;
    /* (team, agent, role) */
    case GMB_STMT_TEAM_SETROLE:
        sb_put(&c->out, ", "); tm_str(c, s->channel);
        sb_put(&c->out, ", "); tm_str(c, s->send_value);
        break;
    /* (team, mode) — mode stringa */
    case GMB_STMT_TEAM_SETSTRATEGY:
    case GMB_STMT_TEAM_SETLATENT:
    case GMB_STMT_TEAM_SETREFLECTION:
        sb_put(&c->out, ", "); tm_str(c, s->send_value ? s->send_value : s->channel);
        break;
    /* (team, int) */
    case GMB_STMT_TEAM_SETRECURSION:
    case GMB_STMT_TEAM_SETTHOUGHTS:
    case GMB_STMT_TEAM_SETPRUNE:
        sb_put(&c->out, ", "); tm_num(c, s->channel);
        break;
    /* (team, double) */
    case GMB_STMT_TEAM_SETCOMPLEXITY:
        sb_put(&c->out, ", (double)("); tm_num(c, s->channel); sb_put(&c->out, ")");
        break;
    /* (team, which, cfg) */
    case GMB_STMT_TEAM_SETLINK:
        sb_put(&c->out, ", "); tm_str(c, s->channel);
        sb_put(&c->out, ", "); tm_str(c, s->send_value);
        break;
    /* (team, personas, judge) */
    case GMB_STMT_TEAM_SETCRITIC:
        sb_put(&c->out, ", "); tm_str(c, s->channel);
        sb_put(&c->out, ", "); tm_str(c, s->send_value);
        break;
    /* (team, int, int, int) */
    case GMB_STMT_TEAM_SETDAG:
        sb_put(&c->out, ", (int)("); tm_num(c, s->channel); sb_put(&c->out, ")");
        sb_put(&c->out, ", (int)("); tm_num(c, s->send_value); sb_put(&c->out, ")");
        sb_put(&c->out, ", (int)("); tm_num(c, s->cond_mutex); sb_put(&c->out, ")");
        break;
    /* (team, dataset) */
    case GMB_STMT_TEAM_TRAINOUTER:
    case GMB_STMT_TEAM_TRAINTHINKING:
    case GMB_STMT_TEAM_TRAINACTORCRITIC:
    case GMB_STMT_TEAM_DISTILLTHINKING:
    case GMB_STMT_TEAM_MESSAGE:
    case GMB_STMT_TEAM_BROADCAST:
    case GMB_STMT_TEAM_REDEFINE:
        sb_put(&c->out, ", "); tm_str(c, s->channel);
        break;
    /* (team, agent, dataset) */
    case GMB_STMT_TEAM_TRAININNER:
        sb_put(&c->out, ", "); tm_str(c, s->channel);
        sb_put(&c->out, ", "); tm_str(c, s->send_value);
        break;
    /* TEAM SAVE/LOAD: (team, path). Il canone C# usa config/recursivemas/<n>/<n>.team.json
     * (DefaultMarketplacePath), ma TUTTI i backend C — VM/C syscall c100/c101
     * (TeamSaveStmt/TeamLoadStmt) — convergono su "<nome>.team.json" relativo alla CWD del
     * programma: il C99Native allinea ai backend C (parità C↔C, la divergenza vs IL sul
     * path di default è già nota e dichiarata). Il path è UN literal concatenato (il team
     * name è noto a parse-time in win_name). */
    case GMB_STMT_TEAM_SAVE:
    case GMB_STMT_TEAM_LOAD:
        sb_put(&c->out, ", \"");
        if (s->win_name) sb_put(&c->out, s->win_name);
        sb_put(&c->out, ".team.json\"");
        break;
    default:
        break;
    }
    sb_put(&c->out, ");\n");
}

/* ── Funzioni builtin AI.TEAMLIST()/AI.TEAMAGENTS(team) — ritornano stringa ──
 * Il parser consegna il nome NUDO (TEAMLIST) + ns (AI) + tk dedicato; il dispatch
 * è per TokenKind (regola d'oro). Runtime: gmb_team_list_str/gmb_team_agents_str
 * (char* malloc'd) → incapsulati in gmb_string_alloc, il caller libera col suo
 * gestore (pattern del dominio: gmb_string_alloc(gmb_*_str())). */
int emit_bf_team(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk, gmb_expr_t** args, int nargs, int line) {
    (void)name; (void)ns; (void)line;
    if (tk == GMB_TOK_TeamList) {
        /* parità coll'emitter C#: gmb_string_alloc(gmb_team_list_str()) — il char*
         * malloc'd è incapsulato da gmb_string_alloc (copia); il free del temporaneo
         * spetta al chiamante runtime (nota: stesso comportamento dell'emitter C#). */
        sb_put(&c->out, "gmb_string_alloc(gmb_team_list_str())");
        return 1;
    }
    if (tk == GMB_TOK_TeamAgents && nargs >= 1) {
        sb_put(&c->out, "gmb_string_alloc(gmb_team_agents_str(");
        emit_str_operand(c, args[0]);
        sb_put(&c->out, ".data))");
        return 1;
    }
    /* getter del dominio (parità emitter C# NsKindRow, TokenKind → runtime gmb_team_*) */
    if (nargs >= 1 && tk == GMB_TOK_TeamRecursion) {
        sb_put(&c->out, "gmb_team_get_recursion(("); emit_str_operand(c, args[0]); sb_put(&c->out, ").data)"); return 1;
    }
    if (nargs >= 1 && tk == GMB_TOK_TeamLatent) {
        sb_put(&c->out, "gmb_string_alloc(gmb_team_get_latent(("); emit_str_operand(c, args[0]); sb_put(&c->out, ").data))"); return 1;
    }
    if (nargs >= 1 && tk == GMB_TOK_TeamTrainStatus) {
        sb_put(&c->out, "gmb_string_alloc(gmb_team_train_status_str(("); emit_str_operand(c, args[0]); sb_put(&c->out, ").data))"); return 1;
    }
    if (nargs >= 1 && tk == GMB_TOK_TeamLinkInfo) {
        sb_put(&c->out, "gmb_string_alloc(gmb_team_link_info_str(("); emit_str_operand(c, args[0]); sb_put(&c->out, ").data))"); return 1;
    }
    if (nargs >= 1 && tk == GMB_TOK_TeamRuntimeStats) {
        sb_put(&c->out, "gmb_string_alloc(gmb_team_runtime_stats_str(("); emit_str_operand(c, args[0]); sb_put(&c->out, ").data))"); return 1;
    }
    /* ── TEAMRUNSTART / TEAMRUNWAIT / TEAMONSTREAM (gruppo B triage emitter C#) ──
     * Runtime condiviso gmb_team_run_start/wait/on_stream (stessa semantica oracolo C#);
     * char* malloc'd → gmb_string_alloc per le due stringhe (pattern del dominio). */
    if (tk == GMB_TOK_TeamRunStart && nargs >= 2) {
        sb_put(&c->out, "gmb_string_alloc(gmb_team_run_start(("); emit_str_operand(c, args[0]); sb_put(&c->out, ").data, ("); emit_str_operand(c, args[1]); sb_put(&c->out, ").data))"); return 1;
    }
    if (tk == GMB_TOK_TeamRunWait && nargs >= 1) {
        sb_put(&c->out, "gmb_string_alloc(gmb_team_run_wait(("); emit_str_operand(c, args[0]); sb_put(&c->out, ").data))"); return 1;
    }
    if (tk == GMB_TOK_TeamOnStream && nargs >= 1) {
        sb_put(&c->out, "(gmb_double_t)gmb_team_on_stream(("); emit_str_operand(c, args[0]); sb_put(&c->out, ").data)"); return 1;
    }
    return 0;
}
