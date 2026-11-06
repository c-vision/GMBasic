/* ============================================================================
 * GMBasic — Shared dynamic-library loading helpers (gmb_ffi_dl.c)
 * FFI dlopen/dlsym condiviso dal runtime C: usato da TUTTI i backend che hanno
 * bisogno di caricare librerie native a runtime (DECLARE ... LIB):
 *   - C99   (il C99CodeEmitter NON emette più gmb_ffi_open/dlsym inline: usa questi)
 *   - VM/C  (OP_CALL_FFI in gmb_vm_core.c non è più uno stub)
 *   - host IL (via P/Invoke, se serve)
 *
 * Funzioni:
 *   gmb_ffi_dl_open(path)   — dlopen con cache (path → handle), fallback estensioni
 *                             (.dylib/.so/.dll) e match per basename sulle librerie
 *                             PRE-CARICATE (gmb_ffi_dl_preload) dei pacchetti REQUIRE.
 *   gmb_ffi_dl_sym(lib, n)  — dlsym wrapper.
 *   gmb_ffi_dl_preload(path)— registra una .so/.dylib di un pacchetto risolto; il match
 *                             per basename rende DECLARE LIB "nome" risolvibile anche
 *                             se il file reale è lib<nome>.<ext> in un path assoluto.
 * Su Windows mappa a LoadLibraryA/GetProcAddress.
 * ============================================================================ */

#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <direct.h>
#define GMB_DL_OPEN(name)      ((void*)LoadLibraryA(name))
#define GMB_DL_SYM(handle,name) ((void*)GetProcAddress((HMODULE)(handle), (name)))
#else
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#define GMB_DL_OPEN(name)      dlopen((name), RTLD_LAZY)
#define GMB_DL_SYM(handle,name) dlsym((handle), (name))
#endif

#define GMB_FFI_DL_CACHE_MAX 64

/* --- Preload (librerie dei pacchetti REQUIRE risolti) --- */
typedef struct {
    void*  handle;
    char   path[1024];
    char   base[128];   /* basename minuscolo senza prefisso lib- né estensione */
} gmb_ffi_dl_entry_t;

static gmb_ffi_dl_entry_t g_preloaded[GMB_FFI_DL_CACHE_MAX];
static int g_preloaded_count = 0;

static void gmb_ffi_dl_basename(const char* path, char* out, size_t cap)
{
    const char* b = strrchr(path, '/');
    b = (b != NULL) ? b + 1 : path;
    size_t n = strlen(b);
    if (n >= cap) n = cap - 1;
    memcpy(out, b, n);
    out[n] = '\0';
    /* strippa prefisso `lib-` */
    if (n > 3 && out[0] == 'l' && out[1] == 'i' && out[2] == 'b')
    {
        memmove(out, out + 3, n - 3);
        out[n - 3] = '\0';
        n -= 3;
    }
    /* tronca all'estensione ed abbassa il case */
    for (char* p = out; *p; p++)
    {
        *p = (char)tolower((unsigned char)*p);
        if (*p == '.') { *p = '\0'; break; }
    }
}

/* Cerca `name`/`lib<base>` in una cartella libs (piatto + una live sotto). 1 se trovata. */
static int dl_try_in_root(const char* libsRoot, const char* name, const char* base, char* out, size_t cap)
{
    const char* exts[] = { ".dylib", ".so", ".dll" };
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        char cand[8192];
        snprintf(cand, sizeof(cand), "%s/%s%s", libsRoot, name, exts[i]);
        FILE* f = fopen(cand, "rb");
        if (f) { fclose(f); snprintf(out, cap, "%s", cand); return 1; }
        snprintf(cand, sizeof(cand), "%s/lib%s%s", libsRoot, base, exts[i]);
        f = fopen(cand, "rb");
        if (f) { fclose(f); snprintf(out, cap, "%s", cand); return 1; }
    }
#if !defined(_WIN32) && !defined(_WIN64)
    {
        DIR* ld = opendir(libsRoot);
        if (ld != NULL) {
            struct dirent* sd;
            while ((sd = readdir(ld)) != NULL) {
                if (sd->d_name[0] == '.') continue;
                for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
                    char sc[8192];
                    snprintf(sc, sizeof(sc), "%s/%s/%s%s", libsRoot, sd->d_name, name, exts[i]);
                    FILE* sf = fopen(sc, "rb");
                    if (sf) { fclose(sf); closedir(ld); snprintf(out, cap, "%s", sc); return 1; }
                    snprintf(sc, sizeof(sc), "%s/%s/lib%s%s", libsRoot, sd->d_name, base, exts[i]);
                    sf = fopen(sc, "rb");
                    if (sf) { fclose(sf); closedir(ld); snprintf(out, cap, "%s", sc); return 1; }
                }
            }
            closedir(ld);
        }
    }
