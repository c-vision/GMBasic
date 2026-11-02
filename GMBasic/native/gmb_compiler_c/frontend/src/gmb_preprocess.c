/* native/gmb_runtime_c/src/gmb_preprocess.c — preprocessore (speculare a
 * Lexer.PhasePreProcessAll + EvaluatePreprocessorExpr). Gestisce le direttive
 * #DEF/#UNDEF/#IFDEF/#IFUNDEF/#IF/#ELSEIF/#ELSE/#ENDIF con uno stack di inclusione.
 * Le righe escluse non passano al tokenizer.
 */
#include "gmb_preprocess.h"
#include "gmb_tokenkind.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define GMB_PP_MAX_SYMS 256
#define GMB_PP_MAX_DEPTH 64

/* Vocabolario del preprocessore (frontiera: opera PRIMA del lexer). Le direttive #IF/#ELSE/
 * ... e i simboli predefiniti TRUE/FALSE NON passano dal Lexer: tabella LOCALE qui, mai
 * gmb_token_names/from_name (i lookup per nome esistono solo in lexer/parser). */
enum { PPW_NONE, PPW_DEF, PPW_UNDEF, PPW_IFDEF, PPW_IFUNDEF,
       PPW_IF, PPW_ELSEIF, PPW_ELSE, PPW_ENDIF, PPW_TRUE, PPW_FALSE };
static int pp_word(const char* w)
{
    if (!w) return PPW_NONE;
    static const struct { const char* w; int k; } tab[] = {
        { "DEF", PPW_DEF }, { "UNDEF", PPW_UNDEF }, { "IFDEF", PPW_IFDEF },
        { "IFUNDEF", PPW_IFUNDEF }, { "IF", PPW_IF }, { "ELSEIF", PPW_ELSEIF },
        { "ELSE", PPW_ELSE }, { "ENDIF", PPW_ENDIF },
        { "TRUE", PPW_TRUE }, { "FALSE", PPW_FALSE }, { 0, 0 } };
    for (int i = 0; tab[i].w; i++)
        if (strcasecmp(w, tab[i].w) == 0) return tab[i].k;
    return PPW_NONE;
}

typedef struct { char* name; char* value; } gmb_pp_sym_t;

typedef struct gmb_pp_state {
    gmb_pp_sym_t syms[GMB_PP_MAX_SYMS];
    int nsyms;
} gmb_pp_state_t;

static gmb_pp_state_t g_pp;

static const char* pp_sym_value(const char* name)
{
    for (int i = 0; i < g_pp.nsyms; i++)
        if (strcasecmp(g_pp.syms[i].name, name) == 0) return g_pp.syms[i].value;
    return NULL;
}
static bool pp_sym_defined(const char* name) { return pp_sym_value(name) != NULL; }

static void pp_def(const char* name, const char* value)
{
    for (int i = 0; i < g_pp.nsyms; i++)
        if (strcasecmp(g_pp.syms[i].name, name) == 0) {
            free(g_pp.syms[i].value);
            g_pp.syms[i].value = value ? strdup(value) : NULL;
            return;
        }
    if (g_pp.nsyms < GMB_PP_MAX_SYMS) {
        g_pp.syms[g_pp.nsyms].name = strdup(name);
        g_pp.syms[g_pp.nsyms].value = value ? strdup(value) : NULL;
        g_pp.nsyms++;
    }
}
static void pp_undef(const char* name)
{
    for (int i = 0; i < g_pp.nsyms; i++)
        if (strcasecmp(g_pp.syms[i].name, name) == 0) {
            free(g_pp.syms[i].name); free(g_pp.syms[i].value);
            for (int k = i; k < g_pp.nsyms - 1; k++) g_pp.syms[k] = g_pp.syms[k + 1];
            g_pp.nsyms--;
            return;
        }
}

/* tokenizzatore per l'espressione della #IF: numeri, simboli, operatori, TRUE/FALSE */
typedef struct { double val; bool is_num; char name[64]; } pp_tok_t;

