/* ============================================================================
 * gmb_ide.c — implementazione del modello IDE (gmb_ide.h). Tutto in C.
 * ============================================================================ */
#include "gmb_ide.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static const char* stristr(const char* hay, const char* needle)
{
    if (!hay || !needle) return NULL;
    size_t nl = strlen(needle);
    if (nl == 0) return hay;
    for (; *hay; hay++) if (!strncasecmp(hay, needle, nl)) return hay;
    return NULL;
}

/* ---- keyword del linguaggio (per completion/lint/rename-guard) ---- */
static const char* const kKeywords[] = {
    "DIM","REDIM","AS","SUB","END","FUNCTION","GENERATOR","RETURN","IF","THEN","ELSE","ELSEIF",
    "FOR","TO","STEP","NEXT","DO","LOOP","WHILE","WEND","UNTIL","EXIT","CONTINUE","GOTO","GOSUB",
    "ON","ERROR","RESUME","TRY","CATCH","FINALLY","PRINT","INPUT","LINEINPUT","OPEN","CLOSE","LET",
    "CONST","LOCAL","GLOBAL","STATIC","BYREF","BYVAL","OPTIONAL","TYPE","CLASS","INHERITS","IMPLEMENTS",
    "INTERFACE","NEW","DELETE","PUBLIC","PRIVATE","PROTECTED","VIRTUAL","OVERRIDE","IMPORT","REQUIRE",
    "NAMESPACE","DATA","READ","RESTORE","RANDOMIZE","STOP","SWAP","RUN","DECLARE","YIELD","AND","OR",
    "NOT","XOR","MOD","TRUE","FALSE","NULL","SELECT","CASE","REM","READ","WRITE","APPEND","SEEK"
};
#define GMB_IDE_NKW ((int)(sizeof(kKeywords)/sizeof(kKeywords[0])))

int gmb_ide_is_keyword(const char* word)
{
    if (!word) return 0;
    for (int i = 0; i < GMB_IDE_NKW; i++) if (!strcasecmp(word, kKeywords[i])) return 1;
    return 0;
}

static int is_ident_start(int c) { return isalpha(c) || c == '_'; }
static int is_ident_char(int c) { return isalnum(c) || c == '_'; }

/* Riconosce una parola di tipo routine all'inizio della riga (dopo il numero BASIC). */
static const char* routine_kw(const char* s, int* kind)
{
    while (*s == ' ' || *s == '\t') s++;
    /* salta numero BASIC iniziale */
    const char* p = s;
    while (isdigit((unsigned char)*p)) p++;
    if (p != s && (*p == ' ' || *p == '\t')) { s = p; while (*s == ' ' || *s == '\t') s++; }
    if (!strncasecmp(s, "SUB", 3) && (s[3] == ' ' || s[3] == '(')) { *kind = GMB_IDE_SYM_SUB; return s + 3; }
    if (!strncasecmp(s, "FUNCTION", 8) && (s[8] == ' ' || s[8] == '(')) { *kind = GMB_IDE_SYM_FUNCTION; return s + 8; }
    if (!strncasecmp(s, "GENERATOR", 9) && (s[9] == ' ' || s[9] == '(')) { *kind = GMB_IDE_SYM_GENERATOR; return s + 9; }
    if (!strncasecmp(s, "TYPE", 4) && (s[4] == ' ')) { *kind = GMB_IDE_SYM_TYPE; return s + 4; }
    if (!strncasecmp(s, "CONST", 5) && (s[5] == ' ')) { *kind = GMB_IDE_SYM_CONST; return s + 5; }
    return NULL;
}

static void copy_ident(const char* p, char* out, int cap)
{
    int n = 0;
    while (*p && is_ident_char((unsigned char)*p) && n < cap - 1) out[n++] = *p++;
    out[n] = 0;
}

void gmb_ide_init(gmb_ide_t* ide)
{
    if (!ide) return;
    memset(ide, 0, sizeof(*ide));
    ide->active = -1;
    ide->doc_count = 0;
    ide->analyzed_doc = -1;
    ide->analyzed_dirty = true;
}

