/* FILE: native/gmb_compiler_c/src/cli/gmb_cli_doctor.c
 *
 * `gmb doctor` — diagnosi dell'AMBIENTE e installazione di ciò che manca.
 *
 * PERCHÉ ESISTE. `gmb build` falliva con messaggi che nominavano un FILE (`libgmb_runtime.a
 * non trovata`, `gmbnative non trovato`) ma non dicevano che esiste una root (`GMBASIC_ROOT`),
 * che LLMEnv è un REPO SEPARATO (`LLM_ROOT`), né quale comando rimediare. Chi installa la
 * release compilata non ha il checkout e non può dedurlo: l'unica risposta utile è un elenco
 * di pezzi con OK/MANCA e il comando esatto che li produce.
 *
 *   gmb doctor            elenca root, LLMEnv, companion, statiche/dylib, header (-I), cc,
 *                         cache delle librerie e settings — con il comando per ogni buco.
 *   gmb doctor --fix      installa ciò che è SCARICABILE:
 *                           • LLMEnv assente   → git clone (c-vision/LLMEnv) accanto a GMBasic
 *                           • librerie native  → `gmbadmin libs ensure` (GMBasicModules)
 *                           • gmbcs assente    → `dotnet build GMBasicSharp/CompilerCli`
 *                         NON ricompila il compilatore: per statiche e companion stampa il
 *                         comando (una build imprevista dentro un diagnostico sarebbe un
 *                         effetto collaterale enorme, e richiede la toolchain).
 *   gmb doctor --json     la stessa lista in JSON (CI/IDE).
 *   gmb doctor --abi-dir  stampa SOLO la cartella ABI di LLMEnv (descrittori `*.gmbarch`) e la
 *                         fonte da cui viene; exit 1 se non c'è. La usa `~/.zshrc` per esportare
 *                         `LLMDESC_ABI_DIR` senza cablare nessun percorso.
 *
 * DUE SOLE ENV DI PERCORSO (per direttiva): `GMBASIC_ROOT` (install GMBasic) e `LLM_ROOT`
 * (LLMEnv). Nessuna nuova env di comportamento: qualunque altra regolazione passa dal registry
 * delle opzioni (`paths.*` di system.settings.json, letto da gmb_paths.c).
 *
 * Le due root si RISOLVONO, non si presumono: `gmb_resolve_install_root` e
 * `gmb_resolve_llm_root` (frontend) sono le stesse funzioni che usano CLI, driver e link.
 */
#include "gmb_cli_internal.h"
#include "gmb_inc_resolve.h"
#include "gmb_cc_defaults.h"   /* gmb_cc_cache_libdir (RID host della cache) */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
    char install[CLI_PATH];      /* GMBASIC_ROOT risolta */
    char llm[CLI_PATH];          /* LLM_ROOT risolta */
    char exe[CLI_PATH];
    char driver[CLI_PATH];       /* gmbnative */
    char chunk[CLI_PATH];        /* gmbchunk */
    char vmhost[CLI_PATH];       /* gmb_vm_host */
    char admin[CLI_PATH];        /* gmbadmin */
    char gmbcs[CLI_PATH];        /* engine C# (GMBasicSharp) */
    char runtime_min[CLI_PATH];
    char runtime[CLI_PATH];
    char chart[CLI_PATH];
    char db[CLI_PATH];
    char eval[CLI_PATH];
    char bsp[CLI_PATH];
    char cc[CLI_PATH];
    char cache_libs[CLI_PATH];   /* ~/.gmbasic/libs/<rid> trovata */
    char settings[CLI_PATH];
    char abi[CLI_PATH];          /* cartella ABI di LLMEnv (descrittori *.gmbarch) */
    char abi_how[64];            /* fonte: LLMDESC_ABI_DIR | LLM_ROOT/<rel> */
    int  n_abi;                  /* descrittori trovati */
    int  n_cache_files;
    int  n_inc;                  /* numero di -I REALI che `cc` riceverebbe */
    int  has_runtime_h;
} DocEnv;

