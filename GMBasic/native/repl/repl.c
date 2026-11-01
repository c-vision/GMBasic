/* native/repl/repl.c — speculare a ReplEngine.cs: loop interattivo, 53 comandi.
 * Dispatch: esecuzione vmc|c99 via Native Executor V2; comandi C#-heavy via ponte.
 */
#include "repl.h"
#include "menu.h"
#include "ml.h"
#include "gmb_lexer.h"
#include "gmb_parser.h"
#include "gmb_linker.h"
#include "gmb_macro_expand.h"
#include "gmb_compiler.h"
#include "packages.h"
#include "gmb_graph.h"
#include "gmb_iconsole.h"
#include "gmb_sdl_console.h"
#include "gmb_gfx_input.h"
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <termios.h>

/* Layer controlli TUI (via UNICA): dichiarato extern per non includere gmb_tui_widgets.h
 * (che ridefinisce `struct gmb_window`, gia' presente in un header del REPL). La UI usa
 * gli INDICI d'ordine (gmb_tui_control_count), non i puntatori. */
extern void        gmb_tui_controls_reset(void);
extern void*       gmb_tui_add_codearea(int x, int y, int w, int h, const char* csv);
extern void*       gmb_tui_add_textinput(int x, int y, int w, const char* initial);
extern void*       gmb_tui_add_statusbar(int x, int y, int w, const char* text);
extern void        gmb_tui_control_focus(int i);
extern int         gmb_tui_controls_frame(int ms);
extern int         gmb_tui_sdl_pump(void);
extern void        gmb_tui_sdl_present(void);
extern int         gmb_tui_host_key_pop(void);
extern void        gmb_ui_frame(int ms);
extern void        gmb_tui_screen_flush(void);
extern void        gmb_tui_control_set_text(int i, const char* text);
extern const char* gmb_tui_control_text_ptr(int i);
extern int         gmb_tui_control_count(void);
/* forward declaration delle funzioni brain (gmb_runtime.h non ha guardie e
 * includerlo tira dentro dipendenze non linkate dalla REPL nativa). */
int   gmb_brain_use(const char* names);
int   gmb_brain_detach(const char* name);
char* gmb_brain_registry_list(void);
char* gmb_brain_registry_info(const char* name);
/* grep engine (gmb_runtime, linkato staticamente): variante JSON (ABI C pura). */
char* gmb_grep_search_json(const char* opts_json, const char* path);
char* gmb_grep_format_json(const char* hits_json, int with_line, int with_column);
void  gmb_grep_free(char* p);
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>

/* Sessione di debug globale della REPL. */
gmb_debug_session_t g_dbg;
/* Palette ANSI (PALETTE): nome -> sequenze fg/bg. */
typedef struct { const char* name; const char* fg; const char* bg; } gmb_palette_t;
static const gmb_palette_t gmb_palettes[] = {
    {"default", "\x1b[0m",   "\x1b[49m"},
    {"dark",    "\x1b[97m",  "\x1b[40m"},
    {"light",   "\x1b[30m",  "\x1b[47m"},
    {"c64",     "\x1b[36m",  "\x1b[40m"},
    {"amber",   "\x1b[33m",  "\x1b[40m"},
    {NULL, NULL, NULL}
};
static const char* g_pal_fg = "\x1b[0m";
static const char* g_pal_bg = "\x1b[49m";

/* Applica la palette (cerca per nome, case-insensitive). Ritorna 0 ok. */
static int gmb_palette_apply(const char* name)
{
    for (int i = 0; gmb_palettes[i].name; i++)
        if (strcasecmp(name, gmb_palettes[i].name) == 0) {
            g_pal_fg = gmb_palettes[i].fg;
            g_pal_bg = gmb_palettes[i].bg;
            printf("%s%s(PALETTE: %s)%s\n", g_pal_fg, g_pal_bg, gmb_palettes[i].name, "\x1b[0m");
            return 0;
        }
    fprintf(stderr, "?PALETTE: '%s' sconosciuta (disponibili: default dark light c64 amber)\n", name);
    return -1;
}

/* Settings globali della sessione (caricati in rpl_loop, modificati da SET). */
rpl_settings_t g_settings;
static char g_settings_dir[1024] = "";

/* Risolve un path relativo alla root del repo (GMBASIC.sln trovato salendo). */
/* Risolve una risorsa del repo C# ("REPL/Helps", "Resources", ...).
 * Il marker storico `GMBASIC.sln` non esiste più col monorepo: si cercano i percorsi
 * REALI risalendo dalla CWD — prima la forma monorepo (<root>/GMBasicSharp/<rel>),
 * poi quella del repo C# autonomo (<root>/<rel>). */
void resolve_repo_path(char* out, int cap, const char* rel)
{
    char cwd[2048];
    if (!getcwd(cwd, sizeof(cwd))) { snprintf(out, cap, "%s", rel); return; }
    char cur[2048];
    snprintf(cur, sizeof(cur), "%s", cwd);
    for (;;) {
        char probe[4096];
        snprintf(probe, sizeof(probe), "%s/GMBasicSharp/%s", cur, rel);
        if (access(probe, F_OK) == 0) { snprintf(out, cap, "%s", probe); return; }
        snprintf(probe, sizeof(probe), "%s/%s", cur, rel);
        if (access(probe, F_OK) == 0) { snprintf(out, cap, "%s", probe); return; }
        char* slash = strrchr(cur, '/');
        if (!slash) break;
        *slash = 0;
        if (!*cur) break;
    }
    snprintf(out, cap, "%s/%s", cwd, rel);
}

static int rpl_is_dir_command(const char* cmd)
{
    return strcasecmp(cmd, "LS") == 0 || strcasecmp(cmd, "DIR") == 0;
}

/* GREP: comando di ricerca file avanzato (motore gmb_grep, variante JSON ABI-stabile).
 * Uso: GREP [-rinlcFwv-.] [--type EXT] [--include G] [--exclude G] [--depth N] PATTERN [PATH]
 * Pattern tra apici se contiene spazi. Output: file:line:column:text. */
static void rpl_grep(const char* args)
{
    char toks[64][512]; int nt = 0;
    const char* p = args;
    while (*p && nt < 64) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        int quoted = 0; char q = 0; int j = 0;
        if (*p == '"' || *p == '\'') { q = *p; quoted = 1; p++; }
        while (*p && j < 511) {
            if (quoted && *p == q) { p++; break; }
            if (!quoted && isspace((unsigned char)*p)) break;
            toks[nt][j++] = *p++;
        }
        toks[nt][j] = 0; nt++;
    }
    char opts[4096]; int olen = 0; opts[0] = 0;
    #define JADD(kv) do { if (olen) opts[olen++] = ','; olen += snprintf(opts + olen, sizeof(opts) - olen, "%s", (kv)); } while (0)
    const char* pattern = NULL; const char* path = ".";
    char buf[1024];
    for (int i = 0; i < nt; i++) {
        const char* t = toks[i];
        if (strcmp(t, "-F") == 0 || strcasecmp(t, "--fixed") == 0) JADD("\"fixed\":true");
        else if (strcmp(t, "-i") == 0) JADD("\"ignore_case\":true");
        else if (strcmp(t, "-w") == 0) JADD("\"word\":true");
        else if (strcmp(t, "-v") == 0) JADD("\"invert\":true");
        else if (strcmp(t, "-r") == 0 || strcmp(t, "-R") == 0) JADD("\"recursive\":true");
        else if (strcmp(t, "-n") == 0) JADD("\"line_number\":true");
        else if (strcmp(t, "-l") == 0) JADD("\"files_with_matches\":true");
        else if (strcmp(t, "-c") == 0) JADD("\"count_only\":true");
        else if (strcmp(t, "-o") == 0) JADD("\"only_matching\":true");
        else if (strcmp(t, "-.") == 0) JADD("\"hidden\":true");
        else if (strcmp(t, "--type") == 0 && i + 1 < nt) { snprintf(buf, sizeof(buf), "\"type\":\"%s\"", toks[++i]); JADD(buf); }
        else if (strcmp(t, "--include") == 0 && i + 1 < nt) { snprintf(buf, sizeof(buf), "\"include\":\"%s\"", toks[++i]); JADD(buf); }
        else if (strcmp(t, "--exclude") == 0 && i + 1 < nt) { snprintf(buf, sizeof(buf), "\"exclude\":\"%s\"", toks[++i]); JADD(buf); }
        else if (strcmp(t, "--depth") == 0 && i + 1 < nt) { snprintf(buf, sizeof(buf), "\"max_depth\":%d", atoi(toks[++i])); JADD(buf); }
        else if (strcmp(t, "-m") == 0 && i + 1 < nt) { snprintf(buf, sizeof(buf), "\"max_count\":%d", atoi(toks[++i])); JADD(buf); }
        else if (strcmp(t, "-A") == 0 && i + 1 < nt) { snprintf(buf, sizeof(buf), "\"context_after\":%d", atoi(toks[++i])); JADD(buf); }
        else if (strcmp(t, "-B") == 0 && i + 1 < nt) { snprintf(buf, sizeof(buf), "\"context_before\":%d", atoi(toks[++i])); JADD(buf); }
        else if (strcmp(t, "-C") == 0 && i + 1 < nt) { int cv = atoi(toks[++i]); snprintf(buf, sizeof(buf), "\"context_before\":%d", cv); JADD(buf); snprintf(buf, sizeof(buf), "\"context_after\":%d", cv); JADD(buf); }
        else if (!pattern) pattern = t;
        else path = t;
    }
    if (!pattern) {
        fprintf(stderr, "uso: GREP [-rinlcFwvo-.] [--type EXT] [--include G] [--exclude G] [--depth N] PATTERN [PATH]\n");
        return;
    }
    char pj[2048]; int k = 0;
    for (const char* s = pattern; *s && k < (int)sizeof(pj) - 4; s++) {
        if (*s == '"' || *s == '\\') pj[k++] = '\\';
        pj[k++] = *s;
    }
    pj[k] = 0;
    snprintf(buf, sizeof(buf), "\"pattern\":\"%s\"", pj); JADD(buf);
    opts[olen++] = '}'; opts[olen] = 0;
    char full[4200]; snprintf(full, sizeof(full), "{%s}", opts);
    char* hits = gmb_grep_search_json(full, path);
    char* txt = gmb_grep_format_json(hits, 1, 1);
    if (txt && txt[0]) printf("%s", txt);
    gmb_grep_free(hits);
    gmb_grep_free(txt);
}

