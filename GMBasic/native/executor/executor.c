/* native/executor/executor.c — Native Executor V2 (C): vmc|c99.
 * Esecuzione 100% C: vmc -> gmb_vm_host; c99 -> cc + libgmb_runtime.a.
 * Compilazione .bas->chunk 100% C (lexer/preprocess/macro -> parser -> linker ->
 * compilatore P-Code -> chunk A1/GMBC): nessun ponte C#.
 */
#include "executor.h"
#include "gmb_lexer.h"
#include "gmb_macro_expand.h"
#include "gmb_parser.h"
#include "gmb_compiler.h"
#include "gmb_c99_emit.h"
#include "gmb_runtime.h"     /* gmb_paths_repo_root / gmb_paths_llm_root */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>

static int run_process(const char* cmd)
{
    int rc = system(cmd);
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
}

/* Radice del repo C (= `GMBasic/`). UNA sola risoluzione: `gmb_paths_repo_root` del runtime
 * (cammina dalla dir dell'ESEGUIBILE e dal CWD, riconosce il monorepo e il layout della
 * release: `<cur>/GMBasic`, `<cur>/bin` accanto a `GMBasic/`). Qui non si legge alcuna env:
 * `GMBASIC_REPO_ROOT` era l'ultima env di percorso oltre a GMBASIC_ROOT/LLM_ROOT e le due
 * regole di AGENTS la vietano (parametri solo dal registro, non da getenv). Se il runtime non
 * trova la root, fallback alla CWD (comportamento storico, nessun path inventato). */
static void resolve_repo_root(char* out, size_t cap)
{
    if (gmb_paths_repo_root(out, cap) && out[0]) return;
    char start[2048];
    if (!getcwd(start, sizeof(start))) snprintf(out, cap, ".");
    else snprintf(out, cap, "%s", start);
}

/* Risolve le direttive `IMPORT <Ns>` del modulo principale: per ogni namespace con un file
 * `<Ns>.inc` nella cartella include/ della root (marker include/HttpController.inc, speculare
 * a DiscoverSourceFiles/FindRepoIncludeRoot del C#), prepende ricorsivamente il contenuto del
 * modulo (i suoi IMPORT inclusi) al sorgente main — così le classi/UDT dei moduli sono definite
 * prima dell'uso (il parser C99Native è monofile: NAMESPACE è no-op, IMPORT era ignorato).
 * Namespace builtin senza file (AI/MATH/...) → nessun caricamento. Ritorna buffer malloc o NULL
 * se il main non aveva IMPORT risolvibili (in tal caso il chiamante usa il sorgente originale). */
