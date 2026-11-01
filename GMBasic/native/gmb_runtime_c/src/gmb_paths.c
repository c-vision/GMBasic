/* FILE: native/gmb_runtime_c/src/gmb_paths.c
 * Risoluzione centralizzata dei percorsi dati di GMBASIC per il MONDO C (§0bis: C# e C
 * sono runtime SEPARATI e NON condividono codice). Questo modulo è il corrispettivo C di
 * CommonLib.GmbiPaths.cs (C#): scopre la root del repo, trova system.settings.json e legge
 * la sezione "paths". I backend C (VM/C, C99, C99Native) chiamano queste funzioni — MAI path
 * ~/.gmbasic hardcoded (ora configurabile in system.settings.json sezione "paths").
 *
 * FONTE: la sezione "paths" di system.settings.json. File cercato in quest'ordine:
 *   1. <root>/config/system.settings.json          (copia locale EDITABILE, git-ignorata)
 *   2. <root>/Resources/system.settings.json       (template versionato)
 *   3. <cwd>/system.settings.json                  (distribuzione)
 * dove <root> = GMBASIC_ROOT (env) oppure la dir che contiene GMBASIC.sln risalendo dalla
 * cwd, altrimenti la cwd. Default identici al C#: config/, skills/, rag/, graphs/, brains/,
 * build/libs/<rid>, componenti in <cache>/<comp>/<rid>, cache in /tmp del SO.
 *
 * NB: il file può contenere commenti // (stile JSONC) → strip prima del parse.
 */

#include "gmb_runtime.h"
#include <unistd.h>
#include <sys/stat.h>   /* stat/mkdir: migrazione ~/.gmb → ~/.gmbasic */
#include <ctype.h>
#include <dirent.h>   /* su Windows: shared/include/win32/dirent.h */
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

/* ── RID ───────────────────────────────────────────────────────────────────────────────────
 * Identità della piattaforma per cache e artefatti (`<cache>/<rid>/`, `build/libs/<rid>`).
 * Vive nel CORE (non più in `gmb_boot.c`, che è un target a parte): serve anche al runtime
 * per trovare i plugin dei codec nella cache.
 * Windows ARM64 va riconosciuto: prima ogni Windows rispondeva "win-x64" e su un client
 * arm64 si cercava la cache sbagliata. */
const char* gmb_rid(void)
{
#if defined(_WIN32) || defined(_WIN64)
#  if defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
    return "win-arm64";
#  else
    return "win-x64";
#  endif
#elif defined(__APPLE__)
#  if defined(__aarch64__) || defined(__arm64__)
    return "osx-arm64";
#  else
    return "osx-x64";
#  endif
#elif defined(__aarch64__) || defined(__arm64__)
    return "linux-arm64";
#else
    return "linux-x64";
#endif
}

/* ========================================================================
 * Internals: cache della root + contenuto del file (senza commenti)
 * ======================================================================== */

static char g_pr_root[1024] = "";
static char g_pr_settings[1024] = "";
static char g_pr_json[16384];      /* contenuto system.settings.json (commenti rimossi) */
static int  g_pr_loaded = 0;
/* `app.settings.json` (sezioni di RUNTIME, unite dal compilatore): i suoi `paths` vincono su
 * quelli globali — un compilato distribuito porta SOLO questo file, non `system.settings.json`. */
static char g_pr_app_json[16384];
static size_t g_pr_app_json_len = 0;
static size_t g_pr_json_len = 0;

/* Rimuove i commenti '//' e a blocchi rispettando le stringhe "...". Scrive in `out`. */
static void pr_strip_comments(const char* src, char* out, size_t cap)
{
    size_t oi = 0;
    int in_str = 0;
    for (const char* p = src; *p && oi + 1 < cap; p++)
    {
        if (in_str)
        {
            if (*p == '\\' && p[1]) { if (oi + 2 < cap) { out[oi++] = *p; out[oi++] = p[1]; } p++; continue; }
            out[oi++] = *p;
            if (*p == '"') in_str = 0;
            continue;
        }
        if (*p == '"') { out[oi++] = *p; in_str = 1; continue; }
        if (*p == '/' && p[1] == '/') { while (*p && *p != '\n') p++; if (*p) p--; continue; }
        if (*p == '/' && p[1] == '*')
        {
            p += 2;
            while (*p && !(*p == '*' && p[1] == '/')) p++;
            if (*p) p++;            /* skip '*' */
            if (*p) continue;       /* skip '/' (il for farà p++) */
            continue;
        }
        out[oi++] = *p;
    }
    out[oi] = '\0';
}

/* Cerca la sezione "paths": { ... } (profondità 1) nel JSON pulito. Ritorna ptr dopo '{'. */
static const char* pr_paths_obj(const char* js)
{
    const char* p = strstr(js, "\"paths\"");
    if (!p) return NULL;
    p += strlen("\"paths\"");
    while (*p && *p != ':') p++;
    if (*p == ':') p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '{') return NULL;
    return p + 1;
}

/* Legge il valore stringa di una chiave dentro l'oggetto paths. Ritorna 1 se trovata e
 * valorizzata (non null/stringa vuota), 0 altrimenti. Copia in `out` fino a `cap`. */
