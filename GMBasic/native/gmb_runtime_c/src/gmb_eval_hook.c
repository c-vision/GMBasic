/* gmb_eval_hook.c — registro del valutatore a runtime (compilatore C + VM).
 *
 * Due modalità:
 *  1. IN-PROCESS: un motore che ha già compilatore+VM registra le funzioni
 *     (`gmb_eval_register_env` / `gmb_eval_register_filter`).
 *  2. ON DEMAND: nessun motore nel processo (runtime condiviso, programmi
 *     C99Native) → si carica `libgmb_eval` (dylib) al primo uso e si chiama la
 *     sua `gmb_eval_run_env`. Il compilatore NON è linkato nel binario: viene
 *     usato solo quando un frammento va davvero valutato.
 * Se nessuna delle due è disponibile il chiamante riceve -1 (errore esplicito). */
#include "gmb_runtime.h"

#include <dlfcn.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>            /* _NSGetExecutablePath (sotto __APPLE__, più sotto) */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*eval_run_fn)(const char* src, int as_stmt,
                           gmb_eval_env_get_fn get, gmb_eval_env_set_fn set,
                           void* env, gmb_eval_call_fn call,
                           gmb_eval_print_str_fn pstr, gmb_eval_print_num_fn pnum,
                           gmb_eval_print_nl_fn pnl, void* pctx, gmb_val_t* out);
typedef int (*eval_abi_fn)(void);
typedef const char* (*eval_err_fn)(void);

static gmb_eval_env_fn g_eval_env = NULL;
static gmb_eval_call_fn g_eval_call = NULL;

void gmb_eval_set_call(gmb_eval_call_fn fn) { g_eval_call = fn; }
gmb_eval_call_fn gmb_eval_get_call(void) { return g_eval_call; }
static gmb_eval_env_fn g_eval_filter = NULL;

/* handle della dylib (lazy, una volta) */
static void* g_lib = NULL;
static int g_lib_tried = 0;
static eval_run_fn g_run = NULL;
static eval_err_fn g_run_err = NULL;

void gmb_eval_register_env(gmb_eval_env_fn fn) { g_eval_env = fn; }

void gmb_eval_register_filter(gmb_eval_env_fn fn) { g_eval_filter = fn; }

const char* gmb_eval_last_error(void)
{
    static char err[384];
    /* Il MOTIVO vero sta nella libreria: se è caricata, si riporta il suo ultimo errore
     * (frammento non compilabile, builtin non implementato, eccezione del valutatore…).
     * Prima si diceva sempre "priva di gmb_eval_run_env" — falso appena la libreria era
     * caricata, e mascherava l'errore reale. */
    if (g_run_err) {
        const char* e = g_run_err();
        if (e && *e) return e;
    }
    if (!g_lib) snprintf(err, sizeof err,
        "nessun valutatore a runtime: libro %s non caricabile", "libgmb_eval");
    else snprintf(err, sizeof err, "libgmb_eval priva di gmb_eval_run_env");
    return err;
}

static void* load_eval_lib(void)
{
    if (g_lib_tried) return g_lib;
    g_lib_tried = 1;
#if defined(_WIN32)
    const char* file = "gmb_eval.dll";
#elif defined(__APPLE__)
    const char* file = "libgmb_eval.dylib";
#else
    const char* file = "libgmb_eval.so";
#endif
    char cand[10][2048];
    int n = 0;
    char exe[2048] = "";
#if defined(__APPLE__)
    { uint32_t sz = (uint32_t)sizeof exe; if (_NSGetExecutablePath(exe, &sz) != 0) exe[0] = 0; }
#endif
    if (exe[0]) {
        char dir[2048]; snprintf(dir, sizeof dir, "%s", exe);
        char* sl = strrchr(dir, '/');
        if (sl) {
            *sl = 0;
            snprintf(cand[n++], 2048, "%s/libs/%s/%s", dir, gmb_libs_rid(), file);
            snprintf(cand[n++], 2048, "%s/%s", dir, file);
            snprintf(cand[n++], 2048, "%s/../libs/%s/%s", dir, gmb_libs_rid(), file);
        }
    }
    {
        char root[2048];
        if (gmb_paths_repo_root(root, sizeof root) && root[0]) {
            /* gmb_paths_repo_root restituisce la radice del repo C (…/GMBasic). */
            snprintf(cand[n++], 2048, "%s/native/gmb_vm_c/build/%s", root, file);
            snprintf(cand[n++], 2048, "%s/build/libs/%s/%s", root, gmb_libs_rid(), file);
            snprintf(cand[n++], 2048, "%s/../build/libs/%s/%s", root, gmb_libs_rid(), file);
        }
    }
    {
        char cache[2048];
        gmb_paths_libs_cache_dir(cache, sizeof cache);
        if (cache[0]) snprintf(cand[n++], 2048, "%s/%s/%s", cache, gmb_libs_rid(), file);
    }
    snprintf(cand[n++], 2048, "%s", file);

    for (int i = 0; i < n; i++) {
        void* h = dlopen(cand[i], RTLD_NOW | RTLD_LOCAL);
        if (!h) continue;
        eval_abi_fn abi = (eval_abi_fn)dlsym(h, "gmb_eval_abi");
        eval_run_fn run = (eval_run_fn)dlsym(h, "gmb_eval_run_env");
        if (!abi || abi() != 1 || !run) { dlclose(h); continue; }
        g_lib = h; g_run = run;
        g_run_err = (eval_err_fn)dlsym(h, "gmb_eval_run_error");   /* opzionale */
        return g_lib;
    }
    return NULL;
}

int gmb_eval_env(const char* src, int as_stmt,
                 gmb_eval_env_get_fn get, gmb_eval_env_set_fn set,
                 void* env, gmb_eval_call_fn call,
                 gmb_eval_print_str_fn pstr, gmb_eval_print_num_fn pnum,
                 gmb_eval_print_nl_fn pnl, void* pctx, gmb_val_t* out)
{
    if (g_eval_env) return g_eval_env(src, as_stmt, get, set, env, call, pstr, pnum, pnl, pctx, out);
    if (load_eval_lib() && g_run) return g_run(src, as_stmt, get, set, env, call, pstr, pnum, pnl, pctx, out);
    return -1;
}

int gmb_eval_filter_env(const char* src,
                        gmb_eval_env_get_fn get, gmb_eval_env_set_fn set,
                        void* env, gmb_eval_call_fn call,
                        gmb_eval_print_str_fn pstr, gmb_eval_print_num_fn pnum,
                        gmb_eval_print_nl_fn pnl, void* pctx, gmb_val_t* out)
{
    if (g_eval_filter) return g_eval_filter(src, 2, get, set, env, call, pstr, pnum, pnl, pctx, out);
    if (load_eval_lib() && g_run) return g_run(src, 2, get, set, env, call, pstr, pnum, pnl, pctx, out);
    return -1;
}