static char* gmb_resolve_imports(const char* main_src, const char* bas_file)
{
    char root[2048];
    resolve_repo_root(root, sizeof(root));
    const char* include_dir = NULL;
    {
        static char incdir[4096];
        char dir[4096];
        snprintf(dir, sizeof(dir), "%s", root);
        for (;;) {
            snprintf(incdir, sizeof(incdir), "%s/include", dir);
            char marker[4096];
            snprintf(marker, sizeof(marker), "%s/HttpController.inc", incdir);
            if (access(marker, F_OK) == 0) { include_dir = incdir; break; }
            char* slash = strrchr(dir, '/');
            if (!slash || !*dir) break;
            *slash = 0;
        }
    }
    if (!include_dir) return NULL;

    /* Colleziona i namespace IMPORT dal sorgente (righe `IMPORT <Ns>` in testa/ovunque). */
    char nss[32][64];
    int nns = 0;
    {
        const char* s = main_src;
        while (*s) {
            const char* nl = strchr(s, '\n');
            size_t linelen = nl ? (size_t)(nl - s) : strlen(s);
            const char* line = s;
            /* salta spazi iniziali */
            while (linelen > 0 && (*line == ' ' || *line == '\t')) { line++; linelen--; }
            if (linelen >= 6 && strncasecmp(line, "IMPORT", 6) == 0
                && (linelen == 6 || line[6] == ' ' || line[6] == '\t')) {
                const char* p = line + 6;
                while (*p == ' ' || *p == '\t') p++;
                char ns[64]; int i = 0;
                while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < 63) ns[i++] = *p++;
                ns[i] = 0;
                /* salta WITH ALIAS / (selettivo) / .* — prendiamo solo il namespace semplice */
                if (i > 0 && nns < 32) {
                    int dup = 0;
                    for (int k = 0; k < nns; k++) if (strcasecmp(nss[k], ns) == 0) dup = 1;
                    if (!dup) { snprintf(nss[nns], sizeof(nss[nns]), "%s", ns); nns++; }
                }
            }
            if (!nl) break;
            s = nl + 1;
        }
    }
    if (nns == 0) return NULL;

    /* Accumula i sorgenti dei moduli (prima i loro IMPORT, ricorsione a 1 livello dei .inc). */
    char* merged = NULL;
    size_t mcap = 0, mlen = 0;
    for (int idx = 0; idx < nns; idx++) {
        /* Cerca il modulo il cui NAMESPACE dichiarato matcha <Ns> (case-insens), NON il nome
         * file: il file può chiamarsi Agent.inc mentre il namespace è AGENTS (convenzione
         * C#: IMPORT AGENTS → include/Agent.inc con `NAMESPACE AGENTS`). Volume case-sensitive. */
        char incfile[4608];
        incfile[0] = 0;
        {
            DIR* d = opendir(include_dir);
            if (d) {
                struct dirent* e;
                while ((e = readdir(d)) != NULL && !incfile[0]) {
                    const char* nm = e->d_name;
                    size_t nl = strlen(nm);
                    if (nl <= 4 || strcasecmp(nm + nl - 4, ".inc") != 0) continue;
                    char full[4608];
                    snprintf(full, sizeof(full), "%s/%s", include_dir, nm);
                    FILE* tf = fopen(full, "rb");
                    if (!tf) continue;
                    fseek(tf, 0, SEEK_END); long tlen = ftell(tf); fseek(tf, 0, SEEK_SET);
                    char* tsrc = (char*)malloc((size_t)tlen + 1);
                    if (tlen > 0 && fread(tsrc, 1, (size_t)tlen, tf) != (size_t)tlen) { free(tsrc); fclose(tf); continue; }
                    tsrc[tlen] = 0; fclose(tf);
                    /* cerca "NAMESPACE <Ns>" (case-insens) nel contenuto */
                    const char* need = NULL;
                    for (const char* q = tsrc; (q = strcasestr(q, "NAMESPACE ")) != NULL; q += 10) {
                        const char* p = q + 10;
                        while (*p == ' ' || *p == '\t') p++;
                        if (strncasecmp(p, nss[idx], strlen(nss[idx])) == 0) { need = nss[idx]; break; }
                    }
                    free(tsrc);
                    if (need) snprintf(incfile, sizeof(incfile), "%s", full);
                }
                closedir(d);
            }
        }
        if (!incfile[0]) continue;   /* builtin namespace senza modulo */
        FILE* f = fopen(incfile, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long ilen = ftell(f); fseek(f, 0, SEEK_SET);
        char* isrc = (char*)malloc((size_t)ilen + 1);
        if (ilen > 0 && fread(isrc, 1, (size_t)ilen, f) != (size_t)ilen) { free(isrc); fclose(f); continue; }
        isrc[ilen] = 0; fclose(f);
        if (mlen + (size_t)ilen + 2 > mcap) {
            mcap = (mlen + (size_t)ilen + 2) * 2;
            merged = (char*)realloc(merged, mcap);
        }
        memcpy(merged + mlen, isrc, (size_t)ilen);
        mlen += (size_t)ilen;
        merged[mlen++] = '\n';
        free(isrc);
    }
    if (!merged) return NULL;
    if (mlen + strlen(main_src) + 2 > mcap) {
        mcap = mlen + strlen(main_src) + 2;
        merged = (char*)realloc(merged, mcap);
    }
    memcpy(merged + mlen, main_src, strlen(main_src) + 1);
    (void)bas_file;
    return merged;
}

