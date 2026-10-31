#include "gmb_lexer.h"
#include "gmb_macro_expand.h"
#include "gmb_parser.h"
#include "gmb_c99_emit.h"
#include "gmb_compiler.h"
#include "gmb_inc_resolve.h"
#include "gmb_symtab.h"
#include "gmb_types.h"
#include "gmb_rewrite.h"
#include "gmb_semgraph.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <direct.h>
#endif


/* Compila e linka il C emesso con `cc`.
 *
 * L'ESEGUIBILE E' IL DEFAULT: il driver produce il C come passaggio intermedio e poi
 * materializza il binario, senza che il chiamante debba chiedere `--exe`. Chi vuole il
 * solo C (gli harness che linkano poi col loro lib-set completo) passa `--no-exe`.
 *
 * I FLAG DI COMPILAZIONE NON SONO FISSI: cflags/ldflags/defines arrivano dalla CLI e
 * vengono ACCODATI DOPO i default, cosi' l'ultimo flag vince (in clang/gcc l'ultimo
 * -O / -std prevail). Il default canonico di questi flag vive nei SETTINGS del
 * compilatore (system.settings.json -> sezione "compiler", sovrascrivibile da
 * app.settings.json); i valori qui sotto sono solo il fallback per quando il driver
 * gira standalone, senza un host C# che gli passi i settings risolti.
 */
static int build_exe(const char* cc, const char* csrc, const char* bin,
                     const char** incs, int ninc, const char** objs, int nobj,
                     const char** libs, int nlib,
                     const char* cflags, const char* defines, const char* ldflags) {
    char cmd[16384]; size_t o = 0;
    /* Ottimizzazioni + codegen a sezioni: SEMPRE (il linker puo' scartare codice morto
     * anche a livello di singola funzione). I flag di link per lo scarto vengono aggiunti
     * sotto, per piattaforma, PRIMA degli ldflags dell'utente. */
    o += (size_t)snprintf(cmd + o, sizeof cmd - o,
                          "%s -std=c99 -O2 -ffunction-sections -fdata-sections -w", cc);
    /* defines PRIMA dei cflags: -D non dipende dall'ordine, ma tenere insieme i due
     * blocchi rende la command line leggibile nel log di errore. */
    if (defines && *defines) o += (size_t)snprintf(cmd + o, sizeof cmd - o, " %s", defines);
    if (cflags && *cflags)    o += (size_t)snprintf(cmd + o, sizeof cmd - o, " %s", cflags);
    for (int i = 0; i < ninc; i++) o += (size_t)snprintf(cmd + o, sizeof cmd - o, " -I%s", incs[i]);
    o += (size_t)snprintf(cmd + o, sizeof cmd - o, " \"%s\"", csrc);
    for (int i = 0; i < nobj; i++) o += (size_t)snprintf(cmd + o, sizeof cmd - o, " %s", objs[i]);
    for (int i = 0; i < nlib; i++) o += (size_t)snprintf(cmd + o, sizeof cmd - o, " %s", libs[i]);
    o += (size_t)snprintf(cmd + o, sizeof cmd - o, " -lm -lpthread");
    if (ldflags && *ldflags) o += (size_t)snprintf(cmd + o, sizeof cmd - o, " %s", ldflags);
    /* Scarto del codice morto: SEMPRE, con il flag del linker della piattaforma. */
#if defined(__APPLE__)
    o += (size_t)snprintf(cmd + o, sizeof cmd - o, " -Wl,-dead_strip");
#elif defined(__linux__)
    o += (size_t)snprintf(cmd + o, sizeof cmd - o, " -Wl,--gc-sections");
#endif
    o += (size_t)snprintf(cmd + o, sizeof cmd - o, " -o \"%s\"", bin);
    int rc = system(cmd);
    if (rc != 0) { fprintf(stderr, "build eseguibile FALLITA (rc=%d): %s\n", rc, cmd); return 5; }
    /* stdout e' RISERVATO ai dati: i messaggi di avanzamento vanno su stderr, altrimenti
     * inquinano l'output catturato dagli harness (falso DIFF di parita'). */
    fprintf(stderr, "eseguibile -> %s\n", bin);
    return 0;
}

