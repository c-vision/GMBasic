/* gmb_vm_host.c — Host CLI per la VM/C.
 * Legge un chunk nel formato A1 (GMBC), lo carica con gmb_chunk_load e lo esegue.
 * Uso: gmb_vm_host <file.chunk> [--mt]
 * Il chunk A1 è prodotto dal serializzatore C# (ChunkA1Serializer).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gmb_chunk.h"
#include "gmb_vm_core.h"
#include "gmb_tray.h"
#include <signal.h>
#include "gmb_vm_bsp.h"
#include "gmb_network.h"
#include <dlfcn.h>
#include <stdint.h>
#include "gmb_runtime.h"
#include "gmb_vm_eval.h"   /* gmb_eval_current_vm/task per il dispatcher dei frammenti */
#include "gmb_iconsole.h"
#include "llmengine.h"
#include "gmb_agent_loop.h"   /* §4.10: gmb_agent_run_code + gmb_agent_code_query_set (seam agenti) */
#include "gmb_semquery.h"     /* §4.10: gmb_semquery_file (query semantica, compilatore C) */

#ifdef GMB_GFX_SDL_PRESENT
#include <pthread.h>
#include <unistd.h>
#include "gmb_gfx_present.h"
#include "gmb_gfx_input.h"
#include "tool_registry.h"
#endif

extern int g_vm_host_argc; /* definita in gmb_vm_core.c */
extern const char** g_vm_host_argv; /* definita in gmb_vm_core.c */

static void print_str_hook(void* ctx, const char* s, uint32_t len)
{
    fwrite(s, 1, len, stdout);
}

static void print_num_hook(void* ctx, double v)
{
    printf("%.17g", v);
}

static void print_nl_hook(void* ctx)
{
    putchar('\n');
}

/* Registra i backend AI modello (OCR/audio) dal provider locale gmb_host_ai, via l'ABI
 * comune `gmb_llm_engine`: si risolve UN simbolo (`gmb_llm_engine_llamacpp`) e si usano i
 * metodi della vtable (vision/embed). gmb_ai_llamacpp e' una SHARED con llama.cpp vendored:
 * dlopen a runtime (nessuna dipendenza di build della VM dal toolchain llama.cpp). Se la lib
 * non e' presente, la registrazione e' saltata e il runtime risponde con errore esplicito
 * ("no active OCR provider") — mai un risultato silenzioso (regola no-stub). */
static const LlmEngineLlmEngine* g_ai_eng = NULL;   /* motore AI risolto via ABI (oggi llama.cpp) */

/* Wrapper per la register API del runtime (handle `int`, load a 2 arg con n_gpu_layers):
 * delegano all'ABI comune. */
static int vm_ai_embed_load(const char* path, int n_gpu_layers) {
    if (!g_ai_eng) return 0;
    LlmEngineLlmModel m = g_ai_eng->load_model_opts ? g_ai_eng->load_model_opts(path, n_gpu_layers)
                                              : g_ai_eng->load_model(path);
    return (int)(intptr_t)m;
}
static void vm_ai_embed_unload(int h) { if (g_ai_eng) g_ai_eng->free_model((LlmEngineLlmModel)(intptr_t)h); }
static int vm_ai_embed_dim(int h) { return g_ai_eng ? g_ai_eng->emb_dim((LlmEngineLlmModel)(intptr_t)h) : 0; }
static int vm_ai_embed_text(int h, const char* t, float* o, int c) {
    return g_ai_eng ? g_ai_eng->embed_text((LlmEngineLlmModel)(intptr_t)h, t, o, c) : -1;
}

static void gmb_vm_host_register_ai_backends(void)
{
    void* h = NULL;
    const char* env = gmb_opt_env("GMB_HOST_AI_LIB");
    if (env && *env) h = dlopen(env, RTLD_NOW | RTLD_GLOBAL);
    if (!h) h = dlopen("libllm_ai_llamacpp.dylib", RTLD_NOW | RTLD_GLOBAL);
    if (!h) h = dlopen("libllm_ai_llamacpp.so", RTLD_NOW | RTLD_GLOBAL);
    if (!h) return;
    /* ABI comune LLMEnv: un solo simbolo -> vtable del motore (non piu' i singoli gmb_ai_*). */
    typedef const LlmEngineLlmEngine* (*engine_get_fn)(void);
    engine_get_fn get = (engine_get_fn)dlsym(h, "llmengine_llm_engine_llamacpp");
    if (!get) return;
    g_ai_eng = get();
    if (!g_ai_eng) return;
    if ((g_ai_eng->caps & LLMMODELIO_CAP_VISION) && g_ai_eng->vision_load && g_ai_eng->vision_read_image)
        gmb_ai_register_ocr_backend((void*)g_ai_eng->vision_load, (void*)g_ai_eng->vision_read_image,
                                    (void*)g_ai_eng->vision_read_audio, (void*)g_ai_eng->free_str,
                                    (void*)g_ai_eng->vision_unload);
    if ((g_ai_eng->caps & LLMMODELIO_CAP_EMBED) && g_ai_eng->embed_text)
        gmb_ai_register_embed_backend((void*)vm_ai_embed_load, (void*)vm_ai_embed_unload,
                                      (void*)vm_ai_embed_dim, (void*)vm_ai_embed_text);
}


