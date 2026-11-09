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
    /* build|run: bundle di distribuzione (sottocartelle nella destinazione). */
    int libs;            const char* libs_sel;      /* --libs[=used|all|gruppo|comp|nomi] */
    int plugins;         const char* plugins_sel;   /* --plugins[=all|nomi] */
    int skills;          const char* skills_sel;    /* --skills[=all|nomi]  */
    int brains;          const char* brains_sel;    /* --brains[=all|nomi]  */
    /* -D/--define del PREPROCESSORE BASIC (stessa semantica di gmbcs). */
    char defines[32][160];
    int  ndefines;
    const char* target;  /* build: native|c99 (il resto è dell'engine C#) */
    int interactive;     /* minitests: solo TAG interactive */
    int heavy;           /* minitests: solo TAG heavy */
    /* run: argomenti del PROGRAMMA dopo `--` — inoltrati all'eseguibile (argv[1..]),
     * NON interpretati dal CLI. Il programma li legge con COMMAND$/COMMANDCOUNT. */
    char** prog_argv;
    int    prog_argc;
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
/* Bundle di distribuzione del programma compilato: --libs/--plugins/--skills/--brains.
 * `o` può essere NULL (solo la dylib AI, sempre necessaria). */
typedef struct {
    int         libs;
    const char* libs_sel;
    const char* src;       /* sorgente d'ingresso: per raccogliere le FFI (DECLARE … LIB) */
    int         plugins;
    const char* plugins_sel;
    int         skills;
    const char* skills_sel;
    int         brains;
    const char* brains_sel;
} DeployOpts;
void gmb_deploy_bundle(const char* exe, const char* install, const DeployOpts* o, int quiet);
/* Riconosce UNA opzione extra (bundle, -D/--define, --target). 1 se consumata. */
int  gmb_parse_extra_arg(Options* o, const char* a, int argc, char** argv, int* i);

/* ── comandi e helper di progetto (definiti in gmb_cli_project.c) ──────────────────────── */
int find_driver(Toolchain* tc, const char* argv0);
void build_inc(char* inc, size_t cap, const char* install);
/* Elenco degli -I esistenti (runtime, LLMEnv risolta, header terze parti): la stessa lista
 * che `build_inc` passa a `cc`. `gmb doctor` la usa per dire cosa manca davvero. */
int  gmb_cli_inc_dirs(const char* install, char (*out)[CLI_PATH], int max);

/* `gmb doctor [--fix] [--json] [--llm-repo <url>]`: diagnosi dell'ambiente (GMBASIC_ROOT,
 * LLM_ROOT, companion, statiche, dylib, header, cc, cache) e installazione di ciò che è
 * scaricabile (LLMEnv, librerie native, gmbcs). */
int  gmb_cli_doctor(int argc, char** argv);
void run_check(const Options* o, const char* project, const char* entry,
               DiagList* diags, int* out_sources, int* out_modules);
/* Testo di aiuto del CLI. */
void gmb_cli_usage(void);

/* Engine: -1 = resta sull'engine C; altrimenti exit code (con csharp esegue gmbcs). */
int gmb_cli_engine_maybe(int argc, char** argv);
/* Trova `gmbcs` (PATH, checkout di sviluppo, dir dell'eseguibile). 1 se trovato. */
int gmb_cli_find_gmbcs(const char* explicit_path, char* out, size_t cap);

/* Comandi admin: -1 = non è admin (prosegui); altrimenti exec di `gmbadmin`. */
int gmb_cli_admin_maybe(int argc, char** argv);
/* Trova il tool `gmbadmin` (PATH, build del checkout, dir dell'eseguibile). 1 se trovato. */
int gmb_cli_find_admin(char* out, size_t cap);

/* Precompila le pagine BSP del progetto (`<progetto>/pages/*.bsp` → `.chunk` A1) per il
 * motore BSP COMPLETO in C (VM/C). 0 = fatto/senza pagine (non fa fallire la build). */
int gmb_cli_precompile_bsp(const char* project);

/* `gmb <file.bas>`: -1 = non è un file (prosegui); altrimenti esegue `run --project`. */
int gmb_cli_file_maybe(int argc, char** argv);

/* Build di progetto: emette e linka; ritorna l'exit code del CLI. */
int build_project(const Options* o, const char* project, const char* entry,
                  DiagList* diags, char* out_exe, size_t exe_cap, int* out_is_lib);
int library_directive(const char* path, char* out, size_t cap);

#endif /* GMB_CLI_INTERNAL_H */
