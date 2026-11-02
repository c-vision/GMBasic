/* gmb_brain_registry.c — registry dei cervelli (config/brains.json) e manifest
 * (brain.manifest.json) in C: sorgente UNICA del Second Brain.
 *
 * Il formato è quello del C# `BrainRegistry`/`BrainManifest` (camelCase, indentato):
 * un cervello registrato dal C è letto dal C# e viceversa, senza doppie implementazioni.
 */
#include "gmb_brain_tools.h"

#include "gmb_runtime.h"
#include "gmb_brains.h"
#include "gmb_fsutil.h"
#include "gmb_commonlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ── helper ──────────────────────────────────────────────────────────────── */

static char* read_all(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) n = 0;
    char* b = (char*)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)n, f);
    b[rd] = 0;
    fclose(f);
    return b;
}

static void put_escaped(FILE* f, const char* s)
{
    if (!s) { fputs("null", f); return; }
    fputc('"', f);
    for (const char* p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f); break;
        case '\r': fputs("\\r", f); break;
        case '\t': fputs("\\t", f); break;
        default:
            if (c < 0x20) fprintf(f, "\\u%04x", c);
            else fputc((int)c, f);
        }
    }
    fputc('"', f);
}

static void str_copy(char* dst, size_t cap, const char* src)
{
    snprintf(dst, cap, "%s", (src && *src) ? src : "");
}

static const char* jstr(gmb_val_t o, const char* k)
{
    gmb_val_t v = gmb_val_dict_get(o, k);
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return v.v_string.data;
    return NULL;
}

static long jlong(gmb_val_t o, const char* k, long dflt)
{
    gmb_val_t v = gmb_val_dict_get(o, k);
    if (v.kind == GMB_TYPE_INT) return (long)v.v_int;
    if (v.kind == GMB_TYPE_DOUBLE) return (long)v.v_double;
    return dflt;
}

static int jbool(gmb_val_t o, const char* k, int dflt)
{
    gmb_val_t v = gmb_val_dict_get(o, k);
    if (v.kind == GMB_TYPE_BOOL) return v.v_bool ? 1 : 0;
    return dflt;
}

/* ── stato del registry ──────────────────────────────────────────────────── */

#define BR_MAX 256
typedef struct {
    gmb_brain_rec_t rec;
    char* probe_json;      /* oggetto JSON (per round-trip) */
    char* parsers_json;    /* array JSON */
    char* available_json;  /* array JSON */
} br_entry_t;

static br_entry_t g_brain[BR_MAX];
static int g_nbrain;
static char* g_attached[BR_MAX];
static int g_nattached;
static int g_loaded;
static char g_reg_path[1200];

static void reg_clear(void)
{
    for (int i = 0; i < g_nbrain; i++) {
        free(g_brain[i].probe_json);
        free(g_brain[i].parsers_json);
        free(g_brain[i].available_json);
    }
    memset(g_brain, 0, sizeof g_brain);
    g_nbrain = 0;
    for (int i = 0; i < g_nattached; i++) { free(g_attached[i]); g_attached[i] = NULL; }
    g_nattached = 0;
}

