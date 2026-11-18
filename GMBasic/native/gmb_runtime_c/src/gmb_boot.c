/* gmb_boot.c — PUNTO UNICO per: dove stanno le librerie, come si scaricano, come si caricano.
 *
 * FINALITÀ (direttiva 2026-10-29). Esiste UNA sola radice di ricerca — la cache
 * `~/.gmbasic/libs/<rid>/` — e UN solo risolutore, questo. Il mondo C# non cerca più nulla:
 * chiede `gmb_lib_path("gmb_gfx")` e fa `NativeLibrary.Load(path)`; il mondo C usa
 * `gmb_lib_open("gmb_gfx")` che fa path + dlopen (+ download se manca).
 *
 * Dipendenze volutamente minime (libSystem + libcurl): questa dylib deve poter essere caricata
 * SEMPRE, prima di ogni altra, quindi non linka SDL/gfx/AI.
 *
 * BOOTSTRAP. `gmb_bootstrap(scope)` scarica dal manifest piatto del RID
 * (`index-<rid>.txt`, pubblicato come asset nella release stabile `libs-index`) tutti i file
 * dello scope richiesto, con progress bar su terminale (`==> nome  [===>   ] 42%`).
 * Scope: RUNTIME (eseguire), DEV (compilare: header + statiche), COMPONENTS (torch/chromium/…),
 * ALL (tutto).
 *
 * NIENTE PERCORSI ALTERNATIVI: non c'è più `Resources/libs`, non c'è `libs-runtime`. L'unica
 * eccezione ammessa è il file accanto all'eseguibile (`@loader_path`), per il deploy offline.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "gmb_runtime.h"

/* API del downloader per-file (gmb_libs_fetch.c): qui si usano direttamente, così `gmb_boot`
 * non dipende dal resto del runtime (che le dichiara solo nel header pubblico). */
int gmb_libs_ensure_file_from(const char* index_path, const char* fileName, char* out_path, size_t cap);
int gmb_libs_autoensure(const char* fileName, char* out_path, size_t cap);

#if defined(_WIN32)
#  include <windows.h>
#  define GMB_BOOT_OPEN(p) ((void*)LoadLibraryA(p))
#else
#  include <dlfcn.h>
#  define GMB_BOOT_OPEN(p) dlopen((p), RTLD_NOW | RTLD_LOCAL)
#endif

#if __has_include(<curl/curl.h>)
#include <curl/curl.h>
#define GMB_BOOT_HAVE_CURL 1
#endif

/* Release STABILE dell'indice piatto (aggiornata con --clobber, URL mai mutevole). */
#define GMB_INDEX_URL_FMT \
    "https://github.com/c-vision/GMBasicModules/releases/download/libs-index/index-%s.txt"

/* ── RID: definito in gmb_paths.c (core), così anche il runtime lo vede ─────────────────── */

/* ── cache ───────────────────────────────────────────────────────────────────────────────── */
int gmb_cache(char* out, size_t cap)
{
    if (!out || cap == 0) return 0;
    /* Stessa FONTE del resto del runtime: `paths.libsCacheDir` di system.settings.json
     * (default `~/.gmbasic/libs`). Prima era hardcoded: con la chiave impostata il bootstrap
     * scaricava in `~/.gmbasic/libs` mentre `libs`/loader/link leggevano la cache dei settings
     * → due cache, download in una e link nell'altra (bug trovato il 2026-11-16).
     * Questo modulo include già `gmb_paths.c`: nessuna dipendenza nuova. */
    out[0] = '\0';
    gmb_paths_libs_cache_dir(out, cap);
    return out[0] != '\0';
}

/* ── progress bar (solo su TTY; rate-limited) ────────────────────────────────────────────── */
static int gmb_tty(void)
{
#if defined(_WIN32)
    return 0;
#else
    return isatty(fileno(stderr)) ? 1 : 0;
#endif
}

void gmb_progress(const char* label, double frac01)
{
    if (!gmb_tty()) return;
    if (frac01 < 0) frac01 = 0;
    if (frac01 > 1) frac01 = 1;
    const int width = 24;
    int fill = (int)(frac01 * width);
    fprintf(stderr, "\r  %-28.28s [", label);
    for (int i = 0; i < width; i++) fputc(i < fill ? '=' : ' ', stderr);
    fprintf(stderr, "] %3d%%", (int)(frac01 * 100));
    if (frac01 >= 1) fputc('\n', stderr);
    fflush(stderr);
}

