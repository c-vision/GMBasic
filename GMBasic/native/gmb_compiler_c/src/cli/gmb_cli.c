/* gmb_cli.c — CLI di PROGETTO di GMBasic (O7).
 *
 *   gmb check --project <dir> [--entry <file.bas>] [--json] [--strict]
 *   gmb build --project <dir> [--entry <file.bas>] [--out <exe>] [--json]
 *   gmb run   --project <dir> [--entry <file.bas>] [--json]
 *   gmb minitests [--root <dir>] [--filter <substr>] [--all] [--json]
 *                 [--driver <exe>] [--cc <cc>]
 *
 * Con `LIBRARY "nome"` nel sorgente il prodotto di `build` non è un eseguibile ma
 * `<progetto>/bin/<nome>.dylib`: l'entry è il modulo con la direttiva (un progetto LIBRARY non
 * ha `SUB Main`), il link è `-shared -fPIC` e `run` costruisce senza eseguire.
 *
 * Perché esiste: il compilatore non è mono-file (`NAMESPACE` + `IMPORT`), ma senza un
 * ingresso di progetto l'IDE dovrebbe indovinare l'entry, l'insieme dei sorgenti e le
 * esclusioni. Qui l'insieme NON viene reimplementato: si chiede a `gmb_inc_resolve`
 * (`gmb_project_sources`, `gmb_resolve_module_files`), cioè la stessa discovery che usa
 * il compilatore. Il CLI aggiunge le tre cose che il driver non può sapere:
 *   1. entry esplicita o unica (`SUB Main`), altrimenti errore con i candidati;
 *   2. `work/`, `bin/`, `obj/`, `.git/`, … esclusi dalla discovery (nel resolver);
 *   3. diagnosi: `NAMESPACE` mancante nei moduli e `IMPORT` non risolto = ERRORE
 *      (oggi un nome senza IMPORT diventa una variabile implicita, senza segnale).
 *
 * Codici di uscita (stabili, per l'IDE): 0 ok · 2 compilazione/progetto · 3 parse ·
 * 4 emissione/cc · 5 link · 6 runtime (uscita del programma).
 * stdout = dati (con `--json`), stderr = progresso/diagnostica leggibile.
 *
 * `minitests` è il PUNTO UNICO di verifica della suite (sostituisce
 * tools/c99_compile_errors.sh, che ora è solo un alias): scopre i `.bas` in modo
 * ricorsivo e deterministico, emette il C col driver ed esegue il solo `cc
 * -fsyntax-only`. Nessun link, nessuna esecuzione: misura il codegen, non il runtime.
 * Uscita: 0 tutti PASS · 2 almeno un CC-FAIL/DRIVER-FAIL · 1 setup (driver/root/cc).
 * Il JSON è pensato per il diff fra due run (gate): stesso elenco ⇒ stesso set.
 */
#include "gmb_inc_resolve.h"
#include "gmb_cli_internal.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CLI_MAX_NS 128


/* ── diagnosi ──────────────────────────────────────────────────────────────────────────── */

static void diags_free(DiagList* l) {
    for (int i = 0; i < l->n; i++) {
        free(l->items[i].file);
        free(l->items[i].code);
        free(l->items[i].message);
    }
    free(l->items);
    l->items = NULL; l->n = l->cap = 0;
}

void diag_add(DiagList* l, int is_error, const char* code, const char* file,
                     int line, const char* message) {
    if (l->n == l->cap) {
        int cap = l->cap ? l->cap * 2 : 8;
        Diag* p = (Diag*)realloc(l->items, (size_t)cap * sizeof *p);
        if (!p) return;
        l->items = p; l->cap = cap;
    }
    Diag* d = &l->items[l->n++];
    d->file = strdup(file ? file : "");
    d->code = strdup(code ? code : "");
    d->message = strdup(message ? message : "");
    d->line = line;
    d->is_error = is_error;
}

static int diag_errors(const DiagList* l) {
    int n = 0;
    for (int i = 0; i < l->n; i++) if (l->items[i].is_error) n++;
    return n;
}

static int diag_warnings(const DiagList* l) {
    int n = 0;
    for (int i = 0; i < l->n; i++) if (!l->items[i].is_error) n++;
    return n;
}

/* ── stringhe / file ──────────────────────────────────────────────────────────────────── */

char* read_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char* b = (char*)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    if (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f);
    return b;
}

int is_file(const char* p) {
    struct stat st;
    return p && *p && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

int is_dir(const char* p) {
    struct stat st;
    return p && *p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

void path_dir(const char* path, char* out, size_t cap) {
    snprintf(out, cap, "%s", path ? path : "");
    char* s = strrchr(out, '/');
    if (s) *s = 0; else out[0] = 0;
}

int ends_with(const char* s, const char* suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcasecmp(s + ls - lf, suffix) == 0;
}

/* Crea tutte le cartelle del path (mkdir -p). 0 = ok. */
int mkdir_p(const char* path) {
    char buf[CLI_PATH];
    snprintf(buf, sizeof buf, "%s", path ? path : "");
    for (char* p = buf + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (!is_dir(buf) && mkdir(buf, 0755) != 0 && !is_dir(buf)) return -1;
        *p = '/';
    }
    if (buf[0] && !is_dir(buf) && mkdir(buf, 0755) != 0 && !is_dir(buf)) return -1;
    return 0;
}

/* Quoting POSIX per la command line di popen (i path del progetto possono avere spazi). */
void shell_quote(const char* in, char* out, size_t cap) {
    size_t w = 0;
    if (w + 1 < cap) out[w++] = '\'';
    for (const char* p = in; p && *p && w + 5 < cap; p++) {
        if (*p == '\'') { memcpy(out + w, "'\\''", 4); w += 4; }
        else out[w++] = *p;
    }
    if (w + 1 < cap) out[w++] = '\'';
    out[w] = 0;
}

/* Esegue un comando (shell) e restituisce l'exit code; `*out` riceve stdout+stderr. */
int run_cmd(const char* cmd, char** out) {
    if (out) *out = NULL;
    FILE* p = popen(cmd, "r");
    if (!p) return -1;
    size_t cap = 4096, len = 0;
    char* buf = (char*)malloc(cap);
    if (buf) buf[0] = 0;
    char tmp[1024];
    while (fgets(tmp, sizeof tmp, p)) {
        size_t l = strlen(tmp);
        if (buf && len + l + 1 > cap) {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); buf = NULL; }
            else buf = nb;
        }
        if (buf) { memcpy(buf + len, tmp, l + 1); len += l; }
    }
    int st = pclose(p);
    if (out) *out = buf;
    else free(buf);
    if (st == -1) return -1;
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) return 128 + WTERMSIG(st);
    return -1;
}

