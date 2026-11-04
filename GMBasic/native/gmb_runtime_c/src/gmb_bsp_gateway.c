/* ============================================================================
 * gmb_bsp_gateway.c — renderer BSP COMPLETO via `libgmb_bsp` (VM/C).
 *
 * PERCHÉ. Il runtime C non linka la VM/C (eviterebbe una seconda copia degli stati
 * globali del core) e `gmb_expr` è un valutatore ridotto. Per il BSP "completo"
 * (FUNCTION/SUB degli include, classi, tutto il linguaggio) si carica la dylib
 * `libgmb_bsp` (la stessa usata dal client C# via P/Invoke): la pagina viene
 * precompilata in un chunk A1 dal compilatore C (gmbchunk, `.bsp` → chunk)
 * ed eseguita dalla VM C, con lo stato (agenti/team/workflow/memoria) fornito
 * dall'host tramite l'hook OPZIONALE `gmb_state_hook`.
 *
 * La dylib non conosce il managed né il nostro runtime: le passiamo un puntatore
 * OPACO alla funzione hook (direzione unica: noi chiamiamo lei, lei chiama il
 * nostro hook). Se la dylib manca, `gmb_bsp_try_serve` prosegue col valutatore di
 * espressioni (fallback) — mai un 404 silenzioso.
 * ============================================================================ */
#include "gmb_bsp.h"
#include "gmb_runtime.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

typedef int  (*bsp_abi_fn)(void);
typedef void (*bsp_init_fn)(void);
typedef void (*bsp_hook_fn)(void*, void*);
typedef int  (*bsp_serve_fn)(const char*, const char*, const char*, const char*, char**, int*);
typedef void (*bsp_free_fn)(char*);

static void* g_lib;
static int   g_tried;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static char  g_err[512];
static bsp_init_fn  g_init;
static bsp_hook_fn  g_sethook;
static bsp_serve_fn g_serve;
static bsp_free_fn  g_free;

/* ── Hook di stato: l'host (questo processo) risponde al runtime privato della
 * dylib. LETTURE: si delegano alle stesse funzioni del runtime condiviso. ───── */
static int host_state(void* ctx, const char* surface, const char* a, const char* b,
                      const char* c, char* out, int cap)
{
    (void)ctx; (void)c;
    if (!surface || !out || cap <= 0) return -1;
    char* owned = NULL;
    const char* r = NULL;
    char names[64][64];

    if (!strcmp(surface, "agent.list")) {
        char joined[4096];
        int n = gmb_agent_list(names, 64);
        joined[0] = 0;
        for (int i = 0; i < n; i++) {
            if (i) strncat(joined, "\n", sizeof joined - strlen(joined) - 1);
            strncat(joined, names[i], sizeof joined - strlen(joined) - 1);
        }
        r = joined;
    } else if (!strcmp(surface, "agent.get")) {
        r = gmb_agent_get(a, b);
    } else if (!strcmp(surface, "agent.status")) {
        r = gmb_agent_status(a);
    } else if (!strcmp(surface, "agent.events")) {
        owned = gmb_agent_events_str();
        r = owned;
    } else if (!strcmp(surface, "team.list")) {
        owned = gmb_team_list_str();
        r = owned;
    } else if (!strcmp(surface, "team.agents")) {
        owned = gmb_team_agents_str(a);
        r = owned;
    } else if (!strcmp(surface, "workflow.list")) {
        gmb_string_t s = gmb_workflow_list();
        r = s.data ? s.data : "";
    } else if (!strcmp(surface, "workflow.status")) {
        gmb_string_t s = gmb_workflow_status(a);
        r = s.data ? s.data : "";
    } else if (!strcmp(surface, "workflow.steps")) {
        gmb_string_t s = gmb_workflow_steps(a);
        r = s.data ? s.data : "";
    } else if (!strcmp(surface, "memory.list")) {
        owned = gmb_mem_list();
        r = owned;
    } else if (!strcmp(surface, "memory.load")) {
        owned = gmb_mem_load(a);
        r = owned;
    } else if (!strcmp(surface, "memory.search")) {
        owned = gmb_mem_search(a);
        r = owned;
    } else {
        return -1;   /* non gestita: la dylib usa il proprio registro */
    }

    if (!r) r = "";
    int n = (int)strlen(r);
    int w = n < cap ? n : cap;
    memcpy(out, r, (size_t)w);
    free(owned);
    return n;
}