/* ── path: cache, artefatti di BUILD (se più recenti), file accanto all'eseguibile ─────────
 * L'ordine NON è «cache sempre»: in sviluppo `<root>/build/libs/<rid>/` contiene il motore appena
 * compilato, e la cache contiene il bundle PUBBLICATO (runtime-v1). Se la cache viene preferita
 * comunque, il REPL/Executor carica codice VECCHIO in silenzio mentre le gambe C (link statico)
 * usano i sorgenti correnti: divergenze fantasma su llm/* (bug (open) - Lib-AI-Pubblicate-Stale-
 * Cache-IL-PCode, misurato 2026-10-29: cache `62585711758b` vs build `b735c2210418`).
 * Regola: si sceglie il file con mtime PIÙ RECENTE fra build e cache; a parità, la cache (che ha
 * lo sha verificato dall'indice). In un'app installata `build/libs` non esiste: comportamento di
 * prima (cache → accanto all'eseguibile). */
static int gmb_mtime_newer(const char* a, const char* b)
{
    struct stat sa, sb;
    if (stat(a, &sa) != 0) return 0;
    if (stat(b, &sb) != 0) return 1;
    return sa.st_mtime > sb.st_mtime;
}

int gmb_lib_path(const char* name, char* out, size_t cap)
{
    if (!name || !out || cap == 0) return 0;
    char cachep[2400] = "", buildp[2400] = "";
    int have_cache = 0, have_build = 0;
    char cache[2048];
    if (gmb_cache(cache, sizeof cache))
    {
        struct stat st;
        snprintf(cachep, sizeof cachep, "%s/%s/%s", cache, gmb_rid(), name);
        if (stat(cachep, &st) == 0) have_cache = 1;
    }
    /* sviluppo: artefatto appena costruito (solo se esiste). `gmb_paths_repo_root` può indicare la
     * radice del MONOREPO o quella di GMBasic a seconda del chiamante: si provano entrambe (gli
     * artefatti stanno in <GMBasic>/build/libs/<rid>). */
    {
        char root[2048];
        if (gmb_paths_repo_root(root, sizeof root) && root[0])
        {
            static const char* tails[] = { "/build/libs", "/GMBasic/build/libs" };
            for (int t = 0; t < 2 && !have_build; ++t)
            {
                struct stat st;
                snprintf(buildp, sizeof buildp, "%s%s/%s/%s", root, tails[t], gmb_rid(), name);
                if (stat(buildp, &st) == 0) have_build = 1;
            }
        }
    }
    if (have_build && (!have_cache || gmb_mtime_newer(buildp, cachep)))
    {
        snprintf(out, cap, "%s", buildp);
        return 1;
    }
    if (have_cache)
    {
        snprintf(out, cap, "%s", cachep);
        return 1;
    }
    /* deploy offline: la lib sta accanto all'eseguibile */
    {
        char dir[2048];
        if (gmb_paths_exe_dir(dir, sizeof dir) && dir[0])
        {
            char p[2400];
            snprintf(p, sizeof p, "%s/%s", dir, name);
            struct stat st;
            if (stat(p, &st) == 0) { snprintf(out, cap, "%s", p); return 1; }
        }
    }
    out[0] = '\0';
    return 0;
}

static void lf_note_path(const char* dir, const char* name, char* out, size_t cap)
{
    snprintf(out, cap, "%s/%s", dir, name);
}

/* ── path "any": dal nome usato da P/Invoke al file nella cache ─────────────────────────────
 * `DllImport("SDL2")` → il file si chiama `libSDL2.dylib` (macOS), `libSDL2.so` (Linux),
 * `SDL2.dll` o `libSDL2.dll` (Windows). Questa mappatura sta QUI, non nel C#: il mondo gestito
 * passa il nome grezzo e non deve conoscere né le convenzioni né le estensioni. */
