/* ============================================================================
 * gmb_expr.c — lexer + recursive descent + tree-walk per & / EVAL.
 * Sottoinsieme intenzionale: aritmetica, stringhe, confronti, AND/OR/NOT,
 * ident via env, LET/PRINT, builtin. Vedi Todo/Piano-Valutatore-Espressioni-C.
 * ============================================================================ */
#include "gmb_expr.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef gmb_expr_tok tok_kind;
typedef gmb_expr_nd nd_kind;
typedef gmb_expr_node node;

#define TK_EOF     GMB_XT_EOF
#define TK_NUM     GMB_XT_NUM
#define TK_STR     GMB_XT_STR
#define TK_ID      GMB_XT_ID
#define TK_PLUS    GMB_XT_PLUS
#define TK_MINUS   GMB_XT_MINUS
#define TK_STAR    GMB_XT_STAR
#define TK_SLASH   GMB_XT_SLASH
#define TK_PERCENT GMB_XT_PERCENT
#define TK_CARET   GMB_XT_CARET
#define TK_EQ      GMB_XT_EQ
#define TK_NE      GMB_XT_NE
#define TK_LT      GMB_XT_LT
#define TK_GT      GMB_XT_GT
#define TK_LE      GMB_XT_LE
#define TK_GE      GMB_XT_GE
#define TK_LP      GMB_XT_LP
#define TK_RP      GMB_XT_RP
#define TK_COMMA   GMB_XT_COMMA
#define TK_COLON   GMB_XT_COLON
#define TK_AND     GMB_XT_AND
#define TK_OR      GMB_XT_OR
#define TK_NOT     GMB_XT_NOT
#define TK_LET     GMB_XT_LET
#define TK_DIM     GMB_XT_DIM
#define TK_STATIC  GMB_XT_STATIC
#define TK_PRINT   GMB_XT_PRINT
#define TK_AMP     GMB_XT_AMP
#define TK_IF      GMB_XT_IF
#define TK_THEN    GMB_XT_THEN
#define TK_ELSE    GMB_XT_ELSE
#define TK_ENDIF   GMB_XT_ENDIF
#define TK_SEMI    GMB_XT_SEMI
#define TK_FOR     GMB_XT_FOR
#define TK_TO      GMB_XT_TO
#define TK_NEXT    GMB_XT_NEXT
#define TK_STEP    GMB_XT_STEP
#define TK_WHILE   GMB_XT_WHILE
#define TK_WEND    GMB_XT_WEND
#define TK_END     GMB_XT_END

#define ND_LIT     GMB_XN_LIT
#define ND_ID      GMB_XN_ID
#define ND_UN      GMB_XN_UN
#define ND_BIN     GMB_XN_BIN
#define ND_CALL    GMB_XN_CALL
#define ND_ASSIGN  GMB_XN_ASSIGN
#define ND_DECL    GMB_XN_DECL
#define ND_PRINT   GMB_XN_PRINT
#define ND_SEQ     GMB_XN_SEQ
#define ND_IF      GMB_XN_IF
#define ND_FOR     GMB_XN_FOR
#define ND_WHILE   GMB_XN_WHILE

typedef struct {
    const char* src;
    const char* p;
    tok_kind kind;
    char text[256];
    double num;
    int failed;
    char err[256];
    gmb_expr_get_fn get;
    gmb_expr_set_fn set;
    gmb_expr_call_fn call;
    void* env;
    /* S_eval: scope transiente dell'esecuzione corrente (push in run_ex, child in
     * eval_nested, pop a fine esecuzione). NULL = fuori eval (non accade: run_ex
     * lo crea sempre; i test diretti senza scope usano il fallback backend). */
    gmb_expr_scope_t* scope;
    /* S_eval: 1 se il PRIMO fallimento è "identificatore indefinito" (lookup): il
     * chiamante (VM/C) lo mappa a NIL invece che a errore fatale — parità col C# che
     * spinge NIL per gli ignoti. First-error-wins: impostato solo se !failed. */
    int undef;
    /* Arena a CHUNK IMMOBILI (lista): i puntatori dei nodi/array restano validi.
     * Prima era un unico blocco con realloc: alla crescita il blocco si spostava e
     * TUTTI i pointer già salvati (nodi, n->args, a/b) puntavano alla memoria liberata
     * → crash/garbage su programmi lunghi (pagine BSP: la resa di index.bsp crashava
     * in parse_print/parse_stmt_list). */
    char* arena;                 /* testa della lista arena_chunk_t */
    size_t arena_used;
    size_t arena_cap;
    int capturing;
    char* cap;
    size_t cap_len;
    size_t cap_cap;
} ctx;

static int kw_eq(const char* a, const char* b) { return strcasecmp(a, b) == 0; }

/* Ultimo errore del valutatore, per il contratto C# "MACRO ERROR: <msg>" su VM/C e C99*:
 * gmb_expr_eval_ex/exec_ex ritornano solo un codice, il messaggio va reso al chiamante. */
static _Thread_local char g_last_err[256];

static void fail(ctx* c, const char* m)
{
    if (c->failed) return;
    c->failed = 1;
    snprintf(c->err, sizeof(c->err), "%s", m ? m : "expr error");
    snprintf(g_last_err, sizeof(g_last_err), "%s", c->err);
}

const char* gmb_expr_last_error(void) { return g_last_err; }

typedef struct arena_chunk {
    struct arena_chunk* next;
    size_t used;
    size_t cap;
    char data[];
} arena_chunk_t;

static void* arena_alloc(ctx* c, size_t n)
{
    size_t need = (n + 7u) & ~7u;
    arena_chunk_t* ch = (arena_chunk_t*)c->arena;
    if (!ch || ch->used + need > ch->cap) {
        size_t cap = 8192;
        if (need + 64 > cap) cap = need + 64;
        ch = (arena_chunk_t*)malloc(sizeof(arena_chunk_t) + cap);
        if (!ch) { fail(c, "out of memory"); return NULL; }
        ch->next = (arena_chunk_t*)c->arena;
        ch->used = 0;
        ch->cap = cap;
        c->arena = (char*)ch;
    }
    void* r = ch->data + ch->used;
    ch->used += need;
    memset(r, 0, need);
    return r;
}

static node* nd_new(ctx* c, nd_kind k)
{
    node* n = (node*)arena_alloc(c, sizeof(node));
    if (n) n->kind = k;
    return n;
}

static int is_id_start(int ch) { return isalpha(ch) || ch == '_'; }
static int is_id_char(int ch) { return isalnum(ch) || ch == '_' || ch == '.'; }

static void skip_ws(ctx* c)
{
    for (;;) {
        while (*c->p && isspace((unsigned char)*c->p)) c->p++;
        /* ponytail: `'` è delimitatore di stringa Clipper/SQL, non commento VB.
         * Commenti: // e &&. Ceiling: `' resto riga` BASIC non è commento qui. */
        if (c->p[0] == '/' && c->p[1] == '/') { while (*c->p && *c->p != '\n') c->p++; continue; }
        if (c->p[0] == '&' && c->p[1] == '&') { while (*c->p && *c->p != '\n') c->p++; continue; }
        break;
    }
}