/* Percorsi di default dell'output, derivati dal sorgente.
 *
 * DOVE: <cartella-del-sorgente>/bin/ — è la **cartella di output di default del compilatore**
 * (AGENTS.md §9.6): qui vanno il C generato e il binario. `work/` NON è la destinazione giusta:
 * `work/` è riservata agli **artefatti-dati** (file) prodotti da un programma BASIC per
 * verificare che abbia funzionato, e vive **solo** sotto `shared/samples/` (AGENTS.md §11.1
 * cat.4). Mettere sorgente/C generato/binari in `work/` è l'errore da non ripetere.
 *
 * Il chiamante che vuole altrove passa `out.c` esplicito e/o `--exe <bin>`.
 * Ritorna 0 se la cartella non e' creabile (il default non deve bloccare: chi chiama decide).
 */
static int default_out_path(const char* in, const char* ext, char* buf, size_t n)
{
    const char* slash = strrchr(in, '/');
    const char* base = slash ? slash + 1 : in;
    const char* dot = strrchr(base, '.');
    size_t blen = dot ? (size_t)(dot - base) : strlen(base);
    char dir[3072];
    if (slash) {
        size_t dlen = (size_t)(slash - in);
        if (dlen >= sizeof dir) return -1;
        memcpy(dir, in, dlen);
        dir[dlen] = 0;
    } else {
        snprintf(dir, sizeof dir, ".");
    }
    /* bin/ puo' non esistere: si crea, e' la destinazione standard degli artefatti compilati. */
    char wd[sizeof dir + 8];
    snprintf(wd, sizeof wd, "%s/bin", dir);
#ifdef _WIN32
    (void)_mkdir(wd);
#else
    (void)mkdir(wd, 0777);   /* 0777 & ~umask; se esiste gia' fallisce e va bene */
#endif
    snprintf(buf, n, "%s/gmb_%.*s%s", wd, (int)blen, base, ext);
    return 0;
}

/* Le liste sono globali perche' le funzioni di push sono helper: tenerle in main
 * costringerebbe a passare 6 parametri per una push. Nessun thread: il driver e' mono-thread. */
static const char* incs_arr[64];
static const char* objs_arr[128];
static const char* libs_arr[64];

/* Valore di un'opzione nelle DUE forme accettate: `--opt valore` e `--opt=valore`.
 * Le due forme servono: gli script passano `--cflags "-O3"`, ma chi digita a mano (e il
 * gate C99, che usa --cflags=) scrive `--cflags=-O3`. Accettarle evita il sintomo
 * "opzione sconosciuta: --cflags=-O3" su un flag perfettamente valido. */
static const char* opt_value(const char* arg, const char* name)
{
    size_t n = strlen(name);
    if (strncmp(arg, name, n) != 0) return NULL;
    if (arg[n] == '=') return arg + n + 1;
    if (arg[n] == 0) return "";       /* il valore è argv[i+1] */
    return NULL;
}

/* Come opt_value, ma per i flag booleani: matcha solo il nome esatto. */
static int opt_flag(const char* arg, const char* name)
{
    return strcmp(arg, name) == 0;
}

/* `-I`: forma attaccata (`-I<dir>`, la convenzione di cc) E forma separata (`-I <dir>`).
 * L'attaccata si concede solo alle opzioni corte: per le lunghe (`--cc`) renderebbe
 * indistinguibile un flag valido da un typo (`--ccx` diventerebbe --cc con valore "x"). */
static const char* opt_value_attached(const char* arg, const char* name)
{
    size_t n = strlen(name);
    if (strncmp(arg, name, n) != 0) return NULL;
    if (arg[n] == 0) return "";            /* il valore è argv[i+1] */
    if (arg[n] == '=') return arg + n + 1; /* --opt=valore */
    if (n == 2 && arg[0] == '-') return arg + n;   /* -I<dir> */
    return NULL;
}

/* Valore nell'argv successivo (forma `--opt valore`); NULL se non c'è (opzione malformata). */
static const char* next_value(int argc, char** argv, int* i)
{
    if (*i + 1 >= argc) return NULL;
    return argv[++(*i)];
}

/* Push con capienza: le liste sono accodate e silenziosamente IGNORATE oltre il cap
 * (il comando risultante sarebbe incoerente: meglio perdere una include che mentire). */
static void incs_push(int* n, const char* v) { if (*n < 64 && v && *v) incs_arr[(*n)++] = v; }
static void objs_push(int* n, const char* v) { if (*n < 128 && v && *v) objs_arr[(*n)++] = v; }
static void libs_push(int* n, const char* v) { if (*n < 64 && v && *v) libs_arr[(*n)++] = v; }