int gmb_lib_path_any(const char* name, char* out, size_t cap)
{
    if (!name || !*name) return 0;
    /* già con estensione (es. "gmb_gfx.dylib" o "libgmb_gfx.dylib") */
    if (strchr(name, '.') && gmb_lib_path(name, out, cap)) return 1;

    static const char* exts[] = {
#if defined(_WIN32)
        ".dll", ".so", ".dylib"
#elif defined(__APPLE__)
        ".dylib", ".so", ".dll"
#else
        ".so", ".dylib", ".dll"
#endif
    };
    char cand[512];
    for (size_t i = 0; i < sizeof exts / sizeof exts[0]; i++)
    {
        snprintf(cand, sizeof cand, "lib%s%s", name, exts[i]);   /* libSDL2.dylib */
        if (gmb_lib_path(cand, out, cap)) return 1;
        snprintf(cand, sizeof cand, "%s%s", name, exts[i]);      /* SDL2.dll */
        if (gmb_lib_path(cand, out, cap)) return 1;
    }
    /* nomi versionati nella cache (libzstd.1.5.7.dylib): cerca il prefisso più corto che combacia */
    {
        char dir[2048];
        if (gmb_cache(dir, sizeof dir))
        {
            char full[2304];
            snprintf(full, sizeof full, "%s/%s", dir, gmb_rid());
            DIR* d = opendir(full);
            if (d)
            {
                struct dirent* e;
                char best[256] = "";
                while ((e = readdir(d)) != NULL)
                {
                    if (strncmp(e->d_name, "lib", 3) != 0) continue;
                    if (strncmp(e->d_name + 3, name, strlen(name)) != 0) continue;
                    if (!best[0] || strlen(e->d_name) < strlen(best)) snprintf(best, sizeof best, "%s", e->d_name);
                }
                closedir(d);
                if (best[0]) { lf_note_path(full, best, out, cap); return 1; }
            }
        }
    }
    return 0;
}

/* ── open: path + download on-demand + dlopen ───────────────────────────────────────────── */
int gmb_lib_open(const char* name)
{
    char path[2600];
    if (!gmb_lib_path(name, path, sizeof path))
    {
        if (!gmb_libs_autoensure(name, NULL, 0)) return 0;   /* scarica (sha256) */
        if (!gmb_lib_path(name, path, sizeof path)) return 0;
    }
    void* h = GMB_BOOT_OPEN(path);
    return h != NULL;
}

/* ── componenti esterni (torch/chromium/obscura/ocr): <cache>/components/<nome>/<rid>/ ────── */
int gmb_component(const char* name, char* out, size_t cap)
{
    char cache[2048];
    if (!gmb_cache(cache, sizeof cache)) { if (cap) out[0] = '\0'; return 0; }
    /* <cache>/<componente>/<rid>/  — la stessa disposizione di `GmbiPaths.MlDir/ChromiumDir/…`
     * e dei downloader (`gmb install --components`): una sola radice, nessuna sottocartella
     * "components/" che il C# non conosce. */
    snprintf(out, cap, "%s/%s/%s", cache, name, gmb_rid());
    return 1;
}

/* ── bootstrap: scarica l'indice piatto del RID e TUTTI i file dello scope ────────────────── */

/* scarica `url` in `dest` (senza sha: serve solo per l'indice). PUBBLICA perché il primo uso
 * (`gmb_first_use.c`) scarica l'indice di RID che non sono quello host. */
int gmb_fetch_url(const char* url, const char* dest)
{
#if !defined(GMB_BOOT_HAVE_CURL)
    (void)url; (void)dest;
    fprintf(stderr, "[boot] compilato senza libcurl: impossibile scaricare l'indice\n");
    return 0;
#else
    CURL* c = curl_easy_init();
    if (!c) return 0;
    FILE* f = fopen(dest, "wb");
    if (!f) { curl_easy_cleanup(c); return 0; }
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "gmbasic-boot/1");
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    fclose(f);
    if (rc != CURLE_OK) { remove(dest); return 0; }
    return 1;
#endif
}

/* conta le righe dell'indice che appartengono allo scope richiesto (per la barra di avanzamento) */
static int boot_count_scope(const char* index_path, int scope)
{
    FILE* f = fopen(index_path, "rb");
    if (!f) return 0;
    char line[4096];
    int n = 0;
    while (fgets(line, sizeof line, f))
    {
        if (!strchr(line, '\t')) continue;
        /* lo scope è codificato nella 5ª colonna opzionale: "runtime|dev|component:torch" */
        const char* tag = NULL;
        int tabs = 0;
        for (const char* p = line; *p; p++) if (*p == '\t' && ++tabs == 4) { tag = p + 1; break; }
        if (!tag) { n++; continue; }                       /* senza tag = runtime */
        if (scope & GMB_SCOPE_ALL) n++;
        else if (scope & GMB_SCOPE_DEV && strncmp(tag, "dev", 3) == 0) n++;
        else if (scope & GMB_SCOPE_COMPONENTS && strncmp(tag, "component", 9) == 0) n++;
        else if (scope & GMB_SCOPE_RUNTIME && strncmp(tag, "runtime", 7) == 0) n++;
    }
    fclose(f);
    return n;
}

