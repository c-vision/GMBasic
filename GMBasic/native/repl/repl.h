/* native/repl/repl.h — ReplNative (C): REPL del Native Executor (vmc|c99), mai IL.
 * Struttura speculare a REPL/executor-managed (C#): un modulo C per ogni modulo
 * originale (Arguments, ReplEngine, Orchestrator, editor, helps, settings, runner vmc/c99).
 * L'esecuzione e' 100% C (gmb_vm_host / cc); la compilazione .bas->chunk/.c usa un ponte
 * Percorso 100% C: compilatore P-Code C (gmb_compile) -> chunk -> gmb_vm_host.
 */
#ifndef GMB_REPL_H
#define GMB_REPL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "gmb_debug.h"

/* helper: path relativo alla root repo */
void resolve_repo_path(char* out, int cap, const char* rel);

/* ---- backend (mai IL) ---- */
typedef enum { RPL_VMC, RPL_C99 } rpl_backend_t;

/* ---- Arguments.cs -> arguments.c ---- */
typedef struct rpl_options {
    rpl_backend_t backend;
    const char* file_name;
    bool no_console;
    bool use_sdl;           /* default true (flag -c/-console lo spegne) */
    bool use_system_console;
    bool use_c64_mode;
    bool verbose;
    bool no_optimize;
    bool dump_il;
    const char* compile_bsp_dir;
    const char* gpm_mode;   /* Always/... (C99GpmMode) */
    int grid_width, grid_height;
    const char* font_name;   /* -font name,size */
    int font_size;
    int debug_port;
    const char* run_command;
    const char* defines[64];
    int defines_count;
    /* Engine del compilatore: "c" (default, gmb) oppure "csharp" (gmbcs, opzionale).
     * Parità totale mentre GMBasicSharp esiste (TODO §6, F4); con --engine=c la REPL
     * non tocca il mondo C# per nessun comando. */
    const char* engine;
    const char* csharp_cli;   /* --csharp-cli <path> (altrimenti scoperta standard) */
} rpl_options_t;

/* Parsa la CLI (tutti i flag di Arguments.cs, vmc|c99). Ritorna 0 ok, !=0 errore. */
int rpl_parse_args(int argc, char** argv, rpl_options_t* o);
/* Flag IL vietati (-n/-m/--il-module): appartengono a REPL. */
int rpl_il_forbidden(int argc, char** argv);

/* ---- SettingsManager.cs / IdeSettings.cs -> settings.c ---- */
typedef struct rpl_settings {
    char* import_files[64];
    int import_count;
    const char* font_path;
    int font_size;
    char* palette_name;
    char* theme;
    /* SET opzioni (stato reale della sessione) */
    bool bell, confirm, cursor, century, wrap, escape;
    char dateformat[32];
    char set_theme[32];
} rpl_settings_t;

void rpl_settings_load(const char* project_dir, rpl_settings_t* out);
void rpl_settings_save(const char* project_dir, rpl_settings_t* s);

/* ---- HelpIndex/HelpForm.cs -> helps.c ---- */
int rpl_help_show(const char* topic);

/* ---- GWBasicEditor.cs -> editor.c (buffer LIST/NEW/SAVE/LOAD/EDIT/AUTO/DELETE/RENUM) ---- */
void rpl_editor_load(const char* path);
void rpl_editor_save(const char* path);
void rpl_editor_list(void);
void rpl_editor_new(void);
void rpl_editor_delete(const char* range);
void rpl_editor_auto(int inc);
void rpl_editor_renum(void);
int rpl_editor_count(void);
/* Store numerato (parità _programLines): imposta/rimuove una riga per numero e
 * restituisce il sorgente completo del buffer (per RUN del programma in memoria). */
void rpl_editor_set_line(int num, const char* body);
void rpl_editor_remove_line(int num);
const char* rpl_editor_source(void);
/* Editor full-screen (speculare a GWBasicEditor.cs). */

/* Browser help interattivo (speculare a HelpForm.cs). */
int gmb_help_browse(void);
/* Indice dell'help scritto nel buffer a celle (console a finestra SDL/C64). */
struct gmb_sdl_console;
int gmb_help_print_index(struct gmb_sdl_console* model);
/* IDE full-screen (speculare a GMBasicIde.cs). */
int gmb_ide_run(const char* file);
/* chunk A1: PRGINFO/DASM/LOADPRG */
int gmb_prginfo(const char* file);
int gmb_dasm(const char* file);
/* Contratto a processo col compilatore C (F2): la REPL non include il front-end. */
const char* rpl_gmb_cli(char* out, size_t cap);
int rpl_gmb_dump(const char* what, const char* file, const char* out);
int rpl_gmb_dump_ex(const char* what, const char* file, const char* out, const rpl_options_t* o);
/* Engine C# opzionale (gmbcs): 1 se selezionato con --engine=csharp. */
int rpl_engine_cs(const rpl_options_t* o);
/* Percorso di gmbcs (--csharp-cli o scoperta standard). NULL se assente. */
const char* rpl_cs_cli(const rpl_options_t* o, char* out, size_t cap);
/* Esegue `gmbcs <sub> "<arg>" <extra>`; 0 = ok. Stampa l'errore se il CLI manca. */
int rpl_cs_cmd(const char* sub, const char* arg, const char* extra, const rpl_options_t* o);
const char* rpl_resolve_vm_host(void);
/* F3b: comandi di pacchetto nel mondo C (manifest/keyring/cache). */
int rpl_pkg_cmd(const char* cmd, const char* args);
int gmb_loadprg(const char* file);

/* ---- ReplEngine.cs -> repl.c ---- */
int rpl_loop(const rpl_options_t* o, const char* initial_dir);

/* ---- Orchestrator.cs -> orchestrator.c (compila .bas -> chunk/.c via ponte C#) ---- */

/* ---- GmVmHostRunner.cs/VmCDebugDriver.cs -> vmc_runner.c ---- */
int rpl_run_vmc(const char* bas_file, const rpl_options_t* o);

/* ---- C99DebugDriver.cs -> c99_runner.c ---- */
int rpl_run_c99(const char* bas_file, const rpl_options_t* o);

/* ---- ponte C# (comandi C#-heavy: bsp/convert/package/ml/providers/aipath) -> bridge.c ---- */
int rpl_bridge_cmd(const char* cmd, const char* arg1);

/* GRAPH BUILD (percorso C): grafo wiki via gmb_graph + PageRank (runtime condiviso). */
void rpl_graph_build(const char* wiki_dir);

/* Sessione di debug della REPL (DEBUG/BREAK/STEP/CONT/WATCH/TRACE). */
extern gmb_debug_session_t g_dbg;

#endif