/* ── §6/§7: rewrite nel path + semantic graph/diff come tooling CLI ───────────────
 * Il grafo e la symbol table vivono in C (gmb_symtab/gmb_types/gmb_rewrite/gmb_semgraph):
 * nessuna dipendenza dal compilatore C#. `--semgraph` stampa il grafo JSON del programma;
 * `--semgraph-diff <base.bas>` il diff per id stabile rispetto al programma base. */

static const char* sem_kind_json(gmb_sym_kind_t k) {
    switch (k) {
        case GMB_SYM_NAMESPACE: return "namespace";
        case GMB_SYM_MODULE:    return "module";
        case GMB_SYM_PROCEDURE: return "procedure";
        case GMB_SYM_FUNCTION:  return "function";
        case GMB_SYM_METHOD:    return "method";
        case GMB_SYM_VARIABLE:  return "variable";
        case GMB_SYM_PARAMETER: return "parameter";
        case GMB_SYM_CONSTANT:  return "constant";
        case GMB_SYM_TYPE:      return "type";
        case GMB_SYM_CLASS:     return "class";
        case GMB_SYM_INTERFACE: return "interface";
        case GMB_SYM_ENUM:      return "enum";
        case GMB_SYM_FIELD:     return "field";
        case GMB_SYM_PROPERTY:  return "property";
        case GMB_SYM_OPERATOR:  return "operator";
        default:                return "unknown";
    }
}

static const char* edge_kind_json(gmb_edge_kind_t k) {
    switch (k) {
        case GMB_EDGE_DEFINES:   return "Defines";
        case GMB_EDGE_CALLS:     return "Calls";
        case GMB_EDGE_USES:      return "Uses";
        case GMB_EDGE_READS:     return "Reads";
        case GMB_EDGE_WRITES:    return "Writes";
        case GMB_EDGE_RETURNS:   return "Returns";
        case GMB_EDGE_INHERITS:  return "Inherits";
        case GMB_EDGE_IMPLEMENTS:return "Implements";
        case GMB_EDGE_IMPORTS:   return "Imports";
        case GMB_EDGE_CONTAINS:  return "Contains";
        default:                 return "DependsOn";
    }
}

static const char* symchg_json(gmb_symchg_kind_t k) {
    switch (k) {
        case GMB_SYMCHG_ADDED:      return "Added";
        case GMB_SYMCHG_REMOVED:    return "Removed";
        case GMB_SYMCHG_MOVED:      return "Moved";
        case GMB_SYMCHG_TYPE:       return "Type";
        case GMB_SYMCHG_SIGNATURE:  return "Signature";
        default:                    return "Visibility";
    }
}