/* garantisce i file dell'indice che rientrano nello scope (la barra è globale) */
static int boot_ensure_scope(const char* index_path, int scope, const char* rid)
{
    FILE* f = fopen(index_path, "rb");
    if (!f) return 0;
    const int total = boot_count_scope(index_path, scope);
    int done = 0, fail = 0;
    char line[4096];
    while (fgets(line, sizeof line, f))
    {
        char* nl = strchr(line, '\n'); if (nl) *nl = '\0';
        char* t1 = strchr(line, '\t'); if (!t1) continue;
        char name[512];
        snprintf(name, sizeof name, "%.*s", (int)(t1 - line), line);
        const char* tag = NULL;
        int tabs = 0;
        for (const char* p = line; *p; p++) if (*p == '\t' && ++tabs == 4) { tag = p + 1; break; }
        int in_scope = (scope & GMB_SCOPE_ALL)
                    || (!tag)
                    || (scope & GMB_SCOPE_DEV && strncmp(tag, "dev", 3) == 0)
                    || (scope & GMB_SCOPE_COMPONENTS && strncmp(tag, "component", 9) == 0)
                    || (scope & GMB_SCOPE_RUNTIME && strncmp(tag, "runtime", 7) == 0);
        if (!in_scope) continue;
        /* macOS: gli archivi della toolchain zig per `osx-*` NON si scaricano — DECISIONE
         * (2026-11-16): su macOS si usa `cc`, lo zig sull'host è la parte problematica
         * (AGENTS §"DUE COMPILATORI"), e gli archivi `osx-*` sono in dismissione. Prima il
         * primo uso scaricava ~50 MB di zig per poi non installarlo. */
        if (strncmp(rid, "osx", 3) == 0 && strncmp(name, "zig-", 4) == 0) continue;
        gmb_progress(name, total ? (double)done / total : 0.0);
        if (!gmb_libs_ensure_file_for_rid(index_path, rid, name, NULL, 0)) { fprintf(stderr, "[boot] FALLITO: %s\n", name); fail++; }
        done++;
        gmb_progress(name, total ? (double)done / total : 1.0);
    }
    fclose(f);
    if (fail) fprintf(stderr, "[boot] %d file NON scaricati\n", fail);
    return fail == 0;
}

/* Bootstrap di un RID ESPLICITO: serve al primo uso con ambito "all" (tutti e 6 i RID),
 * dove il RID non è quello della macchina. `gmb_bootstrap` resta il caso host. */
int gmb_bootstrap_rid(const char* rid_explicit, int scope)
{
    const char* rid = (rid_explicit && *rid_explicit) ? rid_explicit : gmb_rid();
    if (scope == 0) scope = GMB_SCOPE_RUNTIME;
    char cache[2048];
    if (!gmb_cache(cache, sizeof cache)) return 0;
    char dir[2304], index[2400], url[2600];
    snprintf(dir, sizeof dir, "%s/%s", cache, rid);
    snprintf(index, sizeof index, "%s/index.txt", dir);
    /* mkdir -p della cache del RID */
    {
        char tmp[2400];
        snprintf(tmp, sizeof tmp, "%s", dir);
        for (char* p = tmp + 1; *p; p++) if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
        mkdir(tmp, 0755);
    }
    struct stat st;
    if (stat(index, &st) != 0)
    {
        snprintf(url, sizeof url, GMB_INDEX_URL_FMT, rid);
        fprintf(stderr, "==> indice librerie per %s\n", rid);
        if (!gmb_fetch_url(url, index))
        {
            fprintf(stderr, "[boot] indice non scaricabile (%s)\n"
                            "       rete assente? `gmb install --from <cartella>` per l'offline.\n", url);
            return 0;
        }
    }
    fprintf(stderr, "==> scarico le librerie (%s)\n",
            scope == GMB_SCOPE_ALL ? "tutte" : (scope & GMB_SCOPE_DEV) ? "runtime+dev" :
            (scope & GMB_SCOPE_COMPONENTS) ? "componenti" : "runtime");
    return boot_ensure_scope(index, scope, rid);
}

int gmb_bootstrap(int scope) { return gmb_bootstrap_rid(gmb_rid(), scope); }
