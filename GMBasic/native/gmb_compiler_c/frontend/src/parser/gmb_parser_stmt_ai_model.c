/* native/gmb_compiler_c/src/parser/gmb_parser_stmt_ai_model.c — speculare a
 * Parser.Stmt.Ai.cs (ParseAiModelStatement), provider MoE out-of-core (GGUF, M14):
 *   LLM MODEL LOAD <nome>, <path> [, RESIDENT]  → GMB_STMT_AIMODEL_LOAD
 *   LLM MODEL UNLOAD <nome>                     → GMB_STMT_AIMODEL_UNLOAD
 *   LLM MODEL CACHE <nome>, <MB>                → GMB_STMT_AIMODEL_CACHE
 *   LLM MODEL PREFETCH <nome>, <topn>           → GMB_STMT_AIMODEL_PREFETCH
 *   LLM MODEL DEVICE <nome>, AUTO|CPU|METAL     → GMB_STMT_AIMODEL_DEVICE
 *   LLM MODEL CONVERT/QUANTIZE/INSPECT/SAVE      → model compiler
 * Il chiamante ha gia' consumato 'LLM' + 'MODEL'; qui il token corrente e' il verbo.
 * Runtime: gmb_moestream_stmt_* del runtime condiviso (dlopen libgmb_ai_moestream).
 * (Ripristinati 2026-09-25: erano stati rimossi per errore insieme al formato .gmoe.) */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void gmb_p_aimodel_add(gmb_stmt_t* s, gmb_expr_t* e)
{
    int cap = s->n_go_args + 1;
    s->go_args = (gmb_expr_t**)realloc(s->go_args, (size_t)cap * sizeof(gmb_expr_t*));
    s->go_args[s->n_go_args++] = e;
}

/* Confronto case-insensitive del lexeme con una keyword (parole che il lexer C non mappa
 * come keyword: ABLATE/ADAPTER/DIFF/CHECKPOINT/MODALITY/PATCH). Le keyword che il lexer
 * CONOSCE (PREFETCH/QUANTIZE/INSPECT/…) si dispatchano per TokenKind, mai per lessema. */
static int gmb_p_aimodel_kw(const gmb_token_t* v, const char* kw)
{
    if (!v || v->length == 0) return 0;
    size_t n = strlen(kw);
    if ((size_t)v->length != n) return 0;
    return strncasecmp(v->lexeme, kw, n) == 0;
}

static gmb_expr_t* gmb_p_aimodel_term(gmb_parser_t* p, int line)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->length == 0) return NULL;
    /* VARIABILE STRINGA (identificatore col suffisso `$`, es. B$) -> ESPRESSIONE (valore a
     * runtime), NON un letterale col suo NOME: altrimenti `LLM MODEL INSPECT B$` passerebbe la
     * stringa "B$". Pari al lato C# (`ParseAiModelArg`: `$`-var = VariableExpr). */
    if ((t->kind == GMB_TOK_Id || t->kind == GMB_TOK_Identifier)
        && t->length >= 1 && t->lexeme[t->length - 1] == '$')
        return gmb_p_parse_expr(p);
    if (t->kind == GMB_TOK_StringLiteral || t->kind == GMB_TOK_Number || t->kind == GMB_TOK_Value
        || t->kind == GMB_TOK_Ampersand) {
        return gmb_p_parse_expr(p);
    }
    /* token nudo (identificatore/keyword come RESIDENT, METAL, un nome) → stringa letterale */
    char buf[512];
    int l = t->length < 511 ? t->length : 511;
    memcpy(buf, t->lexeme, (size_t)l);
    buf[l] = 0;
    size_t bl = strlen(buf);
    if (bl >= 2 && buf[0] == '"' && buf[bl - 1] == '"') { memmove(buf, buf + 1, bl - 2); buf[bl - 2] = 0; }
    gmb_p_adv(p);
    return gmb_expr_str(buf, line);
}