/* Il CLI è mono-thread: un buffer unico da ~100 KB evita 17 array da 4 KB sullo stack. */
static DocEnv g_doc;

/* ── helper di filesystem ─────────────────────────────────────────────────────────────── */

static int doc_file(const char* dir, const char* rel, char* out, size_t cap) {
    if (!dir || !*dir || !rel || !*rel) return 0;
    char p[CLI_PATH];
    if (rel[0] == '/') snprintf(p, sizeof p, "%s", rel);
    else snprintf(p, sizeof p, "%s/%s", dir, rel);
    if (!is_file(p)) return 0;
    if (out && cap) snprintf(out, cap, "%s", p);
    return 1;
}

static int doc_first(char (*dirs)[CLI_PATH], int nd, const char* rel, char* out, size_t cap) {
    for (int i = 0; i < nd; i++) if (doc_file(dirs[i], rel, out, cap)) return 1;
    return 0;
}

/* Cartelle in cui possono stare gli artefatti: accanto all'eseguibile (release), build dei
 * componenti del checkout, cache di build per RID. Solo quelle esistenti. */
static int doc_dirs(const char* install, char (*out)[CLI_PATH], int max) {
    int n = 0;
    char exe[CLI_PATH];
    if (gmb_exe_dir(exe, sizeof exe) && exe[0] && n < max) snprintf(out[n++], CLI_PATH, "%s", exe);
    static const char* rel[] = {
        "build", "native/gmb_runtime_c/build", "native/gmb_vm_c/build",
        "native/gmb_chart/build", "native/gmb_db_c/build", "native/gmb_doc_c/build",
        "native/gmb_torch/build", "native/gmb_agent_runtime/build",
        "native/gmb_compiler_c/build", "native/gmbadmin/build", "native/gmb_bytecode/build",
        "native/gmb_c99native/build",
    };
    for (size_t i = 0; i < sizeof rel / sizeof rel[0] && n < max; i++) {
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/%s", install, rel[i]);
        if (is_dir(p)) snprintf(out[n++], CLI_PATH, "%s", p);
    }
    /* <install>/build/libs/<rid>: una sola sottocartella per macchina, non serve il RID. */
    {
        char base[CLI_PATH];
        snprintf(base, sizeof base, "%s/build/libs", install);
        DIR* d = opendir(base);
        if (d) {
            struct dirent* p;
            while ((p = readdir(d)) != NULL) {
                if (p->d_name[0] == '.') continue;
                char sub[CLI_PATH];
                snprintf(sub, sizeof sub, "%s/%s", base, p->d_name);
                if (is_dir(sub) && n < max) snprintf(out[n++], CLI_PATH, "%s", sub);
            }
            closedir(d);
        }
    }
    /* Sottoprogetti ANNIDATI: `native/<comp>/build/<progetto>_build`. `gmb_chart` non ha una
     * build propria: `gmb_runtime_c/CMakeLists.txt` fa `add_subdirectory(../gmb_chart
     * gmb_chart_build)`, quindi l'archivio vive in
     * `native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a`. Senza questa scansione il
     * dottore dichiarava MANCANTE una libreria presente (un solo livello di annidamento: la
     * profondità è fissa per costruzione, nessuna ricerca ricorsiva arbitraria). */
    {
        char nat[CLI_PATH];
        snprintf(nat, sizeof nat, "%s/native", install);
        DIR* dn = opendir(nat);
        if (dn) {
            struct dirent* c;
            while ((c = readdir(dn)) != NULL) {
                if (c->d_name[0] == '.') continue;
                char b[CLI_PATH];
                snprintf(b, sizeof b, "%s/%s/build", nat, c->d_name);
                if (!is_dir(b)) continue;
                if (n < max) snprintf(out[n++], CLI_PATH, "%s", b);
                DIR* db = opendir(b);
                if (!db) continue;
                struct dirent* s;
                while ((s = readdir(db)) != NULL) {
                    if (s->d_name[0] == '.') continue;
                    char sub[CLI_PATH];
                    snprintf(sub, sizeof sub, "%s/%s", b, s->d_name);
                    if (is_dir(sub) && n < max) snprintf(out[n++], CLI_PATH, "%s", sub);
                }
                closedir(db);
            }
            closedir(dn);
        }
    }
    return n;
}