#ifdef GMB_GFX_SDL_PRESENT
/* gmb_vm_run_mt e' bloccante e Cocoa/SDL impongono che le chiamate SDL stiano
 * sul vero thread main del processo — qui e' semplice perche' main() E' gia'
 * quel thread: la VM gira su un worker dedicato, main() pompa la finestra in
 * loop finche' il worker non segnala il completamento (stesso schema usato
 * per gmb_vm_bridge_run, dove il chiamante non e' sempre il vero main). */
typedef struct { gmb_vm_t* vm; int rc; volatile int finished; } gmb_vm_host_run_args_t;

static void* gmb_vm_host_run_thread_fn(void* arg)
{
    gmb_vm_host_run_args_t* a = (gmb_vm_host_run_args_t*)arg;
    a->rc = gmb_vm_run_mt(a->vm);
    a->finished = 1;
    return NULL;
}
#endif

/* Il VM host è un processo subprocess (l'host C# lo lancia e cattura stdout/stderr).
 * Le librerie AI dlopen'd (es. llama.cpp) registrano destructor C++ statici eseguiti da
 * exit()/__cxa_finalize: se la lib è stata caricata, il return normale da main fa
 * crashare il teardown (backtrace dyld/__cxa_finalize) DOPO che l'output è già stato
 * prodotto. Fix: flush esplicito di stdout/stderr e poi _exit(), che salta gli atexit
 * delle lib dinamiche ma conserva l'output già scritto. */
/* Codice d'uscita canonico per un errore BASIC a runtime NON gestito (nessun TRY/ON ERROR):
 * allineato a IL/PCode (`test_ilrun` ritorna 6) e a C99Native (`gmb_raise_error`). Gli errori di
 * uso/IO restano 2, il successo 0. Vedi Bugs/(solved) - Semantica-Errore-Builtin-Tra-Backend.md. */
#define GMB_VM_HOST_EXIT_RUNTIME_ERROR 6

static int gmb_vm_host_exit(int code)
{
    fflush(stdout);
    fflush(stderr);
    _exit(code);
    return code; /* mai raggiunto */
}

static volatile sig_atomic_t g_quit = 0;
static gmb_vm_debug_t* g_interactive_debug = NULL;
static void gmb_vm_host_on_term(int sig) { (void)sig; g_quit = 1; }
static void gmb_vm_host_on_pause(int sig) {
    (void)sig;
    if (g_interactive_debug) g_interactive_debug->pause_requested = 1;
}

/* Dispatcher per OP_CALL_EXTERNAL dei frammenti EVAL/&: la macro deve poter chiamare una
 * FUNCTION del PROGRAMMA (che vive nel chunk). Il valutatore passa VM e TASK correnti
 * (gmb_eval_current_*), così la procedura gira sullo STESSO task/scheduler (GPM), mai su un
 * task nuovo. Ritorna 0 se eseguita, -1 se il nome non è nel chunk (errore esplicito della
 * macro: "chiamata esterna non disponibile"). */
static int gmb_vm_host_eval_call(void* vmp, const char* name, const gmb_val_t* args, int nargs, gmb_val_t* out)
{
    gmb_vm_t* vm = (gmb_vm_t*)vmp;
    if (!vm) vm = gmb_eval_current_vm();
    gmb_task_t* task = gmb_eval_current_task();
    if (!vm || !task || !name || !*name) return -1;
    return gmb_vm_call_proc_sync_vals(vm, task, name, args, nargs, out);
}

static void gmb_vm_host_eval_register(void) { gmb_vm_eval_set_call(gmb_vm_host_eval_call); }

