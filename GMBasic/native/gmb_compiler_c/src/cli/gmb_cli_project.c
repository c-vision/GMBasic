/* gmb_cli_project.c — comandi `check` e `build` del CLI `gmb`, estratti da gmb_cli.c.
 *
 * PERCHÉ: `gmb_cli.c` era 1602 righe, sopra la soglia di 1200 (AGENTS §0quater). Lo split
 * separa i COMANDI che lavorano sul progetto (scoperta sorgenti/moduli, driver, -I, librerie,
 * link) dal resto (parsing argomenti, minitests, main). Il contratto condiviso è in
 * `gmb_cli_internal.h`; gli helper comuni restano definiti in gmb_cli.c.
 */
#include "gmb_cli_internal.h"
#include "gmb_inc_resolve.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


void run_check(const Options* o, const char* project, const char* entry,
                      DiagList* diags, int* out_sources, int* out_modules) {
    (void)o;
    char** sources = NULL; int ns = 0;
    gmb_project_sources(project, &sources, &ns);
    /* `sorgenti` nel report = file DEL PROGETTO (le librerie dell'install fanno parte della
     * mappa dei namespace, ma non sono file che l'utente deve vedere nel conteggio). */
    int proj_files = 0;
    for (int i = 0; i < ns; i++) if (path_under(sources[i], project)) proj_files++;
    *out_sources = proj_files;

    /* 1. NAMESPACE dichiarati da TUTTI i candidati (un IMPORT può puntare anche a una
     *    libreria dell'install: la mappa è globale, non solo progetto). */
    NameSet declared; memset(&declared, 0, sizeof declared);
    for (int i = 0; i < ns; i++) {
        char names[CLI_MAX_NS][64];
        int nn = gmb_file_namespaces(sources[i], names, CLI_MAX_NS);
        /* Più file possono dichiarare lo stesso NAMESPACE (è legale: Controls/ lo fa per
         * CustomControls). Qui interessa solo l'INSIEME dei namespace dichiarati. */
        for (int k = 0; k < nn; k++) nameset_add(&declared, names[k]);
        /* 2. Ogni `.bas` del PROGETTO (non l'entry) deve dichiarare un NAMESPACE, altrimenti
         *    non può essere importato da nessuno. */
        if (nn == 0 && ends_with(sources[i], ".bas") && path_under(sources[i], project)
            && strcmp(sources[i], entry) != 0) {
            diag_add(diags, 0, "no-namespace", sources[i], 0,
                     "modulo `.bas` senza NAMESPACE: non è importabile");
        }
    }
    /* 3. Grafo raggiungibile dalla entry: un IMPORT che non trova né un modulo né un
     *    namespace builtin è un ERRORE (prima diventava una variabile implicita). */
    char** modules = gmb_resolve_module_files(entry, out_modules);
    int nreach = (*out_modules) + 1;
    char** reach = (char**)calloc((size_t)nreach, sizeof(char*));
    if (reach) {
        reach[0] = (char*)entry;
        for (int i = 0; i < *out_modules; i++) reach[i + 1] = modules[i];
    }
    for (int i = 0; reach && i < nreach; i++) {
        char imps[CLI_MAX_NS][64];
        int ni = gmb_file_imports(reach[i], imps, CLI_MAX_NS);
        for (int k = 0; k < ni; k++) {
            if (nameset_has(&declared, imps[k])) continue;
            if (gmb_namespace_is_builtin(imps[k])) continue;
            char msg[512];
            snprintf(msg, sizeof msg,
                     "IMPORT %s non risolto: nessun modulo con NAMESPACE %s (né namespace builtin)",
                     imps[k], imps[k]);
            diag_add(diags, 1, "import-unresolved", reach[i], import_line(reach[i], imps[k]), msg);
        }
    }
    free(reach);
    gmb_resolve_module_files_free(modules, *out_modules);
    nameset_free(&declared);
    gmb_string_list_free(sources, ns);
}

/* ── build / run ─────────────────────────────────────────────────────────────────────── */

#ifndef GMB_RID_STR
#define GMB_RID_STR "osx-arm64"
#endif

/* Cartella delle librerie/header NATIVE: la CACHE utente del RID (`~/.gmbasic/libs/<rid>/`).
 * Prima si puntava a `Resources/libs/<rid>` e alle librerie del builder: un path ASSOLUTO del
 * builder finisce nel binario e su un'altra macchina non carica. */
