/* gmb_cli_internal.h — contratto INTERNO del CLI `gmb` (gmb_cli.c ↔ gmb_cli_project.c).
 *
 * PERCHÉ ESISTE: `gmb_cli.c` era 1602 righe (soglia 1200, AGENTS §0quater). I comandi
 * `check`/`build` (scoperta sorgenti/moduli, driver, -I, librerie, link) sono stati spostati in
 * `gmb_cli_project.c`; qui stanno i tipi e i prototipi che i due file condividono. I corpi degli
 * helper comuni (lettura file, diagnostica, shell) restano in gmb_cli.c: una sola implementazione.
 */
#ifndef GMB_CLI_INTERNAL_H
#define GMB_CLI_INTERNAL_H

#include <stddef.h>
#include <stdio.h>

/* front-end (stesso compilatore): usato da `gmb dump ast` */
#include "gmb_lexer.h"
#include "gmb_macro_expand.h"
#include "gmb_parser.h"
#include "gmb_linker.h"
#include "gmb_ast.h"

/* F1 — dump dietro OPZIONI (la REPL non include il front-end, chiama queste) */
int gmb_cli_dump_ast(const char* file);
/* Defines di preprocessing (coppie NAME=VAL) applicate dal CLI, non dalla REPL. */
void gmb_cli_set_defines(int argc, char** argv);
void gmb_cli_reset_defines(void);
int gmb_cli_dump_chunk(const char* file, const char* out);
int gmb_cli_dump_c(const char* file, const char* out);
int gmb_cli_dump_disasm(const char* file);
int gmb_cli_eval(const char* code);
/* --quiet: sopprime lo stdout informativo dei driver (chunk -> …). */
void gmb_cli_set_quiet(int on);
/* --gpm (dump c): modo GPM passato al driver C99. */
void gmb_cli_set_gpm(const char* mode);

#define CLI_PATH 4096
#define CLI_MAX_NS 128   /* namespace massimi considerati da `check` (ex gmb_cli.c) */

typedef struct {
    char* file;
    int line;
    char* code;
    char* message;
    int is_error;
} Diag;

typedef struct {
    Diag* items;
    int n, cap;
} DiagList;

typedef struct {
    char** items;
    int n, cap;
} NameSet;

typedef struct {
    char driver[CLI_PATH];
    char install[CLI_PATH];
} Toolchain;

typedef struct {
    const char* argv0;
    const char* command;
    const char* project;
    const char* entry;
    const char* out;
    const char* root;    /* minitests: radice della suite (default <install>/MiniTests) */
    const char* filter;  /* minitests: sottostringa del path (come GMB_MINI_ONLY) */
    const char* driver;  /* minitests: driver esplicito (default: accanto al CLI) */
    const char* cc;      /* minitests: compilatore per -fsyntax-only (default cc) */
    const char* baseline; /* minitests: JSON di un run precedente (solo regressioni) */
    int json;
    int strict;
    int quiet;           /* check|build|run: niente progresso su stderr */
    const char* gpm;     /* check|build|run|dump c: auto|always|linear (emit C99) */
    int all;             /* minitests: includi anche interactive/heavy */
    int interactive;     /* minitests: solo TAG interactive */
    int heavy;           /* minitests: solo TAG heavy */
} Options;

/* ── helper comuni (definiti in gmb_cli.c) ─────────────────────────────────────────────── */
void diag_add(DiagList* l, int is_error, const char* code, const char* file,
              int line, const char* message);
char* read_all(const char* path);
int is_file(const char* p);
int is_dir(const char* p);
void path_dir(const char* path, char* out, size_t cap);
int ends_with(const char* s, const char* suffix);
int mkdir_p(const char* path);
void shell_quote(const char* in, char* out, size_t cap);
int run_cmd(const char* cmd, char** out);
int import_line(const char* path, const char* ns);
int nameset_has(const NameSet* s, const char* v);
void nameset_add(NameSet* s, const char* v);
void nameset_free(NameSet* s);
int path_under(const char* path, const char* root);

/* ── comandi e helper di progetto (definiti in gmb_cli_project.c) ──────────────────────── */
int find_driver(Toolchain* tc, const char* argv0);
void build_inc(char* inc, size_t cap, const char* install);
void run_check(const Options* o, const char* project, const char* entry,
               DiagList* diags, int* out_sources, int* out_modules);
/* Testo di aiuto del CLI. */
void gmb_cli_usage(void);

/* Engine: -1 = resta sull'engine C; altrimenti exit code (con csharp esegue gmbcs). */
int gmb_cli_engine_maybe(int argc, char** argv);

/* Comandi admin: -1 = non è admin (prosegui); altrimenti exec di `gmbadmin`. */
int gmb_cli_admin_maybe(int argc, char** argv);

/* `gmb <file.bas>`: -1 = non è un file (prosegui); altrimenti esegue `run --project`. */
int gmb_cli_file_maybe(int argc, char** argv);

/* Build di progetto: emette e linka; ritorna l'exit code del CLI. */
int build_project(const Options* o, const char* project, const char* entry,
                  DiagList* diags, char* out_exe, size_t exe_cap, int* out_is_lib);
int library_directive(const char* path, char* out, size_t cap);

#endif /* GMB_CLI_INTERNAL_H */