gmb_ide_t* gmb_ide_static(void)
{
    static gmb_ide_t* s = NULL;
    if (!s) { s = (gmb_ide_t*)calloc(1, sizeof(gmb_ide_t)); gmb_ide_init(s); }
    return s;
}

int gmb_ide_active(const gmb_ide_t* ide) { return ide ? ide->active : -1; }
int gmb_ide_tab_count(const gmb_ide_t* ide) { return ide ? ide->doc_count : 0; }
const char* gmb_ide_tab_name(const gmb_ide_t* ide, int idx)
{
    if (!ide || idx < 0 || idx >= ide->doc_count) return "";
    return ide->docs[idx].name;
}

int gmb_ide_tab_open(gmb_ide_t* ide, const char* name, const char* text)
{
    if (!ide) return -1;
    int idx = ide->doc_count < GMB_IDE_MAX_TABS ? ide->doc_count : ide->active;
    if (idx < 0) idx = 0;
    snprintf(ide->docs[idx].name, sizeof(ide->docs[idx].name), "%s", name ? name : "untitled.bas");
    ide->docs[idx].line_count = 0;
    ide->docs[idx].dirty = false;
    if (idx >= ide->doc_count) ide->doc_count = idx + 1;
    ide->active = idx;
    if (text) gmb_ide_set_text(ide, idx, text);
    else { ide->docs[idx].lines[0][0] = 0; ide->docs[idx].line_count = 1; }
    ide->analyzed_dirty = true;
    return idx;
}

int gmb_ide_tab_close(gmb_ide_t* ide, int idx)
{
    if (!ide || idx < 0 || idx >= ide->doc_count) return -1;
    for (int i = idx; i < ide->doc_count - 1; i++) ide->docs[i] = ide->docs[i + 1];
    ide->doc_count--;
    if (ide->active >= ide->doc_count) ide->active = ide->doc_count - 1;
    ide->analyzed_dirty = true;
    return ide->active;
}

int gmb_ide_tab_activate(gmb_ide_t* ide, int idx)
{
    if (!ide || idx < 0 || idx >= ide->doc_count) return -1;
    ide->active = idx;
    ide->analyzed_dirty = true;
    return idx;
}

/* aggiunge una riga al doc (strip del numero BASIC iniziale). */
static void doc_add_line(gmb_ide_doc_t* d, const char* raw)
{
    if (d->line_count >= GMB_IDE_MAX_LINES) return;
    const char* s = raw;
    const char* p = s;
    while (isdigit((unsigned char)*p)) p++;
    if (p != s && (*p == ' ' || *p == '\t')) { s = p + 1; }
    snprintf(d->lines[d->line_count], GMB_IDE_LINE_CAP, "%s", s);
    d->line_count++;
}

void gmb_ide_set_text(gmb_ide_t* ide, int idx, const char* text)
{
    if (!ide || idx < 0 || idx >= GMB_IDE_MAX_TABS) return;
    gmb_ide_doc_t* d = &ide->docs[idx];
    d->line_count = 0;
    if (text)
    {
        const char* p = text;
        while (*p && d->line_count < GMB_IDE_MAX_LINES)
        {
            const char* e = p; while (*e && *e != '\n' && *e != '\r') e++;
            char buf[GMB_IDE_LINE_CAP];
            int n = (int)(e - p); if (n >= GMB_IDE_LINE_CAP) n = GMB_IDE_LINE_CAP - 1;
            memcpy(buf, p, (size_t)n); buf[n] = 0;
            doc_add_line(d, buf);
            while (*e == '\r' || *e == '\n') e++;
            p = e;
        }
    }
    if (d->line_count == 0) { d->lines[0][0] = 0; d->line_count = 1; }
    if (idx == ide->active) ide->analyzed_dirty = true;
}