/* MUTAZIONI: le pagine SSR sono di norma in lettura; se una pagina scrive, si
 * inoltra al runtime host (una sola copia dello stato). -1 = non gestita. */
static int host_mutate(const char* op, const char* a, const char* b, const char* c)
{
    if (!op) return -1;
    if (!strcmp(op, "agent.create")) return gmb_agent_create(a) ? 1 : 0;
    if (!strcmp(op, "agent.delete")) return gmb_agent_delete(a) ? 1 : 0;
    if (!strcmp(op, "agent.set")) return gmb_agent_set(a, b, c) ? 1 : 0;
    if (!strcmp(op, "team.create")) return gmb_team_create(a) ? 1 : 0;
    if (!strcmp(op, "team.delete")) return gmb_team_delete(a) ? 1 : 0;
    if (!strcmp(op, "team.add")) return gmb_team_add_agent(a, b) ? 1 : 0;
    if (!strcmp(op, "team.remove")) return gmb_team_remove_agent(a, b) ? 1 : 0;
    if (!strcmp(op, "team.set_role")) return gmb_team_set_role(a, b, c) ? 1 : 0;
    if (!strcmp(op, "team.set_strategy")) return gmb_team_set_strategy(a, b) ? 1 : 0;
    if (!strcmp(op, "team.set_coordinator")) return gmb_team_set_coordinator(a, b) ? 1 : 0;
    if (!strcmp(op, "workflow.begin")) return gmb_workflow_begin_checked(a) ? 1 : 0;
    if (!strcmp(op, "workflow.delete")) return gmb_workflow_delete(a) ? 1 : 0;
    if (!strcmp(op, "memory.store")) return gmb_mem_store(a, b, c && *c ? atoi(c) : 0) ? 1 : 0;
    if (!strcmp(op, "memory.delete")) return gmb_mem_delete(a) ? 1 : 0;
    return -1;
}

/* L'hook della dylib ha una sola funzione: smistiamo lettura/scrittura dal primo
 * carattere della superficie (le mutazioni usano le stesse stringhe note). */
static int host_hook(void* ctx, const char* surface, const char* a, const char* b,
                     const char* c, char* out, int cap)
{
    /* Le MUTAZIONI arrivano sullo stesso hook (gmb_state_hook_mutate): risposta "1"/"0". */
    if (surface && (!strncmp(surface, "agent.create", 12) || !strncmp(surface, "agent.delete", 12) ||
                    !strncmp(surface, "agent.set", 9) || !strncmp(surface, "team.", 5) ||
                    !strncmp(surface, "workflow.", 9) || !strncmp(surface, "memory.store", 12) ||
                    !strncmp(surface, "memory.delete", 13))) {
        int ok = host_mutate(surface, a, b, c);
        if (ok < 0) return -1;
        if (out && cap > 0) out[0] = ok ? '1' : '0';
        return 1;
    }
    return host_state(ctx, surface, a, b, c, out, cap);
}

static int bsp_try(const char* path)
{
    if (!path || !*path) return 0;
    g_lib = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    return g_lib != NULL;
}