#endif
    return 0;
}

/* --- Auto-discovery librerie (drop-in FFI) -------------------------------------
 * Cerca `<libs>/<name>` o `<libs>/<nome>/<name>`:
 *  (1) risalendo dal CWD (max 16 livelli, nearest-first);
 *  (2) in ASSOLUTO dall'install GMBasic: <root>/libs (nostre lib interne),
 *      <root>/build/libs/<rid> (artefatti di build) -> così qualunque programma BASIC,
 *      ovunque creato, trova le librerie. Le librerie SCARICATE stanno nella cache
 *      (~/.gmbasic/libs/<rid>), che è un passo separato più sotto.
 * Kill-switch GMB_FFI_NO_LIBS_SCAN=1. Stessa logica di FfiService.FindInLibsFolders (C#). */
static int gmb_ffi_dl_find_libs(const char* path, char* out, size_t cap)
{
    const char* no = gmb_opt_env("GMB_FFI_NO_LIBS_SCAN");
    if (no != NULL && strcmp(no, "1") == 0) return 0;

    /* basename senza directory (accetta separatori '/' e '\\') */
    const char* b = strrchr(path, '/');
    const char* b2 = strrchr(path, '\\');
    if (b2 != NULL && (b == NULL || b2 > b)) b = b2;
    b = (b != NULL) ? b + 1 : path;
    if (*b == '\0') return 0;
    char name[256];
    snprintf(name, sizeof(name), "%s", b);
    const char* base = name;
    if (strlen(name) > 3 && strncmp(name, "lib", 3) == 0) base = name + 3;

    char cwd[4096];
#if defined(_WIN32) || defined(_WIN64)
    if (_getcwd(cwd, sizeof(cwd)) == NULL) return 0;
#else
    if (getcwd(cwd, sizeof(cwd)) == NULL) return 0;
#endif
    char cwd0[4096];   /* CWD originale: il walk-up sotto muta `cwd` in place */
    snprintf(cwd0, sizeof(cwd0), "%s", cwd);

    /* (0) LOCALE AL PROGRAMMA (bundle del compilatore): <exe>/libs/<rid>, <exe>/libs, <exe>.
     * Ha la PRECEDENZA su tutto: un programma distribuito con `--libs` non deve dipendere
     * dalla cache né dal checkout della macchina. */
    {
        char exed[4096];
        if (gmb_paths_exe_dir(exed, sizeof(exed)) && exed[0]) {
            char libsdir[8192];
            snprintf(libsdir, sizeof(libsdir), "%s/libs/%s", exed, gmb_libs_rid());
            if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
            snprintf(libsdir, sizeof(libsdir), "%s/libs", exed);
            if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
            if (dl_try_in_root(exed, name, base, out, cap)) return 1;
            /* <progetto>/libs quando l'eseguibile sta in <progetto>/bin */
            snprintf(libsdir, sizeof(libsdir), "%s/../libs", exed);
            if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
        }
    }

    /* (1) risalendo dal CWD (nearest-first) */
    for (int up = 0; up < 16; up++) {
        char libsdir[8192];
        snprintf(libsdir, sizeof(libsdir), "%s/libs", cwd);
        if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
        /* `shared/libs`: le nostre librerie interne vivono qui (condivise dai due compilatori). */
        snprintf(libsdir, sizeof(libsdir), "%s/shared/libs", cwd);
        if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
        /* artefatti di BUILD dei sotto-progetti: `<root>/build/libs/<rid>/` (prima
         * `libs-runtime`, cartella che nel checkout non esiste più). */
        snprintf(libsdir, sizeof(libsdir), "%s/build/libs/%s", cwd, gmb_libs_rid());
        if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
        char* sep = strrchr(cwd, '/');
#if defined(_WIN32) || defined(_WIN64)
        char* sep2 = strrchr(cwd, '\\');
        if (sep2 != NULL && (sep == NULL || sep2 > sep)) sep = sep2;
#endif
        if (sep != NULL) *sep = '\0';
        else break;
    }

    /* (2) ASSOLUTO dall'install GMBasic (indipendente dalla posizione del programma) */
    {
        char root[4096];
        if (gmb_paths_repo_root(root, sizeof(root)) && root[0]) {
            char libsdir[8192];
            snprintf(libsdir, sizeof(libsdir), "%s/libs", root);
            if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
            snprintf(libsdir, sizeof(libsdir), "%s/shared/libs", root);
            if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
            /* artefatti di build dei sotto-progetti (prima `libs-runtime/<rid>/`, poi
             * `Resources/libs/<rid>/`: entrambe non esistono più nel checkout). */
            snprintf(libsdir, sizeof(libsdir), "%s/build/libs/%s", root, gmb_libs_rid());
            if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
        }
    }

    /* (2b) CACHE ON-DEMAND delle librerie native: <cache>/<rid> (GMBasicModules → ~/.gmbasic/libs).
     * Le librerie non stanno più nel repo: chi le ha già scaricate le trova qui. */
    {
        char libcache[4096];
        gmb_paths_libs_cache_dir(libcache, sizeof(libcache));
        if (libcache[0]) {
            char libsdir[8192];
            const char* rid = gmb_libs_rid();   /* una sola mappatura (era osx-arm64 hardcoded) */
            snprintf(libsdir, sizeof(libsdir), "%s/%s", libcache, rid);
            if (dl_try_in_root(libsdir, name, base, out, cap)) return 1;
        }
    }

    /* (2c) DOWNLOAD ON-DEMAND: la libreria non c'è in nessun path → la scarica se l'eseguibile
     * porta accanto un `libs.txt` (emesso dal build: nome<TAB>sha256<TAB>size<TAB>url) e la mette
     * nella cache (~/.gmbasic/libs/<rid>/) con verifica sha256. Senza `libs.txt` è un no-op: il
     * comportamento resta quello di prima (ritorna 0 → il chiamante segnala "non trovata").
     *
     * NOMI: il DECLARE porta il nome LOGICO (`gmbamqp`), mentre l'indice elenca i FILE reali
     * (`libgmbamqp.dylib`). Chiedere l'indice col solo nome logico non trovava mai la riga e il
     * download non partiva: la gamba C diceva "libreria non trovata" mentre il C# (che prova i
     * candidati `lib<n>.ext` / `<n>.ext`) la scaricava. Qui si usano gli stessi candidati, più il
     * nome cercato tal quale (per gli indici che elencano<exe>… file senza prefisso/estensione). */
    {
        char got[8192];
        const char* exts[] = { ".dylib", ".so", ".dll" };
        int have = 0;
        size_t i;
        for (i = 0; i < sizeof(exts) / sizeof(exts[0]) && !have; i++) {
            char cand[512];
            snprintf(cand, sizeof cand, "lib%s%s", base, exts[i]);
            if (gmb_libs_autoensure(cand, got, sizeof got) && got[0]) { have = 1; break; }
            snprintf(cand, sizeof cand, "%s%s", base, exts[i]);
            if (gmb_libs_autoensure(cand, got, sizeof got) && got[0]) { have = 1; break; }
        }
        if (!have && gmb_libs_autoensure(name, got, sizeof got) && got[0]) have = 1;
        if (have) {
            snprintf(out, cap, "%s", got);
            return 1;
        }
    }

    /* (3) FALLBACK: lib PIATTA nella cartella del programma (CWD): mio.bas + libfoo.dylib. */
    {
        const char* exts2[] = { ".dylib", ".so", ".dll" };
        for (size_t i = 0; i < sizeof(exts2) / sizeof(exts2[0]); i++) {
            char sc[8192];
            snprintf(sc, sizeof(sc), "%s/%s%s", cwd0, name, exts2[i]);
            FILE* sf = fopen(sc, "rb");
            if (sf) { fclose(sf); snprintf(out, cap, "%s", sc); return 1; }
            snprintf(sc, sizeof(sc), "%s/lib%s%s", cwd0, base, exts2[i]);
            sf = fopen(sc, "rb");
            if (sf) { fclose(sf); snprintf(out, cap, "%s", sc); return 1; }
        }
    }
    return 0;
}