/* value JSON → stringa compatta (per conservare probe/parsers/available). */
static char* json_compact(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_ARRAY || v.kind == GMB_TYPE_OBJECT) {
        /* ri-serializza i soli tipi che ci servono (array di stringhe, dict di stringhe) */
        size_t cap = 1 << 14, off = 0;
        char* out = (char*)malloc(cap);
        if (!out) return NULL;
        if (v.kind == GMB_TYPE_ARRAY) {
            out[off++] = '[';
            gmb_list_t l = gmb_val_foreach_list(v);
            gmb_int_t n = gmb_list_count(l);
            for (gmb_int_t i = 0; i < n; i++) {
                gmb_val_t e = gmb_list_get(l, i);
                if (e.kind == GMB_TYPE_STRING && e.v_string.data) {
                    char tmp[2048];
                    size_t t = 0;
                    tmp[t++] = '"';
                    for (const char* p = e.v_string.data; *p && t < sizeof tmp - 8; p++) {
                        if (*p == '"' || *p == '\\') tmp[t++] = '\\';
                        if (*p == '\n') { tmp[t++] = '\\'; tmp[t++] = 'n'; continue; }
                        tmp[t++] = *p;
                    }
                    tmp[t++] = '"';
                    tmp[t] = 0;
                    if (off + t + 8 >= cap) { cap = (off + t) * 2 + 1024; char* nb = (char*)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; }
                    if (i) out[off++] = ',';
                    memcpy(out + off, tmp, t);
                    off += t;
                }
            }
            out[off++] = ']';
            out[off] = 0;
            gmb_list_free(&l);
        } else {
            out[off++] = '{';
            gmb_int_t nk = 0;
            gmb_string_t* keys = gmb_val_dict_keys_array(v, &nk);
            int first = 1;
            for (int i = 0; keys && i < (int)nk; i++) {
                if (!keys[i].data) continue;
                gmb_val_t e = gmb_val_dict_get(v, keys[i].data);
                if (e.kind != GMB_TYPE_STRING || !e.v_string.data) continue;
                char tmp[4096];
                size_t t = 0;
                tmp[t++] = '"';
                for (const char* p = keys[i].data; *p && t < sizeof tmp - 8; p++) { if (*p == '"') tmp[t++] = '\\'; tmp[t++] = *p; }
                tmp[t++] = '"';
                tmp[t++] = ':';
                tmp[t++] = '"';
                for (const char* p = e.v_string.data; *p && t < sizeof tmp - 8; p++) {
                    if (*p == '"' || *p == '\\') tmp[t++] = '\\';
                    if (*p == '\n') { tmp[t++] = '\\'; tmp[t++] = 'n'; continue; }
                    tmp[t++] = *p;
                }
                tmp[t++] = '"';
                tmp[t] = 0;
                if (off + t + 8 >= cap) { cap = (off + t) * 2 + 1024; char* nb = (char*)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; }
                if (!first) out[off++] = ',';
                first = 0;
                memcpy(out + off, tmp, t);
                off += t;
            }
            out[off++] = '}';
            out[off] = 0;
        }
        return out;
    }
    return NULL;
}

static br_entry_t* find_entry(const char* name)
{
    for (int i = 0; i < g_nbrain; i++)
        if (!strcasecmp(g_brain[i].rec.name, name)) return &g_brain[i];
    return NULL;
}

static void reg_load(void)
{
    if (g_loaded) return;
    g_loaded = 1;
    gmb_paths_brains_json(g_reg_path, sizeof g_reg_path);
    char* js = read_all(g_reg_path);
    if (!js) return;
    gmb_val_t root = gmb_json_parse(js);
    free(js);
    if (root.kind != GMB_TYPE_OBJECT) return;
    gmb_val_t att = gmb_val_dict_get(root, "attached");
    gmb_list_t al = gmb_val_foreach_list(att);
    gmb_int_t an = gmb_list_count(al);
    for (gmb_int_t i = 0; i < an && g_nattached < BR_MAX; i++) {
        gmb_val_t e = gmb_list_get(al, i);
        if (e.kind == GMB_TYPE_STRING && e.v_string.data && e.v_string.data[0])
            g_attached[g_nattached++] = strdup(e.v_string.data);
    }
    gmb_list_free(&al);

    gmb_val_t brains = gmb_val_dict_get(root, "brains");
    gmb_list_t bl = gmb_val_foreach_list(brains);
    gmb_int_t bn = gmb_list_count(bl);
    for (gmb_int_t i = 0; i < bn && g_nbrain < BR_MAX; i++) {
        gmb_val_t e = gmb_list_get(bl, i);
        if (e.kind != GMB_TYPE_OBJECT) continue;
        br_entry_t* t = &g_brain[g_nbrain++];
        str_copy(t->rec.name, sizeof t->rec.name, jstr(e, "name"));
        str_copy(t->rec.mode, sizeof t->rec.mode, jstr(e, "mode"));
        str_copy(t->rec.source, sizeof t->rec.source, jstr(e, "source"));
        str_copy(t->rec.vault_path, sizeof t->rec.vault_path, jstr(e, "vaultPath"));
        str_copy(t->rec.checksum, sizeof t->rec.checksum, jstr(e, "checksum"));
        str_copy(t->rec.version, sizeof t->rec.version, jstr(e, "version"));
        str_copy(t->rec.installed_at, sizeof t->rec.installed_at, jstr(e, "installedAt"));
        str_copy(t->rec.signed_by, sizeof t->rec.signed_by, jstr(e, "signedBy"));
        t->rec.docs = jlong(e, "docs", 0);
        t->rec.attached = jbool(e, "attached", 0);
        t->probe_json = json_compact(gmb_val_dict_get(e, "probe"));
        t->parsers_json = json_compact(gmb_val_dict_get(e, "parsers"));
        t->available_json = json_compact(gmb_val_dict_get(e, "available"));
    }
    gmb_list_free(&bl);
}