int main(int argc, char** argv)
{
    signal(SIGTERM, gmb_vm_host_on_term);
    signal(SIGINT, gmb_vm_host_on_term);
    if (argc >= 2 && (!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
    {
        fprintf(stderr,
            "Uso: %s <file.chunk> [--mt] [--debug-interactive]\n"
            "  esegue il bytecode A1/GMBC con la VM/C (host esterno);\n"
            "  --mt                 scheduler multi-thread\n"
            "  --debug-interactive  avvia il server di debug per REPL/IDE\n",
            argv[0]);
        return gmb_vm_host_exit(0);
    }
    if (argc < 2)
    {
        fprintf(stderr, "Uso: %s <file.chunk> [--mt] [--debug-interactive]\n", argv[0]);
        return gmb_vm_host_exit(2);
    }

    gmb_iconsole_boot();   /* startup console: replica Executor.Startup.cs (GMB_CONSOLE/GMBASIC_HEADLESS) */
    {   /* Input scriptato dai test (stessa variabile della gamba C99): typeahead TUI. */
        extern void gmb_tui_inject_keys(const char*);
        const char* _scr = gmb_opt_env("GMB_TUI_SCRIPT");
        if (_scr && *_scr) gmb_tui_inject_keys(_scr);
    }
    gmb_vm_host_register_ai_backends();
    gmb_tool_registry_seed_builtins();
    /* §4.10 esposizione agenti: registra i seam del comando AI.AGENTRUNCODE — il loop agenti C
     * (`gmb_agent_run_code`) e la query semantica del compilatore C (`gmb_semquery_file`). */
    gmb_agent_code_query_set(gmb_semquery_file);
    gmb_agent_run_code_json_set(gmb_agent_run_code);
    gmb_agent_run_graph_json_set(gmb_agent_run_graph);
        char _mp[1024]; gmb_paths_mcp_json(_mp, sizeof(_mp));
        if (gmb_mcp_load(_mp) == 0) {
            char _cwd[1024];
            if (getcwd(_cwd, sizeof(_cwd))) {
                for (int _up = 0; _up < 8; _up++) {
                    char _t[1200]; snprintf(_t, sizeof(_t), "%s/config/mcp.json", _cwd);
                    if (gmb_mcp_load(_t) > 0) break;
                    char* _sl = strrchr(_cwd, '/'); if (!_sl || _sl == _cwd) break; *_sl = 0;
                }
            }
        }
    if (getenv("GMBASIC_TEST_FIXTURES"))
    {
        /* Parità col test C# (ProviderServer.EnsureDefaultsLocked): i test usano SOLO i
         * provider fixture, non caricano providers.json. */
        gmb_provider_register("embedding", "fixture-embedding", "fixture", "");
        gmb_provider_register("completion", "fixture-completion", "fixture", "");
        gmb_provider_register("reranker", "keyword-reranker", "keyword", "");
        gmb_provider_set_active("completion", "fixture-completion");
        gmb_provider_set_active("embedding", "fixture-embedding");
    }
    else
    {
        /* Host normale: carica config/providers.json come fa l'host C#. */
        char _pp[1024];
        gmb_paths_providers_json(_pp, sizeof(_pp));
        int _loaded = gmb_provider_v2_load(_pp);
        if (_loaded == 0) {
            char _cwd[1024];
            if (getcwd(_cwd, sizeof(_cwd))) {
                for (int _up = 0; _up < 8; _up++) {
                    char _t[1200];
                    snprintf(_t, sizeof(_t), "%s/config/providers.json", _cwd);
                    if (gmb_provider_v2_load(_t) > 0) break;
                    char* _sl = strrchr(_cwd, '/');
                    if (!_sl || _sl == _cwd) break;
                    *_sl = 0;
                }
            }
        }
    }

    const char* path = argv[1];
    int use_mt = 0;
    int use_debug = 0;
    int debug_interactive = 0;
    int debug_breaks[256]; int debug_break_count = 0;
    int debug_single_step = 0;
    int i = 2;
    /* Opzioni host: --ffi-lib <path> (ripetibile), --mt, --debug, --break <line> (ripetibile), --single-step */
    while (i < argc)
    {
        if (strcmp(argv[i], "--ffi-lib") == 0 && i + 1 < argc)
        {
            gmb_ffi_dl_preload(argv[i + 1]);
            i += 2;
        }
        else if (strcmp(argv[i], "--mt") == 0)
        {
            use_mt = 1;
            i += 1;
        }
        else if (strcmp(argv[i], "--debug") == 0)
        {
            use_debug = 1;
            i += 1;
        }
        else if (strcmp(argv[i], "--debug-interactive") == 0)
        {
            use_debug = 1;
            debug_interactive = 1;
            i += 1;
        }
        else if (strcmp(argv[i], "--break") == 0 && i + 1 < argc)
        {
            if (debug_break_count < 256) debug_breaks[debug_break_count++] = atoi(argv[i+1]);
            i += 2;
        }
        else if (strcmp(argv[i], "--single-step") == 0)
        {
            debug_single_step = 1;
            i += 1;
        }
        else if (strcmp(argv[i], "--entry-source") == 0 && i + 1 < argc)
        {
            /* Percorso del .bas SORGENTE: argv[1] e' il CHUNK compilato (spesso in una dir temp),
             * quindi PROJECTFOLDER()/APPPATH() cadevano sulla cartella del chunk (divergenza tra
             * le gambe). L'harness passa qui il .bas originale -> entry corretto. */
            gmb_path_set_entry(argv[i + 1]);
            /* Cartella ABI: `app.settings.json` sta accanto al `.bas`, non nella CWD dell'host
             * (la REPL lancia da un'altra cartella) -> `paths.abiDir` restava inerte. */
            { char _ad[2048]; snprintf(_ad, sizeof _ad, "%s", argv[i + 1]);
              char* _sl = strrchr(_ad, '/'); if (_sl) { if (_sl == _ad) _sl[1] = 0; else *_sl = 0; }
              else snprintf(_ad, sizeof _ad, ".");
              gmb_abi_set_app_dir(_ad); }
            i += 2;
        }
        else break;
    }
    g_vm_host_argc = argc - i; /* argomenti del programma (oltre path e opzioni host) */
    g_vm_host_argv = (const char**)(argv + i);

    FILE* f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "gmb_vm_host: impossibile aprire %s\n", path);
        return gmb_vm_host_exit(2);
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0)
    {
        fprintf(stderr, "gmb_vm_host: file vuoto o errore\n");
        fclose(f);
        return gmb_vm_host_exit(2);
    }
    uint8_t* data = (uint8_t*)malloc((size_t)sz);
    if (!data)
    {
        fprintf(stderr, "gmb_vm_host: memoria insufficiente\n");
        fclose(f);
        return gmb_vm_host_exit(2);
    }
    size_t n = fread(data, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz)
    {
        fprintf(stderr, "gmb_vm_host: lettura incompleta\n");
        free(data);
        return gmb_vm_host_exit(2);
    }

    gmb_chunk_t* chunk = gmb_chunk_load(data, n, true);
    free(data);
    if (!chunk)
    {
        fprintf(stderr, "gmb_vm_host: chunk A1 non valido (formato o versione errati)\n");
        return gmb_vm_host_exit(1);
    }
    /* TESTO SORGENTE per ERR.SourceLineText: le righe viaggiano nel chunk (coda opzionale).
     * Il chunk vive per tutta l'esecuzione, quindi il registro può puntarne l'array. */
    if (chunk->src_texts && chunk->src_text_count > 0)
        gmb_src_lines_register((const char* const*)chunk->src_texts, (int)chunk->src_text_count);

    gmb_vm_t vm;
    gmb_net_init();
    gmb_vm_bsp_register();   /* BSP: pagine .bsp precompilate eseguite dalla VM */
    gmb_vm_init(&vm, chunk);
    gmb_vm_set_http_vm(&vm);
    gmb_vm_host_eval_register();
    vm.print_str = print_str_hook;
    vm.print_num = print_num_hook;
    vm.print_newline = print_nl_hook;
    gmb_vm_debug_t dbg;
    if (use_debug) {
        memset(&dbg, 0, sizeof(dbg));
        for (int b = 0; b < debug_break_count; b++) {
            if (dbg.breakpoint_count < GMB_MAX_BREAKPOINTS)
                dbg.breakpoints[dbg.breakpoint_count++] = debug_breaks[b];
        }
        vm.debug = &dbg;
        if (debug_single_step) dbg.single_step = 1;
        else if (debug_break_count == 0) dbg.single_step = 1;
    }

    /* Debug: se --debug, esegue con hook breakpoint/step (single task, no GPM MT) */
    if (use_debug) {
        int rc_dbg = 0;
        int debug_quit = 0;
        if (debug_interactive) {
            g_interactive_debug = &dbg;
            signal(SIGUSR1, gmb_vm_host_on_pause);
            fprintf(stderr, "@@GMBDBG READY\n");
            fflush(stderr);
        }
        for (;;) {
            rc_dbg = gmb_vm_run_debug(&vm);
            if (rc_dbg != GMB_TICK_DEBUG_PAUSE) break;
            uint32_t line = gmb_vm_debug_current_line(&vm);
            if (debug_interactive) {
                fprintf(stderr, "@@GMBDBG PAUSED %u\n", line);
                fflush(stderr);
                for (;;) {
                    char command[256];
                    if (!fgets(command, sizeof(command), stdin)) {
                        fprintf(stderr, "@@GMBDBG EOF continue\n");
                        fflush(stderr);
                        vm.debug->single_step = 0;
                        vm.debug->skip_line = line;
                        break;
                    }
                    char *p = command;
                    while (*p == ' ' || *p == '\t') p++;
                    char *end = p + strlen(p);
                    while (end > p && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
                    if (!strcmp(p, "c") || !strcmp(p, "continue")) {
                        vm.debug->single_step = 0;
                        vm.debug->skip_line = line;
                        fprintf(stderr, "@@GMBDBG CONTINUE\n");
                        fflush(stderr);
                        break;
                    }
                    if (!strcmp(p, "s") || !strcmp(p, "step")) {
                        vm.debug->single_step = 1;
                        vm.debug->skip_line = line;
                        vm.debug->step_over_depth = 0;
                        vm.debug->step_out_depth = 0;
                        fprintf(stderr, "@@GMBDBG STEP\n");
                        fflush(stderr);
                        break;
                    }
                    if (!strcmp(p, "o") || !strcmp(p, "over")) {
                        int d = gmb_vm_debug_depth(&vm);
                        vm.debug->single_step = 0;
                        vm.debug->skip_line = 0;
                        vm.debug->step_over_depth = d + 1;   /* 0 = off */
                        vm.debug->step_over_line = line;
                        vm.debug->step_out_depth = 0;
                        fprintf(stderr, "@@GMBDBG STEPOVER\n");
                        fflush(stderr);
                        break;
                    }
                    if (!strcmp(p, "u") || !strcmp(p, "out")) {
                        int d = gmb_vm_debug_depth(&vm);
                        vm.debug->single_step = 0;
                        vm.debug->skip_line = 0;
                        vm.debug->step_out_depth = d + 1;    /* 0 = off */
                        vm.debug->step_over_depth = 0;
                        fprintf(stderr, "@@GMBDBG STEPOUT\n");
                        fflush(stderr);
                        break;
                    }
                    if (!strcmp(p, "q") || !strcmp(p, "quit")) {
                        debug_quit = 1;
                        break;
                    }
                    if (!strcmp(p, "bt") || !strcmp(p, "stack")) {
                        uint32_t frames[64];
                        int n = gmb_vm_debug_stack(&vm, frames, 64);
                        fprintf(stderr, "@@GMBDBG STACK");
                        for (int f = 0; f < n; f++) fprintf(stderr, " %u", frames[f]);
                        fprintf(stderr, "\n");
                        fflush(stderr);
                        continue;
                    }
                    if ((p[0] == 'p') && (p[1] == ' ' || p[1] == '\t')) {
                        char value[2048];
                        char variable[128];
                        snprintf(variable, sizeof(variable), "%s", p + 2);
                        if (gmb_vm_debug_read(&vm, variable, value, sizeof(value))) {
                            for (char *q = value; *q; q++) if (*q == '\n' || *q == '\r') *q = ' ';
                            fprintf(stderr, "@@GMBDBG VALUE %s %s\n", variable, value);
                        } else {
                            fprintf(stderr, "@@GMBDBG VALUE %s <not-found>\n", variable);
                        }
                        fflush(stderr);
                        continue;
                    }
                    if ((p[0] == 'b' || p[0] == 'd') && (p[1] == ' ' || p[1] == '\t')) {
                        int target = atoi(p + 2);
                        int found = -1;
                        for (int b = 0; b < vm.debug->breakpoint_count; b++)
                            if (vm.debug->breakpoints[b] == (uint32_t)target) { found = b; break; }
                        if (target <= 0) {
                            fprintf(stderr, "@@GMBDBG ERROR invalid-line\n");
                        } else if (p[0] == 'b') {
                            if (found < 0 && vm.debug->breakpoint_count < GMB_MAX_BREAKPOINTS)
                                vm.debug->breakpoints[vm.debug->breakpoint_count++] = (uint32_t)target;
                            fprintf(stderr, "@@GMBDBG BREAK %d\n", target);
                        } else {
                            if (found >= 0) {
                                for (int b = found; b < vm.debug->breakpoint_count - 1; b++)
                                    vm.debug->breakpoints[b] = vm.debug->breakpoints[b + 1];
                                vm.debug->breakpoint_count--;
                            }
                            fprintf(stderr, "@@GMBDBG CLEAR %d\n", target);
                        }
                        fflush(stderr);
                        continue;
                    }
                    fprintf(stderr, "@@GMBDBG ERROR unknown-command\n");
                    fflush(stderr);
                }
                if (debug_quit) break;
                continue;
            }
            fprintf(stderr, "* hit breakpoint line %u\n", line);
            vm.debug->single_step = 0;
            continue;
        }
        if (debug_interactive) {
            g_interactive_debug = NULL;
            fprintf(stderr, "@@GMBDBG EXIT %d\n", debug_quit ? 0 : rc_dbg);
            fflush(stderr);
        }
        gmb_vm_free(&vm);
        gmb_chunk_free(chunk);
        return gmb_vm_host_exit(debug_quit ? 0 : (rc_dbg == 0 ? 0 : GMB_VM_HOST_EXIT_RUNTIME_ERROR));
    }

    /* Il GPM (gmb_vm_run_mt) è il runner corretto: gestisce SLEEP, GO (goroutine),
       canali e waitgroup tramite lo scheduler multi-thread (come l'implementazione C#). */
    int rc;
#ifdef GMB_GFX_SDL_PRESENT
    {
        /* Headless RUNTIME: se stdout NON è un TTY (pipe/file — CLI, server, test, harness) o
         * GMBASIC_HEADLESS=1, NON avviare la console grafica SDL. Prima la SDL veniva inizializzata
         * SEMPRE (compile-time) → crash (exit 139) in contesti headless/server. Come il lato C#. */
        /* SDL solo se la CONSOLE scelta è SDL (GMB_CONSOLE 0/1). Con -c (Standard) / -s
         * (System) o headless il programma gira testuale: prima si apriva SEMPRE la finestra
         * SDL anche per un PRINT (il "fondo blu" senza senso, e output in una finestra che
         * nessuno aveva chiesto). */
        extern int gmb_console_mode_get(void);
        int cmode = gmb_console_mode_get();
        const char* hl = getenv("GMBASIC_HEADLESS");
        int headless = (hl && (hl[0] == '1' || strcmp(hl, "true") == 0)) || !isatty(STDOUT_FILENO)
                       || (cmode != 0 && cmode != 1);
        if (headless)
        {
            rc = gmb_vm_run_mt(&vm);
        }
        else
        {
        gmb_vm_host_run_args_t args;
        args.vm = &vm; args.rc = 0; args.finished = 0;
        pthread_t th;
        gmb_gfx_present_start();
        if (pthread_create(&th, NULL, gmb_vm_host_run_thread_fn, &args) == 0)
        {
            while (!args.finished && !g_quit)
            {
                gmb_tray_poll(); /* main thread: esegue i comandi tray accodati dai worker (pattern SDL) */
                gmb_gfx_present_tick();
                /* Input motore C → console VM (INKEY/WAITKEY/GET leggono da lì). */
                {
                    char ikey[64];
                    while (gmb_gfx_input_poll_key(ikey, (int)sizeof(ikey)))
                        gmb_console_sendkey(ikey);
                }
                usleep(16000); /* ~60Hz */
            }
            if (g_quit)
            {
                /* Stop ordinato su SIGTERM/SIGINT: rimuove l'icona e termina (i worker muoiono con il processo). */
                gmb_tray_remove();
                gmb_tray_poll();
                gmb_vm_host_exit(0);
            }
            pthread_join(th, NULL);
            rc = args.rc;
        }
        else
        {
            rc = gmb_vm_run_mt(&vm); /* pthread_create fallito: degrado sicuro */
        }
        gmb_gfx_present_stop();
        }
    }
#else
    rc = gmb_vm_run_mt(&vm);
#endif

    gmb_vm_free(&vm);
    gmb_chunk_free(chunk);
    return gmb_vm_host_exit(rc == 0 ? vm.exit_code : GMB_VM_HOST_EXIT_RUNTIME_ERROR);
}