int gmb_ide_doc_text(const gmb_ide_t* ide, int idx, char* out, int cap)
{
    if (!ide || idx < 0 || idx >= ide->doc_count || !out || cap <= 0) return 0;
    out[0] = 0; int w = 0;
    for (int i = 0; i < ide->docs[idx].line_count; i++)
    {
        const char* l = ide->docs[idx].lines[i];
        int ln = (int)strlen(l);
        if (w + ln + 2 < cap) { memcpy(out + w, l, (size_t)ln); w += ln; out[w++] = '\n'; }
    }
    out[w] = 0;
    return w;
}

int gmb_ide_doc_lines(const gmb_ide_t* ide, int idx)
{
    if (!ide || idx < 0 || idx >= ide->doc_count) return 0;
    return ide->docs[idx].line_count;
}

const char* gmb_ide_doc_line(const gmb_ide_t* ide, int idx, int line)
{
    if (!ide || idx < 0 || idx >= ide->doc_count) return "";
    if (line < 0 || line >= ide->docs[idx].line_count) return "";
    return ide->docs[idx].lines[line];
}

void gmb_ide_doc_set_line(gmb_ide_t* ide, int idx, int line, const char* text)
{
    if (!ide || idx < 0 || idx >= ide->doc_count || line < 0) return;
    if (line >= ide->docs[idx].line_count) return;
    snprintf(ide->docs[idx].lines[line], GMB_IDE_LINE_CAP, "%s", text ? text : "");
    ide->docs[idx].dirty = true;
    ide->analyzed_dirty = true;
}

void gmb_ide_doc_insert_line(gmb_ide_t* ide, int idx, int at, const char* text)
{
    if (!ide || idx < 0 || idx >= ide->doc_count) return;
    gmb_ide_doc_t* d = &ide->docs[idx];
    if (d->line_count >= GMB_IDE_MAX_LINES) return;
    if (at < 0) at = 0;
    if (at > d->line_count) at = d->line_count;
    for (int i = d->line_count; i > at; i--) memcpy(d->lines[i], d->lines[i - 1], GMB_IDE_LINE_CAP);
    snprintf(d->lines[at], GMB_IDE_LINE_CAP, "%s", text ? text : "");
    d->line_count++;
    d->dirty = true;
    ide->analyzed_dirty = true;
}

void gmb_ide_doc_delete_line(gmb_ide_t* ide, int idx, int at)
{
    if (!ide || idx < 0 || idx >= ide->doc_count) return;
    gmb_ide_doc_t* d = &ide->docs[idx];
    if (at < 0 || at >= d->line_count) return;
    for (int i = at; i < d->line_count - 1; i++) memcpy(d->lines[i], d->lines[i + 1], GMB_IDE_LINE_CAP);
    d->line_count--;
    if (d->line_count == 0) { d->lines[0][0] = 0; d->line_count = 1; }
    d->dirty = true;
    ide->analyzed_dirty = true;
}

static int line_has_ident(const char* line, const char* name)
{
    int nl = (int)strlen(name);
    if (nl == 0) return 0;
    const char* p = line;
    while ((p = strstr(p, name)) != NULL)
    {
        int pre = (p == line) ? 0 : is_ident_char((unsigned char)p[-1]);
        int post = is_ident_char((unsigned char)p[nl]);
        if (!pre && !post) return 1;
        p += nl;
    }
    return 0;
}

int gmb_ide_doc_find(const gmb_ide_t* ide, int idx, const char* term, int from_line)
{
    if (!ide || idx < 0 || idx >= ide->doc_count || !term) return -1;
    if (from_line < 0) from_line = 0;
    for (int i = from_line; i < ide->docs[idx].line_count; i++)
        if (strstr(ide->docs[idx].lines[i], term)) return i;
    return -1;
}

/* ---- analisi ---- */
static int sym_cmp(const void* a, const void* b)
{
    const gmb_ide_sym_t* x = a; const gmb_ide_sym_t* y = b;
    if (x->line != y->line) return x->line - y->line;
    return x->kind - y->kind;
}