static int doc_which(const char* name, char* out, size_t cap) {
    const char* pe = getenv("PATH");
    if (pe) {
        char tmp[8192];
        snprintf(tmp, sizeof tmp, "%s", pe);
        for (char* d = strtok(tmp, ":"); d; d = strtok(NULL, ":")) {
            char p[CLI_PATH];
            snprintf(p, sizeof p, "%s/%s", d, name);
            if (access(p, X_OK) == 0) { if (out && cap) snprintf(out, cap, "%s", p); return 1; }
        }
    }
    return 0;
}

/* Esegue un figlio e ne aspetta l'exit code (-1 se non eseguibile). */
static int doc_run(char* const argv[]) {
    pid_t pid = fork();
    if (pid == 0) { execvp(argv[0], argv); _exit(127); }
    if (pid < 0) return -1;
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/* Cache delle librerie native: `~/.gmbasic/libs/<rid>/`. Si preferisce il RID dell'HOST
 * (è quello che serve alle build locali); solo se manca si ripiega sulla sottocartella più
 * popolata (una macchina può avere cache di cross-build: su un Mac con `linux-x64` più
 * popolata il doctor riportava quella, fuorviante per l'utente). */
static void doc_scan_cache(void) {
    const char* home = getenv("HOME");
    if (!home || !*home) return;
    char root[CLI_PATH];
    snprintf(root, sizeof root, "%s/.gmbasic/libs", home);
    /* 1. RID host (se la cartella esiste) */
    {
        char host[CLI_PATH];
        if (gmb_cc_cache_libdir(host, sizeof host)) {
            DIR* hs = opendir(host);
            if (hs) {
                int n = 0;
                struct dirent* q;
                while ((q = readdir(hs)) != NULL) if (q->d_name[0] != '.') n++;
                closedir(hs);
                if (n > 0) {
                    snprintf(g_doc.cache_libs, sizeof g_doc.cache_libs, "%s", host);
                    g_doc.n_cache_files = n;
                    return;
                }
            }
        }
    }
    DIR* d = opendir(root);
    if (!d) { snprintf(g_doc.cache_libs, sizeof g_doc.cache_libs, "%s", root); return; }
    int best = 0;
    struct dirent* p;
    while ((p = readdir(d)) != NULL) {
        if (p->d_name[0] == '.') continue;
        char sub[CLI_PATH];
        snprintf(sub, sizeof sub, "%s/%s", root, p->d_name);
        DIR* s = opendir(sub);
        if (!s) continue;
        int n = 0;
        struct dirent* q;
        while ((q = readdir(s)) != NULL) if (q->d_name[0] != '.') n++;
        closedir(s);
        if (n > best) { best = n; snprintf(g_doc.cache_libs, sizeof g_doc.cache_libs, "%s", sub); }
    }
    closedir(d);
    g_doc.n_cache_files = best;
}

/* ── cartella ABI di LLMEnv (descrittori *.gmbarch) ──────────────────────────────────── */

/* Cartella valida = contiene almeno un `*.gmbarch`. Il descrittore NON si apre: il formato è
 * di LLMEnv (`llmdesc`), qui serve solo sapere se la cartella è quella giusta. */
static int doc_abi_desc(const char* dir, int* count) {
    DIR* d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        if (l > 8 && strcmp(e->d_name + l - 8, ".gmbarch") == 0) n++;
    }
    closedir(d);
    if (count) *count = n;
    return n > 0;
}