static void next_tok(ctx* c)
{
    skip_ws(c);
    const char* s = c->p;
    c->text[0] = 0;
    c->num = 0;
    if (!*s) { c->kind = TK_EOF; return; }

    if (s[0] == '.' && isalpha((unsigned char)s[1])) {
        const char* q = s + 1;
        char w[16];
        int i = 0;
        while (isalpha((unsigned char)*q) && i < 14) w[i++] = (char)*q++;
        w[i] = 0;
        if (*q == '.') q++;
        if (kw_eq(w, "AND")) { c->kind = TK_AND; c->p = q; return; }
        if (kw_eq(w, "OR"))  { c->kind = TK_OR;  c->p = q; return; }
        if (kw_eq(w, "NOT")) { c->kind = TK_NOT; c->p = q; return; }
        if (kw_eq(w, "T"))   { c->kind = TK_NUM; c->num = 1; c->p = q; return; }
        if (kw_eq(w, "F"))   { c->kind = TK_NUM; c->num = 0; c->p = q; return; }
    }

    if (isdigit((unsigned char)*s) || (*s == '.' && isdigit((unsigned char)s[1]))) {
        char* end = NULL;
        c->num = strtod(s, &end);
        c->kind = TK_NUM;
        c->p = end ? end : s + 1;
        return;
    }

    if (*s == '"' || *s == '\'') {
        char q = *s++;
        size_t n = 0;
        /* Il raddoppio `""` (escape BASIC/JSON del quote) va gestito DENTRO il loop:
         * con `while (*s && *s != q)` la guardia usciva prima di vedere la coppia,
         * quindi `PRINT "<html lang=""en"">"` dava "expected expression" (le pagine
         * BSP con attributi HTML non si potevano renderizzare col motore C). */
        while (*s) {
            if (s[0] == q) {
                if (s[1] == q) { if (n + 1 < sizeof(c->text)) c->text[n++] = q; s += 2; continue; }
                s++;            /* quote di chiusura */
                break;
            }
            if (n + 1 < sizeof(c->text)) c->text[n++] = *s;
            s++;
        }
        c->text[n] = 0;
        c->kind = TK_STR;
        c->p = s;
        return;
    }

    if (is_id_start((unsigned char)*s)) {
        size_t n = 0;
        while (is_id_char((unsigned char)*s) && n + 1 < sizeof(c->text))
            c->text[n++] = *s++;
        if (*s == '$' || *s == '%' || *s == '!' || *s == '#' || *s == '&') {
            if (n + 1 < sizeof(c->text)) c->text[n++] = *s++;
        }
        c->text[n] = 0;
        if (kw_eq(c->text, "AND")) { c->kind = TK_AND; c->p = s; return; }
        if (kw_eq(c->text, "OR"))  { c->kind = TK_OR;  c->p = s; return; }
        if (kw_eq(c->text, "NOT")) { c->kind = TK_NOT; c->p = s; return; }
        if (kw_eq(c->text, "LET")) { c->kind = TK_LET; c->p = s; return; }
        if (kw_eq(c->text, "DIM")) { c->kind = TK_DIM; c->p = s; return; }
        if (kw_eq(c->text, "STATIC")) { c->kind = TK_STATIC; c->p = s; return; }
        if (kw_eq(c->text, "REM")) {
            while (*s && *s != '\n' && *s != ':') s++;
            c->p = s;
            next_tok(c);
            return;
        }
        if (kw_eq(c->text, "PRINT")) { c->kind = TK_PRINT; c->p = s; return; }
        if (kw_eq(c->text, "IF"))    { c->kind = TK_IF;    c->p = s; return; }
        if (kw_eq(c->text, "THEN"))  { c->kind = TK_THEN;  c->p = s; return; }
        if (kw_eq(c->text, "ELSE"))  { c->kind = TK_ELSE;  c->p = s; return; }
        if (kw_eq(c->text, "ENDIF")) { c->kind = TK_ENDIF; c->p = s; return; }
        if (kw_eq(c->text, "FOR"))   { c->kind = TK_FOR;   c->p = s; return; }
        if (kw_eq(c->text, "TO"))    { c->kind = TK_TO;    c->p = s; return; }
        if (kw_eq(c->text, "NEXT"))  { c->kind = TK_NEXT;  c->p = s; return; }
        if (kw_eq(c->text, "STEP"))  { c->kind = TK_STEP;  c->p = s; return; }
        if (kw_eq(c->text, "WHILE")) { c->kind = TK_WHILE; c->p = s; return; }
        if (kw_eq(c->text, "WEND"))  { c->kind = TK_WEND;  c->p = s; return; }
        if (kw_eq(c->text, "END")) {
            const char* q = s;
            while (*q && isspace((unsigned char)*q)) q++;
            if (is_id_start((unsigned char)*q)) {
                char w[8];
                size_t i = 0;
                while (is_id_char((unsigned char)*q) && i + 1 < sizeof(w)) w[i++] = *q++;
                w[i] = 0;
                if (kw_eq(w, "IF")) { c->kind = TK_ENDIF; c->p = q; return; }
            }
            c->kind = TK_END;
            c->p = s;
            return;
        }
        /* VMBASIC: VERO = -1 (non 1) — così `BIT = TRUE` matcha il campo letto come "-1". */
        if (kw_eq(c->text, "TRUE"))  { c->kind = TK_NUM; c->num = -1; c->p = s; return; }
        if (kw_eq(c->text, "FALSE")) { c->kind = TK_NUM; c->num = 0; c->p = s; return; }
        c->kind = TK_ID;
        c->p = s;
        return;
    }

    c->p = s + 1;
    switch (*s) {
        case '+': c->kind = TK_PLUS; return;
        case '-': c->kind = TK_MINUS; return;
        case '*': c->kind = TK_STAR; return;
        case '/': c->kind = TK_SLASH; return;
        case '%': c->kind = TK_PERCENT; return;
        case '^': c->kind = TK_CARET; return;
        case '(': c->kind = TK_LP; return;
        case ')': c->kind = TK_RP; return;
        case ',': c->kind = TK_COMMA; return;
        case ':': c->kind = TK_COLON; return;
        case ';': c->kind = TK_SEMI; return;
        case '&': c->kind = TK_AMP; return;
        case '?': c->kind = TK_PRINT; return;
        case '=':
            if (*c->p == '=') c->p++;
            c->kind = TK_EQ;
            return;
        case '<':
            if (*c->p == '>') { c->p++; c->kind = TK_NE; return; }
            if (*c->p == '=') { c->p++; c->kind = TK_LE; return; }
            c->kind = TK_LT;
            return;
        case '>':
            if (*c->p == '=') { c->p++; c->kind = TK_GE; return; }
            c->kind = TK_GT;
            return;
        case '!':
            if (*c->p == '=') { c->p++; c->kind = TK_NE; return; }
            c->kind = TK_NOT;
            return;
        default:
            fail(c, "invalid character");
            c->kind = TK_EOF;
            return;
    }
}