static int pp_lex(const char* s, pp_tok_t* toks, int max)
{
    int n = 0, i = 0;
    while (s[i] && n < max) {
        while (s[i] == ' ' || s[i] == '\t') i++;
        if (!s[i]) break;
        if (isdigit((unsigned char)s[i])) {
            toks[n].is_num = true; toks[n].val = atof(s + i);
            while (isdigit((unsigned char)s[i]) || s[i] == '.') i++;
            n++;
        } else if (isalpha((unsigned char)s[i]) || s[i] == '_') {
            int j = 0;
            while ((isalnum((unsigned char)s[i]) || s[i] == '_') && j < 63) toks[n].name[j++] = s[i++];
            toks[n].name[j] = 0;
            toks[n].is_num = false;
            n++;
        } else { /* operatore */
            toks[n].is_num = false;
            toks[n].name[0] = s[i]; toks[n].name[1] = 0;
            if (s[i] == '=' && s[i+1] == '=') { toks[n].name[1] = '='; toks[n].name[2] = 0; i++; }
            else if (s[i] == '<' && s[i+1] == '=') { toks[n].name[1] = '='; toks[n].name[2] = 0; i++; }
            else if (s[i] == '>' && s[i+1] == '=') { toks[n].name[1] = '='; toks[n].name[2] = 0; i++; }
            else if (s[i] == '<' && s[i+1] == '>') { toks[n].name[1] = '>'; toks[n].name[2] = 0; i++; }
            i++;
            n++;
        }
    }
    return n;
}

/* piccolo valutatore di espressioni (precedenza: or < and < not < cmp < add < mul < unary) */
static double pp_expr(const pp_tok_t* t, int n, int* pos);
static double pp_or(const pp_tok_t* t, int n, int* pos);
static double pp_and(const pp_tok_t* t, int n, int* pos);
static double pp_cmp(const pp_tok_t* t, int n, int* pos);
static double pp_add(const pp_tok_t* t, int n, int* pos);
static double pp_mul(const pp_tok_t* t, int n, int* pos);
static double pp_unary(const pp_tok_t* t, int n, int* pos);
static double pp_primary(const pp_tok_t* t, int n, int* pos);

static bool tok_is(const pp_tok_t* t, const char* s)
{
    if (t->is_num) return false;
    return strcasecmp(t->name, s) == 0;
}
static double tok_val(const pp_tok_t* t)
{
    if (t->is_num) return t->val;
    const char* v = pp_sym_value(t->name);
    if (v) return atof(v);
    int pw = pp_word(t->name);
    if (pw == PPW_TRUE) return 1;
    if (pw == PPW_FALSE) return 0;
    return 0; /* simbolo non definito = 0 */
}