/* Scoperta SENZA percorsi cablati: `$LLMDESC_ABI_DIR` (la prima fonte di `llmdesc`) e poi i
 * layout RELATIVI a LLM_ROOT già risolta (env, install, repo fratello): `ABI/`, `lib/llmenv/ABI/`
 * e `share/llmenv/ABI/` (pkg e Homebrew non mettono l'ABI nella radice del prefisso).
 * 1 = trovata, con cartella e fonte. */
static int doc_abi_find(char* out, size_t cap, char* how, size_t howcap) {
    const char* env = getenv("LLMDESC_ABI_DIR");
    if (env && *env && doc_abi_desc(env, NULL)) {
        snprintf(out, cap, "%s", env);
        snprintf(how, howcap, "LLMDESC_ABI_DIR");
        return 1;
    }
    if (g_doc.llm[0]) {
        static const char* rel[] = { "ABI", "lib/llmenv/ABI", "share/llmenv/ABI" };
        for (size_t i = 0; i < sizeof rel / sizeof rel[0]; i++) {
            char p[CLI_PATH];
            snprintf(p, sizeof p, "%s/%s", g_doc.llm, rel[i]);
            if (doc_abi_desc(p, NULL)) {
                snprintf(out, cap, "%s", p);
                snprintf(how, howcap, "LLM_ROOT/%s", rel[i]);
                return 1;
            }
        }
    }
    if (out && cap) out[0] = 0;
    if (how && howcap) how[0] = 0;
    return 0;
}

/* ── rilevamento ──────────────────────────────────────────────────────────────────────── */