static int gmb_tok_accept(ctx* c, tok_kind k)
{
    if (c->kind != k) return 0;
    next_tok(c);
    return 1;
}

static node* parse_expr(ctx* c);
static node* parse_unary(ctx* c);

static node* parse_args(ctx* c, node* call)
{
    int cap = 4;
    call->args = (node**)arena_alloc(c, (size_t)cap * sizeof(node*));
    if (!gmb_tok_accept(c, TK_RP)) {
        for (;;) {
            if (call->nargs >= cap) {
                cap *= 2;
                node** na = (node**)arena_alloc(c, (size_t)cap * sizeof(node*));
                if (!na) return call;
                memcpy(na, call->args, (size_t)call->nargs * sizeof(node*));
                call->args = na;
            }
            call->args[call->nargs++] = parse_expr(c);
            if (gmb_tok_accept(c, TK_COMMA)) continue;
            if (!gmb_tok_accept(c, TK_RP)) fail(c, "expected )");
            break;
        }
    }
    return call;
}

static node* parse_primary(ctx* c)
{
    if (c->failed) return nd_new(c, ND_LIT);
    if (c->kind == TK_NUM) {
        node* n = nd_new(c, ND_LIT);
        if (n) n->lit = gmb_make_val_double(c->num);
        next_tok(c);
        return n;
    }
    if (c->kind == TK_STR) {
        node* n = nd_new(c, ND_LIT);
        if (n) n->lit = gmb_make_val_string(c->text);
        next_tok(c);
        return n;
    }
    if (c->kind == TK_IF) {
        next_tok(c);
        if (c->kind != TK_LP) { fail(c, "IF in expression needs ("); return nd_new(c, ND_LIT); }
        next_tok(c);
        node* n = nd_new(c, ND_CALL);
        if (n) snprintf(n->name, sizeof(n->name), "%s", "IF");
        return parse_args(c, n);
    }
    if (c->kind == TK_ID) {
        node* n = nd_new(c, ND_ID);
        if (n) snprintf(n->name, sizeof(n->name), "%s", c->text);
        next_tok(c);
        if (c->kind == TK_LP) {
            next_tok(c);
            n->kind = ND_CALL;
            return parse_args(c, n);
        }
        return n;
    }
    if (gmb_tok_accept(c, TK_LP)) {
        node* n = parse_expr(c);
        if (!gmb_tok_accept(c, TK_RP)) fail(c, "expected )");
        return n;
    }
    fail(c, "expected expression");
    return nd_new(c, ND_LIT);
}

static node* parse_power(ctx* c)
{
    node* l = parse_primary(c);
    if (c->kind == TK_CARET) {
        next_tok(c);
        node* n = nd_new(c, ND_BIN);
        if (n) { n->op = TK_CARET; n->a = l; n->b = parse_unary(c); }
        return n;
    }
    return l;
}

static node* parse_unary(ctx* c)
{
    if (c->kind == TK_PLUS || c->kind == TK_MINUS || c->kind == TK_NOT || c->kind == TK_AMP) {
        int op = (int)c->kind;
        next_tok(c);
        node* n = nd_new(c, ND_UN);
        if (n) { n->op = op; n->a = parse_unary(c); }
        return n;
    }
    return parse_power(c);
}

static node* parse_mul(ctx* c)
{
    node* l = parse_unary(c);
    while (c->kind == TK_STAR || c->kind == TK_SLASH || c->kind == TK_PERCENT) {
        int op = (int)c->kind;
        next_tok(c);
        node* n = nd_new(c, ND_BIN);
        if (n) { n->op = op; n->a = l; n->b = parse_unary(c); }
        l = n;
    }
    return l;
}

static node* parse_add(ctx* c)
{
    node* l = parse_mul(c);
    while (c->kind == TK_PLUS || c->kind == TK_MINUS) {
        int op = (int)c->kind;
        next_tok(c);
        node* n = nd_new(c, ND_BIN);
        if (n) { n->op = op; n->a = l; n->b = parse_mul(c); }
        l = n;
    }
    return l;
}

static node* parse_cmp(ctx* c)
{
    node* l = parse_add(c);
    if (c->kind == TK_EQ || c->kind == TK_NE || c->kind == TK_LT ||
        c->kind == TK_GT || c->kind == TK_LE || c->kind == TK_GE) {
        int op = (int)c->kind;
        next_tok(c);
        node* n = nd_new(c, ND_BIN);
        if (n) { n->op = op; n->a = l; n->b = parse_add(c); }
        return n;
    }
    return l;
}

static node* parse_not(ctx* c)
{
    if (c->kind == TK_NOT) {
        next_tok(c);
        node* n = nd_new(c, ND_UN);
        if (n) { n->op = TK_NOT; n->a = parse_not(c); }
        return n;
    }
    return parse_cmp(c);
}

static node* parse_and(ctx* c)
{
    node* l = parse_not(c);
    while (c->kind == TK_AND) {
        next_tok(c);
        node* n = nd_new(c, ND_BIN);
        if (n) { n->op = TK_AND; n->a = l; n->b = parse_not(c); }
        l = n;
    }
    return l;
}

static node* parse_or(ctx* c)
{
    node* l = parse_and(c);
    while (c->kind == TK_OR) {
        next_tok(c);
        node* n = nd_new(c, ND_BIN);
        if (n) { n->op = TK_OR; n->a = l; n->b = parse_and(c); }
        l = n;
    }
    return l;
}

static node* parse_expr(ctx* c) { return parse_or(c); }

static int is_stmt_end(tok_kind k)
{
    return k == TK_EOF || k == TK_ELSE || k == TK_ENDIF
        || k == TK_NEXT || k == TK_WEND;
}

static int is_print_stop(tok_kind k)
{
    return is_stmt_end(k) || k == TK_COLON || k == TK_PRINT || k == TK_LET
        || k == TK_FOR || k == TK_WHILE || k == TK_IF || k == TK_END;
}

static int args_grow(ctx* c, node* n, int* cap)
{
    if (n->nargs < *cap) return 1;
    *cap *= 2;
    node** na = (node**)arena_alloc(c, (size_t)*cap * sizeof(node*));
    if (!na) return 0;
    memcpy(na, n->args, (size_t)n->nargs * sizeof(node*));
    n->args = na;
    return 1;
}

static node* parse_stmt_list(ctx* c);

static node* parse_print(ctx* c)
{
    node* n = nd_new(c, ND_PRINT);
    if (!n) return n;
    int cap = 4;
    n->args = (node**)arena_alloc(c, (size_t)cap * sizeof(node*));
    n->op = 0;
    if (gmb_tok_accept(c, TK_SEMI)) {
        n->op = 1;
        return n;
    }
    if (!is_print_stop(c->kind)) {
        for (;;) {
            if (!args_grow(c, n, &cap)) return n;
            n->args[n->nargs++] = parse_expr(c);
            if (gmb_tok_accept(c, TK_SEMI)) {
                n->op = 1;
                if (is_print_stop(c->kind) || c->kind == TK_SEMI) break;
                continue;
            }
            if (gmb_tok_accept(c, TK_COMMA)) {
                n->op = 0;
                if (is_print_stop(c->kind)) break;
                continue;
            }
            break;
        }
    }
    return n;
}