/* `gmb_vm_host` per POSIZIONE, mai per env: `<root>/bin` (release, accanto al resto dei
 * binari) e `<root>/native/gmb_vm_c/build` (checkout di sviluppo). `root` viene dalla stessa
 * `gmb_paths_repo_root` di tutto il resto, quindi un `gmb`/`gmbrepl` invocato da qualunque
 * cartella trova l'host. NULL se non c'è: il chiamante diagnostica. */
static const char* resolve_vm_host(void)
{
    static char path[2048];
    char root[2048];
    resolve_repo_root(root, sizeof(root));
    static const char* rel[] = { "bin/gmb_vm_host", "native/gmb_vm_c/build/gmb_vm_host" };
    for (size_t i = 0; i < sizeof rel / sizeof rel[0]; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", root, rel[i]);
        if (access(path, X_OK) == 0) return path;
    }
    return NULL;
}

/* Versione PUBBLICA del resolver (decl. in executor.h). Il REPL (repl/vmc_runner.c) aveva una
 * copia `extern` di `gmb_resolve_vm_host` MAI definita (introdotta in 461c92a52) -> gmbrepl
 * non linkava da allora. `resolve_vm_host` era static e serviva solo a gmb_native_exec:
 * ora e' esposta, cosi' esiste UNA sola risoluzione (root del runtime -> bin/ o build/).
 * Ritorna NULL se non trovato. */
const char* gmb_resolve_vm_host(void) { return resolve_vm_host(); }


/* Aggregazione progetto (IMPORT multi-file, mondo C): IMPORT <Ns> può risiedere in
 * include/<Ns>.inc (repo) OPPURE in file compagna della stessa cartella del progetto
 * (es. MyLibrary.inc con NAMESPACE Geometria). Il compilatore managed scansiona la
 * directory: qui prependiamo (in ordine) i contenuti di include/<Ns> risolti e di
 * tutti i file *.inc / *.bas fratelli (escluso il main) prima del sorgente main. */
static int gmb_import_needs_local_files(const char* main_src)
{
    static const char* builtin_ns[] = {
        /* Namespace builtin canonici (specchio gmb_ns_names / IMPORT di un namespace del
         * linguaggio, MAI una libreria di progetto): IMPORT di questi NON deve aggregare i
         * file fratelli della cartella (gmb_aggregate_project) — un IMPORT di un builtin
         * mancante da questa lista (es. OPTIMIZE prima del fix) faceva prependere TUTTI i
         * .bas/.inc compagna, eseguendo codice estraneo (artefatti tipo "eventi registrati"). */
        "JSON","FINANCE","ARRAYS","COLLECTIONS","GRAPHICS","MATH","MATRIX","VECTOR",
        "NET","AI","DBF","ISAM","SQL","RDD","LEDGER","HTTP","CHART","TEAM","IO",
        "SYSTEM","TEXT","CONVERT","AUDIO","TORCH","CHANNELS","TOOLS",
        "ANALYSIS","ATOMIC","AUTH","BLOCKCHAIN","CLIP","COMPRESSION","CONCURRENCY",
        "CONSOLE","CONSTANTS","CONTEXT","CONVERSIONS","CRYPTOGRAPHY","DATETIME","JOY",
        "KDTREE","MEMORY","MEMORYMAP","MOUSE","MUTEX","ONKEY","OPTIMIZE","POOL",
        "PROVIDER","SIGNAL","SPATIAL","SPECIAL","SPEED","STATS","STRINGS","SYNCMAP",
        "TIMESERIES","TRADING","TRADING.BACKTESTS","TRADING.BONDS","TRADING.METRICS",
        "TRADING.OPTIONS","TRADING.PORTFOLIO","TYPEINFO","VECTORDB","WINDOWS",
        "GRAPH","BRAINS","VAULT","NOTE","INGEST","AGENT","BROWSER","COMPUTER","TOOL","UI", NULL
    };
    const char* s = main_src;
    while (*s)
    {
        const char* nl = strchr(s, '\n');
        size_t linelen = nl ? (size_t)(nl - s) : strlen(s);
        const char* line = s;
        while (linelen > 0 && (*line == ' ' || *line == '\t')) { line++; linelen--; }
        if (linelen >= 6 && strncasecmp(line, "IMPORT", 6) == 0
            && (linelen == 6 || line[6] == ' ' || line[6] == '\t'))
        {
            const char* p = line + 6;
            while (*p == ' ' || *p == '\t') p++;
            char ns[64]; int i = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < 63) ns[i++] = *p++;
            ns[i] = 0;
            int builtin = 0;
            for (int k = 0; builtin_ns[k]; k++)
                if (strcasecmp(ns, builtin_ns[k]) == 0) { builtin = 1; break; }
            if (!builtin && i > 0) return 1;   /* namespace locale/esterno → serve i file compagna */
        }
        if (!nl) break;
        s = nl + 1;
    }
    return 0;
}

