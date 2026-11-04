/* gmb_inc_resolve.c — vedi gmb_inc_resolve.h.
 *
 * PARITÀ CON `Executor/Orchestrator.cs` (PreProcessProject + DiscoverSourceFiles):
 *
 *  1. candidati = scansione RICORSIVA di `*.bas`/`*.inc` nella radice del programma;
 *  2. + `importFolders` e `importFiles` dei settings (`system.settings.json` +
 *     `app.settings.json`), risolti sia rispetto al progetto sia rispetto alla root
 *     dell'install — come fa il C#;
 *  3. + le cartelle di ricerca del COMPILATORE (`compiler.searchFolders`), che sostituiscono
 *     il vecchio elenco cablato include/ · Controls/ · <install>/include · <install>/Controls ·
 *     <install>/libs/<nome>: sono ancora i default, ma ora configurabili nei settings;
 *  4. i file di progetto/import "shadowano" per NOME quelli delle cartelle globali;
 *  5. il testo concatenato segue gli `IMPORT` di OGNI file caricato (DFS, dipendenze prima
 *     del dipendente), non solo quelli della entry.
 *
 * O8 — mappa diagnostica C→BASIC: la risoluzione restituisce anche i SEGMENTI del testo
 * concatenato, così una riga del C emesso (e quindi un errore di `cc`, o di emissione)
 * può essere attribuita al `.bas`/`.inc` originale invece che alla riga del file unito.
 */
#include "gmb_inc_resolve.h"
#include "gmb_ns.h"   /* gmb_ns_names / GMB_NS_COUNT: namespace builtin (IMPORT non è un errore) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#define IR_PATH 4200
#define IR_MAX_FILES 2048
#define IR_MAX_DEPTH 24
#define IR_MAX_NS_PER_FILE 8
#define IR_MAX_IMPORTS 96

/* ── liste di path ──────────────────────────────────────────────────────────────────────── */

typedef struct {
    char** items;
    int n, cap;
} PathList;

static void pl_free(PathList* l) {
    if (!l) return;
    for (int i = 0; i < l->n; i++) free(l->items[i]);
    free(l->items);
    l->items = NULL; l->n = l->cap = 0;
}

static int pl_has(const PathList* l, const char* s) {
    if (!l || !s) return 0;
    for (int i = 0; i < l->n; i++) if (strcmp(l->items[i], s) == 0) return 1;
    return 0;
}

static int pl_add(PathList* l, const char* s) {
    if (!l || !s || !*s) return 0;
    if (pl_has(l, s)) return 0;
    if (l->n == l->cap) {
        int cap = l->cap ? l->cap * 2 : 16;
        char** p = (char**)realloc(l->items, (size_t)cap * sizeof *p);
        if (!p) return 0;
        l->items = p; l->cap = cap;
    }
    l->items[l->n] = strdup(s);
    if (!l->items[l->n]) return 0;
    l->n++;
    return 1;
}

static char* read_all(const char* path, long* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char* b = (char*)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    if (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0; fclose(f); if (out_len) *out_len = n;
    return b;
}

static int count_nl(const char* s) {
    int n = 0;
    for (; s && *s; s++) if (*s == '\n') n++;
    return n;
}

/* ── filesystem ─────────────────────────────────────────────────────────────────────────── */

static int is_dir(const char* p) {
    struct stat st;
    return p && *p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_file(const char* p) {
    struct stat st;
    return p && *p && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* Path assoluto canonicale: due path che puntano allo stesso file devono confrontarsi
 * uguali (un file raggiunto via `include/` e via `<root>/include/` non va incluso due
 * volte, altrimenti i SUB del namespace risultano definiti due volte). */
static void canon_path(const char* p, char* out, size_t cap) {
#if defined(_WIN32) || defined(_WIN64)
    char buf[IR_PATH];
    if (_fullpath(buf, p, sizeof buf)) snprintf(out, cap, "%s", buf);
    else snprintf(out, cap, "%s", p);
#else
    (void)cap;
    char rp[IR_PATH];
    if (realpath(p, rp)) snprintf(out, cap, "%s", rp);
    else snprintf(out, cap, "%s", p);
#endif
}

static void dir_of(const char* path, char* out, size_t cap) {
    char buf[IR_PATH]; snprintf(buf, sizeof buf, "%s", path);
    char* slash = strrchr(buf, '/');
    if (slash) *slash = 0; else buf[0] = 0;
    snprintf(out, cap, "%s", buf);
}

static int find_subdir_up(const char* input_path, const char* name, char* out, size_t cap) {
    char buf[IR_PATH]; snprintf(buf, sizeof buf, "%s", input_path);
    char* slash = strrchr(buf, '/');
    if (slash) *slash = 0; else buf[0] = 0;
    for (int up = 0; up < 16; up++) {
        char cand[IR_PATH];
        if (buf[0]) snprintf(cand, sizeof cand, "%s/%s", buf, name);
        else snprintf(cand, sizeof cand, "%s", name);
        if (is_dir(cand)) { snprintf(out, cap, "%s", cand); return 1; }
        char* s2 = strrchr(buf, '/');
        if (s2) *s2 = 0;
        else if (buf[0]) buf[0] = 0;
        else break;
    }
    return 0;
}

/* Directory dell'eseguibile corrente (senza filename). */
static int ir_exe_dir(char* out, size_t cap) {
#if defined(_WIN32) || defined(_WIN64)
    char buf[4096];
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) return 0;
    snprintf(out, cap, "%s", buf);
#elif defined(__APPLE__)
    char buf[4096]; uint32_t sz = (uint32_t)sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) != 0) return 0;
    snprintf(out, cap, "%s", buf);
#else
    char buf[4096]; ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return 0;
    buf[n] = 0; snprintf(out, cap, "%s", buf);
#endif
    char* s = strrchr(out, '/');
    if (s) *s = 0;
    return 1;
}