void gmb_ide_analyze(gmb_ide_t* ide)
{
    if (!ide) return;
    ide->sym_count = 0;
    ide->analyzed_doc = ide->active;
    ide->analyzed_dirty = false;
    if (ide->active < 0 || ide->active >= ide->doc_count) return;
    gmb_ide_doc_t* d = &ide->docs[ide->active];
    for (int i = 0; i < d->line_count && ide->sym_count < GMB_IDE_MAX_SYMS; i++)
    {
        int kind = 0;
        const char* after = routine_kw(d->lines[i], &kind);
        if (after)
        {
            char nm[GMB_IDE_CHAR_CAP]; copy_ident(after + 1, nm, sizeof(nm));
            if (nm[0])
            {
                gmb_ide_sym_t* s = &ide->syms[ide->sym_count++];
                snprintf(s->name, sizeof(s->name), "%s", nm);
                s->kind = kind; s->line = i;
            }
        }
    }
    qsort(ide->syms, (size_t)ide->sym_count, sizeof(gmb_ide_sym_t), sym_cmp);
}

static void ensure_analyzed(gmb_ide_t* ide)
{
    if (ide && (ide->analyzed_dirty || ide->analyzed_doc != ide->active)) gmb_ide_analyze(ide);
}

int gmb_ide_sym_count(gmb_ide_t* ide) { ensure_analyzed(ide); return ide ? ide->sym_count : 0; }
const char* gmb_ide_sym_name(gmb_ide_t* ide, int i) { ensure_analyzed(ide); return (ide && i >= 0 && i < ide->sym_count) ? ide->syms[i].name : ""; }
int gmb_ide_sym_kind(gmb_ide_t* ide, int i) { ensure_analyzed(ide); return (ide && i >= 0 && i < ide->sym_count) ? ide->syms[i].kind : 0; }
int gmb_ide_sym_line(gmb_ide_t* ide, int i) { ensure_analyzed(ide); return (ide && i >= 0 && i < ide->sym_count) ? ide->syms[i].line : -1; }

static const char* kind_name(int k)
{
    switch (k) { case GMB_IDE_SYM_SUB: return "SUB"; case GMB_IDE_SYM_FUNCTION: return "FUNCTION";
                 case GMB_IDE_SYM_GENERATOR: return "GENERATOR"; case GMB_IDE_SYM_TYPE: return "TYPE";
                 case GMB_IDE_SYM_CONST: return "CONST"; default: return "VAR"; }
}

int gmb_ide_outline(gmb_ide_t* ide, char* out, int cap)
{
    ensure_analyzed(ide);
    if (!out || cap <= 0) return 0;
    out[0] = 0; int w = 0;
    for (int i = 0; i < ide->sym_count; i++)
    {
        char tmp[GMB_IDE_CHAR_CAP + 32];
        snprintf(tmp, sizeof(tmp), "%d|%s|%s;", ide->syms[i].line, kind_name(ide->syms[i].kind), ide->syms[i].name);
        int n = (int)strlen(tmp);
        if (w + n + 1 < cap) { memcpy(out + w, tmp, (size_t)n); w += n; }
    }
    out[w] = 0;
    return w;
}

int gmb_ide_complete(gmb_ide_t* ide, const char* prefix, char* out, int cap)
{
    ensure_analyzed(ide);
    if (!out || cap <= 0) return 0;
    out[0] = 0; int w = 0; int pl = prefix ? (int)strlen(prefix) : 0;
    for (int i = 0; i < GMB_IDE_NKW; i++)
    {
        if (pl && strncasecmp(kKeywords[i], prefix, (size_t)pl)) continue;
        int n = (int)strlen(kKeywords[i]);
        if (w + n + 2 < cap) { if (w) out[w++] = ';'; memcpy(out + w, kKeywords[i], (size_t)n); w += n; }
    }
    if (ide)
        for (int i = 0; i < ide->sym_count; i++)
        {
            if (pl && strncasecmp(ide->syms[i].name, prefix, (size_t)pl)) continue;
            int n = (int)strlen(ide->syms[i].name);
            if (w + n + 2 < cap) { if (w) out[w++] = ';'; memcpy(out + w, ide->syms[i].name, (size_t)n); w += n; }
        }
    out[w] = 0;
    return w;
}