static double pp_expr(const pp_tok_t* t, int n, int* pos) { return pp_or(t, n, pos); }
static double pp_or(const pp_tok_t* t, int n, int* pos)
{
    double v = pp_and(t, n, pos);
    while (*pos < n && (tok_is(&t[*pos], "OR") || tok_is(&t[*pos], "||"))) { (*pos)++; v = (v != 0 || pp_and(t, n, pos) != 0); }
    return v;
}
static double pp_and(const pp_tok_t* t, int n, int* pos)
{
    double v = pp_cmp(t, n, pos);
    while (*pos < n && (tok_is(&t[*pos], "AND") || tok_is(&t[*pos], "&&"))) { (*pos)++; v = (v != 0 && pp_cmp(t, n, pos) != 0); }
    return v;
}
static double pp_cmp(const pp_tok_t* t, int n, int* pos)
{
    double v = pp_add(t, n, pos);
    while (*pos < n) {
        double r;
        if (tok_is(&t[*pos], "=") || tok_is(&t[*pos], "==")) { (*pos)++; r = pp_add(t, n, pos); v = (v == r); }
        else if (tok_is(&t[*pos], "<>") || tok_is(&t[*pos], "!=")) { (*pos)++; r = pp_add(t, n, pos); v = (v != r); }
        else if (tok_is(&t[*pos], "<")) { (*pos)++; r = pp_add(t, n, pos); v = (v < r); }
        else if (tok_is(&t[*pos], "<=")) { (*pos)++; r = pp_add(t, n, pos); v = (v <= r); }
        else if (tok_is(&t[*pos], ">")) { (*pos)++; r = pp_add(t, n, pos); v = (v > r); }
        else if (tok_is(&t[*pos], ">=")) { (*pos)++; r = pp_add(t, n, pos); v = (v >= r); }
        else break;
    }
    return v;
}
static double pp_add(const pp_tok_t* t, int n, int* pos)
{
    double v = pp_mul(t, n, pos);
    while (*pos < n) {
        if (tok_is(&t[*pos], "+")) { (*pos)++; v += pp_mul(t, n, pos); }
        else if (tok_is(&t[*pos], "-")) { (*pos)++; v -= pp_mul(t, n, pos); }
        else break;
    }
    return v;
}
static double pp_mul(const pp_tok_t* t, int n, int* pos)
{
    double v = pp_unary(t, n, pos);
    while (*pos < n) {
        if (tok_is(&t[*pos], "*")) { (*pos)++; v *= pp_unary(t, n, pos); }
        else if (tok_is(&t[*pos], "/")) { (*pos)++; v /= pp_unary(t, n, pos); }
        else break;
    }
    return v;
}
static double pp_unary(const pp_tok_t* t, int n, int* pos)
{
    if (*pos < n && tok_is(&t[*pos], "-")) { (*pos)++; return -pp_unary(t, n, pos); }
    if (*pos < n && tok_is(&t[*pos], "NOT")) { (*pos)++; return !pp_unary(t, n, pos); }
    return pp_primary(t, n, pos);
}
static double pp_primary(const pp_tok_t* t, int n, int* pos)
{
    if (*pos >= n) return 0;
    if (tok_is(&t[*pos], "(")) {
        (*pos)++;
        double v = pp_expr(t, n, pos);
        if (*pos < n && tok_is(&t[*pos], ")")) (*pos)++;
        return v;
    }
    double v = tok_val(&t[*pos]);
    (*pos)++;
    return v;
}

static double pp_eval_expr(const char* expr)
{
    pp_tok_t toks[128];
    int n = pp_lex(expr, toks, 128);
    int pos = 0;
    return pp_expr(toks, n, &pos);
}

/* ── API principale ── */
/* Defines ESTERNI (CLI -D/--define): restano fuori da `g_pp` e vengono riapplicati a ogni
 * reset, così non si perdono quando si preprocessa un nuovo sorgente. */
static gmb_pp_sym_t g_base[GMB_PP_MAX_SYMS];
static int g_nbase;

void gmb_preprocess_define_base(const char* name, const char* value)
{
    if (!name || !*name) return;
    for (int i = 0; i < g_nbase; i++)
        if (strcasecmp(g_base[i].name, name) == 0) {
            free(g_base[i].value);
            g_base[i].value = strdup(value && *value ? value : "1");
            pp_def(g_base[i].name, g_base[i].value);
            return;
        }
    if (g_nbase < GMB_PP_MAX_SYMS) {
        g_base[g_nbase].name = strdup(name);
        g_base[g_nbase].value = strdup(value && *value ? value : "1");
        pp_def(g_base[g_nbase].name, g_base[g_nbase].value);
        g_nbase++;
    }
}

void gmb_preprocess_clear_base(void)
{
    for (int i = 0; i < g_nbase; i++) { free(g_base[i].name); free(g_base[i].value); }
    g_nbase = 0;
}

void gmb_preprocess_reset(void)
{
    for (int i = 0; i < g_pp.nsyms; i++) { free(g_pp.syms[i].name); free(g_pp.syms[i].value); }
    g_pp.nsyms = 0;
    /* riapplica i -D della CLI: un #DEF del file precedente non resta, i -D sì. */
    for (int i = 0; i < g_nbase; i++) pp_def(g_base[i].name, g_base[i].value);
}

void gmb_preprocess_define(const char* name, const char* value)
{
    if (!name || !*name) return;
    pp_def(name, value && *value ? value : "1");
}