static int gmb_cache_libdir(char* out, size_t cap) {
    const char* home = getenv("HOME");
    if (!home || !*home) return 0;
    snprintf(out, cap, "%s/.gmbasic/libs/%s", home, GMB_RID_STR);
    return is_dir(out);
}

/* Cerca il driver accanto al CLI, nel build del repo, poi in tools/. */

/* true se il sorgente usa UI/grafica/AI: serve il runtime COMPLETO (`libgmb_runtime.a`).
 * Stessa logica di tools/gmb_c99native_run.sh: parola chiave ALL'INIZIO della riga (dopo spazi)
 * oppure `IMPORT <namespace>`. Non è un parser: una scelta PRUDENTE (in dubbio → completo). */
static int ci_word_at(const char* p, const char* w) {
    size_t n = strlen(w);
    if (strncasecmp(p, w, n) != 0) return 0;
    char c = p[n];
    return (c == ' ' || c == '\t' || c == '(' || c == '\r' || c == '\n' || c == '\0' || c == ',');
}
static int source_needs_full_runtime(const char* entry) {
    char* src = read_all(entry);
    if (!src) return 1;
    static const char* kw[] = {
        "FORM","READ","TORCH","TRAIN","JINJA","SCREEN","GRAPHICS","SPRITE","SPRITESET","PSET",
        "PRESET","CIRCLE","PAINT","DRAW","POLYGON","PCOPY","SCREENSET","PALETTE","MOUSE","WINDOW",
        "IMAGE","BITMAP","TILE","FONT","PLOT","CHART","VIDEO","DBF","ISAM","SPEED","RDD","SQL",
        "NET","HTTP","AI","LLM","MEMORY","VECTORDB","DOC","FETCH","TUI"
    };
    int need = 0;
    const char* p = src;
    while (*p && !need) {
        while (*p == ' ' || *p == '\t') p++;
        if (strncasecmp(p, "IMPORT", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
            const char* q = p + 6;
            while (*q == ' ' || *q == '\t') q++;
            for (size_t k = 0; k < sizeof kw / sizeof kw[0] && !need; k++)
                if (ci_word_at(q, kw[k])) need = 1;
        }
        for (size_t k = 0; k < sizeof kw / sizeof kw[0] && !need; k++)
            if (ci_word_at(p, kw[k])) need = 1;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    free(src);
    return need;
}

int find_driver(Toolchain* tc, const char* argv0) {
    /* La root dell'install serve SEMPRE (include del runtime e lib per il link), anche
     * quando il driver è trovato accanto al CLI nel build del repo. */
    (void)gmb_resolve_install_root(tc->install, sizeof tc->install);

    char self[CLI_PATH];
    snprintf(self, sizeof self, "%s", argv0 ? argv0 : "");
    char dir[CLI_PATH]; path_dir(self, dir, sizeof dir);

    char cand[CLI_PATH];
    snprintf(cand, sizeof cand, "%s/gmb_c99native_driver", dir[0] ? dir : ".");
    if (is_file(cand)) { snprintf(tc->driver, sizeof tc->driver, "%s", cand); return 1; }

    if (!tc->install[0]) return 0;
    snprintf(cand, sizeof cand, "%s/native/gmb_compiler_c/build/gmb_c99native_driver", tc->install);
    if (is_file(cand)) { snprintf(tc->driver, sizeof tc->driver, "%s", cand); return 1; }
    snprintf(cand, sizeof cand, "%s/tools/gmb_c99native_driver", tc->install);
    if (is_file(cand)) { snprintf(tc->driver, sizeof tc->driver, "%s", cand); return 1; }
    return 0;
}

static int add_inc(char* buf, size_t cap, const char* dir) {
    if (!is_dir(dir)) return 0;
    size_t l = strlen(buf);
    char q[CLI_PATH + 8];
    shell_quote(dir, q, sizeof q);
    snprintf(buf + l, cap - l, " -I%s", q);
    return 1;
}

/* Ricetta degli -I per compilare il C emesso: quella di tools/gmb_c99native_run.sh (il C
 * generato include gli header del runtime e di LLMEnv). Una sola copia, usata sia da
 * `build` sia da `minitests`: due ricette divergenti darebbero verdetti diversi. */
void build_inc(char* inc, size_t cap, const char* install) {
    inc[0] = 0;
    const char* rel[] = {
        "native/gmb_runtime_c/include", "native/gmb_runtime_c/src",
        "native/gmb_chart/include", "native/gmb_compiler_c/include",
        "native/gmb_db_c/include", "native/gmb_torch/include",
        "native/gmb_doc_c/include", "native/gmb_agent_runtime",
        "gmbllm/include", "gmbllm/include/quant",
        "gmbllm/include/train", "gmbllm/include/lora",
    };
    for (size_t i = 0; i < sizeof rel / sizeof rel[0]; i++) {
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/%s", install, rel[i]);
        add_inc(inc, cap, p);
    }
    /* LLMEnv è accanto al repo (sorgente unica dei motori AI). */
    const char* llmrel[] = {
        "LLMCodec/include", "LLMJinja", "LLMEngine/moe/include", "LLMEngine/asr/include",
        "LLMEngine/mlx", "LLMEngine/include", "LLMEngine/Options", "LLMEngine/plugin",
        "LLMDesc", "LLMToken/include",
    };
    for (size_t i = 0; i < sizeof llmrel / sizeof llmrel[0]; i++) {
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/../LLMEnv/%s", install, llmrel[i]);
        add_inc(inc, cap, p);
    }
    /* Monorepo: LLMEnv è accanto al workspace (GMBasic/../../LLMEnv). */
    for (size_t i = 0; i < sizeof llmrel / sizeof llmrel[0]; i++) {
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/../../LLMEnv/%s", install, llmrel[i]);
        add_inc(inc, cap, p);
    }
    {
        /* header delle terze parti: gruppo `dev` della cache (`gmb install --dev`). */
        char cache[CLI_PATH];
        if (gmb_cache_libdir(cache, sizeof cache)) {
            char inc2[CLI_PATH];
            snprintf(inc2, sizeof inc2, "%s/include", cache);
            add_inc(inc, cap, inc2);
        }
    }
}

/* Prodotto di un sorgente LIBRARY: una libreria condivisa (.gmblib), non un eseguibile.
 * `dynamic_lookup` è la scelta già fatta da tools/gmb_c99native_run.sh: la libreria si carica
 * in un host che espone il runtime (e le sue dipendenze grafiche), quindi i simboli che il
 * corpo non usa non vengono mai risolti. Senza, il link fallirebbe per le sole SDL che il
 * runtime console trascina con sé anche in una libreria di sola aritmetica. */
#if defined(__APPLE__)
#define GMB_LIB_EXT ".dylib"
#define GMB_LIB_LDFLAGS "-shared -fPIC -Wl,-undefined,dynamic_lookup"
#else
#define GMB_LIB_EXT ".so"
#define GMB_LIB_LDFLAGS "-shared -fPIC"
#endif

/* Frameworks Cocoa/CoreGraphics chiesti dal motore grafico del runtime (SDL si appoggia al
 * CoreGraphics per ImageIO). Su Linux non esistono: la riga di link resta senza. */
#if defined(__APPLE__)
#define GMB_EXE_FRAMEWORKS " -framework CoreGraphics -framework ApplicationServices" \
                           " -framework ImageIO -framework CoreServices -framework AppKit"
#else
#define GMB_EXE_FRAMEWORKS ""
#endif

/* Aggiunge ` -l<name>` a `buf` solo se la libreria esiste davvero in una delle `dirs`.
 * Una `-l` che punta al nulla farebbe fallire il link di un programma che non la usa: il
 * difetto che si sta riparando (SDL richiesta ma assente) tornerebbe identico, solo con un
 * altro messaggio. Su una macchina senza MoltenVK la riga resta quindi valida. */
static int add_lib_if_present(char* buf, size_t cap, const char* const* dirs, size_t nd,
                              const char* name) {
    static const char* exts[] = { ".dylib", ".so", ".a" };
    for (size_t i = 0; i < nd; i++) {
        for (size_t e = 0; e < sizeof exts / sizeof exts[0]; e++) {
            char p[CLI_PATH];
            snprintf(p, sizeof p, "%s/lib%s%s", dirs[i], name, exts[e]);
            if (!is_file(p)) continue;
            size_t l = strlen(buf);
            if (l < cap) snprintf(buf + l, cap - l, " -l%s", name);
            return 1;
        }
    }
    return 0;
}

/* Il C emesso in modalità LIBRARY non ha `int main` (contratto ABI v1: nessun entry point).
 * Stessa sonda di tools/gmb_c99native_run.sh, così build e tool non divergono: la decisione
 * si prende sull'artefatto, non sul testo del sorgente. */
static int c_has_int_main(const char* path) {
    FILE* f = fopen(path, "rb");
    /* Illeggibile: si resta sul percorso eseguibile, che è quello che `cc` ha già validato
     * e segnala da sé. Non si indovina una libreria da un file che non si può leggere. */
    if (!f) return 1;
    static const char needle[] = "int main";
    char buf[65536];
    char carry[sizeof needle];
    size_t keep = 0;            /* coda della finestra precedente (match a cavallo) */
    int found = 0;
    size_t n;
    while (!found && (n = fread(buf + keep, 1, sizeof buf - 1 - keep, f)) > 0) {
        size_t total = keep + n;
        buf[total] = 0;
        if (strstr(buf, needle)) found = 1;
        keep = total < sizeof needle - 1 ? total : sizeof needle - 1;
        memcpy(carry, buf + total - keep, keep);
        memcpy(buf, carry, keep);
    }
    fclose(f);
    return found;
}

/* Nome della libreria dalla direttiva `LIBRARY "nome"`: è la stessa stringa che l'emitter usa
 * come prefisso dei simboli esportati `<nome>_<Proc>`, quindi il file `.gmblib` e l'ABI non
 * possono raccontare nomi diversi. 0 se il file non dichiara la direttiva. */
int library_directive(const char* path, char* out, size_t cap) {
    char* s = read_all(path);
    if (!s) return 0;
    int found = 0;
    for (char* p = s; *p && !found; ) {
        char* eol = strchr(p, '\n');
        char* q = p;
        while (*q == ' ' || *q == '\t') q++;
        if (strncasecmp(q, "LIBRARY", 7) == 0 && (q[7] == ' ' || q[7] == '\t')) {
            const char* r = q + 7;
            while (*r == ' ' || *r == '\t') r++;
            if (*r == '"') {
                const char* e = strchr(r + 1, '"');
                if (e && e > r + 1 && (size_t)(e - r - 1) < cap) {
                    size_t len = (size_t)(e - r - 1);
                    memcpy(out, r + 1, len);
                    out[len] = 0;
                    found = 1;
                }
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    free(s);
    return found;
}

/* Compila il C emesso e lo linka col runtime C. La ricetta dei -I è quella di
 * tools/gmb_c99native_run.sh (il C generato include gli header del runtime e di LLMEnv). */
int build_project(const Options* o, const char* project, const char* entry,
                         DiagList* diags, char* out_exe, size_t exe_cap, int* out_is_lib) {
    if (out_is_lib) *out_is_lib = 0;
    Toolchain tc; memset(&tc, 0, sizeof tc);
    if (!find_driver(&tc, o->argv0)) {
        diag_add(diags, 1, "driver-missing", entry, 0,
                 "gmb_c99native_driver non trovato (build native/gmb_compiler_c)");
        return 5;
    }

    char base[256];
    snprintf(base, sizeof base, "%s", entry);
    char* slash = strrchr(base, '/');
    char* name = slash ? slash + 1 : base;
    char* dot = strrchr(name, '.');
    if (dot) *dot = 0;

    char bin[CLI_PATH];
    snprintf(bin, sizeof bin, "%s/bin", project);
    if (mkdir_p(bin) != 0) {
        diag_add(diags, 1, "io", bin, 0, "impossibile creare la cartella bin/");
        return 2;
    }
    char cpath[CLI_PATH], obj[CLI_PATH], exe[CLI_PATH];
    snprintf(cpath, sizeof cpath, "%s/%s.c", bin, name);
    snprintf(obj, sizeof obj, "%s/%s.o", bin, name);
    if (o->out && *o->out) {
        if (o->out[0] == '/') snprintf(exe, sizeof exe, "%s", o->out);
        else snprintf(exe, sizeof exe, "%s/%s", project, o->out);
    } else {
        snprintf(exe, sizeof exe, "%s/%s", bin, name);
    }
    char exe_dir[CLI_PATH]; path_dir(exe, exe_dir, sizeof exe_dir);
    if (exe_dir[0]) mkdir_p(exe_dir);
    snprintf(out_exe, exe_cap, "%s", exe);

    /* 1. emissione del C */
    char qe[CLI_PATH + 8], qc[CLI_PATH + 8], qd[CLI_PATH + 8], cmd[CLI_PATH * 4];
    shell_quote(entry, qe, sizeof qe);
    shell_quote(cpath, qc, sizeof qc);
    shell_quote(tc.driver, qd, sizeof qd);
    char gpmarg[64] = "";
    if (o->gpm && *o->gpm) snprintf(gpmarg, sizeof gpmarg, " --gpm %s", o->gpm);
    snprintf(cmd, sizeof cmd, "%s %s %s --no-exe%s 2>&1", qd, qe, qc, gpmarg);
    char* log = NULL;
    int rc = run_cmd(cmd, &log);
    if (rc != 0) {
        int code = rc == 3 ? 3 : (rc == 4 ? 4 : 2);
        char msg[1200];
        snprintf(msg, sizeof msg, "emissione fallita (rc=%d): %s", rc, log ? log : "");
        /* la diagnosi del driver contiene già file:riga: la si riporta tal quale */
        diag_add(diags, 1, "emit", entry, 0, msg);
        free(log);
        return code;
    }
    free(log);

    /* 2. compilazione del C */
    char inc[CLI_PATH * 24];
    build_inc(inc, sizeof inc, tc.install);

    shell_quote(obj, qc, sizeof qc);
    char qsrc[CLI_PATH + 8]; shell_quote(cpath, qsrc, sizeof qsrc);
    snprintf(cmd, sizeof cmd, "cc -std=c99 -O2 -w%s -c %s -o %s 2>&1", inc, qsrc, qc);
    rc = run_cmd(cmd, &log);
    if (rc != 0) {
        char msg[1200];
        snprintf(msg, sizeof msg, "cc fallito (rc=%d): %s", rc, log ? log : "");
        diag_add(diags, 1, "cc", cpath, 0, msg);
        free(log);
        return 2;
    }
    free(log);

    /* 3. LIBRARY: il C emesso non ha `int main`, quindi il prodotto è la libreria condivisa
     *    (.gmblib) e non un eseguibile. Senza questo ramo il link moriva con `_main` non
     *    definito su un C corretto. Il nome dell'artefatto è quello della direttiva (lo stesso
     *    prefisso dei simboli esportati); `--out` resta sovrano. */
    int is_lib = !c_has_int_main(cpath);
    if (is_lib && !(o->out && *o->out)) {
        char lib[96];
        if (!library_directive(entry, lib, sizeof lib) || !lib[0])
            snprintf(lib, sizeof lib, "%s", name);
        snprintf(exe, sizeof exe, "%s/%s%s", bin, lib, GMB_LIB_EXT);
        char libdir[CLI_PATH]; path_dir(exe, libdir, sizeof libdir);
        if (libdir[0]) mkdir_p(libdir);
        snprintf(out_exe, exe_cap, "%s", exe);
    }
    if (out_is_lib) *out_is_lib = is_lib;

    /* 4. link col runtime C. "Solo libgmb_runtime" non basta più: il runtime console trascina
     *    il motore grafico (gmb_tui_app → SDL2/SDL2_ttf/SDL2_mixer + Vulkan via MoltenVK) e il
     *    chart, quindi un programma con un solo PRINT moriva sui simboli `_SDL_*`, `_vk*` e
     *    `gmb_chart_*` non definiti. Non si copia però l'intero lib-set di tools/gmb_c99native_run.sh
     *    (db/agent/document/AI): serve ai programmi che usano quei domini, non a un PRINT. */
    char runtime[CLI_PATH]; runtime[0] = 0;
    /* MINIMO per i programmi TESTUALI: il runtime completo trascina il motore AI
     * (llmdesc/gmbllm/llmcodec) che questo CLI non linka, e `gmb build|run` moriva con
     * "_llmdesc_abi_locate non definito" anche su un solo PRINT (bug riprodotto 2026-10-29).
     * Stessa scelta di tools/gmb_c99native_run.sh: min se il sorgente non usa UI/AI. */
    const int need_full = source_needs_full_runtime(entry);
    int use_min = 0;
    if (!need_full) {
        const char* mincand[] = {
            "native/gmb_runtime_c/build/libgmb_runtime_min.a",
            "native/gmb_vm_c/build/gmb_runtime_c_build/libgmb_runtime_min.a",
        };
        for (size_t i = 0; i < sizeof mincand / sizeof mincand[0]; i++) {
            char p[CLI_PATH];
            snprintf(p, sizeof p, "%s/%s", tc.install, mincand[i]);
            /* un archivio VUOTO/parziale (pochi byte: build interrotta) non è una libreria:
             * sceglierlo produce un link senza simboli (`_gmb_print_newline` non definito).
             * Si accetta solo un'archivio con dimensione plausibile. */
            struct stat st_min;
            if (!is_file(p) || stat(p, &st_min) != 0 || st_min.st_size < 65536) continue;
            snprintf(runtime, sizeof runtime, "%s", p);
            use_min = 1;
            break;
        }
    }
    /* Il terzo candidato non esiste più nel build attuale: costa un `is_file` e copre i
     * checkout più datati. */
    const char* rtcand[] = {
        "native/gmb_vm_c/build/gmb_runtime_c_build/libgmb_runtime.a",
        "native/gmb_runtime_c/build/gmb_runtime_c_build/libgmb_runtime.a",
        "native/gmb_runtime_c/build/libgmb_runtime.a",
    };
    for (size_t i = 0; !runtime[0] && i < sizeof rtcand / sizeof rtcand[0]; i++) {
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/%s", tc.install, rtcand[i]);
        if (is_file(p)) { snprintf(runtime, sizeof runtime, "%s", p); break; }
    }
    if (!runtime[0]) {
        diag_add(diags, 1, "runtime-missing", entry, 0,
                 "libgmb_runtime.a non trovata: costruisci il runtime C "
                 "(native/gmb_vm_c) o usa tools/gmb_c99native_run.sh");
        return 5;
    }
    /* Il chart è una static lib accanto al runtime: referenziata dal runtime, quindi va
     * presente al link dell'eseguibile. Per una LIBRARY non serve (i suoi simboli restano
     * dinamici) e `dynamic_lookup` li lascia risolvere all'host. */
    char chart[CLI_PATH], qchart[CLI_PATH + 8];
    chart[0] = 0; qchart[0] = 0;
    if (!is_lib) {
        const char* chartcand[] = {
            "native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a",
            "native/gmb_vm_c/build/gmb_chart_build/libgmb_chart.a",
            "native/gmb_runtime_c/build/libgmb_chart.a",
        };
        for (size_t i = 0; i < sizeof chartcand / sizeof chartcand[0]; i++) {
            char p[CLI_PATH];
            snprintf(p, sizeof p, "%s/%s", tc.install, chartcand[i]);
            if (!is_file(p)) continue;
            snprintf(chart, sizeof chart, "%s", p);
            shell_quote(chart, qchart, sizeof qchart);
            break;
        }
    }

    /* Dir di ricerca delle librerie grafiche: prima quelle nel repo (Resources/libs è il
     * bundle che l'app spedisce), poi le dir di sistema. Le -L si emettono solo se esistono. */
    char ldir_storage[4][CLI_PATH];
    const char* ldirs_ptr[4];
    size_t nldirs = 0;
    char ldirs[CLI_PATH * 4]; ldirs[0] = 0;
    {
        char cache_lib[CLI_PATH];
        const int have_cache = gmb_cache_libdir(cache_lib, sizeof cache_lib);
        const char* lcand[] = { have_cache ? cache_lib : "", "/usr/lib", "/usr/local/lib" };
        for (size_t i = 0; i < sizeof lcand / sizeof lcand[0] && nldirs < 4; i++) {
            char p[CLI_PATH];
            snprintf(p, sizeof p, "%s/%s", tc.install, lcand[i]);
            if (lcand[i][0] == '/' && !is_dir(lcand[i])) continue;
            if (lcand[i][0] != '/' && !is_dir(p)) continue;
            const char* dir = lcand[i][0] == '/' ? lcand[i] : p;
            snprintf(ldir_storage[nldirs], CLI_PATH, "%s", dir);
            ldirs_ptr[nldirs] = ldir_storage[nldirs];
            char q[CLI_PATH + 8];
            shell_quote(dir, q, sizeof q);
            /* -rpath accanto a -L: senza, l'eseguibile linkato trova libSDL2 in fase di link
             * ma non di avvio (`Library not loaded: @rpath/libSDL2.dylib`, nessun LC_RPATH).
             * Stessa scelta di tools/gmb_c99native_run.sh, che bagna di rpath le sue dir. */
            size_t l = strlen(ldirs);
            if (l < sizeof ldirs)
                snprintf(ldirs + l, sizeof ldirs - l, " -L%s -Wl,-rpath,%s", q, q);
            nldirs++;
        }
    }
    char libs[CLI_PATH]; libs[0] = 0;
    if (!is_lib) {
        const char* const* d = ldirs_ptr;
        add_lib_if_present(libs, sizeof libs, d, nldirs, "SDL2");
        add_lib_if_present(libs, sizeof libs, d, nldirs, "SDL2_ttf");
        add_lib_if_present(libs, sizeof libs, d, nldirs, "SDL2_mixer");
#if defined(__APPLE__)
        add_lib_if_present(libs, sizeof libs, d, nldirs, "MoltenVK");
#endif
        /* zlib è di sistema su macOS e Linux: non c'è una dir del repo da sondare. */
        strncat(libs, " -lz", sizeof libs - strlen(libs) - 1);
        /* Il runtime MINIMO linka anche bz2, curl e libcompression (stessa riga di
         * tools/gmb_c99native_run.sh): senza, il link del solo PRINT falliva. */
        if (use_min) {
#if defined(__APPLE__)
            strncat(libs, " -lbz2 -lcurl -lcompression", sizeof libs - strlen(libs) - 1);
#else
            strncat(libs, " -lbz2 -lcurl", sizeof libs - strlen(libs) - 1);
#endif
            /* Il runtime MINIMO include `gmb_abi.o` (forzato da `gmb_paths.c` via
             * `gmb_abi_link_anchor`) che usa `llmdesc_abi_*`: senza questa statica il link
             * falliva con "_llmdesc_abi_locate non definito" anche su un PRINT. */
            const char* ldcand[] = {
                "native/gmb_runtime_c/build/llmdesc_build/libllmdesc.a",
                "native/gmb_vm_c/build/gmb_runtime_c_build/llmdesc_build/libllmdesc.a",
            };
            for (size_t i = 0; i < sizeof ldcand / sizeof ldcand[0]; i++) {
                char lp[CLI_PATH];
                snprintf(lp, sizeof lp, "%s/%s", tc.install, ldcand[i]);
                if (!is_file(lp)) continue;
                char lq[CLI_PATH + 8];
                shell_quote(lp, lq, sizeof lq);
                strncat(libs, " ", sizeof libs - strlen(libs) - 1);
                strncat(libs, lq, sizeof libs - strlen(libs) - 1);
                break;
            }
        }
    }

    char qr[CLI_PATH + 8], qexe[CLI_PATH + 8];
    shell_quote(runtime, qr, sizeof qr);
    shell_quote(exe, qexe, sizeof qexe);
    shell_quote(obj, qc, sizeof qc);
    /* `-lcrypto -lssl` restano in ENTRAMBE le forme del link (simboli attesi al caricamento);
     * la `-L` arriva da `ldirs`, cioè dalla CACHE. */
    if (is_lib)
        snprintf(cmd, sizeof cmd, "cc %s %s %s -lm -lpthread -lcrypto -lssl -o %s 2>&1",
                 GMB_LIB_LDFLAGS, qc, qr, qexe);
    else
        snprintf(cmd, sizeof cmd, "cc %s %s %s%s%s -lm -lpthread -lcrypto -lssl%s -o %s 2>&1",
                 qc, qr, qchart, ldirs, libs, GMB_EXE_FRAMEWORKS, qexe);
    rc = run_cmd(cmd, &log);
    if (rc != 0) {
        char msg[1200];
        const char* hint = "";
        if (log && (strstr(log, "llmdesc") || strstr(log, "gmbllm") || strstr(log, "llmcodec")))
            hint = "  [questo programma usa il motore AI/LLM: il CLI linka il runtime minimo. "
                   "Compilalo con tools/sh/gmb_c99native_run.sh, che linka il lib-set completo]";
        snprintf(msg, sizeof msg, "link fallito (rc=%d): %s%s", rc, log ? log : "", hint);
        diag_add(diags, 1, "link", entry, 0, msg);
        free(log);
        return 5;
    }
    free(log);
    return 0;
}