/* Directory dell'ESEGUIBILE corrente (senza filename). Esposta perché è l'ancora
 * dell'installazione LOCALE dei pacchetti (`<exe>/packages`, modello node_modules):
 * la usa il compilatore (REQUIRE) per cercare i pacchetti dell'app. */
int gmb_exe_dir(char* out, size_t cap) { return ir_exe_dir(out, cap); }

static int ir_walk_sln(const char* seed, char* out, size_t cap) {
    char cur[4096]; snprintf(cur, sizeof cur, "%s", seed);
    for (;;) {
        char probe[IR_PATH]; snprintf(probe, sizeof probe, "%s/GMBASIC.sln", cur);
        if (is_file(probe)) { snprintf(out, cap, "%s", cur); return 1; }
        /* Repo C-only (monorepo): nessuna .sln, marker stabile del runtime condiviso. */
        snprintf(probe, sizeof probe, "%s/native/gmb_runtime_c/include/gmb_runtime.h", cur);
        if (is_file(probe)) { snprintf(out, cap, "%s", cur); return 1; }
        /* Monorepo con `bin/` alla radice: la root dell'install è `<cur>/GMBasic`, non `<cur>`.
         * Senza, un `gmb` invocato da `<monorepo>/bin` (o dal PATH) non trovava né gli header
         * né le statiche del runtime lì accanto: `cc` senza -I e "libgmb_runtime.a non
         * trovata" da qualunque cartella. */
        snprintf(probe, sizeof probe, "%s/GMBasic/native/gmb_runtime_c/include/gmb_runtime.h", cur);
        if (is_file(probe)) { snprintf(out, cap, "%s/GMBasic", cur); return 1; }
        char* s = strrchr(cur, '/');
        if (!s || s == cur) break;
        *s = 0;
    }
    return 0;
}

/* Root dell'install GMBasic: env GMBASIC_ROOT > GMBASIC.sln dall'eseguibile > dal CWD.
 * Così anche un programma creato OVUNQUE risolve <root>/include e <root>/libs. */
static int ir_root(char* out, size_t cap) {
    const char* e = getenv("GMBASIC_ROOT");
    if (e && *e) { snprintf(out, cap, "%s", e); return 1; }
    char exe[4096];
    if (ir_exe_dir(exe, sizeof exe) && ir_walk_sln(exe, out, cap)) return 1;
    char cwd[4096];
    if (getcwd(cwd, sizeof cwd) && ir_walk_sln(cwd, out, cap)) return 1;
    out[0] = 0; return 0;
}

/* LLMEnv è un REPO SEPARATO (motori AI, Apache-2.0), non una sottocartella del monorepo.
 * `build_inc` gli passava due -I hardcoded (`../LLMEnv`, `../../LLMEnv`): su un client che la
 * installa altrove — o che la riceve accanto al bundle di release — gli header AI non si
 * trovavano e ogni programma `IMPORT AI` falliva in `cc`. Qui la root si RISOLVE, una volta. */
static int ir_walk_llm(const char* seed, char* out, size_t cap) {
    char cur[4096]; snprintf(cur, sizeof cur, "%s", seed);
    for (;;) {
        char probe[IR_PATH];
        /* seed È già LLMEnv (`<cur>/LLMCodec/include`). */
        /* ATTENZIONE: il marker è una CARTELLA (`LLMCodec/include`): con is_file() non
         * veniva mai riconosciuto e `gmb build` perdeva TUTTI gli -I di LLMEnv (regressione
         * vista subito: `llmengine_plugin.h` non trovato). */
        snprintf(probe, sizeof probe, "%s/LLMCodec/include", cur);
        if (is_dir(probe)) { snprintf(out, cap, "%s", cur); return 1; }
        /* LLMEnv è una sottocartella del seed (monorepo, release con LLMEnv accanto a bin/). */
        snprintf(probe, sizeof probe, "%s/LLMEnv/LLMCodec/include", cur);
        if (is_dir(probe)) { snprintf(out, cap, "%s/LLMEnv", cur); return 1; }
        char* s = strrchr(cur, '/');
        if (!s || s == cur) break;
        *s = 0;
    }
    return 0;
}

/* Root di LLMEnv: env LLM_ROOT > accanto alla root GMBasic (`../LLMEnv`, `../../LLMEnv`) >
 * risalendo dalla dir dell'eseguibile > dal CWD. Marker: `LLMCodec/include`, la cartella che
 * `cc` riceve come -I. 0 se non c'è: il chiamante decide (diagnostica, non path inventato). */
int gmb_resolve_llm_root(char* out, size_t cap) {
    if (!out || cap == 0) return 0;
    out[0] = 0;
    const char* e = getenv("LLM_ROOT");
    if (e && *e) { snprintf(out, cap, "%s", e); return 1; }
    char inst[4096];
    if (ir_root(inst, sizeof inst) && inst[0]) {
        static const char* rel[] = { "../LLMEnv", "../../LLMEnv" };
        for (size_t i = 0; i < sizeof rel / sizeof rel[0]; i++) {
            char p[IR_PATH], probe[IR_PATH];
            snprintf(p, sizeof p, "%s/%s", inst, rel[i]);
            snprintf(probe, sizeof probe, "%s/LLMCodec/include", p);
            if (is_dir(probe)) { snprintf(out, cap, "%s", p); return 1; }
        }
    }
    char exe[4096];
    if (ir_exe_dir(exe, sizeof exe) && ir_walk_llm(exe, out, cap)) return 1;
    char cwd[4096];
    if (getcwd(cwd, sizeof cwd) && ir_walk_llm(cwd, out, cap)) return 1;
    return 0;
}