/* Esegue UNA riga di comando (usata dal loop interattivo e da BATCH). * Ritorna 1 se QUIT/EXIT, 0 altrimenti. */
static int rpl_exec_line(const char* line, const rpl_options_t* o, char* prompt)
{
    do {
        char cmd[64];
        const char* args = "";
        int i = 0;
        while (line[i] && !isspace((unsigned char)line[i])) i++;
        int cl = i < (int)sizeof(cmd) - 1 ? i : (int)sizeof(cmd) - 1;
        memcpy(cmd, line, cl); cmd[cl] = 0;
        while (line[i] && isspace((unsigned char)line[i])) i++;
        args = line + i;

        /* Riga di programma numerata (parità ReplEngine.cs ProcessReplInput): se il primo
         * token è un intero, imposta/rimuove la riga nel buffer. "10 PRINT ..." imposta,
         * "10" da solo rimuove. */
        if (cmd[0] && strspn(cmd, "0123456789") == strlen(cmd)) {
            int num = atoi(cmd);
            if (num > 0) {
                if (*args == 0) rpl_editor_remove_line(num);
                else rpl_editor_set_line(num, args);
            }
            goto done;
        }

        if (strcasecmp(cmd, "QUIT") == 0 || strcasecmp(cmd, "EXIT") == 0) return 1;
        if (strcasecmp(cmd, "CLS") == 0) { printf("\033[2J\033[H"); goto done; }
        if (strcasecmp(cmd, "PROMPT") == 0) { const char* t = args; while (*t == ' ') t++; snprintf(prompt, sizeof(prompt), "%s ", *t ? t : "gm"); goto done; }
        if (strcasecmp(cmd, "HELP") == 0) {
            char topic[64] = "";
            sscanf(args, "%63s", topic);
            /* Parità con la REPL C# (HandleHelpAsync): `HELP` senza argomento apre il
             * BROWSER interattivo. `HELP COMMANDS` stampa l'elenco dei comandi,
             * `HELP <topic>` la guida del namespace. */
            if (!*topic || strcasecmp(topic, "BROWSE") == 0 || strcasecmp(topic, "ALL") == 0) {
                /* Console a FINESTRA (SDL/C64): niente terminale per il browser a schermo
                 * intero → si scrive l'indice nel buffer a celle e si presenta. */
                gmb_iconsole_t* active = gmb_iconsole_active();
                gmb_sdl_console_t* win = active ? gmb_iconsole_sdl_model(active) : NULL;
                if (win) gmb_help_print_index(win);
                else gmb_help_browse();
                goto done;
            }
            if (strcasecmp(topic, "COMMANDS") == 0) {
                printf("Comandi: QUIT EXIT CLS PROMPT HELP LIST EDIT NEW AUTO SAVE LOAD DELETE DEL RENUM\n"
                       "  SAMPLE COMPILE BSP RUN EXEC LOADPRG PRGINFO AST DASM IL GPM LS CD WHERE WH\n"
                       "  GREP (RG) RN RENAME RM DL DELF CP COPY DP DUPLICATE BL BACKUP DEPLOY CONVERT BATCH CONVFMT\n"
                       "  WEB2MD PROVIDERS DEP PACKAGE HF MLX ML AIPATH SET STATUS DEBUG BREAK CLEAR STEP\n"
                       "  CONT TRACE WATCH UNWATCH PALETTE\n"
                       "GREP [-rinlcFwvo-.] [--type EXT] [--include G] [--exclude G] [--depth N] PATTERN [PATH]\n"
                       "HELP = guida interattiva · HELP <topic> = namespace (shared/helps/<topic>.json)\n"
                       "HELP COMMANDS = questo elenco\n");
            } else rpl_help_show(topic);
            goto done;
        }
        if (rpl_is_dir_command(cmd)) { system("ls -la"); goto done; }
        if (strcasecmp(cmd, "CD") == 0) { if (*args) { if (chdir(args) != 0) fprintf(stderr, "?%s\n", args); } else { char cwd[1024]; getcwd(cwd, sizeof(cwd)); printf("%s\n", cwd); } goto done; }
        if (strcasecmp(cmd, "WHERE") == 0) { printf("%s\n", getcwd(line, sizeof(line))); goto done; }
        if (strcasecmp(cmd, "GREP") == 0 || strcasecmp(cmd, "RG") == 0) { rpl_grep(args); goto done; }
        if (strcasecmp(cmd, "STATUS") == 0) {
            printf("backend=%s mode=%s grid=%dx%d c64=%d sdl=%d\n", o->backend == RPL_VMC ? "vmc" : "c99", o->gpm_mode, o->grid_width, o->grid_height, o->use_c64_mode, o->use_sdl);
            printf("debug=%s trace=%s breakpoints=%d watch=%d\n",
                g_dbg.mode == GMB_DBG_OFF ? "OFF" : g_dbg.mode == GMB_DBG_STEPPING ? "STEPPING" : "RUNNING",
                g_dbg.trace ? "ON" : "OFF", g_dbg.break_count, g_dbg.watch_count);
            printf("palette=%s (SET: %s)\n", g_pal_fg[2] ? "attiva" : "default",
                g_settings.set_theme[0] ? g_settings.set_theme : "(default)");
            printf("settings: bell=%s confirm=%s cursor=%s century=%s wrap=%s escape=%s dateformat=%s\n",
                g_settings.bell ? "ON" : "OFF", g_settings.confirm ? "ON" : "OFF",
                g_settings.cursor ? "ON" : "OFF", g_settings.century ? "ON" : "OFF",
                g_settings.wrap ? "ON" : "OFF", g_settings.escape ? "ON" : "OFF",
                g_settings.dateformat[0] ? g_settings.dateformat : "(default)");
            goto done;
        }
        if (strcasecmp(cmd, "PALETTE") == 0) {
            char pname[64] = "";
            sscanf(args, "%63s", pname);
            if (!*pname) {
                printf("palette disponibili:\n");
                for (int i = 0; gmb_palettes[i].name; i++) printf("  %s\n", gmb_palettes[i].name);
            } else if (strcasecmp(pname, "OFF") == 0) {
                g_pal_fg = "\x1b[0m"; g_pal_bg = "\x1b[49m";
                printf("%s(PALETTE: default)%s\n", g_pal_fg, "\x1b[0m");
            } else gmb_palette_apply(pname);
            goto done;
        }
        if (strcasecmp(cmd, "SET") == 0) {
            char opt[64] = "", val[64] = "";
            sscanf(args, "%63s %63s", opt, val);
            int need_save = 0;
            if (*opt == 0) {
                printf("BELL=%s CONFIRM=%s CURSOR=%s CENTURY=%s WRAP=%s ESCAPE=%s\n",
                    g_settings.bell ? "ON" : "OFF", g_settings.confirm ? "ON" : "OFF",
                    g_settings.cursor ? "ON" : "OFF", g_settings.century ? "ON" : "OFF",
                    g_settings.wrap ? "ON" : "OFF", g_settings.escape ? "ON" : "OFF");
                printf("THEME=%s DATEFORMAT=%s\n", *g_settings.set_theme ? g_settings.set_theme : "(default)",
                       *g_settings.dateformat ? g_settings.dateformat : "(default)");
            } else if (strcasecmp(opt, "BELL") == 0) { g_settings.bell = *val ? strcasecmp(val, "ON") == 0 : !g_settings.bell; need_save = 1; }
            else if (strcasecmp(opt, "CONFIRM") == 0) { g_settings.confirm = *val ? strcasecmp(val, "ON") == 0 : !g_settings.confirm; need_save = 1; }
            else if (strcasecmp(opt, "CURSOR") == 0) { g_settings.cursor = *val ? strcasecmp(val, "ON") == 0 : !g_settings.cursor; need_save = 1; }
            else if (strcasecmp(opt, "CENTURY") == 0) { g_settings.century = *val ? strcasecmp(val, "ON") == 0 : !g_settings.century; need_save = 1; }
            else if (strcasecmp(opt, "WRAP") == 0) { g_settings.wrap = *val ? strcasecmp(val, "ON") == 0 : !g_settings.wrap; need_save = 1; }
            else if (strcasecmp(opt, "ESCAPE") == 0) { g_settings.escape = *val ? strcasecmp(val, "ON") == 0 : !g_settings.escape; need_save = 1; }
            else if (strcasecmp(opt, "THEME") == 0) { if (*val) snprintf(g_settings.set_theme, sizeof(g_settings.set_theme), "%s", val); else g_settings.set_theme[0] = 0; need_save = 1; if (*val) gmb_palette_apply(val); }
            else if (strcasecmp(opt, "DATEFORMAT") == 0) { if (*val) snprintf(g_settings.dateformat, sizeof(g_settings.dateformat), "%s", val); else g_settings.dateformat[0] = 0; need_save = 1; }
            else { fprintf(stderr, "?SET: opzione sconosciuta %s (BELL/CONFIRM/CURSOR/CENTURY/THEME/DATEFORMAT/WRAP/ESCAPE)\n", opt); }
            if (need_save) { rpl_settings_save(g_settings_dir[0] ? g_settings_dir : NULL, &g_settings); printf("(SET salvato)\n"); }
            goto done;
        }
        if (strcasecmp(cmd, "RUN") == 0 || strcasecmp(cmd, "EXEC") == 0) {
            char file[1024];
            snprintf(file, sizeof(file), "%s", args);
            while (*file == ' ') memmove(file, file + 1, strlen(file));
            /* Parità ReplEngine.HandleRun: senza file esegue il PROGRAMMA IN MEMORIA
             * (se non vuoto); altrimenti esegue il file indicato. */
            if (!*file) {
                if (rpl_editor_count() > 0) {
                    snprintf(file, sizeof(file), "/tmp/gmb_repl_%ld.bas", (long)getpid());
                    FILE* tf = fopen(file, "w");
                    if (!tf) { fprintf(stderr, "?RUN: impossibile creare il file temporaneo\n"); goto done; }
                    fputs(rpl_editor_source(), tf);
                    fclose(tf);
                } else {
                    fprintf(stderr, "?RUN: nessun programma in memoria e nessun file indicato\n");
                    goto done;
                }
            }
            if (getenv("GMBASIC_DEBUG")) fprintf(stderr, "[RUN] cmd='%s' args='%s' file='%s'\n", cmd, args, file);
            if (o->backend == RPL_VMC) { int rc = rpl_run_vmc(file, o); if (rc != 0) printf("[VMC ERROR] exit %d\n", rc); }
            else { int rc = rpl_run_c99(file, o); printf(rc == 0 ? "[C99 OK]\n" : "[C99 ERROR] exit %d\n", rc); }
            goto done;
        }
        if (strcasecmp(cmd, "LIBS") == 0) {
            /* Comando LIBS — bundle delle librerie native di supporto (gruppo B) per GMBasicModules.
             * Esegue il tool GMBasic tools/gmb_libs_update.bas (zip+sha per RID). La scrittura del
             * manifest e il publish (gh) sono nel CLI C# `gmb libs update|publish`. */
            char sub[64];
            snprintf(sub, sizeof(sub), "%s", args);
            while (*sub == ' ') memmove(sub, sub + 1, strlen(sub));
            if (!*sub) {
                printf("  LIBS UPDATE    bundle di supporto (tools/gmb_libs_update.bas): zip+sha per RID\n");
                printf("  LIBS           (questo help)\n");
                printf("  NB: manifest + publish via CLI C# `gmb libs update|check|publish`\n");
                goto done;
            }
            if (o->backend == RPL_VMC) { int rc = rpl_run_vmc("tools/gmb_libs_update.bas", o); if (rc != 0) printf("[VMC ERROR] exit %d\n", rc); }
            else { int rc = rpl_run_c99("tools/gmb_libs_update.bas", o); printf(rc == 0 ? "[C99 OK]\n" : "[C99 ERROR] exit %d\n", rc); }
            goto done;
        }
        if (strcasecmp(cmd, "COMPILE") == 0) {
            char out[1024];
            snprintf(out, sizeof(out), "/tmp/gmb_compile_%ld.%s", (long)getpid(), o->backend == RPL_VMC ? "chunk" : "c");
            int rc = rpl_compile_csharp(args, out, o->backend, o);
            printf(rc == 0 ? "compilato -> %s\n" : "?compilazione fallita (%d)\n", out, rc);
            goto done;
        }
        if (strcasecmp(cmd, "LIST") == 0) { rpl_editor_list(); goto done; }
        if (strcasecmp(cmd, "NEW") == 0) { rpl_editor_new(); goto done; }
        if (strcasecmp(cmd, "AUTO") == 0) { rpl_editor_auto(atoi(args)); goto done; }
        if (strcasecmp(cmd, "LOAD") == 0) { rpl_editor_load(args); printf("caricato %d righe\n", rpl_editor_count()); goto done; }
        if (strcasecmp(cmd, "IDE") == 0) {
            char idefile[1024];
            snprintf(idefile, sizeof(idefile), "%s", *args ? args : (o->file_name ? o->file_name : "untitled.bas"));
            gmb_ide_run(idefile);
            goto done;
        }
        if (strcasecmp(cmd, "EDIT") == 0) {
            /* Parità REPL C#: EDIT apre l'IDE a schermo intero (stessa superficie di comandi). */
            char ef[1024];
            snprintf(ef, sizeof(ef), "%s", *args ? args : (o->file_name ? o->file_name : "untitled.bas"));
            gmb_ide_run(ef);
            goto done;
        }
        if (strcasecmp(cmd, "SAVE") == 0) { rpl_editor_save(*args ? args : (o->file_name ? o->file_name : "program.bas")); printf("salvato\n"); goto done; }
        if (strcasecmp(cmd, "DELETE") == 0 || strcasecmp(cmd, "DEL") == 0) { rpl_editor_delete(args); goto done; }
        if (strcasecmp(cmd, "RENUM") == 0) { rpl_editor_renum(); goto done; }
        if (strcasecmp(cmd, "SAMPLE") == 0) {
            char filter[512] = "";
            sscanf(args, "%511s", filter);
            char sampdir[2048];
            resolve_repo_path(sampdir, sizeof(sampdir), "samples");
            char findcmd[4096];
            snprintf(findcmd, sizeof(findcmd), "find \"%s\" -name '*.bas' -type f 2>/dev/null | head -n 200", sampdir);
            FILE* fp = popen(findcmd, "r");
            if (!fp) { fprintf(stderr, "?SAMPLE: find fallito\n"); goto done; }
            const char* items[200];
            char paths[200][1024];
            int n = 0;
            char line[2048];
            while (fgets(line, sizeof(line), fp) && n < 200) {
                line[strcspn(line, "\r\n")] = 0;
                if (!line[0]) continue;
                if (filter[0] && strcasestr(line, filter) == NULL) {
                    char* base = strrchr(line, '/');
                    if (!base || strcasestr(base, filter) == NULL) continue;
                }
                const char* rel = line + strlen(sampdir);
                while (*rel == '/') rel++;
                char desc[1024];
                snprintf(desc, sizeof(desc), "%s", rel);
                // prova a leggere descrizione dalla prima riga ' commento
                FILE* sf = fopen(line, "r");
                if (sf) {
                    char first[1024];
                    if (fgets(first, sizeof(first), sf)) {
                        char* p = first;
                        while (*p == ' ' || *p == '\t' || *p == '\'' || *p == '-') p++;
                        if (*p) {
                            char* nl = strchr(p, '\r'); if (nl) *nl=0;
                            nl = strchr(p, '\n'); if (nl) *nl=0;
                            if (strlen(p) > 3) snprintf(desc, sizeof(desc), "%s — %.80s", rel, p);
                        }
                    }
                    fclose(sf);
                }
                items[n] = strdup(desc);
                snprintf(paths[n], sizeof(paths[n]), "%s", line);
                n++;
            }
            pclose(fp);
            if (n == 0) { printf("(nessun sample%s%s)\n", filter[0] ? " per '" : "", filter[0] ? filter : ""); if (filter[0]) printf("')\n"); goto done; }
            int sel = gmb_menu_show(items, n, filter[0] ? filter : "Samples", true);
            if (sel >= 0 && sel < n) {
                printf("Selezionato: %s\n", paths[sel]);
                char loadcmd[2048];
                snprintf(loadcmd, sizeof(loadcmd), "LOAD %s", paths[sel]);
                rpl_exec_line(loadcmd, o, prompt);
            } else printf("(annullato)\n");
            for (int i = 0; i < n; i++) free((void*)items[i]);
            goto done;
        }
        if (strcasecmp(cmd, "RN") == 0 || strcasecmp(cmd, "RENAME") == 0 || strcasecmp(cmd, "RM") == 0) {
            char old[512], neu[512];
            if (sscanf(args, "%511s %511s", old, neu) == 2) { if (rename(old, neu) != 0) fprintf(stderr, "?rename fallita\n"); } else fprintf(stderr, "?uso: %s <vecchio> <nuovo>\n", cmd);
            goto done;
        }
        if (strcasecmp(cmd, "CP") == 0 || strcasecmp(cmd, "COPY") == 0) {
            char src[512], dst[512];
            if (sscanf(args, "%511s %511s", src, dst) == 2) {
                char c[4096]; snprintf(c, sizeof(c), "cp \"%s\" \"%s\"", src, dst); system(c);
            } else fprintf(stderr, "?uso: %s <src> <dst>\n", cmd);
            goto done;
        }
        if (strcasecmp(cmd, "DP") == 0 || strcasecmp(cmd, "DUPLICATE") == 0) {
            char src[512], dst[512];
            if (sscanf(args, "%511s %511s", src, dst) == 2) { char c[4096]; snprintf(c, sizeof(c), "cp \"%s\" \"%s\"", src, dst); system(c); }
            else fprintf(stderr, "?uso: %s <src> <dst>\n", cmd);
            goto done;
        }
        if (strcasecmp(cmd, "DL") == 0 || strcasecmp(cmd, "DELF") == 0) { if (remove(args) != 0) fprintf(stderr, "?rimozione fallita: %s\n", args); goto done; }
        if (strcasecmp(cmd, "BL") == 0 || strcasecmp(cmd, "BACKUP") == 0) {
            char c[4096]; snprintf(c, sizeof(c), "cp \"%s\" \"%s.bak\"", args, args); system(c); goto done;
        }
        if (strcasecmp(cmd, "BSP") == 0) {
            fprintf(stderr, "?BSP non ancora disponibile nel percorso C — usa la REPL C# (REPL/Program.cs --bsp)\n");
            goto done;
        }
        if (strcasecmp(cmd, "CONVERT") == 0 || strcasecmp(cmd, "CONVFMT") == 0) {
            fprintf(stderr, "?%s non ancora disponibile nel percorso C — usa la REPL C#\n", cmd);
            goto done;
        }
        if (strcasecmp(cmd, "PACKAGE") == 0) { gmb_packages_dispatch(args); goto done; }
        if (strcasecmp(cmd, "ML") == 0 || strcasecmp(cmd, "MLX") == 0) {
            char sub[64] = "";
            sscanf(args, "%63s", sub);
            if (strcasecmp(sub, "STATUS") == 0) gmb_ml_status();
            else if (strcasecmp(sub, "RM") == 0) gmb_ml_rm();
            else gmb_ml_setup(strcasestr(args, "--force") != NULL);
            goto done;
        }
        if (strcasecmp(cmd, "PROVIDERS") == 0) {
            char cfgpath[1024];
            { char cd[1024]; gmb_paths_config_dir(cd, sizeof(cd)); snprintf(cfgpath, sizeof(cfgpath), "%s/ai.json", cd); }
            FILE* cf = fopen(cfgpath, "r");
            if (cf) {
                char buf[8192]; size_t n = fread(buf, 1, sizeof(buf)-1, cf); buf[n]=0; fclose(cf);
                printf("Providers (%s):\n", cfgpath);
                const char* p = buf;
                while ((p = strstr(p, "\"name\"")) != NULL) {
                    const char* c = strchr(p, ':'); if (!c) break;
                    const char* q1 = strchr(c, '"'); if (!q1) break;
                    const char* q2 = strchr(q1+1, '"'); if (!q2) break;
                    char name[64]; size_t nl = q2 - q1 - 1; if (nl >= sizeof(name)) nl = sizeof(name)-1; memcpy(name, q1+1, nl); name[nl]=0;
                    printf("  %-20s (configurato)\n", name);
                    p = q2+1;
                }
            } else {
                struct { const char* name; const char* src; } prov[] = {
                    {"llamacpp", "../LLMEnv/LLMEngine/llama/llmengine_llama.c"},
                    {"ocr (Vision)", "../LLMEnv/LLMEngine/llama/llmengine_llama_ocr.c"},
                    {"mlx", "../LLMEnv/LLMEngine/mlx"},
                    {"tokenizer BPE", "../LLMEnv/LLMToken"},
                    {NULL, NULL}
                };
                printf("Providers (sorgente, config assente %s):\n", cfgpath);
                for (int i = 0; prov[i].name; i++) {
                    char path[2048];
                    resolve_repo_path(path, sizeof(path), prov[i].src);
                    printf("  %-16s %s\n", prov[i].name, access(path, F_OK) == 0 ? "sorgente OK" : "MANCANTE");
                }
            }
            { char mlc[1024], libs[1024], cfgd[1024];
              gmb_paths_ml_dir("", mlc, sizeof(mlc)); gmb_paths_libs_dir(libs, sizeof(libs)); gmb_paths_config_dir(cfgd, sizeof(cfgd));
              printf("  (runtime: %s, %s; libs in %s)\n", cfgd, mlc, libs); }
            goto done;
        }
        if (strcasecmp(cmd, "AIPATH") == 0) {
            char* args2 = (char*)args;
            while (*args2 == ' ') args2++;
            if (*args2) {
                // AIPATH=<paths> -> imposta (parità C# AIPATH=path1;path2;)
                char* eq = strchr(args2, '=');
                if (eq) args2 = eq+1;
                // Salva in config (semplificato: stampa e suggerisce)
                printf("AIPATH impostato a: %s\n", args2);
                { char cd[1024]; gmb_paths_config_dir(cd, sizeof(cd)); printf("  (persistenza: salva manualmente in %s/ai.json -> \"modelPaths\": [\"%s\"])\n", cd, args2); }
            } else {
                char cfgd[1024], mlc[1024];
                gmb_paths_config_dir(cfgd, sizeof(cfgd)); gmb_paths_ml_dir("", mlc, sizeof(mlc));
                printf("  AI cache: %s\n", cfgd);
                printf("  ML cache: %s\n", mlc);
                printf("  Libs native: Resources/libs (AI: gmb_host_ai*, llmtoken, gmb_torch)\n");
                char cfg2[1024]; snprintf(cfg2, sizeof(cfg2), "%s/ai.json", cfgd);
                FILE* cf2 = fopen(cfg2, "r");
                if (cf2) {
                    char buf2[4096]; size_t n2 = fread(buf2, 1, sizeof(buf2)-1, cf2); buf2[n2]=0; fclose(cf2);
                    const char* mp = strstr(buf2, "modelPaths");
                    if (mp) {
                        printf("  Config modelPaths: ");
                        const char* b = strchr(mp, '['); const char* e = b ? strchr(b, ']') : NULL;
                        if (b && e) { char tmp[1024]; size_t l = e-b-1; if (l >= sizeof(tmp)) l = sizeof(tmp)-1; memcpy(tmp, b+1, l); tmp[l]=0; printf("%s\n", tmp); }
                        else printf("(non parsabile)\n");
                    }
                }
            }
            goto done;
        }
        if (strcasecmp(cmd, "WEB2MD") == 0) {
            char url[1024] = "", outfile[1024] = "";
            sscanf(args, "%1023s %1023s", url, outfile);
            if (!*url) { fprintf(stderr, "?uso: WEB2MD <url> [out.md]\n"); goto done; }
            char tmp[1024];
            snprintf(tmp, sizeof(tmp), "/tmp/gmb_web2md_%ld.html", (long)getpid());
            char curl[2048];
            snprintf(curl, sizeof(curl), "curl -s -L \"%s\" -o \"%s\"", url, tmp);
            if (system(curl) != 0) { fprintf(stderr, "?WEB2MD: curl fallito\n"); goto done; }
            FILE* in = fopen(tmp, "r");
            if (!in) { fprintf(stderr, "?WEB2MD: nessun contenuto\n"); goto done; }
            char* out = outfile[0] ? outfile : "web2md.txt";
            FILE* fout = fopen(out, "w");
            if (!fout) { fclose(in); fprintf(stderr, "?WEB2MD: impossibile scrivere %s\n", out); goto done; }
            /* html -> testo: salta tag, decodifica entità base */
            int ch, skip = 0, prev = 0;
            while ((ch = fgetc(in)) != EOF) {
                if (ch == '<') { skip = 1; continue; }
                if (ch == '>') { skip = 0; prev = 0; continue; }
                if (skip) continue;
                if (ch == '&') { prev = ch; continue; }
                if (prev == '&') {
                    char ent[8]; int i = 0; ent[i++] = ch;
                    while (i < 6 && (ch = fgetc(in)) != EOF && ch != ';') ent[i++] = (char)ch;
                    if (ch == ';') {
                        if (strncmp(ent, "amp", 3) == 0) fputc('&', fout);
                        else if (strncmp(ent, "lt", 2) == 0) fputc('<', fout);
                        else if (strncmp(ent, "gt", 2) == 0) fputc('>', fout);
                        else if (strncmp(ent, "quot", 4) == 0) fputc('"', fout);
                        prev = 0; continue;
                    } else { fputc('&', fout); for (int k = 0; k < i; k++) fputc(ent[k], fout); }
                }
                fputc(ch, fout); prev = ch;
            }
            fclose(in); fclose(fout);
            unlink(tmp);
            printf("(WEB2MD -> %s)\n", out);
            goto done;
        }
        if (strcasecmp(cmd, "BATCH") == 0) {
            char bfile[512];
            if (sscanf(args, "%511s", bfile) != 1) { fprintf(stderr, "?uso: BATCH <file>\n"); goto done; }
            FILE* bf = fopen(bfile, "r");
            if (!bf) { fprintf(stderr, "?BATCH: file non trovato: %s\n", bfile); goto done; }
            char bline[1024];
            while (fgets(bline, sizeof(bline), bf)) {
                char* be = bline + strlen(bline);
                while (be > bline && (be[-1] == '\n' || be[-1] == '\r')) *--be = 0;
                if (!*bline || *bline == '\\' || *bline == '#') continue;
                if (rpl_exec_line(bline, o, prompt)) { fclose(bf); return 1; }
            }
            fclose(bf);
            printf("(BATCH completato)\n");
            goto done;
        }
        if (strcasecmp(cmd, "HF") == 0) {
            fprintf(stderr, "?HF (HuggingFace download) non ancora disponibile nel percorso C — usa la REPL C#\n");
            goto done;
        }
        if (strcasecmp(cmd, "RAG") == 0) {
            /* RAG richiede SQLite+embedding: percorso C# (REPL C#/CLI gmb rag ...), non ancora
             * portato nel percorso C. Errore esplicito di divergenza (mai stub). */
            fprintf(stderr, "?RAG non ancora disponibile nel percorso C — usa la REPL C# (comando RAG) o `gmb rag ...`.\n");
            goto done;
        }
        if (strcasecmp(cmd, "GRAPH") == 0) {
            /* GRAPH BUILD in puro C via gmb_graph (runtime condiviso): nodi=.md, edge=wikilink
             * [[...]] risolti per nome. Sottocome GRAPH STATUS. Implementazione C completa. */
            char sub[32] = "";
            char wiki[1024] = "";
            sscanf(args, "%31s %1023s", sub, wiki);
            if (strcasecmp(sub, "BUILD") == 0 && wiki[0]) rpl_graph_build(wiki);
            else if (strcasecmp(sub, "STATUS") == 0) printf("GRAPH STATUS (C): %s\n", wiki[0] ? wiki : "(nessun wiki)"); 
            else fprintf(stderr, "?uso: GRAPH BUILD <wikiDir>  |  GRAPH STATUS\n");
            goto done;
        }
        if (strcasecmp(cmd, "DEPLOY") == 0 || strcasecmp(cmd, "DEP") == 0) {
            char src[512], dst[512];
            if (sscanf(args, "%511s %511s", src, dst) == 2) { char c[4096]; snprintf(c, sizeof(c), "cp -r \"%s\" \"%s\"", src, dst); system(c); }
            else fprintf(stderr, "?uso: %s <src> <dst>\n", cmd);
            goto done;
        }
        if (strcasecmp(cmd, "GPM") == 0) {
            char m[32] = "";
            sscanf(args, "%31s", m);
            if (m[0]) {
                if (strcasecmp(m, "ALWAYS") == 0 || strcasecmp(m, "LINEAR") == 0 || strcasecmp(m, "NEVER") == 0) {
                    /* o->gpm_mode è const char*: teniamo un buffer statico */
                    static char gpm_buf[32];
                    snprintf(gpm_buf, sizeof(gpm_buf), "%s", m);
                    *(const char**)&o->gpm_mode = gpm_buf;
                    printf("GPM mode=%s (usato dal backend C99; vmc usa il runner GPM di default)\n", o->gpm_mode);
                } else fprintf(stderr, "?GPM: modo sconosciuto %s (Always|Linear|Never)\n", m);
            } else printf("GPM mode=%s (Always|Linear|Never)\n", o->gpm_mode);
            goto done;
        }
        if (strcasecmp(cmd, "AST") == 0) {
            /* AST [ALL|ERR|STOP] | AST <file> | AST <filtro> — parità ReplEngine.cs:1913 */
            char aarg[1024];
            snprintf(aarg, sizeof(aarg), "%s", args);
            char* ap = aarg; while (*ap == ' ') ap++;
            if (!*ap) { fprintf(stderr, "?uso: AST <file.bas> | AST ALL|ERR|STOP\n"); goto done; }
            char a1[64] = ""; sscanf(ap, "%63s", a1);
            if (strcasecmp(a1, "ALL") == 0 || strcasecmp(a1, "ERR") == 0 || strcasecmp(a1, "STOP") == 0) {
                int show_ok = strcasecmp(a1, "ERR") != 0;
                int stop_on_err = strcasecmp(a1, "STOP") == 0;
                char sampdir[2048]; resolve_repo_path(sampdir, sizeof(sampdir), "samples");
                char findcmd[4096]; snprintf(findcmd, sizeof(findcmd), "find \"%s\" -name '*.bas' -type f 2>/dev/null", sampdir);
                FILE* fp = popen(findcmd, "r");
                if (!fp) { fprintf(stderr, "?AST: find fallito\n"); goto done; }
                char line[2048]; int ok=0, fail=0;
                while (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\r\n")] = 0; if (!line[0]) continue;
                    FILE* f = fopen(line, "rb"); if (!f) continue;
                    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
                    char* src = (char*)malloc(len+1); fread(src,1,len,f); src[len]=0; fclose(f);
                    gmb_lex_result_t lx = gmb_lexer_scan(src, len);
                    gmb_lex_result_t lx2 = gmb_macro_expand(&lx); lx.owned_src=NULL;
                    char* perr=NULL; gmb_program_t prog = gmb_parse(&lx2, &perr);
                    int is_ok = (perr==NULL);
                    if (is_ok && show_ok) { printf("."); fflush(stdout); }
                    if (!is_ok) { fail++; printf("\n  ERRORE: %s - %s\n", line, perr); free(perr); if (stop_on_err) { gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src); break; } }
                    else ok++;
                    gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
                    if (perr) free(perr);
                }
                pclose(fp);
                printf("\nAST %s: OK=%d FAIL=%d\n", a1, ok, fail);
                goto done;
            }
            char afile[1024];
            snprintf(afile, sizeof(afile), "%s", ap);
            size_t al = strlen(afile);
            while (al > 0 && (afile[0] == '"' || afile[0] == '\'')) { memmove(afile, afile + 1, al); al--; }
            while (al > 0 && (afile[al - 1] == '"' || afile[al - 1] == '\'')) afile[--al] = 0;
            // filtro SAMPLE: se non è un file ma un filtro, cerca nel catalogo
            if (access(afile, F_OK) != 0) {
                char sampdir[2048]; resolve_repo_path(sampdir, sizeof(sampdir), "samples");
                char findcmd2[4096]; snprintf(findcmd2, sizeof(findcmd2), "find \"%s\" -name '*.bas' -type f 2>/dev/null | xargs grep -l \"%s\" 2>/dev/null | head -n 1", sampdir, afile);
                FILE* fp2 = popen(findcmd2, "r");
                if (fp2) {
                    char found[2048]; if (fgets(found, sizeof(found), fp2)) { found[strcspn(found, "\r\n")]=0; snprintf(afile, sizeof(afile), "%s", found); }
                    pclose(fp2);
                }
            }
            FILE* af = fopen(afile, "rb");
            if (!af) { fprintf(stderr, "?AST: file non trovato: %s\n", afile); goto done; }
            fseek(af, 0, SEEK_END); long alen = ftell(af); fseek(af, 0, SEEK_SET);
            char* asrc = (char*)malloc((size_t)alen + 1);
            fread(asrc, 1, (size_t)alen, af); asrc[alen] = 0; fclose(af);
            gmb_lex_result_t lx = gmb_lexer_scan(asrc, alen);
            gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
            lx.owned_src = NULL;
            char* perr = NULL;
            gmb_program_t prog = gmb_parse(&lx2, &perr);
            if (perr) { fprintf(stderr, "?AST parse error: %s\n", perr); free(perr); }
            else {
                // dump con paginazione se output grande
                char* dump = NULL; size_t dlen = 0;
                FILE* mem = open_memstream(&dump, &dlen);
                gmb_ast_dump(prog.stmts, prog.n, mem);
                gmb_link_result_t lr;
                gmb_link_resolve(&lr, prog.stmts, prog.n);
                fprintf(mem, "; Simboli: %d (undefined %d, duplicati %d)\n", lr.count, lr.undefined_count, lr.duplicate_count);
                if (lr.error) fprintf(mem, "; Errore linker: %s (riga %d)\n", lr.error, lr.error_line);
                for (int i = 0; i < lr.count; i++)
                    fprintf(mem, ";   %s %s (riga %d)%s\n", lr.syms[i].kind == GMB_SYM_FUNCTION ? "FUNCTION" : "VAR",
                           lr.syms[i].name, lr.syms[i].decl_line, lr.syms[i].used ? "" : " [non usato]");
                gmb_link_free(&lr);
                fclose(mem);
                if (dlen > 2000) {
                    const char* lines[512]; int n=0;
                    char* p = dump; while (*p && n < 512) { lines[n++]=p; char* nl=strchr(p,'\n'); if(!nl) break; *nl=0; p=nl+1; }
                    // usa menu paginato se disponibile, altrimenti stampa diretto
                    printf("%s\n", dump);
                } else {
                    printf("; AST di %s (%d statement)\n%s", afile, prog.n, dump);
                }
                free(dump);
            }
            gmb_program_free(&prog);
            gmb_lex_result_free(&lx2);
            gmb_lex_result_free(&lx);
            free(asrc);
            goto done;
        }
        if (strcasecmp(cmd, "PRGINFO") == 0) {
            char a1[64] = ""; sscanf(args, "%63s", a1);
            if (strcasecmp(a1, "ALL") == 0 || strcasecmp(a1, "ERR") == 0 || strcasecmp(a1, "STOP") == 0) {
                int show_ok = strcasecmp(a1, "ERR") != 0;
                int stop_on_err = strcasecmp(a1, "STOP") == 0;
                char sampdir[2048]; resolve_repo_path(sampdir, sizeof(sampdir), "samples");
                char findcmd[4096]; snprintf(findcmd, sizeof(findcmd), "find \"%s\" -name '*.bas' -type f 2>/dev/null", sampdir);
                FILE* fp = popen(findcmd, "r");
                if (!fp) { fprintf(stderr, "?DASM: find fallito\n"); goto done; }
                char line[2048]; int ok=0, fail=0;
                while (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\r\n")] = 0; if (!line[0]) continue;
                    char chunk[2048]; snprintf(chunk, sizeof(chunk), "/tmp/gmb_prg_%ld.chunk", (long)getpid());
                    // compile via C pipeline
                    char* src = NULL; FILE* f = fopen(line, "rb");
                    if (f) { fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET); src=(char*)malloc(len+1); fread(src,1,len,f); src[len]=0; fclose(f); }
                    gmb_lex_result_t lx = gmb_lexer_scan(src ? src : "", src ? strlen(src) : 0);
                    gmb_lex_result_t lx2 = gmb_macro_expand(&lx); lx.owned_src=NULL;
                    char* perr=NULL; gmb_program_t prog = gmb_parse(&lx2, &perr);
                    int is_ok = (perr==NULL);
                    if (is_ok) {
                        uint8_t* data=NULL; size_t dlen=0; char* cerr=NULL;
                        int rc = gmb_compile(&prog, &data, &dlen, &cerr);
                        is_ok = (rc==0);
                        if (cerr) free(cerr);
                        if (data) free(data);
                    }
                    if (is_ok && show_ok) { printf("."); fflush(stdout); }
                    if (!is_ok) { fail++; printf("\n  ERRORE: %s - %s\n", line, perr ? perr : "compile fallita"); if (stop_on_err) { if (perr) free(perr); gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); if(src) free(src); break; } }
                    else ok++;
                    if (perr) free(perr);
                    gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); if(src) free(src);
                }
                pclose(fp);
                printf("\nDASM %s: OK=%d FAIL=%d\n", a1, ok, fail);
                goto done;
            }
            gmb_prginfo(args); goto done;
        }
        if (strcasecmp(cmd, "LOADPRG") == 0) { gmb_loadprg(args); goto done; }
        if (strcasecmp(cmd, "DASM") == 0) {
            char a1[64] = ""; sscanf(args, "%63s", a1);
            if (strcasecmp(a1, "ALL") == 0 || strcasecmp(a1, "ERR") == 0 || strcasecmp(a1, "STOP") == 0) {
                int show_ok = strcasecmp(a1, "ERR") != 0;
                int stop_on_err = strcasecmp(a1, "STOP") == 0;
                char sampdir[2048]; resolve_repo_path(sampdir, sizeof(sampdir), "samples");
                char findcmd[4096]; snprintf(findcmd, sizeof(findcmd), "find \"%s\" -name '*.bas' -type f 2>/dev/null", sampdir);
                FILE* fp = popen(findcmd, "r");
                if (!fp) { fprintf(stderr, "?DASM: find fallito\n"); goto done; }
                char line[2048]; int ok=0, fail=0;
                while (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\r\n")] = 0; if (!line[0]) continue;
                    char chunk[2048]; snprintf(chunk, sizeof(chunk), "/tmp/gmb_prg_%ld.chunk", (long)getpid());
                    FILE* f = fopen(line, "rb");
                    if (!f) continue;
                    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
                    char* src = (char*)malloc(len+1); fread(src,1,len,f); src[len]=0; fclose(f);
                    gmb_lex_result_t lx = gmb_lexer_scan(src, len);
                    gmb_lex_result_t lx2 = gmb_macro_expand(&lx); lx.owned_src=NULL;
                    char* perr=NULL; gmb_program_t prog = gmb_parse(&lx2, &perr);
                    int is_ok = (perr==NULL);
                    if (is_ok) {
                        uint8_t* data=NULL; size_t dlen=0; char* cerr=NULL;
                        int rc = gmb_compile(&prog, &data, &dlen, &cerr);
                        is_ok = (rc==0);
                        if (cerr) free(cerr);
                        if (data) free(data);
                    }
                    if (is_ok && show_ok) { printf("."); fflush(stdout); }
                    if (!is_ok) { fail++; printf("\n  ERRORE: %s - %s\n", line, perr ? perr : "compile fallita"); if (stop_on_err) { if (perr) free(perr); gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src); break; } }
                    else ok++;
                    if (perr) free(perr);
                    gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(src);
                }
                pclose(fp);
                printf("\nDASM %s: OK=%d FAIL=%d\n", a1, ok, fail);
                goto done;
            }
            gmb_dasm(args); goto done;
        }
        if (strcasecmp(cmd, "IL") == 0) {
            printf("(IL: diagnostica IL — appartiene a REPL C#; nel nativo usa DASM sul chunk)\n");
            goto done;
        }
        if (strcasecmp(cmd, "WH") == 0) { printf("%s\n", getcwd(line, sizeof(line))); goto done; }
        if (strcasecmp(cmd, "DEBUG") == 0) {
            char arg[64] = "";
            sscanf(args, "%63s", arg);
            if (strcasecmp(arg, "ON") == 0) { g_dbg.mode = GMB_DBG_RUNNING; printf("DEBUG ON\n"); }
            else if (strcasecmp(arg, "OFF") == 0) { g_dbg.mode = GMB_DBG_OFF; printf("DEBUG OFF\n"); }
            else if (strcasecmp(arg, "BREAKS") == 0) {
                if (g_dbg.break_count == 0) printf("(nessun breakpoint)\n");
                else for (int i = 0; i < g_dbg.break_count; i++)
                    printf("  %d: linea %d (%s, hit %u)\n", i + 1, g_dbg.breaks[i].line,
                           g_dbg.breaks[i].enabled ? "attivo" : "disattivo", g_dbg.breaks[i].hits);
            }
            else if (strcasecmp(arg, "WATCH") == 0) {
                if (g_dbg.watch_count == 0) printf("(nessun watch)\n");
                else for (int i = 0; i < g_dbg.watch_count; i++) printf("  %d: %s\n", i + 1, g_dbg.watches[i].name);
            }
            else printf("DEBUG %s (%s)\n", g_dbg.mode == GMB_DBG_OFF ? "OFF" : g_dbg.mode == GMB_DBG_STEPPING ? "STEPPING" : "RUNNING",
                        g_dbg.trace ? "trace ON" : "trace OFF");
            goto done;
        }
        if (strcasecmp(cmd, "BREAK") == 0) {
            int ln = atoi(args);
            if (ln > 0) { int r = gmb_debug_add_break(&g_dbg, ln); printf(r >= 0 ? "breakpoint a linea %d\n" : "?set pieno o linea non valida\n", ln); }
            else { gmb_debug_clear_all_breaks(&g_dbg); printf("breakpoint azzerati\n"); }
            goto done;
        }
        if (strcasecmp(cmd, "CLEAR") == 0) {
            int ln = atoi(args);
            if (ln > 0) { printf(gmb_debug_clear_break(&g_dbg, ln) == 0 ? "breakpoint linea %d rimosso\n" : "?nessun breakpoint a linea %d\n", ln); }
            else { gmb_debug_clear_all_breaks(&g_dbg); printf("breakpoint azzerati\n"); }
            goto done;
        }
        if (strcasecmp(cmd, "STEP") == 0) { g_dbg.mode = GMB_DBG_STEPPING; printf("STEP: esecuzione passo-passo\n"); goto done; }
        if (strcasecmp(cmd, "CONT") == 0) { g_dbg.mode = g_dbg.mode == GMB_DBG_OFF ? GMB_DBG_OFF : GMB_DBG_RUNNING; printf("CONT: riprendi fino a breakpoint/stop\n"); goto done; }
        if (strcasecmp(cmd, "TRACE") == 0) { g_dbg.trace = !g_dbg.trace; printf("TRACE %s\n", g_dbg.trace ? "ON" : "OFF"); goto done; }
        if (strcasecmp(cmd, "WATCH") == 0) {
            char var[GMB_DBG_MAX_VAR] = "";
            sscanf(args, "%63s", var);
            if (*var && strcasecmp(var, "LIST") != 0) {
                printf(gmb_debug_add_watch(&g_dbg, var) >= 0 ? "watch: %s\n" : "?set pieno\n", var);
            } else {
                if (g_dbg.watch_count == 0) printf("(nessun watch)\n");
                else for (int i = 0; i < g_dbg.watch_count; i++) printf("  %d: %s\n", i + 1, g_dbg.watches[i].name);
            }
            goto done;
        }
        if (strcasecmp(cmd, "UNWATCH") == 0) {
            char var[GMB_DBG_MAX_VAR] = "";
            sscanf(args, "%63s", var);
            printf(*var && gmb_debug_clear_watch(&g_dbg, var) == 0 ? "watch rimosso: %s\n" : "?watch non trovato: %s\n", var);
            goto done;
        }
        if (strcasecmp(cmd, "BRAIN") == 0) {
            char sub[32] = "", name[256] = "";
            sscanf(args, "%31s %255s", sub, name);
            if (strcasecmp(sub, "USE") == 0 && name[0]) {
                if (gmb_brain_use(name) == 0) printf("[OK] brain '%s' collegato\n", name);
                else fprintf(stderr, "?brain '%s' non disponibile\n", name);
            } else if (strcasecmp(sub, "DETACH") == 0 && name[0]) {
                if (gmb_brain_detach(name) == 0) printf("[OK] brain '%s' scollegato\n", name);
                else fprintf(stderr, "?brain '%s' non era collegato\n", name);
            } else if (strcasecmp(sub, "LIST") == 0) {
                char* list = gmb_brain_registry_list();
                if (list && list[0]) { printf("%s", list); free(list); }
                else printf("(nessun cervello registrato)\n");
            } else if (strcasecmp(sub, "INFO") == 0 && name[0]) {
                char* info = gmb_brain_registry_info(name);
                if (info) { printf("%s", info); free(info); }
                else fprintf(stderr, "?cervello '%s' non trovato\n", name);
            } else if (strcasecmp(sub, "INSTALL") == 0) {
                fprintf(stderr, "?BRAIN INSTALL non ancora disponibile nel percorso C\n");
            } else if (strcasecmp(sub, "EXPORT") == 0) {
                fprintf(stderr, "?BRAIN EXPORT non ancora disponibile nel percorso C\n");
            } else if (strcasecmp(sub, "SIGN") == 0) {
                fprintf(stderr, "?BRAIN SIGN non ancora disponibile nel percorso C\n");
            } else {
                printf("BRAIN comandi: USE <name> | DETACH <name> | LIST | INFO <name>\n");
                printf("  (INSTALL|EXPORT|SIGN non ancora disponibili nel percorso C)\n");
            }
            goto done;
        }
        if (strcasecmp(cmd, "EVAL") == 0) {
            fprintf(stderr, "?EVAL non ancora disponibile nel percorso C — usa la REPL C#\n");
            goto done;
        }
        fprintf(stderr, "?comando sconosciuto: %s (HELP per l'elenco)\n", cmd);
    } while (0);