static node* parse_stmt(ctx* c)
{
    if (c->kind == TK_IF) {
        next_tok(c);
        if (c->kind == TK_LP) {
            node* n = nd_new(c, ND_CALL);
            if (n) snprintf(n->name, sizeof(n->name), "%s", "IF");
            next_tok(c);
            return parse_args(c, n);
        }
        node* n = nd_new(c, ND_IF);
        if (!n) return nd_new(c, ND_LIT);
        n->a = parse_expr(c);
        if (!gmb_tok_accept(c, TK_THEN)) fail(c, "expected THEN");
        n->b = parse_stmt_list(c);
        if (gmb_tok_accept(c, TK_ELSE)) {
            n->args = (node**)arena_alloc(c, sizeof(node*));
            if (n->args) { n->args[0] = parse_stmt_list(c); n->nargs = 1; }
        }
        gmb_tok_accept(c, TK_ENDIF);
        return n;
    }
    if (c->kind == TK_FOR) {
        next_tok(c);
        node* n = nd_new(c, ND_FOR);
        if (!n) return nd_new(c, ND_LIT);
        if (c->kind != TK_ID) { fail(c, "expected identifier"); return n; }
        snprintf(n->name, sizeof(n->name), "%s", c->text);
        next_tok(c);
        if (!gmb_tok_accept(c, TK_EQ)) fail(c, "expected =");
        n->a = parse_expr(c);
        if (!gmb_tok_accept(c, TK_TO)) fail(c, "expected TO");
        n->b = parse_expr(c);
        int cap = 2;
        n->args = (node**)arena_alloc(c, (size_t)cap * sizeof(node*));
        node* step = NULL;
        if (gmb_tok_accept(c, TK_STEP)) step = parse_expr(c);
        node* body = parse_stmt_list(c);
        if (n->args) {
            n->args[n->nargs++] = body;
            if (step) n->args[n->nargs++] = step;
        }
        if (!gmb_tok_accept(c, TK_NEXT)) fail(c, "expected NEXT");
        if (c->kind == TK_ID) next_tok(c);
        return n;
    }
    if (c->kind == TK_WHILE) {
        next_tok(c);
        node* n = nd_new(c, ND_WHILE);
        if (!n) return nd_new(c, ND_LIT);
        n->a = parse_expr(c);
        n->b = parse_stmt_list(c);
        if (!gmb_tok_accept(c, TK_WEND)) fail(c, "expected WEND");
        return n;
    }
    if (c->kind == TK_END) {
        next_tok(c);
        return nd_new(c, ND_LIT);
    }
    if (c->kind == TK_LET) {
        /* S_eval: LET dichiara SEMPRE nello scope corrente (shadow) → ND_DECL. */
        next_tok(c);
        if (c->kind != TK_ID) { fail(c, "expected identifier"); return nd_new(c, ND_LIT); }
        node* n = nd_new(c, ND_DECL);
        if (n) snprintf(n->name, sizeof(n->name), "%s", c->text);
        next_tok(c);
        if (!gmb_tok_accept(c, TK_EQ)) fail(c, "expected =");
        if (n) n->a = parse_expr(c);
        return n;
    }
    if (c->kind == TK_DIM) {
        /* S_eval: DIM dichiara nello scope corrente (inizializzatore opzionale,
         * clausola AS ignorata — i tipi non esistono nel valutatore; default NIL). */
        next_tok(c);
        if (c->kind != TK_ID) { fail(c, "expected identifier"); return nd_new(c, ND_LIT); }
        node* n = nd_new(c, ND_DECL);
        if (n) snprintf(n->name, sizeof(n->name), "%s", c->text);
        next_tok(c);
        if (c->kind == TK_ID && kw_eq(c->text, "AS")) {
            next_tok(c);
            if (c->kind == TK_ID) next_tok(c);
        }
        if (n) {
            if (c->kind == TK_EQ) { next_tok(c); n->a = parse_expr(c); }
            else n->a = NULL;
        }
        return n;
    }
    if (c->kind == TK_STATIC) {
        /* S_eval (vincolante): scope transiente → errore esplicito, mai no-op. */
        fail(c, "STATIC is not allowed inside EVAL/macro: eval scope is transient.");
        return nd_new(c, ND_LIT);
    }
    if (c->kind == TK_PRINT) {
        next_tok(c);
        return parse_print(c);
    }
    if (c->kind == TK_ID) {
        char name[72];
        snprintf(name, sizeof(name), "%s", c->text);
        const char* save_p = c->p;
        tok_kind save_k = c->kind;
        char save_t[256];
        memcpy(save_t, c->text, sizeof(save_t));
        double save_n = c->num;
        next_tok(c);
        if (c->kind == TK_EQ) {
            next_tok(c);
            node* n = nd_new(c, ND_ASSIGN);
            if (n) { snprintf(n->name, sizeof(n->name), "%s", name); n->a = parse_expr(c); }
            return n;
        }
        c->p = save_p;
        c->kind = save_k;
        memcpy(c->text, save_t, sizeof(c->text));
        c->num = save_n;
        snprintf(c->text, sizeof(c->text), "%s", name);
        c->kind = TK_ID;
    }
    return parse_expr(c);
}

static node* parse_stmt_list(ctx* c)
{
    while (c->kind == TK_COLON) next_tok(c);
    if (is_stmt_end(c->kind)) return nd_new(c, ND_LIT);
    node* l = parse_stmt(c);
    while (c->kind == TK_COLON) next_tok(c);
    if (is_stmt_end(c->kind)) return l;
    node* seq = nd_new(c, ND_SEQ);
    int cap = 4;
    if (!seq) return l;
    seq->args = (node**)arena_alloc(c, (size_t)cap * sizeof(node*));
    if (!seq->args) return l;
    seq->args[seq->nargs++] = l;
    while (!is_stmt_end(c->kind) && !c->failed) {
        if (!args_grow(c, seq, &cap)) return seq;
        seq->args[seq->nargs++] = parse_stmt(c);
        while (c->kind == TK_COLON) next_tok(c);
    }
    return seq;
}

static int is_truthy(gmb_val_t v)
{
    switch (v.kind) {
        case GMB_TYPE_NIL: return 0;
        case GMB_TYPE_INT: return v.v_int != 0;
        case GMB_TYPE_DOUBLE: return v.v_double != 0.0;
        case GMB_TYPE_BOOL: return v.v_bool;
        case GMB_TYPE_STRING: return v.v_string.data && v.v_string.data[0] != 0;
        default: return v.v_ptr != NULL;
    }
}

static const char* val_cstr(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return v.v_string.data;
    return "";
}