static char* gmb_aggregate_project(const char* main_src, const char* bas_file)
{
    if (!gmb_import_needs_local_files(main_src)) return NULL;
    char* out = NULL; size_t cap = 0, len = 0;
    char* resolved = gmb_resolve_imports(main_src, bas_file);
    if (resolved) { out = strdup(resolved); len = strlen(out); cap = len + 1; }
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s", bas_file);
    char* slash = strrchr(dir, '/');
    if (!slash) snprintf(dir, sizeof(dir), "."); else *slash = 0;
    const char* mainbase = strrchr(bas_file, '/');
    mainbase = mainbase ? mainbase + 1 : bas_file;
    DIR* d = opendir(dir);
    if (d)
    {
        /* raccogli i nomi e ordinali per robustezza di ordinamento */
        struct dirent* e;
        char names[64][512]; int n = 0;
        while ((e = readdir(d)) != NULL && n < 64)
        {
            const char* nm = e->d_name;
            size_t el = strlen(nm);
            const char* ext = el > 4 ? nm + el - 4 : "";
            if (strcmp(nm, mainbase) == 0) continue;
            if ((!strcasecmp(ext, ".inc")) || (!strcasecmp(ext, ".bas")))
            {
                snprintf(names[n], sizeof(names[n]), "%s", nm);
                /* insertion sort per nome (case-insensitive) */
                int j = n;
                while (j > 0 && strcasecmp(names[j - 1], names[j]) > 0)
                {
                    char tmp[512]; snprintf(tmp, sizeof(tmp), "%s", names[j - 1]);
                    snprintf(names[j - 1], sizeof(names[j - 1]), "%s", names[j]);
                    snprintf(names[j], sizeof(names[j]), "%s", tmp);
                    j--;
                }
                n++;
            }
        }
        closedir(d);
        for (int i = 0; i < n; i++)
        {
            char path[4700];
            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
            FILE* f = fopen(path, "rb");
            if (!f) continue;
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            if (sz > 0)
            {
                char* buf = (char*)malloc((size_t)sz + 1);
                if (buf && fread(buf, 1, (size_t)sz, f) == (size_t)sz)
                {
                    buf[sz] = 0;
                    size_t nl = strlen(buf);
                    if (len + nl + 2 > cap) { cap = (len + nl + 2) * 2; out = (char*)realloc(out, cap); }
                    if (!out) out = (char*)malloc(cap);
                    memcpy(out + len, buf, nl); len += nl;
                    out[len++] = '\n'; out[len] = 0;
                }
                free(buf);
            }
            fclose(f);
        }
    }
    if (!out) return NULL;
    /* appendi il main */
    size_t ml = strlen(main_src);
    if (len + ml + 1 > cap) { cap = (len + ml + 1) * 2; out = (char*)realloc(out, cap); }
    memcpy(out + len, main_src, ml); len += ml; out[len] = 0;
    if (resolved) free(resolved);
    return out;
}

/* Compila .bas -> chunk A1/GMBC con il pipeline 100% C (specchio della REPL nativa
 * AST: lexer -> macro expand -> parser -> linker -> compilatore P-Code). */
