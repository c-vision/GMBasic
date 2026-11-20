/* native/gmb_runtime_c/src/parser/gmb_parser_expressions.c — speculare a Parser.Expressions.cs */
#include "gmb_parser_internal.h"
#include "gmb_ns.h"
#include "gmb_ns_lookup.h"
#include "gmb_fn_lookup.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

gmb_expr_t* gmb_p_parse_primary(gmb_parser_t* p);

/* Canone data/ora: TIME$/TI$ = STRTIME, DATE$ = STRDATE (alias a stringa: il '$' non e' un
 * semplice suffisso di tipo, TIME$ non e' TIME). Unknown se il nome non e' un alias. */
static gmb_tokenkind_t gmb_p_dollar_alias(const char* n)
{
    if (!n) return GMB_TOK_Unknown;
    if (strcasecmp(n, "TIME$") == 0 || strcasecmp(n, "TI$") == 0) return GMB_TOK_StrTime;
    if (strcasecmp(n, "DATE$") == 0) return GMB_TOK_StrDate;
    /* Grafia legacy con '$' di una funzione builtin (`LCASE$` → LCASE/Lower, `LEFT$` → Left):
     * il '$' è il tipo di ritorno, il nome canonico è quello senza. Stesso criterio del
     * linker C# (Linker.Rewrites: `if (bareNm.EndsWith('$')) bareNm = bareNm[..^1]`).
     * Senza questo, `LCASE$(x)`/`UCASE$(x)` restavano identificatori sconosciuti → il C99
     * falliva con "funzione non definita" e non compilava più `IMPORT PKGTOOLS`
     * (shared/include/PkgTools.inc:81 usa `LCASE$(kind)`). TIME$/DATE$ sono già usciti
     * sopra perché il nome senza '$' NON è il loro equivalente (TIME$ = STRTIME). */
    size_t ln = strlen(n);
    if (ln > 1 && n[ln - 1] == '$') {
        char base[128];
        if (ln - 1 < sizeof(base)) {
            memcpy(base, n, ln - 1);
            base[ln - 1] = 0;
            gmb_tokenkind_t t = gmb_fn_tok(base);
            if (t != GMB_TOK_Unknown) return t;
        }
    }
    return GMB_TOK_Unknown;
}
gmb_expr_t* gmb_p_parse_power(gmb_parser_t* p);

/* Espansione a COMPILE-TIME di una macro con target COSTANTE (letterale stringa) — la
 * semantica Clipper 5.3 compila le macro costanti una sola volta. `&("...")`/`EVAL("...")`:
 * il testo viene ri-lexato/parsato ORA e il nodo risultante sostituito all'AST, così un
 * `&("{ |x| ... }")` diventa una LAMBDA tipizzata (invocabile con parità su tutte e 5 le
 * gambe) senza dipendere dal motore macro a runtime (che sul C99Native non compila block).
 * Ritorna NULL quando la macro deve restare a RUNTIME: target non letterale, testo non
 * parsabile come espressione (comandi PRINT/LET/...), o uguaglianza top-level
 * ("X = 99" in posizione espressione è un confronto valido ma l'utente intende
 * l'assegnazione — la re-interpreta il percorso runtime). */
static gmb_expr_t* gmb_p_try_expand_const_macro(gmb_parser_t* ctx, gmb_expr_t* target, int line)
{
    (void)line;
    if (!target || target->kind != GMB_EXPR_STR || !target->str || !target->str[0]) return NULL;
    /* Il testo va lexato con una parentesi di guardia: un numero a INIZIO stringa
     * ("42") verrebbe lexato come LineNumber (numeri di riga BASIC) e non come
     * espressione. "(" + testo + ")" evita il caso e il parse_expr scarta il gruppo. */
    size_t _tl = strlen(target->str);
    char* _wrapped = (char*)malloc(_tl + 3);
    _wrapped[0] = '(';
    memcpy(_wrapped + 1, target->str, _tl);
    _wrapped[_tl + 1] = ')';
    _wrapped[_tl + 2] = 0;
    gmb_lex_result_t lx = gmb_lexer_scan(_wrapped, (long)(_tl + 2));
    gmb_parser_t sp = {0};
    sp.toks = lx.tokens;
    sp.n = lx.count;
    sp.error = NULL;
    /* Il testo è parte dello STESSO modulo: il sub-parse eredita il contesto del padre per il
     * gate dei namespace (IMPORT/default, nomi dichiarati, procedure utente) — senza questo
     * `EVAL("ANGLE2D(...)")` di un letterale veniva rifiutato qui (programma vuoto) e ricadeva
     * sulla valutazione a runtime, che non ha il dispatcher dei builtin di libreria. */
    sp.parent = ctx;
    gmb_expr_t* e = gmb_p_parse_expr(&sp);
    if (e && sp.error) { gmb_expr_free(e); e = NULL; }
    if (e) {
        while (sp.pos < sp.n && sp.toks[sp.pos].kind == GMB_TOK_Newline) sp.pos++;
        if (!(sp.pos >= sp.n || sp.toks[sp.pos].kind == GMB_TOK_Eof)) { gmb_expr_free(e); e = NULL; }
    }
    if (e && e->kind == GMB_EXPR_BINARY && e->op && strcmp(e->op, "=") == 0) { gmb_expr_free(e); e = NULL; }
    /* Nome NUDO ("ghostVar"/"X"): NON espandere a compile-time — il runtime lo risolve
       nello scope macro (globale/locale/transiente), mentre l'espansione creerebbe un
       globale implicito nel chiamante (leak di EVAL("ghostVar")). */
    if (e && e->kind == GMB_EXPR_VAR) { gmb_expr_free(e); e = NULL; }
    if (sp.error) free(sp.error);
    gmb_lex_result_free(&lx);
    free(_wrapped);
    return e;
}

/* Macro su VARIABILE con contenuto costante noto (DIM <name> = "literal", mai
 * riassegnata): espande a compile-time come la macro costante — così &(cBlockDef)/
 * EVAL(cBlockDef) con un code block letterale diventano una lambda tipizzata anche sui
 * transpiler C, che non hanno un compilatore BASIC a runtime (bug macro-variabile-block). */
static gmb_expr_t* gmb_p_try_expand_const_var(gmb_parser_t* p, gmb_expr_t* target, int line)
{
    if (!target || target->kind != GMB_EXPR_VAR || !target->var) return NULL;
    const char* cv = gmb_p_cstr_get(p, target->var);
    if (!cv || !cv[0]) return NULL;
    gmb_expr_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.kind = GMB_EXPR_STR;
    tmp.str = (char*)cv;
    return gmb_p_try_expand_const_macro(p, &tmp, line);
}
gmb_expr_t* gmb_p_parse_unary(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_mul(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_add(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_cmp(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_not(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_and(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_xor(gmb_parser_t* p);
gmb_expr_t* gmb_p_parse_or(gmb_parser_t* p);

/* .AND. / .OR. / .NOT. (Clipper): Dot + keyword + Dot */
static bool gmb_p_is_dot_op(gmb_parser_t* p, const char* kw)
{
    const gmb_token_t* a = gmb_p_cur(p);
    const gmb_token_t* b = gmb_p_peek(p, 1);
    const gmb_token_t* c = gmb_p_peek(p, 2);
    if (!a || a->kind != GMB_TOK_Dot || !b || !c || c->kind != GMB_TOK_Dot) return false;
    size_t wl = strlen(kw);
    if ((size_t)b->length != wl) return false;
    int r = strncasecmp(b->lexeme, kw, wl);
    return r == 0;
}
/* Bug reale: mancava GMB_TOK_RightBrace qui — l'euristica "numero seguito da qualcosa che
 * NON è un operatore" (usata per riconoscere/scartare un token line-number spurio dopo una
 * virgola, es. vecchio BASIC con numeri di riga) scambiava l'ULTIMO elemento di un literal
 * {1, 2, 3} (che è seguito da '}', non riconosciuto) per un numero di riga e lo SALTAVA:
 * {1,2,3} produceva un array di 2 elementi [1,2], sempre n-1 per qualunque n>1. Il literal a
 * quadre [1,2,3] non aveva il bug perché GMB_TOK_RightBracket era già in lista. */
static bool gmb_p_is_num_op_follower(gmb_parser_t* p)
{
    const gmb_token_t* n = gmb_p_peek(p, 1);
    if (!n) return false;
    switch (n->kind) {
        case GMB_TOK_RightParen: case GMB_TOK_Comma: case GMB_TOK_RightBracket:
        case GMB_TOK_RightBrace:
        case GMB_TOK_Plus: case GMB_TOK_Minus: case GMB_TOK_Multiply: case GMB_TOK_Divide:
        case GMB_TOK_IntegerDivide: case GMB_TOK_Power: case GMB_TOK_Mod:
        case GMB_TOK_Equal: case GMB_TOK_NotEqual: case GMB_TOK_LessThan:
        case GMB_TOK_LessEqual: case GMB_TOK_GreaterThan: case GMB_TOK_GreaterEqual:
        case GMB_TOK_Dot: case GMB_TOK_Eof:
            return true;
        default:
            return false;
    }
}
static bool gmb_p_match_dot_op(gmb_parser_t* p, const char* kw)
{
    if (!gmb_p_is_dot_op(p, kw)) return false;
    gmb_p_adv(p); gmb_p_adv(p); gmb_p_adv(p);
    return true;
}

/* Risolve la qualifica namespace di un nome di call (regola d'oro: il namespace NON si
 * dispatcha mai — l'emitter riceve il NOME NUDO). Se "A.B.C" con A namespace noto
 * (gmb_ns_names), ritorna il puntatore all'ultimo segmento "C" (dentro name) e scrive
 * il namespace "A" (o "A.B" annidato) in ns_out (se non NULL). Se il primo segmento NON
 * è un namespace noto (oggetto/UDT/metodo), ritorna NULL = nome invariato. */
static const char* gmb_p_strip_ns(gmb_parser_t* p, char* name, int len, char* ns_out, int ns_cap) {
    /* trova l'ultimo '.' — se nessuno, non qualificato */
    char* last = NULL;
    for (int i = 0; i < len; i++) if (name[i] == '.') last = &name[i];
    if (!last) return NULL;
    /* prova i prefissi: il primo segmento (A) o A.B (namespace annidati) deve essere noto */
    *last = 0;
    int is_ns = 0;
    int nsl = (int)strlen(name);
    for (int i = 0; i < (int)GMB_NS_COUNT; i++) {
        if (!strcasecmp(name, gmb_ns_names[i])) { is_ns = 1; break; }
    }
    /* namespace UTENTE (direttiva NAMESPACE nel programma, es. package protobuf): rende
     * `NS.MEMBRO` qualificato anche per namespace non-LangNamespace. */
    if (!is_ns && p) {
        for (int i = 0; i < p->ns_decls_n; i++)
            if (!strcasecmp(name, p->ns_decls[i])) { is_ns = 1; break; }
    }
    if (!is_ns) {
        /* namespace annidato a 2 segmenti: A.B.X con "A.B" noto */
        char* dot2 = NULL;
        for (int i = 0; name[i]; i++) if (name[i] == '.') dot2 = &name[i];
        if (dot2) {
            *dot2 = 0;
            for (int i = 0; i < (int)GMB_NS_COUNT; i++) {
                if (!strcasecmp(name, gmb_ns_names[i])) { is_ns = 1; break; }
            }
            *dot2 = '.';
        }
    }
    int alias_i = -1;
    if (!is_ns && p) {
        /* alias di namespace da IMPORT <NS> AS <X>: <X>.FUNC e' un builtin qualificato.
         * Se l'alias ha un ns FISICO registrato (import_aliases_ns), la qualifica risolve
         * a quel namespace (es. IMPORT TRADING.PORTFOLIO AS TP → ns fisico TRADING.PORTFOLIO),
         * cosi' il lookup canonico (ns, nome) funziona anche via alias. name NON si tocca
         * (buffer della call); il ns fisico finisce solo in ns_out. */
        for (int i = 0; i < p->import_aliases_n; i++)
            if (!strcasecmp(name, p->import_aliases[i])) { is_ns = 1; alias_i = i; break; }
    }
    if (is_ns && ns_out && ns_cap > 0) {
        const char* src = name;
        int srcl = nsl;
        if (alias_i >= 0 && p->import_aliases_ns[alias_i][0]) { src = p->import_aliases_ns[alias_i]; srcl = (int)strlen(src); }
        int cp = srcl < ns_cap - 1 ? srcl : ns_cap - 1;
        memcpy(ns_out, src, (size_t)cp); ns_out[cp] = 0;
    }
    *last = '.';
    if (!is_ns) return NULL;
    return last + 1;
}

/* lookup canonico (namespace, lexeme di registro) → TokenKind. La tabella gmb_ns_lookup è
 * GENERATA dalla StdLib (tools/stdlib_tokens.csv): copre i conflitti tra namespace (lo
 * stesso lexeme SHARPERATIO → SharpeRatio in METRICS, PortfolioSharpeRatio in PORTFOLIO) e i
 * lexeme diversi dal nome enum (EXPECTEDRETURN → PortfolioExpectedReturn). MAI dispatch per
 * lexeme: il lexeme è solo la grafia di registro usata per risolvere il DispatchToken. */
/* Metodi STATICI delle CLASSI builtin: NON fanno parte della superficie "free procedure" della
 * StdLib, quindi non finiscono in gmb_ns_lookup.h (generato da stdlib_tokens.csv) e nemmeno in
 * gmb_fn_lookup.h. Caso reale 2026-10-28: `KEY.MAKE(key, mod)` è un MethodMember della
 * ClassDefinition "KEY" (StdLib.Console.cs:365, SymbolFlags.IsStatic|IsMethod) → il frontend C
 * rispondeva "simbolo non trovato: funzione o subroutine 'KEY.MAKE' non definita", benché
 * l'emitter abbia già l'handler (GMB_TOK_Make → gf_key_make in gmb_c99_emit_builtin.c).
 * Tabella MINIMA e dichiarata: la verità resta la StdLib; un nuovo statico di classe builtin va
 * aggiunto qui e, a tendere, esposto dal registry canonico. */
static gmb_tokenkind_t gmb_p_class_static_lookup(const char* ns, const char* name)
{
    if (strcasecmp(ns, "KEY") == 0 && strcasecmp(name, "MAKE") == 0) return GMB_TOK_Make;
    return GMB_TOK_Unknown;
}
static gmb_tokenkind_t gmb_p_ns_lookup(const char* ns, const char* name)
{
    /* Suffisso tipo BASIC ($ % ! #) sul membro qualificato (es. `AI.CONSISTENCYDETAIL$`):
     * il lexer conserva il suffisso nel lexeme (cambia solo il kind) → la lookup
     * esatta mancherebbe. Il suffisso è solo il tipo di ritorno: spogliane UNO in
     * coda prima del confronto (parità C# "$ suffix stripping"). */
    char bare[128];
    size_t bl = strlen(name);
    if (bl >= sizeof(bare)) bl = sizeof(bare) - 1;
    memcpy(bare, name, bl); bare[bl] = 0;
    if (bl > 1 && (bare[bl - 1] == '$' || bare[bl - 1] == '%' || bare[bl - 1] == '!'
            || bare[bl - 1] == '#'))
        bare[bl - 1] = 0;
    for (int i = 0; i < GMB_NS_LOOKUP_COUNT; i++) {
        const gmb_ns_lookup_row_t* r = &gmb_ns_lookup[i];
        if (strcasecmp(r->ns, ns) == 0 && strcasecmp(r->name, bare) == 0)
            return r->tk; /* valore enum emesso a compile-time (tabella generata), mai from_name */
    }
    return GMB_TOK_Unknown;
}

/* comprehension LINQ `FROM <v> IN <src> [WHERE <c>] [ORDER BY <o> [DESC|ASC]] SELECT <sel>`
 * (specchio ParseLinqQuery/CompileListComprehension del C#). Si parsa SOLO quando FROM apre
 * un'espressione (es. `DIM t = FROM ... SELECT ...`). */
static gmb_expr_t* gmb_p_parse_comprehension(gmb_parser_t* p)
{
    int line = gmb_p_cur(p) ? gmb_p_cur(p)->line : 0;
    gmb_p_adv(p); /* FROM */
    const gmb_token_t* vt = gmb_p_cur(p);
    if (!vt || (vt->kind != GMB_TOK_Identifier && vt->kind != GMB_TOK_Unknown)) {
        gmb_p_set_err(p, "comprehension: attesa variabile di iterazione dopo FROM", line);
        return gmb_expr_num(0, line);
    }
    char vname[128]; int vl = vt->length < 127 ? vt->length : 127;
    memcpy(vname, vt->lexeme, (size_t)vl); vname[vl] = 0;
    int vline = vt->line;
    gmb_p_adv(p);
    if (!gmb_p_match(p, GMB_TOK_In)) { gmb_p_set_err(p, "comprehension: atteso IN dopo FROM <var>", line); return gmb_expr_num(0, line); }
    gmb_expr_t* src = gmb_p_parse_expr(p);
    gmb_expr_t* where = NULL; gmb_expr_t* order = NULL; gmb_expr_t* sel = NULL; int desc = 0;
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Where || gmb_p_tok_is_keyword(gmb_p_cur(p), "Where"))) {
        gmb_p_adv(p); where = gmb_p_parse_expr(p);
    }
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Order || gmb_p_tok_is_keyword(gmb_p_cur(p), "Order"))) {
        gmb_p_adv(p);
        if (!(gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_By || gmb_p_tok_is_keyword(gmb_p_cur(p), "By")))) {
            gmb_p_set_err(p, "comprehension: atteso BY dopo ORDER", line); return gmb_expr_num(0, line);
        }
        gmb_p_adv(p); order = gmb_p_parse_expr(p);
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Desc || gmb_p_tok_is_keyword(gmb_p_cur(p), "Desc"))) { desc = 1; gmb_p_adv(p); }
        else if (gmb_p_cur(p) && gmb_p_tok_is_keyword(gmb_p_cur(p), "Asc")) { desc = 0; gmb_p_adv(p); }
    }
    if (!(gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Select || gmb_p_tok_is_keyword(gmb_p_cur(p), "Select")))) {
        gmb_p_set_err(p, "comprehension: atteso SELECT", line); return gmb_expr_num(0, line);
    }
    gmb_p_adv(p); sel = gmb_p_parse_expr(p);
    return gmb_expr_comp(src, vname, where, order, desc, sel, vline);
}