static int lookup(ctx* c, const char* name, gmb_val_t* out)
{
    /* S_eval: prima lo scope transiente (catena), poi il backend. */
    if (c->scope && gmb_expr_scope_get(c->scope, name, out)) return 0;
    if (!c->get) { if (!c->failed) c->undef = 1; fail(c, "undefined identifier"); return -1; }
    if (c->get(c->env, name, out) == 0) return 0;
    size_t n = strlen(name);
    if (n > 1) {
        char last = name[n - 1];
        if (last == '$' || last == '%' || last == '!' || last == '#' || last == '&') {
            char tmp[72];
            snprintf(tmp, sizeof(tmp), "%s", name);
            tmp[n - 1] = 0;
            if (c->scope && gmb_expr_scope_get(c->scope, tmp, out)) return 0;
            if (c->get(c->env, tmp, out) == 0) return 0;
        }
    }
    if (!c->failed) c->undef = 1;
    fail(c, "undefined identifier");
    return -1;
}

/* S_eval: scrittura con scope. decl=1 (LET/DIM, contatore FOR): sempre nel frame
 * corrente. decl=0 (implicita): catena → backend in place → crea nel corrente. */
static void scope_store(ctx* c, const char* name, gmb_val_t v, int decl)
{
    if (c->scope && decl) { gmb_expr_scope_set(c->scope, name, v); return; }
    if (c->scope && gmb_expr_scope_update(c->scope, name, v)) return;
    if (c->set && c->set(c->env, name, v) == 0) return;
    if (c->scope) { gmb_expr_scope_set(c->scope, name, v); return; }
    fail(c, "assignment failed");
}

static gmb_val_t eval_node(ctx* c, node* n);
static int run(const char* src, void* env, gmb_expr_get_fn get, gmb_expr_set_fn set,
               gmb_expr_call_fn call, int as_stmt, int eq_is_assign, gmb_val_t* result);
static int run_ex(const char* src, void* env, gmb_expr_get_fn get, gmb_expr_set_fn set,
                  gmb_expr_call_fn call, int as_stmt, int eq_is_assign,
                  gmb_val_t* result, char** out_text);
/* Come run_ex ma lo scope è figlio di outer->scope (EVAL annidato); outer==NULL = radice. */
static int run_inner(const char* src, void* env, gmb_expr_get_fn get, gmb_expr_set_fn set,
                     gmb_expr_call_fn call, int as_stmt, int eq_is_assign,
                     gmb_val_t* result, char** out_text, ctx* outer);

static int eval_nested(ctx* c, const char* src, gmb_val_t* out)
{
    gmb_val_t v = gmb_val_nil();
    /* S_eval annidato: run_inner crea il frame figlio (parent = scope corrente). */
    int rc = run_inner(src, c->env, c->get, c->set, c->call, 0, 1, &v, NULL, c);
    if (rc != GMB_EXPR_OK)
        rc = run_inner(src, c->env, c->get, c->set, c->call, 1, 1, &v, NULL, c);
    if (out) *out = v;
    return rc;
}

static const char* type_letter(gmb_val_t v)
{
    switch (v.kind) {
        case GMB_TYPE_INT:
        case GMB_TYPE_DOUBLE: return "N";
        case GMB_TYPE_STRING: return "C";
        case GMB_TYPE_BOOL: return "L";
        case GMB_TYPE_ARRAY: return "A";
        case GMB_TYPE_OBJECT: return "O";
        default: return "U";
    }
}

static gmb_val_t call_builtin(ctx* c, node* n)
{
    const char* nm = n->name;

    if ((kw_eq(nm, "IIF") || kw_eq(nm, "IF")) && n->nargs >= 3) {
        gmb_val_t cond = eval_node(c, n->args[0]);
        if (c->failed) return gmb_val_nil();
        return eval_node(c, is_truthy(cond) ? n->args[1] : n->args[2]);
    }
    if (kw_eq(nm, "EVAL") || kw_eq(nm, "MACRO")) {
        gmb_val_t a0 = n->nargs ? eval_node(c, n->args[0]) : gmb_val_nil();
        gmb_val_t r = gmb_val_nil();
        if (c->failed) return r;
        if (eval_nested(c, val_cstr(a0), &r) != GMB_EXPR_OK)
            fail(c, "nested EVAL failed");
        return r;
    }
    if (kw_eq(nm, "TYPE") && n->nargs >= 1) {
        gmb_val_t a0 = eval_node(c, n->args[0]);
        if (c->failed) return gmb_val_nil();
        const char* src = val_cstr(a0);
        gmb_val_t inner = gmb_val_nil();
        int rc = run(src, c->env, c->get, c->set, c->call, 0, 0, &inner);
        if (rc != GMB_EXPR_OK) return gmb_make_val_string("U");
        return gmb_make_val_string(type_letter(inner));
    }

    gmb_val_t args[8];
    int na = n->nargs < 8 ? n->nargs : 8;
    for (int i = 0; i < na; i++) args[i] = eval_node(c, n->args[i]);
    if (c->failed) return gmb_val_nil();

    if (kw_eq(nm, "LEN")) {
        if (na < 1) return gmb_make_val_int(0);
        return gmb_make_val_int(gmb_val_length(args[0]));
    }
    if (kw_eq(nm, "VAL")) {
        if (na < 1) return gmb_make_val_double(0);
        return gmb_make_val_double(gmb_val_to_double(args[0]));
    }
    /* INSTR(hay, needle[, start=1]): posizione 1-based (0 se assente), case-insensitive —
     * stessa semantica del valutatore C# (CommonLib.StringUtils.Instr) e della PCodeVM.
     * Prima assente dal valutatore C → un REQUIRE con INSTR era FALSO sul C e VERO sul C#. */
    if (kw_eq(nm, "INSTR")) {
        gmb_string_t hay = (na >= 1 && args[0].kind == GMB_TYPE_STRING) ? args[0].v_string
                          : gmb_val_to_string(na >= 1 ? args[0] : gmb_val_nil());
        gmb_string_t sub = (na >= 2 && args[1].kind == GMB_TYPE_STRING) ? args[1].v_string
                          : gmb_val_to_string(na >= 2 ? args[1] : gmb_val_nil());
        gmb_int_t start = na >= 3 ? gmb_val_to_int(args[2]) : 1;
        return gmb_make_val_int(gmb_str_instr(hay, sub, start));
    }
    if (kw_eq(nm, "STR") || kw_eq(nm, "STR$")) {
        gmb_string_t s = gmb_val_to_string(na ? args[0] : gmb_val_nil());
        return gmb_make_val_string(s.data ? s.data : "");
    }
    if (kw_eq(nm, "ABS"))
        return gmb_make_val_double(gmb_abs(gmb_val_to_double(args[0])));
    if (kw_eq(nm, "INT"))
        return gmb_make_val_double(gmb_int(gmb_val_to_double(args[0])));
    if (kw_eq(nm, "SQR") || kw_eq(nm, "SQRT"))
        return gmb_make_val_double(gmb_sqr(gmb_val_to_double(args[0])));
    if (kw_eq(nm, "UPPER") || kw_eq(nm, "UPPER$") || kw_eq(nm, "UCASE") || kw_eq(nm, "UCASE$")) {
        gmb_string_t in = args[0].kind == GMB_TYPE_STRING ? args[0].v_string : gmb_val_to_string(args[0]);
        gmb_string_t s = gmb_str_upper(in);
        return gmb_make_val_string(s.data ? s.data : "");
    }
    if (kw_eq(nm, "LOWER") || kw_eq(nm, "LOWER$") || kw_eq(nm, "LCASE") || kw_eq(nm, "LCASE$")) {
        gmb_string_t in = args[0].kind == GMB_TYPE_STRING ? args[0].v_string : gmb_val_to_string(args[0]);
        gmb_string_t s = gmb_str_lower(in);
        return gmb_make_val_string(s.data ? s.data : "");
    }
    if (kw_eq(nm, "TRIM") || kw_eq(nm, "TRIM$")) {
        gmb_string_t in = args[0].kind == GMB_TYPE_STRING ? args[0].v_string : gmb_val_to_string(args[0]);
        gmb_string_t s = gmb_str_trim(in);
        return gmb_make_val_string(s.data ? s.data : "");
    }
    if (kw_eq(nm, "LTRIM") || kw_eq(nm, "LTRIM$")) {
        gmb_string_t in = args[0].kind == GMB_TYPE_STRING ? args[0].v_string : gmb_val_to_string(args[0]);
        gmb_string_t s = gmb_str_ltrim(in);
        return gmb_make_val_string(s.data ? s.data : "");
    }
    if (kw_eq(nm, "RTRIM") || kw_eq(nm, "RTRIM$")) {
        gmb_string_t in = args[0].kind == GMB_TYPE_STRING ? args[0].v_string : gmb_val_to_string(args[0]);
        gmb_string_t s = gmb_str_rtrim(in);
        return gmb_make_val_string(s.data ? s.data : "");
    }
    if (kw_eq(nm, "ROUND")) {
        gmb_int_t d = na >= 2 ? gmb_val_to_int(args[1]) : 0;
        return gmb_make_val_double(gmb_round(gmb_val_to_double(args[0]), d));
    }
    if (kw_eq(nm, "MIN") && na >= 2)
        return gmb_make_val_double(gmb_min(gmb_val_to_double(args[0]), gmb_val_to_double(args[1])));
    if (kw_eq(nm, "MAX") && na >= 2)
        return gmb_make_val_double(gmb_max(gmb_val_to_double(args[0]), gmb_val_to_double(args[1])));
    if (c->call) {
        gmb_val_t out = gmb_val_nil();
        if (c->call(c->env, nm, args, na, &out) == 0) return out;
    }
    fail(c, "unknown function");
    return gmb_val_nil();
}