done:
    return 0;
}

/* Loop REPL DENTRO la finestra SdlConsole. Mirror del vecchio
 * GMBasic Executor.RunMainLoop(): Show() + DisplayBanner("EMULATED CONSOLE MODE") +
 * "READY." + loop PollEvents/ProcessReplInput/Present. L'input arriva dai tasti SDL
 * (gmb_sdl_console_pump -> gmb_gfx_input_push_key); l'output dei comandi è catturato
 * (dup di stdout) e scritto nel buffer della console, così la REPL vive nella finestra. */
/* ── Startup REPL: UNA sola esperienza visiva in ogni modalità ──────────────────────────
 * Riferimento: la REPL C# (`ReplEngine.PrintWelcome` + console). Il layout NON si costruisce
 * con un form TUI: si scrive nel buffer della console (SdlConsole/Standard), quindi la
 * finestra SDL mantiene la dimensione della PROPRIA configurazione (grid da settings,
 * C64 = Sixtyfour) e il contenuto è identico in System/Standard/SDL/C64.
 * Il vecchio loop a controlli (CodeArea+TextInput+StatusBar) ridimensionava la finestra e
 * cambiava il layout: rimosso. */

static void con_writeln_centered(gmb_sdl_console_t* c, int width, const char* text)
{
    int len = (int)strlen(text ? text : "");
    int sp = (width - len) / 2; if (sp < 0) sp = 0;
    for (int i = 0; i < sp; i++) gmb_sdl_console_write(c, " ");
    gmb_sdl_console_write(c, text ? text : "");
    gmb_sdl_console_write(c, "\n");
}