int gmb_ide_goto_def(gmb_ide_t* ide, const char* name)
{
    ensure_analyzed(ide);
    if (!ide || !name) return -1;
    for (int i = 0; i < ide->sym_count; i++)
        if (!strcasecmp(ide->syms[i].name, name)) return ide->syms[i].line;
    return -1;
}

int gmb_ide_find_refs(gmb_ide_t* ide, const char* name, char* out, int cap)
{
    if (!ide || ide->active < 0 || ide->active >= ide->doc_count || !name || !out || cap <= 0) return 0;
    out[0] = 0; int w = 0; int count = 0;
    gmb_ide_doc_t* d = &ide->docs[ide->active];
    for (int i = 0; i < d->line_count; i++)
    {
        if (!line_has_ident(d->lines[i], name)) continue;
        char tmp[24]; snprintf(tmp, sizeof(tmp), "%d;", i);
        int n = (int)strlen(tmp);
        if (w + n + 1 < cap) { memcpy(out + w, tmp, (size_t)n); w += n; }
        count++;
    }
    out[w] = 0;
    return count;
}

int gmb_ide_lint(gmb_ide_t* ide, char* out, int cap)
{
    if (!ide || ide->active < 0 || ide->active >= ide->doc_count || !out || cap <= 0) return 0;
    out[0] = 0; int w = 0; int count = 0;
    gmb_ide_doc_t* d = &ide->docs[ide->active];
    int open_subs = 0, open_ifs = 0, open_fors = 0;
    char cur_routine[GMB_IDE_CHAR_CAP] = "";
    for (int i = 0; i < d->line_count; i++)
    {
        const char* s = d->lines[i]; while (*s == ' ' || *s == '\t') s++;
        /* squiggle: tab, trailing space, parentesi non bilanciate */
        if (strchr(d->lines[i], '\t'))
        { char tmp[128]; snprintf(tmp, sizeof(tmp), "%d|Tab non consentito (usa spazi)|%d;", i, i); int n=(int)strlen(tmp); if (w+n+1<cap){memcpy(out+w,tmp,(size_t)n);w+=n;} }
        { int op=0, cp=0; for (const char* q=d->lines[i]; *q; q++){ if(*q=='(')op++; else if(*q==')')cp++; }
          if (op!=cp) { char tmp[128]; snprintf(tmp,sizeof(tmp),"%d|Parentesi non bilanciate|%d;",i,i); int n=(int)strlen(tmp); if(w+n+1<cap){memcpy(out+w,tmp,(size_t)n);w+=n;} count++; } }
        if (!strncasecmp(s, "IF ", 3) && strstr(s, "THEN") == NULL) open_ifs++;
        if (!strncasecmp(s, "END IF", 6) && open_ifs > 0) open_ifs--;
        if (!strncasecmp(s, "FOR ", 4)) open_fors++;
        if (!strncasecmp(s, "NEXT", 4) && open_fors > 0) open_fors--;
        int kind = 0; const char* after = routine_kw(s, &kind);
        if (after) { open_subs++; copy_ident(after + 1, cur_routine, sizeof(cur_routine)); }
        else if (!strncasecmp(s, "END SUB", 7) || !strncasecmp(s, "END FUNCTION", 12) || !strncasecmp(s, "END GENERATOR", 13)) { if (open_subs > 0) open_subs--; }
    }
    if (open_subs > 0) { char tmp[160]; snprintf(tmp,sizeof(tmp),"%d|%d routine non chiusa/e (manca END SUB/FUNCTION)|%d;", d->line_count>0?d->line_count-1:0, open_subs, -2); int n=(int)strlen(tmp); if(w+n+1<cap){memcpy(out+w,tmp,(size_t)n);w+=n;} count++; }
    if (open_ifs > 0) { char tmp[128]; snprintf(tmp,sizeof(tmp),"%d|%d IF non chiuso/i (manca END IF)|%d;", 0, open_ifs, -4); int n=(int)strlen(tmp); if(w+n+1<cap){memcpy(out+w,tmp,(size_t)n);w+=n;} count++; }
    if (open_fors > 0) { char tmp[128]; snprintf(tmp,sizeof(tmp),"%d|%d FOR non chiuso/i (manca NEXT)|%d;", 0, open_fors, -3); int n=(int)strlen(tmp); if(w+n+1<cap){memcpy(out+w,tmp,(size_t)n);w+=n;} count++; }
    out[w] = 0;
    return count;
}