/* ── settings (system.settings.json + app.settings.json) ───────────────────────────────────
 * Il driver nativo non ha System.Text.Json: serve solo leggere tre chiavi. Il lettore
 * accetta i commenti di riga (`//`) e di blocco, che i settings del repo usano, e le
 * stringhe con escape. Niente dipendenze. */
static const char* json_skip_ws(const char* p) {
    for (;;) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }
        return p;
    }
}

static const char* json_skip_string(const char* p) {
    if (*p != '"') return p;
    p++;
    while (*p) {
        if (*p == '\\' && p[1]) { p += 2; continue; }
        if (*p == '"') return p + 1;
        p++;
    }
    return p;
}

/* Puntatore al PRIMO carattere del valore di `"key"`, fuori dalle stringhe. NULL se la
 * chiave non c'è. Le chiavi cercate (importFolders/importFiles/searchFolders) sono uniche
 * nei due file di settings. */
static const char* json_value_for(const char* json, const char* key) {
    if (!json || !key) return NULL;
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    size_t pl = strlen(pat);
    for (const char* p = json; *p; ) {
        /* La chiave è una stringa: va riconosciuta PRIMA di saltare le stringhe. */
        if (strncmp(p, pat, pl) == 0) {
            const char* q = json_skip_ws(p + pl);
            if (*q == ':') return json_skip_ws(q + 1);
            p += pl;
            continue;
        }
        if (*p == '"') { p = json_skip_string(p); continue; }
        p++;
    }
    return NULL;
}

static char* json_unescape(const char* s, size_t len) {
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    size_t w = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\' && i + 1 < len) {
            char c = s[++i];
            switch (c) {
            case 'n': out[w++] = '\n'; break;
            case 't': out[w++] = '\t'; break;
            case 'r': out[w++] = '\r'; break;
            case 'u':
                /* I path non usano \uXXXX: si salta la sequenza (4 cifre). */
                for (int k = 0; k < 4 && i + 1 < len; k++) i++;
                break;
            default: out[w++] = c; break;
            }
            continue;
        }
        out[w++] = s[i];
    }
    out[w] = 0;
    return out;
}

/* Aggiunge le stringhe di un array JSON `["a", "b"]` a `out`. */
static void json_string_list(const char* p, PathList* out) {
    p = json_skip_ws(p);
    if (*p != '[') return;
    p++;
    for (;;) {
        p = json_skip_ws(p);
        if (!*p || *p == ']') break;
        if (*p == '"') {
            const char* e = json_skip_string(p);
            char* s = json_unescape(p + 1, (size_t)(e - p - 2));
            if (s) { pl_add(out, s); free(s); }
            p = e;
        } else {
            /* valore non stringa: si salta al prossimo separatore */
            while (*p && *p != ',' && *p != ']') p++;
        }
        p = json_skip_ws(p);
        if (*p == ',') p++;
    }
}

typedef struct {
    PathList import_folders;
    PathList import_files;
    PathList search_folders;
} ResolveSettings;

static void settings_free(ResolveSettings* s) {
    if (!s) return;
    pl_free(&s->import_folders);
    pl_free(&s->import_files);
    pl_free(&s->search_folders);
}

/* Cartelle di ricerca di DEFAULT (identiche al vecchio elenco cablato): include/ e
 * Controls/ accanto al programma o risalendo, poi include/Controls/libs dell'install.
 *
 * MONOREPO (2026-10-29): gli asset condivisi sono in <monorepo>/shared e `install`
 * (ir_root) è <monorepo>/GMBasic, quindi le voci d'install sono `{install}/../shared/...`.
 * GMBasic/Controls è stato consolidato in shared/Controls: senza questo, un programma che
 * non sta sotto shared/ non trova la libreria CustomControls. Stessa lista in
 * shared/settings/system.settings.json e in CompilerSettings.SearchFolders (C#). */
static const char* kDefaultSearch[] = {
    "include",
    "Controls",
    "controls",
    "{install}/../shared/include",
    "{install}/../shared/Controls",
    /* Le NOSTRE librerie interne (media, net: amqp/grpc/kafka/mqtt/nats/protobuf) vivono in
     * `shared/libs/<nome>/` — condivise dai due compilatori. `<install>/libs/*` resta per un
     * checkout del solo GMBasic. */
    "{install}/../shared/libs/*",
    "{install}/libs/*",
};

static void add_json_list(const char* json, const char* key, PathList* out) {
    if (!json) return;
    const char* v = json_value_for(json, key);
    if (v) json_string_list(v, out);
}

/* Carica i settings che governano la RISOLUZIONE. Ordine e semantica di
 * SettingsManager.LoadSettings: l'unione per importFolders (system + app), app vince
 * sugli importFiles, `compiler.searchFolders` dell'app SOSTITUISCE quello di sistema. */