static int reg_save(void)
{
    char dir[1200];
    snprintf(dir, sizeof dir, "%s", g_reg_path);
    char* sl = strrchr(dir, '/');
    if (sl) { *sl = 0; gmb_fs_mkdir_p(dir); }

    char tmp[1300];
    snprintf(tmp, sizeof tmp, "%s.tmp", g_reg_path);
    FILE* f = fopen(tmp, "wb");
    if (!f) return 0;
    fputs("{\n  \"attached\": [", f);
    for (int i = 0; i < g_nattached; i++) {
        if (i) fputs(", ", f);
        put_escaped(f, g_attached[i]);
    }
    fputs("],\n  \"brains\": [", f);
    for (int i = 0; i < g_nbrain; i++) {
        br_entry_t* t = &g_brain[i];
        if (i) fputs(",", f);
        fputs("\n    {\n      \"name\": ", f); put_escaped(f, t->rec.name);
        fputs(",\n      \"source\": ", f); put_escaped(f, t->rec.source[0] ? t->rec.source : NULL);
        fputs(",\n      \"vaultPath\": ", f); put_escaped(f, t->rec.vault_path[0] ? t->rec.vault_path : NULL);
        fputs(",\n      \"mode\": ", f); put_escaped(f, t->rec.mode);
        fputs(",\n      \"available\": ", f);
        fputs(t->available_json && t->available_json[0] ? t->available_json : "[]", f);
        fputs(",\n      \"checksum\": ", f); put_escaped(f, t->rec.checksum[0] ? t->rec.checksum : NULL);
        fputs(",\n      \"version\": ", f); put_escaped(f, t->rec.version[0] ? t->rec.version : NULL);
        fprintf(f, ",\n      \"docs\": %ld", t->rec.docs);
        fputs(",\n      \"parsers\": ", f);
        fputs(t->parsers_json && t->parsers_json[0] ? t->parsers_json : "[]", f);
        fprintf(f, ",\n      \"attached\": %s", t->rec.attached ? "true" : "false");
        fputs(",\n      \"installedAt\": ", f); put_escaped(f, t->rec.installed_at[0] ? t->rec.installed_at : NULL);
        fputs(",\n      \"probe\": ", f);
        fputs(t->probe_json && t->probe_json[0] ? t->probe_json : "{}", f);
        fputs(",\n      \"signedBy\": ", f); put_escaped(f, t->rec.signed_by[0] ? t->rec.signed_by : NULL);
        fputs("\n    }", f);
    }
    fputs("\n  ]\n}\n", f);
    fclose(f);
    if (rename(tmp, g_reg_path) != 0) { remove(tmp); return 0; }
    return 1;
}