/* Lookahead: il letterale `[`/`{` corrente è una comprehension (contiene un FOR top-level) */
static bool gmb_p_lit_comp_probe(gmb_parser_t* p)
{
    int depth = 0;
    for (int i = p->pos; i < p->n; i++) {
        gmb_tokenkind_t k = p->toks[i].kind;
        if (k == GMB_TOK_LeftBracket || k == GMB_TOK_LeftBrace) depth++;
        else if (k == GMB_TOK_RightBracket || k == GMB_TOK_RightBrace) { depth--; if (depth <= 0) return false; }
        else if (depth == 1 && (k == GMB_TOK_For || gmb_p_tok_is_keyword(&p->toks[i], "For"))) return true;
    }
    return false;
}

/* Comprehension letterale `[ out FOR v IN src ... [IF cond] ]` (list), `{ out FOR ... }` (set),
 * `{ k : v FOR ... }` (dict). Multi-FOR nidificato supportato. */
static gmb_expr_t* gmb_p_parse_lit_comprehension(gmb_parser_t* p, int line)
{
    int open = p->toks[p->pos].kind;
    int is_brace = (open == GMB_TOK_LeftBrace);
    int close_kind = is_brace ? GMB_TOK_RightBrace : GMB_TOK_RightBracket;
    gmb_p_adv(p);
    gmb_expr_t* e0 = gmb_p_parse_expr(p);
    gmb_expr_t* key = NULL; gmb_expr_t* out;
    if (is_brace && gmb_p_check(p, GMB_TOK_Colon)) {
        gmb_p_adv(p); key = e0; out = gmb_p_parse_expr(p);
    } else { out = e0; }
    int ctype = is_brace ? (key ? 1 : 2) : 0;   /* list / dict / set */
    if (!(gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_For || gmb_p_tok_is_keyword(gmb_p_cur(p), "For")))) {
        gmb_p_set_err(p, "comprehension: attesa FOR dopo l'elemento", line); return gmb_expr_num(0, line);
    }
    const int MAXC = 16;
    const char* vars[MAXC]; gmb_expr_t* srcs[MAXC]; int nsrc = 0;
    while (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_For || gmb_p_tok_is_keyword(gmb_p_cur(p), "For"))) {
        gmb_p_adv(p);
        const gmb_token_t* vt = gmb_p_cur(p);
        if (!vt || (vt->kind != GMB_TOK_Identifier && vt->kind != GMB_TOK_Unknown)) {
            gmb_p_set_err(p, "comprehension: attesa variabile dopo FOR", line); return gmb_expr_num(0, line);
        }
        int vl = vt->length < 63 ? vt->length : 63; char nm[64];
        memcpy(nm, vt->lexeme, (size_t)vl); nm[vl] = 0;
        int vline = vt->line;
        gmb_p_adv(p);
        if (!gmb_p_match(p, GMB_TOK_In)) { gmb_p_set_err(p, "comprehension: atteso IN dopo FOR <var>", line); return gmb_expr_num(0, line); }
        gmb_expr_t* src = gmb_p_parse_expr(p);
        if (nsrc < MAXC) { vars[nsrc] = strdup(nm); srcs[nsrc] = src; nsrc++; }
    }
    if (nsrc == 0) { gmb_p_set_err(p, "comprehension: clausola FOR mancante", line); return gmb_expr_num(0, line); }
    gmb_expr_t* where = NULL;
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_If || gmb_p_tok_is_keyword(gmb_p_cur(p), "If"))) {
        gmb_p_adv(p); where = gmb_p_parse_expr(p);
    }
    /* ORDER BY opzionale, SOLO nella comprehension LISTA (`[ ... ORDER BY k [DESC] ]`) —
     * parità con la forma LINQ (gmb_p_parse_comprehension). Per dict/set l'ordine non ha
     * senso e non è emesso. Prima la forma letterale dava "attesa parentesi di chiusura". */
    gmb_expr_t* order = NULL; int desc = 0;
    if (!is_brace && gmb_p_cur(p)
        && (gmb_p_cur(p)->kind == GMB_TOK_Order || gmb_p_tok_is_keyword(gmb_p_cur(p), "Order"))) {
        gmb_p_adv(p);
        if (!(gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_By || gmb_p_tok_is_keyword(gmb_p_cur(p), "By")))) {
            gmb_p_set_err(p, "comprehension: atteso BY dopo ORDER", line); return gmb_expr_num(0, line);
        }
        gmb_p_adv(p); order = gmb_p_parse_expr(p);
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Desc || gmb_p_tok_is_keyword(gmb_p_cur(p), "Desc"))) { desc = 1; gmb_p_adv(p); }
        else if (gmb_p_cur(p) && gmb_p_tok_is_keyword(gmb_p_cur(p), "Asc")) { desc = 0; gmb_p_adv(p); }
    }
    if (!gmb_p_match(p, (gmb_tokenkind_t)close_kind)) gmb_p_set_err(p, "comprehension: attesa parentesi di chiusura", line);
    gmb_expr_t* node = gmb_expr_comp(srcs[0], vars[0], where, order, desc, out, line);
    node->comp_ctype = ctype; node->comp_key = key;
    if (nsrc > 1) {
        node->comp_more_src = (gmb_expr_t**)calloc((size_t)(nsrc - 1), sizeof(gmb_expr_t*));
        node->comp_more_var = (char**)calloc((size_t)(nsrc - 1), sizeof(char*));
        for (int i = 1; i < nsrc; i++) { node->comp_more_src[i - 1] = srcs[i]; node->comp_more_var[i - 1] = vars[i]; }
        node->n_comp_more = nsrc - 1;
    }
    return node;
}

gmb_expr_t* gmb_p_parse_expr(gmb_parser_t* p)
{
    /* comprehension LINQ: `FROM <v> IN ...` apre un'espressione */
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_From || gmb_p_tok_is_keyword(gmb_p_cur(p), "From")))
        return gmb_p_parse_comprehension(p);
    /* assegnazione inline Clipper: pos := expr  → _assign_(pos, expr) */
    if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Identifier || gmb_p_cur(p)->kind >= GMB_TOK_Define)
        && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_InlineAssign) {
        const gmb_token_t* tgt = gmb_p_cur(p);
        char nm[64]; int l = tgt->length < 63 ? tgt->length : 63;
        memcpy(nm, tgt->lexeme, (size_t)l); nm[l] = 0;
        int ln = tgt->line;
        gmb_p_adv(p); gmb_p_adv(p);
        gmb_expr_t* rhs = gmb_p_parse_expr(p);
        gmb_expr_t** args = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
        args[0] = gmb_expr_var(nm, ln); args[1] = rhs;
        return gmb_expr_call("_assign_", args, 2, ln);
    }
    {
        gmb_expr_t* e = gmb_p_parse_or(p);
        /* `a ?? b` — coalescing: se a è NIL usa b (right-assoc). Prima il token era
         * lessato (GMB_TOK_QuestionMarkQuestion) ma NESSUN punto del parser lo consumava:
         * `x = d[k]? ?? -1` ignorava il fallback (CollectionsSafeAccess). */
        while (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_QuestionMarkQuestion) {
            int ln = gmb_p_cur(p)->line;
            gmb_p_adv(p);
            gmb_expr_t* r = gmb_p_parse_expr(p);
            gmb_expr_t** ca = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
            ca[0] = e; ca[1] = r;
            e = gmb_expr_call("_coalesce_", ca, 2, ln);
        }
        return e;
    }
}

gmb_expr_t* gmb_p_parse_or(gmb_parser_t* p)
{
    gmb_expr_t* e = gmb_p_parse_xor(p);
    for (;;) {
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Or") || (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Or || gmb_p_cur(p)->kind == GMB_TOK_BitOr))) {
            const gmb_token_t* op = gmb_p_cur(p); gmb_p_adv(p);
            e = gmb_expr_binary(op->kind == GMB_TOK_BitOr ? "||" : "OR", e, gmb_p_parse_xor(p), op->line);
        } else if (gmb_p_match_dot_op(p, "OR")) {
            e = gmb_expr_binary(p->toks[p->pos - 1].kind == GMB_TOK_BitOr ? "||" : "OR", e, gmb_p_parse_xor(p), p->pos > 0 ? p->toks[p->pos - 1].line : 0);
        } else break;
    }
    return e;
}
/* XOR bitwise (precedenza OR < XOR < AND). */
gmb_expr_t* gmb_p_parse_xor(gmb_parser_t* p)
{
    gmb_expr_t* e = gmb_p_parse_and(p);
    for (;;) {
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Xor") || (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Xor || gmb_p_cur(p)->kind == GMB_TOK_BitXor))) {
            const gmb_token_t* op = gmb_p_cur(p); gmb_p_adv(p);
            e = gmb_expr_binary(op->kind == GMB_TOK_BitXor ? "^^" : "XOR", e, gmb_p_parse_and(p), op->line);
        } else if (gmb_p_match_dot_op(p, "XOR")) {
            e = gmb_expr_binary(p->toks[p->pos - 1].kind == GMB_TOK_BitXor ? "^^" : "XOR", e, gmb_p_parse_and(p), p->pos > 0 ? p->toks[p->pos - 1].line : 0);
        } else break;
    }
    return e;
}
gmb_expr_t* gmb_p_parse_and(gmb_parser_t* p)
{
    gmb_expr_t* e = gmb_p_parse_not(p);
    for (;;) {
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "And") || (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_And || gmb_p_cur(p)->kind == GMB_TOK_BitAnd))) {
            const gmb_token_t* op = gmb_p_cur(p); gmb_p_adv(p);
            e = gmb_expr_binary(op->kind == GMB_TOK_BitAnd ? "&&" : "AND", e, gmb_p_parse_not(p), op->line);
        } else if (gmb_p_match_dot_op(p, "AND")) {
            e = gmb_expr_binary(p->toks[p->pos - 1].kind == GMB_TOK_BitAnd ? "&&" : "AND", e, gmb_p_parse_not(p), p->pos > 0 ? p->toks[p->pos - 1].line : 0);
        } else break;
    }
    return e;
}
gmb_expr_t* gmb_p_parse_not(gmb_parser_t* p)
{
    if (gmb_p_match_dot_op(p, "NOT")) {
        return gmb_expr_unary("NOT", gmb_p_parse_not(p), p->pos > 0 ? p->toks[p->pos - 1].line : 0);
    }
    if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Not") || (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Not || gmb_p_cur(p)->kind == GMB_TOK_BitNot))) {
        const gmb_token_t* op = gmb_p_cur(p); gmb_p_adv(p);
        return gmb_expr_unary(op->kind == GMB_TOK_BitNot ? "!" : "NOT", gmb_p_parse_not(p), op->line);
    }
    return gmb_p_parse_cmp(p);
}
gmb_expr_t* gmb_p_parse_cmp(gmb_parser_t* p)
{
    gmb_expr_t* e = gmb_p_parse_add(p);
    for (;;) {
        const gmb_token_t* t = gmb_p_cur(p);
        gmb_tokenkind_t k = t ? t->kind : GMB_TOK_Unknown;
        const char* op = NULL;
        if (k == GMB_TOK_Equal && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_LessThan) { gmb_p_adv(p); op = "<="; }
        else if (k == GMB_TOK_FatArrow && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind != GMB_TOK_LeftBrace) { op = ">="; }
        else if (k == GMB_TOK_Equal) op = "=";
        else if (k == GMB_TOK_NotEqual) op = "<>";
        else if (k == GMB_TOK_GreaterThan && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_LessThan) { gmb_p_adv(p); op = "<>"; }
        else if (k == GMB_TOK_LessThan) op = "<";
        else if (k == GMB_TOK_LessEqual) op = "<=";
        else if (k == GMB_TOK_GreaterThan) op = ">";
        else if (k == GMB_TOK_GreaterEqual) op = ">=";
        if (!op) break;
        gmb_p_adv(p);
        e = gmb_expr_binary(op, e, gmb_p_parse_add(p), t->line);
    }
    /* x IS <tipo> — type-check (es. valoreNIL IS NIL, nome$ IS STRING) */
    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Is) {
        const gmb_token_t* ist = gmb_p_cur(p); gmb_p_adv(p);
        const gmb_token_t* ty = gmb_p_cur(p);
        gmb_p_adv(p);
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Of && gmb_p_peek(p, 1)) { gmb_p_adv(p); gmb_p_adv(p); } /* ARRAY OF DOUBLE */
        /* il nome del tipo diventa il var-name dell'operando destro, così l'emitter
           distingue `IS NOTHING` dagli altri type-check */
        char* _isn = ty ? gmb_p_tok_strdup(ty) : strdup("_type_");
        gmb_expr_t* _rv = gmb_expr_var(_isn, ty ? ty->line : 0);
        if (ty && ty->kind != GMB_TOK_Identifier && ty->kind != GMB_TOK_Unknown) _rv->tk = (int)ty->kind;
        e = gmb_expr_binary("IS", e, _rv, ist->line);
    }
    /* x HAS VALUE / x HAS NIL — presenza valore */
    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Has) {
        const gmb_token_t* op = gmb_p_cur(p); gmb_p_adv(p);
        const gmb_token_t* hv = gmb_p_cur(p);
        gmb_p_adv(p);
        e = gmb_expr_binary("HAS", e, gmb_expr_var(hv ? gmb_p_tok_strdup(hv) : "_", hv ? hv->line : 0), op->line);
    }
    /* x IN coll — membership */
    if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_In) {
        const gmb_token_t* op = gmb_p_cur(p); gmb_p_adv(p);
        gmb_expr_t* coll = gmb_p_parse_add(p);
        e = gmb_expr_binary("IN", e, coll ? coll : gmb_expr_var("_", op->line), op->line);
    }
    return e;
}
gmb_expr_t* gmb_p_parse_add(gmb_parser_t* p)
{
    gmb_expr_t* e = gmb_p_parse_mul(p);
    for (;;) {
        const gmb_token_t* t = gmb_p_cur(p);
        if (t && t->kind == GMB_TOK_Plus) { gmb_p_adv(p); e = gmb_expr_binary("+", e, gmb_p_parse_mul(p), t->line); }
        else if (t && t->kind == GMB_TOK_Minus) { gmb_p_adv(p); e = gmb_expr_binary("-", e, gmb_p_parse_mul(p), t->line); }
        /* '&' binario = concatenazione stringhe (idioma BASIC), come in C# (ParseTerm):
         * rimappato su '+' per riusare la stessa semantica su tutti i backend. Prima era
         * IGNORATO in silenzio (restava il solo operando sinistro). Un '&' in testa (macro)
         * resta gestito da gmb_p_parse_unary. */
        else if (t && t->kind == GMB_TOK_Ampersand) { gmb_p_adv(p); e = gmb_expr_binary("+", e, gmb_p_parse_mul(p), t->line); }
        else break;
    }
    return e;
}
/* x[e1, ..., en] (indice/i multipli, ridotto a una CALL x(e1,...)) OPPURE x[start:end]
 * (ArraySliceExpr, bug "C99Native-ArraySlice-Parse"): la '[' e' gia' stata
 * verificata presente dal chiamante, `e` e' la VAR di base (consumata: il chiamante deve
 * scartare il vecchio `e` e usare quello ritornato). Fattorizzato fuori dai DUE loop
 * subscript quasi-duplicati (parse_mul/parse_power) per non doverli tenere sincronizzati
 * a mano: prima dello slice il bug esisteva in entrambi allo stesso modo.
 *
 * Lo slice e' rappresentato come CALL sentinella `__slice__` (stesso schema di
 * `_array_`/`_true_`/`_macro_` gia' in uso nell'emitter): e->str = nome della variabile
 * base (LIST), e->args = {start[, end]} — end assente (nargs==1) significa "fino alla
 * fine", che l'emitter traduce in gmb_list_count(base) invece di un -1 sentinella (stesso
 * canone del C99CodeEmitter.Expressions.Core.cs: `ase.End is null ?
 * gmb_list_count(...) : ...` — mai passare -1 a gmb_list_slice, che non lo gestisce). */
