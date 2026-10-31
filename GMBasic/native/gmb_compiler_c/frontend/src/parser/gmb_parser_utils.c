/* native/gmb_runtime_c/src/parser/gmb_parser_utils.c — speculare a Parser.Utils.cs */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <strings.h>

gmb_stmt_list_t gmb_p_stmt_list_new(void) { gmb_stmt_list_t L = {0}; return L; }

void gmb_p_stmt_list_add(gmb_stmt_list_t* L, gmb_stmt_t* s)
{
    if (L->n >= L->cap) {
        L->cap = L->cap ? L->cap * 2 : 8;
        L->items = (gmb_stmt_t**)realloc(L->items, (size_t)L->cap * sizeof(gmb_stmt_t*));
    }
    L->items[L->n++] = s;
}

void gmb_p_stmt_list_free(gmb_stmt_list_t* L) { for (int i = 0; i < L->n; i++) gmb_stmt_free(L->items[i]); free(L->items); }

void gmb_p_queue_stmt(gmb_parser_t* p, gmb_stmt_t* s)
{
    if (!s) return;
    if (p->n_queued >= p->cap_queued) {
        p->cap_queued = p->cap_queued ? p->cap_queued * 2 : 4;
        p->queued = (gmb_stmt_t**)realloc(p->queued, (size_t)p->cap_queued * sizeof(gmb_stmt_t*));
    }
    p->queued[p->n_queued++] = s;
}

void gmb_p_drain_queued(gmb_parser_t* p, gmb_stmt_list_t* out)
{
    for (int i = 0; i < p->n_queued; i++) gmb_p_stmt_list_add(out, p->queued[i]);
    p->n_queued = 0;
}

/* ── avanzamento token ── */
const gmb_token_t* gmb_p_peek(gmb_parser_t* p, int off)
{
    int i = p->pos + off;
    return (i >= 0 && i < p->n) ? &p->toks[i] : NULL;
}
/* Campo di TYPE con dimensione: "arr(4) AS LONG" / "mat(2,3) AS SINGLE". Il guard del corpo
 * TYPE cerca AS a peek(1) e su questa grafia NON lo trova → il campo SPARISCE dal TYPE (e con
 * lui i byte nel layout di SIZEOF/UDTPACKED). Si bilanciano le parensi partendo da peek(1) e
 * si chiede se, tornati a depth 0, il token è AS sulla stessa riga. Ritorna 0 anche per i
 * metodi di una CLASS (who is_udt=0 non consulta questo guard). */
int gmb_p_udt_field_dims_ahead(gmb_parser_t* p)
{
    int depth = 0;
    for (int i = p->pos + 1; i < p->n; i++) {
        const gmb_token_t* t = &p->toks[i];
        if (t->kind == GMB_TOK_Newline || t->kind == GMB_TOK_Eof) return 0;
        if (t->kind == GMB_TOK_LeftParen) { depth++; continue; }
        if (t->kind == GMB_TOK_RightParen) { if (depth == 0) return 0; depth--; continue; }
        if (depth > 0) continue;
        return t->kind == GMB_TOK_As || gmb_p_tok_is_keyword(t, "As");
    }
    return 0;
}
const gmb_token_t* gmb_p_cur(gmb_parser_t* p) { return gmb_p_peek(p, 0); }
bool gmb_p_at_end(gmb_parser_t* p) { const gmb_token_t* t = gmb_p_cur(p); return !t || t->kind == GMB_TOK_Eof; }
/* END <word> (es. END IF, END FUNCTION): keyword multi-parola. */
bool gmb_p_at_end_compound(gmb_parser_t* p, gmb_tokenkind_t second)
{
    const gmb_token_t* t = gmb_p_cur(p);
    return t && t->kind == GMB_TOK_End && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == second;
}
void gmb_p_adv(gmb_parser_t* p) { if (p->pos < p->n) p->pos++; }
bool gmb_p_check(gmb_parser_t* p, gmb_tokenkind_t k) { const gmb_token_t* t = gmb_p_cur(p); return t && t->kind == k; }
bool gmb_p_match(gmb_parser_t* p, gmb_tokenkind_t k) { if (gmb_p_check(p, k)) { gmb_p_adv(p); return true; } return false; }
void gmb_p_set_err(gmb_parser_t* p, const char* m, int line)
{
    if (!p->error) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s (riga %d)", m, line);
        p->error = strdup(buf);
    }
}