/* --- Auto-discovery cache pacchetti GMB (~/.gmbasic/packages) -----------------------
 * Cerca <cache>/<pkg>/<ver>/lib/<platform>/<name><ext> o lib<base><ext> per la lib
 * richiesta via DECLARE LIB "<name>". Rende risolvibile un pacchetto REQUIRE senza
 * che il chiamante debba pre-caricarlo. Override cache: GMB_PKG_CACHE.
 * Kill-switch: GMB_FFI_NO_PKGCACHE_SCAN=1. */
static int gmb_ffi_dl_find_pkgcache(const char* path, char* out, size_t cap)
{
    const char* no = gmb_opt_env("GMB_FFI_NO_PKGCACHE_SCAN");
    if (no != NULL && strcmp(no, "1") == 0) return 0;
#if !defined(_WIN32) && !defined(_WIN64)
    const char* b = strrchr(path, '/');
    b = (b != NULL) ? b + 1 : path;
    if (*b == '\0') return 0;
    char name[256];
    snprintf(name, sizeof(name), "%s", b);
    const char* base = name;
    if (strlen(name) > 3 && strncmp(name, "lib", 3) == 0) base = name + 3;

    /* Ordine DOCUMENTATO (manuale VI.38 §7bis.1): vince `paths.packagesDir`; la chiave storica
     * `GMB_PKG_CACHE` resta solo come ripiego quando le impostazioni non danno una cartella. */
    char cachebuf[4096];
    cachebuf[0] = '\0';
    gmb_paths_packages_dir(cachebuf, sizeof(cachebuf));   /* paths.packagesDir, altrimenti il default */
    const char* cache = cachebuf;
    if (cache[0] == '\0') {
        const char* legacy = gmb_opt_env("GMB_PKG_CACHE");
        if (legacy && *legacy) cache = legacy;
    }
    /* INSTALLAZIONE LOCALE dell'app (<dir dell'eseguibile>/packages, modello node_modules):
     * ha la PRECEDENZA sulla cache. Stessa regola del compilatore (gmb_pkg_collect_requires),
     * così si compila con un .inc e si carica la dylib DELLO STESSO pacchetto. */
    char localbuf[4096];
    localbuf[0] = '\0';
    {
        char exed[4096];
        if (gmb_paths_exe_dir(exed, sizeof(exed)) && exed[0])
            snprintf(localbuf, sizeof(localbuf), "%s/packages", exed);
    }
#if defined(__APPLE__)
    const char* plat = "osx-arm64";
#else
    const char* plat = "linux-x64";
#endif
    const char* exts[] = { ".dylib", ".so", ".dll" };
    /* Ordine: installazione LOCALE dell'app, poi cache condivisa. */
    const char* roots[2];
    int nroots = 0;
    if (localbuf[0]) roots[nroots++] = localbuf;
    roots[nroots++] = cache;
    int found = 0;
    for (int r = 0; r < nroots && !found; r++) {
        DIR* d = opendir(roots[r]);
        if (d == NULL) continue;
        struct dirent* pd;
        while (!found && (pd = readdir(d)) != NULL) {
            char pdir[4096];
            if (pd->d_name[0] == '.') continue;
            snprintf(pdir, sizeof(pdir), "%s/%s", roots[r], pd->d_name);
            DIR* dv = opendir(pdir);
            if (dv == NULL) continue;
            struct dirent* vd;
            while (!found && (vd = readdir(dv)) != NULL) {
                size_t i;
                if (vd->d_name[0] == '.') continue;
                for (i = 0; i < sizeof(exts) / sizeof(exts[0]) && !found; i++) {
                    char cand[8192];
                    FILE* f;
                    snprintf(cand, sizeof(cand), "%s/%s/lib/%s/%s%s", pdir, vd->d_name, plat, name, exts[i]);
                    f = fopen(cand, "rb");
                    if (f) { fclose(f); snprintf(out, cap, "%s", cand); found = 1; break; }
                    snprintf(cand, sizeof(cand), "%s/%s/lib/%s/lib%s%s", pdir, vd->d_name, plat, base, exts[i]);
                    f = fopen(cand, "rb");
                    if (f) { fclose(f); snprintf(out, cap, "%s", cand); found = 1; break; }
                }
            }
            closedir(dv);
        }
        closedir(d);
    }
    return found;
#else
    (void)path; (void)out; (void)cap;
    return 0;
#endif
}