static gmb_expr_t* gmb_p_parse_bracket_subscript(gmb_parser_t* p, gmb_expr_t* e)
{
    int bline = e->line;
    gmb_p_adv(p);   /* '[' */
    char* bname = e->var; e->var = NULL;
    gmb_expr_free(e);
    /* a[:end] — start omesso: lo slice è riconosciuto SOLO se ':' precede qualunque
     * espressione (altrimenti '[' vuoto '[]' non e' uno slice valido comunque). */
    if (gmb_p_check(p, GMB_TOK_Colon)) {
        gmb_p_adv(p);
        gmb_expr_t** sa = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
        sa[0] = gmb_expr_num(0.0, bline);
        int sn = 1;
        if (!gmb_p_check(p, GMB_TOK_RightBracket)) { sa[1] = gmb_p_parse_expr(p); sn = 2; }
        gmb_p_match(p, GMB_TOK_RightBracket);
        gmb_expr_t* call = gmb_expr_call("__slice__", sa, sn, bline);
        call->str = bname;
        return call;
    }
    gmb_expr_t* first = gmb_p_check(p, GMB_TOK_RightBracket) ? NULL : gmb_p_parse_expr(p);
    if (first && gmb_p_check(p, GMB_TOK_Colon)) {
        /* a[start:end] o a[start:] */
        gmb_p_adv(p);
        gmb_expr_t** sa = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
        sa[0] = first;
        int sn = 1;
        if (!gmb_p_check(p, GMB_TOK_RightBracket)) { sa[1] = gmb_p_parse_expr(p); sn = 2; }
        gmb_p_match(p, GMB_TOK_RightBracket);
        gmb_expr_t* call = gmb_expr_call("__slice__", sa, sn, bline);
        call->str = bname;
        return call;
    }
    /* indice/i normali (eventualmente multipli a virgola): x[e1, ..., en] -> CALL x(e1,...) */
    gmb_expr_t** bx = NULL; int bn = 0, bc = 0;
    if (first) { bc = 4; bx = (gmb_expr_t**)malloc((size_t)bc * sizeof(gmb_expr_t*)); bx[bn++] = first; }
    if (first && gmb_p_match(p, GMB_TOK_Comma)) {
        for (;;) {
            gmb_expr_t* ie = gmb_p_parse_expr(p);
            if (ie) {
                if (bn >= bc) { bc = bc ? bc * 2 : 4; bx = (gmb_expr_t**)realloc(bx, (size_t)bc * sizeof(gmb_expr_t*)); }
                bx[bn++] = ie;
            }
            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
        }
    }
    gmb_p_match(p, GMB_TOK_RightBracket);
    return gmb_expr_call(bname, bx, bn, bline);
}

/* Membri dopo un indice a quadre: `g["p"].name` / `arr[0].campo.sotto` -> _prop_(g["p"], "name") ... Prima il `.membro` dopo `]` non veniva
 * consumato e l'istruzione risultava "non riconosciuta". Un `.metodo(` resta alla gestione esistente. */
static gmb_expr_t* gmb_p_subscript_members(gmb_parser_t* p, gmb_expr_t* e)
{
    while (e && e->kind == GMB_EXPR_CALL && gmb_p_check(p, GMB_TOK_Dot)) {
        const gmb_token_t* m = gmb_p_peek(p, 1);
        const gmb_token_t* m2 = gmb_p_peek(p, 2);
        if (!m || m->length <= 0) break;
        if (m2 && m2->kind == GMB_TOK_LeftParen) break;
        gmb_p_adv(p);   /* '.' */
        int plen = m->length < 127 ? m->length : 127;
        char pn[128]; memcpy(pn, m->lexeme, (size_t)plen); pn[plen] = 0;
        gmb_p_adv(p);   /* membro */
        gmb_expr_t** pargs = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
        pargs[0] = e;
        pargs[1] = gmb_expr_str(strdup(pn), m->line);
        e = gmb_expr_call("_prop_", pargs, 2, m->line);
    }
    return e;
}
gmb_expr_t* gmb_p_parse_mul(gmb_parser_t* p)
{
    gmb_expr_t* e = gmb_p_parse_unary(p);
    /* x[e1, ..., en] (postfix anche qui: parse_mul puo' precedere parse_power per
     * taluni percorsi primari) */
    while (e && e->kind == GMB_EXPR_VAR && gmb_p_check(p, GMB_TOK_LeftBracket)) {
        e = gmb_p_parse_bracket_subscript(p, e);
    }
    for (;;) {
        const gmb_token_t* t = gmb_p_cur(p);
        if (t && t->kind == GMB_TOK_Multiply) { gmb_p_adv(p); e = gmb_expr_binary("*", e, gmb_p_parse_unary(p), t->line); }
        else if (t && t->kind == GMB_TOK_Divide) { gmb_p_adv(p); e = gmb_expr_binary("/", e, gmb_p_parse_unary(p), t->line); }
        else if (t && t->kind == GMB_TOK_IntegerDivide) { gmb_p_adv(p); e = gmb_expr_binary("\\", e, gmb_p_parse_unary(p), t->line); }
        else if (gmb_p_tok_is_keyword(t, "Mod")) { gmb_p_adv(p); e = gmb_expr_binary("MOD", e, gmb_p_parse_unary(p), t->line); }
        else break;
    }
    return e;
}
gmb_expr_t* gmb_p_parse_unary(gmb_parser_t* p)
{
    const gmb_token_t* t = gmb_p_cur(p);
    /* ++x / --x (prefix): NUOVO valore → `_assign_(x, x ± 1)` (che restituisce il nuovo). */
    if (t && (t->kind == GMB_TOK_PlusPlus || t->kind == GMB_TOK_MinusMinus)) {
        int plus = t->kind == GMB_TOK_PlusPlus; int ln = t->line;
        gmb_p_adv(p);
        gmb_expr_t* op = gmb_p_parse_unary(p);
        if (op && (op->kind == GMB_EXPR_VAR || op->kind == GMB_EXPR_CALL)) {
            int nidx = op->kind == GMB_EXPR_CALL ? op->nargs : 0;
            gmb_expr_t** a = (gmb_expr_t**)malloc((size_t)(3 + nidx) * sizeof(gmb_expr_t*));
            a[0] = gmb_expr_num(1.0, ln);                 /* prefix */
            a[1] = gmb_expr_num(plus ? 1.0 : 0.0, ln);
            a[2] = gmb_expr_var(strdup(op->var ? op->var : "?"), ln);
            for (int k = 0; k < nidx; k++) a[3 + k] = gmb_expr_clone(op->args[k]);
            gmb_expr_t* call = gmb_expr_call("_incdec_", a, 3 + nidx, ln);
            gmb_expr_free(op);
            return call;
        }
        gmb_expr_free(op);
        return gmb_expr_num(0.0, ln);
    }
    if (t && t->kind == GMB_TOK_Minus) { gmb_p_adv(p); return gmb_expr_unary("-", gmb_p_parse_unary(p), t->line); }
    if (t && t->kind == GMB_TOK_Plus) { gmb_p_adv(p); return gmb_p_parse_unary(p); }
    /* AWAIT prefisso: parità C# (AwaitExpr compila come il target) — trasparente */
    if (t && t->kind == GMB_TOK_Await) { gmb_p_adv(p); return gmb_p_parse_unary(p); }
    return gmb_p_parse_power(p);
}
/* valore di un dict literal dopo ':': ricombina un decimale che in questo contesto viene
 * talvolta tokenizzato come Number '1' + '.75' (o '1' + '.' + '75') invece che '1.75'
 * (bug: `{"a": 1.75}` nel dict literal archiviava solo la parte intera). */
static gmb_expr_t* gmb_p_dict_value(gmb_parser_t* p, int line)
{
    const gmb_token_t* a = gmb_p_cur(p);
    /* Segno davanti a un valore numerico (`-1000.00`, `+5`): l'espressione generica restituiva
     * il token '-' come variabile (misurato: coppia con value VAR) → il ledger/DBF non poteva
     * formare il record a compile time. */
    if (a && (a->kind == GMB_TOK_Minus || a->kind == GMB_TOK_Plus)) {
        const gmb_token_t* nx = gmb_p_peek(p, 1);
        if (nx && nx->kind == GMB_TOK_Number) {
            int sgn = (a->kind == GMB_TOK_Minus) ? -1 : 1;
            gmb_p_adv(p);
            gmb_expr_t* r = gmb_p_dict_value(p, line);
            if (r && r->kind == GMB_EXPR_NUM) r->num = sgn * r->num;
            return r;
        }
    }
    if (a && a->kind == GMB_TOK_Number && a->lexeme) {
        char av[64]; int al = a->length < 63 ? a->length : 63;
        memcpy(av, a->lexeme, (size_t)al); av[al] = 0;
        if (!strchr(av, '.')) {
            const gmb_token_t* b = gmb_p_peek(p, 1);
            if (b && b->lexeme && b->length >= 2 && b->lexeme[0] == '.') {
                char bv[64]; int bl = b->length < 63 ? b->length : 63;
                memcpy(bv, b->lexeme, (size_t)bl); bv[bl] = 0;
                char full[160]; snprintf(full, sizeof(full), "%s%s", av, bv);
                double d = gmb_p_num_literal(full);
                gmb_p_adv(p); gmb_p_adv(p);
                return gmb_expr_num(d, line);
            }
            if (b && b->kind == GMB_TOK_Dot) {
                const gmb_token_t* c = gmb_p_peek(p, 2);
                if (c && c->kind == GMB_TOK_Number && c->lexeme && c->length > 0 && c->lexeme[0] != '.') {
                    char cv[64]; int cl = c->length < 63 ? c->length : 63;
                    memcpy(cv, c->lexeme, (size_t)cl); cv[cl] = 0;
                    char full2[160]; snprintf(full2, sizeof(full2), "%s.%s", av, cv);
                    double d2 = gmb_p_num_literal(full2);
                    gmb_p_adv(p); gmb_p_adv(p); gmb_p_adv(p);
                    return gmb_expr_num(d2, line);
                }
            }
        }
    }
    return gmb_p_parse_expr(p);
}