/* ── disponibilità su disco ──────────────────────────────────────────────── */

/* Vero se `root` è la cartella INSTALLATA del cervello `name` (basename == name):
 * il fallback "qualsiasi .db/.json dentro" vale solo lì, mai sulle dir globali. */
static int root_belongs_to(const char* root, const char* name)
{
    if (!root || !*root || !name || !*name) return 0;
    const char* base = strrchr(root, '/');
    base = base ? base + 1 : root;
    return !strcasecmp(base, name);
}

static int dir_any_with_ext(const char* dir, const char* ext, char* out, size_t cap)
{
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* e;
    int found = 0;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        size_t l = strlen(e->d_name), xl = strlen(ext);
        if (l <= xl || strcasecmp(e->d_name + l - xl, ext)) continue;
        /* esclude gli export accessori del grafo */
        if (strstr(e->d_name, ".stats.") || strstr(e->d_name, ".state.")) continue;
        snprintf(out, cap, "%s/%s", dir, e->d_name);
        found = 1;
    }
    closedir(d);
    return found;
}

int gmb_brain_reg_rag_db(const char* name, char* out, size_t cap)
{
    if (!name || !*name) return 0;
    reg_load();
    char root[1024];
    if (gmb_brain_reg_root(name, root, sizeof root)) {
        char p[2048];
        snprintf(p, sizeof p, "%s/rag/%s.db", root, name);
        if (access(p, R_OK) == 0) { snprintf(out, cap, "%s", p); return 1; }
        if (root_belongs_to(root, name)) {
            snprintf(p, sizeof p, "%s/rag", root);
            if (dir_any_with_ext(p, ".db", out, cap)) return 1;
        }
    }
    br_entry_t* t = find_entry(name);
    if (t) {
        /* `source` è un FILE (il db) per i cervelli costruiti dal RAG; per quelli creati da
         * una cartella è la cartella stessa: solo un file regolare è un db RAG. */
        struct stat st;
        if (t->rec.source[0] && stat(t->rec.source, &st) == 0 && S_ISREG(st.st_mode) &&
            access(t->rec.source, R_OK) == 0) {
            snprintf(out, cap, "%s", t->rec.source);
            return 1;
        }
        if (t->rec.vault_path[0]) {
            char p[2048];
            snprintf(p, sizeof p, "%s/rag/%s.db", t->rec.vault_path, name);
            if (access(p, R_OK) == 0) { snprintf(out, cap, "%s", p); return 1; }
        }
        if (t->rec.source[0]) {
            char p[2048];
            snprintf(p, sizeof p, "%s/rag/%s.db", t->rec.source, name);
            if (access(p, R_OK) == 0) { snprintf(out, cap, "%s", p); return 1; }
        }
    }
    char ragdir[1024];
    gmb_paths_rag_dir(ragdir, sizeof ragdir);
    char p[2048];
    snprintf(p, sizeof p, "%s/%s.db", ragdir, name);
    if (access(p, R_OK) == 0) { snprintf(out, cap, "%s", p); return 1; }
    return 0;
}

int gmb_brain_reg_graph_json(const char* name, char* out, size_t cap)
{
    if (!name || !*name) return 0;
    reg_load();
    char root[1024];
    if (gmb_brain_reg_root(name, root, sizeof root)) {
        char p[2048];
        snprintf(p, sizeof p, "%s/graphs/%s.json", root, name);
        if (access(p, R_OK) == 0) { snprintf(out, cap, "%s", p); return 1; }
        if (root_belongs_to(root, name)) {
            snprintf(p, sizeof p, "%s/graphs", root);
            if (dir_any_with_ext(p, ".json", out, cap)) return 1;
        }
    }
    br_entry_t* t = find_entry(name);
    if (t) {
        if (t->rec.vault_path[0]) {
            char p[2048];
            snprintf(p, sizeof p, "%s/graphs/%s.json", t->rec.vault_path, name);
            if (access(p, R_OK) == 0) { snprintf(out, cap, "%s", p); return 1; }
        }
    }
    char gdir[1024];
    gmb_paths_graphs_dir(gdir, sizeof gdir);
    char p[2048];
    snprintf(p, sizeof p, "%s/%s.json", gdir, name);
    if (access(p, R_OK) == 0) { snprintf(out, cap, "%s", p); return 1; }
    return 0;
}

