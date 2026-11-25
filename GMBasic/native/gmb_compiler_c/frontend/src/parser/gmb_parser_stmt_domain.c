/* native/gmb_runtime_c/src/parser/gmb_parser_stmt_domain.c — comandi di dominio generici
 * (speculari a Parser.Stmt.Dbf.cs/Isam.cs/Sql.cs/Ledger.cs/Rdd.cs/Speed.cs/Matrix.cs/
 * Vector.cs/Torch.cs/Ai.cs/Network.cs/...): sintassi legacy a spazio
 *   NAMESPACE VERBO <termini separati da virgola o clausole keyword>...
 * L'intera riga viene catturata senza perdite: verbo + termini (keyword/identificatori/
 * numeri/stringhe). La validazione semantica per dominio resta C#; la sintassi è 1:1. */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

static gmb_expr_t* gmb_p_term_from_token(const gmb_token_t* t, int line)
{
    if (!t || t->length <= 0) return NULL;
    if (t->kind == GMB_TOK_Number) {
        char buf[64]; int l = t->length < 63 ? t->length : 63;
        memcpy(buf, t->lexeme, (size_t)l); buf[l] = 0;
        return gmb_expr_num(gmb_p_num_literal(buf), line);
    }
    if (t->kind == GMB_TOK_StringLiteral) {
        char buf[512]; int l = t->length < 511 ? t->length : 511;
        memcpy(buf, t->lexeme, (size_t)l); buf[l] = 0;
        size_t bl = strlen(buf);
        if (bl >= 2 && buf[0] == '"' && buf[bl - 1] == '"') { memmove(buf, buf + 1, bl - 2); buf[bl - 2] = 0; bl -= 2; }
        /* unescape BASIC "" -> " (STESSA regola di gmb_p_parse_primary, riga ~542).
         * Senza, i termini stringa degli statement di dominio conservavano le virgolette
         * raddoppiate: `AI CONTEXT VIEW a, "x""y"` stampava x""y sul C99Native e x"y su
         * IL/PCode (divergenza misurata il 2026-09-16). */
        char out[512]; int o = 0;
        for (size_t i = 0; i < bl && o < 510; i++) {
            if (buf[i] == '"' && i + 1 < bl && buf[i + 1] == '"') { out[o++] = '"'; i++; }
            else out[o++] = buf[i];
        }
        out[o] = 0;
        return gmb_expr_str(out, line);
    }
    gmb_expr_t* e = gmb_expr_var(gmb_p_tok_strdup(t), line);
    /* il kind del termine: per i sotto-verbi di statement (REFRESH/MARKETPLACE/... che sono
     * keyword del Lexer) l'emitter dispatcha su e->tk, MAI su gmb_tokenkind_from_name. */
    if (t->kind != GMB_TOK_Identifier && t->kind != GMB_TOK_Unknown) e->tk = (int)t->kind;
    return e;
}

/* consente di registrare i namespace di dominio da dispatchare qui */
static const gmb_tokenkind_t g_domain_ns[] = {
    GMB_TOK_Dbf, GMB_TOK_Isam, GMB_TOK_Sql, GMB_TOK_Ledger, GMB_TOK_Rdd, GMB_TOK_Speed,
    GMB_TOK_Matrix, GMB_TOK_Vector, GMB_TOK_Torch, GMB_TOK_Ai, GMB_TOK_Llm, GMB_TOK_Gmb, GMB_TOK_Train,
    GMB_TOK_WorkflowStatement,
    GMB_TOK_Http, GMB_TOK_WebSocket, GMB_TOK_Plot, GMB_TOK_Sprite, GMB_TOK_Shape, GMB_TOK_VectorDb,
    GMB_TOK_World,  /* motore 3D (P2): WORLD LOAD|FREE|TICK, SECMOVE, HEIGHT, ANIM,
                     * SCROLL, LIGHTFX, LAMP — verbo per TokenKind, mai per lessema */
    GMB_TOK_Scene,  /* motore 3D (P3, renderer mesh): SCENE RESET|VIEW|TEXTURE|OBJECT|VERTEX|
                     * QUAD|BOX|OBJPOS|CAMERA|RENDER|SAVE — verbo normalizzato in GMB_TOK_Scene* */
    GMB_TOK_Clock   /* tempo a passo fisso (2026-11-20): CLOCK PAUSE|RESUME|SCALE|FPS */
};
#define G_DOMAIN_NS_COUNT ((int)(sizeof(g_domain_ns) / sizeof(g_domain_ns[0])))

bool gmb_p_is_domain_ns(gmb_tokenkind_t k)
{
    for (int i = 0; i < G_DOMAIN_NS_COUNT; i++) if (g_domain_ns[i] == k) return true;
    return false;
}