static int gmb_compile_c_chunk(const char* bas_file, const char* out_path)
{
    FILE* f = fopen(bas_file, "rb");
    if (!f) { fprintf(stderr, "?file non trovato: %s\n", bas_file); return 2; }
    fseek(f, 0, SEEK_END); long alen = ftell(f); fseek(f, 0, SEEK_SET);
    char* src = (char*)malloc((size_t)alen + 1);
    if (alen > 0 && fread(src, 1, (size_t)alen, f) != (size_t)alen) { fclose(f); free(src); return 2; }
    src[alen] = 0; fclose(f);

    char* merged2 = gmb_aggregate_project(src, bas_file);
    const char* msrc = merged2 ? merged2 : src;
    long mlen2 = (long)strlen(msrc);
    gmb_lex_result_t lx = gmb_lexer_scan(msrc, mlen2);
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    lx.owned_src = NULL;
    char* perr = NULL;
    gmb_program_t prog = gmb_parse(&lx2, &perr);
    if (perr) { fprintf(stderr, "?compilazione C: parse error: %s\n", perr); free(perr); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(merged2); free(src); return 3; }

    uint8_t* data = NULL; size_t len = 0; char* cerr = NULL;
    int rc = gmb_compile(&prog, &data, &len, &cerr);
    if (rc != 0) { fprintf(stderr, "?compilazione C: %s\n", cerr ? cerr : "errore"); free(cerr); gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(merged2); free(src); return 4; }

    FILE* of = fopen(out_path, "wb");
    if (!of) { fprintf(stderr, "?impossibile scrivere: %s\n", out_path); free(data); gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(merged2); free(src); return 5; }
    fwrite(data, 1, len, of);
    fclose(of);
    free(data);
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx2);
    gmb_lex_result_free(&lx);
    free(merged2);
    free(src);
    return 0;
}

/* Genera il sorgente C99 da un .bas con il pipeline 100% C + emitter nativo. */
static int gmb_emit_c99_source(const char* bas_file, const char* out_path)
{
    FILE* f = fopen(bas_file, "rb");
    if (!f) { fprintf(stderr, "?file non trovato: %s\n", bas_file); return 2; }
    fseek(f, 0, SEEK_END); long alen = ftell(f); fseek(f, 0, SEEK_SET);
    char* src = (char*)malloc((size_t)alen + 1);
    if (alen > 0 && fread(src, 1, (size_t)alen, f) != (size_t)alen) { fclose(f); free(src); return 2; }
    src[alen] = 0; fclose(f);

    /* IMPORT <Ns>: prepende il contenuto di include/<Ns>.inc (moduli riusabili), così le
     * classi/UDT dichiarate nei .inc sono definite prima dell'uso. IMPORT era ignorato. */
    char* resolved = gmb_resolve_imports(src, bas_file);
    char* merged = resolved ? resolved : src;
    long mlen = (long)strlen(merged);

    gmb_lex_result_t lx = gmb_lexer_scan(merged, mlen);
    gmb_lex_result_t lx2 = gmb_macro_expand(&lx);
    lx.owned_src = NULL;
    char* perr = NULL;
    gmb_program_t prog = gmb_parse(&lx2, &perr);
    if (perr) { fprintf(stderr, "?c99: parse error: %s\n", perr); free(perr); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(resolved); free(src); return 3; }

    char* csrc = NULL; char* eerr = NULL;
    int rc = gmb_c99_emit(&prog, &csrc, &eerr);
    if (rc != 0) { fprintf(stderr, "?c99: emissione: %s\n", eerr ? eerr : "errore"); free(eerr); gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(resolved); free(src); return 4; }

    FILE* of = fopen(out_path, "wb");
    if (!of) { fprintf(stderr, "?impossibile scrivere: %s\n", out_path); free(csrc); gmb_program_free(&prog); gmb_lex_result_free(&lx2); gmb_lex_result_free(&lx); free(resolved); free(src); return 5; }
    fwrite(csrc, 1, strlen(csrc), of);
    fclose(of);
    free(csrc);
    gmb_program_free(&prog);
    gmb_lex_result_free(&lx2);
    gmb_lex_result_free(&lx);
    free(resolved);
    free(src);
    return 0;
}