/* Esegue un comando catturando stdout (i comandi usano printf) e scrivendo il risultato
 * nel buffer della console. Stesso contratto del vecchio loop. Ritorna 1 se il REPL esce. */
/* Comandi che disegnano a schermo intero sul TERMINALE (non nel buffer di celle):
 * il browser help (HELP bare / BROWSE / ALL) e l'editor IDE. */
static int cmd_is_interactive(const char* s)
{
    while (*s == ' ' || *s == '\t') s++;
    if (!strncasecmp(s, "EDIT", 4) || !strncasecmp(s, "IDE", 3)) return 1;
    if (strncasecmp(s, "HELP", 4) != 0) return 0;
    const char* r = s + 4;
    while (*r == ' ') r++;
    size_t len = strlen(r);
    while (len && (r[len-1] == ' ' || r[len-1] == '\t')) len--;
    if (len == 0) return 1;                                  /* HELP */
    if (len == 6 && !strncasecmp(r, "BROWSE", 6)) return 1;
    if (len == 3 && !strncasecmp(r, "ALL", 3)) return 1;
    return 0;                                                /* HELP <topic> / COMMANDS */
}

static int con_exec(const rpl_options_t* o, gmb_sdl_console_t* con, const char* cmd, const char* capfile)
{
    if (!cmd || !*cmd) return 0;
    /* I comandi INTERATTIVI (HELP BROWSE, EDIT/IDE) disegnano a schermo intero sul
     * TERMINALE: catturare stdout li trasformerebbe in testo dentro al buffer. */
    if (cmd_is_interactive(cmd)) {
        printf("\x1b[?25h"); fflush(stdout);
        return rpl_exec_line(cmd, o, (char*)"");
    }
    int saved = dup(1), q = 0;
    fflush(stdout);
    FILE* f = fopen(capfile, "w+");
    if (f && saved >= 0) {
        dup2(fileno(f), 1);
        q = rpl_exec_line(cmd, o, (char*)"");
        fflush(stdout);
        dup2(saved, 1); close(saved);
        long sz = ftell(f);
        if (sz > 0) {
            fseek(f, 0, SEEK_SET);
            char buf[65536];
            size_t rd = fread(buf, 1, sizeof(buf) - 1, f);
            buf[rd] = 0;
            gmb_sdl_console_write(con, buf);
        }
        fclose(f);
    } else {
        q = rpl_exec_line(cmd, o, (char*)"");
    }
    return q;
}