static void* bsp_load(void)
{
    if (g_tried) return g_lib;
    g_tried = 1;
#if defined(_WIN32)
    const char* file = "gmb_bsp.dll";
#elif defined(__APPLE__)
    const char* file = "libgmb_bsp.dylib";
#else
    const char* file = "libgmb_bsp.so";
#endif
    char cand[24][4096];
    int n = 0;
    char exe[4096] = "";
#if defined(__APPLE__)
    { uint32_t sz = (uint32_t)sizeof exe;
      if (_NSGetExecutablePath(exe, &sz) != 0) exe[0] = 0; }
#else
    { ssize_t r = readlink("/proc/self/exe", exe, sizeof exe - 1);
      if (r > 0) exe[r] = 0; }
#endif
    if (exe[0]) {
        char dir[4096];
        snprintf(dir, sizeof dir, "%s", exe);
        char* sl = strrchr(dir, '/');
        if (sl) {
            *sl = 0;
            /* LOCALE PRIMA (bundle), poi RISALENDO dal checkout (sviluppo): la dylib
             * può essere in <build>/libs/<rid>, accanto, o in native/gmb_vm_c/build. */
            for (int up = 0; up < 5 && dir[0]; up++) {
                if (n < 24) { snprintf(cand[n], 4096, "%s/libs/%s/%s", dir, gmb_libs_rid(), file); n++; }
                if (n < 24) { snprintf(cand[n], 4096, "%s/libs/%s", dir, file); n++; }
                if (n < 24) { snprintf(cand[n], 4096, "%s/%s", dir, file); n++; }
                if (n < 24) { snprintf(cand[n], 4096, "%s/gmb_vm_c/build/%s", dir, file); n++; }
                if (n < 24) { snprintf(cand[n], 4096, "%s/build/libs/%s/%s", dir, gmb_libs_rid(), file); n++; }
                char* slash = strrchr(dir, '/');
                if (!slash) break;
                *slash = 0;
            }
        }
    }
    {
        char root[2048];
        if (gmb_paths_repo_root(root, sizeof root) && root[0]) {
            if (n < 24) { snprintf(cand[n], 4096, "%s/GMBasic/native/gmb_vm_c/build/%s", root, file); n++; }
            if (n < 24) { snprintf(cand[n], 4096, "%s/build/libs/%s/%s", root, gmb_libs_rid(), file); n++; }
            if (n < 24) { snprintf(cand[n], 4096, "%s/GMBasic/build/libs/%s/%s", root, gmb_libs_rid(), file); n++; }
        }
    }
    {
        char cache[2048];
        gmb_paths_libs_cache_dir(cache, sizeof cache);
        if (cache[0] && n < 24) {
            snprintf(cand[n], 4096, "%s/%s/%s", cache, gmb_libs_rid(), file); n++;
        }
    }
    if (n < 24) { snprintf(cand[n], 4096, "%s", file); n++; }

    /* Accetta SOLO l'ABI corrente: una dylib vecchia nella cache NON deve essere
     * caricata (renderebbe con semantica diversa, es. il vecchio fallback gmb_expr). */
    for (int i = 0; i < n; i++) {
        void* h = dlopen(cand[i], RTLD_LAZY | RTLD_LOCAL);
        if (!h) continue;
        bsp_abi_fn abi = (bsp_abi_fn)dlsym(h, "gmb_bsp_shared_abi");
        bsp_init_fn init = (bsp_init_fn)dlsym(h, "gmb_bsp_shared_init");
        bsp_hook_fn sethook = (bsp_hook_fn)dlsym(h, "gmb_bsp_shared_set_state_hook");
        bsp_serve_fn serve = (bsp_serve_fn)dlsym(h, "gmb_bsp_shared_serve");
        bsp_free_fn freefn = (bsp_free_fn)dlsym(h, "gmb_bsp_shared_free");
        if (!abi || abi() != 2 || !init || !serve || !freefn) { dlclose(h); continue; }
        g_lib = h; g_init = init; g_sethook = sethook; g_serve = serve; g_free = freefn;
        g_init();
        if (g_sethook) g_sethook((void*)host_hook, NULL);
        return g_lib;
    }
    snprintf(g_err, sizeof g_err,
             "BSP completo: %s (ABI 2) non caricabile (cercata accanto all'eseguibile, nel "
             "build del checkout e in ~/.gmbasic/libs/%s)", file, gmb_libs_rid());
    return NULL;
}

/* 1 = pagina gestita dalla VM/C (html malloc, va liberato con bsp_free); 0 = non gestita
 * (dylib assente o sidecar mancante/stantio) → il chiamante usa il valutatore/fallback. */
static void bsp_load_once(void) { (void)bsp_load(); }

int gmb_bsp_gateway_serve(const char* dir, const char* url_path, const char* query,
                          const char* host, char** out_html, int* out_status)
{
    pthread_once(&g_once, bsp_load_once);
    if (!g_lib) return 0;
    char* html = NULL;
    int st = 0;
    int rc = g_serve(dir, url_path, query, host, &html, &st);
    if (rc != 1 || !html) { if (html) g_free(html); return 0; }
    if (out_html) *out_html = html; else g_free(html);
    if (out_status) *out_status = st;
    return 1;
}