static int pr_paths_value(const char* key, char* out, size_t cap)
{
    /* APP prima del GLOBALE: `app.settings.json` è l'unico file che un compilato distribuito
     * porta con sé, e le sue chiavi vincono su quelle di sistema. */
    const char* o = g_pr_app_json_len ? pr_paths_obj(g_pr_app_json) : NULL;
    if (!o) o = pr_paths_obj(g_pr_json);
    if (!o) return 0;
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(o, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p && *p != ':') p++;
    if (*p == ':') p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '"')
    {
        p++;
        size_t oi = 0;
        while (*p && *p != '"' && oi + 1 < cap) out[oi++] = *p++;
        out[oi] = '\0';
        return oi > 0 ? 1 : 0;
    }
    if (strncmp(p, "null", 4) == 0) return 0;
    return 0;
}

/* Espande ~ in home. Copia in out. */
static void pr_expand(const char* in, char* out, size_t cap)
{
    const char* home = getenv("HOME");
    if (in && in[0] == '~' && (in[1] == '/' || in[1] == '\0') && home && *home)
    {
        if (in[1] == '\0') { snprintf(out, cap, "%s", home); return; }
        snprintf(out, cap, "%s%s", home, in + 1);
        return;
    }
    snprintf(out, cap, "%s", in ? in : "");
}

/* Directory dell'ESEGUIBILE corrente (senza il filename). 1 se risolta. */
static int pr_exe_dir(char* out, size_t cap)
{
#if defined(_WIN32) || defined(_WIN64)
    char buf[4096];
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) return 0;
    snprintf(out, cap, "%s", buf);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t sz = (uint32_t)sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) != 0) return 0;
    snprintf(out, cap, "%s", buf);
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return 0;
    buf[n] = '\0';
    snprintf(out, cap, "%s", buf);
#endif
    char* slash = strrchr(out, '/');
    if (slash) *slash = '\0';
    return 1;
}

/* Risale da `seed` cercando GMBASIC.sln; scrive la root in g_pr_root e ritorna 1 se trovata. */
static int pr_walk_to_sln(const char* seed)
{
    char cur[2048];
    snprintf(cur, sizeof(cur), "%s", seed);
    for (;;)
    {
        char probe[4096];
        snprintf(probe, sizeof(probe), "%s/GMBASIC.sln", cur);
        if (access(probe, F_OK) == 0) { snprintf(g_pr_root, sizeof(g_pr_root), "%s", cur); return 1; }
        /* MONOREPO (nessuna .sln): marker stabile del runtime condiviso — la stessa regola
         * di ir_walk_sln nel frontend C e di GmbiPaths.DiscoverRepoRoot nel C#. Senza,
         * la root cadeva sul CWD: un programma eseguito da `shared/minitests/packages/`
         * non trovava più la cache (`~/.gmb/packages`, vuota) e il loader FFI falliva. */
        snprintf(probe, sizeof(probe), "%s/native/gmb_runtime_c/include/gmb_runtime.h", cur);
        if (access(probe, F_OK) == 0) { snprintf(g_pr_root, sizeof(g_pr_root), "%s", cur); return 1; }
        /* Monorepo con la .slnx dentro GMBasicSharp/: root = <monorepo>/GMBasic. */
        snprintf(probe, sizeof(probe), "%s/GMBasicSharp/GMBasicSharp.slnx", cur);
        if (access(probe, F_OK) == 0) {
            char gm[4096];
            snprintf(gm, sizeof(gm), "%s/GMBasic", cur);
            if (access(gm, F_OK) == 0) { snprintf(g_pr_root, sizeof(g_pr_root), "%s", gm); return 1; }
            snprintf(g_pr_root, sizeof(g_pr_root), "%s", cur); return 1;
        }
        char* slash = strrchr(cur, '/');
        if (!slash || slash == cur) break;
        *slash = '\0';
    }
    return 0;
}

extern void gmb_abi_link_anchor(void);   /* tiene gmb_abi.o nel link da libreria statica (constructor di registrazione) */