/* Loop SDL/C64 sul MODELLO della console attiva: nessun form, nessun resize. */
static int rpl_loop_sdl_model(const rpl_options_t* o, gmb_sdl_console_t* con,
                              const char* initial_cmd, const char* mode_line)
{
    char capfile[128]; snprintf(capfile, sizeof(capfile), "/tmp/gmb_repl_cap_%d.txt", (int)getpid());
    gmb_sdl_console_show(con);                     /* apre con la SUA config (testo/C64) */
    gmb_sdl_console_set_cursor_visible(con, 1);
    int W = gmb_screen_width(gmb_sdl_console_screen(con));
    if (W < 20) W = 80;
    /* riga vuota iniziale: come il loop ANSI e come PrintWelcome della REPL C#
     * (senza, il titolo nasce sulla riga 0 e l'ascent tocca il bordo alto). */
    gmb_sdl_console_write(con, "\n");
    con_writeln_centered(con, W, "GMBASIC 1.0.0");
    con_writeln_centered(con, W, "(C) 2026 MINARDI GAETANO. ALL RIGHTS RESERVED.");
    con_writeln_centered(con, W, mode_line);
    gmb_sdl_console_write(con, "\nREADY.\n");
    gmb_sdl_console_render(con);

    if (initial_cmd && *initial_cmd) {
        if (con_exec(o, con, initial_cmd, capfile)) return 0;
        gmb_sdl_console_write(con, "READY.\n");
        gmb_sdl_console_render(con);
    }

    char line[1024]; int n = 0;
    int row0 = gmb_sdl_console_cursor_row(con), col0 = gmb_sdl_console_cursor_col(con);
    for (;;) {
        if (gmb_sdl_console_pump(con)) break;
        int changed = 0;
        char kb[16];
        while (gmb_gfx_input_poll_key(kb, (int)sizeof(kb))) {
            for (const char* p = kb; *p; p++) {
                unsigned char ch = (unsigned char)*p;
                if (ch == '\r' || ch == '\n') {
                    gmb_sdl_console_write(con, "\n");
                    line[n] = 0;
                    int quit = con_exec(o, con, line, capfile);
                    n = 0;
                    if (quit) { gmb_sdl_console_render(con); return 0; }
                    gmb_sdl_console_write(con, "READY.\n");
                    row0 = gmb_sdl_console_cursor_row(con); col0 = gmb_sdl_console_cursor_col(con);
                } else if (ch == 8 || ch == 127) {
                    if (n > 0) n--;
                } else if (ch >= 32) {
                    if (n < (int)sizeof(line) - 1) line[n++] = (char)ch;
                } else continue;
                line[n] = 0;
                gmb_sdl_console_set_cursor_position(con, row0 + 1, col0 + 1);   /* row/col 0-based -> API 1-based */
                gmb_sdl_console_erase_to_eol(con);
                gmb_sdl_console_write(con, line);
                changed = 1;
            }
        }
        int hk = gmb_tui_host_key_pop();
        if (hk == GMB_K_ESC) break;
        if (hk == GMB_K_ENTER) {
            gmb_sdl_console_write(con, "\n");
            line[n] = 0;
            int quit = con_exec(o, con, line, capfile);
            n = 0;
            if (quit) { gmb_sdl_console_render(con); return 0; }
            gmb_sdl_console_write(con, "READY.\n");
            row0 = gmb_sdl_console_cursor_row(con); col0 = gmb_sdl_console_cursor_col(con);
            changed = 1;
        }
        if (changed || 1) gmb_sdl_console_render(con);
        usleep(16000);
    }
    return 0;
}

