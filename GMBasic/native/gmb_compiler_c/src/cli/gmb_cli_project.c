/* gmb_cli_project.c — comandi `check` e `build` del CLI `gmb`, estratti da gmb_cli.c.
 *
 * PERCHÉ: `gmb_cli.c` era 1602 righe, sopra la soglia di 1200 (AGENTS §0quater). Lo split
 * separa i COMANDI che lavorano sul progetto (scoperta sorgenti/moduli, driver, -I, librerie,
 * link) dal resto (parsing argomenti, minitests, main). Il contratto condiviso è in
 * `gmb_cli_internal.h`; gli helper comuni restano definiti in gmb_cli.c.
 */
#include "gmb_cli_internal.h"
#include "gmb_inc_resolve.h"
#include "gmb_embed.h"         /* EMBEDFILE: cartella base dei percorsi relativi */
#include "gmb_cc_defaults.h"   /* ricetta UNICA degli -I e delle statiche (la stessa del driver) */
#include "gmb_lexer.h"   /* check: validazione sintattica (parità con gmbcs) */
#include "gmb_parser.h"

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


/* Valida la sintassi di un file col front-end C (lexer+parser). Prima `check` guardava
 * solo NAMESPACE/IMPORT testuali: un file con un errore di sintassi risultava OK, mentre
 * `gmbcs check` (che compila) lo rifiutava — divergenza reale fra i due compilatori. */
static void check_syntax(const char* path, DiagList* diags)
{
    gmb_embed_set_base_from_path(path);   /* EMBEDFILE: i path relativi sono al programma */
    char* src = read_all(path);
    if (!src) return;
    gmb_lex_result_t lx = gmb_lexer_scan(src, (long)strlen(src));
    char* err = NULL;
    gmb_program_t prog = gmb_parse(&lx, &err);
    if (err && *err) diag_add(diags, 1, "parse", path, 0, err);
    free(err);
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx);
    free(src);
}

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
        if (ends_with(sources[i], ".bas")) check_syntax(sources[i], diags);
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
    return gmb_cc_cache_libdir(out, cap);   /* una sola copia (gmb_cc_defaults.c) */
}

/* Cerca il driver accanto al CLI, nel build del repo, poi in tools/. */

/* Sottostringa case-insensitive (usata per riconoscere i programmi TRAY). */
static int gmb_ci_contains(const char* hay, const char* needle) {
    size_t n = needle ? strlen(needle) : 0;
    if (!hay || n == 0) return 0;
    for (const char* p = hay; *p; p++)
        if (strncasecmp(p, needle, n) == 0) return 1;
    return 0;
}

/* true se il programma usa il namespace TRAY: in quel caso il link deve includere la
 * dylib CONDIVISA gmb_tray (gmb_tray_* non è nella statica del runtime). */
static int source_uses_tray(const char* entry) {
    char* src = read_all(entry);
    if (!src) return 0;
    int used = gmb_ci_contains(src, "TRAY");
    free(src);
    return used;
}