int gmb_native_compile(const char* bas_file, const char* out_path, gmb_exec_backend_t backend)
{
    if (backend == GMB_EXEC_VMC)
        return gmb_compile_c_chunk(bas_file, out_path);
    return gmb_emit_c99_source(bas_file, out_path);
}

/* Aggiunge `-I"dir"` / `-L"dir"` solo se il percorso esiste: una flag su un path assente
 * non serve e un errore di battitura silenzioso ancora meno. */
static void c99_add_flag(char* out, size_t cap, const char* flag, const char* dir)
{
    size_t used = strlen(out);
    if (access(dir, F_OK) != 0) return;
    if (used + strlen(flag) + strlen(dir) + 4 > cap) return;
    snprintf(out + used, cap - used, " %s\"%s\"", flag, dir);
}

int gmb_native_exec(const char* bas_file, gmb_exec_backend_t backend)
{
    if (backend == GMB_EXEC_VMC) {
        char chunk[2048];
        snprintf(chunk, sizeof(chunk), "/tmp/gmb_exec_%ld.chunk", (long)getpid());
        if (gmb_native_compile(bas_file, chunk, GMB_EXEC_VMC) != 0) return 2;
        const char* host = resolve_vm_host();
        if (!host) { fprintf(stderr, "?gmb_vm_host non trovato (atteso in <root>/bin o <root>/native/gmb_vm_c/build; `gmb doctor`)\n"); return -1; }
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\"", host, chunk);
        int rc = run_process(cmd);
        unlink(chunk);
        return rc;
    }
    /* c99: il C emesso + la SOLA libreria del runtime. Nient'altro.
     * `libgmb_runtime.a` è un ARCHIVIO STATICO: entra nell'eseguibile solo ciò che il
     * programma referenzia, quindi `PRINT 1+1` non trascina chart/doc/agent/AI né SDL2,
     * crypto, curl, brotli. Prima questo ramo ricompilava TUTTE le
     * `native/gmb_runtime_c/src/*.c` (più le ASR di LLMEnv) e linkava a forza 12 librerie:
     * un programma di due righe usciva dipendente da tutto e non partiva senza LLMEnv. */
    char cfile[2048], bindir[2048];
    snprintf(cfile, sizeof(cfile), "/tmp/gmb_exec_%ld.c", (long)getpid());
    snprintf(bindir, sizeof(bindir), "/tmp/gmb_exec_%ld", (long)getpid());
    mkdir(bindir, 0755);
    if (gmb_native_compile(bas_file, cfile, GMB_EXEC_C99) != 0) return 2;
    char root[2048];
    resolve_repo_root(root, sizeof(root));
    /* Dove sta l'archivio: build del runtime nel checkout, `lib/` accanto ai binari nella
     * release. Cercato per posizione, come `gmb_vm_host`: nessuna env. */
    char libdir[2048];
    libdir[0] = 0;
    {
        static const char* rel[] = { "native/gmb_runtime_c/build", "lib", "native/gmb_runtime_c" };
        for (size_t i = 0; i < sizeof rel / sizeof rel[0]; i++)
        {
            char p[2048];
            snprintf(p, sizeof p, "%s/%s/libgmb_runtime.a", root, rel[i]);
            if (access(p, F_OK) == 0) { snprintf(libdir, sizeof libdir, "%s/%s", root, rel[i]); break; }
        }
    }
    if (!libdir[0])
    {
        fprintf(stderr, "?libgmb_runtime.a non trovata in <root>/native/gmb_runtime_c/build: "
                        "compila il runtime (`gmb doctor`)\n");
        unlink(cfile);
        return 2;
    }
    char cmd[8192];
    /* -I: le SOLE directory dei 46 header che il prologo del C emesso include davvero
     * (contate con l'emitter, non copiate dalla CLI): runtime 37, agent 4, torch 2,
     * db/doc/gmbllm 1 ciascuno. Niente LLMEnv, niente header ASR/MLX, niente cache: qui si
     * compila UN file generato, il resto non serve — e una lista più lunga sarebbe una
     * seconda ricetta che diverge da `gmb_cli_inc_dirs`. */
    char inc[1200] = {0};
    {
        static const char* rel[] = {
            "native/gmb_runtime_c/include", "native/gmb_agent_runtime",
            "native/gmb_torch/include", "native/gmb_db_c/include",
            "native/gmb_doc_c/include", "gmbllm/include",
        };
        char p[2048];
        for (size_t i = 0; i < sizeof rel / sizeof rel[0]; i++)
        {
            snprintf(p, sizeof p, "%s/%s", root, rel[i]);
            c99_add_flag(inc, sizeof inc, "-I", p);
        }
    }
    /* Una sola directory di LLMEnv: `gmbllm/include/llmmodelio_model.h` (nel prologo)
     * include `llmengine_plugin.h` che sta in `LLMEngine/plugin` — misurato con `cc -M`
     * sulla chiusura transitiva, non copiato dalla lista della CLI. Root da
     * `gmb_paths_llm_root` (mai un path cablato, mai una env). */
    {
        char llmenv[2048];
        if (gmb_paths_llm_root(llmenv, sizeof llmenv) && llmenv[0])
        {
            char p[2048];
            snprintf(p, sizeof p, "%s/LLMEngine/plugin", llmenv);
            c99_add_flag(inc, sizeof inc, "-I", p);
        }
        else
            fprintf(stderr, "?LLMEnv non trovata (header `llmengine_plugin.h` del C emesso): "
                            "`gmb doctor`\n");
    }
    /* -L: archivio del runtime + il chart (build annidata accanto, se c'è) + la cache del RID
     * per le terze parti. Gli archivi sono STATICI: `-lgmb_chart` non porta dentro nulla se il
     * programma non lo chiama. */
    char libs[1600] = {0};
    c99_add_flag(libs, sizeof libs, "-L", libdir);
    {
        char chart[2048];
        snprintf(chart, sizeof chart, "%s/native/gmb_runtime_c/build/gmb_chart_build", root);
        if (access(chart, F_OK) == 0 && strstr(libs, chart) == NULL)
        {
            c99_add_flag(libs, sizeof libs, "-L", chart);
            if (strlen(libs) + 16 < sizeof libs) strcat(libs, " -lgmb_chart");
        }
        char cache[1024], cache_rid[1200];
        cache[0] = 0;
        gmb_paths_libs_cache_dir(cache, sizeof cache);
        if (cache[0])
        {
            snprintf(cache_rid, sizeof cache_rid, "%s/%s", cache, gmb_libs_rid());
            c99_add_flag(libs, sizeof libs, "-L", cache_rid);
            /* rpath per l'eseguibile TEMPORANEO in /tmp: senza, dyld non trova
             * `@rpath/libSDL2.dylib`. Non finisce in nessun binario di release. */
            c99_add_flag(libs, sizeof libs, "-Wl,-rpath,", cache_rid);
        }
    }
    /* Librerie esterne effettivamente referenziate dai membri del runtime trascinati dentro
     * (`nm`/link: SDL2, SDL2_ttf, zlib — niente crypto/curl/brotli/sqlite/pcre2 per un
     * programma che non li usa). `-lz` è di sistema, le SDL vengono dalla cache del RID. */
    snprintf(cmd, sizeof(cmd),
        "cc \"%s\"%s%s -lgmb_runtime -lz -lSDL2 -lSDL2_ttf -lm -o \"%s/prog\"",
        cfile, inc, libs, bindir);
    int rc = run_process(cmd);
    if (rc != 0) { unlink(cfile); return 3; }
    snprintf(cmd, sizeof(cmd), "\"%s/prog\"", bindir);
    rc = run_process(cmd);
    unlink(cfile);
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", bindir);
    run_process(cmd);
    return rc;
}