/* Loop ANSI/System: stesso testo (banner + READY + input) su stdout, nessun form. */
/* Colori ANSI (truecolor) della palette della console attiva: parità con
 * ReplEngine.ApplyConsoleColors/ClearConsoleScreen (sfondo = indice bg, testo = indice fg). */
static void con_emit_colors(int clear)
{
    gmb_iconsole_t* c = gmb_iconsole_active();
    int bg = 0, fg = 15;
    unsigned char br = 0, bgc = 0, bb = 0, fr = 255, fgc = 255, fb = 255;
    if (c) {
        const gmb_screen_config_t* cf = &c->configuration;
        if (cf->default_bg_color >= 0 && cf->default_bg_color < cf->palette_size) {
            bg = cf->default_bg_color; br = cf->resolved_palette[bg][0]; bgc = cf->resolved_palette[bg][1]; bb = cf->resolved_palette[bg][2];
        }
        if (cf->default_fg_color >= 0 && cf->default_fg_color < cf->palette_size) {
            fg = cf->default_fg_color; fr = cf->resolved_palette[fg][0]; fgc = cf->resolved_palette[fg][1]; fb = cf->resolved_palette[fg][2];
        }
    }
    if (clear) printf("\x1b[48;2;%u;%u;%um\x1b[2J\x1b[H", br, bgc, bb);
    printf("\x1b[48;2;%u;%u;%um\x1b[38;2;%u;%u;%um", br, bgc, bb, fr, fgc, fb);
    fflush(stdout);
}