int gmb_ide_rename(gmb_ide_t* ide, const char* from, const char* to)
{
    if (!ide || ide->active < 0 || ide->active >= ide->doc_count) return 0;
    if (!from || !to || !*from || !*to) return 0;
    if (gmb_ide_is_keyword(from) || gmb_ide_is_keyword(to)) return -1;   /* guard: no keyword */
    gmb_ide_doc_t* d = &ide->docs[ide->active];
    int total = 0;
    for (int i = 0; i < d->line_count; i++)
    {
        char buf[GMB_IDE_LINE_CAP]; char* o = buf; const char* p = d->lines[i]; int fl = (int)strlen(from);
        buf[0] = 0;
        while (*p)
        {
            if (!strncmp(p, from, (size_t)fl) &&
                (p == d->lines[i] || !is_ident_char((unsigned char)p[-1])) &&
                !is_ident_char((unsigned char)p[fl]))
            {
                int tl = (int)strlen(to);
                if ((int)(o - buf) + tl < GMB_IDE_LINE_CAP - 1) { memcpy(o, to, (size_t)tl); o += tl; }
                p += fl; total++;
            }
            else if ((int)(o - buf) < GMB_IDE_LINE_CAP - 1) *o++ = *p++;
            else break;
        }
        *o = 0;
        if (strcmp(buf, d->lines[i]) != 0) snprintf(d->lines[i], GMB_IDE_LINE_CAP, "%s", buf);
    }
    if (total) { d->dirty = true; ide->analyzed_dirty = true; }
    return total;
}

int gmb_ide_snippet(const char* kind, char* out, int cap)
{
    if (!out || cap <= 0) return 0;
    const char* tpl = NULL;
    if (!kind) return 0;
    if (!strcasecmp(kind, "SUB")) tpl = "SUB NewSub()\n\nEND SUB\n";
    else if (!strcasecmp(kind, "FUNCTION")) tpl = "FUNCTION NewFunction() AS \n\nEND FUNCTION\n";
    else if (!strcasecmp(kind, "GENERATOR")) tpl = "GENERATOR NewGenerator()\n\nEND GENERATOR\n";
    else if (!strcasecmp(kind, "FOR")) tpl = "FOR i = 1 TO 10 STEP 1\n\nNEXT i\n";
    else if (!strcasecmp(kind, "IF")) tpl = "IF  THEN\n\nEND IF\n";
    else if (!strcasecmp(kind, "WHILE")) tpl = "WHILE \n\nWEND\n";
    else if (!strcasecmp(kind, "DO")) tpl = "DO\n\nLOOP UNTIL \n";
    else if (!strcasecmp(kind, "SELECT")) tpl = "SELECT CASE \n  CASE \n  CASE ELSE \nEND SELECT\n";
    else if (!strcasecmp(kind, "CLASS")) tpl = "CLASS NewClass\n\nEND CLASS\n";
    else if (!strcasecmp(kind, "TRY")) tpl = "TRY\n\nCATCH\n\nFINALLY\n\nEND TRY\n";
    if (!tpl) return 0;
    snprintf(out, cap, "%s", tpl);
    return (int)strlen(out);
}