gmb_stmt_t* gmb_p_parse_ai_model(gmb_parser_t* p, int line)
{
    const gmb_token_t* v = gmb_p_cur(p);
    if (!v) return NULL;

    gmb_stmt_kind_t kind;
    if (v->kind == GMB_TOK_Load) { kind = GMB_STMT_AIMODEL_LOAD; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Unload) { kind = GMB_STMT_AIMODEL_UNLOAD; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Cache) { kind = GMB_STMT_AIMODEL_CACHE; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Prefetch) { kind = GMB_STMT_AIMODEL_PREFETCH; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Device) { kind = GMB_STMT_AIMODEL_DEVICE; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_ConvDoc) { kind = GMB_STMT_AIMODEL_CONVERT; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Promote) { kind = GMB_STMT_AIMODEL_PROMOTE; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Demote) { kind = GMB_STMT_AIMODEL_DEMOTE; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Quantize) { kind = GMB_STMT_AIMODEL_QUANTIZE; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Inspect) { kind = GMB_STMT_AIMODEL_INSPECT; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Save) { kind = GMB_STMT_AIMODEL_SAVE; gmb_p_adv(p); }
    else if (gmb_p_aimodel_kw(v, "ABLATE")) { kind = GMB_STMT_AIMODEL_ABLATE; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Remove) { kind = GMB_STMT_AIMODEL_REMOVE; gmb_p_adv(p); }
    else if (gmb_p_aimodel_kw(v, "DIFF")) { kind = GMB_STMT_AIMODEL_DIFF; gmb_p_adv(p); }
    else if (gmb_p_aimodel_kw(v, "CHECKPOINT")) { kind = GMB_STMT_AIMODEL_CHECKPOINT; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Rollback) { kind = GMB_STMT_AIMODEL_ROLLBACK; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Add) { kind = GMB_STMT_AIMODEL_ADD; gmb_p_adv(p); }
    else if (gmb_p_aimodel_kw(v, "ADAPTER") || v->kind == GMB_TOK_LlmMerge) { kind = GMB_STMT_AIMODEL_ADAPTER; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_LlmModule) { kind = GMB_STMT_AIMODEL_MODULE; gmb_p_adv(p); }
    else if (gmb_p_aimodel_kw(v, "MODALITY")) { kind = GMB_STMT_AIMODEL_MODALITY; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_Validate) { kind = GMB_STMT_AIMODEL_VALIDATE; gmb_p_adv(p); }
    else if (v->kind == GMB_TOK_AiSubTrace) { kind = GMB_STMT_AIMODEL_TRACE; gmb_p_adv(p); }
    else {
        gmb_p_set_err(p, "Atteso LOAD, UNLOAD, CACHE, PREFETCH, DEVICE, CONVERT, PROMOTE, DEMOTE, QUANTIZE, INSPECT, SAVE, ABLATE, DIFF, CHECKPOINT, ROLLBACK, ADD, ADAPTER, MODULE, MODALITY, VALIDATE o TRACE dopo LLM MODEL", line);
        return NULL;
    }

    gmb_stmt_t* s = gmb_stmt_new(kind, line);
    gmb_expr_t* a0 = gmb_p_aimodel_term(p, line);
    if (a0) gmb_p_aimodel_add(s, a0);
    while (gmb_p_check(p, GMB_TOK_Comma)) {
        gmb_p_adv(p); /* consuma la virgola */
        gmb_expr_t* a = gmb_p_aimodel_term(p, line);
        if (a) gmb_p_aimodel_add(s, a);
    }
    while (!gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Colon))
        gmb_p_adv(p);
    return s;
}

/* §4.10 write-side: AI PATCH "target", "rev" ... END PATCH — corpo in then_body; l'apply e' in gmb_semfold. */
gmb_stmt_t* gmb_p_parse_aipatch(gmb_parser_t* p, int line)
{
    gmb_p_adv(p);   /* AI */
    gmb_p_adv(p);   /* PATCH */
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AIPATCH, line);
    gmb_expr_t* te = gmb_p_parse_expr(p);
    s->patch_target = strdup(te && te->kind == GMB_EXPR_STR && te->str ? te->str : "");
    if (gmb_p_match(p, GMB_TOK_Comma)) {
        gmb_expr_t* re = gmb_p_parse_expr(p);
        s->patch_rev = strdup(re && re->kind == GMB_EXPR_STR && re->str ? re->str : "");
    } else { s->patch_rev = strdup(""); }
    gmb_stmt_list_t body; memset(&body, 0, sizeof body);
    gmb_p_parse_block(p, &body, GMB_TOK_End, NULL, line);
    if (gmb_p_match(p, GMB_TOK_End)) {
        const gmb_token_t* pk = gmb_p_cur(p);
        if (pk && (pk->kind == GMB_TOK_AiPatch || gmb_p_tok_is_soft_keyword(pk, "PATCH")))
            gmb_p_adv(p);
    }
    s->then_body = body.items; s->n_then = body.n;
    return s;
}