/* dict literal `{ "k": v, ... }` come termine: CALL "_dict_" con coppie (come parse_primary). */
static gmb_expr_t* gmb_p_parse_dict_lit(gmb_parser_t* p, int line)
{
    gmb_expr_t* kd[64]; gmb_expr_t* vd[64]; int np = 0;
    if (!gmb_p_match(p, GMB_TOK_LeftBrace)) return NULL;
    /* NB: il newline NON chiude un record letterale: i sample lo scrivono su più righe
     * (LEDGER ADD h WITH d, "…" {\n "conto": importo, …\n} INTO id) e la condizione sul
     * Newline lo troncava → "statement non riconosciuto" (verificato con il driver su un file
     * minimo). Il gmb_p_skip_eol interno gestisce le righe vuote. */
    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightBrace)) {
        if (gmb_p_match(p, GMB_TOK_Comma) || gmb_p_match(p, GMB_TOK_Colon)) continue;
        gmb_p_skip_eol(p);
        if (gmb_p_check(p, GMB_TOK_RightBrace)) break;
        /* La CHIAVE di un record letterale è sempre una stringa o un identificatore: leggerla
         * con gmb_p_parse_expr poteva consumare la virgola e la chiave successiva, lasciando
         * coppie corrotte (misurato: seconda coppia con chiave NUM 0 e valore VAR) → l'emissione
         * DBF/LEDGER si fermava. Si legge a livello di TOKEN. */
        gmb_expr_t* ke = NULL;
        {
            const gmb_token_t* kt = gmb_p_cur(p);
            if (kt && (kt->kind == GMB_TOK_StringLiteral || kt->kind == GMB_TOK_Identifier
                       || kt->kind == GMB_TOK_Unknown))
            {
                ke = gmb_p_term_from_token(kt, line);
                gmb_p_adv(p);
            }
        }
        if (!ke) ke = gmb_p_parse_expr(p);
        if (!ke) { gmb_p_adv(p); continue; }
        while (gmb_p_match(p, GMB_TOK_Colon)) {}
        int save_pos = p->pos;
        gmb_expr_t* ve = NULL;
        /* Valore con SEGNO (`-1000.00`, `+5`): l'espressione generica restituisce il token '-'
         * come variabile (misurato: dict [STR][NUM][STR][VAR], kind 2) → il record non si forma a
         * compile time. Qui si legge il numero con il suo segno. */
        {
            const gmb_token_t* sv = gmb_p_cur(p);
            if (sv && (sv->kind == GMB_TOK_Minus || sv->kind == GMB_TOK_Plus))
            {
                int sgn = (sv->kind == GMB_TOK_Minus) ? -1 : 1;
                const gmb_token_t* nv = gmb_p_peek(p, 1);
                if (nv && nv->kind == GMB_TOK_Number)
                {
                    char nb[64]; int l = nv->length < 63 ? nv->length : 63;
                    memcpy(nb, nv->lexeme, (size_t)l); nb[l] = 0;
                    ve = gmb_expr_num(sgn * gmb_p_num_literal(nb), line);
                    gmb_p_adv(p); gmb_p_adv(p);
                }
            }
        }
        if (!ve) ve = gmb_p_parse_expr(p);
        /* Il parse del valore NON deve mangiare la coppia successiva: nei record multi-riga
         * lo faceva (misurato: seconda coppia con chiave NUM 0 e valore VAR → emissione
         * DBF/LEDGER ferma). Se dopo il valore non siamo su ',' o '}', si riavvolge e si
         * legge un termine SINGOLO (con il segno, se c'è). */
        if (ve && !gmb_p_check(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_RightBrace) && !gmb_p_at_end(p))
        {
            gmb_expr_free(ve);
            p->pos = save_pos;
            {
                const gmb_token_t* sv = gmb_p_cur(p);
                int sign = 1;
                if (sv && (sv->kind == GMB_TOK_Minus || sv->kind == GMB_TOK_Plus))
                {
                    sign = (sv->kind == GMB_TOK_Minus) ? -1 : 1;
                    gmb_p_adv(p);
                    sv = gmb_p_cur(p);
                }
                if (sv && sv->kind == GMB_TOK_Number)
                {
                    char nb[64]; int l = sv->length < 63 ? sv->length : 63;
                    memcpy(nb, sv->lexeme, (size_t)l); nb[l] = 0;
                    ve = gmb_expr_num(sign * gmb_p_num_literal(nb), line);
                    gmb_p_adv(p);
                }
                else
                {
                    ve = gmb_p_term_from_token(sv, line);
                    if (ve) gmb_p_adv(p);
                }
            }
        }
        if (!ve) { gmb_expr_free(ke); gmb_p_adv(p); continue; }
        if (np < 64) { kd[np] = ke; vd[np] = ve; np++; }
        else { gmb_expr_free(ke); gmb_expr_free(ve); }
    }
    gmb_p_match(p, GMB_TOK_RightBrace);
    gmb_expr_t** args = (gmb_expr_t**)calloc((size_t)np * 2, sizeof(gmb_expr_t*));
    for (int i = 0; i < np; i++) { args[i * 2] = kd[i]; args[i * 2 + 1] = vd[i]; }
    return gmb_expr_call("_dict_", args, np * 2, line);
}