gmb_expr_t* gmb_p_parse_power(gmb_parser_t* p)
{
    gmb_expr_t* e = gmb_p_parse_primary(p);
    /* x[e1, ..., en] — indicizzazione "moderna" con quadre (LIST/DICT/array): produce lo
     * stesso AST della CALL x(e1,...) così l'emitter array/list/dict la tratta come indice. */
    while (e && e->kind == GMB_EXPR_VAR && gmb_p_check(p, GMB_TOK_LeftBracket)) {
        e = gmb_p_parse_bracket_subscript(p, e);
    }
    /* `{ |x| … }(41)` — invocazione diretta di un literal lambda (valore callable). */
    if (e && e->kind == GMB_EXPR_LAMBDA && gmb_p_check(p, GMB_TOK_LeftParen)) {
        int ln = gmb_p_cur(p)->line;
        gmb_p_adv(p);   /* '(' */
        gmb_expr_t** args = NULL; int na = 0, cap = 0;
        if (!gmb_p_check(p, GMB_TOK_RightParen)) {
            for (;;) {
                gmb_expr_t* a = gmb_p_parse_expr(p);
                if (!a) break;
                if (na >= cap) { cap = cap ? cap * 2 : 4; args = (gmb_expr_t**)realloc(args, (size_t)cap * sizeof(gmb_expr_t*)); }
                args[na++] = a;
                if (gmb_p_match(p, GMB_TOK_Comma)) continue;
                break;
            }
        }
        if (!gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_set_err(p, "lambda: attesa ')' dopo gli argomenti", ln);
        gmb_expr_t* call = gmb_expr_call(NULL, args, na, ln);
        call->callee = e;
        e = call;
    }
    /* x++ / x-- (postfix): VECCHIO valore. Vale per VARIABILE (`x`) e ACCESSO INDICIZZATO
     * (`a(i)`, `m(i,j)`): nodo `_incdec_` (prefix=0, plus, target, indici...). */
    {
        const gmb_token_t* tp = gmb_p_cur(p);
        if (tp && (tp->kind == GMB_TOK_PlusPlus || tp->kind == GMB_TOK_MinusMinus)
            && e && (e->kind == GMB_EXPR_VAR || e->kind == GMB_EXPR_CALL)) {
            int plus = tp->kind == GMB_TOK_PlusPlus; int ln = tp->line;
            int nidx = e->kind == GMB_EXPR_CALL ? e->nargs : 0;
            gmb_p_adv(p);
            gmb_expr_t** a = (gmb_expr_t**)malloc((size_t)(3 + nidx) * sizeof(gmb_expr_t*));
            a[0] = gmb_expr_num(0.0, ln);                 /* postfix */
            a[1] = gmb_expr_num(plus ? 1.0 : 0.0, ln);
            a[2] = gmb_expr_var(strdup(e->var ? e->var : "?"), ln);
            for (int k = 0; k < nidx; k++) a[3 + k] = gmb_expr_clone(e->args[k]);
            gmb_expr_t* call = gmb_expr_call("_incdec_", a, 3 + nidx, ln);
            gmb_expr_free(e);
            e = call;
        }
    }
    const gmb_token_t* t = gmb_p_cur(p);
    if (t && t->kind == GMB_TOK_Multiply && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Multiply) {
        gmb_p_adv(p); gmb_p_adv(p);
        e = gmb_expr_binary("^", e, gmb_p_parse_power(p), t->line);
    } else if (t && t->kind == GMB_TOK_Power) {
        gmb_p_adv(p);
        e = gmb_expr_binary("^", e, gmb_p_parse_unary(p), t->line);
    }
    return e;
}
/* Nomi dei COSTRUTTORI di collezione (NEW DICT/SORTED/…/TIMELINE) — UNICA sorgente (§5, regola
 * d'oro): la maggior parte sono keyword del lexer (TokenKind), alcune sono nomi soft. Centralizza
 * il riconoscimento così i call-site non dispatchano sul nome/lexeme. */
static bool gmb_p_is_collection_ctor(const char* nm)
{
    switch (gmb_keyword_tok(nm)) {
        case GMB_TOK_Dict: case GMB_TOK_Sorted: case GMB_TOK_Queue: case GMB_TOK_Stack:
        case GMB_TOK_HashSet: case GMB_TOK_Tree: case GMB_TOK_Heap: case GMB_TOK_Bag:
        case GMB_TOK_Cache: case GMB_TOK_Graph: case GMB_TOK_Timeline:
            return true;
        default: break;
    }
    static const char* const soft[] = { "AVLTREE", "REDBLACK", "BPLUSTREE", "TRIE", "MAXHEAP",
        "BITREE", "HASHRING", "RINGBUFFER", "SPARSESET", NULL };
    for (int i = 0; soft[i]; i++) if (strcasecmp(nm, soft[i]) == 0) return true;
    return false;
}

gmb_expr_t* gmb_p_parse_primary(gmb_parser_t* p)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t) return NULL;
    int line = t->line;
    /* Bug corretto (2026-08-31, audit macro/EVAL): "&espr" (macro Clipper, valuta a runtime
     * il testo prodotto da espr come codice) non era MAI riconosciuto qui — '&' non aveva
     * NESSUN caso dedicato in questo parser di espressioni, quindi ogni "y = &(...)" falliva
     * silenziosamente o produceva un valore inerte. La VM (gmb_vm_core.c, vm_eval_macro,
     * OP_EVAL_MACRO_EXPR/STMT=105/106) ha già un motore di valutazione funzionante — mancava
     * SOLO il collegamento dal compilatore, stesso schema del bug AFILTER/AMAP (mai
     * registrati nel compilatore pur avendo un handler VM pronto). Fix: '&' come PREFISSO
     * di espressione produce un nodo sentinella "_macro_" (stesso pattern già in uso per
     * "_array_"/"_switch_"/"_dict_" — un GMB_EXPR_CALL con nome speciale invece di un nuovo
     * kind AST), con l'espressione sorgente (tipicamente una stringa) come unico argomento —
     * dispatchato in cc_expr a "valuta la stringa, poi OP_EVAL_MACRO_EXPR" (vedi
     * gmb_compiler.c). La forma STATEMENT ("&(cmd)" da sola su una riga) riusa lo stesso
     * meccanismo (gmb_parser_statements.c non consuma più '&' da sola: lo lascia a
     * gmb_p_parse_expr, che arriva qui). EVAL(espr) è zucchero sintattico dichiarato
     * equivalente a &espr (stesso commento nel sample canarino) — stessa costruzione. */
    if (t->kind == GMB_TOK_Ampersand || t->kind == GMB_TOK_Eval) {
        bool is_eval = t->kind == GMB_TOK_Eval;
        gmb_p_adv(p);
        gmb_expr_t* src;
        if (is_eval) {
            /* EVAL(target [, args...]): parentesi obbligatorie, funzione-simile.
             * Clipper 5.3: EVAL(bBlock, args...) invoca il code block; con una stringa
             * resta il macro-evaluator (dispatch per tipo a runtime, vedi gmb_c99_emit.c). */
            if (!gmb_p_match(p, GMB_TOK_LeftParen)) gmb_p_set_err(p, "atteso '(' dopo EVAL", line);
            src = gmb_p_parse_expr(p);
            int na = 1, ca = 4;
            gmb_expr_t** args = (gmb_expr_t**)malloc((size_t)ca * sizeof(gmb_expr_t*));
            args[0] = src;
            while (gmb_p_match(p, GMB_TOK_Comma)) {
                if (na >= ca) { ca *= 2; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                args[na++] = gmb_p_parse_expr(p);
            }
            if (!gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_set_err(p, "attesa ')'", line);
            if (na == 1) {
                gmb_expr_t* expanded = gmb_p_try_expand_const_macro(p, src, line);
                if (!expanded) expanded = gmb_p_try_expand_const_var(p, src, line);
                if (expanded) { free(args); gmb_expr_free(src); return expanded; }
            }
            return gmb_expr_call("_macro_", args, na, line);
        }
        src = gmb_p_parse_unary(p);
        /* Macro COSTANTE &("..."): compilata a compile-time (vedi l'helper). */
        {
            gmb_expr_t* expanded = gmb_p_try_expand_const_macro(p, src, line);
            if (!expanded) expanded = gmb_p_try_expand_const_var(p, src, line);
            if (expanded) { gmb_expr_free(src); return expanded; }
        }
        /* args va allocato sull'heap: gmb_expr_call salva il puntatore SENZA copiare
           (e->args = args) — un array automatico locale diventerebbe un dangling
           pointer non appena questa funzione ritorna (bug reale, causava un crash/UB
           al primo uso di "&espr": trovato SOLO eseguendo, mai a colpo d'occhio). */
        gmb_expr_t** args = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*));
        args[0] = src;
        return gmb_expr_call("_macro_", args, 1, line);
    }
    if (t->kind == GMB_TOK_Number) {
        gmb_p_adv(p);
        char buf[64]; int l = t->length < 63 ? t->length : 63;
        memcpy(buf, t->lexeme, (size_t)l); buf[l] = 0;
        return gmb_expr_num(gmb_p_num_literal(buf), line);
    }
    /* TRUE/FALSE: keyword del Lexer (TokenKind True/False) → costante sentinella
     * `_true_`/`_false_` (dispatch per-token, MAI sul lexeme). Il Clipper .T./.F. è
     * gestito a parte con lo stesso risultato. */
    if (t->kind == GMB_TOK_True || t->kind == GMB_TOK_False) {
        gmb_p_adv(p);
        return gmb_expr_call(strdup(t->kind == GMB_TOK_True ? "_true_" : "_false_"), NULL, 0, line);
    }
    if (t->kind == GMB_TOK_StringLiteral) {
        gmb_p_adv(p);
        char buf[512]; int l = t->length < 511 ? t->length : 511;
        memcpy(buf, t->lexeme, (size_t)l); buf[l] = 0;
        /* rimuovi i delimitatori esterni: "..." oppure `...` (raw string). Senza il
         * secondo caso i backtick restavano nel valore (`{"user":...}`) e JSON.Parse
         * falliva → <nil> su C99Native (CollectionsModern §16). */
        size_t bl = strlen(buf);
        int raw = 0;
        if (bl >= 6 && buf[0] == '"' && buf[1] == '"' && buf[2] == '"'
            && buf[bl - 1] == '"' && buf[bl - 2] == '"' && buf[bl - 3] == '"') {
            /* raw triple-quote """...""" (nessun escape, nessuna interpretazione) */
            memmove(buf, buf + 3, bl - 6); buf[bl - 6] = 0; bl -= 6; raw = 1;
        }
        else if (bl >= 2 && buf[0] == '"' && buf[bl - 1] == '"') { memmove(buf, buf + 1, bl - 2); buf[bl - 2] = 0; bl -= 2; }
        else if (bl >= 2 && buf[0] == '`' && buf[bl - 1] == '`') {
            memmove(buf, buf + 1, bl - 2); buf[bl - 2] = 0; bl -= 2; raw = 1;
        }
        if (raw) return gmb_expr_str(buf, line);
        /* unescape BASIC "" -> " (parità col lexer C#; es. SET LINK outer, "{""hidden"":64}") */
        char out[512]; int o = 0;
        for (size_t i = 0; i < bl && o < 510; i++) {
            if (buf[i] == '"' && i + 1 < bl && buf[i + 1] == '"') { out[o++] = '"'; i++; }
            else out[o++] = buf[i];
        }
        out[o] = 0;
        return gmb_expr_str(out, line);
    }
    if (t->kind == GMB_TOK_LeftParen) {
        gmb_p_adv(p);
        /* campo nominato `nome: valore` (tupla nominata, es. `(x: 10, y: 20)`): SOLO se
         * l'identificatore corrente è seguito da ':' — guardo avanti PRIMA di parsare
         * come espressione (altrimenti gmb_p_parse_expr consuma "x" da solo e si ferma
         * su ':' inatteso). Il nome del PRIMO elemento va controllato qui perché "e"
         * sotto è già parsato prima di sapere se si tratta di una tupla. */
        char* fname0 = NULL;
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Identifier
            && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Colon) {
            fname0 = gmb_p_tok_strdup(gmb_p_cur(p));
            gmb_p_adv(p); gmb_p_adv(p); /* nome e ':' */
        }
        gmb_expr_t* e = gmb_p_parse_expr(p);
        /* tupla: (a, b, c) o (x: a, y: b, ...) — RACCOGLIE gli elementi (CALL
         * "_tuple_lit_", stesso pattern _array_/_dict_; emissione in gmb_c99_emit.c:
         * gmb_val_tuple(...)). I nomi campo (se presenti) sono codificati come stringa
         * CSV in e->ns ("x,y,z" o ",y," per campi misti/anonimi — indice vuoto = nessun
         * nome), consultata per risolvere `t.nome` → `t(indice)` (vedi gmb_c99_emit.c,
         * resolve_tuple_field). Prima gli elementi oltre il primo venivano PARSATI E
         * SCARTATI (gmb_expr_free): il letterale tupla non era affatto rappresentato
         * nell'AST — causa radice del gap tuple/parita transpiler-native lato
         * C99Native (non solo l'interpolazione mai valutata: le tuple non esistevano
         * proprio). Se non c'è virgola E non c'è nome resta un semplice raggruppamento
         * `(expr)` (invariato, incluso il generator-expr sotto, che non ha mai una
         * virgola top-level prima di FOR). */
        if (fname0 || gmb_p_check(p, GMB_TOK_Comma)) {
            gmb_expr_t** tup = NULL; int ntup = 0, captup = 0;
            char** fnames = NULL; int any_name = fname0 != NULL;
            captup = 4; tup = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*) * (size_t)captup);
            fnames = (char**)malloc(sizeof(char*) * (size_t)captup);
            tup[ntup] = e; fnames[ntup] = fname0; ntup++;
            while (gmb_p_match(p, GMB_TOK_Comma)) {
                char* fn = NULL;
                if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Identifier
                    && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Colon) {
                    fn = gmb_p_tok_strdup(gmb_p_cur(p));
                    gmb_p_adv(p); gmb_p_adv(p);
                    any_name = 1;
                }
                gmb_expr_t* el = gmb_p_parse_expr(p);
                if (!el) { free(fn); break; }
                if (ntup >= captup) {
                    captup *= 2;
                    tup = (gmb_expr_t**)realloc(tup, sizeof(gmb_expr_t*) * (size_t)captup);
                    fnames = (char**)realloc(fnames, sizeof(char*) * (size_t)captup);
                }
                tup[ntup] = el; fnames[ntup] = fn; ntup++;
            }
            if (!gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_set_err(p, "attesa ')'", line);
            gmb_expr_t* tl = gmb_expr_call("_tuple_lit_", tup, ntup, line);
            if (any_name) {
                size_t totlen = 1;
                for (int i = 0; i < ntup; i++) totlen += (fnames[i] ? strlen(fnames[i]) : 0) + 1;
                char* csv = (char*)malloc(totlen); csv[0] = 0;
                for (int i = 0; i < ntup; i++) {
                    if (i > 0) strcat(csv, ",");
                    if (fnames[i]) strcat(csv, fnames[i]);
                }
                free(tl->ns); tl->ns = csv;
            }
            for (int i = 0; i < ntup; i++) free(fnames[i]);
            free(fnames);
            return tl;
        }
        /* generator expression: (expr FOR var IN coll [IF cond]) — non esiste una vera
         * generator/lazy machinery nel backend nativo, quindi si desugar a un AMAP (e,
         * se c'è IF, un AFILTER a monte) sulla collezione, riusando builtin già corretti
         * e testati invece di introdurre un nuovo protocollo di iterazione a livello di
         * espressione. Prima di questo fix i token della clausola FOR venivano solo
         * SCARTATI: "e" restava l'espressione nuda valutata una volta sola (mai un
         * array/lista), per questo FOR EACH su un generator dava sempre output vuoto. */
        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_For || gmb_p_tok_is_keyword(gmb_p_cur(p), "For"))) {
            gmb_p_adv(p);
            const gmb_token_t* vt = gmb_p_cur(p);
            char* varname = vt ? gmb_p_tok_strdup(vt) : NULL;
            if (vt) gmb_p_adv(p);
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_In || gmb_p_tok_is_keyword(gmb_p_cur(p), "In")))
                gmb_p_adv(p);
            gmb_expr_t* coll = gmb_p_parse_expr(p);
            gmb_expr_t* cond = NULL;
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_If || gmb_p_tok_is_keyword(gmb_p_cur(p), "If"))) {
                gmb_p_adv(p);
                cond = gmb_p_parse_expr(p);
            }
            if (varname) {
                if (cond) {
                    char** cparams = (char**)malloc(sizeof(char*));
                    cparams[0] = strdup(varname);
                    gmb_expr_t* condfn = gmb_expr_lambda(cparams, 1, cond, NULL, 0, 0, line);
                    gmb_expr_t** fargs = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                    fargs[0] = coll; fargs[1] = condfn;
                    coll = gmb_expr_call("AFILTER", fargs, 2, line);
                }
                char** mparams = (char**)malloc(sizeof(char*));
                mparams[0] = varname;
                gmb_expr_t* mapfn = gmb_expr_lambda(mparams, 1, e, NULL, 0, 0, line);
                gmb_expr_t** margs = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                margs[0] = coll; margs[1] = mapfn;
                e = gmb_expr_call("AMAP", margs, 2, line);
                /* GENERATOR EXPRESSION (desugared): semanticamente è una LISTA del
                 * linguaggio (come il List<object> delle gambe C#), non un array nativo
                 * esplicito: marca il nodo così il DIM che lo inizializza può rispondere
                 * TYPENAME "LIST`1" (guida Arrays §0.3). Un AMAP scritto a mano resta
                 * ARRAY. */
                if (e) e->is_list_literal = 1;
            }
        }
        if (!gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_set_err(p, "attesa ')'", line);
        return e;
    }
    if (t->kind == GMB_TOK_LeftBracket || t->kind == GMB_TOK_LeftBrace) {
        /* comprehension `[ out FOR ... ]` / `{ k:v FOR ... }` / `{ out FOR ... }`
         * (rilevata da un FOR top-level dentro il letterale) */
        if (gmb_p_lit_comp_probe(p)) return gmb_p_parse_lit_comprehension(p, line);
        /* array literal {a, b, c} oppure dict { k: v, ... } */
        bool is_brace = t->kind == GMB_TOK_LeftBrace;
        gmb_p_adv(p);
        if (is_brace && gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Bar
            || (gmb_p_cur(p)->kind == GMB_TOK_Unknown && gmb_p_cur(p)->length == 1 && gmb_p_cur(p)->lexeme[0] == '|')
            /* `{ || body }`: lista parametri VUOTA. Il lessema può arrivare come UN SOLO
             * token "||" (operatore) invece di due barre: senza questo caso il blocco non
             * era riconosciuto e il corpo finiva fuori, con "parse error: attesa ')'"
             * (bug clipper/eval_block.bas, `PRINT EVAL({ || 3.14 })`). */
            || (gmb_p_cur(p)->length == 2 && gmb_p_cur(p)->lexeme[0] == '|' && gmb_p_cur(p)->lexeme[1] == '|'))) {
            int cb_empty_params = (gmb_p_cur(p)->length == 2
                                   && gmb_p_cur(p)->lexeme[0] == '|' && gmb_p_cur(p)->lexeme[1] == '|');
            /* codeblock Clipper: { |params| body } — prima venivano SOLO scartati i token
             * fino a '}' (return di uno stub "_fn_" senza parametri né corpo): qualunque
             * AEVAL/AFILTER/ASCAN/AMAP passato un codeblock Clipper era un no-op silenzioso
             * (nessun crash, nessun errore). Fix: estrae davvero la lista parametri fra le
             * barre e il corpo (lista di statement fino al '}' di chiusura), speculare al
             * ramo "SUB(params) { body }" poco sotto — stesso schema di lambda con corpo a
             * blocco, is_sub=0 perché un codeblock può contenere un RETURN che produce un
             * valore (semantica Clipper, es. AFILTER/ASCAN col predicato). */
            gmb_p_adv(p); /* consuma la barra di apertura (o il token `||`) */
            char** cbparams = NULL; int cbnp = 0, cbcp = 0;
            /* Maschera parametri STRING (`{|s AS STRING, ...|}`): la clausola AS veniva
             * SCARTATA senza registrare il tipo — la λ nativa restava double e
             * `{ |s AS STRING, t AS STRING| RETURN s + t }` dava errore C (parità col
             * parser FN(...) che già registra pstr; C#/IL/PCode/VM-C ok). */
            uint8_t* pstr = NULL;
            while (!cb_empty_params && !gmb_p_at_end(p) && !(gmb_p_check(p, GMB_TOK_Bar)
                   || (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Unknown && gmb_p_cur(p)->length == 1 && gmb_p_cur(p)->lexeme[0] == '|'))) {
                if (gmb_p_match(p, GMB_TOK_Comma)) continue;
                const gmb_token_t* pt = gmb_p_cur(p);
                if (!pt) break;
                if (cbnp >= cbcp) {
                    int old = cbcp;
                    cbcp = cbcp ? cbcp * 2 : 4;
                    cbparams = (char**)realloc(cbparams, (size_t)cbcp * sizeof(char*));
                    pstr = (uint8_t*)realloc(pstr, (size_t)cbcp);
                    memset(pstr + old, 0, (size_t)(cbcp - old));
                }
                cbparams[cbnp] = gmb_p_tok_strdup(pt);
                gmb_p_adv(p);
                /* Bug corretto (2026-09-02): "{|a AS DOUBLE, b AS DOUBLE| ...}" (parametri
                 * tipizzati, come già supportato da FN(...) => expr) veniva letto TOKEN PER
                 * TOKEN come se ogni token fosse un NOME di parametro a sé — "AS" e "DOUBLE"
                 * diventavano essi stessi due parametri fantasma in più, disallineando
                 * completamente il binding posizionale degli argomenti reali alla chiamata
                 * (mai un errore: "a"/"b" semplicemente non ricevevano mai il valore
                 * giusto). Scarta "AS <tipo>" dopo il nome, stesso schema già usato dal
                 * parser di FN(...) poco sopra in questo file. */
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As") || gmb_p_check(p, GMB_TOK_As)) {
                    const gmb_token_t* tt = gmb_p_peek(p, 1);
                    if (gmb_p_tok_is_string_type_name(tt))
                        pstr[cbnp] = 1;
                    gmb_p_adv(p);
                    if (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Comma)
                        && !gmb_p_check(p, GMB_TOK_Bar)
                        && !(gmb_p_cur(p)->kind == GMB_TOK_Unknown && gmb_p_cur(p)->length == 1 && gmb_p_cur(p)->lexeme[0] == '|'))
                        gmb_p_adv(p);
                }
                cbnp++;
            }
            if (!cb_empty_params) gmb_p_adv(p); /* consuma la barra di chiusura */
            /* Corpo: PRIMA gli statement (RETURN/PRINT/assegnazioni: un '=' a livello
             * statement è assegnazione, mai confronto), POI un'ESPRESSIONE nuda = ritorno
             * implicito Clipper 5.3 (`{ |x| x + 1 }` ≡ `{ |x| RETURN x + 1 }`, `{ || 3.14 }`). */
            gmb_stmt_list_t cbbody = gmb_p_stmt_list_new();
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightBrace)) {
                if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_check(p, GMB_TOK_Colon)) { gmb_p_adv(p); continue; }
                int cbbefore = p->pos;
                gmb_tokenkind_t cbstart = gmb_p_cur(p)->kind;
                char* saved_err = p->error;
                p->error = NULL;
                bool cbml = false;
                gmb_stmt_t* cbst = gmb_p_parse_stmt(p, &cbml);
                /* Un REM prodotto da un token che NON è un commento/riga vuota è il
                 * fallback "numero di riga come statement": NON è uno statement del
                 * corpo → si tenta come espressione (ritorno implicito Clipper). */
                int cb_rem_fallback = cbst && cbst->kind == GMB_STMT_REM
                    && cbstart != GMB_TOK_Rem && cbstart != GMB_TOK_Newline
                    && cbstart != GMB_TOK_Colon && cbstart != GMB_TOK_LineNumber;
                int stmt_ok = cbst && !p->error && !cb_rem_fallback
                    && (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_check(p, GMB_TOK_Colon)
                        || gmb_p_check(p, GMB_TOK_RightBrace) || gmb_p_at_end(p));
                if (stmt_ok) {
                    gmb_p_stmt_list_add(&cbbody, cbst);
                    gmb_p_drain_queued(p, &cbbody);
                    free(p->error);
                    p->error = saved_err;
                    continue;
                }
                if (cbst) gmb_stmt_free(cbst);
                p->pos = cbbefore;
                free(p->error);
                p->error = saved_err;
                /* Espressione nuda → RETURN implicito (valore dell'ultima espressione). */
                gmb_expr_t* cbe = gmb_p_parse_expr(p);
                if (!cbe || !gmb_p_check(p, GMB_TOK_RightBrace)) {
                    gmb_p_set_err(p, "codeblock: atteso '}' dopo l'espressione", line);
                    break;
                }
                gmb_stmt_t* cbret = gmb_stmt_new(GMB_STMT_RETURN, line);
                cbret->ret_value = cbe;
                gmb_p_stmt_list_add(&cbbody, cbret);
                break;
            }
            if (!gmb_p_match(p, GMB_TOK_RightBrace)) gmb_p_set_err(p, "codeblock: attesa '}'", line);
            {
                gmb_expr_t* clam = gmb_expr_lambda(cbparams, cbnp, NULL, cbbody.items, cbbody.n, 0, line);
                clam->lambda_params_str = pstr;
                return clam;
            }
        }
        if (is_brace && gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_Function || gmb_p_tok_is_keyword(gmb_p_cur(p), "Sub"))) {
            /* SUB(x) { body } — lambda SUB con corpo a blocco */
            int depth = 1;
            while (!gmb_p_at_end(p) && depth > 0) {
                if (gmb_p_check(p, GMB_TOK_LeftBrace)) depth++;
                else if (gmb_p_check(p, GMB_TOK_RightBrace)) depth--;
                if (depth > 0) gmb_p_adv(p);
            }
            gmb_p_match(p, GMB_TOK_RightBrace);
            return gmb_expr_call("_fn_", NULL, 0, line);
        }
        gmb_expr_t** args = NULL; int na = 0, ca = 0;
        bool is_dict = false;
        if (!gmb_p_check(p, GMB_TOK_RightBrace)) {
            for (;;) {
                gmb_p_skip_eol(p);
                if (gmb_p_check(p, GMB_TOK_RightBrace) || gmb_p_check(p, GMB_TOK_RightBracket)) break;
                int kpos = p->pos;
                gmb_expr_t* first = gmb_p_parse_expr(p);
                /* Se il parse della chiave ha consumato oltre la coppia (né ':' né ',' né '}'),
                 * si riavvolge e si legge un PRIMARY: nei record letterali su più righe
                 * (LEDGER ADD … { \n "conto": importo, … }) la chiave successiva veniva
                 * mangiata e le coppie risultavano corrotte (misurato: chiave NUM 0 e valore
                 * VAR) → DBF/LEDGER si fermavano. Additivo: si attiva solo nel caso rotto. */
                if (first && !gmb_p_check(p, GMB_TOK_Colon) && !gmb_p_check(p, GMB_TOK_Comma)
                    && !gmb_p_check(p, GMB_TOK_RightBrace) && !gmb_p_at_end(p))
                {
                    gmb_expr_free(first);
                    p->pos = kpos;
                    /* Un valore NEGATIVO (`-1000.00`) va letto come numero con segno: il primary
                     * da solo restituirebbe il token '-' come variabile (misurato: value VAR). */
                    {
                        const gmb_token_t* sv = gmb_p_cur(p);
                        int sgn = 1;
                        if (sv && (sv->kind == GMB_TOK_Minus || sv->kind == GMB_TOK_Plus))
                        {
                            sgn = (sv->kind == GMB_TOK_Minus) ? -1 : 1;
                            gmb_p_adv(p);
                            sv = gmb_p_cur(p);
                        }
                        if (sv && sv->kind == GMB_TOK_Number)
                        {
                            char nb[64]; int l = sv->length < 63 ? sv->length : 63;
                            memcpy(nb, sv->lexeme, (size_t)l); nb[l] = 0;
                            first = gmb_expr_num(sgn * gmb_p_num_literal(nb), line);
                            gmb_p_adv(p);
                        }
                        else first = gmb_p_parse_primary(p);
                    }
                }
                if (is_brace && first && first->kind == GMB_EXPR_BINARY && first->op
                    && strcmp(first->op, "=") == 0) {
                    /* Dict-literal Clipper `{ chiave = valore, ... }` (accanto a `{ chiave : valore }`):
                     * il parser aveva già letto `chiave = valore` come CONFRONTO → unwrap in key/value. */
                    is_dict = true;
                    if (na + 2 >= ca) { ca = ca ? ca * 2 : 8; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                    args[na++] = first->left; args[na++] = first->right;
                    first->left = NULL; first->right = NULL; gmb_expr_free(first);   /* guscio */
                } else if (gmb_p_match(p, GMB_TOK_Colon)) {
                    is_dict = true;
                    gmb_expr_t* v = gmb_p_dict_value(p, line);
                    if (na + 2 >= ca) { ca = ca ? ca * 2 : 8; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                    args[na++] = first; args[na++] = v;
                } else {
                    if (na >= ca) { ca = ca ? ca * 2 : 4; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                    args[na++] = first;
                }
                if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                gmb_p_skip_eol(p);
                if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_LineNumber
                    || (gmb_p_cur(p)->kind == GMB_TOK_Number && gmb_p_peek(p,1)
                        && gmb_p_peek(p,1)->line == gmb_p_cur(p)->line
                        && gmb_p_peek(p,1)->kind != GMB_TOK_Colon   /* `2: "due"`: chiave numerica di un dict, NON un numero di riga */
                        && !gmb_p_is_num_op_follower(p)))) gmb_p_adv(p);
            }
        }
        gmb_p_skip_eol(p);
        gmb_p_match(p, is_brace ? GMB_TOK_RightBrace : GMB_TOK_RightBracket);
        {
            /* QUADRE vs GRAFFE: fuori da un DIM di array, `[ ]` è un letterale di LISTA
             * (guide/utente/Arrays §0.2-0.3). Il flag sopravvive fino all'emitter, che
             * sceglie gmb_list_t quando il target non dichiara un array. */
            gmb_expr_t* lit = gmb_expr_call(is_dict ? "_dict_" : "_array_", args, na, line);
            if (!is_dict && !is_brace) lit->is_list_literal = 1;
            return lit;
        }
    }
    if (t->kind == GMB_TOK_InterpolatedStringLiteral) {
        /* stringa interpolata $"...{expr}...": spezza in segmenti letterale/espressione
         * (CALL "_interp_", stesso pattern di "_array_"/"_dict_" sopra — la concatenazione
         * type-aware è nell'emitter). Ogni {expr} è ri-lessato e ri-parsato come sorgente
         * indipendente (gmb_lexer_scan + gmb_p_parse_expr su un gmb_parser_t locale): il
         * lexer emette l'intera $"..." come UN token, quindi non c'è altro modo di ottenere
         * i sotto-token. `{{`/`}}` sono ESCAPE per una graffa letterale singola (convenzione
         * C#/mlx_lm, testata dalla guida di StringsInterpolation.bas): vanno risolti PRIMA
         * di cercare la graffa di apertura, perché `{`/`}` sono ANCHE la sintassi dei
         * letterali array/dict `{ e1, e2 }` in questo linguaggio — senza l'escape, `{{x}}`
         * viene letto come UN segmento che contiene il letterale array `{x}`, invece di
         * "{x}" letterale (bug trovato durante il fix, coperto qui). Bracket counting per
         * {..{..}..} annidati (dict/array literal come sotto-espressione, es. `{MyDict}`
         * dentro l'interpolazione); NON gestisce parentesi graffe dentro stringhe annidate
         * al suo interno (edge case raro, fuori scope). Il "format specifier" opzionale
         * dopo `:` (`{value:##0.00}`, sintassi documentata ma non implementata nemmeno da
         * IL per tutti i formati, es. `:X`/hex) viene ignorato: si valuta solo l'espressione
         * prima del PRIMO `:` a profondità 0 (nessuna formattazione applicata — fuori scope,
         * stesso gap degli altri backend sui formati non banali). Prima l'intero contenuto
         * restava letterale (Gap 2, Bugs/ (nota "tuple/parita transpiler/native")). */
        gmb_p_adv(p);
        int l = t->length;
        char* buf = (char*)malloc((size_t)l + 1);
        memcpy(buf, t->lexeme, (size_t)l); buf[l] = 0;
        char* p1 = buf;
        if (*p1 == '$') p1++;
        size_t bl = strlen(p1);
        if (bl >= 2 && p1[0] == '"' && p1[bl - 1] == '"') { memmove(p1, p1 + 1, bl - 2); p1[bl - 2] = 0; bl -= 2; }
        gmb_expr_t** segs = NULL; int nseg = 0, capseg = 0;
        char* lit = (char*)malloc(bl + 1); size_t litlen = 0;
        size_t i = 0;
        while (i < bl) {
            if (p1[i] == '{' && i + 1 < bl && p1[i + 1] == '{') { lit[litlen++] = '{'; i += 2; continue; }
            if (p1[i] == '}' && i + 1 < bl && p1[i + 1] == '}') { lit[litlen++] = '}'; i += 2; continue; }
            if (p1[i] != '{') { lit[litlen++] = p1[i++]; continue; }
            if (litlen > 0) {
                lit[litlen] = 0;
                if (capseg <= nseg) { capseg = capseg ? capseg * 2 : 4; segs = (gmb_expr_t**)realloc(segs, sizeof(gmb_expr_t*) * (size_t)capseg); }
                segs[nseg++] = gmb_expr_str(lit, line);
                litlen = 0;
            }
            size_t start = i + 1, j = start; int depth = 1;
            while (j < bl && depth > 0) {
                if (p1[j] == '{') depth++;
                else if (p1[j] == '}') { depth--; if (depth == 0) break; }
                j++;
            }
            /* Format specifier opzionale `:fmt` a profondità 0 (fuori da eventuali
             * sotto-espressioni annidate): tronca alla sola espressione, il resto si
             * ignora (vedi nota sopra). */
            size_t exprend = j;
            { int fdepth = 0; for (size_t k = start; k < j; k++) {
                if (p1[k] == '{' || p1[k] == '(' || p1[k] == '[') fdepth++;
                else if (p1[k] == '}' || p1[k] == ')' || p1[k] == ']') fdepth--;
                else if (p1[k] == ':' && fdepth == 0) { exprend = k; break; }
              } }
            size_t exprlen = (exprend > start) ? exprend - start : 0;
            if (exprlen > 0) {
                char* exprsrc = (char*)malloc(exprlen + 1);
                memcpy(exprsrc, p1 + start, exprlen); exprsrc[exprlen] = 0;
                gmb_lex_result_t lx = gmb_lexer_scan(exprsrc, (long)exprlen);
                if (!lx.error && lx.count > 0) {
                    gmb_parser_t sub; memset(&sub, 0, sizeof(sub));
                    sub.toks = lx.tokens; sub.n = lx.count; sub.pos = 0;
                    gmb_expr_t* se = gmb_p_parse_expr(&sub);
                    if (se) {
                        if (capseg <= nseg) { capseg = capseg ? capseg * 2 : 4; segs = (gmb_expr_t**)realloc(segs, sizeof(gmb_expr_t*) * (size_t)capseg); }
                        segs[nseg++] = se;
                    }
                }
                gmb_lex_result_free(&lx);
                free(exprsrc);
            }
            i = (j < bl) ? j + 1 : bl;
        }
        if (litlen > 0) {
            lit[litlen] = 0;
            if (capseg <= nseg) { capseg = capseg ? capseg * 2 : 4; segs = (gmb_expr_t**)realloc(segs, sizeof(gmb_expr_t*) * (size_t)capseg); }
            segs[nseg++] = gmb_expr_str(lit, line);
        }
        free(lit);
        free(buf);
        if (nseg == 0) { free(segs); return gmb_expr_str("", line); }
        return gmb_expr_call("_interp_", segs, nseg, line);
    }
    if (t->kind == GMB_TOK_Switch) {
        /* SWITCH(expr) { CASE v: ris, ... ELSE: ris } — espressione (parità Clipper/C#).
         * Prima i bracci venivano SCARTATI (return "_switch_" senza argomenti) → "funzione
         * '_switch_' non definita" sul C99Native (minitests/constructs/expr_switchexpr.bas).
         * Ora: args = [sel, k1, r1, k2, r2, ...]; ramo ELSE/DEFAULT = chiave sentinella
         * "#DEFAULT#" (GMB_EXPR_STR), interpretata dall'emitter come arm sempre preso. */
        gmb_p_adv(p);
        gmb_expr_t* se = NULL;
        if (gmb_p_match(p, GMB_TOK_LeftParen)) {
            se = gmb_p_parse_expr(p);
            gmb_p_match(p, GMB_TOK_RightParen);
        }
        int scap = 8;
        gmb_expr_t** sargs = (gmb_expr_t**)malloc((size_t)scap * sizeof(gmb_expr_t*));
        int na = 0;
        sargs[na++] = se ? se : gmb_expr_num(0.0, line);
        if (gmb_p_match(p, GMB_TOK_LeftBrace)) {
            for (;;) {
                gmb_p_skip_eol(p);
                if (gmb_p_at_end(p)) break;
                if (gmb_p_match(p, GMB_TOK_RightBrace)) break;
                if (gmb_p_check(p, GMB_TOK_Case) || gmb_p_tok_is_keyword(gmb_p_cur(p), "Case")) gmb_p_adv(p);
                gmb_expr_t* key = NULL;
                if (gmb_p_tok_is_keyword(gmb_p_cur(p), "Else") || gmb_p_tok_is_keyword(gmb_p_cur(p), "Default")
                    || gmb_p_check(p, GMB_TOK_Else)) {
                    gmb_p_adv(p);
                    key = gmb_expr_str("#DEFAULT#", gmb_p_cur(p) ? gmb_p_cur(p)->line : line);
                } else key = gmb_p_parse_expr(p);
                gmb_p_match(p, GMB_TOK_Colon);
                gmb_expr_t* res = gmb_p_parse_expr(p);
                if (na + 2 > scap) { scap = scap * 2; sargs = (gmb_expr_t**)realloc(sargs, (size_t)scap * sizeof(gmb_expr_t*)); }
                sargs[na++] = key;
                sargs[na++] = res;
                gmb_p_match(p, GMB_TOK_Comma);
            }
        }
        return gmb_expr_call("_switch_", sargs, na, line);
    }
    if (t->kind == GMB_TOK_New) {
        /* NEW Name(args) — oggetto: parse come var/call */
        gmb_p_adv(p);
        const gmb_token_t* nw = gmb_p_cur(p);
        if (!nw) return gmb_expr_var("_new_", line);
        /* NEW LIST [OF <Tipo>] [()] — lista tipizzata: CALL sintetica "LISTOF:<Elem>" */
        if (nw->kind == GMB_TOK_List || gmb_p_tok_is_keyword(nw, "List")) {
            gmb_p_adv(p);
            char elem[64];
            snprintf(elem, sizeof(elem), "ANY");
            if (gmb_p_cur(p) && gmb_p_tok_is_keyword(gmb_p_cur(p), "Of")) {
                gmb_p_adv(p);
                const gmb_token_t* el = gmb_p_cur(p);
                if (el && el->kind != GMB_TOK_Newline && !gmb_p_at_end(p)) {
                    int l = el->length < 63 ? el->length : 63;
                    memcpy(elem, el->lexeme, (size_t)l); elem[l] = 0;
                    gmb_p_adv(p);
                }
            }
            if (gmb_p_check(p, GMB_TOK_LessThan)) {
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) gmb_p_adv(p);
                gmb_p_match(p, GMB_TOK_GreaterThan);
            }
            /* NEW LIST<...>(sorgente) / NEW LIST<...>(args...) — consuma le parentesi in
             * modo BILANCIATO (una Keys() interna non deve fermare la scansione sulla sua
             * ')' lasciando token orfani: bug che sfasava tutto il body della funzione). */
            if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                int depth = 1;
                while (!gmb_p_at_end(p) && depth > 0) {
                    if (gmb_p_check(p, GMB_TOK_LeftParen)) depth++;
                    else if (gmb_p_check(p, GMB_TOK_RightParen)) depth--;
                    if (depth > 0) gmb_p_adv(p);
                }
                gmb_p_match(p, GMB_TOK_RightParen);
            }
            /* NEW LIST [OF T] {} — inizializzatore: solo VUOTO è supportato (parità C#:
             * "NEW LIST OF T con inizializzatore non ancora supportato"). Consuma '{}' e
             * ritorna la lista vuota tipizzata. Con contenuto NON consuma → errore a valle
             * (stesso esito esplicito del C#). Senza questo, il '{}' restava orfano e
             * `x = NEW LIST OF T {}` (assegnazione, non DIM) dava "statement non
             * riconosciuto" — bug SPEED.SETMANY, colpiva anche i campioni di database. */
            if (gmb_p_check(p, GMB_TOK_LeftBrace) && gmb_p_peek(p, 1)
                && gmb_p_peek(p, 1)->kind == GMB_TOK_RightBrace) {
                gmb_p_adv(p);   /* '{' */
                gmb_p_adv(p);   /* '}' */
            }
            char lnm[96];
            snprintf(lnm, sizeof(lnm), "LISTOF:%s", elem);
            return gmb_expr_call(strdup(lnm), NULL, 0, line);
        }
        char nm[128]; int l = nw->length < 127 ? nw->length : 127;
        memcpy(nm, nw->lexeme, (size_t)l); nm[l] = 0;
        gmb_p_adv(p);
        /* NEW HTTPOPTIONS → oggetto opzioni (val boxed, dict vuoto). */
        if (gmb_keyword_tok(nm) == GMB_TOK_HttpOptions) {
            if (gmb_p_match(p, GMB_TOK_LeftParen)) gmb_p_match(p, GMB_TOK_RightParen);
            return gmb_expr_call(strdup("_new_httpoptions_"), NULL, 0, line);
        }
        /* NEW HTTPCONFIG → oggetto config boxed con i default canonici (gmb_httpconfig_new,
         * helper condiviso del runtime C — parità con managed NewHttpConfigDefaults). */
        if (gmb_keyword_tok(nm) == GMB_TOK_HttpConfig) {
            if (gmb_p_match(p, GMB_TOK_LeftParen)) gmb_p_match(p, GMB_TOK_RightParen);
            return gmb_expr_call(strdup("_new_httpconfig_"), NULL, 0, line);
        }
        /* NEW JSON → oggetto JSON vuoto (val boxed dict). */
        if (gmb_keyword_tok(nm) == GMB_TOK_Json) {
            if (gmb_p_match(p, GMB_TOK_LeftParen)) gmb_p_match(p, GMB_TOK_RightParen);
            return gmb_expr_call(strdup("_new_json_"), NULL, 0, line);
        }
        /* NEW COOKIEJAR → dict cookie vuoto (val boxed dict), parità C# CookieJarType
         * (HttpFetchAdvanced: jarA.Clear / opts.CookieJar = jarA / gmb_http_jar_capture
         * sul lato fetch e gmb_http_jar_cookie_header per il Cookie: header in uscita). */
        if (gmb_keyword_tok(nm) == GMB_TOK_CookieJar) {
            if (gmb_p_match(p, GMB_TOK_LeftParen)) gmb_p_match(p, GMB_TOK_RightParen);
            return gmb_expr_call(strdup("_new_cookiejar_"), NULL, 0, line);
        }
        /* NEW NS.Cls(...) — tipo qualificato (es. AGENTS.AgentHandle): il C99Native registra
         * le classi col nome nudo → usa l'ULTIMO segmento come costruttore. Bug 2026-09-03:
         * il '.' non veniva consumato e NEW AGENTS.AgentHandle(...) era un parse error. */
        if (gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->lexeme
            && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof
            && gmb_p_peek(p, 1)->kind != GMB_TOK_LeftParen) {
            /* Il segmento può NON essere un GMB_TOK_Identifier (es. il nome-classe "Auto"
             * coincide con la keyword AUTO → GMB_TOK_Auto): si accetta qualunque nome e si
             * consumano eventuali segmenti multipli, salvando l'ULTIMO (nome nudo classe,
             * come registrata — NAMESPACE è no-op nel C99Native). Prima `NEW Veicoli.Auto(...)`
             * con "Auto" keyword → nm restava "Veicoli" → `a = Veicoli` (cc error). */
            const gmb_token_t* sub = NULL;
            while (gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->lexeme
                   && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof
                   && gmb_p_peek(p, 1)->kind != GMB_TOK_LeftParen) {
                gmb_p_adv(p);   /* '.' */
                sub = gmb_p_cur(p);
                gmb_p_adv(p);   /* sottotipo */
            }
            if (sub) { int sl = sub->length < 127 ? sub->length : 127; memcpy(nm, sub->lexeme, (size_t)sl); nm[sl] = 0; }
        }
        /* tipo generico: NEW LIST<LONG>() — salta <...> */
        if (gmb_p_check(p, GMB_TOK_LessThan)) {
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_GreaterThan)) gmb_p_adv(p);
            gmb_p_match(p, GMB_TOK_GreaterThan);
        }
        if (gmb_p_check(p, GMB_TOK_LeftParen) && gmb_p_is_collection_ctor(nm)) {
            gmb_p_adv(p);
            gmb_expr_t* init = NULL;
            if (gmb_p_check(p, GMB_TOK_LeftBracket)) init = gmb_p_parse_expr(p);
            /* NEW HASHRING<STRING>(Servers): argomento NON letterale (variabile/espressione).
             * Prima veniva ignorato (col nuovo oggetto si perdeva la lista dei nodi). */
            else if (!gmb_p_check(p, GMB_TOK_RightParen)) init = gmb_p_parse_expr(p);
            { int depth = 1; while (!gmb_p_at_end(p) && depth > 0) {
                if (gmb_p_check(p, GMB_TOK_LeftParen)) depth++;
                else if (gmb_p_check(p, GMB_TOK_RightParen)) depth--;
                if (depth > 0) gmb_p_adv(p); } }
            gmb_p_match(p, GMB_TOK_RightParen);
            char cnm2[96];
            /* Il nome del container è una keyword del lexer (tabella di frontiera,
             * direttiva 2026-09-08): il TokenKind si risolve QUI nel parser, mai a valle. */
            int ctk2 = gmb_keyword_tok(nm);
            if (init) { snprintf(cnm2, sizeof(cnm2), "COLLNEWINIT:%s", nm);
                gmb_expr_t** iargs = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*)); iargs[0] = init;
                return gmb_expr_call_typed(strdup(cnm2), (gmb_tokenkind_t)ctk2, iargs, 1, line); }
            snprintf(cnm2, sizeof(cnm2), "COLLNEW:%s", nm);
            return gmb_expr_call_typed(strdup(cnm2), (gmb_tokenkind_t)ctk2, NULL, 0, line);
        }
        if (gmb_p_match(p, GMB_TOK_LeftParen)) {
            gmb_expr_t** args = NULL; int na = 0, ca = 0;
            while (!gmb_p_check(p, GMB_TOK_RightParen) && !gmb_p_at_end(p)) {
                if (gmb_p_check(p, GMB_TOK_Comma)) { gmb_p_adv(p); continue; }
                if (na >= ca) { ca = ca ? ca * 2 : 4; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                args[na++] = gmb_p_parse_expr(p);
                if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                        gmb_p_skip_eol(p);
                        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_LineNumber
                            || (gmb_p_cur(p)->kind == GMB_TOK_Number && gmb_p_peek(p,1)
                                && gmb_p_peek(p,1)->line == gmb_p_cur(p)->line
                                && !gmb_p_is_num_op_follower(p)))) gmb_p_adv(p);
            }
            gmb_p_match(p, GMB_TOK_RightParen);
            /* Visione 6.6: risolvi il builtin per nome → TokenKind di dispatch quando il
             * lexeme corrisponde a una grafia StdLib non ambigua o a un nome enum canonico
             * (gmb_fn_tok, tabella generata — mai la names[] globale disallineata).
             * Fallback al nome se non risolto (GMB_TOK_Unknown): il dispatch resta per lexeme. */
            {
                gmb_tokenkind_t tk = gmb_fn_tok(nm);
                if (tk != GMB_TOK_Unknown)
                    return gmb_expr_call_typed(strdup(nm), tk, args, na, line);
            }
            return gmb_expr_call(strdup(nm), args, na, line);
        }
        /* NEW DICT/SORTED/QUEUE/STACK/HASHSET [OF <T,..>] — costruttore collezione vuoto:
         * sintesi COLLNEW:<Tipo> (il DIM tipizzato la inizializza con gmb_dict/list_new). */
        if (gmb_p_is_collection_ctor(nm)) {
            if (gmb_p_cur(p) && (gmb_p_tok_is_keyword(gmb_p_cur(p), "Of")
                || (gmb_p_cur(p)->kind == GMB_TOK_Of))) {
                gmb_p_adv(p);
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline)) {
                    const gmb_token_t* ot = gmb_p_cur(p);
                    if (!ot) break;
                    if (ot->lexeme && isalpha((unsigned char)ot->lexeme[0])) gmb_p_adv(p);
                    else if (gmb_p_check(p, GMB_TOK_Comma)) gmb_p_adv(p);
                    else break;
                }
            }
            char cnm[96]; snprintf(cnm, sizeof(cnm), "COLLNEW:%s", nm);
            /* Bug C99Native-Collections (2026-09-10): la forma con OF (senza parentesi)
             * creava una call UNTYPED — tk=Unknown, l'emettitore (COLLNEW:, gmb_tk_dictish)
             * cadeva su gmb_list_new() per un DICT/SORTED → assign gmb_dict_t = gmb_list_t
             * (2 errori C su CollectionsBasics: PricesLegacy/SortedL). Come il ramo con
             * parentesi (sopra, riga 786): gmb_expr_call_typed con il TokenKind del
             * container risolto qui nel parser (tabella di frontiera, direttiva 2026-09-08). */
            return gmb_expr_call_typed(strdup(cnm), gmb_keyword_tok(nm), NULL, 0, line);
        }
        /* `NEW <Tipo>` SENZA parentesi (idioma classico dei TYPE): è comunque una
         * COSTRUZIONE di istanza (0 argomenti) — prima diventava una VAR e l'emitter
         * creava una variabile implicita col nome del TIPO (`gmb_double_t Arr4 = 0`) e
         * `n = NEW Arr4` emetteva `n = gmb_obj_copy_typed(Arr4, ...)` (C non compilabile
         * con "passing 'double' to 'gmb_val_t'"). */
        return gmb_expr_call(strdup(nm), NULL, 0, line);
    }
    if (t->kind == GMB_TOK_Dot && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->length == 1 && gmb_p_peek(p, 2)
        && (gmb_p_peek(p, 1)->lexeme[0] == 'T' || gmb_p_peek(p, 1)->lexeme[0] == 'F' || gmb_p_peek(p, 1)->lexeme[0] == 't' || gmb_p_peek(p, 1)->lexeme[0] == 'f')
        && gmb_p_peek(p, 2)->kind == GMB_TOK_Dot) {
        /* .T. / .F. (Clipher) */
        char tf = gmb_p_peek(p, 1)->lexeme[0];
        gmb_p_adv(p); gmb_p_adv(p); gmb_p_adv(p);
        return gmb_expr_call(tf == 'T' || tf == 't' ? "_true_" : "_false_", NULL, 0, line);
    }
    if (t->kind == GMB_TOK_Unknown && t->length == 1 && t->lexeme[0] == '%') {
        /* costante %NOME (es. %OK, %CHAT): reference a costante. Il prefisso % viene
           conservato nel nome: il compilatore lo risolve nella tabella costanti builtin. */
        gmb_p_adv(p);
        const gmb_token_t* cn = gmb_p_cur(p);
        if (cn) { char nm[65]; int l = cn->length < 63 ? cn->length : 63; nm[0] = '%'; memcpy(nm + 1, cn->lexeme, (size_t)l); nm[l + 1] = 0; gmb_p_adv(p); return gmb_expr_var(nm, line); }
        return gmb_expr_var("_", line);
    }
    if ((t->kind == GMB_TOK_Function || gmb_p_tok_is_keyword(t, "Sub"))
        && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_LeftParen) {
        /* FN(params) [AS tipo] => expr / SUB(params) { statements }.
         * Prima il parser consumava e LIBERAVA parametri/corpo, riducendo tutto a _fn_ NIL:
         * il compilatore non poteva quindi individuare catture né emettere OP_CLOSURE. */
        int is_sub = gmb_p_tok_is_keyword(t, "Sub");
        gmb_p_adv(p);
        gmb_p_match(p, GMB_TOK_LeftParen);
        char** params = NULL; int np = 0, cp = 0;
        uint8_t* pstr = NULL; uint8_t* parr = NULL; int pscap = 0;
        char* cls0 = NULL; char* cls1 = NULL;   /* tipo UDT dei primi 2 parametri (se annotato) */
        while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen)) {
            if (gmb_p_match(p, GMB_TOK_Comma)) continue;
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "ByRef") || gmb_p_tok_is_keyword(gmb_p_cur(p), "ByVal"))
                gmb_p_adv(p);
            const gmb_token_t* pt = gmb_p_cur(p);
            if (!pt) break;
            if (np >= cp) { cp = cp ? cp * 2 : 4; params = (char**)realloc(params, (size_t)cp * sizeof(char*)); pstr = (uint8_t*)realloc(pstr, (size_t)cp); parr = (uint8_t*)realloc(parr, (size_t)cp); pscap = cp; }
            params[np] = gmb_p_tok_strdup(pt);
            pstr[np] = 0;
            parr[np] = 0;
            np++;
            gmb_p_adv(p);
            if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                parr[np - 1] = 1;   /* parametro array: FN(v() AS DOUBLE) */
                while (!gmb_p_at_end(p) && !gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_adv(p);
            }
            if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As")) {
                gmb_p_adv(p);
                const gmb_token_t* tt2 = gmb_p_cur(p);
                if (tt2 && !gmb_p_check(p, GMB_TOK_Comma) && !gmb_p_check(p, GMB_TOK_RightParen)) {
                    if (gmb_p_tok_is_string_type_name(tt2)) pstr[np - 1] = 1;
                    /* Tipo UDT/CLASSE del parametro (`FN(a AS Vendita, b AS Vendita) => ...`):
                     * si annota il NOME del tipo per i primi due parametri, così il C99Native
                     * tipizza la lambda come l'emitter C# (che legge i DataType dall'AST C#).
                     * Senza, il comparatore ASORT riceveva gmb_double_t invece del UDT boxato
                     * → campo a 0 nel corpo → ordinamento mai applicato (bug reale su
                     * ArraysComplexProcessing). Solo parole non-keyword (i tipi scalari sono
                     * keyword del lexer). */
                    else if (tt2->lexeme && (np == 1 || np == 2) && tt2->length > 0 && tt2->length < 63) {
                        /* NB: tt2->lexeme NON e' NUL-terminato (e' una slice del sorgente):
                         * va copiato PRIMA di gmb_keyword_tok, altrimenti la lookup legge
                         * oltre il token e un tipo scalare (CURRENCY/INTEGER/...) viene
                         * scambiato per un UDT. */
                        char tb[64];
                        memcpy(tb, tt2->lexeme, (size_t)tt2->length); tb[tt2->length] = 0;
                        if (gmb_keyword_tok(tb) == GMB_TOK_Identifier) {
                            char* cp = strdup(tb);
                            if (np == 1) { free(cls0); cls0 = cp; } else { free(cls1); cls1 = cp; }
                        }
                    }
                    gmb_p_adv(p);
                }
            }
        }
        if (!gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_set_err(p, "lambda: attesa ')'", line);
        int lambda_ret_str = 0;
        int lambda_ret_arr = 0;
        if (gmb_p_tok_is_keyword(gmb_p_cur(p), "As")) {
            gmb_p_adv(p);
            const gmb_token_t* rtk = gmb_p_cur(p);
            if (gmb_p_tok_is_string_type_name(rtk)) lambda_ret_str = 1;
            if (gmb_p_tok_is_keyword(rtk, "ARRAY")) lambda_ret_arr = 1;
            while (!gmb_p_at_end(p) && gmb_p_cur(p)->kind != GMB_TOK_Newline
                   && !gmb_p_check(p, GMB_TOK_FatArrow) && !gmb_p_check(p, GMB_TOK_Arrow)
                   && !gmb_p_check(p, GMB_TOK_Equal) && !gmb_p_check(p, GMB_TOK_Comma)
                   && !gmb_p_check(p, GMB_TOK_Colon)) gmb_p_adv(p);
        }
#define GMB_LAMBDA_NEW(BODYEXPR, BODYARR, NBODY) \
        do { gmb_expr_t* _le = gmb_expr_lambda(params, np, (BODYEXPR), (BODYARR), (NBODY), is_sub, line); \
             _le->lambda_params_str = pstr; pstr = NULL; _le->lambda_params_arr = parr; parr = NULL; _le->lambda_ret_str = lambda_ret_str; _le->lambda_ret_arr = lambda_ret_arr; \
             if (cls0) { _le->lambda_param_cls = cls0; cls0 = NULL; } \
             if (cls1) { _le->lambda_param_cls2 = cls1; cls1 = NULL; } \
             if (lambda_ret_arr) { \
                 gmb_expr_t* _be = (BODYEXPR); \
                 if (_be && _be->kind == GMB_EXPR_CALL && _be->var && strcmp(_be->var, "_array_") == 0) _le->lambda_ret_arr_len = _be->nargs; \
                 else { gmb_stmt_t** _ba = (gmb_stmt_t**)(BODYARR); for (int _bi = 0; _bi < (NBODY); _bi++) { gmb_stmt_t* _bs = _ba[_bi]; \
                     if (_bs && _bs->kind == GMB_STMT_RETURN && _bs->ret_value && _bs->ret_value->kind == GMB_EXPR_CALL && _bs->ret_value->var && strcmp(_bs->ret_value->var, "_array_") == 0) _le->lambda_ret_arr_len = _bs->ret_value->nargs; } } \
             } \
             return _le; } while (0)
        if (gmb_p_match(p, GMB_TOK_LeftBrace)) {
            gmb_stmt_list_t body = gmb_p_stmt_list_new();
            while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightBrace)) {
                if (gmb_p_check(p, GMB_TOK_Newline) || gmb_p_check(p, GMB_TOK_Colon)) { gmb_p_adv(p); continue; }
                bool ml = false;
                int before = p->pos;
                gmb_stmt_t* st = gmb_p_parse_stmt(p, &ml);
                if (st) gmb_p_stmt_list_add(&body, st);
                gmb_p_drain_queued(p, &body);
                if (p->pos == before) {
                    /* chiamo a funzione "nuda" come statement (es. AEVAL v, SUB(o) { Foo(o) }):
                     * il parser di statement non lo riconosce, ma un'espressione sì — la si
                     * avvolge in GMB_STMT_EXPR (cc_expr + OP_POP) invece di abbandonare. */
                    int eb = p->pos;
                    gmb_expr_t* ee = gmb_p_parse_expr(p);
                    if (ee) {
                        gmb_stmt_t* ex = gmb_stmt_new(GMB_STMT_EXPR, line);
                        ex->expr = ee;
                        gmb_p_stmt_list_add(&body, ex);
                        gmb_p_drain_queued(p, &body);
                        continue;
                    }
                    gmb_p_set_err(p, "lambda SUB: statement non riconosciuto", line); break;
                }
            }
            if (!gmb_p_match(p, GMB_TOK_RightBrace)) gmb_p_set_err(p, "lambda SUB: attesa '}'", line);
            GMB_LAMBDA_NEW(NULL, body.items, body.n);
        }
        /* forma a blocco: FN(params) AS TYPE: <body> END FN */
        if (gmb_p_match(p, GMB_TOK_Colon)) {
            gmb_stmt_list_t fb = gmb_p_stmt_list_new();
            gmb_p_parse_block(p, &fb, GMB_TOK_EndFunction, "EndFunction", line);
            if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_End) { gmb_p_adv(p); if (gmb_p_cur(p)) gmb_p_adv(p); }
            GMB_LAMBDA_NEW(NULL, fb.items, fb.n);
        }
        if (gmb_p_match(p, GMB_TOK_FatArrow) || gmb_p_match(p, GMB_TOK_Arrow) || gmb_p_match(p, GMB_TOK_Equal)) {
            gmb_expr_t* body = gmb_p_parse_expr(p);
            GMB_LAMBDA_NEW(body, NULL, 0);
        }