static void doc_probe(void) {
    memset(&g_doc, 0, sizeof g_doc);
    (void)gmb_resolve_install_root(g_doc.install, sizeof g_doc.install);
    (void)gmb_resolve_llm_root(g_doc.llm, sizeof g_doc.llm);
    (void)gmb_exe_dir(g_doc.exe, sizeof g_doc.exe);
    {   /* path NORMALIZZATI nel report: la root scoperta può contenere `../..` (es.
         * `<GMBasic>/../../LLMEnv`) e un diagnostico deve mostrare il percorso reale. */
        char real[CLI_PATH];
        if (g_doc.install[0] && realpath(g_doc.install, real))
            snprintf(g_doc.install, sizeof g_doc.install, "%s", real);
        if (g_doc.llm[0] && realpath(g_doc.llm, real))
            snprintf(g_doc.llm, sizeof g_doc.llm, "%s", real);
    }

    /* Cartella ABI dei descrittori: se c'è, è quella che `llmdesc` userà (l'env vince su tutto). */
    if (doc_abi_find(g_doc.abi, sizeof g_doc.abi, g_doc.abi_how, sizeof g_doc.abi_how))
        (void)doc_abi_desc(g_doc.abi, &g_doc.n_abi);

    /* 48 slot: companion+statiche dell'install, `build/libs/<rid>` e i sottoprogetti
     * annidati (`build/gmb_chart_build`, `build/gmb_runtime_c_build`, …). */
    char (*dirs)[CLI_PATH] = (char (*)[CLI_PATH])malloc(48 * CLI_PATH);
    int nd = dirs ? doc_dirs(g_doc.install, dirs, 48) : 0;

    if (dirs) {
        (void)doc_first(dirs, nd, "gmbnative", g_doc.driver, sizeof g_doc.driver);
        (void)doc_first(dirs, nd, "gmbchunk", g_doc.chunk, sizeof g_doc.chunk);
        (void)doc_first(dirs, nd, "gmb_vm_host", g_doc.vmhost, sizeof g_doc.vmhost);
        (void)doc_first(dirs, nd, "gmbadmin", g_doc.admin, sizeof g_doc.admin);
        (void)doc_first(dirs, nd, "libgmb_runtime_min.a", g_doc.runtime_min, sizeof g_doc.runtime_min);
        (void)doc_first(dirs, nd, "libgmb_runtime.a", g_doc.runtime, sizeof g_doc.runtime);
        (void)doc_first(dirs, nd, "libgmb_chart.a", g_doc.chart, sizeof g_doc.chart);
        (void)doc_first(dirs, nd, "libgmb_db.a", g_doc.db, sizeof g_doc.db);
        (void)doc_first(dirs, nd, "libgmb_eval.dylib", g_doc.eval, sizeof g_doc.eval);
        if (!g_doc.eval[0]) (void)doc_first(dirs, nd, "libgmb_eval.so", g_doc.eval, sizeof g_doc.eval);
        (void)doc_first(dirs, nd, "libgmb_bsp.dylib", g_doc.bsp, sizeof g_doc.bsp);
        if (!g_doc.bsp[0]) (void)doc_first(dirs, nd, "libgmb_bsp.so", g_doc.bsp, sizeof g_doc.bsp);
        free(dirs);
    }

    /* gmbcs: PATH + checkout (gmb_cli_find_gmbcs), poi le cfg del checkout sotto <root>/.. */
    if (!gmb_cli_find_gmbcs(NULL, g_doc.gmbcs, sizeof g_doc.gmbcs)) {
        static const char* cfgs[] = { "Debug", "Release" };
        for (size_t i = 0; i < sizeof cfgs / sizeof cfgs[0] && !g_doc.gmbcs[0]; i++) {
            char p[CLI_PATH];
            snprintf(p, sizeof p, "%s/../GMBasicSharp/CompilerCli/bin/%s/net10.0/gmbcs",
                     g_doc.install, cfgs[i]);
            if (is_file(p)) snprintf(g_doc.gmbcs, sizeof g_doc.gmbcs, "%s", p);
        }
    }
    {   /* sempre ASSOLUTO nel report: gmb_cli_find_gmbcs restituisce anche path relativi
         * alla CWD (checkout di sviluppo), illeggibili come "dove sta". */
        char real[CLI_PATH];
        if (g_doc.gmbcs[0] && realpath(g_doc.gmbcs, real))
            snprintf(g_doc.gmbcs, sizeof g_doc.gmbcs, "%s", real);
    }

    (void)doc_which("cc", g_doc.cc, sizeof g_doc.cc);
    doc_scan_cache();

    {
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/../shared/settings/system.settings.json", g_doc.install);
        if (is_file(p)) snprintf(g_doc.settings, sizeof g_doc.settings, "%s", p);
        else if (!doc_file(g_doc.exe, "system.settings.json", g_doc.settings, sizeof g_doc.settings))
            snprintf(g_doc.settings, sizeof g_doc.settings, "%s", p);   /* atteso, assente */
    }

    g_doc.has_runtime_h = doc_file(g_doc.install, "native/gmb_runtime_c/include/gmb_runtime.h", NULL, 0);
    {
        char (*inc)[CLI_PATH] = (char (*)[CLI_PATH])malloc(32 * CLI_PATH);
        if (inc) { g_doc.n_inc = gmb_cli_inc_dirs(g_doc.install, inc, 32); free(inc); }
    }
}

/* ── --fix: installa ciò che è scaricabile ───────────────────────────────────────────── */