void gmb_p_declare_name(gmb_parser_t* p, const char* name)
{
    if (!p || !name || !*name) return;
    if (p->n_declared >= (int)(sizeof(p->declared) / sizeof(p->declared[0]))) return;
    snprintf(p->declared[p->n_declared], sizeof(p->declared[0]), "%s", name);
    p->n_declared++;
}

/* Costanti stringa note a compile-time (DIM ... = "literal", mai riassegnate). */
void gmb_p_cstr_set(gmb_parser_t* p, const char* name, const char* value)
{
    if (!p || !name || !*name || !value) return;
    for (int i = 0; i < p->n_cstr; i++)
        if (strcasecmp(p->cstr_names[i], name) == 0) {
            free(p->cstr_vals[i]);
            p->cstr_vals[i] = strdup(value);
            return;
        }
    if (p->n_cstr >= (int)(sizeof(p->cstr_names) / sizeof(p->cstr_names[0]))) return;
    snprintf(p->cstr_names[p->n_cstr], sizeof(p->cstr_names[0]), "%s", name);
    p->cstr_vals[p->n_cstr] = strdup(value);
    p->n_cstr++;
}

void gmb_p_cstr_clear(gmb_parser_t* p, const char* name)
{
    if (!p || !name || !*name) return;
    for (int i = 0; i < p->n_cstr; i++)
        if (strcasecmp(p->cstr_names[i], name) == 0) {
            free(p->cstr_vals[i]);
            p->cstr_vals[i] = p->cstr_vals[p->n_cstr - 1];
            snprintf(p->cstr_names[i], sizeof(p->cstr_names[0]), "%s", p->cstr_names[p->n_cstr - 1]);
            p->n_cstr--;
            return;
        }
}

const char* gmb_p_cstr_get(const gmb_parser_t* p, const char* name)
{
    if (!p || !name || !*name) return NULL;
    for (int i = 0; i < p->n_cstr; i++)
        if (strcasecmp(p->cstr_names[i], name) == 0) return p->cstr_vals[i];
    return NULL;
}

/* Fold di un'espressione COSTANTE in stringa: letterale, variabile con costante nota
 * (gmb_p_cstr_get), concatenazione `+` di costanti. Conservativo: qualunque altro caso
 * → 0 (nessuna espansione). Abilita `DIM s = pre + "{ |x| ... }"` + `&(s)` a compile-time
 * (bug macro dinamiche con code block). */
int gmb_p_const_str_fold(const gmb_parser_t* p, const gmb_expr_t* e, char* out, size_t cap)
{
    if (!e || !out || cap == 0) return 0;
    if (e->kind == GMB_EXPR_STR) {
        snprintf(out, cap, "%s", e->str ? e->str : "");
        return 1;
    }
    if (e->kind == GMB_EXPR_VAR && e->var) {
        const char* v = gmb_p_cstr_get(p, e->var);
        if (!v) return 0;
        snprintf(out, cap, "%s", v);
        return 1;
    }
    if (e->kind == GMB_EXPR_BINARY && e->op && strcmp(e->op, "+") == 0) {
        char l[1024], r[1024];
        if (!gmb_p_const_str_fold(p, e->left, l, sizeof(l))) return 0;
        if (!gmb_p_const_str_fold(p, e->right, r, sizeof(r))) return 0;
        snprintf(out, cap, "%s%s", l, r);
        return 1;
    }
    return 0;
}

void gmb_p_cstr_reset(gmb_parser_t* p)
{
    if (!p) return;
    for (int i = 0; i < p->n_cstr; i++) free(p->cstr_vals[i]);
    p->n_cstr = 0;
}

bool gmb_p_is_declared(const gmb_parser_t* p, const char* name)
{
    if (!p || !name || !*name) return false;
    for (int i = 0; i < p->n_declared; i++)
        if (strcasecmp(p->declared[i], name) == 0) return true;
    return false;
}

bool gmb_p_is_declared_tok(const gmb_parser_t* p, const gmb_token_t* t)
{
    if (!p || !t || !t->lexeme || t->length <= 0) return false;
    char nm[64];
    int ln = t->length < (int)sizeof(nm) - 1 ? t->length : (int)sizeof(nm) - 1;
    memcpy(nm, t->lexeme, (size_t)ln);
    nm[ln] = 0;
    return gmb_p_is_declared(p, nm);
}

bool gmb_p_tok_is_keyword(const gmb_token_t* t, const char* kw)
{
    if (!t) return false;
    /* Frontiera parser (direttiva 2026-09-08): kw è la grafia di una keyword del LEXER
     * → la si risolve nella keyword-table (mai gmb_tokenkind_name, tabella disallineata). */
    if (t->kind == GMB_TOK_Identifier) return false;
    return gmb_keyword_tok(kw) == (int)t->kind;
}