/* TRAIN FINETUNE|DPO|RESUME: le firme §4 a 3 argomenti posizionali (model, arg2, arg3) sono
 * ripiegate in un dict di opzioni — {"dataset":..,"ckpt":..} | {"pairs":..,"ckpt":..} |
 * {"steps":..} — esattamente come il parser C# (parità 4 backend). Se il 2° arg è già un dict
 * ({opts}, forma storica) non si tocca nulla. */
static void gmb_p_fold_train_opts(gmb_stmt_t* s, const char* k2, const char* k3)
{
    if (!s || s->n_go_args < 2) return;
    gmb_expr_t* a1 = s->go_args[1];
    if (a1 && a1->kind == GMB_EXPR_CALL && a1->var && strcmp(a1->var, "_dict_") == 0) return;
    gmb_expr_t* keys[16]; gmb_expr_t* vals[16]; int np = 0;
    if (k2 && k2[0]) { keys[np] = gmb_expr_str(k2, s->line); vals[np] = a1; np++; }
    if (s->n_go_args >= 3) {
        gmb_expr_t* a2 = s->go_args[2];
        if (a2 && a2->kind == GMB_EXPR_CALL && a2->var && strcmp(a2->var, "_dict_") == 0) {
            for (int i = 0; i + 1 < a2->nargs && np < 16; i += 2) { keys[np] = a2->args[i]; vals[np] = a2->args[i+1]; np++; }
        } else if (k3 && k3[0]) {
            keys[np] = gmb_expr_str(k3, s->line); vals[np] = a2; np++;
            if (s->n_go_args >= 4) {
                gmb_expr_t* a3 = s->go_args[3];
                if (a3 && a3->kind == GMB_EXPR_CALL && a3->var && strcmp(a3->var, "_dict_") == 0)
                    for (int i = 0; i + 1 < a3->nargs && np < 16; i += 2) { keys[np] = a3->args[i]; vals[np] = a3->args[i+1]; np++; }
            }
        }
    }
    gmb_expr_t** cargs = (gmb_expr_t**)calloc((size_t)(np ? np : 1) * 2, sizeof(gmb_expr_t*));
    for (int i = 0; i < np; i++) { cargs[i*2] = keys[i]; cargs[i*2+1] = vals[i]; }
    gmb_expr_t* dict = gmb_expr_call("_dict_", cargs, np * 2, s->line);
    gmb_expr_t** ng = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
    ng[0] = s->go_args[0]; ng[1] = dict;
    free(s->go_args);
    s->go_args = ng; s->n_go_args = 2;
}

/* B2 — alias TOP-LEVEL dei sotto-verbi LLM TENSOR (tokenizer/chat-template): `LLM TOKENIZE …`
 * viene riscritto a `LLM TENSOR TOKENIZE …` (nessun TokenKind dedicato, stesso dispatch). */
static int gmb_p_llm_tensor_alias(const char* v)
{
    return v && (strcasecmp(v, "TOKENIZE") == 0 || strcasecmp(v, "TOKENS") == 0
        || strcasecmp(v, "DETOKENIZE") == 0 || strcasecmp(v, "DETOK") == 0
        || strcasecmp(v, "VOCAB") == 0 || strcasecmp(v, "TOKENIZER") == 0
        || strcasecmp(v, "TOKINFO") == 0         || strcasecmp(v, "TEMPLATE") == 0
        || strcasecmp(v, "CHATTEMPLATE") == 0 || strcasecmp(v, "TEMPLATETEST") == 0
        || strcasecmp(v, "APPLYTEMPLATE") == 0 || strcasecmp(v, "CHATTEST") == 0
        || strcasecmp(v, "REGISTRY") == 0 || strcasecmp(v, "CALIBRATE") == 0
        || strcasecmp(v, "ACTIVATIONSTATS") == 0 || strcasecmp(v, "QUANTERROR") == 0
        || strcasecmp(v, "COMPARE") == 0
        || strcasecmp(v, "DENSE") == 0
        || strcasecmp(v, "DERIVE") == 0
        || strcasecmp(v, "MAKEPATCH") == 0 || strcasecmp(v, "APPLYPATCH") == 0);
}