static int doc_fix_llmenv(const char* repo) {
    if (!g_doc.install[0]) {
        fprintf(stderr, "doctor: GMBASIC_ROOT non risolta: non so dove installare LLMEnv\n");
        return 0;
    }
    char git[CLI_PATH];
    if (!doc_which("git", git, sizeof git)) {
        fprintf(stderr, "doctor: git non trovato: installa LLMEnv a mano in %s/../LLMEnv\n", g_doc.install);
        return 0;
    }
    char dest[CLI_PATH];
    snprintf(dest, sizeof dest, "%s/../LLMEnv", g_doc.install);
    if (is_dir(dest)) {
        fprintf(stderr, "doctor: %s esiste ma non ha LLMCodec/include: non la sovrascrivo\n", dest);
        return 0;
    }
    fprintf(stderr, "doctor: installo LLMEnv → %s (git clone --depth 1)\n", dest);
    char* av[] = { git, "clone", "--depth", "1", (char*)repo, dest, NULL };
    int rc = doc_run(av);
    if (rc != 0) { fprintf(stderr, "doctor: git clone rc=%d\n", rc); return 0; }
    char real[CLI_PATH];
    fprintf(stderr, "doctor: LLMEnv installata in %s\n", realpath(dest, real) ? real : dest);
    return 1;
}

static int doc_fix_libs(void) {
    char admin[CLI_PATH];
    if (!gmb_cli_find_admin(admin, sizeof admin)) {
        fprintf(stderr, "doctor: gmbadmin non trovato: librerie native non scaricabili\n");
        return 0;
    }
    fprintf(stderr, "doctor: scarico le librerie native (gmbadmin libs ensure)…\n");
    char* av[] = { admin, "libs", "ensure", NULL };
    int rc = doc_run(av);
    if (rc != 0) fprintf(stderr, "doctor: `gmbadmin libs ensure` rc=%d\n", rc);
    return rc == 0;
}

static int doc_fix_gmbcs(void) {
    char csproj[CLI_PATH];
    csproj[0] = 0;
    static const char* names[] = { "CompilerCli.csproj", "gmbcs.csproj" };
    for (size_t i = 0; i < sizeof names / sizeof names[0] && !csproj[0]; i++) {
        char p[CLI_PATH];
        snprintf(p, sizeof p, "%s/../GMBasicSharp/CompilerCli/%s", g_doc.install, names[i]);
        if (is_file(p)) snprintf(csproj, sizeof csproj, "%s", p);
    }
    if (!csproj[0]) {
        fprintf(stderr, "doctor: progetto gmbcs non trovato (atteso GMBasicSharp/CompilerCli/*.csproj)\n");
        return 0;
    }
    char dotnet[CLI_PATH];
    if (!doc_which("dotnet", dotnet, sizeof dotnet)) {
        fprintf(stderr, "doctor: dotnet non installato: `--engine csharp` resterà indisponibile\n");
        return 0;
    }
    fprintf(stderr, "doctor: compilo gmbcs (dotnet build %s)\n", csproj);
    char* av[] = { dotnet, "build", csproj, "-v", "q", NULL };
    int rc = doc_run(av);
    if (rc != 0) fprintf(stderr, "doctor: dotnet build rc=%d\n", rc);
    return rc == 0;
}

/* ── report ──────────────────────────────────────────────────────────────────────────── */

typedef struct {
    const char* name;
    int required;
    int ok;
    const char* found;
    const char* hint;
} DocRow;

static void doc_json_str(FILE* f, const char* s) {
    fputc('"', f);
    for (const unsigned char* p = (const unsigned char*)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f);  break;
        case '\t': fputs("\\t", f);  break;
        default:
            if (*p < 0x20) fprintf(f, "\\u%04x", *p);
            else fputc(*p, f);
        }
    }
    fputc('"', f);
}