#undef GMB_LAMBDA_NEW
        for (int i = 0; i < np; i++) free(params[i]);
        free(params);
        free(pstr);
        free(parr);
        gmb_p_set_err(p, "lambda: atteso '=>' o corpo tra graffe", line);
        return gmb_expr_call("_fn_", NULL, 0, line);
    }
    if (t->kind == GMB_TOK_Identifier || (t->kind >= GMB_TOK_Define)) {
        /* può essere chiamata di funzione (identifier '('), variabile o member access (a.b) */
        gmb_p_adv(p);
        char name[128]; int l = t->length < 127 ? t->length : 127;
        memcpy(name, t->lexeme, (size_t)l); name[l] = 0;
        /* c?.Campo — null-conditional member access (bug "C99Native-
         * NullConditionalAccess-Non-Parsato"): percorso PARALLELO alla catena dotted-string
         * sottostante, intercettato SOLO quando '?' è seguito da '.', senza toccare la
         * concatenazione esistente per '.' semplice. Produce il sentinel _nilprop_(base,
         * "Campo"), stesso schema già in uso per _prop_ (call.membro, vedi sotto) e
         * __slice__: nil-check + gmb_dict_get sul ramo non-nil nell'emitter. */
        if (gmb_p_check(p, GMB_TOK_QuestionMark) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind == GMB_TOK_Dot) {
            gmb_p_adv(p); /* '?' */
            gmb_p_adv(p); /* '.' */
            const gmb_token_t* m = gmb_p_cur(p);
            if (m && m->kind != GMB_TOK_Eof && m->kind != GMB_TOK_Newline) {
                char fbuf[128]; int fl = m->length < 127 ? m->length : 127;
                memcpy(fbuf, m->lexeme, (size_t)fl); fbuf[fl] = 0;
                gmb_p_adv(p);
                gmb_expr_t** npargs = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                npargs[0] = gmb_expr_var(strdup(name), line);
                npargs[1] = gmb_expr_str(strdup(fbuf), m->line);
                return gmb_expr_call("_nilprop_", npargs, 2, line);
            }
        }
        /* member/namespace access: Nome.membro[(...)] oppure Nome.membro.membro... */
        while (gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof
           && !gmb_p_at_end_compound(p, GMB_TOK_Dot) && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline
           && !gmb_p_is_dot_op(p, "AND") && !gmb_p_is_dot_op(p, "OR") && !gmb_p_is_dot_op(p, "NOT")) {
            gmb_p_adv(p);
            const gmb_token_t* m = gmb_p_cur(p);
            if (!m) break;
            char seg[128]; int ml = m->length < 127 ? m->length : 127;
            memcpy(seg, m->lexeme, (size_t)ml); seg[ml] = 0;
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "%s.%s", name, seg);
            if (strlen(tmp) >= sizeof(name)) break;
            strcpy(name, tmp);
            gmb_p_adv(p);
            /* regola d'oro: la qualifica namespace non si dispatcha mai — se il primo
             * segmento è un namespace noto (gmb_ns_names), l'emitter deve ricevere solo
             * il nome nudo (es. FINANCE.CUMIPMT → CUMIPMT). I metodi su oggetto/UDT
             * (obj.metodo) NON hanno il primo segmento nei namespace → restano qualificati. */
            if (gmb_p_check(p, GMB_TOK_LeftParen)) {
                char nsbuf[128]; nsbuf[0] = 0;
                const char* bare = gmb_p_strip_ns(p, name, (int)strlen(name), nsbuf, (int)sizeof(nsbuf));
                if (bare) { char barebuf[128]; int bl = 0; while (bare[bl] && bl < 127) { barebuf[bl] = bare[bl]; bl++; } barebuf[bl] = 0; strcpy(name, barebuf); }
                /* Statico di CLASSE builtin il cui namespace NON è nel registry (KEY): il
                 * qualificatore resta nel nome ("KEY.MAKE") perché gmb_p_strip_ns conosce solo
                 * i namespace della superficie. Qui si separa e si marca nsbuf, così il
                 * dispatch arriva al TokenKind canonico (GMB_TOK_Make) e all'handler già
                 * esistente nell'emitter (gf_key_make). Vedi gmb_p_class_static_lookup. */
                if (!nsbuf[0]) {
                    const char* _csdot = strchr(name, '.');
                    if (_csdot && _csdot != name && !strchr(_csdot + 1, '.')) {
                        char _csns[64]; size_t _csl = (size_t)(_csdot - name);
                        if (_csl > 0 && _csl < sizeof(_csns)) {
                            memcpy(_csns, name, _csl); _csns[_csl] = 0;
                            if (gmb_p_class_static_lookup(_csns, _csdot + 1) != GMB_TOK_Unknown) {
                                snprintf(nsbuf, sizeof(nsbuf), "%s", _csns);
                                snprintf(name, 128, "%s", _csdot + 1);
                            }
                        }
                    }
                }
                gmb_p_adv(p);
                gmb_expr_t** args = NULL; int na = 0, ca = 0;
                if (!gmb_p_check(p, GMB_TOK_RightParen)) {
                    for (;;) {
                        if (na >= ca) { ca = ca ? ca * 2 : 4; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                        args[na++] = gmb_p_parse_expr(p);
                        if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                        gmb_p_skip_eol(p);
                        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_LineNumber
                            || (gmb_p_cur(p)->kind == GMB_TOK_Number && gmb_p_peek(p,1)
                                && gmb_p_peek(p,1)->line == gmb_p_cur(p)->line
                                && !gmb_p_is_num_op_follower(p)))) gmb_p_adv(p);
                    }
                }
                if (!gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_set_err(p, "attesa ')' in chiamata", line);
                /* regola d'oro: risolvi il TokenKind della call (mai dispatch per lexeme).
                 * Fonte canonica: gmb_ns_lookup (ns, lexeme di REGISTRO) → TokenKind, che
                 * gestisce i conflitti per namespace (SHARPERATIO in METRICS vs PORTFOLIO)
                 * e i lexeme ≠ nome enum (EXPECTEDRETURN → PortfolioExpectedReturn).
                 * Fallback SOLO su nomi enum (chiavi canoniche) — mai keyword del lexer. */
                /* `WORLD` e `CLOCK` non sono impliciti: anche le LETTURE qualificate
                 * (`WORLD.TIME()`, `WORLD.MOVING(1)`, `CLOCK.DT()`, `CLOCK.ALPHA()`, …) richiedono
                 * il loro IMPORT, come i comandi (stessa regola e stesso messaggio). */
                if (nsbuf[0] && (strcasecmp(nsbuf, "WORLD") == 0 || strcasecmp(nsbuf, "CLOCK") == 0)
                    && !gmb_p_require_import(p, nsbuf, line))
                    return gmb_expr_num(0, line);
                int ct = -1;
                if (nsbuf[0]) {
                    gmb_tokenkind_t ctNS = gmb_p_ns_lookup(nsbuf, name);
                    if (ctNS != GMB_TOK_Unknown) ct = (int)ctNS;
                    /* statico di CLASSE builtin (KEY.MAKE): fuori dalla superficie procedure */
                    else {
                        gmb_tokenkind_t ctCS = gmb_p_class_static_lookup(nsbuf, name);
                        if (ctCS != GMB_TOK_Unknown) ct = (int)ctCS;
                    }
                    if (ct < 0) {
                    /* fallback al nome NUDO: serve ai builtin di namespace che il lookup
                     * (ns,nome) non risolve. Il caso FFI-collide (membro utente/FFI omonimo di
                     * un builtin, es. PROTOBUF.PARSE) è risolto a valle nell'emitter C99Native,
                     * che per una call qualificata preferisce il simbolo utente/FFI (find_fn_arity)
                     * al builtin nudo (vedi emit_call). */
                    ct = (int)gmb_fn_tok(name) != (int)GMB_TOK_Unknown ? (int)gmb_fn_tok(name) : -1;
                    }
                } else {
                    ct = (int)gmb_fn_tok(name) != (int)GMB_TOK_Unknown ? (int)gmb_fn_tok(name) : -1;
                }
                /* CUA/dominio: il token è NAMESPACED (es. ToolSearch). Il membro nudo da
                 * solo collide (SEARCH = funzione stringhe): con ns noto prova prima
                 * ns+membro (TOOL+SEARCH -> ToolSearch), fallback al nudo. */
                if (nsbuf[0] && ct < 0) {
                    char cand[200];
                    int cl = snprintf(cand, sizeof(cand), "%s%s", nsbuf, name);
                    if (cl > 0 && cl < (int)sizeof(cand)) {
                        gmb_tokenkind_t ct2 = gmb_fn_tok(cand);
                        if (ct2 != GMB_TOK_Unknown) ct = (int)ct2;
                    }
                }
                gmb_expr_t* mc = nsbuf[0] ? gmb_expr_call_ns(name, nsbuf, args, na, line)
                                          : (ct >= 0 ? gmb_expr_call_typed(name, (gmb_tokenkind_t)ct, args, na, line)
                                                    : gmb_expr_call(name, args, na, line));
                if (nsbuf[0] && ct >= 0) mc->tk = (gmb_tokenkind_t)ct;
                /* chiamata concatenata: obj.metodo(args).altro.metodo(args) — consuma la
                 * catena; una property SENZA parentesi dopo la call (f().Campo) viene
                 * avvolta in _prop_(f,"Campo") invece di essere persa (campo NIL). */
                for (;;) {
                    if (!(gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1) && gmb_p_peek(p, 1)->kind != GMB_TOK_Newline && gmb_p_peek(p, 1)->kind != GMB_TOK_Eof)) break;
                    gmb_p_adv(p);
                    const gmb_token_t* m2 = gmb_p_cur(p);
                    if (!m2) break;
                    gmb_p_adv(p);
                    if (gmb_p_match(p, GMB_TOK_LeftParen)) {
                        int d = 1;
                        while (!gmb_p_at_end(p) && d > 0) {
                            if (gmb_p_match(p, GMB_TOK_LeftParen)) d++;
                            else if (gmb_p_match(p, GMB_TOK_RightParen)) d--;
                            else if (gmb_p_at_end(p)) break;
                            else gmb_p_adv(p);
                        }
                    } else {
                        int plen = m2->length < 127 ? m2->length : 127;
                        char pn[128]; memcpy(pn, m2->lexeme, (size_t)plen); pn[plen] = 0;
                        gmb_expr_t** pargs = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                        pargs[0] = mc;
                        pargs[1] = gmb_expr_str(strdup(pn), m2->line);
                        mc = gmb_expr_call("_prop_", pargs, 2, m2->line);
                    }
                }
                return mc;
            }
            /* indice a parentesi quadre: .membro[expr] — Bug corretto (2026-09-07):
             * prima le quadre venivano SCARTATE (loop di skip fino a ']'), quindi
             * `obj.campo[1]` su una lista/dict JSON restituiva il campo INTERO (niente
             * indice) invece dell'elemento. Ora parsa l'indice e produce la stessa CALL
             * "membro"(idx) del caso a parentesi tonde (cc_expr member+index). */
            if (gmb_p_check(p, GMB_TOK_LeftBracket)) {
                gmb_expr_t** bargs = NULL; int bn = 0, bc = 0, bsafe = 0;
                while (gmb_p_match(p, GMB_TOK_LeftBracket)) {
                    gmb_expr_t* bidx = gmb_p_parse_expr(p);
                    gmb_p_match(p, GMB_TOK_RightBracket);
                    if (gmb_p_match(p, GMB_TOK_QuestionMark)) bsafe = 1;
                    if (bidx) {
                        if (bn >= bc) { bc = bc ? bc * 2 : 4; bargs = (gmb_expr_t**)realloc(bargs, (size_t)bc * sizeof(gmb_expr_t*)); }
                        bargs[bn++] = bidx;
                    }
                }
                gmb_expr_t* bcall = gmb_expr_call(strdup(name), bargs, bn, line);
                bcall->is_safe = bsafe;
                return bcall;
            }
        }
        /* QUERY["key"]: QUERY è una keyword riservata dynamic-scope per le pagine BSP (mai
         * una DIM normale — seminata per NOME dall'host prima della run, vedi
         * gmb_vm_bsp.c/vm_apply_bsp_seeds), NON un array/dict qualsiasi. Il ramo generico
         * subito sotto SCARTA il contenuto delle quadre per QUALSIASI identificatore (bug
         * pre-esistente più ampio, fuori scope di questo fix — vedi TODO-PCODE-BUGS.md
         * classe "dynamic scope silenzioso"): bug reale, `QUERY["name"]` compilava come
         * riferimento nudo all'INTERO dict QUERY (l'oggetto, non il valore "name"
         * indicizzato), senza mai un errore. Fix mirato SOLO per QUERY (non tocca il
         * comportamento generico array/dict, per non rischiare regressioni su codice che
         * già dipende da quel comportamento in altri contesti). Vedi gmb_compiler.c per
         * il lato compilatore (auto-crea/trova lo slot globale "QUERY" per nome, poi
         * LOAD_ELEMENT — stesso contratto di FORM("key"), già un GMB_EXPR_CALL corretto).
         */
        if (gmb_p_check(p, GMB_TOK_LeftBracket) && strcasecmp(name, "QUERY") == 0) {
            gmb_expr_t* qexpr = NULL;
            while (gmb_p_match(p, GMB_TOK_LeftBracket)) {
                gmb_expr_t* idx = gmb_p_parse_expr(p);
                gmb_p_match(p, GMB_TOK_RightBracket);
                gmb_p_match(p, GMB_TOK_QuestionMark); /* d["x"]? — null-check Clipper */
                if (!qexpr) {
                    gmb_expr_t** qargs = (gmb_expr_t**)malloc(sizeof(gmb_expr_t*));
                    qargs[0] = idx;
                    qexpr = gmb_expr_call(name, qargs, 1, line);
                } else {
                    gmb_expr_t** ia = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                    ia[0] = qexpr; ia[1] = idx;
                    qexpr = gmb_expr_call("_idx_", ia, 2, line);
                }
            }
            return qexpr;
        }
        /* indici a quadre dopo la variabile: pkgs[i]["Name"] / l[1] / d["k"] → CALL(name, i)
         * (prima venivano SILENZIOSAMENTE SCARTATI: il postfix in parse_power/mul non li
         * vedeva mai perché questo ramo li consumava senza produrre AST — bug che rendeva
         * inutilizzabile la sintassi moderna a quadre per array/list/dict). */
        {
            int any_br = 0;
            int safe_br = 0;   /* arr[i]? — accesso sicuro */
            gmb_expr_t** iargs = NULL; int ni = 0, ci = 0;
            while (gmb_p_check(p, GMB_TOK_LeftBracket)) {
                any_br = 1;
                gmb_p_adv(p);
                /* a[start:end] (ArraySliceExpr, bug "C99Native-ArraySlice-Parse"): QUESTO
                 * ramo (indice/i dopo un identificatore in posizione PRIMARY) e' quello che
                 * intercetta DAVVERO un `a[...]` che segue un identificatore — il postfix in
                 * parse_mul/parse_power (dove lo slice era stato aggiunto per primo) non vede mai
                 * '[' perche' a quel punto la CALL e' gia' stata costruita qui e ha gia' fatto
                 * `return`. Nessun lookahead con backtrack: si parsa la prima espressione UNA
                 * SOLA volta (o si usa 0 come start implicito se ':' e' subito il primo token,
                 * `a[:end]`) e si decide guardando cosa la segue. Stesso schema sentinella
                 * `__slice__`/e->str = base, vedi gmb_p_parse_bracket_subscript. */
                gmb_expr_t* first_idx = gmb_p_check(p, GMB_TOK_RightBracket) || gmb_p_check(p, GMB_TOK_Colon)
                                         ? NULL : gmb_p_parse_expr(p);
                if (gmb_p_check(p, GMB_TOK_Colon)) {
                    gmb_p_adv(p); /* ':' */
                    gmb_expr_t** sa = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                    sa[0] = first_idx ? first_idx : gmb_expr_num(0.0, line);
                    int sn = 1;
                    if (!gmb_p_check(p, GMB_TOK_RightBracket)) { sa[1] = gmb_p_parse_expr(p); sn = 2; }
                    gmb_p_match(p, GMB_TOK_RightBracket);
                    gmb_expr_t* call = gmb_expr_call("__slice__", sa, sn, line);
                    call->str = strdup(name);
                    free(iargs);
                    return call;
                }
                /* non e' uno slice: first_idx (se presente) e' il primo indice normale,
                 * il resto (virgola-separato) continua come prima. */
                if (first_idx) {
                    if (ni >= ci) { ci = ci ? ci * 2 : 4; iargs = (gmb_expr_t**)realloc(iargs, (size_t)ci * sizeof(gmb_expr_t*)); }
                    iargs[ni++] = first_idx;
                    while (gmb_p_match(p, GMB_TOK_Comma)) {
                        gmb_p_skip_eol(p);
                        gmb_expr_t* ie = gmb_p_parse_expr(p);
                        if (ie) {
                            if (ni >= ci) { ci = ci ? ci * 2 : 4; iargs = (gmb_expr_t**)realloc(iargs, (size_t)ci * sizeof(gmb_expr_t*)); }
                            iargs[ni++] = ie;
                        }
                    }
                }
                gmb_p_match(p, GMB_TOK_RightBracket);
                if (gmb_p_match(p, GMB_TOK_QuestionMark)) safe_br = 1;   /* d["x"]? */
                /* un solo gruppo a quadre per ora (multi-gruppo raro: [a][b] non mappato) */
                break;
            }
            if (any_br) {
                int ct3 = (int)gmb_fn_tok(name);
                if (ct3 == (int)GMB_TOK_Unknown) ct3 = -1;
                gmb_expr_t* cbr = ct3 >= 0 ? gmb_expr_call_typed(name, (gmb_tokenkind_t)ct3, iargs, ni, line)
                                           : gmb_expr_call(name, iargs, ni, line);
                if (safe_br) { cbr->is_safe = 1; safe_br = 0; }
                /* gruppi a quadre SUCCESSIVI al primo: arr[i]["k"] → _idx_(base, idx) annidato
                 * (parità _idx_ dell'emitter, stesso schema del ramo QUERY). Prima il 2°/…
                 * gruppo era NON mappato → il '[' restava → condizione IF "atteso THEN". */
                while (gmb_p_check(p, GMB_TOK_LeftBracket)) {
                    gmb_p_adv(p);
                    gmb_expr_t* idx2 = (gmb_p_check(p, GMB_TOK_RightBracket) || gmb_p_check(p, GMB_TOK_Colon))
                                       ? NULL : gmb_p_parse_expr(p);
                    gmb_p_match(p, GMB_TOK_RightBracket);
                    gmb_p_match(p, GMB_TOK_QuestionMark);   /* d["x"]? — null-check Clipper */
                    if (!idx2) break;
                    gmb_expr_t** ia2 = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                    ia2[0] = cbr; ia2[1] = idx2;
                    cbr = gmb_expr_call("_idx_", ia2, 2, line);
                }
                /* arr[i].Membro / d["k"].Membro: il membro dopo l'indice diventa _prop_(indicizzato, "Membro") (vedi gmb_p_subscript_members) */
                return gmb_p_subscript_members(p, cbr);
            }
        }
        /* tipo composto: LIST OF INTEGER, ARRAY OF DOUBLE, CHANNEL OF INTEGER ... */
        if (gmb_p_cur(p) && gmb_p_cur(p)->kind == GMB_TOK_Of && gmb_p_peek(p, 1)) {
            gmb_p_adv(p);
            const gmb_token_t* ty = gmb_p_cur(p);
            gmb_p_adv(p);
            if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_LeftParen || gmb_p_cur(p)->kind == GMB_TOK_LeftBracket)) {
                while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_RightParen) && !gmb_p_check(p, GMB_TOK_RightBracket)) gmb_p_adv(p);
                gmb_p_match(p, GMB_TOK_RightParen); gmb_p_match(p, GMB_TOK_RightBracket);
            }
            return gmb_expr_var(ty ? gmb_p_tok_strdup(ty) : "_of_", ty ? ty->line : line);
        }
        if (gmb_p_check(p, GMB_TOK_LeftParen)) {
            gmb_p_adv(p);
            gmb_expr_t** args = NULL; int na = 0, ca = 0;
            if (!gmb_p_check(p, GMB_TOK_RightParen)) {
                for (;;) {
                    if (na >= ca) { ca = ca ? ca * 2 : 4; args = (gmb_expr_t**)realloc(args, (size_t)ca * sizeof(gmb_expr_t*)); }
                    args[na++] = gmb_p_parse_expr(p);
                    if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                        gmb_p_skip_eol(p);
                        if (gmb_p_cur(p) && (gmb_p_cur(p)->kind == GMB_TOK_LineNumber
                            || (gmb_p_cur(p)->kind == GMB_TOK_Number && gmb_p_peek(p,1)
                                && gmb_p_peek(p,1)->line == gmb_p_cur(p)->line
                                && !gmb_p_is_num_op_follower(p)))) gmb_p_adv(p);
                    gmb_p_skip_eol(p);
                }
            }
            if (!gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_set_err(p, "attesa ')' in chiamata", line);
            /* regola d'oro: risolvi il TokenKind della call (mai dispatch per lexeme) */
            int ct2 = (int)gmb_p_dollar_alias(name);
            if (ct2 == (int)GMB_TOK_Unknown) ct2 = (int)gmb_fn_tok(name);
            /* Nome NUDO di funzione di LIBRERIA con namespace IMPORTATO (es. `X()` con
             * IMPORT MOUSE → MOUSE.X; `UBOUND(x)` con IMPORT SYSTEM): risolvi via la tabella
             * canonica (ns, nome) sui namespace importati. Il C# risolve così; senza, l'emitter
             * dava "simbolo non trovato: X è una funzione di libreria (namespace MOUSE)". */
            const char* ibns = NULL;
            /* Risolvi sui namespace IMPORTATI anche quando il nome ha GIÀ un tk da keyword/
             * gmb_fn_tok, SE la call è a 0 argomenti: una call nuda 0-arg non può essere la
             * variante omonima (es. LEFT editora stringhe richiede 2 arg) → l'import ha
             * precedenza (LEFT()→MOUSE.LEFT, CLICK()→MOUSE.CLICK). */
            if ((ct2 == (int)GMB_TOK_Unknown || na == 0) && p) {
                for (int ii = 0; ii < p->import_aliases_n; ii++) {
                    const char* chns = p->import_aliases_ns[ii][0] ? p->import_aliases_ns[ii]
                                                                   : p->import_aliases[ii];
                    gmb_tokenkind_t tkn = gmb_p_ns_lookup(chns, name);
                    if (tkn != GMB_TOK_Unknown) { ct2 = (int)tkn; ibns = chns; break; }
                }
            }
            if (ct2 == (int)GMB_TOK_Unknown) ct2 = -1;
            /* Se il lookup per NOME fallisce ma il TOKEN originale è una keyword valida
             * (es. STR$ → lexer produce GMB_TOK_Str, ma il lookup per nome "STR$" non
             * lo trova perché il mapping è "Str"), usa il tokenkind del token: il suffisso
             * $/%/! è solo il tipo di ritorno delle funzioni legacy. Bug 2026-09-03: STR$(x)
             * era dispatchato per lexeme (tk=0) e l'emissione restava vuota. */
            if (ct2 < 0 && t->kind != GMB_TOK_Identifier && t->kind != GMB_TOK_Unknown)
                ct2 = (int)t->kind;
            /* GATE dei NAMESPACE (parità `KnownLibraryNotImported` del linker C#): un nome
             * NUDO di funzione di libreria è lecito SOLO se il suo namespace è fra i default
             * del linguaggio o è stato IMPORTato — `ANGLE2D(...)` richiede `IMPORT VECTOR`,
             * altrimenti si usa `VECTOR.ANGLE2D(...)`. `ibns` non-NULL = risolto su un
             * namespace IMPORTato (riga sopra) → già lecito. La forma qualificata non passa
             * da qui (name contiene '.'). Su errore p->error è settato e gmb_parse abortisce. */
            if (ct2 >= 0 && !ibns && name[0] && !strchr(name, '.'))
                (void)gmb_p_check_bare_builtin(p, (gmb_tokenkind_t)ct2, name, line);
            gmb_expr_t* call = ct2 >= 0 ? gmb_expr_call_typed(name, (gmb_tokenkind_t)ct2, args, na, line)
                                        : gmb_expr_call(name, args, na, line);
            if (gmb_p_match(p, GMB_TOK_QuestionMark)) call->is_safe = 1;   /* arr(9)? */
            /* risultato di chiamata indicizzato: FROMHEX(s)(0) → _idx_(call, 0).
             * CATENA: cj("a")(0)("b")("c") → _idx_ annidati (prima solo un livello →
             * parse error, es. AiServerDemo/LlmServerChatDemo: JSON annidato). */
            while (gmb_p_check(p, GMB_TOK_LeftParen)) {
                gmb_p_adv(p);
                gmb_expr_t* idx = gmb_p_parse_expr(p);
                gmb_p_match(p, GMB_TOK_RightParen);
                gmb_expr_t** idxargs = (gmb_expr_t**)malloc(2 * sizeof(gmb_expr_t*));
                idxargs[0] = call; idxargs[1] = idx;
                call = gmb_expr_call("_idx_", idxargs, 2, line);
            }
            /* se la call è già stata indicizzata (catena), il membro successivo NON va
             * fuso sul nome originale: si ritorna il valore indicizzato. */
            if (call != NULL && call->var && strcmp(call->var, "_idx_") == 0 && gmb_p_check(p, GMB_TOK_Dot))
                return call;
            /* call.membro: all_matches(0).Value — 2.7(d): PRESERVA gli indici:
             * arr(i).Membro → CALL("arr.Membro", [i]); arr(i).Metodo(args) →
             * CALL("arr.Metodo", [i, args...]) (l'emitter separa indici/metodo) */
            if (gmb_p_check(p, GMB_TOK_Dot) && gmb_p_peek(p, 1)) {
                gmb_p_adv(p);
                const gmb_token_t* m = gmb_p_cur(p);
                char qn[256];
                snprintf(qn, sizeof(qn), "%s.%s", name, m ? gmb_p_tok_strdup(m) : "?");
                if (m) gmb_p_adv(p);
                int mna = call->nargs;
                gmb_expr_t** margs = mna > 0 ? (gmb_expr_t**)malloc((size_t)mna * sizeof(gmb_expr_t*)) : NULL;
                for (int k = 0; k < mna; k++) margs[k] = call->args[k];
                free(call->args); free(call->var); free(call);
                if (gmb_p_check(p, GMB_TOK_LeftParen)) {
                    gmb_p_adv(p);
                    if (!gmb_p_check(p, GMB_TOK_RightParen)) {
                        for (;;) {
                            margs = (gmb_expr_t**)realloc(margs, (size_t)(mna + 1) * sizeof(gmb_expr_t*));
                            margs[mna++] = gmb_p_parse_expr(p);
                            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
                            gmb_p_skip_eol(p);
                        }
                    }
                    if (!gmb_p_match(p, GMB_TOK_RightParen)) gmb_p_set_err(p, "attesa ')' in chiamata", line);
                }
                {
                    gmb_expr_t* mc2 = gmb_expr_call(strdup(qn), margs, mna, line);
                    /* regola d'oro: porta il token del PREFISSO quando il membro e' fuso sul
                     * risultato di una call (es. JSON.Parse(...).campo -> pf_tk=Parse): mai
                     * from_name sul prefisso a valle. */
                    mc2->pf_tk = ct2 >= 0 ? (gmb_tokenkind_t)ct2 : 0;
                    return mc2;
                }
            }
            return call;        }
        /* Qualifica namespace su un riferimento a VALORE (costante/global PUBLIC esportato,
         * SENZA parentesi di chiamata), es. `Calcoli.PIGRECO`. Il ramo di CALL sopra riduce
         * gia' `Ns.membro(...)` a nome nudo tramite gmb_p_strip_ns; un riferimento a valore
         * qualificato invece usciva dal member-loop senza mai passare da li', accumulando
         * `name="Calcoli.PIGRECO"` e cadendo qui come var sconosciuta (mai registrata col
         * qualificatore) → NIL. Regola d'oro: il namespace non si dispatcha mai — le entita'
         * PUBLIC del namespace sono registrate col NOME NUDO nel modulo importatore, quindi
         * riduco "Ns.Membro" → "Membro" SOLO quando il primo segmento e' un namespace noto
         * (builtin gmb_ns_names o alias di IMPORT). Se non lo e' (oggetto/UDT obj.campo),
         * gmb_p_strip_ns ritorna NULL e il nome resta qualificato per il member access. */
        {
            char nsbuf[128]; nsbuf[0] = 0;
            const char* bare = gmb_p_strip_ns(p, name, (int)strlen(name), nsbuf, (int)sizeof(nsbuf));
            if (bare && nsbuf[0]) {
                char barebuf[128]; int bl = 0;
                while (bare[bl] && bl < 127) { barebuf[bl] = bare[bl]; bl++; }
                barebuf[bl] = 0;
                strcpy(name, barebuf);
            }
        }
        /* TIME$/TI$/DATE$ NUDI (senza parentesi): chiamata a 0 argomenti dell'alias a stringa. */
        {
            gmb_tokenkind_t dal = gmb_p_dollar_alias(name);
            if (dal == GMB_TOK_Unknown && (t->kind == GMB_TOK_StrTime || t->kind == GMB_TOK_StrDate || t->kind == GMB_TOK_StrTi))
                dal = t->kind;   /* STRTIME/STRDATE/STRTI nudi: stessa chiamata a 0 argomenti (parita' IL/PCode/VM-C) */
            if (dal != GMB_TOK_Unknown) return gmb_expr_call_typed(name, dal, NULL, 0, line);
        }
        return gmb_expr_var(name, line);
    }
    gmb_p_set_err(p, "espressione non valida", t->line);
    return NULL;
}