int gmb_brain_reg_root(const char* name, char* out, size_t cap)
{
    if (!name || !*name) return 0;
    char dir[1024];
    if (gmb_brain_lookup(name, NULL, dir, sizeof dir) && dir[0]) { snprintf(out, cap, "%s", dir); return 1; }
    char bdir[1024];
    gmb_paths_brains_dir(bdir, sizeof bdir);
    char p[2048];
    snprintf(p, sizeof p, "%s/%s", bdir, name);
    if (access(p, F_OK) == 0) { snprintf(out, cap, "%s", p); return 1; }
    return 0;
}

int gmb_brain_reg_available(const char* name, char* out, size_t cap)
{
    out[0] = 0;
    char p[2048];
    int rag = gmb_brain_reg_rag_db(name, p, sizeof p);
    int graph = gmb_brain_reg_graph_json(name, p, sizeof p);
    if (rag && graph) snprintf(out, cap, "rag,graph");
    else if (rag) snprintf(out, cap, "rag");
    else if (graph) snprintf(out, cap, "graph");
    return (rag || graph) ? 1 : 0;
}

int gmb_brain_reg_attached(const char* name)
{
    reg_load();
    for (int i = 0; i < g_nattached; i++) if (!strcasecmp(g_attached[i], name)) return 1;
    return 0;
}

/* ── manifest ────────────────────────────────────────────────────────────── */

char* gmb_brain_manifest_hash_file(const char* path)
{
    char hex[65] = "";
    if (!gmb_sha256_file_hex(path, hex, sizeof hex)) return NULL;
    /* MAIUSCOLO come Convert.ToHexString del C#: il confronto all'import è Ordinal
     * (case-sensitive) e un hash minuscolo farebbe fallire la verifica del cervello. */
    for (char* p = hex; *p; p++) *p = (char)toupper((unsigned char)*p);
    char* out = (char*)malloc(strlen(hex) + 8);
    if (!out) return NULL;
    sprintf(out, "sha256:%s", hex);
    return out;
}

static void manifest_root(const char* root, char* out, size_t cap)
{
    snprintf(out, cap, "%s/brain.manifest.json", root);
}

char* gmb_brain_manifest_load(const char* root)
{
    char p[2200];
    manifest_root(root, p, sizeof p);
    return read_all(p);
}

int gmb_brain_manifest_meta(const char* root, char* name, size_t ncap, char* version, size_t vcap,
                            char* mode, size_t mcap, long* docs)
{
    if (name && ncap) name[0] = 0;
    if (version && vcap) version[0] = 0;
    if (mode && mcap) mode[0] = 0;
    if (docs) *docs = 0;
    char* js = gmb_brain_manifest_load(root);
    if (!js) return 0;
    gmb_val_t m = gmb_json_parse(js);
    free(js);
    if (m.kind != GMB_TYPE_OBJECT) return 0;
    if (name) str_copy(name, ncap, jstr(m, "name"));
    if (version) str_copy(version, vcap, jstr(m, "version"));
    if (mode) str_copy(mode, mcap, jstr(m, "mode"));
    if (docs) *docs = jlong(m, "docs", 0);
    return 1;
}