static int doc_report(int json) {
    DocRow rows[20];
    int nr = 0;
    #define DOC_ROW(nm, req, okc, fnd, hnt) \
        do { rows[nr].name = (nm); rows[nr].required = (req); rows[nr].ok = (okc); \
             rows[nr].found = (fnd); rows[nr].hint = (hnt); nr++; } while (0)

    const int root_ok = g_doc.install[0] && is_dir(g_doc.install);
    DOC_ROW("GMBASIC_ROOT", 1, root_ok, g_doc.install,
            "export GMBASIC_ROOT=<cartella che contiene native/>");
    DOC_ROW("LLM_ROOT", 1, g_doc.llm[0] && is_dir(g_doc.llm), g_doc.llm,
            "gmb doctor --fix   (git clone c-vision/LLMEnv → <root>/../LLMEnv)");
    {   /* una riga sola: cartella, fonte e descrittori (la fonte dice CHI ha deciso) */
        static char abi_found[CLI_PATH + 128];
        if (g_doc.abi[0])
            snprintf(abi_found, sizeof abi_found, "%s  (%s, %d descrittori)",
                     g_doc.abi, g_doc.abi_how, g_doc.n_abi);
        else abi_found[0] = 0;
        DOC_ROW("ABI (LLMEnv)", 0, g_doc.abi[0], abi_found,
                "export LLMDESC_ABI_DIR=<cartella ABI>   (`gmb doctor --abi-dir` la stampa)");
    }
    DOC_ROW("gmbnative", 1, g_doc.driver[0], g_doc.driver,
            "cmake --build native/gmb_compiler_c/build --target gmbnative");
    DOC_ROW("gmbchunk", 1, g_doc.chunk[0], g_doc.chunk,
            "cmake --build native/gmb_bytecode/build --target gmbchunk");
    DOC_ROW("gmb_vm_host", 0, g_doc.vmhost[0], g_doc.vmhost,
            "cmake --build native/gmb_vm_c/build --target gmb_vm_host (REPL/notebook/EVAL)");
    DOC_ROW("gmbadmin", 0, g_doc.admin[0], g_doc.admin,
            "cmake --build native/gmbadmin/build (pkg/libs/brain/setup)");
    DOC_ROW("gmbcs", 0, g_doc.gmbcs[0], g_doc.gmbcs,
            "gmb doctor --fix   (dotnet build GMBasicSharp/CompilerCli)");
    DOC_ROW("libgmb_runtime_min.a", 1, g_doc.runtime_min[0], g_doc.runtime_min,
            "cmake --build native/gmb_runtime_c/build");
    DOC_ROW("libgmb_runtime.a", 1, g_doc.runtime[0], g_doc.runtime,
            "cmake --build native/gmb_runtime_c/build");
    DOC_ROW("libgmb_chart.a", 0, g_doc.chart[0], g_doc.chart,
            "cmake --build native/gmb_runtime_c/build (gmb_chart è un sottoprogetto)");
    DOC_ROW("libgmb_db.a", 0, g_doc.db[0], g_doc.db,
            "cmake --build native/gmb_db_c/build (DBF/ISAM)");
    DOC_ROW("libgmb_eval.dylib", 0, g_doc.eval[0], g_doc.eval,
            "cmake --build native/gmb_vm_c/build (EVAL, REPL, notebook)");
    DOC_ROW("libgmb_bsp.dylib", 0, g_doc.bsp[0], g_doc.bsp,
            "cmake --build native/gmb_vm_c/build (pagine BSP)");
    DOC_ROW("header (-I)", 1, g_doc.n_inc > 0 && g_doc.has_runtime_h, "",
            "reinstalla la release (gmbllm/, shared/include, native/*/include)");
    DOC_ROW("cc", 1, g_doc.cc[0], g_doc.cc, "xcode-select --install");
    DOC_ROW("cache libs", 0, g_doc.n_cache_files > 0, g_doc.cache_libs,
            "gmb libs ensure   (GMBasicModules)");
    DOC_ROW("settings", 1, g_doc.settings[0] && is_file(g_doc.settings), g_doc.settings,
            "reinstalla shared/settings (o copia system.settings.json accanto a bin/)");
    #undef DOC_ROW

    int ok = 0, reqmiss = 0;
    for (int i = 0; i < nr; i++) {
        if (rows[i].ok) ok++;
        else if (rows[i].required) reqmiss++;
    }

    if (json) {
        printf("{\n  \"ok\": %s,\n  \"missingRequired\": %d,\n  \"checks\": [\n",
               reqmiss == 0 ? "true" : "false", reqmiss);
        for (int i = 0; i < nr; i++) {
            printf("    { \"name\": ");
            doc_json_str(stdout, rows[i].name);
            printf(", \"ok\": %s, \"required\": %s, \"found\": ",
                   rows[i].ok ? "true" : "false", rows[i].required ? "true" : "false");
            doc_json_str(stdout, rows[i].found ? rows[i].found : "");
            printf(", \"hint\": ");
            doc_json_str(stdout, rows[i].hint ? rows[i].hint : "");
            printf(" }%s\n", i + 1 < nr ? "," : "");
        }
        printf("  ]\n}\n");
    } else {
        printf("gmb doctor — ambiente GMBASIC\n\n");
        for (int i = 0; i < nr; i++) {
            if (rows[i].ok) {
                printf("  [ok] %-22s %s\n", rows[i].name,
                       (rows[i].found && rows[i].found[0]) ? rows[i].found : "ok");
            } else {
                printf("  [--] %-22s MANCA%s%s%s\n", rows[i].name,
                       rows[i].hint ? "  →  " : "", rows[i].hint ? rows[i].hint : "",
                       rows[i].required ? "" : "   (facoltativo)");
            }
        }
        printf("\n  ambiente: %d/%d ok", ok, nr);
        if (reqmiss) printf(" · %d obbligatori mancanti", reqmiss);
        printf("\n");
        if (reqmiss) printf("  per rimediare: gmb doctor --fix   (installa LLMEnv e le librerie scaricabili)\n");
    }
    return reqmiss == 0 ? 0 : 1;
}