static int rpl_loop_ansi(const rpl_options_t* o, const char* initial_cmd, const char* mode_line);

/* ── Loop della StandardConsole (-c): BUFFER A CELLE, non un terminale ─────────
 * La StandardConsole C# è una console bufferizzata: mantiene una griglia di celle
 * (ScreenCell), ha scroll region proprie, VIEW PRINT, SAVE/RESTORE SCREEN, LOCATE e
 * `CONSOLE.SCROLLMODE`. Le scritture vanno NEL BUFFER; Present()/RenderBuffer()
 * disegna lo schermo (ANSI, con la palette X3 — sfondo blu, testo bianco).
 * L'input è a tasti (raw sul tty): ogni tasto aggiorna il buffer e si ripresenta. */
static int  g_std_fd = -1;
static struct termios g_std_orig;

static int std_tty_open(void)
{
    g_std_fd = open("/dev/tty", O_RDWR);
    if (g_std_fd < 0) return -1;
    struct termios raw;
    if (tcgetattr(g_std_fd, &g_std_orig) != 0) { close(g_std_fd); g_std_fd = -1; return -1; }
    raw = g_std_orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_iflag &= ~(IXON | ICRNL | INLCR | IGNCR);
#ifdef IUCLC
    raw.c_iflag &= ~IUCLC;   /* niente mappa maiuscole→minuscole in input */
#endif
    raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;
    tcsetattr(g_std_fd, TCSANOW, &raw);
    return 0;
}
static void std_tty_close(void)
{
    if (g_std_fd >= 0) { tcsetattr(g_std_fd, TCSANOW, &g_std_orig); close(g_std_fd); g_std_fd = -1; }
}
static int std_key(int ms)
{
    if (g_std_fd < 0) return -1;
    fd_set f; struct timeval tv;
    FD_ZERO(&f); FD_SET(g_std_fd, &f);
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    if (select(g_std_fd + 1, &f, NULL, NULL, &tv) <= 0) return -1;
    unsigned char c;
    if (read(g_std_fd, &c, 1) != 1) return -1;
    return (int)c;
}

/* Parità ReplEngine.ProcessReplInput: se la riga non inizia con una cifra (quindi non è
 * una riga di programma numerata), le LETTERE digitate vanno in MAIUSCOLO. */
static char rpl_edit_char(const char* line, int n, int k)
{
    char ch = (char)k;
    int numbered = (n > 0 && line[0] >= '0' && line[0] <= '9');
    if (!numbered && ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    return ch;
}

static int rpl_loop_standard(const rpl_options_t* o, gmb_iconsole_t* con,
                             gmb_sdl_console_t* model, const char* initial_cmd,
                             const char* mode_line)
{
    char capfile[128];
    snprintf(capfile, sizeof(capfile), "/tmp/gmb_repl_cap_%d.txt", (int)getpid());
    if (std_tty_open() != 0) return rpl_loop_ansi(o, initial_cmd, mode_line);
    if (con->poll_for_resize) con->poll_for_resize(con);     /* buffer = terminale fisico */
    int W = gmb_screen_width(gmb_sdl_console_screen(model));
    if (W < 20) W = 80;
    gmb_sdl_console_set_cursor_visible(model, 1);
    gmb_sdl_console_clear_screen(model);
    gmb_sdl_console_set_cursor_position(model, 1, 1);
    gmb_sdl_console_write(model, "\n");
    con_writeln_centered(model, W, "GMBASIC 1.0.0");
    con_writeln_centered(model, W, "(C) 2026 MINARDI GAETANO. ALL RIGHTS RESERVED.");
    con_writeln_centered(model, W, mode_line);
    gmb_sdl_console_write(model, "\nREADY.\n");
    if (con->present) con->present(con);

    if (initial_cmd && *initial_cmd) {
        if (con_exec(o, model, initial_cmd, capfile)) { std_tty_close(); return 0; }
        gmb_sdl_console_write(model, "READY.\n");
        if (con->present) con->present(con);
    }

    char line[1024]; int n = 0;
    int row0 = gmb_sdl_console_cursor_row(model), col0 = gmb_sdl_console_cursor_col(model);
    for (;;) {
        int k = std_key(30);
        if (k < 0) {
            int pw = gmb_screen_width(gmb_sdl_console_screen(model));
            if (con->poll_for_resize) con->poll_for_resize(con);
            if (gmb_screen_width(gmb_sdl_console_screen(model)) != pw) {
                int bh = gmb_screen_height(gmb_sdl_console_screen(model));
                if (row0 >= bh) row0 = bh - 1;
                if (con->present) con->present(con);
            }
            continue;
        }
        if (k == 3) n = 0;                        /* Ctrl+C: riga annullata */
        else if (k == 4) break;                   /* Ctrl+D: esce */
        else if (k == 13 || k == 10) {
            gmb_sdl_console_write(model, "\n");
            line[n] = 0;
            int interactive = cmd_is_interactive(line);
            int quit = con_exec(o, model, line, capfile);
            n = 0;
            if (interactive && !quit) { gmb_sdl_console_write(model, "\n"); }
            if (quit) { if (con->present) con->present(con); break; }
            gmb_sdl_console_write(model, "READY.\n");
            row0 = gmb_sdl_console_cursor_row(model); col0 = gmb_sdl_console_cursor_col(model);
        } else if (k == 8 || k == 127) {
            if (n > 0) n--;
        } else if (k >= 32) {
            if (n < (int)sizeof(line) - 1) line[n++] = rpl_edit_char(line, n, k);
        } else continue;
        line[n] = 0;
        gmb_sdl_console_set_cursor_position(model, row0 + 1, col0 + 1);
        gmb_sdl_console_erase_to_eol(model);
        gmb_sdl_console_write(model, line);
        if (con->present) con->present(con);
    }
    std_tty_close();
    printf("\x1b[0m"); fflush(stdout);
    if (con->present) con->present(con);
    return 0;
}

/* ── Console testuali: due comportamenti DISTINTI, come nel C# ────────────────
 * Riferimento misurato (REPL C# catturata in un pty, senza finestre):
 *
 *  -s  SystemConsole   → NESSUN colore, NESSUNA pulizia: solo testo sul terminale
 *                        dell'utente (il suo tema decide i colori).
 *                        "
" + 3 righe centrate + "
READY.
"
 *  -c  StandardConsole → ResetColor (fg 15 / bg 1) + Clear() con lo sfondo attivo:
 *                        "[38;2;255;255;255m[48;2;0;0;170m"
 *                        "[H[2J[3J" "[?12l[?25h"
 *                        poi lo stesso welcome; OGNI scrittura riemette i colori
 *                        (WriteInternalNative) → testo bianco su fondo blu. */
static void con_welcome(int W, const char* mode_line)
{
    #define CENTERED(t) do { int _l = (int)strlen(t); int _s = (W - _l) / 2; if (_s < 0) _s = 0; \
        printf("%*s%s\n", _s, "", t); } while (0)
    printf("\n");
    CENTERED("GMBASIC 1.0.0");
    CENTERED("(C) 2026 MINARDI GAETANO. ALL RIGHTS RESERVED.");
    CENTERED(mode_line);
    printf("\nREADY.\n");
    fflush(stdout);
    #undef CENTERED
}

static int rpl_loop_ansi(const rpl_options_t* o, const char* initial_cmd, const char* mode_line)
{
    /* SystemConsole: wrapper diretto del terminale, con la palette applicata
     * (ResetColor + Clear come la console C#: sfondo blu 1, testo 15). */
    int W = 80;
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 20) W = ws.ws_col;
    con_emit_colors(0);
    printf("\x1b[H\x1b[2J\x1b[3J");
    printf("\x1b[?12l\x1b[?25h");
    con_emit_colors(0);
    fflush(stdout);
    con_welcome(W, mode_line);
    char line[1024];
    if (initial_cmd && *initial_cmd) {
        int q0 = rpl_exec_line(initial_cmd, o, (char*)"READY.\n");
        if (q0) { printf("\x1b[0m"); return 0; }
        con_emit_colors(0);
        printf("READY.\n"); fflush(stdout);
    }
    /* Senza terminale (output rediretto / pipeline): righe da stdin, senza editing. */
    if (std_tty_open() != 0) {
        while (fgets(line, sizeof(line), stdin)) {
            size_t k = strlen(line);
            while (k && (line[k - 1] == '\n' || line[k - 1] == '\r')) line[--k] = 0;
            if (!line[0]) { con_emit_colors(0); printf("READY.\n"); fflush(stdout); continue; }
            int q1 = rpl_exec_line(line, o, (char*)"READY.\n");
            if (q1) break;
            con_emit_colors(0);
            printf("READY.\n"); fflush(stdout);
        }
        printf("\x1b[0m"); fflush(stdout);
        return 0;
    }
    /* Terminale: editor a TASTI (raw) con la stessa regola di ReplEngine C#:
     * le lettere digitate vanno in MAIUSCOLO se la riga non è numerata. */
    int n = 0; line[0] = 0;
    for (;;) {
        int k = std_key(30);
        if (k < 0) continue;
        if (k == 3) { n = 0; line[0] = 0; printf("\r\x1b[K"); fflush(stdout); continue; }   /* Ctrl+C */
        if (k == 4) break;                                                                  /* Ctrl+D */
        if (k == 13 || k == 10) {
            printf("\n");
            line[n] = 0;
            int quit = rpl_exec_line(line, o, (char*)"READY.\n");
            n = 0; line[0] = 0;
            if (quit) break;
            con_emit_colors(0);
            printf("READY.\n"); fflush(stdout);
            continue;
        }
        if (k == 8 || k == 127) { if (n > 0) n--; }
        else if (k >= 32) { if (n < (int)sizeof(line) - 1) line[n++] = rpl_edit_char(line, n, k); }
        else continue;
        line[n] = 0;
        printf("\r%s\x1b[K", line);
        fflush(stdout);
    }
    std_tty_close();
    printf("\x1b[0m\n"); fflush(stdout);
    return 0;
}

/* Loop HEADLESS (GMBASIC_HEADLESS=1 / -noconsole): log LINEARE su stdout, nessun rendering
 * a celle/ANSI. */
static int rpl_loop_headless(const rpl_options_t* o, const char* initial_cmd)
{
    char line[1024];
    printf("GMBASIC 1.0.0\n(C) 2026 MINARDI GAETANO. ALL RIGHTS RESERVED.\nHEADLESS MODE - C VERSION\nREADY.\n");
    fflush(stdout);
    if (initial_cmd && *initial_cmd) {
        int q = rpl_exec_line(initial_cmd, o, (char*)"READY.\n");
        if (q) return 0;
        printf("READY.\n");
        fflush(stdout);
    }
    while (fgets(line, sizeof(line), stdin)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!line[0]) { printf("READY.\n"); fflush(stdout); continue; }
        int q = rpl_exec_line(line, o, (char*)"READY.\n");
        if (q) break;
        printf("READY.\n");
        fflush(stdout);
    }
    return 0;
}