static void pr_load(void)
{
    g_pr_loaded = 1;
    gmb_abi_link_anchor();
    /* root: env GMBASIC_ROOT > GMBASIC.sln dall'ESEGUIBILE > dal CWD > cwd.
     * L'ESEGUIBILE prima del CWD: un programma BASIC creato OVUNQUE (CWD esterna) risolve
     * comunque la root dell'install GMBasic dal processo host (gmb_vm_host, REPL, ...). */
    const char* envroot = getenv("GMBASIC_ROOT");
    if (envroot && *envroot)
    {
        snprintf(g_pr_root, sizeof(g_pr_root), "%s", envroot);
    }
    else
    {
        int found = 0;
        char exedir[2048];
        if (pr_exe_dir(exedir, sizeof(exedir))) found = pr_walk_to_sln(exedir);
        if (!found)
        {
            char cwd[2048];
            if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), ".");
            if (!pr_walk_to_sln(cwd)) snprintf(g_pr_root, sizeof(g_pr_root), "%s", cwd);
        }
    }

    /* File `system.settings.json`: è CONDIVISO dai due compilatori e vive in
     * `<root>/../config/` (radice del monorepo). Ordine:
     *   1. <root>/../shared/settings/system.settings.json   CONFIG COMUNE (canonica: una sola)
     *   2. <root>/config/system.settings.json      override locale (solo se la comune non c'è:
     *                                              un checkout del solo GMBasic)
     *   3. <root>/Resources/system.settings.json   legacy (il file non sta più negli asset)
     *   4. system.settings.json nella CWD
     * La comune viene PRIMA per costruzione: una copia locale stantia non può più scavalcare
     * quella versionata (era il caso reale: due file divergenti, si usava quello sbagliato). */
    /* PRIMA la cartella dell'ESEGUIBILE: un programma compilato si distribuisce SENZA
     * compilatore e porta con sé `system.settings.json` (lo copia il build). Se il file è lì,
     * vince: il programma usa i SUOI settings, non quelli della macchina. */
    const char* cands[6];
    char e0[2048], e1[2048], c0[2048], c1[2048], c2[2048], c3[2048];
    e0[0] = e1[0] = '\0';
    {
        char exe[2048];
        if (gmb_paths_exe_dir(exe, sizeof exe) && exe[0])
        {
            snprintf(e0, sizeof(e0), "%s/system.settings.json", exe);
            snprintf(e1, sizeof(e1), "%s/config/system.settings.json", exe);
        }
    }
    snprintf(c0, sizeof(c0), "%s/../shared/settings/system.settings.json", g_pr_root);
    snprintf(c1, sizeof(c1), "%s/config/system.settings.json", g_pr_root);
    snprintf(c2, sizeof(c2), "%s/Resources/system.settings.json", g_pr_root);
    snprintf(c3, sizeof(c3), "system.settings.json");
    cands[0] = e0; cands[1] = e1; cands[2] = c0; cands[3] = c1; cands[4] = c2; cands[5] = c3;
    g_pr_settings[0] = '\0';
    for (int i = 0; i < 6; i++)
    {
        if (access(cands[i], F_OK) == 0) { snprintf(g_pr_settings, sizeof(g_pr_settings), "%s", cands[i]); break; }
    }

    /* `app.settings.json`: override dell'applicazione, e nel compilato distribuito è l'UNICO
     * file di settings (il compilatore ci fonde le sezioni di runtime di system.settings.json). */
    {
        extern int gmb_paths_app_settings(char* out, size_t cap);
        char ap[2600];
        if (gmb_paths_app_settings(ap, sizeof ap) && ap[0])
        {
            FILE* af = fopen(ap, "rb");
            if (af)
            {
                size_t an = fread(g_pr_app_json, 1, sizeof(g_pr_app_json) - 1, af);
                fclose(af);
                g_pr_app_json[an] = '\0';
                char aclean[16384];
                pr_strip_comments(g_pr_app_json, aclean, sizeof(aclean));
                snprintf(g_pr_app_json, sizeof(g_pr_app_json), "%s", aclean);
                g_pr_app_json_len = strlen(g_pr_app_json);
            }
        }
    }

    if (!g_pr_settings[0]) return;
    FILE* f = fopen(g_pr_settings, "rb");
    if (!f) return;
    size_t n = fread(g_pr_json, 1, sizeof(g_pr_json) - 1, f);
    fclose(f);
    g_pr_json[n] = '\0';
    char clean[16384];
    pr_strip_comments(g_pr_json, clean, sizeof(clean));
    snprintf(g_pr_json, sizeof(g_pr_json), "%s", clean);
    g_pr_json_len = strlen(g_pr_json);
}

/* ========================================================================
 * API pubbliche
 * ======================================================================== */

int gmb_paths_repo_root(char* out, size_t cap)
{
    if (!g_pr_loaded) pr_load();
    snprintf(out, cap, "%s", g_pr_root);
    return g_pr_root[0] ? 1 : 0;
}

/* ── Cartella `Resources/` CONDIVISA dei due mondi ────────────────────────────────────────────
 * UNA sola, alla radice del monorepo: font (`font.ttf` Cascadia, `Sixtyfour.ttf`), sprite/PNG e manifest dei
 * componenti. Prima il mondo C# aveva la sua copia in `GMBasicSharp/Resources/` e il mondo C
 * cercava `<repo>/Resources` (che nel monorepo è la cartella del solo GMBasic).
 *
 * ORDINE: CWD/Resources → risalendo dalla CWD (max 12 livelli) → <exe>/Resources →
 *         risalendo dalla dir dell'eseguibile → <repo-root>/Resources → <repo-root>/../Resources.
 * MARKER: la cartella deve contenere `font.ttf` (evita di pescare una `Resources` qualsiasi).
 * Nessun percorso alternativo silenzioso: se non la trova restituisce 0 e il chiamante decide.
 */