int gmb_cli_doctor(int argc, char** argv) {
    int fix = 0, json = 0, abi_only = 0;
    const char* llm_repo = "https://github.com/c-vision/LLMEnv.git";
    for (int i = 2; i < argc; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "--fix")) { fix = 1; continue; }
        if (!strcmp(a, "--json")) { json = 1; continue; }
        if (!strcmp(a, "--abi-dir")) { abi_only = 1; continue; }
        if (!strcmp(a, "--llm-repo") && i + 1 < argc) { llm_repo = argv[++i]; continue; }
        if (!strncmp(a, "--llm-repo=", 11)) { llm_repo = a + 11; continue; }
        fprintf(stderr, "uso: gmb doctor [--fix] [--json] [--abi-dir] [--llm-repo <url>]\n");
        return 2;
    }

    if (abi_only) {   /* modalità STRETTA: risolve e stampa solo la cartella ABI (shell, CI) */
        char real[CLI_PATH];
        (void)gmb_resolve_llm_root(g_doc.llm, sizeof g_doc.llm);
        if (g_doc.llm[0] && realpath(g_doc.llm, real))
            snprintf(g_doc.llm, sizeof g_doc.llm, "%s", real);
        if (!doc_abi_find(g_doc.abi, sizeof g_doc.abi, g_doc.abi_how, sizeof g_doc.abi_how)) return 1;
        printf("%s\n", g_doc.abi);
        return 0;
    }

    doc_probe();
    if (fix) {
        int did = 0;
        /* `LLM_ROOT` può puntare a una cartella inesistente: conta come MANCANTE anche se
         * l'env è impostata, altrimenti --fix dichiarerebbe "niente da fare" su un ambiente
         * che il report stesso segna rosso. */
        if (!(g_doc.llm[0] && is_dir(g_doc.llm))) did += doc_fix_llmenv(llm_repo);
        if (g_doc.n_cache_files == 0) did += doc_fix_libs();
        if (!g_doc.gmbcs[0]) did += doc_fix_gmbcs();
        if (did) doc_probe();
        else fprintf(stderr, "doctor: niente da installare (o non installabile senza toolchain)\n");
    }
    return doc_report(json);
}