bool gmb_p_tok_is_soft_keyword(const gmb_token_t* t, const char* kw)
{
    if (!t || !t->lexeme || !kw) return false;
    size_t n = strlen(kw);
    return t->length == (int)n && strncasecmp(t->lexeme, kw, n) == 0;
}

char* gmb_p_tok_strdup(const gmb_token_t* t)
{
    if (!t || !t->lexeme || t->length <= 0) return NULL;
    char* b = (char*)malloc((size_t)t->length + 1);
    memcpy(b, t->lexeme, (size_t)t->length);
    b[t->length] = 0;
    return b;
}

/* 2026-09-24: cattura il nome del namespace della direttiva NAMESPACE corrente in
 * p->cur_ns (anche dotted: TRADING.PORTFOLIO). Riga vuota/senza nome = reset a "" (globale).
 * Serve allo stamp sulle definizioni FUNCTION/DECLARE per il check di shadowing dei builtin. */
void gmb_p_capture_namespace(gmb_parser_t* p)
{
    char nsb[96]; size_t nl = 0; nsb[0] = 0;
    const gmb_token_t* nt = gmb_p_cur(p);
    if (nt && nt->lexeme && nt->length > 0 && nt->kind != GMB_TOK_Newline
        && nt->kind != GMB_TOK_Colon && nt->kind != GMB_TOK_Eof && nt->kind != GMB_TOK_Rem) {
        size_t L = (size_t)nt->length; if (L > sizeof(nsb) - 1) L = sizeof(nsb) - 1;
        memcpy(nsb, nt->lexeme, L); nl = L; nsb[nl] = 0; gmb_p_adv(p);
        for (;;) {
            const gmb_token_t* dt = gmb_p_cur(p);
            const gmb_token_t* at = (dt && dt->kind == GMB_TOK_Dot) ? gmb_p_peek(p, 1) : NULL;
            if (!at || !at->lexeme || at->length <= 0) break;
            if (nl + 1 < sizeof(nsb)) { nsb[nl++] = '.'; nsb[nl] = 0; }
            size_t A = (size_t)at->length;
            if (nl + A >= sizeof(nsb)) A = sizeof(nsb) - 1 - nl;
            memcpy(nsb + nl, at->lexeme, A); nl += A; nsb[nl] = 0;
            gmb_p_adv(p); gmb_p_adv(p);
        }
    }
    if (nsb[0]) {
        strncpy(p->cur_ns, nsb, sizeof(p->cur_ns) - 1); p->cur_ns[sizeof(p->cur_ns) - 1] = 0;
        /* Registra il namespace UTENTE (una sola volta) così gmb_p_strip_ns riconosce
         * `NS.MEMBRO` come qualificato anche per namespace definiti nel programma. */
        int known = 0;
        for (int i = 0; i < p->ns_decls_n; i++) if (!strcasecmp(p->ns_decls[i], nsb)) { known = 1; break; }
        if (!known && p->ns_decls_n < 64) {
            strncpy(p->ns_decls[p->ns_decls_n], nsb, sizeof(p->ns_decls[0]) - 1);
            p->ns_decls[p->ns_decls_n][sizeof(p->ns_decls[0]) - 1] = 0;
            p->ns_decls_n++;
        }
    }
    else p->cur_ns[0] = 0;
}

/* valore numerico di un token LineNumber (0 se non valido) */
static int gmb_p_bas_of(const gmb_token_t* t)
{
    if (!t || !t->lexeme || t->length <= 0 || t->length > 14) return 0;
    char nb[16]; memcpy(nb, t->lexeme, (size_t)t->length); nb[t->length] = 0;
    return atoi(nb);
}

/* 2.8: valore di un letterale numerico BASIC — hex 0x/&H, binario 0b, ottale 0o,
 * suffisso tipo %#!@$ (parità PCodeVM; atof da solo darebbe 0 per 0b/0o/&H) */
double gmb_p_num_literal(const char* b)
{
    if (!b || !*b) return 0.0;
    size_t n = strlen(b);
    if (n > 0 && strchr("%#!@$", b[n - 1])) n--;
    char t[80];
    if (n >= sizeof(t)) n = sizeof(t) - 1;
    memcpy(t, b, n); t[n] = 0;
    if (n >= 2 && t[0] == '&' && (t[1] == 'H' || t[1] == 'h')) return (double)strtoll(t + 2, NULL, 16);
    if (n >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) return (double)strtoll(t + 2, NULL, 16);
    if (n >= 2 && t[0] == '0' && (t[1] == 'b' || t[1] == 'B')) return (double)strtoll(t + 2, NULL, 2);
    if (n >= 2 && t[0] == '0' && (t[1] == 'o' || t[1] == 'O')) return (double)strtoll(t + 2, NULL, 8);
    return atof(t);
}