int find_driver(Toolchain* tc, const char* argv0) {
    /* La root dell'install serve SEMPRE (include del runtime e lib per il link), anche
     * quando il driver è trovato accanto al CLI nel build del repo. */
    (void)gmb_resolve_install_root(tc->install, sizeof tc->install);

    /* Cartella dell'ESEGUIBILE, non di argv[0]: invocato dal PATH (`gmb`, senza `/`) argv[0]
     * non contiene alcuna directory, quindi i companion accanto al binario (`gmbnative`,
     * `gmbchunk`) restavano invisibili e `gmb build` rispondeva "gmbnative non trovato"
     * pur avendo il tool lì accanto. */
    char dir[CLI_PATH];
    if (!gmb_exe_dir(dir, sizeof dir) || !dir[0]) {
        char self[CLI_PATH];
        snprintf(self, sizeof self, "%s", argv0 ? argv0 : "");
        path_dir(self, dir, sizeof dir);
    }

    char cand[CLI_PATH];
    snprintf(cand, sizeof cand, "%s/gmbnative", dir[0] ? dir : ".");
    if (is_file(cand)) { snprintf(tc->driver, sizeof tc->driver, "%s", cand); return 1; }

    if (!tc->install[0]) return 0;
    snprintf(cand, sizeof cand, "%s/native/gmb_compiler_c/build/gmbnative", tc->install);
    if (is_file(cand)) { snprintf(tc->driver, sizeof tc->driver, "%s", cand); return 1; }
    snprintf(cand, sizeof cand, "%s/tools/gmbnative", tc->install);
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

#define CLI_MAX_INC 32

/* Elenco degli -I REALI (solo cartelle esistenti), in ordine di priorità: runtime e domini
 * dell'install → LLMEnv (root RISOLTA, non un path hardcoded) → header terze parti (cache
 * `dev`). UNA sola implementazione: la usano il link (`build_inc`) E `gmb doctor`, che
 * altrimenti potrebbe dichiarare "a posto" un ambiente con cui `cc` non compila. */
int gmb_cli_inc_dirs(const char* install, char (*out)[CLI_PATH], int max) {
    /* Ricetta UNICA in `gmb_cc_defaults.c`: la stessa lista che passa il driver `gmbnative`
     * quando `cc` compila il C emesso. Una copia sola = un verdetto solo. */
    return gmb_cc_inc_dirs(install, out, max);
}

/* Ricetta degli -I per compilare il C emesso: quella di tools/gmb_c99native_run.sh (il C
 * generato include gli header del runtime e di LLMEnv). Una sola copia, usata sia da
 * `build` sia da `minitests`: due ricette divergenti darebbero verdetti diversi. */
void build_inc(char* inc, size_t cap, const char* install) {
    inc[0] = 0;
    /* 32 × 4096 = 128 KB: heap, non stack (build_inc è chiamata da main). */
    char (*dirs)[CLI_PATH] = (char (*)[CLI_PATH])malloc((size_t)CLI_MAX_INC * CLI_PATH);
    if (!dirs) return;
    int n = gmb_cli_inc_dirs(install, dirs, CLI_MAX_INC);
    for (int i = 0; i < n; i++) add_inc(inc, cap, dirs[i]);
    free(dirs);
}

/* Prodotto di un sorgente LIBRARY: una libreria condivisa (.gmblib), non un eseguibile.
 * `dynamic_lookup` è la scelta già fatta da tools/gmb_c99native_run.sh: la libreria si carica
 * in un host che espone il runtime (e le sue dipendenze grafiche), quindi i simboli che il
 * corpo non usa non vengono mai risolti. Senza, il link fallirebbe per le sole SDL che il
 * runtime console trascina con sé anche in una libreria di sola aritmetica. */
#if defined(__APPLE__)
#define GMB_LIB_EXT ".dylib"
#define GMB_LIB_LDFLAGS "-shared -fPIC -Wl,-undefined,dynamic_lookup"
/* macOS: `dlopen` risolve i simboli del core con la ricerca a due livelli (dynamic_lookup). */
#define GMB_EXE_EXPORT_DYNAMIC ""
/* Tiene in LC_LOAD_DYLIB solo le librerie che il binario USA davvero: senza, il `-lSDL2`
 * incondizionato del link faceva dipendere anche un `PRINT` da SDL/Vulkan (e `--alllibs`
 * ne copiava 30 inutilmente). Stessa scelta di tools/gmb_c99native_run.sh. */
#define GMB_EXE_STRIP_UNUSED_LIBS "-Wl,-dead_strip_dylibs"
#else
#define GMB_LIB_EXT ".so"
#define GMB_LIB_LDFLAGS "-shared -fPIC"
#define GMB_EXE_STRIP_UNUSED_LIBS "-Wl,--as-needed"
/* ELF: la dylib AI (`libgmb_runtime_ai.so`, caricata con `dlopen`) risolve a load-time i
 * simboli del core (`gmb_raise_error`, `gmb_paths_*`, …). Se l'eseguibile non li esporta
 * nella tabella dinamica il caricamento fallisce con "undefined symbol": `--export-dynamic`
 * li pubblica (l'equivalente di `-rdynamic`). */
#define GMB_EXE_EXPORT_DYNAMIC " -Wl,--export-dynamic"
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
                 "gmbnative non trovato (build native/gmb_compiler_c)");
        return 5;
    }

    char base[256];
    snprintf(base, sizeof base, "%s", entry);
    char* slash = strrchr(base, '/');
    char* name = slash ? slash + 1 : base;
    char* dot = strrchr(name, '.');
    if (dot) *dot = 0;

    /* --target: il C compila solo nativo/C99. `il|pcode` sono dell'engine C#. */
    if (o->target && *o->target && strcasecmp(o->target, "native") != 0 && strcasecmp(o->target, "c99") != 0) {
        diag_add(diags, 1, "target", entry, 0,
                 "il target indicato è dell'engine C#: usa `--engine csharp` "
                 "(il compilatore C produce solo eseguibili nativi/C99)");
        return 2;
    }

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
    /* -D/--define: definiscono il preprocessore BASIC nel DRIVER (processo separato). */
    char defargs[32 * 200]; defargs[0] = 0;
    for (int di = 0; di < o->ndefines; di++) {
        char q[192];
        shell_quote(o->defines[di], q, sizeof q);
        size_t used = strlen(defargs);
        snprintf(defargs + used, sizeof defargs - used, " --define %s", q);
    }
    snprintf(cmd, sizeof cmd, "%s %s %s --no-exe%s%s 2>&1", qd, qe, qc, gpmarg, defargs);
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
    /* MINIMO per i programmi TESTUALI: il runtime completo trascina il motore AI
     * (llmdesc/gmbllm/llmcodec) che questo CLI non linka, e `gmb build|run` moriva con
     * "_llmdesc_abi_locate non definito" anche su un solo PRINT (bug riprodotto 2026-10-29).
     * Stessa scelta di tools/gmb_c99native_run.sh: min se il sorgente non usa UI/AI.
     * La scoperta vive in `gmb_cc_defaults.c`: una copia sola, condivisa col driver
     * `gmbnative` (che linka il C emesso con la STESSA ricetta). */
    char runtime[CLI_PATH];
    const int need_full = gmb_cc_source_needs_full_runtime(entry);
    if (!gmb_cc_runtime_lib(tc.install, need_full, runtime, sizeof runtime)) {
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
    if (!is_lib && gmb_cc_chart_lib(tc.install, chart, sizeof chart))
        shell_quote(chart, qchart, sizeof qchart);

    /* Dir di ricerca delle librerie grafiche: prima quelle nel repo (Resources/libs è il
     * bundle che l'app spedisce), poi le dir di sistema. Le -L si emettono solo se esistono. */
    char ldir_storage[4][CLI_PATH];
    const char* ldirs_ptr[4];
    size_t nldirs = 0;
    /* PRIMA di ogni dir assoluta: la cartella dell'eseguibile. Così le copie di `--alllibs`
     * vincono sulla cache (comportamento di una distribuzione) e restano valide su una
     * macchina dove la cache non esiste. */
    char ldirs[CLI_PATH * 4];
#if defined(__APPLE__)
    snprintf(ldirs, sizeof ldirs, " -Wl,-rpath,@loader_path/libs/" GMB_RID_STR " -Wl,-rpath,@loader_path");
#else
    snprintf(ldirs, sizeof ldirs, " -Wl,-rpath,'$ORIGIN/libs/" GMB_RID_STR "' -Wl,-rpath,'$ORIGIN'");
#endif
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
            /* SOLO `-L` (serve al LINK) e NON `-Wl,-rpath,<dir assoluta>`: un rpath assoluto
             * (`$HOME/.gmbasic/libs/<rid>`, `/usr/lib`) resta cucito nel binario e lo lega
             * alla macchina che ha compilato — erano le 2 voci assolute di ogni eseguibile
             * prodotto da `gmb build`. A RUNTIME le dylib arrivano da `libs/<rid>/` accanto
             * all'eseguibile (prima voce di `ldirs`, relativa) e `gmb_deploy_bundle` ce le
             * porta con sé, SDL3 compresa (`dp_ensure_sdl3`): il programma gira da qualunque
             * cartella, anche su un'altra macchina. */
            size_t l = strlen(ldirs);
            if (l < sizeof ldirs)
                snprintf(ldirs + l, sizeof ldirs - l, " -L%s", q);
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
        /* bz2/curl(/libcompression): usati dal runtime anche nel percorso COMPLETO
         * (gmb_network.c chiama curl_easy_*). Prima erano solo nel ramo `use_min`. */
#if defined(__APPLE__)
        strncat(libs, " -lbz2 -lcurl -lcompression", sizeof libs - strlen(libs) - 1);
#else
        strncat(libs, " -lbz2 -lcurl", sizeof libs - strlen(libs) - 1);
#endif
        /* `gmb_abi.o` (forzato da `gmb_paths.c` via `gmb_abi_link_anchor`) usa
         * `llmdesc_abi_*`/`llmdesc_set_abi_policy`: la statica va linkata SEMPRE, anche
         * col runtime COMPLETO — prima il ramo `!use_min` la ometteva e ogni programma
         * grafico falliva con "_llmdesc_set_abi_policy non definito". */
        {
            char lp[CLI_PATH];
            if (gmb_cc_llmdesc_lib(tc.install, lp, sizeof lp)) {
                char lq[CLI_PATH + 8];
                shell_quote(lp, lq, sizeof lq);
                strncat(libs, " ", sizeof libs - strlen(libs) - 1);
                strncat(libs, lq, sizeof libs - strlen(libs) - 1);
            }
        }
        /* FULL runtime: DBF/ISAM/SPEED/RDD/SQL vivono nella statica `gmb_db_c` (fuori dal
         * runtime). Primail link completo la ometteva: un programma SPEED/DBF falliva con
         * "_gmb_db_speed_validate non definito". Solo per i programmi che la richiedono. */
        if (need_full) {
            /* Compressione (LZ4/LZMA/zstd/brotli/pcre2): `gmb_compress.c` e il runtime
             * completo le referenziano; nella cache i nomi hanno la VERSIONE nel file
             * (`libzstd.1.5.7.dylib`), quindi -l non basta: si linkano per path (come
             * tools/sh/gmb_c99native_run.sh). Senza: "_LZ4F_compressFrame non definito". */
            char cache3[CLI_PATH];
            if (gmb_cache_libdir(cache3, sizeof cache3)) {
                static const char* comp[] = {
                    "libzstd.1.5.7.dylib", "liblz4.1.10.0.dylib", "liblzma.5.dylib",
                    "libbrotlienc.1.dylib", "libbrotlidec.1.dylib", "libpcre2-8.0.dylib",
                };
                for (size_t i = 0; i < sizeof comp / sizeof comp[0]; i++) {
                    char cp[CLI_PATH];
                    snprintf(cp, sizeof cp, "%s/%s", cache3, comp[i]);
                    if (!is_file(cp)) continue;
                    char cq[CLI_PATH + 8];
                    shell_quote(cp, cq, sizeof cq);
                    strncat(libs, " ", sizeof libs - strlen(libs) - 1);
                    strncat(libs, cq, sizeof libs - strlen(libs) - 1);
                }
            }
            /* AI/LLM/agente: il runtime completo referenzia agent/doc/llm. Stesso set di
             * tools/sh/gmb_c99native_run.sh (ordine: chi USA precede chi DEFINISCE). */
            static const char* const more[] = {
                "native/gmbadmin/build/gmb_doc_build/libgmb_doc.a",
                "native/gmb_vm_c/build/gmb_doc_c_build/libgmb_doc.a",
                "native/gmbadmin/build/gmb_agent_runtime_build/libgmb_agent_runtime.a",
                "native/gmb_vm_c/build/gmb_agent_runtime_build/libgmb_agent_runtime.a",
                "native/gmb_runtime_c/build/gmbllm_build/libgmbllm_core.a",
                "native/gmb_vm_c/build/gmb_runtime_c_build/gmbllm_build/libgmbllm_core.a",
                "native/gmb_runtime_c/build/llmcodec_build/libllmcodec.a",
                "native/gmb_vm_c/build/gmb_runtime_c_build/llmcodec_build/libllmcodec.a",
                "native/gmb_runtime_c/build/llmengine_plugin_build/libllmengine_plugin.a",
                "native/gmb_vm_c/build/gmb_runtime_c_build/llmengine_plugin_build/libllmengine_plugin.a",
                "native/gmb_runtime_c/build/llmasr_build/libllm_asr.a",
                "native/gmb_vm_c/build/gmb_runtime_c_build/llmasr_build/libllm_asr.a",
            };
            for (size_t i = 0; i < sizeof more / sizeof more[0]; i++) {
                char mp[CLI_PATH];
                snprintf(mp, sizeof mp, "%s/%s", tc.install, more[i]);
                if (!is_file(mp)) continue;
                char mq[CLI_PATH + 8];
                shell_quote(mp, mq, sizeof mq);
                strncat(libs, " ", sizeof libs - strlen(libs) - 1);
                strncat(libs, mq, sizeof libs - strlen(libs) - 1);
            }
            /* codec dylib di LLMEnv (sibling del repo) + tokenizer: path esplicito + rpath. */
            {
                char codec[CLI_PATH];
                snprintf(codec, sizeof codec, "%s/../LLMEnv/LLMEngine/mlx/build-gmbasic", tc.install);
                if (is_dir(codec)) {
                    char q[CLI_PATH + 8];
                    shell_quote(codec, q, sizeof q);
                    strncat(libs, " -L", sizeof libs - strlen(libs) - 1);
                    strncat(libs, q, sizeof libs - strlen(libs) - 1);
                    strncat(libs, " -lllmcodec_ggml -lllmcodec_mlx_affine -lllmcodec_fp8"
                                   " -Wl,-rpath,", sizeof libs - strlen(libs) - 1);
                    strncat(libs, q, sizeof libs - strlen(libs) - 1);
                    char moe[CLI_PATH];
                    snprintf(moe, sizeof moe, "%s/llmengine_moe_mlx_build/libllmengine_moe.a", codec);
                    if (is_file(moe)) {
                        char mq[CLI_PATH + 8];
                        shell_quote(moe, mq, sizeof mq);
                        strncat(libs, " ", sizeof libs - strlen(libs) - 1);
                        strncat(libs, mq, sizeof libs - strlen(libs) - 1);
                    }
                }
                const char* tok[] = {
                    ".." "/LLMEnv/build/LLMToken/libllmtoken.dylib",
                    ".." "/LLMEnv/build/dist/osx-arm64/libllmtoken.dylib",
                };
                for (size_t i = 0; i < sizeof tok / sizeof tok[0]; i++) {
                    char tp[CLI_PATH];
                    snprintf(tp, sizeof tp, "%s/%s", tc.install, tok[i]);
                    if (!is_file(tp)) continue;
                    char tq[CLI_PATH + 8];
                    shell_quote(tp, tq, sizeof tq);
                    strncat(libs, " ", sizeof libs - strlen(libs) - 1);
                    strncat(libs, tq, sizeof libs - strlen(libs) - 1);
                    break;
                }
            }
            const char* dbcand[] = {
                "native/gmb_db_c/build/libgmb_db.a",
                "native/gmb_vm_c/build/gmb_db_c_build/libgmb_db.a",
            };
            for (size_t i = 0; i < sizeof dbcand / sizeof dbcand[0]; i++) {
                char dp[CLI_PATH];
                snprintf(dp, sizeof dp, "%s/%s", tc.install, dbcand[i]);
                if (!is_file(dp)) continue;
                char dq[CLI_PATH + 8];
                shell_quote(dp, dq, sizeof dq);
                strncat(libs, " ", sizeof libs - strlen(libs) - 1);
                strncat(libs, dq, sizeof libs - strlen(libs) - 1);
#if defined(__APPLE__)
                strncat(libs, " -lsqlite3", sizeof libs - strlen(libs) - 1);
#endif
                break;
            }
        }
    }

    /* TRAY: gmb_tray_* vive nella dylib condivisa `gmb_tray` (target CMake omonimo, non
     * nella statica). Va aggiunta al link SOLO per i programmi che usano TRAY; senza, il
     * CLI falliva con "_gmb_tray_available undefined" (arm64). Prima la CACHE
     * (~/.gmbasic/libs/<rid>), poi il build del checkout (<install>/build/libs/<rid>). */
    if (!is_lib && source_uses_tray(entry)) {
        char cache2[CLI_PATH];
        if (gmb_cache_libdir(cache2, sizeof cache2)) {
            char q[CLI_PATH + 8];
            shell_quote(cache2, q, sizeof q);
            strncat(ldirs, " -L", sizeof ldirs - strlen(ldirs) - 1);
            strncat(ldirs, q, sizeof ldirs - strlen(ldirs) - 1);
        }
        {
            char build2[CLI_PATH + 16];
            snprintf(build2, sizeof build2, "%s/build/libs/%s", tc.install, GMB_RID_STR);
            if (is_dir(build2)) {
                char q[CLI_PATH + 8];
                shell_quote(build2, q, sizeof q);
                strncat(ldirs, " -L", sizeof ldirs - strlen(ldirs) - 1);
                strncat(ldirs, q, sizeof ldirs - strlen(ldirs) - 1);
                strncat(ldirs, " -Wl,-rpath,", sizeof ldirs - strlen(ldirs) - 1);
                strncat(ldirs, q, sizeof ldirs - strlen(ldirs) - 1);
            }
        }
        strncat(libs, " -lgmb_tray", sizeof libs - strlen(libs) - 1);
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
        snprintf(cmd, sizeof cmd, "cc " GMB_EXE_STRIP_UNUSED_LIBS " %s %s %s%s%s%s -lm -lpthread -lcrypto -lssl%s -o %s 2>&1",
                 qc, qr, qchart, ldirs, libs, GMB_EXE_EXPORT_DYNAMIC, GMB_EXE_FRAMEWORKS, qexe);
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
    /* Il compilato è ESEGUIBILE per contratto, non per effetto collaterale di `cc`/umask:
     * il bit di esecuzione si mette esplicitamente (0755) su ogni artefatto linkato. */
    chmod(exe, 0755);
    /* Bundle di distribuzione: la dylib AI è SEMPRE in <out>/libs/<rid>/; `--libs` aggiunge
     * la chiusura (default), tutto/gruppi/nomi/componenti; `--plugins/--skills/--brains`
     * creano le sottocartelle. Per una LIBRARY il caricamento è dell'host: si salta. */
    if (!is_lib) {
        DeployOpts d;
        memset(&d, 0, sizeof d);
        d.libs = o->libs;             d.libs_sel = o->libs_sel;
        d.src = entry;
        d.plugins = o->plugins;       d.plugins_sel = o->plugins_sel;
        d.skills = o->skills;         d.skills_sel = o->skills_sel;
        d.brains = o->brains;         d.brains_sel = o->brains_sel;
        gmb_deploy_bundle(exe, tc.install, &d, o->quiet);
        /* Pagine BSP: sidecar A1 per il motore COMPLETO (VM/C). Stesso passo di build:
         * il progetto non deve lanciare tool a mano. */
        gmb_cli_precompile_bsp(project);
    }
    return 0;
}