static void settings_load(const char* project_dir, ResolveSettings* s) {
    memset(s, 0, sizeof *s);
    char proj[IR_PATH];
    if (project_dir && *project_dir) canon_path(project_dir, proj, sizeof proj);
    else proj[0] = 0;
    char install[4096]; install[0] = 0;
    (void)ir_root(install, sizeof install);

    PathList sys_search; memset(&sys_search, 0, sizeof sys_search);
    int have_sys_search = 0;

    /* system.settings.json: config/ (copia locale editabile) > Resources/ > accanto all'exe. */
    char syspath[IR_PATH] = "";
    if (install[0]) {
        char cand[IR_PATH];
        snprintf(cand, sizeof cand, "%s/config/system.settings.json", install);
        if (is_file(cand)) snprintf(syspath, sizeof syspath, "%s", cand);
        if (!syspath[0]) {
            snprintf(cand, sizeof cand, "%s/Resources/system.settings.json", install);
            if (is_file(cand)) snprintf(syspath, sizeof syspath, "%s", cand);
        }
    }
    if (!syspath[0]) {
        char exe[4096];
        if (ir_exe_dir(exe, sizeof exe)) {
            char cand[IR_PATH]; snprintf(cand, sizeof cand, "%s/system.settings.json", exe);
            if (is_file(cand)) snprintf(syspath, sizeof syspath, "%s", cand);
        }
    }
    char* sysjson = syspath[0] ? read_all(syspath, NULL) : NULL;
    if (sysjson) {
        add_json_list(sysjson, "importFolders", &s->import_folders);
        add_json_list(sysjson, "importFiles", &s->import_files);
        add_json_list(sysjson, "searchFolders", &sys_search);
        /* La CHIAVE presente vale anche se la lista è vuota: `[]` disattiva, come in
         * CompilerSettings.Resolve del C# (assente = conserva/default). */
        have_sys_search = json_value_for(sysjson, "searchFolders") != NULL;
        free(sysjson);
    }

    /* app.settings.json: accanto al programma. */
    char apppath[IR_PATH];
    if (proj[0]) snprintf(apppath, sizeof apppath, "%s/app.settings.json", proj);
    else snprintf(apppath, sizeof apppath, "app.settings.json");
    char* appjson = is_file(apppath) ? read_all(apppath, NULL) : NULL;
    if (appjson) {
        add_json_list(appjson, "importFolders", &s->import_folders);
        add_json_list(appjson, "importFiles", &s->import_files);
    }

    PathList app_search; memset(&app_search, 0, sizeof app_search);
    int have_app_search = 0;
    if (appjson) {
        add_json_list(appjson, "searchFolders", &app_search);
        have_app_search = json_value_for(appjson, "searchFolders") != NULL;
    }
    if (have_app_search) {
        for (int i = 0; i < app_search.n; i++) pl_add(&s->search_folders, app_search.items[i]);
    } else if (have_sys_search) {
        for (int i = 0; i < sys_search.n; i++) pl_add(&s->search_folders, sys_search.items[i]);
    } else {
        for (size_t i = 0; i < sizeof kDefaultSearch / sizeof kDefaultSearch[0]; i++)
            pl_add(&s->search_folders, kDefaultSearch[i]);
    }
    pl_free(&app_search);
    pl_free(&sys_search);
    if (appjson) free(appjson);
}

/* ── namespace e IMPORT ─────────────────────────────────────────────────────────────────── */

/* Tutti i namespace dichiarati da un file (riga-istruzione `NAMESPACE <X>`). */
static void file_namespaces(const char* path, char out[][64], int* nout, int cap) {
    int cnt = 0;
    char* s = read_all(path, NULL);
    if (!s) { if (nout) *nout = 0; return; }
    for (char* p = s; *p && cnt < cap; ) {
        char* eol = strchr(p, '\n');
        char* q = p;
        while (*q == ' ' || *q == '\t') q++;
        if (strncmp(q, "NAMESPACE ", 10) == 0) {
            const char* r = q + 10;
            char nm[64]; size_t i = 0;
            while (r[i] && !isspace((unsigned char)r[i]) && i + 1 < sizeof nm) {
                nm[i] = (char)toupper((unsigned char)r[i]); i++;
            }
            nm[i] = 0;
            if (i > 0) { snprintf(out[cnt], 64, "%s", nm); cnt++; }
        }
        if (!eol) break;
        p = eol + 1;
    }
    free(s);
    if (nout) *nout = cnt;
}

/* Namespace IMPORTati da un testo. Riga per riga (come ScanSourceForImports del C#):
 * ignora i commenti `'`, accetta `IMPORT A`, `IMPORT A, B`, `IMPORT A.*`, `IMPORT A (m)`,
 * `IMPORT A AS x` e `IMPORT A WITH ALIAS`. I nomi sono normalizzati in maiuscolo:
 * negli IMPORT i namespace si confrontano senza distinguere le maiuscole. */