int gmb_preprocess_line(const char* line, char* out, int cap)
{
    /* direttive: restituiscono 1 se la riga va scartata, 0 se va tenuta */
    const char* t = line;
    while (*t == ' ' || *t == '\t') t++;
    if (*t != '#') { snprintf(out, cap, "%s", line); return 0; }

    char dir[32] = "";
    sscanf(t + 1, "%31s", dir);
    char rest[1024] = "";
    /* estrai il resto dopo la direttiva */
    const char* r = t + 1;
    while (*r && !isspace((unsigned char)*r)) r++;
    while (*r == ' ' || *r == '\t') r++;
    snprintf(rest, sizeof(rest), "%s", r);

    if (pp_word(dir) == PPW_DEF) {
        char name[128] = "";
        sscanf(rest, "%127s", name);
        const char* v = rest + strlen(name);
        while (*v == ' ' || *v == '\t') v++;
        pp_def(name, *v ? v : "1");
        return 1; /* direttiva: non va in output */
    }
    if (pp_word(dir) == PPW_UNDEF) {
        char name[128] = "";
        sscanf(rest, "%127s", name);
        pp_undef(name);
        return 1;
    }
    if (pp_word(dir) == PPW_IFDEF) {
        char name[128] = "";
        sscanf(rest, "%127s", name);
        return pp_sym_defined(name) ? 1 : 1; /* la riga #IFDEF va sempre scartata */
    }
    if (pp_word(dir) == PPW_IFUNDEF) { return 1; }
    if (pp_word(dir) == PPW_IF) { return 1; }
    if (pp_word(dir) == PPW_ELSEIF) { return 1; }
    if (pp_word(dir) == PPW_ELSE) { return 1; }
    if (pp_word(dir) == PPW_ENDIF) { return 1; }
    /* riga # non riconosciuta: la teniamo */
    snprintf(out, cap, "%s", line);
    return 0;
}

/* ═══ STATO A SESSIONE: stack di inclusione #IF/#IFDEF/#ELSEIF/#ELSE/#ENDIF ═══ */
/* Stato di un livello #IF/#IFDEF: `outer` = il livello superiore includeva, `active` = il
 * RAMO CORRENTE è incluso, `taken` = almeno un ramo precedente è stato preso.
 * Prima esisteva solo `any_branch` (usato sia come "preso" sia come "attivo"): con
 * `#IFDEF VERO … #ELSE` il ramo ELSE restava incluso → ENTRAMBE le diramazioni nel codice
 * (bug reale, scoperto confrontando C e C# su -D). */
typedef struct gmb_pp_session {
    bool outer[GMB_PP_MAX_DEPTH];
    bool active[GMB_PP_MAX_DEPTH];
    bool taken[GMB_PP_MAX_DEPTH];
    int depth;
} gmb_pp_session_t;

static bool pp_including(const gmb_pp_session_t* s)
{
    if (s->depth == 0) return true;
    return s->active[s->depth - 1];
}

void gmb_preprocess_init(gmb_pp_session_t* s) { s->depth = 0; }

/* Processa una riga sorgente. out = riga da tokenizzare (se keep). Ritorna true se va
 * tenuta (non è una direttiva, o è dentro un blocco incluso). */