static int rpl_loop_controls(const rpl_options_t* o, gmb_sdl_console_t* sdl, const char* initial_cmd)
{
    extern int gmb_console_mode_get(void);
    const char* hl = getenv("GMBASIC_HEADLESS");
    int headless = (hl && *hl && strcmp(hl, "0") != 0);
    /* La riga del modo porta anche il RAMO che sta eseguendo: "… - C VERSION". */
    static char mode_buf[128];
    const char* mode_base;
    if (headless) mode_base = "HEADLESS MODE";
    else if (gmb_console_mode_get() == 1) mode_base = "C64 MODE";
    else if (sdl) mode_base = "SDL CONSOLE MODE";
    else if (gmb_console_mode_get() == 3) mode_base = "SYSTEM CONSOLE MODE";
    else mode_base = "STANDARD CONSOLE MODE";
    snprintf(mode_buf, sizeof(mode_buf), "%s - C VERSION", mode_base);
    const char* mode_line = mode_buf;
    if (sdl) return rpl_loop_sdl_model(o, sdl, initial_cmd, mode_line);
    {   /* Standard (-c): console bufferizzata a celle + Present(). */
        gmb_iconsole_t* con = gmb_iconsole_active();
        if (con && gmb_console_mode_get() == 2) {
            gmb_sdl_console_t* model = gmb_iconsole_model(con);
            if (model) return rpl_loop_standard(o, con, model, initial_cmd, mode_line);
        }
    }
    return rpl_loop_ansi(o, initial_cmd, mode_line);   /* System: ANSI puro */
}

int rpl_loop(const rpl_options_t* o, const char* initial_dir)
{
    rpl_settings_load(initial_dir, &g_settings);
    if (initial_dir) snprintf(g_settings_dir, sizeof(g_settings_dir), "%s", initial_dir);
    else getcwd(g_settings_dir, sizeof(g_settings_dir));
    /* C5: applica palette/font salvati (parità SettingsManager.cs) */
    if (g_settings.palette_name && *g_settings.palette_name) gmb_palette_apply(g_settings.palette_name);
    else if (g_settings.set_theme[0]) gmb_palette_apply(g_settings.set_theme);
    if (o->verbose) fprintf(stderr, "[settings] import=%d font=%s palette=%s\n", g_settings.import_count,
        g_settings.font_path ? g_settings.font_path : "(default)", g_settings.palette_name ? g_settings.palette_name : (g_settings.set_theme[0] ? g_settings.set_theme : "(default)"));
    /* HEADLESS: log lineare (niente schermo a celle). */
    { const char* hl = getenv("GMBASIC_HEADLESS");
      if (hl && *hl && strcmp(hl, "0") != 0) return rpl_loop_headless(o, o->run_command); }
    /* UN SOLO loop, costruito dal registry dei controlli: si rende su SDLConsole (finestra)
     * o TUI/Terminale secondo il contesto attivo. Nessun rendering proprio. --run è eseguito
     * come primo comando DENTRO il loop (il suo output va nel controllo, non su stdout). */
    { gmb_iconsole_t* con = gmb_iconsole_active();
      gmb_sdl_console_t* sdl = con ? gmb_iconsole_sdl_model(con) : NULL;
      return rpl_loop_controls(o, sdl, o->run_command); }
}

/* ── GRAPH BUILD (percorso C): grafo wiki via gmb_graph (runtime condiviso) ──
 * Nodi = file .md (path relativo), edge = wikilink [[target]] risolti per nome file
 * (caso-insensitive, fallback basename). Scrive graphs/<nome>.stats.json con PageRank top.
 * NOTA: il percorso C è completo per l'export (graphml/mermaid/stats.json via gmb_graph);
 * il C# resta il percorso canonico ma qui i file sono identici nello scopo. Il valore del
 * percorso C è dimostrare gmb_graph + PageRank nativi senza dipendere dal C#. */
void rpl_graph_build(const char* wiki_dir)
{
    int64_t g = gmb_graph_create();
    if (g < 0) { fprintf(stderr, "?gmb_graph_create fallito\n"); return; }
    char dir[1024];
    if (realpath(wiki_dir, dir) == NULL) { fprintf(stderr, "?wiki non trovata: %s\n", wiki_dir); return; }

    /* enumerate *.md ricorsivamente */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "find \"%s\" -name '*.md' -type f 2>/dev/null", dir);
    FILE* fp = popen(cmd, "r");
    if (!fp) { fprintf(stderr, "?find fallito\n"); return; }
    char line[2048];
    int nodes = 0, edges = 0, dangling = 0;
    while (fgets(line, sizeof(line), fp))
    {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0]) continue;
        const char* rel = line + strlen(dir);
        while (*rel == '/') rel++;
        gmb_graph_add_node(g, rel);
        nodes++;
    }
    pclose(fp);

    /* secondo pass: wikilink [[target]] per edge (risoluzione per basename case-insensitive) */
    fp = popen(cmd, "r");
    if (fp)
    {
        while (fgets(line, sizeof(line), fp))
        {
            line[strcspn(line, "\r\n")] = 0;
            if (!line[0]) continue;
            const char* rel = line + strlen(dir);
            while (*rel == '/') rel++;
            FILE* f = fopen(line, "r");
            if (!f) continue;
            char buf[8192];
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[n] = 0;
            /* scansione [[...]] semplicistica: nessuna regex in C, pattern testuale */
            const char* p = buf;
            while ((p = strstr(p, "[[")) != NULL)
            {
                const char* e = strstr(p + 2, "]]");
                if (!e) break;
                char target[512];
                size_t tl = (size_t)(e - p - 2);
                if (tl > 0 && tl < sizeof(target))
                {
                    memcpy(target, p + 2, tl); target[tl] = 0;
                    /* rimuovi alias |... e heading #... */
                    char* bar = strchr(target, '|'); if (bar) *bar = 0;
                    char* hash = strchr(target, '#'); if (hash) *hash = 0;
                    /* trim */
                    char* s = target; while (*s == ' ') s++;
                    char* t2 = s + strlen(s); while (t2 > s && t2[-1] == ' ') *--t2 = 0;
                    if (*s && strcmp(s, rel) != 0)
                    {
                        int found = gmb_graph_has_node(g, s);
                        if (found) { gmb_graph_add_edge(g, rel, s, 1.0); edges++; }
                        else
                        {
                            /* fallback path-based: append .md (es. [[Cartella/Nota]] -> Nota.md) */
                            char withmd[640];
                            snprintf(withmd, sizeof(withmd), "%s.md", s);
                            if (gmb_graph_has_node(g, withmd)) { gmb_graph_add_edge(g, rel, withmd, 1.0); edges++; }
                            else dangling++;
                        }
                    }
                }
                p = e + 2;
            }
        }
        pclose(fp);
    }

    /* PageRank */
    int pr_len = 0;
    gmb_graph_pagerank_t* pr = gmb_graph_pagerank(g, 0.85, 100, &pr_len);
    printf("GRAPH BUILD (C): %d nodi, %d edge, %d dangling\n", nodes, edges, dangling);
    if (pr && pr_len > 0)
    {
        printf("PageRank top-5:\n");
        for (int i = 0; i < pr_len && i < 5; i++)
            printf("  %.4f  %s\n", pr[i].rank, pr[i].node);
        /* free(pr) alla fine (dopo l'export stats.json che riusa pr) */
    }

    /* ── export file (graphml + mermaid + stats.json) in graphs/<nome> — parità C# ── */
    const char* base = strrchr(dir, '/');
    const char* leaf = base ? base + 1 : dir;
    char graphs[1024];
    gmb_paths_graphs_dir(graphs, sizeof(graphs));
    mkdir(graphs, 0755);
    char gml[1100], mmd[1100], sts[1100];
    snprintf(gml, sizeof(gml), "%s/%s.graphml", graphs, leaf);
    snprintf(mmd, sizeof(mmd), "%s/%s.mermaid", graphs, leaf);
    snprintf(sts, sizeof(sts), "%s/%s.stats.json", graphs, leaf);

    FILE* fg = fopen(gml, "w");
    if (fg)
    {
        fprintf(fg, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(fg, "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n");
        fprintf(fg, "  <key id=\"title\" for=\"node\" attr.name=\"title\" attr.type=\"string\"/>\n");
        fprintf(fg, "  <graph id=\"G\" edgedefault=\"directed\">\n");
        int nn = 0;
        char** alln = gmb_graph_nodes(g, &nn);
        for (int i = 0; i < nn && alln && alln[i]; i++)
            fprintf(fg, "    <node id=\"%s\"><data key=\"title\">%s</data></node>\n", alln[i], alln[i]);
        free(alln);
        int ne = 0;
        gmb_graph_edge_full_t* allE = gmb_graph_edge_list(g, &ne);
        for (int i = 0; i < ne && allE; i++)
            fprintf(fg, "    <edge source=\"%s\" target=\"%s\"/>\n", allE[i].from, allE[i].to);
        free(allE);
        fprintf(fg, "  </graph>\n</graphml>\n");
        fclose(fg);
    }
    FILE* fm = fopen(mmd, "w");
    if (fm)
    {
        fprintf(fm, "graph TD\n");
        int nn = 0;
        char** alln = gmb_graph_nodes(g, &nn);
        for (int i = 0; i < nn && alln && alln[i]; i++)
        {
            /* sanitize id mermaid: sostituisci i non-alnum con '_' */
            const char* s = alln[i];
            fprintf(fm, "    N%d[\"%s\"]\n", i, s);
        }
        free(alln);
        int ne = 0;
        gmb_graph_edge_full_t* allE = gmb_graph_edge_list(g, &ne);
        for (int i = 0; i < ne && allE; i++)
        {
            /* mappa from/to al loro indice (lookup lineare, grafi piccoli ok) */
            int fi = -1, ti = -1;
            int n2 = 0;
            char** a2 = gmb_graph_nodes(g, &n2);
            for (int k = 0; k < n2 && a2 && a2[k]; k++)
            {
                if (!strcmp(a2[k], allE[i].from)) fi = k;
                if (!strcmp(a2[k], allE[i].to)) ti = k;
            }
            free(a2);
            if (fi >= 0 && ti >= 0) fprintf(fm, "    N%d --> N%d\n", fi, ti);
        }
        free(allE);
        fclose(fm);
    }
    FILE* fst = fopen(sts, "w");
    if (fst)
    {
        fprintf(fst, "{\"name\":\"%s\",\"nodes\":%d,\"edges\":%d,\"dangling\":%d", leaf, nodes, edges, dangling);
        if (pr_len > 0)
        {
            fprintf(fst, ",\"pageRankTop\":[");
            int np = 0;
            char** a3 = gmb_graph_nodes(g, &np);
            (void)a3; free(a3);
            for (int i = 0; i < pr_len && i < 5; i++)
                fprintf(fst, "%s{\"node\":\"%s\",\"rank\":%.6f}", i ? "," : "", pr[i].node ? pr[i].node : "", pr[i].rank);
            fprintf(fst, "]");
        }
        fprintf(fst, "}\n");
        fclose(fst);
    }
    printf("export C: %s.graphml/.mermaid/.stats.json\n", leaf);
    if (pr) free(pr);
    gmb_graph_free(g);
}