static void scan_imports(const char* text, char out[][64], int* nout, int cap) {
    int n = 0;
    for (const char* p = text; p && *p && n < cap; ) {
        const char* eol = strchr(p, '\n');
        const char* end = eol ? eol : p + strlen(p);
        const char* q = p;
        while (q < end && (*q == ' ' || *q == '\t')) q++;
        /* numero di riga opzionale */
        while (q < end && isdigit((unsigned char)*q)) q++;
        while (q < end && (*q == ' ' || *q == '\t')) q++;
        if (end - q >= 7 && strncasecmp(q, "IMPORT ", 7) == 0) {
            q += 7;
            while (q < end) {
                while (q < end && (*q == ' ' || *q == '\t' || *q == ',')) q++;
                if (q >= end || *q == '\'') break;
                const char* st = q;
                while (q < end && (isalnum((unsigned char)*q) || *q == '_' || *q == '.')) q++;
                size_t len = (size_t)(q - st);
                if (len >= 2 && st[len - 1] == '*' && st[len - 2] == '.') len -= 2;
                if (len > 0) {
                    char nm[64]; size_t k = 0;
                    for (; k < len && k + 1 < sizeof nm; k++) nm[k] = (char)toupper((unsigned char)st[k]);
                    nm[k] = 0;
                    int dup = 0;
                    for (int i = 0; i < n; i++) if (strcmp(out[i], nm) == 0) { dup = 1; break; }
                    if (!dup && k > 0) { snprintf(out[n], 64, "%s", nm); n++; }
                }
                /* Salta la coda della clausola (lista selettiva, AS/WITH ALIAS) fino alla
                 * virgola di separazione o alla fine della riga. */
                int paren = 0;
                while (q < end) {
                    if (*q == '\'') { q = end; break; }
                    if (*q == '(') paren++;
                    else if (*q == ')') { if (paren > 0) paren--; }
                    else if (*q == ',' && paren == 0) break;
                    q++;
                }
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    if (nout) *nout = n;
}

/* ── discovery dei candidati ────────────────────────────────────────────────────────────── */

typedef struct {
    char* path;    /* assoluto canonicale */
    char* base;    /* basename: shadowing fra radici globali */
    char ns[IR_MAX_NS_PER_FILE][64];
    int nns;
} Cand;

typedef struct {
    Cand items[IR_MAX_FILES];
    int n;
    PathList claimed;   /* basename (lowercase) già presi da progetto/import */
    PathList dirs;      /* directory già scansionate */
} Discovery;

static int has_source_ext(const char* name) {
    size_t l = strlen(name);
    if (l < 4) return 0;
    return strcasecmp(name + l - 4, ".bas") == 0 || strcasecmp(name + l - 4, ".inc") == 0;
}

static void lower_copy(const char* s, char* out, size_t cap) {
    size_t i = 0;
    for (; s && s[i] && i + 1 < cap; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[i] = 0;
}

static int discovery_has_path(const Discovery* d, const char* path) {
    for (int i = 0; i < d->n; i++) if (strcmp(d->items[i].path, path) == 0) return 1;
    return 0;
}

/* Definita più sotto (dopo `dir_excluded`): la radice esclusa (cache/vendor pacchetti). */
static int under_excluded_root(const char* path);

static void discovery_add(Discovery* d, const char* path, int shadow) {
    if (d->n >= IR_MAX_FILES) return;
    char canon[IR_PATH];
    canon_path(path, canon, sizeof canon);
    if (under_excluded_root(canon)) return;   /* cache/vendor dei pacchetti: già preposti dal REQUIRE */
    if (discovery_has_path(d, canon)) return;
    const char* base = strrchr(canon, '/');
    base = base ? base + 1 : canon;
    char lbase[256]; lower_copy(base, lbase, sizeof lbase);
    if (shadow && pl_has(&d->claimed, lbase)) return;
    Cand* c = &d->items[d->n];
    memset(c, 0, sizeof *c);
    c->path = strdup(canon);
    c->base = strdup(lbase);
    if (!c->path || !c->base) { free(c->path); free(c->base); return; }
    file_namespaces(canon, c->ns, &c->nns, IR_MAX_NS_PER_FILE);
    d->n++;
    if (!shadow) pl_add(&d->claimed, lbase);
}

static int cmp_str(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

/* Cartelle che non fanno parte dei sorgenti di un progetto (output e VCS).
 * L'elenco è lo stesso del CLI di progetto (`gmb check --project`). */
static int dir_excluded(const char* name) {
    static const char* const kExcluded[] = {
        "work", "bin", "obj", ".git", ".vs", ".build", "node_modules",
    };
    for (size_t i = 0; i < sizeof kExcluded / sizeof kExcluded[0]; i++)
        if (strcasecmp(name, kExcluded[i]) == 0) return 1;
    return 0;
}

/* Radici ESCLUSE dalla discovery (cache/vendor dei pacchetti), impostate ESPLICITAMENTE dal
 * chiamante. PERCHÉ: cache e vendor stanno spesso DENTRO la cartella dei sorgenti
 * (layout `vendor/`: <progetto>/{app.bas, vendor/<pkg>/<ver>/<pkg>.inc}). Gli `.inc` dei
 * pacchetti dichiarano `NAMESPACE X`, quindi la discovery degli IMPORT li trova e li
 * prepone; ma il REQUIRE li ha GIÀ preposti → stesse dichiarazioni due volte e l'emitter
 * fallisce con "la procedura 'X' ... è già definita". Non è un'euristica sul nome della
 * cartella: sono i percorsi reali passati dal driver. */
#define IR_MAX_EXCL 4
static char g_excl_roots[IR_MAX_EXCL][IR_PATH];
static int g_n_excl;

void gmb_inc_exclude_root(const char* dir) {
    if (!dir || !*dir) { g_n_excl = 0; return; }
    if (g_n_excl >= IR_MAX_EXCL) return;
    canon_path(dir, g_excl_roots[g_n_excl], sizeof g_excl_roots[0]);
    if (g_excl_roots[g_n_excl][0]) g_n_excl++;
}

/* 1 se `path` (canonico) è dentro una radice esclusa (confronto per COMPONENTE di path). */
static int under_excluded_root(const char* path) {
    if (!path) return 0;
    for (int i = 0; i < g_n_excl; i++) {
        size_t rl = strlen(g_excl_roots[i]);
        if (rl == 0) continue;
        if (strncmp(path, g_excl_roots[i], rl) != 0) continue;
        if (path[rl] == 0 || path[rl] == '/') return 1;
    }
    return 0;
}

/* Scansione RICORSIVA di una cartella (parità Directory.GetFiles(..., AllDirectories)).
 * `shadow=1` per le radici globali: un file con lo stesso NOME di uno di progetto/import
 * viene ignorato, invece di finire due volte nella compilazione. */
static void discover_dir(Discovery* d, const char* dir, int shadow, int depth) {
    if (!dir || !*dir || depth > IR_MAX_DEPTH || d->n >= IR_MAX_FILES) return;
    char canon[IR_PATH];
    canon_path(dir, canon, sizeof canon);
    if (under_excluded_root(canon)) return;   /* cartella esclusa: non scendere */
    if (!is_dir(canon)) return;
    if (!pl_add(&d->dirs, canon)) return;

    DIR* dp = opendir(canon);
    if (!dp) return;
    PathList names; memset(&names, 0, sizeof names);
    struct dirent* ent;
    while ((ent = readdir(dp))) {
        if (ent->d_name[0] == '.') continue;
        char full[IR_PATH];
        snprintf(full, sizeof full, "%s/%s", canon, ent->d_name);
        if (is_dir(full)) continue;
        if (!has_source_ext(ent->d_name)) continue;
        pl_add(&names, ent->d_name);
    }
    closedir(dp);
    /* ordine deterministico: l'ordine di readdir cambia fra filesystem e renderebbe
     * instabile sia l'emissione sia i test. */
    if (names.n > 1) qsort(names.items, (size_t)names.n, sizeof(char*), cmp_str);
    for (int i = 0; i < names.n; i++) {
        char full[IR_PATH];
        snprintf(full, sizeof full, "%s/%s", canon, names.items[i]);
        discovery_add(d, full, shadow);
    }
    pl_free(&names);

    /* sottocartelle */
    dp = opendir(canon);
    if (!dp) return;
    PathList subs; memset(&subs, 0, sizeof subs);
    while ((ent = readdir(dp))) {
        if (ent->d_name[0] == '.') continue;
        if (dir_excluded(ent->d_name)) continue;   /* work/, bin/, obj/, .git/ ... */
        char full[IR_PATH];
        snprintf(full, sizeof full, "%s/%s", canon, ent->d_name);
        if (is_dir(full)) pl_add(&subs, full);
    }
    closedir(dp);
    if (subs.n > 1) qsort(subs.items, (size_t)subs.n, sizeof(char*), cmp_str);
    for (int i = 0; i < subs.n; i++) discover_dir(d, subs.items[i], shadow, depth + 1);
    pl_free(&subs);
}

/* Sostituzione dei placeholder `{project}` / `{install}` (case-insensitive). */
static void expand_placeholders(const char* in, const char* project, const char* install,
                                char* out, size_t cap) {
    size_t w = 0;
    for (const char* p = in; *p && w + 1 < cap; ) {
        if (p[0] == '{' && strncasecmp(p, "{project}", 9) == 0) {
            w += (size_t)snprintf(out + w, cap - w, "%s", project ? project : "");
            p += 9;
        } else if (p[0] == '{' && strncasecmp(p, "{install}", 9) == 0) {
            w += (size_t)snprintf(out + w, cap - w, "%s", install ? install : "");
            p += 9;
        } else {
            out[w++] = *p++;
        }
    }
    out[w] = 0;
}

/* Risolve una cartella di ricerca. Senza placeholder: relativa al progetto; se non
 * esiste, risalendo le cartelle (include/ e Controls/ della root), poi relativa
 * all'install — così il comportamento storico resta identico. */
static int resolve_search_dir(const char* entry, const char* project, const char* install,
                              char* out, size_t cap) {
    int hinted = strstr(entry, "{project}") || strstr(entry, "{PROJECT}") ||
                 strstr(entry, "{install}") || strstr(entry, "{INSTALL}");
    char buf[IR_PATH];
    expand_placeholders(entry, project, install, buf, sizeof buf);
    if (buf[0] == 0) return 0;
    /* Un path ASSOLUTO vale così com'è (parità Path.Combine del C#, che scarta i
     * segmenti precedenti quando il successivo è rooted). */
    int absolute = buf[0] == '/' || buf[0] == '\\'
                   || (isalpha((unsigned char)buf[0]) && buf[1] == ':');
    if (hinted || absolute) { snprintf(out, cap, "%s", buf); return 1; }
    char cand[IR_PATH];
    if (project && project[0]) {
        snprintf(cand, sizeof cand, "%s/%s", project, buf);
        if (is_dir(cand)) { snprintf(out, cap, "%s", cand); return 1; }
        char fake[IR_PATH];
        snprintf(fake, sizeof fake, "%s/.", project);
        if (find_subdir_up(fake, buf, out, cap)) return 1;
    }
    if (install && install[0]) {
        snprintf(cand, sizeof cand, "%s/%s", install, buf);
        if (is_dir(cand)) { snprintf(out, cap, "%s", cand); return 1; }
    }
    return 0;
}

/* Aggiunge una voce di ricerca: `*` finale = ogni sottocartella (una di livello). */
static void add_search_entry(Discovery* d, const char* entry, const char* project,
                             const char* install, int shadow) {
    char buf[IR_PATH];
    expand_placeholders(entry, project, install, buf, sizeof buf);
    size_t l = strlen(buf);
    int glob = l >= 2 && buf[l - 1] == '*' && (buf[l - 2] == '/' || buf[l - 2] == '\\');
    if (glob) buf[l - 2] = 0;
    char dir[IR_PATH];
    if (glob) {
        if (!is_dir(buf)) return;
        DIR* dp = opendir(buf);
        if (!dp) return;
        PathList subs; memset(&subs, 0, sizeof subs);
        struct dirent* ent;
        while ((ent = readdir(dp))) {
            if (ent->d_name[0] == '.') continue;
            char full[IR_PATH];
            snprintf(full, sizeof full, "%s/%s", buf, ent->d_name);
            if (is_dir(full)) pl_add(&subs, full);
        }
        closedir(dp);
        if (subs.n > 1) qsort(subs.items, (size_t)subs.n, sizeof(char*), cmp_str);
        for (int i = 0; i < subs.n; i++) discover_dir(d, subs.items[i], shadow, 0);
        pl_free(&subs);
        return;
    }
    if (!resolve_search_dir(entry, project, install, dir, sizeof dir)) return;
    discover_dir(d, dir, shadow, 0);
}

static void discovery_free(Discovery* d) {
    for (int i = 0; i < d->n; i++) { free(d->items[i].path); free(d->items[i].base); }
    d->n = 0;
    pl_free(&d->claimed);
    pl_free(&d->dirs);
}

/* Costruisce l'elenco dei candidati per una radice di programma. `entry_path` (se non
 * NULL) è aggiunta come candidato anche quando vive fuori dalla radice. */
static void discovery_build_at(Discovery* d, const char* project_dir, const char* entry_path) {
    memset(d, 0, sizeof *d);
    char proj[IR_PATH];
    if (project_dir && *project_dir) canon_path(project_dir, proj, sizeof proj);
    else proj[0] = 0;
    char install[4096]; install[0] = 0;
    (void)ir_root(install, sizeof install);
    ResolveSettings st;
    settings_load(proj, &st);

    /* 1. radice del programma, ricorsiva */
    if (proj[0]) discover_dir(d, proj, 0, 0);

    /* 2. importFolders (relative al progetto e alla root dell'install) + importFiles */
    for (int i = 0; i < st.import_folders.n; i++) {
        const char* f = st.import_folders.items[i];
        char buf[IR_PATH];
        expand_placeholders(f, proj, install, buf, sizeof buf);
        char dir[IR_PATH];
        if (proj[0] && (snprintf(dir, sizeof dir, "%s/%s", proj, buf), is_dir(dir)))
            discover_dir(d, dir, 0, 0);
        if (install[0] && (snprintf(dir, sizeof dir, "%s/%s", install, buf), is_dir(dir)))
            discover_dir(d, dir, 0, 0);
    }
    for (int i = 0; i < st.import_files.n; i++) {
        const char* f = st.import_files.items[i];
        char buf[IR_PATH];
        expand_placeholders(f, proj, install, buf, sizeof buf);
        char file[IR_PATH];
        if (proj[0] && (snprintf(file, sizeof file, "%s/%s", proj, buf), is_file(file)))
            discovery_add(d, file, 0);
        else if (install[0] && (snprintf(file, sizeof file, "%s/%s", install, buf), is_file(file)))
            discovery_add(d, file, 0);
    }

    /* 3. cartelle di ricerca del compilatore: shadowing per nome delle sorgenti locali */
    for (int i = 0; i < st.search_folders.n; i++)
        add_search_entry(d, st.search_folders.items[i], proj, install, 1);

    /* La entry è sempre un candidato, anche se vive fuori dalla radice del programma. */
    if (entry_path && *entry_path && !discovery_has_path(d, entry_path))
        discovery_add(d, entry_path, 0);
    settings_free(&st);
}

/* Candidati per un sorgente di ingresso (radice = cartella del file). */
static void discovery_build(Discovery* d, const char* input_path) {
    char proj[IR_PATH]; dir_of(input_path, proj, sizeof proj);
    discovery_build_at(d, proj, input_path);
}

/* ── grafo IMPORT (transitivo) ─────────────────────────────────────────────────────────── */

static void visit_file(Discovery* d, const char* path, PathList* order, PathList* done) {
    char canon[IR_PATH];
    canon_path(path, canon, sizeof canon);
    if (pl_has(done, canon)) return;
    pl_add(done, canon);

    char* text = read_all(canon, NULL);
    if (!text) return;
    char imps[IR_MAX_IMPORTS][64];
    int ni = 0;
    scan_imports(text, imps, &ni, IR_MAX_IMPORTS);
    free(text);

    for (int i = 0; i < ni; i++) {
        for (int c = 0; c < d->n; c++) {
            Cand* cand = &d->items[c];
            int hit = 0;
            for (int k = 0; k < cand->nns && !hit; k++)
                if (strcmp(cand->ns[k], imps[i]) == 0) hit = 1;
            if (!hit) continue;
            if (strcmp(cand->path, canon) == 0) continue;
            visit_file(d, cand->path, order, done);
        }
    }
    /* post-order: le dipendenze sono emesse PRIMA di chi le importa. */
    pl_add(order, canon);
}

/* Elenco in ordine di emissione dei moduli importati (entry esclusa, malloc). */
char** gmb_resolve_module_files(const char* input_path, int* out_n) {
    if (out_n) *out_n = 0;
    if (!input_path || !*input_path) return NULL;
    Discovery d;
    discovery_build(&d, input_path);
    PathList order; memset(&order, 0, sizeof order);
    PathList done; memset(&done, 0, sizeof done);
    char entry[IR_PATH];
    canon_path(input_path, entry, sizeof entry);
    visit_file(&d, input_path, &order, &done);
    pl_free(&done);
    discovery_free(&d);

    int n = 0;
    for (int i = 0; i < order.n; i++) if (strcmp(order.items[i], entry) != 0) n++;
    char** files = n > 0 ? (char**)calloc((size_t)n, sizeof(char*)) : NULL;
    int w = 0;
    for (int i = 0; i < order.n && files; i++)
        if (strcmp(order.items[i], entry) != 0) files[w++] = strdup(order.items[i]);
    pl_free(&order);
    if (out_n) *out_n = files ? w : 0;
    return files;
}

void gmb_resolve_module_files_free(char** files, int n) {
    for (int i = 0; i < n; i++) free(files[i]);
    free(files);
}

/* ── progetto (O7): candidati, namespace, IMPORT ──────────────────────────────────────────
 * Usati dal CLI `gmb check/build/run --project`. La separazione è voluta: il CLI non
 * reimplementa la discovery, la chiede al resolver che è la stessa usata dal compilatore. */

int gmb_project_sources(const char* project_dir, char*** out_files, int* out_n) {
    if (out_files) *out_files = NULL;
    if (out_n) *out_n = 0;
    if (!project_dir || !*project_dir) return 0;
    Discovery d;
    discovery_build_at(&d, project_dir, NULL);
    char** files = d.n > 0 ? (char**)calloc((size_t)d.n, sizeof(char*)) : NULL;
    int n = 0;
    for (int i = 0; i < d.n && files; i++) {
        files[n] = strdup(d.items[i].path);
        if (files[n]) n++;
    }
    discovery_free(&d);
    if (out_files) *out_files = files;
    if (out_n) *out_n = n;
    return n;
}

int gmb_file_namespaces(const char* path, char out[][64], int max) {
    int n = 0;
    file_namespaces(path, out, &n, max);
    return n;
}

int gmb_file_imports(const char* path, char out[][64], int max) {
    char* text = read_all(path, NULL);
    if (!text) return 0;
    int n = 0;
    scan_imports(text, out, &n, max);
    free(text);
    return n;
}

int gmb_namespace_is_builtin(const char* ns) {
    if (!ns || !*ns) return 0;
    for (size_t i = 0; i < GMB_NS_COUNT; i++)
        if (strcasecmp(ns, gmb_ns_names[i]) == 0) return 1;
    return 0;
}

int gmb_resolve_install_root(char* out, size_t cap) {
    return ir_root(out, cap);
}

void gmb_string_list_free(char** items, int n) {
    for (int i = 0; i < n; i++) free(items[i]);
    free(items);
}

/* ── O8: mappa diagnostica (segmenti del testo concatenato) ─────────────────────────────── */

void gmb_src_map_init(GMB_SRC_MAP* m) {
    if (!m) return;
    m->segs = NULL; m->n = 0; m->cap = 0;
}

void gmb_src_map_free(GMB_SRC_MAP* m) {
    if (!m) return;
    for (int i = 0; i < m->n; i++) free(m->segs[i].file);
    free(m->segs);
    m->segs = NULL; m->n = 0; m->cap = 0;
}

static void src_map_add(GMB_SRC_MAP* m, int line, const char* file) {
    if (!m || !file || !*file) return;
    if (m->n == m->cap) {
        int cap = m->cap ? m->cap * 2 : 8;
        GMB_SRC_SEG* p = (GMB_SRC_SEG*)realloc(m->segs, (size_t)cap * sizeof *p);
        if (!p) return;
        m->segs = p;
        m->cap = cap;
    }
    m->segs[m->n].line = line;
    m->segs[m->n].file = strdup(file);
    if (m->segs[m->n].file) m->n++;
}

void gmb_src_map_shift(GMB_SRC_MAP* m, int delta) {
    if (!m || delta == 0) return;
    for (int i = 0; i < m->n; i++) m->segs[i].line += delta;
}

int gmb_src_map_locate(const GMB_SRC_MAP* m, int line, const char** file, int* file_line) {
    if (!m || m->n == 0 || line <= 0) return 0;
    int best = -1;
    for (int i = 0; i < m->n; i++) {
        if (m->segs[i].line <= line && (best < 0 || m->segs[i].line > m->segs[best].line)) best = i;
    }
    if (best < 0) return 0;
    if (file) *file = m->segs[best].file;
    if (file_line) *file_line = 1 + (line - m->segs[best].line);
    return 1;
}

char* gmb_resolve_includes_mapped(const char* src, const char* input_path, GMB_SRC_MAP* map) {
    if (!src || !input_path) return src ? strdup(src) : NULL;
    int nmod = 0;
    char** mods = gmb_resolve_module_files(input_path, &nmod);
    if (!mods || nmod == 0) {
        src_map_add(map, 1, input_path);
        gmb_resolve_module_files_free(mods, nmod);
        return strdup(src);
    }

    size_t cap = strlen(src) + 64; char* out = (char*)malloc(cap); out[0] = 0; size_t olen = 0;
    int cur_line = 1;   /* riga 1-based in cui comincia il prossimo testo copiato */
    for (int m = 0; m < nmod; m++) {
        long n = 0; char* mod = read_all(mods[m], &n);
        if (!mod) continue;
        if (olen + (size_t)n + 2 > cap) { cap = (olen + (size_t)n + 2) * 2; out = (char*)realloc(out, cap); }
        src_map_add(map, cur_line, mods[m]);
        memcpy(out + olen, mod, (size_t)n); olen += (size_t)n; out[olen++] = '\n'; out[olen] = 0;
        cur_line += count_nl(mod) + 1;
        {
            static const char kReset[] = "\nNAMESPACE\n";
            size_t rl = sizeof kReset - 1;
            if (olen + rl + 1 > cap) { cap = (olen + rl + 1) * 2; out = (char*)realloc(out, cap); }
            memcpy(out + olen, kReset, rl); olen += rl; out[olen] = 0;
            cur_line += count_nl(kReset);
        }
        free(mod);
    }
    if (olen + strlen(src) + 1 > cap) { cap = olen + strlen(src) + 1; out = (char*)realloc(out, cap); }
    src_map_add(map, cur_line, input_path);
    memcpy(out + olen, src, strlen(src) + 1);
    gmb_resolve_module_files_free(mods, nmod);
    return out;
}

char* gmb_resolve_includes(const char* src, const char* input_path) {
    return gmb_resolve_includes_mapped(src, input_path, NULL);
}