/* ---- make ---- */
int gmb_ide_make_bas(gmb_ide_t* ide, const char* out_path)
{
    if (!ide || ide->active < 0 || ide->active >= ide->doc_count || !out_path) return -1;
    /* Scrive il sorgente normalizzato (il REPL/compilatore lo compila in bytecode A1). */
    FILE* f = fopen(out_path, "w");
    if (!f) return -2;
    for (int i = 0; i < ide->docs[ide->active].line_count; i++)
        fprintf(f, "%s\n", ide->docs[ide->active].lines[i]);
    fclose(f);
    return 0;
}

int gmb_ide_make_lib(gmb_ide_t* ide, const char* out_path)
{
    if (!ide || ide->active < 0 || ide->active >= ide->doc_count || !out_path) return -1;
    /* Una libreria NON deve avere SUB Main: verifica + scrive. */
    gmb_ide_doc_t* d = &ide->docs[ide->active];
    for (int i = 0; i < d->line_count; i++)
        if (stristr(d->lines[i], "SUB Main") && stristr(d->lines[i], "MAIN")) return -3;  /* rifiuta se ha Main */
    FILE* f = fopen(out_path, "w");
    if (!f) return -2;
    for (int i = 0; i < d->line_count; i++) fprintf(f, "%s\n", d->lines[i]);
    fclose(f);
    return 0;
}

/* ============================================================================
 * Wrapper FFI-friendly (firma banale per `DECLARE ... LIB "gmb_gfx"` da BASIC):
 * una singola istanza statica + un buffer statico condiviso per i ritorni stringa.
 * Il BASIC dichiara e chiama queste; la logica resta in gmb_ide_*.
 * ============================================================================ */
static char g_ide_strbuf[1 << 20];

const char* gmb_idef_outline(void) { gmb_ide_outline(gmb_ide_static(), g_ide_strbuf, sizeof(g_ide_strbuf)); return g_ide_strbuf; }
const char* gmb_idef_complete(const char* prefix) { gmb_ide_complete(gmb_ide_static(), prefix, g_ide_strbuf, sizeof(g_ide_strbuf)); return g_ide_strbuf; }
long long   gmb_idef_gotodef(const char* name) { return gmb_ide_goto_def(gmb_ide_static(), name); }
const char* gmb_idef_refs(const char* name) { gmb_ide_find_refs(gmb_ide_static(), name, g_ide_strbuf, sizeof(g_ide_strbuf)); return g_ide_strbuf; }
const char* gmb_idef_lint(void) { gmb_ide_lint(gmb_ide_static(), g_ide_strbuf, sizeof(g_ide_strbuf)); return g_ide_strbuf; }
int         gmb_idef_rename(const char* from, const char* to) { return gmb_ide_rename(gmb_ide_static(), from, to); }
const char* gmb_idef_snippet(const char* kind) { gmb_ide_snippet(kind, g_ide_strbuf, sizeof(g_ide_strbuf)); return g_ide_strbuf; }
int         gmb_idef_tabcount(void) { return gmb_ide_tab_count(gmb_ide_static()); }
const char* gmb_idef_tabname(int i) { return gmb_ide_tab_name(gmb_ide_static(), i); }
int         gmb_idef_tabopen(const char* name, const char* text) { return gmb_ide_tab_open(gmb_ide_static(), name, text); }
int         gmb_idef_tabclose(int idx) { return gmb_ide_tab_close(gmb_ide_static(), idx); }
int         gmb_idef_tabactivate(int idx) { return gmb_ide_tab_activate(gmb_ide_static(), idx); }
void        gmb_idef_settext(const char* text) { gmb_ide_set_text(gmb_ide_static(), gmb_ide_active(gmb_ide_static()), text); }
const char* gmb_idef_text(int idx) { gmb_ide_doc_text(gmb_ide_static(), idx, g_ide_strbuf, sizeof(g_ide_strbuf)); return g_ide_strbuf; }
int         gmb_idef_makebas(const char* path) { return gmb_ide_make_bas(gmb_ide_static(), path); }
int         gmb_idef_makelib(const char* path) { return gmb_ide_make_lib(gmb_ide_static(), path); }