/* Registra una libreria di pacchetto (path assoluto): dlopen subito (così i suoi
 * simboli dipendenti si risolvono) e conserva il basename per il match di nome. */
void gmb_ffi_dl_preload(const char* abs_path)
{
    if (abs_path == NULL || *abs_path == '\0') return;
    if (g_preloaded_count >= GMB_FFI_DL_CACHE_MAX) return;
    for (int i = 0; i < g_preloaded_count; i++)
        if (strcmp(g_preloaded[i].path, abs_path) == 0) return;

    void* h = GMB_DL_OPEN(abs_path);
    if (h == NULL) return;

    gmb_ffi_dl_entry_t* e = &g_preloaded[g_preloaded_count++];
    e->handle = h;
    strncpy(e->path, abs_path, sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = '\0';
    gmb_ffi_dl_basename(abs_path, e->base, sizeof(e->base));
}

/* Apre una libreria: cache path→handle, fallback estensioni, poi match per basename
 * sulle librerie pre-caricate (i pacchetti REQUIRE). Ritorna NULL se non trovata.
 *
 * ISOLAMENTO (C-Autonomous-Flow-TODO 11.3): qui si caricano SOLO librerie native
 * (.dylib/.so/.dll native). dlopen/LoadLibrary NON possono caricare assembly .NET
 * (managed): falliscono con errore OS (es. ERROR_BAD_EXE_FORMAT su Windows). Il
 * runtime C non carica MAI codice gestito — nessun ponte C→C#. */
void* gmb_ffi_dl_open(const char* path)
{
    if (path == NULL || *path == '\0') return NULL;

    void* h = GMB_DL_OPEN(path);
    if (h != NULL) return h;

    char buf[1152];
    const char* exts[] = { ".dylib", ".so", ".dll" };
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
    {
        snprintf(buf, sizeof(buf), "%s%s", path, exts[i]);
        h = GMB_DL_OPEN(buf);
        if (h != NULL) return h;
    }

    /* auto-discovery `libs/` (drop-in FFI, 2026-09-22): <dir>/libs/ risalendo dal CWD */
    char libsPath[8192];
    if (gmb_ffi_dl_find_libs(path, libsPath, sizeof(libsPath)))
    {
        gmb_ffi_dl_preload(libsPath);   /* dlopen + registra per basename */
        for (int i = 0; i < g_preloaded_count; i++)
            if (strcmp(g_preloaded[i].path, libsPath) == 0)
                return g_preloaded[i].handle;
    }

    /* auto-discovery cache pacchetti GMB (~/.gmbasic/packages): lib dei pacchetti REQUIRE */
    char pkgPath[8192];
    if (gmb_ffi_dl_find_pkgcache(path, pkgPath, sizeof(pkgPath)))
    {
        gmb_ffi_dl_preload(pkgPath);
        for (int i = 0; i < g_preloaded_count; i++)
            if (strcmp(g_preloaded[i].path, pkgPath) == 0)
                return g_preloaded[i].handle;
    }

    /* match per basename sulle librerie pre-caricate dei pacchetti */
    char want[128];
    gmb_ffi_dl_basename(path, want, sizeof(want));
    if (want[0] != '\0')
        for (int i = 0; i < g_preloaded_count; i++)
            if (strcmp(g_preloaded[i].base, want) == 0)
                return g_preloaded[i].handle;

    /* NON SILENZIOSO (direttiva: mai risultati silenziosi). Un DECLARE LIB che non trova la
     * libreria restituiva 0 come se la funzione fosse stata eseguita: il programma proseguiva
     * con dati falsi. Ora lo si dice UNA volta su stderr, con i posti cercati, e il valore di
     * ritorno resta 0 (le sonde tipo `AVAILABLE()` continuano a funzionare). */
    {
        static const char* warned[16];
        static int nwarned = 0;
        int already = 0;
        for (int i = 0; i < nwarned; i++) if (strcmp(warned[i], path) == 0) { already = 1; break; }
        if (!already) {
            if (nwarned < (int)(sizeof(warned) / sizeof(warned[0]))) warned[nwarned++] = path;
            {
                char cachebuf2[4096];
                cachebuf2[0] = '\0';
                gmb_paths_packages_dir(cachebuf2, sizeof(cachebuf2));   /* vince paths.packagesDir */
                const char* eff = cachebuf2;
                if (eff[0] == '\0') { const char* legacy = gmb_opt_env("GMB_PKG_CACHE"); if (legacy && *legacy) eff = legacy; }
                fprintf(stderr, "[FFI] libreria '%s' NON trovata: cercata come file, in <progetto>/libs, "
                                "in <dir dell'eseguibile>/packages e in %s. Installala con "
                                "`gmb pkg install <pacchetto>` (o --local).\n", path, eff ? eff : "?");
                fflush(stderr);
            }
        }
    }
    return NULL;
}

/* Risolve un simbolo esportato; NULL se libreria/simbolo assenti. */
void* gmb_ffi_dl_sym(void* lib, const char* name)
{
    if (lib == NULL || name == NULL) return NULL;
    return GMB_DL_SYM(lib, name);
}
