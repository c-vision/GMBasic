/* native/gmb_compiler_c/src/parser/gmb_parser_stmt_agent.c — speculare a
 * Parser.Stmt.AiAgent.cs / Parser.Stmt.Ai.cs (ParseAiAgentStatement):
 *   AI AGENT CREATE <nome>
 *   AI AGENT DELETE <nome>
 *   AI AGENT SET <nome>, <chiave> [valore]
 * I comandi agent scrivono/leggono agenti registrati nel runtime C (gmb_agent_*).
 *
 * Rappresentazione: CREATE/DELETE → win_name (nome letterale).
 * SET → go_args[0]=nome, go_args[1]=chiave, go_args[2]=valore (tutti espressioni; una
 * stringa letterale è gmb_expr_str). Così l'emitter gestisce un solo schema.
 *
 * Casi nel wrapper A1 (`include/Agent.inc`, dentro property SET di classe):
 *   AI AGENT SET SELF._name, "model", value      ← nome=campo, chiave=stringa, valore=param
 *   AI AGENT SET SELF._name, "temperature", &(STR$(value))
 *   AI AGENT SET SELF._name, "model", &s
 * `value` è il parametro del setter → token keyword GMB_TOK_Value (variabile, NON letterale).
 * `SELF._name` è un accesso a campo. Chiave/valore non quotati → stringa letterale (C#). */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static const gmb_tokenkind_t gmb_tk_nx_term_toks[] = { GMB_TOK_LeftParen, GMB_TOK_LeftBracket, GMB_TOK_Dot, GMB_TOK_Arrow };
static int gmb_tk_nx_term_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_tk_nx_term_toks) / sizeof(gmb_tk_nx_term_toks[0]); i++)
        if (gmb_tk_nx_term_toks[i] == k) return 1;
    return 0;
}
static const gmb_tokenkind_t gmb_tk_agent_keyend_toks[] = { GMB_TOK_Newline, GMB_TOK_Colon, GMB_TOK_Eof };
static int gmb_tk_agent_keyend_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_tk_agent_keyend_toks) / sizeof(gmb_tk_agent_keyend_toks[0]); i++)
        if (gmb_tk_agent_keyend_toks[i] == k) return 1;
    return 0;
}
static const gmb_tokenkind_t gmb_agent_val_toks[] = {
    GMB_TOK_Value, GMB_TOK_Ampersand, GMB_TOK_Eval, GMB_TOK_Number,
    GMB_TOK_StringLiteral, GMB_TOK_On, GMB_TOK_Off
};
static int gmb_agent_val_in(gmb_tokenkind_t k)
{
    for (unsigned i = 0; i < sizeof(gmb_agent_val_toks) / sizeof(gmb_agent_val_toks[0]); i++)
        if (gmb_agent_val_toks[i] == k) return 1;
    return 0;
}

static void gmb_p_agent_add_arg(gmb_stmt_t* s, gmb_expr_t* e)
{
    int cap = s->n_go_args + 1;
    s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*));
    s->go_args[s->n_go_args++] = e;
}

/* TRUE se il token corrente inizia un'espressione (variabile `value`, macro `&`, EVAL,
 * numero, stringa quotata, oppure identifier seguito da `(`/`.`/`[`/`->`). */
static bool gmb_p_agent_is_expr(gmb_parser_t* p)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return false;
    /* Argomento PARENTESIZZATO `(value)` — la forma usata dalle PROPERTY di include/Agent.inc
     * (`AI AGENT SET SELF._name "model" (value)`): senza questo il `(` diventava la stringa
     * "(" e le PropSet scrivevano "(" invece del valore (AgentObject). */
    if (t->kind == GMB_TOK_LeftParen) return true;
    if (gmb_agent_val_in(t->kind))
        return true;
    if ((t->kind == GMB_TOK_Identifier || t->kind == GMB_TOK_Self) && gmb_p_peek(p, 1)) {
        gmb_tokenkind_t nx = gmb_p_peek(p, 1)->kind;
        if (gmb_tk_nx_term_in(nx)) return true;
    }
    /* identifier nudo DICHIARATO (DIM/param) → variabile, non letterale-nome (bug P4) */
    if (t->kind == GMB_TOK_Identifier && gmb_p_is_declared_tok(p, t)) return true;
    return false;
}