/* SCENE (motore 3D, renderer mesh — P3): i verbi NON sono keyword hard. `VIEW`, `TEXTURE`,
 * `QUAD`, `BOX` e `SAVE` esistono già come parole del linguaggio; `RESET`, `OBJECT`, `VERTEX`,
 * `OBJPOS`, `CAMERA` e `RENDER` sono nomi che i programmi usano come variabili, e riservarli
 * romperebbe il codice esistente. Il lessema si traduce QUI, una volta sola, nei TokenKind
 * GMB_TOK_Scene*: a valle (emitter C99) il dispatch è per TokenKind, mai per lessema (§12),
 * esattamente come per `CLOCK FPS`. Il parser C# fa la stessa cosa in Parser.Stmt.Scene.cs. */
static int gmb_p_scene_verb_tk(const char* v)
{
    static const struct { const char* word; gmb_tokenkind_t tk; } map[] = {
        { "RESET",   GMB_TOK_SceneReset  }, { "VIEW",    GMB_TOK_SceneView    },
        { "TEXTURE", GMB_TOK_SceneTexture}, { "OBJECT",  GMB_TOK_SceneObject  },
        { "VERTEX",  GMB_TOK_SceneVertex }, { "QUAD",    GMB_TOK_SceneQuad    },
        { "BOX",     GMB_TOK_SceneBox    }, { "OBJPOS",  GMB_TOK_SceneObjPos  },
        { "CAMERA",  GMB_TOK_SceneCamera }, { "RENDER",  GMB_TOK_SceneRender  },
        { "SAVE",    GMB_TOK_SceneSave   }, { "LIGHT",   GMB_TOK_SceneLight },
        { "SMOOTH",  GMB_TOK_SceneSmooth }, { "ALPHA",   GMB_TOK_SceneAlpha },
        { "TWOSIDED", GMB_TOK_SceneTwoSided }, { "PRESENT", GMB_TOK_ScenePresent },
        /* P4 */
        { "LAMP", GMB_TOK_SceneLamp }, { "FOG", GMB_TOK_SceneFog },
        { "FAR", GMB_TOK_SceneFar }, { "SKY", GMB_TOK_SceneSky },
        { "COLORMAP", GMB_TOK_SceneColormap }, { "THING", GMB_TOK_SceneThing },
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (strcasecmp(v, map[i].word) == 0) return (int)map[i].tk;
    return 0;
}

gmb_stmt_t* gmb_p_parse_domain_stmt(gmb_parser_t* p, int line)
{
    return gmb_p_parse_domain_stmt_ov(p, line, NULL, 0);
}

/* Variante con namespace SOVRASCRITTO (per il bare command fallback: `USE …` nudo → RDD):
 * se ov_name!=NULL il ns è quello (win_name/win_tk) e il cursore è GIÀ sul verbo; altrimenti
 * si legge il ns dal token corrente (comportamento storico). */
gmb_stmt_t* gmb_p_parse_domain_stmt_ov(gmb_parser_t* p, int line, const char* ov_name, gmb_tokenkind_t ov_tk)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_DOMAIN, line);
    if (ov_name) { s->win_name = strdup(ov_name); s->win_tk = ov_tk; }
    else {
        const gmb_token_t* ns = gmb_p_cur(p);
        if (!ns) { gmb_stmt_free(s); return NULL; }
        s->win_name = gmb_p_tok_strdup(ns);      /* namespace */
        s->win_tk = ns->kind;                    /* token del dominio (mai from_name a valle) */
        gmb_p_adv(p);
    }
    /* verbo: parola chiave (o identificatore) immediatamente successiva.
     * act_tk = TokenKind P_* del sottocomando (tassonomia L/P): keyword di dominio
     * direttamente, altrimenti verbi-statement risolti dalla CommandName map. */
    const gmb_token_t* v = gmb_p_cur(p);
    if (v && v->length > 0 && v->kind != GMB_TOK_Newline && v->kind != GMB_TOK_Eof) {
        int alias_keep = 0;
        if (s->win_name && strcasecmp(s->win_name, "LLM") == 0 && v->kind == GMB_TOK_Identifier) {
            char vb[48];
            int vl = v->length < 47 ? v->length : 47;
            memcpy(vb, v->lexeme, (size_t)vl); vb[vl] = 0;
            if (gmb_p_llm_tensor_alias(vb)) {
                /* B2: alias top-level → LLM TENSOR <verbo>. mutex_action=TENSOR; il verbo NON si
                 * consuma: il lettore di termini lo parsa come go_args[0] (a[0]->var per l'emitter). */
                s->mutex_action = strdup("TENSOR");
                s->act_tk = GMB_TOK_LlmTensor;
                alias_keep = 1;
            }
        }
        if (!alias_keep) {
            s->mutex_action = strdup(gmb_p_tok_strdup(v));
            /* Regola d'oro (§12): il verbo di dominio si classifica SOLO per TokenKind.
             * PLUGINS/PLUGIN/PERMISSION/CONNECTOR/CONNECTORS sono keyword nel lexer C
             * (come in common/Lexer/Lexer.Keywords.cs) e arrivano qui con il proprio
             * kind: nessun confronto per lessema. Un verbo che non e' keyword resta
             * GMB_TOK_Identifier e non ha act_tk (il dispatch a valle usa mutex_action). */
            if (v->kind != GMB_TOK_Identifier) s->act_tk = (int)v->kind;
            /* SCENE: verbo → TokenKind dedicato, una volta sola, qui (vedi gmb_p_scene_verb_tk). */
            if (s->win_tk == GMB_TOK_Scene) {
                char sv[16]; int sl = v->length < 15 ? v->length : 15;
                memcpy(sv, v->lexeme, (size_t)sl); sv[sl] = 0;
                int stk = gmb_p_scene_verb_tk(sv);
                if (stk) s->act_tk = stk;
            }
            gmb_p_adv(p);
            /* AI LIST OPTIONS: il verbo `LIST` e' keyword, la parola `OPTIONS` NO (l'unica
             * keyword e' OPTION, che serve a `AI SET OPTION`). La si riconosce UNA volta qui,
             * nel parser, e si normalizza nel TokenKind dedicato: gli stadi a valle (emitter
             * C99, compilatore bytecode) dispatchano SOLO per TokenKind, mai per lessema. */
            if (s->win_name && strcasecmp(s->win_name, "AI") == 0 && v->kind == GMB_TOK_List) {
                const gmb_token_t* nx = gmb_p_cur(p);
                if (nx && nx->kind == GMB_TOK_Identifier && nx->length == 7
                    && strncasecmp(nx->lexeme, "OPTIONS", 7) == 0) {
                    gmb_p_adv(p);
                    s->act_tk = GMB_TOK_AiListOptions;
                }
            }
            /* CLOCK FPS: `FPS` NON è una keyword del lessico (i programmi esistenti usano `fps`
             * come nome di variabile), quindi il verbo arriva come GMB_TOK_Identifier. Si
             * normalizza QUI, una volta sola, nel TokenKind dedicato: a valle (emitter C99) il
             * dispatch è per TokenKind, mai per lessema (§12). */
            if (s->win_tk == GMB_TOK_Clock && v->kind == GMB_TOK_Identifier && s->mutex_action
                && strcasecmp(s->mutex_action, "FPS") == 0) {
                s->act_tk = GMB_TOK_Fps;
            }
            /* VECTOR ADD WRAP dest, src, min, max — verbo su DUE parole (canone C#
             * LinAlgVectorAddWrapCommand). Il lettore generico prende una sola parola:
             * senza questo WRAP diventava il primo argomento (dest) → C invalido. */
            if (s->win_tk == GMB_TOK_Vector) {
                const gmb_token_t* w = gmb_p_cur(p);
                if (w && w->length == 4 && w->kind != GMB_TOK_StringLiteral
                    && strncasecmp(w->lexeme, "WRAP", 4) == 0
                    && (strcasecmp(s->mutex_action, "ADD") == 0 || strcasecmp(s->mutex_action, "SUB") == 0)) {
                    char vb[40];
                    size_t bl = strlen(s->mutex_action);
                    if (bl > sizeof(vb) - 7) bl = sizeof(vb) - 7;
                    memcpy(vb, s->mutex_action, bl); vb[bl] = 0;
                    strcat(vb, " WRAP");
                    free(s->mutex_action);
                    s->mutex_action = strdup(vb);
                    s->act_tk = GMB_TOK_LinAlgVectorAddWrapCommand;
                    gmb_p_adv(p);
                }
            }
        }
    }
    /* termini fino a fine statement */
    int cap = 0;
    /* AI WORKER RUN <nome>: nel lexer C `RUN` NON è keyword hard (soft keyword, come nella
     * forma `RUN <sub>`), quindi il termine arrivava come VAR "RUN" e l'emitter cadeva sul
     * ramo INVOKE: `AI WORKER RUN "w1"` emetteva `gmb_worker_invoke("RUN")` — nome del
     * worker PERSO e contabilita' di un'invocazione (misurato: spend=251 rescued=1 su
     * C99Native contro 0|0 sulle altre tre gambe). Il SUB-VERBO si normalizza qui nel
     * TokenKind dedicato (a[0]->tk = GMB_TOK_Run), che e' cio' che l'emitter dispatcha:
     * regola d'oro §12, mai per lessema. */
    if (s->win_tk == GMB_TOK_Ai && s->act_tk == (int)GMB_TOK_AiSubWorker
        && gmb_p_tok_is_soft_keyword(gmb_p_cur(p), "RUN")) {
        gmb_p_adv(p);
        gmb_expr_t* sub = gmb_expr_var("RUN", line);
        if (sub) {
            sub->tk = GMB_TOK_Run;
            if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
            s->go_args[s->n_go_args++] = sub;
        }
    }
    while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p)) {
        if (gmb_p_check(p, GMB_TOK_Comma)) { gmb_p_adv(p); continue; }
        /* ':' = separatore di STATEMENT sulla stessa riga: termina gli argomenti.
         * Prima lo si saltava e si continuava: `DBF SKIP : LOOP` assorbiva LOOP come
         * argomento → "blocco non terminato" nel DO WHILE a una riga (c99n §11). */
        if (gmb_p_check(p, GMB_TOK_Colon)) break;
        if (gmb_p_check(p, GMB_TOK_LeftBrace)) {
            gmb_expr_t* dlit = gmb_p_parse_dict_lit(p, line);
            if (dlit) {
                if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
                s->go_args[s->n_go_args++] = dlit;
            }
            continue;
        }
        int term_pos = p->pos;
        /* SCENE SMOOTH|ALPHA|TWOSIDED ON|OFF (idea #5): ON e OFF sono KEYWORD del lessico,
         * non espressioni, quindi il lettore generico ne faceva una VAR "ON"/"OFF" → su
         * tutte le gambe l'errore era "variabile non definita". Si convertono QUI, una
         * volta sola, nel letterale 1/0: a valle resta un argomento numerico come gli altri,
         * e il verbo è già l'unico dispatch (act_tk). Il namespace non si ricontrolla:
         * questi tre act_tk li assegna solo gmb_p_scene_verb_tk (verbi di SCENE). */
        if (s->act_tk == (int)GMB_TOK_SceneSmooth || s->act_tk == (int)GMB_TOK_SceneAlpha
            || s->act_tk == (int)GMB_TOK_SceneTwoSided) {
            const gmb_token_t* ft = gmb_p_cur(p);
            if (ft && (ft->kind == GMB_TOK_On || ft->kind == GMB_TOK_Off)) {
                gmb_expr_t* fe = gmb_expr_num(ft->kind == GMB_TOK_On ? 1.0 : 0.0, line);
                gmb_p_adv(p);
                if (fe) {
                    if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
                    s->go_args[s->n_go_args++] = fe;
                }
                continue;
            }
        }
        /* Macro `&(expr)` / `&var` in POSIZIONE DI TERMINE (es. `ISAM SET FILTER TO &(f) ON -1`, dopo la
         * clausola TO): si parsa come ESPRESSIONE (call sentinella "_macro_"), che l'emitter sa espandere. */
        if (gmb_p_check(p, GMB_TOK_Ampersand)) {
            gmb_expr_t* me = gmb_p_parse_expr(p);
            if (!me) { gmb_p_adv(p); continue; }
            if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
            s->go_args[s->n_go_args++] = me;
            continue;
        }
        /* Argomento PARENTESIZZATO `(expr)` (es. `AI AGENT SET n "k" (value)` — la forma
         * usata dalle PROPERTY di include/Agent.inc): il token `(` non è un termine → si
         * parsa l'ESPRESSIONE. Prima diventava la VAR "(" e l'emitter emetteva la stringa
         * "(" → le PropSet scrivevano "(" invece del valore (AgentObject). */
        if (gmb_p_check(p, GMB_TOK_LeftParen)) {
            gmb_expr_t* pe = gmb_p_parse_expr(p);
            if (!pe) { gmb_p_adv(p); continue; }
            if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
            s->go_args[s->n_go_args++] = pe;
            continue;
        }
        /* Costante %NOME (es. %CHAT, %EMBEDDING): il lexer la spezza in `%` + nome —
         * qui si ricombina come fa il parser delle espressioni (stessa regola), altrimenti
         * l'emitter vedeva `_I` + nome separati (misurato: `AI SET ACTIVE %EMBEDDING, x`
         * emetteva set_active(_I, EMBEDDING)). */
        {
            const gmb_token_t* tt = gmb_p_cur(p);
            if (tt && tt->kind == GMB_TOK_Unknown && tt->length == 1 && tt->lexeme[0] == '%') {
                const gmb_token_t* cn = gmb_p_peek(p, 1);
                if (cn) {
                    char nm[65]; int l = cn->length < 63 ? cn->length : 63;
                    nm[0] = '%'; memcpy(nm + 1, cn->lexeme, (size_t)l); nm[l + 1] = 0;
                    gmb_expr_t* ce = gmb_expr_var(nm, line);
                    gmb_p_adv(p); gmb_p_adv(p);
                    if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
                    s->go_args[s->n_go_args++] = ce;
                    continue;
                }
            }
        }
        /* TERMINE CON SEGNO (`-2`, `+5`, `-var`, `-f(x)`) come argomento: il lettore generico
         * vedeva il '-' come variabile (sanitizzata "_2D") → `VECTOR FILL RANDOM DX, -2, 2`
         * generava velocità tutte positive, e `SCENE BOX -dw, …` diventava
         * `gmb_scene_box(_2D, dw, 0, _2D, dw, dw)` (argomento spurio + shift, gamba C).
         * Numero: si piega subito nel letterale (comportamento storico). Qualunque altro
         * operando: si riavvolge e si parsa l'ESPRESSIONE intera, unaria compresa. */
        {
            const gmb_token_t* st = gmb_p_cur(p);
            if (st && (st->kind == GMB_TOK_Minus || st->kind == GMB_TOK_Plus)) {
                const gmb_token_t* nt = gmb_p_peek(p, 1);
                if (nt && nt->kind != GMB_TOK_Number && nt->kind != GMB_TOK_Newline
                    && nt->kind != GMB_TOK_Comma && nt->kind != GMB_TOK_Eof
                    && nt->kind != GMB_TOK_Colon) {
                    p->pos = term_pos;
                    gmb_expr_t* se = gmb_p_parse_expr(p);
                    if (!se) { gmb_p_adv(p); continue; }
                    if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
                    s->go_args[s->n_go_args++] = se;
                    continue;
                }
                if (nt && nt->kind == GMB_TOK_Number) {
                    char nb[64]; int l = nt->length < 63 ? nt->length : 63;
                    memcpy(nb, nt->lexeme, (size_t)l); nb[l] = 0;
                    gmb_expr_t* ne = gmb_expr_num((st->kind == GMB_TOK_Minus ? -1.0 : 1.0) * gmb_p_num_literal(nb), line);
                    gmb_p_adv(p); gmb_p_adv(p);
                    if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
                    s->go_args[s->n_go_args++] = ne;
                    continue;
                }
            }
        }
        gmb_expr_t* e = gmb_p_term_from_token(gmb_p_cur(p), line);
        if (!e) { gmb_p_adv(p); continue; }
        /* Se dopo il termine c'è un OPERATORE BINARIO, l'argomento è un'ESPRESSIONE (es.
         * `DBF CREATE dir + "/x.dbf"`), non una lista di termini: si riavvolge e si parsa
         * l'espressione intera. Senza questo, go_args diventava [dir, "/x.dbf"] e l'emitter
         * usava solo il PRIMO termine → `gmb_db_dbf_create(<cartella>, …)` (misurato su
         * samples/Databases/DbDbfUncoveredCoverage.bas: create/open sulla cartella
         * → lastrec=0 e DBF.LOOKUP vuoto sulla gamba C99Native, mentre IL dava 3 e 'N2').
         * Il riavvolgimento scatta SOLO con un operatore davanti, quindi le clausole
         * (ALIAS/FROM TYPE/INTO/…) restano termini singoli come prima. */
        gmb_p_adv(p);
        {
            const gmb_token_t* nx = gmb_p_cur(p);
            bool is_op = nx && (nx->kind == GMB_TOK_Plus || nx->kind == GMB_TOK_Minus
                             || nx->kind == GMB_TOK_Multiply || nx->kind == GMB_TOK_Divide
                             || nx->kind == GMB_TOK_IntegerDivide || nx->kind == GMB_TOK_Power
                             || nx->kind == GMB_TOK_Mod || nx->kind == GMB_TOK_Ampersand
                             || nx->kind == GMB_TOK_Equal || nx->kind == GMB_TOK_NotEqual
                             || nx->kind == GMB_TOK_LessThan || nx->kind == GMB_TOK_LessEqual
                             || nx->kind == GMB_TOK_GreaterThan || nx->kind == GMB_TOK_GreaterEqual
                             || nx->kind == GMB_TOK_Dot
                             /* SUBSCRIPT/INDEX: `AI SET ACTIVE %CHAT, m(0)` — il termine è `m`
                              * e il `(0)` seguente va incluso (altrimenti l'argomento resta il
                              * solo `m` → `m.data` su una lista: cc-error, ProviderDiscoverRouting). */
                             || nx->kind == GMB_TOK_LeftParen || nx->kind == GMB_TOK_LeftBracket);
            /* La clausola `TO` seguita da `&` e' la macro `TO &(f)`, NON la concatenazione `TO & (f)`: il termine
             * `TO` resta isolato e la macro viene parsata al giro successivo (vedi sopra). */
            if (is_op && nx->kind == GMB_TOK_Ampersand && e->kind == GMB_EXPR_VAR && e->var && strcasecmp(e->var, "TO") == 0)
                is_op = false;
            /* Clausola KEYWORD seguita da un operatore: NON è un'espressione. `DBF REPLACE
             * Saldo WITH -120` veniva riscritto come un'unica espressione `WITH - 120` →
             * go_args = [.., Saldo, (WITH-120)] e il pair della REPLACE spariva (Saldo mai
             * scritto: -120 letto come 0). Vale per le clausole che possono precedere un
             * valore negativo o un'espressione (WITH/TO/ON/...). */
            if (is_op && e->kind == GMB_EXPR_VAR && e->var) {
                static const char* const cl[] = { "WITH", "TO", "ON", "ALIAS", "AS", "INTO",
                    "FROM", "KEY", "LEN", "FOR", "FIELDS", "VIA", "INDEX", "ORDER",
                    "DEFAULT", "PATH", "TYPE", NULL };
                for (int ci = 0; cl[ci]; ci++)
                    if (strcasecmp(e->var, cl[ci]) == 0) { is_op = false; break; }
            }
            if (is_op)
            {
                gmb_expr_free(e);
                p->pos = term_pos;
                e = gmb_p_parse_expr(p);
                if (!e) continue;
            }
        }
        if (s->n_go_args >= cap) { cap = cap ? cap * 2 : 8; s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*)); }
        s->go_args[s->n_go_args++] = e;
    }
    /* `LLM … INTO <target>` (2026-10-30): il RISULTATO va nella variabile invece di essere
     * stampato — come per AI VERIFY … INTO. Il token INTO arriva come termine (clausola): qui si
     * riconosce, si prende il termine successivo come target e si scarta la coda. Vale per la
     * famiglia TENSOR (`LLM TOKENS …`, `LLM TENSOR <sub> …`). */
    if (s->win_name && strcasecmp(s->win_name, "LLM") == 0 && s->mutex_action && s->n_go_args >= 3
        && (strcasecmp(s->mutex_action, "TENSOR") == 0 || strcasecmp(s->mutex_action, "POLICY") == 0
            || strcasecmp(s->mutex_action, "ARCH") == 0 || strcasecmp(s->mutex_action, "MODULE") == 0
            || strcasecmp(s->mutex_action, "KERNEL") == 0 || strcasecmp(s->mutex_action, "KV") == 0)) {
        for (int i = 1; i < s->n_go_args; i++) {
            gmb_expr_t* ai = s->go_args[i];
            if (ai && ai->kind == GMB_EXPR_VAR && ai->var && strcasecmp(ai->var, "INTO") == 0) {
                if (i + 1 < s->n_go_args) {
                    gmb_expr_t* tgt = s->go_args[i + 1];
                    if (tgt && tgt->kind == GMB_EXPR_VAR && tgt->var) {
                        free(s->net_assign);
                        s->net_assign = strdup(tgt->var);
                        for (int k = i; k < s->n_go_args; k++) { gmb_expr_free(s->go_args[k]); s->go_args[k] = NULL; }
                        s->n_go_args = i;
                    }
                }
                break;
            }
        }
    }
    /* §4: TRAIN FINETUNE <model>, <dataset>, <out> | TRAIN DPO <model>, <pairs>, <out> |
     * TRAIN RESUME <run>[, <steps>] → ripiega i posizionali in un dict di opzioni. */
    if (s->win_name && s->mutex_action && strcasecmp(s->win_name, "TRAIN") == 0) {
        if (strcasecmp(s->mutex_action, "FINETUNE") == 0) gmb_p_fold_train_opts(s, "dataset", "ckpt");
        else if (strcasecmp(s->mutex_action, "DPO") == 0) gmb_p_fold_train_opts(s, "pairs", "ckpt");
        else if (strcasecmp(s->mutex_action, "GRPO") == 0) gmb_p_fold_train_opts(s, "dataset", "ckpt");
        else if (strcasecmp(s->mutex_action, "RESUME") == 0) gmb_p_fold_train_opts(s, "steps", "");
    }
    return s;
}