/* Bug corretto (2026-08-31, audit lambda/closure): una riga numerata la cui riga è
 * SOLO un commento o vuota (es. "100 REM Routine...", seguita da "110 PRINT ...")
 * perdeva il proprio numero SENZA MAI attribuirlo a nessuno statement — ogni reset/
 * sovrascrittura di last_bas_line sotto scartava silenziosamente il valore corrente
 * se non ancora "consumato" da un vero statement (bas_fresh==1 = numero visto ma
 * mai attribuito). Un GOTO/GOSUB verso QUEL numero specifico non trovava quindi mai
 * un'etichetta corrispondente (bug: FunctionsMain.bas, "100 REM Routine GOSUB
 * numerata" seguita da "110 PRINT ..." — "GOSUB 100" non trovava mai l'indirizzo di
 * "100", saltando su bytecode non valido). Fix: PRIMA di scartare/sovrascrivere un
 * last_bas_line ancora "fresh" (mai attribuito), accoda (gmb_p_queue_stmt) un nodo
 * REM dedicato con QUEL numero — ogni chiamante che already drena la coda (il loop
 * di programma in gmb_parser_core.c, i parser di blocco IF/FOR/WHILE/SUB/...)
 * riceve così uno statement REM in più, indirizzabile via GOTO/GOSUB, invece di
 * perdere silenziosamente il numero. Additivo puro: nessun comportamento esistente
 * cambia per il numero "vincente" (quello davvero attribuito allo statement reale
 * che segue), solo i numeri altrimenti scartati vengono ora preservati. */
static void gmb_p_queue_bas_rem(gmb_parser_t* p, int line)
{
    if (!p->bas_fresh || p->last_bas_line <= 0) return;
    gmb_stmt_t* rem = gmb_stmt_new(GMB_STMT_REM, line);
    rem->bas_line = p->last_bas_line;
    gmb_p_queue_stmt(p, rem);
}

/* salta newline/commento/line number (riga di programma); registra in
 * last_bas_line il numero riga BASIC della riga logica corrente (2.8: lbl_N) */
void gmb_p_skip_eol(gmb_parser_t* p)
{
    while (gmb_p_at_end(p)) return;
    const gmb_token_t* t = gmb_p_cur(p);
    if (t->kind == GMB_TOK_LineNumber) {
        gmb_p_queue_bas_rem(p, t->line);
        p->last_bas_line = gmb_p_bas_of(t); p->bas_fresh = 1; gmb_p_adv(p);
    }
    /* continuazione riga: '_' seguito da Newline (parità BASIC/altri dialetti) */
    for (;;) {
        const gmb_token_t* c = gmb_p_cur(p);
        if (c && c->kind == GMB_TOK_Identifier && c->length == 1 && c->lexeme[0] == '_' && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Newline) {
            gmb_p_queue_bas_rem(p, c->line);
            gmb_p_adv(p); gmb_p_adv(p);
            p->last_bas_line = 0; p->bas_fresh = 0;
            if (gmb_p_check(p, GMB_TOK_LineNumber)) { p->last_bas_line = gmb_p_bas_of(gmb_p_cur(p)); p->bas_fresh = 1; gmb_p_adv(p); }
            continue;
        }
        break;
    }
    for (;;) {
        if (gmb_p_check(p, GMB_TOK_Newline)) {
            gmb_p_queue_bas_rem(p, gmb_p_cur(p)->line);
            p->last_bas_line = 0; p->bas_fresh = 0; gmb_p_adv(p); continue;
        }
        if (gmb_p_check(p, GMB_TOK_LineNumber)) {
            gmb_p_queue_bas_rem(p, gmb_p_cur(p)->line);
            p->last_bas_line = gmb_p_bas_of(gmb_p_cur(p)); p->bas_fresh = 1; gmb_p_adv(p); continue;
        }
        break;
    }
}

/* ── espressioni (precedenza: or < and < not < cmp < add < mul < unary < power) ── */
gmb_expr_t* gmb_p_parse_expr(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_or(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_and(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_not(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_cmp(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_add(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_mul(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_unary(gmb_parser_t* p);