/* Termine: stringa quotata → contenuto; identifier/keyword nudo → stringa letterale
 * (parità ParseAiValueToken); altrimenti espressione completa. La macro `&x` usata come
 * VALORE del SET (es. `AI AGENT SET ..., &s` del wrapper A1) è un workaround del parser C#
 * (gli identifier nudi erano trattati come letterali): qui il C99Native gestisce già le
 * variabili come espressioni, quindi `&x` si riduce a usare x direttamente (niente eval). */
static gmb_expr_t* gmb_p_agent_term(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->length == 0) return NULL;
    /* `&espr`: macro Clipper. In posizione di valore del SET → usa l'espressione interna
     * (una variabile `&s` = il valore di s), NON la call `_macro_` che l'emitter C99 non
     * supporta. */
    if (t->kind == GMB_TOK_Ampersand || (t->kind == GMB_TOK_Eval && gmb_p_peek(p, 1)
        && gmb_p_peek(p, 1)->kind == GMB_TOK_LeftParen)) {
        gmb_p_adv(p);   /* & */
        if (gmb_p_check(p, GMB_TOK_LeftParen)) gmb_p_adv(p);   /* EVAL( */
        gmb_expr_t* inner = gmb_p_parse_expr(p);
        if (gmb_p_check(p, GMB_TOK_RightParen)) gmb_p_adv(p);  /* ) */
        return inner;
    }
    if (gmb_p_agent_is_expr(p)) {
        /* `value`/SELF._name/numero/stringa → espressione (variabile, campo, macro) */
        return gmb_p_parse_expr(p);
    }
    /* identifier nudo o keyword → stringa letterale */
    char buf[512]; int l = t->length < 511 ? t->length : 511;
    memcpy(buf, t->lexeme, (size_t)l); buf[l] = 0;
    size_t bl = strlen(buf);
    if (bl >= 2 && buf[0] == '"' && buf[bl - 1] == '"') { memmove(buf, buf + 1, bl - 2); buf[bl - 2] = 0; }
    gmb_p_adv(p);
    return gmb_expr_str(buf, line);
}

static void gmb_p_agent_to_eol(gmb_parser_t* p)
{
    while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)
        && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
}

/* CREATE/DELETE: nome = primo termine. Tenuto in go_args[0] (schema unico per
 * l'emitter) e copiato in win_name come stringa letterale per chi lo vuole. */
static gmb_stmt_t* gmb_p_agent_name_stmt(gmb_parser_t* p, int line, gmb_stmt_kind_t kind)
{
    gmb_stmt_t* s = gmb_stmt_new(kind, line);
    gmb_expr_t* name = gmb_p_agent_term(p, line);
    if (name) {
        gmb_p_agent_add_arg(s, name);
        if (name->kind == GMB_EXPR_STR) s->win_name = strdup(name->str ? name->str : "");
        else if (name->kind == GMB_EXPR_VAR) s->win_name = strdup(name->var ? name->var : "");
    }
    gmb_p_agent_to_eol(p);
    return s;
}