static int pr_has_font(const char* dir)
{
    char probe[2600];
    snprintf(probe, sizeof probe, "%s/font.ttf", dir);
    FILE* f = fopen(probe, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

int gmb_paths_resources_dir(char* out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = '\0';

    /* (1) risalendo dalla CWD (nearest-first) */
    char cur[2048];
    if (getcwd(cur, sizeof cur))
    {
        char walk[2048];
        snprintf(walk, sizeof walk, "%s", cur);
        for (int up = 0; up < 12; up++)
        {
            char cand[2600];
            snprintf(cand, sizeof cand, "%s/Resources", walk);
            if (pr_has_font(cand)) { snprintf(out, cap, "%s", cand); return 1; }
            char* sep = strrchr(walk, '/');
            if (!sep) break;
            *sep = '\0';
        }
    }

    /* (2) risalendo dalla dir dell'ESEGUIBILE (app installata: <exe>/Resources) */
    char exe[2048];
    if (gmb_paths_exe_dir(exe, sizeof exe) && exe[0])
    {
        snprintf(exe, sizeof exe, "%s", exe);
        for (int up = 0; up < 6; up++)
        {
            char cand[2600];
            snprintf(cand, sizeof cand, "%s/Resources", exe);
            if (pr_has_font(cand)) { snprintf(out, cap, "%s", cand); return 1; }
            char* sep = strrchr(exe, '/');
            if (!sep) break;
            *sep = '\0';
        }
    }

    /* (3) root del repo e root del monorepo (GMBasic/.. = Resources condivisa) */
    if (!g_pr_loaded) pr_load();
    if (g_pr_root[0])
    {
        char cand[2600];
        snprintf(cand, sizeof cand, "%s/Resources", g_pr_root);
        if (pr_has_font(cand)) { snprintf(out, cap, "%s", cand); return 1; }
        snprintf(cand, sizeof cand, "%s/../Resources", g_pr_root);
        if (pr_has_font(cand)) { snprintf(out, cap, "%s", cand); return 1; }
    }
    return 0;
}

/* `app.settings.json`: override dell'APPLICAZIONE. Si cerca nella CWD (dove sta il programma
 * mentre lo si sviluppa) e poi ACCANTO ALL'ESEGUIBILE — un compilato distribuito viene lanciato
 * da qualunque cartella e deve comunque trovare il proprio override. */
int gmb_paths_app_settings(char* out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = '\0';
    if (access("app.settings.json", F_OK) == 0) { snprintf(out, cap, "%s", "app.settings.json"); return 1; }
    char exe[2048];
    if (gmb_paths_exe_dir(exe, sizeof exe) && exe[0])
    {
        char p[2600];
        snprintf(p, sizeof(p), "%s/app.settings.json", exe);
        if (access(p, F_OK) == 0) { snprintf(out, cap, "%s", p); return 1; }
    }
    return 0;
}

int gmb_paths_settings_file(char* out, size_t cap)
{
    if (!g_pr_loaded) pr_load();
    snprintf(out, cap, "%s", g_pr_settings);
    return g_pr_settings[0] ? 1 : 0;
}

/* Risolve una cartella dati: chiave paths se valorizzata, altrimenti default relativo root. */
static void pr_dir(const char* key, const char* defrel, char* out, size_t cap)
{
    if (!g_pr_loaded) pr_load();
    char v[1024];
    if (pr_paths_value(key, v, sizeof(v)))
    {
        char ex[2048];
        pr_expand(v, ex, sizeof(ex));
        if (ex[0] == '/') snprintf(out, cap, "%s", ex);
        else snprintf(out, cap, "%s/%s", g_pr_root, ex);
        return;
    }
    snprintf(out, cap, "%s/%s", g_pr_root, defrel);
}

/* skills/, plugins/, rag/ e graphs/ sono CONDIVISI dai due mondi (C# e C li leggono): nel monorepo
 * vivono in `<root>/../shared/<nome>`, ed è lì la copia UNICA. Un checkout del solo GMBasic — o
 * un'app installata — non ha `shared/`: in quel caso si ricade su `<root>/<nome>` (comportamento
 * di prima). L'eventuale chiave `paths.*Dir` esplicita vince sempre.
 *
 * ATTENZIONE ALLA CONDIZIONE: si testa l'esistenza di `<root>/../shared` (il PADRE), non di
 * `<root>/../shared/<nome>`. Prima si testava la sottocartella: cancellando `shared/rag/` (i
 * cervelli ora vivono nella cache, 2026-10-31) la risoluzione ricadeva su `<root>/rag` e il
 * primo RAG BUILD avrebbe ricreato `GMBasic/rag/` — esattamente la sporcizia che §9 vieta. Ora
 * la cartella si ricrea **dove deve stare**, sotto `shared/`. */
static void pr_shared_or_root(const char* key, const char* defrel, char* out, size_t cap)
{
    if (!g_pr_loaded) pr_load();
    char v[1024];
    if (pr_paths_value(key, v, sizeof(v)))
    {
        char ex[2048];
        pr_expand(v, ex, sizeof(ex));
        if (ex[0] == '/') snprintf(out, cap, "%s", ex);
        else snprintf(out, cap, "%s/%s", g_pr_root, ex);
        return;
    }
    {
        char shared_parent[2048];
        snprintf(shared_parent, sizeof(shared_parent), "%s/../shared", g_pr_root);
        if (access(shared_parent, F_OK) == 0) {
            snprintf(out, cap, "%s/%s", shared_parent, defrel);
            return;
        }
    }
    snprintf(out, cap, "%s/%s", g_pr_root, defrel);
}

static void pr_user_root(const char* name, char* out, size_t cap);

/* RAG e GRAFI sono dati che il COMPILATORE PRODUCE (RAG BUILD, BRAINVAULTBUILD, MERGE,
 * INGEST): il default e' la CACHE utente (`~/.gmbasic/rag`, `~/.gmbasic/graphs`), NON una
 * cartella del checkout — altrimenti lo stesso cervello esiste due volte (shared/ e cache) e
 * il checkout si riempie di output (decisione 2026-10-31). `paths.ragDir`/`paths.graphsDir`
 * lasciano al programmatore la scelta del percorso (assoluto o relativo alla root). */
static void pr_user_dir(const char* key, const char* defrel, char* out, size_t cap)
{
    if (!out || !cap) return;
    if (!g_pr_loaded) pr_load();
    char v[1024];
    if (pr_paths_value(key, v, sizeof(v)) && v[0])
    {
        char ex[2048];
        pr_expand(v, ex, sizeof(ex));
        if (ex[0] == '/') { snprintf(out, cap, "%s", ex); return; }
        snprintf(out, cap, "%s/%s", g_pr_root, ex);
        return;
    }
    pr_user_root(defrel, out, cap);
}

void gmb_paths_config_dir(char* out, size_t cap) { pr_dir("configDir", "config", out, cap); }
void gmb_paths_skills_dir(char* out, size_t cap) { pr_shared_or_root("skillsDir", "skills", out, cap); }
void gmb_paths_plugins_dir(char* out, size_t cap) { pr_shared_or_root("pluginsDir", "plugins", out, cap); }
void gmb_paths_logs_dir(char* out, size_t cap) { pr_dir("logsDir", "ai-logs", out, cap); }
/* RAG e grafi sono DATI condivisi dai due mondi (C# li scrive, il runtime li legge): nel
 * monorepo la copia unica sta in `<root>/../shared/<nome>`; altrove `<root>/<nome>`. */
void gmb_paths_rag_dir(char* out, size_t cap) { pr_user_dir("ragDir", "rag", out, cap); }
void gmb_paths_graphs_dir(char* out, size_t cap) { pr_user_dir("graphsDir", "graphs", out, cap); }
void gmb_paths_brains_dir(char* out, size_t cap) { pr_dir("brainsDir", "brains", out, cap); }
void gmb_paths_brains_exports_dir(char* out, size_t cap) { pr_dir("brainsExportsDir", "brains-exports", out, cap); }

/* CACHE dei cervelli scaricati (on demand): `~/.gmbasic/brains` — la stessa radice utente dei
 * pacchetti e delle librerie. Chiave `paths.brainsCacheDir` per spostarla. */
static void pr_user_root(const char* name, char* out, size_t cap);
void gmb_paths_brains_cache_dir(char* out, size_t cap)
{
    if (!g_pr_loaded) pr_load();
    char v[1024];
    if (pr_paths_value("brainsCacheDir", v, sizeof(v)))
    {
        char ex[2048];
        pr_expand(v, ex, sizeof(ex));
        if (ex[0] == '/') { snprintf(out, cap, "%s", ex); return; }
    }
    pr_user_root("brains", out, cap);
}

/* Radice UTENTE dei dati di un sottosistema: ~/.gmbasic/<nome> (HOME può mancare: stringa vuota). */
static void pr_user_root(const char* name, char* out, size_t cap)
{
    if (!out || cap == 0) return;
    const char* home = getenv("HOME");
    if (home && *home) snprintf(out, cap, "%s/.gmbasic/%s", home, name);
    else snprintf(out, cap, "%s", "");
}

/* Manifest dei cervelli distribuiti (schema di ml-manifest.json). Ordine di ricerca:
 *   1. `paths.brainsManifest` (esplicito)
 *   2. <exe>/settings/brains-manifest.json        (client/bundle)
 *   3. <root>/../shared/settings/brains-manifest.json  (MONOREPO: la copia unica sta in shared/)
 *   4. <root>/shared/settings/brains-manifest.json
 *   5. <exe>/Resources/brains-manifest.json       (bundle legacy, dove sta ml-manifest)
 *   6. <root>/../shared/resources/brains-manifest.json
 *   7. <root>/Resources/brains-manifest.json
 *   8. <root>/settings/brains-manifest.json
 * Ritorna 1 se il file esiste; altrimenti 0 lasciando in `out` il percorso canonico (per un
 * messaggio d'errore utile). */
int gmb_paths_brains_manifest(char* out, size_t cap)
{
    if (!out || !cap) return 0;
    out[0] = '\0';
    if (!g_pr_loaded) pr_load();
    char v[1024];
    if (pr_paths_value("brainsManifest", v, sizeof(v)))
    {
        char ex[2048];
        pr_expand(v, ex, sizeof(ex));
        if (ex[0] == '/') { snprintf(out, cap, "%s", ex); return access(ex, F_OK) == 0; }
    }
    char cand[8][4096];
    char exe[2048] = "";
    (void)gmb_paths_exe_dir(exe, sizeof exe);
    snprintf(cand[0], sizeof cand[0], "%s/settings/brains-manifest.json", exe);
    snprintf(cand[1], sizeof cand[1], "%s/../shared/settings/brains-manifest.json", g_pr_root);
    snprintf(cand[2], sizeof cand[2], "%s/shared/settings/brains-manifest.json", g_pr_root);
    snprintf(cand[3], sizeof cand[3], "%s/Resources/brains-manifest.json", exe);
    snprintf(cand[4], sizeof cand[4], "%s/../shared/resources/brains-manifest.json", g_pr_root);
    snprintf(cand[5], sizeof cand[5], "%s/Resources/brains-manifest.json", g_pr_root);
    snprintf(cand[6], sizeof cand[6], "%s/settings/brains-manifest.json", g_pr_root);
    for (int i = 0; i < 7; i++) {
        if (cand[i][0] != '/') continue;
        if (access(cand[i], F_OK) == 0) { snprintf(out, cap, "%s", cand[i]); return 1; }
    }
    snprintf(out, cap, "%s", cand[1]);
    return 0;
}

/* File di CONFIGURAZIONE CONDIVISA del compilatore: `shared/settings/<name>` (manifest, seed
 * di provider, ...). Stesso spirito di gmb_paths_brains_manifest: nel monorepo la copia unica
 * sta in `shared/settings`; un programma distribuito non ce l'ha e usa il proprio
 * `<configDir>/<name>` (copiato dal bundle) oppure i default del codice.
 * Ritorna 1 se il file esiste (percorso in `out`); 0 lasciando il candidato canonico. */
int gmb_paths_shared_settings_file(const char* name, char* out, size_t cap)
{
    if (!out || !cap || !name || !*name) return 0;
    out[0] = '\0';
    if (!g_pr_loaded) pr_load();
    char exe[2048] = "";
    (void)gmb_paths_exe_dir(exe, sizeof exe);
    char cand[3][4096];
    snprintf(cand[0], sizeof cand[0], "%s/../shared/settings/%s", g_pr_root, name);
    snprintf(cand[1], sizeof cand[1], "%s/shared/settings/%s", g_pr_root, name);
    snprintf(cand[2], sizeof cand[2], "%s/../shared/settings/%s", exe, name);
    for (int i = 0; i < 3; i++) {
        if (cand[i][0] != '/') continue;
        if (access(cand[i], F_OK) == 0) { snprintf(out, cap, "%s", cand[i]); return 1; }
    }
    snprintf(out, cap, "%s", cand[0]);
    return 0;
}

void gmb_paths_libs_dir(char* out, size_t cap)
{
    if (!g_pr_loaded) pr_load();
    char v[1024];
    if (pr_paths_value("libsDir", v, sizeof(v)))
    {
        char ex[2048];
        pr_expand(v, ex, sizeof(ex));
        if (ex[0] == '/') snprintf(out, cap, "%s", ex);
        else snprintf(out, cap, "%s/%s", g_pr_root, ex);
        return;
    }
    /* Artefatti di BUILD dei sotto-progetti: `<root>/build/libs/`. Il RID lo aggiunge il
     * chiamante (stesso contratto di prima, quando si restituiva `<root>/libs-runtime`):
     * così questo modulo non dipende dalla mappatura RID, che vive altrove. */
    snprintf(out, cap, "%s/build/libs", g_pr_root);
}

/* Cache delle LIBRERIE NATIVE scaricate on-demand (support/engine/core/torch): ~/.gmbasic/libs.
 * Stessa regola del mondo C# (GmbiPaths.LibsCacheRoot): chiave "libsCacheDir" nella sezione
 * paths di system.settings.json, altrimenti ~/.gmbasic/libs. Il RID lo aggiunge il chiamante
 * (<cache>/<rid>/), perché è la stessa cartella per checkout e per client installato. */
/* Migrazione una tantum ~/.gmb → ~/.gmbasic (radice UNICA dei dati utente, decisione
 * 2026-10-29). Il mondo C# fa il merge completo quando parte; qui serve almeno la CACHE delle
 * librerie, perché un eseguibile C99 può girare senza che il C# sia mai stato eseguito.
 * Conservativa: sposta solo se la destinazione non esiste; non cancella nulla che non sia stato
 * spostato. Idempotente. */
static void pr_migrate_gmb_root(void)
{
    const char* home = getenv("HOME");
    if (!home || !*home) return;
    char old_root[2048], new_root[2048];
    snprintf(old_root, sizeof old_root, "%s/.gmb", home);
    snprintf(new_root, sizeof new_root, "%s/.gmbasic", home);
    struct stat st;
    if (stat(old_root, &st) != 0) return;              /* niente da migrare */
    mkdir(new_root, 0755);
    static const char* box[] = { "libs", "packages", "daemons", "ml", "chromium", "ocr", "obscura",
                                 "config.json", "trusted-keys.json" };
    for (size_t i = 0; i < sizeof box / sizeof box[0]; i++)
    {
        char a[2304], b[2304];
        snprintf(a, sizeof a, "%s/%s", old_root, box[i]);
        snprintf(b, sizeof b, "%s/%s", new_root, box[i]);
        if (stat(a, &st) != 0) continue;
        if (stat(b, &st) == 0) continue;                /* già presente: non sovrascrivere */
        rename(a, b);                                   /* stesso filesystem: atomico */
    }
    rmdir(old_root);                                    /* solo se è rimasto vuoto */
}

void gmb_paths_resolve_model(const char* in, char* out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!in || !*in) return;

    /* SOLO espansione di '~' (HOME). Nessuna rimappatura dei percorsi relativi: quelli sono
     * relativi alla CWD per contratto (fixture del repo, es. ../../samples/Fixtures/models/...)
     * e toccarli cambierebbe il comportamento di campioni e MiniTest gia' corretti.
     * Serve perché campioni e MiniTest possono indicare un modello locale come
     * "~/ai/Models/<nome>" senza contenere il nome utente. */
    pr_expand(in, out, cap);
}

void gmb_paths_libs_cache_dir(char* out, size_t cap)
{
    if (!out || cap == 0) return;
    if (!g_pr_loaded) pr_load();
    pr_migrate_gmb_root();
    char v[1024];
    if (pr_paths_value("libsCacheDir", v, sizeof(v)) && v[0])
    {
        char ex[2048];
        pr_expand(v, ex, sizeof(ex));
        if (ex[0] == '/') snprintf(out, cap, "%s", ex);
        else snprintf(out, cap, "%s/%s", g_pr_root, ex);
        return;
    }
    const char* home = getenv("HOME");
    if (home && *home) { snprintf(out, cap, "%s/.gmbasic/libs", home); return; }
    snprintf(out, cap, "%s", "");
}

/* Cartella dei pacchetti installati dal package manager (default <root>/packages):
 * chiave "packagesDir" nella sezione paths di system.settings.json, altrimenti <root>/packages. */
int gmb_paths_exe_dir(char* out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = '\0';
    return pr_exe_dir(out, cap);
}

static int gmb_pkgdir_looks_like_cache(const char* dir)
{
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* p; int ok = 0;
    while (!ok && (p = readdir(d)) != NULL) {
        if (p->d_name[0] == '.') continue;
        char sub[4096];
        snprintf(sub, sizeof(sub), "%s/%s", dir, p->d_name);
        DIR* d2 = opendir(sub);
        if (!d2) continue;
        struct dirent* q;
        while ((q = readdir(d2)) != NULL) {
            if (q->d_name[0] == '.') continue;
            char man[8192];
            snprintf(man, sizeof(man), "%s/%s/gmb-package.json", sub, q->d_name);
            if (access(man, F_OK) == 0) { ok = 1; break; }
        }
        closedir(d2);
    }
    closedir(d);
    return ok;
}

/* 1 se `dir` è un CHECKOUT SORGENTE di GMBasic (macchina di SVILUPPO, compilatore presente). */
static int pr_is_source_checkout(const char* dir)
{
    char p[4096];
    if (!dir || !*dir) return 0;
    snprintf(p, sizeof(p), "%s/GMBASIC.sln", dir); if (access(p, F_OK) == 0) return 1;
    snprintf(p, sizeof(p), "%s/GMBasicSharp/GMBasicSharp.slnx", dir); if (access(p, F_OK) == 0) return 1;
    snprintf(p, sizeof(p), "%s/native/gmb_runtime_c/include/gmb_runtime.h", dir); if (access(p, F_OK) == 0) return 1;
    return 0;
}

/* Cache UTENTE: ~/.gmbasic/packages/<nome>/<versione>/ — è il posto dei pacchetti su una
 * macchina CLIENT (app installata, nessun compilatore, nessun checkout).
 *
 * RADICE UNICA. Qui c'era `~/.gmb/packages`, mentre il C# (GmbiPaths.ResolvePackagesDir, che è
 * chi INSTALLA i pacchetti) usa `~/.gmbasic/packages`: due cartelle diverse nella stessa
 * macchina — un pacchetto installato dal C# non si trovava dal C. Ora la radice è una sola,
 * come per le librerie (`~/.gmbasic/libs`). */
static void pr_user_pkg_cache(char* out, size_t cap)
{
    const char* home = getenv("HOME");
    if (home && *home) { snprintf(out, cap, "%s/.gmbasic/packages", home); return; }
    snprintf(out, cap, "%s", "");
}

/* Directory dell'ESEGUIBILE corrente (senza filename): ancora dell'installazione LOCALE dei
 * pacchetti (`<exe>/packages`, modello node_modules). 1 se risolta. */
int gmb_paths_exe_dir(char* out, size_t cap);

void gmb_paths_packages_dir(char* out, size_t cap)
{
    if (!g_pr_loaded) pr_load();
    out[0] = '\0';
    /* 1) SETTING ESPLICITO (paths.packagesDir): vince sempre, nessun fallback. */
    {
        char v[1024];
        if (pr_paths_value("packagesDir", v, sizeof(v)) && v[0]) {
            char ex[2048];
            pr_expand(v, ex, sizeof(ex));
            if (ex[0] == '/') snprintf(out, cap, "%s", ex);
            else snprintf(out, cap, "%s/%s", g_pr_root, ex);
            return;
        }
    }
    /* 2) MACCHINA DI SVILUPPO (checkout sorgente): <root>/packages e, nel monorepo,
     *    <root>/../packages — accettata solo se HA il layout della cache
     *    (`<nome>/<versione>/gmb-package.json`: `shared/minitests/packages` NON lo è). */
    if (pr_is_source_checkout(g_pr_root)) {
        char alt[4096];
        snprintf(out, cap, "%s/packages", g_pr_root);
        if (gmb_pkgdir_looks_like_cache(out)) return;
        /* MONOREPO: <root>/../packages (NON <root>/packages/../packages: era il bug — il
         * fallback puntava alla stessa cartella e non trovava mai la radice del monorepo). */
        snprintf(alt, sizeof(alt), "%s/../packages", g_pr_root);
        if (gmb_pkgdir_looks_like_cache(alt)) {
            char rl[4096];
            snprintf(out, cap, "%s", realpath(alt, rl) ? rl : alt);
        }
        return;   /* checkout senza cache popolata: <root>/packages (verrà creata) */
    }
    /* 3) CLIENT (nessun checkout): ~/.gmbasic/packages. Niente CWD-walk: era un'euristica che
     *    poteva agganciare una qualsiasi cartella `packages` (già costata webp/avif). */
    pr_user_pkg_cache(out, cap);
    if (out[0]) return;
    /* ultima risorsa (HOME non impostata): <root>/packages */
    snprintf(out, cap, "%s/packages", g_pr_root);
}

void gmb_paths_temp_dir(char* out, size_t cap)
{
    if (!g_pr_loaded) pr_load();
    char v[1024];
    if (pr_paths_value("tempDir", v, sizeof(v)))
    {
        char ex[2048];
        pr_expand(v, ex, sizeof(ex));
        if (ex[0] == '/') { snprintf(out, cap, "%s", ex); return; }
        snprintf(out, cap, "%s/%s", g_pr_root, ex);
        return;
    }
    const char* tmp = getenv("TMPDIR");
    if (tmp && *tmp) snprintf(out, cap, "%s", tmp);
    else snprintf(out, cap, "%s", "/tmp");
}

/* File di stato dentro configDir. */
void gmb_paths_vaults_db(char* out, size_t cap)
{
    char d[1024]; gmb_paths_config_dir(d, sizeof(d));
    snprintf(out, cap, "%s/vaults.db", d);
}
void gmb_paths_brain_db(char* out, size_t cap)
{
    char d[1024]; gmb_paths_config_dir(d, sizeof(d));
    snprintf(out, cap, "%s/brain.db", d);
}
void gmb_paths_brains_json(char* out, size_t cap)
{
    char d[1024]; gmb_paths_config_dir(d, sizeof(d));
    snprintf(out, cap, "%s/brains.json", d);
}
void gmb_paths_providers_json(char* out, size_t cap)
{
    char d[1024]; gmb_paths_config_dir(d, sizeof(d));
    snprintf(out, cap, "%s/providers.json", d);
}
void gmb_paths_mcp_json(char* out, size_t cap)
{
    char d[1024]; gmb_paths_config_dir(d, sizeof(d));
    snprintf(out, cap, "%s/mcp.json", d);
}
void gmb_paths_plugins_json(char* out, size_t cap)
{
    char d[1024]; gmb_paths_config_dir(d, sizeof(d));
    snprintf(out, cap, "%s/plugins.json", d);
}
void gmb_paths_skills_index_json(char* out, size_t cap)
{
    char d[1024]; gmb_paths_config_dir(d, sizeof(d));
    snprintf(out, cap, "%s/skills-index.json", d);
}
void gmb_paths_recursivemas_dir(char* out, size_t cap)
{
    char d[1024]; gmb_paths_config_dir(d, sizeof(d));
    snprintf(out, cap, "%s/recursivemas", d);
}

/* Componenti runtime scaricati on-demand: nella CACHE utente (<cache>/<comp>/<rid>), come
 * `GmbiPaths.MlDir/ChromiumDir/ObscuraDir` nel mondo C#. Prima stavano in `<root>/libs-runtime/`
 * (cartella del checkout, ora inesistente): una seconda copia che il client non aveva. */
void gmb_paths_ml_dir(const char* rid, char* out, size_t cap)
{
    char d[1024]; gmb_paths_libs_cache_dir(d, sizeof(d));
    snprintf(out, cap, "%s/ml/%s", d, rid && *rid ? rid : "osx-arm64");
}
void gmb_paths_chromium_dir(const char* rid, char* out, size_t cap)
{
    char d[1024]; gmb_paths_libs_cache_dir(d, sizeof(d));
    snprintf(out, cap, "%s/chromium/%s", d, rid && *rid ? rid : "osx-arm64");
}
void gmb_paths_obscura_dir(const char* rid, char* out, size_t cap)
{
    char d[1024]; gmb_paths_libs_cache_dir(d, sizeof(d));
    snprintf(out, cap, "%s/obscura/%s", d, rid && *rid ? rid : "osx-arm64");
}

/* Marketplace di skills/plugins: CACHE UTENTE (~/.gmbasic/<nome>-marketplaces), non la
 * cartella temporanea. `AI SKILLS|PLUGINS MARKETPLACE "url"` AGGIUNGE il clone ai percorsi di
 * ricerca: è un'installazione, non uno staging; in temp il SO la cancellava lasciando percorsi
 * configurati verso cartelle sparite. Parità: GmbiPaths.SkillsMarketplacesDir (C#). */
void gmb_paths_skills_marketplaces_dir(char* out, size_t cap)
{
    pr_user_root("skills-marketplaces", out, cap);
}
void gmb_paths_plugins_marketplaces_dir(char* out, size_t cap)
{
    pr_user_root("plugins-marketplaces", out, cap);
}
void gmb_paths_kvswap_dir(char* out, size_t cap)
{
    char d[1024]; gmb_paths_temp_dir(d, sizeof(d));
    snprintf(out, cap, "%s/gmbasic-kv-swap", d);
}

int gmb_paths_get(const char* key, char* out, size_t cap)
{
    if (!g_pr_loaded) pr_load();
    if (!key || !*key) return 0;
    char v[1024];
    if (!pr_paths_value(key, v, sizeof(v))) return 0;
    char ex[2048];
    pr_expand(v, ex, sizeof(ex));
    if (ex[0] == '/') snprintf(out, cap, "%s", ex);
    else snprintf(out, cap, "%s/%s", g_pr_root, ex);
    return 1;
}