/* Il file ha un `SUB Main` (con o senza parentesi) fuori dai commenti? */
static int has_main_sub(const char* path) {
    char* s = read_all(path);
    if (!s) return 0;
    int found = 0;
    for (char* p = s; *p && !found; ) {
        char* eol = strchr(p, '\n');
        char* q = p;
        while (*q == ' ' || *q == '\t') q++;
        while (*q >= '0' && *q <= '9') q++;
        while (*q == ' ' || *q == '\t') q++;
        if (strncasecmp(q, "SUB ", 4) == 0) {
            const char* r = q + 4;
            while (*r == ' ' || *r == '\t') r++;
            if (strncasecmp(r, "MAIN", 4) == 0) {
                char c = r[4];
                if (c == 0 || c == '(' || isspace((unsigned char)c) || c == '\'') found = 1;
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    free(s);
    return found;
}

/* Riga (1-based) dell'`IMPORT <ns>` in un file: serve a diagnosticare con file:riga. */
int import_line(const char* path, const char* ns) {
    char* s = read_all(path);
    if (!s) return 0;
    int line = 0, hit = 0;
    for (char* p = s; *p && !hit; ) {
        char* eol = strchr(p, '\n');
        line++;
        char* q = p;
        while (*q == ' ' || *q == '\t') q++;
        while (*q >= '0' && *q <= '9') q++;
        while (*q == ' ' || *q == '\t') q++;
        if (strncasecmp(q, "IMPORT ", 7) == 0) {
            const char* r = q + 7;
            while (*r == ' ' || *r == '\t' || *r == ',') r++;
            size_t nl = strlen(ns);
            if (strncasecmp(r, ns, nl) == 0) {
                char c = r[nl];
                if (c == 0 || c == ',' || c == '(' || c == ' ' || c == '\t' || c == '\''
                    || c == '\r' || c == '\n' || c == '*') hit = line;
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    free(s);
    return hit;
}

static void json_str(FILE* f, const char* s) {
    fputc('"', f);
    for (const unsigned char* p = (const unsigned char*)(s ? s : ""); *p; p++) {
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

int nameset_has(const NameSet* s, const char* v) {
    for (int i = 0; i < s->n; i++) if (strcmp(s->items[i], v) == 0) return 1;
    return 0;
}

void nameset_add(NameSet* s, const char* v) {
    if (!v || !*v || nameset_has(s, v)) return;
    if (s->n == s->cap) {
        int cap = s->cap ? s->cap * 2 : 32;
        char** p = (char**)realloc(s->items, (size_t)cap * sizeof *p);
        if (!p) return;
        s->items = p; s->cap = cap;
    }
    s->items[s->n++] = strdup(v);
}

void nameset_free(NameSet* s) {
    for (int i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items);
    s->items = NULL; s->n = s->cap = 0;
}

/* Set di path in minuscolo: il baseline è confrontato case-insensitive. macOS/Windows non
 * distinguono, e il checkout può avere `MiniTests/` o `minitests/`: senza questo il baseline
 * registrato su una macchina risulterebbe composto da file "nuovi" su un'altra. */
static void nameset_add_lower(NameSet* s, const char* v) {
    if (!v || !*v) return;
    char buf[CLI_PATH];
    size_t i = 0;
    for (; v[i] && i + 1 < sizeof buf; i++) buf[i] = (char)tolower((unsigned char)v[i]);
    buf[i] = 0;
    nameset_add(s, buf);
}

static int nameset_has_lower(const NameSet* s, const char* v) {
    char buf[CLI_PATH];
    size_t i = 0;
    for (; v && v[i] && i + 1 < sizeof buf; i++) buf[i] = (char)tolower((unsigned char)v[i]);
    buf[i] = 0;
    return nameset_has(s, buf);
}

int path_under(const char* path, const char* root) {
    size_t l = strlen(root);
    return strncmp(path, root, l) == 0 && (path[l] == '/' || path[l] == 0);
}

/* ── opzioni ──────────────────────────────────────────────────────────────────────────── */


/* ── entry ────────────────────────────────────────────────────────────────────────────── */

/* Definita accanto a `build_project`: stessa sonda con cui il link decide se il prodotto è una
 * libreria condivisa. */

/* Sceglie l'entry: `--entry` esplicita, altrimenti l'unico `*.bas` con `SUB Main` nella
 * radice del progetto; se nessuno ha `SUB Main`, l'unico modulo con la direttiva `LIBRARY`
 * (un progetto LIBRARY non ha entry point per contratto). Né zero né più di uno: errore. */

static char* pick_entry(const char* project, const char* explicit_entry, DiagList* diags,
                        char*** out_candidates, int* out_ncand) {
    if (out_candidates) *out_candidates = NULL;
    if (out_ncand) *out_ncand = 0;

    if (explicit_entry && *explicit_entry) {
        char path[CLI_PATH];
        if (explicit_entry[0] == '/') snprintf(path, sizeof path, "%s", explicit_entry);
        else snprintf(path, sizeof path, "%s/%s", project, explicit_entry);
        if (!is_file(path)) {
            diag_add(diags, 1, "entry-missing", path, 0, "entry indicata ma non trovata");
            return NULL;
        }
        char* rp = realpath(path, NULL);
        char* res = strdup(rp ? rp : path);
        free(rp);
        return res;
    }

    DIR* d = opendir(project);
    if (!d) {
        diag_add(diags, 1, "project-missing", project, 0, "cartella del progetto non leggibile");
        return NULL;
    }
    char** cand = NULL; int nc = 0, cap = 0;
    /* Un progetto LIBRARY non ha `SUB Main` per contratto: l'entry è il modulo con la
     * direttiva `LIBRARY "..."`. Raccolta separata, usata solo se nessun `SUB Main` esiste
     * (un file che ha entrambi resta un programma: `main` vince). */
    char** libcand = NULL; int nlc = 0, libcap = 0;
    struct dirent* ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.' || !ends_with(ent->d_name, ".bas")) continue;
        char path[CLI_PATH];
        snprintf(path, sizeof path, "%s/%s", project, ent->d_name);
        if (!has_main_sub(path)) {
            char libname[96];
            if (!library_directive(path, libname, sizeof libname)) continue;
            if (nlc == libcap) {
                libcap = libcap ? libcap * 2 : 4;
                char** p = (char**)realloc(libcand, (size_t)libcap * sizeof *p);
                if (!p) break;
                libcand = p;
            }
            libcand[nlc++] = strdup(path);
            continue;
        }
        if (nc == cap) {
            cap = cap ? cap * 2 : 4;
            char** p = (char**)realloc(cand, (size_t)cap * sizeof *p);
            if (!p) break;
            cand = p;
        }
        cand[nc++] = strdup(path);
    }
    closedir(d);

    int lib_entry = 0;
    if (nc == 0) {
        for (int i = 0; i < nlc; i++) {
            if (nc == cap) {
                int ncap = cap ? cap * 2 : 4;
                char** p = (char**)realloc(cand, (size_t)ncap * sizeof *p);
                if (!p) break;
                cand = p; cap = ncap;
            }
            cand[nc++] = libcand[i];
            libcand[i] = NULL;          /* ownership trasferita */
            lib_entry = 1;
        }
    }
    for (int i = 0; i < nlc; i++) free(libcand[i]);
    free(libcand);

    if (out_candidates) *out_candidates = cand;
    if (out_ncand) *out_ncand = nc;
    if (nc == 1) {
        char* rp = realpath(cand[0], NULL);
        char* res = strdup(rp ? rp : cand[0]);
        free(rp);
        return res;
    }
    if (nc == 0) {
        diag_add(diags, 1, "entry-missing", project, 0,
                 "nessun `SUB Main` né direttiva `LIBRARY \"...\"` nella radice del progetto: "
                 "usa --entry <file.bas>");
    } else {
        char msg[1024];
        snprintf(msg, sizeof msg, "%d entry candidate con %s: usa --entry <file.bas>", nc,
                 lib_entry ? "`LIBRARY`" : "`SUB Main`");
        diag_add(diags, 1, "entry-ambiguous", project, 0, msg);
    }
    return NULL;
}

/* ── check ───────────────────────────────────────────────────────────────────────────── */

/* ── minitests ──────────────────────────────────────────────────────────────────────────
 * Punto UNICO di verifica della suite. Prima esistevano due strade (tools/c99_compile_errors.sh
 * per i soli errori di compilazione, tools/minitest_batch.sh per l'esecuzione) con elenchi di
 * file e ricette -I diverse: i conteggi non erano confrontabili. Qui la scoperta è ricorsiva
 * e ordinata (readdir non lo è: senza sort due run darebbero diff inutili) e la ricetta degli
 * -I è la STESSA di `build` (build_inc).
 */

typedef struct {
    int interactive;
    int heavy;
} MiniTags;

/* Tutti i `.bas` sotto `dir`, ricorsivo. readdir non è ordinato: si ordina dopo. */
static void walk_bas(const char* dir, NameSet* out) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        char path[CLI_PATH];
        snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
        if (is_dir(path)) { walk_bas(path, out); continue; }
        if (!is_file(path) || !ends_with(ent->d_name, ".bas")) continue;
        nameset_add(out, path);
    }
    closedir(d);
}

static int cmp_str(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

/* I TAG del MiniTest (' TAG: interactive / heavy, in colonna 1 come li scrive la suite). */
static void mini_tags(const char* path, MiniTags* t) {
    t->interactive = 0;
    t->heavy = 0;
    char* s = read_all(path);
    if (!s) return;
    for (char* p = s; *p; ) {
        if (strncmp(p, "' TAG: ", 7) == 0) {
            const char* v = p + 7;
            if (strncasecmp(v, "interactive", 11) == 0) t->interactive = 1;
            else if (strncasecmp(v, "heavy", 5) == 0) t->heavy = 1;
        }
        char* eol = strchr(p, '\n');
        if (!eol) break;
        p = eol + 1;
    }
    free(s);
}

/* Copia in `out` fino a `max` righe di `text` che contengono "error:" (solo_errori)
 * oppure non vuote, unite da '\n'. Serve sia a `cc` sia al driver. */
static char* replace_all(const char* text, const char* from, const char* to);

/* Toglie il prefisso "<root>/" da un testo: i dettagli di DRIVER-FAIL/CC-FAIL citano il path
 * assoluto del MiniTest, e il baseline e' VERSIONATO (deve essere identico su ogni macchina). */
static char* strip_root_prefix(const char* text, const char* root) {
    if (!text) return NULL;
    char* out = NULL;
    if (root && *root) {
        char pat[CLI_PATH + 2];
        snprintf(pat, sizeof pat, "%s/", root);
        out = replace_all(text, pat, "");
    }
    /* Anche la HOME va normalizzata: i messaggi di REQUIRE citano la cache dei pacchetti
     * (~/.gmb/packages/...), che su un'altra macchina avrebbe un altro prefisso. Qui HOME serve
     * solo a RENDERE LEGGIBILE un messaggio, non a decidere un percorso. */
    {
        const char* home = getenv("HOME");
        if (home && *home) {
            char hp[CLI_PATH + 2];
            snprintf(hp, sizeof hp, "%s/", home);
            char* src = out ? out : (char*)text;
            char* next = replace_all(src, hp, "~/");
            if (out) free(out);
            out = next;
        }
    }
    return out;
}

static void collect_lines(const char* text, int only_errors, int max, char* out, size_t cap) {
    out[0] = 0;
    size_t w = 0;
    int kept = 0;
    for (const char* p = text; p && *p && kept < max; ) {
        const char* eol = strchr(p, '\n');
        size_t l = eol ? (size_t)(eol - p) : strlen(p);
        char line[1024];
        if (l >= sizeof line) l = sizeof line - 1;
        memcpy(line, p, l);
        line[l] = 0;
        const char* q = line;
        while (*q == ' ' || *q == '\t') q++;
        int take = only_errors ? (strcasestr(q, "error:") != NULL) : (*q != 0);
        if (take && *q) {
            size_t ll = strlen(q);
            if (w + ll + 2 < cap) {
                if (kept) out[w++] = '\n';
                memcpy(out + w, q, ll);
                w += ll;
                out[w] = 0;
                kept++;
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
}

/* Copia di `text` con ogni occorrenza di `from` sostituita da `to` (malloc). */
static char* replace_all(const char* text, const char* from, const char* to) {
    if (!text) return NULL;
    size_t lf = strlen(from), lt = strlen(to), n = 0;
    for (const char* p = text; (p = strstr(p, from)) != NULL; p += lf) n++;
    size_t cap = strlen(text) + n * (lt > lf ? lt - lf : 0) + 1;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t w = 0;
    for (const char* p = text; *p; ) {
        const char* hit = strstr(p, from);
        if (!hit) { size_t l = strlen(p); memcpy(out + w, p, l); w += l; break; }
        size_t l = (size_t)(hit - p);
        memcpy(out + w, p, l);
        w += l;
        memcpy(out + w, to, lt);
        w += lt;
        p = hit + lf;
    }
    out[w] = 0;
    return out;
}

static int count_errors(const char* text) {
    int n = 0;
    for (const char* p = text; p && *p; ) {
        const char* eol = strchr(p, '\n');
        size_t l = eol ? (size_t)(eol - p) : strlen(p);
        char line[1024];
        if (l >= sizeof line) l = sizeof line - 1;
        memcpy(line, p, l);
        line[l] = 0;
        if (strcasestr(line, "error:")) n++;
        if (!eol) break;
        p = eol + 1;
    }
    return n;
}

/* Categoria di un errore `cc`: raggruppa per TIPO di errore, non per simbolo.
 * Normalizza ogni parte fra apici in '*' e toglie le parentesi (`(aka 'double')`, `(fixme)`):
 * altrimenti la lunghezza del nome del tipo cambia il punto di taglio e lo STESSO errore
 * finisce in più categorie. Il taglio è a un confine di parola, per lo stesso motivo. */
static void cc_category(const char* line, char* out, size_t cap) {
    out[0] = 0;
    const char* e = strcasestr(line, "error:");
    if (!e) return;
    e += 6;
    while (*e == ' ') e++;
    char tmp[192];
    size_t w = 0;
    while (*e && *e != '\n' && w + 5 < sizeof tmp) {
        if (*e == '(') {
            int depth = 1;
            e++;
            while (*e && *e != '\n' && depth) {
                if (*e == '(') depth++;
                else if (*e == ')') depth--;
                e++;
            }
            if (w && tmp[w - 1] != ' ') tmp[w++] = ' ';
            continue;
        }
        if (*e == '\'' || *e == '"') {
            char q = *e++;
            while (*e && *e != q && *e != '\n') e++;
            if (*e == q) e++;
            memcpy(tmp + w, "'*'", 3);
            w += 3;
            continue;
        }
        tmp[w++] = *e++;
    }
    tmp[w] = 0;
    /* spazi doppi (lasciati dalla rimozione delle parentesi) fuori dai piedi */
    size_t r = 0, c = 0;
    for (; tmp[r]; r++) {
        if (tmp[r] == ' ' && c && tmp[c - 1] == ' ') continue;
        tmp[c++] = tmp[r];
    }
    tmp[c] = 0;
    /* prime 7 parole: un taglio a metà parola separerebbe lo stesso errore */
    int words = 0;
    size_t k = 0;
    while (tmp[k] && words < 7) {
        if (tmp[k] == ' ') words++;
        k++;
    }
    tmp[k] = 0;
    while (k && (tmp[k - 1] == ' ' || tmp[k - 1] == ',')) tmp[--k] = 0;
    snprintf(out, cap, "%s", tmp);
}

#define MINI_DETAIL_CAP 1200
#define MINI_MAX_CATS 64

typedef struct {
    char path[CLI_PATH];
    char status[16];
    int rc;
    int errors;
    int crash;
    char detail[MINI_DETAIL_CAP];
} MiniFail;

typedef struct {
    char text[128];
    int count;
} MiniCat;

/* Elenco dei file da un JSON di `minitests --json` (le sole chiavi `"file"` sono quelle di
 * `failures`: gli altri oggetti non ne hanno). Serve a `--baseline`: senza un parser JSON nel
 * CLI resta una scansione delle nostre stesse chiavi, quindi va tenuta allineata al writer. */
static void json_files_into_set(const char* json, NameSet* out) {
    const char* p = json;
    while ((p = strstr(p, "\"file\":")) != NULL) {
        p += 7;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '"') continue;
        p++;
        char buf[CLI_PATH];
        size_t w = 0;
        while (*p && *p != '"' && w + 1 < sizeof buf) {
            if (*p == '\\' && p[1]) p++;
            buf[w++] = *p++;
        }
        buf[w] = 0;
        nameset_add(out, buf);
        if (*p == '"') p++;
    }
}

/* Path stampato: relativo all'install se è sotto (come faceva lo script), altrimenti
 * relativo alla radice della suite. Un solo formato per run e JSON. */
static void mini_display_path(const char* abs, const char* root, const char* install,
                              char* out, size_t cap) {
    size_t li = strlen(install);
    if (install[0] && path_under(abs, install) && abs[li] == '/') {
        snprintf(out, cap, "%s", abs + li + 1);
        return;
    }
    size_t lr = strlen(root);
    if (path_under(abs, root) && abs[lr] == '/') {
        /* RELATIVO alla radice dei MiniTest: il baseline e' versionato e deve essere identico
         * su ogni macchina (prima conteneva /Users/<nome>/..., quindi non era portabile). */
        snprintf(out, cap, "%s", abs + lr + 1);
        return;
    }
    snprintf(out, cap, "%s", abs);
}

static int run_minitests(const Options* o) {
    Toolchain tc;
    memset(&tc, 0, sizeof tc);

    char driver[CLI_PATH];
    driver[0] = 0;
    if (o->driver && *o->driver) {
        /* Un driver esplicito inesistente è un errore di SETUP (rc 1), non un verdetto sul
         * codegen: senza questo ogni file diventerebbe DRIVER-FAIL per colpa dell'ambiente. */
        if (strchr(o->driver, '/') && !is_file(o->driver)) {
            fprintf(stderr, "gmb minitests: driver non trovato: %s\n", o->driver);
            return 1;
        }
        snprintf(driver, sizeof driver, "%s", o->driver);
        (void)gmb_resolve_install_root(tc.install, sizeof tc.install);
    } else if (!find_driver(&tc, o->argv0)) {
        fprintf(stderr, "gmb minitests: gmb_c99native_driver non trovato "
                        "(build native/gmb_compiler_c o --driver <exe>)\n");
        return 1;
    } else {
        snprintf(driver, sizeof driver, "%s", tc.driver);
    }

    char root[CLI_PATH];
    if (o->root && *o->root) snprintf(root, sizeof root, "%s", o->root);
    else snprintf(root, sizeof root, "%s/MiniTests", tc.install);
    char root_real[CLI_PATH];
    if (realpath(root, root_real)) snprintf(root, sizeof root, "%s", root_real);
    if (!is_dir(root)) {
        fprintf(stderr, "gmb minitests: radice non trovata: %s\n", root);
        return 1;
    }

    if (o->cc && *o->cc && strchr(o->cc, '/') && !is_file(o->cc)) {
        fprintf(stderr, "gmb minitests: compilatore non trovato: %s\n", o->cc);
        return 1;
    }
    const char* cc = (o->cc && *o->cc) ? o->cc : "cc";
    char inc[CLI_PATH * 24];
    build_inc(inc, sizeof inc, tc.install);

    NameSet files;
    memset(&files, 0, sizeof files);
    walk_bas(root, &files);
    qsort(files.items, (size_t)files.n, sizeof *files.items, cmp_str);

    char tmp[CLI_PATH];
    snprintf(tmp, sizeof tmp, "/tmp/gmb-minitests.XXXXXX");
    if (!mkdtemp(tmp)) {
        perror("gmb minitests: mkdtemp");
        nameset_free(&files);
        return 1;
    }
    char cpath[CLI_PATH];
    snprintf(cpath, sizeof cpath, "%s/out.c", tmp);

    MiniFail* fails = NULL;
    int nfails = 0, capfails = 0;
    MiniCat cats[MINI_MAX_CATS];
    int ncats = 0;
    int total = files.n, run = 0, pass = 0, ccfail = 0, drvfail = 0, crash = 0;
    int skip_i = 0, skip_h = 0;

    for (int i = 0; i < files.n; i++) {
        const char* f = files.items[i];
        char disp[CLI_PATH];
        mini_display_path(f, root, tc.install, disp, sizeof disp);
        if (o->filter && *o->filter && !strstr(disp, o->filter)) continue;

        MiniTags tags;
        mini_tags(f, &tags);
        if (tags.interactive && !o->all && !o->interactive) { skip_i++; continue; }
        if (tags.heavy && !o->all && !o->heavy) { skip_h++; continue; }
        run++;

        /* 1. .bas → C (nessun eseguibile) */
        char qd[CLI_PATH + 8], qf[CLI_PATH + 8], qc[CLI_PATH + 8], cmd[CLI_PATH * 4];
        shell_quote(driver, qd, sizeof qd);
        shell_quote(f, qf, sizeof qf);
        shell_quote(cpath, qc, sizeof qc);
        snprintf(cmd, sizeof cmd, "%s %s %s --no-exe 2>&1", qd, qf, qc);
        char* log = NULL;
        int rc = run_cmd(cmd, &log);

        char detail[MINI_DETAIL_CAP];
        detail[0] = 0;
        int errors = 0;
        int drv_stage = 0;

        if (rc != 0) {
            drvfail++;
            drv_stage = 1;
            int crashed = rc >= 128;
            if (crashed) crash++;
            {
                char* nolog = strip_root_prefix(log ? log : "", root);
                collect_lines(nolog ? nolog : (log ? log : ""), 0, 2, detail, sizeof detail);
                free(nolog);
            }
            fprintf(stderr, "DRIVER-FAIL %s (rc=%d%s)%s%s\n", disp, rc,
                    crashed ? ", crash" : "", detail[0] ? ": " : "", detail);
        } else {
            /* 2. solo sintassi: il link e l'esecuzione non c'entrano col codegen */
            char qcc[CLI_PATH + 8];
            shell_quote(cc, qcc, sizeof qcc);
            snprintf(cmd, sizeof cmd,
                     "%s -fsyntax-only%s -w -DHAVE_OPENSSL -DGMB_PARQUET_HAVE_ZSTD %s 2>&1",
                     qcc, inc, qc);
            free(log);
            log = NULL;
            rc = run_cmd(cmd, &log);
            if (rc != 0) {
                ccfail++;
                /* Gli errori negli header citano il C generato, che vive in una dir
                 * temporanea diversa a ogni run: senza normalizzarlo il baseline cambierebbe
                 * a ogni `--update` e nessuno potrebbe leggere il dettaglio dopo. */
                char* shown = replace_all(log, cpath, "<out.c>");
                char* noroot = strip_root_prefix(shown ? shown : (log ? log : ""), root);
                const char* txt = noroot ? noroot : (shown ? shown : (log ? log : ""));
                errors = count_errors(txt);
                collect_lines(txt, 1, 3, detail, sizeof detail);
                free(noroot);
                free(shown);
                fprintf(stderr, "=== CC-FAIL %s (%d error%s) ===\n", disp, errors,
                        errors == 1 ? "e" : "i");
                for (const char* p = detail; *p; ) {
                    const char* eol = strchr(p, '\n');
                    int l = eol ? (int)(eol - p) : (int)strlen(p);
                    fprintf(stderr, "  %.*s\n", l, p);
                    if (!eol) break;
                    p = eol + 1;
                }
                /* istogramma: una voce per FILE (il primo errore è il più significativo) */
                char cat[128];
                cc_category(detail, cat, sizeof cat);
                if (cat[0]) {
                    int k = 0;
                    for (; k < ncats; k++) if (!strcmp(cats[k].text, cat)) { cats[k].count++; break; }
                    if (k == ncats && ncats < MINI_MAX_CATS) {
                        snprintf(cats[ncats].text, sizeof cats[ncats].text, "%s", cat);
                        cats[ncats].count = 1;
                        ncats++;
                    }
                }
            } else {
                pass++;
            }
        }
        free(log);

        if (rc != 0) {
            if (nfails == capfails) {
                int cap = capfails ? capfails * 2 : 64;
                MiniFail* p = (MiniFail*)realloc(fails, (size_t)cap * sizeof *p);
                if (!p) break;
                fails = p;
                capfails = cap;
            }
            MiniFail* mf = &fails[nfails++];
            snprintf(mf->path, sizeof mf->path, "%s", disp);
            snprintf(mf->status, sizeof mf->status, "%s", drv_stage ? "DRIVER-FAIL" : "CC-FAIL");
            mf->rc = rc;
            mf->errors = errors;
            mf->crash = drv_stage && rc >= 128;
            snprintf(mf->detail, sizeof mf->detail, "%s", detail);
        }
    }

    unlink(cpath);
    rmdir(tmp);

    /* Zero file eseguiti non è un successo: con un `--filter` sbagliato (o una suite vuota) il
     * gate resterebbe verde senza aver verificato nulla. */
    if (run == 0) {
        fprintf(stderr, "gmb minitests: nessun file da verificare%s%s%s\n",
                (o->filter && *o->filter) ? " per il filtro '" : "",
                (o->filter && *o->filter) ? o->filter : "",
                (o->filter && *o->filter) ? "'" : "");
        unlink(cpath);
        rmdir(tmp);
        nameset_free(&files);
        free(fails);
        return 1;
    }

    /* `--baseline`: il pavimento di fallimenti pre-esistenti non è 0 (oggi 84 file), quindi
     * un gate "verde se 0 FAIL" resterebbe rosso per sempre. Col baseline si è rossi solo per
     * le REGRESSIONI (FAIL che prima non c'erano) e per i file spariti dalla suite. */
    NameSet base, base_lc;
    memset(&base, 0, sizeof base);
    memset(&base_lc, 0, sizeof base_lc);
    int have_base = 0, newfails = 0, fixed = 0;
    NameSet cur;
    memset(&cur, 0, sizeof cur);
    if (o->baseline && *o->baseline) {
        char* txt = read_all(o->baseline);
        if (!txt) {
            fprintf(stderr, "gmb minitests: baseline non leggibile: %s\n", o->baseline);
            nameset_free(&files);
            free(fails);
            return 1;
        }
        json_files_into_set(txt, &base);
        free(txt);
        have_base = 1;
        if (o->filter && *o->filter)
            fprintf(stderr, "gmb minitests: avviso, --baseline con --filter: i file non "
                            "eseguiti risultano 'risolti'\n");
        /* confronto case-insensitive: `base` resta com'è per stamparlo, `base_lc` serve al match */
        for (int i = 0; i < base.n; i++) nameset_add_lower(&base_lc, base.items[i]);
        for (int i = 0; i < nfails; i++) nameset_add_lower(&cur, fails[i].path);
        for (int i = 0; i < nfails; i++)
            if (!nameset_has_lower(&base_lc, fails[i].path)) newfails++;
        for (int i = 0; i < base.n; i++)
            if (!nameset_has_lower(&cur, base.items[i])) fixed++;
    }

    int ok = have_base ? (newfails == 0) : (ccfail == 0 && drvfail == 0);

    if (o->json) {
        printf("{\n  \"command\": \"minitests\",\n  \"root\": ");
        json_str(stdout, ".");   /* relativo alla radice: nessun path assoluto nel baseline */
        printf(",\n  \"total\": %d,\n  \"run\": %d,\n  \"skipped_interactive\": %d,"
               "\n  \"skipped_heavy\": %d,\n  \"pass\": %d,\n  \"cc_fail\": %d,"
               "\n  \"driver_fail\": %d,\n  \"driver_crash\": %d,",
               total, run, skip_i, skip_h, pass, ccfail, drvfail, crash);
        if (have_base) printf("\n  \"new_failures\": %d,\n  \"fixed\": %d,", newfails, fixed);
        printf("\n  \"ok\": %s,\n", ok ? "true" : "false");
        printf("  \"cc_categories\": [");
        for (int i = 0; i < ncats; i++) {
            printf("%s\n    {\"category\": ", i ? "," : "");
            json_str(stdout, cats[i].text);
            printf(", \"count\": %d}", cats[i].count);
        }
        printf("\n  ],\n  \"failures\": [");
        for (int i = 0; i < nfails; i++) {
            MiniFail* mf = &fails[i];
            printf("%s\n    {\"file\": ", i ? "," : "");
            json_str(stdout, mf->path);
            printf(", \"status\": ");
            json_str(stdout, mf->status);
            printf(", \"exit\": %d, \"errors\": %d, \"crash\": %s, \"detail\": ",
                   mf->rc, mf->errors, mf->crash ? "true" : "false");
            json_str(stdout, mf->detail);
            printf("}");
        }
        printf("\n  ]");
        if (have_base) {
            printf(",\n  \"regressions\": [");
            int first = 1;
            for (int i = 0; i < nfails; i++) {
                if (nameset_has_lower(&base_lc, fails[i].path)) continue;
                printf("%s", first ? "" : ",");
                printf("\n    ");
                json_str(stdout, fails[i].path);
                first = 0;
            }
            printf("\n  ],\n  \"resolved\": [");
            first = 1;
            for (int i = 0; i < base.n; i++) {
                if (nameset_has_lower(&cur, base.items[i])) continue;
                printf("%s", first ? "" : ",");
                printf("\n    ");
                json_str(stdout, base.items[i]);
                first = 0;
            }
            printf("\n  ]");
        }
        printf("\n}\n");
    } else {
        fprintf(stderr, "---------------------------------\n");
        if (o->filter && *o->filter)
            fprintf(stderr, "filtro: %s\n", o->filter);
        if (skip_i || skip_h)
            fprintf(stderr, "esclusi per TAG: %d interactive, %d heavy (--all per includerli)\n",
                    skip_i, skip_h);
        if (ncats) {
            fprintf(stderr, "CC-FAIL per categoria (un voto per file):\n");
            for (int i = 0; i < ncats; i++)
                fprintf(stderr, "  %3d  %s\n", cats[i].count, cats[i].text);
        }
        fprintf(stderr, "MiniTests: %d file, %d eseguiti — PASS %d · CC-FAIL %d · "
                        "DRIVER-FAIL %d (di cui %d crash)\n",
                total, run, pass, ccfail, drvfail, crash);
        /* riga storica di tools/c99_compile_errors.sh, tenuta per chi la legge ancora */
        fprintf(stderr, "C99 compile-error: %d\n", ccfail);
        if (have_base) {
            for (int i = 0; i < nfails; i++)
                if (!nameset_has_lower(&base_lc, fails[i].path))
                    fprintf(stderr, "REGRESSIONE %s\n", fails[i].path);
            for (int i = 0; i < base.n; i++)
                if (!nameset_has_lower(&cur, base.items[i]))
                    fprintf(stderr, "RISOLTO %s\n", base.items[i]);
            fprintf(stderr, "baseline: %d nuovi FAIL, %d risolti — %s\n", newfails, fixed,
                    ok ? "nessuna regressione" : "REGRESSIONE");
        }
        fprintf(stderr, "MiniTests: %s\n", ok ? "PASS" : "FAIL");
    }

    nameset_free(&cur);
    nameset_free(&base_lc);
    nameset_free(&base);
    free(fails);
    nameset_free(&files);
    return ok ? 0 : 2;
}

/* ── main ────────────────────────────────────────────────────────────────────────────── */


int main(int argc, char** argv) {
    Options o; memset(&o, 0, sizeof o);
    o.argv0 = argv[0];
    if (argc < 2) { gmb_cli_usage(); return 2; }
    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "help")) {
        gmb_cli_usage();
        return 0;
    }
    {
        int arc = gmb_cli_admin_maybe(argc, argv);   /* pkg/fixtures/setup/... → gmbadmin (C) */
        if (arc >= 0) return arc;
    }
    {
        int erc = gmb_cli_engine_maybe(argc, argv);
        if (erc >= 0) return erc;
    }
    {
        int frc = gmb_cli_file_maybe(argc, argv);   /* `gmb <file.bas>` → run --project */
        if (frc >= 0) return frc;
    }
    o.command = argv[1];
    /* F1: dump/eval sono OPZIONI del compilatore usate dalla REPL (nessun accesso diretto
     * a lexer/parser dal lato REPL). Forme: `gmb dump ast|chunk|c <file> [--out f]`,
     * `gmb eval "<istruzioni>"`. */
    if (!strcmp(o.command, "dump") || !strcmp(o.command, "eval")) {
        gmb_cli_set_defines(argc, argv);
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--quiet") || !strcmp(argv[i], "-q")) gmb_cli_set_quiet(1);
            if (!strcmp(argv[i], "--gpm") && i + 1 < argc) gmb_cli_set_gpm(argv[i + 1]);
            else if (!strncmp(argv[i], "--gpm=", 6)) gmb_cli_set_gpm(argv[i] + 6);
        }
    }
    if (!strcmp(o.command, "dump")) {
        if (argc < 4) { fprintf(stderr, "uso: gmb dump ast|chunk|c|disasm <file.bas> [--out <file>]\n"); return 2; }
        const char* what = argv[2];
        const char* file = argv[3];
        const char* out = NULL;
        for (int i = 4; i < argc; i++) {
            if (!strncmp(argv[i], "--out=", 6)) out = argv[i] + 6;
            else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        }
        int rc = 2;
        if (!strcmp(what, "ast"))    rc = gmb_cli_dump_ast(file);
        else if (!strcmp(what, "chunk"))  rc = gmb_cli_dump_chunk(file, out);
        else if (!strcmp(what, "c"))      rc = gmb_cli_dump_c(file, out);
        else if (!strcmp(what, "disasm")) rc = gmb_cli_dump_disasm(file);
        gmb_cli_reset_defines();
        return rc;
        fprintf(stderr, "gmb: dump sconosciuto '%s' (ast|chunk|c|disasm)\n", what);
        gmb_cli_reset_defines();
        return 2;
    }
    if (!strcmp(o.command, "eval")) {
        if (argc < 3) { fprintf(stderr, "uso: gmb eval \"<istruzioni BASIC>\"\n"); return 2; }
        int rc = gmb_cli_eval(argv[2]);
        gmb_cli_reset_defines();
        return rc;
    }
    if (strcmp(o.command, "check") && strcmp(o.command, "build") && strcmp(o.command, "run")
        && strcmp(o.command, "minitests")) {
        fprintf(stderr, "gmb: comando sconosciuto '%s'\n", o.command);
        gmb_cli_usage();
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "--json")) { o.json = 1; continue; }
        if (!strcmp(a, "--strict")) { o.strict = 1; continue; }
        if (!strcmp(a, "--quiet") || !strcmp(a, "-q")) { o.quiet = 1; continue; }
        /* Engine: già consumato dal dispatch (qui solo per non far fallire il parsing). */
        if (!strcmp(a, "--engine") && i + 1 < argc) { i++; continue; }
        if (!strncmp(a, "--engine=", 9)) continue;
        if (!strcmp(a, "--csharp-cli") && i + 1 < argc) { i++; continue; }
        if (!strncmp(a, "--csharp-cli=", 13)) continue;
        if (!strcmp(a, "--all")) { o.all = 1; continue; }
        /* Bundle di distribuzione (--libs/--plugins/--skills/--brains): parser in
         * gmb_cli_deploy.c (tiene questo file sotto la soglia di 1200 righe). */
        if (gmb_parse_bundle_arg(&o, a)) continue;
        if (!strcmp(a, "--interactive")) { o.interactive = 1; continue; }
        if (!strcmp(a, "--heavy")) { o.heavy = 1; continue; }
        const char* v = NULL;
        if (!strncmp(a, "--project=", 10)) { o.project = a + 10; continue; }
        if (!strncmp(a, "--entry=", 8)) { o.entry = a + 8; continue; }
        if (!strncmp(a, "--out=", 6)) { o.out = a + 6; continue; }
        if (!strncmp(a, "--root=", 7)) { o.root = a + 7; continue; }
        if (!strncmp(a, "--filter=", 9)) { o.filter = a + 9; continue; }
        if (!strncmp(a, "--driver=", 9)) { o.driver = a + 9; continue; }
        if (!strncmp(a, "--cc=", 5)) { o.cc = a + 5; continue; }
        if (!strncmp(a, "--baseline=", 11)) { o.baseline = a + 11; continue; }
        if (!strncmp(a, "--gpm=", 6)) { o.gpm = a + 6; continue; }
        if ((!strcmp(a, "--project") || !strcmp(a, "--entry") || !strcmp(a, "--out")
             || !strcmp(a, "--root") || !strcmp(a, "--filter") || !strcmp(a, "--driver")
             || !strcmp(a, "--cc") || !strcmp(a, "--baseline") || !strcmp(a, "--gpm")) && i + 1 < argc) {
            v = argv[++i];
            if (!strcmp(a, "--project")) o.project = v;
            else if (!strcmp(a, "--entry")) o.entry = v;
            else if (!strcmp(a, "--out")) o.out = v;
            else if (!strcmp(a, "--root")) o.root = v;
            else if (!strcmp(a, "--filter")) o.filter = v;
            else if (!strcmp(a, "--driver")) o.driver = v;
            else if (!strcmp(a, "--baseline")) o.baseline = v;
            else if (!strcmp(a, "--gpm")) o.gpm = v;
            else o.cc = v;
            continue;
        }
        fprintf(stderr, "gmb: opzione sconosciuta '%s'\n", a);
        gmb_cli_usage();
        return 2;
    }
    /* minitests non ha un progetto: la radice è la suite stessa. */
    if (!strcmp(o.command, "minitests")) return run_minitests(&o);

    if (!o.project || !*o.project) {
        fprintf(stderr, "gmb: --project <dir> è obbligatorio\n");
        gmb_cli_usage();
        return 2;
    }

    char proj[CLI_PATH];
    if (!realpath(o.project, proj)) {
        fprintf(stderr, "gmb: progetto non trovato: %s\n", o.project);
        return 2;
    }

    DiagList diags; memset(&diags, 0, sizeof diags);
    char** cand = NULL; int ncand = 0;
    char* entry = pick_entry(proj, o.entry, &diags, &cand, &ncand);
    int sources = 0, modules = 0, rc = 0;
    char exe[CLI_PATH]; exe[0] = 0;
    int is_lib = 0;
    (void)cand;

    if (entry) {
        if (!strcmp(o.command, "check")) {
            run_check(&o, proj, entry, &diags, &sources, &modules);
        } else {
            rc = build_project(&o, proj, entry, &diags, exe, sizeof exe, &is_lib);
            if (rc == 0) {
                char** srcs = NULL; int ns = 0;
                gmb_project_sources(proj, &srcs, &ns);
                for (int i = 0; i < ns; i++) if (path_under(srcs[i], proj)) sources++;
                gmb_string_list_free(srcs, ns);
                char** mods = gmb_resolve_module_files(entry, &modules);
                gmb_resolve_module_files_free(mods, modules);
            }
        }
    }

    int errors = diag_errors(&diags);
    int warnings = diag_warnings(&diags);
    if (!entry && errors == 0) errors = 1;
    if (o.strict) errors += warnings;
    if (rc == 0 && errors > 0) rc = 2;

    if (o.json) {
        printf("{\n  \"command\": "); json_str(stdout, o.command);
        printf(",\n  \"project\": "); json_str(stdout, proj);
        printf(",\n  \"entry\": "); if (entry) json_str(stdout, entry); else printf("null");
        printf(",\n  \"sources\": %d,\n  \"modules\": %d,\n", sources, modules);
        if (exe[0]) { printf("  \"exe\": "); json_str(stdout, exe); printf(",\n");
                      printf("  \"library\": %s,\n", is_lib ? "true" : "false"); }
        printf("  \"ok\": %s,\n  \"diagnostics\": [", errors == 0 ? "true" : "false");
        for (int i = 0; i < diags.n; i++) {
            Diag* d = &diags.items[i];
            printf("%s\n    {\"severity\": \"%s\", \"code\": ", i ? "," : "",
                   d->is_error ? "error" : "warning");
            json_str(stdout, d->code);
            printf(", \"file\": "); json_str(stdout, d->file);
            printf(", \"line\": %d, \"message\": ", d->line);
            json_str(stdout, d->message);
            printf("}");
        }
        printf("\n  ]\n}\n");
    } else {
        for (int i = 0; i < diags.n; i++) {
            Diag* d = &diags.items[i];
            if (d->line > 0) fprintf(stderr, "%s:%d: %s: %s\n", d->file, d->line,
                                     d->is_error ? "errore" : "avviso", d->message);
            else fprintf(stderr, "%s: %s: %s\n", d->file,
                         d->is_error ? "errore" : "avviso", d->message);
        }
        if (!o.quiet) {
            if (entry) fprintf(stderr, "gmb %s: entry %s\n", o.command, entry);
            fprintf(stderr, "gmb %s: %d sorgenti, %d moduli, %d errori, %d avvisi — %s\n",
                    o.command, sources, modules, diag_errors(&diags), warnings,
                    (rc == 0 && errors == 0) ? "OK" : "FALLITO");
            if (exe[0]) fprintf(stderr, "gmb: %s %s\n", is_lib ? "libreria" : "eseguibile", exe);
        }
    }

    if (rc == 0 && errors > 0) rc = 2;

    /* run: esegue l'eseguibile con CWD = progetto (il figlio eredita stdout/stderr).
     * Una LIBRARY non ha entry point: `run` costruisce la .gmblib e si ferma lì. */
    if (rc == 0 && entry && !strcmp(o.command, "run")) {
        if (is_lib) {
            fprintf(stderr, "gmb run: %s è una libreria (LIBRARY), nessun entry point da eseguire\n", exe);
        } else {
            if (chdir(proj) != 0) { perror("gmb: chdir"); return 2; }
            pid_t pid = fork();
            if (pid == 0) {
                execl(exe, exe, (char*)NULL);
                _exit(127);
            }
            if (pid < 0) { perror("gmb: fork"); return 6; }
            int st = 0;
            if (waitpid(pid, &st, 0) < 0) return 6;
            if (WIFEXITED(st)) rc = WEXITSTATUS(st) ? 6 : 0;
            else rc = 6;
        }
    }

    for (int i = 0; i < ncand; i++) free(cand[i]);
    free(cand);
    free(entry);
    diags_free(&diags);
    return rc;
}