int gmb_brain_manifest_verify(const char* root, char* err, size_t cap)
{
    if (err && cap) err[0] = 0;
    char* js = gmb_brain_manifest_load(root);
    if (!js) return 1;   /* assente: non è un errore (cervello costruito localmente) */
    gmb_val_t m = gmb_json_parse(js);
    free(js);
    if (m.kind != GMB_TYPE_OBJECT) {
        if (err) snprintf(err, cap, "manifest non è un oggetto JSON");
        return -1;
    }
    /* i file dichiarati devono esistere e avere lo sha256 indicato */
    gmb_val_t files = gmb_val_dict_get(m, "files");
    if (files.kind == GMB_TYPE_OBJECT) {
        gmb_int_t nk = 0;
        gmb_string_t* keys = gmb_val_dict_keys_array(files, &nk);
        for (int i = 0; keys && i < (int)nk; i++) {
            if (!keys[i].data) continue;
            gmb_val_t v = gmb_val_dict_get(files, keys[i].data);
            if (v.kind != GMB_TYPE_STRING || !v.v_string.data) continue;
            char full[2600];
            snprintf(full, sizeof full, "%s/%s", root, keys[i].data);
            if (access(full, R_OK) != 0) {
                if (err) snprintf(err, cap, "file mancante: %s", keys[i].data);
                return -1;
            }
            char* h = gmb_brain_manifest_hash_file(full);
            if (!h || strcasecmp(h, v.v_string.data) != 0) {
                free(h);
                if (err) snprintf(err, cap, "checksum diverso per %s", keys[i].data);
                return -1;
            }
            free(h);
        }
    }
    return 0;
}

/* ── info effettiva ──────────────────────────────────────────────────────── */

int gmb_brain_reg_info(const char* name, gmb_brain_rec_t* out)
{
    if (!name || !*name || !out) return 0;
    memset(out, 0, sizeof *out);
    reg_load();
    br_entry_t* t = find_entry(name);
    char avail[64] = "";
    gmb_brain_reg_available(name, avail, sizeof avail);
    char root[1024] = "";
    int has_root = gmb_brain_reg_root(name, root, sizeof root);
    if (!t && !avail[0]) return 0;

    snprintf(out->name, sizeof out->name, "%s", name);
    snprintf(out->available, sizeof out->available, "%s", avail);
    out->attached = gmb_brain_reg_attached(name);
    if (t) {
        snprintf(out->mode, sizeof out->mode, "%s", t->rec.mode);
        snprintf(out->source, sizeof out->source, "%s", t->rec.source);
        snprintf(out->vault_path, sizeof out->vault_path, "%s", t->rec.vault_path);
        snprintf(out->checksum, sizeof out->checksum, "%s", t->rec.checksum);
        snprintf(out->version, sizeof out->version, "%s", t->rec.version);
        snprintf(out->installed_at, sizeof out->installed_at, "%s", t->rec.installed_at);
        snprintf(out->signed_by, sizeof out->signed_by, "%s", t->rec.signed_by);
        out->docs = t->rec.docs;
    }
    /* il manifest completa i campi mancanti */
    if (has_root) {
        char mn[128] = "", mv[64] = "", mm[16] = "";
        long md = 0;
        if (gmb_brain_manifest_meta(root, mn, sizeof mn, mv, sizeof mv, mm, sizeof mm, &md)) {
            if (!out->version[0]) snprintf(out->version, sizeof out->version, "%s", mv);
            if (!out->mode[0]) snprintf(out->mode, sizeof out->mode, "%s", mm);
            if (out->docs <= 0) out->docs = md;
            if (!out->name[0]) snprintf(out->name, sizeof out->name, "%s", mn);
            if (!out->checksum[0]) {
                char* js = gmb_brain_manifest_load(root);
                if (js) {
                    gmb_val_t m = gmb_json_parse(js);
                    free(js);
                    const char* cs = jstr(m, "checksum");
                    if (cs) snprintf(out->checksum, sizeof out->checksum, "%s", cs);
                    const char* sb = jstr(m, "signedBy");
                    if (sb && !out->signed_by[0]) snprintf(out->signed_by, sizeof out->signed_by, "%s", sb);
                }
            }
            if (!out->vault_path[0]) snprintf(out->vault_path, sizeof out->vault_path, "%s", root);
        } else if (!out->vault_path[0] && has_root) {
            snprintf(out->vault_path, sizeof out->vault_path, "%s", root);
        }
    }
    if (!out->mode[0]) {
        if (!strcmp(avail, "rag,graph")) snprintf(out->mode, sizeof out->mode, "both");
        else if (avail[0]) snprintf(out->mode, sizeof out->mode, "%s", avail);
    }
    return 1;
}