gmb_stmt_t* gmb_p_parse_agent(gmb_parser_t* p, int line)
{
    /* il chiamante ha consumato 'AI' + token 'AGENT'; qui il token corrente è il verbo:
     * CREATE | DELETE | SET */
    const gmb_token_t* v = gmb_p_cur(p);
    if (!v) return NULL;

    if (v->kind == GMB_TOK_Create || gmb_p_tok_is_keyword(v, "Create")) {
        gmb_p_adv(p);
        return gmb_p_agent_name_stmt(p, line, GMB_STMT_AGENT_CREATE);
    }
    if (v->kind == GMB_TOK_Delete || gmb_p_tok_is_keyword(v, "Delete")) {
        gmb_p_adv(p);
        return gmb_p_agent_name_stmt(p, line, GMB_STMT_AGENT_DELETE);
    }
    /* REGISTER TOOL: AI AGENT REGISTER TOOL <agent>, <tool> → gmb_agent_register_tool.
     * TRACE ON/OFF: flag trace agente. */
    if (v->kind == GMB_TOK_Register || gmb_p_tok_is_keyword(v, "Register")) {
        gmb_p_adv(p);
        const gmb_token_t* tt = gmb_p_cur(p);
        if (!tt || tt->kind != GMB_TOK_AiSubTool) { gmb_p_set_err(p, "Atteso TOOL dopo AI AGENT REGISTER", line); return NULL; }
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AGENT_REGISTER_TOOL, line);
        gmb_expr_t* ag = gmb_p_agent_term(p, line); if (ag) gmb_p_agent_add_arg(s, ag);
        gmb_p_match(p, GMB_TOK_Comma);
        gmb_expr_t* tool = gmb_p_agent_term(p, line); if (tool) gmb_p_agent_add_arg(s, tool);
        gmb_p_agent_to_eol(p);
        return s;
    }
    if (v->kind == GMB_TOK_AiAgentTrace || v->kind == GMB_TOK_AiSubTrace || gmb_p_tok_is_keyword(v, "Trace")) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AGENT_TRACE, line);
        const gmb_token_t* on = gmb_p_cur(p);
        if (on && (on->kind == GMB_TOK_AiAgentTraceOn || on->kind == GMB_TOK_AiAgentTraceOff || gmb_p_tok_is_keyword(on, "On") || gmb_p_tok_is_keyword(on, "Off"))) {
            s->win_name = strdup((on->kind == GMB_TOK_AiAgentTraceOn || gmb_p_tok_is_keyword(on, "On")) ? "ON" : "OFF");
            gmb_p_adv(p);
        }
        gmb_p_agent_to_eol(p);
        return s;
    }
    /* ASK STREAM <nome>, <prompt> INTO <Sub>: streaming completion → callback SUB per-token
     * (GPM). go_args = [nome, prompt, callbackSub]. */
    if (gmb_p_tok_is_keyword(v, "Ask") || (v && v->kind == GMB_TOK_AiAgentAsk)) {
        const gmb_token_t* nx = gmb_p_peek(p, 1);
        if (nx && (gmb_p_tok_is_keyword(nx, "Stream") || nx->kind == GMB_TOK_AiAgentAskStream)) {
            gmb_p_adv(p); gmb_p_adv(p);
            gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AGENT_ASKSTREAM, line);
            gmb_expr_t* nm = gmb_p_agent_term(p, line); if (nm) gmb_p_agent_add_arg(s, nm);
            gmb_p_match(p, GMB_TOK_Comma);
            gmb_expr_t* pr = gmb_p_agent_term(p, line); if (pr) gmb_p_agent_add_arg(s, pr);
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Into")) gmb_p_adv(p);
            const gmb_token_t* cb = gmb_p_cur(p);
            if (cb && cb->length > 0 && (cb->kind == GMB_TOK_Identifier || cb->kind == GMB_TOK_Value)) {
                int l2 = cb->length < 127 ? cb->length : 127;
                char cn[128]; memcpy(cn, cb->lexeme, (size_t)l2); cn[l2] = 0;
                gmb_p_agent_add_arg(s, gmb_expr_str(strdup(cn), line));
                gmb_p_adv(p);
            }
            gmb_p_agent_to_eol(p); return s;
        }
    }
    /* SERVE <port> / STOP SERVER: server HTTP agent (AI AGENT SERVE). */
    if (gmb_p_tok_is_keyword(v, "Serve") || (v && v->kind == GMB_TOK_AiSubServe)) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AGENT_SERVE, line);
        gmb_expr_t* port = gmb_p_agent_term(p, line); if (port) gmb_p_agent_add_arg(s, port);
        gmb_p_agent_to_eol(p); return s;
    }
    if (gmb_p_tok_is_keyword(v, "Stop") && gmb_p_tok_is_keyword(gmb_p_peek(p, 1), "Server")) {
        gmb_p_adv(p); gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AGENT_STOP_SERVER, line);
        gmb_p_agent_to_eol(p); return s;
    }
    /* CODE: AI AGENT CODE <task>, <path> → codegen via LLM, scrive file (gmb_agent_code). */
    if ((v && v->kind == GMB_TOK_AiAgentCode) || gmb_p_tok_is_soft_keyword(v, "Code")) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AGENT_CODE, line);
        gmb_expr_t* task = gmb_p_agent_term(p, line); if (task) gmb_p_agent_add_arg(s, task);
        gmb_p_match(p, GMB_TOK_Comma);
        gmb_expr_t* path = gmb_p_agent_term(p, line); if (path) gmb_p_agent_add_arg(s, path);
        gmb_p_agent_to_eol(p); return s;
    }
    /* SAVE/LOAD: AI AGENT SAVE <nome>, <path> → persistenza agente su file JSON
     * (gmb_agent_save/load del runtime C). go_args = [nome, path]. */
    if (gmb_p_tok_is_keyword(v, "Save") || gmb_p_tok_is_keyword(v, "Load")) {
        bool is_save = gmb_p_tok_is_keyword(v, "Save");
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(is_save ? GMB_STMT_AGENT_SAVE : GMB_STMT_AGENT_LOAD, line);
        gmb_expr_t* name = gmb_p_agent_term(p, line);
        if (name) gmb_p_agent_add_arg(s, name);
        gmb_p_match(p, GMB_TOK_Comma);
        gmb_expr_t* path = gmb_p_agent_term(p, line);
        if (path) gmb_p_agent_add_arg(s, path);
        gmb_p_agent_to_eol(p);
        return s;
    }
    /* SEND/RECEIVE: coda messaggi agent (parity C# ParseAiAgentSend/Receive — go_args col
     * nome agente/target come espressione). SEND <to>,<task>,<input>[,<payment>] →
     * AGENT_SEND con go_args=[to,task,input,(payment)]. RECEIVE <agent> INTO <var> →
     * AGENT_RECEIVE con go_args=[agent,var]. */
    if (gmb_p_tok_is_keyword(v, "Send") || gmb_p_tok_is_keyword(v, "Receive")) {
        bool is_send = gmb_p_tok_is_keyword(v, "Send");
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(is_send ? GMB_STMT_AGENT_SEND : GMB_STMT_AGENT_RECEIVE, line);
        gmb_expr_t* a0 = gmb_p_agent_term(p, line);
        if (a0) gmb_p_agent_add_arg(s, a0);
        if (is_send) {
            gmb_p_match(p, GMB_TOK_Comma);
            gmb_expr_t* task = gmb_p_agent_term(p, line); if (task) gmb_p_agent_add_arg(s, task);
            gmb_p_match(p, GMB_TOK_Comma);
            gmb_expr_t* input = gmb_p_agent_term(p, line); if (input) gmb_p_agent_add_arg(s, input);
            if (gmb_p_match(p, GMB_TOK_Comma)) {
                gmb_expr_t* pay = gmb_p_agent_term(p, line); if (pay) gmb_p_agent_add_arg(s, pay);
            } else {
                gmb_p_agent_add_arg(s, gmb_expr_num(0.0, line)); /* payment default 0 */
            }
        } else {
            /* RECEIVE <agent> INTO <var>: il target è una VARIABILE da assegnare (l'emitter
             * emette <var> = gmb_agent_queue_receive_s(agent)), non una stringa letterale. */
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Into")) gmb_p_adv(p);
            const gmb_token_t* vt = gmb_p_cur(p);
            if (vt && vt->length > 0 && (vt->kind == GMB_TOK_Identifier || vt->kind == GMB_TOK_Value)) {
                int l2 = vt->length < 127 ? vt->length : 127;
                char vn[128]; memcpy(vn, vt->lexeme, (size_t)l2); vn[l2] = 0;
                gmb_p_agent_add_arg(s, gmb_expr_var(strdup(vn), line));
                gmb_p_adv(p);
            } else {
                gmb_expr_t* var = gmb_p_agent_term(p, line); if (var) gmb_p_agent_add_arg(s, var);
            }
        }
        gmb_p_agent_to_eol(p);
        return s;
    }
    /* PAUSE/RESUME/STOP (queue-op, single agent): parity C# ParseAiAgentQueueOp. */
    if (gmb_p_tok_is_keyword(v, "Pause") || gmb_p_tok_is_keyword(v, "Resume") || gmb_p_tok_is_keyword(v, "Stop")) {
        bool is_pause = gmb_p_tok_is_keyword(v, "Pause");
        bool is_resume = gmb_p_tok_is_keyword(v, "Resume");
        gmb_p_adv(p);
        return gmb_p_agent_name_stmt(p, line, is_pause ? GMB_STMT_AGENT_PAUSE : (is_resume ? GMB_STMT_AGENT_RESUME : GMB_STMT_AGENT_STOP));
    }
    if (v->kind == GMB_TOK_Set || gmb_p_tok_is_keyword(v, "Set")) {
        gmb_p_adv(p);
        gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AGENT_SET, line);
        /* nome agente (può essere espressione SELF._name o stringa) */
        gmb_expr_t* name = gmb_p_agent_term(p, line);
        if (name) gmb_p_agent_add_arg(s, name);
        /* Sintassi canonica (parità del parser C#): virgola opzionale dopo il nome e/o clausola
         * 'KEY' esplicita. Forme: 'AI AGENT SET "n", k v' | '..."n", k, v' | '..."n" KEY k VALUE v'. */
        gmb_p_match(p, GMB_TOK_Comma);
        { const gmb_token_t* k0 = gmb_p_cur(p);
          if (k0 && gmb_p_tok_is_keyword(k0, "Key")) gmb_p_adv(p); }
        const gmb_token_t* kt = gmb_p_cur(p);
        if (!kt || gmb_tk_agent_keyend_in(kt->kind)) {
            gmb_p_set_err(p, "Attesa chiave dopo AI AGENT SET <nome>", line);
            return s;
        }
        /* La CHIAVE è un NOME (parità C# ParseAiKeyToken), non un'espressione: con
         * `AI AGENT SET (name) systemPrompt (v)` il termine `systemPrompt` veniva letto
         * come chiamata di funzione (l'identificatore è "expr"), e l'emitter falliva con
         * "simbolo non trovato: systemPrompt". Un identificatore qui è sempre letterale. */
        gmb_expr_t* key;
        if (kt->kind == GMB_TOK_Identifier) {
            char kb[512];
            int kl = kt->length < 511 ? kt->length : 511;
            memcpy(kb, kt->lexeme, (size_t)kl); kb[kl] = 0;
            gmb_p_adv(p);
            key = gmb_expr_str(kb, line);
        } else {
            key = gmb_p_agent_term(p, line);
        }
        if (key) gmb_p_agent_add_arg(s, key);
        /* virgola opzionale dopo la chiave + clausola 'VALUE' esplicita */
        gmb_p_match(p, GMB_TOK_Comma);
        { const gmb_token_t* v0 = gmb_p_cur(p);
          if (v0 && gmb_p_tok_is_keyword(v0, "Value")) gmb_p_adv(p); }
        /* valore: opzionale */
        if (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon)
            && !gmb_p_at_end(p)) {
            /* ON/OFF → stringa "ON"/"OFF" (parità C# ParseAiValueToken): se trattati come
             * espressione costante finivano come numero (SET sessionstreaming ON → "0"). */
            const gmb_token_t* vt = gmb_p_cur(p);
            gmb_expr_t* val;
            if (vt && (vt->kind == GMB_TOK_On || vt->kind == GMB_TOK_Off)) {
                gmb_p_adv(p);
                val = gmb_expr_str(vt->kind == GMB_TOK_On ? "ON" : "OFF", line);
            } else {
                val = gmb_p_agent_term(p, line);
            }
            if (val) gmb_p_agent_add_arg(s, val);
        }
        gmb_p_agent_to_eol(p);
        return s;
    }

    gmb_p_set_err(p, "Atteso CREATE, DELETE o SET dopo AI AGENT", line);
    return NULL;
}