bool gmb_preprocess_step(gmb_pp_session_t* s, const char* line, char* out, int cap)
{
    const char* t = line;
    while (*t == ' ' || *t == '\t') t++;
    if (*t != '#') {
        snprintf(out, cap, "%s", line);
        return pp_including(s);
    }
    char dir[32] = "";
    sscanf(t + 1, "%31s", dir);
    const char* r = t + 1;
    while (*r && !isspace((unsigned char)*r)) r++;
    while (*r == ' ' || *r == '\t') r++;
    char rest[1024] = "";
    snprintf(rest, sizeof(rest), "%s", r);

    if (pp_word(dir) == PPW_DEF) {
        char name[128] = "";
        sscanf(rest, "%127s", name);
        /* macro-style: #DEF NOME(...) — la gestisce il MacroExpander, la passiamo */
        /* macro-style: #DEF NOME(...) — la gestisce il MacroExpander, la passiamo */
        if (strchr(name, '(') || (rest + strlen(name) && *(rest + strlen(name)) == '(')) {
            snprintf(out, cap, "%s", line);
            return true;
        }
        const char* after = rest + strlen(name);
        while (*after == ' ' || *after == '\t') after++;
        const char* v = after;
        while (*v == ' ' || *v == '\t') v++;
        pp_def(name, *v ? v : "1");
        /* passa la #DEF anche al MacroExpander (sostituzione nome->valore nel codice) */
        snprintf(out, cap, "%s", line);
        return true;
    }
    if (pp_word(dir) == PPW_UNDEF) {
        char name[128] = "";
        sscanf(rest, "%127s", name);
        pp_undef(name);
        return false;
    }
    if (pp_word(dir) == PPW_IFDEF) {
        char name[128] = "";
        sscanf(rest, "%127s", name);
        bool cond = pp_sym_defined(name);
        if (s->depth < GMB_PP_MAX_DEPTH) {
            s->outer[s->depth] = pp_including(s);
            s->active[s->depth] = s->outer[s->depth] && cond;
            s->taken[s->depth] = cond;
            s->depth++;
        }
        return false;
    }
    if (pp_word(dir) == PPW_IFUNDEF) {
        char name[128] = "";
        sscanf(rest, "%127s", name);
        bool cond = !pp_sym_defined(name);
        if (s->depth < GMB_PP_MAX_DEPTH) {
            s->outer[s->depth] = pp_including(s);
            s->active[s->depth] = s->outer[s->depth] && cond;
            s->taken[s->depth] = cond;
            s->depth++;
        }
        return false;
    }
    if (pp_word(dir) == PPW_IF) {
        bool cond = pp_eval_expr(rest) != 0;
        if (s->depth < GMB_PP_MAX_DEPTH) {
            s->outer[s->depth] = pp_including(s);
            s->active[s->depth] = s->outer[s->depth] && cond;
            s->taken[s->depth] = cond;
            s->depth++;
        }
        return false;
    }
    if (pp_word(dir) == PPW_ELSEIF) {
        if (s->depth > 0) {
            int d = s->depth - 1;
            bool cond = pp_eval_expr(rest) != 0;
            s->active[d] = s->outer[d] && !s->taken[d] && cond;
            if (cond) s->taken[d] = true;
        }
        return false;
    }
    if (pp_word(dir) == PPW_ELSE) {
        if (s->depth > 0) {
            int d = s->depth - 1;
            s->active[d] = s->outer[d] && !s->taken[d];
            s->taken[d] = true;
        }
        return false;
    }
    if (pp_word(dir) == PPW_ENDIF) {
        if (s->depth > 0) s->depth--;
        return false;
    }
    /* direttiva sconosciuta: la teniamo (commento?) */
    snprintf(out, cap, "%s", line);
    return true;
}

/* Preprocessa l'intero sorgente: applica le direttive per riga e restituisce il
 * buffer filtrato (allocato). Ritorna NULL in caso di errore di allocazione. */
char* gmb_preprocess_source(const char* src, long len)
{
    gmb_preprocess_reset();
    gmb_pp_session_t s;
    gmb_preprocess_init(&s);
    char* out = (char*)malloc((size_t)len * 3 + 1);   /* le #DEF pass-through possono espandere */
    if (!out) return NULL;
    long oi = 0;
    const char* p = src;
    while (p < src + len) {
        const char* nl = memchr(p, '\n', (size_t)(src + len - p));
        long linelen = nl ? (long)(nl - p) : (long)(src + len - p);
        char line[8192];
        long cl = linelen < 8191 ? linelen : 8191;
        memcpy(line, p, (size_t)cl); line[cl] = 0;
        char kept[8192];
        if (gmb_preprocess_step(&s, line, kept, sizeof(kept))) {
            long kl = (long)strlen(kept);
            if (oi + kl + 2 < len * 3 + 1) { memcpy(out + oi, kept, (size_t)kl); oi += kl; out[oi++] = '\n'; }
        }
        p += linelen;
        if (nl) p++; /* salta il \n */
    }
    out[oi] = 0;
    return out;
}