/* JSON string escaping (solo i caratteri che rompono il JSON; il resto resta). */
static void json_escape(const char* s, char* out, size_t cap) {
    size_t o = 0;
    if (cap == 0) return;
    if (!s) s = "";
    for (const char* p = s; *p && o + 1 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { if (o + 2 >= cap) break; out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { if (o + 2 >= cap) break; out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { if (o + 2 >= cap) break; out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { if (o + 2 >= cap) break; out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20)  { if (o + 1 < cap) out[o++] = '?'; }
        else out[o++] = (char)c;
    }
    out[o] = 0;
}

static void print_semgraph_json(const gmb_semgraph_t* g) {
    printf("{\"syms\":[");
    for (size_t i = 0; i < g->nsyms; i++) {
        const gmb_symnode_t* s = &g->syms[i];
        char id[600], nm[300], ns[300], ty[140];
        json_escape(s->id, id, sizeof id);
        json_escape(s->name, nm, sizeof nm);
        json_escape(s->ns, ns, sizeof ns);
        json_escape(s->type_name, ty, sizeof ty);
        if (i) printf(",");
        printf("{\"id\":\"%s\",\"kind\":\"%s\",\"name\":\"%s\",\"ns\":\"%s\",\"line\":%d,\"col\":%d,\"type\":\"%s\"}",
               id, sem_kind_json(s->kind), nm, ns, s->line, s->col, ty);
    }
    printf("],\"refs\":[");
    for (size_t i = 0; i < g->nrefs; i++) {
        const gmb_refnode_t* r = &g->refs[i];
        char id[600], txt[300], sym[600], ty[140];
        json_escape(r->id, id, sizeof id);
        json_escape(r->text, txt, sizeof txt);
        json_escape(r->sym_id, sym, sizeof sym);
        json_escape(r->type_name, ty, sizeof ty);
        if (i) printf(",");
        printf("{\"id\":\"%s\",\"kind\":\"%s\",\"text\":\"%s\",\"line\":%d,\"col\":%d,\"sym\":\"%s\",\"type\":\"%s\"}",
               id, r->node_kind, txt, r->line, r->col, sym, ty);
    }
    printf("],\"edges\":[");
    for (size_t i = 0; i < g->nedges; i++) {
        const gmb_edge_t* e = &g->edges[i];
        char from[600], to[600];
        json_escape(e->from, from, sizeof from);
        json_escape(e->to, to, sizeof to);
        if (i) printf(",");
        printf("{\"from\":\"%s\",\"to\":\"%s\",\"kind\":\"%s\",\"line\":%d,\"col\":%d}",
               from, to, edge_kind_json(e->kind), e->line, e->col);
    }
    printf("]}\n");
}

static void print_diff_json(const gmb_semgraph_diff_t* d) {
    printf("{\"added\":%d,\"removed\":%d,\"changed\":%d,\"added_edges\":%zu,\"removed_edges\":%zu,\"changes\":[",
           d->n_added, d->n_removed, d->n_changed, d->n_added_edges, d->n_removed_edges);
    for (size_t i = 0; i < d->n_changes; i++) {
        char id[600];
        json_escape(d->changes[i].id, id, sizeof id);
        if (i) printf(",");
        printf("{\"id\":\"%s\",\"kind\":\"%s\"}", id, symchg_json(d->changes[i].kind));
    }
    printf("],\"impacted\":[");
    for (size_t i = 0; i < d->n_impacted; i++) {
        char id[600];
        json_escape(d->impacted[i], id, sizeof id);
        if (i) printf(",");
        printf("\"%s\"", id);
    }
    printf("]}\n");
}

/* Programma da un file SENZA include/REQUIRE (per il diff del tooling). */
static int load_plain_program(const char* path, gmb_program_t* out, char** err) {
    *err = NULL;
    FILE* f = fopen(path, "rb");
    if (!f) { *err = strdup("file non trovato"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* raw = (char*)malloc((size_t)n + 1);
    if (!raw) { fclose(f); *err = strdup("memoria"); return 1; }
    if (n > 0 && fread(raw, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(raw); *err = strdup("lettura"); return 1; }
    raw[n] = 0; fclose(f);
    gmb_lex_result_t lx = gmb_lexer_scan(raw, n);
    if (lx.error) { *err = strdup(lx.error); gmb_lex_result_free(&lx); free(raw); return 1; }
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    lx.owned_src = NULL;
    gmb_program_t p = gmb_parse(&lx2, err);
    if (*err) { gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(raw); return 1; }
    /* L'AST riferisce i token/stringhe posseduti dai risultati del lexer: NON liberarli
     * qui (use-after-free). Il processo termina subito dopo la stampa del diff; la
     * memoria resta valida fino ad allora (tooling one-shot, nessun ciclo lungo). */
    *out = p;
    return 0;
}

/* ── O8: mappa diagnostica C→BASIC ────────────────────────────────────────────────────────
 * Il driver conosce i segmenti del sorgente concatenato (gmb_resolve_includes_mapped) e li
 * passa all'emitter, che scrive `#line <riga bas> "<file bas>"` prima di ogni statement:
 * gli errori di `cc` citano già il `.bas`. Con `--line-map <file>` il driver scrive anche un
 * JSON con gli anchor, per chi compila il C per conto proprio (IDE, harness):
 *   {"entry":..., "segments":[{"line":1,"file":...}],
 *    "anchors":[{"c_line":N,"line":B,"file":...}]}
 * Un errore alla riga C `N+k` corrisponde a `<file>:B+k`. */
static void json_str(FILE* f, const char* s) {
    fputc('"', f);
    for (const unsigned char* p = (const unsigned char*)s; p && *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f); break;
        case '\r': fputs("\\r", f); break;
        case '\t': fputs("\\t", f); break;
        default:
            if (*p < 0x20) fprintf(f, "\\u%04x", *p);
            else fputc(*p, f);
        }
    }
    fputc('"', f);
}

static int count_nl(const char* s) {
    int n = 0;
    for (; s && *s; s++) if (*s == '\n') n++;
    return n;
}

/* Le direttive di dichiarazioni che non emettono codice (NAMESPACE, IMPORT, SUB) escono in
 * sequenza: l'ultima vince. Si tiene solo quella che precede davvero del C, per non
 * attribuire al BASIC sbagliato il codice generato che segue. In place: l'output è più corto. */
static void compact_line_directives(char* s) {
    char* r = s;
    char* w = s;
    while (*r) {
        char* eol = strchr(r, '\n');
        size_t len = eol ? (size_t)(eol - r + 1) : strlen(r);
        int is_dir = len > 6 && strncmp(r, "#line ", 6) == 0;
        if (is_dir) {
            /* prossima riga non vuota */
            char* n = eol ? eol + 1 : r + len;
            while (*n == '\n' || *n == '\r') n++;
            if (*n == 0 || strncmp(n, "#line ", 6) == 0) { r += len; continue; }
        }
        memmove(w, r, len);
        w += len;
        r += len;
    }
    *w = 0;
}

static int write_line_map(const char* path, const char* entry, const GMB_SRC_MAP* smap, const char* csrc) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "line-map: impossibile scrivere %s\n", path); return -1; }
    fprintf(f, "{\n  \"entry\": ");
    json_str(f, entry);
    fprintf(f, ",\n  \"segments\": [");
    for (int i = 0; i < smap->n; i++) {
        fprintf(f, "%s\n    {\"line\": %d, \"file\": ", i ? "," : "", smap->segs[i].line);
        json_str(f, smap->segs[i].file);
        fputc('}', f);
    }
    fprintf(f, "\n  ],\n  \"anchors\": [");
    int cline = 0, nanch = 0;
    for (const char* p = csrc; p && *p; ) {
        const char* eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        cline++;
        if (len > 6 && strncmp(p, "#line ", 6) == 0) {
            const char* q = p + 6;
            int n = 0;
            while (*q >= '0' && *q <= '9') { n = n * 10 + (*q - '0'); q++; }
            while (*q == ' ' || *q == '\t') q++;
            if (n > 0 && *q == '"') {
                char file[4096]; size_t fi = 0;
                for (const char* e = q + 1; *e && *e != '"'; e++) {
                    if (*e == '\\' && e[1]) e++;
                    if (fi + 1 < sizeof file) file[fi++] = *e;
                }
                file[fi] = 0;
                if (fi > 0) {
                    fprintf(f, "%s\n    {\"c_line\": %d, \"line\": %d, \"file\": ",
                            nanch++ ? "," : "", cline + 1, n);
                    json_str(f, file);
                    fputc('}', f);
                }
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    fprintf(f, "\n  ]\n}\n");
    fclose(f);
    fprintf(stderr, "line-map -> %s (%d anchor)\n", path, nanch);
    return 0;
}

static int pkg_cache_is_dir(const char* p)
{
    struct stat st;
    return p && *p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
            "uso: %s <file.bas> [out.c] [--exe <bin>] [--no-exe] [-I dir] [--obj f] [--lib f]\n"
            "          [--cc cc] [--cflags \"f\"] [--ldflags \"f\"] [--defines \"f\"]\n"
                        "[--line-map <file.json>] [--semgraph] [--semgraph-diff <base.bas>]\n"
            "  di default emette il C e poi COMPILA/LINKA l'eseguibile; --no-exe ferma al solo C;\n"
            "  --exe <bin> sceglie il nome. Ogni opzione accetta anche --opt=valore.\n", argv[0]);
        return 2;
    }
    const char* in = argv[1];
    int no_exe = 0;
    int want_semgraph = 0;
    const char* semgraph_base = NULL;
    const char* exe_opt = NULL;
    const char* line_map = NULL;   /* O8: --line-map <file.json> */
    /* out.c: se non indicato, derivato dal sorgente (default_out_path). Un argomento che
     * inizia con '-' e' un'opzione, non il path di uscita: altrimenti `driver a.bas --no-exe`
     * scriverebbe il C in un file chiamato "--no-exe". */
    const char* out_opt = (argc > 2 && argv[2][0] != '-') ? argv[2] : NULL;
    char out_buf[4096];
    const char* out = out_opt;
    if (!out) {
        if (default_out_path(in, ".c", out_buf, sizeof out_buf) != 0) {
            fprintf(stderr, "percorso di uscita troppo lungo per: %s\n", in); return 2;
        }
        out = out_buf;
    }
    int argi = out_opt ? 3 : 2;
    const char* cc = "cc";
    const char* cflags = NULL; const char* ldflags = NULL; const char* defines = NULL;
    int ninc = 0, nobj = 0, nlib = 0;
    for (int i = argi; i < argc; i++) {
        const char* a = argv[i];
        const char* v;
        /* `-I` e `--obj`/`--lib` possono ripetersi: si accodano, non si sovrascrivono. */
        /* v == "" significa "il valore è l'argv successivo" (forma `--opt valore`):
         * va PRESO DA lì, altrimenti il path diventava un'opzione sconosciuta. */
        if ((v = opt_value_attached(a, "-I")) != NULL) { incs_push(&ninc, v[0] ? v : next_value(argc, argv, &i)); continue; }
        if ((v = opt_value(a, "--obj")) != NULL) { objs_push(&nobj, v[0] ? v : next_value(argc, argv, &i)); continue; }
        if ((v = opt_value(a, "--lib")) != NULL) { libs_push(&nlib, v[0] ? v : next_value(argc, argv, &i)); continue; }
        if ((v = opt_value(a, "--exe")) != NULL) { exe_opt = v[0] ? v : next_value(argc, argv, &i); continue; }
        if ((v = opt_value(a, "--cc")) != NULL) {
            const char* c = v[0] ? v : next_value(argc, argv, &i);
            if (c) cc = c;                      /* --cc senza valore: si tiene "cc" */
            continue;
        }
        if ((v = opt_value(a, "--cflags")) != NULL) { cflags = v; continue; }
        if ((v = opt_value(a, "--ldflags")) != NULL) { ldflags = v; continue; }
        if ((v = opt_value(a, "--defines")) != NULL) { defines = v; continue; }
        if (opt_flag(a, "--semgraph")) { want_semgraph = 1; continue; }
        if ((v = opt_value(a, "--semgraph-diff")) != NULL) { semgraph_base = v[0] ? v : next_value(argc, argv, &i); continue; }
        if ((v = opt_value(a, "--line-map")) != NULL) { line_map = v[0] ? v : next_value(argc, argv, &i); continue; }
        if (opt_flag(a, "--no-exe")) { no_exe = 1; continue; }
        fprintf(stderr, "opzione sconosciuta: %s\n", a);
        fprintf(stderr, "uso: %s <file.bas> [out.c] [--exe <bin>] [--no-exe] [-I dir] "
                        "[--obj f] [--lib f] [--cc cc] [--cflags \"f\"] [--ldflags \"f\"] [--defines \"f\"]\n", argv[0]);
        return 2;
    }
    FILE* f = fopen(in, "rb");
    if (!f) { fprintf(stderr, "file non trovato: %s\n", in); return 2; }
    fseek(f, 0, SEEK_END); long alen = ftell(f); fseek(f, 0, SEEK_SET);
    char* raw = (char*)malloc((size_t)alen + 1);
    if (alen > 0 && fread(raw, 1, (size_t)alen, f) != (size_t)alen) { fclose(f); free(raw); return 2; }
    raw[alen] = 0; fclose(f);
    /* Cache pacchetti = cartella `packages/` del PROGETTO (GMBasic/packages): i pacchetti
     * stanno solo lì; ~/.gmbasic/packages è eliminato.
     * ORDINE: la cache va impostata (ed ESCLUSA dalla discovery degli IMPORT) PRIMA di
     * risolvere gli include — altrimenti, con la cache DENTRO la cartella dei sorgenti
     * (layout `vendor/`), l'`.inc` del pacchetto viene preposto due volte (IMPORT + REQUIRE)
     * e l'emitter fallisce con "la procedura 'X' ... è già definita". */
    {
        static char pkgcache[2048];
        if (gmb_resolve_install_root(pkgcache, sizeof pkgcache) && pkgcache[0]) {
            size_t pl = strlen(pkgcache);
            snprintf(pkgcache + pl, sizeof pkgcache - pl, "/packages");
            /* UNA sola cartella per repo: se il progetto non ha packages/ (monorepo: la
             * cartella e alla RADICE del monorepo, non dentro GMBasic/), si usa il parent. */
            if (!pkg_cache_is_dir(pkgcache)) {
                snprintf(pkgcache + pl, sizeof pkgcache - pl, "/../packages");
            }
            gmb_pkg_set_cache_dir(pkgcache);
        } else {
            /* CLIENT (nessun checkout sorgente): cache UTENTE ~/.gmbasic/packages, la stessa che
             * usa il PackageManager (`gmb pkg install`) e il loader FFI del runtime. */
            const char* home = getenv("HOME");
            if (home && *home) {
                snprintf(pkgcache, sizeof pkgcache, "%s/.gmbasic/packages", home);
                gmb_pkg_set_cache_dir(pkgcache);
            }
        }
        if (pkgcache[0]) {
            /* La cache EFFETTIVA è GMB_PKG_CACHE se impostata (override documentato):
             * è quella da escludere dalla discovery degli IMPORT. */
            const char* envc = getenv("GMB_PKG_CACHE");
            gmb_inc_exclude_root((envc && *envc) ? envc : pkgcache);
            /* Anche il VENDOR di progetto (path fisso `<dir del .bas>/vendor`) è fuori dalla
             * discovery degli IMPORT: i suoi .inc arrivano dal REQUIRE del compilatore. */
            {
                char vdir[4096];
                snprintf(vdir, sizeof(vdir), "%s/vendor", proj_dir_of(in));
                gmb_inc_exclude_root(vdir);
            }
        }
    }
    /* Include/IMPORT: DOPO aver escluso la cache dei pacchetti dalla discovery. */
    GMB_SRC_MAP smap; gmb_src_map_init(&smap);
    char* src = gmb_resolve_includes_mapped(raw, in, &smap);
    free(raw);
    /* REQUIRE: prepend degli export (.inc/.bas) dei pacchetti risolti (~/.gmbasic/packages). */
    {
        char* reqsrc = gmb_pkg_collect_requires(src, proj_dir_of(in));
        /* Un REQUIRE non risolto e' un ERRORE, non "nessun pacchetto": senza questo controllo
         * si compilerebbe in silenzio e il fallimento arriverebbe dopo, come "funzione non
         * definita" (il sintomo peggiore: un binario che gira con una libratzione diversa). */
        if (gmb_pkg_require_failed()) {
            fprintf(stderr, "errore: uno o piu' pacchetti REQUIRE non risolti (vedi sopra).\n");
            free(reqsrc); free(src); gmb_src_map_free(&smap);
            return 6;
        }
        if (reqsrc != NULL) {
            size_t rl = strlen(reqsrc), sl = strlen(src);
            char* comb = (char*)malloc(rl + sl + 2);
            if (comb) { memcpy(comb, reqsrc, rl); comb[rl] = '\n'; memcpy(comb + rl + 1, src, sl + 1); free(src); src = comb; }
            /* Il prefisso dei pacchetti sposta tutte le righe: gli anchor seguono. */
            gmb_src_map_shift(&smap, count_nl(reqsrc) + 1);
            free(reqsrc);
        }
    }
    alen = (long)strlen(src);
    gmb_lex_result_t lx = gmb_lexer_scan(src, alen);
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    lx.owned_src = NULL;
    char* perr = NULL;
    gmb_program_t prog = gmb_parse(&lx2, &perr);
    if (perr) { fprintf(stderr, "parse error: %s\n", perr); free(perr); goto fail; }
    /* §6: gmb_rewrite nel path dell'emitter. Symbol table + type registry in C
     * (nessuna dipendenza dal C#), poi enrichment di resolved_sym/resolved_type. */
    gmb_symtab_t st; gmb_symtab_init(&st); gmb_symtab_build(&st, prog.stmts, prog.n);
    gmb_typereg_t tr; gmb_typereg_init(&tr); gmb_typereg_build(&tr, prog.stmts, prog.n);
    (void)gmb_rewrite_program(prog.stmts, prog.n, &st, &tr);
    /* §7: il grafo semantico (e il diff) è esposto dal tooling CLI, in C. */
    if (want_semgraph || semgraph_base) {
        gmb_semgraph_t g; memset(&g, 0, sizeof g);
        gmb_semgraph_extract(&g, prog.stmts, prog.n, in);
        if (semgraph_base) {
            gmb_program_t bp; char* be = NULL;
            if (load_plain_program(semgraph_base, &bp, &be) != 0) {
                fprintf(stderr, "semgraph-diff: %s\n", be ? be : "errore");
                free(be); gmb_semgraph_free(&g);
                gmb_typereg_free(&tr); gmb_symtab_free(&st);
                gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
                return 3;
            }
            gmb_symtab_t bst; gmb_symtab_init(&bst); gmb_symtab_build(&bst, bp.stmts, bp.n);
            gmb_typereg_t btr; gmb_typereg_init(&btr); gmb_typereg_build(&btr, bp.stmts, bp.n);
            (void)gmb_rewrite_program(bp.stmts, bp.n, &bst, &btr);
            gmb_semgraph_t bg; memset(&bg, 0, sizeof bg);
            gmb_semgraph_extract(&bg, bp.stmts, bp.n, semgraph_base);
            gmb_semgraph_diff_t d; memset(&d, 0, sizeof d);
            gmb_semgraph_diff(&bg, &g, &d);
            print_diff_json(&d);
            gmb_semgraph_diff_free(&d);
            gmb_semgraph_free(&bg);
            gmb_typereg_free(&btr); gmb_symtab_free(&bst);
            gmb_program_free(&bp);
        } else {
            print_semgraph_json(&g);
        }
        gmb_semgraph_free(&g);
        gmb_typereg_free(&tr); gmb_symtab_free(&st);
        gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
        return 0;
    }
    char* csrc = NULL; char* eerr = NULL;
    gmb_c99_set_source_file(in);   /* §3.6: LLM.FILEOUTLINE usa il nome file sorgente */
    /* O8: gli anchor dei segmenti fanno emettere `#line` verso il `.bas` di ogni statement. */
    {
        static gmb_c99_anchor_t anchors[256];
        int na = smap.n < (int)(sizeof anchors / sizeof anchors[0])
                     ? smap.n : (int)(sizeof anchors / sizeof anchors[0]);
        for (int k = 0; k < na; k++) { anchors[k].line = smap.segs[k].line; anchors[k].file = smap.segs[k].file; }
        gmb_c99_set_source_anchors(anchors, na);
    }
    if (gmb_c99_emit(&prog, &csrc, &eerr) != 0) {
        fprintf(stderr, "emissione: %s\n", eerr ? eerr : "errore"); free(eerr);
        gmb_typereg_free(&tr); gmb_symtab_free(&st);
        gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
        gmb_src_map_free(&smap);
        return 4;
    }
    compact_line_directives(csrc);
    if (line_map) (void)write_line_map(line_map, in, &smap, csrc);
    gmb_src_map_free(&smap);   /* l'emissione è finita: gli anchor non servono più */
    FILE* of = fopen(out, "wb");
    if (!of) { fprintf(stderr, "impossibile scrivere: %s\n", out); free(csrc); gmb_typereg_free(&tr); gmb_symtab_free(&st); goto fail; }
    fwrite(csrc, 1, strlen(csrc), of); fclose(of);
    /* su stderr, NON su stdout: stdout resta pulito per l'harness (falso DIFF di parità) */
    fprintf(stderr, "emesso -> %s (%zu bytes)\n", out, strlen(csrc));
    if (!no_exe) {
        /* Binario di default: nome derivato dal sorgente, dir temporanea di sistema
         * (l'emitter emette SEMPRE un `int main`, quindi qui e' sempre un eseguibile:
         * la build di una libreria/.gmblib e' compito del CHIAMANTE, che conosce il
         * lib-set e il formato — non di questo driver). */
        char bin_buf[4096];
        const char* bin = exe_opt;
        if (!bin) {
            if (default_out_path(in, "", bin_buf, sizeof bin_buf) != 0) {
                fprintf(stderr, "percorso di uscita troppo lungo per: %s\n", in);
                free(csrc); gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
                return 2;
            }
            bin = bin_buf;
        }
        if (build_exe(cc, out, bin, incs_arr, ninc, objs_arr, nobj, libs_arr, nlib,
                      cflags, defines, ldflags) != 0) {
            free(csrc); gmb_typereg_free(&tr); gmb_symtab_free(&st); gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
            return 5;
        }
    }
    free(csrc);
    gmb_typereg_free(&tr); gmb_symtab_free(&st);
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
    return 0;
fail:
    gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
    return 3;
}