static int cmp_vals(gmb_val_t a, gmb_val_t b)
{
    if (a.kind == GMB_TYPE_STRING || b.kind == GMB_TYPE_STRING) {
        gmb_string_t sa = a.kind == GMB_TYPE_STRING ? a.v_string : gmb_val_to_string(a);
        gmb_string_t sb = b.kind == GMB_TYPE_STRING ? b.v_string : gmb_val_to_string(b);
        const char* da = sa.data ? sa.data : "";
        const char* db = sb.data ? sb.data : "";
        return strcmp(da, db);
    }
    double da = gmb_val_to_double(a), db = gmb_val_to_double(b);
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static void cap_append(ctx* c, const char* s)
{
    if (!c || !c->capturing || !s) return;
    size_t n = strlen(s);
    if (c->cap_len + n + 1 > c->cap_cap) {
        size_t nc = c->cap_cap ? c->cap_cap * 2 : 128;
        while (nc < c->cap_len + n + 1) nc *= 2;
        char* p = (char*)realloc(c->cap, nc);
        if (!p) { fail(c, "out of memory"); return; }
        c->cap = p;
        c->cap_cap = nc;
    }
    memcpy(c->cap + c->cap_len, s, n);
    c->cap_len += n;
    c->cap[c->cap_len] = 0;
}

/* Formattazione del PRINT identica a gmb_print_val (gmb_print_int/double/bool):
 * padding BASIC con spazio iniziale/finale per i numeri — senza, l'output CATTURATO
 * di una macro-comando (`&("PRINT 1")`) usciva "1" invece di " 1 " (bug
 * ClipperMacroCanary: ordine giusto ma spaziatura diversa da IL/VM-C). */
static void cap_append_num_double(ctx* c, double d)
{
    char buf[64];
    if (isnan(d)) snprintf(buf, sizeof(buf), "NaN");
    else if (isinf(d)) snprintf(buf, sizeof(buf), d > 0 ? "Infinity" : "-Infinity");
    else {
        snprintf(buf, sizeof(buf), "%.15g", d);
        char* e = strchr(buf, 'e');
        if (e) { *e = 'E'; if (e[1] != '-' && e[1] != '+') { memmove(e + 2, e + 1, strlen(e + 1) + 1); e[1] = '+'; } }
    }
    char out[80];
    int n = 0;
    if (d >= 0.0) out[n++] = ' ';
    int len = (int)strlen(buf);
    memcpy(out + n, buf, (size_t)len);
    n += len;
    out[n++] = ' ';
    out[n] = '\0';
    cap_append(c, out);
}

static void print_one(ctx* c, gmb_val_t v)
{
    if (!c->capturing) {
        gmb_print_val(v);
        return;
    }
    switch (v.kind) {
        case GMB_TYPE_INT: {
            /* gmb_print_int: (i>=0?" ":"") + i + " " */
            char buf[40];
            int n = 0;
            if (v.v_int >= 0) buf[n++] = ' ';
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%lld", (long long)v.v_int);
            if (n < (int)sizeof(buf) - 1) buf[n++] = ' ';
            buf[n] = '\0';
            cap_append(c, buf);
            break;
        }
        case GMB_TYPE_DOUBLE:
            cap_append_num_double(c, v.v_double);
            break;
        case GMB_TYPE_BOOL:
            /* gmb_print_bool: "-1 " / " 0 " */
            cap_append(c, v.v_bool ? "-1 " : " 0 ");
            break;
        case GMB_TYPE_STRING:
            cap_append(c, v.v_string.data ? v.v_string.data : "");
            break;
        default:
            break;
    }
}

static gmb_val_t eval_node(ctx* c, node* n)
{
    if (!n || c->failed) return gmb_val_nil();
    switch (n->kind) {
        case ND_LIT: return n->lit;
        case ND_ID: {
            gmb_val_t v = gmb_val_nil();
            lookup(c, n->name, &v);
            return v;
        }
        case ND_UN: {
            if (n->op == TK_AMP) {
                gmb_val_t inner = eval_node(c, n->a);
                gmb_val_t r = gmb_val_nil();
                if (eval_nested(c, val_cstr(inner), &r) != GMB_EXPR_OK)
                    fail(c, "nested & failed");
                return r;
            }
            gmb_val_t a = eval_node(c, n->a);
            if (n->op == TK_MINUS) return gmb_make_val_double(-gmb_val_to_double(a));
            if (n->op == TK_PLUS) return gmb_make_val_double(gmb_val_to_double(a));
            if (n->op == TK_NOT) return gmb_make_val_bool(!is_truthy(a));
            return a;
        }
        case ND_BIN: {
            if (n->op == TK_AND) {
                gmb_val_t a = eval_node(c, n->a);
                if (!is_truthy(a)) return gmb_make_val_bool(0);
                return gmb_make_val_bool(is_truthy(eval_node(c, n->b)));
            }
            if (n->op == TK_OR) {
                gmb_val_t a = eval_node(c, n->a);
                if (is_truthy(a)) return gmb_make_val_bool(1);
                return gmb_make_val_bool(is_truthy(eval_node(c, n->b)));
            }
            gmb_val_t a = eval_node(c, n->a);
            gmb_val_t b = eval_node(c, n->b);
            if (n->op == TK_PLUS) {
                if (a.kind == GMB_TYPE_STRING || b.kind == GMB_TYPE_STRING) {
                    gmb_string_t sa = a.kind == GMB_TYPE_STRING ? a.v_string : gmb_val_to_string(a);
                    gmb_string_t sb = b.kind == GMB_TYPE_STRING ? b.v_string : gmb_val_to_string(b);
                    gmb_string_t cat = gmb_string_concat(sa, sb);
                    return gmb_make_val_string(cat.data ? cat.data : "");
                }
                if (a.kind == GMB_TYPE_INT && b.kind == GMB_TYPE_INT)
                    return gmb_make_val_int(a.v_int + b.v_int);
                return gmb_make_val_double(gmb_val_to_double(a) + gmb_val_to_double(b));
            }
            if (n->op == TK_MINUS)
                return gmb_make_val_double(gmb_val_to_double(a) - gmb_val_to_double(b));
            if (n->op == TK_STAR)
                return gmb_make_val_double(gmb_val_to_double(a) * gmb_val_to_double(b));
            if (n->op == TK_SLASH) {
                double d = gmb_val_to_double(b);
                if (d == 0.0) { fail(c, "division by zero"); return gmb_val_nil(); }
                return gmb_make_val_double(gmb_val_to_double(a) / d);
            }
            if (n->op == TK_PERCENT) {
                double d = gmb_val_to_double(b);
                if (d == 0.0) { fail(c, "division by zero"); return gmb_val_nil(); }
                return gmb_make_val_double(fmod(gmb_val_to_double(a), d));
            }
            if (n->op == TK_CARET)
                return gmb_make_val_double(pow(gmb_val_to_double(a), gmb_val_to_double(b)));
            {
                int cmp = cmp_vals(a, b);
                int ok = 0;
                if (n->op == TK_EQ) ok = cmp == 0;
                else if (n->op == TK_NE) ok = cmp != 0;
                else if (n->op == TK_LT) ok = cmp < 0;
                else if (n->op == TK_GT) ok = cmp > 0;
                else if (n->op == TK_LE) ok = cmp <= 0;
                else if (n->op == TK_GE) ok = cmp >= 0;
                return gmb_make_val_bool(ok);
            }
        }
        case ND_CALL: return call_builtin(c, n);
        case ND_IF: {
            gmb_val_t cond = eval_node(c, n->a);
            if (c->failed) return gmb_val_nil();
            if (is_truthy(cond)) return eval_node(c, n->b);
            if (n->nargs >= 1) return eval_node(c, n->args[0]);
            return gmb_val_nil();
        }
        case ND_ASSIGN: {
            gmb_val_t v = eval_node(c, n->a);
            if (c->failed) return v;
            if (!c->set && !c->scope) { fail(c, "assignment without env"); return v; }
            scope_store(c, n->name, v, 0);
            return v;
        }
        case ND_DECL: {
            /* S_eval: DIM/LET dichiarano nel frame corrente (muoiono col pop). */
            gmb_val_t v = n->a ? eval_node(c, n->a) : gmb_val_nil();
            if (c->failed) return v;
            if (!c->scope) {
                if (!c->set) { fail(c, "declaration without env"); return v; }
                if (c->set(c->env, n->name, v) != 0) fail(c, "assignment failed");
                return v;
            }
            gmb_expr_scope_set(c->scope, n->name, v);
            return v;
        }
        case ND_PRINT: {
            for (int i = 0; i < n->nargs; i++) {
                gmb_val_t v = eval_node(c, n->args[i]);
                if (c->failed) return gmb_val_nil();
                if (i) {
                    if (c->capturing) cap_append(c, " ");
                    else fputc(' ', stdout);
                }
                print_one(c, v);
            }
            if (!n->op) {
                if (c->capturing) cap_append(c, "\n");
                else gmb_print_newline();
            }
            return gmb_val_nil();
        }
        case ND_FOR: {
            if (!c->set && !c->scope) { fail(c, "FOR without env"); return gmb_val_nil(); }
            double start = gmb_val_to_double(eval_node(c, n->a));
            double end = gmb_val_to_double(eval_node(c, n->b));
            double step = 1.0;
            if (n->nargs >= 2 && n->args[1])
                step = gmb_val_to_double(eval_node(c, n->args[1]));
            if (c->failed) return gmb_val_nil();
            if (step == 0.0) { fail(c, "FOR STEP 0"); return gmb_val_nil(); }
            node* body = (n->nargs >= 1) ? n->args[0] : NULL;
            int niter = 0;
            gmb_val_t last = gmb_val_nil();
            for (double i = start; ; i += step) {
                if (step > 0.0 && i > end + 1e-12) break;
                if (step < 0.0 && i < end - 1e-12) break;
                if (++niter > 10000000) { fail(c, "FOR overflow"); break; }
                gmb_val_t iv = gmb_make_val_double(i);
                /* S_eval (spec §6.1): il contatore è block-local → frame corrente. */
                scope_store(c, n->name, iv, 1);
                if (c->failed) break;
                last = eval_node(c, body);
                if (c->failed) break;
            }
            return last;
        }
        case ND_WHILE: {
            int niter = 0;
            gmb_val_t last = gmb_val_nil();
            for (;;) {
                gmb_val_t cond = eval_node(c, n->a);
                if (c->failed || !is_truthy(cond)) break;
                if (++niter > 10000000) { fail(c, "WHILE overflow"); break; }
                last = eval_node(c, n->b);
                if (c->failed) break;
            }
            return last;
        }
        case ND_SEQ: {
            gmb_val_t last = gmb_val_nil();
            for (int i = 0; i < n->nargs; i++) last = eval_node(c, n->args[i]);
            return last;
        }
        default: return gmb_val_nil();
    }
}

int gmb_expr_parse(const char* src, int as_stmt, int eq_is_assign, gmb_expr_prog* out)
{
    ctx c;
    if (!out) return GMB_EXPR_ERR_PARSE;
    memset(out, 0, sizeof(*out));
    memset(&c, 0, sizeof(c));
    c.src = src ? src : "";
    c.p = c.src;
    next_tok(&c);
    node* root = as_stmt ? parse_stmt_list(&c) : parse_expr(&c);
    if (eq_is_assign && !as_stmt && !c.failed && root && root->kind == ND_BIN && root->op == TK_EQ &&
        root->a && root->a->kind == ND_ID) {
        node* asg = nd_new(&c, ND_ASSIGN);
        if (asg) { snprintf(asg->name, sizeof(asg->name), "%s", root->a->name); asg->a = root->b; }
        root = asg;
    }
    if (!c.failed && c.kind != TK_EOF && !(as_stmt && c.kind == TK_COLON) &&
        !(as_stmt && c.kind == TK_ENDIF))
        fail(&c, "trailing input");
    out->root = root;
    out->arena = c.arena;
    out->failed = c.failed;
    snprintf(out->err, sizeof(out->err), "%s", c.err);
    return c.failed ? GMB_EXPR_ERR_PARSE : GMB_EXPR_OK;
}

void gmb_expr_prog_free(gmb_expr_prog* p)
{
    if (!p) return;
    arena_chunk_t* ch = (arena_chunk_t*)p->arena;
    while (ch) { arena_chunk_t* nx = ch->next; free(ch); ch = nx; }
    p->arena = NULL;
    memset(p, 0, sizeof(*p));
}

static int run_inner(const char* src, void* env, gmb_expr_get_fn get, gmb_expr_set_fn set,
                     gmb_expr_call_fn call, int as_stmt, int eq_is_assign,
                     gmb_val_t* result, char** out_text, ctx* outer)
{
    gmb_expr_prog prog;
    int prc = gmb_expr_parse(src, as_stmt, eq_is_assign, &prog);
    ctx c;
    memset(&c, 0, sizeof(c));
    c.get = get;
    c.set = set;
    c.call = call;
    c.env = env;
    c.capturing = out_text != NULL;
    c.failed = prog.failed;
    if (prog.err[0]) snprintf(c.err, sizeof(c.err), "%s", prog.err);
    /* S_eval: frame figlio dell'outer (EVAL annidato) o radice (EVAL top-level). */
    c.scope = gmb_expr_scope_push(outer ? outer->scope : NULL);
    gmb_val_t v = gmb_val_nil();
    if (prc == GMB_EXPR_OK) v = eval_node(&c, prog.root);
    /* First-error-wins: undef (identificatore ignoto) solo se PRIMO fallimento. */
    int rc = (prc != GMB_EXPR_OK) ? prc : (c.failed ? (c.undef ? GMB_EXPR_ERR_UNDEF : GMB_EXPR_ERR_EVAL) : GMB_EXPR_OK);
    gmb_expr_scope_pop(c.scope);
    c.scope = NULL;
    if (result) *result = v;
    if (out_text) {
        *out_text = c.cap ? c.cap : (char*)calloc(1, 1);
        c.cap = NULL;
    } else {
        free(c.cap);
    }
    gmb_expr_prog_free(&prog);
    return rc;
}

static int run_ex(const char* src, void* env, gmb_expr_get_fn get, gmb_expr_set_fn set,
                  gmb_expr_call_fn call, int as_stmt, int eq_is_assign,
                  gmb_val_t* result, char** out_text)
{
    return run_inner(src, env, get, set, call, as_stmt, eq_is_assign, result, out_text, NULL);
}

static int run(const char* src, void* env, gmb_expr_get_fn get, gmb_expr_set_fn set,
               gmb_expr_call_fn call, int as_stmt, int eq_is_assign, gmb_val_t* result)
{
    return run_ex(src, env, get, set, call, as_stmt, eq_is_assign, result, NULL);
}

int gmb_expr_eval_ex(const char* src, void* env,
                     gmb_expr_get_fn get, gmb_expr_set_fn set,
                     gmb_expr_call_fn call, gmb_val_t* result)
{
    if (!src) src = "";
    while (*src && isspace((unsigned char)*src)) src++;
    if (!*src) {
        if (result) *result = gmb_val_nil();
        return GMB_EXPR_OK;
    }
    gmb_val_t v = gmb_val_nil();
    int rc = run(src, env, get, set, call, 0, 1, &v);
    if (rc != GMB_EXPR_OK)
        rc = run(src, env, get, set, call, 1, 1, &v);
    if (result) *result = v;
    return rc;
}

int gmb_expr_eval(const char* src, void* env,
                  gmb_expr_get_fn get, gmb_expr_set_fn set,
                  gmb_val_t* result)
{
    return gmb_expr_eval_ex(src, env, get, set, NULL, result);
}

int gmb_expr_eval_filter(const char* src, void* env,
                         gmb_expr_get_fn get, gmb_expr_set_fn set,
                         gmb_val_t* result)
{
    gmb_val_t unused = gmb_val_nil();
    if (!src) src = "";
    while (*src && isspace((unsigned char)*src)) src++;
    if (!*src) {
        if (result) *result = gmb_make_val_bool(1);
        return GMB_EXPR_OK;
    }
    return run(src, env, get, set, NULL, 0, 0, result ? result : &unused);
}

int gmb_expr_eval_filter_ex(const char* src, void* env,
                            gmb_expr_get_fn get, gmb_expr_set_fn set,
                            gmb_expr_call_fn call,
                            gmb_val_t* result)
{
    gmb_val_t unused = gmb_val_nil();
    if (!src) src = "";
    while (*src && isspace((unsigned char)*src)) src++;
    if (!*src) {
        if (result) *result = gmb_make_val_bool(1);
        return GMB_EXPR_OK;
    }
    return run(src, env, get, set, call, 0, 0, result ? result : &unused);
}

int gmb_expr_exec_ex(const char* src, void* env,
                     gmb_expr_get_fn get, gmb_expr_set_fn set,
                     gmb_expr_call_fn call)
{
    gmb_val_t unused = gmb_val_nil();
    return run(src ? src : "", env, get, set, call, 1, 1, &unused);
}

int gmb_expr_exec(const char* src, void* env,
                  gmb_expr_get_fn get, gmb_expr_set_fn set)
{
    return gmb_expr_exec_ex(src, env, get, set, NULL);
}

int gmb_expr_exec_capture(const char* src, void* env,
                          gmb_expr_get_fn get, gmb_expr_set_fn set,
                          gmb_expr_call_fn call, char** out_text)
{
    gmb_val_t unused = gmb_val_nil();
    return run_ex(src ? src : "", env, get, set, call, 1, 1, &unused, out_text);
}

int gmb_expr_dict_get(void* env, const char* name, gmb_val_t* out)
{
    gmb_dict_t* d = (gmb_dict_t*)env;
    if (!d || !name || !out) return -1;
    for (size_t i = 0; i < d->count; i++) {
        if (d->items[i].key && strcasecmp(d->items[i].key, name) == 0) {
            *out = d->items[i].value;
            return 0;
        }
    }
    return -1;
}

int gmb_expr_dict_set(void* env, const char* name, gmb_val_t in)
{
    gmb_dict_t* d = (gmb_dict_t*)env;
    if (!d || !name) return -1;
    for (size_t i = 0; i < d->count; i++) {
        if (d->items[i].key && strcasecmp(d->items[i].key, name) == 0) {
            d->items[i].value = in;
            return 0;
        }
    }
    gmb_dict_set(d, name, in);
    return 0;
}