static void now_stamp(char* out, size_t cap)
{
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(out, cap, "%Y-%m-%d %H:%M:%S", &tmv);
}

int gmb_brain_reg_register(const char* name, const char* source, const char* vault_path,
                           const char* mode, const char* checksum, const char* version,
                           long docs, const char* signed_by)
{
    if (!name || !*name) return 0;
    reg_load();
    br_entry_t* t = find_entry(name);
    if (!t) {
        if (g_nbrain >= BR_MAX) return 0;
        t = &g_brain[g_nbrain++];
        memset(t, 0, sizeof *t);
        snprintf(t->rec.name, sizeof t->rec.name, "%s", name);
    }
    if (source) snprintf(t->rec.source, sizeof t->rec.source, "%s", source);
    if (vault_path) snprintf(t->rec.vault_path, sizeof t->rec.vault_path, "%s", vault_path);
    if (mode) snprintf(t->rec.mode, sizeof t->rec.mode, "%s", mode);
    if (checksum) snprintf(t->rec.checksum, sizeof t->rec.checksum, "%s", checksum);
    if (version) snprintf(t->rec.version, sizeof t->rec.version, "%s", version);
    if (signed_by) snprintf(t->rec.signed_by, sizeof t->rec.signed_by, "%s", signed_by);
    if (docs > 0) t->rec.docs = docs;
    if (!t->rec.installed_at[0]) now_stamp(t->rec.installed_at, sizeof t->rec.installed_at);
    t->rec.attached = gmb_brain_reg_attached(name);
    char avail[64] = "";
    gmb_brain_reg_available(name, avail, sizeof avail);
    snprintf(t->rec.available, sizeof t->rec.available, "%s", avail);
    /* available_json per il round-trip */
    free(t->available_json);
    char aj[128];
    if (!avail[0]) snprintf(aj, sizeof aj, "[]");
    else if (!strcmp(avail, "rag,graph")) snprintf(aj, sizeof aj, "[\"rag\", \"graph\"]");
    else snprintf(aj, sizeof aj, "[\"%s\"]", avail);
    t->available_json = strdup(aj);
    if (!t->parsers_json) t->parsers_json = strdup("[]");
    if (!t->probe_json) t->probe_json = strdup("{}");
    return reg_save();
}

int gmb_brain_reg_unregister(const char* name)
{
    if (!name || !*name) return 0;
    reg_load();
    int idx = -1;
    for (int i = 0; i < g_nbrain; i++) if (!strcasecmp(g_brain[i].rec.name, name)) { idx = i; break; }
    if (idx < 0) return 0;
    free(g_brain[idx].probe_json);
    free(g_brain[idx].parsers_json);
    free(g_brain[idx].available_json);
    for (int i = idx; i + 1 < g_nbrain; i++) g_brain[i] = g_brain[i + 1];
    g_nbrain--;
    memset(&g_brain[g_nbrain], 0, sizeof g_brain[0]);
    /* rimuove anche da attached */
    for (int i = 0; i < g_nattached; i++) {
        if (!strcasecmp(g_attached[i], name)) {
            free(g_attached[i]);
            for (int j = i; j + 1 < g_nattached; j++) g_attached[j] = g_attached[j + 1];
            g_nattached--;
            break;
        }
    }
    return reg_save();
}
